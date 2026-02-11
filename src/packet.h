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
  PACKET_CODE_ACTION = 0x00,
  PACKET_CODE_SET = 0x01,
  PACKET_CODE_GET = 0x02,
  PACKET_CODE_LOG = 0x03,
  PACKET_CODE_USER = 0xFF,
};

enum {
  PACKET_DATA_ADVANCED_KEY = 0x00,
  PACKET_DATA_KEYMAP = 0x01,
  PACKET_DATA_RGB_BASE_CONFIG = 0x02,
  PACKET_DATA_RGB_CONFIG = 0x03,
  PACKET_DATA_DYNAMIC_KEY = 0x04,
  PACKET_DATA_CONFIG_INDEX = 0x05,
  PACKET_DATA_CONFIG = 0x06,
  PACKET_DATA_DEBUG = 0x07,
  PACKET_DATA_REPORT = 0x08,
  PACKET_DATA_VERSION = 0x09,
  PACKET_DATA_MACRO = 0x0A,
};

typedef struct __PacketBase
{
  uint8_t code;
  uint8_t buf[];
} __PACKED PacketBase;

typedef struct __PacketCommand
{
  uint8_t code;
} __PACKED PacketCommand;

typedef struct __PacketData
{
  uint8_t code;
  uint8_t type;
} __PACKED PacketData;

typedef struct __PacketAdvancedKey
{
  uint8_t code;
  uint8_t type;
  uint16_t index;
  AdvancedKeyConfigurationNormalized data;
} PacketAdvancedKey;

typedef struct __PacketRGBBaseConfig
{
  uint8_t code;
  uint8_t type;
  uint8_t mode;
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t secondary_r;
  uint8_t secondary_g;
  uint8_t secondary_b;
  float speed;
  uint16_t direction;
  uint8_t density;
  uint8_t brightness;
} __PACKED PacketRGBBaseConfig;

typedef struct __PacketRGBConfig
{
  uint8_t code;
  uint8_t type;
  uint16_t index;
  uint8_t mode;
  uint8_t r;
  uint8_t g;
  uint8_t b;
  float speed;
} __PACKED PacketRGBConfig;

typedef struct __PacketRGBConfigs
{
  uint8_t code;
  uint8_t type;
  uint8_t length;
  struct
  {
    uint16_t index;
    uint8_t mode;
    uint8_t r;
    uint8_t g;
    uint8_t b;
    float speed;
  } __PACKED data[];
} __PACKED PacketRGBConfigs;

typedef struct __PacketKeymap
{
  uint8_t code;
  uint8_t type;
  uint8_t layer;
  uint16_t start;
  uint8_t length;
  uint16_t keymap[];
} __PACKED PacketKeymap;

typedef struct __PacketDynamicKey
{
  uint8_t code;
  uint8_t type;
  uint8_t index;
  uint8_t reserved;
  uint8_t dynamic_key[];
} __PACKED PacketDynamicKey;

typedef struct __PacketConfigIndex
{
  uint8_t code;
  uint8_t type;
  uint8_t index;
} __PACKED PacketConfigIndex;


typedef struct __PacketConfig
{
  uint8_t code;
  uint8_t type;
  uint8_t length;
  uint8_t reserved;
  struct
  {
    uint8_t index;
    uint8_t value;
  } __PACKED data[];
} __PACKED PacketConfig;

typedef struct __PacketDebug
{
  uint8_t code;
  uint8_t type;
  uint8_t length;
  struct
  {
    uint16_t index;
    uint8_t state;
    uint8_t report_state;
    float raw;
    float value;
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

typedef struct __PacketVersion
{
  uint8_t code;
  uint8_t type;
  uint16_t info_length;
  uint32_t major;
  uint32_t minor;
  uint32_t patch;
  uint8_t info[];
} __PACKED PacketVersion;

typedef struct __PacketMacro
{
  uint8_t code;
  uint8_t type;
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

typedef struct __PacketLog
{
  uint8_t code;
  uint8_t level;
  uint16_t length;
  uint8_t data[];
} __PACKED PacketLog;

void packet_process_buffer(uint8_t *buf, uint16_t len);
void packet_process(uint8_t *buf, uint16_t len);
void packet_process_advanced_key(PacketData*data);
void packet_process_rgb_base_config(PacketData*data);
void packet_process_rgb_config(PacketData*data);
void packet_process_keymap(PacketData*data);
void packet_process_dynamic_key(PacketData*data);
void packet_process_config_index(PacketData*data);
void packet_process_config(PacketData*data);
void packet_process_debug(PacketData*data);
void packet_process_macro(PacketData*data);
void packet_process_user(uint8_t *buf, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif //PACKET_H
