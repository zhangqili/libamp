#include <gtest/gtest.h>

#include "keyboard.h"
#include "dynamic_key.h"
#include "layer.h"
#include "rgb.h"
#include "math.h"
#include "test_fixture.h"

extern "C" void _dynamic_key_add_buffer(KeyboardEvent event, DynamicKey*dynamic_key);

#include "layer.h"

namespace {

uint8_t user_poller_count = 0;
uint32_t user_poller_tick = 0;
KeyboardEvent last_user_poller_event = {};

void reset_user_poller_capture(void)
{
    user_poller_count = 0;
    user_poller_tick = 0;
    last_user_poller_event = {};
}

void bind_dynamic_key(uint16_t key_id, uint8_t dynamic_key_id = 0)
{
    g_keymap[0][key_id] = DYNAMIC_KEY | (dynamic_key_id << 8);
    g_keymap_cache[key_id] = g_keymap[0][key_id];
}

Keycode collection_keycode(uint8_t collection, uint8_t subcode)
{
    return ((Keycode)subcode << 8) | collection;
}

void expect_dynamic_key_buffer(Keycode first_key, Keycode second_key = KEY_NO_EVENT)
{
    keyboard_clear_buffer();
    dynamic_key_add_buffer();
    keyboard_buffer_send();
    EXPECT_EQ(keyboard_send_buffer[2], first_key);
    EXPECT_EQ(keyboard_send_buffer[3], second_key);
}

} // namespace

