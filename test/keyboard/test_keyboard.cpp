#include <gtest/gtest.h>

#include "analog.h"
#include "keyboard.h"
#include "layer.h"
#include "math.h"
#include "test_fixture.h"

TEST(Keyboard, DebounceMatchesEveryCounterAndStateClass)
{
#if DEBOUNCE_PRESS > 0 || DEBOUNCE_RELEASE > 0
    for (uint8_t physical : {0, 1, 2, 255}) {
        for (uint8_t report : {0, 1, 2, 255}) {
            for (int counter = INT8_MIN; counter <= INT8_MAX; counter++) {
                Key key = {};
                key.state = physical;
                key.report_state = report;
                key.debounce = counter;
                bool expected = report;
                int8_t next_counter = counter;
                if (counter < 0) {
                    next_counter = counter + 1;
                } else if (report != 0 && physical == 0) {
#if DEBOUNCE_RELEASE_EAGER
                    expected = false;
                    next_counter = -(int8_t)DEBOUNCE_RELEASE;
#else
                    next_counter = (int8_t)(counter + 1);
                    if (next_counter >= (int8_t)DEBOUNCE_RELEASE) {
                        expected = false;
                        next_counter = 0;
                    }
#endif
                } else if (report == 0 && physical != 0) {
#if DEBOUNCE_PRESS_EAGER
                    expected = true;
                    next_counter = -(int8_t)DEBOUNCE_PRESS;
#else
                    next_counter = (int8_t)(counter + 1);
                    if (next_counter >= (int8_t)DEBOUNCE_PRESS) {
                        expected = true;
                        next_counter = 0;
                    }
#endif
                } else if (counter > 0) {
                    next_counter = 0;
                }
                ASSERT_EQ(expected, keyboard_key_debounce(&key));
                ASSERT_EQ(next_counter, key.debounce);
                ASSERT_EQ(physical, key.state);
                ASSERT_EQ(report, key.report_state);
            }
        }
    }
#else
    for (uint8_t physical : {0, 1, 2, 255}) {
        Key key = {};
        key.state = physical;
        EXPECT_EQ(physical != 0, keyboard_key_debounce(&key));
    }
#endif
}

TEST(Keyboard, EveryBindingPreservesItsUnchangedDispatchPolicy)
{
    for (uint32_t code = 0; code <= UINT16_MAX; code++) {
#if KEYBOARD_SKIP_UNCHANGED_EVENTS
        const uint8_t main = code & 0xff;
        const uint8_t sub = code >> 8;
        bool expected = main == KEY_USER;
#ifdef MOUSE_ENABLE
        expected |= main == MOUSE_COLLECTION && sub >= 0x10;
#endif
#ifdef JOYSTICK_ENABLE
        expected |= main == JOYSTICK_COLLECTION && sub >= 0x20;
#endif
#ifdef GAMEPAD_ENABLE
        expected |= main == GAMEPAD_COLLECTION && sub > GAMEPAD_RT;
#endif
#else
        const bool expected = true;
#endif
        ASSERT_EQ(expected, keyboard_keycode_dispatches_unchanged((Keycode)code)) << code;
        for (uint8_t type : {KEYBOARD_EVENT_KEY_FALSE, KEYBOARD_EVENT_KEY_UP,
                             KEYBOARD_EVENT_KEY_TRUE, KEYBOARD_EVENT_KEY_DOWN}) {
            const KeyboardEvent event = MK_VIRTUAL_EVENT(code, type, nullptr);
            ASSERT_EQ(expected || EVENT_CHANGED(type), keyboard_event_needs_dispatch(event)) << code << ',' << unsigned(type);
        }
    }
}

