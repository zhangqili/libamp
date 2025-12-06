/*
 * Copyright (c) 2024 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file advanced_key.c
 * @brief Implementation of advanced key logic.
 * @details Implements the core algorithms for analog keys, including:
 * - Digital Mode
 * - Normal Analog Mode (Adjustable Actuation)
 * - Rapid Trigger Mode (Dynamic Activation)
 * - Speed Mode (Velocity Activation)
 * - Auto-calibration and Normalization
 * @author Zhangqi Li
 * @date 2024
 */

#include "advanced_key.h"
#include "keyboard_def.h"
#include "analog.h"
#include "filter.h"

/**
 * @brief Updates key state for Digital Mode.
 * @details Functions like a simple on/off switch based on the value presence.
 */
static inline bool advanced_key_update_digital_mode(AdvancedKey* advanced_key)
{
    return (bool)advanced_key->value;
}

/**
 * @brief Updates key state for Normal Analog Mode.
 * @details Implements hysteresis with separate activation and deactivation thresholds.
 */
static inline bool advanced_key_update_analog_normal_mode(AdvancedKey* advanced_key)
{
    if((advanced_key->value - ANALOG_VALUE_MIN) > advanced_key->config.activation_value)
    {
        return true;
    }
    if((advanced_key->value - ANALOG_VALUE_MIN) < advanced_key->config.deactivation_value)
    {
        return false;
    }
    return advanced_key->key.state;
}

/**
 * @brief Updates key state for Rapid Trigger Mode.
 * @details Tracks an extremum (peak/valley) to allow the key to actuate or reset
 * based on travel distance relative to the last direction change, rather than
 * absolute position.
 */
static inline bool advanced_key_update_analog_rapid_mode(AdvancedKey* advanced_key)
{
    bool state = advanced_key->key.state;

    // Handle Top Deadzone: Force release if within upper deadzone
    if ((advanced_key->value - ANALOG_VALUE_MIN) <= advanced_key->config.upper_deadzone)
    {
        if (advanced_key->value < advanced_key->extremum)
        {
            advanced_key->extremum = advanced_key->value;
        }
        return false;
    }

    // Handle Bottom Deadzone: Force actuate if bottomed out (within lower deadzone)
    if (advanced_key->value >= ANALOG_VALUE_MAX - advanced_key->config.lower_deadzone)
    {
        if (advanced_key->value > advanced_key->extremum)
        {
            advanced_key->extremum = advanced_key->value;
        }
        return true;
    }

    // Rapid Trigger Release Logic:
    // If currently pressed, and we lifted up by 'release_distance' from the deepest point
    if (advanced_key->key.state && advanced_key->extremum - advanced_key->value >= advanced_key->config.release_distance)
    {
        state =false;
        advanced_key->extremum = advanced_key->value;
    }

    // Rapid Trigger Activation Logic:
    // If currently released, and we pressed down by 'trigger_distance' from the highest point
    if (!advanced_key->key.state && advanced_key->value - advanced_key->extremum >= advanced_key->config.trigger_distance)
    {
        state = true;
        advanced_key->extremum = advanced_key->value;
    }

    // Extremum Tracking: Update the reference point (highest or lowest reached so far)
    if ((advanced_key->key.state && advanced_key->value > advanced_key->extremum) ||
        (!advanced_key->key.state && advanced_key->value < advanced_key->extremum))
    {
        advanced_key->extremum = advanced_key->value;
    }
    return state;
}

/**
 * @brief Updates key state for Speed Mode.
 * @details Triggers based on the instantaneous velocity (difference) of the key press.
 */
static inline bool advanced_key_update_analog_speed_mode(AdvancedKey* advanced_key)
{
    bool state = advanced_key->key.state;
    // Check if press velocity exceeds trigger speed
    if (advanced_key->difference > advanced_key->config.trigger_speed)
    {
        state = true;
    }
    // Check if release velocity exceeds release speed
    if (advanced_key->difference < -advanced_key->config.release_speed)
    {
        state = false;
    }
    // Enforce deadzones
    if ((advanced_key->value - ANALOG_VALUE_MIN) <= advanced_key->config.upper_deadzone)
    {
        state = false;
    }
    if (advanced_key->value >= ANALOG_VALUE_MAX - advanced_key->config.lower_deadzone)
    {
        state = true;
    }
    return state;
}

