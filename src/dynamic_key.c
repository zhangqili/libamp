/*
 * Copyright (c) 2024 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */


#include "dynamic_key.h"
#include "command.h"
#include "keyboard.h"

#define DK_TAP_DURATION 5

#ifdef DYNAMICKEY_ENABLE
DynamicKey g_keyboard_dynamic_keys[DYNAMIC_KEY_NUM];
#endif

void dynamic_key_update(DynamicKey*dynamic_key,AdvancedKey*advanced_key, bool state)
{
    switch (dynamic_key->type)
    {
    case DYNAMIC_KEY_STROKE:
        dynamic_key_s_update(dynamic_key, advanced_key, state);
        break;
    case DYNAMIC_KEY_MOD_TAP:
        dynamic_key_mt_update(dynamic_key, advanced_key, state);
        break;
    case DYNAMIC_KEY_TOGGLE_KEY:
        dynamic_key_tk_update(dynamic_key, advanced_key, state);
        break;
    case DYNAMIC_KEY_MUTEX:
        dynamic_key_m_update(dynamic_key, advanced_key, state);
        break;
    default:
        break;
    }
}

void dynamic_key_event_handler(KeyboardEvent event)
{
    switch (event.event)
    {
    case KEYBOARD_EVENT_KEY_UP:
        break;
    case KEYBOARD_EVENT_KEY_DOWN:
        break;
    case KEYBOARD_EVENT_KEY_TRUE:
        dynamic_key_add_buffer(event, &g_keyboard_dynamic_keys[MODIFIER(event.keycode)]);
        break;
    case KEYBOARD_EVENT_KEY_FALSE:
        break;
    default:
        break;
    }
}


void dynamic_key_add_buffer(KeyboardEvent event, DynamicKey*dynamic_key)
{
    switch (dynamic_key->type)
    {
    case DYNAMIC_KEY_STROKE:
        DynamicKeyStroke4x4*dynamic_key_s=(DynamicKeyStroke4x4*)dynamic_key;
        for (int i = 0; i < 4; i++)
        {
            if (BIT_GET(dynamic_key_s->key_state,i))
                keyboard_event_handler(MK_EVENT(dynamic_key_s->key_binding[i], KEYBOARD_EVENT_KEY_TRUE, event.key));
        }
        break;
    case DYNAMIC_KEY_MOD_TAP:
        DynamicKeyModTap*dynamic_key_mt=(DynamicKeyModTap*)dynamic_key;
        keyboard_event_handler(MK_EVENT(dynamic_key_mt->key_binding[dynamic_key_mt->state], KEYBOARD_EVENT_KEY_TRUE, event.key));
        break;
    case DYNAMIC_KEY_TOGGLE_KEY:
        DynamicKeyToggleKey*dynamic_key_tk=(DynamicKeyToggleKey*)dynamic_key;
        keyboard_event_handler(MK_EVENT(dynamic_key_tk->key_binding, KEYBOARD_EVENT_KEY_TRUE, event.key));
        break;
    case DYNAMIC_KEY_MUTEX:
        {
            DynamicKeyMutex*dynamic_key_m=(DynamicKeyMutex*)dynamic_key;
            AdvancedKey*key0 = &g_keyboard_advanced_keys[dynamic_key_m->key_id[0]];
            AdvancedKey*key1 = &g_keyboard_advanced_keys[dynamic_key_m->key_id[1]];
            if (key0->key.report_state)
                keyboard_event_handler(MK_EVENT(dynamic_key_m->key_binding[0], KEYBOARD_EVENT_KEY_TRUE, event.key));
            if (key1->key.report_state)
                keyboard_event_handler(MK_EVENT(dynamic_key_m->key_binding[1], KEYBOARD_EVENT_KEY_TRUE, event.key));
        }
        break;
    default:
        break;
    }
}

#define DKS_PRESS_BEGIN 0
#define DKS_PRESS_FULLY 2
#define DKS_RELEASE_BEGIN 4
#define DKS_RELEASE_FULLY 6
#define DKS_GET_KEY_CONTROL(key_ctrl, n) (((key_ctrl) >> (n)) & 0x03)
void dynamic_key_s_update(DynamicKey*dynamic_key, AdvancedKey*key, bool state)
{
    UNUSED(state);
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
    advanced_key_update_state(key, dynamic_key_s->key_state > 0);
    key->key.report_state = key->key.state;
    dynamic_key_s->value = current_value;
}

void dynamic_key_mt_update(DynamicKey*dynamic_key, AdvancedKey*key, bool state)
{
    DynamicKeyModTap*dynamic_key_mt=(DynamicKeyModTap*)dynamic_key;
    if (!(key->key.state) && state)
    {
        dynamic_key_mt->begin_time = g_keyboard_tick;
    }
    if ((key->key.state) && !state)
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
    if (state && !key->key.report_state && (g_keyboard_tick - dynamic_key_mt->begin_time > dynamic_key_mt->duration))
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
    advanced_key_update_state(key, state);
}

