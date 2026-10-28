/*
 * Copyright (c) 2024 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "rgb.h"
#include "string.h"
#include "math.h"
#include "stdlib.h"
#include "driver.h"

/* Distances between LED locations, in micrometers. */
#define MANHATTAN_DISTANCE(m, n) (abs((m)->x - (n)->x) + abs((m)->y - (n)->y))
#define MANHATTAN_DISTANCE_DIRECT(x1, y1, x2, y2) (abs((x1) - (x2)) + abs((y1) - (y2)))
#define EUCLIDEAN_DISTANCE(m, n) sqrtf(((float)(m)->x - (float)(n)->x) * ((float)(m)->x - (float)(n)->x) + \
                                       ((float)(m)->y - (float)(n)->y) * ((float)(m)->y - (float)(n)->y))

/* Elapsed distance of a time-driven effect: milliseconds times speed. */
#define CALC_SPAN(tick, speed) ((float)(KEYBOARD_TICK_TO_TIME(tick)) * (float)(speed))

/* Hue angles are tracked in thousandths of a degree before being wrapped. */
#define HUE_CYCLE_MILLIDEGREES 360000u
/* 2^32 mod HUE_CYCLE_MILLIDEGREES: folds a product that overflowed 32 bits
 * back into one word without a 64-bit division. */
#define HUE_CYCLE_FOLD_2P32 167296u
_Static_assert((UINT64_C(1) << 32) % HUE_CYCLE_MILLIDEGREES == HUE_CYCLE_FOLD_2P32,
               "HUE_CYCLE_FOLD_2P32 must be 2^32 mod HUE_CYCLE_MILLIDEGREES");

/* Trigger mode fades by 0.9999 per unit of span; expf(x * ln 0.9999) is the
 * same curve as powf(0.9999, x) without the general-purpose pow overhead. */
#define RGB_TRIGGER_DECAY_LN (-1.0000500033e-4f)
/* Once the exponent is below this, expf() returns under 0.0037 and
 * rgb_scale() truncates every channel to zero (255 * 0.0037 < 1), so mixing
 * the color in changes nothing and the key is skipped without calling expf().
 * ln(1/255) is -5.54; -5.6 keeps a margin for rounding. */
#define RGB_TRIGGER_DECAY_SILENT (-5.6f)

#ifndef RGB_CUSTOM_INVERSE_MAPPING
uint16_t g_rgb_inverse_mapping[TOTAL_KEY_NUM];
#else
__WEAK const uint16_t g_rgb_inverse_mapping[TOTAL_KEY_NUM];
#endif
__WEAK const uint16_t g_rgb_mapping[RGB_NUM];
__WEAK const RGBLocation g_rgb_locations[RGB_NUM];

volatile bool g_rgb_hid_mode;
RGBBaseConfig g_rgb_base_config;
RGBConfig g_rgb_configs[RGB_NUM];
ColorRGB g_rgb_colors[RGB_NUM];

/* Ripple effects in flight, one node per activated key. */
static RGBArgumentList rgb_argument_list;
static RGBArgumentListNode rgb_argument_list_buffer[RGB_ARGUMENT_LIST_BUFFER_LENGTH];

/* Frame pacing: every input of the renderer (tick, key state, effect start
 * times) changes in keyboard_task(), so one frame per tick is the most that
 * can ever reach the LEDs, and RGB_MAX_FRAME_RATE may space frames further
 * apart. keyboard_process() may call rgb_process() far more often than that. */
static uint32_t rgb_frame_tick;
static bool rgb_frame_rendered;

#if defined(RGB_GAMMA_ENABLE) && defined(RGB_GAMMA_LUT_ENABLE)
/* Gamma curve and global brightness folded into one byte-to-byte table, so a
 * frame costs three lookups per LED instead of three powf() calls. Rebuilt
 * by rgb_set_brightness(). */
static uint8_t rgb_output_lut[256];

static void rgb_output_lut_rebuild(uint8_t brightness)
{
    const float brightness_scale = brightness / 255.0f;
    for (int value = 0; value < 256; value++)
    {
        rgb_output_lut[value] = (uint8_t)(GAMMA_CORRECT(value, 255) * brightness_scale + 0.5f);
    }
}
#endif

