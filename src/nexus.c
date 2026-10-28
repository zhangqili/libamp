/*
 * Copyright (c) 2025 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "nexus.h"
#include "packet.h"
#include "driver.h"
#include "analog.h"
#include "layer.h"
#include <stddef.h>
#include <string.h>

/*
 * Concurrency model
 * -----------------
 * nexus_process_buffer() usually runs from the transport interrupt, while
 * nexus_process() runs from the keyboard tick and nexus_poll() plus the
 * synchronous request helpers run from the foreground. Everything that crosses
 * a context boundary is therefore published through word-sized stores to
 * volatile fields, with a compiler fence between filling a buffer and raising
 * the flag that announces it. This is sufficient on a single core; a
 * multi-core port would need hardware barriers in NEXUS_FENCE().
 */
#define NEXUS_FENCE() COMPILER_FENCE()

#define NEXUS_MIN(a, b) ((a) < (b) ? (a) : (b))
#define NEXUS_LINK_TIMEOUT_TICKS    KEYBOARD_TIME_TO_TICK(NEXUS_LINK_TIMEOUT_MS)
#define NEXUS_REQUEST_TIMEOUT_TICKS KEYBOARD_TIME_TO_TICK(NEXUS_REQUEST_TIMEOUT_MS)

/* ==========================================================================
 * Frame encoders shared by master and slave
 * ========================================================================== */

void nexus_report_encode(PacketNexus *packet, uint16_t index, AnalogRawValue raw,
                         AnalogValue value, const volatile uint32_t *bitmap, uint16_t key_count)
{
    const uint16_t byte_count = (uint16_t)NEXUS_MIN((key_count + 7) / 8, sizeof(packet->bits));

    packet->index = (uint8_t)((index & 0x7F) | NEXUS_REPORT_FLAG);
#if NEXUS_SLICE_LENGTH_MAX >= 128
    packet->index_high = (uint8_t)(index >> 7);
#endif
#if NEXUS_VALUE_MAX != 0
    packet->value = nexus_value_to_wire(value);
#else
    UNUSED(value);
#endif
    packet->raw = raw;

    memset(packet->bits, 0, sizeof(packet->bits));
    for (uint16_t i = 0; i < byte_count; i++)
    {
        packet->bits[i] = (uint8_t)(bitmap[i / 4] >> ((i % 4) * 8));
    }
    if (byte_count != 0 && (key_count % 8) != 0)
    {
        packet->bits[byte_count - 1] &= (uint8_t)((1U << (key_count % 8)) - 1);
    }
}

uint16_t nexus_raw_report_encode(uint8_t *buf, const AnalogRawValue *raws, uint16_t count)
{
    if (count == 0)
    {
        return 0;
    }
    buf[0] = (uint8_t)((raws[0] & 0x7F) | NEXUS_REPORT_FLAG);
    buf[1] = (uint8_t)(raws[0] >> 7);
    for (uint16_t i = 1; i < count; i++)
    {
        buf[2 * i] = (uint8_t)(raws[i] & 0xFF);
        buf[2 * i + 1] = (uint8_t)(raws[i] >> 8);
    }
    return (uint16_t)(count * 2);
}

#if !NEXUS_IS_SLAVE

/* ==========================================================================
 * Master
 * ========================================================================== */

__WEAK NexusSlaveKeymap g_nexus_slave_configs[NEXUS_SLAVE_NUM];
uint8_t g_nexus_slave_buffer[NEXUS_SLAVE_NUM][NEXUS_RX_BUFFER_SIZE];

#define NEXUS_NO_OWNER 0xFF