extern "C" void keyboard_user_event_poller(KeyboardEvent event, uint32_t tick)
{
    user_poller_count++;
    user_poller_tick = tick;
    last_user_poller_event = event;
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
    g_keymap[0][0] = DYNAMIC_KEY | ((0) << 8);
    g_keymap_cache[0] = g_keymap[0][0];

    keyboard_advanced_key_update(&g_keyboard_advanced_keys[0], A_ANTI_NORM(1.0));
    dynamic_key_process();
    keyboard_clear_buffer();
    if (g_keyboard_advanced_keys[0].key.report_state)
    {
        keyboard_add_buffer(MK_EVENT(layer_cache_get_keycode(0), KEYBOARD_EVENT_NO_EVENT, &g_keyboard_advanced_keys[0]));
    }
    keyboard_buffer_send();
    //EXPECT_TRUE(dynamic_key.mt.vkey0.report_state);
    EXPECT_EQ(keyboard_send_buffer[2], KEY_NO_EVENT);
    g_keyboard_tick += 200;


    keyboard_advanced_key_update(&g_keyboard_advanced_keys[0], A_ANTI_NORM(1.0));
    dynamic_key_process();
    keyboard_clear_buffer();
    dynamic_key_add_buffer();
    keyboard_buffer_send();
    //EXPECT_TRUE(dynamic_key.mt.vkey1.report_state);
    EXPECT_EQ(keyboard_send_buffer[2], KEY_B);

    keyboard_advanced_key_update(&g_keyboard_advanced_keys[0], A_ANTI_NORM(0.0));
    dynamic_key_process();
    keyboard_clear_buffer();
    dynamic_key_add_buffer();
    keyboard_buffer_send();
    EXPECT_EQ(keyboard_send_buffer[2], KEY_NO_EVENT);

    keyboard_advanced_key_update(&g_keyboard_advanced_keys[0], A_ANTI_NORM(1.0));
    dynamic_key_process();
    keyboard_clear_buffer();
    dynamic_key_add_buffer();
    keyboard_buffer_send();
    EXPECT_EQ(keyboard_send_buffer[2], KEY_NO_EVENT);
    g_keyboard_tick += 50;

    keyboard_advanced_key_update(&g_keyboard_advanced_keys[0], A_ANTI_NORM(0.0));
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
    g_keymap[0][0] = DYNAMIC_KEY | ((0) << 8);
    g_keymap_cache[0] = g_keymap[0][0];

    keyboard_advanced_key_update(&g_keyboard_advanced_keys[0], A_ANTI_NORM(1.0));
    dynamic_key_process();
    keyboard_clear_buffer();
    dynamic_key_add_buffer();
    keyboard_buffer_send();
    EXPECT_EQ(g_dynamic_keys[0].tk.state, true);
    EXPECT_EQ(keyboard_send_buffer[2], KEY_A);

    keyboard_advanced_key_update(&g_keyboard_advanced_keys[0], A_ANTI_NORM(0.0));
    dynamic_key_process();
    keyboard_clear_buffer();
    dynamic_key_add_buffer();
    keyboard_buffer_send();
    EXPECT_EQ(g_dynamic_keys[0].tk.state, true);
    EXPECT_EQ(keyboard_send_buffer[2], KEY_A);

    keyboard_advanced_key_update(&g_keyboard_advanced_keys[0], A_ANTI_NORM(1.0));
    dynamic_key_process();
    keyboard_clear_buffer();
    dynamic_key_add_buffer();
    keyboard_buffer_send();
    EXPECT_EQ(g_dynamic_keys[0].tk.state, false);
    EXPECT_EQ(keyboard_send_buffer[2], KEY_NO_EVENT);

    keyboard_advanced_key_update(&g_keyboard_advanced_keys[0], A_ANTI_NORM(0.0));
    dynamic_key_process();
    keyboard_clear_buffer();
    dynamic_key_add_buffer();
    keyboard_buffer_send();
    EXPECT_EQ(g_dynamic_keys[0].tk.state, false);
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
            .press_begin_distance = A_ANTI_NORM(0.25),
            .press_fully_distance = A_ANTI_NORM(0.75),
            .release_begin_distance = A_ANTI_NORM(0.75),
            .release_fully_distance = A_ANTI_NORM(0.25),
        }
    };
    memcpy(&g_dynamic_keys[0], &dynamic_key, sizeof(DynamicKey));
    g_keymap[0][0] = DYNAMIC_KEY | ((0) << 8);
    g_keymap_cache[0] = g_keymap[0][0];

    keyboard_advanced_key_update(&g_keyboard_advanced_keys[0],A_ANTI_NORM(1));
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[0],A_ANTI_NORM(0));
    dynamic_key_process();

    // A keep activating 
    // B deactivate
    // C deactivate
    // D keep activating
    g_keyboard_tick+=10;
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[0],A_ANTI_NORM(0.3));
    dynamic_key_process();
    keyboard_clear_buffer();
    dynamic_key_add_buffer();
    keyboard_buffer_send();
    EXPECT_EQ(keyboard_send_buffer[2], KEY_A);
    EXPECT_EQ(keyboard_send_buffer[3], KEY_D);
    EXPECT_EQ(keyboard_send_buffer[4], KEY_NO_EVENT);

    g_keyboard_tick+=10;
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[0],A_ANTI_NORM(0.4));
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
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[0],A_ANTI_NORM(0.8));
    dynamic_key_process();
    keyboard_clear_buffer();
    dynamic_key_add_buffer();
    keyboard_buffer_send();
    EXPECT_EQ(keyboard_send_buffer[2], KEY_A);
    EXPECT_EQ(keyboard_send_buffer[3], KEY_B);
    EXPECT_EQ(keyboard_send_buffer[4], KEY_C);
    EXPECT_EQ(keyboard_send_buffer[5], KEY_NO_EVENT);

    g_keyboard_tick+=10;
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[0],A_ANTI_NORM(0.9));
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
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[0],A_ANTI_NORM(0.6));
    dynamic_key_process();
    keyboard_clear_buffer();
    dynamic_key_add_buffer();
    keyboard_buffer_send();
    EXPECT_EQ(keyboard_send_buffer[2], KEY_A);
    EXPECT_EQ(keyboard_send_buffer[3], KEY_C);
    EXPECT_EQ(keyboard_send_buffer[4], KEY_D);
    EXPECT_EQ(keyboard_send_buffer[5], KEY_NO_EVENT);

    g_keyboard_tick+=10;
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[0],A_ANTI_NORM(0.5));
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
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[0],A_ANTI_NORM(0.2));
    dynamic_key_process();
    keyboard_clear_buffer();
    dynamic_key_add_buffer();
    keyboard_buffer_send();
    EXPECT_EQ(keyboard_send_buffer[2], KEY_C);
    EXPECT_EQ(keyboard_send_buffer[3], KEY_NO_EVENT);

    g_keyboard_tick+=10;
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[0],A_ANTI_NORM(0.1));
    dynamic_key_process();
    keyboard_clear_buffer();
    dynamic_key_add_buffer();
    keyboard_buffer_send();
    EXPECT_EQ(keyboard_send_buffer[2], KEY_C);
    EXPECT_EQ(keyboard_send_buffer[3], KEY_NO_EVENT);
}

