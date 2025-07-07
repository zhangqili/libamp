// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
/*
 * Copyright (c) 2025 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "lamp_array.h"
#include "keyboard.h"
#include "string.h"

static uint16_t current_lamp_id = 0;

uint16_t lamp_array_get_lamp_array_attributes_report(uint8_t* buffer) {
    LampArrayAttributesReport report = {
        REPORT_ID_LIGHTING_LAMP_ARRAY_ATTRIBUTES,
        RGB_NUM,
        RGB_WIDTH * KEY_SWITCH_DISTANCE,
        RGB_HEIGHT * KEY_SWITCH_DISTANCE,
        RGB_DEPTH * KEY_SWITCH_DISTANCE,
        LAMPARRAY_KIND,
        LAMPARRAY_UPDATE_INTERVAL
    };

    memcpy(buffer, &report, sizeof(LampArrayAttributesReport));

    return sizeof(LampArrayAttributesReport);
}

static inline Position get_lamp_position(uint16_t lamp_id)
{
    return (Position){g_rgb_locations[lamp_id].x * KEY_SWITCH_DISTANCE, 
                      g_rgb_locations[lamp_id].y * KEY_SWITCH_DISTANCE,
                      0};
}

uint16_t lamp_array_get_lamp_attributes_report(uint8_t* buffer) {
    LampAttributesResponseReport report = {
        REPORT_ID_LIGHTING_LAMP_ATTRIBUTES_REQUEST,
        current_lamp_id,                                        // LampId
        get_lamp_position(current_lamp_id),                     // Lamp position               
        LAMPARRAY_UPDATE_INTERVAL,                              // Lamp update interval
        LAMP_PURPOSE_CONTROL,                                   // Lamp purpose
        255,                                                    // Red level count
        255,                                                    // Blue level count
        255,                                                    // Green level count
        1,                                                      // Intensity
        1,                                                      // Is Programmable
        KEY_NO_EVENT                                            // InputBinding
    };

    memcpy(buffer, &report, sizeof(LampAttributesResponseReport));
    current_lamp_id = current_lamp_id + 1 >= RGB_NUM ?  current_lamp_id : current_lamp_id + 1;

    return sizeof(LampAttributesResponseReport);
}

static inline void lamp_set_color(uint16_t lamp_id, LampColor color)
{
    if (lamp_id >= RGB_NUM) {
        return;
    }
    rgb_set(lamp_id, color.red, color.green, color.blue);
}

void lamp_array_set_lamp_attributes_id(const uint8_t* buffer) {
    LampAttributesRequestReport* report = (LampAttributesRequestReport*) buffer;
    current_lamp_id = report->lamp_id;
}

void lamp_array_set_multiple_lamps(const uint8_t* buffer) {
    LampMultiUpdateReport* report = (LampMultiUpdateReport*) buffer;
    uint16_t last_id = report->lamp_ids[0];
    for (int i = 0; i <= report->lamp_count; i++)
    {
        //Deal with Microsoft's Dynamic Lighting's bugs
        if(report->lamp_ids[i] == 0 && last_id > 0)
        {
            break;
        }
        lamp_set_color(report->lamp_ids[i], report->colors[i]);
        last_id = report->lamp_ids[i];
    }
}

void lamp_array_set_lamp_range(const uint8_t* buffer) {
    LampRangeUpdateReport* report = (LampRangeUpdateReport*) buffer;
    if (report->lamp_id_start >= RGB_NUM)
    {
        return;
    }
    for (int i = report->lamp_id_start; i <= report->lamp_id_end && i < RGB_NUM; i++)
    {
        lamp_set_color(i, report->color);
    }
}

void lamp_array_set_autonomous_mode(const uint8_t* buffer) {
    LampArrayControlReport* report = (LampArrayControlReport*) buffer;
    g_rgb_hid_mode = !report->autonomous_mode;
}
