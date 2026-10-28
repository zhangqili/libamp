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
#include "packet_buffer.h"
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
#include "midi.h"
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
#ifdef GAMEPAD_ENABLE
#include "gamepad.h"
#endif
#ifdef CONSOLE_ENABLE
#include "console.h"
#endif
#include "event_cache.h"
#include "event_buffer.h"

__WEAK AdvancedKey g_keyboard_advanced_keys[ADVANCED_KEY_NUM];
__WEAK Key g_keyboard_keys[KEY_NUM];
KeyboardLEDState g_keyboard_led_state;
__WEAK volatile KeyboardConfig g_keyboard_config;
Keycode g_keymap[LAYER_NUM][TOTAL_KEY_NUM];

__WEAK const Keycode g_default_keymap[LAYER_NUM][TOTAL_KEY_NUM];

volatile bool g_keyboard_is_suspend;
__WEAK volatile KeyboardReportFlag g_keyboard_report_flags;

volatile uint32_t g_keyboard_bitmap[KEY_BITMAP_SIZE];

static uint32_t target_calibration_tick;
static bool calibration_pending;

static EventLoopQueue event_buffer;
static EventLoopQueueElm event_buffers[EVENT_BUFFER_LENGTH];


#ifdef NKRO_ENABLE
static KeyboardNKROReport keyboard_nkro_buffer;
#endif
static Keyboard6KROReport keyboard_6kro_buffer;

static int keyboard_6kro_report_add(Keyboard6KROReport *buf, Keycode keycode);
static int keyboard_6kro_report_send(Keyboard6KROReport *buf);
static void keyboard_6kro_report_clear(Keyboard6KROReport *buf);

static int keyboard_nkro_report_add(KeyboardNKROReport*buf,Keycode keycode);
static int keyboard_nkro_report_send(KeyboardNKROReport*buf);
static void keyboard_nkro_report_clear(KeyboardNKROReport*buf);


static int keyboard_6kro_report_add(Keyboard6KROReport *buf, Keycode keycode)
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

static int keyboard_6kro_report_send(Keyboard6KROReport* buf)
{
    return hid_send_keyboard((uint8_t*)buf, offsetof(Keyboard6KROReport, keynum));
}

static void keyboard_6kro_report_clear(Keyboard6KROReport* buf)
{
    memset(buf, 0, sizeof(Keyboard6KROReport));
}

static int keyboard_nkro_report_add(KeyboardNKROReport*buf,Keycode keycode)
{
    const uint8_t key = KEYCODE_GET_MAIN(keycode);
    if (key >= NKRO_REPORT_BITS * 8)
    {
        return 1;
    }
    buf->buffer[key / 8] |= (uint8_t)(1U << (key % 8));
    buf->modifier |= KEYCODE_GET_SUB(keycode);
    return 0;
}

static int keyboard_nkro_report_send(KeyboardNKROReport*buf)
{
    return hid_send_nkro((uint8_t*)buf, sizeof(KeyboardNKROReport));
}

static void keyboard_nkro_report_clear(KeyboardNKROReport*buf)
{
    memset(buf, 0, sizeof(KeyboardNKROReport));
}

void keyboard_report_clear(void)
{
#ifdef NKRO_ENABLE
    if (g_keyboard_config.nkro)
    {
        keyboard_nkro_report_clear(&keyboard_nkro_buffer);
    }
#endif
    keyboard_6kro_report_clear(&keyboard_6kro_buffer);
}


static inline void keyboard_report_apply_winlock(uint8_t *modifier)
{
    if (g_keyboard_config.winlock)
    {
        *modifier &= (uint8_t)~(KEY_LEFT_GUI | KEY_RIGHT_GUI);
    }
}

int keyboard_report_send(void)
{
    keyboard_report_apply_winlock(&keyboard_6kro_buffer.modifier);
#ifdef KEYBOARD_SHARED_EP
    keyboard_6kro_buffer.report_id = REPORT_ID_KEYBOARD;
#endif
#ifdef NKRO_ENABLE
    keyboard_report_apply_winlock(&keyboard_nkro_buffer.modifier);
    keyboard_nkro_buffer.report_id = REPORT_ID_NKRO;
#endif
#if defined(MIXED_KRO_ENABLE)
    if (g_keyboard_config.nkro)
    {
        return keyboard_6kro_report_send(&keyboard_6kro_buffer) || keyboard_nkro_report_send(&keyboard_nkro_buffer);
    }
#elif defined(NKRO_ENABLE)
    if (g_keyboard_config.nkro)
    {
        return keyboard_nkro_report_send(&keyboard_nkro_buffer);
    }
#endif
    return keyboard_6kro_report_send(&keyboard_6kro_buffer);
}

