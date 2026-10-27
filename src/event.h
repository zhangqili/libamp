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
#define MK_EVENT(keycode, event, key) ((KeyboardEvent){(Keycode)(keycode), (uint8_t)(event), false, (void*)(key)})
#define MK_VIRTUAL_EVENT(keycode, event, key) ((KeyboardEvent){(Keycode)(keycode), (uint8_t)(event), true, (void*)(key)})
/* Event type from "the state changed on this poll" and "the state is now pressed":
 * bit 0 is the change flag, bit 1 the current state (see KeyboardEventType). */
#define EVENT_TYPE(changed, state) ((uint8_t)(((changed) ? 0x01 : 0x00) | ((state) ? 0x02 : 0x00)))
#define CALC_EVENT(state, next_state) EVENT_TYPE(((bool)(state)) != ((bool)(next_state)), (next_state))
#define EVENT_CHANGED(event) ((event) & 0x01)
#define EVENT_STATE(event) (((event) >> 1) & 0x01)

#endif //EVENT_H
