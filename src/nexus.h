/*
 * Copyright (c) 2025 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef NEXUS_H_
#define NEXUS_H_

#include "keyboard.h"

#ifdef __cplusplus
extern "C" {
#endif

void nexus_init(void);
void nexus_process(void);
void nexus_process_buffer(uint8_t slave_id, uint8_t *buf, uint16_t len);
int  nexus_send_report(void);

#ifdef __cplusplus
}
#endif

#endif /* NEXUS_H_ */
