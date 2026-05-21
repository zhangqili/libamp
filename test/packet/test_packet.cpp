#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstring>

#include "packet.h"
#include "rgb.h"
#include "test_fixture.h"

namespace {

template <typename T>
T *packet_as(std::array<uint8_t, 64>& buffer)
{
    return reinterpret_cast<T *>(buffer.data());
}

constexpr size_t keymap_packet_size(uint8_t length)
{
    return offsetof(PacketKeymap, keymap) + length * sizeof(Keycode);
}

size_t rgb_configs_packet_size(uint8_t length)
{
    return offsetof(PacketRGBConfigs, data) + length * sizeof(reinterpret_cast<PacketRGBConfigs *>(0)->data[0]);
}

constexpr size_t dynamic_key_packet_size()
{
    return offsetof(PacketDynamicKey, dynamic_key) + sizeof(DynamicKey);
}

size_t config_packet_size(uint8_t length)
{
    return offsetof(PacketConfig, data) + length * sizeof(reinterpret_cast<PacketConfig *>(0)->data[0]);
}

size_t debug_packet_size(uint8_t length)
{
    return offsetof(PacketDebug, data) + length * sizeof(reinterpret_cast<PacketDebug *>(0)->data[0]);
}

AdvancedKeyConfiguration packet_advanced_key_config()
{
    AdvancedKeyConfiguration config = {};
    config.mode = ADVANCED_KEY_ANALOG_SPEED_MODE;
    config.calibration_mode = ADVANCED_KEY_AUTO_CALIBRATION_POSITIVE;
    config.activation_value = A_ANTI_NORM(0.51);
    config.deactivation_value = A_ANTI_NORM(0.41);
    config.trigger_distance = A_ANTI_NORM(0.07);
    config.release_distance = A_ANTI_NORM(0.08);
    config.trigger_speed = A_ANTI_NORM(0.01);
    config.release_speed = A_ANTI_NORM(0.02);
    config.upper_deadzone = A_ANTI_NORM(0.03);
    config.lower_deadzone = A_ANTI_NORM(0.20);
    config.upper_bound = 2048;
    config.lower_bound = 100;
    return config;
}

DynamicKey make_dynamic_key()
{
    DynamicKey dynamic_key = {};
    dynamic_key.dks.type = DYNAMIC_KEY_STROKE;
    dynamic_key.dks.key_binding[0] = KEY_A;
    dynamic_key.dks.key_binding[1] = KEY_B;
    dynamic_key.dks.key_binding[2] = KEY_C;
    dynamic_key.dks.key_binding[3] = KEY_D;
    dynamic_key.dks.key_control[0] = DKS_KEY_CONTROL(DKS_HOLD, DKS_HOLD, DKS_HOLD, DKS_RELEASE);
    dynamic_key.dks.press_begin_distance = A_ANTI_NORM(0.25);
    dynamic_key.dks.press_fully_distance = A_ANTI_NORM(0.75);
    dynamic_key.dks.release_begin_distance = A_ANTI_NORM(0.70);
    dynamic_key.dks.release_fully_distance = A_ANTI_NORM(0.20);
    return dynamic_key;
}

} // namespace

TEST(Packet, SetAndGetKeymap)
{
    std::array<uint8_t, 64> buffer = {};
    PacketKeymap *packet = packet_as<PacketKeymap>(buffer);
    packet->code = PACKET_CODE_SET;
    packet->type = PACKET_DATA_KEYMAP;
    packet->layer = 1;
    packet->start = 3;
    packet->length = 5;
    packet->keymap[0] = KEY_A;
    packet->keymap[1] = KEY_B;
    packet->keymap[2] = KEY_C;
    packet->keymap[3] = KEY_D;
    packet->keymap[4] = KEY_E;

    packet_process(buffer.data(), keymap_packet_size(packet->length));

    EXPECT_EQ(KEY_A, g_keymap[1][3]);
    EXPECT_EQ(KEY_B, g_keymap[1][4]);
    EXPECT_EQ(KEY_C, g_keymap[1][5]);
    EXPECT_EQ(KEY_D, g_keymap[1][6]);
    EXPECT_EQ(KEY_E, g_keymap[1][7]);
    EXPECT_EQ(0, std::memcmp(raw_send_buffer, buffer.data(), 63));

    buffer.fill(0);
    packet = packet_as<PacketKeymap>(buffer);
    packet->code = PACKET_CODE_GET;
    packet->type = PACKET_DATA_KEYMAP;
    packet->layer = 1;
    packet->start = 3;
    packet->length = 5;

    packet_process(buffer.data(), keymap_packet_size(packet->length));

    EXPECT_EQ(KEY_A, packet->keymap[0]);
    EXPECT_EQ(KEY_B, packet->keymap[1]);
    EXPECT_EQ(KEY_C, packet->keymap[2]);
    EXPECT_EQ(KEY_D, packet->keymap[3]);
    EXPECT_EQ(KEY_E, packet->keymap[4]);
}

