#include <gtest/gtest.h>

#include <cstring>

extern "C" {
#include "nexus.h"
#include "packet.h"
}

extern "C" {
const uint16_t g_nexus_test_slave_map[] = {2, 5, 8};
NexusSlaveKeymap g_nexus_slave_configs[NEXUS_SLAVE_NUM] = {
    {3, g_nexus_test_slave_map},
};
}

namespace {

struct CapturedNexusPacket {
    uint8_t slave_id;
    PacketAdvancedKey packet;
};

CapturedNexusPacket captured_packets[8];
size_t captured_packet_count;
bool captured_decode_ok;
bool synthesize_version_response;
bool corrupt_response_id;

void reset_capture()
{
    std::memset(captured_packets, 0, sizeof(captured_packets));
    std::memset(g_nexus_slave_buffer, 0, sizeof(g_nexus_slave_buffer));
    captured_packet_count = 0;
    captured_decode_ok = true;
    synthesize_version_response = false;
    corrupt_response_id = false;
    g_keyboard_tick = 0;
}

void set_test_config(uint16_t key_index)
{
    AdvancedKeyConfiguration *config = &g_keyboard_advanced_keys[key_index].config;
    std::memset(config, 0, sizeof(*config));
    config->mode = ADVANCED_KEY_ANALOG_RAPID_MODE;
    config->activation_value = 1234;
    config->deactivation_value = 567;
    config->trigger_distance = 42;
    config->release_distance = 24;
    config->upper_deadzone = 100;
    config->lower_deadzone = 200;
}

} // namespace

extern "C" int nexus_send(uint8_t slave_id, uint8_t *report, uint16_t len)
{
    // v2 协议：板间直接传输原始 packet（code(0) id(1) type(2) body(3...)）
    if (report == NULL || len == 0 || len > NEXUS_RX_BUFFER_SIZE)
    {
        captured_decode_ok = false;
        return 1;
    }

    if (captured_packet_count < sizeof(captured_packets) / sizeof(captured_packets[0]))
    {
        CapturedNexusPacket *captured = &captured_packets[captured_packet_count++];
        captured->slave_id = slave_id;
        std::memset(&captured->packet, 0, sizeof(captured->packet));
        const size_t capture_len = len < sizeof(captured->packet) ? len : sizeof(captured->packet);
        std::memcpy(&captured->packet, report, capture_len);
    }

    // 模拟从机回显：把请求（含事务 id）写回响应缓冲
    std::memset(g_nexus_slave_buffer[slave_id], 0, NEXUS_BUFFER_SIZE);
    std::memcpy(g_nexus_slave_buffer[slave_id], report, len);
    if (synthesize_version_response && report[2] == PACKET_DATA_VERSION)
    {
        PacketVersion *version = reinterpret_cast<PacketVersion *>(g_nexus_slave_buffer[slave_id]);
        version->info_length = 4;
        version->major = 1;
        version->minor = 2;
        version->patch = 3;
        std::memcpy(version->info, "test", 4);
    }
    if (corrupt_response_id)
    {
        g_nexus_slave_buffer[slave_id][1]++;
        g_keyboard_tick = g_keyboard_tick + 2;
    }
    return 0;
}

TEST(NexusRequest, AssignsIdAndCopiesRawResponse)
{
    reset_capture();
    uint8_t request[64] = {0};
    uint8_t response[64] = {0};
    request[0] = PACKET_CODE_GET;
    request[2] = PACKET_DATA_CONFIG;
    request[63] = 0xA5;

    ASSERT_EQ(0, nexus_request_timeout(0, request, sizeof(request), 1,
                                       response, sizeof(response)));
    EXPECT_NE(0, response[1]);
    EXPECT_EQ(PACKET_CODE_GET, response[0]);
    EXPECT_EQ(PACKET_DATA_CONFIG, response[2]);
    EXPECT_EQ(0xA5, response[63]);
    EXPECT_EQ(0, g_nexus_slave_buffer[0][0]);
}

