#include <gtest/gtest.h>

#include <cmath>
#include <cstring>

#include "rgb.h"
#include "test_fixture.h"

namespace {

uint8_t gamma_correct(uint8_t value, uint8_t brightness)
{
#ifdef RGB_GAMMA_ENABLE
    return static_cast<uint8_t>(std::pow(value / 255.0f, RGB_GAMMA) * 255.0f * (brightness / 255.0f) + 0.5f);
#else
    return static_cast<uint8_t>((value * brightness) >> 8);
#endif
}

} // namespace

TEST(Color, ConvertsPrimaryColorsBetweenRgbAndHsv)
{
    ColorRGB red = {255, 0, 0};
    ColorHSV hsv = {};

    rgb_to_hsv(&hsv, &red);
    EXPECT_EQ(0, hsv.h);
    EXPECT_EQ(100, hsv.s);
    EXPECT_EQ(100, hsv.v);

    ColorRGB round_trip = {};
    hsv_to_rgb(&round_trip, &hsv);
    EXPECT_EQ(255, round_trip.r);
    EXPECT_EQ(0, round_trip.g);
    EXPECT_EQ(0, round_trip.b);
}

TEST(Color, MixSaturatesAtByteMax)
{
    ColorRGB dest = {250, 10, 100};
    ColorRGB source = {10, 250, 200};

    color_mix(&dest, &source);

    EXPECT_EQ(255, dest.r);
    EXPECT_EQ(255, dest.g);
    EXPECT_EQ(255, dest.b);
}

TEST(RGB, FactoryResetAppliesDefaultBaseAndPerKeyConfigs)
{
    std::memset(&g_rgb_base_config, 0, sizeof(g_rgb_base_config));
    std::memset(g_rgb_configs, 0, sizeof(g_rgb_configs));

    rgb_factory_reset();

    EXPECT_EQ(RGB_BASE_MODE_BLANK, g_rgb_base_config.mode);
    EXPECT_EQ(255, g_rgb_base_config.brightness);
    EXPECT_EQ(32, g_rgb_base_config.density);
    EXPECT_EQ(static_cast<int16_t>(RGB_DEFAULT_SPEED), g_rgb_base_config.speed);
    EXPECT_EQ(RGB_DEFAULT_MODE, g_rgb_configs[0].mode);
    EXPECT_EQ(g_rgb_base_config.rgb.r, g_rgb_configs[0].rgb.r);
    EXPECT_EQ(g_rgb_base_config.rgb.g, g_rgb_configs[0].rgb.g);
    EXPECT_EQ(g_rgb_base_config.rgb.b, g_rgb_configs[0].rgb.b);
}

TEST(RGB, SetAppliesBrightnessAndGammaBeforeWritingLedBuffer)
{
    g_rgb_base_config.brightness = 128;

    rgb_set(3, 128, 64, 32);

    EXPECT_EQ(gamma_correct(128, 128), led_color_buffer[3].r);
    EXPECT_EQ(gamma_correct(64, 128), led_color_buffer[3].g);
    EXPECT_EQ(gamma_correct(32, 128), led_color_buffer[3].b);
}

TEST(RGB, FixedModeFlushesDeterministicLedColors)
{
    libamp_test_clear_output_buffers();
    g_rgb_base_config.mode = RGB_BASE_MODE_BLANK;
    g_rgb_base_config.brightness = 255;

    for (uint16_t i = 0; i < RGB_NUM; i++) {
        g_rgb_configs[i].mode = RGB_MODE_FIXED;
        g_rgb_configs[i].rgb = {0, 0, 0};
    }
    g_rgb_configs[0].rgb = {255, 128, 0};

    rgb_process();

    EXPECT_EQ(gamma_correct(255, 255), led_color_buffer[0].r);
    EXPECT_EQ(gamma_correct(128, 255), led_color_buffer[0].g);
    EXPECT_EQ(gamma_correct(0, 255), led_color_buffer[0].b);
    EXPECT_EQ(1U, led_flush_count);
}

TEST(RGB, HidModeOnlyFlushesExistingHostLedState)
{
    libamp_test_clear_output_buffers();
    g_rgb_hid_mode = true;
    g_rgb_base_config.mode = RGB_BASE_MODE_BLANK;
    led_color_buffer[0] = {7, 8, 9};

    rgb_process();

    EXPECT_EQ(7, led_color_buffer[0].r);
    EXPECT_EQ(8, led_color_buffer[0].g);
    EXPECT_EQ(9, led_color_buffer[0].b);
    EXPECT_EQ(1U, led_flush_count);
}
