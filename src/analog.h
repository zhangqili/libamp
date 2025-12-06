/*
 * Copyright (c) 2024 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file analog.h
 * @brief Analog signal processing and management.
 * @details This header defines structures and functions for handling analog inputs,
 * specifically using ring buffers for moving average filtering to smooth out ADC noise.
 * It manages the mapping, scanning, and processing of analog keys.
 * @author Zhangqi Li
 * @date 2024
 */

#ifndef ANALOG_H_
#define ANALOG_H_

#include "filter.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Converts Binary Coded Decimal (BCD) to Gray code.
 * @details Useful for ensuring that only one bit changes at a time during transitions,
 * often used in hardware multiplexing logic.
 * @param x The BCD value to convert.
 */
#define BCD_TO_GRAY(x) ((x)^((x)>>1))

/** @brief The length of the ring buffer used for moving average filtering. */
#ifndef RING_BUF_LEN
#define RING_BUF_LEN 8
#endif

/** @brief The total number of analog buffers, usually corresponding to the number of advanced keys. */
#ifndef ANALOG_BUFFER_LENGTH
#define ANALOG_BUFFER_LENGTH ADVANCED_KEY_NUM
#endif

/** @brief Maximum number of analog channels supported. */
#ifndef ANALOG_CHANNEL_MAX
#define ANALOG_CHANNEL_MAX 16
#endif

/** @brief Sentinel value indicating no analog map exists for a key. */
#define ANALOG_NO_MAP    0xFFFF

/**
 * @brief Circular buffer structure for storing historical ADC values.
 * @details Used to implement a moving average filter.
 */
typedef struct __RingBuf
{
    uint16_t datas[RING_BUF_LEN]; /**< Array storing the most recent ADC readings. */
    uint16_t pointer;             /**< Index pointing to the current head of the buffer. */
#ifdef OPTIMIZE_MOVING_AVERAGE_FOR_RINGBUF
    uint32_t sum;                 /**< Running sum of the buffer elements for O(1) average calculation. */
    bool dirty;                   /**< Flag indicating if the sum needs to be recalculated. */
#endif
} RingBuf;

/** @brief Global array of adaptive Schmidt trigger filters for each key. */
extern AdaptiveSchimidtFilter g_analog_filters[ADVANCED_KEY_NUM];

/** @brief Global array of ring buffers for each key. */
extern RingBuf g_adc_ringbufs[ANALOG_BUFFER_LENGTH];

/** @brief The currently active analog multiplexer channel. */
extern uint8_t g_analog_active_channel;

/** @brief Mapping table converting Key IDs to specific Analog indices/channels. */
extern const uint16_t g_analog_map[ADVANCED_KEY_NUM];

/**
 * @brief Initializes the analog subsystem.
 */
void analog_init(void);

/**
 * @brief Selects the active analog channel for multiplexing.
 * @param x The channel index to select.
 */
void analog_channel_select(uint8_t x);

/**
 * @brief Scans all analog keys and updates their raw read values.
 */
void analog_scan(void);

/**
 * @brief Scans and processes analog keys.
 * @details Updates the logical state of keys based on new analog readings.
 */
void analog_check(void);

/**
 * @brief Resets the calibration range for all analog keys based on current readings.
 */
void analog_reset_range(void);

/**
 * @brief Pushes a new data point into the ring buffer.
 * @param ringbuf Pointer to the ring buffer.
 * @param data The new analog raw value to push.
 */
void ringbuf_push(RingBuf *ringbuf, AnalogRawValue data);

/**
 * @brief Calculates the average value of the data in the ring buffer.
 * @param ringbuf Pointer to the ring buffer.
 * @return The calculated average value.
 */
AnalogRawValue ringbuf_avg(RingBuf *ringbuf);

#ifdef __cplusplus
}
#endif

#endif /* ANALOG_H_ */
