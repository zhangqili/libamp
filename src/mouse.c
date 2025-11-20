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
    if (MOUSE_KEYCODE_IS_MOVE(event.keycode))
    {
        g_keyboard_report_flags.mouse = true;
        KEYBOARD_KEY_SET_REPORT_STATE(event.key, true);
        return;
    }
    switch (event.event)
    {
    case KEYBOARD_EVENT_KEY_DOWN:
        keyboard_key_event_down_callback((Key*)event.key);
        g_keyboard_report_flags.mouse = true;
        break;
    case KEYBOARD_EVENT_KEY_TRUE:
        break;
    case KEYBOARD_EVENT_KEY_UP:
        g_keyboard_report_flags.mouse = true;
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

void mouse_add_buffer(KeyboardEvent event)
{
    if (MOUSE_KEYCODE_IS_MOVE(event.keycode))
    {
        g_keyboard_report_flags.mouse = true;
        mouse_set_axis(event.keycode, KEYBOARD_GET_KEY_EFFECTIVE_ANALOG_VALUE(event.key));
        return;
    }
    switch (KEYCODE_GET_SUB(event.keycode))
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
    switch (KEYCODE_GET_SUB(keycode))
    {
    case MOUSE_MOVE_UP:
        mouse.y -= mouse_value;
        break;
    case MOUSE_MOVE_DOWN:
        mouse.y += mouse_value;
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

static inline bool mouse_should_send(Mouse * restrict mouse, Mouse * restrict prev_mouse)
{
    bool changed = ((mouse->buttons != prev_mouse->buttons) ||
#ifdef MOUSE_EXTENDED_REPORT
                    (mouse->boot_x != 0 && mouse->boot_x != prev_mouse->boot_x) || (mouse->boot_y != 0 && mouse->boot_y != prev_mouse->boot_y) ||
#endif
                    (mouse->x != 0 && mouse->x != prev_mouse->x) || (mouse->y != 0 && mouse->y != prev_mouse->y) || (mouse->h != 0 && mouse->h != prev_mouse->h) || (mouse->v != 0 && mouse->v != prev_mouse->v)) ||
                    (mouse->x || mouse->y || mouse->v || mouse->h);
    return changed;
}

int mouse_buffer_send(void)
{
    static Mouse prev_mouse;
#ifdef MOUSE_SHARED_EP
    mouse.report_id = REPORT_ID_MOUSE;
#endif
    int ret = 0;
    if (mouse_should_send(&mouse, &prev_mouse))
    {
        ret = hid_send_mouse((uint8_t*)&mouse, sizeof(Mouse));
        if (!ret)
        {
            prev_mouse = mouse;
        }
    }
    return ret;
}
