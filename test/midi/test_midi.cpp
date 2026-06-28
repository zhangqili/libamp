#include <gtest/gtest.h>

#include "keyboard.h"
#include "midi.h"
#include "test_fixture.h"

namespace {

constexpr uint8_t kCinNoteOff = 0x08;
constexpr uint8_t kCinNoteOn = 0x09;
constexpr uint8_t kCinControlChange = 0x0B;
constexpr uint8_t kCinPitchBend = 0x0E;
constexpr uint8_t kCinSingleByte = 0x0F;
constexpr uint8_t kCinSysexContinue = 0x04;
constexpr uint8_t kCinSysexEnd2 = 0x06;

Key digital_key;

MIDIEventPacket make_packet(uint8_t cin, uint8_t data1, uint8_t data2 = 0, uint8_t data3 = 0)
{
    MIDIEventPacket packet = {};
    packet.Event = cin;
    packet.Data1 = data1;
    packet.Data2 = data2;
    packet.Data3 = data3;
    return packet;
}

KeyboardEvent make_midi_event(Keycode keycode, KeyboardEventType type)
{
    digital_key.id = ADVANCED_KEY_NUM;
    return MK_EVENT(keycode, type, &digital_key);
}

void expect_midi_packet(uint8_t event, uint8_t data1, uint8_t data2, uint8_t data3)
{
    EXPECT_EQ(event, midi_send_buffer[0]);
    EXPECT_EQ(data1, midi_send_buffer[1]);
    EXPECT_EQ(data2, midi_send_buffer[2]);
    EXPECT_EQ(data3, midi_send_buffer[3]);
}

class MidiTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        libamp_test_reset_environment();
        midi_init();
        libamp_test_clear_output_buffers();
    }
};

} // namespace

TEST_F(MidiTest, SendRejectsNullAndWritesPacket)
{
    EXPECT_EQ(1, midi_send(nullptr));

    MIDIEventPacket packet = make_packet(kCinNoteOn, 0x90, 60, 127);
    EXPECT_EQ(0, midi_send(&packet));

    expect_midi_packet(kCinNoteOn, 0x90, 60, 127);
}

TEST_F(MidiTest, ToneKeyDownAndUpSendNotePackets)
{
    midi_event_handler(make_midi_event(QK_MIDI_NOTE_C_0, KEYBOARD_EVENT_KEY_DOWN));
    expect_midi_packet(kCinNoteOn, 0x90, 48, 127);

    libamp_test_clear_output_buffers();
    midi_event_handler(make_midi_event(QK_MIDI_NOTE_C_0, KEYBOARD_EVENT_KEY_UP));
    expect_midi_packet(kCinNoteOff, 0x80, 48, 127);
}

TEST_F(MidiTest, SustainAndAllNotesOffSendControlChange)
{
    midi_event_handler(make_midi_event(QK_MIDI_SUSTAIN, KEYBOARD_EVENT_KEY_DOWN));
    expect_midi_packet(kCinControlChange, 0xB0, 0x40, 127);

    libamp_test_clear_output_buffers();
    midi_event_handler(make_midi_event(QK_MIDI_ALL_NOTES_OFF, KEYBOARD_EVENT_KEY_DOWN));
    expect_midi_packet(kCinControlChange, 0xB0, 0x7B, 0);
}

TEST_F(MidiTest, PitchBendKeysSendFourteenBitValues)
{
    midi_event_handler(make_midi_event(QK_MIDI_PITCH_BEND_DOWN, KEYBOARD_EVENT_KEY_DOWN));
    expect_midi_packet(kCinPitchBend, 0xE0, 0x00, 0x00);

    libamp_test_clear_output_buffers();
    midi_event_handler(make_midi_event(QK_MIDI_PITCH_BEND_DOWN, KEYBOARD_EVENT_KEY_UP));
    expect_midi_packet(kCinPitchBend, 0xE0, 0x00, 0x40);

    libamp_test_clear_output_buffers();
    midi_event_handler(make_midi_event(QK_MIDI_PITCH_BEND_UP, KEYBOARD_EVENT_KEY_DOWN));
    expect_midi_packet(kCinPitchBend, 0xE0, 0x7F, 0x7F);
}

