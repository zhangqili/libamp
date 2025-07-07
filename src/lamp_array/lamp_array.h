// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
/*
 * Copyright (c) 2025 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef LAMP_ARRAY_H_
#define LAMP_ARRAY_H_
#include "rgb.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef LAMPARRAY_KIND
#define LAMPARRAY_KIND LAMPARRAY_KIND_KEYBOARD
#endif

#ifndef LAMPARRAY_UPDATE_INTERVAL
#define LAMPARRAY_UPDATE_INTERVAL 10000
#endif

typedef struct 
{
    uint8_t red;
    uint8_t green;
    uint8_t blue; 
    uint8_t intensity; 
} __PACKED LampColor;

typedef struct 
{
    uint32_t x;
    uint32_t y;
    uint32_t z;
} __PACKED Position;

typedef struct 
{
    uint8_t report_id;
    uint16_t lamp_count;
    
    uint32_t width;
    uint32_t height;
    uint32_t depth;

    uint32_t lamp_array_kind;
    uint32_t min_update_interval;
} __PACKED LampArrayAttributesReport;

typedef struct 
{
    uint8_t report_id;
    uint16_t lamp_id;
} __PACKED LampAttributesRequestReport;

enum {
    LAMPARRAY_KIND_UNDEFINED,
    LAMPARRAY_KIND_KEYBOARD,
    LAMPARRAY_KIND_MOUSE,
    LAMPARRAY_KIND_GAME_CONTROLLER,
    LAMPARRAY_KIND_PERIPHERAL,
    LAMPARRAY_KIND_SCENE,
    LAMPARRAY_KIND_NOTIFICATION,
    LAMPARRAY_KIND_CHASSIS,
    LAMPARRAY_KIND_WEARABLE,
    LAMPARRAY_KIND_FURNITURE,
    LAMPARRAY_KIND_ART,
    LAMPARRAY_KIND_HEADSET,
    LAMPARRAY_KIND_MICROPHONE,
    LAMPARRAY_KIND_SPEAKER,
};

enum {
    LAMP_PURPOSE_CONTROL      = 0x01,
    LAMP_PURPOSE_ACCENT       = 0x02,
    LAMP_PURPOSE_BRANDING     = 0x04,
    LAMP_PURPOSE_STATUS       = 0x08,
    LAMP_PURPOSE_ILLUMINATION = 0x10,
    LAMP_PURPOSE_PRESENTATION = 0x20,
};

typedef struct 
{
    uint8_t report_id;
    uint16_t lamp_id;

    Position lamp_position;

    uint32_t update_latency;
    uint32_t lamp_purposes;

    uint8_t red_level_count;
    uint8_t green_level_count;
    uint8_t blue_level_count;
    uint8_t intensity_level_count;

    uint8_t is_programmable;
    uint8_t input_binding;
} __PACKED LampAttributesResponseReport;

typedef struct 
{
    uint8_t report_id;
    uint8_t lamp_count;
    uint8_t flags;
    uint16_t lamp_ids[8];

    LampColor colors[8];
} __PACKED LampMultiUpdateReport;

typedef struct 
{
    uint8_t report_id;
    uint8_t flags;
    uint16_t lamp_id_start;
    uint16_t lamp_id_end;

    LampColor color;
} __PACKED LampRangeUpdateReport;

typedef struct 
{
    uint8_t report_id;
    uint8_t autonomous_mode;
} __PACKED LampArrayControlReport;

uint16_t lamp_array_get_lamp_array_attributes_report(uint8_t* buffer);
uint16_t lamp_array_get_lamp_attributes_report(uint8_t* buffer);

void lamp_array_set_lamp_attributes_id(const uint8_t* buffer);
void lamp_array_set_multiple_lamps(const uint8_t* buffer);
void lamp_array_set_lamp_range(const uint8_t* buffer);
void lamp_array_set_autonomous_mode(const uint8_t* buffer);

#ifdef __cplusplus
}
#endif

#endif /* LAMP_ARRAY_H_ */