void rgb_init(void)
{
    rgb_forward_list_init(&rgb_argument_list, rgb_argument_list_buffer, RGB_ARGUMENT_LIST_BUFFER_LENGTH);
#ifndef RGB_CUSTOM_INVERSE_MAPPING
    /* Keys without an LED must read as unmapped, not as LED 0. */
    memset(g_rgb_inverse_mapping, 0xFF, sizeof(g_rgb_inverse_mapping));
    for (uint16_t i = 0; i < RGB_NUM; i++)
    {
        if (g_rgb_mapping[i] < TOTAL_KEY_NUM)
        {
            g_rgb_inverse_mapping[g_rgb_mapping[i]] = i;
        }
    }
#endif
    rgb_frame_rendered = false;
    rgb_set_brightness(g_rgb_base_config.brightness);
}

/* ------------------------------------------------------------------------- */
/* Base layer: whole-board effects                                           */
/* ------------------------------------------------------------------------- */

/* Time offset of the base effects in degrees: (elapsed ms * speed) mod 360,
 * using the full boot clock. The milliseconds are reduced first, and the remaining
 * product, below 2^34, is folded at 2^32 with the precomputed residue. */
static float rgb_base_time_offset(void)
{
    const uint32_t milliseconds = (uint32_t)(KEYBOARD_TICK_TO_TIME64(keyboard_tick_get64()) % HUE_CYCLE_MILLIDEGREES);
    const int32_t speed = g_rgb_base_config.speed;
    const uint64_t product = (uint64_t)milliseconds * (uint32_t)(speed < 0 ? -speed : speed);
    uint32_t wrapped = ((uint32_t)(product >> 32) * HUE_CYCLE_FOLD_2P32 + (uint32_t)product % HUE_CYCLE_MILLIDEGREES)
                       % HUE_CYCLE_MILLIDEGREES;
    if (speed < 0 && wrapped != 0)
    {
        wrapped = HUE_CYCLE_MILLIDEGREES - wrapped;
    }
    return (float)wrapped * 0.001f;
}

/* Wrap an angle in degrees into [0, 360) without fmodf: a truncating
 * conversion stands in for the division, two compares fix up the edges. */
static inline float rgb_wrap_degrees(float degrees)
{
    float wrapped = degrees - 360.0f * (float)(int32_t)(degrees * (1.0f / 360.0f));
    if (wrapped < 0.0f)
    {
        wrapped += 360.0f;
    }
    else if (wrapped >= 360.0f)
    {
        wrapped -= 360.0f;
    }
    return wrapped;
}

/* Position of an LED along the effect direction, in key units. */
static inline float rgb_projection(const RGBLocation *location, float direction_cos, float direction_sin)
{
    return ((float)location->x * direction_cos + (float)location->y * direction_sin) * (1.0f / KEY_SWITCH_DISTANCE);
}

static void rgb_render_base(void)
{
#if !RGB_BASE_MODE_USE_RAINBOW && !RGB_BASE_MODE_USE_WAVE
    return; /* no base effect compiled in */
#else
    if (g_rgb_base_config.mode != RGB_BASE_MODE_RAINBOW && g_rgb_base_config.mode != RGB_BASE_MODE_WAVE)
    {
        return;
    }
    const float direction = (float)g_rgb_base_config.direction * ((float)M_PI / 180.0f);
    const float direction_sin = sinf(direction);
    const float direction_cos = cosf(direction);
    const float time_offset = rgb_base_time_offset();
    const float density = g_rgb_base_config.density;
    ColorRGB temp_rgb;

    switch (g_rgb_base_config.mode)
    {
#if RGB_BASE_MODE_USE_RAINBOW
    case RGB_BASE_MODE_RAINBOW:
    {
        ColorHSV hsv;
        memcpy(&hsv, &g_rgb_base_config.hsv, sizeof(hsv));
        const float base_hue = hsv.h;
        for (uint16_t i = 0; i < RGB_NUM; i++)
        {
            const float hue = base_hue + rgb_projection(&g_rgb_locations[i], direction_cos, direction_sin) * density + time_offset;
            hsv.h = (uint16_t)rgb_wrap_degrees(hue);
            color_set_hsv(&temp_rgb, &hsv);
            color_mix(&g_rgb_colors[i], &temp_rgb);
        }
        break;
    }
#endif
#if RGB_BASE_MODE_USE_WAVE
    case RGB_BASE_MODE_WAVE:
    {
        const ColorRGB primary = g_rgb_base_config.rgb;
        const ColorRGB secondary = g_rgb_base_config.secondary_rgb;
        for (uint16_t i = 0; i < RGB_NUM; i++)
        {
            /* Triangle wave between the two colors along the direction. */
            const float phase = rgb_projection(&g_rgb_locations[i], direction_cos, direction_sin) * density + time_offset;
            const float intensity = fabsf(rgb_wrap_degrees(phase) * (1.0f / 180.0f) - 1.0f);
            const float secondary_intensity = 1.0f - intensity;
            temp_rgb.r = (uint8_t)(intensity * (float)primary.r + secondary_intensity * (float)secondary.r);
            temp_rgb.g = (uint8_t)(intensity * (float)primary.g + secondary_intensity * (float)secondary.g);
            temp_rgb.b = (uint8_t)(intensity * (float)primary.b + secondary_intensity * (float)secondary.b);
            color_mix(&g_rgb_colors[i], &temp_rgb);
        }
        break;
    }
#endif
    default:
        break;
    }
#endif
}

