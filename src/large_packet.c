/*
 * Copyright (c) 2026 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "packet.h"

#include "file_system.h"
#include "script.h"
#include "storage.h"
#include <string.h>

typedef struct {
    File file;
    bool active;
    bool write;
    uint8_t type;
    uint32_t total_size;
    uint32_t offset;
    const char *final_name;
    const char *temp_name;
} LargeTransfer;

static LargeTransfer transfer;

static AmpStatus large_object_info(uint8_t type, const char **final_name,
                                   const char **temp_name, uint32_t *max_size)
{
#if defined(SCRIPT_ENABLE) && defined(LFS_ENABLE) && defined(STORAGE_ENABLE)
    if (type == PACKET_DATA_SCRIPT_SCOURCE)
    {
        *final_name = "scripts/main.js";
        *temp_name = "scripts/main.js.tmp";
        *max_size = SCRIPT_SOURCE_BUFFER_SIZE;
        return AMP_STATUS_OK;
    }
#if SCRIPT_RUNTIME_STRATEGY == SCRIPT_AOT
    if (type == PACKET_DATA_SCRIPT_BYTECODE)
    {
        *final_name = "scripts/main.bin";
        *temp_name = "scripts/main.bin.tmp";
        *max_size = SCRIPT_BYTECODE_BUFFER_SIZE;
        return AMP_STATUS_OK;
    }
#endif
#else
    UNUSED(type);
    UNUSED(final_name);
    UNUSED(temp_name);
    UNUSED(max_size);
#endif
    return AMP_STATUS_UNSUPPORTED;
}

static void large_reset(void)
{
    memset(&transfer, 0, sizeof(transfer));
}

static AmpStatus large_abort(void)
{
    if (!transfer.active)
    {
        return AMP_STATUS_OK;
    }
    bool write = transfer.write;
    const char *temp_name = transfer.temp_name;
    int close_result = fs_close(&transfer.file);
    if (write && temp_name != NULL)
    {
        (void)fs_unlink(temp_name);
    }
    large_reset();
    return close_result < 0 ? AMP_STATUS_IO_ERROR : AMP_STATUS_OK;
}

static AmpStatus process_set_start(const AmpFrame *request)
{
    const PacketLargeStart *body = (const PacketLargeStart *)request->body;
    const char *final_name = NULL;
    const char *temp_name = NULL;
    uint32_t max_size = 0;
    AmpStatus status = large_object_info(request->header.type, &final_name,
                                         &temp_name, &max_size);
    if (status != AMP_STATUS_OK)
    {
        return status;
    }
    if (body->total_size > max_size)
    {
        return AMP_STATUS_INVALID_ARGUMENT;
    }

    /*
     * START is also the recovery boundary for an abandoned transfer.  A host
     * can disappear without being able to send ABORT (for example when its
     * browser tab is closed), so a valid new START must not remain blocked by
     * stale in-memory session state.
     */
    status = large_abort();
    if (status != AMP_STATUS_OK)
    {
        return status;
    }

    (void)fs_unlink(temp_name);
    if (fs_open(&transfer.file, temp_name,
                FS_O_WRONLY | FS_O_CREAT | FS_O_TRUNC) < 0)
    {
        large_reset();
        return AMP_STATUS_IO_ERROR;
    }
    transfer.active = true;
    transfer.write = true;
    transfer.type = request->header.type;
    transfer.total_size = body->total_size;
    transfer.final_name = final_name;
    transfer.temp_name = temp_name;
    return AMP_STATUS_OK;
}

static AmpStatus process_set_payload(const AmpFrame *request)
{
    const PacketLargePayload *body = (const PacketLargePayload *)request->body;
    if (!transfer.active || !transfer.write || transfer.type != request->header.type)
    {
        return AMP_STATUS_INVALID_STATE;
    }
    if (body->chunk_length > PACKET_LARGE_CHUNK_SIZE || body->offset != transfer.offset ||
        (uint64_t)body->offset + body->chunk_length > transfer.total_size)
    {
        return AMP_STATUS_INVALID_ARGUMENT;
    }
    if (fs_seek(&transfer.file, body->offset, FS_SEEK_SET) < 0 ||
        fs_write(&transfer.file, (void *)body->data, body->chunk_length) != body->chunk_length)
    {
        (void)large_abort();
        return AMP_STATUS_IO_ERROR;
    }
    transfer.offset += body->chunk_length;
    return AMP_STATUS_OK;
}

static AmpStatus process_set_end(const AmpFrame *request)
{
    if (!transfer.active || !transfer.write || transfer.type != request->header.type)
    {
        return AMP_STATUS_INVALID_STATE;
    }
    if (transfer.offset != transfer.total_size)
    {
        return AMP_STATUS_INVALID_STATE;
    }

    const char *temp_name = transfer.temp_name;
    const char *final_name = transfer.final_name;
    int sync_result = fs_sync(&transfer.file);
    int close_result = fs_close(&transfer.file);
    if (sync_result < 0 || close_result < 0)
    {
        (void)fs_unlink(temp_name);
        large_reset();
        return AMP_STATUS_IO_ERROR;
    }
    if (fs_rename(temp_name, final_name) < 0)
    {
        (void)fs_unlink(temp_name);
        large_reset();
        return AMP_STATUS_IO_ERROR;
    }
    large_reset();
    return AMP_STATUS_OK;
}

