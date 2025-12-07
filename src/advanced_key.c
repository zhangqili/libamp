/*
 * Copyright (c) 2024 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "advanced_key.h"
#include "keyboard_def.h"
#include "analog.h"

static inline bool advanced_key_update_digital_mode(AdvancedKey* advanced_key)
{
    return (bool)advanced_key->value;
}

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

static inline bool advanced_key_update_analog_rapid_mode(AdvancedKey* advanced_key)
{
    bool state = advanced_key->key.state;
    if ((advanced_key->value - ANALOG_VALUE_MIN) <= advanced_key->config.upper_deadzone)
    {
        if (advanced_key->value < advanced_key->extremum)
        {
            advanced_key->extremum = advanced_key->value;
        }
        return false;
    }
    if (advanced_key->value >= ANALOG_VALUE_MAX - advanced_key->config.lower_deadzone)
    {
        if (advanced_key->value > advanced_key->extremum)
        {
            advanced_key->extremum = advanced_key->value;
        }
        return true;
    }
    if (advanced_key->key.state && advanced_key->extremum - advanced_key->value >= advanced_key->config.release_distance)
    {
        state =false;
        advanced_key->extremum = advanced_key->value;
    }
    if (!advanced_key->key.state && advanced_key->value - advanced_key->extremum >= advanced_key->config.trigger_distance)
    {
        state = true;
        advanced_key->extremum = advanced_key->value;
    }
    if ((advanced_key->key.state && advanced_key->value > advanced_key->extremum) ||
        (!advanced_key->key.state && advanced_key->value < advanced_key->extremum))
    {
        advanced_key->extremum = advanced_key->value;
    }
    return state;
}

static inline bool advanced_key_update_analog_speed_mode(AdvancedKey* advanced_key)
{
    bool state = advanced_key->key.state;
    if (advanced_key->difference > advanced_key->config.trigger_speed)
    {
        state = true;
    }
    if (advanced_key->difference < -advanced_key->config.release_speed)
    {
        state = false;
    }
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

bool advanced_key_update(AdvancedKey* advanced_key, AnalogValue value)
{
    if (advanced_key->config.mode == ADVANCED_KEY_DIGITAL_MODE)
    {
        advanced_key->difference = value - advanced_key->value;
        advanced_key->value = value;
        return advanced_key_update_state(advanced_key, advanced_key_update_digital_mode(advanced_key));
    }
#if defined(FILTER_ENABLE) && FILTER_DOMAIN == FILTER_DOMAIN_NORMALIZED
    value = analog_filter(&g_analog_filters[advanced_key->key.id], value);
#endif
#if defined(FILTER_HYSTERESIS_ENABLE) && FILTER_DOMAIN == FILTER_DOMAIN_NORMALIZED
    value = hysteresis_filter(&g_analog_hysteresis_filters[advanced_key->key.id], value);
#endif
    advanced_key->difference = value - advanced_key->value;
    advanced_key->value = value;
    bool state = advanced_key->key.state;
    switch (advanced_key->config.mode)
    {
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

bool advanced_key_update_raw(AdvancedKey* advanced_key, AnalogRawValue raw)
{
    if (advanced_key->config.mode == ADVANCED_KEY_DIGITAL_MODE)
    {
        return advanced_key_update(advanced_key, raw);
    }
#if defined(FILTER_ENABLE) && FILTER_DOMAIN == FILTER_DOMAIN_RAW
    raw = analog_filter(&g_analog_filters[advanced_key->key.id], raw);
#endif
#if defined(FILTER_HYSTERESIS_ENABLE) && FILTER_DOMAIN == FILTER_DOMAIN_RAW
    raw = hysteresis_filter(&g_analog_hysteresis_filters[advanced_key->key.id], raw);
#endif
#ifdef CALIBRATION_LPF_ENABLE
    static AnalogRawValue low_pass_raws[ADVANCED_KEY_NUM];
#ifndef FIXED_POINT_EXPERIMENTAL
    low_pass_raws[advanced_key->key.id] = 
        raw * (1.0f/16.0f)  + low_pass_raws[advanced_key->key.id] * (15.0f/16.0f);
#else
    low_pass_raws[advanced_key->key.id] = 
        ((uint32_t)raw + ((uint32_t)low_pass_raws[advanced_key->key.id]<<4) - low_pass_raws[advanced_key->key.id]) >> 4; 
#endif
    AnalogRawValue lpf_value = low_pass_raws[advanced_key->key.id];
#else
    AnalogRawValue lpf_value = raw;
#endif
    advanced_key->raw = raw;
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
        return advanced_key_update(advanced_key, ANALOG_VALUE_MIN);
    default:
        break;
    }
    
    return advanced_key_update(advanced_key, advanced_key_normalize(advanced_key, raw));
}

bool advanced_key_update_state(AdvancedKey* advanced_key, bool state)
{
    return key_update(&(advanced_key->key), state);
}

__WEAK AnalogValue advanced_key_normalize(AdvancedKey* advanced_key, AnalogRawValue value)
{
#ifdef OPTIMIZE_FOR_FLOAT_DIVISION
    return  ANALOG_VALUE_MIN + A_ANTI_NORM(advanced_key->config.upper_bound - value) * advanced_key->range_reciprocal;
#else
    return  ANALOG_VALUE_MIN + A_ANTI_NORM(advanced_key->config.upper_bound - value) / (advanced_key->config.upper_bound - advanced_key->config.lower_bound);
#endif
}

void advanced_key_set_range(AdvancedKey* advanced_key, AnalogRawValue upper, AnalogRawValue lower)
{
    advanced_key->config.upper_bound = upper;
    advanced_key->config.lower_bound = lower;
#ifdef OPTIMIZE_FOR_FLOAT_DIVISION
    advanced_key->range_reciprocal = 1.0f/(advanced_key->config.upper_bound - advanced_key->config.lower_bound);
#endif
}

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

void advanced_key_set_deadzone(AdvancedKey* advanced_key, AnalogValue upper, AnalogValue lower)
{
    advanced_key->config.upper_deadzone = upper;
    advanced_key->config.lower_deadzone = lower;
}

__WEAK AnalogRawValue advanced_key_read_raw(AdvancedKey *advanced_key)
{
    return ringbuf_avg(&g_adc_ringbufs[g_analog_map[advanced_key->key.id]]);
}

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
