#include <gtest/gtest.h>

#include "keyboard.h"
#include "dynamic_key.h"
#include "layer.h"
#include "math.h"

extern uint8_t keyboard_send_buffer[64];

extern "C" void _dynamic_key_add_buffer(KeyboardEvent event, DynamicKey*dynamic_key);

#include "layer.h"

void dk_advanced_key_update(AdvancedKey* advanced_key, AnalogValue value)
{
    keyboard_event_handler(MK_EVENT(layer_cache_get_keycode(advanced_key->key.id), 
                                                advanced_key_update(advanced_key, value) ? 
                                               advanced_key->key.state ? KEYBOARD_EVENT_KEY_DOWN : KEYBOARD_EVENT_KEY_UP
                                               : advanced_key->key.state ? KEYBOARD_EVENT_KEY_TRUE : KEYBOARD_EVENT_KEY_FALSE ,
                                                advanced_key));
}

TEST(DynamicKey, ModTap)
{
    static DynamicKey dynamic_key = 
    {
        .mt = 
        {
            .type = DYNAMIC_KEY_MOD_TAP,
            .key_binding = {KEY_A, KEY_B},
            .duration = 100,
        }
    };
    memcpy(&g_dynamic_keys[0],&dynamic_key,sizeof(DynamicKey));
    g_keyboard.keymap[0][0] = DYNAMIC_KEY | ((0) << 8);
    g_keymap_cache[0] = g_keyboard.keymap[0][0];

    dk_advanced_key_update(&g_keyboard.advanced_keys[0], 1.0);
    dynamic_key_process();
    keyboard_clear_buffer();
    if (g_keyboard.advanced_keys[0].key.report_state)
    {
        keyboard_add_buffer(MK_EVENT(layer_cache_get_keycode(0), KEYBOARD_EVENT_NO_EVENT, &g_keyboard.advanced_keys[0]));
    }
    keyboard_buffer_send();
    //EXPECT_TRUE(dynamic_key.mt.vkey0.report_state);
    EXPECT_EQ(keyboard_send_buffer[2], KEY_NO_EVENT);
    g_keyboard_tick += 200;


    dk_advanced_key_update(&g_keyboard.advanced_keys[0], 1.0);
    dynamic_key_process();
    keyboard_clear_buffer();
    dynamic_key_add_buffer();
    keyboard_buffer_send();
    //EXPECT_TRUE(dynamic_key.mt.vkey1.report_state);
    EXPECT_EQ(keyboard_send_buffer[2], KEY_B);

    dk_advanced_key_update(&g_keyboard.advanced_keys[0], 0.0);
    dynamic_key_process();
    keyboard_clear_buffer();
    dynamic_key_add_buffer();
    keyboard_buffer_send();
    EXPECT_EQ(keyboard_send_buffer[2], KEY_NO_EVENT);

    dk_advanced_key_update(&g_keyboard.advanced_keys[0], 1.0);
    dynamic_key_process();
    keyboard_clear_buffer();
    dynamic_key_add_buffer();
    keyboard_buffer_send();
    EXPECT_EQ(keyboard_send_buffer[2], KEY_NO_EVENT);
    g_keyboard_tick += 50;

    dk_advanced_key_update(&g_keyboard.advanced_keys[0], 0.0);
    dynamic_key_process();
    keyboard_clear_buffer();
    dynamic_key_add_buffer();
    keyboard_buffer_send();
    //EXPECT_TRUE(dynamic_key.mt.vkey0.state);
    EXPECT_EQ(keyboard_send_buffer[2], KEY_A);
}

