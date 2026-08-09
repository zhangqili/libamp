#include <gtest/gtest.h>

#include <cstring>

extern "C" {
#include "nexus.h"
#include "packet.h"
}

extern "C" {
const uint16_t g_nexus_test_slave_map[] = {2, 5, 8};
NexusSlaveConfig g_nexus_slave_configs[NEXUS_SLAVE_NUM] = {
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

void reset_capture()
{
    std::memset(captured_packets, 0, sizeof(captured_packets));
    std::memset(g_nexus_slave_buffer, 0, sizeof(g_nexus_slave_buffer));
    captured_packet_count = 0;
    captured_decode_ok = true;
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
    if (report == NULL || len == 0 || len > sizeof(PacketAdvancedKey))
    {
        captured_decode_ok = false;
        return 1;
    }

    if (captured_packet_count < sizeof(captured_packets) / sizeof(captured_packets[0]))
    {
        CapturedNexusPacket *captured = &captured_packets[captured_packet_count++];
        captured->slave_id = slave_id;
        std::memset(&captured->packet, 0, sizeof(captured->packet));
        std::memcpy(&captured->packet, report, len);
    }

    // 模拟从机回显：把请求（含事务 id）写回响应缓冲
    std::memset(g_nexus_slave_buffer[slave_id], 0, NEXUS_BUFFER_SIZE);
    std::memcpy(g_nexus_slave_buffer[slave_id], report, NEXUS_BUFFER_SIZE);
    return 0;
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