TEST(Packet, SetAndGetAdvancedKey)
{
    std::array<uint8_t, 64> buffer = {};
    PacketAdvancedKey *packet = packet_as<PacketAdvancedKey>(buffer);
    packet->code = PACKET_CODE_SET;
    packet->type = PACKET_DATA_ADVANCED_KEY;
    packet->index = 3;
    packet->data = packet_advanced_key_config();

    packet_process(buffer.data(), sizeof(PacketAdvancedKey));

    EXPECT_EQ(packet->data.mode, g_keyboard_advanced_keys[3].config.mode);
    EXPECT_EQ(packet->data.activation_value, g_keyboard_advanced_keys[3].config.activation_value);
    EXPECT_EQ(packet->data.deactivation_value, g_keyboard_advanced_keys[3].config.deactivation_value);
    EXPECT_EQ(packet->data.trigger_distance, g_keyboard_advanced_keys[3].config.trigger_distance);
    EXPECT_EQ(packet->data.release_distance, g_keyboard_advanced_keys[3].config.release_distance);
    EXPECT_EQ(packet->data.trigger_speed, g_keyboard_advanced_keys[3].config.trigger_speed);
    EXPECT_EQ(packet->data.release_speed, g_keyboard_advanced_keys[3].config.release_speed);
    EXPECT_EQ(packet->data.upper_deadzone, g_keyboard_advanced_keys[3].config.upper_deadzone);
    EXPECT_EQ(packet->data.lower_deadzone, g_keyboard_advanced_keys[3].config.lower_deadzone);

    buffer.fill(0);
    packet = packet_as<PacketAdvancedKey>(buffer);
    packet->code = PACKET_CODE_GET;
    packet->type = PACKET_DATA_ADVANCED_KEY;
    packet->index = 3;

    packet_process(buffer.data(), sizeof(PacketAdvancedKey));

    EXPECT_EQ(g_keyboard_advanced_keys[3].config.mode, packet->data.mode);
    EXPECT_EQ(g_keyboard_advanced_keys[3].config.activation_value, packet->data.activation_value);
}

TEST(Packet, RejectsOutOfRangeAdvancedKeyIndex)
{
    const auto original = g_keyboard_advanced_keys[0].config;
    std::array<uint8_t, 64> buffer = {};
    PacketAdvancedKey *packet = packet_as<PacketAdvancedKey>(buffer);
    packet->code = PACKET_CODE_SET;
    packet->type = PACKET_DATA_ADVANCED_KEY;
    packet->index = ADVANCED_KEY_NUM;
    packet->data = packet_advanced_key_config();

    packet_process(buffer.data(), sizeof(PacketAdvancedKey));

    EXPECT_EQ(0, std::memcmp(&original, &g_keyboard_advanced_keys[0].config, sizeof(original)));
}