TEST(DynamicKey, ToggleKey)
{
    static DynamicKey dynamic_key = 
    {
        .tk = 
        {
            .type = DYNAMIC_KEY_TOGGLE_KEY,
            .key_binding = KEY_A,
            .key_id = 0,
        }
    };
    memcpy(&g_dynamic_keys[0],&dynamic_key,sizeof(DynamicKey));
    g_keyboard.keymap[0][0] = DYNAMIC_KEY | ((0) << 8);
    g_keymap_cache[0] = g_keyboard.keymap[0][0];

    dk_advanced_key_update(&g_keyboard.advanced_keys[0], 1.0);
    dynamic_key_process();
    keyboard_clear_buffer();
    dynamic_key_add_buffer();
    keyboard_buffer_send();
    EXPECT_EQ(g_dynamic_keys[0].tk.state, true);
    EXPECT_EQ(g_keyboard.advanced_keys[0].key.report_state, true);
    EXPECT_EQ(keyboard_send_buffer[2], KEY_A);

    dk_advanced_key_update(&g_keyboard.advanced_keys[0], 0.0);
    dynamic_key_process();
    keyboard_clear_buffer();
    dynamic_key_add_buffer();
    keyboard_buffer_send();
    EXPECT_EQ(g_dynamic_keys[0].tk.state, true);
    EXPECT_EQ(g_keyboard.advanced_keys[0].key.report_state, true);
    EXPECT_EQ(keyboard_send_buffer[2], KEY_A);

    dk_advanced_key_update(&g_keyboard.advanced_keys[0], 1.0);
    dynamic_key_process();
    keyboard_clear_buffer();
    dynamic_key_add_buffer();
    keyboard_buffer_send();
    EXPECT_EQ(g_dynamic_keys[0].tk.state, false);
    EXPECT_EQ(g_keyboard.advanced_keys[0].key.report_state, false);
    EXPECT_EQ(keyboard_send_buffer[2], KEY_NO_EVENT);

    dk_advanced_key_update(&g_keyboard.advanced_keys[0], 0.0);
    dynamic_key_process();
    keyboard_clear_buffer();
    dynamic_key_add_buffer();
    keyboard_buffer_send();
    EXPECT_EQ(g_dynamic_keys[0].tk.state, false);
    EXPECT_EQ(g_keyboard.advanced_keys[0].key.report_state, false);
    EXPECT_EQ(keyboard_send_buffer[2], KEY_NO_EVENT);
}

TEST(DynamicKey, DynamicKeyStroke)
{
    g_keyboard.keymap[0][0] = DYNAMIC_KEY | (0 << 8);
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
    memcpy(&g_dynamic_keys[0], &dynamic_key, sizeof(DynamicKey));
    g_keyboard.keymap[0][0] = DYNAMIC_KEY | ((0) << 8);
    g_keymap_cache[0] = g_keyboard.keymap[0][0];

    dk_advanced_key_update(&g_keyboard.advanced_keys[0],A_ANIT_NORM(1));
    dk_advanced_key_update(&g_keyboard.advanced_keys[0],A_ANIT_NORM(0));
    dynamic_key_process();

    // A keep activating 
    // B deactivate
    // C deactivate
    // D keep activating
    g_keyboard_tick+=10;
    dk_advanced_key_update(&g_keyboard.advanced_keys[0],A_ANIT_NORM(0.3));
    dynamic_key_process();
    keyboard_clear_buffer();
    dynamic_key_add_buffer();
    keyboard_buffer_send();
    EXPECT_EQ(keyboard_send_buffer[2], KEY_A);
    EXPECT_EQ(keyboard_send_buffer[3], KEY_D);
    EXPECT_EQ(keyboard_send_buffer[4], KEY_NO_EVENT);

    g_keyboard_tick+=10;
    dk_advanced_key_update(&g_keyboard.advanced_keys[0],A_ANIT_NORM(0.4));
    dynamic_key_process();
    keyboard_clear_buffer();
    dynamic_key_add_buffer();
    keyboard_buffer_send();
    EXPECT_EQ(keyboard_send_buffer[2], KEY_A);
    EXPECT_EQ(keyboard_send_buffer[3], KEY_D);
    EXPECT_EQ(keyboard_send_buffer[4], KEY_NO_EVENT);

    // A keep activating 
    // B keep activating
    // C keep activating
    // D deactivate
    g_keyboard_tick+=10;
    dk_advanced_key_update(&g_keyboard.advanced_keys[0],A_ANIT_NORM(0.8));
    dynamic_key_process();
    keyboard_clear_buffer();
    dynamic_key_add_buffer();
    keyboard_buffer_send();
    EXPECT_EQ(keyboard_send_buffer[2], KEY_A);
    EXPECT_EQ(keyboard_send_buffer[3], KEY_B);
    EXPECT_EQ(keyboard_send_buffer[4], KEY_C);
    EXPECT_EQ(keyboard_send_buffer[5], KEY_NO_EVENT);

    g_keyboard_tick+=10;
    dk_advanced_key_update(&g_keyboard.advanced_keys[0],A_ANIT_NORM(0.9));
    dynamic_key_process();
    keyboard_clear_buffer();
    dynamic_key_add_buffer();
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
    dk_advanced_key_update(&g_keyboard.advanced_keys[0],A_ANIT_NORM(0.6));
    dynamic_key_process();
    keyboard_clear_buffer();
    dynamic_key_add_buffer();
    keyboard_buffer_send();
    EXPECT_EQ(keyboard_send_buffer[2], KEY_A);
    EXPECT_EQ(keyboard_send_buffer[3], KEY_C);
    EXPECT_EQ(keyboard_send_buffer[4], KEY_D);
    EXPECT_EQ(keyboard_send_buffer[5], KEY_NO_EVENT);

    g_keyboard_tick+=10;
    dk_advanced_key_update(&g_keyboard.advanced_keys[0],A_ANIT_NORM(0.5));
    dynamic_key_process();
    keyboard_clear_buffer();
    dynamic_key_add_buffer();
    keyboard_buffer_send();
    EXPECT_EQ(keyboard_send_buffer[2], KEY_A);
    EXPECT_EQ(keyboard_send_buffer[3], KEY_NO_EVENT);

    // A deactivate 
    // B deactivate
    // C keep activating 
    // D deactivate
    g_keyboard_tick+=10;
    dk_advanced_key_update(&g_keyboard.advanced_keys[0],A_ANIT_NORM(0.2));
    dynamic_key_process();
    keyboard_clear_buffer();
    dynamic_key_add_buffer();
    keyboard_buffer_send();
    EXPECT_EQ(keyboard_send_buffer[2], KEY_C);
    EXPECT_EQ(keyboard_send_buffer[3], KEY_NO_EVENT);

    g_keyboard_tick+=10;
    dk_advanced_key_update(&g_keyboard.advanced_keys[0],A_ANIT_NORM(0.1));
    dynamic_key_process();
    keyboard_clear_buffer();
    dynamic_key_add_buffer();
    keyboard_buffer_send();
    EXPECT_EQ(keyboard_send_buffer[2], KEY_C);
    EXPECT_EQ(keyboard_send_buffer[3], KEY_NO_EVENT);
}

