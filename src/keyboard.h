/*
 * Copyright (c) 2024 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file keyboard.h
 * @brief Core keyboard functionality, definitions, and state management.
 * @author Zhangqi Li
 * @date 2024
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

/** @brief Major version number of the keyboard firmware. */
#ifndef KEYBOARD_VERSION_MAJOR
#define KEYBOARD_VERSION_MAJOR LIBAMP_VERSION_MAJOR
#endif

/** @brief Minor version number of the keyboard firmware. */
#ifndef KEYBOARD_VERSION_MINOR
#define KEYBOARD_VERSION_MINOR LIBAMP_VERSION_MINOR
#endif

/** @brief Patch version number of the keyboard firmware. */
#ifndef KEYBOARD_VERSION_PATCH
#define KEYBOARD_VERSION_PATCH LIBAMP_VERSION_PATCH
#endif

/** @brief Additional version information string. */
#ifndef KEYBOARD_VERSION_INFO
#define KEYBOARD_VERSION_INFO  LIBAMP_VERSION_INFO
#endif

/** @brief Default polling rate in Hz. */
#ifndef POLLING_RATE
#define POLLING_RATE 1000
#endif

/** @brief Number of bytes for the NKRO report buffer. */
#define NKRO_REPORT_BITS 30

/** @brief Total number of keys (advanced keys + standard keys). */
#define TOTAL_KEY_NUM (ADVANCED_KEY_NUM + KEY_NUM)

/**
 * @brief Macro to generate a keyboard configuration command.
 * @param index The configuration index.
 * @param action The action to perform (ON, OFF, TOGGLE).
 */
#define KEYBOARD_CONFIG(index, action) ((((KEYBOARD_CONFIG_BASE + (index)) | ((action) << 6)) << 8) | KEYBOARD_OPERATION)

/** @brief Converts milliseconds to keyboard ticks based on the polling rate. */
#define KEYBOARD_TIME_TO_TICK(x)   ((uint32_t)(((uint64_t)(x) * POLLING_RATE) / 1000))

/** @brief Converts keyboard ticks to milliseconds based on the polling rate. */
#define KEYBOARD_TICK_TO_TIME(x)   ((uint32_t)(((uint64_t)(x) * 1000) / POLLING_RATE))

/** @brief Size of the key bitmap array. */
#define KEY_BITMAP_SIZE ((TOTAL_KEY_NUM + sizeof(uint32_t)*8 - 1) / (sizeof(uint32_t)*8))

/**
 * @brief Standard 6-Key Rollover (6KRO) HID report buffer.
 */
typedef struct
{
#ifdef KEYBOARD_SHARED_EP
    uint8_t report_id; /**< Report ID for shared endpoint. */
#endif
    uint8_t modifier;  /**< Modifier keys bitmask. */
    uint8_t reserved;  /**< Reserved byte (padding). */
    uint8_t buffer[6]; /**< Array to store up to 6 keycodes. */
    uint8_t keynum;    /**< Current number of keys in the buffer. */
} __PACKED Keyboard_6KROBuffer;

/**
 * @brief N-Key Rollover (NKRO) HID report buffer.
 */
typedef struct
{
    uint8_t report_id;               /**< Report ID. */
    uint8_t modifier;                /**< Modifier keys bitmask. */
    uint8_t buffer[NKRO_REPORT_BITS];/**< Bitmask buffer for all keys. */
} __PACKED Keyboard_NKROBuffer;

/**
 * @brief Enumeration of keyboard operating states.
 */
typedef enum
{
    KEYBOARD_STATE_IDLE, /**< Normal idle state. */
    KEYBOARD_STATE_DEBUG,/**< Debugging state. */
} KEYBOARD_STATE;

/**
 * @brief Enumeration for keyboard configuration options.
 */
enum
{
    KEYBOARD_CONFIG_DEBUG           = 0, /**< Toggle debug mode. */
    KEYBOARD_CONFIG_NKRO            = 1, /**< Toggle NKRO mode. */
    KEYBOARD_CONFIG_WINLOCK         = 2, /**< Toggle Windows key lock. */
    KEYBOARD_CONFIG_CONTINOUS_POLL  = 3, /**< Toggle continuous polling. */
    KEYBOARD_CONFIG_ENABLE_REPORT   = 4, /**< Enable/Disable HID reporting. */
    KEYBOARD_CONFIG_NUM             = 5, /**< Total number of config options. */
};

/**
 * @brief Enumeration for configuration actions.
 */
enum
{
    KEYBOARD_CONFIG_ON      = 0, /**< Turn configuration option ON. */
    KEYBOARD_CONFIG_OFF     = 1, /**< Turn configuration option OFF. */
    KEYBOARD_CONFIG_TOGGLE  = 2, /**< Toggle configuration option. */
};

/**
 * @brief Union for keyboard configuration flags.
 */
typedef union
{
    uint8_t raw; /**< Raw byte access to configuration flags. */
    struct
    {
        bool debug : 1;          /**< Debug mode flag. */
        bool nkro : 1;           /**< NKRO mode flag. */
        bool winlock : 1;        /**< Windows lock flag. */
        bool continous_poll : 1; /**< Continuous polling flag. */
        bool enable_report : 1;  /**< Report enabling flag. */
        uint8_t reserved : 3;    /**< Reserved bits. */
    };
} __PACKED KeyboardConfig;

