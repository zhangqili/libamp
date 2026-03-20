/*
 * Copyright (c) 2026 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef EVENT_CACHE_H_
#define EVENT_CACHE_H_

#include "keyboard.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef EVENT_CACHE_LENGTH
#define EVENT_CACHE_LENGTH 16
#endif

#ifndef EVENT_CACHE_BUFFER_LENGTH
#define EVENT_CACHE_BUFFER_LENGTH 4
#endif

typedef struct __EventCache
{
    KeyboardEvent event;
    void* owner;
} EventCache;

typedef struct __EventCacheListNode
{
    EventCache data;
    int16_t next;
} EventCacheListNode;
typedef struct __EventCacheList
{
    EventCacheListNode *data;
    int16_t head;
    int16_t tail;
    int16_t len;
    int16_t free_node;
} EventCacheList;

extern EventCacheList g_event_buffer_list;

void event_cache_init(void);
void event_cache_add_buffer(void);

void event_forward_list_init(EventCacheList* list, EventCacheListNode* data, uint16_t len);
void event_forward_list_erase_after(EventCacheList* list, EventCacheListNode* data);
void event_forward_list_insert_after(EventCacheList* list, EventCacheListNode* data, EventCache t);
void event_forward_list_push_front(EventCacheList* list, EventCache t);
void event_forward_list_remove_first(EventCacheList* list, EventCache t);
void event_forward_list_remove_specific_owner(EventCacheList* list, void* owner);
bool event_forward_list_exists_keycode(EventCacheList* list, void* owner, Keycode keycode);
void event_forward_list_remove_first_keycode(EventCacheList* list, void* owner, Keycode keycode);

void event_cache_buffer_push(KeyboardEvent event, void* owner);
void event_cache_push(KeyboardEvent event, void* owner);

#ifdef __cplusplus
}
#endif

#endif /* EVENT_CACHE_H_ */
