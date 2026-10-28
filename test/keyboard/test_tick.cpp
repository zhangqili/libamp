#include <gtest/gtest.h>
#include <atomic>
#include <thread>
#include <vector>

#include "keyboard.h"

class KeyboardTick : public ::testing::Test {
protected:
    void SetUp() override { keyboard_tick_reset(); }
};

TEST_F(KeyboardTick, LowWordWrapMatchesConfiguredWidth)
{
    EXPECT_EQ(0u, keyboard_tick_get64());
    keyboard_tick_inc(UINT32_MAX - 15u);
    const uint32_t start = keyboard_tick_get32();
    keyboard_tick_inc(32);
    EXPECT_EQ(16u, keyboard_tick_get32());
#if KEYBOARD_TICK_64BIT_ENABLE
    EXPECT_EQ((UINT64_C(1) << 32) + 16u, keyboard_tick_get64());
#else
    EXPECT_EQ(16u, keyboard_tick_get64());
#endif
    EXPECT_EQ(32u, tick_elapsed32(keyboard_tick_get32(), start));
    keyboard_tick_inc(UINT32_MAX);
#if KEYBOARD_TICK_64BIT_ENABLE
    EXPECT_EQ((UINT64_C(2) << 32) + 15u, keyboard_tick_get64());
#else
    EXPECT_EQ(15u, keyboard_tick_get64());
#endif
    keyboard_tick_inc(0);
    EXPECT_EQ(15u, keyboard_tick_get32());
}

TEST_F(KeyboardTick, InitializationClearsCounterAndSnapshots)
{
    keyboard_tick_inc(UINT32_MAX);
    keyboard_tick_inc(42);
    keyboard_tick_init();
    EXPECT_EQ(0u, keyboard_tick_get32());
    EXPECT_EQ(0u, keyboard_tick_get64());
    keyboard_tick_inc(7);
    EXPECT_EQ(7u, keyboard_tick_get32());
    EXPECT_EQ(7u, keyboard_tick_get64());
}

TEST_F(KeyboardTick, DeadlinesIncludeEqualityAndCrossWrap)
{
    EXPECT_FALSE(tick_reached32(0xfffffff0u, 0x10u));
    EXPECT_FALSE(tick_reached32(0xfu, 0x10u));
    EXPECT_TRUE(tick_reached32(0x10u, 0x10u));
    EXPECT_TRUE(tick_reached32(0x11u, 0x10u));
    EXPECT_TRUE(tick_reached32(0x10u, 0xfffffff0u));
    EXPECT_TRUE(tick_reached32(0, 0));
    EXPECT_FALSE(tick_reached32(UINT32_MAX, 0));
    EXPECT_TRUE(tick_reached32(0, UINT32_MAX));
    EXPECT_TRUE(tick_reached32(0x7fffffffu, 0));
    EXPECT_FALSE(tick_reached32(0x80000001u, 0));
}

TEST_F(KeyboardTick, TimeConversionsKeepHighBitsAndEvaluateOnce)
{
    const uint64_t ms = (UINT64_C(1) << 33) + 1234;
    const uint64_t ticks = (ms / 1000) * POLLING_RATE + ((ms % 1000) * POLLING_RATE) / 1000;
    EXPECT_EQ(ticks, KEYBOARD_TIME_TO_TICK64(ms));
    EXPECT_EQ((ticks / POLLING_RATE) * 1000 + ((ticks % POLLING_RATE) * 1000) / POLLING_RATE,
              KEYBOARD_TICK_TO_TIME64(ticks));
    uint64_t input = ms;
    EXPECT_EQ(ticks, KEYBOARD_TIME_TO_TICK64(input++));
    EXPECT_EQ(ms + 1, input);
    input = ticks;
    const uint64_t expected_ms = KEYBOARD_TICK_TO_TIME64(input);
    EXPECT_EQ(expected_ms, KEYBOARD_TICK_TO_TIME64(input++));
    EXPECT_EQ(ticks + 1, input);
}

#if KEYBOARD_TICK_64BIT_ENABLE
TEST_F(KeyboardTick, ConcurrentReadersNeverObserveTornCarry)
{
    constexpr uint32_t step = UINT32_MAX / 3u;
    constexpr unsigned iterations = 300000;
    std::atomic<bool> start{false}, done{false}, valid{true};
    std::atomic<unsigned> ready{0};
    std::vector<std::thread> readers;
    for (unsigned i = 0; i < 3; i++) {
        readers.emplace_back([&] {
            ready.fetch_add(1);
            while (!start.load()) { std::this_thread::yield(); }
            uint64_t previous = 0;
            do {
                const uint64_t now = keyboard_tick_get64();
                const uint32_t low = keyboard_tick_get32();
                (void)low;
                if (now < previous || now % step != 0) { valid.store(false); }
                previous = now;
            } while (!done.load());
        });
    }
    while (ready.load() != readers.size()) { std::this_thread::yield(); }
    start.store(true);
    for (unsigned i = 0; i < iterations; i++) { keyboard_tick_inc(step); }
    done.store(true);
    for (auto &reader : readers) { reader.join(); }
    EXPECT_TRUE(valid.load());
    EXPECT_EQ(static_cast<uint64_t>(iterations) * step, keyboard_tick_get64());
    EXPECT_EQ(static_cast<uint32_t>(static_cast<uint64_t>(iterations) * step), keyboard_tick_get32());
}
#endif
