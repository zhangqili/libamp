/* Copyright 2016 Jack Humbert
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "process_midi.h"

#include "midi.h"
#include "qmk_midi.h"
#include "math.h"

static uint8_t tone_status[MIDI_TONE_COUNT];

static uint8_t  midi_modulation;
static int8_t   midi_modulation_step;
static uint16_t midi_modulation_timer;
MIDIConfig   midi_config;

inline uint8_t compute_velocity(uint8_t setting) {
    return setting * (128 / (MIDI_VELOCITY_MAX - MIDI_VELOCITY_MIN));
}

void midi_init(void) {
    midi_config.octave              = MIDI_OCTAVE_2 - MIDI_OCTAVE_MIN;
    midi_config.transpose           = 0;
    midi_config.velocity            = 127;
    midi_config.channel             = 0;
    midi_config.modulation_interval = 8;

    for (uint8_t i = 0; i < MIDI_TONE_COUNT; i++) {
        tone_status[i] = MIDI_INVALID_NOTE;
    }

    midi_modulation       = 0;
    midi_modulation_step  = 0;
    midi_modulation_timer = 0;
}

uint8_t midi_compute_note(uint16_t keycode) {
    return 12 * midi_config.octave + (keycode - MIDI_TONE_MIN) + midi_config.transpose;
}

bool midi_event_handler(KeyboardEvent event)
{
    uint8_t keycode = MODIFIER(event.keycode);
    uint8_t velocity = 0;
    if (IS_ADVANCED_KEY(event.key))
    {
        float intensity = fabs(((AdvancedKey*)event.key)->difference/(float)MIDI_REF_VELOCITY);
        if (intensity > 1.0f)
        {
            intensity = 1.0f;
        }
        velocity = intensity*127;
    }
    else
    {
        velocity = midi_config.velocity;
    }
    
    if (KEYCODE(event.keycode) == MIDI_NOTE)
    {
        uint8_t channel  = midi_config.channel;
        if ((event.event == KEYBOARD_EVENT_KEY_DOWN))
        {
            midi_send_noteon(&midi_device, channel, keycode, velocity);
        }
        else
        {
            midi_send_noteoff(&midi_device, channel, keycode, velocity);
        }
        return false;
    }
    switch (keycode) {
        case MIDI_TONE_MIN ... MIDI_TONE_MAX: {
            uint8_t channel  = midi_config.channel;
            uint8_t tone     = keycode - MIDI_TONE_MIN;
            if ((event.event == KEYBOARD_EVENT_KEY_DOWN)) {
                if (tone_status[tone] == MIDI_INVALID_NOTE) {
                    uint8_t note = midi_compute_note(keycode);
                    midi_send_noteon(&midi_device, channel, note, velocity);
                    //dprintf("midi noteon channel:%d note:%d velocity:%d\n", channel, note, velocity);
                    tone_status[tone] = note;
                }
            } else {
                uint8_t note = tone_status[tone];
                if (note != MIDI_INVALID_NOTE) {
                    midi_send_noteoff(&midi_device, channel, note, velocity);
                    //dprintf("midi noteoff channel:%d note:%d velocity:%d\n", channel, note, velocity);
                }
                tone_status[tone] = MIDI_INVALID_NOTE;
            }
            return false;
        }
        case MIDI_OCTAVE_MIN ... MIDI_OCTAVE_MAX:
            if ((event.event == KEYBOARD_EVENT_KEY_DOWN)) {
                midi_config.octave = keycode - MIDI_OCTAVE_MIN;
                //dprintf("midi octave %d\n", midi_config.octave);
            }
            return false;
        case MIDI_OCTAVE_DOWN:
            if ((event.event == KEYBOARD_EVENT_KEY_DOWN) && midi_config.octave > 0) {
                midi_config.octave--;
                //dprintf("midi octave %d\n", midi_config.octave);
            }
            return false;
        case MIDI_OCTAVE_UP:
            if ((event.event == KEYBOARD_EVENT_KEY_DOWN) && midi_config.octave < (MIDI_OCTAVE_MAX - MIDI_OCTAVE_MIN)) {
                midi_config.octave++;
                //dprintf("midi octave %d\n", midi_config.octave);
            }
            return false;
        case MIDI_TRANSPOSE_MIN ... MIDI_TRANSPOSE_MAX:
            if ((event.event == KEYBOARD_EVENT_KEY_DOWN)) {
                midi_config.transpose = keycode - MIDI_TRANSPOSE_0;
                //dprintf("midi transpose %d\n", midi_config.transpose);
            }
            return false;
        case MIDI_TRANSPOSE_DOWN:
            if ((event.event == KEYBOARD_EVENT_KEY_DOWN) && midi_config.transpose > (MIDI_TRANSPOSE_MIN - MIDI_TRANSPOSE_0)) {
                midi_config.transpose--;
                //dprintf("midi transpose %d\n", midi_config.transpose);
            }
            return false;
        case MIDI_TRANSPOSE_UP:
            if ((event.event == KEYBOARD_EVENT_KEY_DOWN) && midi_config.transpose < (MIDI_TRANSPOSE_MAX - MIDI_TRANSPOSE_0)) {
                const bool positive = midi_config.transpose > 0;
                midi_config.transpose++;
                if (positive && midi_config.transpose < 0) midi_config.transpose--;
                //dprintf("midi transpose %d\n", midi_config.transpose);
            }
            return false;
        case MIDI_VELOCITY_MIN ... MIDI_VELOCITY_MAX:
            if ((event.event == KEYBOARD_EVENT_KEY_DOWN)) {
                midi_config.velocity = (uint8_t)(keycode - MIDI_VELOCITY_MIN)* (128 / (MIDI_VELOCITY_MAX - MIDI_VELOCITY_MIN));
                //dprintf("midi velocity %d\n", midi_config.velocity);
            }
            return false;
        case MIDI_VELOCITY_DOWN:
            if ((event.event == KEYBOARD_EVENT_KEY_DOWN) && midi_config.velocity > 0) {
                if (midi_config.velocity == 127) {
                    midi_config.velocity -= 10;
                } else if (midi_config.velocity > 12) {
                    midi_config.velocity -= 13;
                } else {
                    midi_config.velocity = 0;
                }

                //dprintf("midi velocity %d\n", midi_config.velocity);
            }
            return false;
        case MIDI_VELOCITY_UP:
            if ((event.event == KEYBOARD_EVENT_KEY_DOWN) && midi_config.velocity < 127) {
                if (midi_config.velocity < 115) {
                    midi_config.velocity += 13;
                } else {
                    midi_config.velocity = 127;
                }
                //dprintf("midi velocity %d\n", midi_config.velocity);
            }
            return false;
        case MIDI_CHANNEL_MIN ... MIDI_CHANNEL_MAX:
            if ((event.event == KEYBOARD_EVENT_KEY_DOWN)) {
                midi_config.channel = keycode - MIDI_CHANNEL_MIN;
                //dprintf("midi channel %d\n", midi_config.channel);
            }
            return false;
        case MIDI_CHANNEL_DOWN:
            if ((event.event == KEYBOARD_EVENT_KEY_DOWN)) {
                midi_config.channel--;
                //dprintf("midi channel %d\n", midi_config.channel);
            }
            return false;
        case MIDI_CHANNEL_UP:
            if ((event.event == KEYBOARD_EVENT_KEY_DOWN)) {
                midi_config.channel++;
                //dprintf("midi channel %d\n", midi_config.channel);
            }
            return false;
        case MIDI_ALL_NOTES_OFF:
            if ((event.event == KEYBOARD_EVENT_KEY_DOWN)) {
                midi_send_cc(&midi_device, midi_config.channel, 0x7B, 0);
                //dprintf("midi all notes off\n");
            }
            return false;
        case MIDI_SUSTAIN:
            midi_send_cc(&midi_device, midi_config.channel, 0x40, (event.event == KEYBOARD_EVENT_KEY_DOWN) ? 127 : 0);
            //dprintf("midi sustain %d\n", (event.event == KEYBOARD_EVENT_KEY_DOWN));
            return false;
        case MIDI_PORTAMENTO:
            midi_send_cc(&midi_device, midi_config.channel, 0x41, (event.event == KEYBOARD_EVENT_KEY_DOWN) ? 127 : 0);
            //dprintf("midi portamento %d\n", (event.event == KEYBOARD_EVENT_KEY_DOWN));
            return false;
        case MIDI_SOSTENUTO:
            midi_send_cc(&midi_device, midi_config.channel, 0x42, (event.event == KEYBOARD_EVENT_KEY_DOWN) ? 127 : 0);
            //dprintf("midi sostenuto %d\n", (event.event == KEYBOARD_EVENT_KEY_DOWN));
            return false;
        case MIDI_SOFT:
            midi_send_cc(&midi_device, midi_config.channel, 0x43, (event.event == KEYBOARD_EVENT_KEY_DOWN) ? 127 : 0);
            //dprintf("midi soft %d\n", (event.event == KEYBOARD_EVENT_KEY_DOWN));
            return false;
        case MIDI_LEGATO:
            midi_send_cc(&midi_device, midi_config.channel, 0x44, (event.event == KEYBOARD_EVENT_KEY_DOWN) ? 127 : 0);
            //dprintf("midi legato %d\n", (event.event == KEYBOARD_EVENT_KEY_DOWN));
            return false;
        case MIDI_MODULATION:
            midi_modulation_step = (event.event == KEYBOARD_EVENT_KEY_DOWN) ? 1 : -1;
            return false;
        case MIDI_MODULATION_SPEED_DOWN:
            if ((event.event == KEYBOARD_EVENT_KEY_DOWN)) {
                midi_config.modulation_interval++;
                // prevent overflow
                if (midi_config.modulation_interval == 0) midi_config.modulation_interval--;
                //dprintf("midi modulation interval %d\n", midi_config.modulation_interval);
            }
            return false;
        case MIDI_MODULATION_SPEED_UP:
            if ((event.event == KEYBOARD_EVENT_KEY_DOWN) && midi_config.modulation_interval > 0) {
                midi_config.modulation_interval--;
                //dprintf("midi modulation interval %d\n", midi_config.modulation_interval);
            }
            return false;
        case MIDI_PITCH_BEND_DOWN:
            if ((event.event == KEYBOARD_EVENT_KEY_DOWN)) {
                midi_send_pitchbend(&midi_device, midi_config.channel, -0x2000);
                //dprintf("midi pitchbend channel:%d amount:%d\n", midi_config.channel, -0x2000);
            } else {
                midi_send_pitchbend(&midi_device, midi_config.channel, 0);
                //dprintf("midi pitchbend channel:%d amount:%d\n", midi_config.channel, 0);
            }
            return false;
        case MIDI_PITCH_BEND_UP:
            if ((event.event == KEYBOARD_EVENT_KEY_DOWN)) {
                midi_send_pitchbend(&midi_device, midi_config.channel, 0x1fff);
                //dprintf("midi pitchbend channel:%d amount:%d\n", midi_config.channel, 0x1fff);
            } else {
                midi_send_pitchbend(&midi_device, midi_config.channel, 0);
                //dprintf("midi pitchbend channel:%d amount:%d\n", midi_config.channel, 0);
            }
            return false;
    };

    return true;
}

void midi_task(void) {
    midi_device_process(&midi_device);
#ifdef MIDI_ADVANCED
    if (timer_elapsed(midi_modulation_timer) < midi_config.modulation_interval) return;
    midi_modulation_timer = timer_read();

    if (midi_modulation_step != 0) {
        dprintf("midi modulation %d\n", midi_modulation);
        midi_send_cc(&midi_device, midi_config.channel, 0x1, midi_modulation);

        if (midi_modulation_step < 0 && midi_modulation < -midi_modulation_step) {
            midi_modulation      = 0;
            midi_modulation_step = 0;
            return;
        }

        midi_modulation += midi_modulation_step;

        if (midi_modulation > 127) midi_modulation = 127;
    }
#endif
}
