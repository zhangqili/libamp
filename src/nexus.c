/*
 * Copyright (c) 2025 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "nexus.h"
#include "packet.h"
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
uint8_t g_nexus_slave_buffer[NEXUS_SLAVE_NUM][NEXUS_RX_BUFFER_SIZE];

__WEAK NexusSlaveKeymap g_nexus_slave_configs[NEXUS_SLAVE_NUM];

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
    PacketAdvancedKey packet;
    memset(&packet, 0, sizeof(packet));
    packet.code = PACKET_CODE_SET;
    packet.type = PACKET_DATA_ADVANCED_KEY;
    packet.index = local_index;
    memcpy(&packet.data, &g_keyboard_advanced_keys[key_index].config, sizeof(AdvancedKeyConfiguration));
    return nexus_send_timeout(slave_id, (const uint8_t *)&packet, sizeof(packet), NEXUS_TIMEOUT);
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
    for (uint8_t i = 0; i < NEXUS_SLAVE_NUM; i++)
    {
        PacketEvent packet = {0};
        packet.code = PACKET_CODE_EVENT;
        packet.flag = PACKET_EVENT_NO_EVENT;
        packet.event = KEYBOARD_EVENT_KEY_UP;
        packet.keycode = KEYCODE(KEYBOARD_OPERATION, KEYBOARD_CALIBRATE);
        packet.is_virtual = true;
        packet.use_keymap = false;

        (void)nexus_send_timeout(i, (const uint8_t *)&packet, sizeof(packet), NEXUS_TIMEOUT);
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
#if defined(NEXUS_IS_SLAVE) && NEXUS_IS_SLAVE
    UNUSED(slave_id);
    if (buf != NULL && len != 0)
    {
        packet_process(buf, len);
    }
#else
    if (slave_id >= NEXUS_SLAVE_NUM || buf == NULL || len == 0)
    {
        return;
    }
    slave_flags[slave_id] = true;

    if (!(buf[0] & 0x80))
    {
        uint16_t copy_len = len > NEXUS_RX_BUFFER_SIZE ? NEXUS_RX_BUFFER_SIZE : len;
        if (buf != g_nexus_slave_buffer[slave_id])
        {
            memset(g_nexus_slave_buffer[slave_id], 0, NEXUS_RX_BUFFER_SIZE);
            memcpy(g_nexus_slave_buffer[slave_id], buf, copy_len);
        }
        else if (copy_len < NEXUS_RX_BUFFER_SIZE)
        {
            memset(g_nexus_slave_buffer[slave_id] + copy_len, 0, NEXUS_RX_BUFFER_SIZE - copy_len);
        }
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

int nexus_request_timeout(uint8_t slave_id, const uint8_t *request,
                          uint16_t request_len, uint32_t timeout,
                          uint8_t *out_response, uint16_t response_capacity)
{
    static uint8_t sequence;
    uint8_t buffer[64];
    if (slave_id >= NEXUS_SLAVE_NUM || request == NULL ||
        request_len < sizeof(PacketData) || request_len > sizeof(buffer))
    {
        return 1;
    }

    memset(buffer, 0, sizeof(buffer));
    memcpy(buffer, request, request_len);

    if (request[0] == PACKET_CODE_EVENT)
    {
        return nexus_send(slave_id, buffer, request_len);
    }

    uint8_t id = ++sequence;
    if (id == 0)
    {
        id = ++sequence;
    }
    buffer[1] = id;

    const uint32_t start = g_keyboard_tick;
    bool sent = false;
    while ((uint32_t)(g_keyboard_tick - start) < timeout)
    {
        if (!sent)
        {
            sent = nexus_send(slave_id, buffer, request_len) == 0;
            if (!sent)
            {
                continue;
            }
        }

        volatile uint8_t *response = g_nexus_slave_buffer[slave_id];
        if (response[0] == request[0] && response[1] == id)
        {
            if (out_response != NULL && response_capacity != 0)
            {
                const uint16_t copy_len = response_capacity < NEXUS_RX_BUFFER_SIZE
                                              ? response_capacity
                                              : NEXUS_RX_BUFFER_SIZE;
                memcpy(out_response, (const void *)response, copy_len);
            }
            memset(g_nexus_slave_buffer[slave_id], 0, NEXUS_RX_BUFFER_SIZE);
            slave_flags[slave_id] = false;
            return 0;
        }
    }
    return 1;
}

int nexus_send_timeout(uint8_t slave_id, const uint8_t *report, uint16_t len, uint32_t timeout)
{
    return nexus_request_timeout(slave_id, report, len, timeout, NULL, 0);
}
