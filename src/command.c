/*
 * Copyright (c) 2024 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "command.h"
#include "keyboard.h"
#include "keyboard_def.h"
#include "rgb.h"
#include "string.h"
#include "packet.h"
#include "layer.h"
#include "driver.h"

static inline void command_advanced_key_config_normalize(AdvancedKeyConfigurationNormalized* buffer, AdvancedKeyConfiguration* config)
{
    buffer->mode = config->mode;
    //buffer->calibration_mode = config->calibration_mode;
    buffer->activation_value = A_NORM(config->activation_value);
    buffer->deactivation_value = A_NORM(config->deactivation_value);
    buffer->trigger_distance = A_NORM(config->trigger_distance);
    buffer->release_distance = A_NORM(config->release_distance);
    buffer->trigger_speed = A_NORM(config->trigger_speed);
    buffer->release_speed = A_NORM(config->release_speed);
    buffer->upper_deadzone = A_NORM(config->upper_deadzone);
    buffer->lower_deadzone = A_NORM(config->lower_deadzone);
    //buffer->upper_bound = config->upper_bound;
    //buffer->lower_bound = config->lower_bound;
}

static inline void command_advanced_key_config_anti_normalize(AdvancedKeyConfiguration* config, AdvancedKeyConfigurationNormalized* buffer)
{
    config->mode = buffer->mode;
    //config->calibration_mode = buffer->calibration_mode;
    config->activation_value = A_ANIT_NORM(buffer->activation_value);
    config->deactivation_value = A_ANIT_NORM(buffer->deactivation_value);
    config->trigger_distance = A_ANIT_NORM(buffer->trigger_distance);
    config->release_distance = A_ANIT_NORM(buffer->release_distance);
    config->trigger_speed = A_ANIT_NORM(buffer->trigger_speed);
    config->release_speed = A_ANIT_NORM(buffer->release_speed);
    config->upper_deadzone = A_ANIT_NORM(buffer->upper_deadzone);
    config->lower_deadzone = A_ANIT_NORM(buffer->lower_deadzone);
    //config->upper_bound = buffer->upper_bound;
    //config->lower_bound = buffer->lower_bound;
}

void unload_cargo(uint8_t *buf)
{
    switch (((PacketData*)buf)->type)
    {
    case PACKET_DATA_ADVANCED_KEY: // Advanced Key
        {
            PacketAdvancedKey* packet = (PacketAdvancedKey*)buf;
            uint16_t key_index = packet->index;
            command_advanced_key_config_anti_normalize(&g_keyboard_advanced_keys[key_index].config, &packet->data);
        }
        break;
#ifdef RGB_ENABLE
    case PACKET_DATA_RGB_BASE_CONFIG: // Global LED
        {
            PacketRGBBaseConfig* packet = (PacketRGBBaseConfig*)buf;
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
        break;
    case PACKET_DATA_RGB_CONFIG: // LED
        {
            PacketRGBConfigs* packet = (PacketRGBConfigs*)buf;
            for (uint8_t i = 0; i < packet->length; i++)
            {
                uint16_t key_index = g_rgb_mapping[packet->data[i].index];
                if (key_index < RGB_NUM)
                {
                    g_rgb_configs[key_index].mode  = packet->data[i].mode;
                    g_rgb_configs[key_index].rgb.r = packet->data[i].r;
                    g_rgb_configs[key_index].rgb.g = packet->data[i].g;
                    g_rgb_configs[key_index].rgb.b = packet->data[i].b;
                    rgb_to_hsv(&g_rgb_configs[key_index].hsv, &g_rgb_configs[key_index].rgb);
                    g_rgb_configs[key_index].speed = packet->data[i].speed;
                }
            }
        }
        break;
#endif
    case PACKET_DATA_KEYMAP: // Keymap
        {
            PacketKeymap* packet = (PacketKeymap*)buf;
            //if (packet->layer < LAYER_NUM && packet->start + packet->length <= (ADVANCED_KEY_NUM + KEY_NUM))
            //{
            //    memcpy(&g_keymap[packet->layer][packet->start], packet->keymap, packet->length * sizeof(Keycode));
            //}
            for (uint16_t i = 0; i < packet->length; i++)
            {
                g_keymap[packet->layer][packet->start + i] = packet->keymap[i];
                if (!g_keymap_lock[packet->start + i])
                {
                    g_keymap_cache[packet->start + i] = layer_get_keycode(packet->start + i, g_current_layer); 
                }
            }
            
        }
        break;
#ifdef DYNAMICKEY_ENABLE
    case PACKET_DATA_DYNAMIC_KEY: // Dynamic Key
        {
            PacketDynamicKey *packet = (PacketDynamicKey *)buf;
            if (packet->index<DYNAMIC_KEY_NUM)
            {
                switch (((DynamicKey*)packet->dynamic_key)->type)
                {
                case DYNAMIC_KEY_STROKE:
                    dynamic_key_stroke_anti_normalize((DynamicKeyStroke4x4*)&g_keyboard_dynamic_keys[packet->index],
                        (DynamicKeyStroke4x4Normalized*)&packet->dynamic_key);
                    break;
                default:
                    memcpy(&g_keyboard_dynamic_keys[packet->index], &packet->dynamic_key, sizeof(DynamicKey));
                    break;
                }
            }
        }
        break;
#endif
    default:
        break;
    }
}

static uint16_t page_index;
void start_load_cargo(void)
{
    page_index = 0;
}

int load_cargo(void)
{
    uint8_t buf[64] = {0};
    ((PacketData *)buf)->code = 0xFF;
    // max 62 remain
    switch ((page_index >> 8) & 0xFF)
    {
    case PACKET_DATA_ADVANCED_KEY: // Advanced Key
        {
            PacketAdvancedKey *packet = (PacketAdvancedKey *)buf;
            packet->type = PACKET_DATA_ADVANCED_KEY;
            uint8_t key_index = page_index & 0xFF;
            packet->index = key_index;
            command_advanced_key_config_normalize(&packet->data, &g_keyboard_advanced_keys[key_index].config);
            if (!hid_send_raw(buf, 63))
            {
                page_index++;
                if ((page_index & 0xFF) >= ADVANCED_KEY_NUM)
                {
                    page_index = 0x0100;
                }
            }
        }
        return 1;
#ifdef RGB_ENABLE
    case PACKET_DATA_RGB_BASE_CONFIG: // Global LED
        {
            PacketRGBBaseConfig *packet = (PacketRGBBaseConfig *)buf;
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
            if (!hid_send_raw(buf,63))
            {
                page_index = 0x0200;
            }
        }
        return 1;
    case PACKET_DATA_RGB_CONFIG: // LED
        {
            PacketRGBConfigs *packet = (PacketRGBConfigs *)buf;
            uint8_t key_base_index = (page_index & 0xFF)*6;
            packet->type = PACKET_DATA_RGB_CONFIG;
            packet->length = key_base_index + 6 <= RGB_NUM ? 6 : RGB_NUM - key_base_index;
            for (uint8_t i = 0; i < packet->length; i++)
            {
                uint8_t key_index = key_base_index + i;
                if (key_index < RGB_NUM)
                {
                    uint8_t rgb_index = g_rgb_mapping[key_index];
                    packet->data[i].index = key_index;
                    packet->data[i].mode = g_rgb_configs[rgb_index].mode;
                    packet->data[i].r = g_rgb_configs[rgb_index].rgb.r;
                    packet->data[i].g = g_rgb_configs[rgb_index].rgb.g;
                    packet->data[i].b = g_rgb_configs[rgb_index].rgb.b;
                    memcpy(&packet->data[i].speed, &g_rgb_configs[rgb_index].speed, sizeof(float));
                }
            }
            if (!hid_send_raw(buf,63))
            {
                page_index++;
                if ((page_index & 0xFF)*6>=RGB_NUM)
                {
                    page_index = 0x0300;
                }
            }
        }
        return 1;
#endif
#define LAYER_PAGE_LENGTH 16
#define LAYER_PAGE_EXPECTED_NUM ((ADVANCED_KEY_NUM + KEY_NUM + 15) / 16)
    case PACKET_DATA_KEYMAP: // Keymap
        {
            PacketKeymap *packet = (PacketKeymap *)buf;
            packet->type = PACKET_DATA_KEYMAP;
            uint8_t layer_index = (page_index & 0xFF) / LAYER_PAGE_EXPECTED_NUM;
            uint8_t layer_page_index = (page_index & 0xFF) % LAYER_PAGE_EXPECTED_NUM;
            packet->layer = layer_index;
            packet->start = layer_page_index*LAYER_PAGE_LENGTH;
            packet->length = (layer_page_index + 1)*LAYER_PAGE_LENGTH <= (ADVANCED_KEY_NUM + KEY_NUM) ? 
                LAYER_PAGE_LENGTH : 
                (ADVANCED_KEY_NUM + KEY_NUM) - layer_page_index*LAYER_PAGE_LENGTH;
            memcpy(&packet->keymap, &g_keymap[layer_index][packet->start], packet->length*sizeof(Keycode));
            if (!hid_send_raw(buf,63))
            {
                page_index++;
                if ((page_index & 0xFF)>=LAYER_NUM*LAYER_PAGE_EXPECTED_NUM)
                {
                    page_index = 0x0400;
                }
            }
        }
        return 1;
#ifdef DYNAMICKEY_ENABLE
    case PACKET_DATA_DYNAMIC_KEY: // Dynamic Key
        PacketDynamicKey *packet = (PacketDynamicKey *)buf;
        packet->type = PACKET_DATA_DYNAMIC_KEY;
        uint8_t dk_index = (page_index & 0xFF);
        if (dk_index >= DYNAMIC_KEY_NUM)
        {
            page_index=0x8000;
            return 1;
        }
        packet->index = dk_index;
        switch (g_keyboard_dynamic_keys[dk_index].type)
        {
        case DYNAMIC_KEY_STROKE:
            dynamic_key_stroke_normalize((DynamicKeyStroke4x4Normalized*)&packet->dynamic_key,
                (DynamicKeyStroke4x4*)&g_keyboard_dynamic_keys[dk_index]);
            break;
        default:
            memcpy(&packet->dynamic_key,&g_keyboard_dynamic_keys[dk_index],sizeof(DynamicKey));
            break;
        }
        if (!hid_send_raw(buf,63))
        {
            page_index++;
        }
        return 1;
#endif
    case 0x80: // config index
        buf[1] = 0x80;
        buf[2] = g_current_config_index;
        if (!hid_send_raw(buf,63))
        {
            page_index = 0xFFFF;
        }
        return 1;
    default:
        return 0;
    }
}

void send_debug_info(void)
{
    static uint8_t buffer[64];
    static uint32_t report_num = 0;
    static uint8_t times = 0;
    times++;
    if (times < DEBUG_INTERVAL)
    {
        return;
    }
    times = 0;
    PacketDebug* packet = (PacketDebug*)buffer;

#ifdef CONTINOUS_DEBUG
    packet->code = 0xFE;
    packet->length = 5;
    for (int i = 0; i < packet->length; i++)
    {
        uint8_t key_index = (report_num + i) % ADVANCED_KEY_NUM;
        packet->data[i].index = key_index;
        packet->data[i].state = g_keyboard_advanced_keys[key_index].key.report_state;
        packet->data[i].raw = g_keyboard_advanced_keys[key_index].raw;
        packet->data[i].value = g_keyboard_advanced_keys[key_index].value;
    }
    hid_send_raw(buffer, 63);
    report_num += packet->length;
#else
    static AnalogRawValue analog_buffer[64];
    uint8_t count = 0;
    packet->code = 0xFE;
    int i = 0;
    for (i = report_num%ADVANCED_KEY_NUM; i < ADVANCED_KEY_NUM; i++)
    {
        const uint8_t key_index = i;
        AdvancedKey* key = &g_keyboard_advanced_keys[key_index];
        if (analog_buffer[key_index] != key->raw)
        {
            packet->data[count].index = i;
            packet->data[count].state = key->key.report_state;
            packet->data[count].raw = key->raw;
            packet->data[count].value = key->value;
        }
        analog_buffer[key_index] = key->raw;
        count++;
        if (count >= 5)
        {
            break;
        }
    }
    packet->length = count;
    report_num += (i - report_num%ADVANCED_KEY_NUM);
    if (packet->length)
    {
        hid_send_raw(buffer, 63);
    }
#endif
}

void command_parse(uint8_t *buf, uint8_t len)
{
    UNUSED(len);
    PacketBase *packet = (PacketBase *)buf;
    switch (packet->code)
    {
    case 0x80: // Save
        keyboard_save();
        break;
    case 0x81: // System Reset
        keyboard_reboot();
        break;
    case 0x82: // Factory Reset
        keyboard_factory_reset();
        break;
    case 0x90:
        keyboard_set_config_index(buf[1]);
        break;
    case 0xB0: // Set Debug
        g_keyboard_state = buf[1];
        break;
    case 0xB1: // upload config
        g_keyboard_state = KEYBOARD_STATE_UPLOAD_CONFIG;
        start_load_cargo();
        break;
    case 0xFF: // Legacy
        unload_cargo(buf);
        break;
    default:
        break;
    }
}
