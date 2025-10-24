#include <gtest/gtest.h>

#include "rgb.h"
#include "packet.h"

TEST(Packet, PacketKeymap)
{
    static PacketKeymap packet = 
    {
        .code = PACKET_CODE_SET,
        .type = PACKET_DATA_KEYMAP,
        .layer = 1,
        .start = 3,
        .length = 5,
        .keymap = {4,5,6,7,8}
    };
    packet_process((uint8_t*)&packet,sizeof(packet));
    EXPECT_EQ(g_keyboard.keymap[1][3], 4);
    EXPECT_EQ(g_keyboard.keymap[1][4], 5);
    EXPECT_EQ(g_keyboard.keymap[1][5], 6);
    EXPECT_EQ(g_keyboard.keymap[1][6], 7);
    EXPECT_EQ(g_keyboard.keymap[1][7], 8);
}

TEST(Packet, SetPacketAdvancedKey)
{
    static PacketAdvancedKey packet = 
    {
        .code = PACKET_CODE_SET,
        .type = PACKET_DATA_ADVANCED_KEY,
        .index = 3,
        .data =
        {
            .mode = DEFAULT_ADVANCED_KEY_MODE,
            .calibration_mode = DEFAULT_CALIBRATION_MODE,
            .activation_value = DEFAULT_ACTIVATION_VALUE,
            .deactivation_value = DEFAULT_DEACTIVATION_VALUE,
            .trigger_distance = DEFAULT_TRIGGER_DISTANCE,
            .release_distance = DEFAULT_RELEASE_DISTANCE,
            .trigger_speed = 0.01,
            .release_speed = 0.01,
            .upper_deadzone = DEFAULT_UPPER_DEADZONE,
            .lower_deadzone = DEFAULT_LOWER_DEADZONE,
            .upper_bound = 2048,
            .lower_bound = 0,
        }
    };
    packet_process((uint8_t*)&packet,sizeof(packet));
    EXPECT_EQ(g_keyboard.advanced_keys[3].config.mode, DEFAULT_ADVANCED_KEY_MODE);
    //EXPECT_EQ(g_keyboard.advanced_keys[3].config.calibration_mode, DEFAULT_CALIBRATION_MODE);
    EXPECT_FLOAT_EQ(g_keyboard.advanced_keys[3].config.activation_value, A_ANIT_NORM(DEFAULT_ACTIVATION_VALUE));
    EXPECT_FLOAT_EQ(g_keyboard.advanced_keys[3].config.deactivation_value, A_ANIT_NORM(DEFAULT_DEACTIVATION_VALUE));
    EXPECT_FLOAT_EQ(g_keyboard.advanced_keys[3].config.trigger_distance, A_ANIT_NORM(DEFAULT_TRIGGER_DISTANCE));
    EXPECT_FLOAT_EQ(g_keyboard.advanced_keys[3].config.release_distance, A_ANIT_NORM(DEFAULT_RELEASE_DISTANCE));
    EXPECT_FLOAT_EQ(g_keyboard.advanced_keys[3].config.trigger_speed, A_ANIT_NORM(0.01));
    EXPECT_FLOAT_EQ(g_keyboard.advanced_keys[3].config.release_speed, A_ANIT_NORM(0.01));
    EXPECT_FLOAT_EQ(g_keyboard.advanced_keys[3].config.upper_deadzone, A_ANIT_NORM(DEFAULT_UPPER_DEADZONE));
    EXPECT_FLOAT_EQ(g_keyboard.advanced_keys[3].config.lower_deadzone, A_ANIT_NORM(DEFAULT_LOWER_DEADZONE));
    //EXPECT_FLOAT_EQ(g_keyboard.advanced_keys[3].config.upper_bound, 2048);
    //EXPECT_FLOAT_EQ(g_keyboard.advanced_keys[3].config.lower_bound, 0);
}

