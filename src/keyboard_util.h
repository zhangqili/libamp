/*
 * Copyright (c) 2026 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef KEYBOARD_UTIL_H
#define KEYBOARD_UTIL_H

#include "stdint.h"

#ifndef BIT
#define BIT(x)  (1UL << (x))
#endif

#ifndef BIT_SET
#define BIT_SET(value, bit) ((value) |= BIT(bit))
#endif

#ifndef BIT_RESET
#define BIT_RESET(value, bit) ((value) &= ~BIT(bit))
#endif

#ifndef BIT_TOGGLE
#define BIT_TOGGLE(value, bit) ((value) ^= BIT(bit))
#endif

#ifndef BIT_GET
#define BIT_GET(value, bit) ((value) & (BIT(bit)))
#endif

#ifndef IS_POS_EDGE
#define IS_POS_EDGE(state, next_state) (!(state) && (next_state))
#endif

#ifndef IS_NEG_EDGE
#define IS_NEG_EDGE(state, next_state) ((state) && !(next_state))
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static inline int32_t clamp_i32(int32_t val, int32_t min_val, int32_t max_val) {
    if (val > max_val) return max_val;
    if (val < min_val) return min_val;
    return val;
}

static inline float clamp_f32(float val, float min_val, float max_val) {
    if (val > max_val) return max_val;
    if (val < min_val) return min_val;
    return val;
}

#endif //KEYBOARD_UTIL_H
