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

typedef struct __ColorHSV
{
    uint16_t h;
    uint8_t s;
    uint8_t v;
} ColorHSV;

void rgb_to_hsv(ColorHSV * restrict hsv, const ColorRGB * restrict rgb);
void hsv_to_rgb(ColorRGB * restrict rgb, const ColorHSV * restrict hsv);
void color_get_rgb(const Color * restrict color, ColorRGB * restrict rgb);
void color_set_rgb(Color * restrict color, const ColorRGB * restrict rgb);
void colorf_set_rgb(ColorFloat * restrict color, const ColorRGB * restrict rgb);
void colorf_set_hsv(ColorFloat * restrict color, const ColorHSV * restrict rgb);
void color_get_hsv(const Color * restrict color, ColorHSV * restrict hsv);
void color_set_hsv(Color * restrict color, const ColorHSV * restrict hsv);
void color_mix(Color *dest, const Color *source);
void colorf_mix(ColorFloat *dest, const ColorFloat *source);

#ifdef __cplusplus
}
#endif

#endif