/* ------------------------------------------------------------------------- */
/* Ripple layer: effects spreading from an activated key                      */
/* ------------------------------------------------------------------------- */

/* A ripple is done once its fading edge has passed all four board corners. */
static inline bool rgb_ripple_has_left_board(const RGBLocation *origin, float distance)
{
    const float edge = distance - FADING_DISTANCE_UM;
    return MANHATTAN_DISTANCE_DIRECT(origin->x, RGB_LEFT_UM, origin->y, RGB_TOP_UM) < edge &&
           MANHATTAN_DISTANCE_DIRECT(origin->x, RGB_LEFT_UM, origin->y, RGB_BOTTOM_UM) < edge &&
           MANHATTAN_DISTANCE_DIRECT(origin->x, RGB_RIGHT_UM, origin->y, RGB_TOP_UM) < edge &&
           MANHATTAN_DISTANCE_DIRECT(origin->x, RGB_RIGHT_UM, origin->y, RGB_BOTTOM_UM) < edge;
}

/* Fade from full at the wave front to zero FADING_DISTANCE behind it; LEDs
 * ahead of the front light up over the last key unit before the front. */
static inline float rgb_fading_edge(float front_offset)
{
    if (front_offset > 0.0f)
    {
        const float remaining = FADING_DISTANCE_UM - front_offset;
        return remaining > 0.0f ? remaining * (1.0f / FADING_DISTANCE_UM) : 0.0f;
    }
    const float ahead = UNIT_TO_UM(1.0f) + front_offset;
    return ahead > 0.0f ? ahead * (1.0f / UNIT_TO_UM(1.0f)) : 0.0f;
}

/* One key unit wide band centered on the wave front. */
static inline float rgb_band_edge(float front_offset)
{
    const float intensity = UNIT_TO_UM(1.0f) - fabsf(front_offset);
    return intensity > 0.0f ? intensity * (1.0f / UNIT_TO_UM(1.0f)) : 0.0f;
}

static inline bool rgb_same_row(const RGBLocation *a, const RGBLocation *b)
{
    return abs(a->y - b->y) < UNIT_TO_UM(0.5f);
}

/* Intensity (0..1) that a ripple of the given mode, started at origin and
 * having travelled distance, contributes to the LED at target. */
static float rgb_ripple_intensity(RGBMode mode, const RGBLocation *origin, const RGBLocation *target, float distance)
{
    switch (mode)
    {
#if RGB_MODE_USE_STRING
    case RGB_MODE_STRING:
        return rgb_same_row(origin, target) ? rgb_band_edge(distance - (float)abs(origin->x - target->x)) : 0.0f;
#endif
#if RGB_MODE_USE_FADING_STRING
    case RGB_MODE_FADING_STRING:
        return rgb_same_row(origin, target) ? rgb_fading_edge(distance - (float)abs(origin->x - target->x)) : 0.0f;
#endif
#if RGB_MODE_USE_DIAMOND_RIPPLE
    case RGB_MODE_DIAMOND_RIPPLE:
        return rgb_band_edge(distance - (float)MANHATTAN_DISTANCE(origin, target));
#endif
#if RGB_MODE_USE_FADING_DIAMOND_RIPPLE
    case RGB_MODE_FADING_DIAMOND_RIPPLE:
        return rgb_fading_edge(distance - (float)MANHATTAN_DISTANCE(origin, target));
#endif
#if RGB_MODE_USE_BUBBLE
    case RGB_MODE_BUBBLE:
    {
        const float euclidean_distance = EUCLIDEAN_DISTANCE(origin, target);
        if (euclidean_distance > BUBBLE_DISTANCE_UM)
        {
            return 0.0f;
        }
        return rgb_fading_edge(distance - euclidean_distance);
    }
#endif
    default:
        return 0.0f;
    }
}

