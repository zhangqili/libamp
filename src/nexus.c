/*
 * Copyright (c) 2025 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "nexus.h"
#include "packet.h"
#include "amp_protocol.h"
#include "driver.h"
#include "stddef.h"
#include "string.h"
#include "storage.h"
#include "analog.h"

#define NEXUS_TIMEOUT  POLLING_RATE
#define NEXUS_MIN(a, b) ((a) < (b) ? (a) : (b))
#define NEXUS_LOCAL_KEY_COUNT NEXUS_MIN(TOTAL_KEY_NUM, NEXUS_SLICE_LENGTH_MAX)
#define NEXUS_LOCAL_ADVANCED_KEY_COUNT NEXUS_MIN(ADVANCED_KEY_NUM, NEXUS_SLICE_LENGTH_MAX)
#define NEXUS_LOCAL_BITMAP_SIZE ((NEXUS_LOCAL_KEY_COUNT + 7) / 8)
#define NEXUS_OPCODE_SET_ADVANCED_KEY 0x7001
#define NEXUS_OPCODE_KEY_EVENT        0x7002

typedef struct {
    uint16_t index;
    AdvancedKeyConfiguration config;
} __PACKED NexusAdvancedKeyConfig;

static bool slave_flags[NEXUS_SLAVE_NUM];
static uint32_t slave_bitmap[NEXUS_SLAVE_NUM][(NEXUS_SLICE_LENGTH_MAX+31)/32];
#if NEXUS_USE_RAW
static AnalogRawValue nexus_slave_raw_values[ADVANCED_KEY_NUM];
#endif
uint8_t g_nexus_slave_buffer[NEXUS_SLAVE_NUM][NEXUS_BUFFER_SIZE];

__WEAK NexusSlaveConfig g_nexus_slave_configs[NEXUS_SLAVE_NUM];

static uint16_t nexus_slave_config_length(uint8_t slave_id)
{
    uint16_t length = g_nexus_slave_configs[slave_id].length;
    if (length > NEXUS_SLICE_LENGTH_MAX)
    {
        length = NEXUS_SLICE_LENGTH_MAX;
    }
    return length;
}

static int nexus_send_advanced_key_config(uint8_t slave_id, uint16_t local_index, uint16_t key_index)
{
    uint8_t report[AMP_FRAME_REPORT_SIZE];
    const AdvancedKeyConfiguration *config = &g_keyboard_advanced_keys[key_index].config;
    NexusAdvancedKeyConfig payload = {.index = local_index};
    memcpy(&payload.config, config, sizeof(payload.config));
    if (amp_frame_encode(report, sizeof(report), AMP_CHANNEL_USER, 0,
                         (uint16_t)(slave_id + 1U), 1,
                         NEXUS_OPCODE_SET_ADVANCED_KEY, AMP_STATUS_OK,
                         &payload, sizeof(payload)) < 0)
    {
        return 1;
    }
    return nexus_send_timeout(slave_id, report, sizeof(report), NEXUS_TIMEOUT);
}

static inline int nexus_config_slave(uint8_t slave_id)
{
    const uint16_t length = nexus_slave_config_length(slave_id);
    const uint16_t *map = g_nexus_slave_configs[slave_id].map;
    int ret = 0;

    if (map == NULL)
    {
        return 0;
    }

    for (uint16_t i = 0; i < length; i++)
    {
        const uint16_t key_index = map[i];
        if (key_index >= ADVANCED_KEY_NUM ||
            nexus_send_advanced_key_config(slave_id, i, key_index) != 0)
        {
            ret = 1;
        }
    }
    return ret;
}

int nexus_sync_advanced_key_config(uint16_t key_index)
{
    if (key_index >= ADVANCED_KEY_NUM)
    {
        return 1;
    }

    int ret = 0;
    for (uint8_t slave_id = 0; slave_id < NEXUS_SLAVE_NUM; slave_id++)
    {
        const uint16_t length = nexus_slave_config_length(slave_id);
        const uint16_t *map = g_nexus_slave_configs[slave_id].map;
        if (map == NULL)
        {
            continue;
        }

        for (uint16_t local_index = 0; local_index < length; local_index++)
        {
            if (map[local_index] == key_index &&
                nexus_send_advanced_key_config(slave_id, local_index, key_index) != 0)
            {
                ret = 1;
            }
        }
    }
    return ret;
}

void nexus_calibrate(void)
{
    for (int i = 0; i < NEXUS_SLAVE_NUM; i++)
    {
        uint8_t report[AMP_FRAME_REPORT_SIZE];
        AmpKeyEvent event = {
            .event = KEYBOARD_EVENT_KEY_DOWN,
            .keycode = KEYCODE(KEYBOARD_OPERATION, KEYBOARD_CALIBRATE),
            .is_virtual = true,
        };
        if (amp_frame_encode(report, sizeof(report), AMP_CHANNEL_USER, 0,
                             (uint16_t)(i + 1U), 1, NEXUS_OPCODE_KEY_EVENT,
                             AMP_STATUS_OK, &event, sizeof(event)) >= 0)
        {
            (void)nexus_send_timeout(i, report, sizeof(report), NEXUS_TIMEOUT);
        }
    }
}

void nexus_init(void)
{
    for (int i = 0; i < NEXUS_SLAVE_NUM; i++)
    {
        (void)nexus_config_slave(i);
    }
}

void nexus_process(void)
{
#if NEXUS_USE_RAW
    for (uint16_t i = 0; i < ADVANCED_KEY_NUM; i++)
    {
        AdvancedKey*advanced_key = &g_keyboard_advanced_keys[i];
        keyboard_advanced_key_update_raw(advanced_key, nexus_slave_raw_values[g_analog_map[i]]);
    }
#else
    for (uint8_t slave_id = 0; slave_id < NEXUS_SLAVE_NUM; slave_id++)
    {
        const uint16_t length = nexus_slave_config_length(slave_id);
        const uint16_t *map = g_nexus_slave_configs[slave_id].map;
        if (map == NULL)
        {
            continue;
        }

        for (uint16_t j = 0; j < length; j++)
        {
            const bool state = BIT_GET(slave_bitmap[slave_id][j/32], j%32);
            const uint16_t index = map[j];
            if (index >= TOTAL_KEY_NUM)
            {
                continue;
            }
            keyboard_key_update(keyboard_get_key(index), state);
        }
    }
#endif
}

void nexus_process_buffer(uint8_t slave_id, uint8_t *buf, uint16_t len)
{
#if NEXUS_IS_SLAVE
    AmpFrame frame;
    uint8_t response[AMP_FRAME_REPORT_SIZE];

    if (len < AMP_FRAME_REPORT_SIZE ||
        !amp_frame_decode(buf, len, &frame) ||
        amp_frame_channel(&frame.header) != AMP_CHANNEL_USER ||
        amp_frame_flags(&frame.header) != 0)
    {
        return;
    }
    AmpStatus status = AMP_STATUS_UNSUPPORTED;
    if (frame.header.opcode == NEXUS_OPCODE_SET_ADVANCED_KEY &&
        frame.header.payload_len == sizeof(NexusAdvancedKeyConfig))
    {
        NexusAdvancedKeyConfig payload;
        memcpy(&payload, frame.payload, sizeof(payload));
        if (payload.index < ADVANCED_KEY_NUM)
        {
            memcpy(&g_keyboard_advanced_keys[payload.index].config,
                   &payload.config, sizeof(payload.config));
            advanced_key_set_range(&g_keyboard_advanced_keys[payload.index],
                                   payload.config.upper_bound,
                                   payload.config.lower_bound);
            status = AMP_STATUS_OK;
        }
        else
        {
            status = AMP_STATUS_INVALID_ARGUMENT;
        }
    }
    else if (frame.header.opcode == NEXUS_OPCODE_KEY_EVENT &&
             frame.header.payload_len == sizeof(AmpKeyEvent))
    {
        AmpKeyEvent payload;
        memcpy(&payload, frame.payload, sizeof(payload));
        KeyboardEvent event = {
            .event = payload.event,
            .keycode = payload.keycode,
            .is_virtual = payload.is_virtual,
            .key = payload.is_virtual ? NULL : keyboard_get_key(payload.key_id),
        };
        keyboard_event_handler(event);
        status = AMP_STATUS_OK;
    }
    if (amp_frame_encode(response, sizeof(response), AMP_CHANNEL_USER,
                         AMP_FRAME_FLAG_RESPONSE, frame.header.session_id,
                         frame.header.request_id, frame.header.opcode, status,
                         NULL, 0) >= 0)
    {
        (void)nexus_report(response, sizeof(response));
    }
#else
    if (slave_id >= NEXUS_SLAVE_NUM || buf == NULL || len == 0)
    {
        return;
    }
    slave_flags[slave_id] = true;

    if (amp_is_frame(buf, len))
    {
        const uint16_t copy_len = NEXUS_MIN(len, NEXUS_BUFFER_SIZE);
        if (copy_len < AMP_FRAME_HEADER_SIZE)
        {
            return;
        }
        if (buf != g_nexus_slave_buffer[slave_id])
        {
            memset(g_nexus_slave_buffer[slave_id], 0, NEXUS_BUFFER_SIZE);
            memcpy(g_nexus_slave_buffer[slave_id], buf, copy_len);
        }
        else if (copy_len < NEXUS_BUFFER_SIZE)
        {
            memset(g_nexus_slave_buffer[slave_id] + copy_len, 0, NEXUS_BUFFER_SIZE - copy_len);
        }
        return;
    }

    if (!(buf[0] & 0x80))
    {
        return;
    }
#if NEXUS_USE_RAW
    uint16_t* raw_values = (uint16_t*)buf;
    const uint16_t length = nexus_slave_config_length(slave_id);
    const uint16_t *map = g_nexus_slave_configs[slave_id].map;
    if (map == NULL || length == 0 || len < sizeof(uint16_t))
    {
        return;
    }
    nexus_slave_raw_values[map[0]] = (buf[0] & 0x7F) + ((buf[1])<<7);
    const uint16_t raw_count = len / sizeof(uint16_t);
    for (uint16_t i = 1; i < length && i < raw_count; i++)
    {
        nexus_slave_raw_values[map[i]] = raw_values[i];
    }
#else
    PacketNexus* packet = (PacketNexus*)buf;
    uint16_t index = packet->index & 0x7f;
    const uint16_t length = nexus_slave_config_length(slave_id);
    const uint16_t *map = g_nexus_slave_configs[slave_id].map;
#if NEXUS_SLICE_LENGTH_MAX >= 128
    index |= ((uint16_t)(uint8_t)packet->index_high) << 7;
#endif
    if (map == NULL || index >= length)
    {
        return;
    }

    const size_t copy_len = (length + 7) / 8;
    const size_t packet_bits_offset = offsetof(PacketNexus, bits);
    if (len < packet_bits_offset + copy_len)
    {
        return;
    }
    memset(slave_bitmap[slave_id], 0, sizeof(slave_bitmap[slave_id]));
    memcpy(slave_bitmap[slave_id], packet->bits, copy_len);

    const uint16_t key_index = map[index];
    if (key_index < ADVANCED_KEY_NUM)
    {
        AdvancedKey *advanced_key = &g_keyboard_advanced_keys[key_index];
        advanced_key->filtered_raw = packet->raw;
#if NEXUS_VALUE_MAX != 0
        advanced_key->value = packet->value * (1/65536.f) * ANALOG_VALUE_RANGE;
#endif
    }
#endif
#endif
}

int nexus_send_report(void)
{
#if NEXUS_USE_RAW
    static uint8_t buffer[NEXUS_SLICE_LENGTH_MAX*sizeof(uint16_t)];
    uint16_t* raw_buffer = (uint16_t*)buffer;
    if (NEXUS_LOCAL_ADVANCED_KEY_COUNT == 0)
    {
        return 0;
    }
    uint16_t raw1 = advanced_key_read_raw(&g_keyboard_advanced_keys[0]);
    buffer[0] = raw1 & 0x7F;
    buffer[0] |= 0x80;
    buffer[1] = raw1 >> 7;
    for (uint16_t i = 1; i < NEXUS_LOCAL_ADVANCED_KEY_COUNT; i++)
    {
        AdvancedKey* advanced_key = &g_keyboard_advanced_keys[i];
        raw_buffer[i] = advanced_key_read_raw(advanced_key);
    }
    return nexus_report(buffer, sizeof(buffer));
#else
    static uint16_t counter;
    static uint8_t buffer[sizeof(PacketNexus)];

    if (NEXUS_LOCAL_KEY_COUNT == 0)
    {
        return 0;
    }
    if (counter >= NEXUS_LOCAL_KEY_COUNT)
    {
        counter = 0;
    }

    // No value field
    PacketNexus* packet = (PacketNexus*)buffer;
    packet->index = counter & 0x7F;
    packet->index |= 0x80;
#if NEXUS_SLICE_LENGTH_MAX >= 128
    packet->index_high = (counter >> 7) & 0xFF;
#endif
    Key* key = keyboard_get_key(counter);
    packet->raw = keyboard_get_key_raw_value(key);
#if NEXUS_VALUE_MAX != 0
    packet->value = (keyboard_get_key_analog_value(key)*NEXUS_VALUE_MAX/ANALOG_VALUE_RANGE);
#endif
    memset(packet->bits, 0, sizeof(packet->bits));
    memcpy(packet->bits, (const void*)g_keyboard_bitmap, NEXUS_LOCAL_BITMAP_SIZE);
    nexus_report(buffer, sizeof(PacketNexus));
    counter++;
    if (counter >= NEXUS_LOCAL_KEY_COUNT)
    {
        counter = 0;
    }
    return 0;
#endif
}

int nexus_send_timeout(uint8_t slave_id, const uint8_t *report, uint16_t len, uint32_t timeout)
{
    static uint16_t sequence;
    uint8_t frame_report[AMP_FRAME_REPORT_SIZE];
    uint16_t seq = ++sequence;
    if (seq == 0)
    {
        seq = ++sequence;
    }
    if (slave_id >= NEXUS_SLAVE_NUM || report == NULL || len != AMP_FRAME_REPORT_SIZE)
    {
        return 1;
    }
    memcpy(frame_report, report, AMP_FRAME_REPORT_SIZE);
    AmpFrameHeader *request_header = (AmpFrameHeader *)frame_report;
    request_header->proto = AMP_FRAME_PROTO;
    request_header->version = AMP_WIRE_VERSION;
    request_header->channel_flags = (uint8_t)(AMP_CHANNEL_USER << 4);
    request_header->session_id = (uint16_t)(slave_id + 1U);
    request_header->request_id = seq;
    request_header->status = AMP_STATUS_OK;

    const uint32_t start = g_keyboard_tick;
    uint32_t retry_count = 0;
    uint16_t count = 0;
    retry:
    while (start + timeout > g_keyboard_tick)
    {
        if (nexus_send(slave_id, frame_report, AMP_FRAME_REPORT_SIZE) == 0)
        {
            break;
        }
    }
    while (start + timeout > g_keyboard_tick)
    {
        AmpFrameHeader *header = (AmpFrameHeader *)g_nexus_slave_buffer[slave_id];
        if (header->proto == AMP_FRAME_PROTO &&
            header->version == AMP_WIRE_VERSION &&
            header->request_id == seq &&
            amp_frame_channel(header) == AMP_CHANNEL_USER &&
            header->opcode == request_header->opcode &&
            (amp_frame_flags(header) & AMP_FRAME_FLAG_RESPONSE))
        {
            uint8_t status = header->status;
            memset(g_nexus_slave_buffer[slave_id], 0, NEXUS_BUFFER_SIZE);
            slave_flags[slave_id] = false;
            return status == AMP_STATUS_OK ? 0 : 1;
        }
        count++;
        if (count > 10000)
        {
            count = 0;
            retry_count++;
            if (retry_count > NEXUS_RETRY_COUNT)
            {
                break;
            }
            goto retry;
        }
    }
    return 1;
}