typedef struct __NexusSlave
{
    /* Resolved at nexus_init(): slot j of the slave drives master key keys[j]
     * (NULL when the slot is unmapped or out of range). */
    Key *keys[NEXUS_SLICE_LENGTH_MAX];
    const uint16_t *map;
    uint16_t length;

    /* Published by nexus_process_buffer() from the transport context. */
    volatile uint32_t bitmap[NEXUS_BITMAP_WORDS];
    volatile uint32_t last_report_tick;
    volatile bool report_seen;
    volatile bool response_ready;   /* g_nexus_slave_buffer holds a fresh echo */

    /* Configuration sync. `pending` may be raised from any context by
     * nexus_sync_advanced_key_config(); everything else belongs to nexus_poll(). */
    volatile uint8_t pending[NEXUS_SLICE_LENGTH_MAX];
    bool online;
    bool request_in_flight;
    bool request_acked;
    uint8_t request_id;
    uint16_t request_slot;
    uint16_t next_slot;
    uint32_t request_tick;
} NexusSlave;

static NexusSlave nexus_slaves[NEXUS_SLAVE_NUM];
static uint8_t nexus_sequence;  /* transaction ids 1..255, foreground only */

#if NEXUS_USE_RAW
/* Latest raw sample of every remote key, indexed by master key id. */
static volatile AnalogRawValue nexus_remote_raw[ADVANCED_KEY_NUM];
static uint8_t nexus_key_owner[ADVANCED_KEY_NUM];
#endif

static inline bool nexus_slave_online_at(const NexusSlave *slave, uint32_t tick)
{
    return slave->report_seen &&
           tick_elapsed32(tick, slave->last_report_tick) <= NEXUS_LINK_TIMEOUT_TICKS;
}

bool nexus_slave_is_online(uint8_t slave_id)
{
    if (slave_id >= NEXUS_SLAVE_NUM)
    {
        return false;
    }
    return nexus_slave_online_at(&nexus_slaves[slave_id], keyboard_tick_get32());
}

static uint8_t nexus_next_id(void)
{
    nexus_sequence++;
    if (nexus_sequence == 0)
    {
        nexus_sequence = 1;
    }
    return nexus_sequence;
}

/* Queue the configuration of every analog key the slave maps. */
static void nexus_queue_slave_config(NexusSlave *slave)
{
    for (uint16_t j = 0; j < slave->length; j++)
    {
        if (slave->keys[j] != NULL && IS_ADVANCED_KEY(slave->keys[j]))
        {
            slave->pending[j] = 1;
        }
    }
}

void nexus_init(void)
{
    memset(nexus_slaves, 0, sizeof(nexus_slaves));
    nexus_sequence = 0;
#if NEXUS_USE_RAW
    memset(nexus_key_owner, NEXUS_NO_OWNER, sizeof(nexus_key_owner));
    memset((void *)nexus_remote_raw, 0, sizeof(nexus_remote_raw));
#endif

    for (uint8_t s = 0; s < NEXUS_SLAVE_NUM; s++)
    {
        NexusSlave *slave = &nexus_slaves[s];
        const NexusSlaveKeymap *config = &g_nexus_slave_configs[s];

        slave->map = config->map;
        slave->length = config->map != NULL ? (uint16_t)NEXUS_MIN(config->length, NEXUS_SLICE_LENGTH_MAX) : 0;
        for (uint16_t j = 0; j < slave->length; j++)
        {
            const uint16_t key_index = slave->map[j];
            slave->keys[j] = keyboard_get_key(key_index);
#if NEXUS_USE_RAW
            if (key_index < ADVANCED_KEY_NUM)
            {
                nexus_key_owner[key_index] = s;
            }
#endif
        }
        /* Sent once the slave shows up, see nexus_poll(). */
        nexus_queue_slave_config(slave);
    }
}

/* --------------------------------------------------------------------------
 * Receiving frames (transport context)
 * -------------------------------------------------------------------------- */

