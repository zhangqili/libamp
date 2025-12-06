/*
 * Copyright (c) 2024 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file advanced_key.h
 * @brief Interface for advanced analog key processing.
 * @details This header defines structures and functions for handling analog keys,
 * typically used for magnetic switches (Hall Effect). It supports features like
 * Rapid Trigger, adjustable actuation points, velocity detection, and automatic calibration.
 * @author Zhangqi Li
 * @date 2024
 */

#ifndef ADVANCED_KEY_H_
#define ADVANCED_KEY_H_

#include "key.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Definition of analog value types.
 * @details Uses float for normalized values (0.0 to 1.0) by default, or fixed-point
 * if FIXED_POINT_EXPERIMENTAL is defined.
 */
#ifndef FIXED_POINT_EXPERIMENTAL
typedef float AnalogValue;      /**< Normalized analog value type. */
typedef float AnalogRawValue;   /**< Raw ADC value type. */
#ifndef ANALOG_VALUE_MAX
#define ANALOG_VALUE_MAX 1.0f   /**< Maximum normalized value. */
#endif
#ifndef ANALOG_VALUE_MIN
#define ANALOG_VALUE_MIN 0.0f   /**< Minimum normalized value. */
#endif
#else
/* Experimental fixed-point support */
typedef int16_t AnalogValue;
typedef int16_t AnalogRawValue;
#ifndef ANALOG_VALUE_MAX
#define ANALOG_VALUE_MAX 32767
#endif
#ifndef ANALOG_VALUE_MIN
#define ANALOG_VALUE_MIN 0
#endif
#endif

/** @brief Range of the analog value (Max - Min). */
#define ANALOG_VALUE_RANGE (ANALOG_VALUE_MAX - ANALOG_VALUE_MIN)

#ifdef OPTIMIZE_FOR_FLOAT_DIVISION
/** @brief Normalize macro using multiplication for performance. */
#define ANALOG_VALUE_NORMALIZE(x) ((x)*(1/(float)ANALOG_VALUE_RANGE))
#else
/** @brief Normalize macro using division. */
#define ANALOG_VALUE_NORMALIZE(x) ((x)/(float)ANALOG_VALUE_RANGE)
#endif

/** @brief Denormalize macro to convert back to analog range scale. */
#define ANALOG_VALUE_ANTI_NORMALIZE(x) ((AnalogValue)(((float)(x))*ANALOG_VALUE_RANGE))

/* Short aliases for normalization macros */
#define A_NORM ANALOG_VALUE_NORMALIZE
#define A_ANIT_NORM ANALOG_VALUE_ANTI_NORMALIZE

/** @brief Checks if a given key ID corresponds to an advanced key. */
#define IS_ADVANCED_KEY(key) (((Key*)(key))->id < ADVANCED_KEY_NUM)

/**
 * @brief Enumeration of key triggering modes.
 */
typedef enum
{
    ADVANCED_KEY_DIGITAL_MODE,       /**< Simple on/off threshold mode. */
    ADVANCED_KEY_ANALOG_NORMAL_MODE, /**< Standard adjustable actuation point mode. */
    ADVANCED_KEY_ANALOG_RAPID_MODE,  /**< Rapid Trigger mode (dynamic actuation/reset). */
    ADVANCED_KEY_ANALOG_SPEED_MODE   /**< Velocity-based triggering mode. */
} KeyMode;

/**
 * @brief Enumeration of auto-calibration modes.
 */
typedef enum
{
    ADVANCED_KEY_NO_CALIBRATION,             /**< Calibration disabled. */
    ADVANCED_KEY_AUTO_CALIBRATION_POSITIVE,  /**< Auto-expand range in positive direction. */
    ADVANCED_KEY_AUTO_CALIBRATION_NEGATIVE,  /**< Auto-expand range in negative direction. */
    ADVANCED_KEY_AUTO_CALIBRATION_UNDEFINED, /**< Undefined state, determines direction automatically. */
} CalibrationMode;

/**
 * @brief Configuration structure for an advanced key.
 * @details Contains all threshold and mode settings for a specific key.
 */
