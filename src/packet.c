#include "packet.h"
#include "rgb.h"
#include "layer.h"
#include "driver.h"

#include "string.h"

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


void packet_process(uint8_t *buf, uint16_t len)
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
        case PACKET_DATA_CONFIG_INDEX:
            packet_process_config_index(packet);
            break;
        case PACKET_DATA_CONFIG:
            packet_process_config(packet);
            break;
        case PACKET_DATA_DEBUG:
            packet_process_debug(packet);
            break;
        default:
            break;
        }
        break;
    case PACKET_CODE_ACTION:
        keyboard_operation_event_handler(MK_EVENT(((((PacketBase*)packet)->buf[0]) << 8) | KEYBOARD_OPERATION, KEYBOARD_EVENT_KEY_DOWN, NULL));
        break;
    default:
        break;
    }
    hid_send_raw(buf, 63);
}

void packet_process_advanced_key(PacketData*data)
{   
    PacketAdvancedKey* packet = (PacketAdvancedKey*)data;
    uint16_t key_index = packet->index;
    if (data->code == PACKET_CODE_SET)
    {
        command_advanced_key_config_anti_normalize(&g_keyboard_advanced_keys[key_index].config, &packet->data);
    }
    else if (data->code == PACKET_CODE_GET)
    {
        command_advanced_key_config_normalize(&packet->data, &g_keyboard_advanced_keys[key_index].config);
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
            uint8_t key_index =  g_rgb_inverse_mapping[packet->data[i].index];
            if (key_index < RGB_NUM)
            {
                uint8_t rgb_index = g_rgb_inverse_mapping[key_index];
                packet->data[i].index = key_index;
                packet->data[i].mode = g_rgb_configs[rgb_index].mode;
                packet->data[i].r = g_rgb_configs[rgb_index].rgb.r;
                packet->data[i].g = g_rgb_configs[rgb_index].rgb.g;
                packet->data[i].b = g_rgb_configs[rgb_index].rgb.b;
                memcpy(&packet->data[i].speed, &g_rgb_configs[rgb_index].speed, sizeof(float));
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
            switch (((DynamicKey*)packet->dynamic_key)->type)
            {
            case DYNAMIC_KEY_STROKE:
                dynamic_key_stroke_anti_normalize((DynamicKeyStroke4x4*)&g_dynamic_keys[packet->index],
                    (DynamicKeyStroke4x4Normalized*)&packet->dynamic_key);
                break;
            default:
                memcpy(&g_dynamic_keys[packet->index], &packet->dynamic_key, sizeof(DynamicKey));
                break;
            }
        }
    }
    else if (data->code == PACKET_CODE_GET)
    {
        packet->type = PACKET_DATA_DYNAMIC_KEY;
        uint8_t dk_index = packet->index;
        if (dk_index >= DYNAMIC_KEY_NUM)
        {
            return;
        }
        switch (g_dynamic_keys[dk_index].type)
        {
        case DYNAMIC_KEY_STROKE:
            dynamic_key_stroke_normalize((DynamicKeyStroke4x4Normalized*)&packet->dynamic_key,
                (DynamicKeyStroke4x4*)&g_dynamic_keys[dk_index]);
            break;
        default:
            memcpy(&packet->dynamic_key,&g_dynamic_keys[dk_index],sizeof(DynamicKey));
            break;
        }
    }
}

void packet_process_config_index(PacketData*data)
{
    PacketConfigIndex* packet = (PacketConfigIndex*)data;
    if (data->code == PACKET_CODE_SET)
    {       
        keyboard_set_config_index(packet->index);
    }
    else if (data->code == PACKET_CODE_GET)
    {
        packet->index = g_current_config_index;
    }
}

void packet_process_config(PacketData*data)
{
    PacketConfig* packet = (PacketConfig*)data;
    if (data->code == PACKET_CODE_SET)
    {       
        for (int i = 0; i < packet->length; i++)
        {
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
            packet->data[i].value = (bool)BIT_GET(g_keyboard_config.raw, packet->data[i].index);
        }
    }
}

void packet_process_debug(PacketData*data)
{
    PacketDebug* packet = (PacketDebug*)data;
    if (data->code == PACKET_CODE_GET)
    {       
        for (uint8_t i = 0; i < packet->length; i++)
        {
            uint8_t key_index =  packet->data[i].index;
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