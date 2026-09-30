#include <gtest/gtest.h>

#include <cstring>

#include "advanced_key.h"
#include "joystick.h"
#include "keyboard.h"
#include "layer.h"
#include "test_fixture.h"

TEST(Joystick, Buffer)
{
    joystick_report_clear();
    joystick_report_add(MK_EVENT(JOYSTICK_COLLECTION|(0<<8), KEYBOARD_EVENT_NO_EVENT, NULL));
    joystick_report_add(MK_EVENT(JOYSTICK_COLLECTION|(9<<8), KEYBOARD_EVENT_NO_EVENT, NULL));
    joystick_report_send();
    JoystickReport* joystick = (JoystickReport*)shared_ep_send_buffer;
    EXPECT_EQ(joystick->buttons[0], BIT(0));
    EXPECT_EQ(joystick->buttons[1], BIT(1));
}

TEST(Joystick, Axis)
{
    g_keymap[0][42] = JOYSTICK_COLLECTION | (0x01 << 13) | 0 << 8; 
    g_keymap[0][43] = JOYSTICK_COLLECTION | (0x02 << 13) | 1 << 8;
    g_keymap[0][44] = JOYSTICK_COLLECTION | (0x03 << 13) | 2 << 8;
    g_keymap[0][45] = JOYSTICK_COLLECTION | (0x07 << 13) | 3 << 8;
    layer_cache_refresh();

    advanced_key_update(&g_keyboard_advanced_keys[42], A_ANTI_NORM(0.6));
    advanced_key_update(&g_keyboard_advanced_keys[43], A_ANTI_NORM(0.7));
    advanced_key_update(&g_keyboard_advanced_keys[44], A_ANTI_NORM(0.8));
    advanced_key_update(&g_keyboard_advanced_keys[45], A_ANTI_NORM(0.9));

    joystick_report_clear();
    joystick_report_add(
        MK_EVENT(layer_cache_get_keycode(g_keyboard_advanced_keys[42].key.id), KEYBOARD_EVENT_NO_EVENT, &g_keyboard_advanced_keys[42]));
    joystick_report_add(
        MK_EVENT(layer_cache_get_keycode(g_keyboard_advanced_keys[43].key.id), KEYBOARD_EVENT_NO_EVENT, &g_keyboard_advanced_keys[43]));
    joystick_report_add(
        MK_EVENT(layer_cache_get_keycode(g_keyboard_advanced_keys[44].key.id), KEYBOARD_EVENT_NO_EVENT, &g_keyboard_advanced_keys[44]));
    joystick_report_add(
        MK_EVENT(layer_cache_get_keycode(g_keyboard_advanced_keys[45].key.id), KEYBOARD_EVENT_NO_EVENT, &g_keyboard_advanced_keys[45]));
    joystick_report_send();

    JoystickReport* joystick = (JoystickReport*)shared_ep_send_buffer;
    EXPECT_NEAR(joystick->axes[0], (int8_t)((A_ANTI_NORM(0.6) - ANALOG_VALUE_MIN) / (float)ANALOG_VALUE_RANGE * JOYSTICK_MAX_VALUE), 1);
    EXPECT_NEAR(joystick->axes[1], (int8_t)((ANALOG_VALUE_MIN - A_ANTI_NORM(0.7)) / (float)ANALOG_VALUE_RANGE * JOYSTICK_MAX_VALUE), 1);
    EXPECT_NEAR(joystick->axes[2], (int8_t)(((A_ANTI_NORM(0.8) - ANALOG_VALUE_MIN) / (float)ANALOG_VALUE_RANGE * JOYSTICK_MAX_VALUE)*2 - JOYSTICK_MAX_VALUE), 1);
    EXPECT_NEAR(joystick->axes[3], (int8_t)(-(((A_ANTI_NORM(0.9) - ANALOG_VALUE_MIN) / (float)ANALOG_VALUE_RANGE * JOYSTICK_MAX_VALUE)*2 - JOYSTICK_MAX_VALUE)), 1);
}

namespace {

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

// Feed `value` to the key, then build and send a report and return the joystick
// report that was produced. The internal report is cleared first so every call
// starts from the resting position instead of accumulating into the axis.
JoystickReport report_axis_for_travel(AdvancedKey* advanced_key, AnalogValue value)
{
    keyboard_advanced_key_update(advanced_key, value);
    joystick_report_clear();
    libamp_test_clear_output_buffers();
    g_keyboard_report_flags.joystick = false;
    keyboard_report_build();
    joystick_report_send();

    JoystickReport report;
    std::memcpy(&report, shared_ep_send_buffer, sizeof(report));
    return report;
}

} // namespace

// A joystick axis binding on an advanced key follows the analog travel of the
// key: the axis moves as the key is pressed, even when the advanced key itself
// has not triggered yet.
TEST(Joystick, AdvancedKeyAxisFollowsTravelBeforeTrigger)
{
    for (size_t i = 0; i < KEY_BITMAP_SIZE; i++)
    {
        g_keyboard_bitmap[i] = 0;
    }

    AdvancedKey* advanced_key = &g_keyboard_advanced_keys[0];
    configure_untriggerable_advanced_key(advanced_key);
    // Axis 0, positive direction.
    g_keymap_cache[advanced_key->key.id] = JOYSTICK_COLLECTION | (0x01 << 13);

    // At rest the axis must stay centered.
    JoystickReport report = report_axis_for_travel(advanced_key, ANALOG_VALUE_MIN);
    EXPECT_FALSE(advanced_key->key.state);
    EXPECT_EQ(report.axes[0], 0);

    // Traveling further must move the axis although the advanced key has not
    // triggered.
    report = report_axis_for_travel(advanced_key, A_ANTI_NORM(0.20));
    EXPECT_FALSE(advanced_key->key.state) << "the advanced key must not have triggered at this travel";
    const JoystickAxis first_axis = report.axes[0];
    EXPECT_GT(first_axis, 0) << "the axis must follow the travel before the advanced key triggers";

    // More travel must push the axis further.
    report = report_axis_for_travel(advanced_key, A_ANTI_NORM(0.40));
    EXPECT_FALSE(advanced_key->key.state);
    EXPECT_GT(report.axes[0], first_axis);

    // Releasing back to rest must recenter the axis.
    report = report_axis_for_travel(advanced_key, ANALOG_VALUE_MIN);
    EXPECT_FALSE(advanced_key->key.state);
    EXPECT_EQ(report.axes[0], 0);
}