TEST(Packet, SetPacketRGBConfigs)
{
    static PacketRGBConfigs packet = 
    {
        .code = PACKET_CODE_SET,
        .type = PACKET_DATA_RGB_CONFIG,
        .length = 2,
        .data =
        {
            {
                .index = 3,
                .mode = RGB_MODE_LINEAR,
                .r = 255,
                .g = 0,
                .b = 0,
                .speed = 0.01
            },
            {
                .index = 5,
                .mode = RGB_MODE_FADING_DIAMOND_RIPPLE,
                .r = 0,
                .g = 255,
                .b = 0,
                .speed = 0.01
                
            }
        }
    };
    packet_process((uint8_t*)&packet,sizeof(packet));

    uint16_t key_index;
    key_index = g_rgb_mapping[3];
    ColorHSV hsv;
    ColorRGB rgb;
    rgb.r = 255;
    rgb.g = 0;
    rgb.b = 0;
    rgb_to_hsv(&hsv,&rgb);
    EXPECT_TRUE(!memcmp(&hsv,&g_rgb_configs[key_index].hsv,sizeof(hsv)));
    EXPECT_TRUE(!memcmp(&packet.data->r,&g_rgb_configs[key_index].rgb,sizeof(ColorRGB)));
    EXPECT_EQ(g_rgb_configs[key_index].mode, RGB_MODE_LINEAR);
    EXPECT_FLOAT_EQ(g_rgb_configs[key_index].speed, 0.01);

    key_index = g_rgb_mapping[5];
    rgb.r = 0;
    rgb.g = 255;
    rgb.b = 0;
    rgb_to_hsv(&hsv,&rgb);
    EXPECT_EQ(hsv.h, g_rgb_configs[key_index].hsv.h);
    EXPECT_EQ(hsv.s, g_rgb_configs[key_index].hsv.s);
    EXPECT_EQ(hsv.v, g_rgb_configs[key_index].hsv.v);
    EXPECT_TRUE(!memcmp(&rgb,&g_rgb_configs[key_index].rgb,sizeof(ColorRGB)));
    EXPECT_EQ(g_rgb_configs[key_index].mode, RGB_MODE_FADING_DIAMOND_RIPPLE);
    EXPECT_FLOAT_EQ(g_rgb_configs[key_index].speed, 0.01);
}


TEST(Packet, SetPacketDynamicKey)
{
    static DynamicKeyStroke4x4Normalized dynamic_key = 
    {
        .type = DYNAMIC_KEY_STROKE,
        .key_binding = {KEY_A, KEY_B, KEY_C, KEY_D},
        .key_control = {
            DKS_KEY_CONTROL(DKS_HOLD,   DKS_HOLD,   DKS_HOLD,   DKS_RELEASE), 
            DKS_KEY_CONTROL(DKS_RELEASE,DKS_HOLD,   DKS_RELEASE,DKS_RELEASE),
            DKS_KEY_CONTROL(DKS_RELEASE,DKS_HOLD,   DKS_TAP,    DKS_HOLD),
            DKS_KEY_CONTROL(DKS_HOLD,   DKS_RELEASE,DKS_TAP,    DKS_RELEASE)
        },
        .press_begin_distance = 0.25,
        .press_fully_distance = 0.75,
        .release_begin_distance = 0.75,
        .release_fully_distance = 0.25,
    };
    static DynamicKey dynamic_key1 = 
    {
        .tk = 
        {
            .type = DYNAMIC_KEY_TOGGLE_KEY,
            .key_binding = KEY_A,
        }
    };
    uint8_t buffer[64];
    memset(buffer, 0, sizeof(buffer));
    PacketDynamicKey* packet = (PacketDynamicKey*)buffer;
    packet->code = PACKET_CODE_SET;
    packet->type = PACKET_DATA_DYNAMIC_KEY,
    packet->index = 1,
    memcpy(packet->dynamic_key,&dynamic_key,sizeof(DynamicKeyStroke4x4Normalized));
    packet_process((uint8_t*)packet,sizeof(packet));
    EXPECT_EQ(g_dynamic_keys[1].type, DYNAMIC_KEY_STROKE);
    EXPECT_FLOAT_EQ(g_dynamic_keys[1].dks.press_fully_distance, A_ANIT_NORM(dynamic_key.press_fully_distance));

    memset(buffer, 0, sizeof(buffer));
    packet->code = PACKET_CODE_SET;
    packet->type = PACKET_DATA_DYNAMIC_KEY,
    packet->index = 0,
    memcpy(packet->dynamic_key,&dynamic_key1,sizeof(DynamicKey));
    packet_process((uint8_t*)packet,sizeof(packet));
    EXPECT_EQ(g_dynamic_keys[1].type, DYNAMIC_KEY_STROKE);
    EXPECT_TRUE(!memcmp(&g_dynamic_keys[0], &dynamic_key1, sizeof(DynamicKey)));
}
