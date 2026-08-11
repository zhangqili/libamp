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
  PACKET_BUFFER_CODE_RESPONSE,
  PACKET_BUFFER_CODE_EVENT,
  PACKET_BUFFER_CODE_CONSOLE,
  PACKET_BUFFER_CODE_DEBUG,
  PACKET_BUFFER_CODE_NUM,
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
