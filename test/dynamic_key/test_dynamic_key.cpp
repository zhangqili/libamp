#include <gtest/gtest.h>

#include "keyboard.h"
#include "dynamic_key.h"
#include "math.h"

extern uint8_t keyboard_send_buffer[64];
extern uint8_t mouse_send_buffer[64];

TEST(DynamicKey, ModTap)
{
    static AdvancedKey advanced_key;
    static DynamicKey dynamic_key = 
    {
        .mt = 
        {
            .type = DYNAMIC_KEY_MOD_TAP,
            .key_binding = {KEY_A, KEY_B},
            .duration = 100,
        }
    };
    dynamic_key_update(&dynamic_key, &advanced_key, true);
    keyboard_buffer_clear();
    if (advanced_key.key.report_state)
    {
        dynamic_key_add_buffer(&dynamic_key);
    }
    keyboard_buffer_send();
    EXPECT_EQ(dynamic_key.mt.state, DYNAMIC_KEY_ACTION_TAP);
    EXPECT_EQ(keyboard_send_buffer[2], KEY_NO_EVENT);
    g_keyboard_tick += 200;

    dynamic_key_update(&dynamic_key, &advanced_key, true);
    keyboard_buffer_clear();
    if (advanced_key.key.report_state)
    {
        dynamic_key_add_buffer(&dynamic_key);
    }
    keyboard_buffer_send();
    EXPECT_EQ(dynamic_key.mt.state, DYNAMIC_KEY_ACTION_HOLD);
    EXPECT_EQ(keyboard_send_buffer[2], KEY_B);

    dynamic_key_update(&dynamic_key, &advanced_key, false);
    keyboard_buffer_clear();
    if (advanced_key.key.report_state)
    {
        dynamic_key_add_buffer(&dynamic_key);
    }
    keyboard_buffer_send();
    EXPECT_EQ(keyboard_send_buffer[2], KEY_NO_EVENT);


    dynamic_key_update(&dynamic_key, &advanced_key, true);
    keyboard_buffer_clear();
    if (advanced_key.key.report_state)
    {
        dynamic_key_add_buffer(&dynamic_key);
    }
    keyboard_buffer_send();
    EXPECT_EQ(keyboard_send_buffer[2], KEY_NO_EVENT);
    g_keyboard_tick += 50;

    dynamic_key_update(&dynamic_key, &advanced_key, false);
    keyboard_buffer_clear();
    if (advanced_key.key.report_state)
    {
        dynamic_key_add_buffer(&dynamic_key);
    }
    keyboard_buffer_send();
    EXPECT_EQ(dynamic_key.mt.state, DYNAMIC_KEY_ACTION_TAP);
    EXPECT_EQ(keyboard_send_buffer[2], KEY_A);
}

TEST(DynamicKey, ToggleKey)
{
    static AdvancedKey advanced_key;
    static DynamicKey dynamic_key = 
    {
        .tk = 
        {
            .type = DYNAMIC_KEY_TOGGLE_KEY,
            .key_binding = KEY_A,
        }
    };

    dynamic_key_update(&dynamic_key, &advanced_key, true);
    keyboard_buffer_clear();
    if (advanced_key.key.report_state)
    {
        dynamic_key_add_buffer(&dynamic_key);
    }
    keyboard_buffer_send();
    EXPECT_EQ(dynamic_key.tk.state, true);
    EXPECT_EQ(keyboard_send_buffer[2], KEY_A);

    dynamic_key_update(&dynamic_key, &advanced_key, false);
    keyboard_buffer_clear();
    if (advanced_key.key.report_state)
    {
        dynamic_key_add_buffer(&dynamic_key);
    }
    keyboard_buffer_send();
    EXPECT_EQ(dynamic_key.tk.state, true);
    EXPECT_EQ(keyboard_send_buffer[2], KEY_A);

    dynamic_key_update(&dynamic_key, &advanced_key, true);
    keyboard_buffer_clear();
    if (advanced_key.key.report_state)
    {
        dynamic_key_add_buffer(&dynamic_key);
    }
    keyboard_buffer_send();
    EXPECT_EQ(dynamic_key.tk.state, false);
    EXPECT_EQ(keyboard_send_buffer[2], KEY_NO_EVENT);


    dynamic_key_update(&dynamic_key, &advanced_key, false);
    keyboard_buffer_clear();
    if (advanced_key.key.report_state)
    {
        dynamic_key_add_buffer(&dynamic_key);
    }
    keyboard_buffer_send();
    EXPECT_EQ(dynamic_key.tk.state, false);
    EXPECT_EQ(keyboard_send_buffer[2], KEY_NO_EVENT);
}

