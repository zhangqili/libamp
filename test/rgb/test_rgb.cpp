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

// The scalar conversion before the interpolation optimization is an exact
// byte oracle, including percentage clamping and invalid hues.
ColorRGB scalar_hsv_to_rgb(const ColorHSV &hsv)
{
    const uint32_t v = hsv.v > 100 ? 100 : hsv.v;
    const uint32_t s = hsv.s > 100 ? 100 : hsv.s;
    const uint8_t value = static_cast<uint8_t>(v * 255u / 100u);
    if (s == 0) {
        return {value, value, value};
    }
    const uint32_t sector = hsv.h / 60u;
    const uint32_t fraction = hsv.h - sector * 60u;
    const uint8_t x = static_cast<uint8_t>(v * (100u - s) * 255u / 10000u);
    const uint8_t y = static_cast<uint8_t>(v * (6000u - s * fraction) * 255u / 600000u);
    const uint8_t z = static_cast<uint8_t>(v * (6000u - s * (60u - fraction)) * 255u / 600000u);
    const ColorRGB sectors[] = {
        {value, z, x}, {y, value, x}, {x, value, z},
        {x, y, value}, {z, x, value}, {value, x, y},
    };
    return sector < 6 ? sectors[sector] : ColorRGB{0, 0, 0};
}

} // namespace

TEST(Color, HsvConversionMatchesScalarForEveryValidInput)
{
    for (uint16_t h = 0; h < 360; h++) {
        for (uint16_t s = 0; s <= 100; s++) {
            for (uint16_t v = 0; v <= 100; v++) {
                const ColorHSV hsv = {h, static_cast<uint8_t>(s), static_cast<uint8_t>(v)};
                const ColorRGB expected = scalar_hsv_to_rgb(hsv);
                ColorRGB actual = {};
                hsv_to_rgb(&actual, &hsv);
                if (actual.r != expected.r || actual.g != expected.g || actual.b != expected.b) {
                    FAIL() << "HSV " << h << ',' << s << ',' << v << " changed its RGB bytes";
                }
            }
        }
    }
}

TEST(Color, HsvConversionPreservesClampingAndEveryRawHue)
{
    const uint8_t percentages[] = {0, 1, 50, 100, 101, 255};
    for (uint32_t h = 0; h <= UINT16_MAX; h++) {
        for (uint8_t s : percentages) {
            for (uint8_t v : percentages) {
                const ColorHSV hsv = {static_cast<uint16_t>(h), s, v};
                const ColorRGB expected = scalar_hsv_to_rgb(hsv);
                // Invalid chromatic hues must overwrite the destination with
                // black; grayscale ignores hue, including every invalid hue.
                ColorRGB actual = {0xA5, 0x5A, 0xC3};
                hsv_to_rgb(&actual, &hsv);
                if (actual.r != expected.r || actual.g != expected.g || actual.b != expected.b) {
                    FAIL() << "HSV " << h << ',' << unsigned(s) << ',' << unsigned(v)
                           << " changed its clamped or invalid-hue RGB bytes";
                }
            }
        }
    }
}

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
    rgb_set_brightness(128);

    rgb_set(3, 128, 64, 32);

    EXPECT_EQ(gamma_correct(128, 128), led_color_buffer[3].r);
    EXPECT_EQ(gamma_correct(64, 128), led_color_buffer[3].g);
    EXPECT_EQ(gamma_correct(32, 128), led_color_buffer[3].b);
}

TEST(RGB, SetBrightnessUpdatesOutputAcrossRepeatedValues)
{
    for (uint8_t brightness : {0, 255, 128, 128, 1, 0, 255}) {
        rgb_set_brightness(brightness);
        rgb_set(3, 128, 64, 255);

        EXPECT_EQ(brightness, g_rgb_base_config.brightness);
        EXPECT_EQ(gamma_correct(128, brightness), led_color_buffer[3].r);
        EXPECT_EQ(gamma_correct(64, brightness), led_color_buffer[3].g);
        EXPECT_EQ(gamma_correct(255, brightness), led_color_buffer[3].b);
    }
}

