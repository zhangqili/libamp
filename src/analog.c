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

#if FILTER_TYPE == FILTER_TYPE_KALMAN
static inline void analog_kalman_filter_init(void)
{
    float sum[ADVANCED_KEY_NUM] = {0.0f};
    float sum_sq[ADVANCED_KEY_NUM] = {0.0f};

    for (int i = 0; i < 128; i++)
    {
        for (int j = 0; j < ADVANCED_KEY_NUM; j++)
        {
#if FILTER_DOMAIN == FILTER_DOMAIN_RAW
            float raw_val = advanced_key_read_raw(&g_keyboard_advanced_keys[j]);
#else
            float raw_val = advanced_key_normalize(&g_keyboard_advanced_keys[j], advanced_key_read_raw(&g_keyboard_advanced_keys[j]));
#endif
            sum[j] += raw_val;
            sum_sq[j] += raw_val * raw_val;
        }
        keyboard_delay(1);
    }
    for (int i = 0; i < ADVANCED_KEY_NUM; i++)
    {    
        float mean = sum[i] / 128.0f;
        float variance = (sum_sq[i] / 128.0f) - (mean * mean);
        float estimated_R = variance > 0.5f ? variance : 0.5f;
    
        kalman_filter_init(&g_analog_filters[i], 1.0f/(float)POLLING_RATE, 0.01f, 0.1f, estimated_R);
    }
}
#endif

void analog_init(void)
{
#if defined(FILTER_HYSTERESIS_ENABLE)
    for (uint16_t i = 0; i < ADVANCED_KEY_NUM; i++)
    {
#if FILTER_DOMAIN == FILTER_DOMAIN_RAW
        hysteresis_filter_init(&g_analog_hysteresis_filters[i], advanced_key_read_raw(&g_keyboard_advanced_keys[i]));
#else
        hysteresis_filter_init(&g_analog_hysteresis_filters[i], advanced_key_normalize(&g_keyboard_advanced_keys[i], advanced_key_read_raw(&g_keyboard_advanced_keys[i])));
#endif
    }
#endif 
#if defined(FILTER_ENABLE)
#if FILTER_TYPE == FILTER_TYPE_LOW_PASS
    for (uint16_t i = 0; i < ADVANCED_KEY_NUM; i++)
    {
#if FILTER_DOMAIN == FILTER_DOMAIN_RAW
        lowpass_filter_init(&g_analog_filters[i], advanced_key_read_raw(&g_keyboard_advanced_keys[i]));
#else
        lowpass_filter_init(&g_analog_filters[i], advanced_key_normalize(&g_keyboard_advanced_keys[i], advanced_key_read_raw(&g_keyboard_advanced_keys[i])));
#endif
    }
#elif FILTER_TYPE == FILTER_TYPE_KALMAN
    analog_kalman_filter_init();
#endif
#endif
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
    uint8_t calibration_modes[ADVANCED_KEY_NUM];
    for (uint16_t i = 0; i < ADVANCED_KEY_NUM; i++)
    {
        calibration_modes[i] = g_keyboard_advanced_keys[i].config.calibration_mode;
    }
    for (int i = 0; i < 1024; i++)
    {
        analog_check();//dummy scan to filter the noise
    }
    for (uint16_t i = 0; i < ADVANCED_KEY_NUM; i++)
    {
        AdvancedKey*advanced_key = &g_keyboard_advanced_keys[i];
        advanced_key->config.calibration_mode = calibration_modes[i];
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