TEST(DynamicKey, DynamicKeyStroke)
{
    g_keymap[0][0] = DYNAMIC_KEY | (0 << 8);
    static DynamicKey dynamic_key = 
    {
        .dks = 
        {
            .type = DYNAMIC_KEY_STROKE,
            .key_binding = {KEY_A, KEY_B, KEY_C, KEY_D},
            .key_control = {0x0FFF, 0x00F0, 0xF1F0, 0x010F},
            .press_begin_distance = (AnalogValue)(0.25*(ANALOG_VALUE_RANGE)),
            .press_fully_distance = (AnalogValue)(0.75*(ANALOG_VALUE_RANGE)),
            .release_begin_distance = (AnalogValue)(0.75*(ANALOG_VALUE_RANGE)),
            .release_fully_distance = (AnalogValue)(0.25*(ANALOG_VALUE_RANGE)),
        }
    };
    memcpy(&g_keyboard_dynamic_keys[0], &dynamic_key, sizeof(DynamicKey));
    advanced_key_update(&g_keyboard_advanced_keys[0],1);
    advanced_key_update(&g_keyboard_advanced_keys[0],0);

    // A keep activating 
    // B deactivate
    // C deactivate
    // D keep activating
    g_keyboard_tick+=10;
    advanced_key_update(&g_keyboard_advanced_keys[0],0.3);
    keyboard_buffer_clear();
    if (g_keyboard_advanced_keys[0].key.report_state)
    {
        keyboard_advanced_key_event_handler(&g_keyboard_advanced_keys[0],
            keyboard_make_event(&g_keyboard_advanced_keys[0].key, KEYBOARD_EVENT_KEY_TRUE));
    }
    keyboard_buffer_send();
    EXPECT_EQ(keyboard_send_buffer[2], KEY_A);
    EXPECT_EQ(keyboard_send_buffer[3], KEY_D);
    EXPECT_EQ(keyboard_send_buffer[4], KEY_NO_EVENT);

    g_keyboard_tick+=10;
    advanced_key_update(&g_keyboard_advanced_keys[0],0.4);
    keyboard_buffer_clear();
    if (g_keyboard_advanced_keys[0].key.report_state)
    {
        keyboard_advanced_key_event_handler(&g_keyboard_advanced_keys[0],
            keyboard_make_event(&g_keyboard_advanced_keys[0].key, KEYBOARD_EVENT_KEY_TRUE));
    }
    keyboard_buffer_send();
    EXPECT_EQ(keyboard_send_buffer[2], KEY_A);
    EXPECT_EQ(keyboard_send_buffer[3], KEY_D);
    EXPECT_EQ(keyboard_send_buffer[4], KEY_NO_EVENT);

    // A keep activating 
    // B keep activating
    // C keep activating
    // D deactivate
    g_keyboard_tick+=10;
    advanced_key_update(&g_keyboard_advanced_keys[0],0.8);
    keyboard_buffer_clear();
    if (g_keyboard_advanced_keys[0].key.report_state)
    {
        keyboard_advanced_key_event_handler(&g_keyboard_advanced_keys[0],
            keyboard_make_event(&g_keyboard_advanced_keys[0].key, KEYBOARD_EVENT_KEY_TRUE));
    }
    keyboard_buffer_send();
    EXPECT_EQ(keyboard_send_buffer[2], KEY_A);
    EXPECT_EQ(keyboard_send_buffer[3], KEY_B);
    EXPECT_EQ(keyboard_send_buffer[4], KEY_C);
    EXPECT_EQ(keyboard_send_buffer[5], KEY_NO_EVENT);

    g_keyboard_tick+=10;
    advanced_key_update(&g_keyboard_advanced_keys[0],0.9);
    keyboard_buffer_clear();
    if (g_keyboard_advanced_keys[0].key.report_state)
    {
        keyboard_advanced_key_event_handler(&g_keyboard_advanced_keys[0],
            keyboard_make_event(&g_keyboard_advanced_keys[0].key, KEYBOARD_EVENT_KEY_TRUE));
    }
    keyboard_buffer_send();
    EXPECT_EQ(keyboard_send_buffer[2], KEY_A);
    EXPECT_EQ(keyboard_send_buffer[3], KEY_B);
    EXPECT_EQ(keyboard_send_buffer[4], KEY_C);
    EXPECT_EQ(keyboard_send_buffer[5], KEY_NO_EVENT);

    // A keep activating 
    // B deactivate
    // C activate once
    // D activate once
    g_keyboard_tick+=10;
    advanced_key_update(&g_keyboard_advanced_keys[0],0.6);
    keyboard_buffer_clear();
    if (g_keyboard_advanced_keys[0].key.report_state)
    {
        keyboard_advanced_key_event_handler(&g_keyboard_advanced_keys[0],
            keyboard_make_event(&g_keyboard_advanced_keys[0].key, KEYBOARD_EVENT_KEY_TRUE));
    }
    keyboard_buffer_send();
    EXPECT_EQ(keyboard_send_buffer[2], KEY_A);
    EXPECT_EQ(keyboard_send_buffer[3], KEY_C);
    EXPECT_EQ(keyboard_send_buffer[4], KEY_D);
    EXPECT_EQ(keyboard_send_buffer[5], KEY_NO_EVENT);

    g_keyboard_tick+=10;
    advanced_key_update(&g_keyboard_advanced_keys[0],0.5);
    keyboard_buffer_clear();
    if (g_keyboard_advanced_keys[0].key.report_state)
    {
        keyboard_advanced_key_event_handler(&g_keyboard_advanced_keys[0],
            keyboard_make_event(&g_keyboard_advanced_keys[0].key, KEYBOARD_EVENT_KEY_TRUE));
    }
    keyboard_buffer_send();
    EXPECT_EQ(keyboard_send_buffer[2], KEY_A);
    EXPECT_EQ(keyboard_send_buffer[3], KEY_NO_EVENT);

    // A deactivate 
    // B deactivate
    // C keep activating 
    // D deactivate
    g_keyboard_tick+=10;
    advanced_key_update(&g_keyboard_advanced_keys[0],0.2);
    keyboard_buffer_clear();
    if (g_keyboard_advanced_keys[0].key.report_state)
    {
        keyboard_advanced_key_event_handler(&g_keyboard_advanced_keys[0],
            keyboard_make_event(&g_keyboard_advanced_keys[0].key, KEYBOARD_EVENT_KEY_TRUE));
    }
    keyboard_buffer_send();
    EXPECT_EQ(keyboard_send_buffer[2], KEY_C);
    EXPECT_EQ(keyboard_send_buffer[3], KEY_NO_EVENT);

    g_keyboard_tick+=10;
    advanced_key_update(&g_keyboard_advanced_keys[0],0.1);
    keyboard_buffer_clear();
    if (g_keyboard_advanced_keys[0].key.report_state)
    {
        keyboard_advanced_key_event_handler(&g_keyboard_advanced_keys[0],
            keyboard_make_event(&g_keyboard_advanced_keys[0].key, KEYBOARD_EVENT_KEY_TRUE));
    }
    keyboard_buffer_send();
    EXPECT_EQ(keyboard_send_buffer[2], KEY_C);
    EXPECT_EQ(keyboard_send_buffer[3], KEY_NO_EVENT);
}

