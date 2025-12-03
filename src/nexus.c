/*
 * Copyright (c) 2025 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "nexus.h"
#include "packet.h"
#include "driver.h"
#include "string.h"
#include "storage.h"

#define NEXUS_TIMEOUT  POLLING_RATE

static bool slave_flags[NEXUS_SLAVE_NUM];
static uint32_t slave_bitmap[NEXUS_SLAVE_NUM][(NEXUS_SLICE_LENGTH_MAX+31)/32];
#if NEXUS_USE_RAW
static AnalogRawValue nexus_slave_raw_values[ADVANCED_KEY_NUM];
#endif
uint8_t g_nexus_slave_buffer[NEXUS_SLAVE_NUM][NEXUS_BUFFER_SIZE];

__WEAK NexusSlaveConfig g_nexus_slave_configs[NEXUS_SLAVE_NUM];

static inline void nexus_config_slave(uint8_t slave_id)
{
    uint8_t buffer[64];
    const uint16_t length = g_nexus_slave_configs[slave_id].length;
    for (int i = 0; i < length; i++)
    {
        AdvancedKey *key = &g_keyboard_advanced_keys[g_nexus_slave_configs[slave_id].map[i]];
        PacketAdvancedKey *packet = (PacketAdvancedKey *)buffer;
        memset(buffer, 0, sizeof(packet));
        packet->code = PACKET_CODE_SET;
        packet->type = PACKET_DATA_ADVANCED_KEY;
        packet->index = i;
        advanced_key_config_normalize(&packet->data, &key->config);
        nexus_send_timeout(slave_id,buffer,64,NEXUS_TIMEOUT);
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
}

void nexus_init(void)
{
    for (int i = 0; i < NEXUS_SLAVE_NUM; i++)
    {
        nexus_config_slave(i);
    }
}

void nexus_process(void)
{
#if NEXUS_USE_RAW
    for (uint16_t i = 0; i < ADVANCED_KEY_NUM; i++)
    {
        AdvancedKey*advanced_key = &g_keyboard_advanced_keys[i];
        keyboard_advanced_key_update_raw(advanced_key, nexus_slave_raw_values[i]);
    }
#else
    for (uint8_t slave_id = 0; slave_id < NEXUS_SLAVE_NUM; slave_id++)
    {
        for (int j = 0; j < g_nexus_slave_configs[slave_id].length; j++)
        {
            const bool state = BIT_GET(slave_bitmap[slave_id][j/32], j%32);
            const uint16_t index = g_nexus_slave_configs[slave_id].map[j];
            Key* key = keyboard_get_key(index);
            keyboard_key_update(key, state);
        }
    }
#endif
}

void nexus_process_buffer(uint8_t slave_id, uint8_t *buf, uint16_t len)
{
#if NEXUS_IS_SLAVE
    packet_process_buffer(buf, len);
    nexus_report(buf,len);
#else
    //memcpy(g_nexus_slave_buffer[slave_id], buf, len);
    slave_flags[slave_id] = true;
    if (!(((PacketBase*)buf)->code & 0x80))
    {
        return;
    }
#if NEXUS_USE_RAW
    uint16_t* raw_values = (uint16_t*)buf;
    nexus_slave_raw_values[g_nexus_slave_configs[slave_id].map[0]] = (buf[0] & 0x7F) + ((buf[1])<<7);
    for (int i = 1; i < g_nexus_slave_configs[slave_id].length; i++)
    {
        nexus_slave_raw_values[g_nexus_slave_configs[slave_id].map[i]] = raw_values[i];
    }
#else
    PacketNexus* packet = (PacketNexus*)buf;
    uint16_t index = packet->index & 0x7f;
    memcpy(&slave_bitmap[slave_id], packet->bits, (g_nexus_slave_configs[slave_id].length+7)/8);
    Key* key = keyboard_get_key(g_nexus_slave_configs[slave_id].map[index]);
    if (IS_ADVANCED_KEY(key))
    {
        ((AdvancedKey*)key)->raw = packet->raw;
        ((AdvancedKey*)key)->value = packet->value * (1/65536.f) * ANALOG_VALUE_RANGE;
    }
#endif
#endif
}

int nexus_send_report(void)
{
#if NEXUS_USE_RAW
    static uint8_t buffer[NEXUS_SLICE_LENGTH_MAX*sizeof(uint16_t)];
    uint16_t* raw_buffer = (uint16_t*)buffer;
    uint16_t raw1 = advanced_key_read(&g_keyboard_advanced_keys[0]);
    buffer[0] = raw1 & 0x7F;
    buffer[0] |= 0x80;
    buffer[1] = raw1 >> 7;
    for (int i = 1; i < ADVANCED_KEY_NUM; i++)
    {
        AdvancedKey* advanced_key = &g_keyboard_advanced_keys[i];
        raw_buffer[i] = advanced_key_read(advanced_key);
    }
    return nexus_report(buffer, sizeof(buffer));
#else
    static uint16_t counter;
    static uint8_t buffer[16];

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
    packet->value = (keyboard_get_key_analog_value(key)*(1/(float)ANALOG_VALUE_RANGE)*NEXUS_VALUE_MAX);
#endif
    memcpy(packet->bits, (void*)g_keyboard_bitmap, (TOTAL_KEY_NUM+7)/8);
    nexus_report(buffer, sizeof(PacketNexus));
    counter++;
    if (counter>=TOTAL_KEY_NUM)
    {
        counter = 0;
    }
    return 0;
#endif
}

int nexus_send_timeout(uint8_t slave_id, const uint8_t *report, uint16_t len, uint32_t timeout)
{
    const uint32_t start = g_keyboard_tick;
    uint32_t retry_count = 0;
    uint16_t count = 0;
    retry:
    while (start + timeout > g_keyboard_tick)
    {
        if (nexus_send(slave_id, (uint8_t*)report, len) == 0)
        {
            break;
        }
    }
    while (start + timeout > g_keyboard_tick)
    {
        if (g_nexus_slave_buffer[slave_id][0] == report[0] && g_nexus_slave_buffer[slave_id][1] == report[1])
        {
            g_nexus_slave_buffer[slave_id][0] = 0;
            g_nexus_slave_buffer[slave_id][1] = 0;
            slave_flags[slave_id] = false;
            return 0;
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