TEST(DynamicKey, MutexDistancePriority)
{
    g_keyboard.keymap[0][0] = DYNAMIC_KEY | (0 << 8);
    g_keyboard.keymap[0][1] = DYNAMIC_KEY | (0 << 8);
    g_keymap_cache[0] = g_keyboard.keymap[0][0];
    g_keymap_cache[1] = g_keyboard.keymap[0][1];
    DynamicKey*dynamic_key = &g_dynamic_keys[0];
    dynamic_key->m.type = DYNAMIC_KEY_MUTEX;
    dynamic_key->m.mode = DK_MUTEX_DISTANCE_PRIORITY;
    dynamic_key->m.key_binding[0] = KEY_A;
    dynamic_key->m.key_binding[1] = KEY_B;
    dynamic_key->m.key_id[0] = 0;
    dynamic_key->m.key_id[1] = 1;

    dk_advanced_key_update(&g_keyboard.advanced_keys[0],A_ANIT_NORM(1));
    dk_advanced_key_update(&g_keyboard.advanced_keys[1],A_ANIT_NORM(1));
    dk_advanced_key_update(&g_keyboard.advanced_keys[0],A_ANIT_NORM(0));
    dk_advanced_key_update(&g_keyboard.advanced_keys[1],A_ANIT_NORM(0));
    dynamic_key_process();

    dk_advanced_key_update(&g_keyboard.advanced_keys[0],A_ANIT_NORM(0.3));
    dk_advanced_key_update(&g_keyboard.advanced_keys[1],A_ANIT_NORM(0));
    dynamic_key_process();
    keyboard_clear_buffer();
    dynamic_key_add_buffer();
    keyboard_buffer_send();
    EXPECT_EQ(keyboard_send_buffer[2], KEY_A);
    EXPECT_EQ(keyboard_send_buffer[3], KEY_NO_EVENT);


    dk_advanced_key_update(&g_keyboard.advanced_keys[0],A_ANIT_NORM(0.3));
    dk_advanced_key_update(&g_keyboard.advanced_keys[1],A_ANIT_NORM(0.9));
    dk_advanced_key_update(&g_keyboard.advanced_keys[0],A_ANIT_NORM(0.3));
    dk_advanced_key_update(&g_keyboard.advanced_keys[1],A_ANIT_NORM(0.9));
    dynamic_key_process();
    keyboard_clear_buffer();
    dynamic_key_add_buffer();
    keyboard_buffer_send();
    EXPECT_EQ(keyboard_send_buffer[2], KEY_B);
    EXPECT_EQ(keyboard_send_buffer[3], KEY_NO_EVENT);

    dk_advanced_key_update(&g_keyboard.advanced_keys[0],A_ANIT_NORM(1));
    dk_advanced_key_update(&g_keyboard.advanced_keys[1],A_ANIT_NORM(0.9));
    dk_advanced_key_update(&g_keyboard.advanced_keys[0],A_ANIT_NORM(1));
    dk_advanced_key_update(&g_keyboard.advanced_keys[1],A_ANIT_NORM(0.9));
    dynamic_key_process();
    keyboard_clear_buffer();
    dynamic_key_add_buffer();
    keyboard_buffer_send();
    EXPECT_EQ(keyboard_send_buffer[2], KEY_A);
    EXPECT_EQ(keyboard_send_buffer[3], KEY_NO_EVENT);
    
    dk_advanced_key_update(&g_keyboard.advanced_keys[0],A_ANIT_NORM(0));
    dk_advanced_key_update(&g_keyboard.advanced_keys[1],A_ANIT_NORM(0));
    dk_advanced_key_update(&g_keyboard.advanced_keys[0],A_ANIT_NORM(1));
    dk_advanced_key_update(&g_keyboard.advanced_keys[1],A_ANIT_NORM(0.9));
    dynamic_key_process();
    keyboard_clear_buffer();
    dynamic_key_add_buffer();
    keyboard_buffer_send();
    EXPECT_EQ(keyboard_send_buffer[2], KEY_A);
    EXPECT_EQ(keyboard_send_buffer[3], KEY_NO_EVENT);
    EXPECT_EQ(keyboard_send_buffer[4], KEY_NO_EVENT);
    EXPECT_EQ(keyboard_send_buffer[5], KEY_NO_EVENT);
    EXPECT_EQ(keyboard_send_buffer[6], KEY_NO_EVENT);

    dk_advanced_key_update(&g_keyboard.advanced_keys[0],A_ANIT_NORM(0));
    dk_advanced_key_update(&g_keyboard.advanced_keys[1],A_ANIT_NORM(0));
    dk_advanced_key_update(&g_keyboard.advanced_keys[0],A_ANIT_NORM(0));
    dk_advanced_key_update(&g_keyboard.advanced_keys[1],A_ANIT_NORM(0));
    dynamic_key_process();
    keyboard_clear_buffer();
    dynamic_key_add_buffer();
    keyboard_buffer_send();
    EXPECT_EQ(dynamic_key->m.key_report_state[0], false);
    EXPECT_EQ(dynamic_key->m.key_report_state[1], false);
    EXPECT_EQ(keyboard_send_buffer[2], KEY_NO_EVENT);
    EXPECT_EQ(keyboard_send_buffer[3], KEY_NO_EVENT);
    EXPECT_EQ(keyboard_send_buffer[4], KEY_NO_EVENT);
    EXPECT_EQ(keyboard_send_buffer[5], KEY_NO_EVENT);
    EXPECT_EQ(keyboard_send_buffer[6], KEY_NO_EVENT);

    dynamic_key->m.mode |= 0x80;
    dk_advanced_key_update(&g_keyboard.advanced_keys[0],A_ANIT_NORM(0));
    dk_advanced_key_update(&g_keyboard.advanced_keys[1],A_ANIT_NORM(0));
    dk_advanced_key_update(&g_keyboard.advanced_keys[0],A_ANIT_NORM(1));
    dk_advanced_key_update(&g_keyboard.advanced_keys[1],A_ANIT_NORM(0.9));
    dynamic_key_process();
    keyboard_clear_buffer();
    dynamic_key_add_buffer();
    keyboard_buffer_send();
    EXPECT_EQ(keyboard_send_buffer[2], KEY_A);
    EXPECT_EQ(keyboard_send_buffer[3], KEY_B);
    EXPECT_EQ(keyboard_send_buffer[4], KEY_NO_EVENT);
    EXPECT_EQ(keyboard_send_buffer[5], KEY_NO_EVENT);
    EXPECT_EQ(keyboard_send_buffer[6], KEY_NO_EVENT);
}

