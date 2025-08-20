#include <gtest/gtest.h>

#include "advanced_key.h"
#include "math.h"

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
            .activation_value = A_ANIT_NORM(0.50),
            .deactivation_value = A_ANIT_NORM(0.49),
        },
    };
    advanced_key_update(&advanced_key, A_ANIT_NORM(0.2));
    EXPECT_FALSE(advanced_key.key.state);
    advanced_key_update(&advanced_key, A_ANIT_NORM(0.6));
    EXPECT_TRUE(advanced_key.key.state);
    advanced_key_update(&advanced_key, A_ANIT_NORM(0.4));
    EXPECT_FALSE(advanced_key.key.state);
    advanced_key_update(&advanced_key, A_ANIT_NORM(0.8));
    EXPECT_TRUE(advanced_key.key.state);
}

TEST(AdvancedKeyTest, RapidTriggerMode)
{
    static AdvancedKey advanced_key = 
    {
        .config =
        {
            .mode = ADVANCED_KEY_ANALOG_RAPID_MODE,
            .trigger_distance = A_ANIT_NORM(0.08),
            .release_distance = A_ANIT_NORM(0.08),
            .upper_deadzone = A_ANIT_NORM(0.10),
            .lower_deadzone = A_ANIT_NORM(0.20),
        },
    };
    advanced_key_update(&advanced_key, A_ANIT_NORM(0.09));
    EXPECT_FALSE(advanced_key.key.state);
    advanced_key_update(&advanced_key, A_ANIT_NORM(0.12));
    EXPECT_TRUE(advanced_key.key.state);
    EXPECT_FLOAT_EQ(advanced_key.extremum, A_ANIT_NORM(0.12));
    advanced_key_update(&advanced_key, A_ANIT_NORM(0.60));
    EXPECT_TRUE(advanced_key.key.state);
    EXPECT_FLOAT_EQ(advanced_key.extremum, A_ANIT_NORM(0.60));
    advanced_key_update(&advanced_key, A_ANIT_NORM(0.50));
    EXPECT_FALSE(advanced_key.key.state);
    EXPECT_FLOAT_EQ(advanced_key.extremum, A_ANIT_NORM(0.50));
    advanced_key_update(&advanced_key, A_ANIT_NORM(0.60));
    EXPECT_TRUE(advanced_key.key.state);
    EXPECT_FLOAT_EQ(advanced_key.extremum, A_ANIT_NORM(0.60));
    advanced_key_update(&advanced_key, A_ANIT_NORM(1.00));
    EXPECT_TRUE(advanced_key.key.state);
    EXPECT_FLOAT_EQ(advanced_key.extremum, A_ANIT_NORM(1.00));
    advanced_key_update(&advanced_key, A_ANIT_NORM(0.82));
    EXPECT_TRUE(advanced_key.key.state);
    EXPECT_FLOAT_EQ(advanced_key.extremum, A_ANIT_NORM(1.00));
    advanced_key_update(&advanced_key, A_ANIT_NORM(0.78));
    EXPECT_FALSE(advanced_key.key.state);
    EXPECT_FLOAT_EQ(advanced_key.extremum, A_ANIT_NORM(0.78));
}