void keyboard_report_add(KeyboardEvent event)
{
#ifdef MIXED_KRO_ENABLE
    if (keyboard_6kro_report_add(&keyboard_6kro_buffer, event.keycode) && g_keyboard_config.nkro)
    {
        keyboard_nkro_report_add(&keyboard_nkro_buffer, KEYCODE_GET_MAIN(event.keycode));
    }
#elif defined(NKRO_ENABLE)
    if (g_keyboard_config.nkro)
    {
        keyboard_nkro_report_add(&keyboard_nkro_buffer, event.keycode);
    }
    else
    {
        keyboard_6kro_report_add(&keyboard_6kro_buffer, event.keycode);
    }
#else
    keyboard_6kro_report_add(&keyboard_6kro_buffer, event.keycode);
#endif
}


void keyboard_keycode_event_handler(KeyboardEvent event)
{
    switch (event.event)
    {
    case KEYBOARD_EVENT_KEY_DOWN:
        g_keyboard_report_flags.keyboard = true;
        keyboard_key_event_down_dispatch(event);
        break;
    case KEYBOARD_EVENT_KEY_TRUE:
        break;
    case KEYBOARD_EVENT_KEY_UP:
        g_keyboard_report_flags.keyboard = true;
        keyboard_key_event_up_dispatch(event);
        break;
    case KEYBOARD_EVENT_KEY_FALSE:
        break;
    default:
        break;
    }
}

static void keyboard_key_event_dispatch(KeyboardEvent event, bool pressed)
{
    event_loop_queue_push(&event_buffer, (EventLoopQueueElm){event, keyboard_tick_get32()});
#ifdef SCRIPT_ENABLE
    script_key_event_handler(event);
#endif
#ifdef MACRO_ENABLE
    macro_record_handler(event);
#endif
    if (!event.is_virtual)
    {
        if (pressed)
        {
            keyboard_key_event_down_callback((Key*)event.key);
        }
        else
        {
            keyboard_key_event_up_callback((Key*)event.key);
        }
    }
}

void keyboard_key_event_down_dispatch(KeyboardEvent event)
{
    keyboard_key_event_dispatch(event, true);
}

void keyboard_key_event_up_dispatch(KeyboardEvent event)
{
    keyboard_key_event_dispatch(event, false);
}

