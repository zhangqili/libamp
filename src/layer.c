/*
 * Copyright (c) 2024 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "layer.h"

#include "string.h"

uint8_t g_current_layer;
static uint16_t layer_state;
__WEAK Keycode g_keymap_cache[TOTAL_KEY_NUM];
bool g_keymap_lock[TOTAL_KEY_NUM];

void layer_event_handler(KeyboardEvent event)
{
    const uint8_t layer = ((event.keycode >> 8) & 0x0F);
    switch (event.event)
    {
    case KEYBOARD_EVENT_KEY_DOWN:
        if (!event.is_virtual)
        {
            keyboard_key_event_down_callback((Key*)event.key);
        }
        switch ((event.keycode >> 12) & 0x0F)
        {
        case LAYER_MOMENTARY:
            layer_toggle(layer);
            break;
        case LAYER_TURN_ON:
            layer_set(layer);
            break;
        case LAYER_TURN_OFF:
            layer_reset(layer);
            break;
        case LAYER_TOGGLE:
            layer_toggle(layer);
            break;
        default:
            break;
        }
        layer_cache_refresh();
        break;
    case KEYBOARD_EVENT_KEY_UP:
        switch ((event.keycode >> 12) & 0x0F)
        {
        case LAYER_MOMENTARY:
            layer_toggle(layer);
            break;
        default:
            break;
        }
        layer_cache_refresh();
        break;
    default:
        break;
    }
}

uint8_t layer_get(void)
{
#ifdef __GNUC__
    if (layer_state == 0)
        return 0;
    return 31 - __builtin_clz(layer_state);
#else
    for (int i = 15; i > 0; i--)
    {
        if (BIT_GET(layer_state,i))
        {
            return i;
        }
    }
    return 0;
#endif
}

void layer_set(uint8_t layer)
{
    BIT_SET(layer_state,layer);
    g_current_layer = layer_get();
}

void layer_reset(uint8_t layer)
{
    BIT_RESET(layer_state,layer);
    g_current_layer = layer_get();
}

void layer_toggle(uint8_t layer)
{
    BIT_TOGGLE(layer_state,layer);
    g_current_layer = layer_get();
}

Keycode layer_get_keycode(uint16_t id, int8_t layer)
{
    Keycode keycode = 0;
    while (layer>=0)
    {
        keycode = g_keymap[layer][id];
        if (KEYCODE_GET_MAIN(keycode) == KEY_TRANSPARENT)
        {
            layer--;
        }
        else
        {
            return keycode;
        }
    }
    return KEY_NO_EVENT;
}

