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
#include"driver.h"

#ifndef STORAGE_FLASH_BASE_ADDRESS
#define STORAGE_FLASH_BASE_ADDRESS 0x00000000
#endif

#define STORAGE_FLASH_RESERVED_SIZE 0x0400

#ifdef LFS_ENABLE
#ifndef LFS_READ_SIZE
#error "LFS_READ_SIZE is not defined."
#endif
#ifndef LFS_PROG_SIZE
#error "LFS_PROG_SIZE is not defined."
#endif
#ifndef LFS_BLOCK_SIZE
#error "LFS_BLOCK_SIZE is not defined."
#endif
#ifndef LFS_BLOCK_COUNT
#error "LFS_BLOCK_COUNT is not defined."
#endif
#ifndef LFS_CACHE_SIZE
#define LFS_CACHE_SIZE 16
#endif
#ifndef LFS_LOOKAHEAD_SIZE
#error "LFS_READ_SIZE is not defined."
#endif
#ifndef LFS_BLOCK_CYCLES
#error "LFS_READ_SIZE is not defined."
#endif
#endif

#define STORAGE_ADVANCED_KEY_CONFIG_SIZE (sizeof(AdvancedKeyConfigurationNormalized) * ADVANCED_KEY_NUM)
#define STORAGE_KEYMAP_SIZE (sizeof(g_keymap))
#ifdef RGB_ENABLE
#define STORAGE_RGB_CONFIG_SIZE (sizeof(g_rgb_base_config) +  sizeof(g_rgb_configs))
#else
#define STORAGE_RGB_CONFIG_SIZE 0
#endif
#ifdef DYNAMICKEY_ENABLE
#define STORAGE_DYNAMIC_KEY_CONFIG_SIZE (sizeof(g_keyboard_dynamic_keys))
#else
#define STORAGE_DYNAMIC_KEY_CONFIG_SIZE 0
#endif

#define STORAGE_CONFIG_FILE_SIZE (STORAGE_ADVANCED_KEY_CONFIG_SIZE + STORAGE_KEYMAP_SIZE + STORAGE_RGB_CONFIG_SIZE + STORAGE_DYNAMIC_KEY_CONFIG_SIZE)
#define STORAGE_CONFIG_FILE_ADDRESS(n) (STORAGE_FLASH_BASE_ADDRESS + STORAGE_FLASH_RESERVED_SIZE + ((n) * sizeof(STORAGE_CONFIG_FILE_SIZE)))

uint8_t g_current_config_index = 0;

static int lfs_flash_read(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void *buffer, lfs_size_t size)
{
    return flash_read(c->block_size * block + off, size, (uint8_t *)buffer);
}

static int lfs_flash_prog(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, const void *buffer, lfs_size_t size)
{
    return flash_write(c->block_size * block + off, size, (uint8_t *)buffer);
}

static int lfs_flash_erase(const struct lfs_config *c, lfs_block_t block)
{
    return flash_erase((block * c->block_size), c->block_size);
}

static int _sync(const struct lfs_config *c)
{
    UNUSED(c);
    return LFS_ERR_OK;
}

#ifdef LFS_ENABLE
static uint8_t read_buffer[LFS_CACHE_SIZE];
static uint8_t prog_buffer[LFS_CACHE_SIZE];
static uint8_t lookahead_buffer[LFS_CACHE_SIZE];
static lfs_t lfs;
static const struct lfs_config _lfs_config =
{
    // block device operations
    .read  = lfs_flash_read,
    .prog  = lfs_flash_prog,
    .erase = lfs_flash_erase,
    .sync  = _sync,

    // block device configuration
    .read_size = LFS_READ_SIZE,
    .prog_size = LFS_PROG_SIZE,
    .block_size = LFS_BLOCK_SIZE,
    .block_count = LFS_BLOCK_COUNT,

    .cache_size = LFS_CACHE_SIZE,
    .lookahead_size = LFS_LOOKAHEAD_SIZE,
    .block_cycles = LFS_BLOCK_CYCLES,

    .read_buffer = read_buffer,
    .prog_buffer = prog_buffer,
    .lookahead_buffer = lookahead_buffer,
};

lfs_t * storage_get_lfs(void)
{
    return &lfs;
}

struct lfs_config * storage_get_lfs_config(void)
{
    return &_lfs_config;
}
#endif

#ifndef LFS_ENABLE
static inline int config_file_read(uint8_t *buffer, uint32_t size, uint32_t offset)
{
    return flash_read(STORAGE_CONFIG_FILE_ADDRESS(g_current_config_index) + offset, size, buffer);
}

