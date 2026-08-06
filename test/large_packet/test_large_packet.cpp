#include <gtest/gtest.h>

#include <cstring>

#include "file_system.h"
#include "packet.h"
#include "script.h"
#include "storage.h"

namespace {

AmpStatus process_large(uint8_t code, uint8_t type, const void *body,
                        AmpFrame *out_response = nullptr)
{
    AmpFrame request = {};
    AmpFrame response = {};
    request.header.proto = AMP_FRAME_PROTO;
    request.header.code = code;
    request.header.type = type;
    if (body != nullptr)
    {
        std::memcpy(request.body, body, AMP_FRAME_BODY_SIZE);
    }
    AmpStatus status = large_packet_process(&request, &response);
    if (out_response != nullptr)
    {
        *out_response = response;
    }
    return status;
}

void abort_bytecode_transfer()
{
    PacketLargeControl abort = {};
    abort.sub_cmd = LARGE_DATA_CMD_ABORT;
    (void)process_large(PACKET_CODE_LARGE_SET,
                        PACKET_DATA_SCRIPT_BYTECODE, &abort);
}

void upload_bytecode(const char *data, uint8_t length)
{
    abort_bytecode_transfer();
    PacketLargeStart start = {};
    start.sub_cmd = LARGE_DATA_CMD_START;
    start.total_size = length;
    ASSERT_EQ(AMP_STATUS_OK,
              process_large(PACKET_CODE_LARGE_SET,
                            PACKET_DATA_SCRIPT_BYTECODE, &start));

    PacketLargePayload payload = {};
    payload.sub_cmd = LARGE_DATA_CMD_PAYLOAD;
    payload.chunk_length = length;
    std::memcpy(payload.data, data, length);
    ASSERT_EQ(AMP_STATUS_OK,
              process_large(PACKET_CODE_LARGE_SET,
                            PACKET_DATA_SCRIPT_BYTECODE, &payload));

    PacketLargeControl end = {};
    end.sub_cmd = LARGE_DATA_CMD_END;
    ASSERT_EQ(AMP_STATUS_OK,
              process_large(PACKET_CODE_LARGE_SET,
                            PACKET_DATA_SCRIPT_BYTECODE, &end));
}

} // namespace

TEST(LargePacketV3, CommitsScriptBytecodeOnlyAfterEnd)
{
#if defined(SCRIPT_ENABLE) && SCRIPT_RUNTIME_STRATEGY == SCRIPT_AOT
    upload_bytecode("abcde", 5);
    std::memset(g_script_bytecode_buffer, 0, sizeof(g_script_bytecode_buffer));
    storage_read_script();
    EXPECT_EQ(0, std::memcmp(g_script_bytecode_buffer, "abcde", 5));
#else
    GTEST_SKIP() << "Large packet script test targets the AOT bytecode path.";
#endif
}

TEST(LargePacketV3, RejectsOutOfOrderPayload)
{
#if defined(SCRIPT_ENABLE) && SCRIPT_RUNTIME_STRATEGY == SCRIPT_AOT
    abort_bytecode_transfer();
    PacketLargeStart start = {};
    start.sub_cmd = LARGE_DATA_CMD_START;
    start.total_size = 4;
    ASSERT_EQ(AMP_STATUS_OK,
              process_large(PACKET_CODE_LARGE_SET,
                            PACKET_DATA_SCRIPT_BYTECODE, &start));

    PacketLargePayload payload = {};
    payload.sub_cmd = LARGE_DATA_CMD_PAYLOAD;
    payload.offset = 2;
    payload.chunk_length = 2;
    std::memcpy(payload.data, "wx", 2);
    EXPECT_EQ(AMP_STATUS_INVALID_ARGUMENT,
              process_large(PACKET_CODE_LARGE_SET,
                            PACKET_DATA_SCRIPT_BYTECODE, &payload));
    abort_bytecode_transfer();
#else
    GTEST_SKIP() << "Large packet script test targets the AOT bytecode path.";
#endif
}

