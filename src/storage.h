/*
 * Copyright (c) 2024 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef STORAGE_H_
#define STORAGE_H_

#include "keyboard.h"
#include "dynamic_key.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef STORAGE_PROFILE_FILE_NUM
#define STORAGE_PROFILE_FILE_NUM 4
#endif

extern uint8_t g_current_profile_index;

int storage_mount(void);
void storage_unmount(void);
int storage_check_version(void);
uint8_t storage_read_profile_index(void);
void storage_save_profile_index(void);
void storage_read_profile(void);
void storage_save_profile(void);
void storage_save_script(void);
void storage_read_script(void);

#ifdef __cplusplus
}
#endif

#endif