static AmpStatus process_large_set(const AmpFrame *request)
{
    const PacketLarge *body = (const PacketLarge *)request->body;
    switch (body->sub_cmd)
    {
    case LARGE_DATA_CMD_START:
        return process_set_start(request);
    case LARGE_DATA_CMD_PAYLOAD:
        return process_set_payload(request);
    case LARGE_DATA_CMD_END:
        return process_set_end(request);
    case LARGE_DATA_CMD_ABORT:
        return large_abort();
    default:
        return AMP_STATUS_INVALID_ARGUMENT;
    }
}

static AmpStatus process_get_start(const AmpFrame *request, AmpFrame *response)
{
    const char *final_name = NULL;
    const char *temp_name = NULL;
    uint32_t max_size = 0;
    AmpStatus status = large_object_info(request->header.type, &final_name,
                                         &temp_name, &max_size);
    UNUSED(temp_name);
    UNUSED(max_size);
    if (status != AMP_STATUS_OK)
    {
        return status;
    }
    status = large_abort();
    if (status != AMP_STATUS_OK)
    {
        return status;
    }
    /* Script objects are optional. Treat a missing object as an empty file so
       configuration discovery is not blocked after a factory reset. */
    if (fs_open(&transfer.file, final_name, FS_O_RDWR | FS_O_CREAT) < 0)
    {
        large_reset();
        return AMP_STATUS_IO_ERROR;
    }
    FilePosition size = fs_size(&transfer.file);
    if (size < 0)
    {
        (void)fs_close(&transfer.file);
        large_reset();
        return AMP_STATUS_IO_ERROR;
    }

    transfer.active = true;
    transfer.write = false;
    transfer.type = request->header.type;
    transfer.total_size = (uint32_t)size;
    transfer.final_name = final_name;
    PacketLargeStart *out = (PacketLargeStart *)response->body;
    out->sub_cmd = LARGE_DATA_CMD_START;
    out->total_size = transfer.total_size;
    return AMP_STATUS_OK;
}

static AmpStatus process_get_payload(const AmpFrame *request, AmpFrame *response)
{
    const PacketLargePayload *body = (const PacketLargePayload *)request->body;
    if (!transfer.active || transfer.write || transfer.type != request->header.type)
    {
        return AMP_STATUS_INVALID_STATE;
    }
    if (body->chunk_length > PACKET_LARGE_CHUNK_SIZE ||
        body->offset > transfer.total_size)
    {
        return AMP_STATUS_INVALID_ARGUMENT;
    }

    uint32_t remaining = transfer.total_size - body->offset;
    uint8_t read_length = body->chunk_length;
    if (read_length > remaining)
    {
        read_length = (uint8_t)remaining;
    }
    PacketLargePayload *out = (PacketLargePayload *)response->body;
    out->sub_cmd = LARGE_DATA_CMD_PAYLOAD;
    out->offset = body->offset;
    if (fs_seek(&transfer.file, body->offset, FS_SEEK_SET) < 0)
    {
        return AMP_STATUS_IO_ERROR;
    }
    size_t actual = fs_read(&transfer.file, out->data, read_length);
    if (actual != read_length)
    {
        return AMP_STATUS_IO_ERROR;
    }
    out->chunk_length = (uint8_t)actual;
    transfer.offset = body->offset + (uint32_t)actual;
    return AMP_STATUS_OK;
}

static AmpStatus process_get_end(const AmpFrame *request)
{
    if (!transfer.active || transfer.write || transfer.type != request->header.type)
    {
        return AMP_STATUS_INVALID_STATE;
    }
    int result = fs_close(&transfer.file);
    large_reset();
    return result < 0 ? AMP_STATUS_IO_ERROR : AMP_STATUS_OK;
}

static AmpStatus process_large_get(const AmpFrame *request, AmpFrame *response)
{
    const PacketLarge *body = (const PacketLarge *)request->body;
    switch (body->sub_cmd)
    {
    case LARGE_DATA_CMD_START:
        return process_get_start(request, response);
    case LARGE_DATA_CMD_PAYLOAD:
        return process_get_payload(request, response);
    case LARGE_DATA_CMD_END:
        return process_get_end(request);
    case LARGE_DATA_CMD_ABORT:
        return large_abort();
    default:
        return AMP_STATUS_INVALID_ARGUMENT;
    }
}

AmpStatus large_packet_process(const AmpFrame *request, AmpFrame *response)
{
    if (request == NULL || response == NULL)
    {
        return AMP_STATUS_INVALID_ARGUMENT;
    }
    if (request->header.code == PACKET_CODE_LARGE_SET)
    {
        return process_large_set(request);
    }
    if (request->header.code == PACKET_CODE_LARGE_GET)
    {
        return process_large_get(request, response);
    }
    return AMP_STATUS_UNSUPPORTED;
}
