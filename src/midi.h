/*
 * Copyright (c) 2026 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LIBAMP_MIDI_H_
#define LIBAMP_MIDI_H_

#include "keyboard.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MIDI_TONE_MIN       MIDI_NOTE_C_0
#define MIDI_TONE_MAX       MIDI_NOTE_B_5
#define MIDI_OCTAVE_MIN     MIDI_OCTAVE_N2
#define MIDI_OCTAVE_MAX     MIDI_OCTAVE_7
#define MIDI_TRANSPOSE_MIN  MIDI_TRANSPOSE_N6
#define MIDI_TRANSPOSE_MAX  MIDI_TRANSPOSE_6
#define MIDI_VELOCITY_MIN   MIDI_VELOCITY_0
#define MIDI_VELOCITY_MAX   MIDI_VELOCITY_10
#define MIDI_CHANNEL_MIN    MIDI_CHANNEL_1
#define MIDI_CHANNEL_MAX    MIDI_CHANNEL_16

#define MIDI_INVALID_NOTE                   0xFF
#define MIDI_TONE_COUNT                     (MIDI_TONE_MAX - MIDI_TONE_MIN + 1)
#define MIDI_MESSAGE_NO_CHANNEL             0xFF

#ifndef MIDI_DEFAULT_AUDIO_HANDLER_ENABLE
#    define MIDI_DEFAULT_AUDIO_HANDLER_ENABLE 1
#endif

typedef struct __MIDIEventPacket {
    uint8_t Event;
    uint8_t Data1;
    uint8_t Data2;
    uint8_t Data3;
} __PACKED MIDIEventPacket;

typedef enum {
    MIDI_MESSAGE_NOTE_OFF,
    MIDI_MESSAGE_NOTE_ON,
    MIDI_MESSAGE_AFTERTOUCH,
    MIDI_MESSAGE_CONTROL_CHANGE,
    MIDI_MESSAGE_PROGRAM_CHANGE,
    MIDI_MESSAGE_CHANNEL_PRESSURE,
    MIDI_MESSAGE_PITCH_BEND,
    MIDI_MESSAGE_TIME_CODE,
    MIDI_MESSAGE_SONG_POSITION,
    MIDI_MESSAGE_SONG_SELECT,
    MIDI_MESSAGE_TUNE_REQUEST,
    MIDI_MESSAGE_REALTIME,
    MIDI_MESSAGE_SYSEX,
} MIDIMessageType;

typedef struct {
    MIDIMessageType type;
    uint8_t status;
    uint8_t channel;
    uint8_t bytes[3];
    uint8_t length;
    bool sysex_start;
    bool sysex_end;
} MIDIMessage;

typedef union {
    uint32_t raw;
    struct {
        uint8_t octave : 4;
        int8_t transpose : 4;
        uint8_t velocity : 7;
        uint8_t channel : 4;
        uint8_t modulation_interval : 4;
    };
} MIDIConfig;

extern MIDIConfig midi_config;

void midi_init(void);
void midi_task(void);
void midi_event_handler(KeyboardEvent event);
int midi_send(MIDIEventPacket *event);
void midi_input_callback(MIDIEventPacket *event);
uint8_t midi_compute_note(uint16_t keycode);
void midi_message_callback(const MIDIMessage *message);
void midi_play_note(float frequency, uint8_t velocity);
void midi_stop_note(float frequency);
void midi_stop_all_notes(void);

#ifdef __cplusplus
}
#endif

#endif /* LIBAMP_MIDI_H_ */
