/*
 * Host micro-benchmark for the libamp hot paths. Not a test: it links the same
 * fixture as the gtest binary and prints the average cost of one keyboard_task()
 * tick and one rgb_process() frame under a few representative loads.
 *
 *   ./build/tests/test/libamp_bench [ticks] [frames]
 */
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "analog.h"
#include "keyboard.h"
#include "layer.h"
#include "nexus.h"
#include "rgb.h"
#include "test_fixture.h"

/* nexus.c is linked as a bitmap-mode master; give it one slave carrying the
 * first sixteen keys so the master's per-tick key loop can be timed. */
static_assert(NEXUS_SLICE_LENGTH_MAX >= 16, "the bench slave maps sixteen keys");
extern "C" {
const uint16_t bench_slave_map[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
NexusSlaveKeymap g_nexus_slave_configs[NEXUS_SLAVE_NUM] = {
    {16, bench_slave_map},
};
}

namespace {

using Clock = std::chrono::steady_clock;

double ns_per_call(void (*fn)(void), long iterations)
{
    for (long i = 0; i < iterations / 4; i++)
    {
        fn(); /* warm up caches, predictors and the CPU clock before timing */
    }
    const auto start = Clock::now();
    for (long i = 0; i < iterations; i++)
    {
        fn();
    }
    const auto end = Clock::now();
    return std::chrono::duration<double, std::nano>(end - start).count() / (double)iterations;
}

void calibrate_all_keys(void)
{
    for (int i = 0; i < ADVANCED_KEY_NUM; i++)
    {
        /* The fixture's full keymap contains layer/profile/reset keys. A
         * simultaneous press must not reconfigure the workload being timed. */
        g_keymap[0][i] = KEY_A + i % 26;
        g_keyboard_advanced_keys[i].config.calibration_mode = ADVANCED_KEY_AUTO_CALIBRATION_UNDEFINED;
        advanced_key_reset_range(&g_keyboard_advanced_keys[i], 2048);
    }
    layer_cache_refresh();
}

void feed_raw(uint16_t raw)
{
    for (int i = 0; i < ANALOG_BUFFER_LENGTH; i++)
    {
        ringbuf_push(&g_adc_ringbufs[i], raw);
    }
}

/* Every key rests at the top of its travel: the common idle case. */
void tick_idle(void)
{
    feed_raw(2048);
    keyboard_tick_inc(1);
    keyboard_task();
}

/* Every key travels through a slow sine wave, so keys press and release and
 * report building sees pressed keys most of the time. */
void tick_travel(void)
{
    feed_raw((uint16_t)((std::cos(keyboard_tick_get32() / 100.0f) + 1.0f) * 1024.0f));
    keyboard_tick_inc(1);
    keyboard_task();
}

/* Range updates run in the input tick whenever auto-calibration sees a new
 * extremum. Measure them separately from already-calibrated travel. */
void update_ranges(void)
{
    const uint16_t span = (keyboard_tick_get32() & 2047u) + 1;
    keyboard_tick_inc(1);
    for (int i = 0; i < ADVANCED_KEY_NUM; i++)
    {
        advanced_key_set_range(&g_keyboard_advanced_keys[i], 2048, 2048 - span);
    }
}

void convert_hues(void)
{
    for (uint16_t hue = 0; hue < 360; hue++)
    {
        const ColorHSV hsv = {hue, 78, 99};
        hsv_to_rgb(&led_color_buffer[hue % RGB_NUM], &hsv);
    }
}

uint32_t slave_bitmap;

/* One report frame from slave 0, as its transport interrupt would deliver it. */
void deliver_slave_frame(uint32_t bitmap)
{
    PacketNexus packet;
    const volatile uint32_t bits[NEXUS_BITMAP_WORDS] = {bitmap};
    nexus_report_encode(&packet, 0, 2048, 0, bits, 16);
    nexus_process_buffer(0, reinterpret_cast<uint8_t *>(&packet), sizeof(packet));
}

/* Sixteen resting slave keys: the usual master load. The slave's frames are
 * delivered only often enough to keep the link alive, so this times the
 * master's per-tick key loop itself. */
void nexus_tick_idle(void)
{
    if ((keyboard_tick_get32() & 63u) == 0u)
    {
        deliver_slave_frame(0);
    }
    keyboard_tick_inc(1);
    nexus_process();
}

/* Every slave key changes state on every tick: the worst case for the loop. */
void nexus_tick_toggle(void)
{
    slave_bitmap ^= 0xFFFFu;
    deliver_slave_frame(slave_bitmap);
    keyboard_tick_inc(1);
    nexus_process();
}

void frame(void)
{
    keyboard_tick_inc(1);
    rgb_process();
}

/* Keep all eight ripples alive for the whole measurement. Cycling a bounded
 * effect age avoids timing mostly empty frames after RGB_MAX_DURATION. */
uint32_t ripple_tick;
void ripple_frame(void)
{
    libamp_test_set_tick(ripple_tick + 1 + ((keyboard_tick_get32() - ripple_tick) % 128));
    rgb_process();
}

/* Released trigger keys at a representative point in their fade, rather than
 * an already black effect after millions of keyboard ticks. */
void trigger_frame(void)
{
    keyboard_tick_inc(1);
    for (int i = 0; i < RGB_NUM; i++)
    {
        g_rgb_configs[i].begin_tick = keyboard_tick_get32() - KEYBOARD_TIME_TO_TICK(100);
    }
    rgb_process();
}

void set_per_key_mode(RGBMode mode)
{
    for (int i = 0; i < RGB_NUM; i++)
    {
        g_rgb_configs[i].mode = mode;
        g_rgb_configs[i].rgb = {200, 100, 50};
        g_rgb_configs[i].speed = 20;
    }
}

} // namespace

int main(int argc, char **argv)
{
    const long ticks = argc > 1 ? std::atol(argv[1]) : 200000;
    const long frames = argc > 2 ? std::atol(argv[2]) : 5000;
    if (ticks <= 0 || frames <= 0)
    {
        std::fprintf(stderr, "usage: %s [positive ticks] [positive frames]\n", argv[0]);
        return EXIT_FAILURE;
    }

    libamp_test_reset_environment();
    calibrate_all_keys();
    for (int i = 0; i < 2000; i++)
    {
        tick_travel(); /* let auto-calibration settle on the sine range */
    }
    if (g_keyboard_advanced_keys[0].value == ANALOG_VALUE_MIN)
    {
        std::fprintf(stderr, "benchmark did not scan the travelling analog keys\n");
        return EXIT_FAILURE;
    }

    std::printf("keyboard_task, idle keys      : %8.1f ns/tick\n", ns_per_call(tick_idle, ticks));
    std::printf("keyboard_task, travelling keys: %8.1f ns/tick\n", ns_per_call(tick_travel, ticks));
    std::printf("advanced_key_set_range        : %8.3f ns/key\n", ns_per_call(update_ranges, ticks) / ADVANCED_KEY_NUM);
    std::printf("hsv_to_rgb, saturated colors  : %8.3f ns/color\n", ns_per_call(convert_hues, frames) / 360);

    nexus_init();
    std::printf("nexus_process, 16 idle slave keys   : %8.1f ns/tick\n", ns_per_call(nexus_tick_idle, ticks));
    std::printf("nexus_process, 16 toggling slave keys: %8.1f ns/tick\n", ns_per_call(nexus_tick_toggle, ticks));

    g_rgb_base_config.mode = RGB_BASE_MODE_RAINBOW;
    rgb_set_brightness(200);
    set_per_key_mode(RGB_MODE_LINEAR);
    std::printf("rgb_process, rainbow + linear  : %8.1f ns/frame\n", ns_per_call(frame, frames));

    g_rgb_base_config.mode = RGB_BASE_MODE_WAVE;
    set_per_key_mode(RGB_MODE_TRIGGER);
    for (int i = 0; i < ADVANCED_KEY_NUM; i++)
    {
        g_keyboard_advanced_keys[i].key.report_state = false;
    }
    std::printf("rgb_process, wave + fading trigger: %8.1f ns/frame\n", ns_per_call(trigger_frame, frames));
    for (int i = 0; i < ADVANCED_KEY_NUM; i++)
    {
        g_keyboard_advanced_keys[i].key.report_state = true;
    }
    std::printf("rgb_process, wave + held trigger  : %8.1f ns/frame\n", ns_per_call(frame, frames));

    rgb_init(); /* discard effects left by the keyboard workload */
    g_rgb_base_config.mode = RGB_BASE_MODE_BLANK;
    set_per_key_mode(RGB_MODE_BUBBLE);
    ripple_tick = keyboard_tick_get32();
    for (int i = 0; i < 8; i++)
    {
        rgb_activate(i * 7, keyboard_tick_get32()); /* eight live ripples */
    }
    std::printf("rgb_process, 8 live bubble ripples: %8.1f ns/frame\n", ns_per_call(ripple_frame, frames));
    std::printf("(sanity: %u led flushes, led[0] = %u,%u,%u, key[0] value = %u)\n",
                (unsigned)led_flush_count, led_color_buffer[0].r, led_color_buffer[0].g, led_color_buffer[0].b,
                (unsigned)g_keyboard_advanced_keys[0].value);
    return 0;
}
