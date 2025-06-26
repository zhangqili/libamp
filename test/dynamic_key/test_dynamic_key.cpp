#include <gtest/gtest.h>

#include "keyboard.h"
#include "dynamic_key.h"
#include "layer.h"
#include "math.h"

extern uint8_t keyboard_send_buffer[64];

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
    keyboard_clear_buffer();
    if (advanced_key.key.report_state)
    {
        dynamic_key_add_buffer(MK_EVENT(KEY_NO_EVENT, KEYBOARD_EVENT_KEY_TRUE, &advanced_key), &dynamic_key);
    }
    keyboard_buffer_send();
    EXPECT_EQ(dynamic_key.mt.state, DYNAMIC_KEY_ACTION_TAP);
    EXPECT_EQ(keyboard_send_buffer[2], KEY_NO_EVENT);
    g_keyboard_tick += 200;

    dynamic_key_update(&dynamic_key, &advanced_key, true);
    keyboard_clear_buffer();
    if (advanced_key.key.report_state)
    {
        dynamic_key_add_buffer(MK_EVENT(KEY_NO_EVENT, KEYBOARD_EVENT_KEY_TRUE, &advanced_key), &dynamic_key);
    }
    keyboard_buffer_send();
    EXPECT_EQ(dynamic_key.mt.state, DYNAMIC_KEY_ACTION_HOLD);
    EXPECT_EQ(keyboard_send_buffer[2], KEY_B);

    dynamic_key_update(&dynamic_key, &advanced_key, false);
    keyboard_clear_buffer();
    
    if (advanced_key.key.report_state)
    {
        dynamic_key_add_buffer(MK_EVENT(KEY_NO_EVENT, KEYBOARD_EVENT_KEY_TRUE, &advanced_key), &dynamic_key);
    }
    keyboard_buffer_send();
    EXPECT_EQ(keyboard_send_buffer[2], KEY_NO_EVENT);


    dynamic_key_update(&dynamic_key, &advanced_key, true);
    keyboard_clear_buffer();
    if (advanced_key.key.report_state)
    {
        dynamic_key_add_buffer(MK_EVENT(KEY_NO_EVENT, KEYBOARD_EVENT_KEY_TRUE, &advanced_key), &dynamic_key);
    }
    keyboard_buffer_send();
    EXPECT_EQ(keyboard_send_buffer[2], KEY_NO_EVENT);
    g_keyboard_tick += 50;

    dynamic_key_update(&dynamic_key, &advanced_key, false);
    keyboard_clear_buffer();
    if (advanced_key.key.report_state)
    {
        dynamic_key_add_buffer(MK_EVENT(KEY_NO_EVENT, KEYBOARD_EVENT_KEY_TRUE, &advanced_key), &dynamic_key);
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
    keyboard_clear_buffer();
    if (advanced_key.key.report_state)
    {
        dynamic_key_add_buffer(MK_EVENT(KEY_NO_EVENT, KEYBOARD_EVENT_KEY_TRUE, &advanced_key), &dynamic_key);
    }
    keyboard_buffer_send();
    EXPECT_EQ(dynamic_key.tk.state, true);
    EXPECT_EQ(keyboard_send_buffer[2], KEY_A);

    dynamic_key_update(&dynamic_key, &advanced_key, false);
    keyboard_clear_buffer();
    if (advanced_key.key.report_state)
    {
        dynamic_key_add_buffer(MK_EVENT(KEY_NO_EVENT, KEYBOARD_EVENT_KEY_TRUE, &advanced_key), &dynamic_key);
    }
    keyboard_buffer_send();
    EXPECT_EQ(dynamic_key.tk.state, true);
    EXPECT_EQ(keyboard_send_buffer[2], KEY_A);

    dynamic_key_update(&dynamic_key, &advanced_key, true);
    keyboard_clear_buffer();
    if (advanced_key.key.report_state)
    {
        dynamic_key_add_buffer(MK_EVENT(KEY_NO_EVENT, KEYBOARD_EVENT_KEY_TRUE, &advanced_key), &dynamic_key);
    }
    keyboard_buffer_send();
    EXPECT_EQ(dynamic_key.tk.state, false);
    EXPECT_EQ(keyboard_send_buffer[2], KEY_NO_EVENT);


    dynamic_key_update(&dynamic_key, &advanced_key, false);
    keyboard_clear_buffer();
    if (advanced_key.key.report_state)
    {
        dynamic_key_add_buffer(MK_EVENT(KEY_NO_EVENT, KEYBOARD_EVENT_KEY_TRUE, &advanced_key), &dynamic_key);
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
            .key_control = {
                DKS_KEY_CONTROL(DKS_HOLD,   DKS_HOLD,   DKS_HOLD,   DKS_RELEASE), 
                DKS_KEY_CONTROL(DKS_RELEASE,DKS_HOLD,   DKS_RELEASE,DKS_RELEASE),
                DKS_KEY_CONTROL(DKS_RELEASE,DKS_HOLD,   DKS_TAP,    DKS_HOLD),
                DKS_KEY_CONTROL(DKS_HOLD,   DKS_RELEASE,DKS_TAP,    DKS_RELEASE)
            },
            .press_begin_distance = A_ANIT_NORM(0.25),
            .press_fully_distance = A_ANIT_NORM(0.75),
            .release_begin_distance = A_ANIT_NORM(0.75),
            .release_fully_distance = A_ANIT_NORM(0.25),
        }
    };
    memcpy(&g_keyboard_dynamic_keys[0], &dynamic_key, sizeof(DynamicKey));
    advanced_key_update(&g_keyboard_advanced_keys[0],A_ANIT_NORM(1));
    advanced_key_update(&g_keyboard_advanced_keys[0],A_ANIT_NORM(0));

    // A keep activating 
    // B deactivate
    // C deactivate
    // D keep activating
    g_keyboard_tick+=10;
    advanced_key_update(&g_keyboard_advanced_keys[0],A_ANIT_NORM(0.3));
    keyboard_clear_buffer();
    if (g_keyboard_advanced_keys[0].key.report_state)
    {
        keyboard_advanced_key_event_handler(&g_keyboard_advanced_keys[0],
            MK_EVENT(layer_cache_get_keycode(g_keyboard_advanced_keys[0].key.id), KEYBOARD_EVENT_KEY_TRUE, &g_keyboard_advanced_keys[0]));
    }
    keyboard_buffer_send();
    EXPECT_EQ(keyboard_send_buffer[2], KEY_A);
    EXPECT_EQ(keyboard_send_buffer[3], KEY_D);
    EXPECT_EQ(keyboard_send_buffer[4], KEY_NO_EVENT);

    g_keyboard_tick+=10;
    advanced_key_update(&g_keyboard_advanced_keys[0],A_ANIT_NORM(0.4));
    keyboard_clear_buffer();
    if (g_keyboard_advanced_keys[0].key.report_state)
    {
        keyboard_advanced_key_event_handler(&g_keyboard_advanced_keys[0],
            MK_EVENT(layer_cache_get_keycode(g_keyboard_advanced_keys[0].key.id), KEYBOARD_EVENT_KEY_TRUE, &g_keyboard_advanced_keys[0]));
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
    advanced_key_update(&g_keyboard_advanced_keys[0],A_ANIT_NORM(0.8));
    keyboard_clear_buffer();
    if (g_keyboard_advanced_keys[0].key.report_state)
    {
        keyboard_advanced_key_event_handler(&g_keyboard_advanced_keys[0],
            MK_EVENT(layer_cache_get_keycode(g_keyboard_advanced_keys[0].key.id), KEYBOARD_EVENT_KEY_TRUE, &g_keyboard_advanced_keys[0]));
    }
    keyboard_buffer_send();
    EXPECT_EQ(keyboard_send_buffer[2], KEY_A);
    EXPECT_EQ(keyboard_send_buffer[3], KEY_B);
    EXPECT_EQ(keyboard_send_buffer[4], KEY_C);
    EXPECT_EQ(keyboard_send_buffer[5], KEY_NO_EVENT);

    g_keyboard_tick+=10;
    advanced_key_update(&g_keyboard_advanced_keys[0],A_ANIT_NORM(0.9));
    keyboard_clear_buffer();
    if (g_keyboard_advanced_keys[0].key.report_state)
    {
        keyboard_advanced_key_event_handler(&g_keyboard_advanced_keys[0],
            MK_EVENT(layer_cache_get_keycode(g_keyboard_advanced_keys[0].key.id), KEYBOARD_EVENT_KEY_TRUE, &g_keyboard_advanced_keys[0]));
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
    advanced_key_update(&g_keyboard_advanced_keys[0],A_ANIT_NORM(0.6));
    keyboard_clear_buffer();
    if (g_keyboard_advanced_keys[0].key.report_state)
    {
        keyboard_advanced_key_event_handler(&g_keyboard_advanced_keys[0],
            MK_EVENT(layer_cache_get_keycode(g_keyboard_advanced_keys[0].key.id), KEYBOARD_EVENT_KEY_TRUE, &g_keyboard_advanced_keys[0]));
    }
    keyboard_buffer_send();
    EXPECT_EQ(keyboard_send_buffer[2], KEY_A);
    EXPECT_EQ(keyboard_send_buffer[3], KEY_C);
    EXPECT_EQ(keyboard_send_buffer[4], KEY_D);
    EXPECT_EQ(keyboard_send_buffer[5], KEY_NO_EVENT);

    g_keyboard_tick+=10;
    advanced_key_update(&g_keyboard_advanced_keys[0],A_ANIT_NORM(0.5));
    keyboard_clear_buffer();
    if (g_keyboard_advanced_keys[0].key.report_state)
    {
        keyboard_advanced_key_event_handler(&g_keyboard_advanced_keys[0],
            MK_EVENT(layer_cache_get_keycode(g_keyboard_advanced_keys[0].key.id), KEYBOARD_EVENT_KEY_TRUE, &g_keyboard_advanced_keys[0]));
    }
    keyboard_buffer_send();
    EXPECT_EQ(keyboard_send_buffer[2], KEY_A);
    EXPECT_EQ(keyboard_send_buffer[3], KEY_NO_EVENT);

    // A deactivate 
    // B deactivate
    // C keep activating 
    // D deactivate
    g_keyboard_tick+=10;
    advanced_key_update(&g_keyboard_advanced_keys[0],A_ANIT_NORM(0.2));
    keyboard_clear_buffer();
    if (g_keyboard_advanced_keys[0].key.report_state)
    {
        keyboard_advanced_key_event_handler(&g_keyboard_advanced_keys[0],
            MK_EVENT(layer_cache_get_keycode(g_keyboard_advanced_keys[0].key.id), KEYBOARD_EVENT_KEY_TRUE, &g_keyboard_advanced_keys[0]));
    }
    keyboard_buffer_send();
    EXPECT_EQ(keyboard_send_buffer[2], KEY_C);
    EXPECT_EQ(keyboard_send_buffer[3], KEY_NO_EVENT);

    g_keyboard_tick+=10;
    advanced_key_update(&g_keyboard_advanced_keys[0],A_ANIT_NORM(0.1));
    keyboard_clear_buffer();
    if (g_keyboard_advanced_keys[0].key.report_state)
    {
        keyboard_advanced_key_event_handler(&g_keyboard_advanced_keys[0],
            MK_EVENT(layer_cache_get_keycode(g_keyboard_advanced_keys[0].key.id), KEYBOARD_EVENT_KEY_TRUE, &g_keyboard_advanced_keys[0]));
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

    advanced_key_update(&g_keyboard_advanced_keys[0],A_ANIT_NORM(1));
    advanced_key_update(&g_keyboard_advanced_keys[1],A_ANIT_NORM(1));
    advanced_key_update(&g_keyboard_advanced_keys[0],A_ANIT_NORM(0));
    advanced_key_update(&g_keyboard_advanced_keys[1],A_ANIT_NORM(0));

    advanced_key_update(&g_keyboard_advanced_keys[0],A_ANIT_NORM(0.3));
    advanced_key_update(&g_keyboard_advanced_keys[1],A_ANIT_NORM(0));
    keyboard_clear_buffer();
    if (g_keyboard_advanced_keys[0].key.report_state)
    {
        keyboard_advanced_key_event_handler(&g_keyboard_advanced_keys[0],
            MK_EVENT(layer_cache_get_keycode(g_keyboard_advanced_keys[0].key.id), KEYBOARD_EVENT_KEY_TRUE, &g_keyboard_advanced_keys[0]));
    }
    if (g_keyboard_advanced_keys[1].key.report_state)
    {
        keyboard_advanced_key_event_handler(&g_keyboard_advanced_keys[1],
            MK_EVENT(layer_cache_get_keycode(g_keyboard_advanced_keys[1].key.id), KEYBOARD_EVENT_KEY_TRUE, &g_keyboard_advanced_keys[1]));
    }
    keyboard_buffer_send();
    EXPECT_EQ(keyboard_send_buffer[2], KEY_A);
    EXPECT_EQ(keyboard_send_buffer[3], KEY_NO_EVENT);


    advanced_key_update(&g_keyboard_advanced_keys[0],A_ANIT_NORM(0.3));
    advanced_key_update(&g_keyboard_advanced_keys[1],A_ANIT_NORM(0.9));
    advanced_key_update(&g_keyboard_advanced_keys[0],A_ANIT_NORM(0.3));
    advanced_key_update(&g_keyboard_advanced_keys[1],A_ANIT_NORM(0.9));
    keyboard_clear_buffer();
    if (g_keyboard_advanced_keys[0].key.report_state)
    {
        keyboard_advanced_key_event_handler(&g_keyboard_advanced_keys[0],
            MK_EVENT(layer_cache_get_keycode(g_keyboard_advanced_keys[0].key.id), KEYBOARD_EVENT_KEY_TRUE, &g_keyboard_advanced_keys[0]));
    }
    if (g_keyboard_advanced_keys[1].key.report_state)
    {
        keyboard_advanced_key_event_handler(&g_keyboard_advanced_keys[1],
            MK_EVENT(layer_cache_get_keycode(g_keyboard_advanced_keys[1].key.id), KEYBOARD_EVENT_KEY_TRUE, &g_keyboard_advanced_keys[1]));
    }
    keyboard_buffer_send();
    EXPECT_EQ(keyboard_send_buffer[2], KEY_B);
    EXPECT_EQ(keyboard_send_buffer[3], KEY_NO_EVENT);

    advanced_key_update(&g_keyboard_advanced_keys[0],A_ANIT_NORM(1));
    advanced_key_update(&g_keyboard_advanced_keys[1],A_ANIT_NORM(0.9));
    advanced_key_update(&g_keyboard_advanced_keys[0],A_ANIT_NORM(1));
    advanced_key_update(&g_keyboard_advanced_keys[1],A_ANIT_NORM(0.9));
    keyboard_clear_buffer();
    if (g_keyboard_advanced_keys[0].key.report_state)
    {
        keyboard_advanced_key_event_handler(&g_keyboard_advanced_keys[0],
            MK_EVENT(layer_cache_get_keycode(g_keyboard_advanced_keys[0].key.id), KEYBOARD_EVENT_KEY_TRUE, &g_keyboard_advanced_keys[0]));
    }
    if (g_keyboard_advanced_keys[1].key.report_state)
    {
        keyboard_advanced_key_event_handler(&g_keyboard_advanced_keys[1],
            MK_EVENT(layer_cache_get_keycode(g_keyboard_advanced_keys[1].key.id), KEYBOARD_EVENT_KEY_TRUE, &g_keyboard_advanced_keys[1]));
    }
    keyboard_buffer_send();
    EXPECT_EQ(keyboard_send_buffer[2], KEY_A);
    EXPECT_EQ(keyboard_send_buffer[3], KEY_NO_EVENT);

    dynamic_key->m.mode |= 0x80;
    advanced_key_update(&g_keyboard_advanced_keys[0],A_ANIT_NORM(0));
    advanced_key_update(&g_keyboard_advanced_keys[1],A_ANIT_NORM(0));
    advanced_key_update(&g_keyboard_advanced_keys[0],A_ANIT_NORM(1));
    advanced_key_update(&g_keyboard_advanced_keys[1],A_ANIT_NORM(0.9));
    advanced_key_update(&g_keyboard_advanced_keys[0],A_ANIT_NORM(1));
    advanced_key_update(&g_keyboard_advanced_keys[1],A_ANIT_NORM(0.9));
    keyboard_clear_buffer();
    if (g_keyboard_advanced_keys[0].key.report_state)
    {
        keyboard_advanced_key_event_handler(&g_keyboard_advanced_keys[0],
            MK_EVENT(layer_cache_get_keycode(g_keyboard_advanced_keys[0].key.id), KEYBOARD_EVENT_KEY_TRUE, &g_keyboard_advanced_keys[0]));
    }
    if (g_keyboard_advanced_keys[1].key.report_state)
    {
        keyboard_advanced_key_event_handler(&g_keyboard_advanced_keys[1],
            MK_EVENT(layer_cache_get_keycode(g_keyboard_advanced_keys[1].key.id), KEYBOARD_EVENT_KEY_TRUE, &g_keyboard_advanced_keys[1]));
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

    advanced_key_update(&g_keyboard_advanced_keys[0],A_ANIT_NORM(1));
    advanced_key_update(&g_keyboard_advanced_keys[1],A_ANIT_NORM(1));
    advanced_key_update(&g_keyboard_advanced_keys[0],A_ANIT_NORM(0));
    advanced_key_update(&g_keyboard_advanced_keys[1],A_ANIT_NORM(0));

    advanced_key_update(&g_keyboard_advanced_keys[0],A_ANIT_NORM(0.6));
    advanced_key_update(&g_keyboard_advanced_keys[1],A_ANIT_NORM(0));
    EXPECT_TRUE(g_keyboard_advanced_keys[0].key.state);
    EXPECT_FALSE(g_keyboard_advanced_keys[1].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[0].key.report_state);
    EXPECT_FALSE(g_keyboard_advanced_keys[1].key.report_state);

    advanced_key_update(&g_keyboard_advanced_keys[0],A_ANIT_NORM(0.6));
    advanced_key_update(&g_keyboard_advanced_keys[1],A_ANIT_NORM(0.6));
    EXPECT_TRUE(g_keyboard_advanced_keys[0].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[1].key.state);
    EXPECT_FALSE(g_keyboard_advanced_keys[0].key.report_state);
    EXPECT_TRUE(g_keyboard_advanced_keys[1].key.report_state);

    advanced_key_update(&g_keyboard_advanced_keys[0],A_ANIT_NORM(0.3));
    advanced_key_update(&g_keyboard_advanced_keys[1],A_ANIT_NORM(0.6));
    EXPECT_FALSE(g_keyboard_advanced_keys[0].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[1].key.state);
    EXPECT_FALSE(g_keyboard_advanced_keys[0].key.report_state);
    EXPECT_TRUE(g_keyboard_advanced_keys[1].key.report_state);

    advanced_key_update(&g_keyboard_advanced_keys[0],A_ANIT_NORM(0.6));
    advanced_key_update(&g_keyboard_advanced_keys[1],A_ANIT_NORM(0.6));
    EXPECT_TRUE(g_keyboard_advanced_keys[0].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[1].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[0].key.report_state);
    EXPECT_FALSE(g_keyboard_advanced_keys[1].key.report_state);

    advanced_key_update(&g_keyboard_advanced_keys[0],A_ANIT_NORM(0.9));
    advanced_key_update(&g_keyboard_advanced_keys[1],A_ANIT_NORM(0.9));
    EXPECT_TRUE(g_keyboard_advanced_keys[0].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[1].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[0].key.report_state);
    EXPECT_FALSE(g_keyboard_advanced_keys[1].key.report_state);

    dynamic_key->m.mode |= 0x80;
    advanced_key_update(&g_keyboard_advanced_keys[0],A_ANIT_NORM(0));
    advanced_key_update(&g_keyboard_advanced_keys[1],A_ANIT_NORM(0));
    advanced_key_update(&g_keyboard_advanced_keys[0],A_ANIT_NORM(0.9));
    advanced_key_update(&g_keyboard_advanced_keys[1],A_ANIT_NORM(0.9));
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

    advanced_key_update(&g_keyboard_advanced_keys[0],A_ANIT_NORM(1));
    advanced_key_update(&g_keyboard_advanced_keys[1],A_ANIT_NORM(1));
    advanced_key_update(&g_keyboard_advanced_keys[0],A_ANIT_NORM(0));
    advanced_key_update(&g_keyboard_advanced_keys[1],A_ANIT_NORM(0));

    advanced_key_update(&g_keyboard_advanced_keys[0],A_ANIT_NORM(0.6));
    advanced_key_update(&g_keyboard_advanced_keys[1],A_ANIT_NORM(0));
    EXPECT_TRUE(g_keyboard_advanced_keys[0].key.state);
    EXPECT_FALSE(g_keyboard_advanced_keys[1].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[0].key.report_state);
    EXPECT_FALSE(g_keyboard_advanced_keys[1].key.report_state);

    advanced_key_update(&g_keyboard_advanced_keys[0],A_ANIT_NORM(0.6));
    advanced_key_update(&g_keyboard_advanced_keys[1],A_ANIT_NORM(0.6));
    EXPECT_TRUE(g_keyboard_advanced_keys[0].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[1].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[0].key.report_state);
    EXPECT_FALSE(g_keyboard_advanced_keys[1].key.report_state);

    advanced_key_update(&g_keyboard_advanced_keys[0],A_ANIT_NORM(0.3));
    advanced_key_update(&g_keyboard_advanced_keys[1],A_ANIT_NORM(0.6));
    EXPECT_FALSE(g_keyboard_advanced_keys[0].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[1].key.state);
    EXPECT_FALSE(g_keyboard_advanced_keys[0].key.report_state);
    EXPECT_TRUE(g_keyboard_advanced_keys[1].key.report_state);

    advanced_key_update(&g_keyboard_advanced_keys[0],A_ANIT_NORM(0.6));
    advanced_key_update(&g_keyboard_advanced_keys[1],A_ANIT_NORM(0.6));
    EXPECT_TRUE(g_keyboard_advanced_keys[0].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[1].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[0].key.report_state);
    EXPECT_FALSE(g_keyboard_advanced_keys[1].key.report_state);

    advanced_key_update(&g_keyboard_advanced_keys[0],A_ANIT_NORM(0.9));
    advanced_key_update(&g_keyboard_advanced_keys[1],A_ANIT_NORM(0.9));
    EXPECT_TRUE(g_keyboard_advanced_keys[0].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[1].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[0].key.report_state);
    EXPECT_FALSE(g_keyboard_advanced_keys[1].key.report_state);

    dynamic_key->m.mode |= 0x80;
    advanced_key_update(&g_keyboard_advanced_keys[0],A_ANIT_NORM(0));
    advanced_key_update(&g_keyboard_advanced_keys[1],A_ANIT_NORM(0));
    advanced_key_update(&g_keyboard_advanced_keys[0],A_ANIT_NORM(0.9));
    advanced_key_update(&g_keyboard_advanced_keys[1],A_ANIT_NORM(0.9));
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

    advanced_key_update(&g_keyboard_advanced_keys[0],A_ANIT_NORM(1));
    advanced_key_update(&g_keyboard_advanced_keys[1],A_ANIT_NORM(1));
    advanced_key_update(&g_keyboard_advanced_keys[0],A_ANIT_NORM(0));
    advanced_key_update(&g_keyboard_advanced_keys[1],A_ANIT_NORM(0));

    advanced_key_update(&g_keyboard_advanced_keys[0],A_ANIT_NORM(0.6));
    advanced_key_update(&g_keyboard_advanced_keys[1],A_ANIT_NORM(0));
    EXPECT_TRUE(g_keyboard_advanced_keys[0].key.state);
    EXPECT_FALSE(g_keyboard_advanced_keys[1].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[0].key.report_state);
    EXPECT_FALSE(g_keyboard_advanced_keys[1].key.report_state);

    advanced_key_update(&g_keyboard_advanced_keys[0],A_ANIT_NORM(0.6));
    advanced_key_update(&g_keyboard_advanced_keys[1],A_ANIT_NORM(0.6));
    EXPECT_TRUE(g_keyboard_advanced_keys[0].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[1].key.state);
    EXPECT_FALSE(g_keyboard_advanced_keys[0].key.report_state);
    EXPECT_TRUE(g_keyboard_advanced_keys[1].key.report_state);

    advanced_key_update(&g_keyboard_advanced_keys[0],A_ANIT_NORM(0.3));
    advanced_key_update(&g_keyboard_advanced_keys[1],A_ANIT_NORM(0.6));
    EXPECT_FALSE(g_keyboard_advanced_keys[0].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[1].key.state);
    EXPECT_FALSE(g_keyboard_advanced_keys[0].key.report_state);
    EXPECT_TRUE(g_keyboard_advanced_keys[1].key.report_state);

    advanced_key_update(&g_keyboard_advanced_keys[0],A_ANIT_NORM(0.6));
    advanced_key_update(&g_keyboard_advanced_keys[1],A_ANIT_NORM(0.6));
    EXPECT_TRUE(g_keyboard_advanced_keys[0].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[1].key.state);
    EXPECT_FALSE(g_keyboard_advanced_keys[0].key.report_state);
    EXPECT_TRUE(g_keyboard_advanced_keys[1].key.report_state);

    advanced_key_update(&g_keyboard_advanced_keys[0],A_ANIT_NORM(0.9));
    advanced_key_update(&g_keyboard_advanced_keys[1],A_ANIT_NORM(0.9));
    EXPECT_TRUE(g_keyboard_advanced_keys[0].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[1].key.state);
    EXPECT_FALSE(g_keyboard_advanced_keys[0].key.report_state);
    EXPECT_TRUE(g_keyboard_advanced_keys[1].key.report_state);

    dynamic_key->m.mode |= 0x80;
    advanced_key_update(&g_keyboard_advanced_keys[0],A_ANIT_NORM(0));
    advanced_key_update(&g_keyboard_advanced_keys[1],A_ANIT_NORM(0));
    advanced_key_update(&g_keyboard_advanced_keys[0],A_ANIT_NORM(0.9));
    advanced_key_update(&g_keyboard_advanced_keys[1],A_ANIT_NORM(0.9));
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

    advanced_key_update(&g_keyboard_advanced_keys[0],A_ANIT_NORM(1));
    advanced_key_update(&g_keyboard_advanced_keys[1],A_ANIT_NORM(1));
    advanced_key_update(&g_keyboard_advanced_keys[0],A_ANIT_NORM(0));
    advanced_key_update(&g_keyboard_advanced_keys[1],A_ANIT_NORM(0));

    advanced_key_update(&g_keyboard_advanced_keys[0],A_ANIT_NORM(0.6));
    advanced_key_update(&g_keyboard_advanced_keys[1],A_ANIT_NORM(0));
    EXPECT_TRUE(g_keyboard_advanced_keys[0].key.state);
    EXPECT_FALSE(g_keyboard_advanced_keys[1].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[0].key.report_state);
    EXPECT_FALSE(g_keyboard_advanced_keys[1].key.report_state);

    advanced_key_update(&g_keyboard_advanced_keys[0],A_ANIT_NORM(0.6));
    advanced_key_update(&g_keyboard_advanced_keys[1],A_ANIT_NORM(0.6));
    EXPECT_TRUE(g_keyboard_advanced_keys[0].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[1].key.state);
    EXPECT_FALSE(g_keyboard_advanced_keys[0].key.report_state);
    EXPECT_FALSE(g_keyboard_advanced_keys[1].key.report_state);

    advanced_key_update(&g_keyboard_advanced_keys[0],A_ANIT_NORM(0.3));
    advanced_key_update(&g_keyboard_advanced_keys[1],A_ANIT_NORM(0.6));
    EXPECT_FALSE(g_keyboard_advanced_keys[0].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[1].key.state);
    EXPECT_FALSE(g_keyboard_advanced_keys[0].key.report_state);
    EXPECT_TRUE(g_keyboard_advanced_keys[1].key.report_state);

    advanced_key_update(&g_keyboard_advanced_keys[0],A_ANIT_NORM(0.6));
    advanced_key_update(&g_keyboard_advanced_keys[1],A_ANIT_NORM(0.6));
    EXPECT_TRUE(g_keyboard_advanced_keys[0].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[1].key.state);
    EXPECT_FALSE(g_keyboard_advanced_keys[0].key.report_state);
    EXPECT_FALSE(g_keyboard_advanced_keys[1].key.report_state);

    advanced_key_update(&g_keyboard_advanced_keys[0],A_ANIT_NORM(0.9));
    advanced_key_update(&g_keyboard_advanced_keys[1],A_ANIT_NORM(0.9));
    EXPECT_TRUE(g_keyboard_advanced_keys[0].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[1].key.state);
    EXPECT_FALSE(g_keyboard_advanced_keys[0].key.report_state);
    EXPECT_FALSE(g_keyboard_advanced_keys[1].key.report_state);

    dynamic_key->m.mode |= 0x80;
    advanced_key_update(&g_keyboard_advanced_keys[0],A_ANIT_NORM(0));
    advanced_key_update(&g_keyboard_advanced_keys[1],A_ANIT_NORM(0));
    advanced_key_update(&g_keyboard_advanced_keys[0],A_ANIT_NORM(0.9));
    advanced_key_update(&g_keyboard_advanced_keys[1],A_ANIT_NORM(0.9));
    EXPECT_TRUE(g_keyboard_advanced_keys[0].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[1].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[0].key.report_state);
    EXPECT_TRUE(g_keyboard_advanced_keys[1].key.report_state);
}