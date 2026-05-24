/*
 * Copyright (c) 2026 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef AMP_PROTOCOL_H
#define AMP_PROTOCOL_H

#include "stdint.h"
#include "stdbool.h"
#include "keyboard_def.h"

#ifdef __cplusplus
extern "C" {
#endif

#define AMP_FRAME_PROTO       0x41
#define AMP_FRAME_REPORT_SIZE 64

typedef enum {
    AMP_CHANNEL_CONTROL    = 0,
    AMP_CHANNEL_DEBUG      = 1,
    AMP_CHANNEL_CONSOLE    = 2,
    AMP_CHANNEL_LARGE      = 3,
    AMP_CHANNEL_NEXUS_CTRL = 4,
    AMP_CHANNEL_USER       = 15,
} AmpChannel;

enum {
    AMP_FRAME_FLAG_REQ_ACK = 0x01,
    AMP_FRAME_FLAG_RESP    = 0x02,
    AMP_FRAME_FLAG_ERROR   = 0x04,
    AMP_FRAME_FLAG_MORE    = 0x08,
};

typedef struct __AmpFrameHeader
{
    uint8_t proto;
    uint8_t channel_flags;
    uint8_t seq;
    uint8_t code;
    uint8_t type;
    uint8_t len;
} __PACKED AmpFrameHeader;

#define AMP_FRAME_HEADER_SIZE ((uint8_t)sizeof(AmpFrameHeader))
#define AMP_FRAME_MAX_PAYLOAD (AMP_FRAME_REPORT_SIZE - AMP_FRAME_HEADER_SIZE)

typedef struct __AmpFrame
{
    AmpFrameHeader header;
    uint8_t payload[AMP_FRAME_MAX_PAYLOAD];
} __PACKED AmpFrame;

#ifndef AMP_RX_QUEUE_LENGTH
#define AMP_RX_QUEUE_LENGTH 4
#endif

#ifndef AMP_TX_HIGH_QUEUE_LENGTH
#define AMP_TX_HIGH_QUEUE_LENGTH 4
#endif

#ifndef AMP_TX_STREAM_QUEUE_LENGTH
#define AMP_TX_STREAM_QUEUE_LENGTH 4
#endif

#define AMP_TX_POLICY_CONTROL_PRIORITY 1
#define AMP_TX_POLICY_RELIABLE_FIFO    2

#ifndef AMP_TX_POLICY
#define AMP_TX_POLICY AMP_TX_POLICY_CONTROL_PRIORITY
#endif

static inline uint8_t amp_frame_channel(const AmpFrameHeader *header)
{
    return (uint8_t)(header->channel_flags >> 4);
}

static inline uint8_t amp_frame_flags(const AmpFrameHeader *header)
{
    return (uint8_t)(header->channel_flags & 0x0F);
}

bool amp_is_frame(const uint8_t *report, uint16_t len);
bool amp_frame_decode(const uint8_t *report, uint16_t len, AmpFrame *frame);
int amp_frame_encode(uint8_t *report, uint8_t channel, uint8_t flags, uint8_t seq, uint8_t code, uint8_t type, const uint8_t *payload, uint8_t payload_len);

int amp_send_frame(uint8_t channel, uint8_t flags, uint8_t seq, uint8_t code, uint8_t type, const uint8_t *payload, uint8_t payload_len, bool stream);
int amp_send_encoded_report(const uint8_t *report, bool stream);
int amp_send_console_log(const uint8_t *data, uint8_t len);
int amp_send_error(uint8_t channel, uint8_t seq, uint8_t code, uint8_t type, uint8_t error_code);

void amp_transport_receive_report(const uint8_t *report, uint16_t len);
void amp_transport_poll(void);
void amp_transport_raw_sent(void);
void amp_transport_kick(void);

#ifdef __cplusplus
}
#endif

#endif // AMP_PROTOCOL_H
