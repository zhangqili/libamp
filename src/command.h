/*
 * Copyright (c) 2024 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef COMMAND_H_
#define COMMAND_H_

#include"stdint.h"
#include"keyboard.h"

#ifdef __cplusplus
extern "C" {
#endif

void command_parse(uint8_t *buf,uint8_t len);
void start_load_cargo(void);
int load_cargo(void);
void send_debug_info(void);

#ifdef __cplusplus
}
#endif

#endif