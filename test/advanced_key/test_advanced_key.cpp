#include <gtest/gtest.h>

#include "advanced_key.h"
#include "math.h"

/* Characterize the complete raw path against the public, separate calibration
 * and normalized-update operations. The board fixture supplies a nonlinear
 * normalizer, so bypassing the weak hook cannot accidentally pass this test. */
#if !defined(FILTER_ENABLE) && !defined(FILTER_HYSTERESIS_ENABLE) && !defined(CALIBRATION_LPF_ENABLE)
TEST(AdvancedKeyTest, RawPipelineMatchesSeparateUpdates)
{
    uint32_t random = 0x13579bdf;
    for (uint8_t mode = 0; mode <= ADVANCED_KEY_ANALOG_SPEED_MODE + 1; mode++) {
        for (uint8_t calibration = 0; calibration <= ADVANCED_KEY_AUTO_CALIBRATION_UNDEFINED + 1; calibration++) {
            SCOPED_TRACE(mode);
            SCOPED_TRACE(calibration);
            AdvancedKey actual = {};
            actual.config.mode = mode;
            actual.config.calibration_mode = calibration;
            actual.config.activation_value = 30000;
            actual.config.deactivation_value = 29000;
            actual.config.trigger_distance = 2500;
            actual.config.release_distance = 1800;
            actual.config.trigger_speed = 1000;
            actual.config.release_speed = 900;
            actual.config.upper_deadzone = 100;
            actual.config.lower_deadzone = 200;
            advanced_key_set_range(&actual, 32768, 16384);
            AdvancedKey expected = actual;
            for (unsigned i = 0; i < 4096; i++) {
                random = random * 1664525u + 1013904223u;
                const AnalogRawValue raw = i < 4 ? (AnalogRawValue)(32768 + (int)i - 2) : random >> 16;
                // Configuration writes are public and must take effect immediately.
                if ((i & 127) == 0) {
                    actual.config.mode = expected.config.mode = (mode + i / 128) % 5;
                    actual.config.calibration_mode = expected.config.calibration_mode = calibration;
                }
                expected.raw = raw;
                AnalogValue value = raw;
                if (expected.config.mode != ADVANCED_KEY_DIGITAL_MODE) {
                    expected.filtered_raw = raw;
                    const int delta = (int)raw - expected.config.upper_bound;
                    bool normalize = true;
                    switch (expected.config.calibration_mode) {
                    case ADVANCED_KEY_AUTO_CALIBRATION_POSITIVE:
                        if (raw > expected.config.lower_bound)
                            advanced_key_set_range(&expected, expected.config.upper_bound, raw);
                        break;
                    case ADVANCED_KEY_AUTO_CALIBRATION_NEGATIVE:
                        if (raw < expected.config.lower_bound)
                            advanced_key_set_range(&expected, expected.config.upper_bound, raw);
                        break;
                    case ADVANCED_KEY_AUTO_CALIBRATION_UNDEFINED:
                        if (delta > DEFAULT_ESTIMATED_RANGE || -delta > DEFAULT_ESTIMATED_RANGE) {
                            expected.config.calibration_mode = delta > 0 ? ADVANCED_KEY_AUTO_CALIBRATION_POSITIVE : ADVANCED_KEY_AUTO_CALIBRATION_NEGATIVE;
                            advanced_key_set_range(&expected, expected.config.upper_bound, raw);
                        } else {
                            normalize = false;
                        }
                        break;
                    default:
                        break;
                    }
                    value = normalize ? advanced_key_normalize(&expected, raw) : ANALOG_VALUE_MIN;
                }
                const bool changed = advanced_key_update(&expected, value);
                ASSERT_EQ(changed, advanced_key_update_raw(&actual, raw)) << i;
                ASSERT_EQ(expected.key.state, actual.key.state) << i;
                ASSERT_EQ(expected.value, actual.value) << i;
                ASSERT_EQ(expected.raw, actual.raw) << i;
                ASSERT_EQ(expected.filtered_raw, actual.filtered_raw) << i;
                ASSERT_EQ(expected.difference, actual.difference) << i;
                ASSERT_EQ(expected.extremum, actual.extremum) << i;
                ASSERT_EQ(expected.config.calibration_mode, actual.config.calibration_mode) << i;
                ASSERT_EQ(expected.config.lower_bound, actual.config.lower_bound) << i;
                ASSERT_EQ(expected.q_scale_to_index, actual.q_scale_to_index) << i;
            }
        }
    }
}
#endif