TEST(DynamicKey, DerivedEventsRemainPhysicalForPoller)
{
    reset_user_poller_capture();
    bind_dynamic_key(0);
    DynamicKey* dynamic_key = &g_dynamic_keys[0];
    dynamic_key->tk.type = DYNAMIC_KEY_TOGGLE_KEY;
    dynamic_key->tk.key_binding = KEY_USER;
    dynamic_key->tk.key_id = 0;

    keyboard_advanced_key_update(&g_keyboard_advanced_keys[0], A_ANTI_NORM(1.0f));
    dynamic_key_process();
    keyboard_process();

    EXPECT_EQ(user_poller_count, 1);
    EXPECT_EQ(user_poller_tick, g_keyboard_tick);
    EXPECT_EQ(KEY_USER, KEYCODE_GET_MAIN(last_user_poller_event.keycode));
    EXPECT_EQ(KEYBOARD_EVENT_KEY_DOWN, last_user_poller_event.event);
    EXPECT_FALSE(last_user_poller_event.is_virtual);
}

TEST(DynamicKey, DerivedEventsSetReportFlags)
{
    bind_dynamic_key(0);
    DynamicKey* dynamic_key = &g_dynamic_keys[0];
    dynamic_key->tk.type = DYNAMIC_KEY_TOGGLE_KEY;
    dynamic_key->tk.key_binding = collection_keycode(MOUSE_COLLECTION, MOUSE_LBUTTON);
    dynamic_key->tk.key_id = 0;

    keyboard_advanced_key_update(&g_keyboard_advanced_keys[0], A_ANTI_NORM(1.0f));
    g_keyboard_report_flags.raw = 0;
    dynamic_key_process();

    EXPECT_TRUE((bool)g_keyboard_report_flags.mouse);
}

TEST(DynamicKey, DerivedEventsDoNotRetriggerRgbActivation)
{
    bind_dynamic_key(0);
    DynamicKey* dynamic_key = &g_dynamic_keys[0];
    dynamic_key->tk.type = DYNAMIC_KEY_TOGGLE_KEY;
    dynamic_key->tk.key_binding = KEY_A;
    dynamic_key->tk.key_id = 0;

    uint16_t rgb_index = g_rgb_inverse_mapping[0];
    ASSERT_NE(0xFFFF, rgb_index);
    g_rgb_configs[rgb_index].mode = RGB_MODE_BUBBLE;
    g_rgb_configs[rgb_index].begin_tick = 0;

    g_keyboard_tick = 100;
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[0], A_ANTI_NORM(1.0f));
    keyboard_process();
    EXPECT_EQ(0u, g_rgb_configs[rgb_index].begin_tick);

    g_keyboard_tick = 150;
    dynamic_key_process();
    keyboard_process();
    EXPECT_EQ(150u, g_rgb_configs[rgb_index].begin_tick);
}