void keyboard_event_handler(KeyboardEvent event)
{
    if (!keyboard_event_needs_dispatch(event))
    {
        return;
    }
    const uint8_t keycode = KEYCODE_GET_MAIN(event.keycode);
    if (!event.is_virtual)
    {
        layer_lock_handler(event);
    }
    switch (keycode)
    {
#ifdef MOUSE_ENABLE
    case MOUSE_COLLECTION:
        mouse_event_handler(event);
        break;
#endif
    case LAYER_CONTROL:
        layer_event_handler(event);
        break;
#ifdef DYNAMICKEY_ENABLE
    case DYNAMIC_KEY:
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
#ifdef SCRIPT_ENABLE
    case SCRIPT_COLLECTION:
        script_event_handler(event);
        break;
#endif
#ifdef MACRO_ENABLE
    case MACRO_COLLECTION:
        macro_event_handler(event);
        break;
#endif
#ifdef GAMEPAD_ENABLE
    case GAMEPAD_COLLECTION:
        gamepad_event_handler(event);
        break;
#endif
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

void keyboard_event_poller(KeyboardEvent event, uint32_t tick)
{
#ifdef SCRIPT_ENABLE
    if (KEYCODE_GET_MAIN(event.keycode) == SCRIPT_COLLECTION)
    {
        script_event_poller(event, tick);
    }
    script_key_event_poller(event, tick);
#endif
    if (!event.is_virtual && event.event == KEYBOARD_EVENT_KEY_DOWN)
    {    
        Key * key = (Key*)event.key;
#ifdef RGB_ENABLE
        rgb_activate(key->id, keyboard_tick_get32());
#endif
        // keyboard_key_event_down_callback((Key*)event.key);
    }
    switch (KEYCODE_GET_MAIN(event.keycode))
    {
    case SCRIPT_COLLECTION:
        break;
    case KEYBOARD_OPERATION:
        keyboard_operation_event_poller(event, tick);
        break;
    case KEY_USER:
        keyboard_user_event_poller(event, tick);
        break;
    default:
        break;
    }
}

void keyboard_event_report_handler(KeyboardEvent event)
{
    const uint8_t keycode = KEYCODE_GET_MAIN(event.keycode);
    if (keycode <= KEY_EXSEL)
    {
        keyboard_report_add(event);
        return;
    }
    switch (KEYCODE_GET_MAIN(event.keycode))
    {
#ifdef MOUSE_ENABLE
    case MOUSE_COLLECTION:
        mouse_report_add(event);
        break;
#endif
#ifdef EXTRAKEY_ENABLE
    case CONSUMER_COLLECTION:
    case SYSTEM_COLLECTION:
        extra_key_report_add(event);
        break;
#endif
#ifdef JOYSTICK_ENABLE
    case JOYSTICK_COLLECTION:
        joystick_report_add(event);
        break;
#endif
#ifdef GAMEPAD_ENABLE
    case GAMEPAD_COLLECTION:
        gamepad_report_add(event);
        break;
#endif
    case LAYER_CONTROL:
        break;
    case KEYBOARD_OPERATION:
        break;
    case KEY_USER:
        break;
    default:
        break;
    }
}

#ifdef RGB_ENABLE
#define KEYBOARD_RGB_BRIGHTNESS_STEP 16
static void keyboard_rgb_brightness_step(int step)
{
    const int brightness = (int)g_rgb_base_config.brightness + step;
    rgb_set_brightness((uint8_t)(brightness > 255 ? 255 : (brightness < 0 ? 0 : brightness)));
}
#endif

/* Commands below KEYBOARD_CONFIG_BASE, executed on key press. */
static void keyboard_operation_command(KeyboardEvent event, uint8_t command)
{
    switch (command)
    {
    case KEYBOARD_REBOOT:
        keyboard_reboot();
        break;
    case KEYBOARD_FACTORY_RESET:
        keyboard_factory_reset();
        packet_notify_event(PACKET_EVENT_CONFIG_CHANGED);
        break;
    case KEYBOARD_SAVE:
        keyboard_profile_save();
        break;
    case KEYBOARD_BOOTLOADER:
        keyboard_jump_to_bootloader();
        break;
    case KEYBOARD_RESET_TO_DEFAULT:
        keyboard_profile_reset_to_default();
        packet_notify_event(PACKET_EVENT_CONFIG_CHANGED);
        break;
    case KEYBOARD_RECOVERY:
        keyboard_profile_restore();
        break;
    case KEYBOARD_FORMAT_STORAGE:
        keyboard_storage_format();
        break;
#ifdef RGB_ENABLE
    case KEYBOARD_RGB_BRIGHTNESS_UP:
        keyboard_rgb_brightness_step(KEYBOARD_RGB_BRIGHTNESS_STEP);
        break;
    case KEYBOARD_RGB_BRIGHTNESS_DOWN:
        keyboard_rgb_brightness_step(-KEYBOARD_RGB_BRIGHTNESS_STEP);
        break;
#endif
    case KEYBOARD_PROFILE0:
    case KEYBOARD_PROFILE1:
    case KEYBOARD_PROFILE2:
    case KEYBOARD_PROFILE3:
        keyboard_profile_select(command - KEYBOARD_PROFILE0);
        packet_notify_event(PACKET_EVENT_CONFIG_CHANGED);
        break;
    default:
        UNUSED(event);
        break;
    }
}

/* Configuration bit operations encoded with KEYBOARD_CONFIG(). The action
 * numbering (0 clears, 1 sets, 2 toggles) is the one host configurators emit
 * and is kept as is. */
static void keyboard_operation_config(KeyboardEvent event)
{
    const uint8_t bit = KEYBOARD_CONFIG_GET_INDEX(event.keycode);
    switch (KEYBOARD_CONFIG_GET_ACTION(event.keycode))
    {
    case KEYBOARD_CONFIG_ON:
        BIT_RESET(g_keyboard_config.raw, bit);
        break;
    case KEYBOARD_CONFIG_OFF:
        BIT_SET(g_keyboard_config.raw, bit);
        break;
    case KEYBOARD_CONFIG_TOGGLE:
        BIT_TOGGLE(g_keyboard_config.raw, bit);
        break;
    default:
        break;
    }
}

static void keyboard_operation_event_handler_(KeyboardEvent event)
{
    const uint8_t code = KEYBOARD_OPERATION_GET_CODE(event.keycode);
    switch (event.event)
    {
    case KEYBOARD_EVENT_KEY_UP:
        keyboard_key_event_up_dispatch(event);
        if (KEYCODE_GET_SUB(event.keycode) == KEYBOARD_CALIBRATE)
        {
#if defined(NEXUS_ENABLE) && !NEXUS_IS_SLAVE
            nexus_calibrate();
#endif
            target_calibration_tick = keyboard_tick_get32() + KEYBOARD_TIME_TO_TICK(CALIBRATION_DELAY);
            calibration_pending = true;
        }
        break;
    case KEYBOARD_EVENT_KEY_DOWN:
        keyboard_key_event_down_dispatch(event);
        if (code < KEYBOARD_CONFIG_BASE)
        {
            keyboard_operation_command(event, code);
        }
        else
        {
            keyboard_operation_config(event);
        }
        break;
    default:
        break;
    }
}

void keyboard_operation_event_handler(KeyboardEvent event)
{
#ifndef KEYBOARD_OPERATION_POLLING
    keyboard_operation_event_handler_(event);
#endif
}

void keyboard_operation_event_poller(KeyboardEvent event, uint32_t tick)
{
    UNUSED(tick);
#ifdef KEYBOARD_OPERATION_POLLING
    keyboard_operation_event_handler_(event);
#endif
}

void keyboard_key_event_down_callback(Key*key)
{
    keyboard_key_event_down_callback_user(key);
#ifdef KPS_ENABLE
    record_kps_tick();
#endif
#ifdef COUNTER_ENABLE
    g_key_counts[key->id]++;
#if defined(STORAGE_ENABLE) && defined(RECORD_PERSIST_ENABLE)
    record_schedule_idle_save();
#endif
#endif
}

void keyboard_key_event_up_callback(Key*key)
{
    keyboard_key_event_up_callback_user(key);
}

__WEAK void keyboard_key_event_down_callback_user(Key*key)
{

}

__WEAK void keyboard_key_event_up_callback_user(Key*key)
{

}

void keyboard_report_clear_all(void)
{
    keyboard_report_clear();
#ifdef MOUSE_ENABLE
    mouse_report_clear();
#endif
#ifdef JOYSTICK_ENABLE
    joystick_report_clear();
#endif
#ifdef GAMEPAD_ENABLE
    gamepad_report_clear();
#endif
}

void keyboard_init(void)
{
    keyboard_tick_init();
    g_keyboard_config.enable_report = true;
    g_keyboard_report_flags.raw = 0;
    target_calibration_tick = 0;
    calibration_pending = false;
    for (uint16_t i = 0; i < ADVANCED_KEY_NUM; i++)
    {
        advanced_key_init(&g_keyboard_advanced_keys[i], i);
    }
    for (uint16_t i = 0; i < KEY_NUM; i++)
    {
        key_init(&g_keyboard_keys[i], ADVANCED_KEY_NUM + i);
    }
    memset((void*)g_keyboard_bitmap, 0, sizeof(g_keyboard_bitmap));
    layer_init();
#ifdef RGB_ENABLE
    rgb_init();
#endif
#ifdef MIDI_ENABLE
    midi_init();
#endif
#if defined(MACRO_ENABLE) || defined(SCRIPT_ENABLE)
    event_cache_init();
#endif
#ifdef MACRO_ENABLE
    macro_init();
#endif
#ifdef STORAGE_ENABLE
    storage_mount();
    if (storage_check_version())
    {
        keyboard_factory_reset();
    }
#ifdef RECORD_PERSIST_ENABLE
    storage_read_statistics();
#ifdef COUNTER_ENABLE
    memcpy(g_key_counts_at_startup, g_key_counts, sizeof(g_key_counts));
#endif
    record_init();
#endif
#endif
    event_loop_queue_init(&event_buffer, event_buffers, EVENT_BUFFER_LENGTH);
    keyboard_profile_restore();
#ifdef NEXUS_ENABLE
#if NEXUS_IS_SLAVE
    g_keyboard_config.enable_report = false;
#else
    nexus_init();
#endif
#endif
#ifdef SCRIPT_ENABLE
    //script_init();
#endif
}

__WEAK void keyboard_profile_reset_to_default(void)
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
#ifdef SCRIPT_ENABLE
    script_factory_reset();
#endif
}

void keyboard_factory_reset(void)
{
    keyboard_profile_reset_to_default();
#ifdef STORAGE_ENABLE
    for (int i = 0; i < STORAGE_PROFILE_FILE_NUM; i++)
    {
        g_current_profile_index = i;
        storage_save_profile();
    }
    g_current_profile_index = 0;
    keyboard_profile_select(0);
#endif
}

void keyboard_storage_format(void)
{
#ifdef STORAGE_ENABLE
    storage_format();
    keyboard_reboot();
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
    switch (event.event)
    {
    case KEYBOARD_EVENT_KEY_DOWN:
        keyboard_key_event_down_dispatch(event);
        break;
    case KEYBOARD_EVENT_KEY_UP:
        keyboard_key_event_up_dispatch(event);
        break;
    default:
        break;
    }
}

__WEAK void keyboard_user_event_poller(KeyboardEvent event, uint32_t tick)
{
    UNUSED(event);
    UNUSED(tick);
}

__WEAK void keyboard_scan(void)
{

}

void keyboard_profile_restore(void)
{
    storage_read_profile_index();
#ifdef STORAGE_ENABLE
    storage_read_profile();
#else
    keyboard_profile_reset_to_default();
#endif
}

void keyboard_profile_save(void)
{
#ifdef STORAGE_ENABLE
    storage_save_profile();
#endif
}

void keyboard_profile_select(uint8_t index)
{
#ifdef STORAGE_ENABLE
    if (index < STORAGE_PROFILE_FILE_NUM)
    {
        g_current_profile_index = index;
    }
    storage_save_profile_index();
#else
    UNUSED(index);
#endif

    keyboard_profile_restore();
}

void keyboard_report_build(void)
{
#ifndef OPTIMIZE_KEY_BITMAP
    for (int i = 0; i < ADVANCED_KEY_NUM; i++)
    {
        AdvancedKey*key = &g_keyboard_advanced_keys[i];
        if (key->key.report_state)
        {
            keyboard_event_report_handler(MK_EVENT(layer_cache_get_keycode(key->key.id), KEYBOARD_EVENT_NO_EVENT, key));
        }
    }
    for (int i = 0; i < KEY_NUM; i++)
    {        
        Key*key = &g_keyboard_keys[i];
        if (key->report_state)
        {
            keyboard_event_report_handler(MK_EVENT(layer_cache_get_keycode(key->id), KEYBOARD_EVENT_NO_EVENT, key));
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
            keyboard_event_report_handler(MK_EVENT(layer_cache_get_keycode(id), KEYBOARD_EVENT_NO_EVENT, key));
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
                keyboard_event_report_handler(MK_EVENT(layer_cache_get_keycode(id), KEYBOARD_EVENT_NO_EVENT, key));
            }
        }
#endif
    }
#endif
#ifdef DYNAMICKEY_ENABLE
    dynamic_key_add_to_report();
#endif
#if defined(MACRO_ENABLE) || defined(SCRIPT_ENABLE)
    event_cache_add_to_report();
#endif
}

