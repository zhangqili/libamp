/*
 * Copyright (c) 2025 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef PACKET_H
#define PACKET_H

#include "keyboard.h"
#include "storage.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
  PACKET_CODE_EVENT = 0x00,
  PACKET_CODE_SET = 0x01,
  PACKET_CODE_GET = 0x02,
  PACKET_CODE_CONSOLE = 0x03,
  PACKET_CODE_LARGE_SET = 0x04,
  PACKET_CODE_LARGE_GET = 0x05,
  PACKET_CODE_DEBUG = 0x06,
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
  //PACKET_DATA_DEBUG = 0x08,
  //PACKET_DATA_REPORT = 0x09,
  PACKET_DATA_MACRO = 0x0A,
  PACKET_DATA_FEATURE = 0x0B,
  PACKET_DATA_SCRIPT_SCOURCE = 0x0C,
  PACKET_DATA_SCRIPT_BYTECODE = 0x0D,
  PACKET_DATA_RECORD = 0x0E,
  PACKET_DATA_LAYOUT_OPTIONS = 0x0F,
};

typedef struct __PacketBase
{
  uint8_t code;
  uint8_t buf[];
} __PACKED PacketBase;

enum {
  PACKET_EVENT_NO_EVENT = 0x00,
  PACKET_EVENT_CONFIG_CHANGED = 0x01,
};

typedef struct __PacketEvent
{
  uint8_t code;
  uint8_t flag;
  uint8_t event;
  uint16_t keycode;
  uint16_t id;
  uint8_t is_virtual;
  uint8_t use_keymap;
} __PACKED PacketEvent;

typedef struct __PacketDataHeader
{
  uint8_t code;
  uint8_t id;
  uint8_t type;
} __PACKED PacketDataHeader;

typedef struct __PacketVersion
{
  uint8_t code;
  uint8_t id;
  uint8_t type;
  uint16_t info_length;
  uint32_t major;
  uint32_t minor;
  uint32_t patch;
  uint8_t info[];
} __PACKED PacketVersion;

typedef struct __PacketAdvancedKey
{
  PacketDataHeader header;
  uint16_t index;
  AdvancedKeyConfiguration data;
} __PACKED PacketAdvancedKey;

typedef struct __PacketKeymap
{
  PacketDataHeader header;
  uint8_t layer;
  uint16_t start;
  uint8_t length;
  uint16_t keymap[];
} __PACKED PacketKeymap;

typedef struct __PacketRGBBaseConfig
{
  PacketDataHeader header;
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
} __PACKED PacketRGBBaseConfig;

typedef struct __PacketRGBConfigs
{
  PacketDataHeader header;
  uint8_t length;
  struct
  {
    uint16_t index;
    uint8_t mode;
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint16_t speed;
  } __PACKED data[];
} __PACKED PacketRGBConfigs;

typedef struct __PacketDynamicKey
{
  PacketDataHeader header;
  uint8_t index;
  uint8_t reserved;
  uint8_t dynamic_key[];
} __PACKED PacketDynamicKey;

typedef struct __PacketProfileIndex
{
  PacketDataHeader header;
  uint8_t index;
} __PACKED PacketProfileIndex;


typedef struct __PacketConfig
{
  PacketDataHeader header;
  uint8_t length;
  uint8_t reserved;
  struct
  {
    uint8_t index;
    uint8_t value;
  } __PACKED data[];
} __PACKED PacketConfig;

typedef struct __PacketMacro
{
  PacketDataHeader header;
  uint8_t macro_index;
  uint16_t length;
  struct
  {
    uint32_t delay;
    uint16_t index;
    uint16_t key_id;
    uint8_t is_virtual;
    uint8_t event;
    uint16_t keycode;
  } __PACKED data[];
} __PACKED PacketMacro;

typedef struct __PacketFeature
{
  PacketDataHeader header;
  uint32_t features;
  uint32_t rgb_features;
  uint8_t script_support;
} __PACKED PacketFeature;

typedef struct __PacketLargeData
{
    PacketDataHeader data_header;
    uint8_t sub_cmd;

    union
    {
        struct
        {
            uint32_t total_size;
            uint32_t checksum;
        } __PACKED header;
        struct
        {
            uint32_t offset;
            uint16_t length;
            uint8_t  data[];
        } __PACKED payload;
    };
} __PACKED PacketLargeData;


enum {
  PACKET_DATA_RECORD_RUNTIME = 0x00,
  PACKET_DATA_RECORD_KEYCOUNT = 0x01,
};
typedef struct __PacketRecord
{
  PacketDataHeader header;
  uint8_t sub_cmd;
  uint8_t data[];
} __PACKED PacketRecord;

typedef struct __PacketRecordRuntime
{
  PacketDataHeader header;
  uint8_t sub_cmd;
  uint64_t uptime;
  uint64_t runtime;
} __PACKED PacketRecordRuntime;

typedef struct __PacketRecordKeyCount
{
  PacketDataHeader header;
  uint8_t sub_cmd;
  uint16_t length;
  struct
  {
    uint16_t key_index;
    uint32_t count;
  } __PACKED data[];
} __PACKED PacketRecordKeyCount;

typedef struct __PacketDebug
{
  uint8_t code;
  uint8_t length;
  uint32_t tick;
  struct
  {
    uint16_t index;
    uint8_t state;
    uint8_t report_state;
    uint16_t raw;
    uint16_t filtered_raw;
    uint16_t value;
  } __PACKED data[];
} __PACKED PacketDebug;

typedef struct __PacketReport
{
  uint8_t code;
  uint8_t type;
  uint8_t report_type;
  uint8_t length;
  uint8_t data[];
} __PACKED PacketReport;

typedef struct __PacketLog
{
  uint8_t code;
  uint8_t reserved;
  uint16_t length;
  uint8_t data[];
} __PACKED PacketLog;

typedef struct __PacketLayoutOptions
{
  PacketDataHeader header;
  uint64_t layout_options;
} __PACKED PacketLayoutOptions;

void packet_process_buffer(uint8_t *buf, uint16_t len);
void packet_process(uint8_t *buf, uint16_t len);
void packet_process_advanced_key(PacketDataHeader*data);
void packet_process_rgb_base_config(PacketDataHeader*data);
void packet_process_rgb_config(PacketDataHeader*data);
void packet_process_keymap(PacketDataHeader*data);
void packet_process_dynamic_key(PacketDataHeader*data);
void packet_process_profile_index(PacketDataHeader*data);
void packet_process_config(PacketDataHeader*data);
void packet_process_debug(PacketDebug*data);
void packet_fill_debug(PacketDebug*data);
void packet_process_macro(PacketDataHeader*data);
void packet_process_feature(PacketDataHeader*data);
void packet_process_record(PacketDataHeader*data);
void packet_process_layout_options(PacketDataHeader*data);

void packet_send_version_packet(void);
void packet_notify_event(uint8_t packet_event);
void packet_send_debug_packet(void);
void packet_process_user(uint8_t *buf, uint16_t len);
void large_packet_process(PacketLargeData *buf);

#ifdef __cplusplus
}
#endif

#endif //PACKET_H
