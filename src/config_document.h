/*
 * Copyright (c) 2026 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef AMP_CONFIG_DOCUMENT_H
#define AMP_CONFIG_DOCUMENT_H

#include "amp_protocol.h"
#include "advanced_key.h"
#include "dynamic_key.h"
#include "macro.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AMP_CONFIG_DOCUMENT_MAGIC 0x43504d41UL /* "AMPC", little endian */
#define AMP_CONFIG_SCHEMA_VERSION 1

typedef enum {
    AMP_CONFIG_SECTION_GENERAL       = 1,
    AMP_CONFIG_SECTION_ADVANCED_KEYS = 2,
    AMP_CONFIG_SECTION_KEYMAP        = 3,
    AMP_CONFIG_SECTION_RGB           = 4,
    AMP_CONFIG_SECTION_DYNAMIC_KEYS  = 5,
    AMP_CONFIG_SECTION_MACROS        = 6,
    AMP_CONFIG_SECTION_SCRIPT        = 7,
    AMP_CONFIG_SECTION_USER_BASE     = 0x8000,
} AmpConfigSectionType;

typedef struct {
    uint32_t magic;
    uint16_t schema_version;
    uint16_t profile_id;
    uint32_t section_count;
} __PACKED AmpConfigDocumentHeader;

typedef struct {
    uint16_t section_type;
    uint16_t section_version;
    uint32_t section_length;
} __PACKED AmpConfigSectionHeader;

typedef struct {
    uint16_t record_version;
    uint16_t record_size;
    uint32_t record_count;
} __PACKED AmpRecordArrayHeader;

typedef struct {
    uint16_t key_index;
    uint16_t dynamic_type;
    uint16_t record_version;
    uint16_t payload_len;
} __PACKED AmpDynamicKeyRecordHeader;

typedef struct {
    uint8_t flags;
    uint8_t reserved[3];
} __PACKED AmpConfigGeneral;

typedef struct {
    uint8_t mode;
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t secondary_r;
    uint8_t secondary_g;
    uint8_t secondary_b;
    int16_t speed;
    uint16_t direction;
    uint8_t density;
    uint8_t brightness;
} __PACKED AmpRgbBaseWire;

typedef struct {
    uint8_t mode;
    uint8_t r;
    uint8_t g;
    uint8_t b;
    int16_t speed;
} __PACKED AmpRgbWire;

typedef struct {
    uint32_t delay;
    uint16_t key_id;
    uint8_t is_virtual;
    uint8_t event;
    uint16_t keycode;
} __PACKED AmpMacroActionWire;

#define AMP_CONFIG_GENERAL_SECTION_SIZE \
    (sizeof(AmpConfigSectionHeader) + sizeof(AmpConfigGeneral))
#define AMP_CONFIG_ADVANCED_SECTION_SIZE \
    (sizeof(AmpConfigSectionHeader) + sizeof(AmpRecordArrayHeader) + \
     sizeof(AdvancedKeyConfiguration) * ADVANCED_KEY_NUM)
#define AMP_CONFIG_KEYMAP_SECTION_SIZE \
    (sizeof(AmpConfigSectionHeader) + sizeof(AmpRecordArrayHeader) + \
     sizeof(uint16_t) * LAYER_NUM * TOTAL_KEY_NUM)
#ifdef RGB_ENABLE
#define AMP_CONFIG_RGB_SECTION_SIZE \
    (sizeof(AmpConfigSectionHeader) + sizeof(AmpRgbBaseWire) + \
     sizeof(AmpRecordArrayHeader) + sizeof(AmpRgbWire) * RGB_NUM)
#else
#define AMP_CONFIG_RGB_SECTION_SIZE 0
#endif
#ifdef DYNAMICKEY_ENABLE
#define AMP_CONFIG_DYNAMIC_SECTION_SIZE \
    (sizeof(AmpConfigSectionHeader) + \
     (sizeof(AmpDynamicKeyRecordHeader) + sizeof(DynamicKey)) * DYNAMIC_KEY_NUM)
#else
#define AMP_CONFIG_DYNAMIC_SECTION_SIZE 0
#endif
#ifdef MACRO_ENABLE
#define AMP_CONFIG_MACRO_SECTION_SIZE \
    (sizeof(AmpConfigSectionHeader) + sizeof(AmpRecordArrayHeader) + \
     sizeof(AmpMacroActionWire) * MACRO_NUM * MACRO_MAX_ACTIONS)
#else
#define AMP_CONFIG_MACRO_SECTION_SIZE 0
#endif

#define AMP_CONFIG_DOCUMENT_MAX_SIZE \
    (sizeof(AmpConfigDocumentHeader) + AMP_CONFIG_GENERAL_SECTION_SIZE + \
     AMP_CONFIG_ADVANCED_SECTION_SIZE + AMP_CONFIG_KEYMAP_SECTION_SIZE + \
     AMP_CONFIG_RGB_SECTION_SIZE + AMP_CONFIG_DYNAMIC_SECTION_SIZE + \
     AMP_CONFIG_MACRO_SECTION_SIZE)

STATIC_ASSERT(sizeof(AdvancedKeyConfiguration) == 22,
              "AdvancedKeyConfiguration wire version 1 must be 22 bytes");
STATIC_ASSERT(sizeof(DynamicKey) == 56,
              "DynamicKey wire version 1 must be 56 bytes");

int amp_config_document_write(const char *path, uint16_t profile_id);
AmpStatus amp_config_document_validate(const char *path, uint16_t profile_id);
AmpStatus amp_config_document_apply(const char *path, uint16_t profile_id);

#ifdef __cplusplus
}
#endif

#endif /* AMP_CONFIG_DOCUMENT_H */