TEST(RGB, InitRebuildsBrightnessAfterConfigImport)
{
    rgb_set_brightness(32);
    RGBBaseConfig imported = g_rgb_base_config;
    imported.brightness = 137;
    g_rgb_base_config = imported;

    rgb_init();
    rgb_set(3, 128, 64, 255);

    EXPECT_EQ(gamma_correct(128, 137), led_color_buffer[3].r);
    EXPECT_EQ(gamma_correct(64, 137), led_color_buffer[3].g);
    EXPECT_EQ(gamma_correct(255, 137), led_color_buffer[3].b);
}

TEST(RGB, BrightnessKeysUpdateOutputAndClampAtLimits)
{
    const struct {
        uint8_t initial;
        uint8_t command;
        uint8_t expected;
    } cases[] = {
        {250, KEYBOARD_RGB_BRIGHTNESS_UP, 255},
        {8, KEYBOARD_RGB_BRIGHTNESS_DOWN, 0},
        {32, KEYBOARD_RGB_BRIGHTNESS_UP, 48},
        {32, KEYBOARD_RGB_BRIGHTNESS_DOWN, 16},
    };
    for (const auto &test : cases) {
        rgb_set_brightness(test.initial);
        const KeyboardEvent event = MK_VIRTUAL_EVENT(
            KEYCODE(KEYBOARD_OPERATION, test.command), KEYBOARD_EVENT_KEY_DOWN, keyboard_get_key(0));
        keyboard_operation_event_handler(event);
        keyboard_operation_event_poller(event, g_keyboard_tick);
        rgb_set(3, 128, 64, 255);

        EXPECT_EQ(test.expected, g_rgb_base_config.brightness);
        EXPECT_EQ(gamma_correct(128, test.expected), led_color_buffer[3].r);
        EXPECT_EQ(gamma_correct(64, test.expected), led_color_buffer[3].g);
        EXPECT_EQ(gamma_correct(255, test.expected), led_color_buffer[3].b);
    }
}

