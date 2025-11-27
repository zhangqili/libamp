/*
 * Copyright (c) 2025 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef DISTRIB_H_
#define DISTRIB_H_

#include "keyboard.h"
#ifdef MOUSE_ENABLE
#include "mouse.h"
#endif
#ifdef EXTRAKEY_ENABLE
#include "extra_key.h"
#endif
#ifdef JOYSTICK_ENABLE
#include "joystick.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct __Assignment
{
    Keyboard_6KROBuffer keyboard_6kro_buffer;
#ifdef NKRO_ENABLE
    Keyboard_NKROBuffer keyboard_nkro_buffer;
#endif
#ifdef MOUSE_ENABLE
    Mouse mouse_buffer;
#endif
#ifdef EXTRAKEY_ENABLE
    ExtraKey consumer_buffer;
    ExtraKey system_buffer;
#endif
#ifdef JOYSTICK_ENABLE
    Joystick joystick_buffer;
#endif
} Assignment;

void assign_init(void);
void assign_process(uint8_t slave_id, uint8_t *buf, uint16_t len);
bool assign_get_state(uint16_t index);
int assign_report(void);

#ifdef __cplusplus
}
#endif

#endif /* DISTRIB_H_ */
