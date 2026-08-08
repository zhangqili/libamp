#ifndef LIBAMP_TEST_FIXTURE_H_
#define LIBAMP_TEST_FIXTURE_H_

#include "amp_protocol.h"
#include "midi.h"
#include "rgb.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LIBAMP_TEST_REPORT_BUFFER_SIZE AMP_FRAME_REPORT_SIZE

extern uint8_t shared_ep_send_buffer[LIBAMP_TEST_REPORT_BUFFER_SIZE];
extern uint8_t keyboard_send_buffer[LIBAMP_TEST_REPORT_BUFFER_SIZE];
extern uint8_t raw_send_buffer[LIBAMP_TEST_REPORT_BUFFER_SIZE];
extern int raw_send_result;
extern uint32_t raw_send_count;
extern uint32_t amp_transport_prepare_session_count;
extern uint8_t midi_send_buffer[LIBAMP_TEST_REPORT_BUFFER_SIZE];
extern uint8_t gamepad_send_buffer[LIBAMP_TEST_REPORT_BUFFER_SIZE];
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
