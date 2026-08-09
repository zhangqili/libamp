#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstring>

#include "macro.h"
#include "packet.h"
#include "packet_buffer.h"
#include "rgb.h"
#include "test_fixture.h"

namespace {

using PacketBuffer = std::array<uint8_t, 64>;

template <typename T>
T *packet_as(PacketBuffer& buffer)
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

size_t macro_packet_size(uint8_t length)
{
    return offsetof(PacketMacro, data) + length * sizeof(reinterpret_cast<PacketMacro *>(0)->data[0]);
}

Keycode collection_keycode(uint8_t collection, uint8_t subcode)
{
    return ((Keycode)subcode << 8) | collection;
}

// v2 协议：packet_process 处理后的包会被原样回显到 USB packet_buffer，
// flush 后 raw_send_buffer 中即为处理后的完整 v2 包（code, id, type, body...）。
void expect_raw_echo_matches_packet(const PacketBuffer& expected, uint16_t expected_len)
{
    packet_buffer_flush();
    ASSERT_EQ(0, std::memcmp(raw_send_buffer, expected.data(), expected_len));
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
    PacketBuffer buffer = {};
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
    expect_raw_echo_matches_packet(buffer, keymap_packet_size(packet->length));

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

    expect_raw_echo_matches_packet(buffer, keymap_packet_size(packet->length));
}

TEST(Packet, VersionPacketEchoesThroughDataBuffer)
{
    packet_send_version_packet();
    packet_buffer_flush();

    // v2：版本通知直接以 GET+Version 包推送到 USB 数据缓冲
    EXPECT_EQ(PACKET_CODE_GET, raw_send_buffer[0]);
    EXPECT_EQ(PACKET_DATA_VERSION, raw_send_buffer[2]);
    EXPECT_GT(raw_send_buffer[3] | (raw_send_buffer[4] << 8), 0u);
}

TEST(Packet, SetAndGetAdvancedKey)
{
    PacketBuffer buffer = {};
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
    PacketBuffer buffer = {};
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
    PacketBuffer buffer = {};
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
    PacketBuffer buffer = {};
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
    PacketBuffer buffer = {};
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
    PacketBuffer buffer = {};
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
    PacketBuffer buffer = {};
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

TEST(Packet, EventPhysicalPreservesReportState)
{
    Key* key = keyboard_get_key(2);
    ASSERT_NE(nullptr, key);
    key->report_state = true;
    g_keyboard_report_flags.raw = 0;

    PacketBuffer buffer = {};
    PacketEvent* packet = packet_as<PacketEvent>(buffer);
    packet->code = PACKET_CODE_EVENT;
    packet->event = KEYBOARD_EVENT_KEY_UP;
    packet->keycode = KEY_A;
    packet->id = 2;
    packet->is_virtual = false;
    packet->use_keymap = false;

    packet_process(buffer.data(), sizeof(PacketEvent));

    EXPECT_TRUE(key->report_state);
    EXPECT_TRUE((bool)g_keyboard_report_flags.keyboard);
}

TEST(Packet, EventVirtualUsesExplicitKeycode)
{
    g_keyboard_report_flags.raw = 0;

    PacketBuffer buffer = {};
    PacketEvent* packet = packet_as<PacketEvent>(buffer);
    packet->code = PACKET_CODE_EVENT;
    packet->event = KEYBOARD_EVENT_KEY_DOWN;
    packet->keycode = collection_keycode(MOUSE_COLLECTION, MOUSE_LBUTTON);
    packet->id = 0;
    packet->is_virtual = true;
    packet->use_keymap = false;

    packet_process(buffer.data(), sizeof(PacketEvent));

    EXPECT_TRUE((bool)g_keyboard_report_flags.mouse);
    EXPECT_FALSE((bool)g_keyboard_report_flags.keyboard);
}

TEST(Packet, SetAndGetMacroActions)
{
    PacketBuffer buffer = {};
    PacketMacro* packet = packet_as<PacketMacro>(buffer);
    packet->code = PACKET_CODE_SET;
    packet->type = PACKET_DATA_MACRO;
    packet->macro_index = 0;
    packet->length = 2;
    packet->data[0].index = 0;
    packet->data[0].delay = 10;
    packet->data[0].key_id = 3;
    packet->data[0].is_virtual = false;
    packet->data[0].event = KEYBOARD_EVENT_KEY_DOWN;
    packet->data[0].keycode = KEY_A;
    packet->data[1].index = 1;
    packet->data[1].delay = 20;
    packet->data[1].key_id = 4;
    packet->data[1].is_virtual = true;
    packet->data[1].event = KEYBOARD_EVENT_KEY_UP;
    packet->data[1].keycode = KEY_B;

    packet_process(buffer.data(), macro_packet_size(packet->length));

    EXPECT_EQ(10u, g_macros[0].actions[0].delay);
    EXPECT_EQ(KEY_A, g_macros[0].actions[0].event.keycode);
    EXPECT_EQ(keyboard_get_key(3), g_macros[0].actions[0].event.key);
    EXPECT_FALSE(g_macros[0].actions[0].event.is_virtual);
    EXPECT_TRUE(g_macros[0].actions[1].event.is_virtual);

    buffer.fill(0);
    packet = packet_as<PacketMacro>(buffer);
    packet->code = PACKET_CODE_GET;
    packet->type = PACKET_DATA_MACRO;
    packet->macro_index = 0;
    packet->length = 2;
    packet->data[0].index = 0;
    packet->data[1].index = 1;

    packet_process(buffer.data(), macro_packet_size(packet->length));

    EXPECT_EQ(10u, packet->data[0].delay);
    EXPECT_EQ(3, packet->data[0].key_id);
    EXPECT_EQ(KEYBOARD_EVENT_KEY_DOWN, packet->data[0].event);
    EXPECT_EQ(KEY_A, packet->data[0].keycode);
    EXPECT_EQ(20u, packet->data[1].delay);
    EXPECT_EQ(4, packet->data[1].key_id);
    EXPECT_EQ(KEYBOARD_EVENT_KEY_UP, packet->data[1].event);
    EXPECT_EQ(KEY_B, packet->data[1].keycode);
    EXPECT_TRUE(packet->data[1].is_virtual);
}

TEST(Packet, GetDebugAndVersionResponses)
{
    constexpr uint32_t kDebugTick = 1234;
    g_keyboard_tick = kDebugTick;
    g_keyboard_advanced_keys[2].raw = 111;
    g_keyboard_advanced_keys[2].value = 222;
    g_keyboard_advanced_keys[2].key.state = true;
    g_keyboard_advanced_keys[2].key.report_state = true;

    // v2 Debug：独立 PacketCode(0x06) 订阅，数据经固件推流路径（packet_fill_debug）回填
    PacketBuffer buffer = {};
    PacketDebug *debug = packet_as<PacketDebug>(buffer);
    debug->code = PACKET_CODE_DEBUG;
    debug->length = 1;
    debug->data[0].index = 2;

    packet_process(buffer.data(), debug_packet_size(debug->length));

    packet_send_debug_packet();
    packet_buffer_flush();

    // 推流包：code(0) length(1) tick(2-5) item(6+)
    // item: index(6-7) state(8) report_state(9) raw(10-11) filtered_raw(12-13) value(14-15)
    EXPECT_EQ(PACKET_CODE_DEBUG, raw_send_buffer[0]);
    EXPECT_EQ(1, raw_send_buffer[1]);
    EXPECT_EQ(kDebugTick, raw_send_buffer[2] | (raw_send_buffer[3] << 8) |
                              (raw_send_buffer[4] << 16) | (raw_send_buffer[5] << 24));
    EXPECT_EQ(2, raw_send_buffer[6] | (raw_send_buffer[7] << 8));
    EXPECT_EQ(1, raw_send_buffer[8]);
    EXPECT_EQ(1, raw_send_buffer[9]);
    EXPECT_EQ(111, raw_send_buffer[10] | (raw_send_buffer[11] << 8));
    EXPECT_EQ(222, raw_send_buffer[14] | (raw_send_buffer[15] << 8));

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
