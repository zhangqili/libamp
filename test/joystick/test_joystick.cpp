#include <gtest/gtest.h>

#include "keyboard.h"
#include "joystick.h"
#include "layer.h"

extern uint8_t joystick_send_buffer[64];

TEST(Joystick, Buffer)
{
    joystick_buffer_clear();
    joystick_event_handler({JOYSTICK_COLLECTION|(0<<8), KEYBOARD_EVENT_KEY_TRUE});
    joystick_event_handler({JOYSTICK_COLLECTION|(9<<8), KEYBOARD_EVENT_KEY_TRUE});
    joystick_buffer_send();
    Joystick* joystick = (Joystick*)joystick_send_buffer;
    EXPECT_EQ(joystick->buttons[0], BIT(0));
    EXPECT_EQ(joystick->buttons[1], BIT(1));
}

TEST(Joystick, Axis)
{
    g_keymap[0][42] = JOYSTICK_COLLECTION | (0x01 << 13) | 0 << 8; 
    g_keymap[0][43] = JOYSTICK_COLLECTION | (0x02 << 13) | 1 << 8;
    g_keymap[0][44] = JOYSTICK_COLLECTION | (0x03 << 13) | 2 << 8;
    g_keymap[0][45] = JOYSTICK_COLLECTION | (0x07 << 13) | 3 << 8;

    advanced_key_update(&g_keyboard_advanced_keys[42], 0.6);
    advanced_key_update(&g_keyboard_advanced_keys[43], 0.7);
    advanced_key_update(&g_keyboard_advanced_keys[44], 0.8);
    advanced_key_update(&g_keyboard_advanced_keys[45], 0.9);

    joystick_buffer_clear();
    keyboard_advanced_key_event_handler(&g_keyboard_advanced_keys[42], 
        keyboard_make_event(&g_keyboard_advanced_keys[42].key, KEYBOARD_EVENT_KEY_TRUE));
    keyboard_advanced_key_event_handler(&g_keyboard_advanced_keys[43], 
        keyboard_make_event(&g_keyboard_advanced_keys[43].key, KEYBOARD_EVENT_KEY_TRUE));
    keyboard_advanced_key_event_handler(&g_keyboard_advanced_keys[44], 
        keyboard_make_event(&g_keyboard_advanced_keys[44].key, KEYBOARD_EVENT_KEY_TRUE));
    keyboard_advanced_key_event_handler(&g_keyboard_advanced_keys[45], 
        keyboard_make_event(&g_keyboard_advanced_keys[45].key, KEYBOARD_EVENT_KEY_TRUE));
    joystick_buffer_send();

    Joystick* joystick = (Joystick*)joystick_send_buffer;
    EXPECT_NEAR(joystick->axes[0], (int8_t)((0.6 - ANALOG_VALUE_MIN) / (float)ANALOG_VALUE_RANGE * JOYSTICK_MAX_VALUE), 1);
    EXPECT_NEAR(joystick->axes[1], (int8_t)((ANALOG_VALUE_MIN - 0.7) / (float)ANALOG_VALUE_RANGE * JOYSTICK_MAX_VALUE), 1);
    EXPECT_NEAR(joystick->axes[2], (int8_t)(((0.8 - ANALOG_VALUE_MIN) / (float)ANALOG_VALUE_RANGE * JOYSTICK_MAX_VALUE)*2 - JOYSTICK_MAX_VALUE), 1);
    EXPECT_NEAR(joystick->axes[3], (int8_t)(-(((0.9 - ANALOG_VALUE_MIN) / (float)ANALOG_VALUE_RANGE * JOYSTICK_MAX_VALUE)*2 - JOYSTICK_MAX_VALUE)), 1);
}