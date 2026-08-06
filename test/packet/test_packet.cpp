#include <gtest/gtest.h>

#include <array>
#include <cstring>

#include "amp_protocol.h"
#include "macro.h"
#include "packet.h"
#include "rgb.h"
#include "test_fixture.h"

namespace {

using Report = std::array<uint8_t, AMP_FRAME_REPORT_SIZE>;

AmpFrame transact(uint8_t code, uint8_t type, const void *body = nullptr,
                  uint8_t channel = AMP_CHANNEL_CONTROL, uint8_t seq = 7)
{
    AmpFrame request = {};
    request.header.proto = AMP_FRAME_PROTO;
    request.header.channel_flags =
        (uint8_t)((channel << 4) | AMP_FRAME_FLAG_REQ_ACK);
    request.header.seq = seq;
    request.header.code = code;
    request.header.type = type;
    if (body != nullptr)
    {
        std::memcpy(request.body, body, AMP_FRAME_BODY_SIZE);
    }
    std::memset(raw_send_buffer, 0, sizeof(raw_send_buffer));
    packet_process_frame(&request);

    AmpFrame response = {};
    EXPECT_TRUE(amp_frame_decode(raw_send_buffer, &response));
    return response;
}

AdvancedKeyConfiguration advanced_config()
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

} // namespace

TEST(AmpProtocol, EncodesAndDecodesFixedV3Frame)
{
    Report report = {};
    std::array<uint8_t, AMP_FRAME_BODY_SIZE> body = {};
    body[0] = 0x11;
    body[57] = 0x22;

    ASSERT_EQ(0, amp_frame_encode(report.data(), AMP_CHANNEL_DEBUG,
                                  AMP_FRAME_FLAG_REQ_ACK, 9, PACKET_CODE_GET,
                                  PACKET_DATA_DEBUG, AMP_STATUS_OK, body.data()));

    AmpFrame frame = {};
    ASSERT_TRUE(amp_frame_decode(report.data(), &frame));
    EXPECT_EQ(AMP_FRAME_PROTO, frame.header.proto);
    EXPECT_EQ(AMP_CHANNEL_DEBUG, amp_frame_channel(&frame.header));
    EXPECT_EQ(AMP_FRAME_FLAG_REQ_ACK, amp_frame_flags(&frame.header));
    EXPECT_EQ(9, frame.header.seq);
    EXPECT_EQ(PACKET_CODE_GET, frame.header.code);
    EXPECT_EQ(PACKET_DATA_DEBUG, frame.header.type);
    EXPECT_EQ(AMP_STATUS_OK, frame.header.status);
    EXPECT_EQ(0x11, frame.body[0]);
    EXPECT_EQ(0x22, frame.body[57]);
}

TEST(AmpProtocol, SessionResetDropsQueuedReports)
{
    std::array<uint8_t, AMP_FRAME_BODY_SIZE> body = {};
    raw_send_result = 1;

    ASSERT_EQ(0, amp_send_frame(AMP_CHANNEL_CONTROL, AMP_FRAME_FLAG_RESP,
                                9, PACKET_CODE_GET, PACKET_DATA_VERSION,
                                AMP_STATUS_OK, body.data(), false));
    ASSERT_EQ(0, amp_send_frame(AMP_CHANNEL_DEBUG, 0, 0, PACKET_CODE_GET,
                                PACKET_DATA_DEBUG, AMP_STATUS_OK,
                                body.data(), true));
    ASSERT_GT(raw_send_count, 0u);

    amp_transport_reset_session();
    raw_send_result = 0;
    raw_send_count = 0;
    amp_transport_kick();

    EXPECT_EQ(0u, raw_send_count);
}

TEST(PacketV3, SetAndGetKeymap)
{
    PacketKeymap set = {};
    set.layer = 1;
    set.start = 3;
    set.count = 5;
    const uint16_t expected[] = {KEY_A, KEY_B, KEY_C, KEY_D, KEY_E};
    std::memcpy(set.keycodes, expected, sizeof(expected));

    AmpFrame response = transact(PACKET_CODE_SET, PACKET_DATA_KEYMAP, &set);
    ASSERT_EQ(AMP_STATUS_OK, response.header.status);
    EXPECT_EQ(KEY_A, g_keymap[1][3]);
    EXPECT_EQ(KEY_E, g_keymap[1][7]);

    PacketKeymap get = {};
    get.layer = 1;
    get.start = 3;
    get.count = 5;
    response = transact(PACKET_CODE_GET, PACKET_DATA_KEYMAP, &get);
    ASSERT_EQ(AMP_STATUS_OK, response.header.status);
    const PacketKeymap *out = reinterpret_cast<const PacketKeymap *>(response.body);
    EXPECT_EQ(5, out->count);
    EXPECT_EQ(0, std::memcmp(out->keycodes, expected, sizeof(expected)));
}

