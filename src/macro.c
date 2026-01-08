/*
 * Copyright (c) 2025 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "macro.h"
#include "event_buffer.h"

Macro g_macros[MACRO_NUMS];
static MacroAction actions[MACRO_NUMS][MACRO_MAX_ACTIONS];

void macro_init(void)
{
    for (int i = 0; i < MACRO_NUMS; i++)
    {
        g_macros[i].actions = actions[i];
    }
}

void macro_event_handler(KeyboardEvent event)
{
    switch (event.event)
    {
    case KEYBOARD_EVENT_KEY_DOWN:
    {
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
            g_macros[index].begin_tick = g_keyboard_tick + g_macros[index].actions[0].delay;
            break;
        case MACRO_PLAYING_START_CIRCULARLY_NO_GAP:
            macro_start_play_circularly(&g_macros[index]);
            g_macros[index].begin_tick = g_keyboard_tick + g_macros[index].actions[0].delay;
            break;
        case MACRO_PLAYING_STOP:
            macro_stop_play(&g_macros[index]);
            event_forward_list_remove_specific_owner(&g_event_buffer_list, &g_macros[index]);
            break;
        case MACRO_PLAYING_PAUSE:
            break;
        default:
            break;
        }
        break;
    }
    case KEYBOARD_EVENT_KEY_TRUE:
        break;
    case KEYBOARD_EVENT_KEY_UP:
        break;
    case KEYBOARD_EVENT_KEY_FALSE:
        break;
    default:
        break;
    }
}

void macro_record_handler(KeyboardEvent event)
{
    if (event.keycode && (event.event == KEYBOARD_EVENT_KEY_DOWN || event.event == KEYBOARD_EVENT_KEY_UP))
    {
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
}

void macro_start_record(Macro*macro)
{
    macro->begin_tick = g_keyboard_tick;
    macro->state = MACRO_STATE_RECORDING;
    macro->index = 0;
}

void macro_stop_record(Macro*macro)
{
    macro->actions[macro->index].delay = g_keyboard_tick - macro->begin_tick;
    macro->actions[macro->index].event.keycode = KEY_NO_EVENT;
    macro->state = MACRO_STATE_IDLE;
    macro->index=0;
}

void macro_record(Macro*macro,KeyboardEvent event)
{
    macro->actions[macro->index].event = event;
    macro->actions[macro->index].delay = g_keyboard_tick - macro->begin_tick;
    macro->index++;
    if (macro->index >= MACRO_MAX_ACTIONS)
    {
        macro->index--;
        macro_stop_record(macro);
    }
}

void macro_start_play_once(Macro*macro)
{
    macro->begin_tick = g_keyboard_tick;
    macro->state = MACRO_STATE_PLAYING_ONCE;
    macro->index = 0;
}

void macro_start_play_circularly(Macro*macro)
{
    macro->begin_tick = g_keyboard_tick;
    macro->state = MACRO_STATE_PLAYING_CIRCULARLY;
    macro->index = 0;
}

void macro_stop_play(Macro*macro)
{
    macro->begin_tick = g_keyboard_tick;
    macro->state = MACRO_STATE_IDLE;
    macro->index = 0;
}

void macro_process(void)
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
            while (macro->actions[macro->index].delay + macro->begin_tick <= g_keyboard_tick)
            {
                KeyboardEvent event = macro->actions[macro->index].event;
                uint8_t report_state = ((Key*)event.key)->report_state;
                if (!event.keycode)
                {
                    if (macro->state == MACRO_STATE_PLAYING_ONCE)
                    {
                        macro->state = MACRO_STATE_IDLE;
                    }
                    else
                    {
                        macro_start_play_circularly(macro);
                        macro->begin_tick = g_keyboard_tick + macro->actions[0].delay;
                    }
                    event_forward_list_remove_specific_owner(&g_event_buffer_list, macro);
                    break;
                }
                keyboard_event_handler(macro->actions[macro->index].event);
                if (!event.is_virtual)
                {
                    keyboard_key_set_report_state((Key*)event.key, report_state);//protect key state
                }
                if (macro->actions[macro->index].event.event == KEYBOARD_EVENT_KEY_DOWN)
                {
                    event_forward_list_insert_after(&g_event_buffer_list, &g_event_buffer_list.data[g_event_buffer_list.head], (EventBuffer){event,macro});
                }
                else
                {
                    event_forward_list_remove_first(&g_event_buffer_list, (EventBuffer){event,macro});
                }
                macro->index++;
            }
            break;
        
        default:
            break;
        } 
    }
}
