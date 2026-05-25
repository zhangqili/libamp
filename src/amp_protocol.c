/*
 * Copyright (c) 2026 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "amp_protocol.h"

#include "driver.h"
#include "packet.h"
#include "stddef.h"
#include "string.h"

typedef struct
{
    uint8_t report[AMP_FRAME_REPORT_SIZE];
} AmpReportSlot;

static AmpReportSlot rx_queue[AMP_RX_QUEUE_LENGTH];
static uint8_t rx_head;
static uint8_t rx_tail;
static uint8_t rx_len;

static AmpReportSlot tx_high_queue[AMP_TX_HIGH_QUEUE_LENGTH];
static uint8_t tx_high_head;
static uint8_t tx_high_tail;
static uint8_t tx_high_len;

static AmpReportSlot tx_stream_queue[AMP_TX_STREAM_QUEUE_LENGTH];
static uint8_t tx_stream_head;
static uint8_t tx_stream_tail;
static uint8_t tx_stream_len;

static uint8_t *queue_reserve(AmpReportSlot *queue, uint8_t capacity, uint8_t *tail, uint8_t *len)
{
    if (*len >= capacity)
    {
        return NULL;
    }
    uint8_t *report = queue[*tail].report;
    *tail = (uint8_t)((*tail + 1) % capacity);
    (*len)++;
    return report;
}

static bool queue_push(AmpReportSlot *queue, uint8_t capacity, uint8_t *tail, uint8_t *len, const uint8_t *report)
{
    uint8_t *slot = queue_reserve(queue, capacity, tail, len);
    if (slot == NULL)
    {
        return false;
    }
    memcpy(slot, report, AMP_FRAME_REPORT_SIZE);
    return true;
}

static bool queue_push_report(AmpReportSlot *queue, uint8_t capacity, uint8_t *tail, uint8_t *len, const uint8_t *report, uint16_t report_len)
{
    uint8_t *slot = queue_reserve(queue, capacity, tail, len);
    if (slot == NULL)
    {
        return false;
    }

    if (report_len > AMP_FRAME_REPORT_SIZE)
    {
        report_len = AMP_FRAME_REPORT_SIZE;
    }

    memset(slot, 0, AMP_FRAME_REPORT_SIZE);
    memcpy(slot, report, report_len);
    return true;
}

static bool queue_push_drop_oldest(AmpReportSlot *queue, uint8_t capacity, uint8_t *head, uint8_t *tail, uint8_t *len, const uint8_t *report)
{
    if (*len >= capacity)
    {
        *head = (uint8_t)((*head + 1) % capacity);
        (*len)--;
    }
    return queue_push(queue, capacity, tail, len, report);
}

static const uint8_t *queue_peek_ptr(AmpReportSlot *queue, uint8_t len, uint8_t head)
{
    if (len == 0)
    {
        return NULL;
    }
    return queue[head].report;
}

static void queue_pop(uint8_t capacity, uint8_t *head, uint8_t *len)
{
    if (*len == 0)
    {
        return;
    }
    *head = (uint8_t)((*head + 1) % capacity);
    (*len)--;
}

bool amp_is_frame(const uint8_t *report, uint16_t len)
{
    return report != NULL && len >= AMP_FRAME_HEADER_SIZE && report[0] == AMP_FRAME_PROTO;
}

bool amp_frame_decode(const uint8_t *report, uint16_t len, AmpFrame *frame)
{
    if (!amp_is_frame(report, len) || frame == NULL)
    {
        return false;
    }
    const AmpFrameHeader *header = (const AmpFrameHeader *)report;
    if (header->len > AMP_FRAME_MAX_PAYLOAD || (uint16_t)(AMP_FRAME_HEADER_SIZE + header->len) > len)
    {
        return false;
    }

    memset(frame, 0, sizeof(*frame));
    memcpy(&frame->header, report, AMP_FRAME_HEADER_SIZE);
    if (header->len > 0)
    {
        memcpy(frame->payload, report + AMP_FRAME_HEADER_SIZE, header->len);
    }
    return true;
}

int amp_frame_encode(uint8_t *report, uint8_t channel, uint8_t flags, uint8_t seq, uint8_t code, uint8_t type, const uint8_t *payload, uint8_t payload_len)
{
    if (report == NULL || payload_len > AMP_FRAME_MAX_PAYLOAD || (payload_len > 0 && payload == NULL))
    {
        return 1;
    }

    memset(report, 0, AMP_FRAME_REPORT_SIZE);
    AmpFrameHeader *header = (AmpFrameHeader *)report;
    header->proto = AMP_FRAME_PROTO;
    header->channel_flags = (uint8_t)(((channel & 0x0F) << 4) | (flags & 0x0F));
    header->seq = seq;
    header->code = code;
    header->type = type;
    header->len = payload_len;
    if (payload_len > 0)
    {
        memcpy(report + AMP_FRAME_HEADER_SIZE, payload, payload_len);
    }
    return 0;
}

static int amp_enqueue_report(const uint8_t *report, bool stream)
{
    if (stream)
    {
#if AMP_TX_POLICY == AMP_TX_POLICY_RELIABLE_FIFO
        if (!queue_push(tx_stream_queue, AMP_TX_STREAM_QUEUE_LENGTH, &tx_stream_tail, &tx_stream_len, report))
        {
            return 1;
        }
#else
        queue_push_drop_oldest(tx_stream_queue, AMP_TX_STREAM_QUEUE_LENGTH, &tx_stream_head, &tx_stream_tail, &tx_stream_len, report);
#endif
    }
    else if (!queue_push(tx_high_queue, AMP_TX_HIGH_QUEUE_LENGTH, &tx_high_tail, &tx_high_len, report))
    {
        return 1;
    }

    amp_transport_kick();
    return 0;
}

static uint8_t *amp_reserve_tx_report(bool stream)
{
    if (stream)
    {
#if AMP_TX_POLICY == AMP_TX_POLICY_RELIABLE_FIFO
        return queue_reserve(tx_stream_queue, AMP_TX_STREAM_QUEUE_LENGTH, &tx_stream_tail, &tx_stream_len);
#else
        if (tx_stream_len >= AMP_TX_STREAM_QUEUE_LENGTH)
        {
            queue_pop(AMP_TX_STREAM_QUEUE_LENGTH, &tx_stream_head, &tx_stream_len);
        }
        return queue_reserve(tx_stream_queue, AMP_TX_STREAM_QUEUE_LENGTH, &tx_stream_tail, &tx_stream_len);
#endif
    }
    return queue_reserve(tx_high_queue, AMP_TX_HIGH_QUEUE_LENGTH, &tx_high_tail, &tx_high_len);
}

int amp_send_frame(uint8_t channel, uint8_t flags, uint8_t seq, uint8_t code, uint8_t type, const uint8_t *payload, uint8_t payload_len, bool stream)
{
    if (payload_len > AMP_FRAME_MAX_PAYLOAD || (payload_len > 0 && payload == NULL))
    {
        return 1;
    }
    uint8_t *report = amp_reserve_tx_report(stream);
    if (report == NULL)
    {
        return 1;
    }
    (void)amp_frame_encode(report, channel, flags, seq, code, type, payload, payload_len);
    amp_transport_kick();
    return 0;
}

int amp_send_encoded_report(const uint8_t *report, bool stream)
{
    if (report == NULL)
    {
        return 1;
    }
    return amp_enqueue_report(report, stream);
}

int amp_send_console_log(const uint8_t *data, uint8_t len)
{
    if (len > AMP_FRAME_MAX_PAYLOAD)
    {
        len = AMP_FRAME_MAX_PAYLOAD;
    }
    return amp_send_frame(AMP_CHANNEL_CONSOLE, 0, 0, PACKET_CODE_LOG, 0, data, len, true);
}

int amp_send_error(uint8_t channel, uint8_t seq, uint8_t code, uint8_t type, uint8_t error_code)
{
    return amp_send_frame(channel, (uint8_t)(AMP_FRAME_FLAG_RESP | AMP_FRAME_FLAG_ERROR), seq, code, type, &error_code, 1, false);
}

bool amp_transport_control_event_can_enqueue(void)
{
#if AMP_TX_HIGH_QUEUE_LENGTH > 1
    return tx_high_len < (AMP_TX_HIGH_QUEUE_LENGTH - 1);
#else
    return false;
#endif
}

void amp_transport_receive_report(const uint8_t *report, uint16_t len)
{
    if (report == NULL || len == 0)
    {
        return;
    }

    if (!queue_push_report(rx_queue, AMP_RX_QUEUE_LENGTH, &rx_tail, &rx_len, report, len))
    {
        queue_pop(AMP_RX_QUEUE_LENGTH, &rx_head, &rx_len);
        queue_push_report(rx_queue, AMP_RX_QUEUE_LENGTH, &rx_tail, &rx_len, report, len);
    }
}

void amp_transport_kick(void)
{
    const uint8_t *report = queue_peek_ptr(tx_high_queue, tx_high_len, tx_high_head);
    bool from_high = false;
    if (report != NULL)
    {
        from_high = true;
    }
    else
    {
        report = queue_peek_ptr(tx_stream_queue, tx_stream_len, tx_stream_head);
    }

    if (report == NULL)
    {
        return;
    }

    if (hid_send_raw((uint8_t *)report, AMP_FRAME_REPORT_SIZE) == 0)
    {
        if (from_high)
        {
            queue_pop(AMP_TX_HIGH_QUEUE_LENGTH, &tx_high_head, &tx_high_len);
        }
        else
        {
            queue_pop(AMP_TX_STREAM_QUEUE_LENGTH, &tx_stream_head, &tx_stream_len);
        }
    }
}

void amp_transport_raw_sent(void)
{
    amp_transport_kick();
}

static void amp_process_frame(const AmpFrame *frame)
{
    packet_process_frame(frame);
}

void amp_transport_poll(void)
{
    while (rx_len > 0)
    {
        const uint8_t *report = queue_peek_ptr(rx_queue, rx_len, rx_head);
        AmpFrame frame;
        bool decoded = report != NULL && amp_frame_decode(report, AMP_FRAME_REPORT_SIZE, &frame);
        queue_pop(AMP_RX_QUEUE_LENGTH, &rx_head, &rx_len);

        if (decoded)
        {
            amp_process_frame(&frame);
        }
    }
    amp_transport_kick();
}