TEST(DynamicKey, DynamicKeyStrokeHysteresisPreventsThresholdNoise)
{
    bind_dynamic_key(0);
    const AnalogValue threshold = A_ANTI_NORM(0.5f);
    const AnalogValue half_hysteresis = DYNAMIC_KEY_HYSTERESIS / 2;
    DynamicKey* dynamic_key = &g_dynamic_keys[0];
    dynamic_key->dks.type = DYNAMIC_KEY_STROKE;
    dynamic_key->dks.key_binding[0] = KEY_A;
    dynamic_key->dks.key_control[0] = DKS_KEY_CONTROL(DKS_HOLD, DKS_HOLD, DKS_RELEASE, DKS_RELEASE);
    dynamic_key->dks.press_begin_distance = threshold;
    dynamic_key->dks.press_fully_distance = threshold;
    dynamic_key->dks.release_begin_distance = threshold;
    dynamic_key->dks.release_fully_distance = threshold;
    dynamic_key->dks.key_id = 0;

    keyboard_advanced_key_update(&g_keyboard_advanced_keys[0], threshold - half_hysteresis);
    dynamic_key_process();
    expect_dynamic_key_buffer(KEY_NO_EVENT);

    keyboard_advanced_key_update(&g_keyboard_advanced_keys[0], threshold + half_hysteresis);
    dynamic_key_process();
    expect_dynamic_key_buffer(KEY_NO_EVENT);

    keyboard_advanced_key_update(&g_keyboard_advanced_keys[0], threshold - half_hysteresis);
    dynamic_key_process();
    expect_dynamic_key_buffer(KEY_NO_EVENT);

    keyboard_advanced_key_update(&g_keyboard_advanced_keys[0], threshold + DYNAMIC_KEY_HYSTERESIS + 1);
    dynamic_key_process();
    expect_dynamic_key_buffer(KEY_A);

    keyboard_advanced_key_update(&g_keyboard_advanced_keys[0], threshold - half_hysteresis);
    dynamic_key_process();
    expect_dynamic_key_buffer(KEY_A);

    keyboard_advanced_key_update(&g_keyboard_advanced_keys[0], threshold - DYNAMIC_KEY_HYSTERESIS - 1);
    dynamic_key_process();
    expect_dynamic_key_buffer(KEY_NO_EVENT);
}

TEST(DynamicKey, MutexDistancePriority)
{
    g_keymap[0][0] = DYNAMIC_KEY | (0 << 8);
    g_keymap[0][1] = DYNAMIC_KEY | (0 << 8);
    g_keymap_cache[0] = g_keymap[0][0];
    g_keymap_cache[1] = g_keymap[0][1];
    DynamicKey*dynamic_key = &g_dynamic_keys[0];
    dynamic_key->m.type = DYNAMIC_KEY_MUTEX;
    dynamic_key->m.mode = DK_MUTEX_DISTANCE_PRIORITY;
    dynamic_key->m.key_binding[0] = KEY_A;
    dynamic_key->m.key_binding[1] = KEY_B;
    dynamic_key->m.key_id[0] = 0;
    dynamic_key->m.key_id[1] = 1;

    keyboard_advanced_key_update(&g_keyboard_advanced_keys[0],A_ANTI_NORM(1));
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[1],A_ANTI_NORM(1));
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[0],A_ANTI_NORM(0));
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[1],A_ANTI_NORM(0));
    dynamic_key_process();

    keyboard_advanced_key_update(&g_keyboard_advanced_keys[0],A_ANTI_NORM(0.3));
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[1],A_ANTI_NORM(0));
    dynamic_key_process();
    keyboard_clear_buffer();
    dynamic_key_add_buffer();
    keyboard_buffer_send();
    EXPECT_EQ(keyboard_send_buffer[2], KEY_A);
    EXPECT_EQ(keyboard_send_buffer[3], KEY_NO_EVENT);


    keyboard_advanced_key_update(&g_keyboard_advanced_keys[0],A_ANTI_NORM(0.3));
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[1],A_ANTI_NORM(0.9));
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[0],A_ANTI_NORM(0.3));
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[1],A_ANTI_NORM(0.9));
    dynamic_key_process();
    keyboard_clear_buffer();
    dynamic_key_add_buffer();
    keyboard_buffer_send();
    EXPECT_EQ(keyboard_send_buffer[2], KEY_B);
    EXPECT_EQ(keyboard_send_buffer[3], KEY_NO_EVENT);

    keyboard_advanced_key_update(&g_keyboard_advanced_keys[0],A_ANTI_NORM(1));
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[1],A_ANTI_NORM(0.9));
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[0],A_ANTI_NORM(1));
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[1],A_ANTI_NORM(0.9));
    dynamic_key_process();
    keyboard_clear_buffer();
    dynamic_key_add_buffer();
    keyboard_buffer_send();
    EXPECT_EQ(keyboard_send_buffer[2], KEY_A);
    EXPECT_EQ(keyboard_send_buffer[3], KEY_NO_EVENT);
    
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[0],A_ANTI_NORM(0));
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[1],A_ANTI_NORM(0));
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[0],A_ANTI_NORM(1));
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[1],A_ANTI_NORM(0.9));
    dynamic_key_process();
    keyboard_clear_buffer();
    dynamic_key_add_buffer();
    keyboard_buffer_send();
    EXPECT_EQ(keyboard_send_buffer[2], KEY_A);
    EXPECT_EQ(keyboard_send_buffer[3], KEY_NO_EVENT);
    EXPECT_EQ(keyboard_send_buffer[4], KEY_NO_EVENT);
    EXPECT_EQ(keyboard_send_buffer[5], KEY_NO_EVENT);
    EXPECT_EQ(keyboard_send_buffer[6], KEY_NO_EVENT);

    keyboard_advanced_key_update(&g_keyboard_advanced_keys[0],A_ANTI_NORM(0));
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[1],A_ANTI_NORM(0));
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[0],A_ANTI_NORM(0));
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[1],A_ANTI_NORM(0));
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
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[0],A_ANTI_NORM(0));
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[1],A_ANTI_NORM(0));
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[0],A_ANTI_NORM(1));
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[1],A_ANTI_NORM(0.9));
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

