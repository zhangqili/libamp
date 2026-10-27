/*
 * Copyright (c) 2024 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef COLOR_H_
#define COLOR_H_

#include "stdint.h"
#include "stddef.h"
#include "stdbool.h"

#ifdef __cplusplus
#ifndef restrict
#if defined(__GNUC__) || defined(__clang__)
    #define restrict __restrict__
#elif defined(_MSC_VER)
    #define restrict __restrict
#else
    #define restrict
#endif
#endif
extern "C" {
#endif

typedef struct __ColorRGB
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
} ColorRGB;

typedef struct __ColorRGBFloat
{
    float r;
    float g;
    float b;
} ColorRGBFloat;

typedef ColorRGB Color;
typedef ColorRGBFloat ColorFloat;

/* h in degrees (0..359), s and v in percent (0..100). */
typedef struct __ColorHSV
{
    uint16_t h;
    uint8_t s;
    uint8_t v;
} ColorHSV;

void rgb_to_hsv(ColorHSV * restrict hsv, const ColorRGB * restrict rgb);
void hsv_to_rgb(ColorRGB * restrict rgb, const ColorHSV * restrict hsv);
void colorf_set_hsv(ColorFloat * restrict color, const ColorHSV * restrict rgb);
void color_get_hsv(const Color * restrict color, ColorHSV * restrict hsv);
void color_set_hsv(Color * restrict color, const ColorHSV * restrict hsv);

/* The small per-LED helpers are inline: the effect renderers call them for
 * every LED of every frame. */
static inline void color_get_rgb(const Color* restrict color, ColorRGB* restrict rgb)
{
    *rgb = *color;
}

static inline void color_set_rgb(Color* restrict color, const ColorRGB* restrict rgb)
{
    *color = *rgb;
}

static inline void colorf_set_rgb(ColorFloat * restrict color, const ColorRGB * restrict rgb)
{
    color->r = rgb->r;
    color->g = rgb->g;
    color->b = rgb->b;
}

/* Additive blend, saturating at 255 per channel. */
static inline void color_mix(Color *dest, const Color *source)
{
    const uint16_t r = (uint16_t)dest->r + source->r;
    const uint16_t g = (uint16_t)dest->g + source->g;
    const uint16_t b = (uint16_t)dest->b + source->b;
    dest->r = r > 255 ? 255 : (uint8_t)r;
    dest->g = g > 255 ? 255 : (uint8_t)g;
    dest->b = b > 255 ? 255 : (uint8_t)b;
}

static inline void colorf_mix(ColorFloat *dest, const ColorFloat *source)
{
    const float r = dest->r + source->r;
    const float g = dest->g + source->g;
    const float b = dest->b + source->b;
    dest->r = r > 255.0f ? 255.0f : r;
    dest->g = g > 255.0f ? 255.0f : g;
    dest->b = b > 255.0f ? 255.0f : b;
}

#ifdef __cplusplus
}
#endif

#endif
