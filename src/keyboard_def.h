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

#define LIBAMP_STR_HELPER(x) #x
#define LIBAMP_STR(x) LIBAMP_STR_HELPER(x)

#define LIBAMP_VERSION_STRING \
    LIBAMP_STR(LIBAMP_VERSION_MAJOR) "." \
    LIBAMP_STR(LIBAMP_VERSION_MINOR) "." \
    LIBAMP_STR(LIBAMP_VERSION_PATCH) "-" \
    LIBAMP_VERSION_INFO

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

#ifdef __cplusplus
    #define restrict __restrict__
#endif

#endif //KEYBOARD_DEF_H