TEST(DynamicKey, MutexDistancePriorityHysteresisFiltersLosingKeyNoise)
{
    bind_dynamic_key(0);
    bind_dynamic_key(1);
    const AnalogValue base = A_ANTI_NORM(0.4f);
    const AnalogValue half_hysteresis = DYNAMIC_KEY_HYSTERESIS / 2;
    DynamicKey* dynamic_key = &g_dynamic_keys[0];
    dynamic_key->m.type = DYNAMIC_KEY_MUTEX;
    dynamic_key->m.mode = DK_MUTEX_DISTANCE_PRIORITY;
    dynamic_key->m.key_binding[0] = KEY_A;
    dynamic_key->m.key_binding[1] = KEY_B;
    dynamic_key->m.key_id[0] = 0;
    dynamic_key->m.key_id[1] = 1;

    keyboard_advanced_key_update(&g_keyboard_advanced_keys[0], base);
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[1], base - DYNAMIC_KEY_HYSTERESIS - 1);
    dynamic_key_process();
    expect_dynamic_key_buffer(KEY_A);

    keyboard_advanced_key_update(&g_keyboard_advanced_keys[0], base);
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[1], base - DYNAMIC_KEY_HYSTERESIS - 1 + half_hysteresis);
    dynamic_key_process();
    expect_dynamic_key_buffer(KEY_A);

    keyboard_advanced_key_update(&g_keyboard_advanced_keys[0], base);
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[1], base + DYNAMIC_KEY_HYSTERESIS + 1);
    dynamic_key_process();
    expect_dynamic_key_buffer(KEY_B);
}

TEST(DynamicKey, MutexSimultaneousDeadzoneHysteresisFiltersEdgeNoise)
{
    bind_dynamic_key(0);
    bind_dynamic_key(1);
    AdvancedKey* advanced_key0 = &g_keyboard_advanced_keys[0];
    AdvancedKey* advanced_key1 = &g_keyboard_advanced_keys[1];
    const AnalogValue key0_enter_threshold = ANALOG_VALUE_MAX - advanced_key0->config.lower_deadzone;
    const AnalogValue key1_enter_threshold = ANALOG_VALUE_MAX - advanced_key1->config.lower_deadzone;
    const AnalogValue half_hysteresis = DYNAMIC_KEY_HYSTERESIS / 2;
    DynamicKey* dynamic_key = &g_dynamic_keys[0];
    dynamic_key->m.type = DYNAMIC_KEY_MUTEX;
    dynamic_key->m.mode = DK_MUTEX_DISTANCE_PRIORITY | 0x80;
    dynamic_key->m.key_binding[0] = KEY_A;
    dynamic_key->m.key_binding[1] = KEY_B;
    dynamic_key->m.key_id[0] = 0;
    dynamic_key->m.key_id[1] = 1;

    keyboard_advanced_key_update(advanced_key0, key0_enter_threshold + DYNAMIC_KEY_HYSTERESIS + 1);
    keyboard_advanced_key_update(advanced_key1, key1_enter_threshold + DYNAMIC_KEY_HYSTERESIS + 1);
    dynamic_key_process();
    expect_dynamic_key_buffer(KEY_A, KEY_B);

    keyboard_advanced_key_update(advanced_key0, key0_enter_threshold - half_hysteresis);
    keyboard_advanced_key_update(advanced_key1, key1_enter_threshold - half_hysteresis);
    dynamic_key_process();
    expect_dynamic_key_buffer(KEY_A, KEY_B);

    keyboard_advanced_key_update(advanced_key0, ANALOG_VALUE_MIN);
    keyboard_advanced_key_update(advanced_key1, ANALOG_VALUE_MIN);
    dynamic_key_process();
    expect_dynamic_key_buffer(KEY_NO_EVENT);
    EXPECT_FALSE(dynamic_key->m.key_report_state[0]);
    EXPECT_FALSE(dynamic_key->m.key_report_state[1]);
}

