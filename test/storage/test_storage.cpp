#include <gtest/gtest.h>

#include <array>
#include <cstring>

#include "dynamic_key.h"
#include "file_system.h"
#include "rgb.h"
#include "script.h"
#include "storage.h"
#include "test_fixture.h"

namespace {

AdvancedKeyConfiguration make_advanced_config(uint16_t index)
{
    AdvancedKeyConfiguration config = {};
    config.mode = index % 4;
    config.calibration_mode = (index + 1) % 4;
    config.activation_value = static_cast<AnalogValue>(1000 + index);
    config.deactivation_value = static_cast<AnalogValue>(900 + index);
    config.trigger_distance = static_cast<AnalogValue>(100 + index);
    config.release_distance = static_cast<AnalogValue>(110 + index);
    config.trigger_speed = static_cast<AnalogValue>(120 + index);
    config.release_speed = static_cast<AnalogValue>(130 + index);
    config.upper_deadzone = static_cast<AnalogValue>(10 + index);
    config.lower_deadzone = static_cast<AnalogValue>(20 + index);
    config.upper_bound = static_cast<AnalogRawValue>(3000 + index);
    config.lower_bound = static_cast<AnalogRawValue>(1000 + index);
    return config;
}

void fill_profile(uint16_t seed)
{
    for (uint16_t i = 0; i < ADVANCED_KEY_NUM; i++) {
        g_keyboard_advanced_keys[i].config = make_advanced_config(seed + i);
    }

    for (uint16_t layer = 0; layer < LAYER_NUM; layer++) {
        for (uint16_t key = 0; key < TOTAL_KEY_NUM; key++) {
            g_keymap[layer][key] = static_cast<Keycode>(seed + layer * 257 + key);
        }
    }

    g_rgb_base_config.mode = RGB_BASE_MODE_WAVE;
    g_rgb_base_config.rgb = {
        static_cast<uint8_t>(seed + 1),
        static_cast<uint8_t>(seed + 2),
        static_cast<uint8_t>(seed + 3),
    };
    rgb_to_hsv(&g_rgb_base_config.hsv, &g_rgb_base_config.rgb);
    g_rgb_base_config.secondary_rgb = {
        static_cast<uint8_t>(seed + 4),
        static_cast<uint8_t>(seed + 5),
        static_cast<uint8_t>(seed + 6),
    };
    rgb_to_hsv(&g_rgb_base_config.secondary_hsv, &g_rgb_base_config.secondary_rgb);
    g_rgb_base_config.speed = static_cast<int16_t>(seed + 7);
    g_rgb_base_config.begin_tick = 12345;
    g_rgb_base_config.direction = static_cast<uint16_t>(seed + 8);
    g_rgb_base_config.density = static_cast<uint8_t>(seed + 9);
    g_rgb_base_config.brightness = static_cast<uint8_t>(seed + 10);

    for (uint16_t i = 0; i < RGB_NUM; i++) {
        g_rgb_configs[i].mode = static_cast<RGBMode>(i % (RGB_MODE_BUBBLE + 1));
        g_rgb_configs[i].rgb = {
            static_cast<uint8_t>(seed + i + 1),
            static_cast<uint8_t>(seed + i + 2),
            static_cast<uint8_t>(seed + i + 3),
        };
        rgb_to_hsv(&g_rgb_configs[i].hsv, &g_rgb_configs[i].rgb);
        g_rgb_configs[i].speed = static_cast<int16_t>(seed + i + 4);
        g_rgb_configs[i].begin_tick = 1000 + i;
    }

    for (uint16_t i = 0; i < DYNAMIC_KEY_NUM; i++) {
        g_dynamic_keys[i] = {};
        g_dynamic_keys[i].dks.type = DYNAMIC_KEY_STROKE;
        g_dynamic_keys[i].dks.key_binding[0] = static_cast<Keycode>(KEY_A + (i % 10));
        g_dynamic_keys[i].dks.key_binding[1] = static_cast<Keycode>(KEY_B + (i % 10));
        g_dynamic_keys[i].dks.key_control[0] = DKS_KEY_CONTROL(DKS_HOLD, DKS_TAP, DKS_RELEASE, DKS_HOLD);
        g_dynamic_keys[i].dks.press_begin_distance = static_cast<AnalogValue>(seed + i + 20);
        g_dynamic_keys[i].dks.press_fully_distance = static_cast<AnalogValue>(seed + i + 30);
        g_dynamic_keys[i].dks.release_begin_distance = static_cast<AnalogValue>(seed + i + 40);
        g_dynamic_keys[i].dks.release_fully_distance = static_cast<AnalogValue>(seed + i + 50);
    }
}

template <typename T>
void expect_memory_eq(const T& expected, const T& actual)
{
    EXPECT_EQ(0, std::memcmp(&expected, &actual, sizeof(T)));
}

void expect_color_eq(ColorRGB expected, ColorRGB actual)
{
    EXPECT_EQ(expected.r, actual.r);
    EXPECT_EQ(expected.g, actual.g);
    EXPECT_EQ(expected.b, actual.b);
}

void expect_hsv_eq(ColorHSV expected, ColorHSV actual)
{
    EXPECT_EQ(expected.h, actual.h);
    EXPECT_EQ(expected.s, actual.s);
    EXPECT_EQ(expected.v, actual.v);
}

void expect_rgb_base_config_eq(const RGBBaseConfig& expected, const RGBBaseConfig& actual)
{
    EXPECT_EQ(expected.mode, actual.mode);
    expect_color_eq(expected.rgb, actual.rgb);
    expect_hsv_eq(expected.hsv, actual.hsv);
    expect_color_eq(expected.secondary_rgb, actual.secondary_rgb);
    expect_hsv_eq(expected.secondary_hsv, actual.secondary_hsv);
    EXPECT_EQ(expected.speed, actual.speed);
    EXPECT_EQ(expected.begin_tick, actual.begin_tick);
    EXPECT_EQ(expected.direction, actual.direction);
    EXPECT_EQ(expected.density, actual.density);
    EXPECT_EQ(expected.brightness, actual.brightness);
}

void expect_rgb_config_eq(const RGBConfig& expected, const RGBConfig& actual)
{
    EXPECT_EQ(expected.mode, actual.mode);
    expect_color_eq(expected.rgb, actual.rgb);
    expect_hsv_eq(expected.hsv, actual.hsv);
    EXPECT_EQ(expected.speed, actual.speed);
    EXPECT_EQ(expected.begin_tick, actual.begin_tick);
}

} // namespace

