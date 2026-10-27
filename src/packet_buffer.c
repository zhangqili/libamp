/*
 * Copyright (c) 2026 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "packet_buffer.h"
#include "driver.h"

#ifdef NEXUS_ENABLE
#include "nexus.h"
#endif

#include "string.h"

/* One outgoing packet per code, waiting for keyboard_task() to send it. A slot
 * is filled from whatever context produced the packet (USB or Nexus receive
 * interrupt, the tick, the main loop) and emptied from the tick, so each slot
 * has its own flag written with a single store, never a read-modify-write of
 * a shared byte that another context could interrupt. */
static uint8_t packet_buffer[PACKET_BUFFER_CODE_NUM][PACKET_BUFFER_LENGTH];
static uint16_t packet_buffer_lengths[PACKET_BUFFER_CODE_NUM];
static volatile bool packet_buffer_pending[PACKET_BUFFER_CODE_NUM];

int packet_buffer_push(const uint8_t *data, uint16_t length, uint8_t code)
{
    if (code >= PACKET_BUFFER_CODE_NUM || length > PACKET_BUFFER_LENGTH || packet_buffer_pending[code])
    {
        return 1;
    }
    memcpy(packet_buffer[code], data, length);
    packet_buffer_lengths[code] = length;
    COMPILER_FENCE(); /* the packet is complete before it is announced */
    packet_buffer_pending[code] = true;
    return 0;
}

int packet_buffer_flush(void)
{
    for (uint8_t code = 0; code < PACKET_BUFFER_CODE_NUM; code++)
    {
        if (!packet_buffer_pending[code])
        {
            continue;
        }
#if defined(NEXUS_ENABLE) && NEXUS_IS_SLAVE
        if (nexus_report(packet_buffer[code], packet_buffer_lengths[code]) != 0)
#else
        if (hid_send_raw(packet_buffer[code], packet_buffer_lengths[code]) != 0)
#endif
        {
            return 1; /* transport busy: keep the packet, try again next tick */
        }
        COMPILER_FENCE(); /* the transport has copied the packet */
        packet_buffer_pending[code] = false;
    }
    return 0;
}