#if !defined(FILTER_ENABLE) && !defined(FILTER_HYSTERESIS_ENABLE)
TEST(AdvancedKeyTest, RapidTriggerMatchesStateMachineAcrossBoundaries)
{
    uint32_t random = 0x2468ace0;
    for (unsigned initial_state = 0; initial_state < 2; initial_state++) {
        for (unsigned distance : {0u, 1u, 4096u, 65535u}) {
            AdvancedKey key = {};
            key.config.mode = ADVANCED_KEY_ANALOG_RAPID_MODE;
            key.config.trigger_distance = distance;
            key.config.release_distance = distance;
            key.config.upper_deadzone = 512;
            key.config.lower_deadzone = 1024;
            key.key.state = initial_state;
            key.extremum = 32768;
            for (unsigned i = 0; i < 65536; i++) {
                random = random * 1664525u + 1013904223u;
                const AnalogValue boundaries[] = {0, 512, 513, 64510, 64511, 65535};
                const AnalogValue value = i % 8 < 6 ? boundaries[i % 8] : random >> 16;
                const bool was_pressed = key.key.state;
                bool pressed = was_pressed;
                AnalogValue extremum = key.extremum;
                if (value - ANALOG_VALUE_MIN <= key.config.upper_deadzone) {
                    pressed = false;
                    extremum = std::min(extremum, value);
                } else if (value >= ANALOG_VALUE_MAX - key.config.lower_deadzone) {
                    pressed = true;
                    extremum = std::max(extremum, value);
                } else if (was_pressed) {
                    if ((int)extremum - value >= (int)distance) {
                        pressed = false;
                        extremum = value;
                    }
                    extremum = std::max(extremum, value);
                } else {
                    if ((int)value - extremum >= (int)distance) {
                        pressed = true;
                        extremum = value;
                    }
                    extremum = std::min(extremum, value);
                }
                const int16_t difference = value - key.value;
                ASSERT_EQ(pressed != was_pressed, advanced_key_update(&key, value)) << i;
                ASSERT_EQ(pressed, key.key.state) << i;
                ASSERT_EQ(extremum, key.extremum) << i;
                ASSERT_EQ(difference, key.difference) << i;
                ASSERT_EQ(value, key.value) << i;
            }
        }
    }
}
#endif

/* A normalizer override consumes this public Q16 scale. Compare every raw
 * span and direction with the wide signed definition, including spans whose
 * quotient uses the high bit when LUT_LENGTH is configured near 65535. */
TEST(AdvancedKeyTest, CalibrationScaleMatchesEveryRawSpan)
{
    AdvancedKey key = {};
    const int64_t numerator = (int64_t)LUT_LENGTH << 16;
    for (uint32_t span = 1; span <= UINT16_MAX; span++)
    {
        advanced_key_set_range(&key, (AnalogRawValue)span, 0);
        ASSERT_EQ((int32_t)(numerator / span), key.q_scale_to_index) << "span=" << span;
        ASSERT_EQ(span, key.config.upper_bound);
        ASSERT_EQ(0, key.config.lower_bound);

        advanced_key_set_range(&key, 0, (AnalogRawValue)span);
        ASSERT_EQ((int32_t)(-numerator / span), key.q_scale_to_index) << "span=" << span;
        ASSERT_EQ(0, key.config.upper_bound);
        ASSERT_EQ(span, key.config.lower_bound);
    }
    advanced_key_set_range(&key, 2048, 2048);
    EXPECT_EQ(0, key.q_scale_to_index);
    EXPECT_EQ(ANALOG_VALUE_MIN, advanced_key_normalize(&key, 0));
    EXPECT_EQ(ANALOG_VALUE_MIN, advanced_key_normalize(&key, UINT16_MAX));
}

TEST(AdvancedKeyTest, DigitalMode)
{
    static AdvancedKey advanced_key =
    {
        .config = 
        {
            .mode = ADVANCED_KEY_DIGITAL_MODE,
        }
    };
    advanced_key_update(&advanced_key, true);
    EXPECT_TRUE(advanced_key.key.state);
    advanced_key_update(&advanced_key, false);
    EXPECT_FALSE(advanced_key.key.state);
}

TEST(AdvancedKeyTest, NormalMode)
{
    static AdvancedKey advanced_key = 
    {
        .config = 
        {
            .mode = ADVANCED_KEY_ANALOG_NORMAL_MODE,
            .activation_value = A_ANTI_NORM(0.50),
            .deactivation_value = A_ANTI_NORM(0.49),
        },
    };
    advanced_key_update(&advanced_key, A_ANTI_NORM(0.2));
    EXPECT_FALSE(advanced_key.key.state);
    advanced_key_update(&advanced_key, A_ANTI_NORM(0.6));
    EXPECT_TRUE(advanced_key.key.state);
    advanced_key_update(&advanced_key, A_ANTI_NORM(0.4));
    EXPECT_FALSE(advanced_key.key.state);
    advanced_key_update(&advanced_key, A_ANTI_NORM(0.8));
    EXPECT_TRUE(advanced_key.key.state);
}

