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

static uint8_t packet_buffer[PACKET_BUFFER_CODE_NUM][PACKET_BUFFER_LENGTH];
static uint16_t packet_buffer_lengths[PACKET_BUFFER_CODE_NUM];
static uint8_t packet_buffer_flags;


int packet_buffer_push(const uint8_t*data,uint16_t length,uint8_t code)
{
    if (!BIT_GET(packet_buffer_flags,code))
    {
        memcpy(packet_buffer[code],data,length);
        packet_buffer_lengths[code] = length;
        BIT_SET(packet_buffer_flags, code);
        return 0;
    }
    else
    {
        return 1;
    }
}

int packet_buffer_flush(void)
{
    if (!packet_buffer_flags)
    {
        return 0;
    }
    for (size_t i = 0; i < PACKET_BUFFER_CODE_NUM; i++)
    {
        if (BIT_GET(packet_buffer_flags,i))
        {
#if defined(NEXUS_ENABLE) && NEXUS_IS_SLAVE
            if (!nexus_report(packet_buffer[i], packet_buffer_lengths[i]))
#else
            if (!hid_send_raw(packet_buffer[i], packet_buffer_lengths[i]))
#endif
            {
                BIT_RESET(packet_buffer_flags, i);
            }
            else
            {
                return 1;
            }
        }
    }
    return 0;
}