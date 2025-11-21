/*
 * Copyright (c) 2024 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef STORAGE_H_
#define STORAGE_H_

#include "keyboard.h"
#ifdef DYNAMICKEY_ENABLE
#include "dynamic_key.h"
#endif

#include "lfs.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef STORAGE_CONFIG_FILE_NUM
#define STORAGE_CONFIG_FILE_NUM 4
#endif

typedef struct __AdvancedKeyConfigurationNormalized
{
    uint8_t mode;
    uint8_t calibration_mode;
    uint8_t reserved[2];
    float activation_value;
    float deactivation_value;
    float trigger_distance;
    float release_distance;
    float trigger_speed;
    float release_speed;
    float upper_deadzone;
    float lower_deadzone;
    float upper_bound;
    float lower_bound;
} AdvancedKeyConfigurationNormalized;

typedef struct __DynamicKeyStroke4x4Normalized
{
    uint32_t type;
    Keycode key_binding[4];
    uint8_t key_control[4];
    float press_begin_distance;
    float press_fully_distance;
    float release_begin_distance;
    float release_fully_distance;
    uint16_t key_id;
    float value;
    uint32_t key_end_tick[4];
    uint8_t key_state;
} DynamicKeyStroke4x4Normalized;

extern uint8_t g_current_config_index;

#ifdef LFS_ENABLE
lfs_t * storage_get_lfs(void);
#endif

void advanced_key_config_normalize(AdvancedKeyConfigurationNormalized* buffer, const AdvancedKeyConfiguration* config);
void advanced_key_config_anti_normalize(AdvancedKeyConfiguration* config, const AdvancedKeyConfigurationNormalized* buffer);
#ifdef DYNAMICKEY_ENABLE
void dynamic_key_stroke_normalize(DynamicKeyStroke4x4Normalized* buffer, DynamicKeyStroke4x4* dks);
void dynamic_key_stroke_anti_normalize(DynamicKeyStroke4x4* dks, DynamicKeyStroke4x4Normalized* buffer);
#endif

int storage_mount(void);
void storage_unmount(void);
uint8_t storage_read_config_index(void);
void storage_save_config_index(void);
void storage_read_config(void);
void storage_save_config(void);

#ifdef __cplusplus
}
#endif

#endif