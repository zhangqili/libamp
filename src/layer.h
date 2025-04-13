/*
 * Copyright (c) 2024 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef LAYER_H_
#define LAYER_H_
#include "stdint.h"
#include "stdbool.h"
#include "keycode.h"
#include "event.h"
#include "keyboard_conf.h"

#ifdef __cplusplus
extern "C" {
#endif

#define layer_cache_get_keycode(layer) g_keymap_cache[(layer)]

extern uint8_t g_current_layer;
extern Keycode g_keymap_cache[ADVANCED_KEY_NUM + KEY_NUM];
extern bool g_keymap_lock[ADVANCED_KEY_NUM + KEY_NUM];

void layer_control(KeyboardEvent event);
uint8_t layer_get(void);
void layer_set(uint8_t layer);
void layer_reset(uint8_t layer);
void layer_toggle(uint8_t layer);
void layer_lock(uint16_t id);
void layer_unlock(uint16_t id);
void layer_cache_refresh(void);
Keycode layer_get_keycode(uint16_t id, int8_t layer);

#ifdef __cplusplus
}
#endif

#endif /* LAYER_H_ */