static inline bool rgb_mode_is_ripple(RGBMode mode)
{
    switch (mode)
    {
    case RGB_MODE_STRING:
    case RGB_MODE_FADING_STRING:
    case RGB_MODE_DIAMOND_RIPPLE:
    case RGB_MODE_FADING_DIAMOND_RIPPLE:
    case RGB_MODE_BUBBLE:
        return true;
    default:
        return false;
    }
}

static void rgb_render_ripples(uint32_t tick)
{
    RGBArgumentList *list = &rgb_argument_list;
    for (int16_t *iterator_ptr = &list->data[list->head].next; *iterator_ptr >= 0;)
    {
        RGBArgumentListNode *node = &list->data[*iterator_ptr];
        const RGBArgument *ripple = &node->data;
        const RGBConfig *config = &g_rgb_configs[ripple->rgb_ptr];
        const RGBLocation *origin = &g_rgb_locations[ripple->rgb_ptr];
        const float distance = CALC_SPAN(tick_elapsed32(tick, ripple->begin_tick), config->speed);

        if (rgb_ripple_has_left_board(origin, distance))
        {
            /* Unlink the node and return it to the free list. */
            const int16_t free_node = *iterator_ptr;
            *iterator_ptr = node->next;
            node->next = list->free_node;
            list->free_node = free_node;
            continue;
        }

        if (rgb_mode_is_ripple(config->mode))
        {
            for (uint16_t j = 0; j < RGB_NUM; j++)
            {
                const float intensity = rgb_ripple_intensity(config->mode, origin, &g_rgb_locations[j], distance);
                if (intensity <= 0.0f)
                {
                    continue;
                }
                /* Ripples add at half intensity so overlapping waves do not saturate at once. */
                const ColorRGB temp_rgb = {
                    (uint8_t)((uint8_t)(intensity * (float)config->rgb.r) >> 1),
                    (uint8_t)((uint8_t)(intensity * (float)config->rgb.g) >> 1),
                    (uint8_t)((uint8_t)(intensity * (float)config->rgb.b) >> 1),
                };
                color_mix(&g_rgb_colors[j], &temp_rgb);
            }
        }
        iterator_ptr = &list->data[*iterator_ptr].next;
    }
}

/* ------------------------------------------------------------------------- */
/* Key layer: per-LED effects driven by the key the LED belongs to            */
/* ------------------------------------------------------------------------- */

static inline ColorRGB rgb_scale(const ColorRGB *color, float intensity)
{
    const ColorRGB scaled = {
        (uint8_t)(intensity * (float)color->r),
        (uint8_t)(intensity * (float)color->g),
        (uint8_t)(intensity * (float)color->b),
    };
    return scaled;
}

#if RGB_MODE_USE_JELLY
/* Spread a pressed key's travel to its neighbours, fading with distance. */
static void rgb_render_jelly(uint16_t index, float travel)
{
    if (travel <= 0.0f)
    {
        return;
    }
    const float reach = JELLY_DISTANCE_UM * travel;
    for (uint16_t j = 0; j < RGB_NUM; j++)
    {
        float intensity = reach - (float)MANHATTAN_DISTANCE(&g_rgb_locations[j], &g_rgb_locations[index]);
        if (intensity <= 0.0f)
        {
            continue;
        }
        intensity = intensity > UNIT_TO_UM(1) ? 1.0f : intensity * (1.0f / UNIT_TO_UM(1));
        const ColorRGB temp_rgb = {
            (uint8_t)((uint8_t)(intensity * (float)g_rgb_configs[j].rgb.r) >> 1),
            (uint8_t)((uint8_t)(intensity * (float)g_rgb_configs[j].rgb.g) >> 1),
            (uint8_t)((uint8_t)(intensity * (float)g_rgb_configs[j].rgb.b) >> 1),
        };
        color_mix(&g_rgb_colors[j], &temp_rgb);
    }
}
#endif