TEST_F(MidiTest, UsbInputNotePacketsDriveAudioCallbacks)
{
    MIDIEventPacket note_on = make_packet(kCinNoteOn, 0x90, 60, 64);
    midi_input_callback(&note_on);
    EXPECT_EQ(0u, audio_play_note_count);

    midi_task();

    EXPECT_EQ(1u, audio_play_note_count);
    EXPECT_EQ(8u, audio_last_play_velocity);
    EXPECT_GT(audio_last_play_frequency, 520.0f);
    EXPECT_LT(audio_last_play_frequency, 525.0f);

    MIDIEventPacket note_off = make_packet(kCinNoteOff, 0x80, 60, 64);
    midi_input_callback(&note_off);
    midi_task();

    EXPECT_EQ(1u, audio_stop_note_count);
    EXPECT_GT(audio_last_stop_frequency, 520.0f);
    EXPECT_LT(audio_last_stop_frequency, 525.0f);
}

TEST_F(MidiTest, UsbInputNotePacketDispatchesMessageCallback)
{
    MIDIEventPacket note_on = make_packet(kCinNoteOn, 0x90, 60, 64);
    midi_input_callback(&note_on);
    EXPECT_EQ(0u, midi_message_callback_count);

    midi_task();

    EXPECT_EQ(1u, midi_message_callback_count);
    EXPECT_EQ(MIDI_MESSAGE_NOTE_ON, midi_last_message.type);
    EXPECT_EQ(0x90, midi_last_message.status);
    EXPECT_EQ(0u, midi_last_message.channel);
    EXPECT_EQ(3u, midi_last_message.length);
    EXPECT_EQ(0x90, midi_last_message.bytes[0]);
    EXPECT_EQ(60u, midi_last_message.bytes[1]);
    EXPECT_EQ(64u, midi_last_message.bytes[2]);
    EXPECT_FALSE(midi_last_message.sysex_start);
    EXPECT_FALSE(midi_last_message.sysex_end);
}

TEST_F(MidiTest, UsbInputRealtimeStopStopsAllNotes)
{
    MIDIEventPacket stop = make_packet(kCinSingleByte, 0xFC);
    midi_input_callback(&stop);
    midi_task();

    EXPECT_EQ(1u, midi_message_callback_count);
    EXPECT_EQ(MIDI_MESSAGE_REALTIME, midi_last_message.type);
    EXPECT_EQ(0xFC, midi_last_message.status);
    EXPECT_EQ(MIDI_MESSAGE_NO_CHANNEL, midi_last_message.channel);
    EXPECT_EQ(1u, midi_last_message.length);
    EXPECT_EQ(0xFC, midi_last_message.bytes[0]);
    EXPECT_EQ(1u, audio_stop_all_notes_count);
}

TEST_F(MidiTest, UsbInputSysexPacketsDrainWithoutAudioSideEffects)
{
    MIDIEventPacket start = make_packet(kCinSysexContinue, 0xF0, 0x7D, 0x01);
    MIDIEventPacket body = make_packet(kCinSysexContinue, 0x02, 0x03, 0x04);
    MIDIEventPacket end = make_packet(kCinSysexEnd2, 0x05, 0xF7);

    midi_input_callback(&start);
    midi_input_callback(&body);
    midi_input_callback(&end);
    midi_task();
    midi_task();

    EXPECT_EQ(8u, midi_message_callback_count);
    EXPECT_EQ(MIDI_MESSAGE_SYSEX, midi_last_message.type);
    EXPECT_EQ(0xF7, midi_last_message.status);
    EXPECT_EQ(MIDI_MESSAGE_NO_CHANNEL, midi_last_message.channel);
    EXPECT_EQ(1u, midi_last_message.length);
    EXPECT_EQ(0xF7, midi_last_message.bytes[0]);
    EXPECT_FALSE(midi_last_message.sysex_start);
    EXPECT_TRUE(midi_last_message.sysex_end);
    EXPECT_EQ(0u, audio_play_note_count);
    EXPECT_EQ(0u, audio_stop_note_count);
    EXPECT_EQ(0u, audio_stop_all_notes_count);
}