static inline int config_file_write(const uint8_t *buffer, uint32_t size, uint32_t offset)
{
    return flash_write(STORAGE_CONFIG_FILE_ADDRESS(g_current_config_index) + offset, size, buffer);
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
#ifdef LFS_ENABLE
    // mount the filesystem
    int err = lfs_mount(&lfs, &_lfs_config);
    // reformat if we can't mount the filesystem
    // this should only happen on the first boot
    if (err)
    {
        lfs_format(&lfs, &_lfs_config);
        lfs_mount(&lfs, &_lfs_config);
    }
    return err;
#endif
}

void storage_unmount(void)
{
#ifdef LFS_ENABLE
    lfs_unmount(&lfs);
#endif
}

uint8_t storage_read_config_index(void)
{
#ifdef LFS_ENABLE
    lfs_file_t lfs_file;
    uint8_t index = 0;
    lfs_file_open(&lfs, &lfs_file, "config_index", LFS_O_RDWR | LFS_O_CREAT);
    lfs_file_rewind(&lfs, &lfs_file);
    lfs_file_read(&lfs, &lfs_file, &index, sizeof(index));
    lfs_file_close(&lfs, &lfs_file);
    if (index >= STORAGE_CONFIG_FILE_NUM)
    {
        index = 0;
    }
    g_current_config_index = index;
    return index;
#else
    flash_read(STORAGE_FLASH_BASE_ADDRESS, sizeof(g_current_config_index), (uint8_t *)&g_current_config_index);
#endif
}

void storage_save_config_index(void)
{
#ifdef LFS_ENABLE
    lfs_file_t lfs_file;
    lfs_file_open(&lfs, &lfs_file, "config_index", LFS_O_RDWR | LFS_O_CREAT);
    lfs_file_rewind(&lfs, &lfs_file);
    lfs_file_write(&lfs, &lfs_file, &g_current_config_index, sizeof(g_current_config_index));
    lfs_file_close(&lfs, &lfs_file);
#else
    flash_write(STORAGE_FLASH_BASE_ADDRESS, sizeof(g_current_config_index), (uint8_t *)&g_current_config_index);
#endif
}

