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

#define STORAGE_ADVANCED_KEY_CONFIG_SIZE (sizeof(AdvancedKeyConfiguration) * ADVANCED_KEY_NUM)
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

static inline void save_advanced_key_config(FileStream *file, AdvancedKey* key)
{
    fs_write(((void *)(&key->config)), sizeof(AdvancedKeyConfiguration), 1, file);
}

static inline void read_advanced_key_config(FileStream *file, AdvancedKey* key)
{
    fs_read(&key->config, sizeof(AdvancedKeyConfiguration), 1, file);
    advanced_key_set_range(key, key->config.upper_bound, key->config.lower_bound);
}

int storage_mount(void)
{
    return fs_init();
}

int storage_check_version(void)
{
    FileStream file;
    int res = fs_open(&file, "system/version", FS_O_RDWR | FS_O_CREAT);
    if (res < 0)    {
        return 0;
    }
    uint32_t version[3] = {0};
    bool need_factory_reset = false;
    bool need_update = false;
    fs_read(&version, sizeof(version), 1, &file);
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
        fs_rewind(&file);
        fs_write(&version, sizeof(version), 1, &file);
    }
    fs_close(&file);
    return need_factory_reset;
}

void storage_unmount(void)
{

}

uint8_t storage_read_profile_index(void)
{
    FileStream file;
    int res = fs_open(&file, "system/profile_index", FS_O_RDWR | FS_O_CREAT);
    if (res < 0)
    {
        g_current_profile_index = 0;
        return 0;
    }
    uint8_t index = 0;
    fs_read(&index, sizeof(index), 1, &file);
    fs_close(&file);
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
    FileStream file;
    int res = fs_open(&file, "system/profile_index", FS_O_RDWR | FS_O_CREAT);
    if (res < 0)
    {
        return;
    }
    fs_write(&g_current_profile_index, sizeof(g_current_profile_index), 1, &file);
    fs_close(&file);
}

void storage_read_profile(void)
{
    char config_file_name[] = "profiles/profile0";
    config_file_name[sizeof(config_file_name) - 2] = g_current_profile_index + '0';
    FileStream file;
    int res = fs_open(&file, config_file_name, FS_O_RDWR | FS_O_CREAT);
    if (res < 0)    {
        return;
    }
    for (int i = 0; i < ADVANCED_KEY_NUM; i++)
    {
        read_advanced_key_config(&file, &g_keyboard_advanced_keys[i]);
    }
    fs_read(g_keymap, sizeof(g_keymap), 1, &file);
    layer_cache_refresh();
#ifdef RGB_ENABLE
    fs_read(&g_rgb_base_config, sizeof(g_rgb_base_config), 1, &file);
    fs_read(&g_rgb_configs, sizeof(g_rgb_configs), 1, &file);
#endif
#ifdef DYNAMICKEY_ENABLE
    for (int i = 0; i < DYNAMIC_KEY_NUM; i++)
    {
        fs_read(&g_dynamic_keys[i], sizeof(DynamicKey), 1, &file);
    }
#endif
    fs_close(&file);
}

void storage_save_profile(void)
{
    char config_file_name[] = "profiles/profile0";
    config_file_name[sizeof(config_file_name) - 2] = g_current_profile_index + '0';
    FileStream file;
    int res = fs_open(&file, config_file_name, FS_O_RDWR | FS_O_CREAT);
    if (res < 0)
    {
        return;
    }
    for (uint8_t i = 0; i < ADVANCED_KEY_NUM; i++)
    {
        save_advanced_key_config(&file, &g_keyboard_advanced_keys[i]);
    }
    fs_write(g_keymap, sizeof(g_keymap), 1, &file);
#ifdef RGB_ENABLE
    fs_write(&g_rgb_base_config, sizeof(g_rgb_base_config), 1, &file);
    fs_write(&g_rgb_configs, sizeof(g_rgb_configs), 1, &file);
#endif
#ifdef DYNAMICKEY_ENABLE
    for (uint8_t i = 0; i < DYNAMIC_KEY_NUM; i++)
    {
        fs_write(&g_dynamic_keys[i], sizeof(DynamicKey), 1, &file);
        break;
    }
#endif
    fs_close(&file);
}

void storage_save_script(void)
{
#ifdef SCRIPT_ENABLE
#if SCRIPT_RUNTIME_STRATEGY == SCRIPT_AOT
    {
        FileStream file;
        int res = fs_open(&file, "scripts/main.bin", FS_O_RDWR | FS_O_CREAT);
        if (res >= 0)
        {
            fs_write(g_script_bytecode_buffer, sizeof(g_script_bytecode_buffer), 1, &file);
            fs_close(&file);
        }
    }
#endif
#if SCRIPT_RUNTIME_STRATEGY == SCRIPT_JIT
    {

        FileStream file;
        int res = fs_open(&file, "scripts/main.js", FS_O_RDWR | FS_O_CREAT);
        if (res >= 0)
        {
            fs_write(g_script_source_buffer, sizeof(g_script_source_buffer), 1, &file);
            fs_close(&file);
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

        FileStream file;
        int res = fs_open(&file, "scripts/main.bin", FS_O_RDWR | FS_O_CREAT);
        if (res >= 0)
        {
            fs_read(g_script_bytecode_buffer, sizeof(g_script_bytecode_buffer), 1, &file);
            fs_close(&file);
        }
    }
#endif
#if SCRIPT_RUNTIME_STRATEGY == SCRIPT_JIT
    {

        FileStream file;
        int res = fs_open(&file, "scripts/main.js", FS_O_RDWR | FS_O_CREAT);
        if (res >= 0)
        {
            fs_read(g_script_source_buffer, sizeof(g_script_source_buffer), 1, &file);
            fs_close(&file);
        }
    }
#endif
#endif
}
