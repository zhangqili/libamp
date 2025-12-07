/*
 * Copyright (c) 2024 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "filter.h"
#include "analog.h"

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

void filter_reset(void)
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
