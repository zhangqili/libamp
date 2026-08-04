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

#define STATIC_ASSERT_CONCAT_IMPL(a, b) a##b
#define STATIC_ASSERT_CONCAT(a, b) STATIC_ASSERT_CONCAT_IMPL(a, b)

/* C++ */
#if defined(__cplusplus)

    #if __cplusplus >= 201103L
        #define STATIC_ASSERT(cond, msg) static_assert((cond), msg)
    #else
        /* C++98/C++03 回退 */
        #define STATIC_ASSERT(cond, msg)                               \
            typedef char STATIC_ASSERT_CONCAT(                         \
                static_assertion_at_line_, __LINE__                    \
            )[(cond) ? 1 : -1]
    #endif


/* C23 */
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L

    #define STATIC_ASSERT(cond, msg) static_assert((cond), msg)

/* C11 */
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L

    #define STATIC_ASSERT(cond, msg) _Static_assert((cond), msg)

/* C90/C99 */
#else

    #define STATIC_ASSERT(cond, msg)                                   \
        typedef char STATIC_ASSERT_CONCAT(                             \
            static_assertion_at_line_, __LINE__                        \
        )[(cond) ? 1 : -1]

#endif


#ifdef __cplusplus
    #define restrict __restrict__
#endif

#endif //KEYBOARD_DEF_H
