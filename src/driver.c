/*
 * Copyright (c) 2025 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "driver.h"
#include "keyboard_def.h"
#include "stdio.h"

/**
 * @file driver.c
 * @brief Default weak implementations for the hardware driver interface.
 * @details This file provides placeholder implementations for all driver functions
 * defined in driver.h. These functions are marked with `__WEAK`.
 * * **Purpose of Weak Definitions:**
 * If the user's project does not provide a specific implementation for a function
 * (e.g., `hid_send_keyboard`), the linker will use the default version defined here
 * to prevent compilation errors. The default versions usually print a warning
 * indicating the feature is not implemented.
 * * @author Zhangqi Li
 * @date 2025
 */

 /**
 * @brief Default implementation: Send Shared Endpoint HID report.
 * @note **Weak Symbol**: Override this in your hardware-specific code.
 */
__WEAK int hid_send_shared_ep(uint8_t *report, uint16_t len)
{
    UNUSED(report);
    UNUSED(len);
    printf("hid_send_shared_ep needs to be implemented.\n");
    return 0;
}

/**
 * @brief Default implementation: Send Standard Keyboard HID report.
 * @note **Weak Symbol**: Override this in your hardware-specific code.
 */
__WEAK int hid_send_keyboard(uint8_t *report, uint16_t len)
{
    UNUSED(report);
    UNUSED(len);
    printf("hid_send_keyboard needs to be implemented.\n");
    return 0;
}

/**
 * @brief Default implementation: Send NKRO HID report.
 * @note **Weak Symbol**: Override this in your hardware-specific code.
 */
__WEAK int hid_send_nkro(uint8_t *report, uint16_t len)
{
    UNUSED(report);
    UNUSED(len);
    printf("hid_send_nkro needs to be implemented.\n");
    return 0;
}

/**
 * @brief Default implementation: Send Mouse HID report.
 * @note **Weak Symbol**: Override this in your hardware-specific code.
 */
__WEAK int hid_send_mouse(uint8_t *report, uint16_t len)
{
    UNUSED(report);
    UNUSED(len);
    printf("hid_send_mouse needs to be implemented.\n");
    return 0;
}

/**
 * @brief Default implementation: Send Extra Key (System/Consumer) HID report.
 * @note **Weak Symbol**: Override this in your hardware-specific code.
 */
__WEAK int hid_send_extra_key(uint8_t *report, uint16_t len)
{
    UNUSED(report);
    UNUSED(len);
    printf("hid_send_extra_key needs to be implemented.\n");
    return 0;
}

/**
 * @brief Default implementation: Send Joystick HID report.
 * @note **Weak Symbol**: Override this in your hardware-specific code.
 */
__WEAK int hid_send_joystick(uint8_t *report, uint16_t len)
{
    UNUSED(report);
    UNUSED(len);
    printf("hid_send_joystick needs to be implemented.\n");
    return 0;
}

/**
 * @brief Default implementation: Send Digitizer HID report.
 * @note **Weak Symbol**: Override this in your hardware-specific code.
 */
__WEAK int hid_send_digitizer(uint8_t *report, uint16_t len)
{
    UNUSED(report);
    UNUSED(len);
    printf("hid_send_digitizer needs to be implemented.\n");
    return 0;
}   

/**
 * @brief Default implementation: Send Programmable Button HID report.
 * @note **Weak Symbol**: Override this in your hardware-specific code.
 */
__WEAK int hid_send_programmable_button(uint8_t *report, uint16_t len)
{
    UNUSED(report);
    UNUSED(len);
    printf("hid_send_programmable_button needs to be implemented.\n");
    return 0;
}   

/**
 * @brief Default implementation: Send Raw HID report.
 * @note **Weak Symbol**: Override this in your hardware-specific code.
 */
__WEAK int hid_send_raw(uint8_t *report, uint16_t len)
{
    UNUSED(report);
    UNUSED(len);
    printf("hid_send_raw needs to be implemented.\n");
    return 0;
}

/**
 * @brief Default implementation: Send MIDI packet.
 * @note **Weak Symbol**: Override this in your hardware-specific code.
 */
__WEAK int send_midi(uint8_t *report, uint16_t len)
{
    UNUSED(report);
    UNUSED(len);
    printf("send_midi needs to be implemented.\n");
    return 0;
}

/**
 * @brief Default implementation: Send Remote Wakeup signal.
 * @note **Weak Symbol**: Override this in your hardware-specific code.
 */
__WEAK int send_remote_wakeup(void)
{
    printf("send_remote_wakeup needs to be implemented.\n");
    return 0;
}

/**
 * @brief Default implementation: Read from Flash/EEPROM.
 * @note **Weak Symbol**: Override this in your hardware-specific code to support persistent storage.
 */
__WEAK int flash_read(uint32_t addr, uint32_t size, uint8_t *data)
{
    UNUSED(addr);
    UNUSED(size);
    UNUSED(data);
    printf("flash_read needs to be implemented.\n");
    return 0;
}

/**
 * @brief Default implementation: Write to Flash/EEPROM.
 * @note **Weak Symbol**: Override this in your hardware-specific code.
 */
__WEAK int flash_write(uint32_t addr, uint32_t size, const uint8_t *data)
{
    UNUSED(addr);
    UNUSED(size);
    UNUSED(data);
    printf("flash_write needs to be implemented.\n");
    return 0;
}

/**
 * @brief Default implementation: Erase Flash/EEPROM area.
 * @note **Weak Symbol**: Override this in your hardware-specific code.
 */
__WEAK int flash_erase(uint32_t addr, uint32_t size)
{
    UNUSED(addr);
    UNUSED(size);
    printf("flash_erase needs to be implemented.\n");
    return 0;
}

/**
 * @brief Default implementation: Set LED color.
 * @note **Weak Symbol**: Override this in your hardware-specific code to support RGB lighting.
 */
__WEAK int led_set(uint16_t index, uint8_t r, uint8_t g, uint8_t b)
{
    UNUSED(index);
    UNUSED(r);
    UNUSED(g);
    UNUSED(b);
    printf("led_set needs to be implemented.\n");
    return 0;
}

/**
 * @brief Default implementation: Send Nexus protocol data.
 * @note **Weak Symbol**: Override this in your hardware-specific code.
 */
__WEAK int nexus_send(uint8_t slave_id, uint8_t *report, uint16_t len)
{
    UNUSED(slave_id);
    UNUSED(report);
    UNUSED(len);
    printf("nexus_send needs to be implemented.\n");
    return 0;
}

/**
 * @brief Default implementation: Report Nexus protocol data.
 * @note **Weak Symbol**: Override this in your hardware-specific code.
 */
__WEAK int nexus_report(uint8_t *report, uint16_t len)
{
    UNUSED(report);
    UNUSED(len);
    printf("nexus_send needs to be implemented.\n");
    return 0;
}