TEST(DynamicKey, MutexDistancePriority)
{
    g_keymap[0][0] = DYNAMIC_KEY | (1 << 8);
    g_keymap[0][1] = DYNAMIC_KEY | (1 << 8);
    DynamicKey*dynamic_key = &g_keyboard_dynamic_keys[1];
    dynamic_key->m.type = DYNAMIC_KEY_MUTEX;
    dynamic_key->m.mode = DK_MUTEX_DISTANCE_PRIORITY;
    dynamic_key->m.key_binding[0] = KEY_A;
    dynamic_key->m.key_binding[1] = KEY_B;
    dynamic_key->m.key_id[0] = 0;
    dynamic_key->m.key_id[1] = 1;

    advanced_key_update(&g_keyboard_advanced_keys[0],1);
    advanced_key_update(&g_keyboard_advanced_keys[1],1);
    advanced_key_update(&g_keyboard_advanced_keys[0],0);
    advanced_key_update(&g_keyboard_advanced_keys[1],0);

    advanced_key_update(&g_keyboard_advanced_keys[0],0.3);
    advanced_key_update(&g_keyboard_advanced_keys[1],0);
    keyboard_buffer_clear();
    if (g_keyboard_advanced_keys[0].key.report_state)
    {
        keyboard_advanced_key_event_handler(&g_keyboard_advanced_keys[0],
            keyboard_make_event(&g_keyboard_advanced_keys[0].key, KEYBOARD_EVENT_KEY_TRUE));
    }
    if (g_keyboard_advanced_keys[1].key.report_state)
    {
        keyboard_advanced_key_event_handler(&g_keyboard_advanced_keys[1],
            keyboard_make_event(&g_keyboard_advanced_keys[1].key, KEYBOARD_EVENT_KEY_TRUE));
    }
    keyboard_buffer_send();
    EXPECT_EQ(keyboard_send_buffer[2], KEY_A);
    EXPECT_EQ(keyboard_send_buffer[3], KEY_NO_EVENT);


    advanced_key_update(&g_keyboard_advanced_keys[0],0.3);
    advanced_key_update(&g_keyboard_advanced_keys[1],0.9);
    advanced_key_update(&g_keyboard_advanced_keys[0],0.3);
    advanced_key_update(&g_keyboard_advanced_keys[1],0.9);
    keyboard_buffer_clear();
    if (g_keyboard_advanced_keys[0].key.report_state)
    {
        keyboard_advanced_key_event_handler(&g_keyboard_advanced_keys[0],
            keyboard_make_event(&g_keyboard_advanced_keys[0].key, KEYBOARD_EVENT_KEY_TRUE));
    }
    if (g_keyboard_advanced_keys[1].key.report_state)
    {
        keyboard_advanced_key_event_handler(&g_keyboard_advanced_keys[1],
            keyboard_make_event(&g_keyboard_advanced_keys[1].key, KEYBOARD_EVENT_KEY_TRUE));
    }
    keyboard_buffer_send();
    EXPECT_EQ(keyboard_send_buffer[2], KEY_B);
    EXPECT_EQ(keyboard_send_buffer[3], KEY_NO_EVENT);

    advanced_key_update(&g_keyboard_advanced_keys[0],1);
    advanced_key_update(&g_keyboard_advanced_keys[1],0.9);
    advanced_key_update(&g_keyboard_advanced_keys[0],1);
    advanced_key_update(&g_keyboard_advanced_keys[1],0.9);
    keyboard_buffer_clear();
    if (g_keyboard_advanced_keys[0].key.report_state)
    {
        keyboard_advanced_key_event_handler(&g_keyboard_advanced_keys[0],
            keyboard_make_event(&g_keyboard_advanced_keys[0].key, KEYBOARD_EVENT_KEY_TRUE));
    }
    if (g_keyboard_advanced_keys[1].key.report_state)
    {
        keyboard_advanced_key_event_handler(&g_keyboard_advanced_keys[1],
            keyboard_make_event(&g_keyboard_advanced_keys[1].key, KEYBOARD_EVENT_KEY_TRUE));
    }
    keyboard_buffer_send();
    EXPECT_EQ(keyboard_send_buffer[2], KEY_A);
    EXPECT_EQ(keyboard_send_buffer[3], KEY_NO_EVENT);

    dynamic_key->m.mode |= 0x80;
    advanced_key_update(&g_keyboard_advanced_keys[0],0);
    advanced_key_update(&g_keyboard_advanced_keys[1],0);
    advanced_key_update(&g_keyboard_advanced_keys[0],1);
    advanced_key_update(&g_keyboard_advanced_keys[1],0.9);
    advanced_key_update(&g_keyboard_advanced_keys[0],1);
    advanced_key_update(&g_keyboard_advanced_keys[1],0.9);
    keyboard_buffer_clear();
    if (g_keyboard_advanced_keys[0].key.report_state)
    {
        keyboard_advanced_key_event_handler(&g_keyboard_advanced_keys[0],
            keyboard_make_event(&g_keyboard_advanced_keys[0].key, KEYBOARD_EVENT_KEY_TRUE));
    }
    if (g_keyboard_advanced_keys[1].key.report_state)
    {
        keyboard_advanced_key_event_handler(&g_keyboard_advanced_keys[1],
            keyboard_make_event(&g_keyboard_advanced_keys[1].key, KEYBOARD_EVENT_KEY_TRUE));
    }
    keyboard_buffer_send();
    EXPECT_EQ(keyboard_send_buffer[2], KEY_A);
    EXPECT_EQ(keyboard_send_buffer[3], KEY_B);
    EXPECT_EQ(keyboard_send_buffer[4], KEY_A);
    EXPECT_EQ(keyboard_send_buffer[5], KEY_B);
    EXPECT_EQ(keyboard_send_buffer[6], KEY_NO_EVENT);
}

