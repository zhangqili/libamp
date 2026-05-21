#include <gtest/gtest.h>

#include <array>
#include <cstring>

#include "packet.h"
#include "script.h"
#include "storage.h"

namespace {

enum {
    kLargeDataStart = 0,
    kLargeDataPayload = 1,
};

PacketLargeData *packet_from(std::array<uint8_t, 64>& buffer)
{
    return reinterpret_cast<PacketLargeData *>(buffer.data());
}

} // namespace

TEST(LargePacket, WritesScriptBytecodePayloadsToStorage)
{
#if defined(SCRIPT_ENABLE) && SCRIPT_RUNTIME_STRATEGY == SCRIPT_AOT
    std::array<uint8_t, 64> buffer = {};
    PacketLargeData *packet = packet_from(buffer);

    packet->code = PACKET_CODE_LARGE_SET;
    packet->type = PACKET_DATA_SCRIPT_BYTECODE;
    packet->sub_cmd = kLargeDataStart;
    packet->header.total_size = 5;
    packet->header.checksum = 0;
    large_packet_process(packet);

    buffer.fill(0);
    packet = packet_from(buffer);
    packet->code = PACKET_CODE_LARGE_SET;
    packet->type = PACKET_DATA_SCRIPT_BYTECODE;
    packet->sub_cmd = kLargeDataPayload;
    packet->payload.offset = 0;
    packet->payload.length = 5;
    std::memcpy(packet->payload.data, "abcde", 5);
    large_packet_process(packet);

    std::memset(g_script_bytecode_buffer, 0, sizeof(g_script_bytecode_buffer));
    storage_read_script();

    EXPECT_EQ(0, std::memcmp(g_script_bytecode_buffer, "abcde", 5));
#else
    GTEST_SKIP() << "Large packet script test currently targets the default AOT bytecode path.";
#endif
}

TEST(LargePacket, RejectsOutOfOrderScriptPayload)
{
#if defined(SCRIPT_ENABLE) && SCRIPT_RUNTIME_STRATEGY == SCRIPT_AOT
    std::array<uint8_t, 64> buffer = {};
    PacketLargeData *packet = packet_from(buffer);

    packet->code = PACKET_CODE_LARGE_SET;
    packet->type = PACKET_DATA_SCRIPT_BYTECODE;
    packet->sub_cmd = kLargeDataStart;
    packet->header.total_size = 4;
    large_packet_process(packet);

    buffer.fill(0);
    packet = packet_from(buffer);
    packet->code = PACKET_CODE_LARGE_SET;
    packet->type = PACKET_DATA_SCRIPT_BYTECODE;
    packet->sub_cmd = kLargeDataPayload;
    packet->payload.offset = 2;
    packet->payload.length = 4;
    std::memcpy(packet->payload.data, "wxyz", 4);
    large_packet_process(packet);

    std::memset(g_script_bytecode_buffer, 0xAA, sizeof(g_script_bytecode_buffer));
    storage_read_script();

    EXPECT_EQ(0xAA, g_script_bytecode_buffer[0]);
#else
    GTEST_SKIP() << "Large packet script test currently targets the default AOT bytecode path.";
#endif
}
