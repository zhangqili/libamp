/*
 * Copyright (c) 2025 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef PROGRAMMABLE_BUTTON_H
#define PROGRAMMABLE_BUTTON_H

#include "keyboard.h"
 
#ifdef __cplusplus
extern "C" {
#endif

typedef struct __ProgrammableButton
{
    uint8_t  report_id;
    uint32_t usage;
} __PACKED ProgrammableButton;

#ifdef __cplusplus
}
#endif

#endif //PROGRAMMABLE_BUTTON_H
  