/*
 * Copyright (c) 2024 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "color.h"

/* All arithmetic below is single precision on purpose: Cortex-M4F/M7 cores
 * have a float FPU only, and a double literal silently turns an expression
 * into a software double-precision call. */

void rgb_to_hsv(ColorHSV * restrict hsv, const ColorRGB * restrict rgb)
{
    const float r = rgb->r;
    const float g = rgb->g;
    const float b = rgb->b;
    float max = r > g ? r : g;
    float min = r < g ? r : g;
    max = b > max ? b : max;
    min = b < min ? b : min;
    const float chroma = max - min;

    if (chroma == 0.0f)
    {
        hsv->h = 0;
    }
    else if (max == r)
    {
        hsv->h = (uint16_t)(g >= b ? 60.0f * (g - b) / chroma : 60.0f * (g - b) / chroma + 360.0f);
    }
    else if (max == g)
    {
        hsv->h = (uint16_t)(60.0f * (b - r) / chroma + 120.0f);
    }
    else
    {
        hsv->h = (uint16_t)(60.0f * (r - g) / chroma + 240.0f);
    }
    hsv->s = (uint8_t)(max == 0.0f ? 0.0f : 100.0f * (1.0f - min / max));
    hsv->v = (uint8_t)(max * 100.0f / 255.0f);
}

/* Integer HSV to RGB. Bounded reciprocals and constant divisors avoid runtime
 * division and floating point in this per-LED operation. Every channel still
 * uses the floor of the exact percentage conversion. */
void hsv_to_rgb(ColorRGB * restrict rgb, const ColorHSV * restrict hsv)
{
    const uint32_t v = hsv->v > 100 ? 100 : hsv->v;
    const uint32_t s = hsv->s > 100 ? 100 : hsv->s;
    /* 5223/2048 = 255/100 + 3/10240. For v <= 100 the excess is
     * below 1/20, the smallest fractional step of v*255/100. */
    const uint8_t value = (uint8_t)((v * 5223u) >> 11);
    if (s == 0)
    {
        rgb->r = value;
        rgb->g = value;
        rgb->b = value;
        return;
    }
    /* This reciprocal is exact for every uint16_t hue: its excess over 1/60
     * is 28/(60*2^21), giving an error below 1/60 even at UINT16_MAX.
     * The product also fits in 32 bits, avoiding a wide multiply on M4. */
    const uint32_t sector = ((uint32_t)hsv->h * 34953u) >> 21;
    /* Distance from the odd sector boundary gives the rising channel for
     * even sectors and the falling channel for odd sectors. */
    const int32_t distance = (int32_t)hsv->h - (int32_t)((sector | 1u) * 60u);
    const uint32_t interpolation = distance < 0 ? (uint32_t)-distance : (uint32_t)distance;
    const uint32_t scaled_value = v * 255u;
    const uint8_t x = (uint8_t)(scaled_value * (100u - s) / 10000u);
    const uint8_t intermediate = (uint8_t)(scaled_value * (6000u - s * interpolation) / 600000u);
    switch (sector)
    {
        case 0:  rgb->r = value;        rgb->g = intermediate; rgb->b = x;            break;
        case 1:  rgb->r = intermediate; rgb->g = value;        rgb->b = x;            break;
        case 2:  rgb->r = x;            rgb->g = value;        rgb->b = intermediate; break;
        case 3:  rgb->r = x;            rgb->g = intermediate; rgb->b = value;        break;
        case 4:  rgb->r = intermediate; rgb->g = x;            rgb->b = value;        break;
        case 5:  rgb->r = value;        rgb->g = x;            rgb->b = intermediate; break;
        default: rgb->r = 0;     rgb->g = 0;     rgb->b = 0;     break; /* hue out of range */
    }
}

void colorf_set_hsv(ColorFloat * restrict color, const ColorHSV * restrict hsv)
{
    ColorRGB temp_rgb = {0, 0, 0};
    hsv_to_rgb(&temp_rgb, hsv);
    colorf_set_rgb(color, &temp_rgb);
}

void color_get_hsv(const Color* restrict color, ColorHSV* restrict hsv)
{
    rgb_to_hsv(hsv, color);
}

void color_set_hsv(Color* restrict color, const ColorHSV* restrict hsv)
{
    hsv_to_rgb(color, hsv);
}