TEST(DynamicKey, MutexLastPriority)
{
    g_keymap[0][0] = DYNAMIC_KEY | (1 << 8);
    g_keymap[0][1] = DYNAMIC_KEY | (1 << 8);
    DynamicKey*dynamic_key = &g_keyboard_dynamic_keys[1];
    dynamic_key->m.type = DYNAMIC_KEY_MUTEX;
    dynamic_key->m.mode = DK_MUTEX_LAST_PRIORITY;
    dynamic_key->m.key_binding[0] = KEY_A;
    dynamic_key->m.key_binding[1] = KEY_B;
    dynamic_key->m.key_id[0] = 0;
    dynamic_key->m.key_id[1] = 1;

    advanced_key_update(&g_keyboard_advanced_keys[0],1);
    advanced_key_update(&g_keyboard_advanced_keys[1],1);
    advanced_key_update(&g_keyboard_advanced_keys[0],0);
    advanced_key_update(&g_keyboard_advanced_keys[1],0);

    advanced_key_update(&g_keyboard_advanced_keys[0],0.6);
    advanced_key_update(&g_keyboard_advanced_keys[1],0);
    EXPECT_TRUE(g_keyboard_advanced_keys[0].key.state);
    EXPECT_FALSE(g_keyboard_advanced_keys[1].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[0].key.report_state);
    EXPECT_FALSE(g_keyboard_advanced_keys[1].key.report_state);

    advanced_key_update(&g_keyboard_advanced_keys[0],0.6);
    advanced_key_update(&g_keyboard_advanced_keys[1],0.6);
    EXPECT_TRUE(g_keyboard_advanced_keys[0].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[1].key.state);
    EXPECT_FALSE(g_keyboard_advanced_keys[0].key.report_state);
    EXPECT_TRUE(g_keyboard_advanced_keys[1].key.report_state);

    advanced_key_update(&g_keyboard_advanced_keys[0],0.3);
    advanced_key_update(&g_keyboard_advanced_keys[1],0.6);
    EXPECT_FALSE(g_keyboard_advanced_keys[0].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[1].key.state);
    EXPECT_FALSE(g_keyboard_advanced_keys[0].key.report_state);
    EXPECT_TRUE(g_keyboard_advanced_keys[1].key.report_state);

    advanced_key_update(&g_keyboard_advanced_keys[0],0.6);
    advanced_key_update(&g_keyboard_advanced_keys[1],0.6);
    EXPECT_TRUE(g_keyboard_advanced_keys[0].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[1].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[0].key.report_state);
    EXPECT_FALSE(g_keyboard_advanced_keys[1].key.report_state);

    advanced_key_update(&g_keyboard_advanced_keys[0],0.9);
    advanced_key_update(&g_keyboard_advanced_keys[1],0.9);
    EXPECT_TRUE(g_keyboard_advanced_keys[0].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[1].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[0].key.report_state);
    EXPECT_FALSE(g_keyboard_advanced_keys[1].key.report_state);

    dynamic_key->m.mode |= 0x80;
    advanced_key_update(&g_keyboard_advanced_keys[0],0.0);
    advanced_key_update(&g_keyboard_advanced_keys[1],0.0);
    advanced_key_update(&g_keyboard_advanced_keys[0],0.9);
    advanced_key_update(&g_keyboard_advanced_keys[1],0.9);
    EXPECT_TRUE(g_keyboard_advanced_keys[0].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[1].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[0].key.report_state);
    EXPECT_TRUE(g_keyboard_advanced_keys[1].key.report_state);
}

