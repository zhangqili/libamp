/*
 * Copyright (c) 2024 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef MOUSE_H_
#define MOUSE_H_

#include "stdint.h"
#include "keyboard_def.h"
#include "keyboard_conf.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef WHEEL_EXTENDED_REPORT
typedef int16_t MouseInt;
#ifndef MOUSE_MAX_VALUE
#define MOUSE_MAX_VALUE 32767
#endif
#else
typedef int8_t MouseInt;
#ifndef MOUSE_MAX_VALUE
#define MOUSE_MAX_VALUE 127
#endif
#endif

#ifndef MOUSE_MAX_SPEED
#define MOUSE_MAX_SPEED 1000
#endif

#define MOUSE_KEYCODE_IS_MOVE(keycode) (KEYCODE_GET_SUB((keycode)) >= MOUSE_MOVE_UP)

typedef struct __Mouse {
#ifdef MOUSE_SHARED_EP
    uint8_t report_id;
#endif
    uint8_t buttons;
#ifdef MOUSE_EXTENDED_REPORT
    int8_t boot_x;
    int8_t boot_y;
#endif
    MouseInt x;
    MouseInt y;
    MouseInt v;
    MouseInt h;
} __PACKED Mouse;

//void mouse_buffer_clear(Mouse*mouse);
void mouse_event_handler(KeyboardEvent event);
void mouse_buffer_clear(void);
void mouse_add_buffer(uint16_t keycode);
void mouse_set_axis(Keycode keycode, AnalogValue value);
int mouse_buffer_send(void);

#ifdef __cplusplus
}
#endif

#endif /* MOUSE_H_ */
