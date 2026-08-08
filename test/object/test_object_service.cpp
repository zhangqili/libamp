#include <gtest/gtest.h>

#include <algorithm>
#include <cstring>
#include <vector>

extern "C" {
#include "config_document.h"
#include "packet.h"
#include "storage.h"
}

namespace {

AmpStatus object_call(uint16_t opcode, const void *request, uint16_t request_len,
                      uint8_t *response = nullptr, uint16_t *response_len = nullptr)
{
    uint8_t local_response[AMP_FRAME_PAYLOAD_SIZE] = {};
    uint16_t local_length = 0;
    AmpStatus status = object_service_process(
        opcode, static_cast<const uint8_t *>(request), request_len,
        response == nullptr ? local_response : response,
        response_len == nullptr ? &local_length : response_len);
    return status;
}

std::vector<uint8_t> read_profile(uint16_t profile, uint32_t *revision = nullptr)
{
    AmpObjectOpenReadRequest open = {AMP_OBJECT_CONFIG_PROFILE, profile};
    uint8_t response[AMP_FRAME_PAYLOAD_SIZE] = {};
    uint16_t response_len = 0;
    EXPECT_EQ(AMP_STATUS_OK, object_call(AMP_OBJECT_OPEN_READ, &open, sizeof(open),
                                        response, &response_len));
    EXPECT_EQ(sizeof(AmpObjectOpenReadResponse), response_len);
    AmpObjectOpenReadResponse metadata;
    std::memcpy(&metadata, response, sizeof(metadata));
    if (revision != nullptr) *revision = metadata.revision;
    std::vector<uint8_t> result(metadata.total_size);
    for (uint32_t offset = 0; offset < metadata.total_size;)
    {
        AmpObjectReadRequest request = {
            metadata.transaction_id,
            offset,
            static_cast<uint16_t>(std::min<uint32_t>(48, metadata.total_size - offset)),
        };
        if (object_call(AMP_OBJECT_READ, &request, sizeof(request),
                        response, &response_len) != AMP_STATUS_OK)
        {
            ADD_FAILURE() << "OBJECT_READ failed at offset " << offset;
            return {};
        }
        uint32_t actual_offset;
        std::memcpy(&actual_offset, response, sizeof(actual_offset));
        if (actual_offset != offset || response_len <= sizeof(actual_offset))
        {
            ADD_FAILURE() << "Invalid OBJECT_READ response at offset " << offset;
            return {};
        }
        std::memcpy(result.data() + offset, response + sizeof(actual_offset),
                    response_len - sizeof(actual_offset));
        offset += response_len - sizeof(actual_offset);
    }
    AmpObjectTransactionRequest close = {metadata.transaction_id};
    EXPECT_EQ(AMP_STATUS_OK,
              object_call(AMP_OBJECT_CLOSE_READ, &close, sizeof(close)));
    return result;
}

uint32_t write_profile(uint16_t profile, uint32_t revision,
                       const std::vector<uint8_t> &document)
{
    /* CRC helper mirrors the protocol's standard IEEE CRC32. */
    uint32_t crc = 0xffffffffU;
    for (uint8_t value : document)
    {
        crc ^= value;
        for (uint8_t bit = 0; bit < 8; bit++)
        {
            crc = (crc >> 1) ^ (0xedb88320U &
                                static_cast<uint32_t>(-
                                    static_cast<int32_t>(crc & 1U)));
        }
    }
    crc ^= 0xffffffffU;
    AmpObjectOpenWriteRequest open = {
        AMP_OBJECT_CONFIG_PROFILE,
        profile,
        revision,
        static_cast<uint32_t>(document.size()),
        crc,
    };
    uint8_t response[AMP_FRAME_PAYLOAD_SIZE] = {};
    uint16_t response_len = 0;
    EXPECT_EQ(AMP_STATUS_OK, object_call(AMP_OBJECT_OPEN_WRITE, &open, sizeof(open),
                                        response, &response_len));
    AmpObjectOpenWriteResponse opened;
    std::memcpy(&opened, response, sizeof(opened));
    for (uint32_t offset = 0; offset < document.size(); offset += 46)
    {
        uint16_t length = static_cast<uint16_t>(
            std::min<size_t>(46, document.size() - offset));
        uint8_t request[AMP_FRAME_PAYLOAD_SIZE] = {};
        std::memcpy(request, &opened.transaction_id, sizeof(opened.transaction_id));
        std::memcpy(request + 2, &offset, sizeof(offset));
        std::memcpy(request + 6, document.data() + offset, length);
        EXPECT_EQ(AMP_STATUS_OK,
                  object_call(AMP_OBJECT_WRITE, request, 6 + length));
    }
    AmpObjectTransactionRequest commit = {opened.transaction_id};
    EXPECT_EQ(AMP_STATUS_OK, object_call(AMP_OBJECT_COMMIT, &commit, sizeof(commit),
                                        response, &response_len));
    AmpObjectCommitResponse committed;
    std::memcpy(&committed, response, sizeof(committed));
    return committed.new_revision;
}

} // namespace