TEST(DynamicKey, MutexLastPriority)
{
    g_keyboard.keymap[0][0] = DYNAMIC_KEY | (0 << 8);
    g_keyboard.keymap[0][1] = DYNAMIC_KEY | (0 << 8);
    g_keymap_cache[0] = g_keyboard.keymap[0][0];
    g_keymap_cache[1] = g_keyboard.keymap[0][1];
    DynamicKey*dynamic_key = &g_dynamic_keys[0];
    dynamic_key->m.type = DYNAMIC_KEY_MUTEX;
    dynamic_key->m.mode = DK_MUTEX_LAST_PRIORITY;
    dynamic_key->m.key_binding[0] = KEY_A;
    dynamic_key->m.key_binding[1] = KEY_B;
    dynamic_key->m.key_id[0] = 0;
    dynamic_key->m.key_id[1] = 1;

    dk_advanced_key_update(&g_keyboard.advanced_keys[0],A_ANIT_NORM(1));
    dk_advanced_key_update(&g_keyboard.advanced_keys[1],A_ANIT_NORM(1));
    dk_advanced_key_update(&g_keyboard.advanced_keys[0],A_ANIT_NORM(0));
    dk_advanced_key_update(&g_keyboard.advanced_keys[1],A_ANIT_NORM(0));
    dynamic_key_process();

    dk_advanced_key_update(&g_keyboard.advanced_keys[0],A_ANIT_NORM(0.6));
    dk_advanced_key_update(&g_keyboard.advanced_keys[1],A_ANIT_NORM(0));
    dynamic_key_process();
    EXPECT_TRUE(g_keyboard.advanced_keys[0].key.state);
    EXPECT_FALSE(g_keyboard.advanced_keys[1].key.state);
    EXPECT_TRUE(g_keyboard.advanced_keys[0].key.report_state);
    EXPECT_FALSE(g_keyboard.advanced_keys[1].key.report_state);

    dk_advanced_key_update(&g_keyboard.advanced_keys[0],A_ANIT_NORM(0.6));
    dk_advanced_key_update(&g_keyboard.advanced_keys[1],A_ANIT_NORM(0.6));
    dynamic_key_process();
    EXPECT_TRUE(g_keyboard.advanced_keys[0].key.state);
    EXPECT_TRUE(g_keyboard.advanced_keys[1].key.state);
    EXPECT_FALSE(g_keyboard.advanced_keys[0].key.report_state);
    EXPECT_TRUE(g_keyboard.advanced_keys[1].key.report_state);

    dk_advanced_key_update(&g_keyboard.advanced_keys[0],A_ANIT_NORM(0.3));
    dk_advanced_key_update(&g_keyboard.advanced_keys[1],A_ANIT_NORM(0.6));
    dynamic_key_process();
    EXPECT_FALSE(g_keyboard.advanced_keys[0].key.state);
    EXPECT_TRUE(g_keyboard.advanced_keys[1].key.state);
    EXPECT_FALSE(g_keyboard.advanced_keys[0].key.report_state);
    EXPECT_TRUE(g_keyboard.advanced_keys[1].key.report_state);

    dk_advanced_key_update(&g_keyboard.advanced_keys[0],A_ANIT_NORM(0.6));
    dk_advanced_key_update(&g_keyboard.advanced_keys[1],A_ANIT_NORM(0.6));
    dynamic_key_process();
    EXPECT_TRUE(g_keyboard.advanced_keys[0].key.state);
    EXPECT_TRUE(g_keyboard.advanced_keys[1].key.state);
    EXPECT_TRUE(g_keyboard.advanced_keys[0].key.report_state);
    EXPECT_FALSE(g_keyboard.advanced_keys[1].key.report_state);

    dk_advanced_key_update(&g_keyboard.advanced_keys[0],A_ANIT_NORM(0.9));
    dk_advanced_key_update(&g_keyboard.advanced_keys[1],A_ANIT_NORM(0.9));
    dynamic_key_process();
    EXPECT_TRUE(g_keyboard.advanced_keys[0].key.state);
    EXPECT_TRUE(g_keyboard.advanced_keys[1].key.state);
    EXPECT_TRUE(g_keyboard.advanced_keys[0].key.report_state);
    EXPECT_FALSE(g_keyboard.advanced_keys[1].key.report_state);

    dynamic_key->m.mode |= 0x80;
    dk_advanced_key_update(&g_keyboard.advanced_keys[0],A_ANIT_NORM(0));
    dk_advanced_key_update(&g_keyboard.advanced_keys[1],A_ANIT_NORM(0));
    dk_advanced_key_update(&g_keyboard.advanced_keys[0],A_ANIT_NORM(0.9));
    dk_advanced_key_update(&g_keyboard.advanced_keys[1],A_ANIT_NORM(0.9));
    dynamic_key_process();
    EXPECT_TRUE(g_keyboard.advanced_keys[0].key.state);
    EXPECT_TRUE(g_keyboard.advanced_keys[1].key.state);
    EXPECT_TRUE(g_keyboard.advanced_keys[0].key.report_state);
    EXPECT_TRUE(g_keyboard.advanced_keys[1].key.report_state);
}

