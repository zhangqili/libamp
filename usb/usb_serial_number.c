/*
 * Copyright (c) 2024 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "keyboard_config.h"
#include "keyboard_def.h"
#include "usb_serial_number.h"

static size_t usb_descriptor_copy_serial_ascii(char *buffer, size_t buffer_size, const char *source)
{
    size_t length = 0;

    if (buffer == NULL || buffer_size == 0) {
        return 0;
    }

    if (source != NULL) {
        while (source[length] != '\0' && length + 1 < buffer_size) {
            buffer[length] = source[length];
            length++;
        }
    }

    buffer[length] = '\0';
    return length;
}

__WEAK size_t usb_descriptor_get_serial_number(char *buffer, size_t buffer_size)
{
#if defined(SERIAL_NUMBER)
    return usb_descriptor_copy_serial_ascii(buffer, buffer_size, SERIAL_NUMBER);
#else
    if (buffer != NULL && buffer_size > 0) {
        buffer[0] = '\0';
    }
    return 0;
#endif
}

size_t usb_descriptor_bytes_to_hex_string(char *buffer, size_t buffer_size, const uint8_t *data, size_t data_size)
{
    static const char hex_str[] = "0123456789ABCDEF";

    if (buffer == NULL || buffer_size == 0) {
        return 0;
    }

    if (data == NULL || data_size == 0) {
        buffer[0] = '\0';
        return 0;
    }

    size_t byte_count = data_size;
    const size_t max_byte_count = (buffer_size - 1) / 2;

    if (byte_count > max_byte_count) {
        byte_count = max_byte_count;
    }

    for (size_t i = 0; i < byte_count; i++) {
        buffer[(i * 2) + 0] = hex_str[data[i] >> 4];
        buffer[(i * 2) + 1] = hex_str[data[i] & 0x0F];
    }

    buffer[byte_count * 2] = '\0';
    return byte_count * 2;
}

const char *usb_descriptor_get_serial_number_ascii(void)
{
    static char serial[USB_DESCRIPTOR_SERIAL_NUMBER_ASCII_BUFFER_SIZE] = {0};
    size_t length = usb_descriptor_get_serial_number(serial, sizeof(serial));

    if (length >= sizeof(serial)) {
        length = sizeof(serial) - 1;
    }

    serial[length] = '\0';
    return serial;
}
