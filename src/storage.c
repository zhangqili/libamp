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
#include "file_system.h"
#include "config_document.h"
#include "string.h"

#if defined(STORAGE_ENABLE) && (!defined(LFS_ENABLE))
#error "STORAGE_ENABLE requires LFS_ENABLE"
#endif

#ifndef STORAGE_FLASH_BASE_ADDRESS
#define STORAGE_FLASH_BASE_ADDRESS 0x00000000
#endif

#define STORAGE_FLASH_RESERVED_SIZE 0x0400

uint8_t g_current_profile_index = 0;

int storage_mount(void)
{
    return fs_init();
}

int storage_check_version(void)
{
    File file;
    int res = fs_open(&file, "system/version", FS_O_RDWR | FS_O_CREAT);
    if (res < 0)    {
        return 0;
    }
    uint32_t version[4] = {0};
    bool need_factory_reset = false;
    bool need_update = false;
    fs_read(&file, &version, sizeof(version));
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
    if (version[3] != AMP_CONFIG_SCHEMA_VERSION)
    {
        version[3] = AMP_CONFIG_SCHEMA_VERSION;
        need_factory_reset = true;
        need_update = true;
    }
    if (need_update)
    {
        fs_seek(&file, 0, FS_SEEK_SET);
        fs_write(&file, &version, sizeof(version));
    }
    fs_close(&file);
    return need_factory_reset;
}

void storage_unmount(void)
{

}

uint8_t storage_read_profile_index(void)
{
    File file;
    int res = fs_open(&file, "system/profile_index", FS_O_RDWR | FS_O_CREAT);
    if (res < 0)
    {
        g_current_profile_index = 0;
        return 0;
    }
    uint8_t index = 0;
    fs_read(&file, &index, sizeof(index));
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
    File file;
    int res = fs_open(&file, "system/profile_index", FS_O_RDWR | FS_O_CREAT);
    if (res < 0)
    {
        return;
    }
    fs_write(&file, &g_current_profile_index, sizeof(g_current_profile_index));
    fs_close(&file);
}

void storage_read_profile(void)
{
    char path[24];
    if (storage_profile_path(path, sizeof(path), g_current_profile_index))
    {
        (void)amp_config_document_apply(path, g_current_profile_index);
    }
}

void storage_save_profile(void)
{
    char path[24];
    char temporary[28];
    if (!storage_profile_path(path, sizeof(path), g_current_profile_index))
    {
        return;
    }
    memcpy(temporary, path, strlen(path) + 1);
    memcpy(temporary + strlen(path), ".tmp", 5);
    (void)fs_unlink(temporary);
    if (amp_config_document_write(temporary, g_current_profile_index) == 0 &&
        fs_rename(temporary, path) >= 0)
    {
        (void)storage_bump_profile_revision(g_current_profile_index);
    }
    else
    {
        (void)fs_unlink(temporary);
    }
}

bool storage_profile_path(char *buffer, size_t buffer_size, uint16_t profile_id)
{
    static const char prefix[] = "profiles/profile";
    if (buffer == NULL || buffer_size < sizeof(prefix) + 3 ||
        profile_id >= STORAGE_PROFILE_FILE_NUM || profile_id > 999)
    {
        return false;
    }
    size_t index = 0;
    memcpy(buffer, prefix, sizeof(prefix) - 1);
    index = sizeof(prefix) - 1;
    if (profile_id >= 100)
    {
        buffer[index++] = (char)('0' + profile_id / 100);
    }
    if (profile_id >= 10)
    {
        buffer[index++] = (char)('0' + (profile_id / 10) % 10);
    }
    buffer[index++] = (char)('0' + profile_id % 10);
    buffer[index] = '\0';
    return true;
}

static void read_profile_revisions(uint32_t revisions[STORAGE_PROFILE_FILE_NUM])
{
    File file;
    memset(revisions, 0, sizeof(uint32_t) * STORAGE_PROFILE_FILE_NUM);
    if (fs_open(&file, "system/profile_revisions", FS_O_RDONLY) < 0)
    {
        return;
    }
    size_t actual = fs_read(&file, revisions,
                            sizeof(uint32_t) * STORAGE_PROFILE_FILE_NUM);
    (void)fs_close(&file);
    if (actual != sizeof(uint32_t) * STORAGE_PROFILE_FILE_NUM)
    {
        memset(revisions, 0, sizeof(uint32_t) * STORAGE_PROFILE_FILE_NUM);
    }
}

static bool write_profile_revisions(
    const uint32_t revisions[STORAGE_PROFILE_FILE_NUM])
{
    File file;
    if (fs_open(&file, "system/profile_revisions",
                FS_O_WRONLY | FS_O_CREAT | FS_O_TRUNC) < 0)
    {
        return false;
    }
    bool ok = fs_write(&file, (void *)revisions,
                       sizeof(uint32_t) * STORAGE_PROFILE_FILE_NUM) ==
              sizeof(uint32_t) * STORAGE_PROFILE_FILE_NUM;
    ok = ok && fs_sync(&file) >= 0;
    if (fs_close(&file) < 0)
    {
        ok = false;
    }
    return ok;
}

uint32_t storage_get_profile_revision(uint16_t profile_id)
{
    uint32_t revisions[STORAGE_PROFILE_FILE_NUM];
    if (profile_id >= STORAGE_PROFILE_FILE_NUM)
    {
        return 0;
    }
    read_profile_revisions(revisions);
    return revisions[profile_id];
}

uint32_t storage_bump_profile_revision(uint16_t profile_id)
{
    uint32_t revisions[STORAGE_PROFILE_FILE_NUM];
    if (profile_id >= STORAGE_PROFILE_FILE_NUM)
    {
        return 0;
    }
    read_profile_revisions(revisions);
    revisions[profile_id]++;
    if (revisions[profile_id] == 0)
    {
        revisions[profile_id] = 1;
    }
    return write_profile_revisions(revisions) ? revisions[profile_id] : 0;
}

void storage_save_script(void)
{
#ifdef SCRIPT_ENABLE
#if SCRIPT_RUNTIME_STRATEGY == SCRIPT_AOT
    {
        File file;
        int res = fs_open(&file, "scripts/main.bin", FS_O_RDWR | FS_O_CREAT);
        if (res >= 0)
        {
            fs_write(&file, g_script_bytecode_buffer, sizeof(g_script_bytecode_buffer));
            fs_close(&file);
        }
    }
#endif
#if SCRIPT_RUNTIME_STRATEGY == SCRIPT_JIT
    {

        File file;
        int res = fs_open(&file, "scripts/main.js", FS_O_RDWR | FS_O_CREAT);
        if (res >= 0)
        {
            fs_write(&file, g_script_source_buffer, sizeof(g_script_source_buffer));
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

        File file;
        int res = fs_open(&file, "scripts/main.bin", FS_O_RDWR | FS_O_CREAT);
        if (res >= 0)
        {
            fs_read(&file, g_script_bytecode_buffer, sizeof(g_script_bytecode_buffer));
            fs_close(&file);
        }
    }
#endif
#if SCRIPT_RUNTIME_STRATEGY == SCRIPT_JIT
    {

        File file;
        int res = fs_open(&file, "scripts/main.js", FS_O_RDWR | FS_O_CREAT);
        if (res >= 0)
        {
            fs_read(&file, g_script_source_buffer, sizeof(g_script_source_buffer));
            fs_close(&file);
        }
    }
#endif
#endif
}
