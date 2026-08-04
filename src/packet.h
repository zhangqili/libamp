/*
 * Copyright (c) 2025 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef PACKET_H
#define PACKET_H

#include "keyboard.h"
#include "storage.h"
#include "amp_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    PACKET_CODE_EVENT = 0x00,
    PACKET_CODE_SET = 0x01,
    PACKET_CODE_GET = 0x02,
    PACKET_CODE_LOG = 0x03,
    PACKET_CODE_LARGE_SET = 0x04,
    PACKET_CODE_LARGE_GET = 0x05,
    PACKET_CODE_USER = 0xFF,
};

enum {
    PACKET_DATA_VERSION = 0x00,
    PACKET_DATA_ADVANCED_KEY = 0x01,
    PACKET_DATA_KEYMAP = 0x02,
    PACKET_DATA_RGB_BASE_CONFIG = 0x03,
    PACKET_DATA_RGB_CONFIG = 0x04,
    PACKET_DATA_DYNAMIC_KEY = 0x05,
    PACKET_DATA_PROFILE_INDEX = 0x06,
    PACKET_DATA_CONFIG = 0x07,
    PACKET_DATA_DEBUG = 0x08,
    PACKET_DATA_REPORT = 0x09,
    PACKET_DATA_MACRO = 0x0A,
    PACKET_DATA_FEATURE = 0x0B,
    PACKET_DATA_SCRIPT_SCOURCE = 0x0C,
    PACKET_DATA_SCRIPT_BYTECODE = 0x0D,
};

enum {
    LARGE_DATA_CMD_START = 0,
    LARGE_DATA_CMD_PAYLOAD = 1,
    LARGE_DATA_CMD_END = 2,
    LARGE_DATA_CMD_ABORT = 3,
};

#define PACKET_ADVANCED_KEY_ITEMS 2
#define PACKET_KEYMAP_ITEMS 27
#define PACKET_RGB_ITEMS 7
#define PACKET_CONFIG_ITEMS 28
#define PACKET_DEBUG_ITEMS 5
#define PACKET_MACRO_ITEMS 4
#define PACKET_LARGE_CHUNK_SIZE 52
#define PACKET_CONSOLE_DATA_SIZE 57
#define PACKET_DYNAMIC_KEY_DATA_OFFSET sizeof(uint16_t)

typedef struct {
    uint8_t event;
    uint16_t keycode;
    uint16_t id;
    uint8_t is_virtual;
    uint8_t use_keymap;
    uint8_t reserved[51];
} __PACKED PacketEvent;

typedef struct {
    uint8_t count;
    struct {
        uint16_t index;
        AdvancedKeyConfiguration config;
    } __PACKED items[PACKET_ADVANCED_KEY_ITEMS];
    uint8_t reserved[9];
} __PACKED PacketAdvancedKeys;

typedef struct {
    uint8_t layer;
    uint16_t start;
    uint8_t count;
    uint16_t keycodes[PACKET_KEYMAP_ITEMS];
} __PACKED PacketKeymap;

typedef struct {
    uint8_t mode;
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t secondary_r;
    uint8_t secondary_g;
    uint8_t secondary_b;
    uint16_t speed;
    uint16_t direction;
    uint8_t density;
    uint8_t brightness;
    uint8_t reserved[45];
} __PACKED PacketRgbBaseConfig;

typedef struct {
    uint8_t count;
    struct {
        uint16_t index;
        uint8_t mode;
        uint8_t r;
        uint8_t g;
        uint8_t b;
        uint16_t speed;
    } __PACKED  items[PACKET_RGB_ITEMS];
    uint8_t reserved[1];
} __PACKED PacketRgbItems;

typedef struct {
    uint8_t index;
    uint8_t reserved[57];
} __PACKED PacketProfileIndex;

typedef struct {
    uint8_t count;
    struct {
        uint8_t index;
        uint8_t value;
    } __PACKED  items[PACKET_CONFIG_ITEMS];
    uint8_t reserved[1];
} __PACKED PacketConfig;

typedef struct {
    uint8_t count;
    uint32_t tick;
    struct {
        uint16_t index;
        uint8_t state;
        uint8_t report_state;
        uint16_t value;
        uint16_t raw;
        uint16_t filtered_raw;
    } __PACKED items[PACKET_DEBUG_ITEMS];
    uint8_t reserved[3];
} __PACKED PacketDebug;

typedef struct {
    uint8_t macro_index;
    uint8_t count;
    struct {
        uint32_t delay;
        uint16_t index;
        uint16_t key_id;
        uint8_t is_virtual;
        uint8_t event;
        uint16_t keycode;
    } __PACKED  items[PACKET_MACRO_ITEMS];
    uint8_t reserved[8];
} __PACKED PacketMacro;

typedef struct {
    uint16_t major;
    uint16_t minor;
    uint16_t patch;
    uint8_t info_length;
    uint8_t info[51];
} __PACKED PacketVersion;

typedef struct {
    uint8_t length;
    uint8_t data[PACKET_CONSOLE_DATA_SIZE];
} __PACKED PacketConsole;

typedef struct {
    uint8_t sub_cmd;
    uint32_t total_size;
    uint8_t reserved[53];
} __PACKED PacketLargeStart;

typedef struct {
    uint8_t sub_cmd;
    uint32_t offset;
    uint8_t chunk_length;
    uint8_t data[PACKET_LARGE_CHUNK_SIZE];
} __PACKED PacketLargePayload;

typedef struct {
    uint8_t sub_cmd;
    uint8_t reserved[57];
} __PACKED PacketLargeControl;

typedef union {
    uint8_t sub_cmd;
    PacketLargeStart start;
    PacketLargePayload payload;
    PacketLargeControl control;
    uint8_t raw[AMP_FRAME_BODY_SIZE];
} __PACKED PacketLarge;

#define AMP_PACKET_ASSERT(type) STATIC_ASSERT(sizeof(type) == AMP_FRAME_BODY_SIZE, #type " must be 58 bytes")

AMP_PACKET_ASSERT(PacketEvent);
AMP_PACKET_ASSERT(PacketAdvancedKeys);
AMP_PACKET_ASSERT(PacketKeymap);
AMP_PACKET_ASSERT(PacketRgbBaseConfig);
AMP_PACKET_ASSERT(PacketRgbItems);
AMP_PACKET_ASSERT(PacketProfileIndex);
AMP_PACKET_ASSERT(PacketConfig);
AMP_PACKET_ASSERT(PacketDebug);
AMP_PACKET_ASSERT(PacketMacro);
AMP_PACKET_ASSERT(PacketVersion);
AMP_PACKET_ASSERT(PacketConsole);
AMP_PACKET_ASSERT(PacketLargeStart);
AMP_PACKET_ASSERT(PacketLargePayload);
AMP_PACKET_ASSERT(PacketLargeControl);
AMP_PACKET_ASSERT(PacketLarge);

STATIC_ASSERT(PACKET_DYNAMIC_KEY_DATA_OFFSET + sizeof(DynamicKey) == AMP_FRAME_BODY_SIZE,
               "DynamicKey must fill the remaining Amp packet body");

#undef AMP_PACKET_ASSERT

void packet_process_frame(const AmpFrame *frame);
bool packet_process_frame_to_report(const AmpFrame *frame, uint8_t channel,
                                    uint8_t flags, uint8_t report[AMP_FRAME_REPORT_SIZE]);

void packet_send_version_packet(void);
void packet_process_version_notifications(void);
void packet_send_debug_packet(void);

AmpStatus packet_process_user(uint8_t code, uint8_t type,
                              const uint8_t request[AMP_FRAME_BODY_SIZE],
                              uint8_t response[AMP_FRAME_BODY_SIZE]);
AmpStatus large_packet_process(const AmpFrame *request, AmpFrame *response);

#ifdef __cplusplus
}
#endif

#endif // PACKET_H
