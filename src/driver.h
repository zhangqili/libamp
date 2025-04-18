/*
 * Copyright (c) 2025 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef DRIVER_H
#define DRIVER_H

#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

int hid_send_shared_ep(uint8_t *report, uint16_t len);
int hid_send_keyboard(uint8_t *report, uint16_t len);
int hid_send_nkro(uint8_t *report, uint16_t len);
int hid_send_mouse(uint8_t *report, uint16_t len);
int hid_send_extra_key(uint8_t *report, uint16_t len);
int hid_send_joystick(uint8_t *report, uint16_t len);
int hid_send_digitizer(uint8_t *report, uint16_t len);
int hid_send_programmable_button(uint8_t *report, uint16_t len);
int hid_send_raw(uint8_t *report, uint16_t len);
int send_midi(uint8_t *report, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif //DRIVER_H