TEST(AdvancedKeyTest, RapidTriggerMode)
{
    static AdvancedKey advanced_key = 
    {
        .config =
        {
            .mode = ADVANCED_KEY_ANALOG_RAPID_MODE,
            .trigger_distance = A_ANTI_NORM(0.08),
            .release_distance = A_ANTI_NORM(0.08),
            .upper_deadzone = A_ANTI_NORM(0.10),
            .lower_deadzone = A_ANTI_NORM(0.20),
        },
    };
    advanced_key_update(&advanced_key, A_ANTI_NORM(0.09));
    EXPECT_FALSE(advanced_key.key.state);
    advanced_key_update(&advanced_key, A_ANTI_NORM(0.12));
    EXPECT_TRUE(advanced_key.key.state);
    EXPECT_EQ(advanced_key.extremum, A_ANTI_NORM(0.12));
    advanced_key_update(&advanced_key, A_ANTI_NORM(0.60));
    EXPECT_TRUE(advanced_key.key.state);
    EXPECT_EQ(advanced_key.extremum, A_ANTI_NORM(0.60));
    advanced_key_update(&advanced_key, A_ANTI_NORM(0.50));
    EXPECT_FALSE(advanced_key.key.state);
    EXPECT_EQ(advanced_key.extremum, A_ANTI_NORM(0.50));
    advanced_key_update(&advanced_key, A_ANTI_NORM(0.60));
    EXPECT_TRUE(advanced_key.key.state);
    EXPECT_EQ(advanced_key.extremum, A_ANTI_NORM(0.60));
    advanced_key_update(&advanced_key, A_ANTI_NORM(1.00));
    EXPECT_TRUE(advanced_key.key.state);
    EXPECT_EQ(advanced_key.extremum, A_ANTI_NORM(1.00));
    advanced_key_update(&advanced_key, A_ANTI_NORM(0.82));
    EXPECT_TRUE(advanced_key.key.state);
    EXPECT_EQ(advanced_key.extremum, A_ANTI_NORM(1.00));
    advanced_key_update(&advanced_key, A_ANTI_NORM(0.78));
    EXPECT_FALSE(advanced_key.key.state);
    EXPECT_EQ(advanced_key.extremum, A_ANTI_NORM(0.78));
}

// Expected difference between two normalized travels, computed the same way the
// library does it (difference of the converted values) so it stays well defined
// for negative deltas; a negative float cast to AnalogValue is undefined.
#define A_DIFF(a, b) ((int32_t)A_ANTI_NORM(a) - (int32_t)A_ANTI_NORM(b))

TEST(AdvancedKeyTest, SpeedMode)
{
    static AdvancedKey advanced_key = 
    {
        .config =
        {
            .mode = ADVANCED_KEY_ANALOG_SPEED_MODE,
            .trigger_speed = A_ANTI_NORM(0.04),
            .release_speed = A_ANTI_NORM(0.04),
            .upper_deadzone = A_ANTI_NORM(0.10),
            .lower_deadzone = A_ANTI_NORM(0.20),
        },
    };
    advanced_key_update(&advanced_key, A_ANTI_NORM(0.09));
    EXPECT_NEAR(advanced_key.difference, A_ANTI_NORM(0.09),A_ANTI_NORM(1e-4));
    EXPECT_FALSE(advanced_key.key.state);
    advanced_key_update(&advanced_key, A_ANTI_NORM(0.12));
    EXPECT_NEAR(advanced_key.difference, A_DIFF(0.12, 0.09), A_ANTI_NORM(1e-4));
    EXPECT_FALSE(advanced_key.key.state);
    advanced_key_update(&advanced_key, A_ANTI_NORM(0.20));
    EXPECT_NEAR(advanced_key.difference, A_DIFF(0.20, 0.12), A_ANTI_NORM(1e-4));
    EXPECT_TRUE(advanced_key.key.state);
    advanced_key_update(&advanced_key, A_ANTI_NORM(0.60));
    EXPECT_NEAR(advanced_key.difference, A_DIFF(0.60, 0.20), A_ANTI_NORM(1e-4));
    EXPECT_TRUE(advanced_key.key.state);
    advanced_key_update(&advanced_key, A_ANTI_NORM(0.80));
    EXPECT_NEAR(advanced_key.difference, A_DIFF(0.80, 0.60), A_ANTI_NORM(1e-4));
    EXPECT_TRUE(advanced_key.key.state);
    advanced_key_update(&advanced_key, A_ANTI_NORM(0.78));
    EXPECT_NEAR(advanced_key.difference, A_DIFF(0.78, 0.80), A_ANTI_NORM(1e-4));
    EXPECT_TRUE(advanced_key.key.state);
    advanced_key_update(&advanced_key, A_ANTI_NORM(0.72));
    EXPECT_NEAR(advanced_key.difference, A_DIFF(0.72, 0.78), A_ANTI_NORM(1e-4));
    EXPECT_FALSE(advanced_key.key.state);
    advanced_key_update(&advanced_key, A_ANTI_NORM(0.74));
    EXPECT_NEAR(advanced_key.difference, A_DIFF(0.74, 0.72), A_ANTI_NORM(1e-4));
    EXPECT_FALSE(advanced_key.key.state);
    advanced_key_update(&advanced_key, A_ANTI_NORM(0.76));
    EXPECT_NEAR(advanced_key.difference, A_DIFF(0.76, 0.74), A_ANTI_NORM(1e-4));
    EXPECT_FALSE(advanced_key.key.state);
    advanced_key_update(&advanced_key, A_ANTI_NORM(0.78));
    EXPECT_NEAR(advanced_key.difference, A_DIFF(0.78, 0.76), A_ANTI_NORM(1e-4));
    EXPECT_FALSE(advanced_key.key.state);
    advanced_key_update(&advanced_key, A_ANTI_NORM(0.81));
    EXPECT_NEAR(advanced_key.difference, A_DIFF(0.81, 0.78), A_ANTI_NORM(1e-4));
    EXPECT_TRUE(advanced_key.key.state);
}