void keyboard_report_send_all(void)
{   
#ifdef MOUSE_ENABLE
    if (g_keyboard_report_flags.mouse)
    {
        if (!mouse_report_send())
        {
            g_keyboard_report_flags.mouse = false;
        }
    }
#endif
#ifdef EXTRAKEY_ENABLE
    if (g_keyboard_report_flags.consumer)
    {
        if (!consumer_key_report_send())
        {
            g_keyboard_report_flags.consumer = false;
        }
    }
    if (g_keyboard_report_flags.system)
    {
        if (!system_key_report_send())
        {
            g_keyboard_report_flags.system = false;
        }
    }
#endif
    if (g_keyboard_report_flags.keyboard)
    {
        if (!keyboard_report_send())
        {
            g_keyboard_report_flags.keyboard = false;
        }
    }
#ifdef JOYSTICK_ENABLE
    if (g_keyboard_report_flags.joystick)
    {
        if (!joystick_report_send())
        {
            g_keyboard_report_flags.joystick = false;
        }
    }
#endif
#ifdef GAMEPAD_ENABLE
    if (g_keyboard_report_flags.gamepad)
    {
        if (!gamepad_report_send())
        {
            g_keyboard_report_flags.gamepad = false;
        }
    }
#endif
}

static void keyboard_task_update_advanced_keys(void)
{
    for (uint16_t i = 0; i < ADVANCED_KEY_NUM; i++)
    {
        AdvancedKey *advanced_key = &g_keyboard_advanced_keys[i];
        keyboard_advanced_key_update_raw(advanced_key, advanced_key_read_raw(advanced_key));
    }
}