/* Travel of the key with dead zones removed, 0..1; 0 for an unmapped LED.
 * Only the modes that use it pay for it. */
static inline float rgb_key_travel(const Key *key)
{
    return key != NULL ? (float)keyboard_get_key_effective_analog_value((Key *)key) * (1.0f / ANALOG_VALUE_RANGE) : 0.0f;
}

static void rgb_render_keys(uint32_t tick)
{
    ColorRGB temp_rgb;
    UNUSED(tick);
#if RGB_MODE_USE_CYCLE
    uint32_t cycle_milliseconds = 0;
    bool cycle_milliseconds_ready = false;
#endif
    for (uint16_t i = 0; i < RGB_NUM; i++)
    {
        Color *target_color = &g_rgb_colors[i];
        RGBConfig *config = &g_rgb_configs[i];
        const Key *key = keyboard_get_key(g_rgb_mapping[i]);
        const bool report_state = key != NULL && key->report_state;
        UNUSED(report_state);   /* only some compiled-in modes use these */
        UNUSED(key);

        switch (config->mode)
        {
#if RGB_MODE_USE_LINEAR
        case RGB_MODE_LINEAR:
            temp_rgb = rgb_scale(&config->rgb, rgb_key_travel(key));
            color_mix(target_color, &temp_rgb);
            break;
#endif
#if RGB_MODE_USE_TRIGGER
        case RGB_MODE_TRIGGER:
        {
            if (report_state)
            {
                config->begin_tick = tick;
                color_mix(target_color, &config->rgb);
                break;
            }
            const uint32_t elapsed_ticks = tick_elapsed32(tick, config->begin_tick);
            const float exponent = CALC_SPAN(elapsed_ticks, config->speed) * RGB_TRIGGER_DECAY_LN;
            if (exponent < RGB_TRIGGER_DECAY_SILENT)
            {
                break;  /* faded to black: nothing to mix in */
            }
            temp_rgb = rgb_scale(&config->rgb, expf(exponent));
            color_mix(target_color, &temp_rgb);
            break;
        }
#endif
        case RGB_MODE_FIXED:
            color_set_rgb(target_color, &config->rgb);
            break;
#if RGB_MODE_USE_STATIC
        case RGB_MODE_STATIC:
            color_mix(target_color, &config->rgb);
            break;
#endif
#if RGB_MODE_USE_CYCLE
        case RGB_MODE_CYCLE:
        {
            if (!cycle_milliseconds_ready)
            {
                cycle_milliseconds = (uint32_t)(KEYBOARD_TICK_TO_TIME64(keyboard_tick_get64()) % 360u);
                cycle_milliseconds_ready = true;
            }
            const int32_t hue_offset = (int32_t)cycle_milliseconds * config->speed;
            const int32_t hue = (int32_t)config->hsv.h + hue_offset % 360;
            const ColorHSV hsv = {
                (uint16_t)((hue % 360 + 360) % 360),
                config->hsv.s,
                config->hsv.v,
            };
            color_set_hsv(&temp_rgb, &hsv);
            color_mix(target_color, &temp_rgb);
            break;
        }
#endif
#if RGB_MODE_USE_JELLY
        case RGB_MODE_JELLY:
            rgb_render_jelly(i, rgb_key_travel(key));
            break;
#endif
        default:
            break;
        }
        UNUSED(temp_rgb);
    }
}

/* ------------------------------------------------------------------------- */
/* Frame                                                                      */
/* ------------------------------------------------------------------------- */

void rgb_process(void)
{
    const uint32_t tick = keyboard_tick_get32();
    if (rgb_frame_rendered && !rgb_frame_is_due(tick, rgb_frame_tick, RGB_FRAME_INTERVAL_TICKS))
    {
        return;
    }
    rgb_frame_tick = tick;
    rgb_frame_rendered = true;

    if (!g_rgb_base_config.mode
#ifdef SUSPEND_ENABLE
        || g_keyboard_is_suspend
#endif
    )
    {
        rgb_turn_off();
        return;
    }
    if (g_rgb_hid_mode)
    {
        /* The host owns the colors (HID LampArray); only push the frame out. */
        led_flush();
        return;
    }

    memset(g_rgb_colors, 0, sizeof(g_rgb_colors));
    rgb_render_base();
    rgb_render_ripples(tick);
    rgb_render_keys(tick);
    rgb_flush();
}