#if NEXUS_USE_RAW
static bool nexus_receive_report(NexusSlave *slave, const uint8_t *buf, uint16_t len)
{
    const uint16_t count = (uint16_t)NEXUS_MIN(len / 2, slave->length);
    if (count == 0)
    {
        return false;
    }
    for (uint16_t i = 0; i < count; i++)
    {
        const uint16_t key_index = slave->map[i];
        if (key_index >= ADVANCED_KEY_NUM)
        {
            continue;
        }
        AnalogRawValue raw;
        if (i == 0)
        {
            raw = (AnalogRawValue)((buf[0] & 0x7F) | ((uint16_t)buf[1] << 7));
        }
        else
        {
            raw = (AnalogRawValue)(buf[2 * i] | ((uint16_t)buf[2 * i + 1] << 8));
        }
        nexus_remote_raw[key_index] = raw;
    }
    return true;
}
#else
static bool nexus_receive_report(NexusSlave *slave, const uint8_t *buf, uint16_t len)
{
    const PacketNexus *packet = (const PacketNexus *)buf;
    const uint16_t bitmap_bytes = (uint16_t)((slave->length + 7) / 8);
    if (len < offsetof(PacketNexus, bits) + bitmap_bytes)
    {
        return false;
    }

    /* Rebuild each word locally and publish it with a single store, so the
     * tick never observes a half-written bitmap. */
    for (uint16_t w = 0; w < NEXUS_BITMAP_WORDS; w++)
    {
        const uint16_t byte = (uint16_t)(w * 4);
        uint32_t word = 0;
        if (byte < bitmap_bytes)
        {
            word = packet->bits[byte];
        }
        if (byte + 1U < sizeof(packet->bits) && byte + 1U < bitmap_bytes)
        {
            word |= (uint32_t)packet->bits[byte + 1] << 8;
        }
        if (byte + 2U < sizeof(packet->bits) && byte + 2U < bitmap_bytes)
        {
            word |= (uint32_t)packet->bits[byte + 2] << 16;
        }
        if (byte + 3U < sizeof(packet->bits) && byte + 3U < bitmap_bytes)
        {
            word |= (uint32_t)packet->bits[byte + 3] << 24;
        }
        slave->bitmap[w] = word;
    }

    uint16_t index = packet->index & 0x7F;
#if NEXUS_SLICE_LENGTH_MAX >= 128
    index |= (uint16_t)packet->index_high << 7;
#endif
    if (index < slave->length && slave->keys[index] != NULL && IS_ADVANCED_KEY(slave->keys[index]))
    {
        AdvancedKey *advanced_key = (AdvancedKey *)slave->keys[index];
        advanced_key->raw = packet->raw;
        advanced_key->filtered_raw = packet->raw;
#if NEXUS_VALUE_MAX != 0
        advanced_key->value = nexus_value_from_wire(packet->value);
#endif
    }
    return true;
}
#endif

static void nexus_receive_echo(uint8_t slave_id, NexusSlave *slave, const uint8_t *buf, uint16_t len)
{
    uint8_t *dest = g_nexus_slave_buffer[slave_id];
    const uint16_t copy_len = (uint16_t)NEXUS_MIN(len, NEXUS_RX_BUFFER_SIZE);

    if (buf != dest)
    {
        memcpy(dest, buf, copy_len);
    }
    if (copy_len < NEXUS_RX_BUFFER_SIZE)
    {
        memset(dest + copy_len, 0, NEXUS_RX_BUFFER_SIZE - copy_len);
    }
    NEXUS_FENCE();
    slave->response_ready = true;
}

void nexus_process_buffer(uint8_t slave_id, uint8_t *buf, uint16_t len)
{
    if (slave_id >= NEXUS_SLAVE_NUM || buf == NULL || len == 0)
    {
        return;
    }
    NexusSlave *slave = &nexus_slaves[slave_id];

    if (buf[0] & NEXUS_REPORT_FLAG)
    {
        if (nexus_receive_report(slave, buf, len))
        {
            slave->last_report_tick = keyboard_tick_get32();
            slave->report_seen = true;
        }
        return;
    }
    nexus_receive_echo(slave_id, slave, buf, len);
}

/* --------------------------------------------------------------------------
 * Applying reports (keyboard tick)
 * -------------------------------------------------------------------------- */

