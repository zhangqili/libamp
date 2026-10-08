#include <gtest/gtest.h>

#include <array>
#include <cstring>

#include "dynamic_key.h"
#include "file_system.h"
#include "record.h"
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

/* fs_format() only erases. It creates nothing itself: the directories are made
 * by fs_init(). */
TEST(Storage, FormatErasesWithoutCreatingAnything)
{
    FileStat info;

    g_current_profile_index = 0;
    fill_profile(33);
    storage_save_profile();

    ASSERT_EQ(0, fs_format());
    ASSERT_EQ(0, fs_init());

    EXPECT_NE(0, fs_stat("profiles/profile0", &info));
    EXPECT_NE(0, fs_stat("system/version", &info));
    /* The layout is fs_init()'s, and it is empty. */
    EXPECT_EQ(0, fs_stat("profiles", &info));
    EXPECT_EQ(0, fs_stat("system", &info));
}

/* The KEYBOARD_FORMAT_STORAGE operation erases the volume and nothing else: it
 * stores no configuration and no version, so the next boot puts the factory
 * configuration in and only then marks the volume as initialized. */
TEST(Storage, FormatStorageOperationErasesTheVolume)
{
    FileStat info;

    g_current_profile_index = 0;
    fill_profile(11);
    storage_save_profile();
    storage_save_statistics();
    ASSERT_EQ(0, fs_stat("profiles/profile0", &info));
    ASSERT_EQ(0, fs_stat("system/stat", &info));

    keyboard_event_handler(MK_VIRTUAL_EVENT((KEYBOARD_FORMAT_STORAGE << 8) | KEYBOARD_OPERATION,
                                            KEYBOARD_EVENT_KEY_DOWN, NULL));

    /* Everything is gone, the version with it; the volume is mounted again. */
    EXPECT_NE(0, fs_stat("profiles/profile0", &info));
    EXPECT_NE(0, fs_stat("system/stat", &info));
    EXPECT_NE(0, fs_stat("system/version", &info));

    /* The next boot initializes it... */
    EXPECT_TRUE(storage_check_version());
    keyboard_factory_reset();   /* what keyboard_init() does in that case */
    EXPECT_EQ(0, fs_stat("profiles/profile0", &info));
    EXPECT_EQ(0, std::memcmp(g_default_keymap, g_keymap, sizeof(g_keymap)));
    /* ...and the boots after that find it initialized. */
    EXPECT_FALSE(storage_check_version());
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

TEST(Storage, StatisticsRoundTrip)
{
    g_runtime = 123456789ULL;
    for (uint16_t i = 0; i < TOTAL_KEY_NUM; i++) {
        g_key_counts[i] = i * 17U + 3U;
    }

    storage_save_statistics();
    g_runtime = 0;
    std::memset(g_key_counts, 0, sizeof(g_key_counts));

    storage_read_statistics();

    EXPECT_EQ(123456789ULL, g_runtime);
    for (uint16_t i = 0; i < TOTAL_KEY_NUM; i++) {
        EXPECT_EQ(i * 17U + 3U, g_key_counts[i]);
    }
}

TEST(Storage, StatisticsRestoreDuringLibampInit)
{
    g_runtime = 987654321ULL;
    for (uint16_t i = 0; i < TOTAL_KEY_NUM; i++) {
        g_key_counts[i] = i * 23U + 5U;
    }
    storage_save_statistics();

    g_runtime = 0;
    std::memset(g_key_counts, 0, sizeof(g_key_counts));
    keyboard_init();

    EXPECT_EQ(987654321ULL, g_runtime);
    for (uint16_t i = 0; i < TOTAL_KEY_NUM; i++) {
        EXPECT_EQ(i * 23U + 5U, g_key_counts[i]);
    }
}

namespace {
struct SavedStatistics {
    uint64_t runtime = 0;
    std::array<uint32_t, TOTAL_KEY_NUM> counts = {};
};

SavedStatistics read_saved_statistics()
{
    SavedStatistics saved;
    File file;
    EXPECT_GE(fs_open(&file, "system/stat", FS_O_RDONLY), 0);
    EXPECT_EQ(sizeof(saved.runtime), fs_read(&file, &saved.runtime, sizeof(saved.runtime)));
    EXPECT_EQ(sizeof(saved.counts), fs_read(&file, saved.counts.data(), sizeof(saved.counts)));
    EXPECT_EQ(0, fs_close(&file));
    return saved;
}
}

TEST(Storage, StatisticsSaveAtConfiguredInterval)
{
    const uint32_t interval_ticks = KEYBOARD_TIME_TO_TICK(RECORD_STATISTICS_SAVE_INTERVAL_MS);
    storage_save_statistics();
    g_runtime = 17;
    g_keyboard_tick = interval_ticks - 1U;
    record_process();
    EXPECT_EQ(0ULL, read_saved_statistics().runtime);

    g_keyboard_tick = interval_ticks;
    record_process();
    const auto saved = read_saved_statistics();
    EXPECT_EQ(17ULL + RECORD_STATISTICS_SAVE_INTERVAL_MS, saved.runtime);
    g_keyboard_tick += interval_ticks - 1U;
    record_process();
    EXPECT_EQ(saved.runtime, read_saved_statistics().runtime);
    g_keyboard_tick = g_keyboard_tick + 1U;
    record_process();
    EXPECT_EQ(17ULL + 2ULL * RECORD_STATISTICS_SAVE_INTERVAL_MS, read_saved_statistics().runtime);
}

TEST(Storage, StatisticsActivityRestartsIdleCountdown)
{
    const uint32_t idle_ticks = KEYBOARD_TIME_TO_TICK(RECORD_STATISTICS_IDLE_SAVE_MS);
    storage_save_statistics();
    auto *key = keyboard_get_key(0);
    keyboard_key_event_down_callback(key);
    keyboard_key_event_up_callback(key);
    g_keyboard_tick = idle_ticks - 1U;
    keyboard_key_event_down_callback(key);
    keyboard_key_event_up_callback(key);
    g_keyboard_tick = idle_ticks;
    record_process();
    EXPECT_EQ(0U, read_saved_statistics().counts[0]);

    g_keyboard_tick = 2U * idle_ticks - 2U;
    record_process();
    EXPECT_EQ(0U, read_saved_statistics().counts[0]);
    g_keyboard_tick = g_keyboard_tick + 1U;
    record_process();
    EXPECT_EQ(2U, read_saved_statistics().counts[0]);

    // A new count change advances the normal interval to another 120-second wait.
    keyboard_key_event_down_callback(key);
    keyboard_key_event_up_callback(key);
    g_keyboard_tick += idle_ticks;
    record_process();
    const auto saved = read_saved_statistics();
    EXPECT_EQ(3U, saved.counts[0]);
    g_keyboard_tick += idle_ticks;
    record_process();
    EXPECT_EQ(saved.runtime, read_saved_statistics().runtime);
}

TEST(Storage, StatisticsReleasesAndHeldKeysDoNotChangeCountdown)
{
    const uint32_t idle_ticks = KEYBOARD_TIME_TO_TICK(RECORD_STATISTICS_IDLE_SAVE_MS);
    storage_save_statistics();
    auto *first = keyboard_get_key(0);
    keyboard_key_event_down_callback(first);
    g_keyboard_tick = idle_ticks / 2U;
    keyboard_key_event_up_callback(first);
    g_keyboard_tick = idle_ticks;
    record_process();
    EXPECT_EQ(1U, read_saved_statistics().counts[0]);

    keyboard_key_event_down_callback(first);
    g_keyboard_tick += idle_ticks - 1U;
    record_process();
    EXPECT_EQ(1U, read_saved_statistics().counts[0]);
    g_keyboard_tick = g_keyboard_tick + 1U;
    record_process();
    EXPECT_EQ(2U, read_saved_statistics().counts[0]);
}

TEST(Storage, StatisticsCountChangeNearDeadlineLeavesFullIdleDelay)
{
    const uint32_t interval_ticks = KEYBOARD_TIME_TO_TICK(RECORD_STATISTICS_SAVE_INTERVAL_MS);
    const uint32_t idle_ticks = KEYBOARD_TIME_TO_TICK(RECORD_STATISTICS_IDLE_SAVE_MS);
    storage_save_statistics();
    g_keyboard_tick = interval_ticks - 1U;
    keyboard_key_event_down_callback(keyboard_get_key(0));
    g_keyboard_tick = interval_ticks;
    record_process();
    EXPECT_EQ(0U, read_saved_statistics().counts[0]);
    g_keyboard_tick = interval_ticks + idle_ticks - 1U;
    record_process();
    EXPECT_EQ(1U, read_saved_statistics().counts[0]);
}

TEST(Storage, StatisticsIdleCountdownHandlesTickWrap)
{
    const uint32_t idle_ticks = KEYBOARD_TIME_TO_TICK(RECORD_STATISTICS_IDLE_SAVE_MS);
    storage_save_statistics();
    g_keyboard_tick = UINT32_MAX - idle_ticks / 2U;
    keyboard_key_event_down_callback(keyboard_get_key(0));
    keyboard_key_event_up_callback(keyboard_get_key(0));
    g_keyboard_tick += idle_ticks - 1U;
    record_process();
    EXPECT_EQ(0U, read_saved_statistics().counts[0]);
    g_keyboard_tick = g_keyboard_tick + 1U;
    record_process();
    EXPECT_EQ(1U, read_saved_statistics().counts[0]);
}

TEST(Storage, ExplicitStatisticsSaveRestoresNormalInterval)
{
    const uint32_t idle_ticks = KEYBOARD_TIME_TO_TICK(RECORD_STATISTICS_IDLE_SAVE_MS);
    keyboard_key_event_down_callback(keyboard_get_key(0));
    keyboard_key_event_up_callback(keyboard_get_key(0));
    record_reset_save_timer();
    storage_save_statistics();
    const auto saved = read_saved_statistics();
    EXPECT_EQ(1U, saved.counts[0]);
    g_keyboard_tick = idle_ticks * 2U;
    record_process();
    EXPECT_EQ(saved.runtime, read_saved_statistics().runtime);
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

#if FILE_SYSTEM_TYPE == FILE_SYSTEM_FILEX
extern "C" uint8_t flash_buffer[FS_BLOCK_SIZE * FS_BLOCK_COUNT];

namespace {
struct FileXTestFile : File {
    FileXTestFile() : File{} {}
    ~FileXTestFile()
    {
        flash_read_fail_after = flash_write_fail_after = flash_erase_fail_after = -1;
        if (handle.fx_file_id == FX_FILE_ID) (void)fs_close(this);
    }
};
}

TEST(FileX, RemountPreservesClosedAndSyncedFiles)
{
    FileXTestFile file;
    Directory old_dir{};
    ASSERT_EQ(0, fs_opendir(&old_dir, "/"));
    char text[] = "FileX persists across remount";
    ASSERT_EQ(0, fs_open(&file, "system/persistence.txt", FS_O_CREAT | FS_O_RDWR));
    ASSERT_EQ(sizeof(text), fs_write(&file, text, sizeof(text)));
    ASSERT_EQ(0, fs_sync(&file));
    ASSERT_EQ(0, fs_init());
    EXPECT_LT(fs_tell(&file), 0);
    FileStat info;
    EXPECT_LT(fs_readdir(&old_dir, &info), 0);
    ASSERT_EQ(0, fs_open(&file, "/system/persistence.txt", FS_O_RDONLY));
    char result[sizeof(text)] = {};
    ASSERT_EQ(sizeof(result), fs_read(&file, result, sizeof(result)));
    EXPECT_STREQ(text, result);
    ASSERT_EQ(0, fs_close(&file));
    ASSERT_EQ(0, fs_init());
    ASSERT_EQ(0, fs_open(&file, "system/persistence.txt", FS_O_RDONLY));
    EXPECT_EQ(sizeof(text), static_cast<size_t>(fs_size(&file)));
    EXPECT_EQ(0U, flash_invalid_accesses);
}

TEST(FileX, OpenFlagsPermissionsAndAppend)
{
    FileXTestFile file, other;
    char text[] = "abc";
    EXPECT_LT(fs_open(&file, "absent", FS_O_RDONLY), 0);
    ASSERT_EQ(0, fs_open(&file, "flags", FS_O_CREAT | FS_O_EXCL | FS_O_WRONLY));
    EXPECT_LT(fs_open(&other, "flags", FS_O_CREAT | FS_O_EXCL | FS_O_RDWR), 0);
    EXPECT_LT(fs_open(&file, "another", FS_O_CREAT | FS_O_RDWR), 0);
    EXPECT_EQ(0U, fs_read(&file, text, 1));
    ASSERT_EQ(3U, fs_write(&file, text, 3));
    ASSERT_EQ(0, fs_close(&file));
    ASSERT_EQ(0, fs_open(&file, "flags", FS_O_RDWR | FS_O_APPEND));
    ASSERT_EQ(0, fs_seek(&file, 0, FS_SEEK_SET));
    ASSERT_EQ(3U, fs_write(&file, text, 3));
    EXPECT_EQ(6, fs_size(&file));
    ASSERT_EQ(0, fs_seek(&file, 0, FS_SEEK_SET));
    char result[7] = {};
    EXPECT_EQ(6U, fs_read(&file, result, sizeof(result)));
    EXPECT_STREQ("abcabc", result);
    EXPECT_EQ(0U, fs_read(&file, result, 1));
    ASSERT_EQ(0, fs_close(&file));
    ASSERT_EQ(0, fs_open(&file, "flags", FS_O_RDONLY));
    EXPECT_EQ(0U, fs_write(&file, text, 1));
    EXPECT_LT(fs_truncate(&file, 0), 0);
    ASSERT_EQ(0, fs_close(&file));
    ASSERT_EQ(0, fs_open(&file, "flags", FS_O_WRONLY | FS_O_TRUNC));
    EXPECT_EQ(0, fs_size(&file));
    EXPECT_LT(fs_open(&other, nullptr, FS_O_RDONLY), 0);
    EXPECT_EQ(0U, fs_read(nullptr, text, 1));
}

TEST(FileX, SeekGapsTruncateAndPageBoundaries)
{
    FileXTestFile file;
    ASSERT_EQ(0, fs_open(&file, "seek", FS_O_CREAT | FS_O_RDWR));
    std::array<uint8_t, 1501> pattern;
    for (size_t i = 0; i < pattern.size(); ++i) pattern[i] = static_cast<uint8_t>(i);
    ASSERT_EQ(0, fs_seek(&file, 513, FS_SEEK_SET));
    EXPECT_EQ(0, fs_size(&file));
    ASSERT_EQ(pattern.size(), fs_write(&file, pattern.data(), pattern.size()));
    EXPECT_EQ(2014, fs_tell(&file));
    EXPECT_LT(fs_seek(&file, -2015, FS_SEEK_CUR), 0);
    ASSERT_EQ(0, fs_seek(&file, -1501, FS_SEEK_END));
    std::array<uint8_t, 1501> result;
    ASSERT_EQ(result.size(), fs_read(&file, result.data(), result.size()));
    EXPECT_EQ(pattern, result);
    ASSERT_EQ(0, fs_seek(&file, 0, FS_SEEK_SET));
    std::array<uint8_t, 513> gap;
    ASSERT_EQ(gap.size(), fs_read(&file, gap.data(), gap.size()));
    for (auto byte : gap) EXPECT_EQ(0, byte);
    ASSERT_EQ(0, fs_truncate(&file, 10));
    EXPECT_EQ(513, fs_tell(&file));
    ASSERT_EQ(0, fs_truncate(&file, 800));
    EXPECT_EQ(800, fs_size(&file));
    ASSERT_EQ(0, fs_seek(&file, 10, FS_SEEK_SET));
    gap.fill(1);
    ASSERT_EQ(gap.size(), fs_read(&file, gap.data(), gap.size()));
    for (auto byte : gap) EXPECT_EQ(0, byte);
    ASSERT_EQ(0, fs_sync(&file));
    EXPECT_EQ(0U, flash_invalid_accesses);
}

TEST(FileX, IndependentDirectoriesLongNamesAndMetadata)
{
    ASSERT_EQ(0, fs_mkdir("first", 0));
    ASSERT_EQ(0, fs_mkdir("second", 0));
    const char *long_name = "a file name much longer than eight dot three.txt";
    FileXTestFile file;
    std::string path = std::string("first/") + long_name;
    ASSERT_EQ(0, fs_open(&file, path.c_str(), FS_O_CREAT | FS_O_RDWR));
    char data[] = "data";
    ASSERT_EQ(4U, fs_write(&file, data, 4));
    ASSERT_EQ(0, fs_close(&file));
    FileStat info;
    ASSERT_EQ(0, fs_stat(path.c_str(), &info));
    EXPECT_STREQ(long_name, info.name);
    EXPECT_EQ(4U, info.size);
    EXPECT_EQ(FS_TYPE_REG, info.type);
    Directory first{}, second{};
    ASSERT_EQ(0, fs_opendir(&first, "first/"));
    ASSERT_EQ(0, fs_opendir(&second, "/second"));
    ASSERT_EQ(0, fs_seekdir(&first, 2));
    ASSERT_EQ(0, fs_seekdir(&second, 2));
    EXPECT_EQ(0, fs_readdir(&second, &info));
    ASSERT_EQ(1, fs_readdir(&first, &info));
    EXPECT_STREQ(long_name, info.name);
    int end = fs_telldir(&first);
    EXPECT_EQ(0, fs_readdir(&first, &info));
    EXPECT_EQ(end, fs_telldir(&first));
    EXPECT_LT(fs_seekdir(&first, end + 1), 0);
    ASSERT_EQ(0, fs_seekdir(&first, 2));
    ASSERT_EQ(1, fs_readdir(&first, &info));
    EXPECT_STREQ(long_name, info.name);
    ASSERT_EQ(0, fs_rewinddir(&first));
    ASSERT_EQ(1, fs_readdir(&first, &info));
    EXPECT_STREQ(".", info.name);
    EXPECT_LT(fs_rmdir("first"), 0);
    ASSERT_EQ(0, fs_closedir(&first));
    ASSERT_EQ(0, fs_closedir(&second));
    ASSERT_EQ(0, fs_rename(path.c_str(), "second/renamed.txt"));
    EXPECT_LT(fs_stat(path.c_str(), &info), 0);
    ASSERT_EQ(0, fs_stat("second/renamed.txt", &info));
    ASSERT_EQ(0, fs_unlink("second/renamed.txt"));
    ASSERT_EQ(0, fs_rmdir("first"));
    ASSERT_EQ(0, fs_rename("second", "renamed-directory"));
    ASSERT_EQ(0, fs_rmdir("renamed-directory"));
    EXPECT_EQ(0U, flash_invalid_accesses);
}

TEST(FileX, FullVolumeReleasesSpaceForReuse)
{
    VolumeStat before{}, full{}, reclaimed{};
    ASSERT_EQ(0, fs_statvfs("/", &before));
    EXPECT_LT(before.f_blocks * before.f_bsize, uint64_t(FS_BLOCK_SIZE) * FS_BLOCK_COUNT);
    FileXTestFile file;
    ASSERT_EQ(0, fs_open(&file, "fill", FS_O_CREAT | FS_O_RDWR));
    std::array<uint8_t, 4096> data;
    data.fill(0x5a);
    size_t written = 0;
    while (written <= before.f_blocks * before.f_bsize) {
        size_t actual = fs_write(&file, data.data(), data.size());
        written += actual;
        if (actual != data.size()) break;
    }
    EXPECT_GT(written, 0U);
    EXPECT_LE(written, before.f_bfree * before.f_frsize);
    ASSERT_EQ(0, fs_close(&file));
    ASSERT_EQ(0, fs_statvfs("/", &full));
    EXPECT_LT(full.f_bfree, before.f_bfree);
    ASSERT_EQ(0, fs_unlink("fill"));
    ASSERT_EQ(0, fs_statvfs("/", &reclaimed));
    EXPECT_EQ(before.f_bfree, reclaimed.f_bfree);
    ASSERT_EQ(0, fs_open(&file, "reuse", FS_O_CREAT | FS_O_RDWR));
    ASSERT_EQ(data.size(), fs_write(&file, data.data(), data.size()));
    ASSERT_EQ(0, fs_sync(&file));
    EXPECT_EQ(0U, flash_invalid_accesses);
}

TEST(FileX, CorruptVolumeIsRebuiltOnce)
{
    std::memset(flash_buffer + FILEX_FLASH_OFFSET, 0, FS_BLOCK_SIZE * FS_BLOCK_COUNT);
    ASSERT_EQ(0, fs_init());
    FileStat info;
    for (auto path : {"profiles", "system", "scripts"}) {
        ASSERT_EQ(0, fs_stat(path, &info));
        EXPECT_EQ(FS_TYPE_DIR, info.type);
    }
    EXPECT_EQ(0U, flash_invalid_accesses);
}

TEST(FileX, FlashFailuresPropagateAndRecoveryIsBounded)
{
    flash_read_fail_after = 0;
    EXPECT_LT(fs_init(), 0);
    flash_read_fail_after = -1;
    ASSERT_EQ(0, fs_init());
    std::memset(flash_buffer + FILEX_FLASH_OFFSET, 0, FS_BLOCK_SIZE * FS_BLOCK_COUNT);
    uint32_t erases = flash_erase_calls;
    flash_erase_fail_after = 0;
    EXPECT_LT(fs_init(), 0);
    EXPECT_LE(flash_erase_calls - erases, 2U);
    flash_erase_fail_after = -1;
    ASSERT_EQ(0, fs_init());
    FileXTestFile file;
    ASSERT_EQ(0, fs_open(&file, "failure", FS_O_CREAT | FS_O_RDWR));
    std::array<uint8_t, 8192> data{};
    flash_write_fail_after = 0;
    size_t actual = fs_write(&file, data.data(), data.size());
    EXPECT_TRUE(actual < data.size() || fs_sync(&file) < 0);
    flash_write_fail_after = -1;
    EXPECT_EQ(0U, flash_invalid_accesses);
}

TEST(FileX, ReadFailureAndPartitionBoundaries)
{
    FileXTestFile file;
    ASSERT_EQ(0, fs_open(&file, "read-failure", FS_O_CREAT | FS_O_RDWR));
    std::array<uint8_t, 8192> data;
    data.fill(0x73);
    ASSERT_EQ(data.size(), fs_write(&file, data.data(), data.size()));
    ASSERT_EQ(0, fs_sync(&file));
    ASSERT_EQ(0, fs_seek(&file, 0, FS_SEEK_SET));
    ASSERT_EQ(FX_SUCCESS, fx_media_cache_invalidate(file.handle.fx_file_media_ptr));
    flash_read_fail_after = 0;
    EXPECT_EQ(0U, fs_read(&file, data.data(), data.size()));
    flash_read_fail_after = -1;
    ASSERT_EQ(0, fs_seek(&file, 0, FS_SEEK_SET));
    ASSERT_EQ(data.size(), fs_read(&file, data.data(), data.size()));
    for (auto byte : data) ASSERT_EQ(0x73, byte);
    ASSERT_EQ(0, fs_close(&file));
    for (size_t i = 0; i < FILEX_FLASH_OFFSET; ++i) ASSERT_EQ(0xff, flash_buffer[i]);
    for (size_t i = FILEX_FLASH_OFFSET + FS_BLOCK_SIZE * FS_BLOCK_COUNT;
         i < sizeof(flash_buffer); ++i) ASSERT_EQ(0xff, flash_buffer[i]);
    EXPECT_EQ(0U, flash_invalid_accesses);
}
#endif