TEST(Keyboard, UnchangedInputRefreshesAxesButNotButtons)
{
    const struct {
        Keycode binding;
        uint8_t report_flags;
    } cases[] = {
        {KEY_A, 0},
#ifdef MOUSE_ENABLE
        {MOUSE_COLLECTION | (MOUSE_LBUTTON << 8), 0},
        {MOUSE_COLLECTION | (MOUSE_MOVE_RIGHT << 8), 1u << MOUSE_REPORT_FLAG},
#endif
#ifdef JOYSTICK_ENABLE
        {JOYSTICK_COLLECTION, 0},
        {JOYSTICK_COLLECTION | (1u << 13), 1u << JOYSTICK_REPORT_FLAG},
#endif
#ifdef GAMEPAD_ENABLE
        {GAMEPAD_COLLECTION | (GAMEPAD_A << 8), 0},
        {GAMEPAD_COLLECTION | (GAMEPAD_LXP << 8), 1u << 5},
#endif
    };
    AdvancedKey *key = &g_keyboard_advanced_keys[0];
    for (const auto &item : cases) {
        for (bool pressed : {false, true}) {
            SCOPED_TRACE(item.binding);
            SCOPED_TRACE(pressed);
            layer_init();
            g_keymap[0][0] = item.binding;
            layer_cache_refresh();
            advanced_key_init(key, 0);
            key->config.mode = ADVANCED_KEY_DIGITAL_MODE;
            key->key.state = key->key.report_state = pressed;
            g_keyboard_report_flags.raw = 0;

            EXPECT_FALSE(keyboard_advanced_key_update(key, pressed ? ANALOG_VALUE_MAX : 0));
            EXPECT_EQ(item.report_flags, g_keyboard_report_flags.raw);
            EXPECT_EQ(pressed || item.report_flags != 0, key->key.report_state);
        }
    }
}


void keyboard_advanced_key_update_state(AdvancedKey *key, bool state)
{
    keyboard_event_handler(MK_EVENT(layer_cache_get_keycode(key->key.id), 
                                            advanced_key_update_state(key, state) ? 
                                            key->key.state ? KEYBOARD_EVENT_KEY_DOWN : KEYBOARD_EVENT_KEY_UP
                                            : key->key.state ? KEYBOARD_EVENT_KEY_TRUE : KEYBOARD_EVENT_KEY_FALSE ,
                                            key));
}

TEST(Keyboard, KeyboardTask)
{
    for (int i = 0; i < ADVANCED_KEY_NUM; i++)
    {
        g_keyboard_advanced_keys[i].config.calibration_mode = ADVANCED_KEY_AUTO_CALIBRATION_UNDEFINED;
        advanced_key_reset_range(&g_keyboard_advanced_keys[i], 2048);
    }
    for (int tick = 0; tick < 1000; tick++)
    {
        for (int i = 0; i < ANALOG_BUFFER_LENGTH; i++)
        {
            ringbuf_push(&g_adc_ringbufs[i], (cos(tick/100.f)+1)*1024);   
        }
        keyboard_task();
    }
}

