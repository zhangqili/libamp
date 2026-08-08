/*
 * Copyright (c) 2026 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef AMP_PROTOCOL_H
#define AMP_PROTOCOL_H

#include "stdbool.h"
#include "stdint.h"
#include "keyboard_config.h"
#include "keyboard_def.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AMP_FRAME_PROTO        0x41
#define AMP_WIRE_VERSION       4
#define AMP_FRAME_REPORT_SIZE  64
#define AMP_FRAME_HEADER_SIZE  12
#ifndef AMP_FRAME_PAYLOAD_SIZE
#define AMP_FRAME_PAYLOAD_SIZE (AMP_FRAME_REPORT_SIZE - AMP_FRAME_HEADER_SIZE)
#endif
#define AMP_FRAME_MESSAGE_SIZE (AMP_FRAME_HEADER_SIZE + AMP_FRAME_PAYLOAD_SIZE)

typedef enum {
    AMP_CHANNEL_CONTROL   = 0,
    AMP_CHANNEL_CONFIG    = 1,
    AMP_CHANNEL_OBJECT    = 2,
    AMP_CHANNEL_DEBUG     = 3,
    AMP_CHANNEL_CONSOLE   = 4,
    AMP_CHANNEL_TELEMETRY = 5,
    AMP_CHANNEL_USER      = 15,
} AmpChannel;

enum {
    AMP_FRAME_FLAG_RESPONSE = 0x01,
    AMP_FRAME_FLAG_EVENT    = 0x02,
};

typedef enum {
    AMP_STATUS_OK               = 0,
    AMP_STATUS_UNSUPPORTED      = 1,
    AMP_STATUS_INVALID_ARGUMENT = 2,
    AMP_STATUS_BUSY             = 3,
    AMP_STATUS_IO_ERROR         = 4,
    AMP_STATUS_INVALID_STATE    = 5,
    AMP_STATUS_CONFLICT         = 6,
    AMP_STATUS_STALE_REVISION   = 7,
    AMP_STATUS_NOT_FOUND        = 8,
    AMP_STATUS_INTEGRITY_ERROR  = 9,
    AMP_STATUS_ABORTED          = 10,
    AMP_STATUS_NO_SPACE         = 11,
} AmpStatus;

typedef enum {
    AMP_QUEUE_RESPONSE,
    AMP_QUEUE_CONTROL,
    AMP_QUEUE_STREAM,
} AmpQueueClass;

typedef struct __AmpFrameHeader {
    uint8_t proto;
    uint8_t version;
    uint8_t channel_flags;
    uint8_t status;
    uint16_t session_id;
    uint16_t request_id;
    uint16_t opcode;
    uint16_t payload_len;
} __PACKED AmpFrameHeader;

typedef struct __AmpFrame {
    AmpFrameHeader header;
    uint8_t payload[AMP_FRAME_PAYLOAD_SIZE];
} __PACKED AmpFrame;

STATIC_ASSERT(sizeof(AmpFrameHeader) == AMP_FRAME_HEADER_SIZE,
              "AmpFrameHeader must be 12 bytes");
STATIC_ASSERT(sizeof(AmpFrame) == AMP_FRAME_MESSAGE_SIZE,
              "AmpFrame must contain one complete logical message");

#ifndef AMP_RX_QUEUE_LENGTH
#define AMP_RX_QUEUE_LENGTH 4
#endif
#ifndef AMP_TX_RESPONSE_QUEUE_LENGTH
#define AMP_TX_RESPONSE_QUEUE_LENGTH 4
#endif
#ifndef AMP_TX_CONTROL_QUEUE_LENGTH
#define AMP_TX_CONTROL_QUEUE_LENGTH 4
#endif
#ifndef AMP_TX_STREAM_QUEUE_LENGTH
#define AMP_TX_STREAM_QUEUE_LENGTH 4
#endif

static inline uint8_t amp_frame_channel(const AmpFrameHeader *header)
{
    return (uint8_t)(header->channel_flags >> 4);
}

static inline uint8_t amp_frame_flags(const AmpFrameHeader *header)
{
    return (uint8_t)(header->channel_flags & 0x0f);
}

bool amp_is_frame(const uint8_t *message, uint16_t message_len);
bool amp_frame_decode(const uint8_t *message, uint16_t message_len, AmpFrame *frame);
int amp_frame_encode(uint8_t *message, uint16_t capacity, uint8_t channel,
                     uint8_t flags, uint16_t session_id, uint16_t request_id,
                     uint16_t opcode, AmpStatus status, const void *payload,
                     uint16_t payload_len);

int amp_send_frame(uint8_t channel, uint8_t flags, uint16_t session_id,
                   uint16_t request_id, uint16_t opcode, AmpStatus status,
                   const void *payload, uint16_t payload_len,
                   AmpQueueClass queue_class);
int amp_send_encoded_message(const uint8_t *message,
                             AmpQueueClass queue_class);
int amp_send_console_log(const uint8_t *data, uint8_t len);
bool amp_transport_control_event_can_enqueue(void);
bool amp_transport_stream_event_can_enqueue(void);

void amp_transport_receive(const uint8_t *message, uint16_t message_len);
void amp_transport_reset_session(void);
void amp_transport_poll(void);
void amp_transport_raw_sent(void);
void amp_transport_kick(void);
void amp_transport_prepare_session(void);
int amp_transport_send(const uint8_t *message, uint16_t message_len);
uint16_t amp_transport_max_payload(void);

uint16_t amp_session_id(void);
bool amp_session_is_active(void);
uint16_t amp_session_max_rx_payload(void);
uint16_t amp_session_max_tx_payload(void);
void amp_session_begin(uint16_t session_id, uint16_t peer_max_rx_payload,
                       uint16_t peer_max_tx_payload);

#ifdef __cplusplus
}
#endif

#endif /* AMP_PROTOCOL_H */