TEST(LargePacketV3, RequiresCompleteSizeBeforeEnd)
{
#if defined(SCRIPT_ENABLE) && SCRIPT_RUNTIME_STRATEGY == SCRIPT_AOT
    abort_bytecode_transfer();
    PacketLargeStart start = {};
    start.sub_cmd = LARGE_DATA_CMD_START;
    start.total_size = 4;
    ASSERT_EQ(AMP_STATUS_OK,
              process_large(PACKET_CODE_LARGE_SET,
                            PACKET_DATA_SCRIPT_BYTECODE, &start));
    PacketLargeControl end = {};
    end.sub_cmd = LARGE_DATA_CMD_END;
    EXPECT_EQ(AMP_STATUS_INVALID_STATE,
              process_large(PACKET_CODE_LARGE_SET,
                            PACKET_DATA_SCRIPT_BYTECODE, &end));
    abort_bytecode_transfer();
#else
    GTEST_SKIP() << "Large packet script test targets the AOT bytecode path.";
#endif
}

TEST(LargePacketV3, GetsPayloadAndClosesOnEnd)
{
#if defined(SCRIPT_ENABLE) && SCRIPT_RUNTIME_STRATEGY == SCRIPT_AOT
    upload_bytecode("abcde", 5);

    PacketLargeStart start = {};
    start.sub_cmd = LARGE_DATA_CMD_START;
    AmpFrame response = {};
    ASSERT_EQ(AMP_STATUS_OK,
              process_large(PACKET_CODE_LARGE_GET,
                            PACKET_DATA_SCRIPT_BYTECODE, &start, &response));
    EXPECT_EQ(5u,
              reinterpret_cast<const PacketLargeStart *>(response.body)->total_size);

    PacketLargePayload payload = {};
    payload.sub_cmd = LARGE_DATA_CMD_PAYLOAD;
    payload.offset = 1;
    payload.chunk_length = 3;
    ASSERT_EQ(AMP_STATUS_OK,
              process_large(PACKET_CODE_LARGE_GET,
                            PACKET_DATA_SCRIPT_BYTECODE, &payload, &response));
    const PacketLargePayload *out =
        reinterpret_cast<const PacketLargePayload *>(response.body);
    EXPECT_EQ(1u, out->offset);
    EXPECT_EQ(3, out->chunk_length);
    EXPECT_EQ(0, std::memcmp(out->data, "bcd", 3));

    PacketLargeControl end = {};
    end.sub_cmd = LARGE_DATA_CMD_END;
    EXPECT_EQ(AMP_STATUS_OK,
              process_large(PACKET_CODE_LARGE_GET,
                            PACKET_DATA_SCRIPT_BYTECODE, &end));
#else
    GTEST_SKIP() << "Large packet script test targets the AOT bytecode path.";
#endif
}

TEST(LargePacketV3, MissingScriptObjectIsAnEmptyDownload)
{
#if defined(SCRIPT_ENABLE) && SCRIPT_RUNTIME_STRATEGY == SCRIPT_AOT
    constexpr uint8_t type = PACKET_DATA_SCRIPT_BYTECODE;
    const char *path = "scripts/main.bin";
#elif defined(SCRIPT_ENABLE) && SCRIPT_RUNTIME_STRATEGY == SCRIPT_JIT
    constexpr uint8_t type = PACKET_DATA_SCRIPT_SCOURCE;
    const char *path = "scripts/main.js";
#else
    GTEST_SKIP() << "Large packet script support is disabled.";
#endif

#ifdef SCRIPT_ENABLE
    PacketLargeControl abort = {};
    abort.sub_cmd = LARGE_DATA_CMD_ABORT;
    (void)process_large(PACKET_CODE_LARGE_GET, type, &abort);
    (void)fs_unlink(path);

    PacketLargeStart start = {};
    start.sub_cmd = LARGE_DATA_CMD_START;
    AmpFrame response = {};
    ASSERT_EQ(AMP_STATUS_OK,
              process_large(PACKET_CODE_LARGE_GET, type, &start, &response));
    EXPECT_EQ(0u,
              reinterpret_cast<const PacketLargeStart *>(response.body)->total_size);

    PacketLargeControl end = {};
    end.sub_cmd = LARGE_DATA_CMD_END;
    EXPECT_EQ(AMP_STATUS_OK,
              process_large(PACKET_CODE_LARGE_GET, type, &end));
#endif
}

