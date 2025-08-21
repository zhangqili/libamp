/*
 * Copyright (c) 2024 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "stdlib.h"
#include "stdio.h"
#include "rgb.h"
#include "analog.h"
#include "advanced_key.h"

AdaptiveSchimidtFilter g_analog_filters[ANALOG_BUFFER_LENGTH];

RingBuf g_adc_ringbufs[ANALOG_BUFFER_LENGTH];

uint8_t g_analog_active_channel;

__WEAK const uint16_t g_analog_map[ANALOG_BUFFER_LENGTH];

void analog_init(void)
{
}

__WEAK void analog_channel_select(uint8_t x)
{
    UNUSED(x);
}

__WEAK void analog_scan(void)
{
    for (uint16_t i = 0; i < ADVANCED_KEY_NUM; i++)
    {
        AdvancedKey*advanced_key = &g_keyboard_advanced_keys[i];
        advanced_key->raw = advanced_key_read(advanced_key);
    }
}

void analog_check(void)
{
    for (uint16_t i = 0; i < ADVANCED_KEY_NUM; i++)
    {
        AdvancedKey*advanced_key = &g_keyboard_advanced_keys[i];
        advanced_key_update_raw(advanced_key, advanced_key_read(advanced_key));
    }
}

void analog_reset_range(void)
{
    analog_scan();
    for (uint16_t i = 0; i < ADVANCED_KEY_NUM; i++)
    {
        AdvancedKey*advanced_key = &g_keyboard_advanced_keys[i];
        advanced_key_reset_range(advanced_key, advanced_key->raw);
    }
}

void ringbuf_push(RingBuf* ringbuf, AnalogRawValue data)
{
    ringbuf->pointer++;
    if (ringbuf->pointer >= RING_BUF_LEN)
    {
        ringbuf->pointer = 0;
    }
    ringbuf->datas[ringbuf->pointer] = data;
}

AnalogRawValue ringbuf_avg(RingBuf* ringbuf)
{
    uint32_t avg = 0;
    for (int i = 0; i < RING_BUF_LEN; i++)
    {
        avg += ringbuf->datas[i];
    }
#ifdef OPTIMIZE_FOR_FLOAT_DIVISION
    return (AnalogValue)(avg*(1/((float)RING_BUF_LEN)));
#else
    return (AnalogValue)(avg/RING_BUF_LEN);
#endif
}