TEST(DynamicKey, MutexLastPriority)
{
    g_keymap[0][0] = DYNAMIC_KEY | (0 << 8);
    g_keymap[0][1] = DYNAMIC_KEY | (0 << 8);
    g_keymap_cache[0] = g_keymap[0][0];
    g_keymap_cache[1] = g_keymap[0][1];
    DynamicKey*dynamic_key = &g_dynamic_keys[0];
    dynamic_key->m.type = DYNAMIC_KEY_MUTEX;
    dynamic_key->m.mode = DK_MUTEX_LAST_PRIORITY;
    dynamic_key->m.key_binding[0] = KEY_A;
    dynamic_key->m.key_binding[1] = KEY_B;
    dynamic_key->m.key_id[0] = 0;
    dynamic_key->m.key_id[1] = 1;

    keyboard_advanced_key_update(&g_keyboard_advanced_keys[0],A_ANTI_NORM(1));
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[1],A_ANTI_NORM(1));
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[0],A_ANTI_NORM(0));
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[1],A_ANTI_NORM(0));
    dynamic_key_process();

    keyboard_advanced_key_update(&g_keyboard_advanced_keys[0],A_ANTI_NORM(0.6));
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[1],A_ANTI_NORM(0));
    dynamic_key_process();
    EXPECT_TRUE(g_keyboard_advanced_keys[0].key.state);
    EXPECT_FALSE(g_keyboard_advanced_keys[1].key.state);

    keyboard_advanced_key_update(&g_keyboard_advanced_keys[0],A_ANTI_NORM(0.6));
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[1],A_ANTI_NORM(0.6));
    dynamic_key_process();
    EXPECT_TRUE(g_keyboard_advanced_keys[0].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[1].key.state);

    keyboard_advanced_key_update(&g_keyboard_advanced_keys[0],A_ANTI_NORM(0.3));
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[1],A_ANTI_NORM(0.6));
    dynamic_key_process();
    EXPECT_FALSE(g_keyboard_advanced_keys[0].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[1].key.state);

    keyboard_advanced_key_update(&g_keyboard_advanced_keys[0],A_ANTI_NORM(0.6));
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[1],A_ANTI_NORM(0.6));
    dynamic_key_process();
    EXPECT_TRUE(g_keyboard_advanced_keys[0].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[1].key.state);

    keyboard_advanced_key_update(&g_keyboard_advanced_keys[0],A_ANTI_NORM(0.9));
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[1],A_ANTI_NORM(0.9));
    dynamic_key_process();
    EXPECT_TRUE(g_keyboard_advanced_keys[0].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[1].key.state);

    dynamic_key->m.mode |= 0x80;
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[0],A_ANTI_NORM(0));
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[1],A_ANTI_NORM(0));
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[0],A_ANTI_NORM(0.9));
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[1],A_ANTI_NORM(0.9));
    dynamic_key_process();
    EXPECT_TRUE(g_keyboard_advanced_keys[0].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[1].key.state);
}