TEST(DynamicKey, MutexKey1Priority)
{
    g_keymap[0][0] = DYNAMIC_KEY | (1 << 8);
    g_keymap[0][1] = DYNAMIC_KEY | (1 << 8);
    DynamicKey*dynamic_key = &g_keyboard_dynamic_keys[1];
    dynamic_key->m.type = DYNAMIC_KEY_MUTEX;
    dynamic_key->m.mode = DK_MUTEX_KEY1_PRIORITY;
    dynamic_key->m.key_binding[0] = KEY_A;
    dynamic_key->m.key_binding[1] = KEY_B;
    dynamic_key->m.key_id[0] = 0;
    dynamic_key->m.key_id[1] = 1;

    advanced_key_update(&g_keyboard_advanced_keys[0],1);
    advanced_key_update(&g_keyboard_advanced_keys[1],1);
    advanced_key_update(&g_keyboard_advanced_keys[0],0);
    advanced_key_update(&g_keyboard_advanced_keys[1],0);

    advanced_key_update(&g_keyboard_advanced_keys[0],0.6);
    advanced_key_update(&g_keyboard_advanced_keys[1],0);
    EXPECT_TRUE(g_keyboard_advanced_keys[0].key.state);
    EXPECT_FALSE(g_keyboard_advanced_keys[1].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[0].key.report_state);
    EXPECT_FALSE(g_keyboard_advanced_keys[1].key.report_state);

    advanced_key_update(&g_keyboard_advanced_keys[0],0.6);
    advanced_key_update(&g_keyboard_advanced_keys[1],0.6);
    EXPECT_TRUE(g_keyboard_advanced_keys[0].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[1].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[0].key.report_state);
    EXPECT_FALSE(g_keyboard_advanced_keys[1].key.report_state);

    advanced_key_update(&g_keyboard_advanced_keys[0],0.3);
    advanced_key_update(&g_keyboard_advanced_keys[1],0.6);
    EXPECT_FALSE(g_keyboard_advanced_keys[0].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[1].key.state);
    EXPECT_FALSE(g_keyboard_advanced_keys[0].key.report_state);
    EXPECT_TRUE(g_keyboard_advanced_keys[1].key.report_state);

    advanced_key_update(&g_keyboard_advanced_keys[0],0.6);
    advanced_key_update(&g_keyboard_advanced_keys[1],0.6);
    EXPECT_TRUE(g_keyboard_advanced_keys[0].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[1].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[0].key.report_state);
    EXPECT_FALSE(g_keyboard_advanced_keys[1].key.report_state);

    advanced_key_update(&g_keyboard_advanced_keys[0],0.9);
    advanced_key_update(&g_keyboard_advanced_keys[1],0.9);
    EXPECT_TRUE(g_keyboard_advanced_keys[0].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[1].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[0].key.report_state);
    EXPECT_FALSE(g_keyboard_advanced_keys[1].key.report_state);

    dynamic_key->m.mode |= 0x80;
    advanced_key_update(&g_keyboard_advanced_keys[0],0.0);
    advanced_key_update(&g_keyboard_advanced_keys[1],0.0);
    advanced_key_update(&g_keyboard_advanced_keys[0],0.9);
    advanced_key_update(&g_keyboard_advanced_keys[1],0.9);
    EXPECT_TRUE(g_keyboard_advanced_keys[0].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[1].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[0].key.report_state);
    EXPECT_TRUE(g_keyboard_advanced_keys[1].key.report_state);
}

