#ifndef LIBAMP_TEST_FIXTURE_H_
#define LIBAMP_TEST_FIXTURE_H_

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

void libamp_test_reset_environment(void);
void libamp_test_clear_output_buffers(void);

#ifdef __cplusplus
}
#endif

#endif /* LIBAMP_TEST_FIXTURE_H_ */