__WEAK void rgb_update_callback(void)
{

}

void rgb_set_brightness(uint8_t brightness)
{
    g_rgb_base_config.brightness = brightness;
#if defined(RGB_GAMMA_ENABLE) && defined(RGB_GAMMA_LUT_ENABLE)
    rgb_output_lut_rebuild(brightness);
#endif
}

void rgb_set(uint16_t index, uint8_t r, uint8_t g, uint8_t b)
{
#if defined(RGB_GAMMA_ENABLE) && defined(RGB_GAMMA_LUT_ENABLE)
    led_set(index, rgb_output_lut[r], rgb_output_lut[g], rgb_output_lut[b]);
#elif defined(RGB_GAMMA_ENABLE)
    const float brightness_scale = g_rgb_base_config.brightness / 255.0f;
    r = (uint8_t)(GAMMA_CORRECT(r, 255) * brightness_scale + 0.5f);
    g = (uint8_t)(GAMMA_CORRECT(g, 255) * brightness_scale + 0.5f);
    b = (uint8_t)(GAMMA_CORRECT(b, 255) * brightness_scale + 0.5f);
    led_set(index, r, g, b);
#else
    const uint8_t brightness = g_rgb_base_config.brightness;
    led_set(index, (uint8_t)((r * brightness) >> 8), (uint8_t)((g * brightness) >> 8), (uint8_t)((b * brightness) >> 8));
#endif
}

void rgb_init_flash(void)
{
    const RGBLocation location = PORT_LOCATION;
    const uint32_t begin_tick = keyboard_tick_get32();
    while (KEYBOARD_TICK_TO_TIME(tick_elapsed32(keyboard_tick_get32(), begin_tick)) < RGB_FLASH_MAX_DURATION)
    {
        const float distance = (float)KEYBOARD_TICK_TO_TIME(tick_elapsed32(keyboard_tick_get32(), begin_tick)) * RGB_FLASH_RIPPLE_SPEED;
        bool animation_playing = false;
        memset(g_rgb_colors, 0, sizeof(g_rgb_colors));
        for (uint16_t i = 0; i < RGB_NUM; i++)
        {
            float intensity = distance - EUCLIDEAN_DISTANCE(&location, &g_rgb_locations[i]);
            if (intensity > 0.0f)
            {
                intensity = UNIT_TO_UM(10) - intensity > 0.0f ? (UNIT_TO_UM(10) - intensity) * (1.0f / UNIT_TO_UM(10)) : 0.0f;
            }
            else
            {
                intensity = UNIT_TO_UM(1.0f) + intensity > 0.0f ? (UNIT_TO_UM(1.0f) + intensity) * (1.0f / UNIT_TO_UM(1.0f)) : 0.0f;
            }
            if (intensity > 0.0f || distance < UNIT_TO_UM(10))
            {
                animation_playing = true;
            }
            const uint8_t level = (uint8_t)(intensity * 255.0f);
            const ColorRGB temp_rgb = {level, level, level};
            color_mix(&g_rgb_colors[i], &temp_rgb);
        }
        if (!animation_playing)
        {
            break;
        }
        for (uint16_t i = 0; i < RGB_NUM; i++)
        {
            rgb_set(i, g_rgb_colors[i].r, g_rgb_colors[i].g, g_rgb_colors[i].b);
        }
        led_flush();
    }
    rgb_turn_off();
}

void rgb_flash(void)
{
    const uint32_t begin_tick = keyboard_tick_get32();
    while (tick_elapsed32(keyboard_tick_get32(), begin_tick) < RGB_FLASH_MAX_DURATION)
    {
        const float distance = (float)(tick_elapsed32(keyboard_tick_get32(), begin_tick));
        const float half_duration = RGB_FLASH_MAX_DURATION / 2;
        const float intensity = (half_duration - fabsf(distance - half_duration)) * (1.0f / half_duration);
        const uint8_t level = (uint8_t)(intensity * 255.0f);
        for (uint16_t i = 0; i < RGB_NUM; i++)
        {
            rgb_set(i, level, level, level);
        }
        led_flush();
    }
    rgb_turn_off();
}

