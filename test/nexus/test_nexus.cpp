#include <gtest/gtest.h>

#include <cstring>

extern "C" {
#include "amp_protocol.h"
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
    AmpFrame frame;
    if (!amp_frame_decode(report, len, &frame) || frame.header.len > sizeof(PacketAdvancedKey) - 2)
    {
        captured_decode_ok = false;
        return 1;
    }

    if (captured_packet_count < sizeof(captured_packets) / sizeof(captured_packets[0]))
    {
        CapturedNexusPacket *captured = &captured_packets[captured_packet_count++];
        captured->slave_id = slave_id;
        std::memset(&captured->packet, 0, sizeof(captured->packet));
        captured->packet.code = frame.header.code;
        captured->packet.type = frame.header.type;
        std::memcpy(((uint8_t *)&captured->packet) + 2, frame.payload, frame.header.len);
    }

    AmpFrameHeader *response = (AmpFrameHeader *)g_nexus_slave_buffer[slave_id];
    response->proto = AMP_FRAME_PROTO;
    response->channel_flags = (uint8_t)((AMP_CHANNEL_NEXUS_CTRL << 4) | AMP_FRAME_FLAG_RESP);
    response->seq = frame.header.seq;
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