TEST(DynamicKey, MutexKey1Priority)
{
    g_keyboard.keymap[0][0] = DYNAMIC_KEY | (0 << 8);
    g_keyboard.keymap[0][1] = DYNAMIC_KEY | (0 << 8);
    g_keymap_cache[0] = g_keyboard.keymap[0][0];
    g_keymap_cache[1] = g_keyboard.keymap[0][1];
    DynamicKey*dynamic_key = &g_dynamic_keys[0];
    dynamic_key->m.type = DYNAMIC_KEY_MUTEX;
    dynamic_key->m.mode = DK_MUTEX_KEY1_PRIORITY;
    dynamic_key->m.key_binding[0] = KEY_A;
    dynamic_key->m.key_binding[1] = KEY_B;
    dynamic_key->m.key_id[0] = 0;
    dynamic_key->m.key_id[1] = 1;

    dk_advanced_key_update(&g_keyboard.advanced_keys[0],A_ANIT_NORM(1));
    dk_advanced_key_update(&g_keyboard.advanced_keys[1],A_ANIT_NORM(1));
    dk_advanced_key_update(&g_keyboard.advanced_keys[0],A_ANIT_NORM(0));
    dk_advanced_key_update(&g_keyboard.advanced_keys[1],A_ANIT_NORM(0));
    dynamic_key_process();

    dk_advanced_key_update(&g_keyboard.advanced_keys[0],A_ANIT_NORM(0.6));
    dk_advanced_key_update(&g_keyboard.advanced_keys[1],A_ANIT_NORM(0));
    dynamic_key_process();
    EXPECT_TRUE(g_keyboard.advanced_keys[0].key.state);
    EXPECT_FALSE(g_keyboard.advanced_keys[1].key.state);
    EXPECT_TRUE(g_keyboard.advanced_keys[0].key.report_state);
    EXPECT_FALSE(g_keyboard.advanced_keys[1].key.report_state);

    dk_advanced_key_update(&g_keyboard.advanced_keys[0],A_ANIT_NORM(0.6));
    dk_advanced_key_update(&g_keyboard.advanced_keys[1],A_ANIT_NORM(0.6));
    dynamic_key_process();
    EXPECT_TRUE(g_keyboard.advanced_keys[0].key.state);
    EXPECT_TRUE(g_keyboard.advanced_keys[1].key.state);
    EXPECT_TRUE(g_keyboard.advanced_keys[0].key.report_state);
    EXPECT_FALSE(g_keyboard.advanced_keys[1].key.report_state);

    dk_advanced_key_update(&g_keyboard.advanced_keys[0],A_ANIT_NORM(0.3));
    dk_advanced_key_update(&g_keyboard.advanced_keys[1],A_ANIT_NORM(0.6));
    dynamic_key_process();
    EXPECT_FALSE(g_keyboard.advanced_keys[0].key.state);
    EXPECT_TRUE(g_keyboard.advanced_keys[1].key.state);
    EXPECT_FALSE(g_keyboard.advanced_keys[0].key.report_state);
    EXPECT_TRUE(g_keyboard.advanced_keys[1].key.report_state);

    dk_advanced_key_update(&g_keyboard.advanced_keys[0],A_ANIT_NORM(0.6));
    dk_advanced_key_update(&g_keyboard.advanced_keys[1],A_ANIT_NORM(0.6));
    dynamic_key_process();
    EXPECT_TRUE(g_keyboard.advanced_keys[0].key.state);
    EXPECT_TRUE(g_keyboard.advanced_keys[1].key.state);
    EXPECT_TRUE(g_keyboard.advanced_keys[0].key.report_state);
    EXPECT_FALSE(g_keyboard.advanced_keys[1].key.report_state);

    dk_advanced_key_update(&g_keyboard.advanced_keys[0],A_ANIT_NORM(0.9));
    dk_advanced_key_update(&g_keyboard.advanced_keys[1],A_ANIT_NORM(0.9));
    dynamic_key_process();
    EXPECT_TRUE(g_keyboard.advanced_keys[0].key.state);
    EXPECT_TRUE(g_keyboard.advanced_keys[1].key.state);
    EXPECT_TRUE(g_keyboard.advanced_keys[0].key.report_state);
    EXPECT_FALSE(g_keyboard.advanced_keys[1].key.report_state);

    dynamic_key->m.mode |= 0x80;
    dk_advanced_key_update(&g_keyboard.advanced_keys[0],A_ANIT_NORM(0));
    dk_advanced_key_update(&g_keyboard.advanced_keys[1],A_ANIT_NORM(0));
    dk_advanced_key_update(&g_keyboard.advanced_keys[0],A_ANIT_NORM(0.9));
    dk_advanced_key_update(&g_keyboard.advanced_keys[1],A_ANIT_NORM(0.9));
    dynamic_key_process();
    EXPECT_TRUE(g_keyboard.advanced_keys[0].key.state);
    EXPECT_TRUE(g_keyboard.advanced_keys[1].key.state);
    EXPECT_TRUE(g_keyboard.advanced_keys[0].key.report_state);
    EXPECT_TRUE(g_keyboard.advanced_keys[1].key.report_state);
}

