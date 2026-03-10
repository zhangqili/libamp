#include <gtest/gtest.h>

#include "layer.h"
#include "keyboard.h"

class LayerTest : public ::testing::Test {
protected:
    void SetUp() override {
        for (int i = 0; i < 16; i++) {
            layer_reset(i);
        }
        
        for (int i = 0; i < TOTAL_KEY_NUM; i++) {
            g_keymap_lock[i] = false;
            g_keymap_cache[i] = KEY_NO_EVENT;
        }
    }

    void TearDown() override {
    }
};

TEST_F(LayerTest, StateManagement) {
    EXPECT_EQ(layer_get(), 0);
    EXPECT_EQ(g_current_layer, 0);

    layer_set(1);
    EXPECT_EQ(layer_get(), 1);
    EXPECT_EQ(g_current_layer, 1);

    layer_set(3);
    EXPECT_EQ(layer_get(), 3);

    layer_toggle(3);
    EXPECT_EQ(layer_get(), 1);

    layer_reset(1);
    EXPECT_EQ(layer_get(), 0);
}

TEST_F(LayerTest, TransparentKeycodeResolution) {
    const uint16_t test_key_id = 5;
    
    g_keymap[0][test_key_id] = 0x0004;
    g_keymap[1][test_key_id] = KEY_TRANSPARENT;
    g_keymap[2][test_key_id] = 0x0005;

    EXPECT_EQ(layer_get_keycode(test_key_id, 0), 0x0004);
    EXPECT_EQ(layer_get_keycode(test_key_id, 1), 0x0004);
    EXPECT_EQ(layer_get_keycode(test_key_id, 2), 0x0005);
}

TEST_F(LayerTest, EventHandlerMomentary) {
    KeyboardEvent event;
    event.is_virtual = true;
    
    uint8_t target_layer = 2;
    event.keycode = (LAYER_MOMENTARY << 12) | (target_layer << 8);

    event.event = KEYBOARD_EVENT_KEY_DOWN;
    layer_event_handler(event);
    EXPECT_EQ(layer_get(), target_layer);

    event.event = KEYBOARD_EVENT_KEY_UP;
    layer_event_handler(event);
    EXPECT_EQ(layer_get(), 0);
}

TEST_F(LayerTest, CacheAndLock) {
    const uint16_t test_key_id = 10;
    Key mock_key;
    mock_key.id = test_key_id;
    
    KeyboardEvent event;
    event.key = &mock_key;

    g_keymap[0][test_key_id] = 0x0004;
    g_keymap[1][test_key_id] = 0x0005;

    layer_set(0);
    layer_cache_refresh();
    EXPECT_EQ(layer_cache_get_keycode(test_key_id), 0x0004);

    event.event = KEYBOARD_EVENT_KEY_DOWN;
    layer_lock_handler(event);
    EXPECT_TRUE(g_keymap_lock[test_key_id]);

    layer_set(1);
    layer_cache_refresh();
    
    EXPECT_EQ(layer_cache_get_keycode(test_key_id), 0x0004);

    event.event = KEYBOARD_EVENT_KEY_UP;
    layer_lock_handler(event);
    EXPECT_FALSE(g_keymap_lock[test_key_id]);

    EXPECT_EQ(layer_cache_get_keycode(test_key_id), 0x0005);
}