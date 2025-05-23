#include "qmk_midi.h"
#include "sysex_tools.h"
#include "midi.h"
#include "usb_descriptor.h"
#include "process_midi.h"
#include "driver.h"

#ifdef AUDIO_ENABLE
#    include "audio.h"
#    include <math.h>
#endif

/*******************************************************************************
 * MIDI
 ******************************************************************************/

MIDIDevice midi_device;

#define SYSEX_START_OR_CONT 0x40
#define SYSEX_ENDS_IN_1 0x50
#define SYSEX_ENDS_IN_2 0x60
#define SYSEX_ENDS_IN_3 0x70

#define SYS_COMMON_1 0x50
#define SYS_COMMON_2 0x20
#define SYS_COMMON_3 0x30

static void usb_send_func(MIDIDevice* device, uint16_t cnt, uint8_t byte0, uint8_t byte1, uint8_t byte2) {
    UNUSED(device);
    MIDIEventPacket event;
    event.Data1 = byte0;
    event.Data2 = byte1;
    event.Data3 = byte2;

    uint8_t cable = 0;

    // if the length is undefined we assume it is a SYSEX message
    if (midi_packet_length(byte0) == UNDEFINED) {
        switch (cnt) {
            case 3:
                if (byte2 == SYSEX_END)
                    event.Event = MIDI_EVENT(cable, SYSEX_ENDS_IN_3);
                else
                    event.Event = MIDI_EVENT(cable, SYSEX_START_OR_CONT);
                break;
            case 2:
                if (byte1 == SYSEX_END)
                    event.Event = MIDI_EVENT(cable, SYSEX_ENDS_IN_2);
                else
                    event.Event = MIDI_EVENT(cable, SYSEX_START_OR_CONT);
                break;
            case 1:
                if (byte0 == SYSEX_END)
                    event.Event = MIDI_EVENT(cable, SYSEX_ENDS_IN_1);
                else
                    event.Event = MIDI_EVENT(cable, SYSEX_START_OR_CONT);
                break;
            default:
                return; // invalid cnt
        }
    } else {
        // deal with 'system common' messages
        // TODO are there any more?
        switch (byte0 & 0xF0) {
            case MIDI_SONGPOSITION:
                event.Event = MIDI_EVENT(cable, SYS_COMMON_3);
                break;
            case MIDI_SONGSELECT:
            case MIDI_TC_QUARTERFRAME:
                event.Event = MIDI_EVENT(cable, SYS_COMMON_2);
                break;
            default:
                event.Event = MIDI_EVENT(cable, byte0);
                break;
        }
    }

    send_midi_packet(&event);
}

static void usb_get_midi(MIDIDevice* device) {
    MIDIEventPacket event;
    while (recv_midi_packet(&event)) {
        midi_packet_length_t length = midi_packet_length(event.Data1);
        uint8_t              input[3];
        input[0] = event.Data1;
        input[1] = event.Data2;
        input[2] = event.Data3;
        if (length == UNDEFINED) {
            // sysex
            if (event.Event == MIDI_EVENT(0, SYSEX_START_OR_CONT) || event.Event == MIDI_EVENT(0, SYSEX_ENDS_IN_3)) {
                length = 3;
            } else if (event.Event == MIDI_EVENT(0, SYSEX_ENDS_IN_2)) {
                length = 2;
            } else if (event.Event == MIDI_EVENT(0, SYSEX_ENDS_IN_1)) {
                length = 1;
            } else {
                // XXX what to do?
            }
        }

        // pass the data to the device input function
        if (length != UNDEFINED) midi_device_input(device, length, input);
    }
}

static void fallthrough_callback(MIDIDevice* device, uint16_t cnt, uint8_t byte0, uint8_t byte1, uint8_t byte2) {
#ifdef AUDIO_ENABLE
    if (cnt == 3) {
        switch (byte0 & 0xF0) {
            case MIDI_NOTEON:
                play_note(440.0f * powf(2.0f, ((byte1 & 0x7F) - 57) / 12.0f), (byte2 & 0x7F) / 8);
                break;
            case MIDI_NOTEOFF:
                stop_note(440.0f * powf(2.0f, ((byte1 & 0x7F) - 57) / 12.0f));
                break;
        }
    }
    if (byte0 == MIDI_STOP) {
        stop_all_notes();
    }
#endif
}

static void cc_callback(MIDIDevice* device, uint8_t chan, uint8_t num, uint8_t val) {
    // sending it back on the next channel
    // midi_send_cc(device, (chan + 1) % 16, num, val);
}

void midi_init(void);

void setup_midi(void) {
    midi_init();
    midi_device_init(&midi_device);
    midi_device_set_send_func(&midi_device, usb_send_func);
    midi_device_set_pre_input_process_func(&midi_device, usb_get_midi);
    midi_register_fallthrough_callback(&midi_device, fallthrough_callback);
    midi_register_cc_callback(&midi_device, cc_callback);
}


__WEAK void send_midi_packet(MIDIEventPacket* event)
{
    send_midi((uint8_t*)event, sizeof(MIDIEventPacket));
}

__WEAK bool recv_midi_packet(MIDIEventPacket* const event)
{
    UNUSED(event);
    return false;
}
