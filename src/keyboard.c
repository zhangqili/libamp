/*
 * Copyright (c) 2024 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "keyboard.h"
#include "layer.h"
#include "record.h"
#include "driver.h"
#include "packet.h"
#include "analog.h"

#include "stdio.h"
#include "string.h"
#include "math.h"

#ifdef RGB_ENABLE
#include "rgb.h"
#endif
#ifdef STORAGE_ENABLE
#include "storage.h"
#endif
#ifdef MOUSE_ENABLE
#include "mouse.h"
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
#include "process_midi.h"
#endif
#ifdef MACRO_ENABLE
#include "macro.h"
#endif
#ifdef ENCODER_ENABLE
#include "encoder.h"
#endif
#ifdef NEXUS_ENABLE
#include "nexus.h"
#endif
#ifdef SCRIPT_ENABLE
#include "script.h"
#endif
#include "event_buffer.h"

__WEAK AdvancedKey g_keyboard_advanced_keys[ADVANCED_KEY_NUM];
__WEAK Key g_keyboard_keys[KEY_NUM];
KeyboardLED g_keyboard_led_state;
__WEAK KeyboardConfig g_keyboard_config;
Keycode g_keymap[LAYER_NUM][TOTAL_KEY_NUM];

__WEAK const Keycode g_default_keymap[LAYER_NUM][TOTAL_KEY_NUM];

__WEAK volatile uint32_t g_keyboard_tick;
volatile bool g_keyboard_is_suspend;
__WEAK volatile KeyboardReportFlag g_keyboard_report_flags;

#ifdef NKRO_ENABLE
static Keyboard_NKROBuffer keyboard_nkro_buffer;
#endif
static Keyboard_6KROBuffer keyboard_6kro_buffer;

volatile uint32_t g_keyboard_bitmap[KEY_BITMAP_SIZE];


void keyboard_keycode_event_handler(KeyboardEvent event)
{
    switch (event.event)
    {
    case KEYBOARD_EVENT_KEY_DOWN:
        if (!event.is_virtual)
        {    
            keyboard_key_event_down_callback((Key*)event.key);
        }
        g_keyboard_report_flags.keyboard = true;
        break;
    case KEYBOARD_EVENT_KEY_TRUE:
        break;
    case KEYBOARD_EVENT_KEY_UP:
        g_keyboard_report_flags.keyboard = true;
        break;
    case KEYBOARD_EVENT_KEY_FALSE:
        break;
    default:
        break;
    }
}

void keyboard_event_handler(KeyboardEvent event)
{
#ifdef SCRIPT_ENABLE
    script_event_handler(event);
#endif
#ifdef MACRO_ENABLE
    macro_record_handler(event);
#endif
    if (!event.is_virtual)
    {
        layer_lock_handler(event);
    }
    switch (KEYCODE_GET_MAIN(event.keycode))
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
#ifdef MIDI_ENABLE
    case MIDI_COLLECTION:
    case MIDI_NOTE:
        midi_event_handler(event);
        break;
#endif
#ifdef MACRO_ENABLE
    case MACRO_COLLECTION:
        macro_event_handler(event);
        break;
#endif
#ifdef SCRIPT_ENABLE
    case SCRIPT_COLLECTION:
        script_event_handler(event);
        break;
#endif
    case LAYER_CONTROL:
        layer_event_handler(event);
        break;
    case KEYBOARD_OPERATION:
        keyboard_operation_event_handler(event);
        break;
    case KEY_USER:
        keyboard_user_event_handler(event);
        break;
    default:
        keyboard_keycode_event_handler(event);
        break;
    }
}

void keyboard_add_buffer(KeyboardEvent event)
{
    switch (KEYCODE_GET_MAIN(event.keycode))
    {
#ifdef MOUSE_ENABLE
    case MOUSE_COLLECTION:
        mouse_add_buffer(event);
        break;
#endif
#ifdef EXTRAKEY_ENABLE
    case CONSUMER_COLLECTION:
    case SYSTEM_COLLECTION:
        extra_key_add_buffer(event);
        break;
#endif
#ifdef JOYSTICK_ENABLE
    case JOYSTICK_COLLECTION:
        joystick_add_buffer(event);
        break;
#endif
    case LAYER_CONTROL:
        break;
    case KEYBOARD_OPERATION:
        break;
    case KEY_USER:
        break;
    default:
    {
        const uint8_t keycode = KEYCODE_GET_MAIN(event.keycode);
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
    }
    }
}

void keyboard_operation_event_handler(KeyboardEvent event)
{
    switch (event.event)
    {
    case KEYBOARD_EVENT_KEY_UP:
        break;
    case KEYBOARD_EVENT_KEY_DOWN:
        if (!event.is_virtual)
        {
            keyboard_key_event_down_callback((Key*)event.key);
        }
        uint8_t modifier = KEYCODE_GET_SUB(event.keycode);
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

void keyboard_key_event_down_callback(Key*key)
{
    if (IS_ADVANCED_KEY(key))
    {
#ifdef RGB_ENABLE
        rgb_activate(key->id);
#endif
#ifdef KPS_ENABLE
        record_kps_tick();
#endif
#ifdef COUNTER_ENABLE
        g_key_counts[key->id]++;
#endif
    }
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
    buf->modifier |= KEYCODE_GET_SUB(keycode);
    if (KEYCODE_GET_MAIN(keycode) != KEY_NO_EVENT && buf->keynum < 6)
    {
        buf->buffer[buf->keynum] = KEYCODE_GET_MAIN(keycode);
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
    if (KEYCODE_GET_MAIN(keycode) > NKRO_REPORT_BITS*8 )
    {
        return 1;
    }
    uint8_t index = KEYCODE_GET_MAIN(keycode)/8;
    buf->buffer[index] |= (1 << (KEYCODE_GET_MAIN(keycode)%8));
    buf->modifier |= KEYCODE_GET_SUB(keycode);
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
    g_keyboard_config.enable_report = true;
    for (int i = 0; i < ADVANCED_KEY_NUM; i++)
    {
        g_keyboard_advanced_keys[i].key.id = i;
    }
    for (int i = 0; i < KEY_NUM; i++)
    {
        g_keyboard_keys[i].id = ADVANCED_KEY_NUM + i;
    }
#ifdef STORAGE_ENABLE
    storage_mount();
    if (storage_check_version())
    {
        keyboard_factory_reset();
    }
    storage_read_config_index();
#endif
#ifdef RGB_ENABLE
    rgb_init();
#endif
#ifdef MIDI_ENABLE
    setup_midi();
#endif
#if defined(MACRO_ENABLE) || defined(SCRIPT_ENABLE)
    event_buffer_init();
#endif
#ifdef MACRO_ENABLE
    macro_init();
#endif
    keyboard_recovery();
#ifdef NEXUS_ENABLE
#if NEXUS_IS_SLAVE
    g_keyboard_config.enable_report = false;
#else
    nexus_init();
#endif
#endif
#ifdef SCRIPT_ENABLE
    script_init();
#endif
}

__WEAK void keyboard_reset_to_default(void)
{
    memcpy(g_keymap, g_default_keymap, sizeof(g_keymap));
    layer_cache_refresh();
    for (uint8_t i = 0; i < ADVANCED_KEY_NUM; i++)
    {
        g_keyboard_advanced_keys[i].config.mode = DEFAULT_ADVANCED_KEY_MODE;
        g_keyboard_advanced_keys[i].config.trigger_distance = A_ANTI_NORM(DEFAULT_TRIGGER_DISTANCE);
        g_keyboard_advanced_keys[i].config.release_distance = A_ANTI_NORM(DEFAULT_RELEASE_DISTANCE);
        g_keyboard_advanced_keys[i].config.activation_value = A_ANTI_NORM(DEFAULT_ACTIVATION_VALUE);
        g_keyboard_advanced_keys[i].config.deactivation_value = A_ANTI_NORM(DEFAULT_DEACTIVATION_VALUE);
        g_keyboard_advanced_keys[i].config.calibration_mode = DEFAULT_CALIBRATION_MODE;
        advanced_key_set_deadzone(g_keyboard_advanced_keys + i, 
            A_ANTI_NORM(DEFAULT_UPPER_DEADZONE), 
            A_ANTI_NORM(DEFAULT_LOWER_DEADZONE));
    }
#ifdef RGB_ENABLE
    rgb_factory_reset();
#endif
#ifdef DYNAMICKEY_ENABLE
    memset(g_dynamic_keys, 0, sizeof(g_dynamic_keys));
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
#ifndef OPTIMIZE_KEY_BITMAP
    for (int i = 0; i < ADVANCED_KEY_NUM; i++)
    {
        AdvancedKey*key = &g_keyboard_advanced_keys[i];
        if (key->key.report_state)
        {
            keyboard_add_buffer(MK_EVENT(layer_cache_get_keycode(key->key.id), KEYBOARD_EVENT_NO_EVENT, key));
        }
    }
    for (int i = 0; i < KEY_NUM; i++)
    {        
        Key*key = &g_keyboard_keys[i];
        if (key->report_state)
        {
            keyboard_add_buffer(MK_EVENT(layer_cache_get_keycode(key->id), KEYBOARD_EVENT_NO_EVENT, key));
        }
    }
#else
    for (uint16_t i = 0; i < KEY_BITMAP_SIZE; i++)
    {
        uint32_t block = g_keyboard_bitmap[i];
#ifdef __GNUC__
        while (block != 0)
        {
            int bit_index = __builtin_ctz(block);
            uint16_t id = i * 32 + bit_index;
            Key* key = keyboard_get_key(id);
            keyboard_add_buffer(MK_EVENT(layer_cache_get_keycode(id), KEYBOARD_EVENT_NO_EVENT, key));
            BIT_RESET(block, bit_index);
        }
#else
        if (!block)
            continue;
        for (int bit_index = 0; bit_index < 32; bit_index++)
        {
            if (block & BIT(bit_index))
            {
                uint16_t id = i * 32 + bit_index;
                if (id >= (TOTAL_KEY_NUM)) break;
                Key* key = keyboard_get_key(id);
                keyboard_add_buffer(MK_EVENT(layer_cache_get_keycode(id), KEYBOARD_EVENT_NO_EVENT, key));
            }
        }
#endif
    }
#endif
#ifdef DYNAMICKEY_ENABLE
    dynamic_key_add_buffer();
#endif
#if defined(MACRO_ENABLE) || defined(SCRIPT_ENABLE)
    event_buffer_add_buffer();
#endif
}

void keyboard_send_report(void)
{   
#ifdef MOUSE_ENABLE
    if (g_keyboard_report_flags.mouse)
    {
        if (!mouse_buffer_send())
        {
            g_keyboard_report_flags.mouse = false;
        }
    }
#endif
#ifdef EXTRAKEY_ENABLE
    if (g_keyboard_report_flags.consumer)
    {
        if (!consumer_key_buffer_send())
        {
            g_keyboard_report_flags.consumer = false;
        }
    }
    if (g_keyboard_report_flags.system)
    {
        if (!system_key_buffer_send())
        {
            g_keyboard_report_flags.system = false;
        }
    }
#endif
    if (g_keyboard_report_flags.keyboard)
    {
        if (!keyboard_buffer_send())
        {
            g_keyboard_report_flags.keyboard = false;
        }
    }
#ifdef JOYSTICK_ENABLE
    if (g_keyboard_report_flags.joystick)
    {
        if (!joystick_buffer_send())
        {
            g_keyboard_report_flags.joystick = false;
        }
    }
#endif
}

__WEAK void keyboard_task(void)
{
    keyboard_scan();
#ifdef ENCODER_ENABLE
    encoder_process();
#endif
#if defined(NEXUS_ENABLE) && NEXUS_IS_SLAVE
    for (uint16_t i = 0; i < ADVANCED_KEY_NUM; i++)
    {
        AdvancedKey*advanced_key = &g_keyboard_advanced_keys[i];
        keyboard_advanced_key_update_raw(advanced_key, advanced_key_read_raw(advanced_key));
    }
    if (g_keyboard_config.enable_report)
    {
        nexus_send_report();
    }
    return;
#else
#if defined(NEXUS_ENABLE)
    nexus_process();
#else
    for (uint16_t i = 0; i < ADVANCED_KEY_NUM; i++)
    {
        AdvancedKey*advanced_key = &g_keyboard_advanced_keys[i];
        keyboard_advanced_key_update_raw(advanced_key, advanced_key_read_raw(advanced_key));
    }
#endif
#ifdef SCRIPT_ENABLE
    script_process();
#endif
#ifdef MACRO_ENABLE
    macro_process();
#endif
#ifdef DYNAMICKEY_ENABLE
    dynamic_key_process();
#endif
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
        g_keyboard_report_flags.keyboard = true;
    }
    if (g_keyboard_config.enable_report && g_keyboard_report_flags.raw)
    {
        keyboard_clear_buffer();
        keyboard_fill_buffer();
        keyboard_send_report();
    }
#endif
}

__WEAK void keyboard_delay(uint32_t ms)
{
    UNUSED(ms);
}

bool keyboard_key_update(Key *key, bool state)
{
    bool changed = key_update(key, state);
    changed = keyboard_key_set_report_state(key, keyboard_key_debounce(key));
    keyboard_event_handler(MK_EVENT(layer_cache_get_keycode(key->id), changed | (key->report_state<<1), key));
    return changed;
}

bool keyboard_advanced_key_update(AdvancedKey *advanced_key, AnalogValue value)
{
    bool changed = advanced_key_update(advanced_key, value);
    changed = keyboard_key_set_report_state(&advanced_key->key, keyboard_key_debounce(&advanced_key->key));
    keyboard_event_handler(MK_EVENT(layer_cache_get_keycode(advanced_key->key.id), changed | (advanced_key->key.report_state<<1), advanced_key));
    return changed;
}

bool keyboard_advanced_key_update_raw(AdvancedKey *advanced_key, AnalogRawValue raw)
{
    bool changed = advanced_key_update_raw(advanced_key, raw);
    changed = keyboard_key_set_report_state(&advanced_key->key, keyboard_key_debounce(&advanced_key->key));
    keyboard_event_handler(MK_EVENT(layer_cache_get_keycode(advanced_key->key.id), changed | (advanced_key->key.report_state<<1), advanced_key));
    return changed;
}
