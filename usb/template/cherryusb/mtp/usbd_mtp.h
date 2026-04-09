/*
 * Copyright (c) 2026 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef USBD_MTP_H
#define USBD_MTP_H

#include "usbd_core.h"

#ifdef __cplusplus
extern "C" {
#endif

struct usbd_interface *usbd_mtp_init_intf(uint8_t busid, struct usbd_interface *intf, const uint8_t *desc, uint32_t desc_len, uint8_t ep_out, uint8_t ep_in, uint8_t ep_int);
void usbd_mtp_task(void);


#ifdef __cplusplus
}
#endif

#endif //USBD_MTP_H
