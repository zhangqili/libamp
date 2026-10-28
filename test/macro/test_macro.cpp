#include "test_fixture.h"
#include <gtest/gtest.h>

#include "macro.h"

TEST(Macro, InitClearsRuntimeStateAndKeepsActionStorage)
{
    g_macros[0].state = MACRO_STATE_PLAYING_ONCE;
    g_macros[0].index = 7;
    g_macros[0].length = 3;

    macro_init();

    EXPECT_EQ(MACRO_STATE_IDLE, g_macros[0].state);
    EXPECT_EQ(0, g_macros[0].index);
    EXPECT_EQ(0, g_macros[0].length);
    ASSERT_NE(nullptr, g_macros[0].actions);
}

TEST(Macro, RecordWritesTimedActionsAndTerminator)
{
    Macro *macro = &g_macros[0];
    Key key = {};
    key.id = 0;

    libamp_test_set_tick(100);
    macro_start_record(macro);

    KeyboardEvent down = MK_EVENT(KEY_A, KEYBOARD_EVENT_KEY_DOWN, &key);
    libamp_test_set_tick(125);
    macro_record(macro, down);

    KeyboardEvent up = MK_EVENT(KEY_A, KEYBOARD_EVENT_KEY_UP, &key);
    libamp_test_set_tick(140);
    macro_record(macro, up);

    libamp_test_set_tick(150);
    macro_stop_record(macro);

    EXPECT_EQ(MACRO_STATE_IDLE, macro->state);
    EXPECT_EQ(0, macro->index);
    EXPECT_EQ(25U, macro->actions[0].delay);
    EXPECT_EQ(KEY_A, macro->actions[0].event.keycode);
    EXPECT_EQ(KEYBOARD_EVENT_KEY_DOWN, macro->actions[0].event.event);
    EXPECT_EQ(40U, macro->actions[1].delay);
    EXPECT_EQ(KEYBOARD_EVENT_KEY_UP, macro->actions[1].event.event);
    EXPECT_EQ(50U, macro->actions[2].delay);
    EXPECT_EQ(KEY_NO_EVENT, macro->actions[2].event.keycode);
}

TEST(Macro, PlayOnceReturnsToIdleAtTerminator)
{
    Macro *macro = &g_macros[0];
    Key key = {};
    key.id = 0;
    macro->actions[0].delay = 5;
    macro->actions[0].event = MK_EVENT(KEY_A, KEYBOARD_EVENT_KEY_DOWN, &key);
    macro->actions[1].delay = 10;
    macro->actions[1].event = MK_EVENT(KEY_A, KEYBOARD_EVENT_KEY_UP, &key);
    macro->actions[2].delay = 15;
    macro->actions[2].event = MK_EVENT(KEY_NO_EVENT, KEYBOARD_EVENT_NO_EVENT, &key);

    libamp_test_set_tick(100);
    macro_start_play_once(macro);
    libamp_test_set_tick(115);

    macro_process();

    EXPECT_EQ(MACRO_STATE_IDLE, macro->state);
    EXPECT_EQ(0, macro->index);
}

TEST(Macro, PlaybackWaitsForDeadlineAcrossLowWordWrap)
{
    Macro *macro = &g_macros[0];
    Key key = {};
    macro->actions[0] = {20, MK_EVENT(KEY_A, KEYBOARD_EVENT_KEY_DOWN, &key)};
    macro->actions[1] = {30, MK_EVENT(KEY_NO_EVENT, KEYBOARD_EVENT_NO_EVENT, &key)};
    libamp_test_set_tick(UINT32_MAX - 9u);
    macro_start_play_once(macro);

    macro_process();
    EXPECT_EQ(0u, macro->index);
    keyboard_tick_inc(19);
    macro_process();
    EXPECT_EQ(0u, macro->index);
    keyboard_tick_inc(1);
    macro_process();
    EXPECT_EQ(1u, macro->index);
    EXPECT_EQ(MACRO_STATE_PLAYING_ONCE, macro->state);
    keyboard_tick_inc(10);
    macro_process();
    EXPECT_EQ(MACRO_STATE_IDLE, macro->state);
}
