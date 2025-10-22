/*
 * Copyright (c) 2025 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "macro.h"
#include "keyboard.h"

Macro g_macros[MACRO_NUMS];
static MacroAction actions[MACRO_NUMS][MACRO_MAX_ACTIONS];
MacroArgumentList macro_argument_list;
static MacroArgumentListNode macro_argument_list_buffer[MACRO_BUFFER_LENGTH];

void macro_init(void)
{
    for (int i = 0; i < MACRO_NUMS; i++)
    {
        g_macros[i].actions = actions[i];
    }
    macro_forward_list_init(&macro_argument_list, macro_argument_list_buffer, MACRO_BUFFER_LENGTH);
}

void macro_event_handler(KeyboardEvent event)
{
    switch (event.event)
    {
    case KEYBOARD_EVENT_KEY_DOWN:
        uint8_t index = MACRO_KEYCODE_GET_INDEX(event.keycode);
        switch (MACRO_KEYCODE_GET_KEYCODE(event.keycode))
        {
        case MACRO_RECORDING_START:
            macro_start_record(&g_macros[index]);
            break;
        case MACRO_RECORDING_STOP:
            macro_stop_record(&g_macros[index]);
            break;
        case MACRO_RECORDING_TOGGLE:
            if(g_macros[index].state == MACRO_STATE_IDLE)
                macro_start_record(&g_macros[index]);
            else if(g_macros[index].state == MACRO_STATE_RECORDING)
                macro_stop_record(&g_macros[index]);
            break;
        case MACRO_PLAYING_START_ONCE:
            macro_start_play_once(&g_macros[index]);
            break;
        case MACRO_PLAYING_START_CIRCULARLY:
            macro_start_play_circularly(&g_macros[index]);
            break;
        case MACRO_PLAYING_START_ONCE_NO_GAP:
            macro_start_play_once(&g_macros[index]);
            g_macros[index].begin_time = g_keyboard_tick + g_macros[index].actions[0].delay;
            break;
        case MACRO_PLAYING_START_CIRCULARLY_NO_GAP:
            macro_start_play_circularly(&g_macros[index]);
            g_macros[index].begin_time = g_keyboard_tick + g_macros[index].actions[0].delay;
            break;
        case MACRO_PLAYING_STOP:
            macro_stop_play(&g_macros[index]);
            macro_forward_list_remove_specific_owner(&macro_argument_list, index);
            break;
        case MACRO_PLAYING_PAUSE:
            break;
        default:
            break;
        }
        break;
    case KEYBOARD_EVENT_KEY_TRUE:
        break;
    case KEYBOARD_EVENT_KEY_UP:
        break;
    case KEYBOARD_EVENT_KEY_FALSE:
        break;
    default:
        break;
    }
    if (KEYCODE_GET_MAIN(event.keycode) == MACRO_COLLECTION)
    {
        return;
    }
    for (int i = 0; i < MACRO_NUMS; i++)
    {
        Macro *macro = &g_macros[i];
        switch (macro->state)
        {
        case MACRO_STATE_RECORDING:
            macro_record(macro,event);
            break;
        case MACRO_STATE_PLAYING_ONCE:
            break;
        default:
            break;
        }
    }
}

void macro_start_record(Macro*macro)
{
    macro->begin_time = g_keyboard_tick;
    macro->state = MACRO_STATE_RECORDING;
    macro->index = 0;
}

void macro_stop_record(Macro*macro)
{
    macro->actions[macro->index].delay = g_keyboard_tick - macro->begin_time;
    macro->actions[macro->index].event.keycode = MACRO_COLLECTION|(MACRO_END<<12);
    macro->state = MACRO_STATE_IDLE;
    macro->index=0;
}

void macro_record(Macro*macro,KeyboardEvent event)
{
    if (event.event == KEYBOARD_EVENT_KEY_DOWN || event.event == KEYBOARD_EVENT_KEY_UP)
    {
        macro->actions[macro->index].event = event;
        macro->actions[macro->index].delay = g_keyboard_tick - macro->begin_time;
        macro->index++;
        if (macro->index >= MACRO_MAX_ACTIONS)
        {
            macro->index--;
            macro_stop_record(macro);
        }
    }
}

void macro_start_play_once(Macro*macro)
{
    macro->begin_time = g_keyboard_tick;
    macro->state = MACRO_STATE_PLAYING_ONCE;
    macro->index = 0;
}

void macro_start_play_circularly(Macro*macro)
{
    macro->begin_time = g_keyboard_tick;
    macro->state = MACRO_STATE_PLAYING_CIRCULARLY;
    macro->index = 0;
}

void macro_stop_play(Macro*macro)
{
    macro->begin_time = g_keyboard_tick;
    macro->state = MACRO_STATE_IDLE;
    macro->index = 0;
}

void macro_tick(void)
{
    for (int i = 0; i < MACRO_NUMS; i++)
    {
        Macro *macro = &g_macros[i];
        switch (macro->state)
        {
        case MACRO_STATE_RECORDING:
            break;
        case MACRO_STATE_PLAYING_ONCE:
        case MACRO_STATE_PLAYING_CIRCULARLY:
            while (macro->actions[macro->index].delay + macro->begin_time <= g_keyboard_tick)
            {
                KeyboardEvent*event = &(macro->actions[macro->index].event);
                uint8_t report_state = ((Key*)event->key)->report_state;
                if (event->keycode == (MACRO_COLLECTION|(MACRO_END<<12)))
                {
                    if (macro->state == MACRO_STATE_PLAYING_ONCE)
                    {
                        macro->state = MACRO_STATE_IDLE;
                    }
                    else
                    {
                        macro_start_play_circularly(macro);
                        macro->begin_time = g_keyboard_tick + macro->actions[0].delay;
                    }
                    macro_forward_list_remove_specific_owner(&macro_argument_list, i);
                    break;
                }
                keyboard_event_handler(macro->actions[macro->index].event);
                ((Key*)event->key)->report_state = report_state;//protect key state
                if (macro->actions[macro->index].event.event == KEYBOARD_EVENT_KEY_DOWN)
                {
                    macro_forward_list_insert_after(&macro_argument_list, &macro_argument_list.data[macro_argument_list.head], (MacroArgument){event,i});
                }
                else
                {
                    macro_forward_list_remove_first(&macro_argument_list, (MacroArgument){event,i});
                }
                macro->index++;
            }
            break;
        
        default:
            break;
        } 
    }
}

void macro_add_buffer(void)
{
    MacroArgumentList * list = &macro_argument_list;
    MacroArgumentListNode * last_node = &list->data[list->head];
    UNUSED(last_node);
    for (int16_t iterator = list->data[list->head].next; iterator >= 0;)
    {
        MacroArgumentListNode* node = &(list->data[iterator]);
        MacroArgument *item = &(node->data);
        keyboard_add_buffer(*item->event);
        last_node = node;
        iterator = list->data[iterator].next;
    }
}

void macro_forward_list_init(MacroArgumentList* list, MacroArgumentListNode* data, uint16_t len)
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
    macro_forward_list_push_front(list, (MacroArgument){NULL,MACRO_NUMS});
}

void macro_forward_list_erase_after(MacroArgumentList* list, MacroArgumentListNode* data)
{
    int16_t target = 0;
    target = data->next;
    data->next = list->data[target].next;
    list->data[target].next = list->free_node;
    list->free_node = target;
}

void macro_forward_list_insert_after(MacroArgumentList* list, MacroArgumentListNode* data, MacroArgument t)
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

void macro_forward_list_push_front(MacroArgumentList* list, MacroArgument t)
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

void macro_forward_list_remove_first(MacroArgumentList* list, MacroArgument t)
{
    MacroArgumentListNode * last_node = &list->data[list->head];
    for (int16_t iterator = list->data[list->head].next; iterator >= 0;)
    {
        MacroArgumentListNode* node = &(list->data[iterator]);
        MacroArgument *item = &(node->data);
        if ((*item).event->key == t.event->key && (*item).event->keycode == t.event->keycode)
        {
            macro_forward_list_erase_after(list, last_node);
            return;
        }
        last_node = node;
        iterator = list->data[iterator].next;
    }
}

void macro_forward_list_remove_specific_owner(MacroArgumentList* list, uint8_t owner)
{
    MacroArgumentListNode * last_node = &list->data[list->head];
    for (int16_t iterator = list->data[list->head].next; iterator >= 0;)
    {
        MacroArgumentListNode* node = &(list->data[iterator]);
        MacroArgument *item = &(node->data);
        if ((*item).owner == owner)
        {
            keyboard_event_handler(MK_EVENT((*item).event->keycode,KEYBOARD_EVENT_KEY_UP,(*item).event->key));
            macro_forward_list_erase_after(list, last_node);
            iterator = last_node->next;
            continue;
        }
        last_node = node;
        iterator = list->data[iterator].next;
    }
}
