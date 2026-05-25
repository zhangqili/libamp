/*
 * Copyright (c) 2026 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef CONSOLE_H
#define CONSOLE_H

#include "keyboard.h"
 
#ifdef __cplusplus
extern "C" {
#endif

#ifndef CONSOLE_BUFFER_LENGTH
#define CONSOLE_BUFFER_LENGTH 256
#endif

typedef char ConsoleBufferElm;

typedef struct __ConsoleBuffer
{
    ConsoleBufferElm data[CONSOLE_BUFFER_LENGTH];
    int16_t front;
    int16_t rear;
    int16_t len;
} ConsoleBuffer;

void console_buffer_init(ConsoleBuffer* q);
bool console_buffer_is_empty(ConsoleBuffer* q);
bool console_buffer_is_full(ConsoleBuffer* q);
bool console_buffer_pop(ConsoleBuffer* q, ConsoleBufferElm* out_char);
bool console_buffer_push(ConsoleBuffer* q, ConsoleBufferElm t);
void console_printf(const char *format, ...);
char console_read_char(void);
void console_send_char(char c);
void console_flush(void);

#ifdef __cplusplus
}
#endif

#endif //CONSOLE_H
 