#if !NEXUS_USE_RAW
/* keyboard_key_update() does nothing observable for a key whose state already
 * matches the slave's bit, whose debounce has settled and whose keycode is not
 * dispatched while unchanged (keyboard_keycode_dispatches_unchanged()). The
 * tick skips those keys, so its cost follows what is happening on the slaves
 * rather than how many keys they carry. The keycode is read here, at the
 * key's turn, so a layer switched by an earlier key in the same tick is seen
 * exactly as it would be inside keyboard_key_update(). */
static inline bool nexus_key_update_has_effect(const Key *key, bool state)
{
    /* These adjacent bytes can be checked with one halfword load. memcpy
     * keeps this valid for every alignment and under strict aliasing; the
     * repeated-byte expected value is also independent of endianness. The
     * layout fallback keeps the test correct if Key ever gains padding here. */
    if (offsetof(Key, report_state) == offsetof(Key, state) + 1)
    {
        uint16_t states;
        memcpy(&states, &key->state, sizeof(states));
        uint32_t unsettled = states ^ ((uint32_t)state * 0x0101U);
#if DEBOUNCE_PRESS > 0 || DEBOUNCE_RELEASE > 0
        unsettled |= (uint8_t)key->debounce;
#endif
        if (unsettled != 0)
        {
            return true;
        }
    }
    else
    {
        if (key->state != state || key->report_state != key->state)
        {
            return true;
        }
#if DEBOUNCE_PRESS > 0 || DEBOUNCE_RELEASE > 0
        if (key->debounce != 0)
        {
            return true;
        }
#endif
    }
    return keyboard_keycode_dispatches_unchanged(layer_cache_get_keycode(key->id));
}

static inline Key *const *nexus_bitmap_word_end(const NexusSlave *slave, uint32_t first)
{
#if NEXUS_BITMAP_WORDS > 1
    return slave->keys + NEXUS_MIN(slave->length, first + 32);
#else
    UNUSED(first);
    return slave->keys + slave->length;
#endif
}

static inline void nexus_apply_bitmap_word(NexusSlave *slave, uint32_t bitmap, uint32_t first)
{
    Key *const *next = slave->keys + first;
    Key *const *end = nexus_bitmap_word_end(slave, first);

    /* Resting words need no per-slot bit extraction, and their expected key
     * state is the constant zero. Still inspect every live key at its turn:
     * callbacks and layer changes can alter the keys that follow them. */
    if (bitmap == 0)
    {
        while (next < end)
        {
            Key *key = *next++;
            if (key != NULL && nexus_key_update_has_effect(key, false))
            {
                /* Keep the resting loop compact. Active words below inline
                 * the update to reuse their already-loaded key state. */
                keyboard_key_update(key, false);
                /* Only a dispatched update can call user code and change
                 * the mapping. Refresh the bound after it, preserving even
                 * a nexus_init() performed by a key callback. */
                end = nexus_bitmap_word_end(slave, first);
            }
        }
    }
    else
    {
        while (next < end)
        {
            Key *key = *next++;
            const bool state = bitmap & 1U;
            bitmap >>= 1;
            if (key != NULL && nexus_key_update_has_effect(key, state))
            {
                keyboard_key_update(key, state);
                end = nexus_bitmap_word_end(slave, first);
            }
        }
    }
}
#endif

