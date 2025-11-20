/*
 * Copyright (c) 2024 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef DYNAMIC_KEY_H_
#define DYNAMIC_KEY_H_

#include "keyboard.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DYNAMIC_KEY_NUM 32
typedef enum __DynamicKeyType
{
    DYNAMIC_KEY_NONE,
    DYNAMIC_KEY_STROKE,
    DYNAMIC_KEY_MOD_TAP,
    DYNAMIC_KEY_TOGGLE_KEY,
    DYNAMIC_KEY_MUTEX,
    DYNAMIC_KEY_TYPE_NUM
} DynamicKeyType;

enum
{
    DYNAMIC_KEY_ACTION_TAP,
    DYNAMIC_KEY_ACTION_HOLD,
    DYNAMIC_KEY_ACTION_NUM,
};

enum 
{
    DKS_RELEASE = 0,
    DKS_TAP = 1,
    DKS_HOLD = 3,
};
#define DKS_KEY_CONTROL(a, b, c, d) \
    (((a) & 0x03) | (((b) & 0x03) << 2) | (((c) & 0x03) << 4) | (((d) & 0x03) << 6))
typedef struct __DynamicKeyStroke4x4
{
    uint32_t type;
    Keycode key_binding[4];
    uint8_t key_control[4];
    AnalogValue press_begin_distance;
    AnalogValue press_fully_distance;
    AnalogValue release_begin_distance;
    AnalogValue release_fully_distance;
    uint16_t key_id;
    AnalogValue value;
    uint32_t key_end_time[4];
    uint8_t key_state;
} DynamicKeyStroke4x4;

typedef struct __DynamicKeyModTap
{
    uint32_t type;
    Keycode key_binding[2];
    uint32_t duration;
    uint16_t key_id;
    uint32_t begin_time;
    uint32_t end_time;
    uint8_t state;
    uint8_t key_state;
    uint8_t key_report_state;
} DynamicKeyModTap;

typedef struct __DynamicKeyToggleKey
{
    uint32_t type;
    Keycode key_binding;
    uint16_t key_id;
    uint8_t state;
    uint8_t key_state;
    uint8_t key_report_state;
} DynamicKeyToggleKey;

typedef enum __DynamicKeyMutexMode
{
    DK_MUTEX_DISTANCE_PRIORITY,
    DK_MUTEX_LAST_PRIORITY,
    DK_MUTEX_KEY1_PRIORITY,
    DK_MUTEX_KEY2_PRIORITY,
    DK_MUTEX_NEUTRAL
} DynamicKeyMutexMode;

typedef struct __DynamicKeyMutex
{
    uint32_t type;
    Keycode key_binding[2];
    uint16_t key_id[2];
    uint8_t mode;
    uint8_t key_state[2];
    uint8_t key_report_state[2];
} DynamicKeyMutex;

typedef union __DynamicKey
{
    uint32_t type;
    DynamicKeyStroke4x4 dks;
    DynamicKeyModTap mt;
    DynamicKeyToggleKey tk;
    DynamicKeyMutex m;
    uint32_t aligned_buffer[15];
} DynamicKey;

#ifdef DYNAMICKEY_ENABLE
extern DynamicKey g_dynamic_keys[DYNAMIC_KEY_NUM];
#endif

void dynamic_key_process(void);
void dynamic_key_add_buffer(void);
void dynamic_key_s_process (DynamicKeyStroke4x4*dynamic_key);
void dynamic_key_mt_process(DynamicKeyModTap*dynamic_key);
void dynamic_key_tk_process(DynamicKeyToggleKey*dynamic_key);
void dynamic_key_m_process (DynamicKeyMutex*dynamic_key);

#ifdef __cplusplus
}
#endif

#endif