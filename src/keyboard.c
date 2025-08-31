/*
 * Copyright (c) 2024 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "keyboard.h"
#include "analog.h"
#include "keyboard_conf.h"
#include "mouse.h"
#include "layer.h"
#include "record.h"
#include "process_midi.h"
#include "driver.h"
#include "packet.h"

#include "stdio.h"
#include "string.h"
#include "math.h"

#ifdef RGB_ENABLE
#include "rgb.h"
#endif
#ifdef STORAGE_ENABLE
#include "storage.h"
#endif
#ifdef DYNAMICKEY_ENABLE
#include "dynamic_key.h"
#endif
#ifdef EXTRAKEY_ENABLE
#include "extra_key.h"
#endif
#ifdef JOYSTICK_ENABLE
#include "joystick.h"
#endif
#ifdef MIDI_ENABLE
#include "qmk_midi.h"
#endif

__WEAK const Keycode g_default_keymap[LAYER_NUM][ADVANCED_KEY_NUM + KEY_NUM];
__WEAK AdvancedKey g_keyboard_advanced_keys[ADVANCED_KEY_NUM];
__WEAK Key g_keyboard_keys[KEY_NUM];

uint16_t g_keymap[LAYER_NUM][ADVANCED_KEY_NUM + KEY_NUM];

KeyboardLED g_keyboard_led_state;

uint32_t g_keyboard_tick;

uint8_t g_keyboard_knob_flag;
volatile bool g_keyboard_send_report_enable = true;
volatile KeyboardConfig g_keyboard_config;

volatile bool g_keyboard_is_suspend;
volatile KeyboardReportFlag g_keyboard_report_flags;

#ifdef NKRO_ENABLE
static Keyboard_NKROBuffer keyboard_nkro_buffer;
#endif
static Keyboard_6KROBuffer keyboard_6kro_buffer;

void keyboard_event_handler(KeyboardEvent event)
{
    switch (event.event)
    {
    case KEYBOARD_EVENT_KEY_DOWN:
        layer_lock(((Key*)event.key)->id);
        break;
    case KEYBOARD_EVENT_KEY_UP:
        layer_unlock(((Key*)event.key)->id);
        break;
    case KEYBOARD_EVENT_KEY_TRUE:
        break;
    case KEYBOARD_EVENT_KEY_FALSE:
        break;
    default:
        break;
    }
    switch (KEYCODE(event.keycode))
    {
#ifdef MOUSE_ENABLE
    case MOUSE_COLLECTION:
        mouse_event_handler(event);
        break;
#endif
#ifdef EXTRAKEY_ENABLE
    case CONSUMER_COLLECTION:
    case SYSTEM_COLLECTION:
        extra_key_event_handler(event);
        break;
#endif
#ifdef JOYSTICK_ENABLE
    case JOYSTICK_COLLECTION:
        joystick_event_handler(event);
        break;
#endif
#ifdef DYNAMICKEY_ENABLE
    case DYNAMIC_KEY:
        dynamic_key_event_handler(event);
        break;
#endif
    case LAYER_CONTROL:
        layer_control(event);
        break;
    case KEYBOARD_OPERATION:
        keyboard_operation_event_handler(event);
        break;
    case KEY_USER:
        keyboard_user_event_handler(event);
        break;
        
    default:
        switch (event.event)
        {
        case KEYBOARD_EVENT_KEY_UP:
        case KEYBOARD_EVENT_KEY_DOWN:
            KEYBOARD_REPORT_FLAG_SET(KEYBOARD_REPORT_FLAG);
            break;
        case KEYBOARD_EVENT_KEY_TRUE:
            const uint8_t keycode = KEYCODE(event.keycode);
            if (keycode <= KEY_EXSEL)
            {
#ifdef NKRO_ENABLE
                if (g_keyboard_config.nkro)
                {
                    keyboard_NKRObuffer_add(&keyboard_nkro_buffer, event.keycode);
                }
                else
#endif
                {
                    keyboard_6KRObuffer_add(&keyboard_6kro_buffer, event.keycode);
                }
            }
            break;
        case KEYBOARD_EVENT_KEY_FALSE:
            break;
        default:
            break;
        }
        break;
    }
}

void keyboard_operation_event_handler(KeyboardEvent event)
{
    switch (event.event)
    {
    case KEYBOARD_EVENT_KEY_UP:
        break;
    case KEYBOARD_EVENT_KEY_DOWN:
        uint8_t modifier = MODIFIER(event.keycode);
        if ((modifier & 0x3F) < KEYBOARD_CONFIG_BASE)
        {
            switch (modifier & 0x3F)
            {
            case KEYBOARD_REBOOT:
                keyboard_reboot();
                break;
            case KEYBOARD_FACTORY_RESET:
                keyboard_factory_reset();
                break;
            case KEYBOARD_SAVE:
                keyboard_save();
                break;
            case KEYBOARD_BOOTLOADER:
                keyboard_jump_to_bootloader();
                break;
            case KEYBOARD_RESET_TO_DEFAULT:
                keyboard_reset_to_default();
                break;
#ifdef RGB_ENABLE
            case KEYBOARD_RGB_BRIGHTNESS_UP:
                if ((int16_t)g_rgb_base_config.brightness + 16 < 255)
                {
                    g_rgb_base_config.brightness+=16;
                }
                else
                {
                    g_rgb_base_config.brightness = 255;
                }
                break;
            case KEYBOARD_RGB_BRIGHTNESS_DOWN:
                if ((int16_t)g_rgb_base_config.brightness - 16 > 0)
                {
                    g_rgb_base_config.brightness-=16;
                }
                else
                {
                    g_rgb_base_config.brightness = 0;
                }
                break;
#endif
            case KEYBOARD_CONFIG0:
            case KEYBOARD_CONFIG1:
            case KEYBOARD_CONFIG2:
            case KEYBOARD_CONFIG3:
                keyboard_set_config_index((event.keycode >> 8) & 0x0F);
                break;
            default:
                break;
            }
        }
        else
        {
            switch ((modifier >> 6) & 0x03)
            {
            case 0:
                BIT_RESET(*((uint8_t*)&g_keyboard_config), ((modifier & 0x3F) - KEYBOARD_CONFIG_BASE));
                break;
            case 1:
                BIT_SET(*((uint8_t*)&g_keyboard_config), ((modifier & 0x3F) - KEYBOARD_CONFIG_BASE));
                break;
            case 2:
                BIT_TOGGLE(*((uint8_t*)&g_keyboard_config), ((modifier & 0x3F) - KEYBOARD_CONFIG_BASE));
                break;
            default:
                break;
            }  
        }
        break;
    case KEYBOARD_EVENT_KEY_TRUE:
        break;
    case KEYBOARD_EVENT_KEY_FALSE:
        break;
    default:
        break;
    }
}

void keyboard_advanced_key_event_handler(AdvancedKey*key, KeyboardEvent event)
{
    switch (event.event)
    {
    case KEYBOARD_EVENT_KEY_DOWN:
#ifdef RGB_ENABLE
        rgb_activate(key->key.id);
#endif
#ifdef KPS_ENABLE
        record_kps_tick();
#endif
#ifdef COUNTER_ENABLE
        g_key_counts[key->key.id]++;
#endif
        break;
    case KEYBOARD_EVENT_KEY_UP:
        break;
    case KEYBOARD_EVENT_KEY_TRUE:
        break;
    case KEYBOARD_EVENT_KEY_FALSE:
        break;
    default:
        break;
    }
    keyboard_event_handler(event);
}

int keyboard_buffer_send(void)
{
#ifdef NKRO_ENABLE
    if (g_keyboard_config.nkro)
    {
        keyboard_nkro_buffer.report_id = REPORT_ID_NKRO;
        if (g_keyboard_config.winlock)
        {
            keyboard_nkro_buffer.modifier &= (~(KEY_LEFT_GUI | KEY_RIGHT_GUI)); 
        }
        return keyboard_NKRObuffer_send(&keyboard_nkro_buffer);
    }
#endif
    if (g_keyboard_config.winlock)
    {
        keyboard_6kro_buffer.modifier &= (~(KEY_LEFT_GUI | KEY_RIGHT_GUI)); 
    }
#ifdef KEYBOARD_SHARED_EP
    keyboard_6kro_buffer.report_id = REPORT_ID_KEYBOARD;
#endif
    return keyboard_6KRObuffer_send(&keyboard_6kro_buffer);
}

void keyboard_clear_buffer(void)
{
#ifdef NKRO_ENABLE
    if (g_keyboard_config.nkro)
    {
        keyboard_NKRObuffer_clear(&keyboard_nkro_buffer);
    }
#endif
    keyboard_6KRObuffer_clear(&keyboard_6kro_buffer);
#ifdef MOUSE_ENABLE
    mouse_buffer_clear();
#endif
#ifdef JOYSTICK_ENABLE
    joystick_buffer_clear();
#endif
}

int keyboard_6KRObuffer_add(Keyboard_6KROBuffer *buf, Keycode keycode)
{
    buf->modifier |= MODIFIER(keycode);
    if (KEYCODE(keycode) != KEY_NO_EVENT && buf->keynum < 6)
    {
        buf->buffer[buf->keynum] = KEYCODE(keycode);
        buf->keynum++;
        return 0;
    }
    else
    {
        return 1;
    }
}

int keyboard_6KRObuffer_send(Keyboard_6KROBuffer* buf)
{
#ifdef KEYBOARD_SHARED_EP
    return hid_send_shared_ep((uint8_t*)buf, offsetof(Keyboard_6KROBuffer, keynum));
#else
    return hid_send_keyboard((uint8_t*)buf, offsetof(Keyboard_6KROBuffer, keynum));
#endif
}

void keyboard_6KRObuffer_clear(Keyboard_6KROBuffer* buf)
{
    memset(buf, 0, sizeof(Keyboard_6KROBuffer));
}

int keyboard_NKRObuffer_add(Keyboard_NKROBuffer*buf,Keycode keycode)
{
    if (KEYCODE(keycode) > NKRO_REPORT_BITS*8 )
    {
        return 1;
    }
    uint8_t index = KEYCODE(keycode)/8;
    buf->buffer[index] |= (1 << (KEYCODE(keycode)%8));
    buf->modifier |= MODIFIER(keycode);
    return 0;
}

int keyboard_NKRObuffer_send(Keyboard_NKROBuffer*buf)
{
    return hid_send_nkro((uint8_t*)buf, sizeof(Keyboard_NKROBuffer));
}

void keyboard_NKRObuffer_clear(Keyboard_NKROBuffer*buf)
{
    memset(buf, 0, sizeof(Keyboard_NKROBuffer));
}

void keyboard_init(void)
{
    g_keyboard_tick = 0;
#ifdef STORAGE_ENABLE
    storage_mount();
    storage_read_config_index();
#endif
#ifdef RGB_ENABLE
    rgb_init();
#endif
#ifdef MIDI_ENABLE
    setup_midi();
#endif
    keyboard_recovery();
}

__WEAK void keyboard_reset_to_default(void)
{
    memcpy(g_keymap, g_default_keymap, sizeof(g_keymap));
    layer_cache_refresh();
    for (uint8_t i = 0; i < ADVANCED_KEY_NUM; i++)
    {
        g_keyboard_advanced_keys[i].config.mode = DEFAULT_ADVANCED_KEY_MODE;
        g_keyboard_advanced_keys[i].config.trigger_distance = A_ANIT_NORM(DEFAULT_TRIGGER_DISTANCE);
        g_keyboard_advanced_keys[i].config.release_distance = A_ANIT_NORM(DEFAULT_RELEASE_DISTANCE);
        g_keyboard_advanced_keys[i].config.activation_value = A_ANIT_NORM(DEFAULT_ACTIVATION_VALUE);
        g_keyboard_advanced_keys[i].config.deactivation_value = A_ANIT_NORM(DEFAULT_DEACTIVATION_VALUE);
        g_keyboard_advanced_keys[i].config.calibration_mode = DEFAULT_CALIBRATION_MODE;
        advanced_key_set_deadzone(g_keyboard_advanced_keys + i, 
            A_ANIT_NORM(DEFAULT_UPPER_DEADZONE), 
            A_ANIT_NORM(DEFAULT_LOWER_DEADZONE));
    }
#ifdef RGB_ENABLE
    rgb_factory_reset();
#endif
#ifdef DYNAMICKEY_ENABLE
    memset(g_keyboard_dynamic_keys, 0, sizeof(g_keyboard_dynamic_keys));
#endif
}

void keyboard_factory_reset(void)
{
    keyboard_reset_to_default();
#ifdef STORAGE_ENABLE
    for (int i = 0; i < STORAGE_CONFIG_FILE_NUM; i++)
    {
        g_current_config_index = i;
        storage_save_config();
    }
    g_current_config_index = 0;
    keyboard_set_config_index(0);
#endif
}

__WEAK void keyboard_reboot(void)
{
}

__WEAK void keyboard_jump_to_bootloader(void)
{
}

__WEAK void keyboard_user_event_handler(KeyboardEvent event)
{
    UNUSED(event);
}


__WEAK void keyboard_scan(void)
{

}

void keyboard_recovery(void)
{
#ifdef STORAGE_ENABLE
    storage_read_config();
#else
    keyboard_reset_to_default();
#endif
}

void keyboard_save(void)
{
#ifdef STORAGE_ENABLE
    storage_save_config();
#endif
}

void keyboard_set_config_index(uint8_t index)
{
#ifdef STORAGE_ENABLE
    if (index < STORAGE_CONFIG_FILE_NUM)
    {
        g_current_config_index = index;
    }
    storage_save_config_index();
#else
    UNUSED(index);
#endif

    keyboard_recovery();
}

void keyboard_fill_buffer(void)
{
    for (int i = 0; i < ADVANCED_KEY_NUM; i++)
    {
        keyboard_advanced_key_event_handler(&g_keyboard_advanced_keys[i], 
            MK_EVENT(layer_cache_get_keycode(g_keyboard_advanced_keys[i].key.id), 
            g_keyboard_advanced_keys[i].key.report_state ? KEYBOARD_EVENT_KEY_TRUE : KEYBOARD_EVENT_KEY_FALSE,
            &g_keyboard_advanced_keys[i]));
    }
    for (int i = 0; i < KEY_NUM; i++)
    {        
        keyboard_event_handler(MK_EVENT(layer_cache_get_keycode(g_keyboard_keys[i].id), 
            g_keyboard_keys[i].report_state ? KEYBOARD_EVENT_KEY_TRUE : KEYBOARD_EVENT_KEY_FALSE, &g_keyboard_keys[i]));
    }
}

void keyboard_send_report(void)
{   
#ifdef MOUSE_ENABLE
    if (KEYBOARD_REPORT_FLAG_GET(MOUSE_REPORT_FLAG))
    {
        if (!mouse_buffer_send())
        {
            KEYBOARD_REPORT_FLAG_CLEAR(MOUSE_REPORT_FLAG);
        }
    }
#endif
#ifdef EXTRAKEY_ENABLE
    if (KEYBOARD_REPORT_FLAG_GET(CONSUMER_REPORT_FLAG))
    {
        if (!consumer_key_buffer_send())
        {
            KEYBOARD_REPORT_FLAG_CLEAR(CONSUMER_REPORT_FLAG);
        }
    }
    if (KEYBOARD_REPORT_FLAG_GET(SYSTEM_REPORT_FLAG))
    {
        if (!system_key_buffer_send())
        {
            KEYBOARD_REPORT_FLAG_CLEAR(SYSTEM_REPORT_FLAG);
        }
    }
#endif
    if (KEYBOARD_REPORT_FLAG_GET(KEYBOARD_REPORT_FLAG))
    {
        if (!keyboard_buffer_send())
        {
            KEYBOARD_REPORT_FLAG_CLEAR(KEYBOARD_REPORT_FLAG);
        }
    }
#ifdef JOYSTICK_ENABLE
    if (KEYBOARD_REPORT_FLAG_GET(JOYSTICK_REPORT_FLAG))
    {
        if (!joystick_buffer_send())
        {
            KEYBOARD_REPORT_FLAG_CLEAR(JOYSTICK_REPORT_FLAG);
        }
    }
#endif
}

__WEAK void keyboard_task(void)
{
    keyboard_scan();
    analog_check();
#ifdef SUSPEND_ENABLE
    if (g_keyboard_is_suspend)
    {
        if (g_keyboard_report_flags.raw)
        {
            g_keyboard_is_suspend = false;
            send_remote_wakeup();
        }
        else
        {
            return;
        }
    }
#endif
    if (g_keyboard_config.continous_poll)
    {
        KEYBOARD_REPORT_FLAG_SET(KEYBOARD_REPORT_FLAG);
    }
    if (g_keyboard_send_report_enable)
    {
        keyboard_clear_buffer();
        keyboard_fill_buffer();
        keyboard_send_report();
    }
}

__WEAK void keyboard_delay(uint32_t ms)
{
    UNUSED(ms);
}

void keyboard_key_update(Key *key, bool state)
{
    if (!key->state && state)
    {
        keyboard_event_handler(MK_EVENT(layer_cache_get_keycode(key->id), KEYBOARD_EVENT_KEY_DOWN, key));
    }
    if (key->state && !state)
    {
        keyboard_event_handler(MK_EVENT(layer_cache_get_keycode(key->id), KEYBOARD_EVENT_KEY_UP, key));
    }
    key_update(key, state);
    key->report_state = state;
}

void keyboard_advanced_key_update_state(AdvancedKey *key, bool state)
{
    const Keycode keycode = layer_cache_get_keycode(key->key.id);
    switch (KEYCODE(keycode))
    {
#ifdef DYNAMICKEY_ENABLE
    case DYNAMIC_KEY:
        const uint8_t dynamic_key_index = (keycode>>8)&0xFF;
        dynamic_key_update(&g_keyboard_dynamic_keys[dynamic_key_index], key, state);
        break;
#endif
#ifdef MIDI_ENABLE
    case MIDI_COLLECTION:
    case MIDI_NOTE:
        if (!key->key.state && state)
        {
            KeyboardEvent event = MK_EVENT(layer_cache_get_keycode(key->key.id), KEYBOARD_EVENT_KEY_DOWN, key);
            midi_event_handler(event);
            keyboard_advanced_key_event_handler(key,event);
        }
        if (key->key.state && !state)
        {
            KeyboardEvent event = MK_EVENT(layer_cache_get_keycode(key->key.id), KEYBOARD_EVENT_KEY_UP, key);
            midi_event_handler(event);
            keyboard_advanced_key_event_handler(key,event);
        }
        advanced_key_update_state(key, state);
        key->key.report_state = state;
        break;
#endif
#ifdef MOUSE_ENABLE
    case MOUSE_COLLECTION:
        if (MODIFIER(keycode) & 0xF0)
        {
            KEYBOARD_REPORT_FLAG_SET(MOUSE_REPORT_FLAG);
            key->key.report_state = true;
            break;
        }
        goto default_condition;
#endif
#ifdef JOYSTICK_ENABLE
    case JOYSTICK_COLLECTION:
        if (MODIFIER(keycode) & 0xE0)
        {
            KEYBOARD_REPORT_FLAG_SET(JOYSTICK_REPORT_FLAG);
            key->key.report_state = true;
            break;
        }
#endif
        goto default_condition;
    default:
        default_condition:
        if (!key->key.state && state)
        {
            keyboard_advanced_key_event_handler(key,
                MK_EVENT(layer_cache_get_keycode(key->key.id), KEYBOARD_EVENT_KEY_DOWN, key));
        }
        if (key->key.state && !state)
        {
            keyboard_advanced_key_event_handler(key,
                MK_EVENT(layer_cache_get_keycode(key->key.id), KEYBOARD_EVENT_KEY_UP, key));
        }
        advanced_key_update_state(key, state);
        key->key.report_state = state;
        break;
    }
}