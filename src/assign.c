/*
 * Copyright (c) 2025 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "assign.h"
#include "packet.h"
#include "driver.h"
#include "string.h"
#include "storage.h"

#ifndef ASSIGN_SLAVE_CONFIG
#define ASSIGN_SLAVE_CONFIG {{0,0}};
#endif

#ifndef ASSIGN_SLAVE_NUM
#define ASSIGN_SLAVE_NUM 0
#endif

const struct {
    uint16_t begin;
    uint16_t length;
} slave_configs[] = ASSIGN_SLAVE_CONFIG;

Assignment assignments[ASSIGN_SLAVE_NUM];

extern const uint16_t g_analog_map[ADVANCED_KEY_NUM];

static inline void assign_slave(uint8_t slave_id)
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
        while (assign_send(slave_id,buffer,64) != 0)
        {
            
        }
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
            while (assign_send(slave_id,buffer,64) != 0)
            {

            }
        }
        count++;
    }
}

void assign_init(void)
{
    for (int i = 0; i < ASSIGN_SLAVE_NUM; i++)
    {
        assign_slave(i);
    }
}

void assign_process(uint8_t slave_id, uint8_t *buf, uint16_t len)
{
    Assignment* assignment = &assignments[slave_id];
    PacketReport* packet = (PacketReport*)buf;
    if (packet->code == PACKET_CODE_SET && packet->type == PACKET_DATA_REPORT)
    {
        switch (packet->report_type)
        {
        case KEYBOARD_REPORT_FLAG:
            if (packet->length == 6)
            {
                memcpy(&assignment->keyboard_6kro_buffer, packet->data, 6);
            }
            else
            {
                memcpy(&assignment->keyboard_nkro_buffer, packet->data, sizeof(assignment->keyboard_nkro_buffer));
            }
            g_keyboard_report_flags.keyboard = true;
            break;
        case MOUSE_REPORT_FLAG:
            memcpy(&assignment->mouse_buffer, packet->data, sizeof(assignment->mouse_buffer));
            g_keyboard_report_flags.mouse = true;
            break;
        case CONSUMER_REPORT_FLAG:
            memcpy(&assignment->consumer_buffer, packet->data, sizeof(assignment->consumer_buffer));
            g_keyboard_report_flags.consumer = true;
            break;
        case SYSTEM_REPORT_FLAG:
            memcpy(&assignment->system_buffer, packet->data, sizeof(assignment->system_buffer));
            g_keyboard_report_flags.system = true;
            break;
        case JOYSTICK_REPORT_FLAG:
            memcpy(&assignment->joystick_buffer, packet->data, sizeof(assignment->joystick_buffer));
            g_keyboard_report_flags.joystick = true;
            break;
        default:
            break;
        }
    }
}

bool assign_get_state(uint16_t index)
{
    return 0;//BIT_GET(g_keyboard_bitmap[index/8], index%8);
}

int assign_report(void)
{
    static Assignment assignment;
    memset(&assignment, 0 ,sizeof(Assignment));
#ifdef MOUSE_ENABLE
    if (g_keyboard_report_flags.mouse)
    {
        static Mouse prev_mouse;
        int ret = 0;
        for (int i = 0; i < ASSIGN_SLAVE_NUM; i++)
        {
            assignment.mouse_buffer.buttons |= assignments[i].mouse_buffer.buttons;
            assignment.mouse_buffer.x += assignments[i].mouse_buffer.x;
            assignment.mouse_buffer.y += assignments[i].mouse_buffer.y;
            assignment.mouse_buffer.v += assignments[i].mouse_buffer.v;
            assignment.mouse_buffer.h += assignments[i].mouse_buffer.h;
        }
        if (mouse_should_send(&assignment.mouse_buffer, &prev_mouse))
        {
#ifdef MOUSE_SHARED_EP
            assignment.mouse_buffer.report_id = REPORT_ID_MOUSE;
#endif
            ret = hid_send_mouse((uint8_t*)&assignment.mouse_buffer, sizeof(Mouse));
            if (!ret)
            {
                prev_mouse = assignment.mouse_buffer;
            }
        }
        if (!ret)
        {
            g_keyboard_report_flags.mouse = false;
        }
    }
#endif
#ifdef EXTRAKEY_ENABLE
    if (g_keyboard_report_flags.consumer)
    {
        for (int i = 0; i < ASSIGN_SLAVE_NUM; i++)
        {
            assignment.consumer_buffer = assignments[i].consumer_buffer;
        }
        if (!hid_send_extra_key((uint8_t*)&assignment.consumer_buffer, sizeof(ExtraKey)))
        {
            g_keyboard_report_flags.consumer = false;
        }
    }
    if (g_keyboard_report_flags.system)
    {
        for (int i = 0; i < ASSIGN_SLAVE_NUM; i++)
        {
            assignment.system_buffer = assignments[i].system_buffer;
        }
        if (!hid_send_extra_key((uint8_t*)&assignment.system_buffer, sizeof(ExtraKey)))
        {
            g_keyboard_report_flags.system = false;
        }
    }
#endif
    if (g_keyboard_report_flags.keyboard)
    {
#ifdef NKRO_ENABLE
        if (g_keyboard_config.nkro)
        {
            for (int i = 0; i < ASSIGN_SLAVE_NUM; i++)
            {
                assignment.keyboard_nkro_buffer.modifier |= assignments[i].keyboard_nkro_buffer.modifier;
                for (size_t j = 0; j < sizeof(assignment.keyboard_nkro_buffer.buffer); j++)
                {
                    assignment.keyboard_nkro_buffer.buffer[j] |= assignments[i].keyboard_nkro_buffer.buffer[j];
                }
            }
            assignment.keyboard_nkro_buffer.report_id = REPORT_ID_NKRO;
            if (g_keyboard_config.winlock)
            {
                assignment.keyboard_nkro_buffer.modifier &= (~(KEY_LEFT_GUI | KEY_RIGHT_GUI)); 
            }
            if (!keyboard_NKRObuffer_send(&assignment.keyboard_nkro_buffer))
            {
                g_keyboard_report_flags.keyboard = false;
            }
        }
        else
        {
            // not implmented yet
#ifdef KEYBOARD_SHARED_EP
            keyboard_6kro_buffer.report_id = REPORT_ID_KEYBOARD;
#endif
            if (!keyboard_6KRObuffer_send(&assignment.keyboard_6kro_buffer))
            {
                g_keyboard_report_flags.keyboard = false;
            }
        }
        
#endif
    }
#ifdef JOYSTICK_ENABLE
    if (g_keyboard_report_flags.joystick)
    {
        for (int i = 0; i < ASSIGN_SLAVE_NUM; i++)
        {
            for (int j = 0; j < ((JOYSTICK_BUTTON_COUNT - 1) / 8 + 1); j++)
            {
                assignment.joystick_buffer.buttons[j] |= assignments[i].joystick_buffer.buttons[j]; 
            }
            for (int j = 0; j < (JOYSTICK_AXIS_COUNT); j++)
            {
                assignment.joystick_buffer.axes[j] += assignments[i].joystick_buffer.axes[j]; 
            }
        }
#ifdef JOYSTICK_SHARED_EP
        assignment.joystick_buffer.report_id = REPORT_ID_JOYSTICK;
#endif
        if (!hid_send_joystick((uint8_t*)&assignment.joystick_buffer, sizeof(Joystick)))
        {
            g_keyboard_report_flags.joystick = false;
        }
    }
#endif
    return 0;
}
