/*
 * Copyright (c) 2024 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef __HID_KEYBOARD_H
#define __HID_KEYBOARD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "usbd_core.h"
#include "usbd_hid.h"

#ifdef USB_CONFIG_POWER_MA
#undef USB_CONFIG_POWER_MA
#endif

#define USBD_LANGID_STRING 1033

enum
{
    USB_STATE_IDLE = 0,
    USB_STATE_BUSY
};

typedef struct __USBBuffer
{
    uint8_t read_buffer[64];
    uint8_t send_buffer[64];
    uint8_t state;
} USBBuffer;

void usb_init(uint8_t busid, uintptr_t reg_base);
int usb_send_shared_ep(uint8_t *buffer, uint8_t size);
int usb_send_keyboard(uint8_t *buffer, uint8_t size);
int usb_send_raw(uint8_t *buffer, uint8_t size);
int usb_send_midi(uint8_t *buffer, uint8_t size);
int usb_send_mouse(uint8_t *buffer, uint8_t size);
int usb_send_joystick(uint8_t *buffer, uint8_t size);
int usb_send_digitizer(uint8_t *buffer, uint8_t size);
int usb_send_console(uint8_t *buffer, uint8_t size);

void usbd_event_handler_user(uint8_t busid, uint8_t event);

#ifdef __cplusplus
}
#endif

#endif
