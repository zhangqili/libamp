#include <gtest/gtest.h>
#include <array>
#include <cstddef>
#include <cstring>

#include "packet.h"
#include "packet_buffer.h"
#include "record.h"
#include "test_fixture.h"

TEST(RuntimePacket, IncludesBootUptimeIndependentOfPersistedTotal)
{
    static_assert(offsetof(PacketRecordRuntime, uptime) == 4);
    static_assert(offsetof(PacketRecordRuntime, runtime) == 12);
    static_assert(sizeof(PacketRecordRuntime) == 20);
    libamp_test_set_tick(KEYBOARD_TIME_TO_TICK(12345));
    record_set_runtime(900000);
    std::array<uint8_t, 64> buffer = {};
    auto *packet = reinterpret_cast<PacketRecordRuntime *>(buffer.data());
    packet->header.code = PACKET_CODE_GET;
    packet->header.type = PACKET_DATA_RECORD;
    packet->sub_cmd = PACKET_DATA_RECORD_RUNTIME;
    packet_process(buffer.data(), buffer.size());
    EXPECT_EQ(900000u, packet->runtime);
    EXPECT_EQ(12345u, packet->uptime);
    packet_buffer_flush();
    EXPECT_EQ(0, std::memcmp(raw_send_buffer, buffer.data(), buffer.size()));

    // Clearing the persisted total does not reset keyboard_tick_get32().
    record_set_runtime(0);
    packet_process(buffer.data(), buffer.size());
    EXPECT_EQ(0u, packet->runtime);
    EXPECT_EQ(12345u, packet->uptime);
}

TEST(RuntimePacket, ZeroTickIsAValidUptime)
{
    libamp_test_set_tick(0);
    std::array<uint8_t, 64> buffer = {};
    auto *packet = reinterpret_cast<PacketRecordRuntime *>(buffer.data());
    packet->header.code = PACKET_CODE_GET;
    packet->header.type = PACKET_DATA_RECORD;
    packet->sub_cmd = PACKET_DATA_RECORD_RUNTIME;
    packet_process(buffer.data(), buffer.size());
    EXPECT_EQ(0u, packet->uptime);
}

TEST(RuntimePacket, UptimeAndTotalKeepAdvancingAcrossLowWordWrap)
{
    libamp_test_set_tick(UINT32_MAX - 7u);
    record_set_runtime(UINT64_C(10000000));
    const uint64_t old_ms = KEYBOARD_TICK_TO_TIME64(keyboard_tick_get64());
    keyboard_tick_inc(32);

    std::array<uint8_t, 64> buffer = {};
    auto *packet = reinterpret_cast<PacketRecordRuntime *>(buffer.data());
    packet->header.code = PACKET_CODE_GET;
    packet->header.type = PACKET_DATA_RECORD;
    packet->sub_cmd = PACKET_DATA_RECORD_RUNTIME;
    packet_process(buffer.data(), buffer.size());

    const uint64_t now_ms = KEYBOARD_TICK_TO_TIME64((UINT64_C(1) << 32) + 24u);
    EXPECT_EQ(now_ms, packet->uptime);
    EXPECT_EQ(UINT64_C(10000000) + now_ms - old_ms, packet->runtime);
    record_set_runtime(0);
    keyboard_tick_inc(KEYBOARD_TIME_TO_TICK(100));
    EXPECT_EQ(100u, record_get_runtime());
}