TEST(Storage, CompleteProfileRoundTrip)
{
    g_current_profile_index = 0;
    fill_profile(31);

    std::array<AdvancedKeyConfiguration, ADVANCED_KEY_NUM> advanced_configs;
    for (uint16_t i = 0; i < ADVANCED_KEY_NUM; i++) {
        advanced_configs[i] = g_keyboard_advanced_keys[i].config;
    }
    std::array<Keycode, LAYER_NUM * TOTAL_KEY_NUM> expected_keymap;
    std::memcpy(expected_keymap.data(), g_keymap, sizeof(g_keymap));
    const RGBBaseConfig expected_rgb_base = g_rgb_base_config;
    std::array<RGBConfig, RGB_NUM> expected_rgb_configs;
    std::memcpy(expected_rgb_configs.data(), g_rgb_configs, sizeof(g_rgb_configs));
    std::array<DynamicKey, DYNAMIC_KEY_NUM> expected_dynamic_keys;
    std::memcpy(expected_dynamic_keys.data(), g_dynamic_keys, sizeof(g_dynamic_keys));

    storage_save_profile();

    std::memset(g_keymap, 0, sizeof(g_keymap));
    std::memset(g_rgb_configs, 0, sizeof(g_rgb_configs));
    std::memset(g_dynamic_keys, 0, sizeof(g_dynamic_keys));
    std::memset(&g_rgb_base_config, 0, sizeof(g_rgb_base_config));
    for (auto& key : g_keyboard_advanced_keys) {
        std::memset(&key.config, 0, sizeof(key.config));
    }

    storage_read_profile();

    for (uint16_t i = 0; i < ADVANCED_KEY_NUM; i++) {
        expect_memory_eq(advanced_configs[i], g_keyboard_advanced_keys[i].config);
    }
    EXPECT_EQ(0, std::memcmp(expected_keymap.data(), g_keymap, sizeof(g_keymap)));
    EXPECT_EQ(0, std::memcmp(expected_dynamic_keys.data(), g_dynamic_keys, sizeof(g_dynamic_keys)));

    auto normalized_rgb_base = expected_rgb_base;
    normalized_rgb_base.begin_tick = 0;
    expect_rgb_base_config_eq(normalized_rgb_base, g_rgb_base_config);

    auto normalized_rgb_configs = expected_rgb_configs;
    for (auto& config : normalized_rgb_configs) {
        config.begin_tick = 0;
    }
    for (uint16_t i = 0; i < RGB_NUM; i++) {
        expect_rgb_config_eq(normalized_rgb_configs[i], g_rgb_configs[i]);
    }
}