TEST(DynamicKey, MutexKey1Priority)
{
    g_keymap[0][0] = DYNAMIC_KEY | (0 << 8);
    g_keymap[0][1] = DYNAMIC_KEY | (0 << 8);
    g_keymap_cache[0] = g_keymap[0][0];
    g_keymap_cache[1] = g_keymap[0][1];
    DynamicKey*dynamic_key = &g_dynamic_keys[0];
    dynamic_key->m.type = DYNAMIC_KEY_MUTEX;
    dynamic_key->m.mode = DK_MUTEX_KEY1_PRIORITY;
    dynamic_key->m.key_binding[0] = KEY_A;
    dynamic_key->m.key_binding[1] = KEY_B;
    dynamic_key->m.key_id[0] = 0;
    dynamic_key->m.key_id[1] = 1;

    keyboard_advanced_key_update(&g_keyboard_advanced_keys[0],A_ANTI_NORM(1));
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[1],A_ANTI_NORM(1));
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[0],A_ANTI_NORM(0));
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[1],A_ANTI_NORM(0));
    dynamic_key_process();

    keyboard_advanced_key_update(&g_keyboard_advanced_keys[0],A_ANTI_NORM(0.6));
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[1],A_ANTI_NORM(0));
    dynamic_key_process();
    EXPECT_TRUE(g_keyboard_advanced_keys[0].key.state);
    EXPECT_FALSE(g_keyboard_advanced_keys[1].key.state);

    keyboard_advanced_key_update(&g_keyboard_advanced_keys[0],A_ANTI_NORM(0.6));
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[1],A_ANTI_NORM(0.6));
    dynamic_key_process();
    EXPECT_TRUE(g_keyboard_advanced_keys[0].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[1].key.state);

    keyboard_advanced_key_update(&g_keyboard_advanced_keys[0],A_ANTI_NORM(0.3));
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[1],A_ANTI_NORM(0.6));
    dynamic_key_process();
    EXPECT_FALSE(g_keyboard_advanced_keys[0].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[1].key.state);

    keyboard_advanced_key_update(&g_keyboard_advanced_keys[0],A_ANTI_NORM(0.6));
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[1],A_ANTI_NORM(0.6));
    dynamic_key_process();
    EXPECT_TRUE(g_keyboard_advanced_keys[0].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[1].key.state);

    keyboard_advanced_key_update(&g_keyboard_advanced_keys[0],A_ANTI_NORM(0.9));
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[1],A_ANTI_NORM(0.9));
    dynamic_key_process();
    EXPECT_TRUE(g_keyboard_advanced_keys[0].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[1].key.state);

    dynamic_key->m.mode |= 0x80;
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[0],A_ANTI_NORM(0));
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[1],A_ANTI_NORM(0));
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[0],A_ANTI_NORM(0.9));
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[1],A_ANTI_NORM(0.9));
    dynamic_key_process();
    EXPECT_TRUE(g_keyboard_advanced_keys[0].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[1].key.state);
}

TEST(DynamicKey, MutexKey2Priority)
{
    g_keymap[0][0] = DYNAMIC_KEY | (0 << 8);
    g_keymap[0][1] = DYNAMIC_KEY | (0 << 8);
    g_keymap_cache[0] = g_keymap[0][0];
    g_keymap_cache[1] = g_keymap[0][1];
    DynamicKey*dynamic_key = &g_dynamic_keys[0];
    dynamic_key->m.type = DYNAMIC_KEY_MUTEX;
    dynamic_key->m.mode = DK_MUTEX_KEY2_PRIORITY;
    dynamic_key->m.key_binding[0] = KEY_A;
    dynamic_key->m.key_binding[1] = KEY_B;
    dynamic_key->m.key_id[0] = 0;
    dynamic_key->m.key_id[1] = 1;

    keyboard_advanced_key_update(&g_keyboard_advanced_keys[0],A_ANTI_NORM(1));
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[1],A_ANTI_NORM(1));
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[0],A_ANTI_NORM(0));
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[1],A_ANTI_NORM(0));
    dynamic_key_process();

    keyboard_advanced_key_update(&g_keyboard_advanced_keys[0],A_ANTI_NORM(0.6));
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[1],A_ANTI_NORM(0));
    dynamic_key_process();
    EXPECT_TRUE(g_keyboard_advanced_keys[0].key.state);
    EXPECT_FALSE(g_keyboard_advanced_keys[1].key.state);

    keyboard_advanced_key_update(&g_keyboard_advanced_keys[0],A_ANTI_NORM(0.6));
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[1],A_ANTI_NORM(0.6));
    dynamic_key_process();
    EXPECT_TRUE(g_keyboard_advanced_keys[0].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[1].key.state);

    keyboard_advanced_key_update(&g_keyboard_advanced_keys[0],A_ANTI_NORM(0.3));
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[1],A_ANTI_NORM(0.6));
    dynamic_key_process();
    EXPECT_FALSE(g_keyboard_advanced_keys[0].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[1].key.state);

    keyboard_advanced_key_update(&g_keyboard_advanced_keys[0],A_ANTI_NORM(0.6));
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[1],A_ANTI_NORM(0.6));
    dynamic_key_process();
    EXPECT_TRUE(g_keyboard_advanced_keys[0].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[1].key.state);

    keyboard_advanced_key_update(&g_keyboard_advanced_keys[0],A_ANTI_NORM(0.9));
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[1],A_ANTI_NORM(0.9));
    dynamic_key_process();
    EXPECT_TRUE(g_keyboard_advanced_keys[0].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[1].key.state);

    dynamic_key->m.mode |= 0x80;
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[0],A_ANTI_NORM(0));
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[1],A_ANTI_NORM(0));
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[0],A_ANTI_NORM(0.9));
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[1],A_ANTI_NORM(0.9));
    dynamic_key_process();
    EXPECT_TRUE(g_keyboard_advanced_keys[0].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[1].key.state);
}


