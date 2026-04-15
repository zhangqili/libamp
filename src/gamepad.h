/*
 * Copyright (c) 2026 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef GAMEPAD_H
#define GAMEPAD_H

#include "keyboard.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GAMEPAD_KEYCODE_IS_AXIS(keycode) (KEYCODE_GET_SUB((keycode)) > 17)
/*
 * Copyright (c) 2026, sakumisu
 *
 * SPDX-License-Identifier: Apache-2.0
 * 
 * structure from CherryUSB
 */
typedef struct __Gamepad {
    uint8_t report_id;   /* Always 0x00 */
    uint8_t report_size; /* Always 0x14 (20) */
    uint16_t buttons;    /* DPAD, Start, Back, L3, R3, LB, RB, Guide, A, B, X, Y */
    uint8_t lt;          /* Left trigger (0-255) */
    uint8_t rt;          /* Right trigger (0-255) */
    int16_t lx;          /* Left stick X (-32768 to 32767) */
    int16_t ly;          /* Left stick Y (-32768 to 32767) */
    int16_t rx;          /* Right stick X (-32768 to 32767) */
    int16_t ry;          /* Right stick Y (-32768 to 32767) */
    uint8_t reserved[6]; /* Reserved/padding */
} __PACKED Gamepad;

typedef struct __GamepadOutReport {
    uint8_t report_id;   // 0x00 = rumble, 0x01 = LED
    uint8_t report_size; // 0x08
    uint8_t led;         // LED pattern (0x00 for rumble)
    uint8_t rumble_l;    // Left motor (large, 0-255)
    uint8_t rumble_r;    // Right motor (small, 0-255)
    uint8_t reserved[3]; // Padding
} __PACKED GamepadOutReport;

void gamepad_event_handler(KeyboardEvent event);
void gamepad_buffer_clear(void);
void gamepad_add_buffer(KeyboardEvent event);
void gamepad_set_axis(Keycode keycode, AnalogValue value);
int gamepad_buffer_send(void);
void gamepad_out_callback(GamepadOutReport* report);

#ifdef __cplusplus
}
#endif

#endif //GAMEPAD_H