TEST(DynamicKey, MutexKey2Priority)
{
    g_keymap[0][0] = DYNAMIC_KEY | (1 << 8);
    g_keymap[0][1] = DYNAMIC_KEY | (1 << 8);
    DynamicKey*dynamic_key = &g_keyboard_dynamic_keys[1];
    dynamic_key->m.type = DYNAMIC_KEY_MUTEX;
    dynamic_key->m.mode = DK_MUTEX_KEY2_PRIORITY;
    dynamic_key->m.key_binding[0] = KEY_A;
    dynamic_key->m.key_binding[1] = KEY_B;
    dynamic_key->m.key_id[0] = 0;
    dynamic_key->m.key_id[1] = 1;

    advanced_key_update(&g_keyboard_advanced_keys[0],1);
    advanced_key_update(&g_keyboard_advanced_keys[1],1);
    advanced_key_update(&g_keyboard_advanced_keys[0],0);
    advanced_key_update(&g_keyboard_advanced_keys[1],0);

    advanced_key_update(&g_keyboard_advanced_keys[0],0.6);
    advanced_key_update(&g_keyboard_advanced_keys[1],0);
    EXPECT_TRUE(g_keyboard_advanced_keys[0].key.state);
    EXPECT_FALSE(g_keyboard_advanced_keys[1].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[0].key.report_state);
    EXPECT_FALSE(g_keyboard_advanced_keys[1].key.report_state);

    advanced_key_update(&g_keyboard_advanced_keys[0],0.6);
    advanced_key_update(&g_keyboard_advanced_keys[1],0.6);
    EXPECT_TRUE(g_keyboard_advanced_keys[0].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[1].key.state);
    EXPECT_FALSE(g_keyboard_advanced_keys[0].key.report_state);
    EXPECT_TRUE(g_keyboard_advanced_keys[1].key.report_state);

    advanced_key_update(&g_keyboard_advanced_keys[0],0.3);
    advanced_key_update(&g_keyboard_advanced_keys[1],0.6);
    EXPECT_FALSE(g_keyboard_advanced_keys[0].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[1].key.state);
    EXPECT_FALSE(g_keyboard_advanced_keys[0].key.report_state);
    EXPECT_TRUE(g_keyboard_advanced_keys[1].key.report_state);

    advanced_key_update(&g_keyboard_advanced_keys[0],0.6);
    advanced_key_update(&g_keyboard_advanced_keys[1],0.6);
    EXPECT_TRUE(g_keyboard_advanced_keys[0].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[1].key.state);
    EXPECT_FALSE(g_keyboard_advanced_keys[0].key.report_state);
    EXPECT_TRUE(g_keyboard_advanced_keys[1].key.report_state);

    advanced_key_update(&g_keyboard_advanced_keys[0],0.9);
    advanced_key_update(&g_keyboard_advanced_keys[1],0.9);
    EXPECT_TRUE(g_keyboard_advanced_keys[0].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[1].key.state);
    EXPECT_FALSE(g_keyboard_advanced_keys[0].key.report_state);
    EXPECT_TRUE(g_keyboard_advanced_keys[1].key.report_state);

    dynamic_key->m.mode |= 0x80;
    advanced_key_update(&g_keyboard_advanced_keys[0],0.0);
    advanced_key_update(&g_keyboard_advanced_keys[1],0.0);
    advanced_key_update(&g_keyboard_advanced_keys[0],0.9);
    advanced_key_update(&g_keyboard_advanced_keys[1],0.9);
    EXPECT_TRUE(g_keyboard_advanced_keys[0].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[1].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[0].key.report_state);
    EXPECT_TRUE(g_keyboard_advanced_keys[1].key.report_state);
}


