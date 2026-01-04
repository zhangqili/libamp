/*
 * Copyright (c) 2025 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef MACRO_H_
#define MACRO_H_

#include "keyboard.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef MACRO_NUMS
#define MACRO_NUMS 4
#endif

#ifndef MACRO_MAX_ACTIONS
#define MACRO_MAX_ACTIONS 128
#endif

#ifndef MACRO_BUFFER_LENGTH
#define MACRO_BUFFER_LENGTH 8
#endif

#define MACRO_KEYCODE_GET_INDEX(keycode) (KEYCODE_GET_SUB((keycode)) & 0x0F)
#define MACRO_KEYCODE_GET_KEYCODE(keycode) ((KEYCODE_GET_SUB((keycode)) & 0xF0) >>4)

typedef enum __MacroKeycode
{
    MACRO_END                             = 0x0,
    MACRO_RECORDING_START                 = 0x1,
    MACRO_RECORDING_STOP                  = 0x2,
    MACRO_RECORDING_TOGGLE                = 0x3,
    MACRO_PLAYING_START_ONCE              = 0x4,
    MACRO_PLAYING_START_CIRCULARLY        = 0x5,
    MACRO_PLAYING_START_ONCE_NO_GAP       = 0x6,
    MACRO_PLAYING_START_CIRCULARLY_NO_GAP = 0x7,
    MACRO_PLAYING_STOP                    = 0x8,
    MACRO_PLAYING_PAUSE                   = 0x9,
    MACRO_BEGIN                           = 0xf,
} MacroKeycode;

typedef enum __MacroState
{
    MACRO_STATE_IDLE               = 0x00,
    MACRO_STATE_RECORDING          = 0x01,
    MACRO_STATE_PLAYING_ONCE       = 0x02,
    MACRO_STATE_PLAYING_CIRCULARLY = 0x03
} MacroState;

typedef struct __MacroAction
{
    uint32_t      delay;
    KeyboardEvent event;
} MacroAction;

typedef struct __Macro
{
    uint8_t state;
    uint32_t begin_tick;
    uint16_t length;
    uint16_t index;
    MacroAction* actions;
} Macro;

extern Macro g_macros[MACRO_NUMS];

void macro_init(void);

void macro_event_handler(KeyboardEvent event);
void macro_record_handler(KeyboardEvent event);
void macro_start_record(Macro*macro);
void macro_stop_record(Macro*macro);
void macro_record(Macro*macro,KeyboardEvent event);
void macro_start_play_once(Macro*macro);
void macro_start_play_circularly(Macro*macro);
void macro_stop_play(Macro*macro);
void macro_process(void);

#ifdef __cplusplus
}
#endif

#endif /* MACRO_H_ */
