#include <gtest/gtest.h>
#include "keyboard_tick.h"

static uint64_t platform_tick;

#ifdef TEST_OVERRIDE_TICK64
extern "C" uint64_t keyboard_tick_get64(void)
{
    return platform_tick;
}
#endif

#ifdef TEST_OVERRIDE_TICK32
extern "C" uint32_t keyboard_tick_get32(void)
{
    return (uint32_t)platform_tick;
}
#endif

TEST(KeyboardTickOverride, GettersCanBeOverriddenIndependently)
{
    keyboard_tick_init();
    platform_tick = (UINT64_C(7) << 32) + 123;
    keyboard_tick_inc(999); // Software-backend updates do not change this clock.
#ifdef TEST_OVERRIDE_TICK64
    EXPECT_EQ((UINT64_C(7) << 32) + 123, keyboard_tick_get64());
#else
    EXPECT_EQ(999u, keyboard_tick_get64());
#endif
#ifdef TEST_OVERRIDE_TICK32
    EXPECT_EQ(123u, keyboard_tick_get32());
#else
    EXPECT_EQ(999u, keyboard_tick_get32());
#endif
    platform_tick += UINT32_MAX;
    keyboard_tick_inc(1);
#ifdef TEST_OVERRIDE_TICK64
    EXPECT_EQ((UINT64_C(8) << 32) + 122, keyboard_tick_get64());
#else
    EXPECT_EQ(1000u, keyboard_tick_get64());
#endif
#ifdef TEST_OVERRIDE_TICK32
    EXPECT_EQ(122u, keyboard_tick_get32());
#else
    EXPECT_EQ(1000u, keyboard_tick_get32());
#endif
}
