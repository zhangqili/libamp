/*
 * Copyright (c) 2026 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "console.h"
#include "packet.h"
#include "driver.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

static ConsoleBuffer console_tx_buffer;
//static ConsoleBuffer console_rx_buffer;

void console_buffer_init(ConsoleBuffer* q)
{
    q->front = 0;
    q->rear = 0;
    q->len = 0;
    memset(q->data, 0, CONSOLE_BUFFER_LENGTH);
}

bool console_buffer_is_empty(ConsoleBuffer* q)
{
    return q->len == 0;
}

bool console_buffer_is_full(ConsoleBuffer* q)
{
    return q->len >= CONSOLE_BUFFER_LENGTH;
}

bool console_buffer_push(ConsoleBuffer* q, ConsoleBufferElm t)
{
    if (console_buffer_is_full(q))
    {
        return false;
    }
    q->data[q->rear] = t;
    q->rear = (q->rear + 1) % CONSOLE_BUFFER_LENGTH; 
    q->len++;
    return true;
}

bool console_buffer_pop(ConsoleBuffer* q, ConsoleBufferElm* out_char)
{
    if (console_buffer_is_empty(q))
    {
        return false;
    }
    if (out_char != NULL)
    {
        *out_char = q->data[q->front];
    }
    q->front = (q->front + 1) % CONSOLE_BUFFER_LENGTH;
    q->len--;
    
    return true;
}

char console_read_char(void)
{
    /*
    ConsoleBufferElm c = '\0';
    if (console_buffer_pop(&console_rx_buffer, &c))
    {
        return c;
    }
    return '\0';
    */
}

void console_send_char(char c)
{
    if (console_buffer_is_full(&console_tx_buffer))
    {
        console_flush();
    }

    console_buffer_push(&console_tx_buffer, c);

    //if (c == '\n')
    //{
    //    console_flush();
    //}
}

void console_printf(const char *format, ...)
{
    if (console_buffer_is_full(&console_tx_buffer))
    {
        return;
    }

    int contiguous_free = CONSOLE_BUFFER_LENGTH - console_tx_buffer.rear;
    int total_free = CONSOLE_BUFFER_LENGTH - console_tx_buffer.len;
    unsigned int max_write_len = (contiguous_free < total_free) ? contiguous_free : total_free;

    if (max_write_len <= 1) return;

    va_list args;
    va_start(args, format);
    int written = vsnprintf(&console_tx_buffer.data[console_tx_buffer.rear], max_write_len, format, args);
    va_end(args);

    if (written > 0)
    {
        int actual_len = (written < max_write_len) ? written : (max_write_len - 1);
        console_tx_buffer.rear = (console_tx_buffer.rear + actual_len) % CONSOLE_BUFFER_LENGTH;
        console_tx_buffer.len += actual_len;
    }
}

void console_flush(void)
{
    if (console_buffer_is_empty(&console_tx_buffer))
    {
        return;
    }

    while (!console_buffer_is_empty(&console_tx_buffer))
    {
        char temp_buf[64] = {0};
        PacketLog *packet_log = (PacketLog *)temp_buf;
        uint8_t idx = 0;

        int16_t peek_front = console_tx_buffer.front;
        int16_t peek_len = console_tx_buffer.len;

        while (idx < 59 && peek_len > 0)
        {
            packet_log->data[idx++] = console_tx_buffer.data[peek_front];
            peek_front = (peek_front + 1) % CONSOLE_BUFFER_LENGTH;
            peek_len--;
        }

        packet_log->data[idx] = '\0';
        packet_log->code = PACKET_CODE_LOG;
        packet_log->length = idx;

        uint8_t res = hid_send_raw((uint8_t*)temp_buf, 64);

        if (res == 0)
        {
            console_tx_buffer.front = peek_front;
            console_tx_buffer.len = peek_len;

            if (console_tx_buffer.len == 0)
            {
                console_tx_buffer.front = 0;
                console_tx_buffer.rear = 0;
            }
        }
        else
        {
            break; 
        }
    }
}
