/*
 * Copyright (c) 2025 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef EVENT_H
#define EVENT_H
#include "stdint.h"
#include "keycode.h"
#include "advanced_key.h"

typedef enum
{
    KEYBOARD_EVENT_NO_EVENT,
    KEYBOARD_EVENT_KEY_DOWN,
    KEYBOARD_EVENT_KEY_UP,
    KEYBOARD_EVENT_KEY_TRUE,
    KEYBOARD_EVENT_KEY_FALSE,
    KEYBOARD_EVENT_NUM
} KeyboardEventType;

typedef struct
{
    Keycode keycode;
    uint16_t event;
    void* key;
} KeyboardEvent;
#define MK_EVENT(keycode, event, key) ((KeyboardEvent){(keycode), (event), (key)})

#endif //EVENT_H