TEST(Packet, SetAndGetRGBBaseConfig)
{
    std::array<uint8_t, 64> buffer = {};
    PacketRGBBaseConfig *packet = packet_as<PacketRGBBaseConfig>(buffer);
    packet->code = PACKET_CODE_SET;
    packet->type = PACKET_DATA_RGB_BASE_CONFIG;
    packet->mode = RGB_BASE_MODE_WAVE;
    packet->r = 10;
    packet->g = 20;
    packet->b = 30;
    packet->secondary_r = 40;
    packet->secondary_g = 50;
    packet->secondary_b = 60;
    packet->speed = 70;
    packet->direction = 80;
    packet->density = 90;
    packet->brightness = 100;

    packet_process(buffer.data(), sizeof(PacketRGBBaseConfig));

    EXPECT_EQ(RGB_BASE_MODE_WAVE, g_rgb_base_config.mode);
    EXPECT_EQ(10, g_rgb_base_config.rgb.r);
    EXPECT_EQ(20, g_rgb_base_config.rgb.g);
    EXPECT_EQ(30, g_rgb_base_config.rgb.b);
    EXPECT_EQ(100, g_rgb_base_config.brightness);

    buffer.fill(0);
    packet = packet_as<PacketRGBBaseConfig>(buffer);
    packet->code = PACKET_CODE_GET;
    packet->type = PACKET_DATA_RGB_BASE_CONFIG;

    packet_process(buffer.data(), sizeof(PacketRGBBaseConfig));

    EXPECT_EQ(RGB_BASE_MODE_WAVE, packet->mode);
    EXPECT_EQ(10, packet->r);
    EXPECT_EQ(20, packet->g);
    EXPECT_EQ(30, packet->b);
    EXPECT_EQ(40, packet->secondary_r);
    EXPECT_EQ(50, packet->secondary_g);
    EXPECT_EQ(60, packet->secondary_b);
    EXPECT_EQ(70, packet->speed);
    EXPECT_EQ(80, packet->direction);
    EXPECT_EQ(90, packet->density);
    EXPECT_EQ(100, packet->brightness);
}

TEST(Packet, SetAndGetRGBConfigs)
{
    std::array<uint8_t, 64> buffer = {};
    PacketRGBConfigs *packet = packet_as<PacketRGBConfigs>(buffer);
    packet->code = PACKET_CODE_SET;
    packet->type = PACKET_DATA_RGB_CONFIG;
    packet->length = 2;
    packet->data[0].index = 3;
    packet->data[0].mode = RGB_MODE_LINEAR;
    packet->data[0].r = 255;
    packet->data[0].g = 0;
    packet->data[0].b = 0;
    packet->data[0].speed = 20;
    packet->data[1].index = 5;
    packet->data[1].mode = RGB_MODE_FADING_DIAMOND_RIPPLE;
    packet->data[1].r = 0;
    packet->data[1].g = 255;
    packet->data[1].b = 0;
    packet->data[1].speed = 30;

    packet_process(buffer.data(), rgb_configs_packet_size(packet->length));

    uint16_t rgb_index = g_rgb_inverse_mapping[3];
    EXPECT_EQ(RGB_MODE_LINEAR, g_rgb_configs[rgb_index].mode);
    EXPECT_EQ(255, g_rgb_configs[rgb_index].rgb.r);
    EXPECT_EQ(20, g_rgb_configs[rgb_index].speed);

    rgb_index = g_rgb_inverse_mapping[5];
    EXPECT_EQ(RGB_MODE_FADING_DIAMOND_RIPPLE, g_rgb_configs[rgb_index].mode);
    EXPECT_EQ(255, g_rgb_configs[rgb_index].rgb.g);
    EXPECT_EQ(30, g_rgb_configs[rgb_index].speed);

    buffer.fill(0);
    packet = packet_as<PacketRGBConfigs>(buffer);
    packet->code = PACKET_CODE_GET;
    packet->type = PACKET_DATA_RGB_CONFIG;
    packet->length = 2;
    packet->data[0].index = 3;
    packet->data[1].index = 5;

    packet_process(buffer.data(), rgb_configs_packet_size(packet->length));

    EXPECT_EQ(3, packet->data[0].index);
    EXPECT_EQ(RGB_MODE_LINEAR, packet->data[0].mode);
    EXPECT_EQ(255, packet->data[0].r);
    EXPECT_EQ(20, packet->data[0].speed);
    EXPECT_EQ(5, packet->data[1].index);
    EXPECT_EQ(RGB_MODE_FADING_DIAMOND_RIPPLE, packet->data[1].mode);
    EXPECT_EQ(255, packet->data[1].g);
    EXPECT_EQ(30, packet->data[1].speed);
}

