/*
 * Copyright (c) 2026 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef AMP_PACKET_H
#define AMP_PACKET_H

#include "amp_protocol.h"
#include "keyboard.h"
#include "macro.h"
#include "storage.h"

#define PACKET_CONSOLE_DATA_SIZE (AMP_FRAME_PAYLOAD_SIZE - sizeof(uint32_t))

#ifndef AMP_OBJECT_CRC32_ENABLE
#define AMP_OBJECT_CRC32_ENABLE 1
#endif

#ifndef AMP_OBJECT_TEMP_FILE_ENABLE
#define AMP_OBJECT_TEMP_FILE_ENABLE 1
#endif

#if AMP_OBJECT_CRC32_ENABLE != 0 && AMP_OBJECT_CRC32_ENABLE != 1
#error "AMP_OBJECT_CRC32_ENABLE must be 0 or 1"
#endif

#if AMP_OBJECT_TEMP_FILE_ENABLE != 0 && AMP_OBJECT_TEMP_FILE_ENABLE != 1
#error "AMP_OBJECT_TEMP_FILE_ENABLE must be 0 or 1"
#endif

#ifdef __cplusplus
extern "C" {
#endif

enum {
    AMP_CONTROL_HELLO     = 0x0001,
    AMP_CONTROL_KEY_EVENT = 0x0002,
};

enum {
    AMP_CONFIG_ACTIVATE_PROFILE       = 0x0001,
    AMP_CONFIG_ACTIVE_PROFILE_CHANGED = 0x8001,
    AMP_CONFIG_PROFILE_CHANGED        = 0x8002,
};

enum {
    AMP_OBJECT_OPEN_READ  = 0x0001,
    AMP_OBJECT_READ       = 0x0002,
    AMP_OBJECT_CLOSE_READ = 0x0003,
    AMP_OBJECT_OPEN_WRITE = 0x0004,
    AMP_OBJECT_WRITE      = 0x0005,
    AMP_OBJECT_COMMIT     = 0x0006,
    AMP_OBJECT_ABORT      = 0x0007,
};

enum {
    AMP_DEBUG_SUBSCRIBE   = 0x0001,
    AMP_DEBUG_UNSUBSCRIBE = 0x0002,
    AMP_DEBUG_SAMPLE      = 0x0003,
    AMP_DEBUG_DATA        = 0x8001,
};

enum {
    AMP_CONSOLE_SUBSCRIBE   = 0x0001,
    AMP_CONSOLE_UNSUBSCRIBE = 0x0002,
    AMP_CONSOLE_DATA        = 0x8001,
};

typedef enum {
    AMP_OBJECT_CONFIG_PROFILE  = 1,
    AMP_OBJECT_SCRIPT_SOURCE   = 2,
    AMP_OBJECT_SCRIPT_BYTECODE = 3,
    AMP_OBJECT_USER_BASE       = 0x8000,
} AmpObjectType;

enum {
    AMP_CAP_CONFIG_OBJECT = 1U << 0,
    AMP_CAP_SCRIPT_SOURCE = 1U << 1,
    AMP_CAP_SCRIPT_BYTECODE = 1U << 2,
    AMP_CAP_DEBUG_STREAM = 1U << 3,
    AMP_CAP_CONSOLE_STREAM = 1U << 4,
    AMP_CAP_OBJECT_CRC32 = 1U << 5,
    AMP_CAP_OBJECT_ATOMIC_COMMIT = 1U << 6,
};

typedef struct {
    uint16_t max_rx_payload;
    uint16_t max_tx_payload;
    uint32_t capabilities;
} __PACKED AmpHelloRequest;

typedef struct {
    uint16_t max_rx_payload;
    uint16_t max_tx_payload;
    uint32_t capabilities;
    uint32_t device_state_revision;
    uint16_t active_profile;
    uint16_t profile_count;
    uint16_t firmware_major;
    uint16_t firmware_minor;
    uint16_t firmware_patch;
    uint16_t max_inflight_requests;
    uint16_t advanced_key_count;
    uint16_t total_key_count;
    uint16_t layer_count;
    uint16_t dynamic_key_count;
    uint16_t macro_count;
    uint16_t macro_action_count;
    uint16_t rgb_count;
    uint8_t firmware_info_length;
    uint8_t firmware_info[13];
} __PACKED AmpHelloResponse;

typedef struct {
    uint8_t event;
    uint16_t keycode;
    uint16_t key_id;
    uint8_t is_virtual;
    uint8_t use_keymap;
} __PACKED AmpKeyEvent;

typedef struct {
    uint16_t profile_id;
} __PACKED AmpActivateProfileRequest;

typedef struct {
    uint16_t active_profile;
    uint32_t device_state_revision;
} __PACKED AmpActivateProfileResponse;

typedef struct {
    uint16_t profile_id;
    uint32_t device_state_revision;
    uint8_t reason;
} __PACKED AmpActiveProfileChangedEvent;

typedef struct {
    uint16_t profile_id;
    uint32_t profile_revision;
} __PACKED AmpProfileChangedEvent;

typedef struct {
    uint16_t object_type;
    uint16_t object_id;
} __PACKED AmpObjectOpenReadRequest;

typedef struct {
    uint16_t transaction_id;
    uint32_t revision;
    uint32_t total_size;
    uint32_t crc32;
} __PACKED AmpObjectOpenReadResponse;

typedef struct {
    uint16_t transaction_id;
    uint32_t offset;
    uint16_t requested_length;
} __PACKED AmpObjectReadRequest;

typedef struct {
    uint32_t offset;
    uint8_t data[AMP_FRAME_PAYLOAD_SIZE - sizeof(uint32_t)];
} __PACKED AmpObjectReadResponse;

typedef struct {
    uint16_t transaction_id;
} __PACKED AmpObjectTransactionRequest;

typedef struct {
    uint16_t object_type;
    uint16_t object_id;
    uint32_t expected_revision;
    uint32_t total_size;
    uint32_t crc32;
} __PACKED AmpObjectOpenWriteRequest;

typedef struct {
    uint16_t transaction_id;
} __PACKED AmpObjectOpenWriteResponse;

typedef struct {
    uint16_t transaction_id;
    uint32_t offset;
    uint8_t data[AMP_FRAME_PAYLOAD_SIZE - sizeof(uint16_t) - sizeof(uint32_t)];
} __PACKED AmpObjectWriteRequest;

typedef struct {
    uint32_t new_revision;
} __PACKED AmpObjectCommitResponse;

typedef struct {
    uint16_t index;
    uint8_t state;
    uint8_t report_state;
    uint16_t value;
    uint16_t raw;
    uint16_t filtered_raw;
} __PACKED AmpDebugItem;

typedef struct {
    uint8_t count;
    uint16_t indices[(AMP_FRAME_PAYLOAD_SIZE - 1) / sizeof(uint16_t)];
} __PACKED AmpDebugRequest;

typedef struct {
    uint32_t sequence;
    uint32_t tick;
    uint8_t count;
    AmpDebugItem items[(AMP_FRAME_PAYLOAD_SIZE - 9) / sizeof(AmpDebugItem)];
} __PACKED AmpDebugData;

STATIC_ASSERT(sizeof(AmpHelloResponse) <= AMP_FRAME_PAYLOAD_SIZE,
              "HELLO response must fit in one AMP payload");

void packet_process_frame(const AmpFrame *frame);
void packet_reset_session(void);
void packet_send_version_packet(void);
void packet_process_version_notifications(void);
void packet_send_debug_packet(void);
void packet_notify_profile_changed(uint16_t profile_id, uint32_t revision);
bool packet_console_is_subscribed(void);
uint32_t packet_next_console_sequence(void);

AmpStatus object_service_process(uint16_t opcode, const uint8_t *request,
                                 uint16_t request_len, uint8_t *response,
                                 uint16_t *response_len);
void object_service_reset(void);

AmpStatus packet_process_user(uint16_t opcode, const uint8_t *request,
                              uint16_t request_len, uint8_t *response,
                              uint16_t *response_len);

#ifdef __cplusplus
}
#endif

#endif /* AMP_PACKET_H */