typedef struct __AdvancedKeyConfiguration
{
    uint8_t mode;             /**< Operation mode (@ref KeyMode). */
    uint8_t calibration_mode; /**< Calibration behavior (@ref CalibrationMode). */
    
    AnalogValue activation_value;   /**< Actuation point for Normal mode. */
    AnalogValue deactivation_value; /**< Reset point for Normal mode. */
    
    AnalogValue trigger_distance;   /**< Travel distance required to trigger (Rapid Trigger). */
    AnalogValue release_distance;   /**< Travel distance required to reset (Rapid Trigger). */
    AnalogValue trigger_speed;      /**< Speed threshold for triggering (Speed mode). */
    AnalogValue release_speed;      /**< Speed threshold for releasing (Speed mode). */
    AnalogValue upper_deadzone;     /**< Deadzone at top of travel (normalized). */
    AnalogValue lower_deadzone;     /**< Deadzone at bottom of travel (normalized). */
    AnalogRawValue upper_bound;     /**< Raw ADC value corresponding to top position. */
    AnalogRawValue lower_bound;     /**< Raw ADC value corresponding to bottom position. */
} AdvancedKeyConfiguration;

/**
 * @brief Runtime state structure for an advanced key.
 */
typedef struct __AdvancedKey
{
    Key key;                /**< Base key structure (ID, digital state). */
    AnalogValue value;      /**< Current normalized value. */
    AnalogValue raw;        /**< Current raw value (filtered). */
    AnalogValue extremum;   /**< Tracked peak/valley value for Rapid Trigger logic. */
    AnalogValue difference; /**< Delta value (velocity or travel change). */
#ifdef OPTIMIZE_FOR_FLOAT_DIVISION
    float range_reciprocal; /**< Precomputed reciprocal for fast normalization. */
#endif
    AdvancedKeyConfiguration config; /**< Key configuration. */

} AdvancedKey;

/* Function Prototypes */

/**
 * @brief Updates the key state based on a normalized analog value.
 * @param advanced_key Pointer to the advanced key instance.
 * @param value New normalized analog value.
 * @return True if the key's report state changed.
 */
bool advanced_key_update(AdvancedKey *advanced_key, AnalogValue value);

/**
 * @brief Updates the key state based on a raw analog value.
 * @details Handles calibration and normalization before updating logic.
 * @param advanced_key Pointer to the advanced key instance.
 * @param value New raw analog value.
 * @return True if the key's report state changed.
 */
bool advanced_key_update_raw(AdvancedKey *advanced_key, AnalogValue value);

/**
 * @brief Updates the digital state of the key.
 * @param advanced_key Pointer to the advanced key instance.
 * @param state New digital state.
 * @return True if the state changed.
 */
bool advanced_key_update_state(AdvancedKey *advanced_key, bool state);

/**
 * @brief Normalizes a raw value based on the key's bounds.
 * @param advanced_key Pointer to the advanced key instance.
 * @param value Raw value to normalize.
 * @return Normalized analog value.
 */
AnalogValue advanced_key_normalize(AdvancedKey *advanced_key, AnalogRawValue value);

/**
 * @brief Sets the physical range (calibration bounds) for the key.
 * @param advanced_key Pointer to the advanced key instance.
 * @param upper Upper bound (top of press).
 * @param lower Lower bound (bottom of press).
 */
void advanced_key_set_range(AdvancedKey *advanced_key, AnalogRawValue upper, AnalogRawValue lower);

/**
 * @brief Resets the calibration range based on a current reading.
 * @param advanced_key Pointer to the advanced key instance.
 * @param value Current raw value to center/start range from.
 */
void advanced_key_reset_range(AdvancedKey* advanced_key, AnalogRawValue value);

/**
 * @brief Configures deadzones for the key.
 * @param advanced_key Pointer to the advanced key instance.
 * @param upper Upper deadzone size (normalized).
 * @param lower Lower deadzone size (normalized).
 */
void advanced_key_set_deadzone(AdvancedKey *advanced_key, AnalogValue upper, AnalogValue lower);

/**
 * @brief Reads the filtered raw value from the key.
 * @param advanced_key Pointer to the advanced key instance.
 * @return Filtered raw value.
 */
AnalogRawValue advanced_key_read(AdvancedKey *advanced_key);

/**
 * @brief Reads the unfiltered raw value from the key's buffer.
 * @param advanced_key Pointer to the advanced key instance.
 * @return Unfiltered raw value.
 */
AnalogRawValue advanced_key_read_raw(AdvancedKey *advanced_key);

/**
 * @brief Calculates the effective value within the active travel range.
 * @details Adjusts the value to account for deadzones, scaling 0.0-1.0
 * within the valid travel area.
 * @param advanced_key Pointer to the advanced key instance.
 * @return Effective normalized value.
 */
AnalogValue advanced_key_get_effective_value(AdvancedKey *advanced_key);

#ifdef __cplusplus
}
#endif

#endif