TEST(PacketV3, RejectsOversizedKeymapCount)
{
    PacketKeymap body = {};
    body.layer = 0;
    body.count = PACKET_KEYMAP_ITEMS + 1;
    AmpFrame response = transact(PACKET_CODE_GET, PACKET_DATA_KEYMAP, &body);
    EXPECT_EQ(AMP_STATUS_INVALID_ARGUMENT, response.header.status);
}

TEST(PacketV3, VersionNotificationIsDeferredUntilPoll)
{
    std::memset(raw_send_buffer, 0, sizeof(raw_send_buffer));
    packet_send_version_packet();
    AmpFrame frame = {};
    EXPECT_FALSE(amp_frame_decode(raw_send_buffer, &frame));

    packet_process_version_notifications();
    ASSERT_TRUE(amp_frame_decode(raw_send_buffer, &frame));
    EXPECT_EQ(0, frame.header.seq);
    EXPECT_EQ(0, amp_frame_flags(&frame.header));
    EXPECT_EQ(PACKET_DATA_VERSION, frame.header.type);
    const PacketVersion *version = reinterpret_cast<const PacketVersion *>(frame.body);
    EXPECT_EQ(KEYBOARD_VERSION_MAJOR, version->major);
    EXPECT_EQ(KEYBOARD_VERSION_MINOR, version->minor);
}

TEST(PacketV3, BatchesAdvancedKeys)
{
    PacketAdvancedKeys set = {};
    set.count = 2;
    set.items[0].index = 3;
    set.items[0].config = advanced_config();
    set.items[1].index = 4;
    set.items[1].config = advanced_config();
    set.items[1].config.activation_value = 1234;

    AmpFrame response = transact(PACKET_CODE_SET, PACKET_DATA_ADVANCED_KEY, &set);
    ASSERT_EQ(AMP_STATUS_OK, response.header.status);
    EXPECT_EQ(set.items[0].config.mode, g_keyboard_advanced_keys[3].config.mode);
    EXPECT_EQ(set.items[0].config.activation_value,
              g_keyboard_advanced_keys[3].config.activation_value);
    //EXPECT_EQ(set.items[0].config.calibration_mode,
    //          g_keyboard_advanced_keys[3].config.calibration_mode);
    //EXPECT_EQ(set.items[0].config.upper_bound,
    //          g_keyboard_advanced_keys[3].config.upper_bound);
    //EXPECT_EQ(set.items[0].config.lower_bound,
    //          g_keyboard_advanced_keys[3].config.lower_bound);
    EXPECT_EQ(1234, g_keyboard_advanced_keys[4].config.activation_value);

    PacketAdvancedKeys get = {};
    get.count = 2;
    get.items[0].index = 3;
    get.items[1].index = 4;
    response = transact(PACKET_CODE_GET, PACKET_DATA_ADVANCED_KEY, &get);
    ASSERT_EQ(AMP_STATUS_OK, response.header.status);
    const PacketAdvancedKeys *out =
        reinterpret_cast<const PacketAdvancedKeys *>(response.body);
    EXPECT_EQ(2, out->count);
    EXPECT_EQ(3, out->items[0].index);
    EXPECT_EQ(1234, out->items[1].config.activation_value);
}

TEST(PacketV3, RejectsOutOfRangeAdvancedKeyWithoutMutation)
{
    AdvancedKeyConfiguration original = g_keyboard_advanced_keys[0].config;
    PacketAdvancedKeys set = {};
    set.count = 1;
    set.items[0].index = ADVANCED_KEY_NUM;
    set.items[0].config = advanced_config();
    AmpFrame response = transact(PACKET_CODE_SET, PACKET_DATA_ADVANCED_KEY, &set);
    EXPECT_EQ(AMP_STATUS_INVALID_ARGUMENT, response.header.status);
    EXPECT_EQ(0, std::memcmp(&original, &g_keyboard_advanced_keys[0].config,
                             sizeof(original)));
}