TEST(Packet, DynamicKeyUsesExplicitPayloadSize)
{
    const DynamicKey dynamic_key = make_dynamic_key();
    std::array<uint8_t, 64> buffer = {};
    PacketDynamicKey *packet = packet_as<PacketDynamicKey>(buffer);
    packet->code = PACKET_CODE_SET;
    packet->type = PACKET_DATA_DYNAMIC_KEY;
    packet->index = 1;
    std::memcpy(packet->dynamic_key, &dynamic_key, sizeof(dynamic_key));

    packet_process(buffer.data(), dynamic_key_packet_size());

    EXPECT_EQ(DYNAMIC_KEY_STROKE, g_dynamic_keys[1].type);
    EXPECT_EQ(dynamic_key.dks.press_fully_distance, g_dynamic_keys[1].dks.press_fully_distance);

    buffer.fill(0);
    packet = packet_as<PacketDynamicKey>(buffer);
    packet->code = PACKET_CODE_GET;
    packet->type = PACKET_DATA_DYNAMIC_KEY;
    packet->index = 1;

    packet_process(buffer.data(), dynamic_key_packet_size());

    EXPECT_EQ(0, std::memcmp(packet->dynamic_key, &dynamic_key, sizeof(dynamic_key)));
}

TEST(Packet, SetAndGetProfileIndex)
{
    std::array<uint8_t, 64> buffer = {};
    PacketProfileIndex *packet = packet_as<PacketProfileIndex>(buffer);
    packet->code = PACKET_CODE_SET;
    packet->type = PACKET_DATA_PROFILE_INDEX;
    packet->index = 2;

    packet_process(buffer.data(), sizeof(PacketProfileIndex));

    EXPECT_EQ(2, g_current_profile_index);

    packet->code = PACKET_CODE_GET;
    packet->index = 0;
    packet_process(buffer.data(), sizeof(PacketProfileIndex));

    EXPECT_EQ(2, packet->index);
}

TEST(Packet, SetAndGetKeyboardConfigBits)
{
    std::array<uint8_t, 64> buffer = {};
    PacketConfig *packet = packet_as<PacketConfig>(buffer);
    packet->code = PACKET_CODE_SET;
    packet->type = PACKET_DATA_CONFIG;
    packet->length = 2;
    packet->data[0].index = KEYBOARD_CONFIG_NKRO;
    packet->data[0].value = 1;
    packet->data[1].index = KEYBOARD_CONFIG_WINLOCK;
    packet->data[1].value = 1;

    packet_process(buffer.data(), config_packet_size(packet->length));

    EXPECT_TRUE(g_keyboard_config.nkro);
    EXPECT_TRUE(g_keyboard_config.winlock);

    packet->code = PACKET_CODE_GET;
    packet->data[0].value = 0;
    packet->data[1].value = 0;
    packet_process(buffer.data(), config_packet_size(packet->length));

    EXPECT_EQ(1, packet->data[0].value);
    EXPECT_EQ(1, packet->data[1].value);
}

TEST(Packet, GetDebugAndVersionResponses)
{
    g_keyboard_tick = 1234;
    g_keyboard_advanced_keys[2].raw = 111;
    g_keyboard_advanced_keys[2].value = 222;
    g_keyboard_advanced_keys[2].key.state = true;
    g_keyboard_advanced_keys[2].key.report_state = true;

    std::array<uint8_t, 64> buffer = {};
    PacketDebug *debug = packet_as<PacketDebug>(buffer);
    debug->code = PACKET_CODE_GET;
    debug->type = PACKET_DATA_DEBUG;
    debug->length = 1;
    debug->data[0].index = 2;

    packet_process(buffer.data(), debug_packet_size(debug->length));

    EXPECT_EQ(1234U, debug->tick);
    EXPECT_EQ(111, debug->data[0].raw);
    EXPECT_EQ(222, debug->data[0].value);
    EXPECT_TRUE(debug->data[0].state);
    EXPECT_TRUE(debug->data[0].report_state);

    buffer.fill(0);
    PacketVersion *version = packet_as<PacketVersion>(buffer);
    version->code = PACKET_CODE_GET;
    version->type = PACKET_DATA_VERSION;

    packet_process(buffer.data(), buffer.size());

    EXPECT_EQ(KEYBOARD_VERSION_MAJOR, version->major);
    EXPECT_EQ(KEYBOARD_VERSION_MINOR, version->minor);
    EXPECT_EQ(KEYBOARD_VERSION_PATCH, version->patch);
    EXPECT_EQ(sizeof(KEYBOARD_VERSION_INFO), version->info_length);
    EXPECT_EQ(0, std::memcmp(version->info, KEYBOARD_VERSION_INFO, sizeof(KEYBOARD_VERSION_INFO)));
}
