/*
 * Copyright (c) 2025 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "packet.h"

#include "amp_protocol.h"
#include "layer.h"
#include "rgb.h"
#include "stddef.h"
#include "string.h"
#ifdef MACRO_ENABLE
#include "macro.h"
#endif
#ifdef NEXUS_ENABLE
#include "nexus.h"
#endif

static uint8_t debug_count;
static uint16_t debug_indices[PACKET_DEBUG_ITEMS];
static volatile bool pending_version_notification;

static AmpStatus process_advanced_keys(uint8_t code, const PacketAdvancedKeys *request,
                                       PacketAdvancedKeys *response)
{
    if (request->count > PACKET_ADVANCED_KEY_ITEMS)
    {
        return AMP_STATUS_INVALID_ARGUMENT;
    }
    for (uint8_t i = 0; i < request->count; i++)
    {
        if (request->items[i].index >= ADVANCED_KEY_NUM)
        {
            return AMP_STATUS_INVALID_ARGUMENT;
        }
    }

    if (code == PACKET_CODE_GET)
    {
        response->count = request->count;
        for (uint8_t i = 0; i < request->count; i++)
        {
            uint16_t index = request->items[i].index;
            response->items[i].index = index;
            memcpy(&response->items[i].config,
                   &g_keyboard_advanced_keys[index].config,
                   sizeof(AdvancedKeyConfiguration));
        }
        return AMP_STATUS_OK;
    }
    if (code == PACKET_CODE_SET)
    {
        for (uint8_t i = 0; i < request->count; i++)
        {
            uint16_t index = request->items[i].index;
            AdvancedKeyConfiguration config_buffer;
            memcpy(&config_buffer, &request->items[i].config, sizeof(AdvancedKeyConfiguration));
            AdvancedKeyConfiguration* config = &g_keyboard_advanced_keys[index].config;
            config->mode = config_buffer.mode;
#if  !(defined(NEXUS_ENABLE) && NEXUS_IS_SLAVE)
            //config->calibration_mode = config_buffer.calibration_mode;
#endif
            config->activation_value = config_buffer.activation_value;
            config->deactivation_value = config_buffer.deactivation_value;
            config->trigger_distance = config_buffer.trigger_distance;
            config->release_distance = config_buffer.release_distance;
            config->trigger_speed = config_buffer.trigger_speed;
            config->release_speed = config_buffer.release_speed;
            config->upper_deadzone = config_buffer.upper_deadzone;
            config->lower_deadzone = config_buffer.lower_deadzone;
#if  !(defined(NEXUS_ENABLE) && NEXUS_IS_SLAVE)
            //config->upper_bound = config_buffer.upper_bound;
            //config->lower_bound = config_buffer.lower_bound;
#endif
#if defined(NEXUS_ENABLE) && !NEXUS_IS_SLAVE
            (void)nexus_sync_advanced_key_config(index);
#endif
        }
        return AMP_STATUS_OK;
    }
    return AMP_STATUS_UNSUPPORTED;
}

static AmpStatus process_keymap(uint8_t code, const PacketKeymap *request,
                                PacketKeymap *response)
{
    if (request->count > PACKET_KEYMAP_ITEMS || request->layer >= LAYER_NUM ||
        request->start >= TOTAL_KEY_NUM ||
        (uint32_t)request->start + request->count > TOTAL_KEY_NUM)
    {
        return AMP_STATUS_INVALID_ARGUMENT;
    }

    if (code == PACKET_CODE_GET)
    {
        response->layer = request->layer;
        response->start = request->start;
        response->count = request->count;
        for (uint8_t i = 0; i < request->count; i++)
        {
            response->keycodes[i] = g_keymap[request->layer][request->start + i];
        }
        return AMP_STATUS_OK;
    }
    if (code == PACKET_CODE_SET)
    {
        for (uint8_t i = 0; i < request->count; i++)
        {
            uint16_t index = (uint16_t)(request->start + i);
            g_keymap[request->layer][index] = request->keycodes[i];
            if (!g_keymap_lock[index])
            {
                g_keymap_cache[index] = layer_get_keycode(index, g_current_layer);
            }
        }
        return AMP_STATUS_OK;
    }
    return AMP_STATUS_UNSUPPORTED;
}

static AmpStatus process_rgb_base(uint8_t code, const PacketRgbBaseConfig *request,
                                  PacketRgbBaseConfig *response)
{
#ifdef RGB_ENABLE
    if (code == PACKET_CODE_GET)
    {
        response->mode = g_rgb_base_config.mode;
        response->r = g_rgb_base_config.rgb.r;
        response->g = g_rgb_base_config.rgb.g;
        response->b = g_rgb_base_config.rgb.b;
        response->secondary_r = g_rgb_base_config.secondary_rgb.r;
        response->secondary_g = g_rgb_base_config.secondary_rgb.g;
        response->secondary_b = g_rgb_base_config.secondary_rgb.b;
        response->speed = g_rgb_base_config.speed;
        response->direction = g_rgb_base_config.direction;
        response->density = g_rgb_base_config.density;
        response->brightness = g_rgb_base_config.brightness;
        return AMP_STATUS_OK;
    }
    if (code == PACKET_CODE_SET)
    {
        g_rgb_base_config.mode = request->mode;
        g_rgb_base_config.rgb.r = request->r;
        g_rgb_base_config.rgb.g = request->g;
        g_rgb_base_config.rgb.b = request->b;
        rgb_to_hsv(&g_rgb_base_config.hsv, &g_rgb_base_config.rgb);
        g_rgb_base_config.secondary_rgb.r = request->secondary_r;
        g_rgb_base_config.secondary_rgb.g = request->secondary_g;
        g_rgb_base_config.secondary_rgb.b = request->secondary_b;
        rgb_to_hsv(&g_rgb_base_config.secondary_hsv, &g_rgb_base_config.secondary_rgb);
        g_rgb_base_config.speed = request->speed;
        g_rgb_base_config.direction = request->direction;
        g_rgb_base_config.density = request->density;
        g_rgb_base_config.brightness = request->brightness;
        return AMP_STATUS_OK;
    }
#else
    UNUSED(code);
    UNUSED(request);
    UNUSED(response);
#endif
    return AMP_STATUS_UNSUPPORTED;
}

static AmpStatus process_rgb_items(uint8_t code, const PacketRgbItems *request,
                                   PacketRgbItems *response)
{
#ifdef RGB_ENABLE
    if (request->count > PACKET_RGB_ITEMS)
    {
        return AMP_STATUS_INVALID_ARGUMENT;
    }
    for (uint8_t i = 0; i < request->count; i++)
    {
        if (request->items[i].index >= TOTAL_KEY_NUM)
        {
            return AMP_STATUS_INVALID_ARGUMENT;
        }
    }

    if (code == PACKET_CODE_GET)
    {
        response->count = request->count;
        for (uint8_t i = 0; i < request->count; i++)
        {
            uint16_t key_index = request->items[i].index;
            uint16_t rgb_index = g_rgb_inverse_mapping[key_index];
            response->items[i].index = key_index;
            if (rgb_index < RGB_NUM)
            {
                response->items[i].mode = g_rgb_configs[rgb_index].mode;
                response->items[i].r = g_rgb_configs[rgb_index].rgb.r;
                response->items[i].g = g_rgb_configs[rgb_index].rgb.g;
                response->items[i].b = g_rgb_configs[rgb_index].rgb.b;
                response->items[i].speed = g_rgb_configs[rgb_index].speed;
            }
        }
        return AMP_STATUS_OK;
    }
    if (code == PACKET_CODE_SET)
    {
        for (uint8_t i = 0; i < request->count; i++)
        {
            uint16_t rgb_index = g_rgb_inverse_mapping[request->items[i].index];
            if (rgb_index < RGB_NUM)
            {
                g_rgb_configs[rgb_index].mode = request->items[i].mode;
                g_rgb_configs[rgb_index].rgb.r = request->items[i].r;
                g_rgb_configs[rgb_index].rgb.g = request->items[i].g;
                g_rgb_configs[rgb_index].rgb.b = request->items[i].b;
                rgb_to_hsv(&g_rgb_configs[rgb_index].hsv, &g_rgb_configs[rgb_index].rgb);
                g_rgb_configs[rgb_index].speed = request->items[i].speed;
            }
        }
        return AMP_STATUS_OK;
    }
#else
    UNUSED(code);
    UNUSED(request);
    UNUSED(response);
#endif
    return AMP_STATUS_UNSUPPORTED;
}

static AmpStatus process_dynamic_key(uint8_t code,
                                     const uint8_t request[AMP_FRAME_BODY_SIZE],
                                     uint8_t response[AMP_FRAME_BODY_SIZE])
{
#ifdef DYNAMICKEY_ENABLE
    uint16_t index;
    memcpy(&index, request, sizeof(index));
    if (index >= DYNAMIC_KEY_NUM)
    {
        return AMP_STATUS_INVALID_ARGUMENT;
    }
    if (code == PACKET_CODE_GET)
    {
        memcpy(response, &index, sizeof(index));
        memcpy(response + PACKET_DYNAMIC_KEY_DATA_OFFSET,
               &g_dynamic_keys[index], sizeof(DynamicKey));
        return AMP_STATUS_OK;
    }
    if (code == PACKET_CODE_SET)
    {
        uint32_t type;
        memcpy(&type, request + PACKET_DYNAMIC_KEY_DATA_OFFSET, sizeof(type));
        if (type >= DYNAMIC_KEY_TYPE_NUM)
        {
            return AMP_STATUS_INVALID_ARGUMENT;
        }
        memcpy(&g_dynamic_keys[index],
               request + PACKET_DYNAMIC_KEY_DATA_OFFSET, sizeof(DynamicKey));
        return AMP_STATUS_OK;
    }
#else
    UNUSED(code);
    UNUSED(request);
    UNUSED(response);
#endif
    return AMP_STATUS_UNSUPPORTED;
}

static AmpStatus process_profile(uint8_t code, const PacketProfileIndex *request,
                                 PacketProfileIndex *response)
{
    if (code == PACKET_CODE_GET)
    {
        response->index = g_current_profile_index;
        return AMP_STATUS_OK;
    }
    if (code == PACKET_CODE_SET)
    {
        if (request->index >= STORAGE_PROFILE_FILE_NUM)
        {
            return AMP_STATUS_INVALID_ARGUMENT;
        }
        keyboard_set_profile_index(request->index);
        g_keyboard_config.console = false;
        g_keyboard_config.debug = false;
        return AMP_STATUS_OK;
    }
    return AMP_STATUS_UNSUPPORTED;
}

static AmpStatus process_config(uint8_t code, const PacketConfig *request,
                                PacketConfig *response)
{
    if (request->count > PACKET_CONFIG_ITEMS)
    {
        return AMP_STATUS_INVALID_ARGUMENT;
    }
    for (uint8_t i = 0; i < request->count; i++)
    {
        if (request->items[i].index >= KEYBOARD_CONFIG_NUM)
        {
            return AMP_STATUS_INVALID_ARGUMENT;
        }
    }

    if (code == PACKET_CODE_GET)
    {
        response->count = request->count;
        for (uint8_t i = 0; i < request->count; i++)
        {
            uint8_t index = request->items[i].index;
            response->items[i].index = index;
            response->items[i].value = (uint8_t)BIT_GET(g_keyboard_config.raw, index);
        }
        return AMP_STATUS_OK;
    }
    if (code == PACKET_CODE_SET)
    {
        for (uint8_t i = 0; i < request->count; i++)
        {
            uint8_t index = request->items[i].index;
            if (request->items[i].value)
            {
                BIT_SET(g_keyboard_config.raw, index);
            }
            else
            {
                BIT_RESET(g_keyboard_config.raw, index);
            }
        }
        return AMP_STATUS_OK;
    }
    return AMP_STATUS_UNSUPPORTED;
}

static AmpStatus process_debug(uint8_t code, const PacketDebug *request,
                               PacketDebug *response)
{
    if (code != PACKET_CODE_GET)
    {
        return AMP_STATUS_UNSUPPORTED;
    }
    if (request->count > PACKET_DEBUG_ITEMS)
    {
        return AMP_STATUS_INVALID_ARGUMENT;
    }
    for (uint8_t i = 0; i < request->count; i++)
    {
        if (request->items[i].index >= ADVANCED_KEY_NUM)
        {
            return AMP_STATUS_INVALID_ARGUMENT;
        }
    }

    debug_count = request->count;
    response->count = request->count;
    response->tick = g_keyboard_tick;
    for (uint8_t i = 0; i < request->count; i++)
    {
        uint16_t index = request->items[i].index;
        AdvancedKey *key = &g_keyboard_advanced_keys[index];
        debug_indices[i] = index;
        response->items[i].index = index;
        response->items[i].state = key->key.state;
        response->items[i].report_state = key->key.report_state;
        response->items[i].value = key->value;
        response->items[i].raw = key->raw;
        response->items[i].filtered_raw = key->filtered_raw;
    }
    return AMP_STATUS_OK;
}

static AmpStatus process_macro(uint8_t code, const PacketMacro *request,
                               PacketMacro *response)
{
#ifdef MACRO_ENABLE
    if (request->macro_index >= MACRO_NUM || request->count > PACKET_MACRO_ITEMS)
    {
        return AMP_STATUS_INVALID_ARGUMENT;
    }
    for (uint8_t i = 0; i < request->count; i++)
    {
        if (request->items[i].index >= MACRO_MAX_ACTIONS)
        {
            return AMP_STATUS_INVALID_ARGUMENT;
        }
    }

    if (code == PACKET_CODE_GET)
    {
        response->macro_index = request->macro_index;
        response->count = request->count;
        for (uint8_t i = 0; i < request->count; i++)
        {
            uint16_t index = request->items[i].index;
            MacroAction *action = &g_macros[request->macro_index].actions[index];
            response->items[i].index = index;
            response->items[i].delay = action->delay;
            response->items[i].event = action->event.event;
            response->items[i].is_virtual = action->event.is_virtual;
            response->items[i].keycode = action->event.keycode;
            if (action->event.key != NULL)
            {
                response->items[i].key_id = ((Key *)action->event.key)->id;
            }
        }
        return AMP_STATUS_OK;
    }
    if (code == PACKET_CODE_SET)
    {
        for (uint8_t i = 0; i < request->count; i++)
        {
            MacroAction *action = &g_macros[request->macro_index].actions[request->items[i].index];
            action->delay = request->items[i].delay;
            action->event.event = request->items[i].event;
            action->event.is_virtual = request->items[i].is_virtual;
            action->event.keycode = request->items[i].keycode;
            action->event.key = keyboard_get_key(request->items[i].key_id);
        }
        return AMP_STATUS_OK;
    }
#else
    UNUSED(code);
    UNUSED(request);
    UNUSED(response);
#endif
    return AMP_STATUS_UNSUPPORTED;
}

static void fill_version(PacketVersion *version)
{
    STATIC_ASSERT(sizeof(KEYBOARD_VERSION_INFO) <= sizeof(version->info),
                   "KEYBOARD_VERSION_INFO is too long for Amp v3");
    version->major = KEYBOARD_VERSION_MAJOR;
    version->minor = KEYBOARD_VERSION_MINOR;
    version->patch = KEYBOARD_VERSION_PATCH;
    version->info_length = sizeof(KEYBOARD_VERSION_INFO);
    memcpy(version->info, KEYBOARD_VERSION_INFO, sizeof(KEYBOARD_VERSION_INFO));
}

static AmpStatus process_data(const AmpFrame *request, AmpFrame *response)
{
    switch (request->header.type)
    {
    case PACKET_DATA_VERSION:
        if (request->header.code != PACKET_CODE_GET)
        {
            return AMP_STATUS_UNSUPPORTED;
        }
        g_keyboard_config.console = false;
        g_keyboard_config.debug = false;
        fill_version((PacketVersion *)response->body);
        return AMP_STATUS_OK;
    case PACKET_DATA_ADVANCED_KEY:
        return process_advanced_keys(request->header.code,
                                     (const PacketAdvancedKeys *)request->body,
                                     (PacketAdvancedKeys *)response->body);
    case PACKET_DATA_KEYMAP:
        return process_keymap(request->header.code,
                              (const PacketKeymap *)request->body,
                              (PacketKeymap *)response->body);
    case PACKET_DATA_RGB_BASE_CONFIG:
        return process_rgb_base(request->header.code,
                                (const PacketRgbBaseConfig *)request->body,
                                (PacketRgbBaseConfig *)response->body);
    case PACKET_DATA_RGB_CONFIG:
        return process_rgb_items(request->header.code,
                                 (const PacketRgbItems *)request->body,
                                 (PacketRgbItems *)response->body);
    case PACKET_DATA_DYNAMIC_KEY:
        return process_dynamic_key(request->header.code,
                                   request->body, response->body);
    case PACKET_DATA_PROFILE_INDEX:
        return process_profile(request->header.code,
                               (const PacketProfileIndex *)request->body,
                               (PacketProfileIndex *)response->body);
    case PACKET_DATA_CONFIG:
        return process_config(request->header.code,
                              (const PacketConfig *)request->body,
                              (PacketConfig *)response->body);
    case PACKET_DATA_DEBUG:
        return process_debug(request->header.code,
                             (const PacketDebug *)request->body,
                             (PacketDebug *)response->body);
    case PACKET_DATA_MACRO:
        return process_macro(request->header.code,
                             (const PacketMacro *)request->body,
                             (PacketMacro *)response->body);
    case PACKET_DATA_FEATURE:
        return AMP_STATUS_UNSUPPORTED;
    default:
        return packet_process_user(request->header.code, request->header.type,
                                   request->body, response->body);
    }
}

static AmpStatus process_event(const PacketEvent *packet)
{
    Key *key = packet->is_virtual ? NULL : keyboard_get_key(packet->id);
    uint8_t report_state = false;
    if (!packet->is_virtual && key == NULL)
    {
        return AMP_STATUS_INVALID_ARGUMENT;
    }
    if (key != NULL)
    {
        report_state = key->report_state;
#ifdef KEY_CALLBACK_ENABLE
        if (packet->event == KEYBOARD_EVENT_KEY_DOWN)
        {
            key_emit(key, KEY_EVENT_DOWN);
        }
        else if (packet->event == KEYBOARD_EVENT_KEY_UP)
        {
            key_emit(key, KEY_EVENT_UP);
        }
#endif
    }

    KeyboardEvent event = {
        .keycode = packet->use_keymap ? layer_cache_get_keycode(packet->id) : packet->keycode,
        .event = packet->event,
        .is_virtual = packet->is_virtual,
        .key = key,
    };
    keyboard_event_handler(event);
    if (key != NULL)
    {
        keyboard_key_set_report_state(key, report_state);
    }
    return AMP_STATUS_OK;
}

static AmpStatus packet_dispatch(const AmpFrame *request, AmpFrame *response)
{
    switch (request->header.code)
    {
    case PACKET_CODE_GET:
    case PACKET_CODE_SET:
        return process_data(request, response);
    case PACKET_CODE_EVENT:
        return process_event((const PacketEvent *)request->body);
    case PACKET_CODE_LARGE_SET:
    case PACKET_CODE_LARGE_GET:
#if LARGE_PACKET_ENABLE
        return large_packet_process(request, response);
#else
        return AMP_STATUS_UNSUPPORTED;
#endif
    case PACKET_CODE_USER:
        return packet_process_user(request->header.code, request->header.type,
                                   request->body, response->body);
    default:
        return AMP_STATUS_UNSUPPORTED;
    }
}

static void prepare_response(const AmpFrame *request, AmpFrame *response,
                             uint8_t channel, uint8_t flags)
{
    memset(response, 0, sizeof(*response));
    response->header.proto = AMP_FRAME_PROTO;
    response->header.channel_flags = (uint8_t)(((channel & 0x0F) << 4) | (flags & 0x0F));
    response->header.seq = request->header.seq;
    response->header.code = request->header.code;
    response->header.type = request->header.type;
}

bool packet_process_frame_to_report(const AmpFrame *frame, uint8_t channel,
                                    uint8_t flags, uint8_t report[AMP_FRAME_REPORT_SIZE])
{
    if (frame == NULL || report == NULL)
    {
        return false;
    }
    AmpFrame response;
    prepare_response(frame, &response, channel, flags);
    response.header.status = (uint8_t)packet_dispatch(frame, &response);
    memcpy(report, &response, sizeof(response));
    return true;
}

void packet_process_frame(const AmpFrame *frame)
{
    if (frame == NULL)
    {
        return;
    }

    AmpFrame response;
    uint8_t channel = amp_frame_channel(&frame->header);
    prepare_response(frame, &response, channel, AMP_FRAME_FLAG_RESP);
    response.header.status = (uint8_t)packet_dispatch(frame, &response);

    if (frame->header.seq != 0 ||
        (amp_frame_flags(&frame->header) & AMP_FRAME_FLAG_REQ_ACK))
    {
        (void)amp_send_encoded_report((const uint8_t *)&response, false);
    }
}

static int packet_send_version_packet_now(void)
{
    PacketVersion body = {0};
    fill_version(&body);
    return amp_send_frame(AMP_CHANNEL_CONTROL, 0, 0, PACKET_CODE_GET,
                          PACKET_DATA_VERSION, AMP_STATUS_OK,
                          (const uint8_t *)&body, false);
}

void packet_send_version_packet(void)
{
    pending_version_notification = true;
}

void packet_process_version_notifications(void)
{
    if (!pending_version_notification || !amp_transport_control_event_can_enqueue())
    {
        return;
    }
    if (packet_send_version_packet_now() == 0)
    {
        pending_version_notification = false;
    }
}

void packet_send_debug_packet(void)
{
#if DEBUG_INTERVAL > 0
    static uint16_t timer;
    timer++;
    if (timer < DEBUG_INTERVAL)
    {
        return;
    }
    timer = 0;
#endif
    PacketDebug request_body = {0};
    PacketDebug response_body = {0};
    request_body.count = debug_count;
    for (uint8_t i = 0; i < debug_count; i++)
    {
        request_body.items[i].index = debug_indices[i];
    }
    if (process_debug(PACKET_CODE_GET, &request_body, &response_body) == AMP_STATUS_OK)
    {
        (void)amp_send_frame(AMP_CHANNEL_DEBUG, 0, 0, PACKET_CODE_GET,
                             PACKET_DATA_DEBUG, AMP_STATUS_OK,
                             (const uint8_t *)&response_body, true);
    }
}

__WEAK AmpStatus packet_process_user(uint8_t code, uint8_t type,
                                     const uint8_t request[AMP_FRAME_BODY_SIZE],
                                     uint8_t response[AMP_FRAME_BODY_SIZE])
{
    UNUSED(code);
    UNUSED(type);
    UNUSED(request);
    UNUSED(response);
    return AMP_STATUS_UNSUPPORTED;
}