TEST(AdvancedKeyTest, Value)
{
    static AdvancedKey advanced_key;
    for (int i = 0; i <= ADVANCED_KEY_ANALOG_SPEED_MODE; i++)
    {
        advanced_key.config.mode = i;
        for (int j = 0; j < 1000; j++)
        {
            float value = A_ANTI_NORM((-cos(j/100.f)*0.5+0.5)); // stays within [0, 1]
            advanced_key_update(&advanced_key, value);
            EXPECT_EQ(advanced_key.value, value);
        }
    }
}

TEST(AdvancedKeyTest, Calibration)
{
    const float default_upper_bound = 2048;

    {
        static AdvancedKey advanced_key = 
        {
            .config = 
            {
                .mode = ADVANCED_KEY_ANALOG_NORMAL_MODE,
                .calibration_mode = ADVANCED_KEY_AUTO_CALIBRATION_UNDEFINED,
                .activation_value = A_ANTI_NORM(0.50),
                .deactivation_value = A_ANTI_NORM(0.49),
                .upper_bound = (AnalogRawValue)default_upper_bound,
            },
        };
        advanced_key_update_raw(&advanced_key, default_upper_bound+DEFAULT_ESTIMATED_RANGE-100);
        advanced_key_update_raw(&advanced_key, default_upper_bound+DEFAULT_ESTIMATED_RANGE+100);
        EXPECT_EQ(advanced_key.config.calibration_mode, ADVANCED_KEY_AUTO_CALIBRATION_POSITIVE);
        EXPECT_EQ(advanced_key.config.lower_bound, default_upper_bound+DEFAULT_ESTIMATED_RANGE+100);
        advanced_key_update_raw(&advanced_key, default_upper_bound+DEFAULT_ESTIMATED_RANGE+500);
        EXPECT_EQ(advanced_key.config.calibration_mode, ADVANCED_KEY_AUTO_CALIBRATION_POSITIVE);
        EXPECT_EQ(advanced_key.config.lower_bound, default_upper_bound+DEFAULT_ESTIMATED_RANGE+500);
    }
    {
        static AdvancedKey advanced_key = 
        {
            .config = 
            {
                .mode = ADVANCED_KEY_ANALOG_NORMAL_MODE,
                .calibration_mode = ADVANCED_KEY_AUTO_CALIBRATION_UNDEFINED,
                .activation_value = A_ANTI_NORM(0.50),
                .deactivation_value = A_ANTI_NORM(0.49),
                .upper_bound = (AnalogRawValue)default_upper_bound,
            },
        };
        advanced_key_update_raw(&advanced_key, default_upper_bound-DEFAULT_ESTIMATED_RANGE+100);
        advanced_key_update_raw(&advanced_key, default_upper_bound-DEFAULT_ESTIMATED_RANGE-100);
        EXPECT_EQ(advanced_key.config.calibration_mode, ADVANCED_KEY_AUTO_CALIBRATION_NEGATIVE);
        EXPECT_EQ(advanced_key.config.lower_bound, default_upper_bound-DEFAULT_ESTIMATED_RANGE-100);
        advanced_key_update_raw(&advanced_key, default_upper_bound-DEFAULT_ESTIMATED_RANGE-500);
        EXPECT_EQ(advanced_key.config.calibration_mode, ADVANCED_KEY_AUTO_CALIBRATION_NEGATIVE);
        EXPECT_EQ(advanced_key.config.lower_bound, default_upper_bound-DEFAULT_ESTIMATED_RANGE-500);
    }
}
