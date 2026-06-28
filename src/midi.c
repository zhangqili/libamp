/*
 * Copyright (c) 2026 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "midi.h"

#include "driver.h"

#include <math.h>
#include <stddef.h>

#define USB_MIDI_CABLE                 0x00
#define USB_MIDI_CIN_2BYTE_SYSTEM      0x02
#define USB_MIDI_CIN_3BYTE_SYSTEM      0x03
#define USB_MIDI_CIN_SYSEX_CONTINUE    0x04
#define USB_MIDI_CIN_SYSEX_END_1       0x05
#define USB_MIDI_CIN_SYSEX_END_2       0x06
#define USB_MIDI_CIN_SYSEX_END_3       0x07
#define USB_MIDI_CIN_NOTE_OFF          0x08
#define USB_MIDI_CIN_NOTE_ON           0x09
#define USB_MIDI_CIN_CONTROL_CHANGE    0x0B
#define USB_MIDI_CIN_PITCH_BEND        0x0E
#define USB_MIDI_CIN_SINGLE_BYTE       0x0F

#define MIDI_RX_CAPACITY               192

#define MIDI_DATA_LIMIT                0x7F
#define MIDI_STATUS_BIT                0x80
#define MIDI_STATUS_GROUP_MASK         0xF0
#define MIDI_CHANNEL_MASK              0x0F

#define MIDI_STATUS_NOTE_OFF           0x80
#define MIDI_STATUS_NOTE_ON            0x90
#define MIDI_STATUS_AFTERTOUCH         0xA0
#define MIDI_STATUS_CONTROL_CHANGE     0xB0
#define MIDI_STATUS_PROGRAM_CHANGE     0xC0
#define MIDI_STATUS_CHANNEL_PRESSURE   0xD0
#define MIDI_STATUS_PITCH_BEND         0xE0

#define MIDI_STATUS_SYSEX_START        0xF0
#define MIDI_STATUS_TIME_CODE          0xF1
#define MIDI_STATUS_SONG_POSITION      0xF2
#define MIDI_STATUS_SONG_SELECT        0xF3
#define MIDI_STATUS_TUNE_REQUEST       0xF6
#define MIDI_STATUS_SYSEX_END          0xF7
#define MIDI_STATUS_REALTIME_MIN       0xF8
#define MIDI_STATUS_STOP               0xFC

typedef struct {
    uint8_t data[MIDI_RX_CAPACITY];
    uint8_t head;
    uint8_t tail;
    uint8_t count;
} MidiInputQueue;

typedef struct {
    MidiInputQueue input;
    uint8_t active_status;
    uint8_t running_status;
    uint8_t payload[2];
    uint8_t payload_count;
    uint8_t payload_needed;
    bool sysex_open;
} MidiRuntime;

static MidiRuntime midi_runtime;
static uint8_t tone_status[MIDI_TONE_COUNT];

static uint8_t midi_modulation;
static int8_t midi_modulation_step;
static uint16_t midi_modulation_timer;
MIDIConfig midi_config;

static uint8_t usb_midi_event(uint8_t cable, uint8_t cin)
{
    return (uint8_t)(((cable & MIDI_CHANNEL_MASK) << 4) | (cin & MIDI_CHANNEL_MASK));
}

static bool midi_is_status(uint8_t byte)
{
    return (byte & MIDI_STATUS_BIT) != 0;
}

static bool midi_is_channel_status(uint8_t status)
{
    return status >= MIDI_STATUS_NOTE_OFF && status < MIDI_STATUS_SYSEX_START;
}

static bool midi_is_realtime_status(uint8_t status)
{
    return status >= MIDI_STATUS_REALTIME_MIN;
}

static uint8_t midi_wire_size(uint8_t status)
{
    if (midi_is_channel_status(status))
    {
        switch (status & MIDI_STATUS_GROUP_MASK)
        {
        case MIDI_STATUS_PROGRAM_CHANGE:
        case MIDI_STATUS_CHANNEL_PRESSURE:
            return 2;
        case MIDI_STATUS_NOTE_OFF:
        case MIDI_STATUS_NOTE_ON:
        case MIDI_STATUS_AFTERTOUCH:
        case MIDI_STATUS_CONTROL_CHANGE:
        case MIDI_STATUS_PITCH_BEND:
            return 3;
        default:
            return 0;
        }
    }

    switch (status)
    {
    case MIDI_STATUS_TIME_CODE:
    case MIDI_STATUS_SONG_SELECT:
        return 2;
    case MIDI_STATUS_SONG_POSITION:
        return 3;
    case MIDI_STATUS_TUNE_REQUEST:
        return 1;
    default:
        if (midi_is_realtime_status(status))
        {
            return 1;
        }
        return 0;
    }
}

static uint8_t midi_usb_cin_for_status(uint8_t status)
{
    if (midi_is_channel_status(status))
    {
        return (status & MIDI_STATUS_GROUP_MASK) >> 4;
    }

    switch (status)
    {
    case MIDI_STATUS_TIME_CODE:
    case MIDI_STATUS_SONG_SELECT:
        return USB_MIDI_CIN_2BYTE_SYSTEM;
    case MIDI_STATUS_SONG_POSITION:
        return USB_MIDI_CIN_3BYTE_SYSTEM;
    default:
        return USB_MIDI_CIN_SINGLE_BYTE;
    }
}

static uint8_t midi_usb_payload_size(MIDIEventPacket* event)
{
    switch (event->Event & MIDI_CHANNEL_MASK)
    {
    case USB_MIDI_CIN_2BYTE_SYSTEM:
    case USB_MIDI_CIN_SYSEX_END_2:
    case MIDI_STATUS_PROGRAM_CHANGE >> 4:
    case MIDI_STATUS_CHANNEL_PRESSURE >> 4:
        return 2;
    case USB_MIDI_CIN_3BYTE_SYSTEM:
    case USB_MIDI_CIN_SYSEX_CONTINUE:
    case USB_MIDI_CIN_SYSEX_END_3:
    case USB_MIDI_CIN_NOTE_OFF:
    case USB_MIDI_CIN_NOTE_ON:
    case MIDI_STATUS_AFTERTOUCH >> 4:
    case USB_MIDI_CIN_CONTROL_CHANGE:
    case USB_MIDI_CIN_PITCH_BEND:
        return 3;
    case USB_MIDI_CIN_SYSEX_END_1:
    case USB_MIDI_CIN_SINGLE_BYTE:
        return 1;
    default:
        return midi_wire_size(event->Data1);
    }
}

static void midi_rx_reset(MidiInputQueue* queue)
{
    queue->head = 0;
    queue->tail = 0;
    queue->count = 0;
}

static void midi_rx_push(MidiInputQueue* queue, uint8_t value)
{
    if (queue->count == MIDI_RX_CAPACITY)
    {
        return;
    }
    queue->data[queue->tail] = value;
    queue->tail = (uint8_t)((queue->tail + 1) % MIDI_RX_CAPACITY);
    queue->count++;
}

static bool midi_rx_pop(MidiInputQueue* queue, uint8_t* value)
{
    if (queue->count == 0)
    {
        return false;
    }
    *value = queue->data[queue->head];
    queue->head = (uint8_t)((queue->head + 1) % MIDI_RX_CAPACITY);
    queue->count--;
    return true;
}

static void midi_runtime_clear_message(MidiRuntime* runtime)
{
    runtime->active_status = 0;
    runtime->payload_count = 0;
    runtime->payload_needed = 0;
}

static MIDIMessageType midi_message_type(uint8_t status)
{
    if (midi_is_channel_status(status))
    {
        switch (status & MIDI_STATUS_GROUP_MASK)
        {
        case MIDI_STATUS_NOTE_OFF:
            return MIDI_MESSAGE_NOTE_OFF;
        case MIDI_STATUS_NOTE_ON:
            return MIDI_MESSAGE_NOTE_ON;
        case MIDI_STATUS_AFTERTOUCH:
            return MIDI_MESSAGE_AFTERTOUCH;
        case MIDI_STATUS_CONTROL_CHANGE:
            return MIDI_MESSAGE_CONTROL_CHANGE;
        case MIDI_STATUS_PROGRAM_CHANGE:
            return MIDI_MESSAGE_PROGRAM_CHANGE;
        case MIDI_STATUS_CHANNEL_PRESSURE:
            return MIDI_MESSAGE_CHANNEL_PRESSURE;
        case MIDI_STATUS_PITCH_BEND:
            return MIDI_MESSAGE_PITCH_BEND;
        default:
            break;
        }
    }

    switch (status)
    {
    case MIDI_STATUS_TIME_CODE:
        return MIDI_MESSAGE_TIME_CODE;
    case MIDI_STATUS_SONG_POSITION:
        return MIDI_MESSAGE_SONG_POSITION;
    case MIDI_STATUS_SONG_SELECT:
        return MIDI_MESSAGE_SONG_SELECT;
    case MIDI_STATUS_TUNE_REQUEST:
        return MIDI_MESSAGE_TUNE_REQUEST;
    default:
        if (midi_is_realtime_status(status))
        {
            return MIDI_MESSAGE_REALTIME;
        }
        return MIDI_MESSAGE_SYSEX;
    }
}

static void midi_message_init(MIDIMessage* message, MIDIMessageType type, uint8_t status, uint8_t length)
{
    message->type = type;
    message->status = status;
    message->channel = midi_is_channel_status(status) ? (status & MIDI_CHANNEL_MASK) : MIDI_MESSAGE_NO_CHANNEL;
    message->bytes[0] = 0;
    message->bytes[1] = 0;
    message->bytes[2] = 0;
    message->length = length;
    message->sysex_start = false;
    message->sysex_end = false;
}

#if MIDI_DEFAULT_AUDIO_HANDLER_ENABLE
static void midi_audio_receive(const MIDIMessage* message)
{
#ifdef AUDIO_ENABLE
    if (message->length == 3)
    {
        switch (message->status & MIDI_STATUS_GROUP_MASK)
        {
        case MIDI_STATUS_NOTE_ON:
            midi_play_note(440.0f * powf(2.0f, ((message->bytes[1] & MIDI_DATA_LIMIT) - 57) / 12.0f), (message->bytes[2] & MIDI_DATA_LIMIT) / 8);
            break;
        case MIDI_STATUS_NOTE_OFF:
            midi_stop_note(440.0f * powf(2.0f, ((message->bytes[1] & MIDI_DATA_LIMIT) - 57) / 12.0f));
            break;
        default:
            break;
        }
    }
    if (message->status == MIDI_STATUS_STOP)
    {
        midi_stop_all_notes();
    }
#else
    UNUSED(message);
#endif
}
#endif

static void midi_dispatch_message(uint8_t status, uint8_t data1, uint8_t data2)
{
    MIDIMessage message;
    uint8_t length = midi_wire_size(status);
    midi_message_init(&message, midi_message_type(status), status, length);
    message.bytes[0] = status;
    if (length >= 2)
    {
        message.bytes[1] = data1 & MIDI_DATA_LIMIT;
    }
    if (length >= 3)
    {
        message.bytes[2] = data2 & MIDI_DATA_LIMIT;
    }

    midi_message_callback(&message);
#if MIDI_DEFAULT_AUDIO_HANDLER_ENABLE
    midi_audio_receive(&message);
#endif
}

static void midi_dispatch_sysex_byte(uint8_t byte, bool start, bool end)
{
    MIDIMessage message;
    midi_message_init(&message, MIDI_MESSAGE_SYSEX, end ? MIDI_STATUS_SYSEX_END : MIDI_STATUS_SYSEX_START, 1);
    message.bytes[0] = byte;
    message.sysex_start = start;
    message.sysex_end = end;

    midi_message_callback(&message);
}

static void midi_start_message(MidiRuntime* runtime, uint8_t status)
{
    uint8_t size = midi_wire_size(status);

    runtime->sysex_open = false;
    runtime->active_status = status;
    runtime->payload_count = 0;
    runtime->payload_needed = size > 0 ? (uint8_t)(size - 1) : 0;
    runtime->running_status = midi_is_channel_status(status) ? status : 0;

    if (size == 1)
    {
        midi_dispatch_message(status, 0, 0);
        midi_runtime_clear_message(runtime);
    }
}

static void midi_feed_byte(MidiRuntime* runtime, uint8_t byte)
{
    if (midi_is_realtime_status(byte))
    {
        midi_dispatch_message(byte, 0, 0);
        return;
    }

    if (midi_is_status(byte))
    {
        if (byte == MIDI_STATUS_SYSEX_START)
        {
            runtime->sysex_open = true;
            runtime->running_status = 0;
            midi_runtime_clear_message(runtime);
            midi_dispatch_sysex_byte(byte, true, false);
            return;
        }

        if (byte == MIDI_STATUS_SYSEX_END)
        {
            runtime->sysex_open = false;
            midi_runtime_clear_message(runtime);
            midi_dispatch_sysex_byte(byte, false, true);
            return;
        }

        if (midi_wire_size(byte) > 0)
        {
            midi_start_message(runtime, byte);
        }
        else
        {
            runtime->sysex_open = false;
            runtime->running_status = 0;
            midi_runtime_clear_message(runtime);
        }
        return;
    }

    if (runtime->sysex_open)
    {
        midi_dispatch_sysex_byte(byte & MIDI_DATA_LIMIT, false, false);
        return;
    }

    if (runtime->active_status == 0)
    {
        if (runtime->running_status == 0)
        {
            return;
        }
        midi_start_message(runtime, runtime->running_status);
    }

    if (runtime->payload_count < sizeof(runtime->payload))
    {
        runtime->payload[runtime->payload_count++] = byte & MIDI_DATA_LIMIT;
    }

    if (runtime->payload_count >= runtime->payload_needed)
    {
        uint8_t status = runtime->active_status;
        uint8_t data1 = runtime->payload[0];
        uint8_t data2 = runtime->payload_needed > 1 ? runtime->payload[1] : 0;

        midi_dispatch_message(status, data1, data2);

        if (runtime->running_status != 0 && status == runtime->running_status)
        {
            runtime->active_status = runtime->running_status;
            runtime->payload_count = 0;
            runtime->payload_needed = (uint8_t)(midi_wire_size(runtime->running_status) - 1);
        }
        else
        {
            midi_runtime_clear_message(runtime);
        }
    }
}

static void midi_runtime_init(MidiRuntime* runtime)
{
    midi_rx_reset(&runtime->input);
    runtime->running_status = 0;
    runtime->sysex_open = false;
    midi_runtime_clear_message(runtime);
}

static void midi_runtime_process(MidiRuntime* runtime)
{
    uint8_t value;
    while (midi_rx_pop(&runtime->input, &value))
    {
        midi_feed_byte(runtime, value);
    }
}

static void midi_runtime_input_packet(MidiRuntime* runtime, MIDIEventPacket* event)
{
    uint8_t size = midi_usb_payload_size(event);
    if (size >= 1)
    {
        midi_rx_push(&runtime->input, event->Data1);
    }
    if (size >= 2)
    {
        midi_rx_push(&runtime->input, event->Data2);
    }
    if (size >= 3)
    {
        midi_rx_push(&runtime->input, event->Data3);
    }
}

static int midi_send_packet(uint8_t cin, uint8_t data1, uint8_t data2, uint8_t data3)
{
    MIDIEventPacket event;
    event.Event = usb_midi_event(USB_MIDI_CABLE, cin);
    event.Data1 = data1;
    event.Data2 = data2;
    event.Data3 = data3;
    return midi_send(&event);
}

static int midi_send_message3(uint8_t status, uint8_t data1, uint8_t data2)
{
    return midi_send_packet(midi_usb_cin_for_status(status), status, data1 & MIDI_DATA_LIMIT, data2 & MIDI_DATA_LIMIT);
}

static int midi_send_cc(uint8_t channel, uint8_t number, uint8_t value)
{
    return midi_send_message3(MIDI_STATUS_CONTROL_CHANGE | (channel & MIDI_CHANNEL_MASK), number, value);
}

static int midi_send_note_on(uint8_t channel, uint8_t note, uint8_t velocity)
{
    return midi_send_message3(MIDI_STATUS_NOTE_ON | (channel & MIDI_CHANNEL_MASK), note, velocity);
}

static int midi_send_note_off(uint8_t channel, uint8_t note, uint8_t velocity)
{
    return midi_send_message3(MIDI_STATUS_NOTE_OFF | (channel & MIDI_CHANNEL_MASK), note, velocity);
}

static int midi_send_pitch_bend(uint8_t channel, int16_t amount)
{
    uint16_t value;
    if (amount > 0x1fff)
    {
        value = 0x3FFF;
    }
    else if (amount < -0x2000)
    {
        value = 0;
    }
    else
    {
        value = (uint16_t)(amount + 0x2000);
    }
    return midi_send_message3(MIDI_STATUS_PITCH_BEND | (channel & MIDI_CHANNEL_MASK), value & MIDI_DATA_LIMIT, (value >> 7) & MIDI_DATA_LIMIT);
}

void midi_init(void)
{
    midi_config.octave = MIDI_OCTAVE_2 - MIDI_OCTAVE_MIN;
    midi_config.transpose = 0;
    midi_config.velocity = 127;
    midi_config.channel = 0;
    midi_config.modulation_interval = 8;

    for (uint8_t i = 0; i < MIDI_TONE_COUNT; i++)
    {
        tone_status[i] = MIDI_INVALID_NOTE;
    }

    midi_modulation = 0;
    midi_modulation_step = 0;
    midi_modulation_timer = 0;
    midi_runtime_init(&midi_runtime);
}

uint8_t midi_compute_note(uint16_t keycode)
{
    return 12 * midi_config.octave + (keycode - MIDI_TONE_MIN) + midi_config.transpose;
}

void midi_event_handler(KeyboardEvent event)
{
    uint8_t keycode = KEYCODE_GET_SUB(event.keycode);
    uint8_t velocity = 0;
    if (IS_ADVANCED_KEY(event.key))
    {
        float intensity = fabs(((AdvancedKey*)event.key)->difference * (POLLING_RATE / 1000) / (float)MIDI_REF_VELOCITY);
        if (intensity > 1.0f)
        {
            intensity = 1.0f;
        }
        velocity = intensity * 127;
    }
    else
    {
        velocity = midi_config.velocity;
    }

    if (KEYCODE_GET_MAIN(event.keycode) == MIDI_NOTE)
    {
        uint8_t channel = midi_config.channel;
        switch (event.event)
        {
        case KEYBOARD_EVENT_KEY_DOWN:
            (void)midi_send_note_on(channel, keycode, velocity);
            break;
        case KEYBOARD_EVENT_KEY_UP:
            (void)midi_send_note_off(channel, keycode, velocity);
            break;
        default:
            break;
        }
        return;
    }

    switch (keycode)
    {
    case MIDI_TONE_MIN ... MIDI_TONE_MAX:
    {
        uint8_t channel = midi_config.channel;
        uint8_t tone = keycode - MIDI_TONE_MIN;
        if (event.event == KEYBOARD_EVENT_KEY_DOWN)
        {
            if (tone_status[tone] == MIDI_INVALID_NOTE)
            {
                uint8_t note = midi_compute_note(keycode);
                (void)midi_send_note_on(channel, note, velocity);
                tone_status[tone] = note;
            }
        }
        else
        {
            uint8_t note = tone_status[tone];
            if (note != MIDI_INVALID_NOTE)
            {
                (void)midi_send_note_off(channel, note, velocity);
            }
            tone_status[tone] = MIDI_INVALID_NOTE;
        }
        return;
    }
    case MIDI_OCTAVE_MIN ... MIDI_OCTAVE_MAX:
        if (event.event == KEYBOARD_EVENT_KEY_DOWN)
        {
            midi_config.octave = keycode - MIDI_OCTAVE_MIN;
        }
        return;
    case MIDI_OCTAVE_DOWN:
        if (event.event == KEYBOARD_EVENT_KEY_DOWN && midi_config.octave > 0)
        {
            midi_config.octave--;
        }
        return;
    case MIDI_OCTAVE_UP:
        if (event.event == KEYBOARD_EVENT_KEY_DOWN && midi_config.octave < (MIDI_OCTAVE_MAX - MIDI_OCTAVE_MIN))
        {
            midi_config.octave++;
        }
        return;
    case MIDI_TRANSPOSE_MIN ... MIDI_TRANSPOSE_MAX:
        if (event.event == KEYBOARD_EVENT_KEY_DOWN)
        {
            midi_config.transpose = keycode - MIDI_TRANSPOSE_0;
        }
        return;
    case MIDI_TRANSPOSE_DOWN:
        if (event.event == KEYBOARD_EVENT_KEY_DOWN && midi_config.transpose > (MIDI_TRANSPOSE_MIN - MIDI_TRANSPOSE_0))
        {
            midi_config.transpose--;
        }
        return;
    case MIDI_TRANSPOSE_UP:
        if (event.event == KEYBOARD_EVENT_KEY_DOWN && midi_config.transpose < (MIDI_TRANSPOSE_MAX - MIDI_TRANSPOSE_0))
        {
            const bool positive = midi_config.transpose > 0;
            midi_config.transpose++;
            if (positive && midi_config.transpose < 0)
            {
                midi_config.transpose--;
            }
        }
        return;
    case MIDI_VELOCITY_MIN ... MIDI_VELOCITY_MAX:
        if (event.event == KEYBOARD_EVENT_KEY_DOWN)
        {
            midi_config.velocity = (uint8_t)(keycode - MIDI_VELOCITY_MIN) * (128 / (MIDI_VELOCITY_MAX - MIDI_VELOCITY_MIN));
        }
        return;
    case MIDI_VELOCITY_DOWN:
        if (event.event == KEYBOARD_EVENT_KEY_DOWN && midi_config.velocity > 0)
        {
            if (midi_config.velocity == 127)
            {
                midi_config.velocity -= 10;
            }
            else if (midi_config.velocity > 12)
            {
                midi_config.velocity -= 13;
            }
            else
            {
                midi_config.velocity = 0;
            }
        }
        return;
    case MIDI_VELOCITY_UP:
        if (event.event == KEYBOARD_EVENT_KEY_DOWN && midi_config.velocity < 127)
        {
            if (midi_config.velocity < 115)
            {
                midi_config.velocity += 13;
            }
            else
            {
                midi_config.velocity = 127;
            }
        }
        return;
    case MIDI_CHANNEL_MIN ... MIDI_CHANNEL_MAX:
        if (event.event == KEYBOARD_EVENT_KEY_DOWN)
        {
            midi_config.channel = keycode - MIDI_CHANNEL_MIN;
        }
        return;
    case MIDI_CHANNEL_DOWN:
        if (event.event == KEYBOARD_EVENT_KEY_DOWN)
        {
            midi_config.channel--;
        }
        return;
    case MIDI_CHANNEL_UP:
        if (event.event == KEYBOARD_EVENT_KEY_DOWN)
        {
            midi_config.channel++;
        }
        return;
    case MIDI_ALL_NOTES_OFF:
        if (event.event == KEYBOARD_EVENT_KEY_DOWN)
        {
            (void)midi_send_cc(midi_config.channel, 0x7B, 0);
        }
        return;
    case MIDI_SUSTAIN:
        (void)midi_send_cc(midi_config.channel, 0x40, event.event == KEYBOARD_EVENT_KEY_DOWN ? 127 : 0);
        return;
    case MIDI_PORTAMENTO:
        (void)midi_send_cc(midi_config.channel, 0x41, event.event == KEYBOARD_EVENT_KEY_DOWN ? 127 : 0);
        return;
    case MIDI_SOSTENUTO:
        (void)midi_send_cc(midi_config.channel, 0x42, event.event == KEYBOARD_EVENT_KEY_DOWN ? 127 : 0);
        return;
    case MIDI_SOFT:
        (void)midi_send_cc(midi_config.channel, 0x43, event.event == KEYBOARD_EVENT_KEY_DOWN ? 127 : 0);
        return;
    case MIDI_LEGATO:
        (void)midi_send_cc(midi_config.channel, 0x44, event.event == KEYBOARD_EVENT_KEY_DOWN ? 127 : 0);
        return;
    case MIDI_MODULATION:
        midi_modulation_step = event.event == KEYBOARD_EVENT_KEY_DOWN ? 1 : -1;
        return;
    case MIDI_MODULATION_SPEED_DOWN:
        if (event.event == KEYBOARD_EVENT_KEY_DOWN)
        {
            midi_config.modulation_interval++;
            if (midi_config.modulation_interval == 0)
            {
                midi_config.modulation_interval--;
            }
        }
        return;
    case MIDI_MODULATION_SPEED_UP:
        if (event.event == KEYBOARD_EVENT_KEY_DOWN && midi_config.modulation_interval > 0)
        {
            midi_config.modulation_interval--;
        }
        return;
    case MIDI_PITCH_BEND_DOWN:
        if (event.event == KEYBOARD_EVENT_KEY_DOWN)
        {
            (void)midi_send_pitch_bend(midi_config.channel, -0x2000);
        }
        else
        {
            (void)midi_send_pitch_bend(midi_config.channel, 0);
        }
        return;
    case MIDI_PITCH_BEND_UP:
        if (event.event == KEYBOARD_EVENT_KEY_DOWN)
        {
            (void)midi_send_pitch_bend(midi_config.channel, 0x1fff);
        }
        else
        {
            (void)midi_send_pitch_bend(midi_config.channel, 0);
        }
        return;
    default:
        return;
    }
}

int midi_send(MIDIEventPacket* event)
{
    if (!event)
    {
        return 1;
    }
    return send_midi((uint8_t*)event, sizeof(MIDIEventPacket));
}

void midi_input_callback(MIDIEventPacket* event)
{
    if (!event)
    {
        return;
    }
    midi_runtime_input_packet(&midi_runtime, event);
}

void midi_task(void)
{
    midi_runtime_process(&midi_runtime);
#ifdef MIDI_ADVANCED
    if ((int32_t)(KEYBOARD_TICK_TO_TIME(g_keyboard_tick) - midi_modulation_timer) < midi_config.modulation_interval)
    {
        return;
    }
    midi_modulation_timer = KEYBOARD_TICK_TO_TIME(g_keyboard_tick);

    if (midi_modulation_step != 0)
    {
        (void)midi_send_cc(midi_config.channel, 0x1, midi_modulation);

        if (midi_modulation_step < 0 && midi_modulation < -midi_modulation_step)
        {
            midi_modulation = 0;
            midi_modulation_step = 0;
            return;
        }

        midi_modulation += midi_modulation_step;

        if (midi_modulation > 127)
        {
            midi_modulation = 127;
        }
    }
#endif
}

__WEAK void midi_play_note(float frequency, uint8_t velocity)
{
    UNUSED(frequency);
    UNUSED(velocity);
}

__WEAK void midi_message_callback(const MIDIMessage* message)
{
    UNUSED(message);
}

__WEAK void midi_stop_note(float frequency)
{
    UNUSED(frequency);
}

__WEAK void midi_stop_all_notes(void)
{
    
}
