/*
 * Copyright (c) 2024 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file analog.c
 * @brief Implementation of analog signal processing.
 * @details Implements ring buffer logic for smoothing and default (weak) implementations
 * for analog scanning and initialization.
 * @author Zhangqi Li
 * @date 2024
 */

#include "analog.h"

/* Global Instantiations */
AdaptiveSchimidtFilter g_analog_filters[ADVANCED_KEY_NUM];
RingBuf g_adc_ringbufs[ANALOG_BUFFER_LENGTH];
uint8_t g_analog_active_channel;

/**
 * @brief Default analog map.
 * @note Defined as weak to allow user overrides in configuration files.
 */
__WEAK const uint16_t g_analog_map[ADVANCED_KEY_NUM];

/**
 * @brief Initializes the analog subsystem (Weak implementation).
 * @note Override this function to perform hardware-specific ADC initialization.
 */
__WEAK void analog_init(void)
{
}

/**
 * @brief Selects the analog channel (Weak implementation).
 * @param x The channel index.
 * @note Override this to control hardware multiplexers (MUX).
 */
__WEAK void analog_channel_select(uint8_t x)
{
    UNUSED(x);
}

/**
 * @brief Scans analog keys (Weak implementation).
 * @details Iterates through advanced keys and updates their `raw` field by reading
 * from the implementation specific `advanced_key_read`.
 */
__WEAK void analog_scan(void)
{
    for (uint16_t i = 0; i < ADVANCED_KEY_NUM; i++)
    {
        AdvancedKey*advanced_key = &g_keyboard_advanced_keys[i];
        advanced_key->raw = advanced_key_read(advanced_key);
    }
}

/**
 * @brief Checks and updates key states.
 * @details Reads the current analog value and updates the key's logical state
 * (processing triggers, rapid trigger logic, etc.) via `advanced_key_update_raw`.
 */
void analog_check(void)
{
    for (uint16_t i = 0; i < ADVANCED_KEY_NUM; i++)
    {
        AdvancedKey*advanced_key = &g_keyboard_advanced_keys[i];
        advanced_key_update_raw(advanced_key, advanced_key_read(advanced_key));
    }
}

/**
 * @brief Resets the dynamic range of keys.
 * @details Performs a scan to get current values, then resets the internal
 * calibration bounds (min/max) for each key based on that value.
 */
void analog_reset_range(void)
{
    analog_scan();
    for (uint16_t i = 0; i < ADVANCED_KEY_NUM; i++)
    {
        AdvancedKey*advanced_key = &g_keyboard_advanced_keys[i];
        advanced_key_reset_range(advanced_key, advanced_key->raw);
    }
}

/**
 * @brief Pushes a new value into the ring buffer.
 * @details Updates the circular buffer pointer. If optimization is enabled,
 * it also updates the running sum to allow O(1) averaging.
 * @param ringbuf Pointer to the target ring buffer.
 * @param data The new analog value.
 */
void ringbuf_push(RingBuf* ringbuf, AnalogRawValue data)
{
    ringbuf->pointer++;
    if (ringbuf->pointer >= RING_BUF_LEN)
    {
        ringbuf->pointer = 0;
    }
#ifdef OPTIMIZE_MOVING_AVERAGE_FOR_RINGBUF
    ringbuf->dirty = true;
    ringbuf->sum -= ringbuf->datas[ringbuf->pointer]; // Subtract old value
    ringbuf->sum += data;                             // Add new value
    ringbuf->dirty = false;
#endif
    ringbuf->datas[ringbuf->pointer] = data;
}

/**
 * @brief Computes the average of the ring buffer.
 * @details Used to smooth out noise from the ADC readings.
 * @param ringbuf Pointer to the target ring buffer.
 * @return The average value.
 */
AnalogRawValue ringbuf_avg(RingBuf* ringbuf)
{
#ifdef OPTIMIZE_MOVING_AVERAGE_FOR_RINGBUF
    // If optimized, use the running sum for O(1) calculation
    if (!ringbuf->dirty)
    { 
#ifdef OPTIMIZE_FOR_FLOAT_DIVISION
        // Multiplication is generally faster than division on embedded systems
        return (AnalogValue)(ringbuf->sum*(1/((float)RING_BUF_LEN)));
#else
        return (AnalogValue)(ringbuf->sum/RING_BUF_LEN);
#endif
    }
#endif
    // Fallback or non-optimized: Iterate and sum O(N)
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