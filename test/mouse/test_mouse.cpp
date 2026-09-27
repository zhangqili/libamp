#include <gtest/gtest.h>

#include <cstring>

#include "advanced_key.h"
#include "keyboard.h"
#include "layer.h"
#include "mouse.h"
#include "test_fixture.h"

TEST(Mouse, Buffer)
{
    mouse_report_clear();
    mouse_report_add(MK_EVENT(MOUSE_COLLECTION|(MOUSE_LBUTTON<<8), KEYBOARD_EVENT_NO_EVENT, NULL));
    mouse_report_add(MK_EVENT(MOUSE_COLLECTION|(MOUSE_RBUTTON<<8), KEYBOARD_EVENT_NO_EVENT, NULL));
    mouse_report_add(MK_EVENT(MOUSE_COLLECTION|(MOUSE_WHEEL_DOWN<<8), KEYBOARD_EVENT_NO_EVENT, NULL));
    mouse_report_send();
    MouseReport* mouse= (MouseReport*)shared_ep_send_buffer;
    EXPECT_EQ(mouse->buttons, BIT(0)|BIT(1));
    EXPECT_EQ(mouse->v, -1);
}

namespace {

// mouse_set_axis() turns the travel into a fractional speed and only emits a
// whole movement step once the accumulated fraction crosses an integer (see
// should_move()). A tick that is past that point is therefore needed for a
// moderate travel to produce a non-zero step.
constexpr uint32_t MOUSE_MOVE_TICK = 7;

// Bind an advanced key to a mouse movement keycode. The keymap cache is set
// directly, mirroring the other tests, so the binding does not depend on the
// currently selected layer or on a stale layer lock.
void bind_advanced_key_to_mouse_move(AdvancedKey* advanced_key, uint8_t mouse_keycode)
{
    g_keymap_cache[advanced_key->key.id] = MOUSE_COLLECTION | (mouse_keycode << 8);
}

// Configure an advanced key in rapid trigger mode whose trigger distance is
// far out of reach, so none of the travels used below can trigger it.
void configure_untriggerable_advanced_key(AdvancedKey* advanced_key)
{
    const uint16_t id = advanced_key->key.id;
    std::memset(advanced_key, 0, sizeof(*advanced_key));
    advanced_key->key.id = id;
    advanced_key->config.mode = ADVANCED_KEY_ANALOG_RAPID_MODE;
    advanced_key->config.calibration_mode = ADVANCED_KEY_NO_CALIBRATION;
    advanced_key->config.trigger_distance = A_ANTI_NORM(0.50);
    advanced_key->config.release_distance = A_ANTI_NORM(0.08);
    advanced_key->config.upper_deadzone = A_ANTI_NORM(0.10);
    advanced_key->config.lower_deadzone = A_ANTI_NORM(0.20);
}

// Feed `value` to the key, then build and send a report and return the mouse
// report that was produced. Both the internal mouse report and the outgoing
// buffer are cleared first, so a poll without movement yields an all-zero
// report instead of the result of the previous poll.
MouseReport report_mouse_for_travel(AdvancedKey* advanced_key, AnalogValue value)
{
    keyboard_advanced_key_update(advanced_key, value);
    keyboard_report_clear_all();
    libamp_test_clear_output_buffers();
    g_keyboard_report_flags.mouse = false;
    g_keyboard_tick = MOUSE_MOVE_TICK;
    keyboard_report_build();
    mouse_report_send();

    MouseReport report;
    std::memcpy(&report, shared_ep_send_buffer, sizeof(report));
    return report;
}

} // namespace

// A mouse movement binding on an advanced key follows the analog travel of the
// key: as soon as the travel leaves the upper deadzone the pointer moves, even
// when the advanced key itself has not triggered yet.
TEST(Mouse, AdvancedKeyMoveFollowsTravelBeforeTrigger)
{
    for (size_t i = 0; i < KEY_BITMAP_SIZE; i++)
    {
        g_keyboard_bitmap[i] = 0;
    }

    AdvancedKey* advanced_key = &g_keyboard_advanced_keys[0];
    configure_untriggerable_advanced_key(advanced_key);
    bind_advanced_key_to_mouse_move(advanced_key, MOUSE_MOVE_RIGHT);

    // Travel inside the upper deadzone must not move the pointer.
    MouseReport report = report_mouse_for_travel(advanced_key, A_ANTI_NORM(0.05));
    EXPECT_FALSE(advanced_key->key.state);
    EXPECT_EQ(report.x, 0);
    EXPECT_EQ(report.y, 0);

    // Travel beyond the upper deadzone must move the pointer to the right even
    // though the advanced key has not triggered.
    report = report_mouse_for_travel(advanced_key, A_ANTI_NORM(0.20));
    EXPECT_FALSE(advanced_key->key.state) << "the advanced key must not have triggered at this travel";
    EXPECT_GT(advanced_key_get_effective_value(advanced_key), ANALOG_VALUE_MIN);
    EXPECT_GT(report.x, 0) << "mouse movement must follow the travel above the upper deadzone";
    EXPECT_EQ(report.y, 0);

    // Returning inside the upper deadzone must stop the pointer.
    report = report_mouse_for_travel(advanced_key, A_ANTI_NORM(0.05));
    EXPECT_FALSE(advanced_key->key.state);
    EXPECT_EQ(report.x, 0);
    EXPECT_EQ(report.y, 0);
}
