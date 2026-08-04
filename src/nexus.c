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
    AmpFrame frame = {0};
    PacketAdvancedKeys *body = (PacketAdvancedKeys *)frame.body;
    const AdvancedKeyConfiguration *config = &g_keyboard_advanced_keys[key_index].config;
    frame.header.proto = AMP_FRAME_PROTO;
    frame.header.channel_flags = (uint8_t)(AMP_CHANNEL_NEXUS_CTRL << 4);
    frame.header.code = PACKET_CODE_SET;
    frame.header.type = PACKET_DATA_ADVANCED_KEY;
    body->count = 1;
    body->items[0].index = local_index;
    body->items[0].config.mode = config->mode;
    body->items[0].config.calibration_mode = config->calibration_mode;
    body->items[0].config.activation_value = config->activation_value;
    body->items[0].config.deactivation_value = config->deactivation_value;
    body->items[0].config.trigger_distance = config->trigger_distance;
    body->items[0].config.release_distance = config->release_distance;
    body->items[0].config.trigger_speed = config->trigger_speed;
    body->items[0].config.release_speed = config->release_speed;
    body->items[0].config.upper_deadzone = config->upper_deadzone;
    body->items[0].config.lower_deadzone = config->lower_deadzone;
    body->items[0].config.upper_bound = config->upper_bound;
    body->items[0].config.lower_bound = config->lower_bound;
    return nexus_send_timeout(slave_id, (const uint8_t *)&frame, sizeof(frame), NEXUS_TIMEOUT);
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
        /*
        PacketKeymap *packet_keymap = (PacketKeymap *)buffer;
        memset(buffer, 0, sizeof(packet));
        packet_keymap->code = PACKET_CODE_SET;
        packet_keymap->type = PACKET_DATA_KEYMAP;
        packet_keymap->start = i;
        packet_keymap->length = 1;
        for (int j = 0; j < LAYER_NUM; j++)
        {
            packet_keymap->keymap[0] = g_keymap[j][i];
            packet_keymap->layer = j;
            nexus_send_timeout(slave_id,buffer,64,NEXUS_TIMEOUT);
        }
        */
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
        AmpFrame frame = {0};
        PacketEvent *body = (PacketEvent *)frame.body;
        frame.header.proto = AMP_FRAME_PROTO;
        frame.header.channel_flags = (uint8_t)(AMP_CHANNEL_NEXUS_CTRL << 4);
        frame.header.code = PACKET_CODE_EVENT;
        body->event = KEYBOARD_EVENT_KEY_DOWN;
        body->keycode = KEYCODE(KEYBOARD_OPERATION, KEYBOARD_CALIBRATE);
        body->is_virtual = true;
        nexus_send_timeout(i, (const uint8_t *)&frame, sizeof(frame), NEXUS_TIMEOUT);
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

    if (len < AMP_FRAME_REPORT_SIZE || !amp_frame_decode(buf, &frame))
    {
        return;
    }
    if (packet_process_frame_to_report(&frame, AMP_CHANNEL_NEXUS_CTRL, AMP_FRAME_FLAG_RESP, response))
    {
        nexus_report(response, sizeof(response));
    }
#else
    if (slave_id >= NEXUS_SLAVE_NUM || buf == NULL || len == 0)
    {
        return;
    }
    slave_flags[slave_id] = true;

    if (amp_is_frame(buf))
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
    static uint8_t sequence;
    uint8_t frame_report[AMP_FRAME_REPORT_SIZE];
    uint8_t seq = ++sequence;
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
    request_header->channel_flags = (uint8_t)((AMP_CHANNEL_NEXUS_CTRL << 4) |
                                               AMP_FRAME_FLAG_REQ_ACK);
    request_header->seq = seq;
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
            header->seq == seq &&
            amp_frame_channel(header) == AMP_CHANNEL_NEXUS_CTRL &&
            header->code == request_header->code &&
            header->type == request_header->type &&
            (amp_frame_flags(header) & AMP_FRAME_FLAG_RESP))
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