TEST(LargePacketV3, NewGetStartRecoversAbandonedGet)
{
#if defined(SCRIPT_ENABLE) && SCRIPT_RUNTIME_STRATEGY == SCRIPT_AOT
    upload_bytecode("abcde", 5);

    PacketLargeStart start = {};
    start.sub_cmd = LARGE_DATA_CMD_START;
    AmpFrame response = {};
    ASSERT_EQ(AMP_STATUS_OK,
              process_large(PACKET_CODE_LARGE_GET,
                            PACKET_DATA_SCRIPT_BYTECODE, &start, &response));

    // Simulate the host disappearing before it can send End or Abort.
    ASSERT_EQ(AMP_STATUS_OK,
              process_large(PACKET_CODE_LARGE_GET,
                            PACKET_DATA_SCRIPT_BYTECODE, &start, &response));
    EXPECT_EQ(5u,
              reinterpret_cast<const PacketLargeStart *>(response.body)->total_size);

    PacketLargeControl end = {};
    end.sub_cmd = LARGE_DATA_CMD_END;
    EXPECT_EQ(AMP_STATUS_OK,
              process_large(PACKET_CODE_LARGE_GET,
                            PACKET_DATA_SCRIPT_BYTECODE, &end));
#else
    GTEST_SKIP() << "Large packet script test targets the AOT bytecode path.";
#endif
}

TEST(LargePacketV3, NewSetStartRecoversAbandonedSet)
{
#if defined(SCRIPT_ENABLE) && SCRIPT_RUNTIME_STRATEGY == SCRIPT_AOT
    abort_bytecode_transfer();

    PacketLargeStart start = {};
    start.sub_cmd = LARGE_DATA_CMD_START;
    start.total_size = 4;
    ASSERT_EQ(AMP_STATUS_OK,
              process_large(PACKET_CODE_LARGE_SET,
                            PACKET_DATA_SCRIPT_BYTECODE, &start));

    PacketLargePayload abandoned_payload = {};
    abandoned_payload.sub_cmd = LARGE_DATA_CMD_PAYLOAD;
    abandoned_payload.chunk_length = 2;
    std::memcpy(abandoned_payload.data, "ab", 2);
    ASSERT_EQ(AMP_STATUS_OK,
              process_large(PACKET_CODE_LARGE_SET,
                            PACKET_DATA_SCRIPT_BYTECODE, &abandoned_payload));

    // A new upload must discard the abandoned temporary file and start at 0.
    start.total_size = 3;
    ASSERT_EQ(AMP_STATUS_OK,
              process_large(PACKET_CODE_LARGE_SET,
                            PACKET_DATA_SCRIPT_BYTECODE, &start));

    PacketLargePayload replacement = {};
    replacement.sub_cmd = LARGE_DATA_CMD_PAYLOAD;
    replacement.chunk_length = 3;
    std::memcpy(replacement.data, "xyz", 3);
    ASSERT_EQ(AMP_STATUS_OK,
              process_large(PACKET_CODE_LARGE_SET,
                            PACKET_DATA_SCRIPT_BYTECODE, &replacement));

    PacketLargeControl end = {};
    end.sub_cmd = LARGE_DATA_CMD_END;
    ASSERT_EQ(AMP_STATUS_OK,
              process_large(PACKET_CODE_LARGE_SET,
                            PACKET_DATA_SCRIPT_BYTECODE, &end));

    std::memset(g_script_bytecode_buffer, 0, sizeof(g_script_bytecode_buffer));
    storage_read_script();
    EXPECT_EQ(0, std::memcmp(g_script_bytecode_buffer, "xyz", 3));
#else
    GTEST_SKIP() << "Large packet script test targets the AOT bytecode path.";
#endif
}
