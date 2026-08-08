/*
 * Copyright (c) 2026 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "packet.h"

#include "layer.h"
#include "stddef.h"
#include "string.h"

static bool console_subscribed;
static bool debug_subscribed;
static uint8_t debug_count;
static uint16_t debug_indices[(AMP_FRAME_PAYLOAD_SIZE - 9) / sizeof(AmpDebugItem)];
static uint32_t debug_sequence;
static uint32_t console_sequence;
static uint32_t device_state_revision;

static bool pending_active_profile_event;
static AmpActiveProfileChangedEvent active_profile_event;
static bool pending_profile_event;
static AmpProfileChangedEvent profile_event;

static bool cached_response_valid;
static uint16_t cached_session_id;
static uint16_t cached_request_id;
static uint8_t cached_channel;
static uint16_t cached_opcode;
static uint8_t cached_response[AMP_FRAME_MESSAGE_SIZE];

static uint32_t capabilities(void)
{
    uint32_t result = 0;
#if defined(STORAGE_ENABLE) && defined(LFS_ENABLE)
    result |= AMP_CAP_CONFIG_OBJECT;
#if AMP_OBJECT_CRC32_ENABLE
    result |= AMP_CAP_OBJECT_CRC32;
#endif
#if AMP_OBJECT_TEMP_FILE_ENABLE
    result |= AMP_CAP_OBJECT_ATOMIC_COMMIT;
#endif
#if defined(SCRIPT_ENABLE) && SCRIPT_RUNTIME_STRATEGY == SCRIPT_JIT
    result |= AMP_CAP_SCRIPT_SOURCE;
#endif
#if defined(SCRIPT_ENABLE) && SCRIPT_RUNTIME_STRATEGY == SCRIPT_AOT
    result |= AMP_CAP_SCRIPT_BYTECODE;
#endif
#endif
    result |= AMP_CAP_DEBUG_STREAM;
#ifdef CONSOLE_ENABLE
    result |= AMP_CAP_CONSOLE_STREAM;
#endif
    return result;
}

void packet_reset_session(void)
{
    object_service_reset();
    console_subscribed = false;
    debug_subscribed = false;
    debug_count = 0;
    debug_sequence = 0;
    console_sequence = 0;
    pending_active_profile_event = false;
    pending_profile_event = false;
    cached_response_valid = false;
    g_keyboard_config.debug = false;
    g_keyboard_config.console = false;
}

bool packet_console_is_subscribed(void)
{
    return console_subscribed;
}

uint32_t packet_next_console_sequence(void)
{
    return ++console_sequence;
}

static AmpStatus process_hello(const AmpFrame *request, uint8_t *response,
                               uint16_t *response_len)
{
    if (request->header.session_id == 0 ||
        request->header.payload_len != sizeof(AmpHelloRequest))
    {
        return AMP_STATUS_INVALID_ARGUMENT;
    }
    AmpHelloRequest input;
    memcpy(&input, request->payload, sizeof(input));
    if (input.max_rx_payload == 0 || input.max_tx_payload == 0)
    {
        return AMP_STATUS_INVALID_ARGUMENT;
    }

    uint16_t transport_payload = amp_transport_max_payload();
    if (transport_payload > AMP_FRAME_PAYLOAD_SIZE)
    {
        transport_payload = AMP_FRAME_PAYLOAD_SIZE;
    }
    if (transport_payload < sizeof(AmpHelloResponse) ||
        input.max_rx_payload < sizeof(AmpHelloResponse))
    {
        return AMP_STATUS_NO_SPACE;
    }
    amp_session_begin(request->header.session_id, input.max_rx_payload,
                      input.max_tx_payload);
    AmpHelloResponse output = {
        .max_rx_payload = transport_payload,
        .max_tx_payload = transport_payload,
        .capabilities = capabilities(),
        .device_state_revision = device_state_revision,
        .active_profile = g_current_profile_index,
        .profile_count = STORAGE_PROFILE_FILE_NUM,
        .firmware_major = KEYBOARD_VERSION_MAJOR,
        .firmware_minor = KEYBOARD_VERSION_MINOR,
        .firmware_patch = KEYBOARD_VERSION_PATCH,
        .max_inflight_requests = 1,
        .advanced_key_count = ADVANCED_KEY_NUM,
        .total_key_count = TOTAL_KEY_NUM,
        .layer_count = LAYER_NUM,
#ifdef DYNAMICKEY_ENABLE
        .dynamic_key_count = DYNAMIC_KEY_NUM,
#else
        .dynamic_key_count = 0,
#endif
#ifdef MACRO_ENABLE
        .macro_count = MACRO_NUM,
        .macro_action_count = MACRO_MAX_ACTIONS,
#else
        .macro_count = 0,
        .macro_action_count = 0,
#endif
#ifdef RGB_ENABLE
        .rgb_count = RGB_NUM,
#else
        .rgb_count = 0,
#endif
    };
    size_t info_length = sizeof(KEYBOARD_VERSION_INFO);
    if (info_length > sizeof(output.firmware_info))
    {
        info_length = sizeof(output.firmware_info);
    }
    output.firmware_info_length = (uint8_t)info_length;
    memcpy(output.firmware_info, KEYBOARD_VERSION_INFO, info_length);
    memcpy(response, &output, sizeof(output));
    *response_len = sizeof(output);
    return AMP_STATUS_OK;
}

static AmpStatus process_key_event(const uint8_t *request, uint16_t request_len)
{
    if (request_len != sizeof(AmpKeyEvent))
    {
        return AMP_STATUS_INVALID_ARGUMENT;
    }
    AmpKeyEvent packet;
    memcpy(&packet, request, sizeof(packet));
    Key *key = packet.is_virtual ? NULL : keyboard_get_key(packet.key_id);
    uint8_t report_state = false;
    if (!packet.is_virtual && key == NULL)
    {
        return AMP_STATUS_INVALID_ARGUMENT;
    }
    if (key != NULL)
    {
        report_state = key->report_state;
#ifdef KEY_CALLBACK_ENABLE
        if (packet.event == KEYBOARD_EVENT_KEY_DOWN)
        {
            key_emit(key, KEY_EVENT_DOWN);
        }
        else if (packet.event == KEYBOARD_EVENT_KEY_UP)
        {
            key_emit(key, KEY_EVENT_UP);
        }
#endif
    }
    KeyboardEvent event = {
        .keycode = packet.use_keymap ? layer_cache_get_keycode(packet.key_id) :
                                      packet.keycode,
        .event = packet.event,
        .is_virtual = packet.is_virtual,
        .key = key,
    };
    keyboard_event_handler(event);
    if (key != NULL)
    {
        keyboard_key_set_report_state(key, report_state);
    }
    return AMP_STATUS_OK;
}

static AmpStatus process_control(const AmpFrame *request, uint8_t *response,
                                 uint16_t *response_len)
{
    switch (request->header.opcode)
    {
    case AMP_CONTROL_HELLO:
        return process_hello(request, response, response_len);
    case AMP_CONTROL_KEY_EVENT:
        return process_key_event(request->payload, request->header.payload_len);
    default:
        return AMP_STATUS_UNSUPPORTED;
    }
}

static void queue_active_profile_event(uint8_t reason)
{
    device_state_revision++;
    if (device_state_revision == 0)
    {
        device_state_revision = 1;
    }
    active_profile_event.profile_id = g_current_profile_index;
    active_profile_event.device_state_revision = device_state_revision;
    active_profile_event.reason = reason;
    pending_active_profile_event = true;
}

static AmpStatus process_config(const AmpFrame *request, uint8_t *response,
                                uint16_t *response_len)
{
    if (request->header.opcode != AMP_CONFIG_ACTIVATE_PROFILE ||
        request->header.payload_len != sizeof(AmpActivateProfileRequest))
    {
        return request->header.opcode == AMP_CONFIG_ACTIVATE_PROFILE ?
               AMP_STATUS_INVALID_ARGUMENT : AMP_STATUS_UNSUPPORTED;
    }
    AmpActivateProfileRequest input;
    memcpy(&input, request->payload, sizeof(input));
    if (input.profile_id >= STORAGE_PROFILE_FILE_NUM)
    {
        return AMP_STATUS_INVALID_ARGUMENT;
    }
    if (input.profile_id != g_current_profile_index)
    {
        keyboard_set_profile_index((uint8_t)input.profile_id);
        queue_active_profile_event(0);
    }
    AmpActivateProfileResponse output = {
        .active_profile = g_current_profile_index,
        .device_state_revision = device_state_revision,
    };
    memcpy(response, &output, sizeof(output));
    *response_len = sizeof(output);
    return AMP_STATUS_OK;
}

static AmpStatus fill_debug_data(const uint16_t *indices, uint8_t count,
                                 uint32_t sequence, uint8_t *response,
                                 uint16_t *response_len)
{
    uint16_t max_payload = amp_session_max_tx_payload();
    const uint8_t max_count = max_payload >= 9 ?
        (uint8_t)((max_payload - 9) / sizeof(AmpDebugItem)) : 0;
    if (count > max_count)
    {
        return AMP_STATUS_INVALID_ARGUMENT;
    }
    AmpDebugData data = {
        .sequence = sequence,
        .tick = g_keyboard_tick,
        .count = count,
    };
    for (uint8_t i = 0; i < count; i++)
    {
        if (indices[i] >= ADVANCED_KEY_NUM)
        {
            return AMP_STATUS_INVALID_ARGUMENT;
        }
        AdvancedKey *key = &g_keyboard_advanced_keys[indices[i]];
        data.items[i].index = indices[i];
        data.items[i].state = key->key.state;
        data.items[i].report_state = key->key.report_state;
        data.items[i].value = key->value;
        data.items[i].raw = key->raw;
        data.items[i].filtered_raw = key->filtered_raw;
    }
    *response_len = (uint16_t)(9 + count * sizeof(AmpDebugItem));
    memcpy(response, &data, *response_len);
    return AMP_STATUS_OK;
}

static AmpStatus parse_debug_request(const uint8_t *request,
                                     uint16_t request_len,
                                     uint16_t *indices, uint8_t *count)
{
    if (request_len < 1)
    {
        return AMP_STATUS_INVALID_ARGUMENT;
    }
    *count = request[0];
    uint16_t max_payload = amp_session_max_tx_payload();
    const uint8_t max_count = max_payload >= 9 ?
        (uint8_t)((max_payload - 9) / sizeof(AmpDebugItem)) : 0;
    if (*count > max_count || request_len != 1 + *count * sizeof(uint16_t))
    {
        return AMP_STATUS_INVALID_ARGUMENT;
    }
    memcpy(indices, request + 1, *count * sizeof(uint16_t));
    for (uint8_t i = 0; i < *count; i++)
    {
        if (indices[i] >= ADVANCED_KEY_NUM)
        {
            return AMP_STATUS_INVALID_ARGUMENT;
        }
    }
    return AMP_STATUS_OK;
}

static AmpStatus process_debug(const AmpFrame *request, uint8_t *response,
                               uint16_t *response_len)
{
    switch (request->header.opcode)
    {
    case AMP_DEBUG_SUBSCRIBE:
    {
        uint8_t count;
        AmpStatus status = parse_debug_request(request->payload,
                                               request->header.payload_len,
                                               debug_indices, &count);
        if (status == AMP_STATUS_OK)
        {
            debug_count = count;
            debug_subscribed = true;
            g_keyboard_config.debug = true;
        }
        return status;
    }
    case AMP_DEBUG_UNSUBSCRIBE:
        if (request->header.payload_len != 0)
        {
            return AMP_STATUS_INVALID_ARGUMENT;
        }
        debug_subscribed = false;
        debug_count = 0;
        g_keyboard_config.debug = false;
        return AMP_STATUS_OK;
    case AMP_DEBUG_SAMPLE:
    {
        uint16_t indices[(AMP_FRAME_PAYLOAD_SIZE - 9) / sizeof(AmpDebugItem)];
        uint8_t count;
        AmpStatus status = parse_debug_request(request->payload,
                                               request->header.payload_len,
                                               indices, &count);
        return status == AMP_STATUS_OK ?
            fill_debug_data(indices, count, 0, response, response_len) : status;
    }
    default:
        return AMP_STATUS_UNSUPPORTED;
    }
}

static AmpStatus process_console(const AmpFrame *request)
{
    if (request->header.payload_len != 0)
    {
        return AMP_STATUS_INVALID_ARGUMENT;
    }
    if (request->header.opcode == AMP_CONSOLE_SUBSCRIBE)
    {
        console_subscribed = true;
        g_keyboard_config.console = true;
        return AMP_STATUS_OK;
    }
    if (request->header.opcode == AMP_CONSOLE_UNSUBSCRIBE)
    {
        console_subscribed = false;
        g_keyboard_config.console = false;
        return AMP_STATUS_OK;
    }
    return AMP_STATUS_UNSUPPORTED;
}

static AmpStatus dispatch_request(const AmpFrame *request, uint8_t *response,
                                  uint16_t *response_len)
{
    *response_len = 0;
    switch (amp_frame_channel(&request->header))
    {
    case AMP_CHANNEL_CONTROL:
        return process_control(request, response, response_len);
    case AMP_CHANNEL_CONFIG:
        return process_config(request, response, response_len);
    case AMP_CHANNEL_OBJECT:
        return object_service_process(request->header.opcode, request->payload,
                                      request->header.payload_len, response,
                                      response_len);
    case AMP_CHANNEL_DEBUG:
        return process_debug(request, response, response_len);
    case AMP_CHANNEL_CONSOLE:
        return process_console(request);
    case AMP_CHANNEL_USER:
        return packet_process_user(request->header.opcode, request->payload,
                                   request->header.payload_len, response,
                                   response_len);
    default:
        return AMP_STATUS_UNSUPPORTED;
    }
}

void packet_process_frame(const AmpFrame *frame)
{
    if (frame == NULL || frame->header.request_id == 0 ||
        frame->header.status != AMP_STATUS_OK || amp_frame_flags(&frame->header) != 0)
    {
        return;
    }
    const uint8_t channel = amp_frame_channel(&frame->header);
    const bool hello = channel == AMP_CHANNEL_CONTROL &&
                       frame->header.opcode == AMP_CONTROL_HELLO;
    if (!hello && (!amp_session_is_active() ||
                   frame->header.session_id != amp_session_id()))
    {
        return;
    }
    if (!hello && cached_response_valid &&
        cached_session_id == frame->header.session_id &&
        cached_request_id == frame->header.request_id &&
        cached_channel == channel && cached_opcode == frame->header.opcode)
    {
        (void)amp_send_encoded_message(cached_response, AMP_QUEUE_RESPONSE);
        return;
    }

    uint8_t response[AMP_FRAME_PAYLOAD_SIZE] = {0};
    uint16_t response_len = 0;
    AmpStatus status = dispatch_request(frame, response, &response_len);
    if (response_len > amp_session_max_tx_payload())
    {
        status = AMP_STATUS_NO_SPACE;
        response_len = 0;
    }
    uint16_t session_id = hello ? amp_session_id() : frame->header.session_id;
    if (amp_frame_encode(cached_response, sizeof(cached_response), channel,
                         AMP_FRAME_FLAG_RESPONSE, session_id,
                         frame->header.request_id, frame->header.opcode, status,
                         response, response_len) < 0)
    {
        return;
    }
    cached_response_valid = true;
    cached_session_id = session_id;
    cached_request_id = frame->header.request_id;
    cached_channel = channel;
    cached_opcode = frame->header.opcode;
    (void)amp_send_encoded_message(cached_response, AMP_QUEUE_RESPONSE);
}

void packet_notify_profile_changed(uint16_t profile_id, uint32_t revision)
{
    profile_event.profile_id = profile_id;
    profile_event.profile_revision = revision;
    pending_profile_event = true;
}

void packet_send_version_packet(void)
{
    queue_active_profile_event(1);
}

void packet_process_version_notifications(void)
{
    if (!amp_session_is_active())
    {
        return;
    }
    if (pending_active_profile_event && amp_transport_control_event_can_enqueue())
    {
        if (amp_send_frame(AMP_CHANNEL_CONFIG, AMP_FRAME_FLAG_EVENT,
                           amp_session_id(), 0,
                           AMP_CONFIG_ACTIVE_PROFILE_CHANGED, AMP_STATUS_OK,
                           &active_profile_event, sizeof(active_profile_event),
                           AMP_QUEUE_CONTROL) == 0)
        {
            pending_active_profile_event = false;
        }
    }
    if (pending_profile_event && amp_transport_control_event_can_enqueue())
    {
        if (amp_send_frame(AMP_CHANNEL_CONFIG, AMP_FRAME_FLAG_EVENT,
                           amp_session_id(), 0, AMP_CONFIG_PROFILE_CHANGED,
                           AMP_STATUS_OK, &profile_event, sizeof(profile_event),
                           AMP_QUEUE_CONTROL) == 0)
        {
            pending_profile_event = false;
        }
    }
}

void packet_send_debug_packet(void)
{
    if (!debug_subscribed || !amp_session_is_active() || debug_count == 0 ||
        !amp_transport_stream_event_can_enqueue())
    {
        return;
    }
    uint8_t payload[AMP_FRAME_PAYLOAD_SIZE];
    uint16_t payload_len;
    if (fill_debug_data(debug_indices, debug_count, ++debug_sequence,
                        payload, &payload_len) == AMP_STATUS_OK)
    {
        (void)amp_send_frame(AMP_CHANNEL_DEBUG, AMP_FRAME_FLAG_EVENT,
                             amp_session_id(), 0, AMP_DEBUG_DATA, AMP_STATUS_OK,
                             payload, payload_len, AMP_QUEUE_STREAM);
    }
}

__WEAK AmpStatus packet_process_user(uint16_t opcode, const uint8_t *request,
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
