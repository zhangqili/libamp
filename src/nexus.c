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

#ifndef NEXUS_SLAVE_CONFIG
#define NEXUS_SLAVE_CONFIG {{0,0}};
#endif

#ifndef NEXUS_SLAVE_NUM
#define NEXUS_SLAVE_NUM 1
#endif

#define NEXUS_TIMEOUT  POLLING_RATE

static struct {
    uint16_t begin;
    uint16_t length;
} slave_configs[NEXUS_SLAVE_NUM] = NEXUS_SLAVE_CONFIG;
static bool slave_flags[NEXUS_SLAVE_NUM];
static uint64_t slave_bitmap[NEXUS_SLAVE_NUM];

extern const uint16_t g_analog_map[ADVANCED_KEY_NUM];

static inline int nexus_send_timeout(uint8_t slave_id, uint8_t *report, uint16_t len, uint32_t timeout)
{
    const uint32_t start = g_keyboard_tick;
    while (start + timeout > g_keyboard_tick)
    {
        if (nexus_send(slave_id, report, len) == 0)
        {
            break;
        }
    }
    while (start + timeout > g_keyboard_tick)
    {
        if (slave_flags[slave_id])
        {
            slave_flags[slave_id] = false;
            return 0;
        }
    }
    return 1;
}

static inline void nexus_config_slave(uint8_t slave_id)
{
    uint8_t buffer[64];
    const uint16_t begin = slave_configs[slave_id].begin;
    const uint16_t length = slave_configs[slave_id].length;
    uint16_t count = 0;
    for (int i = 0; i < ADVANCED_KEY_NUM; i++)
    {
        AdvancedKey *key = &g_keyboard_advanced_keys[i];
        const uint16_t analog_index = g_analog_map[key->key.id];
        if (analog_index < begin || analog_index >= begin + length)
        {
            continue;
        }
        PacketAdvancedKey *packet = (PacketAdvancedKey *)buffer;
        memset(buffer, 0, sizeof(packet));
        packet->code = PACKET_CODE_SET;
        packet->type = PACKET_DATA_ADVANCED_KEY;
        packet->index = count;
        advanced_key_config_normalize(&packet->data, &g_keyboard_advanced_keys[i].config);
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
        count++;
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
    for (uint8_t slave_id = 0; slave_id < NEXUS_SLAVE_NUM; slave_id++)
    {
        for (int j = 0; j < slave_configs[slave_id].length; j++)
        {
            Key* key = keyboard_get_key(j+slave_configs[slave_id].begin);
            keyboard_key_update(key, BIT_GET(slave_bitmap[slave_id], j));
        }
    }
}

void nexus_process_buffer(uint8_t slave_id, uint8_t *buf, uint16_t len)
{
#if NEXUS_IS_SLAVE
    packet_process_buffer(buf, len);
    PacketNexus* packet = (PacketNexus*)buf;
    packet->index = ~(uint16_t)0;
    nexus_report(buf,len);
#else
    slave_flags[slave_id] = true;
    PacketNexus* packet = (PacketNexus*)buf;
    if ((uint16_t)~packet->index)
    {
        return;
    }
    memcpy(&slave_bitmap[slave_id], packet->bits, (slave_configs[slave_id].length+7)/8);
    Key* key = keyboard_get_key(packet->index + slave_configs[slave_id].begin);
    if (IS_ADVANCED_KEY(key))
    {
        ((AdvancedKey*)key)->raw = packet->raw;
        ((AdvancedKey*)key)->value = packet->value * (1/65536.f) * ANALOG_VALUE_RANGE;
    }
#endif
}

int nexus_send_report(void)
{
    static uint16_t counter;
    static uint8_t buffer[16];
    PacketNexus* packet = (PacketNexus*)buffer;
    packet->index = counter;
    Key* key = keyboard_get_key(counter);
    packet->raw = keyboard_get_key_raw_value(key);
    packet->value = (int16_t)(keyboard_get_key_analog_value(key)*(1/(float)ANALOG_VALUE_RANGE));
    memcpy(packet->bits, (void*)g_keyboard_bitmap, (TOTAL_KEY_NUM+7)/8);
    nexus_report(buffer, sizeof(PacketNexus) + (TOTAL_KEY_NUM+7)/8);
    counter++;
    if (counter>=TOTAL_KEY_NUM)
    {
        counter = 0;
    }
    return 0;
}
