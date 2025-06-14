/*
 * Copyright (c) 2024 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include"storage.h"
#include"lfs.h"
#include"keyboard.h"
#include"rgb.h"
#include"layer.h"

void advanced_key_config_normalize(AdvancedKeyConfigurationNormalized* buffer, AdvancedKeyConfiguration* config)
{
    buffer->mode = config->mode;
    buffer->calibration_mode = config->calibration_mode;
    buffer->activation_value = A_NORM(config->activation_value);
    buffer->deactivation_value = A_NORM(config->deactivation_value);
    buffer->trigger_distance = A_NORM(config->trigger_distance);
    buffer->release_distance = A_NORM(config->release_distance);
    buffer->trigger_speed = A_NORM(config->trigger_speed);
    buffer->release_speed = A_NORM(config->release_speed);
    buffer->upper_deadzone = A_NORM(config->upper_deadzone);
    buffer->lower_deadzone = A_NORM(config->lower_deadzone);
    buffer->upper_bound = config->upper_bound;
    buffer->lower_bound = config->lower_bound;
}

void advanced_key_config_anti_normalize(AdvancedKeyConfiguration* config, AdvancedKeyConfigurationNormalized* buffer)
{
    config->mode = buffer->mode;
    config->calibration_mode = buffer->calibration_mode;
    config->activation_value = A_ANIT_NORM(buffer->activation_value);
    config->deactivation_value = A_ANIT_NORM(buffer->deactivation_value);
    config->trigger_distance = A_ANIT_NORM(buffer->trigger_distance);
    config->release_distance = A_ANIT_NORM(buffer->release_distance);
    config->trigger_speed = A_ANIT_NORM(buffer->trigger_speed);
    config->release_speed = A_ANIT_NORM(buffer->release_speed);
    config->upper_deadzone = A_ANIT_NORM(buffer->upper_deadzone);
    config->lower_deadzone = A_ANIT_NORM(buffer->lower_deadzone);
    config->upper_bound = buffer->upper_bound;
    config->lower_bound = buffer->lower_bound;
}

void dynamic_key_stroke_normalize(DynamicKeyStroke4x4Normalized* buffer, DynamicKeyStroke4x4* dks)
{
    memcpy(buffer, dks, offsetof(DynamicKeyStroke4x4,press_begin_distance));
    buffer->press_begin_distance = A_NORM(dks->press_begin_distance);
    buffer->press_fully_distance = A_NORM(dks->press_fully_distance);
    buffer->release_begin_distance = A_NORM(dks->release_begin_distance);
    buffer->release_fully_distance = A_NORM(dks->release_fully_distance);
    //memcpy(buffer->key_end_time, dks->key_end_time, sizeof(DynamicKeyStroke4x4) - offsetof(DynamicKeyStroke4x4,key_end_time));
}

void dynamic_key_stroke_anti_normalize(DynamicKeyStroke4x4* dks, DynamicKeyStroke4x4Normalized* buffer)
{
    memcpy(dks, buffer, offsetof(DynamicKeyStroke4x4,press_begin_distance));
    dks->press_begin_distance = A_ANIT_NORM(buffer->press_begin_distance);
    dks->press_fully_distance = A_ANIT_NORM(buffer->press_fully_distance);
    dks->release_begin_distance = A_ANIT_NORM(buffer->release_begin_distance);
    dks->release_fully_distance = A_ANIT_NORM(buffer->release_fully_distance);
    //memcpy(dks->key_end_time, buffer->key_end_time, sizeof(DynamicKeyStroke4x4) - offsetof(DynamicKeyStroke4x4,key_end_time));
}

static inline void save_advanced_key_config(lfs_t *lfs, lfs_file_t *file, AdvancedKey* key)
{
    AdvancedKeyConfigurationNormalized buffer;
    advanced_key_config_normalize(&buffer, &key->config);
    lfs_file_write(lfs, file, ((void *)(&buffer)),
        sizeof(AdvancedKeyConfigurationNormalized));
}

static inline void read_advanced_key_config(lfs_t *lfs, lfs_file_t *file, AdvancedKey* key)
{
    AdvancedKeyConfigurationNormalized buffer;
    lfs_file_read(lfs, file, &buffer,
        sizeof(AdvancedKeyConfigurationNormalized));
    advanced_key_config_anti_normalize(&key->config, &buffer);
    advanced_key_set_range(key, key->config.upper_bound, key->config.lower_bound);\
}

int storage_mount(void)
{
    // mount the filesystem
    int err = lfs_mount(&g_lfs, &g_lfs_config);
    // reformat if we can't mount the filesystem
    // this should only happen on the first boot
    if (err)
    {
        lfs_format(&g_lfs, &g_lfs_config);
        lfs_mount(&g_lfs, &g_lfs_config);
    }
    return err;
}

void storage_unmount(void)
{
    lfs_unmount(&g_lfs);
}

uint8_t storage_read_config_index(void)
{
    lfs_file_t lfs_file;
    uint8_t index = 0;
    lfs_file_open(&g_lfs, &lfs_file, "config_index", LFS_O_RDWR | LFS_O_CREAT);
    lfs_file_rewind(&g_lfs, &lfs_file);
    lfs_file_read(&g_lfs, &lfs_file, &index, sizeof(index));
    lfs_file_close(&g_lfs, &lfs_file);
    if (index >= STORAGE_CONFIG_FILE_NUM)
    {
        index = 0;
    }
    return index;
}

void storage_save_config_index(uint8_t index)
{
    lfs_file_t lfs_file;
    lfs_file_open(&g_lfs, &lfs_file, "config_index", LFS_O_RDWR | LFS_O_CREAT);
    lfs_file_rewind(&g_lfs, &lfs_file);
    lfs_file_write(&g_lfs, &lfs_file, &index, sizeof(index));
    lfs_file_close(&g_lfs, &lfs_file);
}

void storage_read_config(uint8_t index)
{
    lfs_file_t lfs_file;
    char config_file_name[8] = "config0";
    config_file_name[6] = index + '0';

    lfs_file_open(&g_lfs, &lfs_file, config_file_name, LFS_O_RDWR | LFS_O_CREAT);
    lfs_file_rewind(&g_lfs, &lfs_file);
    for (uint8_t i = 0; i < ADVANCED_KEY_NUM; i++)
    {
        read_advanced_key_config(&g_lfs, &lfs_file, &g_keyboard_advanced_keys[i]);
    }
    lfs_file_read(&g_lfs, &lfs_file, g_keymap, sizeof(g_keymap));
    layer_cache_refresh();
    lfs_file_read(&g_lfs, &lfs_file, &g_rgb_base_config, sizeof(g_rgb_base_config));
    lfs_file_read(&g_lfs, &lfs_file, &g_rgb_configs, sizeof(g_rgb_configs));
    for (uint8_t i = 0; i < DYNAMIC_KEY_NUM; i++)
    {
        DynamicKey buffer;
        lfs_file_read(&g_lfs, &lfs_file, &buffer, sizeof(DynamicKey));
        switch (buffer.type)
        {
        case DYNAMIC_KEY_STROKE:
            dynamic_key_stroke_anti_normalize((DynamicKeyStroke4x4*)&g_keyboard_dynamic_keys[i], (DynamicKeyStroke4x4Normalized*)&buffer);
            break;
        default:
            memcpy(&g_keyboard_dynamic_keys[i], &buffer, sizeof(DynamicKey));
            break;
        }
    }
    // remember the storage is not updated until the file is closed successfully
    lfs_file_close(&g_lfs, &lfs_file);
}

void storage_save_config(uint8_t index)
{
    lfs_file_t lfs_file;
    char config_file_name[8] = "config0";
    config_file_name[6] = index + '0';

    lfs_file_open(&g_lfs, &lfs_file, config_file_name, LFS_O_RDWR | LFS_O_CREAT);
    lfs_file_rewind(&g_lfs, &lfs_file);
    for (uint8_t i = 0; i < ADVANCED_KEY_NUM; i++)
    {
        save_advanced_key_config(&g_lfs, &lfs_file, &g_keyboard_advanced_keys[i]);
    }
    lfs_file_write(&g_lfs, &lfs_file, g_keymap, sizeof(g_keymap));
    lfs_file_write(&g_lfs, &lfs_file, &g_rgb_base_config, sizeof(g_rgb_base_config));
    lfs_file_write(&g_lfs, &lfs_file, &g_rgb_configs, sizeof(g_rgb_configs));
    for (uint8_t i = 0; i < DYNAMIC_KEY_NUM; i++)
    {
        switch (g_keyboard_dynamic_keys[i].type)
        {
        case DYNAMIC_KEY_STROKE:
            DynamicKeyStroke4x4Normalized buffer;
            dynamic_key_stroke_normalize(&buffer, (DynamicKeyStroke4x4*)&g_keyboard_dynamic_keys[i]);
            lfs_file_write(&g_lfs, &lfs_file, &buffer, sizeof(DynamicKey));
            break;
        default:
            lfs_file_write(&g_lfs, &lfs_file, &g_keyboard_dynamic_keys[i], sizeof(DynamicKey));
            break;
        }
    }
    // remember the storage is not updated until the file is closed successfully
    lfs_file_close(&g_lfs, &lfs_file);
}
