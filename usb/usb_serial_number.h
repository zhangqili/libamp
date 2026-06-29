/*
 * Copyright (c) 2024 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * USB serial number helpers.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#if defined(SERIAL_NUMBER_USE_CUSTOM)
#    define USB_DESCRIPTOR_USE_CUSTOM_SERIAL 1
#else
#    define USB_DESCRIPTOR_USE_CUSTOM_SERIAL 0
#endif

#if defined(SERIAL_NUMBER) || USB_DESCRIPTOR_USE_CUSTOM_SERIAL
#    define HAS_SERIAL_NUMBER 1
#else
#    define HAS_SERIAL_NUMBER 0
#endif

#ifndef SERIAL_NUMBER_LENGTH
#    if defined(SERIAL_NUMBER)
#        define SERIAL_NUMBER_LENGTH (sizeof(SERIAL_NUMBER) - 1)
#    else
#        define SERIAL_NUMBER_LENGTH 32
#    endif
#endif

#define USB_DESCRIPTOR_SERIAL_NUMBER_ASCII_BUFFER_SIZE ((SERIAL_NUMBER_LENGTH) + 1)

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Override this weak hook in board code to provide a custom ASCII serial number.
 * Return the number of characters written, excluding the trailing null byte.
 */
size_t usb_descriptor_get_serial_number(char *buffer, size_t buffer_size);

/*
 * Convert raw bytes to an uppercase hexadecimal string. The output is always
 * null-terminated when buffer_size is non-zero.
 */
size_t usb_descriptor_bytes_to_hex_string(char *buffer, size_t buffer_size, const uint8_t *data, size_t data_size);

const char *usb_descriptor_get_serial_number_ascii(void);

#ifdef __cplusplus
}
#endif