/**
 * @brief Update function for an advanced key using normalized values.
 * @param advanced_key Pointer to the key.
 * @param value The new normalized analog value.
 * @return True if the key state changed.
 */
bool advanced_key_update(AdvancedKey* advanced_key, AnalogValue value)
{
    advanced_key->difference = value - advanced_key->value;
    advanced_key->value = value;
    bool state = advanced_key->key.state;
    switch (advanced_key->config.mode)
    {
        case ADVANCED_KEY_DIGITAL_MODE:
            state = advanced_key_update_digital_mode(advanced_key);
            break;
        case ADVANCED_KEY_ANALOG_NORMAL_MODE:
            state = advanced_key_update_analog_normal_mode(advanced_key);
            break;
        case ADVANCED_KEY_ANALOG_RAPID_MODE:
            state = advanced_key_update_analog_rapid_mode(advanced_key);
            break;
        case ADVANCED_KEY_ANALOG_SPEED_MODE:
            state = advanced_key_update_analog_speed_mode(advanced_key);
        default:
            break;
    }
    return advanced_key_update_state(advanced_key, state);
}

/**
 * @brief Update function for an advanced key using raw values.
 * @details Performs calibration filtering and boundary expansion before normalization.
 * @param advanced_key Pointer to the key.
 * @param value The new raw analog value.
 * @return True if the key state changed.
 */
bool advanced_key_update_raw(AdvancedKey* advanced_key, AnalogRawValue value)
{
#ifdef CALIBRATION_LPF_ENABLE
    static AnalogRawValue low_pass_raws[ADVANCED_KEY_NUM];
#ifndef FIXED_POINT_EXPERIMENTAL
    // Simple IIR Low Pass Filter for calibration stability
    low_pass_raws[advanced_key->key.id] = 
        value * (1.0f/16.0f)  + low_pass_raws[advanced_key->key.id] * (15.0f/16.0f);
#else
    low_pass_raws[advanced_key->key.id] = 
        ((uint32_t)value + ((uint32_t)low_pass_raws[advanced_key->key.id]<<4) - low_pass_raws[advanced_key->key.id]) >> 4; 
#endif
    AnalogRawValue lpf_value = low_pass_raws[advanced_key->key.id];
#else
    AnalogRawValue lpf_value = value;
#endif
    advanced_key->raw = value;

    // Perform calibration filtering and boundary expansion before normalization
    switch (advanced_key->config.calibration_mode)
    {
    case ADVANCED_KEY_AUTO_CALIBRATION_POSITIVE:
        if (lpf_value > advanced_key->config.lower_bound)
            advanced_key_set_range(advanced_key, advanced_key->config.upper_bound, lpf_value);
        break;
    case ADVANCED_KEY_AUTO_CALIBRATION_NEGATIVE:
        if (lpf_value < advanced_key->config.lower_bound)
            advanced_key_set_range(advanced_key, advanced_key->config.upper_bound, lpf_value);
        break;
    case ADVANCED_KEY_AUTO_CALIBRATION_UNDEFINED:
        // Attempt to detect direction based on large deviation from expected bounds
        if (lpf_value - advanced_key->config.upper_bound > DEFAULT_ESTIMATED_RANGE)
        {
            advanced_key->config.calibration_mode = ADVANCED_KEY_AUTO_CALIBRATION_POSITIVE;
            advanced_key_set_range(advanced_key, advanced_key->config.upper_bound, lpf_value);
            break;
        }
        if (advanced_key->config.upper_bound - lpf_value > DEFAULT_ESTIMATED_RANGE)
        {
            advanced_key->config.calibration_mode = ADVANCED_KEY_AUTO_CALIBRATION_NEGATIVE;
            advanced_key_set_range(advanced_key, advanced_key->config.upper_bound, lpf_value);
            break;
        }
        // If undefined and no movement detected, force to min value
        return advanced_key_update(advanced_key, ANALOG_VALUE_MIN);
    default:
        break;
    }
    if (advanced_key->config.mode == ADVANCED_KEY_DIGITAL_MODE)
        return advanced_key_update(advanced_key, value);
    else
        return advanced_key_update(advanced_key, advanced_key_normalize(advanced_key, value));
}

/**
 * @brief Updates the underlying Key state object.
 */
bool advanced_key_update_state(AdvancedKey* advanced_key, bool state)
{
    return key_update(&(advanced_key->key), state);
}