void nexus_process(void)
{
    const uint32_t tick = keyboard_tick_get32();
#if NEXUS_USE_RAW
    for (uint16_t i = 0; i < ADVANCED_KEY_NUM; i++)
    {
        const uint8_t owner = nexus_key_owner[i];
        if (owner == NEXUS_NO_OWNER)
        {
            continue;   /* local key, scanned by the application */
        }
        AdvancedKey *advanced_key = &g_keyboard_advanced_keys[i];
        /* A vanished slave reads as resting keys. */
        const AnalogRawValue raw = nexus_slave_online_at(&nexus_slaves[owner], tick)
                                       ? nexus_remote_raw[i]
                                       : advanced_key->config.upper_bound;
        keyboard_advanced_key_update_raw(advanced_key, raw);
    }
#else
    for (uint8_t s = 0; s < NEXUS_SLAVE_NUM; s++)
    {
        NexusSlave *slave = &nexus_slaves[s];
        uint32_t bitmap[NEXUS_BITMAP_WORDS];

        if (nexus_slave_online_at(slave, tick))
        {
            for (uint16_t w = 0; w < NEXUS_BITMAP_WORDS; w++)
            {
                bitmap[w] = slave->bitmap[w];
            }
        }
        else
        {
            memset(bitmap, 0, sizeof(bitmap));   /* vanished slave: release */
        }

#if NEXUS_BITMAP_WORDS == 1
        nexus_apply_bitmap_word(slave, bitmap[0], 0);
#else
        for (uint32_t first = 0; first < slave->length; first += 32)
        {
            nexus_apply_bitmap_word(slave, bitmap[first / 32], first);
        }
#endif
    }
#endif
}

/* --------------------------------------------------------------------------
 * Requests and configuration sync (foreground)
 * -------------------------------------------------------------------------- */

/* Drop the echo waiting in g_nexus_slave_buffer, crediting the sync machine
 * first if the echo answers its outstanding request. */
static void nexus_discard_echo(uint8_t slave_id)
{
    NexusSlave *slave = &nexus_slaves[slave_id];
    if (!slave->response_ready)
    {
        return;
    }
    NEXUS_FENCE();
    const uint8_t *response = g_nexus_slave_buffer[slave_id];
    if (slave->request_in_flight && response[0] == PACKET_CODE_SET && response[1] == slave->request_id)
    {
        slave->request_acked = true;
    }
    slave->response_ready = false;
}

/* True when the echo of (code, id) sits in g_nexus_slave_buffer; an echo of
 * any other request is discarded. The caller clears response_ready once it
 * is done with the buffer. */
static bool nexus_take_echo(uint8_t slave_id, uint8_t code, uint8_t id)
{
    NexusSlave *slave = &nexus_slaves[slave_id];
    if (!slave->response_ready)
    {
        return false;
    }
    NEXUS_FENCE();
    const uint8_t *response = g_nexus_slave_buffer[slave_id];
    if (response[0] == code && response[1] == id)
    {
        return true;
    }
    nexus_discard_echo(slave_id);
    return false;
}

static void nexus_send_next_config(uint8_t slave_id, NexusSlave *slave, uint32_t tick)
{
    uint16_t slot = slave->next_slot;
    for (uint16_t n = 0; n < slave->length; n++, slot++)
    {
        if (slot >= slave->length)
        {
            slot = 0;
        }
        if (!slave->pending[slot])
        {
            continue;
        }
        /* Clear first, then read the configuration: an update that lands in
         * between raises the flag again and is sent on a later poll. */
        slave->pending[slot] = 0;
        NEXUS_FENCE();
        slave->next_slot = (uint16_t)(slot + 1);

        PacketAdvancedKey packet;
        memset(&packet, 0, sizeof(packet));
        packet.header.code = PACKET_CODE_SET;
        packet.header.id = nexus_next_id();
        packet.header.type = PACKET_DATA_ADVANCED_KEY;
        packet.index = slot;
        memcpy(&packet.data, &((AdvancedKey *)slave->keys[slot])->config, sizeof(AdvancedKeyConfiguration));

        nexus_discard_echo(slave_id);
        if (nexus_send(slave_id, (uint8_t *)&packet, sizeof(packet)) != 0)
        {
            slave->pending[slot] = 1;   /* transport busy, try again later */
            return;
        }
        slave->request_in_flight = true;
        slave->request_acked = false;
        slave->request_id = packet.header.id;
        slave->request_slot = slot;
        slave->request_tick = tick;
        return;
    }
}

