#include <gtest/gtest.h>

#include "gamepad.h"
#include "test_fixture.h"

namespace {

Keycode gamepad_keycode(uint8_t subcode)
{
    return ((Keycode)subcode << 8) | GAMEPAD_COLLECTION;
}

} // namespace

TEST(Gamepad, ButtonsTriggersAxesAndSend)
{
    Key key = {};
    gamepad_buffer_clear();

    KeyboardEvent button = MK_EVENT(gamepad_keycode(GAMEPAD_A), KEYBOARD_EVENT_KEY_DOWN, &key);
    gamepad_event_handler(button);
    EXPECT_TRUE((bool)g_keyboard_report_flags.gamepad);
    gamepad_add_buffer(button);

    gamepad_add_buffer(MK_EVENT(gamepad_keycode(GAMEPAD_LT), KEYBOARD_EVENT_NO_EVENT, &key));
    gamepad_set_axis(gamepad_keycode(GAMEPAD_LXP), ANALOG_VALUE_MAX);
    gamepad_set_axis(gamepad_keycode(GAMEPAD_LXP), ANALOG_VALUE_MAX);
    gamepad_set_axis(gamepad_keycode(GAMEPAD_RTA), ANALOG_VALUE_MAX);

    ASSERT_EQ(0, gamepad_buffer_send());
    Gamepad* report = reinterpret_cast<Gamepad*>(gamepad_send_buffer);

    EXPECT_EQ(0, report->report_id);
    EXPECT_EQ(static_cast<uint8_t>(sizeof(Gamepad)), report->report_size);
    EXPECT_TRUE((report->buttons & (1u << GAMEPAD_A)) != 0);
    EXPECT_EQ(255, report->lt);
    EXPECT_EQ(255, report->rt);
    EXPECT_EQ(32767, report->lx);
}

TEST(Gamepad, ClearResetsReportState)
{
    Key key = {};
    gamepad_add_buffer(MK_EVENT(gamepad_keycode(GAMEPAD_B), KEYBOARD_EVENT_NO_EVENT, &key));
    gamepad_set_axis(gamepad_keycode(GAMEPAD_LYN), ANALOG_VALUE_MAX);

    gamepad_buffer_clear();
    ASSERT_EQ(0, gamepad_buffer_send());
    Gamepad* report = reinterpret_cast<Gamepad*>(gamepad_send_buffer);

    EXPECT_EQ(0, report->buttons);
    EXPECT_EQ(0, report->lt);
    EXPECT_EQ(0, report->rt);
    EXPECT_EQ(0, report->ly);
}
