/*
 * Copyright (c) 2026 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef LIBAMP_TEST_AUDIO_H_
#define LIBAMP_TEST_AUDIO_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void play_note(float frequency, uint8_t velocity);
void stop_note(float frequency);
void stop_all_notes(void);

#ifdef __cplusplus
}
#endif

#endif /* LIBAMP_TEST_AUDIO_H_ */
