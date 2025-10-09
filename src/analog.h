/*
 * Copyright (c) 2024 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef ANALOG_H_
#define ANALOG_H_

#include "filter.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BCD_TO_GRAY(x) ((x)^((x)>>1))

#ifndef RING_BUF_LEN
#define RING_BUF_LEN 8
#endif

#ifndef ANALOG_BUFFER_LENGTH
#define ANALOG_BUFFER_LENGTH ADVANCED_KEY_NUM
#endif

#ifndef ANALOG_CHANNEL_MAX
#define ANALOG_CHANNEL_MAX 16
#endif

#define ANALOG_NO_MAP    0xFFFF

typedef struct
{
    uint16_t datas[RING_BUF_LEN];
    uint16_t pointer;
}RingBuf;

extern AdaptiveSchimidtFilter g_analog_filters[ADVANCED_KEY_NUM];

extern RingBuf g_adc_ringbufs[ANALOG_BUFFER_LENGTH];

extern uint8_t g_analog_active_channel;

extern const uint16_t g_analog_map[ADVANCED_KEY_NUM];

void analog_init(void);
void analog_channel_select(uint8_t x);
void analog_scan(void);
void analog_check(void);
void analog_reset_range(void);

void ringbuf_push(RingBuf *ringbuf, AnalogRawValue data);
AnalogRawValue ringbuf_avg(RingBuf *ringbuf);

#ifdef __cplusplus
}
#endif

#endif /* ANALOG_H_ */
