/* Differential trace for check_pipeline.py. No host fixture or board needed. */
#include <stdio.h>
#include <string.h>
#include "analog.h"
#if LIBAMP_CHECK_INLINE_PIPELINE
#include "advanced_key.h"
#endif

AdvancedKey g_keyboard_advanced_keys[ADVANCED_KEY_NUM];
const uint16_t g_analog_map[ADVANCED_KEY_NUM] = {7, 2, 5, 0, 6, 1, 4, 3};
static uint64_t digest = UINT64_C(14695981039346656037);
static uint32_t normalize_calls;

static void record(uint32_t value)
{
    digest = (digest ^ value) * UINT64_C(1099511628211);
}

void filter_reset(void) {}

#ifdef CHECK_CUSTOM_NORMALIZER
AnalogValue advanced_key_normalize(AdvancedKey *key, AnalogRawValue raw)
{
    record(key->raw);
    record(key->filtered_raw);
    record(key->config.mode);
    // Overrides may change configuration; the update must see the new mode.
    if ((++normalize_calls & 127u) == 0)
        key->config.mode = (key->config.mode + 1u) % 4u;
    const uint32_t index = advanced_key_lut_index(key, raw);
    return (AnalogValue)((uint64_t)index * index * ANALOG_VALUE_RANGE /
                         ((uint64_t)LUT_LENGTH * LUT_LENGTH)) + ANALOG_VALUE_MIN;
}
#endif

#ifdef KEY_CALLBACK_ENABLE
static void key_callback(void *arg)
{
    AdvancedKey *key = arg;
    record(key->key.state);
    record(key->value);
    record(key->difference);
    record(key->extremum);
    key->config.trigger_distance ^= 1u;
}
#endif

int main(void)
{
    for (unsigned i = 0; i < ADVANCED_KEY_NUM; i++) {
        AdvancedKey *key = &g_keyboard_advanced_keys[i];
        advanced_key_init(key, i);
        key->config.activation_value = 30000;
        key->config.deactivation_value = 28000;
        key->config.trigger_distance = 1500;
        key->config.release_distance = 1000;
        key->config.trigger_speed = 900;
        key->config.release_speed = 700;
        advanced_key_set_deadzone(key, 400, 900);
        advanced_key_set_range(key, i & 1 ? 10000 : 55000, i & 1 ? 50000 : 5000);
#if FILTER_TYPE == FILTER_TYPE_KALMAN
        kalman_filter_init(&g_analog_filters[i], 0.001f, 10.0f, 500.0f, 1.0f);
#else
        lowpass_filter_init(&g_analog_filters[i], 0);
#endif
#ifdef FILTER_HYSTERESIS_ENABLE
        hysteresis_filter_init(&g_analog_hysteresis_filters[i], 0);
#endif
#ifdef KEY_CALLBACK_ENABLE
        key_attach(&key->key, KEY_EVENT_DOWN, key_callback);
        key_attach(&key->key, KEY_EVENT_UP, key_callback);
#endif
    }
    uint32_t random = 0x917a38db;
    for (unsigned tick = 0; tick < 32768; tick++) {
        for (unsigned id = 0; id < ADVANCED_KEY_NUM; id++) {
            AdvancedKey *key = &g_keyboard_advanced_keys[id];
            random = random * 1664525u + 1013904223u;
            const AnalogRawValue sample = tick % 32 == 0 ? 0 : tick % 32 == 1 ? UINT16_MAX : random >> 16;
            ringbuf_push(&g_adc_ringbufs[g_analog_map[id]], sample);
            const AnalogRawValue raw = advanced_key_read_raw(key);
            if ((tick & 63u) == 0) {
                key->config.mode = (id + tick / 64) % 5;
                key->config.calibration_mode = (id + tick / 256) % 5;
                advanced_key_set_range(key, sample, (AnalogRawValue)(random >> 8));
            }
            bool changed;
#if LIBAMP_CHECK_INLINE_PIPELINE
            if (tick & 1u)
                changed = advanced_key_update_raw(key, raw);
            else
#endif
                changed = advanced_key_update_raw(key, raw);
            record(changed);
            record(key->key.state);
            record(key->raw);
            record(key->filtered_raw);
            record(key->value);
            record(key->difference);
            record(key->extremum);
            record(key->q_scale_to_index);
            record(key->config.mode);
            record(key->config.calibration_mode);
            record(key->config.upper_bound);
            record(key->config.lower_bound);
            record(advanced_key_get_effective_value(key));
            const bool debounced = keyboard_key_debounce(&key->key);
            record(debounced);
            key->key.report_state = debounced;
#if DEBOUNCE_PRESS > 0 || DEBOUNCE_RELEASE > 0
            record(key->key.debounce);
#endif
        }
    }
    record(normalize_calls);
    printf("%016llx\n", (unsigned long long)digest);
    return 0;
}
