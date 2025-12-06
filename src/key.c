/*
 * Copyright (c) 2024 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file key.c
 * @brief Implementation of key state management.
 * @details Handles the state transition logic for keys, including edge detection
 * and callback dispatching.
 * @author Zhangqi Li
 * @date 2024
 */

#include "key.h"

/**
 * @brief Updates the key's state and triggers events.
 * @details This function compares the current state with the new state.
 * - Rising Edge (Released -> Pressed): Triggers KEY_EVENT_DOWN.
 * - Falling Edge (Pressed -> Released): Triggers KEY_EVENT_UP.
 * * @param key Pointer to the key instance.
 * @param state The new physical state sampled from the hardware.
 * @return true if the state has changed (edge detected), false if the state remains the same.
 */
bool key_update(Key* key,bool state)
{
    // Detect Rising Edge: Currently released (0), New state is pressed (1)
    if ((!(key->state)) && state)
    {
        if (key->key_cb[KEY_EVENT_DOWN])
            key->key_cb[KEY_EVENT_DOWN](key);
        key->state = state;
        return true;
    }

    // Detect Falling Edge: Currently pressed (1), New state is released (0)
    if ((key->state) && (!state))
    {
        if (key->key_cb[KEY_EVENT_UP])
            key->key_cb[KEY_EVENT_UP](key);
        key->state = state;
        return true;
    }

    // No state change
    key->state = state;
    return false;
}

/**
 * @brief Registers a user callback for a specific key event.
 * @param key Pointer to the key instance.
 * @param e Event type to attach to.
 * @param cb Function pointer to the callback.
 */
void key_attach(Key* key, KEY_EVENT e, key_cb_t cb)
{
    key->key_cb[e] = cb;
}