TEST(RGB, FixedModeFlushesDeterministicLedColors)
{
    libamp_test_clear_output_buffers();
    g_rgb_base_config.mode = RGB_BASE_MODE_BLANK;
    rgb_set_brightness(255);

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

TEST(RGB, RendersOneFrameUntilTheTickAdvances)
{
    libamp_test_clear_output_buffers();
    g_rgb_base_config.mode = RGB_BASE_MODE_BLANK;
    for (uint16_t i = 0; i < RGB_NUM; i++) {
        g_rgb_configs[i].mode = RGB_MODE_FIXED;
        g_rgb_configs[i].rgb = {0, 0, 0};
    }

    rgb_process();
    rgb_process();
    EXPECT_EQ(1U, led_flush_count) << "a second call in the same tick renders nothing";

    g_keyboard_tick++;
    rgb_process();
    EXPECT_EQ(2U, led_flush_count);
}

TEST(RGB, TriggerFadeEndsExactlyWhereTheColorTruncatesToBlack)
{
    libamp_test_clear_output_buffers();
    g_rgb_base_config.mode = RGB_BASE_MODE_BLANK;
    rgb_set_brightness(255);
    for (uint16_t i = 0; i < RGB_NUM; i++) {
        g_rgb_configs[i].mode = RGB_MODE_FIXED;
        g_rgb_configs[i].rgb = {0, 0, 0};
    }
    g_rgb_configs[0].mode = RGB_MODE_TRIGGER;
    g_rgb_configs[0].rgb = {255, 255, 255};
    g_rgb_configs[0].speed = 20;
    Key *key = keyboard_get_key(g_rgb_mapping[0]);
    ASSERT_NE(nullptr, key);

    // The renderer fades by 0.9999 per (millisecond * speed) since the trigger.
    auto faded = [](uint32_t ticks) {
        const float span = (float)KEYBOARD_TICK_TO_TIME(ticks) * 20.0f;
        return (uint8_t)(std::exp(span * -1.0000500033e-4f) * 255.0f);
    };
    static_assert(KEYBOARD_TICK_TO_TIME(2750) * 20 == 55000, "the boundary ticks below assume a 1 kHz tick");

    g_keyboard_tick = 1000;
    key->report_state = 1;
    rgb_process();
    key->report_state = 0;
    EXPECT_EQ(255, g_rgb_colors[0].r);

    g_keyboard_tick = 1000 + 500;
    rgb_process();
    EXPECT_EQ(faded(500), g_rgb_colors[0].r);
    EXPECT_NEAR(94, g_rgb_colors[0].r, 1);

    g_keyboard_tick = 1000 + 2750;   /* exponent -5.50: 255 * 0.00408 is still one count */
    rgb_process();
    EXPECT_EQ(1, g_rgb_colors[0].r);
    EXPECT_EQ(1, g_rgb_colors[0].b);

    g_keyboard_tick = 1000 + 2850;   /* exponent -5.70: truncates to black */
    rgb_process();
    EXPECT_EQ(0, g_rgb_colors[0].r);

    g_keyboard_tick = 1000 + 100000;
    rgb_process();
    EXPECT_EQ(0, g_rgb_colors[0].r);
    EXPECT_EQ(0, g_rgb_colors[0].g);
}

TEST(RGB, TriggerMatchesScalarForHeldAndReleasedKeysAcrossSpeeds)
{
    g_rgb_base_config.mode = RGB_BASE_MODE_BLANK;
    for (uint16_t i = 0; i < RGB_NUM; i++) {
        g_rgb_configs[i].mode = RGB_MODE_FIXED;
        g_rgb_configs[i].rgb = {0, 0, 0};
    }
    RGBConfig &config = g_rgb_configs[0];
    config.mode = RGB_MODE_TRIGGER;
    Key *key = keyboard_get_key(g_rgb_mapping[0]);
    ASSERT_NE(nullptr, key);
    const int16_t speeds[] = {-32768, -20, -1, 0, 1, 20, 32767};
    for (int16_t speed : speeds) {
        SCOPED_TRACE(speed);
        rgb_init();
        config.speed = speed;
        config.rgb = {255, 128, 64};
        key->report_state = 1;
        // Held frames also cross the tick wrap and must keep restarting decay.
        const uint32_t held_ticks[] = {0xFFFFFFF0u, 0xFFFFFFFBu, 1u};
        for (uint32_t tick : held_ticks) {
            g_keyboard_tick = tick;
            rgb_process();
            EXPECT_EQ(tick, config.begin_tick);
            EXPECT_EQ(config.rgb.r, g_rgb_colors[0].r);
            EXPECT_EQ(config.rgb.g, g_rgb_colors[0].g);
            EXPECT_EQ(config.rgb.b, g_rgb_colors[0].b);
        }
        key->report_state = 0;
        // Negative speeds grow the color. Keep those samples within the byte
        // conversion's defined range, including the most negative speed.
        if (speed < 0) {
            config.rgb = {1, 2, 3};
        }
        const uint32_t elapsed_ticks[] = {1, 10, 100, 2850};
        for (uint32_t elapsed : elapsed_ticks) {
            if (speed == -32768 && elapsed != 1) {
                continue;
            }
            if (speed < 0 && elapsed == 2850) {
                continue;
            }
            g_keyboard_tick = config.begin_tick + elapsed;
            const float span = static_cast<float>(KEYBOARD_TICK_TO_TIME(elapsed)) * static_cast<float>(speed);
            const float intensity = std::exp(span * -1.0000500033e-4f);
            rgb_process();
            EXPECT_EQ(static_cast<uint8_t>(intensity * config.rgb.r), g_rgb_colors[0].r);
            EXPECT_EQ(static_cast<uint8_t>(intensity * config.rgb.g), g_rgb_colors[0].g);
            EXPECT_EQ(static_cast<uint8_t>(intensity * config.rgb.b), g_rgb_colors[0].b);
        }
    }
}

TEST(RGB, FrameDueCheckHonoursTheIntervalAcrossTickWrap)
{
    static_assert(RGB_FRAME_INTERVAL_TICKS == 1, "the test configuration keeps one frame per tick");

    EXPECT_FALSE(rgb_frame_is_due(5, 5, 1));
    EXPECT_TRUE(rgb_frame_is_due(6, 5, 1));
    EXPECT_FALSE(rgb_frame_is_due(107, 100, 8));
    EXPECT_TRUE(rgb_frame_is_due(108, 100, 8));
    EXPECT_FALSE(rgb_frame_is_due(5, 0xFFFFFFFEu, 8)) << "seven ticks across the wrap";
    EXPECT_TRUE(rgb_frame_is_due(6, 0xFFFFFFFEu, 8)) << "eight ticks across the wrap";
}

// --- Reference frames -------------------------------------------------------
// Hash a deterministic scenario per renderer so that an optimization of the
// render loop can be shown to reproduce every frame bit for bit. The expected
// values were captured from the renderer before those optimizations.

namespace {

uint32_t fnv1a_colors(uint32_t h)
{
    for (uint16_t i = 0; i < RGB_NUM; i++) {
        const uint8_t bytes[3] = {g_rgb_colors[i].r, g_rgb_colors[i].g, g_rgb_colors[i].b};
        for (uint8_t b : bytes) {
            h ^= b;
            h *= 16777619u;
        }
    }
    return h;
}

uint32_t render_fingerprint(RGBBaseMode base_mode, RGBMode key_mode)
{
    g_rgb_base_config.mode = base_mode;
    g_rgb_base_config.rgb = {200, 80, 30};
    rgb_to_hsv(&g_rgb_base_config.hsv, &g_rgb_base_config.rgb);
    g_rgb_base_config.secondary_rgb = {10, 60, 220};
    rgb_to_hsv(&g_rgb_base_config.secondary_hsv, &g_rgb_base_config.secondary_rgb);
    g_rgb_base_config.speed = 20;
    g_rgb_base_config.direction = 37;
    g_rgb_base_config.density = 3;
    rgb_set_brightness(255);
    for (uint16_t i = 0; i < RGB_NUM; i++) {
        g_rgb_configs[i].mode = key_mode;
        g_rgb_configs[i].rgb = {255, 128, 64};
        g_rgb_configs[i].speed = 20;
        g_rgb_configs[i].begin_tick = 0;
    }
    for (uint16_t i = 0; i < ADVANCED_KEY_NUM; i++) {
        g_keyboard_advanced_keys[i].value = (AnalogValue)(ANALOG_VALUE_MIN + (i * 977u) % ANALOG_VALUE_RANGE);
        g_keyboard_advanced_keys[i].key.report_state = (i % 5) == 0;
    }

    uint32_t h = 2166136261u;
    g_keyboard_tick = 1000;
    for (int frame = 0; frame < 6; frame++) {
        rgb_process();
        h = fnv1a_colors(h);
        g_keyboard_tick += 250;
        if (frame == 1) {
            for (uint16_t i = 0; i < ADVANCED_KEY_NUM; i++) {
                g_keyboard_advanced_keys[i].key.report_state = 0;   /* keys released: fades run */
            }
        }
        if (frame == 2) {
            g_rgb_base_config.direction = 123;      /* direction changes mid-way */
        }
        if (frame == 3) {
            g_rgb_base_config.rgb = {15, 230, 120};  /* base color changes mid-way */
            rgb_to_hsv(&g_rgb_base_config.hsv, &g_rgb_base_config.rgb);
        }
    }
    return h;
}

} // namespace

TEST(RGB, RenderersReproduceTheirReferenceFrames)
{
    EXPECT_EQ(2761176814u, render_fingerprint(RGB_BASE_MODE_RAINBOW, RGB_MODE_LINEAR));
    EXPECT_EQ(965217455u, render_fingerprint(RGB_BASE_MODE_WAVE, RGB_MODE_TRIGGER));
    EXPECT_EQ(3163515849u, render_fingerprint(RGB_BASE_MODE_BLANK, RGB_MODE_JELLY));
    EXPECT_EQ(3643513885u, render_fingerprint(RGB_BASE_MODE_RAINBOW, RGB_MODE_CYCLE));
}