/**
 * @brief Union for Keyboard LED states.
 */
typedef union
{
    uint8_t raw; /**< Raw byte access to LED states. */
    struct
    {
        bool    num_lock : 1;    /**< Num Lock state. */
        bool    caps_lock : 1;   /**< Caps Lock state. */
        bool    scroll_lock : 1; /**< Scroll Lock state. */
        bool    compose : 1;     /**< Compose state. */
        bool    kana : 1;        /**< Kana state. */
        uint8_t reserved : 3;    /**< Reserved bits. */
    };
} KeyboardLED;

/**
 * @brief Union for tracking which HID reports need to be sent.
 */
typedef union
{
    uint8_t raw; /**< Raw byte access to report flags. */
    struct
    {
        bool    keyboard : 1; /**< Keyboard report pending. */
        bool    mouse : 1;    /**< Mouse report pending. */
        bool    consumer : 1; /**< Consumer control report pending. */
        bool    system : 1;   /**< System control report pending. */
        bool    joystick : 1; /**< Joystick report pending. */
        uint8_t reserved : 3; /**< Reserved bits. */
    };
} KeyboardReportFlag;

/**
 * @brief Enumeration of report flag indices.
 */
enum
{
    KEYBOARD_REPORT_FLAG = 0,
    MOUSE_REPORT_FLAG = 1,
    CONSUMER_REPORT_FLAG = 2,
    SYSTEM_REPORT_FLAG = 3,
    JOYSTICK_REPORT_FLAG = 4,
};

/**
 * @brief Enumeration of USB HID Report IDs.
 */
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

/* Global Variables */
extern AdvancedKey g_keyboard_advanced_keys[ADVANCED_KEY_NUM]; /**< Array of advanced keys (analog/magnetic). */
extern Key g_keyboard_keys[KEY_NUM]; /**< Array of standard keys. */
extern KeyboardLED g_keyboard_led_state; /**< Current state of keyboard LEDs. */
extern KeyboardConfig g_keyboard_config; /**< Current keyboard configuration. */
extern Keycode g_keymap[LAYER_NUM][TOTAL_KEY_NUM]; /**< Keymap definition. */

extern const Keycode g_default_keymap[LAYER_NUM][TOTAL_KEY_NUM]; /**< Default keymap (read-only). */

extern volatile uint32_t g_keyboard_tick; /**< System tick counter. */
extern volatile bool g_keyboard_is_suspend; /**< USB suspend state flag. */
extern volatile KeyboardReportFlag g_keyboard_report_flags; /**< Flags indicating pending reports. */

extern volatile uint32_t g_keyboard_bitmap[KEY_BITMAP_SIZE]; /**< Bitmap of active keys. */

/* Function Prototypes */

/**
 * @brief Handles general keyboard events.
 * @param event The keyboard event to handle.
 */
void keyboard_event_handler(KeyboardEvent event);

/**
 * @brief Handles special keyboard operation events (e.g., config changes, reboot).
 * @param event The operation event.
 */
void keyboard_operation_event_handler(KeyboardEvent event);

/**
 * @brief Callback triggered when a key down event occurs.
 * @param key Pointer to the key structure.
 */
void keyboard_key_event_down_callback(Key*key);

/**
 * @brief Adds an event to the keyboard report buffer.
 * @param event The event to add.
 */
void keyboard_add_buffer(KeyboardEvent event);

/**
 * @brief Sends the populated keyboard buffer to the host.
 * @return 0 on success, non-zero on failure.
 */
int keyboard_buffer_send(void);

/**
 * @brief Clears all keyboard report buffers.
 */
void keyboard_clear_buffer(void);

/**
 * @brief Adds a keycode to the 6KRO buffer.
 * @param buf Pointer to the 6KRO buffer.
 * @param keycode The keycode to add.
 * @return 0 on success, 1 if buffer is full or invalid key.
 */
int keyboard_6KRObuffer_add(Keyboard_6KROBuffer *buf, Keycode keycode);

/**
 * @brief Sends the 6KRO buffer content.
 * @param buf Pointer to the 6KRO buffer.
 * @return 0 on success.
 */
int keyboard_6KRObuffer_send(Keyboard_6KROBuffer *buf);

/**
 * @brief Clears the 6KRO buffer.
 * @param buf Pointer to the 6KRO buffer.
 */
void keyboard_6KRObuffer_clear(Keyboard_6KROBuffer *buf);

/**
 * @brief Adds a keycode to the NKRO buffer.
 * @param buf Pointer to the NKRO buffer.
 * @param keycode The keycode to add.
 * @return 0 on success.
 */
int keyboard_NKRObuffer_add(Keyboard_NKROBuffer*buf,Keycode keycode);

/**
 * @brief Sends the NKRO buffer content.
 * @param buf Pointer to the NKRO buffer.
 * @return 0 on success.
 */
