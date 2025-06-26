#include <gtest/gtest.h>

#include "analog.h"
#include "keyboard.h"
#include "layer.h"
#include "math.h"

extern uint8_t shared_ep_send_buffer[64];
extern uint8_t keyboard_send_buffer[64];

TEST(Keyboard, KeyboardTask)
{
    for (int i = 0; i < ADVANCED_KEY_NUM; i++)
    {
        g_keyboard_advanced_keys[i].config.calibration_mode = KEY_AUTO_CALIBRATION_UNDEFINED;
        advanced_key_reset_range(&g_keyboard_advanced_keys[i], 2048);
    }
    for (int tick = 0; tick < 1000; tick++)
    {
        for (int i = 0; i < ANALOG_BUFFER_LENGTH; i++)
        {
            ringbuf_push(&adc_ringbuf[i], (cos(tick/100.f)+1)*1024);   
        }
        keyboard_task();
    }
}

TEST(Keyboard, Layer)
{
    for (int i = 0; i < ADVANCED_KEY_NUM; i++)
    {
        g_keyboard_advanced_keys[i].config.calibration_mode = KEY_AUTO_CALIBRATION_UNDEFINED;
        advanced_key_reset_range(&g_keyboard_advanced_keys[i], 2048);
    }

    EXPECT_EQ(g_keymap[0][10], layer_cache_get_keycode(10));
    // LAYER_MOMENTARY LAYER 1
    keyboard_advanced_key_update_state(&g_keyboard_advanced_keys[60],true);
    EXPECT_EQ(g_keymap[0][60], layer_cache_get_keycode(60));

    keyboard_advanced_key_update_state(&g_keyboard_advanced_keys[1],true);
    EXPECT_EQ(g_keymap[1][1], layer_cache_get_keycode(1));
    
    keyboard_advanced_key_update_state(&g_keyboard_advanced_keys[15],true);
    EXPECT_EQ(g_keymap[0][15], layer_cache_get_keycode(15));
    // LAYER_MOMENTARY LAYER 2
    keyboard_advanced_key_update_state(&g_keyboard_advanced_keys[54],true);
    EXPECT_EQ(g_keymap[1][54], layer_cache_get_keycode(54));

    keyboard_advanced_key_update_state(&g_keyboard_advanced_keys[0],true);
    EXPECT_EQ(g_keymap[2][0], layer_cache_get_keycode(0));

    keyboard_advanced_key_update_state(&g_keyboard_advanced_keys[15],false);
    EXPECT_EQ(g_keymap[0][15], layer_cache_get_keycode(15));
}

TEST(Keyboard, LayerWithSpecificKeycode)
{
    g_keymap[0][15] = (JOYSTICK_COLLECTION) | (0 << 8) | (0x01 << 13); 
    g_keymap[1][15] = (JOYSTICK_COLLECTION) | (1 << 8) | (0x01 << 13);
    layer_cache_refresh();
    for (int i = 0; i < ADVANCED_KEY_NUM; i++)
    {
        g_keyboard_advanced_keys[i].config.calibration_mode = KEY_AUTO_CALIBRATION_UNDEFINED;
        advanced_key_reset_range(&g_keyboard_advanced_keys[i], 2048);
    }

    keyboard_advanced_key_update_state(&g_keyboard_advanced_keys[15],false);
    EXPECT_EQ(g_keymap[0][15], layer_cache_get_keycode(15));
    
    keyboard_advanced_key_update_state(&g_keyboard_advanced_keys[15],true);
    EXPECT_EQ(g_keymap[0][15], layer_cache_get_keycode(15));

    keyboard_advanced_key_update_state(&g_keyboard_advanced_keys[15],false);
    EXPECT_EQ(g_keymap[0][15], layer_cache_get_keycode(15));

    EXPECT_EQ(g_keymap[0][10], layer_cache_get_keycode(10));
    // LAYER_MOMENTARY LAYER 1
    keyboard_advanced_key_update_state(&g_keyboard_advanced_keys[60],true);
    EXPECT_EQ(g_keymap[0][60], layer_cache_get_keycode(60));

    keyboard_advanced_key_update_state(&g_keyboard_advanced_keys[1],true);
    EXPECT_EQ(g_keymap[1][1], layer_cache_get_keycode(1));

    keyboard_advanced_key_update_state(&g_keyboard_advanced_keys[15],false);
    EXPECT_EQ(g_keymap[1][15], layer_cache_get_keycode(15));
    
    keyboard_advanced_key_update_state(&g_keyboard_advanced_keys[15],true);
    EXPECT_EQ(g_keymap[1][15], layer_cache_get_keycode(15));

    keyboard_advanced_key_update_state(&g_keyboard_advanced_keys[15],false);
    EXPECT_EQ(g_keymap[1][15], layer_cache_get_keycode(15));
    // LAYER_MOMENTARY LAYER 2
    keyboard_advanced_key_update_state(&g_keyboard_advanced_keys[54],true);
    EXPECT_EQ(g_keymap[1][54], layer_cache_get_keycode(54));

    keyboard_advanced_key_update_state(&g_keyboard_advanced_keys[0],true);
    EXPECT_EQ(g_keymap[2][0], layer_cache_get_keycode(0));

    keyboard_advanced_key_update_state(&g_keyboard_advanced_keys[15],false);
    EXPECT_EQ(g_keymap[1][15], layer_cache_get_keycode(15));

    // LAYER_MOMENTARY LAYER 1
    keyboard_advanced_key_update_state(&g_keyboard_advanced_keys[60],false);
    EXPECT_EQ(g_keymap[0][60], layer_cache_get_keycode(60));

    // LAYER_MOMENTARY LAYER 2
    keyboard_advanced_key_update_state(&g_keyboard_advanced_keys[54],false);
    EXPECT_EQ(g_keymap[0][54], layer_cache_get_keycode(54));

    keyboard_advanced_key_update_state(&g_keyboard_advanced_keys[15],false);
    EXPECT_EQ(g_keymap[0][15], layer_cache_get_keycode(15));
    
    keyboard_advanced_key_update_state(&g_keyboard_advanced_keys[15],true);
    EXPECT_EQ(g_keymap[0][15], layer_cache_get_keycode(15));

    keyboard_advanced_key_update_state(&g_keyboard_advanced_keys[15],false);
    EXPECT_EQ(g_keymap[0][15], layer_cache_get_keycode(15));
}

