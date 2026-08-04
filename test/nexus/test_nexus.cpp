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
    AmpFrame frame;
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
    if (len != AMP_FRAME_REPORT_SIZE || !amp_frame_decode(report, &frame))
    {
        captured_decode_ok = false;
        return 1;
    }

    if (captured_packet_count < sizeof(captured_packets) / sizeof(captured_packets[0]))
    {
        CapturedNexusPacket *captured = &captured_packets[captured_packet_count++];
        captured->slave_id = slave_id;
        captured->frame = frame;
    }

    AmpFrameHeader *response = (AmpFrameHeader *)g_nexus_slave_buffer[slave_id];
    response->proto = AMP_FRAME_PROTO;
    response->channel_flags = (uint8_t)((AMP_CHANNEL_NEXUS_CTRL << 4) | AMP_FRAME_FLAG_RESP);
    response->seq = frame.header.seq;
    response->code = frame.header.code;
    response->type = frame.header.type;
    response->status = AMP_STATUS_OK;
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
    EXPECT_EQ(PACKET_CODE_SET, captured_packets[0].frame.header.code);
    EXPECT_EQ(PACKET_DATA_ADVANCED_KEY, captured_packets[0].frame.header.type);
    const PacketAdvancedKeys *body =
        reinterpret_cast<const PacketAdvancedKeys *>(captured_packets[0].frame.body);
    ASSERT_EQ(1, body->count);
    EXPECT_EQ(1u, body->items[0].index);
    EXPECT_EQ(g_keyboard_advanced_keys[5].config.mode, body->items[0].config.mode);
    EXPECT_EQ(g_keyboard_advanced_keys[5].config.activation_value,
              body->items[0].config.activation_value);
    EXPECT_EQ(g_keyboard_advanced_keys[5].config.deactivation_value,
              body->items[0].config.deactivation_value);
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
    const PacketAdvancedKeys *body0 =
        reinterpret_cast<const PacketAdvancedKeys *>(captured_packets[0].frame.body);
    const PacketAdvancedKeys *body1 =
        reinterpret_cast<const PacketAdvancedKeys *>(captured_packets[1].frame.body);
    const PacketAdvancedKeys *body2 =
        reinterpret_cast<const PacketAdvancedKeys *>(captured_packets[2].frame.body);
    ASSERT_EQ(1, body0->count);
    ASSERT_EQ(1, body1->count);
    ASSERT_EQ(1, body2->count);
    EXPECT_EQ(0u, body0->items[0].index);
    EXPECT_EQ(1u, body1->items[0].index);
    EXPECT_EQ(2u, body2->items[0].index);
    EXPECT_EQ(g_keyboard_advanced_keys[2].config.activation_value,
              body0->items[0].config.activation_value);
    EXPECT_EQ(g_keyboard_advanced_keys[5].config.activation_value,
              body1->items[0].config.activation_value);
    EXPECT_EQ(g_keyboard_advanced_keys[8].config.activation_value,
              body2->items[0].config.activation_value);
}
