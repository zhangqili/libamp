/*
 * Copyright (c) 2024 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "keyboard.h"
#include "mouse.h"
#include "string.h"
#include "driver.h"

static Mouse mouse;

void mouse_event_handler(KeyboardEvent event)
{
    switch (event.event)
    {
    case KEYBOARD_EVENT_KEY_UP:
    case KEYBOARD_EVENT_KEY_DOWN:
        KEYBOARD_REPORT_FLAG_SET(MOUSE_REPORT_FLAG);
        break;
    case KEYBOARD_EVENT_KEY_TRUE:
        if (MODIFIER(event.keycode) & 0xF0)
        {
            KEYBOARD_REPORT_FLAG_SET(MOUSE_REPORT_FLAG);
            mouse_set_axis(MODIFIER(event.keycode), IS_ADVANCED_KEY(event.key) ? ((AdvancedKey*)event.key)->value : ANALOG_VALUE_MAX);
            break;
        }
        mouse_add_buffer(MODIFIER(event.keycode));
        break;
    case KEYBOARD_EVENT_KEY_FALSE:
        break;
    default:
        break;
    }
}


void mouse_buffer_clear(void)
{
    memset(&mouse, 0, sizeof(Mouse));
}

void mouse_add_buffer(Keycode keycode)
{
    switch (keycode)
    {
    case MOUSE_LBUTTON:
        mouse.buttons |= BIT(0);
        break;
    case MOUSE_RBUTTON:
        mouse.buttons |= BIT(1);
        break;
    case MOUSE_MBUTTON:
        mouse.buttons |= BIT(2);
        break;
    case MOUSE_FORWARD:
        mouse.buttons |= BIT(3);
        break;
    case MOUSE_BACK:
        mouse.buttons |= BIT(4);
        break;
    case MOUSE_WHEEL_UP:
        mouse.v = 1;
        break;
    case MOUSE_WHEEL_DOWN:
        mouse.v = -1;
        break;
    case MOUSE_WHEEL_LEFT:
        mouse.h = -1;
        break;
    case MOUSE_WHEEL_RIGHT:
        mouse.h = 1;
        break;
    default:
        break;
    }
}

static inline bool should_move(int t, int speed)
{
    return ((t + 1) * speed / REPORT_RATE ) - ( t * speed / REPORT_RATE);
}


void mouse_set_axis(Keycode keycode, AnalogValue value)
{
    uint32_t speed = A_NORM((value - ANALOG_VALUE_MIN)) * MOUSE_MAX_SPEED;
    uint32_t mouse_value = speed / REPORT_RATE + should_move(g_keyboard_tick%REPORT_RATE, speed);
    switch (keycode)
    {
    case MOUSE_MOVE_UP:
        mouse.y += mouse_value;
        break;
    case MOUSE_MOVE_DOWN:
        mouse.y -= mouse_value;
        break;
    case MOUSE_MOVE_LEFT:
        mouse.x -= mouse_value;
        break;
    case MOUSE_MOVE_RIGHT:
        mouse.x += mouse_value;
        break;
    default:
        break;
    }

}

int mouse_buffer_send(void)
{
    uint8_t v = mouse.v;
    uint8_t h = mouse.h;
    static uint8_t prev_v;
    static uint8_t prev_h;
    if (prev_v == v)
    {
        mouse.v = 0;
    }
    if (prev_h == h)
    {
        mouse.h = 0;
    }
#ifdef MOUSE_SHARED_EP
    mouse.report_id = REPORT_ID_MOUSE;
#endif
    int ret = hid_send_mouse((uint8_t*)&mouse, sizeof(Mouse));
    if (!ret)
    {
        prev_v = v;
        prev_h = h;
    }
    return ret;
}