void storage_read_config(void)
{
#ifdef LFS_ENABLE
    lfs_file_t lfs_file;
    char config_file_name[8] = "config0";
    config_file_name[6] = g_current_config_index + '0';

    lfs_file_open(&lfs, &lfs_file, config_file_name, LFS_O_RDWR | LFS_O_CREAT);
    lfs_file_rewind(&lfs, &lfs_file);
    for (uint8_t i = 0; i < ADVANCED_KEY_NUM; i++)
    {
        read_advanced_key_config(&lfs, &lfs_file, &g_keyboard_advanced_keys[i]);
    }
    lfs_file_read(&lfs, &lfs_file, g_keymap, sizeof(g_keymap));
    layer_cache_refresh();
#ifdef RGB_ENABLE
    lfs_file_read(&lfs, &lfs_file, &g_rgb_base_config, sizeof(g_rgb_base_config));
    lfs_file_read(&lfs, &lfs_file, &g_rgb_configs, sizeof(g_rgb_configs));
#endif
#ifdef DYNAMICKEY_ENABLE
    for (uint8_t i = 0; i < DYNAMIC_KEY_NUM; i++)
    {
        DynamicKey buffer;
        lfs_file_read(&lfs, &lfs_file, &buffer, sizeof(DynamicKey));
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
#endif
    // remember the storage is not updated until the file is closed successfully
    lfs_file_close(&lfs, &lfs_file);
#else
    uint32_t offset = 0;
    for (uint8_t i = 0; i < ADVANCED_KEY_NUM; i++)
    {
        AdvancedKey *key = &g_keyboard_advanced_keys[i];
        AdvancedKeyConfigurationNormalized buffer;
        config_file_read((uint8_t *)&buffer,
            sizeof(AdvancedKeyConfigurationNormalized), offset);
        advanced_key_config_anti_normalize(&key->config, &buffer);
        advanced_key_set_range(key, key->config.upper_bound, key->config.lower_bound);
        offset += sizeof(AdvancedKeyConfigurationNormalized);
    }

    config_file_read((uint8_t*)g_keymap, sizeof(g_keymap), offset);
    layer_cache_refresh();
    offset += sizeof(g_keymap);

#ifdef RGB_ENABLE
    config_file_read((uint8_t*)&g_rgb_base_config, sizeof(g_rgb_base_config), offset);
    offset += sizeof(g_rgb_base_config);

    config_file_read((uint8_t*)&g_rgb_configs, sizeof(g_rgb_configs), offset);
    offset += sizeof(g_rgb_configs);
#endif

#ifdef DYNAMICKEY_ENABLE
    for (uint8_t i = 0; i < DYNAMIC_KEY_NUM; i++)
    {
        DynamicKey buffer;
        config_file_read((uint8_t*)&buffer, sizeof(DynamicKey), offset);
        switch (buffer.type)
        {
        case DYNAMIC_KEY_STROKE:
            dynamic_key_stroke_anti_normalize((DynamicKeyStroke4x4*)&g_keyboard_dynamic_keys[i], (DynamicKeyStroke4x4Normalized*)&buffer);
            break;
        default:
            memcpy(&g_keyboard_dynamic_keys[i], &buffer, sizeof(DynamicKey));
            break;
        }
        offset += sizeof(DynamicKey);
    }
#endif
#endif
}

void storage_save_config(void)
{
#ifdef LFS_ENABLE
    lfs_file_t lfs_file;
    char config_file_name[8] = "config0";
    config_file_name[6] = g_current_config_index + '0';

    lfs_file_open(&lfs, &lfs_file, config_file_name, LFS_O_RDWR | LFS_O_CREAT);
    lfs_file_rewind(&lfs, &lfs_file);
    for (uint8_t i = 0; i < ADVANCED_KEY_NUM; i++)
    {
        save_advanced_key_config(&lfs, &lfs_file, &g_keyboard_advanced_keys[i]);
    }
    lfs_file_write(&lfs, &lfs_file, g_keymap, sizeof(g_keymap));
#ifdef RGB_ENABLE
    lfs_file_write(&lfs, &lfs_file, &g_rgb_base_config, sizeof(g_rgb_base_config));
    lfs_file_write(&lfs, &lfs_file, &g_rgb_configs, sizeof(g_rgb_configs));
#endif
#ifdef DYNAMICKEY_ENABLE
    for (uint8_t i = 0; i < DYNAMIC_KEY_NUM; i++)
    {
        switch (g_keyboard_dynamic_keys[i].type)
        {
        case DYNAMIC_KEY_STROKE:
            DynamicKeyStroke4x4Normalized buffer;
            dynamic_key_stroke_normalize(&buffer, (DynamicKeyStroke4x4*)&g_keyboard_dynamic_keys[i]);
            lfs_file_write(&lfs, &lfs_file, &buffer, sizeof(DynamicKey));
            break;
        default:
            lfs_file_write(&lfs, &lfs_file, &g_keyboard_dynamic_keys[i], sizeof(DynamicKey));
            break;
        }
    }
#endif
    // remember the storage is not updated until the file is closed successfully
    lfs_file_close(&lfs, &lfs_file);
#else
    uint32_t offset = 0;

    for (uint8_t i = 0; i < ADVANCED_KEY_NUM; i++)
    {
        const AdvancedKey *key = &g_keyboard_advanced_keys[i];
        AdvancedKeyConfigurationNormalized buffer;
        advanced_key_config_normalize(&buffer, &key->config);
        config_file_write(((void *)(&buffer)),
            sizeof(AdvancedKeyConfigurationNormalized), offset);
        offset += sizeof(AdvancedKeyConfigurationNormalized);
    }

    config_file_write((uint8_t*)g_keymap, sizeof(g_keymap), offset);
    layer_cache_refresh();
    offset += sizeof(g_keymap);

#ifdef RGB_ENABLE
    config_file_write((uint8_t*)&g_rgb_base_config, sizeof(g_rgb_base_config), offset);
    offset += sizeof(g_rgb_base_config);

    config_file_write((uint8_t*)&g_rgb_configs, sizeof(g_rgb_configs), offset);
    offset += sizeof(g_rgb_configs);
#endif

#ifdef DYNAMICKEY_ENABLE
    for (uint8_t i = 0; i < DYNAMIC_KEY_NUM; i++)
    {
        switch (g_keyboard_dynamic_keys[i].type)
        {
        case DYNAMIC_KEY_STROKE:
            DynamicKeyStroke4x4Normalized buffer;
            dynamic_key_stroke_normalize(&buffer, (DynamicKeyStroke4x4*)&g_keyboard_dynamic_keys[i]);
            config_file_write((uint8_t*)&buffer, sizeof(DynamicKey), offset);
            break;
        default:
            config_file_write((uint8_t*)&g_keyboard_dynamic_keys[i], sizeof(DynamicKey), offset);
            break;
        }
        offset += sizeof(DynamicKey);
    }
#endif
#endif
}
