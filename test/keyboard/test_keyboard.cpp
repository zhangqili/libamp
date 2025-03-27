#include <gtest/gtest.h>

#include "analog.h"
#include "keyboard.h"
#include "layer.h"
#include "math.h"

TEST(Keyboard, KeyboardTask)
{
    keyboard_init();
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
    keyboard_init();
    for (int i = 0; i < ADVANCED_KEY_NUM; i++)
    {
        g_keyboard_advanced_keys[i].config.calibration_mode = KEY_AUTO_CALIBRATION_UNDEFINED;
        advanced_key_reset_range(&g_keyboard_advanced_keys[i], 2048);
    }
    keyboard_advanced_key_update_state(&g_keyboard_advanced_keys[60],true);
    EXPECT_EQ(g_keymap[0][60], layer_cache_get_keycode(60));
    keyboard_advanced_key_update_state(&g_keyboard_advanced_keys[1],true);
    EXPECT_EQ(g_keymap[1][1], layer_cache_get_keycode(1));
    keyboard_advanced_key_update_state(&g_keyboard_advanced_keys[15],true);
    EXPECT_EQ(g_keymap[0][15], layer_cache_get_keycode(15));
    keyboard_advanced_key_update_state(&g_keyboard_advanced_keys[54],true);
    EXPECT_EQ(g_keymap[1][54], layer_cache_get_keycode(54));
}