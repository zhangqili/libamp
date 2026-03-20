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
    q->len = len;
}

EventLoopQueueElm event_loop_queue_pop(EventLoopQueue *q)
{
    EventLoopQueueElm a = {{0}, 0};
    if (q->front == q->rear)
        return a;
    q->front = (q->front + 1) % (q->len);
    return q->data[(q->front + q->len - 1) % (q->len)];
}

void event_loop_queue_push(EventLoopQueue *q, EventLoopQueueElm t)
{
    if (((q->rear + 1) % (q->len)) == q->front)
        return;
    q->data[q->rear] = t;
    q->rear = (q->rear + 1) % (q->len);
}
