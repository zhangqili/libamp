#include <gtest/gtest.h>

#include <cstring>

#include "keyboard.h"
#include "nexus.h"
#include "test_fixture.h"

// This target builds nexus.c as a master with NEXUS_USE_RAW=1: slaves stream
// raw samples and the master normalizes them.

extern "C" {
// Slot 3 points past the master's advanced keys and must be ignored.
const uint16_t g_nexus_test_slave_map[] = {2, 5, 8, 0xFFF0};
NexusSlaveKeymap g_nexus_slave_configs[NEXUS_SLAVE_NUM] = {
    {4, g_nexus_test_slave_map},
};

int nexus_send(uint8_t slave_id, uint8_t *report, uint16_t len)
{
    (void)slave_id;
    (void)report;
    (void)len;
    return 0;
}
}

namespace {

constexpr uint32_t kLinkTimeoutTicks = KEYBOARD_TIME_TO_TICK(NEXUS_LINK_TIMEOUT_MS);

class NexusRawTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        libamp_test_reset_environment();
        for (uint16_t i = 0; i < ADVANCED_KEY_NUM; i++)
        {
            g_keyboard_advanced_keys[i].config.calibration_mode = ADVANCED_KEY_NO_CALIBRATION;
            g_keyboard_advanced_keys[i].config.mode = ADVANCED_KEY_ANALOG_NORMAL_MODE;
            advanced_key_set_range(&g_keyboard_advanced_keys[i], 4000, 1000);
        }
        nexus_init();
    }

    void deliver(const AnalogRawValue *raws, uint16_t count)
    {
        uint8_t frame[NEXUS_SLICE_LENGTH_MAX * 2];
        const uint16_t len = nexus_raw_report_encode(frame, raws, count);
        nexus_process_buffer(0, frame, len);
    }
};

} // namespace

TEST_F(NexusRawTest, SamplesLandOnTheMappedMasterKeys)
{
    const AnalogRawValue raws[] = {0x5ABC, 2000, 3000, 4000};

    deliver(raws, 4);
    nexus_process();

    EXPECT_EQ(0x5ABC, g_keyboard_advanced_keys[2].raw) << "first sample keeps 15 bits";
    EXPECT_EQ(2000, g_keyboard_advanced_keys[5].raw);
    EXPECT_EQ(3000, g_keyboard_advanced_keys[8].raw);
    EXPECT_EQ(0, g_keyboard_advanced_keys[4].raw) << "keys no slave owns are left to the application";
}

TEST_F(NexusRawTest, ShortFramesOnlyUpdateTheSamplesTheyCarry)
{
    const AnalogRawValue raws[] = {1111, 2222};

    deliver(raws, 2);
    nexus_process();

    EXPECT_EQ(1111, g_keyboard_advanced_keys[2].raw);
    EXPECT_EQ(2222, g_keyboard_advanced_keys[5].raw);
    EXPECT_EQ(0, g_keyboard_advanced_keys[8].raw);
    EXPECT_TRUE(nexus_slave_is_online(0));

    uint8_t one_byte = NEXUS_REPORT_FLAG;
    g_keyboard_tick += kLinkTimeoutTicks + 1;
    nexus_process_buffer(0, &one_byte, 1);
    EXPECT_FALSE(nexus_slave_is_online(0)) << "a frame without a full sample is not a report";
}

TEST_F(NexusRawTest, SilentSlaveReadsAsRestingKeys)
{
    const AnalogRawValue raws[] = {1000, 1000, 1000};

    deliver(raws, 3);
    nexus_process();
    EXPECT_TRUE(g_keyboard_advanced_keys[5].key.state) << "lower bound is full travel";

    g_keyboard_tick += kLinkTimeoutTicks + 1;
    nexus_process();
    EXPECT_EQ(g_keyboard_advanced_keys[5].config.upper_bound, g_keyboard_advanced_keys[5].raw);
    EXPECT_FALSE(g_keyboard_advanced_keys[5].key.state);
}
