/*
 * Copyright (c) 2025 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef JOYSTICK_H
#define JOYSTICK_H

#include "keyboard.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \file
 *
 * \defgroup joystick HID Joystick
 * \{
 */

#ifndef JOYSTICK_BUTTON_COUNT
#    define JOYSTICK_BUTTON_COUNT 8
#elif JOYSTICK_BUTTON_COUNT > 32
#    error Joystick feature only supports up to 32 buttons
#endif

#ifndef JOYSTICK_AXIS_COUNT
#    define JOYSTICK_AXIS_COUNT 2
#elif JOYSTICK_AXIS_COUNT > 6
#    error Joystick feature only supports up to 6 axes
#endif

#if JOYSTICK_AXIS_COUNT == 0 && JOYSTICK_BUTTON_COUNT == 0
#    error Joystick feature requires at least one axis or button
#endif

#ifndef JOYSTICK_AXIS_RESOLUTION
#    define JOYSTICK_AXIS_RESOLUTION 8
#elif JOYSTICK_AXIS_RESOLUTION < 8 || JOYSTICK_AXIS_RESOLUTION > 16
#    error JOYSTICK_AXIS_RESOLUTION must be between 8 and 16
#endif

#define JOYSTICK_MAX_VALUE ((1L << (JOYSTICK_AXIS_RESOLUTION - 1)) - 1)

#define JOYSTICK_HAT_CENTER -1
#define JOYSTICK_HAT_NORTH 0
#define JOYSTICK_HAT_NORTHEAST 1
#define JOYSTICK_HAT_EAST 2
#define JOYSTICK_HAT_SOUTHEAST 3
#define JOYSTICK_HAT_SOUTH 4
#define JOYSTICK_HAT_SOUTHWEST 5
#define JOYSTICK_HAT_WEST 6
#define JOYSTICK_HAT_NORTHWEST 7

#if JOYSTICK_AXIS_RESOLUTION > 8
typedef int16_t JoystickAxis;
#else
typedef int8_t JoystickAxis;
#endif

typedef struct __Joystick {
#ifdef JOYSTICK_SHARED_EP
    uint8_t report_id;
#endif
#if JOYSTICK_AXIS_COUNT > 0
    JoystickAxis axes[JOYSTICK_AXIS_COUNT];
#endif

#ifdef JOYSTICK_HAS_HAT
    int8_t  hat : 4;
    uint8_t reserved : 4;
#endif

#if JOYSTICK_BUTTON_COUNT > 0
    uint8_t buttons[(JOYSTICK_BUTTON_COUNT - 1) / 8 + 1];
#endif
} __PACKED Joystick;

#define JOYSTICK_KEYCODE_GET_AXIS_MAP(keycode) (KEYCODE_GET_SUB((keycode) >> 5) & 0x03)
#define JOYSTICK_KEYCODE_IS_AXIS_INVERT(keycode) (KEYCODE_GET_SUB((keycode)) & 0x80)
#define JOYSTICK_KEYCODE_IS_AXIS(keycode) (KEYCODE_GET_SUB((keycode)) & 0xE0)
#define JOYSTICK_KEYCODE_GET_AXIS_INDEX(keycode) (KEYCODE_GET_SUB((keycode)) & 0x1F)

void joystick_event_handler(KeyboardEvent event);
void joystick_buffer_clear(void);
void joystick_add_buffer(KeyboardEvent event);
void joystick_set_axis(Keycode keycode, AnalogValue value);
int joystick_buffer_send(void);

#ifdef __cplusplus
}
#endif

#endif //JOYSTICK_H