TEST(ObjectServiceV4, ReadsImmutableConfigSnapshot)
{
    uint32_t revision;
    std::vector<uint8_t> document = read_profile(0, &revision);
    ASSERT_GE(document.size(), sizeof(AmpConfigDocumentHeader));
    AmpConfigDocumentHeader header;
    std::memcpy(&header, document.data(), sizeof(header));
    EXPECT_EQ(AMP_CONFIG_DOCUMENT_MAGIC, header.magic);
    EXPECT_EQ(AMP_CONFIG_SCHEMA_VERSION, header.schema_version);
    EXPECT_EQ(0, header.profile_id);
    EXPECT_EQ(storage_get_profile_revision(0), revision);
}

TEST(ObjectServiceV4, AtomicallyCommitsAndIncrementsRevision)
{
    uint32_t revision;
    std::vector<uint8_t> document = read_profile(0, &revision);
    uint32_t new_revision = write_profile(0, revision, document);
    EXPECT_GT(new_revision, revision);
    uint32_t reread_revision;
    EXPECT_EQ(document, read_profile(0, &reread_revision));
    EXPECT_EQ(new_revision, reread_revision);
}

TEST(ObjectServiceV4, RejectsStaleRevision)
{
    uint32_t revision;
    std::vector<uint8_t> document = read_profile(0, &revision);
    (void)write_profile(0, revision, document);
    AmpObjectOpenWriteRequest stale = {
        AMP_OBJECT_CONFIG_PROFILE,
        0,
        revision,
        static_cast<uint32_t>(document.size()),
        0,
    };
    EXPECT_EQ(AMP_STATUS_STALE_REVISION,
              object_call(AMP_OBJECT_OPEN_WRITE, &stale, sizeof(stale)));
}

TEST(ObjectServiceV4, SessionResetAbortsOpenWrite)
{
    uint32_t revision;
    std::vector<uint8_t> document = read_profile(0, &revision);
    AmpObjectOpenWriteRequest open = {
        AMP_OBJECT_CONFIG_PROFILE,
        0,
        revision,
        static_cast<uint32_t>(document.size()),
        0,
    };
    uint8_t response[AMP_FRAME_PAYLOAD_SIZE] = {};
    uint16_t response_len = 0;
    ASSERT_EQ(AMP_STATUS_OK, object_call(AMP_OBJECT_OPEN_WRITE, &open, sizeof(open),
                                        response, &response_len));
    AmpObjectOpenWriteResponse opened;
    std::memcpy(&opened, response, sizeof(opened));
    object_service_reset();
    AmpObjectTransactionRequest commit = {opened.transaction_id};
    EXPECT_EQ(AMP_STATUS_INVALID_STATE,
              object_call(AMP_OBJECT_COMMIT, &commit, sizeof(commit)));
}

TEST(ObjectServiceV4, RequiresStrictlySequentialWriteOffsets)
{
    uint32_t revision;
    std::vector<uint8_t> document = read_profile(0, &revision);
    ASSERT_GE(document.size(), 3u);
    AmpObjectOpenWriteRequest open = {
        AMP_OBJECT_CONFIG_PROFILE,
        0,
        revision,
        static_cast<uint32_t>(document.size()),
        0,
    };
    uint8_t response[AMP_FRAME_PAYLOAD_SIZE] = {};
    uint16_t response_len = 0;
    ASSERT_EQ(AMP_STATUS_OK, object_call(AMP_OBJECT_OPEN_WRITE, &open, sizeof(open),
                                        response, &response_len));
    AmpObjectOpenWriteResponse opened;
    std::memcpy(&opened, response, sizeof(opened));

    uint8_t request[7] = {};
    std::memcpy(request, &opened.transaction_id, sizeof(opened.transaction_id));
    uint32_t offset = 1;
    std::memcpy(request + 2, &offset, sizeof(offset));
    request[6] = document[1];
    EXPECT_EQ(AMP_STATUS_INVALID_ARGUMENT,
              object_call(AMP_OBJECT_WRITE, request, sizeof(request)));

    offset = 0;
    std::memcpy(request + 2, &offset, sizeof(offset));
    request[6] = document[0];
    EXPECT_EQ(AMP_STATUS_OK, object_call(AMP_OBJECT_WRITE, request, sizeof(request)));
    EXPECT_EQ(AMP_STATUS_INVALID_ARGUMENT,
              object_call(AMP_OBJECT_WRITE, request, sizeof(request)));

    AmpObjectTransactionRequest abort = {opened.transaction_id};
    EXPECT_EQ(AMP_STATUS_OK,
              object_call(AMP_OBJECT_ABORT, &abort, sizeof(abort)));

#if AMP_OBJECT_TEMP_FILE_ENABLE
    EXPECT_EQ(document, read_profile(0));
#else
    EXPECT_NE(document, read_profile(0));
    (void)write_profile(0, revision, document);
#endif
}
