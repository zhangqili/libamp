/*
 * Copyright (c) 2026 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef EVENT_BUFFER_H_
#define EVENT_BUFFER_H_

#include "keyboard.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef EVENT_BUFFER_LENGTH
#define EVENT_BUFFER_LENGTH 32
#endif

#define event_loop_queue_foreach(q, type, item) for (uint16_t __index = (q)->front; __index != (q)->rear; __index = (__index + 1) % (q)->len)\
                                              for (type *item = &((q)->data[__index]); item; item = NULL)

typedef struct __EventArgument
{
    KeyboardEvent event;
    uint32_t tick;
}EventArgument;

typedef EventArgument EventLoopQueueElm;

typedef struct __EventLoopQueue
{
    EventLoopQueueElm *data;
    int16_t front;
    int16_t rear;
    int16_t len;
} EventLoopQueue;

typedef EventLoopQueue EventBuffer;

void event_loop_queue_init(EventLoopQueue* q, EventLoopQueueElm*data, uint16_t len);
EventLoopQueueElm event_loop_queue_pop(EventLoopQueue* q);
void event_loop_queue_push(EventLoopQueue* q, EventLoopQueueElm t);

#ifdef __cplusplus
}
#endif

#endif /* EVENT_BUFFER_H_ */
