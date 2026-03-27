/*
 * Copyright (c) 2024 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include"storage.h"
#include"layer.h"
#include"driver.h"
#ifdef RGB_ENABLE
#include"rgb.h"
#endif
#ifdef SCRIPT_ENABLE
#include"script.h"
#endif
#include "file_stream.h"
#include "string.h"

#if defined(STORAGE_ENABLE) && (!defined(LFS_ENABLE))
#error "STORAGE_ENABLE requires LFS_ENABLE"
#endif

#ifndef STORAGE_FLASH_BASE_ADDRESS
#define STORAGE_FLASH_BASE_ADDRESS 0x00000000
#endif

#define STORAGE_FLASH_RESERVED_SIZE 0x0400

#define STORAGE_ADVANCED_KEY_CONFIG_SIZE (sizeof(AdvancedKeyConfigurationNormalized) * ADVANCED_KEY_NUM)
#define STORAGE_KEYMAP_SIZE (sizeof(g_keymap))
#ifdef RGB_ENABLE
#define STORAGE_RGB_CONFIG_SIZE (sizeof(g_rgb_base_config) +  sizeof(g_rgb_configs))
#else
#define STORAGE_RGB_CONFIG_SIZE 0
#endif
#ifdef DYNAMICKEY_ENABLE
#define STORAGE_DYNAMIC_KEY_CONFIG_SIZE (sizeof(g_dynamic_keys))
#else
#define STORAGE_DYNAMIC_KEY_CONFIG_SIZE 0
#endif

#define STORAGE_CONFIG_FILE_SIZE (STORAGE_ADVANCED_KEY_CONFIG_SIZE + STORAGE_KEYMAP_SIZE + STORAGE_RGB_CONFIG_SIZE + STORAGE_DYNAMIC_KEY_CONFIG_SIZE)
#define STORAGE_CONFIG_FILE_ADDRESS(n) (STORAGE_FLASH_BASE_ADDRESS + STORAGE_FLASH_RESERVED_SIZE + ((n) * sizeof(STORAGE_CONFIG_FILE_SIZE)))

uint8_t g_current_profile_index = 0;

#ifndef LFS_ENABLE
static inline int config_file_read(uint8_t *buffer, uint32_t size, uint32_t offset)
{
    return flash_read(STORAGE_CONFIG_FILE_ADDRESS(g_current_profile_index) + offset, size, buffer);
}

static inline int config_file_write(const uint8_t *buffer, uint32_t size, uint32_t offset)
{
    int res = flash_erase(STORAGE_CONFIG_FILE_ADDRESS(g_current_profile_index) + offset, size);
    if (res)
    {
        return res;
    }
    return flash_write(STORAGE_CONFIG_FILE_ADDRESS(g_current_profile_index) + offset, size, buffer);
}
#endif

void advanced_key_config_normalize(AdvancedKeyConfigurationNormalized* buffer, const AdvancedKeyConfiguration* config)
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

void advanced_key_config_anti_normalize(AdvancedKeyConfiguration* config, const AdvancedKeyConfigurationNormalized* buffer)
{
    config->mode = buffer->mode;
    config->calibration_mode = buffer->calibration_mode;
    config->activation_value = A_ANTI_NORM(buffer->activation_value);
    config->deactivation_value = A_ANTI_NORM(buffer->deactivation_value);
    config->trigger_distance = A_ANTI_NORM(buffer->trigger_distance);
    config->release_distance = A_ANTI_NORM(buffer->release_distance);
    config->trigger_speed = A_ANTI_NORM(buffer->trigger_speed);
    config->release_speed = A_ANTI_NORM(buffer->release_speed);
    config->upper_deadzone = A_ANTI_NORM(buffer->upper_deadzone);
    config->lower_deadzone = A_ANTI_NORM(buffer->lower_deadzone);
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
    buffer->key_id = dks->key_id;
}

void dynamic_key_stroke_anti_normalize(DynamicKeyStroke4x4* dks, DynamicKeyStroke4x4Normalized* buffer)
{
    memcpy(dks, buffer, offsetof(DynamicKeyStroke4x4,press_begin_distance));
    dks->press_begin_distance = A_ANTI_NORM(buffer->press_begin_distance);
    dks->press_fully_distance = A_ANTI_NORM(buffer->press_fully_distance);
    dks->release_begin_distance = A_ANTI_NORM(buffer->release_begin_distance);
    dks->release_fully_distance = A_ANTI_NORM(buffer->release_fully_distance);
    dks->key_id = buffer->key_id;
}

static inline void save_advanced_key_config(FileStream *file, AdvancedKey* key)
{
    AdvancedKeyConfigurationNormalized buffer;
    advanced_key_config_normalize(&buffer, &key->config);
    fs_write(((void *)(&buffer)), sizeof(AdvancedKeyConfigurationNormalized), 1, file);
}

static inline void read_advanced_key_config(FileStream *file, AdvancedKey* key)
{
    AdvancedKeyConfigurationNormalized buffer;
    fs_read(&buffer, sizeof(AdvancedKeyConfigurationNormalized), 1, file);
    advanced_key_config_anti_normalize(&key->config, &buffer);
    advanced_key_set_range(key, key->config.upper_bound, key->config.lower_bound);
}

int storage_mount(void)
{
    fs_init();
}

int storage_check_version(void)
{
    FileStream *file = fs_open("version", FS_O_RDWR | FS_O_CREAT);
    if (file == NULL)    {
        return 0;
    }
    uint32_t version[3] = {0};
    bool need_factory_reset = false;
    bool need_update = false;
    fs_read(&version, sizeof(version), 1, file);
    if (version[0] != KEYBOARD_VERSION_MAJOR || version[1] != KEYBOARD_VERSION_MINOR)
    {
        version[0] = KEYBOARD_VERSION_MAJOR;
        version[1] = KEYBOARD_VERSION_MINOR;
        need_factory_reset = true;
        need_update = true;
    }
    if (version[2] != KEYBOARD_VERSION_PATCH)
    {
        version[2] = KEYBOARD_VERSION_PATCH;
        need_update = true;
    }
    if (need_update)
    {
        fs_rewind(file);
        fs_write(&version, sizeof(version), 1, file);
    }
    fs_close(file);
    return need_factory_reset;
}

void storage_unmount(void)
{

}

uint8_t storage_read_profile_index(void)
{
    FileStream *file = fs_open("profile_index", FS_O_RDWR | FS_O_CREAT);
    if (file == NULL)
    {
        g_current_profile_index = 0;
        return 0;
    }
    uint8_t index = 0;
    fs_read(&index, sizeof(index), 1, file);
    fs_close(file);
    if (index >= STORAGE_PROFILE_FILE_NUM)
    {        
        g_current_profile_index = 0;
        return 0;
    }
    g_current_profile_index = index;
    return index;
}

void storage_save_profile_index(void)
{
    FileStream *file = fs_open("profile_index", FS_O_RDWR | FS_O_CREAT);
    if (file == NULL)
    {
        return;
    }
    fs_write(&g_current_profile_index, sizeof(g_current_profile_index), 1, file);
    fs_close(file);
}

void storage_read_profile(void)
{
    char config_file_name[16] = "profile0";
    config_file_name[6] = g_current_profile_index + '0';
    FileStream *file = fs_open(config_file_name, FS_O_RDWR | FS_O_CREAT);
    if (file == NULL)    {
        return;
    }
    for (int i = 0; i < ADVANCED_KEY_NUM; i++)
    {
        read_advanced_key_config(file, &g_keyboard_advanced_keys[i]);
    }
    fs_read(g_keymap, sizeof(g_keymap), 1, file);
    layer_cache_refresh();
#ifdef RGB_ENABLE
    fs_read(&g_rgb_base_config, sizeof(g_rgb_base_config), 1, file);
    fs_read(&g_rgb_configs, sizeof(g_rgb_configs), 1, file);
#endif
#ifdef DYNAMICKEY_ENABLE
    for (int i = 0; i < DYNAMIC_KEY_NUM; i++)
    {
        DynamicKey buffer;
        fs_read(&buffer, sizeof(DynamicKey), 1, file);
        switch (buffer.type)
        {
        case DYNAMIC_KEY_STROKE:
            dynamic_key_stroke_anti_normalize((DynamicKeyStroke4x4*)&g_dynamic_keys[i], (DynamicKeyStroke4x4Normalized*)&buffer);
            break;
        default:        
            memcpy(&g_dynamic_keys[i], &buffer, sizeof(DynamicKey));
            break;
        }
    }
#endif
    fs_close(file);
}

void storage_save_profile(void)
{
    char config_file_name[16] = "profile0";
    config_file_name[6] = g_current_profile_index + '0';
    FileStream *file = fs_open(config_file_name, FS_O_RDWR | FS_O_CREAT);
    if (file == NULL)
    {
        return;
    }
    for (uint8_t i = 0; i < ADVANCED_KEY_NUM; i++)
    {
        save_advanced_key_config(file, &g_keyboard_advanced_keys[i]);
    }
    fs_write(g_keymap, sizeof(g_keymap), 1, file);
#ifdef RGB_ENABLE
    fs_write(&g_rgb_base_config, sizeof(g_rgb_base_config), 1, file);
    fs_write(&g_rgb_configs, sizeof(g_rgb_configs), 1, file);
#endif
#ifdef DYNAMICKEY_ENABLE
    for (uint8_t i = 0; i < DYNAMIC_KEY_NUM; i++)
    {
        switch (g_dynamic_keys[i].type)
        {
        case DYNAMIC_KEY_STROKE:
        {
            DynamicKeyStroke4x4Normalized buffer;
            dynamic_key_stroke_normalize(&buffer, (DynamicKeyStroke4x4*)&g_dynamic_keys[i]);
            fs_write(&buffer, sizeof(DynamicKey), 1, file);
            break;
        }
        default:
            fs_write(&g_dynamic_keys[i], sizeof(DynamicKey), 1, file);
            break;
        }
    }
#endif
    fs_close(file);
}

void storage_save_script(void)
{
#ifdef SCRIPT_ENABLE
#if SCRIPT_RUNTIME_STRATEGY == SCRIPT_AOT
    {
        FileStream *file = fs_open("main.bin", FS_O_RDWR | FS_O_CREAT);
        if (file != NULL)
        {
            fs_write(g_script_bytecode_buffer, sizeof(g_script_bytecode_buffer), 1, file);
            fs_close(file);
        }
    }
#endif
#if SCRIPT_RUNTIME_STRATEGY == SCRIPT_JIT
    {

        FileStream *file = fs_open("main.js", FS_O_RDWR | FS_O_CREAT);
        if (file != NULL)
        {
            fs_write(g_script_source_buffer, sizeof(g_script_source_buffer), 1, file);
            fs_close(file);
        }
    }
#endif
#endif
}

void storage_read_script(void)
{
#ifdef SCRIPT_ENABLE
#if SCRIPT_RUNTIME_STRATEGY == SCRIPT_AOT
    {

        FileStream *file = fs_open("main.bin", FS_O_RDWR | FS_O_CREAT);
        if (file != NULL)
        {
            fs_read(g_script_bytecode_buffer, sizeof(g_script_bytecode_buffer), 1, file);
            fs_close(file);
        }
    }
#endif
#if SCRIPT_RUNTIME_STRATEGY == SCRIPT_JIT
    {

        FileStream *file = fs_open("main.js", FS_O_RDWR | FS_O_CREAT);
        if (file != NULL)
        {
            fs_read(g_script_source_buffer, sizeof(g_script_source_buffer), 1, file);
            fs_close(file);
        }
    }
#endif
#endif
}
