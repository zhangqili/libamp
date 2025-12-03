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

#ifndef NEXUS_SLAVE_NUM
#define NEXUS_SLAVE_NUM 1
#endif

#ifndef NEXUS_SLICE_LENGTH_MAX
#define NEXUS_SLICE_LENGTH_MAX 16
#endif

#ifndef NEXUS_VALUE_MAX
#define NEXUS_VALUE_MAX 65535
#endif

typedef struct __PacketNexus
{
  int8_t index;
#if NEXUS_SLICE_LENGTH_MAX >= 128
  int8_t index_high;
#endif
#if NEXUS_VALUE_MAX == 0
  // No value field
#elif NEXUS_VALUE_MAX < 256
  uint8_t value;
#else
  uint16_t value;
#endif
  uint16_t raw;
  uint8_t bits[(NEXUS_SLICE_LENGTH_MAX + 7) / 8];
} __PACKED PacketNexus;

typedef struct __NexusSlaveConfig
{
    uint16_t length;
    const uint16_t *map;
} NexusSlaveConfig;

void nexus_init(void);
void nexus_process(void);
void nexus_process_buffer(uint8_t slave_id, uint8_t *buf, uint16_t len);
int  nexus_send_report(void);

#ifdef __cplusplus
}
#endif

#endif /* NEXUS_H_ */