TEST(Keyboard, 6KROBuffer)
{
    g_keyboard_nkro_enable = false;
    keyboard_clear_buffer();
    keyboard_event_handler({KEY_A|(KEY_LEFT_CTRL << 8), KEYBOARD_EVENT_KEY_TRUE});
    keyboard_event_handler({KEY_B|(KEY_LEFT_ALT << 8), KEYBOARD_EVENT_KEY_TRUE});
    keyboard_event_handler({KEY_C|(KEY_LEFT_SHIFT << 8), KEYBOARD_EVENT_KEY_TRUE});
    keyboard_event_handler({KEY_D|(KEY_LEFT_GUI << 8), KEYBOARD_EVENT_KEY_TRUE});
    keyboard_event_handler({KEY_A|(KEY_RIGHT_CTRL << 8), KEYBOARD_EVENT_KEY_TRUE});
    keyboard_event_handler({KEY_B|(KEY_RIGHT_ALT << 8), KEYBOARD_EVENT_KEY_TRUE});
    keyboard_event_handler({KEY_C|(KEY_RIGHT_SHIFT << 8), KEYBOARD_EVENT_KEY_TRUE});
    keyboard_event_handler({KEY_D|(KEY_RIGHT_GUI << 8), KEYBOARD_EVENT_KEY_TRUE});
    keyboard_buffer_send();
    EXPECT_EQ(keyboard_send_buffer[0], 0xFF);
    EXPECT_EQ(keyboard_send_buffer[2], KEY_A);
    EXPECT_EQ(keyboard_send_buffer[3], KEY_B);
    EXPECT_EQ(keyboard_send_buffer[4], KEY_C);
    EXPECT_EQ(keyboard_send_buffer[5], KEY_D);
    EXPECT_EQ(keyboard_send_buffer[6], KEY_A);
    EXPECT_EQ(keyboard_send_buffer[7], KEY_B);
}

TEST(Keyboard, NKROBuffer)
{
    g_keyboard_nkro_enable = true;
    keyboard_clear_buffer();
    keyboard_event_handler({KEY_A|(KEY_LEFT_CTRL<<8), KEYBOARD_EVENT_KEY_TRUE});
    keyboard_event_handler({KEY_S|(KEY_LEFT_ALT<<8), KEYBOARD_EVENT_KEY_TRUE});
    keyboard_buffer_send();
    EXPECT_EQ(shared_ep_send_buffer[1], KEY_LEFT_CTRL|KEY_LEFT_ALT);
    EXPECT_EQ(shared_ep_send_buffer[KEY_A/8 + 2], BIT(4));
    EXPECT_EQ(shared_ep_send_buffer[KEY_S/8 + 2], BIT(KEY_S%8));
}