TEST(DynamicKey, MutexKeyNeural)
{
    g_keymap[0][0] = DYNAMIC_KEY | (1 << 8);
    g_keymap[0][1] = DYNAMIC_KEY | (1 << 8);
    DynamicKey*dynamic_key = &g_keyboard_dynamic_keys[1];
    dynamic_key->m.type = DYNAMIC_KEY_MUTEX;
    dynamic_key->m.mode = DK_MUTEX_NEUTRAL;
    dynamic_key->m.key_binding[0] = KEY_A;
    dynamic_key->m.key_binding[1] = KEY_B;
    dynamic_key->m.key_id[0] = 0;
    dynamic_key->m.key_id[1] = 1;

    advanced_key_update(&g_keyboard_advanced_keys[0],1);
    advanced_key_update(&g_keyboard_advanced_keys[1],1);
    advanced_key_update(&g_keyboard_advanced_keys[0],0);
    advanced_key_update(&g_keyboard_advanced_keys[1],0);

    advanced_key_update(&g_keyboard_advanced_keys[0],0.6);
    advanced_key_update(&g_keyboard_advanced_keys[1],0);
    EXPECT_TRUE(g_keyboard_advanced_keys[0].key.state);
    EXPECT_FALSE(g_keyboard_advanced_keys[1].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[0].key.report_state);
    EXPECT_FALSE(g_keyboard_advanced_keys[1].key.report_state);

    advanced_key_update(&g_keyboard_advanced_keys[0],0.6);
    advanced_key_update(&g_keyboard_advanced_keys[1],0.6);
    EXPECT_TRUE(g_keyboard_advanced_keys[0].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[1].key.state);
    EXPECT_FALSE(g_keyboard_advanced_keys[0].key.report_state);
    EXPECT_FALSE(g_keyboard_advanced_keys[1].key.report_state);

    advanced_key_update(&g_keyboard_advanced_keys[0],0.3);
    advanced_key_update(&g_keyboard_advanced_keys[1],0.6);
    EXPECT_FALSE(g_keyboard_advanced_keys[0].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[1].key.state);
    EXPECT_FALSE(g_keyboard_advanced_keys[0].key.report_state);
    EXPECT_TRUE(g_keyboard_advanced_keys[1].key.report_state);

    advanced_key_update(&g_keyboard_advanced_keys[0],0.6);
    advanced_key_update(&g_keyboard_advanced_keys[1],0.6);
    EXPECT_TRUE(g_keyboard_advanced_keys[0].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[1].key.state);
    EXPECT_FALSE(g_keyboard_advanced_keys[0].key.report_state);
    EXPECT_FALSE(g_keyboard_advanced_keys[1].key.report_state);

    advanced_key_update(&g_keyboard_advanced_keys[0],0.9);
    advanced_key_update(&g_keyboard_advanced_keys[1],0.9);
    EXPECT_TRUE(g_keyboard_advanced_keys[0].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[1].key.state);
    EXPECT_FALSE(g_keyboard_advanced_keys[0].key.report_state);
    EXPECT_FALSE(g_keyboard_advanced_keys[1].key.report_state);

    dynamic_key->m.mode |= 0x80;
    advanced_key_update(&g_keyboard_advanced_keys[0],0.0);
    advanced_key_update(&g_keyboard_advanced_keys[1],0.0);
    advanced_key_update(&g_keyboard_advanced_keys[0],0.9);
    advanced_key_update(&g_keyboard_advanced_keys[1],0.9);
    EXPECT_TRUE(g_keyboard_advanced_keys[0].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[1].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[0].key.report_state);
    EXPECT_TRUE(g_keyboard_advanced_keys[1].key.report_state);
}