TEST(Storage, ProfilesAreIsolated)
{
    g_current_profile_index = 0;
    fill_profile(10);
    storage_save_profile();
    std::array<Keycode, LAYER_NUM * TOTAL_KEY_NUM> profile0_keymap;
    std::memcpy(profile0_keymap.data(), g_keymap, sizeof(g_keymap));
    std::array<DynamicKey, DYNAMIC_KEY_NUM> profile0_dynamic_keys;
    std::memcpy(profile0_dynamic_keys.data(), g_dynamic_keys, sizeof(g_dynamic_keys));

    g_current_profile_index = 1;
    fill_profile(70);
    storage_save_profile();
    std::array<Keycode, LAYER_NUM * TOTAL_KEY_NUM> profile1_keymap;
    std::memcpy(profile1_keymap.data(), g_keymap, sizeof(g_keymap));

    g_current_profile_index = 0;
    storage_read_profile();
    EXPECT_EQ(0, std::memcmp(profile0_keymap.data(), g_keymap, sizeof(g_keymap)));
    EXPECT_EQ(0, std::memcmp(profile0_dynamic_keys.data(), g_dynamic_keys, sizeof(g_dynamic_keys)));

    g_current_profile_index = 1;
    storage_read_profile();
    EXPECT_EQ(0, std::memcmp(profile1_keymap.data(), g_keymap, sizeof(g_keymap)));
}

TEST(Storage, ProfileIndexRejectsOutOfRangeValue)
{
    g_current_profile_index = 2;
    storage_save_profile_index();
    g_current_profile_index = 0;
    EXPECT_EQ(2, storage_read_profile_index());
    EXPECT_EQ(2, g_current_profile_index);

    g_current_profile_index = STORAGE_PROFILE_FILE_NUM;
    storage_save_profile_index();
    g_current_profile_index = 1;
    EXPECT_EQ(0, storage_read_profile_index());
    EXPECT_EQ(0, g_current_profile_index);
}

TEST(Storage, VersionCheckDifferentiatesPatchAndBreakingVersions)
{
    EXPECT_FALSE(storage_check_version());

    File file;
    ASSERT_GE(fs_open(&file, "system/version", FS_O_RDWR | FS_O_CREAT), 0);
    uint32_t patch_only_update[3] = {
        KEYBOARD_VERSION_MAJOR,
        KEYBOARD_VERSION_MINOR,
        KEYBOARD_VERSION_PATCH + 1,
    };
    ASSERT_EQ(sizeof(patch_only_update), fs_write(&file, patch_only_update, sizeof(patch_only_update)));
    fs_close(&file);
    EXPECT_FALSE(storage_check_version());

    ASSERT_GE(fs_open(&file, "system/version", FS_O_RDWR | FS_O_CREAT), 0);
    uint32_t breaking_update[3] = {
        KEYBOARD_VERSION_MAJOR + 1,
        KEYBOARD_VERSION_MINOR,
        KEYBOARD_VERSION_PATCH,
    };
    ASSERT_EQ(sizeof(breaking_update), fs_write(&file, breaking_update, sizeof(breaking_update)));
    fs_close(&file);
    EXPECT_TRUE(storage_check_version());
}

TEST(Storage, ScriptBytecodeRoundTrip)
{
#if defined(SCRIPT_ENABLE) && SCRIPT_RUNTIME_STRATEGY == SCRIPT_AOT
    for (size_t i = 0; i < sizeof(g_script_bytecode_buffer); i++) {
        g_script_bytecode_buffer[i] = static_cast<uint8_t>((i * 13U) & 0xFFU);
    }
    std::array<uint8_t, sizeof(g_script_bytecode_buffer)> expected_bytecode;
    std::memcpy(expected_bytecode.data(), g_script_bytecode_buffer, sizeof(g_script_bytecode_buffer));

    storage_save_script();
    std::memset(g_script_bytecode_buffer, 0, sizeof(g_script_bytecode_buffer));
    storage_read_script();

    EXPECT_EQ(0, std::memcmp(expected_bytecode.data(), g_script_bytecode_buffer, sizeof(g_script_bytecode_buffer)));
#else
    GTEST_SKIP() << "Script storage test currently targets the default AOT bytecode buffer.";
#endif
}