TEST(AdvancedKeyTest, SpeedMode)
{
    static AdvancedKey advanced_key = 
    {
        .config =
        {
            .mode = ADVANCED_KEY_ANALOG_SPEED_MODE,
            .trigger_speed = A_ANIT_NORM(0.04),
            .release_speed = A_ANIT_NORM(0.04),
            .upper_deadzone = A_ANIT_NORM(0.10),
            .lower_deadzone = A_ANIT_NORM(0.20),
        },
    };
    advanced_key_update(&advanced_key, A_ANIT_NORM(0.09));
    EXPECT_NEAR(advanced_key.difference, A_ANIT_NORM(0.09),A_ANIT_NORM(1e-4));
    EXPECT_FALSE(advanced_key.key.state);
    advanced_key_update(&advanced_key, A_ANIT_NORM(0.12));
    EXPECT_NEAR(advanced_key.difference, A_ANIT_NORM(0.12-0.09), A_ANIT_NORM(1e-4));
    EXPECT_FALSE(advanced_key.key.state);
    advanced_key_update(&advanced_key, A_ANIT_NORM(0.20));
    EXPECT_NEAR(advanced_key.difference, A_ANIT_NORM(0.20-0.12), A_ANIT_NORM(1e-4));
    EXPECT_TRUE(advanced_key.key.state);
    advanced_key_update(&advanced_key, A_ANIT_NORM(0.80));
    EXPECT_NEAR(advanced_key.difference, A_ANIT_NORM(0.80-0.20), A_ANIT_NORM(1e-4));
    EXPECT_TRUE(advanced_key.key.state);
    advanced_key_update(&advanced_key, A_ANIT_NORM(0.78));
    EXPECT_NEAR(advanced_key.difference, A_ANIT_NORM(0.78-0.80), A_ANIT_NORM(1e-4));
    EXPECT_TRUE(advanced_key.key.state);
    advanced_key_update(&advanced_key, A_ANIT_NORM(0.72));
    EXPECT_NEAR(advanced_key.difference, A_ANIT_NORM(0.72-0.78), A_ANIT_NORM(1e-4));
    EXPECT_FALSE(advanced_key.key.state);
    advanced_key_update(&advanced_key, A_ANIT_NORM(0.74));
    EXPECT_NEAR(advanced_key.difference, A_ANIT_NORM(0.74-0.72), A_ANIT_NORM(1e-4));
    EXPECT_FALSE(advanced_key.key.state);
    advanced_key_update(&advanced_key, A_ANIT_NORM(0.76));
    EXPECT_NEAR(advanced_key.difference, A_ANIT_NORM(0.76-0.74), A_ANIT_NORM(1e-4));
    EXPECT_FALSE(advanced_key.key.state);
    advanced_key_update(&advanced_key, A_ANIT_NORM(0.78));
    EXPECT_NEAR(advanced_key.difference, A_ANIT_NORM(0.78-0.76), A_ANIT_NORM(1e-4));
    EXPECT_FALSE(advanced_key.key.state);
    advanced_key_update(&advanced_key, A_ANIT_NORM(0.81));
    EXPECT_NEAR(advanced_key.difference, A_ANIT_NORM(0.81-0.78), A_ANIT_NORM(1e-4));
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
            float value = A_ANIT_NORM((-cos(j/100.f)*0.5+1));
            advanced_key_update(&advanced_key, value);
            EXPECT_FLOAT_EQ(advanced_key.value, value);
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
                .activation_value = A_ANIT_NORM(0.50),
                .deactivation_value = A_ANIT_NORM(0.49),
                .upper_bound = (AnalogRawValue)default_upper_bound,
            },
        };
        advanced_key_update_raw(&advanced_key, default_upper_bound+DEFAULT_ESTIMATED_RANGE-100);
        advanced_key_update_raw(&advanced_key, default_upper_bound+DEFAULT_ESTIMATED_RANGE+100);
        EXPECT_EQ(advanced_key.config.calibration_mode, ADVANCED_KEY_AUTO_CALIBRATION_POSITIVE);
        EXPECT_FLOAT_EQ(advanced_key.config.lower_bound, default_upper_bound+DEFAULT_ESTIMATED_RANGE+100);
        advanced_key_update_raw(&advanced_key, default_upper_bound+DEFAULT_ESTIMATED_RANGE+500);
        EXPECT_EQ(advanced_key.config.calibration_mode, ADVANCED_KEY_AUTO_CALIBRATION_POSITIVE);
        EXPECT_FLOAT_EQ(advanced_key.config.lower_bound, default_upper_bound+DEFAULT_ESTIMATED_RANGE+500);
    }
    {
        static AdvancedKey advanced_key = 
        {
            .config = 
            {
                .mode = ADVANCED_KEY_ANALOG_NORMAL_MODE,
                .calibration_mode = ADVANCED_KEY_AUTO_CALIBRATION_UNDEFINED,
                .activation_value = A_ANIT_NORM(0.50),
                .deactivation_value = A_ANIT_NORM(0.49),
                .upper_bound = (AnalogRawValue)default_upper_bound,
            },
        };
        advanced_key_update_raw(&advanced_key, default_upper_bound-DEFAULT_ESTIMATED_RANGE+100);
        advanced_key_update_raw(&advanced_key, default_upper_bound-DEFAULT_ESTIMATED_RANGE-100);
        EXPECT_EQ(advanced_key.config.calibration_mode, ADVANCED_KEY_AUTO_CALIBRATION_NEGATIVE);
        EXPECT_FLOAT_EQ(advanced_key.config.lower_bound, default_upper_bound-DEFAULT_ESTIMATED_RANGE-100);
        advanced_key_update_raw(&advanced_key, default_upper_bound-DEFAULT_ESTIMATED_RANGE-500);
        EXPECT_EQ(advanced_key.config.calibration_mode, ADVANCED_KEY_AUTO_CALIBRATION_NEGATIVE);
        EXPECT_FLOAT_EQ(advanced_key.config.lower_bound, default_upper_bound-DEFAULT_ESTIMATED_RANGE-500);
    }
}