static void keyboard_task_send_reports(void)
{
#ifdef SUSPEND_ENABLE
    if (g_keyboard_is_suspend)
    {
        if (!g_keyboard_report_flags.raw)
        {
            return; /* nothing to wake the host for */
        }
        g_keyboard_is_suspend = false;
        send_remote_wakeup();
    }
#endif
    if (g_keyboard_config.continuous_poll)
    {
        g_keyboard_report_flags.keyboard = true;
    }
    if (g_keyboard_config.enable_report && g_keyboard_report_flags.raw)
    {
        keyboard_report_clear_all();
        keyboard_report_build();
        keyboard_report_send_all();
    }
    if (g_keyboard_config.debug)
    {
        packet_send_debug_packet();
    }
    packet_buffer_flush();
}

void keyboard_task(void)
{
    keyboard_scan();
#ifdef ENCODER_ENABLE
    encoder_process();
#endif
#if defined(NEXUS_ENABLE) && NEXUS_IS_SLAVE
    /* A slave only samples its keys and streams them to the master. */
    keyboard_task_update_advanced_keys();
    packet_buffer_flush();
    if (g_keyboard_config.enable_report)
    {
        nexus_send_report();
    }
#else
#if defined(NEXUS_ENABLE)
    nexus_process();
#else
    keyboard_task_update_advanced_keys();
#endif
#if defined(SCRIPT_ENABLE) && !defined(SCRIPT_POLLING)
    script_process();
#endif
#ifdef MACRO_ENABLE
    macro_process();
#endif
#ifdef DYNAMICKEY_ENABLE
    dynamic_key_process();
#endif
#ifdef MIDI_ENABLE
    midi_task();
#endif
    keyboard_task_send_reports();
#endif
}

