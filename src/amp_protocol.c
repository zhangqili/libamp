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
static volatile uint8_t rx_head;
static volatile uint8_t rx_tail;

static AmpReportSlot tx_high_queue[AMP_TX_HIGH_QUEUE_LENGTH];
static uint8_t tx_high_head;
static uint8_t tx_high_tail;
static uint8_t tx_high_len;

static AmpReportSlot tx_stream_queue[AMP_TX_STREAM_QUEUE_LENGTH];
static uint8_t tx_stream_head;
static uint8_t tx_stream_tail;
static uint8_t tx_stream_len;

static uint8_t queue_next(uint8_t index, uint8_t capacity)
{
    return (uint8_t)((index + 1U) % capacity);
}

static bool queue_push(AmpReportSlot *queue, uint8_t capacity, uint8_t *tail,
                       uint8_t *len, const uint8_t report[AMP_FRAME_REPORT_SIZE])
{
    if (*len >= capacity)
    {
        return false;
    }
    memcpy(queue[*tail].report, report, AMP_FRAME_REPORT_SIZE);
    *tail = queue_next(*tail, capacity);
    (*len)++;
    return true;
}

static bool rx_queue_push_report(const uint8_t report[AMP_FRAME_REPORT_SIZE])
{
    uint8_t tail = rx_tail;
    uint8_t next_tail = queue_next(tail, AMP_RX_QUEUE_LENGTH);
    if (next_tail == rx_head)
    {
        return false;
    }

    memcpy(rx_queue[tail].report, report, AMP_FRAME_REPORT_SIZE);
    rx_tail = next_tail;
    return true;
}

static bool queue_push_drop_oldest(AmpReportSlot *queue, uint8_t capacity,
                                   uint8_t *head, uint8_t *tail, uint8_t *len,
                                   const uint8_t report[AMP_FRAME_REPORT_SIZE])
{
    if (*len >= capacity)
    {
        *head = queue_next(*head, capacity);
        (*len)--;
    }
    return queue_push(queue, capacity, tail, len, report);
}

static bool queue_push_stream_report(const uint8_t report[AMP_FRAME_REPORT_SIZE])
{
#if AMP_TX_POLICY == AMP_TX_POLICY_RELIABLE_FIFO
    return queue_push(tx_stream_queue, AMP_TX_STREAM_QUEUE_LENGTH,
                      &tx_stream_tail, &tx_stream_len, report);
#else
    return queue_push_drop_oldest(tx_stream_queue, AMP_TX_STREAM_QUEUE_LENGTH,
                                  &tx_stream_head, &tx_stream_tail,
                                  &tx_stream_len, report);
#endif
}

static const uint8_t *queue_peek_ptr(AmpReportSlot *queue, uint8_t len, uint8_t head)
{
    return len == 0 ? NULL : queue[head].report;
}

static void queue_pop(uint8_t capacity, uint8_t *head, uint8_t *len)
{
    if (*len == 0)
    {
        return;
    }
    *head = queue_next(*head, capacity);
    (*len)--;
}

bool amp_is_frame(const uint8_t report[AMP_FRAME_REPORT_SIZE])
{
    return report != NULL && report[0] == AMP_FRAME_PROTO;
}

bool amp_frame_decode(const uint8_t report[AMP_FRAME_REPORT_SIZE], AmpFrame *frame)
{
    if (!amp_is_frame(report) || frame == NULL)
    {
        return false;
    }
    memcpy(frame, report, sizeof(*frame));
    return true;
}

int amp_frame_encode(uint8_t report[AMP_FRAME_REPORT_SIZE], uint8_t channel, uint8_t flags,
                     uint8_t seq, uint8_t code, uint8_t type, uint8_t status,
                     const uint8_t body[AMP_FRAME_BODY_SIZE])
{
    if (report == NULL)
    {
        return 1;
    }

    AmpFrame frame = {0};
    frame.header.proto = AMP_FRAME_PROTO;
    frame.header.channel_flags = (uint8_t)(((channel & 0x0F) << 4) | (flags & 0x0F));
    frame.header.seq = seq;
    frame.header.code = code;
    frame.header.type = type;
    frame.header.status = status;
    if (body != NULL)
    {
        memcpy(frame.body, body, AMP_FRAME_BODY_SIZE);
    }
    memcpy(report, &frame, sizeof(frame));
    return 0;
}

static int amp_enqueue_report(const uint8_t report[AMP_FRAME_REPORT_SIZE], bool stream)
{
    if (stream)
    {
        if (!queue_push_stream_report(report))
        {
            return 1;
        }
    }
    else if (!queue_push(tx_high_queue, AMP_TX_HIGH_QUEUE_LENGTH,
                         &tx_high_tail, &tx_high_len, report))
    {
        return 1;
    }

    amp_transport_kick();
    return 0;
}

int amp_send_frame(uint8_t channel, uint8_t flags, uint8_t seq, uint8_t code, uint8_t type,
                   uint8_t status, const uint8_t body[AMP_FRAME_BODY_SIZE], bool stream)
{
    uint8_t report[AMP_FRAME_REPORT_SIZE];
    if (amp_frame_encode(report, channel, flags, seq, code, type, status, body) != 0)
    {
        return 1;
    }
    return amp_enqueue_report(report, stream);
}

int amp_send_encoded_report(const uint8_t report[AMP_FRAME_REPORT_SIZE], bool stream)
{
    if (report == NULL)
    {
        return 1;
    }
    return amp_enqueue_report(report, stream);
}

int amp_send_console_log(const uint8_t *data, uint8_t len)
{
    uint8_t body[AMP_FRAME_BODY_SIZE] = {0};
    if (len > AMP_FRAME_BODY_SIZE - 1)
    {
        len = AMP_FRAME_BODY_SIZE - 1;
    }
    body[0] = len;
    if (data != NULL && len > 0)
    {
        memcpy(body + 1, data, len);
    }
    return amp_send_frame(AMP_CHANNEL_CONSOLE, 0, 0, PACKET_CODE_LOG, 0,
                          AMP_STATUS_OK, body, true);
}

int amp_send_error(uint8_t channel, uint8_t seq, uint8_t code, uint8_t type, AmpStatus status)
{
    return amp_send_frame(channel, AMP_FRAME_FLAG_RESP, seq, code, type,
                          (uint8_t)status, NULL, false);
}

bool amp_transport_control_event_can_enqueue(void)
{
#if AMP_TX_HIGH_QUEUE_LENGTH > 1
    return tx_high_len < (AMP_TX_HIGH_QUEUE_LENGTH - 1);
#else
    return false;
#endif
}

bool amp_transport_stream_event_can_enqueue(void)
{
#if AMP_TX_STREAM_QUEUE_LENGTH > 1
    return tx_stream_len < (AMP_TX_STREAM_QUEUE_LENGTH - 1);
#else
    return false;
#endif
}

void amp_transport_receive_report(const uint8_t report[AMP_FRAME_REPORT_SIZE])
{
    if (report != NULL)
    {
        (void)rx_queue_push_report(report);
    }
}

void amp_transport_reset_session(void)
{
    rx_head = 0;
    rx_tail = 0;
    tx_high_head = 0;
    tx_high_tail = 0;
    tx_high_len = 0;
    tx_stream_head = 0;
    tx_stream_tail = 0;
    tx_stream_len = 0;
}

void amp_transport_kick(void)
{
    const uint8_t *report = queue_peek_ptr(tx_high_queue, tx_high_len, tx_high_head);
    bool from_high = report != NULL;
    if (report == NULL)
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
        bool decoded = amp_frame_decode(rx_queue[head].report, &frame);
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
