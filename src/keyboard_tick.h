/*
 * Copyright (c) 2026 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef KEYBOARD_TICK_H_
#define KEYBOARD_TICK_H_

#include <stdbool.h>
#include <stdint.h>
#include "keyboard_config.h"

/* Set to 1 in keyboard_config.h to retain ticks beyond the 32-bit wrap. */
#ifndef KEYBOARD_TICK_64BIT_ENABLE
#define KEYBOARD_TICK_64BIT_ENABLE 0
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* One non-reentrant writer advances the software clock in POLLING_RATE units.
 * Readers may run in the foreground or in interrupts, including interrupts
 * which preempt the writer. The default 32-bit backend requires indivisible
 * aligned 32-bit loads/stores on the target and uses no atomic builtins.
 * With KEYBOARD_TICK_64BIT_ENABLE=1, atomic 32-bit loads/stores retain the
 * high word across 32-bit carries and support concurrent host-thread readers.
 * Otherwise get64 zero-extends the wrapping 32-bit clock; it does not extend it.
 * Initialization/reset must finish before readers or the writer start.
 * The weak getters are independent; get32 reads the low word directly.
 * A platform clock must override both getters with the same units and epoch.
 * Overriding one getter does not redirect the other. */
void keyboard_tick_init(void);
void keyboard_tick_reset(void);
void keyboard_tick_inc(uint32_t delta);
uint32_t keyboard_tick_get32(void);
uint64_t keyboard_tick_get64(void);

/* The real elapsed time must be less than 2^32 ticks. */
static inline uint32_t tick_elapsed32(uint32_t now, uint32_t start)
{
    return (uint32_t)(now - start);
}

/* Includes equality. The real signed distance from deadline to now must be
 * strictly between -2^31 and 2^31 ticks; neither 0 nor UINT32_MAX is a sentinel. */
static inline bool tick_reached32(uint32_t now, uint32_t deadline)
{
    return (uint32_t)(now - deadline) < UINT32_C(0x80000000);
}

#ifdef __cplusplus
}
#endif

#endif /* KEYBOARD_TICK_H_ */