TEST(NexusRequest, RejectsResponseWithWrongId)
{
    reset_capture();
    corrupt_response_id = true;
    uint8_t request[64] = {0};
    uint8_t response[64] = {0};
    request[0] = PACKET_CODE_GET;
    request[2] = PACKET_DATA_CONFIG;

    EXPECT_EQ(1, nexus_request_timeout(0, request, sizeof(request), 2,
                                       response, sizeof(response)));
}

TEST(NexusRequest, CopiesVersionResponse)
{
    reset_capture();
    synthesize_version_response = true;
    uint8_t request[64] = {0};
    uint8_t response[64] = {0};
    request[0] = PACKET_CODE_GET;
    request[2] = PACKET_DATA_VERSION;

    ASSERT_EQ(0, nexus_request_timeout(0, request, sizeof(request), 1,
                                       response, sizeof(response)));
    const PacketVersion *version = reinterpret_cast<const PacketVersion *>(response);
    EXPECT_NE(0, version->id);
    EXPECT_EQ(PACKET_DATA_VERSION, version->type);
    EXPECT_EQ(4, version->info_length);
    EXPECT_EQ(1u, version->major);
    EXPECT_EQ(2u, version->minor);
    EXPECT_EQ(3u, version->patch);
    EXPECT_EQ(0, std::memcmp(version->info, "test", 4));
}

TEST(NexusRequest, EventDoesNotConsumeTransactionIdField)
{
    reset_capture();
    PacketEvent event = {};
    event.code = PACKET_CODE_EVENT;
    event.flag = PACKET_EVENT_CONFIG_CHANGED;

    ASSERT_EQ(0, nexus_send_timeout(0, reinterpret_cast<uint8_t *>(&event),
                                    sizeof(event), 1));
    ASSERT_EQ(1u, captured_packet_count);
    EXPECT_EQ(PACKET_CODE_EVENT, captured_packets[0].packet.code);
    EXPECT_EQ(PACKET_EVENT_CONFIG_CHANGED, captured_packets[0].packet.id);
}

TEST(NexusConfigSync, SendsMappedKeyUsingSlaveLocalIndex)
{
    reset_capture();
    set_test_config(5);

    EXPECT_EQ(0, nexus_sync_advanced_key_config(5));

    ASSERT_TRUE(captured_decode_ok);
    ASSERT_EQ(1u, captured_packet_count);
    EXPECT_EQ(0u, captured_packets[0].slave_id);
    EXPECT_EQ(PACKET_CODE_SET, captured_packets[0].packet.code);
    EXPECT_EQ(PACKET_DATA_ADVANCED_KEY, captured_packets[0].packet.type);
    EXPECT_EQ(1u, captured_packets[0].packet.index);
    EXPECT_EQ(g_keyboard_advanced_keys[5].config.mode, captured_packets[0].packet.data.mode);
    EXPECT_EQ(g_keyboard_advanced_keys[5].config.activation_value, captured_packets[0].packet.data.activation_value);
    EXPECT_EQ(g_keyboard_advanced_keys[5].config.deactivation_value, captured_packets[0].packet.data.deactivation_value);
}

TEST(NexusConfigSync, SkipsKeysNotMappedToSlaves)
{
    reset_capture();

    EXPECT_EQ(0, nexus_sync_advanced_key_config(4));

    ASSERT_TRUE(captured_decode_ok);
    EXPECT_EQ(0u, captured_packet_count);
}

TEST(NexusConfigSync, InitSendsAllSlaveLocalConfigs)
{
    reset_capture();
    set_test_config(2);
    set_test_config(5);
    set_test_config(8);

    nexus_init();

    ASSERT_TRUE(captured_decode_ok);
    ASSERT_EQ(3u, captured_packet_count);
    EXPECT_EQ(0u, captured_packets[0].packet.index);
    EXPECT_EQ(1u, captured_packets[1].packet.index);
    EXPECT_EQ(2u, captured_packets[2].packet.index);
    EXPECT_EQ(g_keyboard_advanced_keys[2].config.activation_value, captured_packets[0].packet.data.activation_value);
    EXPECT_EQ(g_keyboard_advanced_keys[5].config.activation_value, captured_packets[1].packet.data.activation_value);
    EXPECT_EQ(g_keyboard_advanced_keys[8].config.activation_value, captured_packets[2].packet.data.activation_value);
}