TEST(PacketV3, SetAndGetRgb)
{
    PacketRgbBaseConfig base = {};
    base.mode = RGB_BASE_MODE_WAVE;
    base.r = 10;
    base.g = 20;
    base.b = 30;
    base.secondary_r = 40;
    base.secondary_g = 50;
    base.secondary_b = 60;
    base.speed = 70;
    base.direction = 80;
    base.density = 90;
    base.brightness = 100;
    ASSERT_EQ(AMP_STATUS_OK,
              transact(PACKET_CODE_SET, PACKET_DATA_RGB_BASE_CONFIG, &base)
                  .header.status);

    AmpFrame response =
        transact(PACKET_CODE_GET, PACKET_DATA_RGB_BASE_CONFIG, nullptr);
    const PacketRgbBaseConfig *base_out =
        reinterpret_cast<const PacketRgbBaseConfig *>(response.body);
    EXPECT_EQ(10, base_out->r);
    EXPECT_EQ(100, base_out->brightness);

    PacketRgbItems items = {};
    items.count = 2;
    items.items[0] = {3, RGB_MODE_LINEAR, 255, 0, 0, 20};
    items.items[1] = {5, RGB_MODE_FADING_DIAMOND_RIPPLE, 0, 255, 0, 30};
    ASSERT_EQ(AMP_STATUS_OK,
              transact(PACKET_CODE_SET, PACKET_DATA_RGB_CONFIG, &items)
                  .header.status);
    PacketRgbItems get = {};
    get.count = 2;
    get.items[0].index = 3;
    get.items[1].index = 5;
    response = transact(PACKET_CODE_GET, PACKET_DATA_RGB_CONFIG, &get);
    const PacketRgbItems *out = reinterpret_cast<const PacketRgbItems *>(response.body);
    EXPECT_EQ(255, out->items[0].r);
    EXPECT_EQ(255, out->items[1].g);
}

TEST(PacketV3, DynamicKeyCopiesNativeStructure)
{
    std::array<uint8_t, AMP_FRAME_BODY_SIZE> set = {};
    const uint16_t index = 1;
    DynamicKey dynamic_key = {};
    dynamic_key.dks.type = DYNAMIC_KEY_STROKE;
    dynamic_key.dks.key_binding[0] = KEY_A;
    dynamic_key.dks.key_binding[1] = KEY_B;
    dynamic_key.dks.press_begin_distance = A_ANTI_NORM(0.25);
    dynamic_key.dks.press_fully_distance = A_ANTI_NORM(0.75);
    dynamic_key.dks.key_id = 6;
    dynamic_key.dks.filtered_value = 1234;
    dynamic_key.dks.key_end_tick[2] = 0x12345678;
    dynamic_key.dks.key_state = 1;
    std::memcpy(set.data(), &index, sizeof(index));
    std::memcpy(set.data() + PACKET_DYNAMIC_KEY_DATA_OFFSET,
                &dynamic_key, sizeof(dynamic_key));
    ASSERT_EQ(AMP_STATUS_OK,
              transact(PACKET_CODE_SET, PACKET_DATA_DYNAMIC_KEY, set.data())
                  .header.status);
    EXPECT_EQ(DYNAMIC_KEY_STROKE, g_dynamic_keys[1].type);
    EXPECT_EQ(dynamic_key.dks.press_fully_distance,
              g_dynamic_keys[1].dks.press_fully_distance);
    EXPECT_EQ(1234, g_dynamic_keys[1].dks.filtered_value);
    EXPECT_EQ(0x12345678U, g_dynamic_keys[1].dks.key_end_tick[2]);
    EXPECT_EQ(1, g_dynamic_keys[1].dks.key_state);

    std::array<uint8_t, AMP_FRAME_BODY_SIZE> get = {};
    std::memcpy(get.data(), &index, sizeof(index));
    AmpFrame response =
        transact(PACKET_CODE_GET, PACKET_DATA_DYNAMIC_KEY, get.data());
    uint16_t response_index;
    DynamicKey out = {};
    std::memcpy(&response_index, response.body, sizeof(response_index));
    std::memcpy(&out, response.body + PACKET_DYNAMIC_KEY_DATA_OFFSET,
                sizeof(out));
    EXPECT_EQ(index, response_index);
    EXPECT_EQ(DYNAMIC_KEY_STROKE, out.type);
    EXPECT_EQ(KEY_A, out.dks.key_binding[0]);
    EXPECT_EQ(6, out.dks.key_id);
    EXPECT_EQ(1234, out.dks.filtered_value);
    EXPECT_EQ(0x12345678U, out.dks.key_end_tick[2]);
    EXPECT_EQ(1, out.dks.key_state);
}

TEST(PacketV3, SetAndGetProfileAndConfig)
{
    PacketProfileIndex profile = {};
    profile.index = 2;
    g_keyboard_config.debug = true;
    g_keyboard_config.console = true;
    ASSERT_EQ(AMP_STATUS_OK,
              transact(PACKET_CODE_SET, PACKET_DATA_PROFILE_INDEX, &profile)
                  .header.status);
    EXPECT_FALSE(g_keyboard_config.debug);
    EXPECT_FALSE(g_keyboard_config.console);
    AmpFrame response =
        transact(PACKET_CODE_GET, PACKET_DATA_PROFILE_INDEX, nullptr);
    EXPECT_EQ(2, reinterpret_cast<const PacketProfileIndex *>(response.body)->index);

    PacketConfig config = {};
    config.count = 2;
    config.items[0] = {KEYBOARD_CONFIG_NKRO, 1};
    config.items[1] = {KEYBOARD_CONFIG_WINLOCK, 1};
    ASSERT_EQ(AMP_STATUS_OK,
              transact(PACKET_CODE_SET, PACKET_DATA_CONFIG, &config)
                  .header.status);
    EXPECT_TRUE(g_keyboard_config.nkro);
    EXPECT_TRUE(g_keyboard_config.winlock);

    config.items[0].value = 0;
    config.items[1].value = 0;
    response = transact(PACKET_CODE_GET, PACKET_DATA_CONFIG, &config);
    const PacketConfig *out = reinterpret_cast<const PacketConfig *>(response.body);
    EXPECT_EQ(1u << KEYBOARD_CONFIG_NKRO, out->items[0].value);
    EXPECT_EQ(1u << KEYBOARD_CONFIG_WINLOCK, out->items[1].value);
}

