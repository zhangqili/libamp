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

#define SCRIPT_AOT 0
#define SCRIPT_JIT 1

#ifndef SCRIPT_RUNTIME_STRATEGY
#define SCRIPT_RUNTIME_STRATEGY SCRIPT_AOT
#endif

#ifndef SCRIPT_SOURCE_BUFFER_SIZE
#define SCRIPT_SOURCE_BUFFER_SIZE  (1 * 1024)
#endif

#ifndef SCRIPT_BYTECODE_BUFFER_SIZE
#define SCRIPT_BYTECODE_BUFFER_SIZE  (1 * 1024)
#endif

#ifndef SCRIPT_MEMORY_SIZE
#define SCRIPT_MEMORY_SIZE  (4 * 1024)
#endif

#ifndef SCRIPT_MAX_TIMERS
#define SCRIPT_MAX_TIMERS 16
#endif

void script_init(void);
void script_reset(void);
void script_process(void);
void script_eval(const char *code_buf, size_t len, const char *filename);
void script_update_source(const char *code, size_t len);
void script_load_bytecode(uint8_t *bytecode_buf, size_t len);
void script_update_bytecode(uint8_t *bytecode_buf, size_t len);
void script_watch(uint16_t id);
void script_event_handler(KeyboardEvent event);

#if SCRIPT_RUNTIME_STRATEGY == SCRIPT_AOT
extern uint8_t g_script_bytecode_buffer[SCRIPT_BYTECODE_BUFFER_SIZE];
#endif
#if SCRIPT_RUNTIME_STRATEGY == SCRIPT_JIT
extern uint8_t g_script_source_buffer[SCRIPT_SOURCE_BUFFER_SIZE];
#endif

#ifdef __cplusplus
}
#endif

#endif /* SCRIPT_H_ */
