/*
 * Copyright (c) 2024 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef KEY_H_
#define KEY_H_

#include "stdint.h"
#include "stddef.h"
#include "stdbool.h"

#include "keyboard_conf.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    KEY_EVENT_DOWN,
    KEY_EVENT_UP,
    KEY_EVENT_NUM
} KEY_EVENT;
typedef void (*key_cb_t)(void *);
typedef struct __KeyBase
{
    uint16_t id;
    uint8_t state;
    uint8_t report_state;
} KeyBase;
typedef struct __Key
{
    uint16_t id;
    uint8_t state;
    uint8_t report_state;
#if DEBOUNCE_PRESS > 0 || DEBOUNCE_RELEASE > 0
    int8_t debounce;
#endif
    key_cb_t key_cb[KEY_EVENT_NUM];
} Key;
bool key_update(Key *key, bool state);
void key_attach(Key *key, KEY_EVENT e, key_cb_t cb);

#ifdef __cplusplus
}
#endif

#endif