TEST(DynamicKey, MutexKeyNeural)
{
    g_keymap[0][0] = DYNAMIC_KEY | (0 << 8);
    g_keymap[0][1] = DYNAMIC_KEY | (0 << 8);
    g_keymap_cache[0] = g_keymap[0][0];
    g_keymap_cache[1] = g_keymap[0][1];
    DynamicKey*dynamic_key = &g_dynamic_keys[0];
    dynamic_key->m.type = DYNAMIC_KEY_MUTEX;
    dynamic_key->m.mode = DK_MUTEX_NEUTRAL;
    dynamic_key->m.key_binding[0] = KEY_A;
    dynamic_key->m.key_binding[1] = KEY_B;
    dynamic_key->m.key_id[0] = 0;
    dynamic_key->m.key_id[1] = 1;

    keyboard_advanced_key_update(&g_keyboard_advanced_keys[0],A_ANTI_NORM(1));
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[1],A_ANTI_NORM(1));
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[0],A_ANTI_NORM(0));
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[1],A_ANTI_NORM(0));
    dynamic_key_process();

    keyboard_advanced_key_update(&g_keyboard_advanced_keys[0],A_ANTI_NORM(0.6));
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[1],A_ANTI_NORM(0));
    dynamic_key_process();
    EXPECT_TRUE(g_keyboard_advanced_keys[0].key.state);
    EXPECT_FALSE(g_keyboard_advanced_keys[1].key.state);

    keyboard_advanced_key_update(&g_keyboard_advanced_keys[0],A_ANTI_NORM(0.6));
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[1],A_ANTI_NORM(0.6));
    dynamic_key_process();
    EXPECT_TRUE(g_keyboard_advanced_keys[0].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[1].key.state);

    keyboard_advanced_key_update(&g_keyboard_advanced_keys[0],A_ANTI_NORM(0.3));
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[1],A_ANTI_NORM(0.6));
    dynamic_key_process();
    EXPECT_FALSE(g_keyboard_advanced_keys[0].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[1].key.state);

    keyboard_advanced_key_update(&g_keyboard_advanced_keys[0],A_ANTI_NORM(0.6));
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[1],A_ANTI_NORM(0.6));
    dynamic_key_process();
    EXPECT_TRUE(g_keyboard_advanced_keys[0].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[1].key.state);

    keyboard_advanced_key_update(&g_keyboard_advanced_keys[0],A_ANTI_NORM(0.9));
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[1],A_ANTI_NORM(0.9));
    dynamic_key_process();
    EXPECT_TRUE(g_keyboard_advanced_keys[0].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[1].key.state);

    dynamic_key->m.mode |= 0x80;
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[0],A_ANTI_NORM(0));
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[1],A_ANTI_NORM(0));
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[0],A_ANTI_NORM(0.9));
    keyboard_advanced_key_update(&g_keyboard_advanced_keys[1],A_ANTI_NORM(0.9));
    dynamic_key_process();
    EXPECT_TRUE(g_keyboard_advanced_keys[0].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[1].key.state);
    EXPECT_TRUE(g_keyboard_advanced_keys[0].key.report_state);
    EXPECT_TRUE(g_keyboard_advanced_keys[1].key.report_state);
}
