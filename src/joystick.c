/*
 * Copyright (c) 2025 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "joystick.h"
#include "keyboard_def.h"
#include "string.h"
#include "driver.h"

static Joystick joystick;

void joystick_event_handler(KeyboardEvent event)
{
    if (JOYSTICK_KEYCODE_IS_AXIS(event.keycode))
    {
        g_keyboard_report_flags.joystick = true;
        ((Key*)event.key)->report_state = true;
        return;
    }
    switch (event.event)
    {
    case KEYBOARD_EVENT_KEY_DOWN:
        keyboard_key_event_down_callback((Key*)event.key);
        g_keyboard_report_flags.joystick = true;
        break;
    case KEYBOARD_EVENT_KEY_TRUE:
        break;
    case KEYBOARD_EVENT_KEY_UP:
        g_keyboard_report_flags.joystick = true;
        break;
    case KEYBOARD_EVENT_KEY_FALSE:
        break;
    default:
        break;
    }
}

void joystick_buffer_clear(void)
{
    memset(&joystick, 0, sizeof(Joystick));
}

void joystick_add_buffer(KeyboardEvent event)
{
    if (JOYSTICK_KEYCODE_IS_AXIS(event.keycode))
    {
        g_keyboard_report_flags.joystick = true;
        joystick_set_axis(event.keycode, KEYBOARD_GET_KEY_ANALOG_VALUE(event.key));
        return;
    }
    uint8_t button = KEYCODE_GET_SUB(event.keycode);
    if (button >= JOYSTICK_BUTTON_COUNT) return;

    joystick.buttons[button / 8] |= 1 << (button % 8);
    //joystick.dirty = true;
}

void joystick_set_axis(Keycode keycode, AnalogValue value)
{
    if (!JOYSTICK_KEYCODE_IS_AXIS(keycode)) return;
    uint8_t axis = JOYSTICK_KEYCODE_GET_AXIS_INDEX(keycode);
    if (axis >= JOYSTICK_AXIS_COUNT) return;
    JoystickAxis analog_value = A_NORM((value - ANALOG_VALUE_MIN)) * JOYSTICK_MAX_VALUE;
    switch (JOYSTICK_KEYCODE_GET_AXIS_MAP(keycode))
    {
    case 0x01:
        joystick.axes[axis] += analog_value;
        break;
    case 0x02:
        joystick.axes[axis] -= analog_value;
        break;
    case 0x03:
        joystick.axes[axis] += (JOYSTICK_KEYCODE_IS_AXIS_INVERT(keycode) ? - (analog_value*2 - JOYSTICK_MAX_VALUE) : (analog_value*2 - JOYSTICK_MAX_VALUE));
        break;
    default:
        break;
    }

}

int joystick_buffer_send(void)
{
#ifdef JOYSTICK_SHARED_EP
    joystick.report_id = REPORT_ID_JOYSTICK;
#endif
    return hid_send_joystick((uint8_t*)&joystick, sizeof(Joystick));
}