TEST(PacketV3, EventPreservesPhysicalReportState)
{
    Key *key = keyboard_get_key(2);
    ASSERT_NE(nullptr, key);
    key->report_state = true;
    g_keyboard_report_flags.raw = 0;

    PacketEvent event = {};
    event.event = KEYBOARD_EVENT_KEY_DOWN;
    event.keycode = KEY_A;
    event.id = 2;
    event.is_virtual = false;
    AmpFrame response = transact(PACKET_CODE_EVENT, 0, &event);
    EXPECT_EQ(AMP_STATUS_OK, response.header.status);
    EXPECT_TRUE(key->report_state);
}

TEST(PacketV3, SetAndGetMacroActions)
{
    PacketMacro set = {};
    set.macro_index = 0;
    set.count = 2;
    set.items[0] = {10, 0, 3, 0, KEYBOARD_EVENT_KEY_DOWN, KEY_A};
    set.items[1] = {20, 1, 4, 1, KEYBOARD_EVENT_KEY_UP, KEY_B};
    ASSERT_EQ(AMP_STATUS_OK,
              transact(PACKET_CODE_SET, PACKET_DATA_MACRO, &set).header.status);
    EXPECT_EQ(10u, g_macros[0].actions[0].delay);
    EXPECT_EQ(KEY_A, g_macros[0].actions[0].event.keycode);

    PacketMacro get = {};
    get.macro_index = 0;
    get.count = 2;
    get.items[0].index = 0;
    get.items[1].index = 1;
    AmpFrame response = transact(PACKET_CODE_GET, PACKET_DATA_MACRO, &get);
    const PacketMacro *out = reinterpret_cast<const PacketMacro *>(response.body);
    EXPECT_EQ(10u, out->items[0].delay);
    EXPECT_EQ(KEY_B, out->items[1].keycode);
    EXPECT_TRUE(out->items[1].is_virtual);
}

TEST(PacketV3, DebugAndVersionResponsesUseFixedBodies)
{
    g_keyboard_tick = 1234;
    g_keyboard_advanced_keys[2].raw = 111;
    g_keyboard_advanced_keys[2].value = 222;
    PacketDebug debug = {};
    debug.count = 1;
    debug.items[0].index = 2;
    AmpFrame response = transact(PACKET_CODE_GET, PACKET_DATA_DEBUG, &debug,
                                 AMP_CHANNEL_DEBUG);
    const PacketDebug *debug_out =
        reinterpret_cast<const PacketDebug *>(response.body);
    EXPECT_EQ(1234u, debug_out->tick);
    EXPECT_EQ(111, debug_out->items[0].raw);
    EXPECT_EQ(222, debug_out->items[0].value);

    response = transact(PACKET_CODE_GET, PACKET_DATA_VERSION, nullptr);
    const PacketVersion *version =
        reinterpret_cast<const PacketVersion *>(response.body);
    EXPECT_EQ(KEYBOARD_VERSION_MAJOR, version->major);
    EXPECT_EQ(KEYBOARD_VERSION_PATCH, version->patch);
    EXPECT_EQ(sizeof(KEYBOARD_VERSION_INFO), version->info_length);
}

TEST(PacketV3, FeatureIsExplicitlyUnsupported)
{
    AmpFrame response = transact(PACKET_CODE_GET, PACKET_DATA_FEATURE, nullptr);
    EXPECT_EQ(AMP_STATUS_UNSUPPORTED, response.header.status);
}

TEST(PacketV3, ResponsePreservesRequestTuple)
{
    AmpFrame response = transact(PACKET_CODE_GET, PACKET_DATA_VERSION, nullptr,
                                 AMP_CHANNEL_CONTROL, 42);
    EXPECT_EQ(AMP_FRAME_FLAG_RESP, amp_frame_flags(&response.header));
    EXPECT_EQ(42, response.header.seq);
    EXPECT_EQ(PACKET_CODE_GET, response.header.code);
    EXPECT_EQ(PACKET_DATA_VERSION, response.header.type);
    EXPECT_EQ(AMP_STATUS_OK, response.header.status);
}
