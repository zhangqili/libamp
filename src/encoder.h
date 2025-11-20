/*
 * Copyright (c) 2025 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef ENCODER_H_
#define ENCODER_H_

#include "keyboard.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef ENCODER_ENABLE
#ifndef ENCODER_NUM
#define ENCODER_NUM 0
#endif

#ifndef ENCODER_TAP_DElAY
#define ENCODER_TAP_DElAY 8
#endif

typedef struct __Encoder
{
    int32_t count;
    int16_t delta;
    uint16_t flag;
    uint16_t cw_id;
    uint16_t ccw_id;
} Encoder;

extern Encoder g_encoders[ENCODER_NUM];

void encoder_input_delta(uint16_t id, int16_t delta);
void encoder_process(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* ENCODER_H_ */
