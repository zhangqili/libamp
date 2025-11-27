/*
 * Copyright (c) 2024 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef KEYBOARD_DEF_H
#define KEYBOARD_DEF_H

#define LIBAMP_VERSION_MAJOR 0
#define LIBAMP_VERSION_MINOR 1
#define LIBAMP_VERSION_PATCH 0
#define LIBAMP_VERSION_INFO  "beta"

#if defined (__ARMCC_VERSION) /* ARM Compiler */
  #ifndef __WEAK
    #define __WEAK  __attribute__((weak))
  #endif
  #ifndef __PACKED
    #define __PACKED  __attribute__((packed))
  #endif
#elif defined ( __GNUC__ ) && !defined (__CC_ARM) /* GNU Compiler */
  #ifndef __WEAK
    #define __WEAK   __attribute__((weak))
  #endif /* __WEAK */
  #ifndef __PACKED
    #define __PACKED __attribute__((__packed__))
  #endif
#endif /* __GNUC__ */

#if !defined(UNUSED)
#define UNUSED(X) (void)X      /* To avoid gcc/g++ warnings */
#endif /* UNUSED */

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

#endif //KEYBOARD_DEF_H
