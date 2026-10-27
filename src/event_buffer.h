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

/* Visit the queued elements in order, oldest first. The body may pop the
 * element it is visiting (keyboard_process() does), nothing else. */
#define event_loop_queue_foreach(q, type, item) for (int16_t __index = (q)->front; __index != (q)->rear; __index = event_loop_queue_next_index((q), __index))\
                                              for (type *item = &((q)->data[__index]); item; item = NULL)

typedef struct __EventArgument
{
    KeyboardEvent event;
    uintptr_t tick;
}EventArgument;

typedef EventArgument EventLoopQueueElm;

/* Single-producer, single-consumer ring: pushed from the tick, popped from the
 * main loop. front and rear are each written by one side only. */
typedef struct __EventLoopQueue
{
    EventLoopQueueElm *data;
    volatile int16_t front;
    volatile int16_t rear;
    int16_t len;
} EventLoopQueue;

typedef EventLoopQueue EventBuffer;

/* Index after index, wrapping at the capacity; a compare instead of a modulo. */
static inline int16_t event_loop_queue_next_index(const EventLoopQueue *q, int16_t index)
{
    return (int16_t)(index + 1 == q->len ? 0 : index + 1);
}

void event_loop_queue_init(EventLoopQueue* q, EventLoopQueueElm*data, uint16_t len);
EventLoopQueueElm event_loop_queue_pop(EventLoopQueue* q);
void event_loop_queue_push(EventLoopQueue* q, EventLoopQueueElm t);

#ifdef __cplusplus
}
#endif

#endif /* EVENT_BUFFER_H_ */
