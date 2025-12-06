/*
 * Copyright (c) 2025 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/**
 * @file driver.h
 * @brief Hardware Abstraction Layer (HAL) interface.
 * @details This header declares the low-level driver functions required by the core library.
 * These functions bridge the gap between the generic library logic and the specific
 * hardware (USB stack, Flash memory, LEDs, etc.).
 * @author Zhangqi Li
 * @date 2025
 */

#ifndef DRIVER_H
#define DRIVER_H

#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Sends a HID report via the Shared Endpoint.
 * @param report Pointer to the report data buffer.
 * @param len Length of the report data.
 * @return 0 on success, non-zero on failure.
 */
int hid_send_shared_ep(uint8_t *report, uint16_t len);

/**
 * @brief Sends a Standard Keyboard HID report (6KRO).
 * @param report Pointer to the keyboard report data.
 * @param len Length of the report.
 * @return 0 on success, non-zero on failure.
 */
int hid_send_keyboard(uint8_t *report, uint16_t len);

/**
 * @brief Sends an N-Key Rollover (NKRO) HID report.
 * @param report Pointer to the NKRO bitmap report.
 * @param len Length of the report.
 * @return 0 on success, non-zero on failure.
 */
int hid_send_nkro(uint8_t *report, uint16_t len);

/**
 * @brief Sends a Mouse HID report.
 * @param report Pointer to the mouse report data.
 * @param len Length of the report.
 * @return 0 on success, non-zero on failure.
 */
int hid_send_mouse(uint8_t *report, uint16_t len);

/**
 * @brief Sends a System/Consumer Control HID report (Extra Keys).
 * @param report Pointer to the extra key report data.
 * @param len Length of the report.
 * @return 0 on success, non-zero on failure.
 */
int hid_send_extra_key(uint8_t *report, uint16_t len);

/**
 * @brief Sends a Joystick HID report.
 * @param report Pointer to the joystick report data.
 * @param len Length of the report.
 * @return 0 on success, non-zero on failure.
 */
int hid_send_joystick(uint8_t *report, uint16_t len);

/**
 * @brief Sends a Digitizer HID report.
 * @param report Pointer to the digitizer report data.
 * @param len Length of the report.
 * @return 0 on success, non-zero on failure.
 */
int hid_send_digitizer(uint8_t *report, uint16_t len);

/**
 * @brief Sends a Programmable Button HID report.
 * @param report Pointer to the report data.
 * @param len Length of the report.
 * @return 0 on success, non-zero on failure.
 */
int hid_send_programmable_button(uint8_t *report, uint16_t len);

/**
 * @brief Sends a Raw HID report (bidirectional communication).
 * @param report Pointer to the raw data.
 * @param len Length of the data.
 * @return 0 on success, non-zero on failure.
 */
int hid_send_raw(uint8_t *report, uint16_t len);

/**
 * @brief Sends a MIDI packet.
 * @param report Pointer to the MIDI event packet.
 * @param len Length of the packet.
 * @return 0 on success, non-zero on failure.
 */
int send_midi(uint8_t *report, uint16_t len);

/**
 * @brief Sends a USB Remote Wakeup signal to the host.
 * @return 0 on success, non-zero on failure.
 */
int send_remote_wakeup(void);

/**
 * @brief Reads data from non-volatile storage (Flash/EEPROM).
 * @param addr The address/offset to read from.
 * @param size The number of bytes to read.
 * @param data Pointer to the buffer where read data will be stored.
 * @return 0 on success, non-zero on failure.
 */
int flash_read(uint32_t addr, uint32_t size, uint8_t *data);

/**
 * @brief Writes data to non-volatile storage (Flash/EEPROM).
 * @param addr The address/offset to write to.
 * @param size The number of bytes to write.
 * @param data Pointer to the data to be written.
 * @return 0 on success, non-zero on failure.
 */
int flash_write(uint32_t addr, uint32_t size, const uint8_t *data);

/**
 * @brief Erases a section of non-volatile storage.
 * @param addr The start address/offset to erase.
 * @param size The size of the area to erase.
 * @return 0 on success, non-zero on failure.
 */
int flash_erase(uint32_t addr, uint32_t size);

/**
 * @brief Sets the color of a specific LED (RGB).
 * @param index The index of the LED.
 * @param r Red component (0-255).
 * @param g Green component (0-255).
 * @param b Blue component (0-255).
 * @return 0 on success, non-zero on failure.
 */
int led_set(uint16_t index, uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief Sends a packet via the Nexus inter-chip protocol.
 * @param slave_id The ID of the target slave device.
 * @param report Pointer to the data packet.
 * @param len Length of the packet.
 * @return 0 on success, non-zero on failure.
 */
int nexus_send(uint8_t slave_id, uint8_t *report, uint16_t len);

/**
 * @brief Reports status or data back to the Nexus master.
 * @param report Pointer to the report data.
 * @param len Length of the report.
 * @return 0 on success, non-zero on failure.
 */
int nexus_report(uint8_t *report, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif //DRIVER_H