void dynamic_key_tk_update(DynamicKey*dynamic_key, AdvancedKey*key, bool state)
{
    DynamicKeyToggleKey*dynamic_key_tk=(DynamicKeyToggleKey*)dynamic_key;
    if (!(key->key.state) && state)
    {
        dynamic_key_tk->state = !dynamic_key_tk->state;
        key->key.report_state = dynamic_key_tk->state;
        if (dynamic_key_tk->state)
        {
            keyboard_advanced_key_event_handler(key, MK_EVENT(dynamic_key_tk->key_binding, KEYBOARD_EVENT_KEY_DOWN, key));
        }
        else
        {
            keyboard_advanced_key_event_handler(key, MK_EVENT(dynamic_key_tk->key_binding, KEYBOARD_EVENT_KEY_UP, key));
        }
    }
    advanced_key_update_state(key, state);
}

void dynamic_key_m_update(DynamicKey*dynamic_key, AdvancedKey*key, bool state)
{
    DynamicKeyMutex*dynamic_key_m=(DynamicKeyMutex*)dynamic_key;
    AdvancedKey*key0 = &g_keyboard_advanced_keys[dynamic_key_m->key_id[0]];
    AdvancedKey*key1 = &g_keyboard_advanced_keys[dynamic_key_m->key_id[1]];

    const bool last_key0_state = key0->key.report_state;
    const bool last_key1_state = key1->key.report_state;

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
            key0->key.report_state = true;
        }
        else if (key0->value != key1->value)
        {
            key0->key.report_state = false;
        }

        if (((key0->value < key1->value) && (key1->value > key1->config.upper_deadzone))||
        ((dynamic_key_m->mode & 0x80) && (key0->value>= (ANALOG_VALUE_MAX - key0->config.lower_deadzone))&&
        (key1->value>= (ANALOG_VALUE_MAX - key1->config.lower_deadzone))))
        {
            key1->key.report_state = true;
        }
        else if (key0->value != key1->value)
        {
            key1->key.report_state = false;
        }

        if (dynamic_key_m->mode & 0xF0)
        {
            if ((key0->value>= (ANALOG_VALUE_MAX - key0->config.lower_deadzone))&&
            (key1->value>= (ANALOG_VALUE_MAX - key1->config.lower_deadzone)))
            {
                key0->key.report_state = true;
                key1->key.report_state = true;
            }
        }
        advanced_key_update_state(key0, key0->key.report_state);
        advanced_key_update_state(key1, key1->key.report_state);
        goto call_event;
    }

    switch (dynamic_key_m->mode & 0x0F)
    {
    case DK_MUTEX_LAST_PRIORITY:
        if (key->key.id == dynamic_key_m->key_id[0])
        {
            if (state && !key->key.state)
            {
                key0->key.report_state = true;
                key1->key.report_state = false;
            }
            if (!state && key->key.state)
            {
                key0->key.report_state = false;
                key1->key.report_state = key1->key.state;
            }
        }
        else if (key->key.id == dynamic_key_m->key_id[1])
        {
            if (state && !key->key.state)
            {
                key0->key.report_state = false;
                key1->key.report_state = true;
            }
            if (!state && key->key.state)
            {
                key0->key.report_state = key0->key.state;
                key1->key.report_state = false;
            }
        }
        advanced_key_update_state(key, state);
        break;
    case DK_MUTEX_KEY1_PRIORITY:
        advanced_key_update_state(key, state);
        key0->key.report_state = key0->key.state;
        key1->key.report_state = key0->key.state ? false : key1->key.state;
        break;
    case DK_MUTEX_KEY2_PRIORITY:
        advanced_key_update_state(key, state);
        key0->key.report_state = key1->key.state ? false : key0->key.state;
        key1->key.report_state = key1->key.state;
        break;
    case DK_MUTEX_NEUTRAL:
        advanced_key_update_state(key, state);
        key0->key.report_state = key0->key.state;
        key1->key.report_state = key1->key.state;
        if (key0->key.state && key1->key.state)
        {
            key0->key.report_state = false;
            key1->key.report_state = false;
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
            key0->key.report_state = true;
            key1->key.report_state = true;
        }
    }
    call_event:
    if (key0->key.report_state && !last_key0_state)
    {
        keyboard_advanced_key_event_handler(key0, MK_EVENT(dynamic_key_m->key_binding[0], KEYBOARD_EVENT_KEY_DOWN, key));
    }
    if (!key0->key.report_state && last_key0_state)
    {
        keyboard_advanced_key_event_handler(key0, MK_EVENT(dynamic_key_m->key_binding[0], KEYBOARD_EVENT_KEY_UP, key));
    }
    if (key1->key.report_state && !last_key1_state)
    {
        keyboard_advanced_key_event_handler(key1, MK_EVENT(dynamic_key_m->key_binding[1], KEYBOARD_EVENT_KEY_DOWN, key));
    }
    if (!key1->key.report_state && last_key1_state)
    {
        keyboard_advanced_key_event_handler(key1, MK_EVENT(dynamic_key_m->key_binding[1], KEYBOARD_EVENT_KEY_UP, key));
    }
}