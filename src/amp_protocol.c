/*
 * Copyright (c) 2026 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "amp_protocol.h"

#include "packet.h"
#include "stddef.h"
#include "string.h"

typedef struct {
    uint8_t message[AMP_FRAME_MESSAGE_SIZE];
} AmpReportSlot;

typedef struct {
    AmpReportSlot *slots;
    uint8_t capacity;
    uint8_t head;
    uint8_t tail;
    uint8_t length;
} AmpTxQueue;

static AmpReportSlot rx_queue[AMP_RX_QUEUE_LENGTH];
static volatile uint8_t rx_head;
static volatile uint8_t rx_tail;

static AmpReportSlot tx_response_slots[AMP_TX_RESPONSE_QUEUE_LENGTH];
static AmpReportSlot tx_control_slots[AMP_TX_CONTROL_QUEUE_LENGTH];
static AmpReportSlot tx_stream_slots[AMP_TX_STREAM_QUEUE_LENGTH];
static AmpTxQueue tx_response = {
    tx_response_slots, AMP_TX_RESPONSE_QUEUE_LENGTH, 0, 0, 0
};
static AmpTxQueue tx_control = {
    tx_control_slots, AMP_TX_CONTROL_QUEUE_LENGTH, 0, 0, 0
};
static AmpTxQueue tx_stream = {
    tx_stream_slots, AMP_TX_STREAM_QUEUE_LENGTH, 0, 0, 0
};

static uint16_t current_session_id;
static uint16_t current_max_rx_payload;
static uint16_t current_max_tx_payload;

static uint16_t min_u16(uint16_t left, uint16_t right)
{
    return left < right ? left : right;
}

static uint16_t local_transport_max_payload(void)
{
    return min_u16(amp_transport_max_payload(), AMP_FRAME_PAYLOAD_SIZE);
}

static uint8_t queue_next(uint8_t index, uint8_t capacity)
{
    return (uint8_t)((index + 1U) % capacity);
}

static void tx_queue_clear(AmpTxQueue *queue)
{
    queue->head = 0;
    queue->tail = 0;
    queue->length = 0;
}

static bool tx_queue_push(AmpTxQueue *queue,
                          const uint8_t *message, uint16_t message_len)
{
    if (queue->length >= queue->capacity || message == NULL ||
        message_len > AMP_FRAME_MESSAGE_SIZE)
    {
        return false;
    }
    memcpy(queue->slots[queue->tail].message, message, message_len);
    queue->tail = queue_next(queue->tail, queue->capacity);
    queue->length++;
    return true;
}

static bool tx_stream_push(const uint8_t *message, uint16_t message_len)
{
    if (tx_stream.length >= tx_stream.capacity)
    {
        tx_stream.head = queue_next(tx_stream.head, tx_stream.capacity);
        tx_stream.length--;
    }
    return tx_queue_push(&tx_stream, message, message_len);
}

static const AmpReportSlot *tx_queue_peek(const AmpTxQueue *queue)
{
    return queue->length == 0 ? NULL : &queue->slots[queue->head];
}

static void tx_queue_pop(AmpTxQueue *queue)
{
    if (queue->length != 0)
    {
        queue->head = queue_next(queue->head, queue->capacity);
        queue->length--;
    }
}

static bool rx_queue_push(const uint8_t *message, uint16_t message_len)
{
    uint8_t tail = rx_tail;
    uint8_t next_tail = queue_next(tail, AMP_RX_QUEUE_LENGTH);
    if (next_tail == rx_head)
    {
        return false;
    }
    if (message == NULL || message_len > AMP_FRAME_MESSAGE_SIZE)
    {
        return false;
    }
    memcpy(rx_queue[tail].message, message, message_len);
    rx_tail = next_tail;
    return true;
}

bool amp_is_frame(const uint8_t *message, uint16_t message_len)
{
    return message != NULL && message_len >= AMP_FRAME_HEADER_SIZE &&
           message[0] == AMP_FRAME_PROTO;
}

bool amp_frame_decode(const uint8_t *message, uint16_t message_len, AmpFrame *frame)
{
    AmpFrameHeader header;
    if (!amp_is_frame(message, message_len) || frame == NULL)
    {
        return false;
    }
    memcpy(&header, message, sizeof(header));
    if (header.version != AMP_WIRE_VERSION ||
        header.payload_len > AMP_FRAME_PAYLOAD_SIZE ||
        (uint32_t)AMP_FRAME_HEADER_SIZE + header.payload_len > message_len)
    {
        return false;
    }
    memset(frame, 0, sizeof(*frame));
    memcpy(&frame->header, &header, sizeof(header));
    if (header.payload_len != 0)
    {
        memcpy(frame->payload, message + AMP_FRAME_HEADER_SIZE,
               header.payload_len);
    }
    return true;
}

int amp_frame_encode(uint8_t *message, uint16_t capacity, uint8_t channel,
                     uint8_t flags, uint16_t session_id, uint16_t request_id,
                     uint16_t opcode, AmpStatus status, const void *payload,
                     uint16_t payload_len)
{
    if (message == NULL || payload_len > AMP_FRAME_PAYLOAD_SIZE ||
        capacity < (uint16_t)(AMP_FRAME_HEADER_SIZE + payload_len))
    {
        return -1;
    }
    AmpFrameHeader header = {
        .proto = AMP_FRAME_PROTO,
        .version = AMP_WIRE_VERSION,
        .channel_flags = (uint8_t)(((channel & 0x0f) << 4) | (flags & 0x0f)),
        .status = (uint8_t)status,
        .session_id = session_id,
        .request_id = request_id,
        .opcode = opcode,
        .payload_len = payload_len,
    };
    memset(message, 0, capacity);
    memcpy(message, &header, sizeof(header));
    if (payload_len != 0 && payload != NULL)
    {
        memcpy(message + AMP_FRAME_HEADER_SIZE, payload, payload_len);
    }
    return AMP_FRAME_HEADER_SIZE + payload_len;
}

static int amp_enqueue_report(const uint8_t *message, uint16_t message_len,
                              AmpQueueClass queue_class)
{
    bool queued = false;
    switch (queue_class)
    {
    case AMP_QUEUE_RESPONSE:
        queued = tx_queue_push(&tx_response, message, message_len);
        break;
    case AMP_QUEUE_CONTROL:
        queued = tx_queue_push(&tx_control, message, message_len);
        break;
    case AMP_QUEUE_STREAM:
        queued = tx_stream_push(message, message_len);
        break;
    default:
        break;
    }
    if (!queued)
    {
        return 1;
    }
    amp_transport_kick();
    return 0;
}

int amp_send_frame(uint8_t channel, uint8_t flags, uint16_t session_id,
                   uint16_t request_id, uint16_t opcode, AmpStatus status,
                   const void *payload, uint16_t payload_len,
                   AmpQueueClass queue_class)
{
    if (current_session_id != 0 && session_id == current_session_id &&
        payload_len > current_max_tx_payload)
    {
        return 1;
    }
    uint8_t message[AMP_FRAME_MESSAGE_SIZE];
    int message_len = amp_frame_encode(message, sizeof(message), channel, flags,
                                       session_id, request_id, opcode, status,
                                       payload, payload_len);
    if (message_len < 0)
    {
        return 1;
    }
    return amp_enqueue_report(message, (uint16_t)message_len, queue_class);
}

int amp_send_encoded_message(const uint8_t *message,
                             AmpQueueClass queue_class)
{
    AmpFrameHeader header;
    if (message == NULL)
    {
        return 1;
    }
    memcpy(&header, message, sizeof(header));
    uint16_t message_len = (uint16_t)(AMP_FRAME_HEADER_SIZE + header.payload_len);
    if (header.proto != AMP_FRAME_PROTO || header.version != AMP_WIRE_VERSION ||
        header.payload_len > AMP_FRAME_PAYLOAD_SIZE ||
        (current_session_id != 0 &&
         header.session_id == current_session_id &&
         header.payload_len > current_max_tx_payload))
    {
        return 1;
    }
    return amp_enqueue_report(message, message_len, queue_class);
}

int amp_send_console_log(const uint8_t *data, uint8_t len)
{
    if (!packet_console_is_subscribed() || current_session_id == 0)
    {
        return 1;
    }
    uint16_t max_payload = amp_session_max_tx_payload();
    if (max_payload < sizeof(uint32_t))
    {
        return 1;
    }
    if (len > max_payload - sizeof(uint32_t))
    {
        len = (uint8_t)(max_payload - sizeof(uint32_t));
    }
    uint8_t payload[AMP_FRAME_PAYLOAD_SIZE];
    uint32_t sequence = packet_next_console_sequence();
    memcpy(payload, &sequence, sizeof(sequence));
    if (len != 0 && data != NULL)
    {
        memcpy(payload + sizeof(sequence), data, len);
    }
    return amp_send_frame(AMP_CHANNEL_CONSOLE, AMP_FRAME_FLAG_EVENT,
                          current_session_id, 0, AMP_CONSOLE_DATA,
                          AMP_STATUS_OK, payload,
                          (uint16_t)(sizeof(sequence) + len), AMP_QUEUE_STREAM);
}

bool amp_transport_control_event_can_enqueue(void)
{
    return tx_control.length < tx_control.capacity;
}

bool amp_transport_stream_event_can_enqueue(void)
{
    return tx_stream.length < tx_stream.capacity;
}

void amp_transport_receive(const uint8_t *message, uint16_t message_len)
{
    if (message == NULL || message_len < AMP_FRAME_HEADER_SIZE)
    {
        return;
    }
    AmpFrameHeader header;
    memcpy(&header, message, sizeof(header));
    uint16_t logical_len = (uint16_t)(AMP_FRAME_HEADER_SIZE + header.payload_len);
    if (header.payload_len > AMP_FRAME_PAYLOAD_SIZE || logical_len > message_len ||
        (current_session_id != 0 &&
         header.session_id == current_session_id &&
         !(amp_frame_channel(&header) == AMP_CHANNEL_CONTROL &&
           header.opcode == AMP_CONTROL_HELLO) &&
         header.payload_len > current_max_rx_payload))
    {
        return;
    }
    (void)rx_queue_push(message, logical_len);
}

void amp_transport_reset_session(void)
{
    rx_head = 0;
    rx_tail = 0;
    tx_queue_clear(&tx_response);
    tx_queue_clear(&tx_control);
    tx_queue_clear(&tx_stream);
    current_session_id = 0;
    current_max_rx_payload = 0;
    current_max_tx_payload = 0;
    packet_reset_session();
}

uint16_t amp_session_id(void)
{
    return current_session_id;
}

bool amp_session_is_active(void)
{
    return current_session_id != 0;
}

uint16_t amp_session_max_rx_payload(void)
{
    return current_session_id == 0 ? local_transport_max_payload() :
                                     current_max_rx_payload;
}

uint16_t amp_session_max_tx_payload(void)
{
    return current_session_id == 0 ? local_transport_max_payload() :
                                     current_max_tx_payload;
}

void amp_session_begin(uint16_t session_id, uint16_t peer_max_rx_payload,
                       uint16_t peer_max_tx_payload)
{
    amp_transport_reset_session();
    amp_transport_prepare_session();
    current_session_id = session_id;
    uint16_t transport_limit = local_transport_max_payload();
    current_max_rx_payload = min_u16(transport_limit, peer_max_tx_payload);
    current_max_tx_payload = min_u16(transport_limit, peer_max_rx_payload);
}

void amp_transport_kick(void)
{
    AmpTxQueue *selected = &tx_response;
    const AmpReportSlot *slot = tx_queue_peek(selected);
    if (slot == NULL)
    {
        selected = &tx_control;
        slot = tx_queue_peek(selected);
    }
    if (slot == NULL)
    {
        selected = &tx_stream;
        slot = tx_queue_peek(selected);
    }
    if (slot != NULL)
    {
        AmpFrameHeader header;
        memcpy(&header, slot->message, sizeof(header));
        uint16_t message_len = (uint16_t)(AMP_FRAME_HEADER_SIZE +
                                          header.payload_len);
        if (amp_transport_send(slot->message, message_len) == 0)
        {
            tx_queue_pop(selected);
        }
    }
}

void amp_transport_poll(void)
{
    for (;;)
    {
        uint8_t head = rx_head;
        if (head == rx_tail)
        {
            break;
        }
        AmpFrame frame;
        AmpFrameHeader header;
        memcpy(&header, rx_queue[head].message, sizeof(header));
        uint16_t message_len = (uint16_t)(AMP_FRAME_HEADER_SIZE +
                                          header.payload_len);
        bool decoded = amp_frame_decode(rx_queue[head].message, message_len,
                                        &frame);
        rx_head = queue_next(head, AMP_RX_QUEUE_LENGTH);
        if (decoded)
        {
            packet_process_frame(&frame);
        }
    }
    amp_transport_kick();
}

void amp_transport_raw_sent(void)
{
    amp_transport_kick();
}
