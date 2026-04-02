/*
 * Copyright (c) 2026 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "event_cache.h"
#include "event_buffer.h"

EventCacheList g_event_buffer_list;
static EventCacheListNode event_buffer_list_buffer[EVENT_CACHE_LENGTH];

static EventLoopQueueElm event_cache_buffers[EVENT_CACHE_BUFFER_LENGTH];
static EventLoopQueue event_cache_buffer;

void event_cache_init(void)
{
    event_forward_list_init(&g_event_buffer_list, event_buffer_list_buffer, EVENT_CACHE_LENGTH);
    event_loop_queue_init(&event_cache_buffer, event_cache_buffers, EVENT_CACHE_BUFFER_LENGTH);
}

void event_cache_add_buffer(void)
{
    EventCacheList * list = &g_event_buffer_list;
    EventCacheListNode * last_node = &list->data[list->head];
    UNUSED(last_node);
    event_loop_queue_foreach(&event_cache_buffer, EventLoopQueueElm, event)
    {
        event_cache_push(event->event, (void*)event->tick);
        event_loop_queue_pop(&event_cache_buffer);
    }
    for (int16_t iterator = list->data[list->head].next; iterator >= 0;)
    {
        EventCacheListNode* node = &(list->data[iterator]);
        EventCache *item = &(node->data);
        keyboard_add_buffer(item->event);
        last_node = node;
        iterator = list->data[iterator].next;
    }
}

void event_forward_list_init(EventCacheList* list, EventCacheListNode* data, uint16_t len)
{
    list->data = data;
    list->head = -1;
    list->tail = 0;
    list->len = len;
    for (int i = 0; i < len; i++)
    {
        list->data[i].next = i + 1;
    }
    list->data[len - 1].next = -1;
    list->free_node = 0;
    event_forward_list_push_front(list, (EventCache){MK_EVENT(0,0,NULL),NULL});
}

void event_forward_list_erase_after(EventCacheList* list, EventCacheListNode* data)
{
    int16_t target = 0;
    target = data->next;
    data->next = list->data[target].next;
    list->data[target].next = list->free_node;
    list->free_node = target;
}

void event_forward_list_insert_after(EventCacheList* list, EventCacheListNode* data, EventCache t)
{
    if (list->free_node == -1)
    {
        return;
    }
    int16_t new_node = list->free_node;
    list->free_node = list->data[list->free_node].next;

    list->data[new_node].data = t;
    list->data[new_node].next = data->next;

    data->next = new_node;
}

void event_forward_list_push_front(EventCacheList* list, EventCache t)
{
    if (list->free_node == -1)
    {
        return;
    }
    int16_t new_node = list->free_node;
    list->free_node = list->data[list->free_node].next;

    list->data[new_node].data = t;
    list->data[new_node].next = list->head;

    list->head = new_node;
}

void event_forward_list_remove_first(EventCacheList* list, EventCache t)
{
    EventCacheListNode * last_node = &list->data[list->head];
    for (int16_t iterator = list->data[list->head].next; iterator >= 0;)
    {
        EventCacheListNode* node = &(list->data[iterator]);
        EventCache *item = &(node->data);
        if (item->event.key == t.event.key && item->event.keycode == t.event.keycode)
        {
            event_forward_list_erase_after(list, last_node);
            return;
        }
        last_node = node;
        iterator = list->data[iterator].next;
    }
}

void event_forward_list_remove_specific_owner(EventCacheList* list, void* owner)
{
    for (int16_t *iterator_ptr = &list->data[list->head].next; *iterator_ptr >= 0;)
    {
        EventCacheListNode* node = &(list->data[*iterator_ptr]);
        EventCache *item = &(node->data);
        if ((*item).owner == owner)
        {
            int16_t free_node = *iterator_ptr;
            keyboard_event_handler(MK_EVENT(item->event.keycode,KEYBOARD_EVENT_KEY_UP,item->event.key));
            *iterator_ptr = node->next;
            node->next = list->free_node;
            list->free_node = free_node;
            continue;
        }
        iterator_ptr = &list->data[*iterator_ptr].next;
    }
}

bool event_forward_list_exists_keycode(EventCacheList* list, void* owner, Keycode keycode)
{
    int16_t current_idx = list->data[list->head].next;
    while (current_idx >= 0)
    {
        EventCacheListNode* node = &list->data[current_idx];
        if (node->data.owner == owner && node->data.event.keycode == keycode)
        {
            return true;
        }
        current_idx = node->next;
    }
    return false;
}

void event_forward_list_remove_first_keycode(EventCacheList* list, void* owner, Keycode keycode)
{
    for (int16_t *iterator_ptr = &list->data[list->head].next; *iterator_ptr >= 0;)
    {
        EventCacheListNode* node = &(list->data[*iterator_ptr]);
        EventCache *item = &(node->data);
        if ((*item).owner == owner && item->event.keycode == keycode)
        {
            int16_t free_node = *iterator_ptr;
            //keyboard_event_handler(MK_EVENT(item->event.keycode,KEYBOARD_EVENT_KEY_UP,item->event.key));
            *iterator_ptr = node->next;
            node->next = list->free_node;
            list->free_node = free_node;
            return;
        }
        iterator_ptr = &list->data[*iterator_ptr].next;
    }
}

void event_cache_buffer_push(KeyboardEvent event, void* owner)
{
    event_loop_queue_push(&event_cache_buffer, (EventLoopQueueElm){event, (uint32_t)owner});
}

void event_cache_push(KeyboardEvent event, void* owner)
{
    event_forward_list_insert_after(&g_event_buffer_list, &g_event_buffer_list.data[g_event_buffer_list.head], (EventCache){event, owner});
}
