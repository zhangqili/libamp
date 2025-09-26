/*
 * Copyright (c) 2024 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#include "dynamic_key.h"
#include "keyboard.h"

#define DK_TAP_DURATION 5

#ifdef DYNAMICKEY_ENABLE
DynamicKey g_keyboard_dynamic_keys[DYNAMIC_KEY_NUM];
#endif

void dynamic_key_event_handler(KeyboardEvent event)
{
    DynamicKey * dynamic_key = &g_keyboard_dynamic_keys[KEYCODE_GET_SUB(event.keycode)];
    AdvancedKey * key = (AdvancedKey*)event.key;
    switch (dynamic_key->type)
    {
    case DYNAMIC_KEY_STROKE:
        dynamic_key_s_event_handler(dynamic_key, event);
        break;
    case DYNAMIC_KEY_MOD_TAP:
        dynamic_key_mt_event_handler(dynamic_key, event);
        break;
    case DYNAMIC_KEY_TOGGLE_KEY:
        dynamic_key_tk_event_handler(dynamic_key, event);
        break;
    case DYNAMIC_KEY_MUTEX:
        dynamic_key_m_event_handler(dynamic_key, event);
        break;
    default:
        break;
    }
}


void _dynamic_key_add_buffer(KeyboardEvent event, DynamicKey*dynamic_key)
{
    switch (dynamic_key->type)
    {
    case DYNAMIC_KEY_STROKE:
        DynamicKeyStroke4x4*dynamic_key_s=(DynamicKeyStroke4x4*)dynamic_key;
        for (int i = 0; i < 4; i++)
        {
            if (BIT_GET(dynamic_key_s->key_state,i))
                keyboard_add_buffer(MK_EVENT(dynamic_key_s->key_binding[i], KEYBOARD_EVENT_NO_EVENT, event.key));
        }
        break;
    case DYNAMIC_KEY_MOD_TAP:
        DynamicKeyModTap*dynamic_key_mt=(DynamicKeyModTap*)dynamic_key;
        keyboard_add_buffer(MK_EVENT(dynamic_key_mt->key_binding[dynamic_key_mt->state], KEYBOARD_EVENT_NO_EVENT, event.key));
        break;
    case DYNAMIC_KEY_TOGGLE_KEY:
        DynamicKeyToggleKey*dynamic_key_tk=(DynamicKeyToggleKey*)dynamic_key;
        keyboard_add_buffer(MK_EVENT(dynamic_key_tk->key_binding, KEYBOARD_EVENT_NO_EVENT, event.key));
        break;
    case DYNAMIC_KEY_MUTEX:
        {
            DynamicKeyMutex*dynamic_key_m=(DynamicKeyMutex*)dynamic_key;
            AdvancedKey*key0 = &g_keyboard_advanced_keys[dynamic_key_m->key_id[0]];
            AdvancedKey*key1 = &g_keyboard_advanced_keys[dynamic_key_m->key_id[1]];
            if (key0->key.report_state)
                keyboard_add_buffer(MK_EVENT(dynamic_key_m->key_binding[0], KEYBOARD_EVENT_NO_EVENT, event.key));
            if (key1->key.report_state)
                keyboard_add_buffer(MK_EVENT(dynamic_key_m->key_binding[1], KEYBOARD_EVENT_NO_EVENT, event.key));
        }
        break;
    default:
        break;
    }
}

void dynamic_key_add_buffer(KeyboardEvent event)
{
    DynamicKey*dynamic_key = &g_keyboard_dynamic_keys[KEYCODE_GET_SUB(event.keycode)];
    _dynamic_key_add_buffer(event, dynamic_key);
}

#define DKS_PRESS_BEGIN 0
#define DKS_PRESS_FULLY 2
#define DKS_RELEASE_BEGIN 4
#define DKS_RELEASE_FULLY 6
#define DKS_GET_KEY_CONTROL(key_ctrl, n) (((key_ctrl) >> (n)) & 0x03)
void dynamic_key_s_event_handler(DynamicKey*dynamic_key, KeyboardEvent event)
{
    AdvancedKey * key = (AdvancedKey*)event.key;
    DynamicKeyStroke4x4*dynamic_key_s=(DynamicKeyStroke4x4*)dynamic_key;
    AnalogValue last_value = dynamic_key_s->value;
    AnalogValue current_value = key->value;
    uint8_t last_key_state = dynamic_key_s->key_state;
    if (current_value > last_value)
    {
        if (current_value - ANALOG_VALUE_MIN >= dynamic_key_s->press_begin_distance &&
            last_value - ANALOG_VALUE_MIN < dynamic_key_s->press_begin_distance)
        {
            for (int i = 0; i < 4; i++)
            {
                switch (DKS_GET_KEY_CONTROL(dynamic_key_s->key_control[i], DKS_PRESS_BEGIN))
                {
                case DKS_RELEASE:
                    BIT_RESET(dynamic_key_s->key_state, i);
                    break;
                case DKS_TAP:
                    dynamic_key_s->key_end_time[i] = g_keyboard_tick + DK_TAP_DURATION;
                    BIT_SET(dynamic_key_s->key_state, i);
                    break;
                case DKS_HOLD:
                    dynamic_key_s->key_end_time[i] = 0xFFFFFFFF;
                    BIT_SET(dynamic_key_s->key_state, i);
                    break;
                default:
                    BIT_RESET(dynamic_key_s->key_state, i);
                    break;
                }
            }
        }
        if (current_value - ANALOG_VALUE_MIN >= dynamic_key_s->press_fully_distance &&
            last_value - ANALOG_VALUE_MIN < dynamic_key_s->press_fully_distance)
        {
            for (int i = 0; i < 4; i++)
            {
                switch (DKS_GET_KEY_CONTROL(dynamic_key_s->key_control[i], DKS_PRESS_FULLY))
                {
                case DKS_RELEASE:
                    BIT_RESET(dynamic_key_s->key_state, i);
                    break;
                case DKS_TAP:
                    dynamic_key_s->key_end_time[i] = g_keyboard_tick + DK_TAP_DURATION;
                    BIT_SET(dynamic_key_s->key_state, i);
                    break;
                case DKS_HOLD:
                    dynamic_key_s->key_end_time[i] = 0xFFFFFFFF;
                    BIT_SET(dynamic_key_s->key_state, i);
                    break;
                default:
                    BIT_RESET(dynamic_key_s->key_state, i);
                    break;
                }
            }
        }
    }
    if (current_value < last_value)
    {
        if (current_value - ANALOG_VALUE_MIN <= dynamic_key_s->release_begin_distance &&
            last_value - ANALOG_VALUE_MIN > dynamic_key_s->release_begin_distance)
        {
            for (int i = 0; i < 4; i++)
            {
                switch (DKS_GET_KEY_CONTROL(dynamic_key_s->key_control[i], DKS_RELEASE_BEGIN))
                {
                case DKS_RELEASE:
                    BIT_RESET(dynamic_key_s->key_state, i);
                    break;
                case DKS_TAP:
                    dynamic_key_s->key_end_time[i] = g_keyboard_tick + DK_TAP_DURATION;
                    BIT_SET(dynamic_key_s->key_state, i);
                    break;
                case DKS_HOLD:
                    dynamic_key_s->key_end_time[i] = 0xFFFFFFFF;
                    BIT_SET(dynamic_key_s->key_state, i);
                    break;
                default:
                    BIT_RESET(dynamic_key_s->key_state, i);
                    break;
                }
            }
        }
        if (current_value - ANALOG_VALUE_MIN <= dynamic_key_s->release_fully_distance &&
            last_value - ANALOG_VALUE_MIN > dynamic_key_s->release_fully_distance)
        {
            for (int i = 0; i < 4; i++)
            {
                switch (DKS_GET_KEY_CONTROL(dynamic_key_s->key_control[i], DKS_RELEASE_FULLY))
                {
                case DKS_RELEASE:
                    BIT_RESET(dynamic_key_s->key_state, i);
                    break;
                case DKS_TAP:
                    dynamic_key_s->key_end_time[i] = g_keyboard_tick + DK_TAP_DURATION;
                    BIT_SET(dynamic_key_s->key_state, i);
                    break;
                case DKS_HOLD:
                    dynamic_key_s->key_end_time[i] = 0xFFFFFFFF;
                    BIT_SET(dynamic_key_s->key_state, i);
                    break;
                default:
                    BIT_RESET(dynamic_key_s->key_state, i);
                    break;
                }
            }
        }
    }

    for (int i = 0; i < 4; i++)
    {
        if (g_keyboard_tick > dynamic_key_s->key_end_time[i])
        {
            BIT_RESET(dynamic_key_s->key_state, i);
        }
        if (BIT_GET(dynamic_key_s->key_state, i) && !BIT_GET(last_key_state, i))
        {
            keyboard_advanced_key_event_handler(key, MK_EVENT(dynamic_key_s->key_binding[i], KEYBOARD_EVENT_KEY_DOWN, key));
        }
        if (!BIT_GET(dynamic_key_s->key_state, i) && BIT_GET(last_key_state, i))
        {
            keyboard_advanced_key_event_handler(key, MK_EVENT(dynamic_key_s->key_binding[i], KEYBOARD_EVENT_KEY_UP, key));
        }
    }
    key->key.report_state = dynamic_key_s->key_state > 0;
    dynamic_key_s->value = current_value;
}

void dynamic_key_mt_event_handler(DynamicKey*dynamic_key, KeyboardEvent event)
{
    AdvancedKey * key = (AdvancedKey*)event.key;
    DynamicKeyModTap*dynamic_key_mt=(DynamicKeyModTap*)dynamic_key;
    if (event.event == KEYBOARD_EVENT_KEY_DOWN)
    {
        dynamic_key_mt->begin_time = g_keyboard_tick;
    }
    if (event.event == KEYBOARD_EVENT_KEY_UP)
    {
        if (g_keyboard_tick - dynamic_key_mt->begin_time < dynamic_key_mt->duration)
        {
            dynamic_key_mt->end_time = g_keyboard_tick+DK_TAP_DURATION;
            dynamic_key_mt->state = DYNAMIC_KEY_ACTION_TAP;
            keyboard_advanced_key_event_handler(key, MK_EVENT(dynamic_key_mt->key_binding[1], KEYBOARD_EVENT_KEY_DOWN, key));
            key->key.report_state = true;
        }
        else
        {
            keyboard_advanced_key_event_handler(key, MK_EVENT(dynamic_key_mt->key_binding[1], KEYBOARD_EVENT_KEY_UP, key));
            key->key.report_state = false;
        }
        dynamic_key_mt->begin_time = g_keyboard_tick;
    }
    if (key->key.state && !key->key.report_state && (g_keyboard_tick - dynamic_key_mt->begin_time > dynamic_key_mt->duration))
    {
        dynamic_key_mt->end_time = 0xFFFFFFFF;
        dynamic_key_mt->state = DYNAMIC_KEY_ACTION_HOLD;
        keyboard_advanced_key_event_handler(key, MK_EVENT(dynamic_key_mt->key_binding[1], KEYBOARD_EVENT_KEY_DOWN, key));
        key->key.report_state = true;
    }
    if (g_keyboard_tick > dynamic_key_mt->end_time && key->key.report_state)
    {
        keyboard_advanced_key_event_handler(key, MK_EVENT(dynamic_key_mt->key_binding[1], KEYBOARD_EVENT_KEY_UP, key));
        key->key.report_state = false;
    }
}

void dynamic_key_tk_event_handler(DynamicKey*dynamic_key, KeyboardEvent event)
{
    AdvancedKey * key = (AdvancedKey*)event.key;
    DynamicKeyToggleKey*dynamic_key_tk=(DynamicKeyToggleKey*)dynamic_key;
    if (event.event == KEYBOARD_EVENT_KEY_DOWN)
    {
        dynamic_key_tk->state = !dynamic_key_tk->state;
        if (dynamic_key_tk->state)
        {
            keyboard_advanced_key_event_handler(key, MK_EVENT(dynamic_key_tk->key_binding, KEYBOARD_EVENT_KEY_DOWN, key));
        }
        else
        {
            keyboard_advanced_key_event_handler(key, MK_EVENT(dynamic_key_tk->key_binding, KEYBOARD_EVENT_KEY_UP, key));
        }
    }
}

void dynamic_key_m_event_handler(DynamicKey*dynamic_key, KeyboardEvent event)
{
    AdvancedKey * key = (AdvancedKey*)event.key;
    DynamicKeyMutex*dynamic_key_m=(DynamicKeyMutex*)dynamic_key;
    AdvancedKey*key0 = &g_keyboard_advanced_keys[dynamic_key_m->key_id[0]];
    AdvancedKey*key1 = &g_keyboard_advanced_keys[dynamic_key_m->key_id[1]];

    const bool last_key0_state = key0->key.report_state;
    const bool last_key1_state = key1->key.report_state;
    bool key0_state = last_key0_state;
    bool key1_state = last_key1_state;

    if ((dynamic_key_m->mode & 0x0F) == DK_MUTEX_DISTANCE_PRIORITY)
    {
        dynamic_key_m->trigger_state = !dynamic_key_m->trigger_state;
        if (!dynamic_key_m->trigger_state)
        {
            return;
        }

        if (((key0->value > key1->value) && (key0->value > key0->config.upper_deadzone)) ||
        ((dynamic_key_m->mode & 0x80) && (key0->value>= (ANALOG_VALUE_MAX - key0->config.lower_deadzone))&&
        (key1->value>= (ANALOG_VALUE_MAX - key1->config.lower_deadzone))))
        {
            key0_state = true;
        }
        else if (key0->value != key1->value)
        {
            key0_state = false;
        }

        if (((key0->value < key1->value) && (key1->value > key1->config.upper_deadzone))||
        ((dynamic_key_m->mode & 0x80) && (key0->value>= (ANALOG_VALUE_MAX - key0->config.lower_deadzone))&&
        (key1->value>= (ANALOG_VALUE_MAX - key1->config.lower_deadzone))))
        {
            key1_state = true;
        }
        else if (key0->value != key1->value)
        {
            key1_state = false;
        }

        if (dynamic_key_m->mode & 0xF0)
        {
            if ((key0->value>= (ANALOG_VALUE_MAX - key0->config.lower_deadzone))&&
            (key1->value>= (ANALOG_VALUE_MAX - key1->config.lower_deadzone)))
            {
                key0_state = true;
                key1_state = true;
            }
        }
        //advanced_key_update_state(key0, key0_state);
        //advanced_key_update_state(key1, key1_state);
        goto call_event;
    }

    switch (dynamic_key_m->mode & 0x0F)
    {
    case DK_MUTEX_LAST_PRIORITY:
        if (key->key.id == dynamic_key_m->key_id[0])
        {
            if (event.event == KEYBOARD_EVENT_KEY_DOWN)
            {
                key0_state = true;
                key1_state = false;
            }
            if (event.event == KEYBOARD_EVENT_KEY_UP)
            {
                key0_state = false;
                key1_state = key1->key.state;
            }
        }
        else if (key->key.id == dynamic_key_m->key_id[1])
        {
            if (event.event == KEYBOARD_EVENT_KEY_DOWN)
            {
                key0_state = false;
                key1_state = true;
            }
            if (event.event == KEYBOARD_EVENT_KEY_UP)
            {
                key0_state = key0->key.state;
                key1_state = false;
            }
        }
        break;
    case DK_MUTEX_KEY1_PRIORITY:
        key0_state = key0->key.state;
        key1_state = key0->key.state ? false : key1->key.state;
        break;
    case DK_MUTEX_KEY2_PRIORITY:
        key0_state = key1->key.state ? false : key0->key.state;
        key1_state = key1->key.state;
        break;
    case DK_MUTEX_NEUTRAL:
        key0_state = key0->key.state;
        key1_state = key1->key.state;
        if (key0->key.state && key1->key.state)
        {
            key0_state = false;
            key1_state = false;
        }
        break;
    default:
        break;
    }
    if (dynamic_key_m->mode & 0xF0)
    {
        if ((key0->value>= (ANALOG_VALUE_MAX - key0->config.lower_deadzone))&&
        (key1->value>= (ANALOG_VALUE_MAX - key1->config.lower_deadzone)))
        {
            key0_state = true;
            key1_state = true;
        }
    }
    call_event:
    if (key0_state && !last_key0_state)
    {
        keyboard_advanced_key_event_handler(key0, MK_EVENT(dynamic_key_m->key_binding[0], KEYBOARD_EVENT_KEY_DOWN, key));
    }
    if (!key0_state && last_key0_state)
    {
        keyboard_advanced_key_event_handler(key0, MK_EVENT(dynamic_key_m->key_binding[0], KEYBOARD_EVENT_KEY_UP, key));
    }
    if (key1_state && !last_key1_state)
    {
        keyboard_advanced_key_event_handler(key1, MK_EVENT(dynamic_key_m->key_binding[1], KEYBOARD_EVENT_KEY_DOWN, key));
    }
    if (!key1_state && last_key1_state)
    {
        keyboard_advanced_key_event_handler(key1, MK_EVENT(dynamic_key_m->key_binding[1], KEYBOARD_EVENT_KEY_UP, key));
    }
    key0->key.report_state = key0_state;
    key1->key.report_state = key1_state;
}