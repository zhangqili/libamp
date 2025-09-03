/*
 * Copyright (c) 2024 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef ADVANCED_KEY_H_
#define ADVANCED_KEY_H_

#include "key.h"
#include "keyboard_conf.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef FIXED_POINT_EXPERIMENTAL
typedef float AnalogValue;
typedef float AnalogRawValue;
#ifndef ANALOG_VALUE_MAX
#define ANALOG_VALUE_MAX 1.0f
#endif
#ifndef ANALOG_VALUE_MIN
#define ANALOG_VALUE_MIN 0.0f
#endif
#else
typedef int16_t AnalogValue;
typedef int16_t AnalogRawValue;
#ifndef ANALOG_VALUE_MAX
#define ANALOG_VALUE_MAX 32767
#endif
#ifndef ANALOG_VALUE_MIN
#define ANALOG_VALUE_MIN 0
#endif
#endif
#define ANALOG_VALUE_RANGE (ANALOG_VALUE_MAX - ANALOG_VALUE_MIN)

#ifdef OPTIMIZE_FOR_FLOAT_DIVISION
#define ANALOG_VALUE_NORMALIZE(x) ((x)*(1/(float)ANALOG_VALUE_RANGE))
#else
#define ANALOG_VALUE_NORMALIZE(x) ((x)/(float)ANALOG_VALUE_RANGE)
#endif
#define ANALOG_VALUE_ANTI_NORMALIZE(x) ((AnalogValue)(((float)(x))*ANALOG_VALUE_RANGE))

#define A_NORM ANALOG_VALUE_NORMALIZE
#define A_ANIT_NORM ANALOG_VALUE_ANTI_NORMALIZE

#define IS_ADVANCED_KEY(key) (((Key*)(key))->id < ADVANCED_KEY_NUM)

typedef enum
{
    ADVANCED_KEY_DIGITAL_MODE,
    ADVANCED_KEY_ANALOG_NORMAL_MODE,
    ADVANCED_KEY_ANALOG_RAPID_MODE,
    ADVANCED_KEY_ANALOG_SPEED_MODE
} KeyMode;
typedef enum
{
    ADVANCED_KEY_NO_CALIBRATION,
    ADVANCED_KEY_AUTO_CALIBRATION_POSITIVE,
    ADVANCED_KEY_AUTO_CALIBRATION_NEGATIVE,
    ADVANCED_KEY_AUTO_CALIBRATION_UNDEFINED,
} CalibrationMode;

typedef struct __AdvancedKeyConfiguration
{
    uint8_t mode;
    uint8_t calibration_mode;
    
    AnalogValue activation_value;   //relative
    AnalogValue deactivation_value; //relative
    
    AnalogValue trigger_distance;   //relative
    AnalogValue release_distance;   //relative
    AnalogValue trigger_speed;      //relative
    AnalogValue release_speed;      //relative
    AnalogValue upper_deadzone;     //relative
    AnalogValue lower_deadzone;     //relative
    AnalogRawValue upper_bound;     //absolute
    AnalogRawValue lower_bound;     //absolute
} AdvancedKeyConfiguration;

typedef struct __AdvancedKey
{
    Key key;
    AnalogValue value;
    AnalogValue raw;
    AnalogValue extremum;
    AnalogValue difference;
#ifdef OPTIMIZE_FOR_FLOAT_DIVISION
    float range_reciprocal;
#endif
    AdvancedKeyConfiguration config;

} AdvancedKey;

void advanced_key_init(AdvancedKey *advanced_key);
void advanced_key_update(AdvancedKey *advanced_key, AnalogValue value);
void advanced_key_update_raw(AdvancedKey *advanced_key, AnalogValue value);
void advanced_key_update_state(AdvancedKey *advanced_key, bool state);
AnalogValue advanced_key_normalize(AdvancedKey *advanced_key, AnalogRawValue value);
void advanced_key_set_range(AdvancedKey *advanced_key, AnalogRawValue upper, AnalogRawValue lower);
void advanced_key_reset_range(AdvancedKey* advanced_key, AnalogRawValue value);
void advanced_key_set_deadzone(AdvancedKey *advanced_key, AnalogValue upper, AnalogValue lower);
AnalogRawValue advanced_key_read(AdvancedKey *advanced_key);
AnalogRawValue advanced_key_read_raw(AdvancedKey *advanced_key);
AnalogValue advanced_key_get_effective_value(AdvancedKey *advanced_key);

#ifdef __cplusplus
}
#endif

#endif