void nexus_poll(void)
{
    const uint32_t tick = keyboard_tick_get32();
    for (uint8_t s = 0; s < NEXUS_SLAVE_NUM; s++)
    {
        NexusSlave *slave = &nexus_slaves[s];

        const bool online = nexus_slave_online_at(slave, tick);
        if (online && !slave->online)
        {
            /* A slave that (re)appeared may have rebooted: resend everything. */
            nexus_queue_slave_config(slave);
        }
        slave->online = online;

        if (slave->request_in_flight)
        {
            if (slave->request_acked || nexus_take_echo(s, PACKET_CODE_SET, slave->request_id))
            {
                slave->request_in_flight = false;
                slave->request_acked = false;
                slave->response_ready = false;
            }
            else if (tick_elapsed32(tick, slave->request_tick) >= NEXUS_REQUEST_TIMEOUT_TICKS)
            {
                slave->request_in_flight = false;
                slave->pending[slave->request_slot] = 1;
            }
        }
        if (!slave->request_in_flight && online)
        {
            nexus_send_next_config(s, slave, tick);
        }
    }
}

int nexus_sync_advanced_key_config(uint16_t key_index)
{
    if (key_index >= ADVANCED_KEY_NUM)
    {
        return 1;
    }
    const Key *key = &g_keyboard_advanced_keys[key_index].key;
    for (uint8_t s = 0; s < NEXUS_SLAVE_NUM; s++)
    {
        NexusSlave *slave = &nexus_slaves[s];
        for (uint16_t j = 0; j < slave->length; j++)
        {
            if (slave->keys[j] == key)
            {
                slave->pending[j] = 1;
            }
        }
    }
    return 0;
}

void nexus_calibrate(void)
{
    PacketEvent packet;
    memset(&packet, 0, sizeof(packet));
    packet.code = PACKET_CODE_EVENT;
    packet.flag = PACKET_EVENT_NO_EVENT;
    packet.event = KEYBOARD_EVENT_KEY_UP;
    packet.keycode = KEYCODE(KEYBOARD_OPERATION, KEYBOARD_CALIBRATE);
    packet.is_virtual = true;
    packet.use_keymap = false;

    for (uint8_t s = 0; s < NEXUS_SLAVE_NUM; s++)
    {
        (void)nexus_send(s, (uint8_t *)&packet, sizeof(packet));
    }
}

int nexus_request_timeout(uint8_t slave_id, const uint8_t *request,
                          uint16_t request_len, uint32_t timeout,
                          uint8_t *out_response, uint16_t response_capacity)
{
    uint8_t buffer[NEXUS_CONTROL_BUFFER_SIZE];
    if (slave_id >= NEXUS_SLAVE_NUM || request == NULL ||
        request_len < sizeof(PacketDataHeader) || request_len > sizeof(buffer))
    {
        return 1;
    }

    memset(buffer, 0, sizeof(buffer));
    memcpy(buffer, request, request_len);

    if (request[0] == PACKET_CODE_EVENT)
    {
        return nexus_send(slave_id, buffer, request_len);
    }

    NexusSlave *slave = &nexus_slaves[slave_id];
    const uint8_t id = nexus_next_id();
    buffer[1] = id;
    nexus_discard_echo(slave_id);

    const uint32_t start = keyboard_tick_get32();
    uint16_t send_failures = 0;
    bool sent = false;
    while (tick_elapsed32(keyboard_tick_get32(), start) < timeout)
    {
        if (!sent)
        {
            sent = nexus_send(slave_id, buffer, request_len) == 0;
            if (!sent)
            {
                if (++send_failures > NEXUS_RETRY_COUNT)
                {
                    return 1;
                }
                continue;
            }
        }
        if (nexus_take_echo(slave_id, request[0], id))
        {
            if (out_response != NULL && response_capacity != 0)
            {
                memcpy(out_response, g_nexus_slave_buffer[slave_id],
                       NEXUS_MIN(response_capacity, NEXUS_RX_BUFFER_SIZE));
            }
            memset(g_nexus_slave_buffer[slave_id], 0, NEXUS_RX_BUFFER_SIZE);
            slave->response_ready = false;
            return 0;
        }
    }
    return 1;
}

