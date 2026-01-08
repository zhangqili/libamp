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
#define EVENT_BUFFER_LENGTH 16
#endif

typedef struct __EventBuffer
{
    KeyboardEvent event;
    void* owner;
} EventBuffer;

typedef struct __EventBufferListNode
{
    EventBuffer data;
    int16_t next;
} EventBufferListNode;
typedef struct __EventBufferList
{
    EventBufferListNode *data;
    int16_t head;
    int16_t tail;
    int16_t len;
    int16_t free_node;
} EventBufferList;

extern EventBufferList g_event_buffer_list;

void event_buffer_init(void);
void event_buffer_add_buffer(void);

void event_forward_list_init(EventBufferList* list, EventBufferListNode* data, uint16_t len);
void event_forward_list_erase_after(EventBufferList* list, EventBufferListNode* data);
void event_forward_list_insert_after(EventBufferList* list, EventBufferListNode* data, EventBuffer t);
void event_forward_list_push_front(EventBufferList* list, EventBuffer t);
void event_forward_list_remove_first(EventBufferList* list, EventBuffer t);
void event_forward_list_remove_specific_owner(EventBufferList* list, void* owner);
bool event_forward_list_exists_keycode(EventBufferList* list, void* owner, Keycode keycode);
void event_forward_list_remove_first_keycode(EventBufferList* list, void* owner, Keycode keycode);

#ifdef __cplusplus
}
#endif

#endif /* EVENT_BUFFER_H_ */
