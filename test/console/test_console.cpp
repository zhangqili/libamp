#include <gtest/gtest.h>

#include <cstring>

#include "amp_protocol.h"
#include "console.h"
#include "packet.h"
#include "test_fixture.h"

namespace {

void reset_console_output(void)
{
    g_keyboard_config.console = true;
    console_flush();
    std::memset(raw_send_buffer, 0, sizeof(raw_send_buffer));
}

} // namespace

TEST(Console, BufferWrapFullAndPop)
{
    ConsoleBuffer buffer = {};
    console_buffer_init(&buffer);

    EXPECT_TRUE(console_buffer_is_empty(&buffer));
    for (int i = 0; i < CONSOLE_BUFFER_LENGTH; i++)
    {
        ASSERT_TRUE(console_buffer_push(&buffer, (ConsoleBufferElm)('A' + (i % 26))));
    }
    EXPECT_TRUE(console_buffer_is_full(&buffer));
    EXPECT_FALSE(console_buffer_push(&buffer, 'x'));

    ConsoleBufferElm value = 0;
    ASSERT_TRUE(console_buffer_pop(&buffer, &value));
    EXPECT_EQ('A', value);
    ASSERT_TRUE(console_buffer_push(&buffer, 'z'));

    for (int i = 1; i < CONSOLE_BUFFER_LENGTH; i++)
    {
        ASSERT_TRUE(console_buffer_pop(&buffer, &value));
    }
    ASSERT_TRUE(console_buffer_pop(&buffer, &value));
    EXPECT_EQ('z', value);
    EXPECT_TRUE(console_buffer_is_empty(&buffer));
    EXPECT_FALSE(console_buffer_pop(&buffer, &value));
}

TEST(Console, DisabledConsoleDropsOutput)
{
    reset_console_output();

    g_keyboard_config.console = false;
    console_send_char('x');
    g_keyboard_config.console = true;
    console_flush();

    AmpFrame frame = {};
    EXPECT_FALSE(amp_frame_decode(raw_send_buffer, sizeof(raw_send_buffer), &frame));
}

TEST(Console, PrintfFlushesConsoleFrame)
{
    reset_console_output();

    static constexpr const char kExpectedMessage[] = "hello 42";
    console_printf("hello %d", 42);
    console_flush();

    AmpFrame frame = {};
    ASSERT_TRUE(amp_frame_decode(raw_send_buffer, sizeof(raw_send_buffer), &frame));
    EXPECT_EQ(AMP_CHANNEL_CONSOLE, amp_frame_channel(&frame.header));
    EXPECT_EQ(PACKET_CODE_LOG, frame.header.code);
    ASSERT_EQ(sizeof(kExpectedMessage) - 1, frame.header.len);
    EXPECT_EQ(0, std::memcmp(frame.payload, kExpectedMessage, sizeof(kExpectedMessage) - 1));
}

TEST(Console, FlushSplitsLargeOutputIntoPayloadSizedFrames)
{
    reset_console_output();

    static constexpr uint8_t kTailLength = 3;
    for (uint8_t i = 0; i < (uint8_t)(AMP_FRAME_MAX_PAYLOAD * 2 + kTailLength); i++)
    {
        console_send_char((char)('a' + (i % 26)));
    }
    console_flush();

    AmpFrame frame = {};
    ASSERT_TRUE(amp_frame_decode(raw_send_buffer, sizeof(raw_send_buffer), &frame));
    EXPECT_EQ(AMP_CHANNEL_CONSOLE, amp_frame_channel(&frame.header));
    EXPECT_EQ(PACKET_CODE_LOG, frame.header.code);
    EXPECT_EQ(kTailLength, frame.header.len);
}
