#include <gtest/gtest.h>
#include <cstring>

#include "event_cache.h"
#include "script.h"
#include "test_fixture.h"

static bool script_holds(Keycode keycode)
{
    keyboard_report_clear_all();
    event_cache_add_to_report();
    keyboard_report_send();
    for (unsigned i = 2; i < 8; i++) {
        if (keyboard_send_buffer[i] == keycode) { return true; }
    }
    return false;
}

TEST(ScriptTick, TimeoutsWaitAcrossSignedMillisecondsAndLowTickWrap)
{
    const uint64_t starts[] = {
        KEYBOARD_TIME_TO_TICK64(INT32_MAX - 5u),
        UINT32_MAX - 5u,
    };
    for (uint64_t start : starts) {
        SCOPED_TRACE(start);
        libamp_test_reset_environment();
        libamp_test_set_tick(start);
        script_reset_runtime();
        g_keyboard_enable_script = true;
        const char code[] = "setTimeout(function () { keyboard.press(4); }, 10);";
        script_eval(code, std::strlen(code), "<tick-test>");
        script_process();
        EXPECT_FALSE(script_holds(KEY_A));
        keyboard_tick_inc(KEYBOARD_TIME_TO_TICK(9));
        script_process();
        EXPECT_FALSE(script_holds(KEY_A));
        keyboard_tick_inc(KEYBOARD_TIME_TO_TICK(1));
        script_process();
        EXPECT_TRUE(script_holds(KEY_A));
        g_keyboard_enable_script = false;
    }
}

TEST(ScriptTick, ExposedClocksPreserveHighBits)
{
    const uint64_t tick = KEYBOARD_TIME_TO_TICK64((UINT64_C(1) << 32) + 123);
    libamp_test_set_tick(tick);
    script_reset_runtime();
    const char code[] =
        "if (keyboard.getTick() > 4294967295 && "
        "keyboard.getTime() === 4294967419 && "
        "Date.now() === 4294967419 && performance.now() === 4294967419) "
        "keyboard.press(4);";
    script_eval(code, std::strlen(code), "<tick-test>");
    EXPECT_TRUE(script_holds(KEY_A));
}