void rgb_turn_off(void)
{
    for (uint16_t i = 0; i < RGB_NUM; i++)
    {
        rgb_set(i, 0, 0, 0);
    }
    led_flush();
}

void rgb_factory_reset(void)
{
    const ColorHSV default_hsv = RGB_DEFAULT_COLOR_HSV;
    g_rgb_base_config.mode = RGB_BASE_MODE_BLANK;
    rgb_set_brightness(255);
    g_rgb_base_config.density = 32;
    g_rgb_base_config.direction = 0;
    g_rgb_base_config.speed = (int16_t)(RGB_DEFAULT_SPEED);
    g_rgb_base_config.hsv = default_hsv;
    color_set_hsv(&g_rgb_base_config.rgb, &default_hsv);
    memset(&g_rgb_base_config.secondary_rgb, 0, sizeof(g_rgb_base_config.secondary_rgb));
    memset(&g_rgb_base_config.secondary_hsv, 0, sizeof(g_rgb_base_config.secondary_hsv));
    for (uint16_t i = 0; i < RGB_NUM; i++)
    {
        g_rgb_configs[i].mode = RGB_DEFAULT_MODE;
        g_rgb_configs[i].hsv = default_hsv;
        g_rgb_configs[i].speed = (int16_t)(RGB_DEFAULT_SPEED);
        color_set_hsv(&g_rgb_configs[i].rgb, &default_hsv);
    }
}

void rgb_flush(void)
{
    rgb_update_callback();
    for (uint16_t i = 0; i < RGB_NUM; i++)
    {
        rgb_set(i, g_rgb_colors[i].r, g_rgb_colors[i].g, g_rgb_colors[i].b);
    }
    led_flush();
}

/* Start the ripple effect of the LED that belongs to key id, if it has one. */
void rgb_activate(uint16_t id, uint32_t tick)
{
    if (id >= TOTAL_KEY_NUM)
    {
        return;
    }
    const uint16_t rgb_index = g_rgb_inverse_mapping[id];
    if (rgb_index >= RGB_NUM)
    {
        return;
    }
    g_rgb_configs[rgb_index].begin_tick = tick;
    if (rgb_mode_is_ripple(g_rgb_configs[rgb_index].mode))
    {
        const RGBArgument ripple = {tick, rgb_index};
        rgb_forward_list_insert_after(&rgb_argument_list, &rgb_argument_list.data[rgb_argument_list.head], ripple);
    }
}

/* ------------------------------------------------------------------------- */
/* Singly linked list of ripples over a fixed node pool. Node 0 after init is  */
/* a sentinel head so insertion never has to special-case an empty list.      */
/* ------------------------------------------------------------------------- */

void rgb_forward_list_init(RGBArgumentList* list, RGBArgumentListNode* data, uint16_t len)
{
    list->data = data;
    list->head = -1;
    list->tail = 0;
    list->len = len;
    for (int i = 0; i < len; i++)
    {
        list->data[i].next = i + 1;
    }
    list->data[len - 1].next = -1;
    list->free_node = 0;
    rgb_forward_list_push_front(list, (RGBArgument){0, 0});
}

void rgb_forward_list_erase_after(RGBArgumentList* list, RGBArgumentListNode* data)
{
    const int16_t target = data->next;
    data->next = list->data[target].next;
    list->data[target].next = list->free_node;
    list->free_node = target;
}

void rgb_forward_list_insert_after(RGBArgumentList* list, RGBArgumentListNode* data, RGBArgument t)
{
    if (list->free_node == -1)
    {
        return;
    }
    const int16_t new_node = list->free_node;
    list->free_node = list->data[list->free_node].next;

    list->data[new_node].data = t;
    list->data[new_node].next = data->next;

    data->next = new_node;
}

void rgb_forward_list_push_front(RGBArgumentList* list, RGBArgument t)
{
    if (list->free_node == -1)
    {
        return;
    }
    const int16_t new_node = list->free_node;
    list->free_node = list->data[list->free_node].next;

    list->data[new_node].data = t;
    list->data[new_node].next = list->head;

    list->head = new_node;
}
