/*
 * Copyright (c) 2025 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef NEXUS_H_
#define NEXUS_H_

#include "keyboard.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Nexus links several boards into one keyboard. The master owns the USB link
 * and the keymap; every slave scans its own keys and streams them to the
 * master through a transport supplied by the application:
 *
 *   nexus_send(slave_id, frame, len)   master -> slave   (driver.h)
 *   nexus_report(frame, len)           slave  -> master  (driver.h)
 *   nexus_process_buffer(slave_id, frame, len) is called by the transport on
 *   either side whenever a complete frame has arrived.
 *
 * Both callbacks must either copy the frame or finish transmitting it before
 * they return: the library reuses its buffers on the next tick.
 *
 * Frame format
 * ------------
 * Bit 7 of the first byte distinguishes the two kinds of frame.
 *
 * Report frame (slave -> master, first byte has NEXUS_REPORT_FLAG set):
 *   bitmap mode   PacketNexus below. `bits` is the slave's key report bitmap
 *                 (bit j = slave key j, little-endian byte order) and `raw`
 *                 and `value` describe the single key selected by `index`;
 *                 the slave cycles `index` through its keys so the master
 *                 eventually sees the analog data of every key.
 *   raw mode      (NEXUS_USE_RAW) little-endian uint16_t samples, one per
 *                 slave key. The first sample carries the flag and is sent as
 *                 (raw & 0x7F) | 0x80, raw >> 7, so it keeps 15 bits.
 *
 * Packet frame (either direction, first byte has NEXUS_REPORT_FLAG clear):
 *   a normal libamp packet (packet.h). For SET/GET requests the master writes
 *   a transaction id into byte 1, the slave processes the packet and echoes it
 *   back, and the master matches the echo by code and id. EVENT packets keep
 *   their flag in byte 1 and are fire-and-forget. PACKET_CODE_USER (0xFF)
 *   cannot travel over Nexus because its code byte has bit 7 set.
 */

#ifndef NEXUS_SLAVE_NUM
#define NEXUS_SLAVE_NUM 1
#endif

#ifndef NEXUS_SLICE_LENGTH_MAX
#define NEXUS_SLICE_LENGTH_MAX 16
#endif

#ifndef NEXUS_VALUE_MAX
#define NEXUS_VALUE_MAX 65535
#endif

#ifndef NEXUS_BUFFER_SIZE
#define NEXUS_BUFFER_SIZE 8
#endif

#ifndef NEXUS_CONTROL_BUFFER_SIZE
#define NEXUS_CONTROL_BUFFER_SIZE 64
#endif

#if NEXUS_BUFFER_SIZE > NEXUS_CONTROL_BUFFER_SIZE
#define NEXUS_RX_BUFFER_SIZE NEXUS_BUFFER_SIZE
#else
#define NEXUS_RX_BUFFER_SIZE NEXUS_CONTROL_BUFFER_SIZE
#endif

/* Give up a synchronous request after this many failed nexus_send() calls. */
#ifndef NEXUS_RETRY_COUNT
#define NEXUS_RETRY_COUNT 100
#endif

/* A slave whose reports stop for longer than this is treated as unplugged:
 * its keys are released and its configuration is sent again when it returns. */
#ifndef NEXUS_LINK_TIMEOUT_MS
#define NEXUS_LINK_TIMEOUT_MS 100
#endif

/* How long the master waits for a slave to echo a SET/GET request. */
#ifndef NEXUS_REQUEST_TIMEOUT_MS
#define NEXUS_REQUEST_TIMEOUT_MS 1000
#endif

#ifndef NEXUS_IS_SLAVE
#define NEXUS_IS_SLAVE 0
#endif

#ifndef NEXUS_USE_RAW
#define NEXUS_USE_RAW 0
#endif

#define NEXUS_REPORT_FLAG 0x80

#define NEXUS_BITMAP_WORDS ((NEXUS_SLICE_LENGTH_MAX + 31) / 32)