int keyboard_NKRObuffer_send(Keyboard_NKROBuffer*buf);

/**
 * @brief Clears the NKRO buffer.
 * @param buf Pointer to the NKRO buffer.
 */
void keyboard_NKRObuffer_clear(Keyboard_NKROBuffer*buf);

/**
 * @brief Updates the state of a standard digital key.
 * @param key Pointer to the key.
 * @param state New state (true = pressed, false = released).
 * @return True if the key state changed.
 */
bool keyboard_key_update(Key *key, bool state);

/**
 * @brief Updates the state of an advanced (analog) key based on a normalized value.
 * @param advanced_key Pointer to the advanced key.
 * @param value Normalized analog value (e.g., 0.0 to 1.0).
 * @return True if the key state changed.
 */
bool keyboard_advanced_key_update(AdvancedKey *advanced_key, AnalogValue value);

/**
 * @brief Updates the state of an advanced key based on a raw analog value.
 * @param advanced_key Pointer to the advanced key.
 * @param raw Raw ADC value.
 * @return True if the key state changed.
 */
bool keyboard_advanced_key_update_raw(AdvancedKey *advanced_key, AnalogRawValue raw);

/**
 * @brief Initializes the keyboard
 */
void keyboard_init(void);

/**
 * @brief Reboots the keyboard.
 */
void keyboard_reboot(void);

/**
 * @brief Resets the keyboard configuration to defaults.
 */
void keyboard_reset_to_default(void);

/**
 * @brief Performs a factory reset (clears storage and resets).
 */
void keyboard_factory_reset(void);

/**
 * @brief Jumps to the bootloader mode.
 */
void keyboard_jump_to_bootloader(void);

/**
 * @brief Handler for user-defined events.
 * @param event The user event.
 */
void keyboard_user_event_handler(KeyboardEvent event);

/**
 * @brief Scans the keyboard matrix/sensors.
 */
void keyboard_scan(void);

/**
 * @brief Fills the report buffers based on current key states.
 */
void keyboard_fill_buffer(void);

/**
 * @brief Sends pending reports to the host.
 */
void keyboard_send_report(void);

/**
 * @brief Recovers keyboard configuration from storage.
 */
void keyboard_recovery(void);

/**
 * @brief Saves current configuration to storage.
 */
void keyboard_save(void);

/**
 * @brief Selects a specific configuration profile.
 * @param index The profile index.
 */
void keyboard_set_config_index(uint8_t index);

/**
 * @brief Main keyboard task loop.
 */
void keyboard_task(void);

/**
 * @brief Delays execution.
 * @param ms Milliseconds to delay.
 */
void keyboard_delay(uint32_t ms);

/* Inline Functions */

/**
 * @brief Retrieves a pointer to a Key structure by its ID.
 * @param id The global key ID.
 * @return Pointer to the Key/AdvancedKey's Key struct, or NULL if invalid.
 */
static inline Key* keyboard_get_key(uint16_t id)
{    
    if (id >= TOTAL_KEY_NUM)
    {
        return NULL;
    }
    return id < ADVANCED_KEY_NUM ? &g_keyboard_advanced_keys[id].key : &g_keyboard_keys[id - ADVANCED_KEY_NUM];
}

/**
 * @brief Sets the report state of a key and updates the global key bitmap.
 * @param key Pointer to the key.
 * @param state The new report state.
 * @return True if the state changed, false otherwise.
 */
static inline bool keyboard_key_set_report_state(Key*key, bool state)
{
    if (key->report_state == state) {
        return false;
    }
    key->report_state = state;
    const uint32_t index = key->id / 32;
    const uint32_t mask = 1U << (key->id % 32);
    g_keyboard_bitmap[index] ^= mask;
    return true;
}

/**
 * @brief Gets the raw value of a key.
 * @param key Pointer to the key.
 * @return The raw analog value for advanced keys, or digital state for standard keys.
 */
static inline AnalogValue keyboard_get_key_raw_value(Key* key)
{    
    return (IS_ADVANCED_KEY((key)) ? ((AdvancedKey*)(key))->raw : key->state);
}

/**
 * @brief Gets the current analog value of a key.
 * @param key Pointer to the key.
 * @return The current value.
 */
static inline AnalogValue keyboard_get_key_analog_value(Key* key)
{    
    return (IS_ADVANCED_KEY((key)) ? ((AdvancedKey*)(key))->value : ((((Key*)(key))->state) * ANALOG_VALUE_RANGE + ANALOG_VALUE_MIN));
}

/**
 * @brief Gets the effective analog value (processed/deadzoned) of a key.
 * @param key Pointer to the key.
 * @return The effective value.
 */
static inline AnalogValue keyboard_get_key_effective_analog_value(Key* key)
{    
    return (IS_ADVANCED_KEY((key)) ? advanced_key_get_effective_value(((AdvancedKey*)(key))) : ((((Key*)(key))->state) * ANALOG_VALUE_RANGE + ANALOG_VALUE_MIN));
}

/**
 * @brief Debounces a key's state.
 * * Applies eager press/release logic based on configuration macros.
 * * @param key Pointer to the key.
 * @return The debounced state.
 */
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