/**
 * @brief Weakly defined normalization function.
 * @details Can be overridden to provide custom curves (e.g., exponential magnetic field decay).
 */
__WEAK AnalogValue advanced_key_normalize(AdvancedKey* advanced_key, AnalogRawValue value)
{
#ifdef OPTIMIZE_FOR_FLOAT_DIVISION
    return  ANALOG_VALUE_MIN + A_ANIT_NORM(advanced_key->config.upper_bound - value) * advanced_key->range_reciprocal;
#else
    return  ANALOG_VALUE_MIN + A_ANIT_NORM(advanced_key->config.upper_bound - value) / (advanced_key->config.upper_bound - advanced_key->config.lower_bound);
#endif
}

/**
 * @brief Sets the physical bounds for calibration.
 */
void advanced_key_set_range(AdvancedKey* advanced_key, AnalogRawValue upper, AnalogRawValue lower)
{
    advanced_key->config.upper_bound = upper;
    advanced_key->config.lower_bound = lower;
#ifdef OPTIMIZE_FOR_FLOAT_DIVISION
    // Pre-calculate reciprocal to avoid division in the update function
    advanced_key->range_reciprocal = 1.0f/(advanced_key->config.upper_bound - advanced_key->config.lower_bound);
#endif
}

/**
 * @brief Resets the range based on a current "resting" value.
 */
void advanced_key_reset_range(AdvancedKey* advanced_key, AnalogRawValue value)
{
    switch (advanced_key->config.calibration_mode)
    {
    case ADVANCED_KEY_AUTO_CALIBRATION_POSITIVE:
        advanced_key_set_range(advanced_key, value, value+DEFAULT_ESTIMATED_RANGE);
        break;
    case ADVANCED_KEY_AUTO_CALIBRATION_NEGATIVE:
        advanced_key_set_range(advanced_key, value, value-DEFAULT_ESTIMATED_RANGE);
        break;
    default:
        advanced_key_set_range(advanced_key, value, value-DEFAULT_ESTIMATED_RANGE);
        break;
    }
}

/**
 * @brief Configures the physical deadzones.
 */
void advanced_key_set_deadzone(AdvancedKey* advanced_key, AnalogValue upper, AnalogValue lower)
{
    advanced_key->config.upper_deadzone = upper;
    advanced_key->config.lower_deadzone = lower;
}

/**
 * @brief Reads the filtered raw value (Weak implementation).
 * @details **Weak Symbol**: This default implementation applies an adaptive Schmidt filter.
 * Users can override this function in their own code to implement custom filtering algorithms (e.g., Kalman, EMA) without modifying the library.
 * @param advanced_key Pointer to the advanced key instance.
 * @return Filtered raw value.
 */
__WEAK AnalogRawValue advanced_key_read(AdvancedKey *advanced_key)
{
    AnalogRawValue raw = advanced_key_read_raw(advanced_key);
#ifdef FILTER_ENABLE
    raw = adaptive_schimidt_filter(&g_analog_filters[advanced_key->key.id], raw);
#endif
    return raw;
}

/**
 * @brief Reads the raw ADC value directly (Weak implementation).
 * @details **Weak Symbol**: This default implementation reads from the internal ring buffer.
 * Users can override this function to support custom hardware interfaces (e.g., external SPI ADCs) or different data sources.
 * @param advanced_key Pointer to the advanced key instance.
 * @return Unfiltered raw value.
 */
__WEAK AnalogRawValue advanced_key_read_raw(AdvancedKey *advanced_key)
{
    return ringbuf_avg(&g_adc_ringbufs[g_analog_map[advanced_key->key.id]]);
}

/**
 * @brief Returns the effective value (0.0-1.0) accounting for deadzones.
 * @details Useful for RGB effects or gamepad axes to ensure full range output.
 */
AnalogValue advanced_key_get_effective_value(AdvancedKey *advanced_key)
{
    AnalogValue value = (advanced_key->value - advanced_key->config.upper_deadzone) / (float)(ANALOG_VALUE_RANGE - advanced_key->config.upper_deadzone - advanced_key->config.lower_deadzone);
    if (value > ANALOG_VALUE_MAX)
    {
        value = ANALOG_VALUE_MAX;
    }
    if (value < ANALOG_VALUE_MIN)
    {
        value = ANALOG_VALUE_MIN;
    }
    return value;
}
