/*
 * Copyright (c) 2026 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "gamepad.h"
#include "string.h"
#include "driver.h"

static Gamepad gamepad;

void gamepad_event_handler(KeyboardEvent event)
{
    if (GAMEPAD_KEYCODE_IS_AXIS(event.keycode))
    {
        g_keyboard_report_flags.gamepad = true;
        if (!event.is_virtual)
        {
            keyboard_key_set_report_state((Key*)(event.key), true);
        }
        return;
    }
    switch (event.event)
    {
    case KEYBOARD_EVENT_KEY_DOWN:
        g_keyboard_report_flags.gamepad = true;
        break;
    case KEYBOARD_EVENT_KEY_TRUE:
        break;
    case KEYBOARD_EVENT_KEY_UP:
        g_keyboard_report_flags.gamepad = true;
        break;
    case KEYBOARD_EVENT_KEY_FALSE:
        break;
    default:
        break;
    }
}

void gamepad_buffer_clear(void)
{
    memset(&gamepad, 0, sizeof(Gamepad));
}

void gamepad_add_buffer(KeyboardEvent event)
{
    if (GAMEPAD_KEYCODE_IS_AXIS(event.keycode))
    {
        g_keyboard_report_flags.gamepad = true;
        gamepad_set_axis(event.keycode, keyboard_get_key_analog_value(event.key));
        return;
    }
    switch (KEYCODE_GET_SUB(event.keycode))
    {
    case GAMEPAD_LT:
        gamepad.lt = 255;
        break;
    case GAMEPAD_RT:
        gamepad.rt = 255;
        break;
    default:
        gamepad.buttons |= BIT(KEYCODE_GET_SUB(event.keycode));
        break;
    }
    return;
}

void gamepad_set_axis(Keycode keycode, AnalogValue value)
{
    if (!GAMEPAD_KEYCODE_IS_AXIS(keycode)) return;
    int32_t analog_value = A_NORM((value - ANALOG_VALUE_MIN)) * 32767;
    switch (KEYCODE_GET_SUB(keycode))
    {
    case GAMEPAD_LXP:
        gamepad.lx = clamp_i32(gamepad.lx + analog_value, -32768, 32767);
        break;
    case GAMEPAD_LXN:
        gamepad.lx = clamp_i32(gamepad.lx - analog_value, -32768, 32767);
        break;
    case GAMEPAD_LYP:
        gamepad.ly = clamp_i32(gamepad.ly + analog_value, -32768, 32767);
        break;
    case GAMEPAD_LYN:
        gamepad.ly = clamp_i32(gamepad.ly - analog_value, -32768, 32767);
        break;
    case GAMEPAD_RXP:
        gamepad.rx = clamp_i32(gamepad.rx + analog_value, -32768, 32767);
        break;
    case GAMEPAD_RXN:
        gamepad.rx = clamp_i32(gamepad.rx - analog_value, -32768, 32767);
        break;
    case GAMEPAD_RYP:
        gamepad.ry = clamp_i32(gamepad.ry + analog_value, -32768, 32767);
        break;
    case GAMEPAD_RYN:
        gamepad.ry = clamp_i32(gamepad.ry - analog_value, -32768, 32767);
        break;
    case GAMEPAD_LTA:
        gamepad.lt = clamp_i32(gamepad.lt + A_NORM((value - ANALOG_VALUE_MIN))*255, 0, 255);
        break;
    case GAMEPAD_RTA:
        gamepad.rt = clamp_i32(gamepad.rt + A_NORM((value - ANALOG_VALUE_MIN))*255, 0, 255);
        break;
    default:
        break;
    }
}

int gamepad_buffer_send(void)
{
    gamepad.report_id = 0;
    gamepad.report_size = 0x14;
    return hid_send_gamepad((uint8_t*)&gamepad, sizeof(Gamepad));
}

__WEAK void gamepad_out_callback(GamepadOutReport* report)
{
    UNUSED(report);
}
