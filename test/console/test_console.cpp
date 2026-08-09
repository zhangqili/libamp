#include <gtest/gtest.h>

#include <cstring>

#include "console.h"
#include "packet.h"
#include "packet_buffer.h"
#include "test_fixture.h"

namespace {

void reset_console_output(void)
{
    g_keyboard_config.console = true;
    console_flush();
    packet_buffer_flush();
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
    packet_buffer_flush();

    // 控制台未开启时不应产生任何输出
    EXPECT_EQ(0, raw_send_buffer[0]);
}

TEST(Console, PrintfFlushesConsolePacket)
{
    reset_console_output();

    static constexpr const char kExpectedMessage[] = "hello 42";
    console_printf("hello %d", 42);
    console_flush();
    packet_buffer_flush();

    // v2 控制台包：code(0)=0x03, reserved(1), length(2-3), data(4+)
    EXPECT_EQ(PACKET_CODE_CONSOLE, raw_send_buffer[0]);
    const uint16_t length = raw_send_buffer[2] | (raw_send_buffer[3] << 8);
    ASSERT_EQ(sizeof(kExpectedMessage) - 1, length);
    EXPECT_EQ(0, std::memcmp(raw_send_buffer + 4, kExpectedMessage, length));
}

TEST(Console, FlushSplitsLargeOutputIntoPacketSizedChunks)
{
    reset_console_output();

    static constexpr uint8_t kChunkLength = PACKET_BUFFER_LENGTH - offsetof(PacketLog, data);
    static constexpr uint8_t kTailLength = 3;
    for (uint8_t i = 0; i < (uint8_t)(kChunkLength * 2 + kTailLength); i++)
    {
        console_send_char((char)('a' + (i % 26)));
    }
    console_flush();
    packet_buffer_flush();

    // 单槽缓冲：一次 flush 只推出一包（整块），其余留在控制台队列
    EXPECT_EQ(PACKET_CODE_CONSOLE, raw_send_buffer[0]);
    const uint16_t length = raw_send_buffer[2] | (raw_send_buffer[3] << 8);
    EXPECT_EQ(kChunkLength, length);
}
