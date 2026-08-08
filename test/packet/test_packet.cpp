#include <gtest/gtest.h>

#include <array>
#include <cstring>

extern "C" {
#include "amp_protocol.h"
#include "packet.h"
#include "test_fixture.h"
}

namespace {

using Report = std::array<uint8_t, AMP_FRAME_REPORT_SIZE>;

AmpFrame transact(uint8_t channel, uint16_t opcode, const void *payload,
                  uint16_t payload_len, uint16_t session = 0x1234,
                  uint16_t request_id = 1)
{
    Report report = {};
    EXPECT_GE(amp_frame_encode(report.data(), report.size(), channel, 0,
                               session, request_id, opcode, AMP_STATUS_OK,
                               payload, payload_len), 0);
    AmpFrame request = {};
    EXPECT_TRUE(amp_frame_decode(report.data(), report.size(), &request));
    std::memset(raw_send_buffer, 0, sizeof(raw_send_buffer));
    packet_process_frame(&request);
    AmpFrame response = {};
    EXPECT_TRUE(amp_frame_decode(raw_send_buffer, AMP_FRAME_REPORT_SIZE, &response));
    return response;
}

AmpFrame hello(uint16_t session = 0x1234, uint16_t request_id = 1)
{
    AmpHelloRequest request = {
        AMP_FRAME_PAYLOAD_SIZE,
        AMP_FRAME_PAYLOAD_SIZE,
        0,
    };
    return transact(AMP_CHANNEL_CONTROL, AMP_CONTROL_HELLO, &request,
                    sizeof(request), session, request_id);
}

} // namespace

TEST(AmpProtocolV4, EncodesLengthDelimitedFrame)
{
    Report report = {};
    const uint8_t payload[] = {0x11, 0x22, 0x33};
    EXPECT_EQ(AMP_FRAME_HEADER_SIZE + sizeof(payload),
              amp_frame_encode(report.data(), report.size(), AMP_CHANNEL_DEBUG,
                               0, 0x2345, 0x3456, AMP_DEBUG_SAMPLE,
                               AMP_STATUS_OK, payload, sizeof(payload)));
    AmpFrame frame = {};
    ASSERT_TRUE(amp_frame_decode(report.data(), report.size(), &frame));
    EXPECT_EQ(AMP_WIRE_VERSION, frame.header.version);
    EXPECT_EQ(AMP_CHANNEL_DEBUG, amp_frame_channel(&frame.header));
    EXPECT_EQ(0x2345, frame.header.session_id);
    EXPECT_EQ(0x3456, frame.header.request_id);
    EXPECT_EQ(AMP_DEBUG_SAMPLE, frame.header.opcode);
    ASSERT_EQ(sizeof(payload), frame.header.payload_len);
    EXPECT_EQ(0, std::memcmp(frame.payload, payload, sizeof(payload)));

    frame.header.payload_len = AMP_FRAME_PAYLOAD_SIZE + 1;
    std::memcpy(report.data(), &frame, sizeof(frame));
    EXPECT_FALSE(amp_frame_decode(report.data(), report.size(), &frame));
}

TEST(AmpProtocolV4, HelloStartsExplicitSession)
{
    const uint32_t prepare_count = amp_transport_prepare_session_count;
    AmpFrame response = hello(0x4567, 0x1001);
    EXPECT_EQ(AMP_FRAME_FLAG_RESPONSE, amp_frame_flags(&response.header));
    EXPECT_EQ(0x4567, response.header.session_id);
    EXPECT_EQ(0x1001, response.header.request_id);
    EXPECT_EQ(AMP_STATUS_OK, response.header.status);
    ASSERT_EQ(sizeof(AmpHelloResponse), response.header.payload_len);
    const auto *body = reinterpret_cast<const AmpHelloResponse *>(response.payload);
    EXPECT_EQ(AMP_FRAME_PAYLOAD_SIZE, body->max_rx_payload);
    EXPECT_EQ(STORAGE_PROFILE_FILE_NUM, body->profile_count);
    EXPECT_EQ(ADVANCED_KEY_NUM, body->advanced_key_count);
#if AMP_OBJECT_CRC32_ENABLE
    EXPECT_NE(0u, body->capabilities & AMP_CAP_OBJECT_CRC32);
#else
    EXPECT_EQ(0u, body->capabilities & AMP_CAP_OBJECT_CRC32);
#endif
#if AMP_OBJECT_TEMP_FILE_ENABLE
    EXPECT_NE(0u, body->capabilities & AMP_CAP_OBJECT_ATOMIC_COMMIT);
#else
    EXPECT_EQ(0u, body->capabilities & AMP_CAP_OBJECT_ATOMIC_COMMIT);
#endif
    EXPECT_TRUE(amp_session_is_active());
    EXPECT_EQ(0x4567, amp_session_id());
    EXPECT_EQ(prepare_count + 1, amp_transport_prepare_session_count);
}

TEST(AmpProtocolV4, InvalidHelloDoesNotPrepareTransportSession)
{
    AmpHelloRequest request = {
        0,
        AMP_FRAME_PAYLOAD_SIZE,
        0,
    };
    const uint32_t prepare_count = amp_transport_prepare_session_count;
    AmpFrame response = transact(AMP_CHANNEL_CONTROL, AMP_CONTROL_HELLO,
                                 &request, sizeof(request), 0x4568, 0x1002);
    EXPECT_EQ(AMP_STATUS_INVALID_ARGUMENT, response.header.status);
    EXPECT_EQ(prepare_count, amp_transport_prepare_session_count);
}