typedef struct __PacketNexus
{
  uint8_t index;              /* slave key described by raw/value, bit 7 set */
#if NEXUS_SLICE_LENGTH_MAX >= 128
  uint8_t index_high;         /* bits 7.. of the key index */
#endif
#if NEXUS_VALUE_MAX == 0
  /* No value field */
#elif NEXUS_VALUE_MAX < 256
  uint8_t value;              /* normalized travel scaled to NEXUS_VALUE_MAX */
#else
  uint16_t value;
#endif
  uint16_t raw;               /* filtered raw sample of that key */
  uint8_t bits[(NEXUS_SLICE_LENGTH_MAX + 7) / 8];
} __PACKED PacketNexus;

typedef struct __NexusSlaveConfig
{
    uint16_t length;          /* number of keys the slave reports */
    const uint16_t *map;      /* slave key j -> master key id map[j] */
} NexusSlaveKeymap;

/* Receive buffer for packet frames echoed by each slave. A transport may DMA
 * straight into it and then call nexus_process_buffer() with that pointer. */
extern uint8_t g_nexus_slave_buffer[NEXUS_SLAVE_NUM][NEXUS_RX_BUFFER_SIZE];

/* Master: resolve the slave maps and queue their configuration. Does not
 * block; the configuration is sent from nexus_poll(). */
void nexus_init(void);
/* Master, keyboard tick: apply the latest slave reports to the local keys. */
void nexus_process(void);
/* Master, foreground (keyboard_process): drive configuration sync and
 * link supervision. */
void nexus_poll(void);
/* Transport callback, any context: a complete frame arrived. */
void nexus_process_buffer(uint8_t slave_id, uint8_t *buf, uint16_t len);
/* Master: queue the configuration of one key for every slave that maps it. */
int nexus_sync_advanced_key_config(uint16_t key_index);
/* Master: ask every slave to recalibrate. */
void nexus_calibrate(void);
/* Master: true while reports from the slave keep arriving. */
bool nexus_slave_is_online(uint8_t slave_id);
/* Slave, keyboard tick: send the report frame for this tick. */
int  nexus_send_report(void);

/* Synchronous request/response from the master. Only call these from a
 * context where g_keyboard_tick keeps advancing (the foreground loop, or an
 * interrupt below the tick's priority): the functions spin until the slave
 * echoes the request or the timeout, in ticks, elapses. */
int nexus_request_timeout(uint8_t slave_id, const uint8_t *request,
                          uint16_t request_len, uint32_t timeout,
                          uint8_t *response, uint16_t response_capacity);
int nexus_send_timeout(uint8_t slave_id, const uint8_t *report, uint16_t len, uint32_t timeout);

/* Frame encoders shared by both ends (pure functions, useful for tests). */
void nexus_report_encode(PacketNexus *packet, uint16_t index, AnalogRawValue raw,
                         AnalogValue value, const volatile uint32_t *bitmap, uint16_t key_count);
uint16_t nexus_raw_report_encode(uint8_t *buf, const AnalogRawValue *raws, uint16_t count);

/* Normalized value <-> wire value. Exact inverses when NEXUS_VALUE_MAX equals
 * ANALOG_VALUE_RANGE; otherwise the wire value is quantized. */
static inline uint16_t nexus_value_to_wire(AnalogValue value)
{
#if NEXUS_VALUE_MAX == ANALOG_VALUE_RANGE
    return (uint16_t)(value - ANALOG_VALUE_MIN);
#else
    return (uint16_t)(((uint32_t)(value - ANALOG_VALUE_MIN) * NEXUS_VALUE_MAX) / ANALOG_VALUE_RANGE);
#endif
}

static inline AnalogValue nexus_value_from_wire(uint16_t wire)
{
#if NEXUS_VALUE_MAX == ANALOG_VALUE_RANGE
    return (AnalogValue)(wire + ANALOG_VALUE_MIN);
#else
    return (AnalogValue)(((uint32_t)wire * ANALOG_VALUE_RANGE) / NEXUS_VALUE_MAX + ANALOG_VALUE_MIN);
#endif
}

#ifdef __cplusplus
}
#endif

#endif /* NEXUS_H_ */
