/*
 * Copyright (c) 2024 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "analog.h"

Filter g_analog_filters[ADVANCED_KEY_NUM];
#if defined(FILTER_HYSTERESIS_ENABLE)
HysteresisFilter g_analog_hysteresis_filters[ADVANCED_KEY_NUM];
#endif

RingBuf g_adc_ringbufs[ANALOG_BUFFER_LENGTH];

uint8_t g_analog_active_channel;

__WEAK const uint16_t g_analog_map[ADVANCED_KEY_NUM];

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
        advanced_key->raw = advanced_key_read_raw(advanced_key);
    }
}

void analog_check(void)
{
    for (uint16_t i = 0; i < ADVANCED_KEY_NUM; i++)
    {
        AdvancedKey*advanced_key = &g_keyboard_advanced_keys[i];
        advanced_key_update_raw(advanced_key, advanced_key_read_raw(advanced_key));
    }
}

void analog_reset_range(void)
{
    for (int i = 0; i < 1024; i++)
    {
        analog_check();//dummy scan to filter the noise
    }
    for (uint16_t i = 0; i < ADVANCED_KEY_NUM; i++)
    {
        AdvancedKey*advanced_key = &g_keyboard_advanced_keys[i];
        advanced_key->config.calibration_mode = ADVANCED_KEY_AUTO_CALIBRATION_UNDEFINED;
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
#ifdef OPTIMIZE_MOVING_AVERAGE_FOR_RINGBUF
    ringbuf->dirty = true;
    ringbuf->sum -= ringbuf->datas[ringbuf->pointer];
    ringbuf->sum += data;
    ringbuf->dirty = false;
#endif
    ringbuf->datas[ringbuf->pointer] = data;
}

AnalogRawValue ringbuf_avg(RingBuf* ringbuf)
{
#ifdef OPTIMIZE_MOVING_AVERAGE_FOR_RINGBUF
    if (!ringbuf->dirty)
    { 
#ifdef OPTIMIZE_FOR_FLOAT_DIVISION
        return (AnalogValue)(ringbuf->sum*(1/((float)RING_BUF_LEN)));
#else
        return (AnalogValue)(ringbuf->sum/RING_BUF_LEN);
#endif
    }
#endif
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
