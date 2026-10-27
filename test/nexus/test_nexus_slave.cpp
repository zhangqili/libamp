#include <gtest/gtest.h>

#include <cstring>

#include "keyboard.h"
#include "layer.h"
#include "nexus.h"
#include "packet.h"
#include "test_fixture.h"

// This target builds nexus.c with NEXUS_IS_SLAVE=1.

namespace {

uint8_t reported_frame[64];
uint16_t reported_len;
uint32_t report_count;

} // namespace

extern "C" int nexus_report(uint8_t *report, uint16_t len)
{
    report_count++;
    reported_len = len < sizeof(reported_frame) ? len : sizeof(reported_frame);
    std::memcpy(reported_frame, report, reported_len);
    return 0;
}

namespace {

class NexusSlaveTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        libamp_test_reset_environment();
        report_count = 0;
        reported_len = 0;
        std::memset(reported_frame, 0, sizeof(reported_frame));
    }
};

} // namespace

TEST_F(NexusSlaveTest, ReportCarriesBitmapAndCyclesThroughKeys)
{
    keyboard_key_set_report_state(&g_keyboard_advanced_keys[1].key, true);
    keyboard_key_set_report_state(&g_keyboard_advanced_keys[9].key, true);
    g_keyboard_advanced_keys[0].filtered_raw = 777;
    g_keyboard_advanced_keys[0].value = 4321;
    g_keyboard_advanced_keys[1].filtered_raw = 888;
    g_keyboard_advanced_keys[1].value = 1234;

    ASSERT_EQ(0, nexus_send_report());
    ASSERT_EQ(sizeof(PacketNexus), reported_len);
    const PacketNexus *packet = reinterpret_cast<const PacketNexus *>(reported_frame);
    EXPECT_EQ(0 | NEXUS_REPORT_FLAG, packet->index);
    EXPECT_EQ(777, packet->raw);
    EXPECT_EQ(nexus_value_to_wire(4321), packet->value);
    EXPECT_EQ(0x02, packet->bits[0]);
    EXPECT_EQ(0x02, packet->bits[1]);

    ASSERT_EQ(0, nexus_send_report());
    EXPECT_EQ(1 | NEXUS_REPORT_FLAG, packet->index);
    EXPECT_EQ(888, packet->raw);
    EXPECT_EQ(nexus_value_to_wire(1234), packet->value);
    EXPECT_EQ(2u, report_count);
}

TEST_F(NexusSlaveTest, IncomingPacketsAreProcessedLocally)
{
    uint8_t buffer[64] = {0};
    PacketKeymap *packet = reinterpret_cast<PacketKeymap *>(buffer);
    packet->header.code = PACKET_CODE_SET;
    packet->header.type = PACKET_DATA_KEYMAP;
    packet->layer = 0;
    packet->start = 3;
    packet->length = 1;
    packet->keymap[0] = KEY_Z;

    nexus_process_buffer(0, buffer, sizeof(buffer));

    EXPECT_EQ(KEY_Z, g_keymap[0][3]);
}

TEST_F(NexusSlaveTest, MasterOnlyEntryPointsAreInertOnASlave)
{
    nexus_init();
    nexus_process();
    nexus_poll();
    nexus_calibrate();
    EXPECT_EQ(1, nexus_sync_advanced_key_config(0));
    EXPECT_FALSE(nexus_slave_is_online(0));
    uint8_t request[8] = {PACKET_CODE_GET};
    EXPECT_EQ(1, nexus_request_timeout(0, request, sizeof(request), 1, NULL, 0));
    EXPECT_EQ(0u, report_count);
}
