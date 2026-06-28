#ifndef LIBAMP_TEST_FIXTURE_H_
#define LIBAMP_TEST_FIXTURE_H_

#include "midi.h"
#include "rgb.h"

#ifdef __cplusplus
extern "C" {
#endif

extern uint8_t shared_ep_send_buffer[64];
extern uint8_t keyboard_send_buffer[64];
extern uint8_t raw_send_buffer[64];
extern uint8_t midi_send_buffer[64];
extern ColorRGB led_color_buffer[RGB_NUM];
extern uint32_t led_flush_count;
extern uint32_t audio_play_note_count;
extern uint32_t audio_stop_note_count;
extern uint32_t audio_stop_all_notes_count;
extern float audio_last_play_frequency;
extern float audio_last_stop_frequency;
extern uint8_t audio_last_play_velocity;
extern uint32_t midi_message_callback_count;
extern MIDIMessage midi_last_message;

void libamp_test_reset_environment(void);
void libamp_test_clear_output_buffers(void);

#ifdef __cplusplus
}
#endif

#endif /* LIBAMP_TEST_FIXTURE_H_ */
