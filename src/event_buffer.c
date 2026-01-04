/*
 * Copyright (c) 2026 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "event_buffer.h"

EventBufferList g_event_buffer_list;
static EventBufferListNode event_buffer_list_buffer[EVENT_BUFFER_LENGTH];

void event_buffer_init(void)
{
    event_forward_list_init(&g_event_buffer_list, event_buffer_list_buffer, EVENT_BUFFER_LENGTH);
}

void event_buffer_add_buffer(void)
{
    EventBufferList * list = &g_event_buffer_list;
    EventBufferListNode * last_node = &list->data[list->head];
    UNUSED(last_node);
    for (int16_t iterator = list->data[list->head].next; iterator >= 0;)
    {
        EventBufferListNode* node = &(list->data[iterator]);
        EventBuffer *item = &(node->data);
        keyboard_add_buffer(item->event);
        last_node = node;
        iterator = list->data[iterator].next;
    }
}

void event_forward_list_init(EventBufferList* list, EventBufferListNode* data, uint16_t len)
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
    event_forward_list_push_front(list, (EventBuffer){MK_EVENT(0,0,NULL),NULL});
}

void event_forward_list_erase_after(EventBufferList* list, EventBufferListNode* data)
{
    int16_t target = 0;
    target = data->next;
    data->next = list->data[target].next;
    list->data[target].next = list->free_node;
    list->free_node = target;
}

void event_forward_list_insert_after(EventBufferList* list, EventBufferListNode* data, EventBuffer t)
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

void event_forward_list_push_front(EventBufferList* list, EventBuffer t)
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

void event_forward_list_remove_first(EventBufferList* list, EventBuffer t)
{
    EventBufferListNode * last_node = &list->data[list->head];
    for (int16_t iterator = list->data[list->head].next; iterator >= 0;)
    {
        EventBufferListNode* node = &(list->data[iterator]);
        EventBuffer *item = &(node->data);
        if (item->event.key == t.event.key && item->event.keycode == t.event.keycode)
        {
            event_forward_list_erase_after(list, last_node);
            return;
        }
        last_node = node;
        iterator = list->data[iterator].next;
    }
}

void event_forward_list_remove_specific_owner(EventBufferList* list, void* owner)
{
    for (int16_t *iterator_ptr = &list->data[list->head].next; *iterator_ptr >= 0;)
    {
        EventBufferListNode* node = &(list->data[*iterator_ptr]);
        EventBuffer *item = &(node->data);
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

bool event_forward_list_exists_keycode(EventBufferList* list, void* owner, Keycode keycode)
{
    int16_t current_idx = list->data[list->head].next;
    while (current_idx >= 0)
    {
        EventBufferListNode* node = &list->data[current_idx];
        if (node->data.owner == owner && node->data.event.keycode == keycode)
        {
            return true;
        }
        current_idx = node->next;
    }
    return false;
}

void event_forward_list_remove_first_keycode(EventBufferList* list, void* owner, Keycode keycode)
{
    for (int16_t *iterator_ptr = &list->data[list->head].next; *iterator_ptr >= 0;)
    {
        EventBufferListNode* node = &(list->data[*iterator_ptr]);
        EventBuffer *item = &(node->data);
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
