/*
 * Copyright (c) 2025 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef EVENT_H
#define EVENT_H
#include "stdint.h"
#include "keycode.h"

typedef enum
{
    KEYBOARD_EVENT_NO_EVENT  = 0x00,
    KEYBOARD_EVENT_KEY_FALSE = 0x00,//b0000'0000
    KEYBOARD_EVENT_KEY_UP    = 0x01,//b0000'0001
    KEYBOARD_EVENT_KEY_TRUE  = 0x02,//b0000'0010
    KEYBOARD_EVENT_KEY_DOWN  = 0x03,//b0000'0011
    KEYBOARD_EVENT_NUM       = 0x05,
} KeyboardEventType;

typedef struct
{
    Keycode keycode;
    uint8_t event;
    uint8_t is_virtual;
    void* key;
} KeyboardEvent;
#define MK_EVENT(keycode, event, key) ((KeyboardEvent){(keycode), (event), false, (key)})
#define MK_VIRTUAL_EVENT(keycode, event, key) ((KeyboardEvent){(keycode), (event), true, (key)})
#define CALC_EVENT(state, next_state) ((((bool)(state)) != ((bool)(next_state))) | (((bool)(next_state)) << 1))

#endif //EVENT_H
