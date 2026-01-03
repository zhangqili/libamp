/*
 * Copyright (c) 2026 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef SCRIPT_H_
#define SCRIPT_H_
#include "keyboard.h"

#ifdef __cplusplus
extern "C" {
#endif

void script_init(void);
void script_process(void);
void script_watch(uint16_t id);
void script_event_handler(KeyboardEvent event);

#ifdef __cplusplus
}
#endif

#endif /* SCRIPT_H_ */
