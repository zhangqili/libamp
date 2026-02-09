/*
 * Copyright (c) 2025 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "encoder.h"

#ifdef ENCODER_ENABLE
__WEAK Encoder g_encoders[ENCODER_NUM];

void encoder_input_delta(uint16_t id, int16_t delta)
{
    if (id<ENCODER_NUM)
    {
        g_encoders[id].delta = delta;
        g_encoders[id].count += delta;
        g_encoders[id].flag = ENCODER_TAP_DElAY;
    }
}

void encoder_input(uint16_t id, int32_t count)
{
    if (id<ENCODER_NUM)
    {
        g_encoders[id].delta = count - g_encoders[id].count;
        g_encoders[id].count = count;
        g_encoders[id].flag = ENCODER_TAP_DElAY;
    }
}

void encoder_process(void)
{
    for (int i = 0; i < ENCODER_NUM; i++)
    {
        if (g_encoders[i].flag)
        {
            g_encoders[i].flag--;
        }
        keyboard_key_update(keyboard_get_key(g_encoders[i].cw_id), g_encoders[i].flag&&g_encoders[i].delta>0);
        keyboard_key_update(keyboard_get_key(g_encoders[i].ccw_id), g_encoders[i].flag&&g_encoders[i].delta<0);
    }
}
#endif