void keyboard_process(void)
{
    event_loop_queue_foreach(&event_buffer, EventLoopQueueElm, event)
    {
        keyboard_event_poller(event->event, event->tick);
        event_loop_queue_pop(&event_buffer);
    }
#if defined(SCRIPT_ENABLE) && defined(SCRIPT_POLLING)
    script_process();
#endif
    if (calibration_pending && tick_reached32(keyboard_tick_get32(), target_calibration_tick))
    {
        calibration_pending = false;
        analog_calibrate();
        packet_notify_event(PACKET_EVENT_CONFIG_CHANGED);
    }
#ifdef RGB_ENABLE
    rgb_process();
#endif
#if defined(NEXUS_ENABLE) && !NEXUS_IS_SLAVE
    nexus_poll();
#endif
#ifdef CONSOLE_ENABLE
    console_flush();
#endif
#ifdef RECORD_PERSIST_ENABLE
    record_process();
#endif
}

static inline bool keyboard_key_report_and_dispatch(Key *key)
{
    const bool changed = keyboard_key_set_report_state(key, keyboard_key_debounce(key));
    const Keycode keycode = layer_cache_get_keycode(key->id);
    if (changed || keyboard_keycode_dispatches_unchanged(keycode))
    {
        /* Most polls have nothing to dispatch. Keep event construction on
         * this branch so Cortex-M does not spill an unused event to stack. */
        keyboard_event_handler(MK_EVENT(keycode, EVENT_TYPE(changed, key->report_state), key));
    }
    return changed;
}

bool keyboard_key_update(Key *key, bool state)
{
    key_update(key, state);
    return keyboard_key_report_and_dispatch(key);
}

bool keyboard_advanced_key_update(AdvancedKey *advanced_key, AnalogValue value)
{
    advanced_key_update(advanced_key, value);
    return keyboard_key_report_and_dispatch(&advanced_key->key);
}

bool keyboard_advanced_key_update_raw(AdvancedKey *advanced_key, AnalogRawValue raw)
{
    advanced_key_update_raw(advanced_key, raw);
    return keyboard_key_report_and_dispatch(&advanced_key->key);
}
