/*
 * Copyright (c) 2026 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "keyboard_tick.h"
#include "keyboard_def.h"

#if KEYBOARD_TICK_64BIT_ENABLE
/* get32 reads the low word directly with a single 32-bit atomic load. */
static uint32_t keyboard_tick_low;

/* Non-carry increments update the active slot's low word in place; because the
 * high word is unchanged, a concurrent reader observing either the old or new
 * low word still reconstructs an exact 64-bit tick. Only a 32-bit carry
 * publishes to the inactive slot and advances the generation, so a reader
 * interrupting the writer can always read a complete snapshot without waiting.
 * Only native 32-bit atomic loads/stores are used; no 64-bit atomic runtime or
 * interrupt-disabling platform hooks are needed. A read must complete before
 * 2^32 carries. */
static struct {
    uint32_t low;
    uint32_t high;
} snapshots[2];
static uint32_t generation;

void keyboard_tick_init(void)
{
    __atomic_store_n(&keyboard_tick_low, 0, __ATOMIC_RELAXED);
    for (unsigned i = 0; i < 2; i++)
    {
        __atomic_store_n(&snapshots[i].low, 0, __ATOMIC_RELAXED);
        __atomic_store_n(&snapshots[i].high, 0, __ATOMIC_RELAXED);
    }
    __atomic_store_n(&generation, 0, __ATOMIC_RELAXED);
}

void keyboard_tick_inc(uint32_t delta)
{
    const uint32_t old_low = __atomic_load_n(&keyboard_tick_low, __ATOMIC_RELAXED);
    const uint32_t new_low = old_low + delta;
    __atomic_store_n(&keyboard_tick_low, new_low, __ATOMIC_RELAXED);

    const uint32_t gen = __atomic_load_n(&generation, __ATOMIC_RELAXED);
    const unsigned slot = gen & 1u;
    if (__builtin_expect(new_low >= old_low, 1))
    {
        __atomic_store_n(&snapshots[slot].low, new_low, __ATOMIC_RELAXED);
        return;
    }

    const uint32_t new_high = __atomic_load_n(&snapshots[slot].high, __ATOMIC_RELAXED) + 1u;
    const uint32_t next = gen + 1u;
    const unsigned next_slot = next & 1u;
    /* Make prior publications visible before reusing a slot. A reader which
     * observes either new word must then detect the intervening generation. */
    __atomic_thread_fence(__ATOMIC_RELEASE);
    __atomic_store_n(&snapshots[next_slot].low, new_low, __ATOMIC_RELAXED);
    __atomic_store_n(&snapshots[next_slot].high, new_high, __ATOMIC_RELAXED);
    __atomic_store_n(&generation, next, __ATOMIC_RELEASE);
}

__WEAK uint32_t keyboard_tick_get32(void)
{
    return __atomic_load_n(&keyboard_tick_low, __ATOMIC_RELAXED);
}

__WEAK uint64_t keyboard_tick_get64(void)
{
    uint32_t before, low, high;
    do
    {
        before = __atomic_load_n(&generation, __ATOMIC_ACQUIRE);
        low = __atomic_load_n(&snapshots[before & 1u].low, __ATOMIC_RELAXED);
        high = __atomic_load_n(&snapshots[before & 1u].high, __ATOMIC_RELAXED);
        /* Keep both word loads before validating the generation. */
        __atomic_thread_fence(__ATOMIC_ACQUIRE);
    } while (before != __atomic_load_n(&generation, __ATOMIC_RELAXED));
    return ((uint64_t)high << 32) | low;
}

#else

/* Single writer; foreground/ISR readers use native aligned 32-bit accesses. */
static volatile uint32_t keyboard_tick;

void keyboard_tick_init(void)
{
    keyboard_tick = 0;
}

void keyboard_tick_inc(uint32_t delta)
{
    keyboard_tick += delta;
}

__WEAK uint32_t keyboard_tick_get32(void)
{
    return keyboard_tick;
}

__WEAK uint64_t keyboard_tick_get64(void)
{
    return keyboard_tick;
}

#endif

void keyboard_tick_reset(void)
{
    keyboard_tick_init();
}

