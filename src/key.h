/*
 * Copyright (c) 2024 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef KEY_H_
#define KEY_H_

/**
 * @file key.h
 * @brief Basic digital key definition and handling.
 * @details Defines the core structure for a key, including its physical state,
 * report state, debounce counters, and event callbacks (key down/up).
 * @author Zhangqi Li
 * @date 2024
 */

#include "stdint.h"
#include "stddef.h"
#include "stdbool.h"

#include "keyboard_conf.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Enumeration of key events.
 */
typedef enum
{
    KEY_EVENT_DOWN, /**< Key press event. */
    KEY_EVENT_UP,   /**< Key release event. */
    KEY_EVENT_NUM   /**< Total number of key events. */
} KEY_EVENT;

/**
 * @brief Function pointer type for key callbacks.
 * @param void* Context pointer (usually points to the Key instance).
 */
typedef void (*key_cb_t)(void *);

/**
 * @brief Base structure for a key.
 * @details Contains the minimal information required to represent a key's state.
 * Often used for serialization or lightweight processing.
 */
typedef struct __KeyBase
{
    uint16_t id;          /**< Unique identifier for the key (e.g., matrix index). */
    uint8_t state;        /**< Current physical/raw state (1 = pressed, 0 = released). */
    uint8_t report_state; /**< Current logical/debounced state sent to the host. */
} KeyBase;

/**
 * @brief Full structure for a key.
 * @details Extends the base functionality with debounce logic and event callbacks.
 */
typedef struct __Key
{
    uint16_t id;          /**< Unique identifier for the key. */
    uint8_t state;        /**< Physical state (1 = pressed, 0 = released). */
    uint8_t report_state; /**< Logical/Report state (after debouncing). */
#if DEBOUNCE_PRESS > 0 || DEBOUNCE_RELEASE > 0
    int8_t debounce;      /**< Debounce counter. Positive for press delay, negative for release delay. */
#endif
    key_cb_t key_cb[KEY_EVENT_NUM]; /**< Array of callback functions for key events. */
} Key;

/**
 * @brief Updates the state of a key and triggers callbacks if the state changes.
 * @param key Pointer to the Key structure.
 * @param state New physical state of the key (true = pressed, false = released).
 * @return true if the key state changed, false otherwise.
 */
bool key_update(Key *key, bool state);

/**
 * @brief Attaches a callback function to a specific key event.
 * @param key Pointer to the Key structure.
 * @param e The event type (KEY_EVENT_DOWN or KEY_EVENT_UP).
 * @param cb The callback function to execute when the event occurs.
 */
void key_attach(Key *key, KEY_EVENT e, key_cb_t cb);

#ifdef __cplusplus
}
#endif

#endif