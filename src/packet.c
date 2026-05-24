/*
 * Copyright (c) 2025 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "packet.h"
#include "rgb.h"
#include "layer.h"
#include "amp_protocol.h"

#include "stddef.h"
#include "string.h"
#ifdef MACRO_ENABLE
#include "macro.h"
#endif
static uint8_t debug_length;
static uint16_t debug_buffer[7];

enum
{
    PACKET_ERROR_BAD_FRAME = 1,
    PACKET_ERROR_TOO_LONG = 2,
};

static uint8_t packet_clamp_payload_len(uint16_t len)
{
    return len > AMP_FRAME_MAX_PAYLOAD ? AMP_FRAME_MAX_PAYLOAD : (uint8_t)len;
}

static bool packet_from_frame(const AmpFrame *frame, uint8_t *packet, uint16_t *packet_len)
{
    if (frame == NULL || packet == NULL || packet_len == NULL)
    {
        return false;
    }

    memset(packet, 0, AMP_FRAME_REPORT_SIZE);
    if (frame->header.code == PACKET_CODE_EVENT)
    {
        if (frame->header.len > AMP_FRAME_REPORT_SIZE - 1)
        {
            return false;
        }
        packet[0] = frame->header.code;
        memcpy(packet + 1, frame->payload, frame->header.len);
        *packet_len = (uint16_t)(frame->header.len + 1);
        return true;
    }
    if (frame->header.code == PACKET_CODE_LOG)
    {
        if (frame->header.len > AMP_FRAME_REPORT_SIZE - offsetof(PacketLog, data))
        {
            return false;
        }
        PacketLog *log = (PacketLog *)packet;
        log->code = PACKET_CODE_LOG;
        log->length = frame->header.len;
        memcpy(log->data, frame->payload, frame->header.len);
        *packet_len = (uint16_t)(offsetof(PacketLog, data) + frame->header.len);
        return true;
    }

    if (frame->header.len > AMP_FRAME_REPORT_SIZE - 2)
    {
        return false;
    }
    packet[0] = frame->header.code;
    packet[1] = frame->header.type;
    memcpy(packet + 2, frame->payload, frame->header.len);
    *packet_len = (uint16_t)(frame->header.len + 2);
    return true;
}

static const uint8_t *packet_payload_ptr(const uint8_t *packet)
{
    switch (packet[0])
    {
    case PACKET_CODE_EVENT:
        return packet + 1;
    case PACKET_CODE_LOG:
        return packet + offsetof(PacketLog, data);
    default:
        return packet + 2;
    }
}

static uint8_t packet_payload_type(const uint8_t *packet)
{
    switch (packet[0])
    {
    case PACKET_CODE_EVENT:
    case PACKET_CODE_LOG:
        return 0;
    default:
        return packet[1];
    }
}

static uint8_t packet_response_payload_len(const uint8_t *packet, uint16_t packet_len)
{
    uint16_t fallback = 0;
    switch (packet[0])
    {
    case PACKET_CODE_EVENT:
        fallback = packet_len > 1 ? (uint16_t)(packet_len - 1) : 0;
        break;
    case PACKET_CODE_LOG:
        fallback = ((const PacketLog *)packet)->length;
        break;
    default:
        fallback = packet_len > 2 ? (uint16_t)(packet_len - 2) : 0;
        break;
    }

    switch (packet[0])
    {
    case PACKET_CODE_LARGE_SET:
    case PACKET_CODE_LARGE_GET:
    {
        const PacketLargeData *large = (const PacketLargeData *)packet;
        if (large->sub_cmd == 0)
        {
            return packet_clamp_payload_len(1 + sizeof(large->header));
        }
        if (large->sub_cmd == 1)
        {
            return packet_clamp_payload_len(1 + sizeof(large->payload.offset) + sizeof(large->payload.length) + large->payload.length);
        }
        return 1;
    }
    case PACKET_CODE_GET:
    case PACKET_CODE_SET:
        switch (((const PacketData *)packet)->type)
        {
        case PACKET_DATA_VERSION:
        {
            const PacketVersion *version = (const PacketVersion *)packet;
            return packet_clamp_payload_len((uint16_t)(offsetof(PacketVersion, info) - 2 + version->info_length));
        }
        case PACKET_DATA_DEBUG:
        {
            const PacketDebug *debug = (const PacketDebug *)packet;
            return packet_clamp_payload_len((uint16_t)(offsetof(PacketDebug, data) - 2 + debug->length * sizeof(debug->data[0])));
        }
        default:
            break;
        }
        break;
    default:
        break;
    }
    return packet_clamp_payload_len(fallback);
}

static int packet_send_response(uint8_t *packet, uint16_t packet_len, uint8_t channel, uint8_t flags, uint8_t seq, bool stream)
{
    uint8_t payload_len = packet_response_payload_len(packet, packet_len);
    return amp_send_frame(channel, flags, seq, packet[0], packet_payload_type(packet), packet_payload_ptr(packet), payload_len, stream);
}

bool packet_process_frame_to_report(const AmpFrame *frame, uint8_t channel, uint8_t flags, uint8_t *report)
{
    uint8_t packet[AMP_FRAME_REPORT_SIZE];
    uint16_t packet_len = 0;
    if (report == NULL || !packet_from_frame(frame, packet, &packet_len))
    {
        return false;
    }

    packet_process_buffer(packet, packet_len);
    uint8_t payload_len = packet_response_payload_len(packet, packet_len);
    return amp_frame_encode(report, channel, flags, frame->header.seq, packet[0], packet_payload_type(packet), packet_payload_ptr(packet), payload_len) == 0;
}

void packet_process_frame(const AmpFrame *frame)
{
    if (frame == NULL)
    {
        return;
    }

    const uint8_t channel = amp_frame_channel(&frame->header);
    const uint8_t flags = amp_frame_flags(&frame->header);
    if (frame->header.seq != 0 || (flags & AMP_FRAME_FLAG_REQ_ACK))
    {
        uint8_t response[AMP_FRAME_REPORT_SIZE];
        if (packet_process_frame_to_report(frame, channel, AMP_FRAME_FLAG_RESP, response))
        {
            amp_send_encoded_report(response, false);
        }
        else
        {
            amp_send_error(channel, frame->header.seq, frame->header.code, frame->header.type, PACKET_ERROR_TOO_LONG);
        }
        return;
    }

    uint8_t packet[AMP_FRAME_REPORT_SIZE];
    uint16_t packet_len = 0;
    if (packet_from_frame(frame, packet, &packet_len))
    {
        packet_process_buffer(packet, packet_len);
    }
    else
    {
        amp_send_error(channel, frame->header.seq, frame->header.code, frame->header.type, PACKET_ERROR_BAD_FRAME);
    }
}

void packet_process_buffer(uint8_t *buf, uint16_t len)
{
    UNUSED(len);
    PacketData *packet = (PacketData *)buf;
    switch (packet->code)
    {
    case PACKET_CODE_SET:
    case PACKET_CODE_GET:
        switch (((PacketData*)packet)->type)
        {
        case PACKET_DATA_ADVANCED_KEY:
            packet_process_advanced_key(packet);
            break;
        case PACKET_DATA_KEYMAP:
            packet_process_keymap(packet);
            break;
#ifdef RGB_ENABLE
        case PACKET_DATA_RGB_BASE_CONFIG:
            packet_process_rgb_base_config(packet);
            break;
        case PACKET_DATA_RGB_CONFIG:
            packet_process_rgb_config(packet);
            break;
#endif
#ifdef DYNAMICKEY_ENABLE
        case PACKET_DATA_DYNAMIC_KEY:
            packet_process_dynamic_key(packet);
            break;
#endif
        case PACKET_DATA_PROFILE_INDEX:
            packet_process_profile_index(packet);
            break;
        case PACKET_DATA_CONFIG:
            packet_process_config(packet);
            break;
        case PACKET_DATA_DEBUG:
            packet_process_debug(packet);
            break;
#ifdef MACRO_ENABLE
        case PACKET_DATA_MACRO:
            packet_process_macro(packet);
            break;
#endif
        case PACKET_DATA_FEATURE:
            packet_process_feature(packet);
            break;
        case PACKET_DATA_VERSION:
            if (packet->code == PACKET_CODE_GET)
            {
                g_keyboard_config.console = false;
                g_keyboard_config.debug = false;
                PacketVersion* packet_version = (PacketVersion*)packet;
                packet_version->info_length = sizeof(KEYBOARD_VERSION_INFO);
                packet_version->major = KEYBOARD_VERSION_MAJOR;
                packet_version->minor = KEYBOARD_VERSION_MINOR;
                packet_version->patch = KEYBOARD_VERSION_PATCH;
                memcpy(packet_version->info, KEYBOARD_VERSION_INFO, sizeof(KEYBOARD_VERSION_INFO));
            }
            break;
        default:
            packet_process_user(buf, len);
            break;
        }
        break;
    case PACKET_CODE_EVENT:
        {
            PacketEvent* packet_event = (PacketEvent*)packet;
            Key *key = packet_event->is_virtual ? NULL : keyboard_get_key(packet_event->id);
            uint8_t report_state = false;
            if (key != NULL)
            {
                report_state = ((Key*)key)->report_state;
#ifdef KEY_CALLBACK_ENABLE
                switch (packet_event->event)
                {
                case KEYBOARD_EVENT_KEY_DOWN:
                    key_emit(key, KEY_EVENT_DOWN);
                    break;
                case KEYBOARD_EVENT_KEY_UP:
                    key_emit(key, KEY_EVENT_UP);
                    break;
                default:
                    break;
                }
#endif
            }
            KeyboardEvent event = {
                .keycode = packet_event->use_keymap ? layer_cache_get_keycode(packet_event->id) : packet_event->keycode,
                .event = packet_event->event,
                .is_virtual = packet_event->is_virtual,
                .key = key,
            };      
            keyboard_event_handler(event);
            if (key != NULL)
            {
                keyboard_key_set_report_state((Key*)event.key, report_state);//protect key state
            }
        }
        break;
    case PACKET_CODE_LARGE_SET:
    case PACKET_CODE_LARGE_GET:
        large_packet_process((PacketLargeData*)buf);
        break;
    default:
        packet_process_user(buf, len);
        break;
    }
}

void packet_process(uint8_t *buf, uint16_t len)
{
    packet_process_buffer(buf, len);
    packet_send_response(buf, len, AMP_CHANNEL_CONTROL, AMP_FRAME_FLAG_RESP, 0, false);
}

void packet_process_advanced_key(PacketData*data)
{   
    PacketAdvancedKey* packet = (PacketAdvancedKey*)data;
    uint16_t key_index = packet->index;
    AdvancedKeyConfiguration config_buffer;
    if (data->code == PACKET_CODE_SET)
    {
        memcpy(&config_buffer, &packet->data, sizeof(AdvancedKeyConfiguration));
        AdvancedKeyConfiguration* config = &g_keyboard_advanced_keys[key_index].config;
        config->mode = config_buffer.mode;
#if defined(NEXUS_ENABLE) && NEXUS_IS_SLAVE
        config->calibration_mode = config_buffer.calibration_mode;
#endif
        config->activation_value = config_buffer.activation_value;
        config->deactivation_value = config_buffer.deactivation_value;
        config->trigger_distance = config_buffer.trigger_distance;
        config->release_distance = config_buffer.release_distance;
        config->trigger_speed = config_buffer.trigger_speed;
        config->release_speed = config_buffer.release_speed;
        config->upper_deadzone = config_buffer.upper_deadzone;
        config->lower_deadzone = config_buffer.lower_deadzone;
#if defined(NEXUS_ENABLE) && NEXUS_IS_SLAVE
        config->upper_bound = config_buffer.upper_bound;
        config->lower_bound = config_buffer.lower_bound;
#endif
    }
    else if (data->code == PACKET_CODE_GET)
    {   
        memcpy(&packet->data, &g_keyboard_advanced_keys[key_index].config, sizeof(AdvancedKeyConfiguration));
    }
}

void packet_process_rgb_base_config(PacketData*data)
{
    PacketRGBBaseConfig* packet = (PacketRGBBaseConfig*)data;
    if (data->code == PACKET_CODE_SET)
    {
        g_rgb_base_config.mode = packet->mode;
        g_rgb_base_config.rgb.r = packet->r;
        g_rgb_base_config.rgb.g = packet->g;
        g_rgb_base_config.rgb.b = packet->b;
        rgb_to_hsv(&g_rgb_base_config.hsv, &g_rgb_base_config.rgb);
        g_rgb_base_config.secondary_rgb.r = packet->secondary_r;
        g_rgb_base_config.secondary_rgb.g = packet->secondary_g;
        g_rgb_base_config.secondary_rgb.b = packet->secondary_b;
        rgb_to_hsv(&g_rgb_base_config.secondary_hsv, &g_rgb_base_config.secondary_rgb);
        g_rgb_base_config.speed = packet->speed;
        g_rgb_base_config.direction = packet->direction;
        g_rgb_base_config.density = packet->density;
        g_rgb_base_config.brightness = packet->brightness;
    }
    else if (data->code == PACKET_CODE_GET)
    {
        packet->type = PACKET_DATA_RGB_BASE_CONFIG;
        packet->mode = g_rgb_base_config.mode;
        packet->r = g_rgb_base_config.rgb.r;
        packet->g = g_rgb_base_config.rgb.g;
        packet->b = g_rgb_base_config.rgb.b;
        packet->secondary_r = g_rgb_base_config.secondary_rgb.r;
        packet->secondary_g = g_rgb_base_config.secondary_rgb.g;
        packet->secondary_b = g_rgb_base_config.secondary_rgb.b;
        packet->speed = g_rgb_base_config.speed;
        packet->direction = g_rgb_base_config.direction;
        packet->density = g_rgb_base_config.density;
        packet->brightness = g_rgb_base_config.brightness;
    }
}

void packet_process_rgb_config(PacketData*data)
{
    PacketRGBConfigs* packet = (PacketRGBConfigs*)data;
    if (data->code == PACKET_CODE_SET)
    {
        for (uint8_t i = 0; i < packet->length; i++)
        {
            uint16_t rgb_index = g_rgb_inverse_mapping[packet->data[i].index];
            if (rgb_index < RGB_NUM)
            {
                g_rgb_configs[rgb_index].mode  = packet->data[i].mode;
                g_rgb_configs[rgb_index].rgb.r = packet->data[i].r;
                g_rgb_configs[rgb_index].rgb.g = packet->data[i].g;
                g_rgb_configs[rgb_index].rgb.b = packet->data[i].b;
                rgb_to_hsv(&g_rgb_configs[rgb_index].hsv, &g_rgb_configs[rgb_index].rgb);
                g_rgb_configs[rgb_index].speed = packet->data[i].speed;
            }
        }
    }
    else if (data->code == PACKET_CODE_GET)
    {
        for (uint8_t i = 0; i < packet->length; i++)
        {
            if (packet->data[i].index >= TOTAL_KEY_NUM)
            {
                continue;
            }
            uint16_t key_index = packet->data[i].index;
            uint16_t rgb_index = g_rgb_inverse_mapping[key_index];
            if (rgb_index < RGB_NUM)
            {
                packet->data[i].index = key_index;
                packet->data[i].mode = g_rgb_configs[rgb_index].mode;
                packet->data[i].r = g_rgb_configs[rgb_index].rgb.r;
                packet->data[i].g = g_rgb_configs[rgb_index].rgb.g;
                packet->data[i].b = g_rgb_configs[rgb_index].rgb.b;
                packet->data[i].speed = g_rgb_configs[rgb_index].speed;
            }
        }
    }
}

void packet_process_keymap(PacketData*data)
{
    PacketKeymap* packet = (PacketKeymap*)data;
    if (data->code == PACKET_CODE_SET)
    {       
        for (uint16_t i = 0; i < packet->length; i++)
        {
            g_keymap[packet->layer][packet->start + i] = packet->keymap[i];
            if (!g_keymap_lock[packet->start + i])
            {
                g_keymap_cache[packet->start + i] = layer_get_keycode(packet->start + i, g_current_layer); 
            }
        }
    }
    else if (data->code == PACKET_CODE_GET)
    {
        for (uint16_t i = 0; i < packet->length; i++)
        {
            packet->keymap[i] = g_keymap[packet->layer][packet->start + i];
        }
    }
}

void packet_process_dynamic_key(PacketData*data)
{
    PacketDynamicKey* packet = (PacketDynamicKey*)data;
    if (data->code == PACKET_CODE_SET)
    {       
        if (packet->index<DYNAMIC_KEY_NUM)
        {
            memcpy(&g_dynamic_keys[packet->index], &packet->dynamic_key, sizeof(DynamicKey));
        }
    }
    else if (data->code == PACKET_CODE_GET)
    {
        packet->type = PACKET_DATA_DYNAMIC_KEY;
        uint8_t dk_index = packet->index;
        if (dk_index < DYNAMIC_KEY_NUM)
        {
            memcpy(&packet->dynamic_key,&g_dynamic_keys[dk_index],sizeof(DynamicKey));
        }
    }
}

void packet_process_profile_index(PacketData*data)
{
    PacketProfileIndex* packet = (PacketProfileIndex*)data;
    if (data->code == PACKET_CODE_SET)
    {       
        keyboard_set_profile_index(packet->index);
    }
    else if (data->code == PACKET_CODE_GET)
    {
        packet->index = g_current_profile_index;
    }
}

void packet_process_config(PacketData*data)
{
    PacketConfig* packet = (PacketConfig*)data;
    if (data->code == PACKET_CODE_SET)
    {       
        for (int i = 0; i < packet->length; i++)
        {
            if (packet->data[i].index >= KEYBOARD_CONFIG_NUM)
            {
                continue;
            }
            if (packet->data[i].value)
            {
                BIT_SET(g_keyboard_config.raw, packet->data[i].index);
            }
            else
            {
                BIT_RESET(g_keyboard_config.raw, packet->data[i].index);
            }
        }
    }
    else if (data->code == PACKET_CODE_GET)
    {
        for (int i = 0; i < packet->length; i++)
        {
            if (packet->data[i].index < KEYBOARD_CONFIG_NUM)
            {
                packet->data[i].value = (bool)BIT_GET(g_keyboard_config.raw, packet->data[i].index);
            }
        }
    }
}

void packet_process_debug(PacketData*data)
{
    PacketDebug* packet = (PacketDebug*)data;
    if (data->code == PACKET_CODE_GET)
    {
        if (packet->length > 6)
        {
            packet->length = 6;
        }
        packet->tick = g_keyboard_tick;
        debug_length = packet->length;
        for (uint8_t i = 0; i < packet->length; i++)
        {
            uint8_t key_index =  packet->data[i].index;
            debug_buffer[i] = key_index;
            if (key_index < ADVANCED_KEY_NUM)
            {
                packet->data[i].raw = g_keyboard_advanced_keys[key_index].raw;
                packet->data[i].value = g_keyboard_advanced_keys[key_index].value;
                packet->data[i].state = g_keyboard_advanced_keys[key_index].key.state;
                packet->data[i].report_state = g_keyboard_advanced_keys[key_index].key.report_state;
            }
        }
    }
}

void packet_process_macro(PacketData*data)
{
#ifdef MACRO_ENABLE
    PacketMacro* packet = (PacketMacro*)data;
    if (packet->length>4)
    {
        return;
    }
    if (packet->macro_index >= MACRO_NUM) 
    {
        return;
    }
    if (data->code == PACKET_CODE_SET)
    {
        for (uint8_t i = 0; i < packet->length; i++)
        {
            uint16_t index = packet->data[i].index;
            if (index < MACRO_MAX_ACTIONS)
            {
                MacroAction *action = &g_macros[packet->macro_index].actions[index];
                action->delay = packet->data[i].delay;
                action->event.event = packet->data[i].event;
                action->event.is_virtual = packet->data[i].is_virtual;
                action->event.keycode = packet->data[i].keycode;
                void * key = keyboard_get_key(packet->data[i].key_id);
                if (key != NULL)
                {
                    action->event.key = key;
                }
            }
        }
    }
    else if (data->code == PACKET_CODE_GET)
    {
        for (uint8_t i = 0; i < packet->length; i++)
        {
            uint8_t index = packet->data[i].index;
            if (index < MACRO_MAX_ACTIONS)
            {
                MacroAction *action = &g_macros[packet->macro_index].actions[index];
                packet->data[i].delay = action->delay;
                packet->data[i].event = action->event.event;
                packet->data[i].is_virtual = action->event.is_virtual;
                packet->data[i].keycode = action->event.keycode;
                if (action->event.key != NULL)
                {
                    packet->data[i].key_id = ((Key*)action->event.key)->id;
                }
            }
        }
    }
#endif
}

void packet_process_feature(PacketData *data)
{
    PacketFeature *packet = (PacketFeature *)data;
    UNUSED(packet);
    if (data->code == PACKET_CODE_GET)
    {
        //todo
    }
}

void packet_send_version_packet(void)
{
    uint8_t buf[64] = {0};
    PacketVersion* packet = (PacketVersion*)buf;
    packet->code = PACKET_CODE_GET;
    packet->type = PACKET_DATA_VERSION;
    packet_process_buffer((uint8_t*)packet, sizeof(buf));
    packet_send_response((uint8_t*)packet, sizeof(buf), AMP_CHANNEL_CONTROL, 0, 0, false);
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
    uint8_t buf[64];
    PacketDebug* packet = (PacketDebug*)buf;
    packet->code = PACKET_CODE_GET;
    packet->type = PACKET_DATA_DEBUG;
    packet->length = debug_length;
    for (uint8_t i = 0; i < debug_length; i++)
    {
        packet->data[i].index = debug_buffer[i];
    }
    packet_process_buffer((uint8_t*)packet, sizeof(PacketDebug) + debug_length * sizeof(packet->data[0]));
    packet_send_response((uint8_t*)packet, sizeof(PacketDebug) + packet->length * sizeof(packet->data[0]), AMP_CHANNEL_DEBUG, 0, 0, true);
}

__WEAK void packet_process_user(uint8_t *buf, uint16_t len)
{

}
