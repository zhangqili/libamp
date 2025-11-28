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
int send_remote_wakeup(void);

int flash_read(uint32_t addr, uint32_t size, uint8_t *data);
int flash_write(uint32_t addr, uint32_t size, const uint8_t *data);
int flash_erase(uint32_t addr, uint32_t size);

int led_set(uint16_t index, uint8_t r, uint8_t g, uint8_t b);


int nexus_send(uint8_t slave_id, uint8_t *report, uint16_t len);
int nexus_report(uint8_t *report, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif //DRIVER_H