TEST(DynamicKey, MutexKey2Priority)
{
    g_keyboard.keymap[0][0] = DYNAMIC_KEY | (0 << 8);
    g_keyboard.keymap[0][1] = DYNAMIC_KEY | (0 << 8);
    g_keymap_cache[0] = g_keyboard.keymap[0][0];
    g_keymap_cache[1] = g_keyboard.keymap[0][1];
    DynamicKey*dynamic_key = &g_dynamic_keys[0];
    dynamic_key->m.type = DYNAMIC_KEY_MUTEX;
    dynamic_key->m.mode = DK_MUTEX_KEY2_PRIORITY;
    dynamic_key->m.key_binding[0] = KEY_A;
    dynamic_key->m.key_binding[1] = KEY_B;
    dynamic_key->m.key_id[0] = 0;
    dynamic_key->m.key_id[1] = 1;

    dk_advanced_key_update(&g_keyboard.advanced_keys[0],A_ANIT_NORM(1));
    dk_advanced_key_update(&g_keyboard.advanced_keys[1],A_ANIT_NORM(1));
    dk_advanced_key_update(&g_keyboard.advanced_keys[0],A_ANIT_NORM(0));
    dk_advanced_key_update(&g_keyboard.advanced_keys[1],A_ANIT_NORM(0));
    dynamic_key_process();

    dk_advanced_key_update(&g_keyboard.advanced_keys[0],A_ANIT_NORM(0.6));
    dk_advanced_key_update(&g_keyboard.advanced_keys[1],A_ANIT_NORM(0));
    dynamic_key_process();
    EXPECT_TRUE(g_keyboard.advanced_keys[0].key.state);
    EXPECT_FALSE(g_keyboard.advanced_keys[1].key.state);
    EXPECT_TRUE(g_keyboard.advanced_keys[0].key.report_state);
    EXPECT_FALSE(g_keyboard.advanced_keys[1].key.report_state);

    dk_advanced_key_update(&g_keyboard.advanced_keys[0],A_ANIT_NORM(0.6));
    dk_advanced_key_update(&g_keyboard.advanced_keys[1],A_ANIT_NORM(0.6));
    dynamic_key_process();
    EXPECT_TRUE(g_keyboard.advanced_keys[0].key.state);
    EXPECT_TRUE(g_keyboard.advanced_keys[1].key.state);
    EXPECT_FALSE(g_keyboard.advanced_keys[0].key.report_state);
    EXPECT_TRUE(g_keyboard.advanced_keys[1].key.report_state);

    dk_advanced_key_update(&g_keyboard.advanced_keys[0],A_ANIT_NORM(0.3));
    dk_advanced_key_update(&g_keyboard.advanced_keys[1],A_ANIT_NORM(0.6));
    dynamic_key_process();
    EXPECT_FALSE(g_keyboard.advanced_keys[0].key.state);
    EXPECT_TRUE(g_keyboard.advanced_keys[1].key.state);
    EXPECT_FALSE(g_keyboard.advanced_keys[0].key.report_state);
    EXPECT_TRUE(g_keyboard.advanced_keys[1].key.report_state);

    dk_advanced_key_update(&g_keyboard.advanced_keys[0],A_ANIT_NORM(0.6));
    dk_advanced_key_update(&g_keyboard.advanced_keys[1],A_ANIT_NORM(0.6));
    dynamic_key_process();
    EXPECT_TRUE(g_keyboard.advanced_keys[0].key.state);
    EXPECT_TRUE(g_keyboard.advanced_keys[1].key.state);
    EXPECT_FALSE(g_keyboard.advanced_keys[0].key.report_state);
    EXPECT_TRUE(g_keyboard.advanced_keys[1].key.report_state);

    dk_advanced_key_update(&g_keyboard.advanced_keys[0],A_ANIT_NORM(0.9));
    dk_advanced_key_update(&g_keyboard.advanced_keys[1],A_ANIT_NORM(0.9));
    dynamic_key_process();
    EXPECT_TRUE(g_keyboard.advanced_keys[0].key.state);
    EXPECT_TRUE(g_keyboard.advanced_keys[1].key.state);
    EXPECT_FALSE(g_keyboard.advanced_keys[0].key.report_state);
    EXPECT_TRUE(g_keyboard.advanced_keys[1].key.report_state);

    dynamic_key->m.mode |= 0x80;
    dk_advanced_key_update(&g_keyboard.advanced_keys[0],A_ANIT_NORM(0));
    dk_advanced_key_update(&g_keyboard.advanced_keys[1],A_ANIT_NORM(0));
    dk_advanced_key_update(&g_keyboard.advanced_keys[0],A_ANIT_NORM(0.9));
    dk_advanced_key_update(&g_keyboard.advanced_keys[1],A_ANIT_NORM(0.9));
    dynamic_key_process();
    EXPECT_TRUE(g_keyboard.advanced_keys[0].key.state);
    EXPECT_TRUE(g_keyboard.advanced_keys[1].key.state);
    EXPECT_TRUE(g_keyboard.advanced_keys[0].key.report_state);
    EXPECT_TRUE(g_keyboard.advanced_keys[1].key.report_state);
}


