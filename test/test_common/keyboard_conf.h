/*
 * Copyright (c) 2024 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef KEYBOARD_CONF_H_
#define KEYBOARD_CONF_H_

/********************/
/* Keyboard General */
/********************/
#define LAYER_NUM               5
#define ADVANCED_KEY_NUM        64
#define KEY_NUM                 0
//#define FIXED_POINT_EXPERIMENTAL
//#define CONTINOUS_POLL
#define OPTIMIZE_FOR_FLOAT_DIVISION
//#define CONTINOUS_DEBUG
#define DEBUG_INTERVAL 1
#define DYNAMICKEY_ENABLE
//#define STORAGE_ENABLE

/********************/
/* Keyboard Default */
/********************/
#define DEFAULT_ADVANCED_KEY_MODE   KEY_ANALOG_NORMAL_MODE
#define DEFAULT_CALIBRATION_MODE    KEY_AUTO_CALIBRATION_UNDEFINED
#define DEFAULT_TRIGGER_DISTANCE    0.08
#define DEFAULT_RELEASE_DISTANCE    0.08
#define DEFAULT_UPPER_DEADZONE      0.00
#define DEFAULT_LOWER_DEADZONE      0.2
#define DEFAULT_ACTIVATION_VALUE    0.5
#define DEFAULT_DEACTIVATION_VALUE  0.49
#define DEFAULT_ESTIMATED_RANGE     500

/**********/
/* Analog */
/**********/
#define RING_BUF_LEN            8

/*******/
/* RGB */
/*******/
#define RGB_ENABLE
#define RGB_NUM                 (ADVANCED_KEY_NUM)
#define ONE_PULSE               (60)
#define ZERO_PULSE              (29)
#define NONE_PULSE              (0)
#define RGB_RESET_LENGTH        (400)
#define RGB_BUFFER_LENGTH       (RGB_RESET_LENGTH+3*8*(RGB_NUM))
#define RGB_MAX_DURATION        2000
#define FADING_DISTANCE         5.0f
#define JELLY_DISTANCE          10.0f
#define PORT_LOCATION           {1, 4.5}
#define RGB_FLASH_MAX_DURATION  1000
#define RGB_FLASH_RIPPLE_SPEED  0.03
#define RGB_DEFAULT_MODE        RGB_MODE_LINEAR
#define RGB_DEFAULT_SPEED       0.03
#define RGB_DEFAULT_COLOR_HSV   {273, 78, 99}
#define RGB_LEFT                0.0f
#define RGB_TOP                 4.5f
#define RGB_RIGHT               15.0f
#define RGB_BOTTOM              -0.5f
#define RGB_USE_LIST_EXPERIMENTAL

/************/
/* Joystick */
/************/
#define JOYSTICK_BUTTON_COUNT 32
#define JOYSTICK_AXIS_COUNT 6

/********/
/* MIDI */
/********/
#define MIDI_REF_VELOCITY 0.01

/**********/
/* Filter */
/**********/
//#define TOLERANCE               3
#define FILTER_ENABLE

/**********/
/* Record */
/**********/
//#define STATIC_RECORD
//#define ANALOG_HISTORY_ENABLE
//#define KPS_ENABLE
//#define KPS_HISTORY_ENABLE
//#define BIT_STREAM_ENABLE
//#define COUNTER_ENABLE

#define KPS_HISTORY_LENGTH      65
#define BIT_STREAM_LENGTH       128
#define ANALOG_HISTORY_LENGTH   129
#define RECORD_MAX_KEY_NUM      8
#define KPS_REFRESH_RATE        144

/*******/
/* USB */
/*******/
#define USB_POLLING_INTERVAL_MS 1
#define FIXED_CONTROL_ENDPOINT_SIZE 0x40
#define VENDOR_ID 0xFEED
#define PRODUCT_ID 22319
#define DEVICE_VER 2
#define FIXED_NUM_CONFIGURATIONS 1

#define MANUFACTURER "dwdwdw5163&Lzq12345"
#define PRODUCT "Oholeo Keyboard"
#define SERIAL_NUMBER "2022123456"

#define RAW_ENABLE
//#define KEYBOARD_SHARED_EP
#define SHARED_EP_ENABLE
#define NKRO_ENABLE
#define MOUSE_ENABLE
#define MOUSE_SHARED_EP
#define EXTRAKEY_ENABLE
#define JOYSTICK_ENABLE
#define JOYSTICK_SHARED_EP
//#define DIGITIZER_ENABLE
//#define DIGITIZER_SHARED_EP
//#define PROGRAMMABLE_BUTTON_ENABLE
#define MIDI_ENABLE

#endif /* KEYBOARD_CONF_H_ */
