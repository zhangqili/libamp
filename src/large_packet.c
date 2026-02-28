/*
 * Copyright (c) 2026 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "packet.h"
#include <string.h> // for memcpy

#include "script.h"
#include "storage.h"

#define LARGE_PKT_HDR_SIZE 9 // code(1)+type(1)+sub(1)+offset(4)+len(2)
#define MAX_PAYLOAD_SIZE (64 - LARGE_PKT_HDR_SIZE)

enum
{
    LARGE_DATA_CMD_START = 0,
    LARGE_DATA_CMD_PAYLOAD = 1,
    LARGE_DATA_CMD_END = 2,
    LARGE_DATA_CMD_ABORT = 3,
};

uint32_t script_source_handle_large_data(uint8_t code, uint8_t sub_cmd, uint32_t val, uint8_t *data, uint16_t len)
{
#if defined(LFS_ENABLE) && defined(STORAGE_ENABLE)
    static const char *SCRIPT_FILENAME = "main.js";
    static lfs_file_t script_file;
    static bool script_file_open = false;
    lfs_t *_lfs = storage_get_lfs();
    if (code == PACKET_CODE_LARGE_SET)
    {
        switch (sub_cmd)
        {
        case LARGE_DATA_CMD_START:
            if (script_file_open)
            {
                lfs_file_close(_lfs, &script_file);
            }

            int err = lfs_file_open(_lfs, &script_file, SCRIPT_FILENAME, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC);
            if (err < 0)
            {
                return false;
            }
            script_file_open = true;
            return 0;

        case LARGE_DATA_CMD_PAYLOAD:
            if (!script_file_open)
            {
                return false;
            }
            if (lfs_file_seek(_lfs, &script_file, val, LFS_SEEK_SET) < 0)
            {
                return false;
            }
            if (lfs_file_write(_lfs, &script_file, data, len) < 0)
            {
                return false;
            }
            return 0;

        case LARGE_DATA_CMD_END:
        case LARGE_DATA_CMD_ABORT:
            if (script_file_open)
            {
                lfs_file_close(_lfs, &script_file);
                script_file_open = false;
            }
#if SCRIPT_RUNTIME_STRATEGY == SCRIPT_JIT
            script_init();
#endif
            return 0;
        }
    }
    else if (code == PACKET_CODE_LARGE_GET)
    {
        switch (sub_cmd)
        {
        case LARGE_DATA_CMD_START:
            if (val == 0)
            {
                if (script_file_open)
                {
                    lfs_file_close(_lfs, &script_file);
                }

                int err = lfs_file_open(_lfs, &script_file, SCRIPT_FILENAME, LFS_O_RDONLY);
                if (err < 0)
                {
                    return 0;
                }
                script_file_open = true;
                return lfs_file_size(_lfs, &script_file);
            }
            else if (val == 1)
            {
                if (!script_file_open)
                    return 0;

                if (lfs_file_seek(_lfs, &script_file, val, LFS_SEEK_SET) < 0)
                {
                    return 0;
                }

                lfs_ssize_t read_len = lfs_file_read(_lfs, &script_file, data, len);
                if (read_len < 0)
                {
                    return 0;
                }
                return (uint16_t)read_len;
            }
            return 0;

        case LARGE_DATA_CMD_PAYLOAD:
            if (!script_file_open)
            {
                return 0;
            }

            if (lfs_file_seek(_lfs, &script_file, val, LFS_SEEK_SET) < 0)
            {
                return 0;
            }

            lfs_ssize_t read_len = lfs_file_read(_lfs, &script_file, data, len);
            if (read_len < 0)
            {
                return 0;
            }
            return (uint16_t)read_len;
        case LARGE_DATA_CMD_END:
        case LARGE_DATA_CMD_ABORT:
            if (script_file_open)
            {
                lfs_file_close(_lfs, &script_file);
                script_file_open = false;
            }
            return 0;
        }
    }
#endif
    return 0;
}

uint32_t script_bytecode_handle_large_data(uint8_t code, uint8_t sub_cmd, uint32_t val, uint8_t *data, uint16_t len)
{
#if defined(LFS_ENABLE) && defined(STORAGE_ENABLE)
#if SCRIPT_RUNTIME_STRATEGY == SCRIPT_AOT
    static const char *SCRIPT_FILENAME = "main.bin";
    static lfs_file_t script_file;
    static bool script_file_open = false;
    lfs_t *_lfs = storage_get_lfs();
    if (code == PACKET_CODE_LARGE_SET)
    {
        switch (sub_cmd)
        {
        case LARGE_DATA_CMD_START:
            if (script_file_open)
            {
                lfs_file_close(_lfs, &script_file);
            }
            lfs_remove(_lfs, SCRIPT_FILENAME);
            int err = lfs_file_open(_lfs, &script_file, SCRIPT_FILENAME, LFS_O_WRONLY | LFS_O_CREAT | LFS_O_TRUNC);
            if (err < 0)
            {
                return false;
            }
            script_file_open = true;
            return 0;

        case LARGE_DATA_CMD_PAYLOAD:
            if (!script_file_open)
            {
                return false;
            }
            if (lfs_file_seek(_lfs, &script_file, val, LFS_SEEK_SET) < 0)
            {
                return false;
            }
            if (lfs_file_write(_lfs, &script_file, data, len) < 0)
            {
                return false;
            }
            return 0;

        case LARGE_DATA_CMD_END:
        case LARGE_DATA_CMD_ABORT:
            if (script_file_open)
            {
                lfs_file_close(_lfs, &script_file);
                script_file_open = false;
            }
            script_init();
            return 0;
        }
    }
    else if (code == PACKET_CODE_LARGE_GET)
    {
        switch (sub_cmd)
        {
        case LARGE_DATA_CMD_START:
            if (val == 0)
            {
                if (script_file_open)
                {
                    lfs_file_close(_lfs, &script_file);
                }

                int err = lfs_file_open(_lfs, &script_file, SCRIPT_FILENAME, LFS_O_RDONLY);
                if (err < 0)
                {
                    return 0;
                }
                script_file_open = true;
                return lfs_file_size(_lfs, &script_file);
            }
            else if (val == 1)
            {
                if (!script_file_open)
                    return 0;

                if (lfs_file_seek(_lfs, &script_file, val, LFS_SEEK_SET) < 0)
                {
                    return 0;
                }

                lfs_ssize_t read_len = lfs_file_read(_lfs, &script_file, data, len);
                if (read_len < 0)
                {
                    return 0;
                }
                return (uint16_t)read_len;
            }
            return 0;

        case LARGE_DATA_CMD_PAYLOAD:
            if (!script_file_open)
            {
                return 0;
            }

            if (lfs_file_seek(_lfs, &script_file, val, LFS_SEEK_SET) < 0)
            {
                return 0;
            }

            lfs_ssize_t read_len = lfs_file_read(_lfs, &script_file, data, len);
            if (read_len < 0)
            {
                return 0;
            }
            return (uint16_t)read_len;
        case LARGE_DATA_CMD_END:
        case LARGE_DATA_CMD_ABORT:
            if (script_file_open)
            {
                lfs_file_close(_lfs, &script_file);
                script_file_open = false;
            }
            return 0;
        }
    }
#endif
#endif
    return 0;
}

uint32_t large_packet_dispatch(uint8_t type, uint8_t code, uint8_t sub_cmd, uint32_t val, uint8_t *data, uint16_t len)
{
    switch (type)
    {
#ifdef SCRIPT_ENABLE
    case PACKET_DATA_SCRIPT_SCOURCE:
        return script_source_handle_large_data(code, sub_cmd, val, data, len);
    case PACKET_DATA_SCRIPT_BYTECODE:
        return script_bytecode_handle_large_data(code, sub_cmd, val, data, len);
#endif
    default:
        return 0;
    }
}

static uint32_t large_rx_total = 0;
static uint32_t large_rx_recv = 0;
static uint8_t large_rx_type = 0;

static void process_large_set(PacketLargeData *pkt)
{
    uint8_t sub_cmd = pkt->sub_cmd;
    uint8_t type = pkt->type;

    if (sub_cmd == LARGE_DATA_CMD_START)
    {
        uint32_t total_size = pkt->header.total_size;

        large_rx_type = type;
        large_rx_total = total_size;
        large_rx_recv = 0;

        large_packet_dispatch(type, PACKET_CODE_LARGE_SET, LARGE_DATA_CMD_START, total_size, NULL, 0);
    }
    else if (sub_cmd == LARGE_DATA_CMD_PAYLOAD)
    {
        uint32_t offset = pkt->payload.offset;
        uint16_t len = pkt->payload.length;

        if (len > MAX_PAYLOAD_SIZE || offset != large_rx_recv)
        {
            large_packet_dispatch(type, PACKET_CODE_LARGE_SET, LARGE_DATA_CMD_ABORT, 0, NULL, 0);
            return;
        }
        large_packet_dispatch(type, PACKET_CODE_LARGE_SET, LARGE_DATA_CMD_PAYLOAD, offset, pkt->payload.data, len);
        large_rx_recv += len;
        if (large_rx_recv >= large_rx_total)
        {
            large_packet_dispatch(type, PACKET_CODE_LARGE_SET, LARGE_DATA_CMD_END, 0, NULL, 0);
            large_rx_total = 0;
            large_rx_recv = 0;
        }
    }
    else if (sub_cmd == LARGE_DATA_CMD_ABORT)
    {
        large_packet_dispatch(type, PACKET_CODE_LARGE_SET, LARGE_DATA_CMD_ABORT, 0, NULL, 0);
        large_rx_total = 0;
        large_rx_recv = 0;
    }
}

static void process_large_get(PacketLargeData *pkt)
{
    uint8_t sub_cmd = pkt->sub_cmd;
    uint8_t type = pkt->type;

    if (sub_cmd == LARGE_DATA_CMD_START)
    {
        uint32_t size = large_packet_dispatch(type, PACKET_CODE_LARGE_GET, LARGE_DATA_CMD_START, 0, NULL, 0);
        uint32_t checksum = large_packet_dispatch(type, PACKET_CODE_LARGE_GET, LARGE_DATA_CMD_START, 1, NULL, 0);

        pkt->header.total_size = size;
        pkt->header.checksum = checksum;
    }
    else if (sub_cmd == LARGE_DATA_CMD_PAYLOAD)
    {
        uint32_t offset = pkt->payload.offset;
        uint16_t req_len = pkt->payload.length;

        if (req_len > MAX_PAYLOAD_SIZE)
            req_len = MAX_PAYLOAD_SIZE;

        uint16_t actual_len = (uint16_t)large_packet_dispatch(type, PACKET_CODE_LARGE_GET, LARGE_DATA_CMD_PAYLOAD, offset, pkt->payload.data, req_len);

        pkt->payload.length = actual_len;
    }
    else if (sub_cmd == LARGE_DATA_CMD_ABORT)
    {
        large_packet_dispatch(type, PACKET_CODE_LARGE_GET, LARGE_DATA_CMD_ABORT, 0, NULL, 0);
    }
}

void large_packet_process(PacketLargeData *buf)
{
    if (buf->code == PACKET_CODE_LARGE_SET)
    {
        process_large_set(buf);
    }
    else if (buf->code == PACKET_CODE_LARGE_GET)
    {
        process_large_get(buf);
    }
}