/*
 * Copyright (c) 2025 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef DIGITIZER_H
#define DIGITIZER_H

#include "stdint.h"
#include "keycode.h"
#include "keyboard_def.h"
#include "keyboard_conf.h"
 
#ifdef __cplusplus
extern "C" {
#endif
 
typedef struct __Digitizer
{
#ifdef DIGITIZER_SHARED_EP
    uint8_t report_id;
#endif
    bool     in_range : 1;
    bool     tip : 1;
    bool     barrel : 1;
    uint8_t  reserved : 5;
    uint16_t x;
    uint16_t y;
} __PACKED Digitizer;

#ifdef __cplusplus
}
#endif

#endif //DIGITIZER_H
 