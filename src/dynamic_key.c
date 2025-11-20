/*
 * Copyright (c) 2024 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "dynamic_key.h"
#include "layer.h"

#define DK_TAP_DURATION 5

#define DYNAMIC_KEY_NOT_MATCH(dynamic_key, key) (KEYCODE_GET_MAIN(layer_cache_get_keycode((key)->id)) != DYNAMIC_KEY || \
        &g_dynamic_keys[KEYCODE_GET_SUB(layer_cache_get_keycode((key)->id))] != ((DynamicKey*)(dynamic_key)))

#ifdef DYNAMICKEY_ENABLE
DynamicKey g_dynamic_keys[DYNAMIC_KEY_NUM];
#endif

void dynamic_key_process(void)
{
    for (int i = 0; i < DYNAMIC_KEY_NUM && g_dynamic_keys[i].type != DYNAMIC_KEY_NONE; i++)
    {
        DynamicKey * dynamic_key = &g_dynamic_keys[i];
        switch (dynamic_key->type)
        {
        case DYNAMIC_KEY_STROKE:
            dynamic_key_s_process((DynamicKeyStroke4x4*)dynamic_key);
            break;
        case DYNAMIC_KEY_MOD_TAP:
            dynamic_key_mt_process((DynamicKeyModTap*)dynamic_key);
            break;
        case DYNAMIC_KEY_TOGGLE_KEY:
            dynamic_key_tk_process((DynamicKeyToggleKey*)dynamic_key);
            break;
        case DYNAMIC_KEY_MUTEX:
            dynamic_key_m_process((DynamicKeyMutex*)dynamic_key);
            break;
        default:
            break;
        }
    }
}

void _dynamic_key_add_buffer(DynamicKey*dynamic_key)
{
    switch (dynamic_key->type)
    {
    case DYNAMIC_KEY_STROKE:
        DynamicKeyStroke4x4*dynamic_key_s=(DynamicKeyStroke4x4*)dynamic_key;
        for (int i = 0; i < 4; i++)
        {
            if (BIT_GET(dynamic_key_s->key_state,i))
                keyboard_add_buffer(MK_EVENT(dynamic_key_s->key_binding[i], KEYBOARD_EVENT_NO_EVENT, keyboard_get_key(dynamic_key_s->key_id)));
        }
        break;
    case DYNAMIC_KEY_MOD_TAP:
        DynamicKeyModTap*dynamic_key_mt=(DynamicKeyModTap*)dynamic_key;
        if (dynamic_key_mt->key_report_state)
        {
            keyboard_add_buffer(MK_EVENT(dynamic_key_mt->key_binding[dynamic_key_mt->state], KEYBOARD_EVENT_NO_EVENT, keyboard_get_key(dynamic_key_mt->key_id)));
        }
        break;
    case DYNAMIC_KEY_TOGGLE_KEY:
        DynamicKeyToggleKey*dynamic_key_tk=(DynamicKeyToggleKey*)dynamic_key;
        if (dynamic_key_tk->state)
        {
            keyboard_add_buffer(MK_EVENT(dynamic_key_tk->key_binding, KEYBOARD_EVENT_NO_EVENT, keyboard_get_key(dynamic_key_tk->key_id)));
        }
        break;
    case DYNAMIC_KEY_MUTEX:
        {
            DynamicKeyMutex*dynamic_key_m=(DynamicKeyMutex*)dynamic_key;
            if (dynamic_key_m->key_report_state[0])
                keyboard_add_buffer(MK_EVENT(dynamic_key_m->key_binding[0], KEYBOARD_EVENT_NO_EVENT,  keyboard_get_key(dynamic_key_m->key_id[0])));
            if (dynamic_key_m->key_report_state[1])
                keyboard_add_buffer(MK_EVENT(dynamic_key_m->key_binding[1], KEYBOARD_EVENT_NO_EVENT,  keyboard_get_key(dynamic_key_m->key_id[1])));
        }
        break;
    default:
        break;
    }
}

void dynamic_key_add_buffer(void)
{
    for (int i = 0; i < DYNAMIC_KEY_NUM && g_dynamic_keys[i].type != DYNAMIC_KEY_NONE; i++)
    {
        DynamicKey*dynamic_key = &g_dynamic_keys[i];
        _dynamic_key_add_buffer(dynamic_key);
    }
}

#define DKS_PRESS_BEGIN 0
#define DKS_PRESS_FULLY 2
#define DKS_RELEASE_BEGIN 4
#define DKS_RELEASE_FULLY 6
#define DKS_GET_KEY_CONTROL(key_ctrl, n) (((key_ctrl) >> (n)) & 0x03)
void dynamic_key_s_process(DynamicKeyStroke4x4*dynamic_key)
{
    DynamicKeyStroke4x4*dynamic_key_s=(DynamicKeyStroke4x4*)dynamic_key;
    Key * key = (Key*)keyboard_get_key(dynamic_key_s->key_id);
    if (DYNAMIC_KEY_NOT_MATCH(dynamic_key,key))
    {
        return;
    }
    AnalogValue last_value = dynamic_key_s->value;
    AnalogValue current_value = KEYBOARD_GET_KEY_ANALOG_VALUE(key);
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
        keyboard_event_handler(MK_EVENT(dynamic_key_s->key_binding[i], 
            CALC_EVENT(BIT_GET(last_key_state, i), BIT_GET(dynamic_key_s->key_state, i)), key));
    }
    KEYBOARD_KEY_SET_REPORT_STATE(key, dynamic_key_s->key_state > 0);
    dynamic_key_s->value = current_value;
}

void dynamic_key_mt_process(DynamicKeyModTap*dynamic_key)
{
    DynamicKeyModTap*dynamic_key_mt=(DynamicKeyModTap*)dynamic_key;
    Key * key = keyboard_get_key(dynamic_key_mt->key_id);
    if (DYNAMIC_KEY_NOT_MATCH(dynamic_key,key))
    {
        return;
    }
    bool last_report_state = dynamic_key_mt->key_report_state;
    bool next_report_state = dynamic_key_mt->key_report_state;
    if (!dynamic_key_mt->key_state && key->state)
    {
        dynamic_key_mt->begin_time = g_keyboard_tick;
    }
    if (dynamic_key_mt->key_state && !key->state)
    {
        if (g_keyboard_tick - dynamic_key_mt->begin_time < dynamic_key_mt->duration)
        {
            dynamic_key_mt->end_time = g_keyboard_tick+DK_TAP_DURATION;
            dynamic_key_mt->state = DYNAMIC_KEY_ACTION_TAP;
            next_report_state = true;
        }
        else
        {
            next_report_state = false;
        }
        dynamic_key_mt->begin_time = g_keyboard_tick;
    }
    if (key->state && !last_report_state && (g_keyboard_tick - dynamic_key_mt->begin_time > dynamic_key_mt->duration))
    {
        dynamic_key_mt->end_time = 0xFFFFFFFF;
        dynamic_key_mt->state = DYNAMIC_KEY_ACTION_HOLD;
        next_report_state = true;
    }
    if (g_keyboard_tick > dynamic_key_mt->end_time && last_report_state)
    {
        next_report_state = false;
    }
    keyboard_event_handler(MK_EVENT(dynamic_key_mt->key_binding[DYNAMIC_KEY_ACTION_TAP], 
        CALC_EVENT(dynamic_key_mt->state == DYNAMIC_KEY_ACTION_TAP && last_report_state, dynamic_key_mt->state == DYNAMIC_KEY_ACTION_TAP && next_report_state), key));
    keyboard_event_handler(MK_EVENT(dynamic_key_mt->key_binding[DYNAMIC_KEY_ACTION_HOLD], 
        CALC_EVENT(dynamic_key_mt->state == DYNAMIC_KEY_ACTION_HOLD && last_report_state, dynamic_key_mt->state == DYNAMIC_KEY_ACTION_HOLD && next_report_state), key));
    dynamic_key_mt->key_state = key->state;
    dynamic_key_mt->key_report_state = next_report_state;
    KEYBOARD_KEY_SET_REPORT_STATE(key, next_report_state);
}

void dynamic_key_tk_process(DynamicKeyToggleKey*dynamic_key)
{
    DynamicKeyToggleKey*dynamic_key_tk=(DynamicKeyToggleKey*)dynamic_key;
    Key * key = keyboard_get_key(dynamic_key_tk->key_id);
    if (DYNAMIC_KEY_NOT_MATCH(dynamic_key,key))
    {
        return;
    }
    bool next_state = dynamic_key_tk->state;
    if (!dynamic_key_tk->key_state && key->state)
    {
        next_state = !dynamic_key_tk->state;
    }
    keyboard_event_handler(MK_EVENT(dynamic_key_tk->key_binding, CALC_EVENT(dynamic_key_tk->state, next_state), key));
    dynamic_key_tk->key_state = key->state;
    dynamic_key_tk->state = next_state;
}

void dynamic_key_m_process(DynamicKeyMutex*dynamic_key)
{
    DynamicKeyMutex*dynamic_key_m=(DynamicKeyMutex*)dynamic_key;
    Key*key0 = (Key*)keyboard_get_key(dynamic_key_m->key_id[0]);
    Key*key1 = (Key*)keyboard_get_key(dynamic_key_m->key_id[1]);
    if (DYNAMIC_KEY_NOT_MATCH(dynamic_key,key0))
    {
        return;
    }
    if (DYNAMIC_KEY_NOT_MATCH(dynamic_key,key1))
    {
        return;
    }
    bool next_key0_report_state = dynamic_key_m->key_report_state[0];
    bool next_key1_report_state = dynamic_key_m->key_report_state[1];

    if ((dynamic_key_m->mode & 0x0F) == DK_MUTEX_DISTANCE_PRIORITY)
    {
        if (!IS_ADVANCED_KEY(key0) || !IS_ADVANCED_KEY(key1))
        {
            goto call_event;
        }
        AdvancedKey*advanced_key0 = (AdvancedKey*)key0;
        AdvancedKey*advanced_key1 = (AdvancedKey*)key1;
        if (advanced_key0->value > advanced_key1->value)
        {
            next_key0_report_state = true;
            next_key1_report_state = false;
        }
        if (advanced_key0->value < advanced_key1->value)
        {
            next_key0_report_state = false;
            next_key1_report_state = true;
        }
        if (advanced_key0->value < advanced_key0->config.upper_deadzone)
        {
            next_key0_report_state = false;
        }
        if (advanced_key1->value < advanced_key1->config.upper_deadzone)
        {
            next_key1_report_state = false;
        }
        //advanced_key_update_state(key0, key0_state);
        //advanced_key_update_state(key1, key1_state);
        goto call_event;
    }

    switch (dynamic_key_m->mode & 0x0F)
    {
    case DK_MUTEX_LAST_PRIORITY:
        if (!dynamic_key_m->key_state[0] && key0->state)
        {
            next_key0_report_state = true;
            next_key1_report_state = false;
        }
        if (dynamic_key_m->key_state[0] && !key0->state)
        {
            next_key0_report_state = false;
            next_key1_report_state = key1->state;
        }
        if (!dynamic_key_m->key_state[1] && key1->state)
        {
            next_key0_report_state = false;
            next_key1_report_state = true;
        }
        if (dynamic_key_m->key_state[1] && !key1->state)
        {
            next_key0_report_state = key0->state;
            next_key1_report_state = false;
        }
        break;
    case DK_MUTEX_KEY1_PRIORITY:
        next_key0_report_state = key0->state;
        next_key1_report_state = key0->state ? false : key1->state;
        break;
    case DK_MUTEX_KEY2_PRIORITY:
        next_key0_report_state = key1->state ? false : key0->state;
        next_key1_report_state = key1->state;
        break;
    case DK_MUTEX_NEUTRAL:
        next_key0_report_state = key0->state;
        next_key1_report_state = key1->state;
        if (key0->state && key1->state)
        {
            next_key0_report_state = false;
            next_key1_report_state = false;
        }
        break;
    default:
        break;
    }
    call_event:
    if (dynamic_key_m->mode & 0xF0)
    {
        if (IS_ADVANCED_KEY(key0) && IS_ADVANCED_KEY(key1))
        {        
            AdvancedKey*advanced_key0 = (AdvancedKey*)key0;
            AdvancedKey*advanced_key1 = (AdvancedKey*)key1;
            if ((advanced_key0->value>= (ANALOG_VALUE_MAX - advanced_key0->config.lower_deadzone))&&
            (advanced_key1->value>= (ANALOG_VALUE_MAX - advanced_key1->config.lower_deadzone)))
            {
                next_key0_report_state = true;
                next_key1_report_state = true;
            }
        }
    }
    keyboard_event_handler(MK_EVENT(dynamic_key_m->key_binding[0], CALC_EVENT(dynamic_key_m->key_report_state[0], next_key0_report_state), key0));
    keyboard_event_handler(MK_EVENT(dynamic_key_m->key_binding[1], CALC_EVENT(dynamic_key_m->key_report_state[1], next_key1_report_state), key1));
    dynamic_key_m->key_state[0] = key0->state;
    dynamic_key_m->key_state[1] = key1->state;
    dynamic_key_m->key_report_state[0] = next_key0_report_state;
    dynamic_key_m->key_report_state[1] = next_key1_report_state;
}