TEST(Keyboard, Layer)
{
    for (int i = 0; i < ADVANCED_KEY_NUM; i++)
    {
        g_keyboard_advanced_keys[i].config.calibration_mode = ADVANCED_KEY_AUTO_CALIBRATION_UNDEFINED;
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
        g_keyboard_advanced_keys[i].config.calibration_mode = ADVANCED_KEY_AUTO_CALIBRATION_UNDEFINED;
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
#ifdef MIXED_KRO_ENABLE
    g_keyboard_config.nkro = true;
#else
    g_keyboard_config.nkro = false;
#endif
    keyboard_report_clear_all();
    keyboard_event_report_handler(MK_EVENT(KEY_A|(KEY_LEFT_CTRL << 8), KEYBOARD_EVENT_NO_EVENT, NULL));
    keyboard_event_report_handler(MK_EVENT(KEY_B|(KEY_LEFT_ALT << 8), KEYBOARD_EVENT_NO_EVENT, NULL));
    keyboard_event_report_handler(MK_EVENT(KEY_C|(KEY_LEFT_SHIFT << 8), KEYBOARD_EVENT_NO_EVENT, NULL));
    keyboard_event_report_handler(MK_EVENT(KEY_D|(KEY_LEFT_GUI << 8), KEYBOARD_EVENT_NO_EVENT, NULL));
    keyboard_event_report_handler(MK_EVENT(KEY_A|(KEY_RIGHT_CTRL << 8), KEYBOARD_EVENT_NO_EVENT, NULL));
    keyboard_event_report_handler(MK_EVENT(KEY_B|(KEY_RIGHT_ALT << 8), KEYBOARD_EVENT_NO_EVENT, NULL));
    keyboard_report_send();
    EXPECT_EQ(keyboard_send_buffer[0], 0x5F);
    EXPECT_EQ(keyboard_send_buffer[2], KEY_A);
    EXPECT_EQ(keyboard_send_buffer[3], KEY_B);
    EXPECT_EQ(keyboard_send_buffer[4], KEY_C);
    EXPECT_EQ(keyboard_send_buffer[5], KEY_D);
    EXPECT_EQ(keyboard_send_buffer[6], KEY_A);
    EXPECT_EQ(keyboard_send_buffer[7], KEY_B);
#ifdef MIXED_KRO_ENABLE
    EXPECT_EQ(shared_ep_send_buffer[0], REPORT_ID_NKRO);
    for (size_t i = 1; i < sizeof(KeyboardNKROReport); i++)
    {
        EXPECT_EQ(shared_ep_send_buffer[i], 0);
    }
#endif
}

#ifndef MIXED_KRO_ENABLE
TEST(Keyboard, NKROBuffer)
{
    g_keyboard_config.nkro = true;
    keyboard_report_clear_all();
    keyboard_event_report_handler(MK_EVENT(KEY_A|(KEY_LEFT_CTRL<<8), KEYBOARD_EVENT_NO_EVENT, NULL));
    keyboard_event_report_handler(MK_EVENT(KEY_S|(KEY_LEFT_ALT<<8), KEYBOARD_EVENT_NO_EVENT, NULL));
    keyboard_report_send();
    EXPECT_EQ(shared_ep_send_buffer[1], KEY_LEFT_CTRL|KEY_LEFT_ALT);
    EXPECT_NE(shared_ep_send_buffer[KEY_A/8 + 2] & BIT(KEY_A % 8), 0);
    EXPECT_NE(shared_ep_send_buffer[KEY_S/8 + 2] & BIT(KEY_S % 8), 0);
}
#endif

#ifdef MIXED_KRO_ENABLE
TEST(Keyboard, MixedKroNkroSplitAndRelease)
{
    const Keycode keys[] = {KEY_A, KEY_S, KEY_D, KEY_F, KEY_G, KEY_H, KEY_J};

    g_keyboard_config.nkro = true;
    keyboard_report_clear_all();
    for (Keycode key : keys)
    {
        keyboard_event_report_handler(MK_EVENT(key, KEYBOARD_EVENT_NO_EVENT, NULL));
    }
    ASSERT_EQ(keyboard_report_send(), 0);

    for (size_t i = 0; i < 6; i++)
    {
        EXPECT_EQ(keyboard_send_buffer[i + 2], keys[i]);
    }
    EXPECT_EQ(shared_ep_send_buffer[0], REPORT_ID_NKRO);
    EXPECT_EQ(shared_ep_send_buffer[1], 0);
    for (size_t i = 0; i < 6; i++)
    {
        EXPECT_EQ(shared_ep_send_buffer[keys[i] / 8 + 2] & BIT(keys[i] % 8), 0);
    }
    EXPECT_NE(shared_ep_send_buffer[KEY_J / 8 + 2] & BIT(KEY_J % 8), 0);

    keyboard_report_clear_all();
    ASSERT_EQ(keyboard_report_send(), 0);
    for (size_t i = 0; i < sizeof(Keyboard6KROReport) - 1; i++)
    {
        EXPECT_EQ(keyboard_send_buffer[i], 0);
    }
    EXPECT_EQ(shared_ep_send_buffer[0], REPORT_ID_NKRO);
    for (size_t i = 1; i < sizeof(KeyboardNKROReport); i++)
    {
        EXPECT_EQ(shared_ep_send_buffer[i], 0);
    }
}

TEST(Keyboard, MixedKroNkroKeepsStandaloneModifiersOutOfNkro)
{
    g_keyboard_config.nkro = true;
    keyboard_report_clear_all();
    keyboard_event_report_handler(MK_EVENT(KEY_LEFT_CTRL << 8, KEYBOARD_EVENT_NO_EVENT, NULL));
    ASSERT_EQ(keyboard_report_send(), 0);

    EXPECT_EQ(keyboard_send_buffer[0], KEY_LEFT_CTRL);
    EXPECT_EQ(shared_ep_send_buffer[0], REPORT_ID_NKRO);
    for (size_t i = 1; i < sizeof(KeyboardNKROReport); i++)
    {
        EXPECT_EQ(shared_ep_send_buffer[i], 0);
    }
}
#endif

TEST(Keyboard, DebouncePress)
{
    static Key key;
    key_update(&key, true);
#if DEBOUNCE_PRESS >= 2
#if DEBOUNCE_PRESS_EAGER
    for (int i = 0; i < DEBOUNCE_PRESS/2-1; i++)
    {        
        key.report_state = keyboard_key_debounce(&key);
    }
    EXPECT_TRUE(key.report_state);
    key_update(&key, false);
    for (int i = 0; i < 1; i++)
    {        
        key.report_state = keyboard_key_debounce(&key);
    }
    EXPECT_TRUE(key.report_state);
    key_update(&key, true);
    for (int i = 0; i < DEBOUNCE_PRESS-DEBOUNCE_PRESS/2-1; i++)
    {        
        key.report_state = keyboard_key_debounce(&key);
    }
    EXPECT_TRUE(key.report_state);
    for (int i = 0; i < 1; i++)
    {        
        key.report_state = keyboard_key_debounce(&key);
    }
    EXPECT_TRUE(key.report_state);
#else
    for (int i = 0; i < DEBOUNCE_PRESS/2-1; i++)
    {        
        key.report_state = keyboard_key_debounce(&key);
    }
    EXPECT_FALSE(key.report_state);
    key_update(&key, false);
    for (int i = 0; i < 1; i++)
    {        
        key.report_state = keyboard_key_debounce(&key);
    }
    EXPECT_FALSE(key.report_state);
    key_update(&key, true);
    for (int i = 0; i < DEBOUNCE_PRESS/2-1; i++)
    {        
        key.report_state = keyboard_key_debounce(&key);
    }
    EXPECT_FALSE(key.report_state);
    for (int i = 0; i < 1; i++)
    {        
        key.report_state = keyboard_key_debounce(&key);
    }
    EXPECT_FALSE(key.report_state);
    for (int i = 0; i < DEBOUNCE_PRESS-DEBOUNCE_PRESS/2-1; i++)
    {        
        key.report_state = keyboard_key_debounce(&key);
    }
    EXPECT_FALSE(key.report_state);
    for (int i = 0; i < 1; i++)
    {        
        key.report_state = keyboard_key_debounce(&key);
    }
    EXPECT_TRUE(key.report_state);
#endif
#else
    EXPECT_TRUE(keyboard_key_debounce(&key));
#endif
}

TEST(Keyboard, DebounceRelease)
{
    static Key key;
    key.state = true;
    key.report_state = true;
    key_update(&key, false);
#if DEBOUNCE_RELEASE >= 2
#if DEBOUNCE_RELEASE_EAGER
    for (int i = 0; i < DEBOUNCE_RELEASE/2-1; i++)
    {        
        key.report_state = keyboard_key_debounce(&key);
    }
    EXPECT_FALSE(key.report_state);
    key_update(&key, true);
    for (int i = 0; i < 1; i++)
    {        
        key.report_state = keyboard_key_debounce(&key);
    }
    EXPECT_FALSE(key.report_state);
    key_update(&key, false);
    for (int i = 0; i < DEBOUNCE_RELEASE-DEBOUNCE_RELEASE/2-1; i++)
    {        
        key.report_state = keyboard_key_debounce(&key);
    }
    EXPECT_FALSE(key.report_state);
    for (int i = 0; i < 1; i++)
    {        
        key.report_state = keyboard_key_debounce(&key);
    }
    EXPECT_FALSE(key.report_state);
#else
    for (int i = 0; i < DEBOUNCE_RELEASE/2-1; i++)
    {        
        key.report_state = keyboard_key_debounce(&key);
    }
    EXPECT_TRUE(key.report_state);
    key_update(&key, true);
    for (int i = 0; i < 1; i++)
    {        
        key.report_state = keyboard_key_debounce(&key);
    }
    EXPECT_TRUE(key.report_state);
    key_update(&key, false);
    for (int i = 0; i < DEBOUNCE_RELEASE/2-1; i++)
    {        
        key.report_state = keyboard_key_debounce(&key);
    }
    EXPECT_TRUE(key.report_state);
    for (int i = 0; i < 1; i++)
    {        
        key.report_state = keyboard_key_debounce(&key);
    }
    EXPECT_TRUE(key.report_state);
    for (int i = 0; i < DEBOUNCE_RELEASE-DEBOUNCE_RELEASE/2-1; i++)
    {        
        key.report_state = keyboard_key_debounce(&key);
    }
    EXPECT_TRUE(key.report_state);
    for (int i = 0; i < 1; i++)
    {        
        key.report_state = keyboard_key_debounce(&key);
    }
    EXPECT_FALSE(key.report_state);
#endif
#else
    EXPECT_FALSE(keyboard_key_debounce(&key));
#endif
}