int nexus_send_timeout(uint8_t slave_id, const uint8_t *report, uint16_t len, uint32_t timeout)
{
    return nexus_request_timeout(slave_id, report, len, timeout, NULL, 0);
}

#else /* NEXUS_IS_SLAVE */

/* ==========================================================================
 * Slave
 * ========================================================================== */

#define NEXUS_LOCAL_KEY_COUNT          NEXUS_MIN(TOTAL_KEY_NUM, NEXUS_SLICE_LENGTH_MAX)
#define NEXUS_LOCAL_ADVANCED_KEY_COUNT NEXUS_MIN(ADVANCED_KEY_NUM, NEXUS_SLICE_LENGTH_MAX)

void nexus_process_buffer(uint8_t slave_id, uint8_t *buf, uint16_t len)
{
    UNUSED(slave_id);
    if (buf != NULL && len != 0)
    {
        packet_process(buf, len);
    }
}

int nexus_send_report(void)
{
#if NEXUS_USE_RAW
    static uint8_t frame[NEXUS_SLICE_LENGTH_MAX * sizeof(uint16_t)];
    AnalogRawValue raws[NEXUS_LOCAL_ADVANCED_KEY_COUNT > 0 ? NEXUS_LOCAL_ADVANCED_KEY_COUNT : 1];

    if (NEXUS_LOCAL_ADVANCED_KEY_COUNT == 0)
    {
        return 0;
    }
    /* keyboard_task() has already sampled every key this tick. */
    for (uint16_t i = 0; i < NEXUS_LOCAL_ADVANCED_KEY_COUNT; i++)
    {
        raws[i] = g_keyboard_advanced_keys[i].raw;
    }
    const uint16_t len = nexus_raw_report_encode(frame, raws, NEXUS_LOCAL_ADVANCED_KEY_COUNT);
    return nexus_report(frame, len);
#else
    static uint16_t cursor;
    static PacketNexus packet;

    if (NEXUS_LOCAL_KEY_COUNT == 0)
    {
        return 0;
    }
    Key *key = keyboard_get_key(cursor);
    nexus_report_encode(&packet, cursor, keyboard_get_key_raw_value(key),
                        keyboard_get_key_analog_value(key), g_keyboard_bitmap, NEXUS_LOCAL_KEY_COUNT);
    cursor = (uint16_t)(cursor + 1 < NEXUS_LOCAL_KEY_COUNT ? cursor + 1 : 0);
    return nexus_report((uint8_t *)&packet, sizeof(packet));
#endif
}

/* The master-side entry points do nothing on a slave, so application code can
 * call them without checking NEXUS_IS_SLAVE. */
void nexus_init(void) {}
void nexus_process(void) {}
void nexus_poll(void) {}
void nexus_calibrate(void) {}

int nexus_sync_advanced_key_config(uint16_t key_index)
{
    UNUSED(key_index);
    return 1;
}

bool nexus_slave_is_online(uint8_t slave_id)
{
    UNUSED(slave_id);
    return false;
}

int nexus_request_timeout(uint8_t slave_id, const uint8_t *request,
                          uint16_t request_len, uint32_t timeout,
                          uint8_t *out_response, uint16_t response_capacity)
{
    UNUSED(slave_id);
    UNUSED(request);
    UNUSED(request_len);
    UNUSED(timeout);
    UNUSED(out_response);
    UNUSED(response_capacity);
    return 1;
}

int nexus_send_timeout(uint8_t slave_id, const uint8_t *report, uint16_t len, uint32_t timeout)
{
    UNUSED(slave_id);
    UNUSED(report);
    UNUSED(len);
    UNUSED(timeout);
    return 1;
}

#endif /* NEXUS_IS_SLAVE */
