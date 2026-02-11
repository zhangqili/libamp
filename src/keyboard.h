/*
 * Copyright (c) 2024 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef KEYBOARD_H_
#define KEYBOARD_H_

#include "key.h"
#include "advanced_key.h"
#include "keyboard_conf.h"
#include "keyboard_def.h"
#include "event.h"
#include "keycode.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef KEYBOARD_VERSION_MAJOR
#define KEYBOARD_VERSION_MAJOR LIBAMP_VERSION_MAJOR
#endif

#ifndef KEYBOARD_VERSION_MINOR
#define KEYBOARD_VERSION_MINOR LIBAMP_VERSION_MINOR
#endif

#ifndef KEYBOARD_VERSION_PATCH
#define KEYBOARD_VERSION_PATCH LIBAMP_VERSION_PATCH
#endif

#ifndef KEYBOARD_VERSION_INFO
#define KEYBOARD_VERSION_INFO  LIBAMP_VERSION_INFO
#endif

#ifndef POLLING_RATE
#define POLLING_RATE 1000
#endif

#define NKRO_REPORT_BITS 30

#define TOTAL_KEY_NUM (ADVANCED_KEY_NUM + KEY_NUM)

#define KEYBOARD_CONFIG(index, action) ((((KEYBOARD_CONFIG_BASE + (index)) | ((action) << 6)) << 8) | KEYBOARD_OPERATION)

#define KEYBOARD_TIME_TO_TICK(x)   ((uint32_t)(((uint64_t)(x) * POLLING_RATE) / 1000))
#define KEYBOARD_TICK_TO_TIME(x)   ((uint32_t)(((uint64_t)(x) * 1000) / POLLING_RATE))

#define KEY_BITMAP_SIZE ((TOTAL_KEY_NUM + sizeof(uint32_t)*8 - 1) / (sizeof(uint32_t)*8))

typedef struct
{
#ifdef KEYBOARD_SHARED_EP
    uint8_t report_id;
#endif
    uint8_t modifier;
    uint8_t reserved;
    uint8_t buffer[6];
    uint8_t keynum;
} __PACKED Keyboard_6KROBuffer;

typedef struct
{
    uint8_t report_id;
    uint8_t modifier;
    uint8_t buffer[NKRO_REPORT_BITS];
} __PACKED Keyboard_NKROBuffer;

typedef enum
{
    KEYBOARD_STATE_IDLE,
    KEYBOARD_STATE_DEBUG,
} KEYBOARD_STATE;

enum
{
    KEYBOARD_CONFIG_DEBUG           = 0,
    KEYBOARD_CONFIG_NKRO            = 1,
    KEYBOARD_CONFIG_WINLOCK         = 2,
    KEYBOARD_CONFIG_CONTINOUS_POLL  = 3,
    KEYBOARD_CONFIG_ENABLE_REPORT   = 4,
    KEYBOARD_CONFIG_NUM             = 5,
};

enum
{
    KEYBOARD_CONFIG_ON      = 0,
    KEYBOARD_CONFIG_OFF     = 1,
    KEYBOARD_CONFIG_TOGGLE  = 2,
};

typedef union
{
    uint8_t raw;
    struct
    {
        bool debug : 1;
        bool nkro : 1;
        bool winlock : 1;
        bool continous_poll : 1;
        bool enable_report : 1;
        uint8_t reserved : 3;
    };
} __PACKED KeyboardConfig;

typedef union
{
    uint8_t raw;
    struct
    {
        bool    num_lock : 1;
        bool    caps_lock : 1;
        bool    scroll_lock : 1;
        bool    compose : 1;
        bool    kana : 1;
        uint8_t reserved : 3;
    };
} KeyboardLED;

typedef union
{
    uint8_t raw;
    struct
    {
        bool    keyboard : 1;
        bool    mouse : 1;
        bool    consumer : 1;
        bool    system : 1;
        bool    joystick : 1;
        uint8_t reserved : 3;
    };
} KeyboardReportFlag;

enum
{
    KEYBOARD_REPORT_FLAG = 0,
    MOUSE_REPORT_FLAG = 1,
    CONSUMER_REPORT_FLAG = 2,
    SYSTEM_REPORT_FLAG = 3,
    JOYSTICK_REPORT_FLAG = 4,
};

enum ReportID { 
    REPORT_ID_ALL = 0,
    REPORT_ID_KEYBOARD = 1,
    REPORT_ID_MOUSE,
    REPORT_ID_SYSTEM,
    REPORT_ID_CONSUMER,
    REPORT_ID_PROGRAMMABLE_BUTTON,
    REPORT_ID_NKRO,
    REPORT_ID_JOYSTICK,
    REPORT_ID_DIGITIZER,
    REPORT_ID_LIGHTING_LAMP_ARRAY_ATTRIBUTES,
    REPORT_ID_LIGHTING_LAMP_ATTRIBUTES_REQUEST,
    REPORT_ID_LIGHTING_LAMP_ATTRIBUTES_RESPONSE,
    REPORT_ID_LIGHTING_LAMP_MULTI_UPDATE,
    REPORT_ID_LIGHTING_LAMP_RANGE_UPDATE,
    REPORT_ID_LIGHTING_LAMP_ARRAY_CONTROL,
    REPORT_ID_COUNT = REPORT_ID_LIGHTING_LAMP_ARRAY_CONTROL
};

extern AdvancedKey g_keyboard_advanced_keys[ADVANCED_KEY_NUM];
extern Key g_keyboard_keys[KEY_NUM];
extern KeyboardLED g_keyboard_led_state;
extern KeyboardConfig g_keyboard_config;
extern Keycode g_keymap[LAYER_NUM][TOTAL_KEY_NUM];


extern const Keycode g_default_keymap[LAYER_NUM][TOTAL_KEY_NUM];

extern volatile uint32_t g_keyboard_tick;
extern volatile bool g_keyboard_is_suspend;
extern volatile KeyboardReportFlag g_keyboard_report_flags;

extern volatile uint32_t g_keyboard_bitmap[KEY_BITMAP_SIZE];

void keyboard_event_handler(KeyboardEvent event);
void keyboard_operation_event_handler(KeyboardEvent event);
void keyboard_key_event_down_callback(Key*key);

void keyboard_add_buffer(KeyboardEvent event);
int keyboard_buffer_send(void);
void keyboard_clear_buffer(void);

int keyboard_6KRObuffer_add(Keyboard_6KROBuffer *buf, Keycode keycode);
int keyboard_6KRObuffer_send(Keyboard_6KROBuffer *buf);
void keyboard_6KRObuffer_clear(Keyboard_6KROBuffer *buf);

int keyboard_NKRObuffer_add(Keyboard_NKROBuffer*buf,Keycode keycode);
int keyboard_NKRObuffer_send(Keyboard_NKROBuffer*buf);
void keyboard_NKRObuffer_clear(Keyboard_NKROBuffer*buf);

bool keyboard_key_update(Key *key, bool state);
bool keyboard_advanced_key_update(AdvancedKey *advanced_key, AnalogValue value);
bool keyboard_advanced_key_update_raw(AdvancedKey *advanced_key, AnalogRawValue raw);

void keyboard_init(void);
void keyboard_reboot(void);
void keyboard_reset_to_default(void);
void keyboard_factory_reset(void);
void keyboard_jump_to_bootloader(void);
void keyboard_user_event_handler(KeyboardEvent event);
void keyboard_scan(void);
void keyboard_fill_buffer(void);
void keyboard_send_report(void);
void keyboard_recovery(void);
void keyboard_save(void);
void keyboard_set_profile_index(uint8_t index);
void keyboard_task(void);
void keyboard_delay(uint32_t ms);

static inline Key* keyboard_get_key(uint16_t id)
{    
    if (id >= TOTAL_KEY_NUM)
    {
        return NULL;
    }
    return id < ADVANCED_KEY_NUM ? &g_keyboard_advanced_keys[id].key : &g_keyboard_keys[id - ADVANCED_KEY_NUM];
}

static inline bool keyboard_key_set_report_state(Key*key, bool state)
{
    if (key->report_state == state) {
        return false;
    }
    key->report_state = state;
    const uint32_t index = key->id / 32;
    const uint32_t mask = 1U << (key->id % 32);
    if (state)
    {
        g_keyboard_bitmap[index] |= mask;
    } 
    else
    {
        g_keyboard_bitmap[index] &= ~mask;
    }
    return true;
}

static inline AnalogValue keyboard_get_key_raw_value(Key* key)
{    
    return (IS_ADVANCED_KEY((key)) ? ((AdvancedKey*)(key))->raw : key->state);
}

static inline AnalogValue keyboard_get_key_analog_value(Key* key)
{    
    return (IS_ADVANCED_KEY((key)) ? ((AdvancedKey*)(key))->value : ((((Key*)(key))->state) * ANALOG_VALUE_RANGE + ANALOG_VALUE_MIN));
}

static inline AnalogValue keyboard_get_key_effective_analog_value(Key* key)
{    
    return (IS_ADVANCED_KEY((key)) ? advanced_key_get_effective_value(((AdvancedKey*)(key))) : ((((Key*)(key))->state) * ANALOG_VALUE_RANGE + ANALOG_VALUE_MIN));
}

static inline bool keyboard_key_debounce(Key *key)
{
#if DEBOUNCE_PRESS > 0 || DEBOUNCE_RELEASE > 0
#if DEBOUNCE_PRESS >= 127 || DEBOUNCE_RELEASE > 127
#warning "Debounce tick is too long!"
#endif
    bool next_report_state = key->report_state;
    if (key->debounce < 0)
    {
        key->debounce++;
        return next_report_state;
    }
    if (key->report_state)
    {
        if (!key->state)
        {
#if DEBOUNCE_RELEASE_EAGER
            next_report_state = 0;
            key->debounce = -(int8_t)DEBOUNCE_RELEASE;
#else
            key->debounce++;
            if (key->debounce >= (int8_t)DEBOUNCE_RELEASE)
            {
                next_report_state = 0;
                key->debounce = 0;
            }
#endif
        }
        else
        {
            if (key->debounce > 0)
                key->debounce = 0;
        }
    }
    else
    {
        if (key->state)
        {
#if DEBOUNCE_PRESS_EAGER
            next_report_state = 1;
            key->debounce = -(int8_t)DEBOUNCE_PRESS;
#else
            key->debounce++;
            if (key->debounce >= (int8_t)DEBOUNCE_PRESS)
            {
                next_report_state = 1;
                key->debounce = 0;
            }
#endif
        }
        else
        {
            if (key->debounce > 0)
                key->debounce = 0;
        }
    }
    return next_report_state;
#else
    return key->state;
#endif
}

#ifdef __cplusplus
}
#endif

#endif /* KEYBOARD_H_ */
