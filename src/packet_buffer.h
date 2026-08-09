/*
 * Copyright (c) 2026 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef PACKET_BUFFER_H
#define PACKET_BUFFER_H

#include "keyboard.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
  PACKET_BUFFER_CODE_EVENT = 0x00,
  PACKET_BUFFER_CODE_DATA = 0x01,
  PACKET_BUFFER_CODE_LARGE_DATA = 0x02,
  PACKET_BUFFER_CODE_CONSOLE = 0x03,
  PACKET_BUFFER_CODE_DEBUG = 0x04,
  PACKET_BUFFER_CODE_USER = 0x05,
  PACKET_BUFFER_CODE_NUM = 0x06,
};

#ifndef PACKET_BUFFER_LENGTH
#define PACKET_BUFFER_LENGTH 64
#endif

int packet_buffer_push(const uint8_t*data,uint16_t length,uint8_t code);
int packet_buffer_flush(void);

#ifdef __cplusplus
}
#endif

#endif //PACKET_BUFFER_H
