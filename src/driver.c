/*
 * Copyright (c) 2025 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "driver.h"
#include "keyboard_def.h"
#include "stdio.h"

__WEAK int hid_send_shared_ep(uint8_t *report, uint16_t len)
{
    UNUSED(report);
    UNUSED(len);
    printf("hid_send_shared_ep needs to be implemented.\n");
    return 0;
}

__WEAK int hid_send_keyboard(uint8_t *report, uint16_t len)
{
    UNUSED(report);
    UNUSED(len);
    printf("hid_send_keyboard needs to be implemented.\n");
    return 0;
}

__WEAK int hid_send_nkro(uint8_t *report, uint16_t len)
{
    UNUSED(report);
    UNUSED(len);
    printf("hid_send_nkro needs to be implemented.\n");
    return 0;
}

__WEAK int hid_send_mouse(uint8_t *report, uint16_t len)
{
    UNUSED(report);
    UNUSED(len);
    printf("hid_send_mouse needs to be implemented.\n");
    return 0;
}

__WEAK int hid_send_extra_key(uint8_t *report, uint16_t len)
{
    UNUSED(report);
    UNUSED(len);
    printf("hid_send_extra_key needs to be implemented.\n");
    return 0;
}

__WEAK int hid_send_joystick(uint8_t *report, uint16_t len)
{
    UNUSED(report);
    UNUSED(len);
    printf("hid_send_joystick needs to be implemented.\n");
    return 0;
}


__WEAK int hid_send_digitizer(uint8_t *report, uint16_t len)
{
    UNUSED(report);
    UNUSED(len);
    printf("hid_send_digitizer needs to be implemented.\n");
    return 0;
}   
__WEAK int hid_send_programmable_button(uint8_t *report, uint16_t len)
{
    UNUSED(report);
    UNUSED(len);
    printf("hid_send_programmable_button needs to be implemented.\n");
    return 0;
}   

__WEAK int hid_send_raw(uint8_t *report, uint16_t len)
{
    UNUSED(report);
    UNUSED(len);
    printf("hid_send_raw needs to be implemented.\n");
    return 0;
}

__WEAK int send_midi(uint8_t *report, uint16_t len)
{
    UNUSED(report);
    UNUSED(len);
    printf("send_midi needs to be implemented.\n");
    return 0;
}

__WEAK int send_remote_wakeup(void)
{
    printf("send_remote_wakeup needs to be implemented.\n");
    return 0;
}

__WEAK int flash_read(uint32_t addr, uint32_t size, uint8_t *data)
{
    UNUSED(addr);
    UNUSED(size);
    UNUSED(data);
    printf("flash_read needs to be implemented.\n");
    return 0;
}

__WEAK int flash_write(uint32_t addr, uint32_t size, const uint8_t *data)
{
    UNUSED(addr);
    UNUSED(size);
    UNUSED(data);
    printf("flash_write needs to be implemented.\n");
    return 0;
}

__WEAK int flash_erase(uint32_t addr, uint32_t size)
{
    UNUSED(addr);
    UNUSED(size);
    printf("flash_erase needs to be implemented.\n");
    return 0;
}

__WEAK int led_set(uint16_t index, uint8_t r, uint8_t g, uint8_t b)
{
    UNUSED(index);
    UNUSED(r);
    UNUSED(g);
    UNUSED(b);
    printf("led_set needs to be implemented.\n");
    return 0;
}

__WEAK int assign_send(uint8_t slave_id, uint8_t *report, uint16_t len)
{
    UNUSED(slave_id);
    UNUSED(report);
    UNUSED(len);
    printf("assign_send needs to be implemented.\n");
    return 0;
}