TEST(AmpProtocolV4, HelloAppliesDirectionalPayloadNegotiation)
{
    AmpHelloRequest request = {
        AMP_FRAME_PAYLOAD_SIZE,
        17,
        0,
    };
    AmpFrame response = transact(AMP_CHANNEL_CONTROL, AMP_CONTROL_HELLO,
                                 &request, sizeof(request), 0x4567, 0x1002);
    ASSERT_EQ(AMP_STATUS_OK, response.header.status);
    EXPECT_EQ(17, amp_session_max_rx_payload());
    EXPECT_EQ(AMP_FRAME_PAYLOAD_SIZE, amp_session_max_tx_payload());
    const auto *body = reinterpret_cast<const AmpHelloResponse *>(response.payload);
    EXPECT_EQ(AMP_FRAME_PAYLOAD_SIZE, body->max_rx_payload);
    EXPECT_EQ(AMP_FRAME_PAYLOAD_SIZE, body->max_tx_payload);
}

TEST(AmpProtocolV4, IgnoresMessagesFromOldSession)
{
    ASSERT_EQ(AMP_STATUS_OK, hello(0x1111).header.status);
    libamp_test_clear_output_buffers();
    AmpActivateProfileRequest request = {1};
    Report report = {};
    ASSERT_GE(amp_frame_encode(report.data(), report.size(), AMP_CHANNEL_CONFIG,
                               0, 0x2222, 2, AMP_CONFIG_ACTIVATE_PROFILE,
                               AMP_STATUS_OK, &request, sizeof(request)), 0);
    AmpFrame frame = {};
    ASSERT_TRUE(amp_frame_decode(report.data(), report.size(), &frame));
    packet_process_frame(&frame);
    EXPECT_EQ(0u, raw_send_count);
}

TEST(AmpProtocolV4, RepeatedRequestReturnsCachedResponse)
{
    ASSERT_EQ(AMP_STATUS_OK, hello().header.status);
    AmpActivateProfileRequest request = {1};
    AmpFrame first = transact(AMP_CHANNEL_CONFIG, AMP_CONFIG_ACTIVATE_PROFILE,
                              &request, sizeof(request), 0x1234, 2);
    ASSERT_EQ(AMP_STATUS_OK, first.header.status);
    const uint32_t first_revision =
        reinterpret_cast<const AmpActivateProfileResponse *>(first.payload)
            ->device_state_revision;
    AmpFrame repeated = transact(AMP_CHANNEL_CONFIG, AMP_CONFIG_ACTIVATE_PROFILE,
                                 &request, sizeof(request), 0x1234, 2);
    ASSERT_EQ(AMP_STATUS_OK, repeated.header.status);
    EXPECT_EQ(first_revision,
              reinterpret_cast<const AmpActivateProfileResponse *>(repeated.payload)
                  ->device_state_revision);
}

TEST(AmpProtocolV4, DebugSamplesUseActualPayloadLength)
{
    ASSERT_EQ(AMP_STATUS_OK, hello().header.status);
    g_keyboard_tick = 1234;
    g_keyboard_advanced_keys[2].raw = 111;
    g_keyboard_advanced_keys[2].value = 222;
    const uint8_t request[] = {1, 2, 0};
    AmpFrame response = transact(AMP_CHANNEL_DEBUG, AMP_DEBUG_SAMPLE,
                                 request, sizeof(request), 0x1234, 2);
    ASSERT_EQ(AMP_STATUS_OK, response.header.status);
    ASSERT_EQ(9 + sizeof(AmpDebugItem), response.header.payload_len);
    const auto *data = reinterpret_cast<const AmpDebugData *>(response.payload);
    EXPECT_EQ(1234u, data->tick);
    EXPECT_EQ(1, data->count);
    EXPECT_EQ(2, data->items[0].index);
    EXPECT_EQ(111, data->items[0].raw);
    EXPECT_EQ(222, data->items[0].value);
}

TEST(AmpProtocolV4, SessionResetDropsAllQueueClasses)
{
    raw_send_result = 1;
    ASSERT_EQ(0, amp_send_frame(AMP_CHANNEL_CONTROL, AMP_FRAME_FLAG_RESPONSE,
                                1, 1, AMP_CONTROL_HELLO, AMP_STATUS_OK,
                                nullptr, 0, AMP_QUEUE_RESPONSE));
    ASSERT_EQ(0, amp_send_frame(AMP_CHANNEL_CONFIG, AMP_FRAME_FLAG_EVENT,
                                1, 0, AMP_CONFIG_PROFILE_CHANGED,
                                AMP_STATUS_OK, nullptr, 0, AMP_QUEUE_CONTROL));
    ASSERT_EQ(0, amp_send_frame(AMP_CHANNEL_DEBUG, AMP_FRAME_FLAG_EVENT,
                                1, 0, AMP_DEBUG_DATA, AMP_STATUS_OK,
                                nullptr, 0, AMP_QUEUE_STREAM));
    amp_transport_reset_session();
    raw_send_result = 0;
    raw_send_count = 0;
    amp_transport_kick();
    EXPECT_EQ(0u, raw_send_count);
}
