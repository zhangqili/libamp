/*
 * Copyright (c) 2026 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "packet.h"

#include "config_document.h"
#include "file_system.h"
#include "script.h"
#include "storage.h"
#include "string.h"

#if defined(STORAGE_ENABLE) && defined(LFS_ENABLE)

#ifndef SCRIPT_SOURCE_BUFFER_SIZE
#define SCRIPT_SOURCE_BUFFER_SIZE 0
#endif
#ifndef SCRIPT_BYTECODE_BUFFER_SIZE
#define SCRIPT_BYTECODE_BUFFER_SIZE 0
#endif

#define AMP_MAX2(a, b) ((a) > (b) ? (a) : (b))
#define AMP_OBJECT_MAX_SIZE AMP_MAX2(AMP_CONFIG_DOCUMENT_MAX_SIZE, \
    AMP_MAX2(SCRIPT_SOURCE_BUFFER_SIZE, SCRIPT_BYTECODE_BUFFER_SIZE))
#if AMP_OBJECT_TEMP_FILE_ENABLE
#define AMP_OBJECT_TEMPORARY_PATH "amp-object.tmp"
#endif

typedef struct {
    File file;
    bool active;
    uint16_t transaction_id;
    uint32_t total_size;
} AmpReadTransaction;

typedef struct {
    File file;
    bool active;
    uint16_t transaction_id;
    uint16_t object_type;
    uint16_t object_id;
    uint32_t expected_revision;
    uint32_t total_size;
#if AMP_OBJECT_CRC32_ENABLE
    uint32_t expected_crc32;
#endif
    uint32_t next_offset;
} AmpWriteTransaction;

static AmpReadTransaction read_transaction;
static AmpWriteTransaction write_transaction;
static uint16_t next_transaction_id = 1;
static uint32_t script_revisions[2];

#if AMP_OBJECT_CRC32_ENABLE
static uint32_t crc32_update(uint32_t crc, const uint8_t *data, size_t length)
{
    while (length-- != 0)
    {
        crc ^= *data++;
        for (uint8_t bit = 0; bit < 8; bit++)
        {
            crc = (crc >> 1) ^ (0xedb88320U & (uint32_t)-(int32_t)(crc & 1U));
        }
    }
    return crc;
}

static AmpStatus file_crc32(File *file, uint32_t *result)
{
    uint8_t buffer[64];
    uint32_t crc = 0xffffffffU;
    if (fs_seek(file, 0, FS_SEEK_SET) < 0)
    {
        return AMP_STATUS_IO_ERROR;
    }
    for (;;)
    {
        size_t actual = fs_read(file, buffer, sizeof(buffer));
        if (actual == 0)
        {
            break;
        }
        crc = crc32_update(crc, buffer, actual);
        if (actual < sizeof(buffer))
        {
            break;
        }
    }
    *result = crc ^ 0xffffffffU;
    return fs_seek(file, 0, FS_SEEK_SET) < 0 ? AMP_STATUS_IO_ERROR :
                                              AMP_STATUS_OK;
}
#endif

static uint16_t allocate_transaction_id(void)
{
    uint16_t result = next_transaction_id++;
    if (result == 0)
    {
        result = next_transaction_id++;
    }
    return result;
}

static uint32_t object_revision(uint16_t type, uint16_t id)
{
    if (type == AMP_OBJECT_CONFIG_PROFILE)
    {
        return storage_get_profile_revision(id);
    }
    if (type == AMP_OBJECT_SCRIPT_SOURCE)
    {
        return script_revisions[0];
    }
    if (type == AMP_OBJECT_SCRIPT_BYTECODE)
    {
        return script_revisions[1];
    }
    return 0;
}

static uint32_t bump_object_revision(uint16_t type, uint16_t id)
{
    if (type == AMP_OBJECT_CONFIG_PROFILE)
    {
        return storage_bump_profile_revision(id);
    }
    uint8_t index = type == AMP_OBJECT_SCRIPT_SOURCE ? 0 : 1;
    script_revisions[index]++;
    if (script_revisions[index] == 0)
    {
        script_revisions[index] = 1;
    }
    return script_revisions[index];
}

static AmpStatus object_info(uint16_t type, uint16_t id, char *final_path,
                             size_t final_size, uint32_t *max_size,
                             bool *optional)
{
#if defined(STORAGE_ENABLE) && defined(LFS_ENABLE)
    if (type == AMP_OBJECT_CONFIG_PROFILE)
    {
        if (!storage_profile_path(final_path, final_size, id))
        {
            return AMP_STATUS_INVALID_ARGUMENT;
        }
        *max_size = AMP_CONFIG_DOCUMENT_MAX_SIZE;
        *optional = false;
        return AMP_STATUS_OK;
    }
#if defined(SCRIPT_ENABLE)
    if (id != 0)
    {
        return AMP_STATUS_INVALID_ARGUMENT;
    }
#if SCRIPT_RUNTIME_STRATEGY == SCRIPT_JIT
    if (type == AMP_OBJECT_SCRIPT_SOURCE)
    {
        if (final_size < sizeof("scripts/main.js"))
        {
            return AMP_STATUS_NO_SPACE;
        }
        memcpy(final_path, "scripts/main.js", sizeof("scripts/main.js"));
        *max_size = SCRIPT_SOURCE_BUFFER_SIZE;
        *optional = true;
        return AMP_STATUS_OK;
    }
#endif
#if SCRIPT_RUNTIME_STRATEGY == SCRIPT_AOT
    if (type == AMP_OBJECT_SCRIPT_BYTECODE)
    {
        if (final_size < sizeof("scripts/main.bin"))
        {
            return AMP_STATUS_NO_SPACE;
        }
        memcpy(final_path, "scripts/main.bin", sizeof("scripts/main.bin"));
        *max_size = SCRIPT_BYTECODE_BUFFER_SIZE;
        *optional = true;
        return AMP_STATUS_OK;
    }
#endif
#endif
#else
    UNUSED(type);
    UNUSED(id);
    UNUSED(final_path);
    UNUSED(final_size);
    UNUSED(max_size);
    UNUSED(optional);
#endif
    return AMP_STATUS_UNSUPPORTED;
}

static void discard_write_object(void)
{
#if AMP_OBJECT_TEMP_FILE_ENABLE
    (void)fs_unlink(AMP_OBJECT_TEMPORARY_PATH);
#endif
}

static AmpStatus close_read(void)
{
    if (!read_transaction.active)
    {
        return AMP_STATUS_OK;
    }
    int result = fs_close(&read_transaction.file);
    memset(&read_transaction, 0, sizeof(read_transaction));
    return result < 0 ? AMP_STATUS_IO_ERROR : AMP_STATUS_OK;
}

static AmpStatus abort_write(void)
{
    if (!write_transaction.active)
    {
        return AMP_STATUS_OK;
    }
    int result = fs_close(&write_transaction.file);
    discard_write_object();
    memset(&write_transaction, 0, sizeof(write_transaction));
    return result < 0 ? AMP_STATUS_IO_ERROR : AMP_STATUS_OK;
}

void object_service_reset(void)
{
    (void)close_read();
    (void)abort_write();
}

static AmpStatus object_open_read(const uint8_t *request, uint16_t request_len,
                                  uint8_t *response, uint16_t *response_len)
{
    if (request_len != sizeof(AmpObjectOpenReadRequest))
    {
        return AMP_STATUS_INVALID_ARGUMENT;
    }
    AmpObjectOpenReadRequest input;
    memcpy(&input, request, sizeof(input));
    char path[32];
    uint32_t max_size;
    bool optional;
    AmpStatus status = object_info(input.object_type, input.object_id, path,
                                   sizeof(path), &max_size, &optional);
    UNUSED(max_size);
    if (status != AMP_STATUS_OK)
    {
        return status;
    }
    status = close_read();
    if (status != AMP_STATUS_OK)
    {
        return status;
    }
    int flags = optional ? FS_O_RDWR | FS_O_CREAT : FS_O_RDONLY;
    if (fs_open(&read_transaction.file, path, flags) < 0)
    {
        return AMP_STATUS_NOT_FOUND;
    }
    FilePosition size = fs_size(&read_transaction.file);
    if (size < 0 || (uint64_t)size > UINT32_MAX)
    {
        (void)fs_close(&read_transaction.file);
        memset(&read_transaction, 0, sizeof(read_transaction));
        return AMP_STATUS_IO_ERROR;
    }
    uint32_t crc = 0;
#if AMP_OBJECT_CRC32_ENABLE
    status = file_crc32(&read_transaction.file, &crc);
    if (status != AMP_STATUS_OK)
    {
        (void)fs_close(&read_transaction.file);
        memset(&read_transaction, 0, sizeof(read_transaction));
        return status;
    }
#endif
    read_transaction.active = true;
    read_transaction.transaction_id = allocate_transaction_id();
    read_transaction.total_size = (uint32_t)size;

    AmpObjectOpenReadResponse output = {
        .transaction_id = read_transaction.transaction_id,
        .revision = object_revision(input.object_type, input.object_id),
        .total_size = read_transaction.total_size,
        .crc32 = crc,
    };
    memcpy(response, &output, sizeof(output));
    *response_len = sizeof(output);
    return AMP_STATUS_OK;
}

static AmpStatus object_read(const uint8_t *request, uint16_t request_len,
                             uint8_t *response, uint16_t *response_len)
{
    if (request_len != sizeof(AmpObjectReadRequest))
    {
        return AMP_STATUS_INVALID_ARGUMENT;
    }
    AmpObjectReadRequest input;
    memcpy(&input, request, sizeof(input));
    if (!read_transaction.active ||
        input.transaction_id != read_transaction.transaction_id)
    {
        return AMP_STATUS_INVALID_STATE;
    }
    if (input.offset > read_transaction.total_size)
    {
        return AMP_STATUS_INVALID_ARGUMENT;
    }
    uint32_t remaining = read_transaction.total_size - input.offset;
    uint16_t length = input.requested_length;
    uint16_t response_capacity = amp_session_max_tx_payload();
    response_capacity = response_capacity > sizeof(input.offset) ?
        (uint16_t)(response_capacity - sizeof(input.offset)) : 0;
    if (length > response_capacity)
    {
        length = response_capacity;
    }
    if (length > remaining)
    {
        length = (uint16_t)remaining;
    }
    if (fs_seek(&read_transaction.file, input.offset, FS_SEEK_SET) < 0)
    {
        return AMP_STATUS_IO_ERROR;
    }
    memcpy(response, &input.offset, sizeof(input.offset));
    if (length != 0 &&
        fs_read(&read_transaction.file, response + sizeof(input.offset), length) != length)
    {
        return AMP_STATUS_IO_ERROR;
    }
    *response_len = (uint16_t)(sizeof(input.offset) + length);
    return AMP_STATUS_OK;
}

static AmpStatus object_close_read(const uint8_t *request, uint16_t request_len)
{
    if (request_len != sizeof(AmpObjectTransactionRequest))
    {
        return AMP_STATUS_INVALID_ARGUMENT;
    }
    AmpObjectTransactionRequest input;
    memcpy(&input, request, sizeof(input));
    if (!read_transaction.active ||
        input.transaction_id != read_transaction.transaction_id)
    {
        return AMP_STATUS_INVALID_STATE;
    }
    return close_read();
}

static AmpStatus object_open_write(const uint8_t *request, uint16_t request_len,
                                   uint8_t *response, uint16_t *response_len)
{
    if (request_len != sizeof(AmpObjectOpenWriteRequest))
    {
        return AMP_STATUS_INVALID_ARGUMENT;
    }
    AmpObjectOpenWriteRequest input;
    memcpy(&input, request, sizeof(input));
    char path[32];
    uint32_t max_size;
    bool optional;
    AmpStatus status = object_info(input.object_type, input.object_id, path,
                                   sizeof(path), &max_size, &optional);
    UNUSED(optional);
    if (status != AMP_STATUS_OK)
    {
        return status;
    }
    if (input.total_size > max_size || input.total_size > AMP_OBJECT_MAX_SIZE)
    {
        return AMP_STATUS_NO_SPACE;
    }
    if (input.expected_revision != object_revision(input.object_type,
                                                   input.object_id))
    {
        return AMP_STATUS_STALE_REVISION;
    }
    status = abort_write();
    if (status != AMP_STATUS_OK)
    {
        return status;
    }
    memset(&write_transaction, 0, sizeof(write_transaction));
#if AMP_OBJECT_TEMP_FILE_ENABLE
    (void)fs_unlink(AMP_OBJECT_TEMPORARY_PATH);
    const char *write_path = AMP_OBJECT_TEMPORARY_PATH;
#else
    const char *write_path = path;
#endif
    if (fs_open(&write_transaction.file, write_path,
                FS_O_RDWR | FS_O_CREAT | FS_O_TRUNC) < 0)
    {
        memset(&write_transaction, 0, sizeof(write_transaction));
        return AMP_STATUS_IO_ERROR;
    }
    write_transaction.active = true;
    write_transaction.transaction_id = allocate_transaction_id();
    write_transaction.object_type = input.object_type;
    write_transaction.object_id = input.object_id;
    write_transaction.expected_revision = input.expected_revision;
    write_transaction.total_size = input.total_size;
#if AMP_OBJECT_CRC32_ENABLE
    write_transaction.expected_crc32 = input.crc32;
#endif
    AmpObjectOpenWriteResponse output = {
        .transaction_id = write_transaction.transaction_id,
    };
    memcpy(response, &output, sizeof(output));
    *response_len = sizeof(output);
    return AMP_STATUS_OK;
}

static AmpStatus object_write(const uint8_t *request, uint16_t request_len)
{
    const uint16_t prefix_size = sizeof(uint16_t) + sizeof(uint32_t);
    if (request_len < prefix_size)
    {
        return AMP_STATUS_INVALID_ARGUMENT;
    }
    uint16_t transaction_id;
    uint32_t offset;
    memcpy(&transaction_id, request, sizeof(transaction_id));
    memcpy(&offset, request + sizeof(transaction_id), sizeof(offset));
    const uint8_t *data = request + prefix_size;
    uint16_t length = (uint16_t)(request_len - prefix_size);
    if (!write_transaction.active ||
        transaction_id != write_transaction.transaction_id)
    {
        return AMP_STATUS_INVALID_STATE;
    }
    if (offset != write_transaction.next_offset ||
        (uint64_t)offset + length > write_transaction.total_size)
    {
        return AMP_STATUS_INVALID_ARGUMENT;
    }
    if (fs_write(&write_transaction.file, (void *)data, length) != length)
    {
        (void)abort_write();
        return AMP_STATUS_IO_ERROR;
    }
    write_transaction.next_offset += length;
    return AMP_STATUS_OK;
}

static AmpStatus object_commit(const uint8_t *request, uint16_t request_len,
                               uint8_t *response, uint16_t *response_len)
{
    if (request_len != sizeof(AmpObjectTransactionRequest))
    {
        return AMP_STATUS_INVALID_ARGUMENT;
    }
    AmpObjectTransactionRequest input;
    memcpy(&input, request, sizeof(input));
    if (!write_transaction.active)
    {
        return AMP_STATUS_INVALID_STATE;
    }
    if (input.transaction_id != write_transaction.transaction_id)
    {
        return AMP_STATUS_INVALID_STATE;
    }
    if (write_transaction.next_offset != write_transaction.total_size)
    {
        return AMP_STATUS_INVALID_STATE;
    }
    char final_path[32];
    uint32_t max_size;
    bool optional;
    AmpStatus status = object_info(write_transaction.object_type,
                                   write_transaction.object_id,
                                   final_path, sizeof(final_path),
                                   &max_size, &optional);
    if (status != AMP_STATUS_OK)
    {
        (void)abort_write();
        return status;
    }
#if AMP_OBJECT_TEMP_FILE_ENABLE
    const char *stored_path = AMP_OBJECT_TEMPORARY_PATH;
#else
    const char *stored_path = final_path;
#endif
#if AMP_OBJECT_CRC32_ENABLE
    uint32_t actual_crc;
    status = file_crc32(&write_transaction.file, &actual_crc);
    if (status != AMP_STATUS_OK || actual_crc != write_transaction.expected_crc32)
    {
        (void)abort_write();
        return status == AMP_STATUS_OK ? AMP_STATUS_INTEGRITY_ERROR : status;
    }
#endif
    if (fs_sync(&write_transaction.file) < 0 ||
        fs_close(&write_transaction.file) < 0)
    {
        discard_write_object();
        memset(&write_transaction, 0, sizeof(write_transaction));
        return AMP_STATUS_IO_ERROR;
    }
    write_transaction.active = false;
    if (write_transaction.object_type == AMP_OBJECT_CONFIG_PROFILE)
    {
        status = amp_config_document_validate(stored_path,
                                              write_transaction.object_id);
        if (status != AMP_STATUS_OK)
        {
            discard_write_object();
            memset(&write_transaction, 0, sizeof(write_transaction));
            return status;
        }
    }
    if (object_revision(write_transaction.object_type,
                        write_transaction.object_id) !=
        write_transaction.expected_revision)
    {
        discard_write_object();
        memset(&write_transaction, 0, sizeof(write_transaction));
        return AMP_STATUS_STALE_REVISION;
    }
#if AMP_OBJECT_TEMP_FILE_ENABLE
    if (fs_rename(AMP_OBJECT_TEMPORARY_PATH, final_path) < 0)
    {
        (void)fs_unlink(AMP_OBJECT_TEMPORARY_PATH);
        memset(&write_transaction, 0, sizeof(write_transaction));
        return AMP_STATUS_IO_ERROR;
    }
#endif
    uint16_t type = write_transaction.object_type;
    uint16_t id = write_transaction.object_id;
    uint32_t revision = bump_object_revision(type, id);
    if (revision == 0)
    {
        memset(&write_transaction, 0, sizeof(write_transaction));
        return AMP_STATUS_IO_ERROR;
    }
    if (type == AMP_OBJECT_CONFIG_PROFILE && id == g_current_profile_index)
    {
        status = amp_config_document_apply(final_path, id);
        if (status != AMP_STATUS_OK)
        {
            memset(&write_transaction, 0, sizeof(write_transaction));
            return status;
        }
    }
    memset(&write_transaction, 0, sizeof(write_transaction));
    if (type == AMP_OBJECT_CONFIG_PROFILE)
    {
        packet_notify_profile_changed(id, revision);
    }
    AmpObjectCommitResponse output = {.new_revision = revision};
    memcpy(response, &output, sizeof(output));
    *response_len = sizeof(output);
    return AMP_STATUS_OK;
}

static AmpStatus object_abort(const uint8_t *request, uint16_t request_len)
{
    if (request_len != sizeof(AmpObjectTransactionRequest))
    {
        return AMP_STATUS_INVALID_ARGUMENT;
    }
    AmpObjectTransactionRequest input;
    memcpy(&input, request, sizeof(input));
    if (write_transaction.active &&
        input.transaction_id == write_transaction.transaction_id)
    {
        return abort_write();
    }
    if (read_transaction.active &&
        input.transaction_id == read_transaction.transaction_id)
    {
        return close_read();
    }
    return AMP_STATUS_INVALID_STATE;
}

AmpStatus object_service_process(uint16_t opcode, const uint8_t *request,
                                 uint16_t request_len, uint8_t *response,
                                 uint16_t *response_len)
{
    if (response_len == NULL)
    {
        return AMP_STATUS_INVALID_ARGUMENT;
    }
    *response_len = 0;
    switch (opcode)
    {
    case AMP_OBJECT_OPEN_READ:
        return object_open_read(request, request_len, response, response_len);
    case AMP_OBJECT_READ:
        return object_read(request, request_len, response, response_len);
    case AMP_OBJECT_CLOSE_READ:
        return object_close_read(request, request_len);
    case AMP_OBJECT_OPEN_WRITE:
        return object_open_write(request, request_len, response, response_len);
    case AMP_OBJECT_WRITE:
        return object_write(request, request_len);
    case AMP_OBJECT_COMMIT:
        return object_commit(request, request_len, response, response_len);
    case AMP_OBJECT_ABORT:
        return object_abort(request, request_len);
    default:
        return AMP_STATUS_UNSUPPORTED;
    }
}

#else

void object_service_reset(void)
{
}

AmpStatus object_service_process(uint16_t opcode, const uint8_t *request,
                                 uint16_t request_len, uint8_t *response,
                                 uint16_t *response_len)
{
    UNUSED(opcode);
    UNUSED(request);
    UNUSED(request_len);
    UNUSED(response);
    if (response_len != NULL)
    {
        *response_len = 0;
    }
    return AMP_STATUS_UNSUPPORTED;
}

#endif