TEST(DynamicKey, MutexKeyNeural)
{
    g_keyboard.keymap[0][0] = DYNAMIC_KEY | (0 << 8);
    g_keyboard.keymap[0][1] = DYNAMIC_KEY | (0 << 8);
    g_keymap_cache[0] = g_keyboard.keymap[0][0];
    g_keymap_cache[1] = g_keyboard.keymap[0][1];
    DynamicKey*dynamic_key = &g_dynamic_keys[0];
    dynamic_key->m.type = DYNAMIC_KEY_MUTEX;
    dynamic_key->m.mode = DK_MUTEX_NEUTRAL;
    dynamic_key->m.key_binding[0] = KEY_A;
    dynamic_key->m.key_binding[1] = KEY_B;
    dynamic_key->m.key_id[0] = 0;
    dynamic_key->m.key_id[1] = 1;

    dk_advanced_key_update(&g_keyboard.advanced_keys[0],A_ANIT_NORM(1));
    dk_advanced_key_update(&g_keyboard.advanced_keys[1],A_ANIT_NORM(1));
    dk_advanced_key_update(&g_keyboard.advanced_keys[0],A_ANIT_NORM(0));
    dk_advanced_key_update(&g_keyboard.advanced_keys[1],A_ANIT_NORM(0));
    dynamic_key_process();

    dk_advanced_key_update(&g_keyboard.advanced_keys[0],A_ANIT_NORM(0.6));
    dk_advanced_key_update(&g_keyboard.advanced_keys[1],A_ANIT_NORM(0));
    dynamic_key_process();
    EXPECT_TRUE(g_keyboard.advanced_keys[0].key.state);
    EXPECT_FALSE(g_keyboard.advanced_keys[1].key.state);
    EXPECT_TRUE(g_keyboard.advanced_keys[0].key.report_state);
    EXPECT_FALSE(g_keyboard.advanced_keys[1].key.report_state);

    dk_advanced_key_update(&g_keyboard.advanced_keys[0],A_ANIT_NORM(0.6));
    dk_advanced_key_update(&g_keyboard.advanced_keys[1],A_ANIT_NORM(0.6));
    dynamic_key_process();
    EXPECT_TRUE(g_keyboard.advanced_keys[0].key.state);
    EXPECT_TRUE(g_keyboard.advanced_keys[1].key.state);
    EXPECT_FALSE(g_keyboard.advanced_keys[0].key.report_state);
    EXPECT_FALSE(g_keyboard.advanced_keys[1].key.report_state);

    dk_advanced_key_update(&g_keyboard.advanced_keys[0],A_ANIT_NORM(0.3));
    dk_advanced_key_update(&g_keyboard.advanced_keys[1],A_ANIT_NORM(0.6));
    dynamic_key_process();
    EXPECT_FALSE(g_keyboard.advanced_keys[0].key.state);
    EXPECT_TRUE(g_keyboard.advanced_keys[1].key.state);
    EXPECT_FALSE(g_keyboard.advanced_keys[0].key.report_state);
    EXPECT_TRUE(g_keyboard.advanced_keys[1].key.report_state);

    dk_advanced_key_update(&g_keyboard.advanced_keys[0],A_ANIT_NORM(0.6));
    dk_advanced_key_update(&g_keyboard.advanced_keys[1],A_ANIT_NORM(0.6));
    dynamic_key_process();
    EXPECT_TRUE(g_keyboard.advanced_keys[0].key.state);
    EXPECT_TRUE(g_keyboard.advanced_keys[1].key.state);
    EXPECT_FALSE(g_keyboard.advanced_keys[0].key.report_state);
    EXPECT_FALSE(g_keyboard.advanced_keys[1].key.report_state);

    dk_advanced_key_update(&g_keyboard.advanced_keys[0],A_ANIT_NORM(0.9));
    dk_advanced_key_update(&g_keyboard.advanced_keys[1],A_ANIT_NORM(0.9));
    dynamic_key_process();
    EXPECT_TRUE(g_keyboard.advanced_keys[0].key.state);
    EXPECT_TRUE(g_keyboard.advanced_keys[1].key.state);
    EXPECT_FALSE(g_keyboard.advanced_keys[0].key.report_state);
    EXPECT_FALSE(g_keyboard.advanced_keys[1].key.report_state);

    dynamic_key->m.mode |= 0x80;
    dk_advanced_key_update(&g_keyboard.advanced_keys[0],A_ANIT_NORM(0));
    dk_advanced_key_update(&g_keyboard.advanced_keys[1],A_ANIT_NORM(0));
    dk_advanced_key_update(&g_keyboard.advanced_keys[0],A_ANIT_NORM(0.9));
    dk_advanced_key_update(&g_keyboard.advanced_keys[1],A_ANIT_NORM(0.9));
    dynamic_key_process();
    EXPECT_TRUE(g_keyboard.advanced_keys[0].key.state);
    EXPECT_TRUE(g_keyboard.advanced_keys[1].key.state);
    EXPECT_TRUE(g_keyboard.advanced_keys[0].key.report_state);
    EXPECT_TRUE(g_keyboard.advanced_keys[1].key.report_state);
}