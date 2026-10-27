/*
 * Copyright (c) 2026 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "event_buffer.h"

void event_loop_queue_init(EventLoopQueue *q, EventLoopQueueElm *data, uint16_t len)
{
    q->data = data;
    q->front = 0;
    q->rear = 0;
    q->len = (int16_t)len;
}

EventLoopQueueElm event_loop_queue_pop(EventLoopQueue *q)
{
    if (q->front == q->rear)
    {
        const EventLoopQueueElm empty = {{0, 0, 0, NULL}, 0};
        return empty;
    }
    const EventLoopQueueElm element = q->data[q->front];
    COMPILER_FENCE(); /* copy out before releasing the slot to the producer */
    q->front = event_loop_queue_next_index(q, q->front);
    return element;
}

void event_loop_queue_push(EventLoopQueue *q, EventLoopQueueElm t)
{
    const int16_t next_rear = event_loop_queue_next_index(q, q->rear);
    if (next_rear == q->front)
    {
        return; /* full: the newest event is dropped */
    }
    q->data[q->rear] = t;
    COMPILER_FENCE(); /* the element is complete before it is published */
    q->rear = next_rear;
}
