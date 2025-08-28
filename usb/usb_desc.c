/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

#include "hid_desc.h"
#include "usbd.h"
#include "keyboard.h"
#include "joystick.h"
#include "audio.h"

#define KEYBOARD_EPSIZE 8
#define SHARED_EPSIZE 32
#define MOUSE_EPSIZE 16
#define RAW_EPSIZE 0x40
#define CONSOLE_EPSIZE 32
#define MIDI_STREAM_EPSIZE 64
#define CDC_NOTIFICATION_EPSIZE 8
#define CDC_EPSIZE 16
#define JOYSTICK_EPSIZE 8
#define DIGITIZER_EPSIZE 8

/*
 * Interface indexes
 */
enum usb_interfaces {
#ifndef KEYBOARD_SHARED_EP
    KEYBOARD_INTERFACE,
#else
    SHARED_INTERFACE,
#    define KEYBOARD_INTERFACE SHARED_INTERFACE
#endif

// It is important that the Raw HID interface is at a constant
// interface number, to support Linux/OSX platforms and chrome.hid
// If Raw HID is enabled, let it be always 1.
#define ENDPOINT_DIR_IN                    0x80
#define ENDPOINT_DIR_OUT                   0x00
#ifdef RAW_ENABLE
    RAW_INTERFACE,
#endif

#if defined(MOUSE_ENABLE) && !defined(MOUSE_SHARED_EP)
    MOUSE_INTERFACE,
#endif

#if defined(SHARED_EP_ENABLE) && !defined(KEYBOARD_SHARED_EP)
    SHARED_INTERFACE,
#endif

#ifdef CONSOLE_ENABLE
    CONSOLE_INTERFACE,
#endif

#ifdef MIDI_ENABLE
    AC_INTERFACE,
    AS_INTERFACE,
#endif

#ifdef VIRTSER_ENABLE
    CCI_INTERFACE,
    CDI_INTERFACE,
#endif

#if defined(JOYSTICK_ENABLE) && !defined(JOYSTICK_SHARED_EP)
    JOYSTICK_INTERFACE,
#endif

#if defined(DIGITIZER_ENABLE) && !defined(DIGITIZER_SHARED_EP)
    DIGITIZER_INTERFACE,
#endif
    TOTAL_INTERFACES
};

#define IS_VALID_INTERFACE(i) ((i) >= 0 && (i) < TOTAL_INTERFACES)

#define NEXT_EPNUM __COUNTER__

/*
 * Endpoint numbers
 */
enum usb_endpoints {
    __unused_epnum__ = NEXT_EPNUM, // Endpoint numbering starts at 1

#ifndef KEYBOARD_SHARED_EP
    KEYBOARD_IN_EPNUM = NEXT_EPNUM,
#else
#    define KEYBOARD_IN_EPNUM SHARED_IN_EPNUM
#endif

#if defined(MOUSE_ENABLE) && !defined(MOUSE_SHARED_EP)
    MOUSE_IN_EPNUM = NEXT_EPNUM,
#else
#    define MOUSE_IN_EPNUM SHARED_IN_EPNUM
#endif

#ifdef RAW_ENABLE
    RAW_IN_EPNUM = NEXT_EPNUM,
#    ifdef USB_ENDPOINTS_ARE_REORDERABLE
#        define RAW_OUT_EPNUM RAW_IN_EPNUM
#    else
    RAW_OUT_EPNUM         = NEXT_EPNUM,
#    endif
#endif

#ifdef SHARED_EP_ENABLE
    SHARED_IN_EPNUM = NEXT_EPNUM,
#endif

#ifdef CONSOLE_ENABLE
    CONSOLE_IN_EPNUM = NEXT_EPNUM,
#endif

#ifdef MIDI_ENABLE
    MIDI_STREAM_IN_EPNUM = NEXT_EPNUM,
#    ifdef USB_ENDPOINTS_ARE_REORDERABLE
#        define MIDI_STREAM_OUT_EPNUM MIDI_STREAM_IN_EPNUM
#    else
    MIDI_STREAM_OUT_EPNUM = NEXT_EPNUM,
#    endif
#endif

#ifdef VIRTSER_ENABLE
    CDC_NOTIFICATION_EPNUM = NEXT_EPNUM,
    CDC_IN_EPNUM           = NEXT_EPNUM,
#    ifdef USB_ENDPOINTS_ARE_REORDERABLE
#        define CDC_OUT_EPNUM CDC_IN_EPNUM
#    else
    CDC_OUT_EPNUM         = NEXT_EPNUM,
#    endif
#endif

#ifdef JOYSTICK_ENABLE
#    if !defined(JOYSTICK_SHARED_EP)
    JOYSTICK_IN_EPNUM = NEXT_EPNUM,
#    else
#        define JOYSTICK_IN_EPNUM SHARED_IN_EPNUM
#    endif
#endif

#ifdef DIGITIZER_ENABLE
#    if !defined(DIGITIZER_SHARED_EP)
    DIGITIZER_IN_EPNUM = NEXT_EPNUM,
#    else
#        define DIGITIZER_IN_EPNUM SHARED_IN_EPNUM
#    endif
#endif
};

#ifndef KEYBOARD_SHARED_EP
#define KEYBOARD_EPIN_ADDR  (ENDPOINT_DIR_IN | KEYBOARD_IN_EPNUM)
#define KEYBOARD_EPOUT_ADDR (ENDPOINT_DIR_OUT | KEYBOARD_OUT_EPNUM)
#endif

#ifdef RAW_ENABLE
#define RAW_EPIN_ADDR  (ENDPOINT_DIR_IN | RAW_IN_EPNUM)
#define RAW_EPOUT_ADDR (ENDPOINT_DIR_OUT | RAW_OUT_EPNUM)
#endif

#if defined(MOUSE_ENABLE) && !defined(MOUSE_SHARED_EP)
#define MOUSE_EPIN_ADDR  (ENDPOINT_DIR_IN | MOUSE_IN_EPNUM)
#endif

#ifdef SHARED_EP_ENABLE
#define SHARED_EPIN_ADDR  (ENDPOINT_DIR_IN | SHARED_IN_EPNUM)
#ifdef KEYBOARD_SHARED_EP
#define SHARED_EPOUT_ADDR (ENDPOINT_DIR_OUT | SHARED_OUT_EPNUM)
#endif
#endif

#ifdef CONSOLE_ENABLE
#define CONSOLE_EPIN_ADDR  (ENDPOINT_DIR_IN | CONSOLE_IN_EPNUM)
#endif

#ifdef MIDI_ENABLE
#define MIDI_EPIN_ADDR  (ENDPOINT_DIR_IN | MIDI_STREAM_IN_EPNUM)
#define MIDI_EPOUT_ADDR  (ENDPOINT_DIR_OUT | MIDI_STREAM_OUT_EPNUM)
#endif

#if defined(JOYSTICK_ENABLE) && !defined(JOYSTICK_SHARED_EP)
#define JOYSTICK_EPIN_ADDR  (ENDPOINT_DIR_IN | JOYSTICK_IN_EPNUM)
#endif


#if defined(DIGITIZER_ENABLE) && !defined(DIGITIZER_SHARED_EP)
#define DIGITIZER_EPIN_ADDR  (ENDPOINT_DIR_IN | DIGITIZER_IN_EPNUM)
#endif
/* A combination of interfaces must have a unique product id, since PC will save device driver after the first plug.
 * Same VID/PID with different interface e.g MSC (first), then CDC (later) will possibly cause system error on PC.
 *
 * Auto ProductID layout's Bitmap:
 *   [MSB]         HID | MSC | CDC          [LSB]
 */
#define _PID_MAP(itf, n)  ( (CFG_TUD_##itf) << (n) )
#define USB_PID           (0x4000 | _PID_MAP(CDC, 0) | _PID_MAP(MSC, 1) | _PID_MAP(HID, 2) | \
                           _PID_MAP(MIDI, 3) | _PID_MAP(VENDOR, 4) )

#define USB_VID   0xCafe
#define USB_BCD   0x0200

//--------------------------------------------------------------------+
// Device Descriptors
//--------------------------------------------------------------------+
tusb_desc_device_t const desc_device =
{
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = USB_BCD,
    .bDeviceClass       = 0x00,
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,

    .idVendor           = USB_VID,
    .idProduct          = USB_PID,
    .bcdDevice          = 0x0100,

    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,

    .bNumConfigurations = 0x01
};


#if TUD_OPT_HIGH_SPEED
// Per USB specs: high speed capable device must report device_qualifier and other_speed_configuration

// other speed configuration
uint8_t desc_other_speed_config[CONFIG_TOTAL_LEN];

// device qualifier is mostly similar to device descriptor since we don't change configuration based on speed
tusb_desc_device_qualifier_t const desc_device_qualifier =
{
  .bLength            = sizeof(tusb_desc_device_qualifier_t),
  .bDescriptorType    = TUSB_DESC_DEVICE_QUALIFIER,
  .bcdUSB             = USB_BCD,

  .bDeviceClass       = 0x00,
  .bDeviceSubClass    = 0x00,
  .bDeviceProtocol    = 0x00,

  .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
  .bNumConfigurations = 0x01,
  .bReserved          = 0x00
};

// Invoked when received GET DEVICE QUALIFIER DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete.
// device_qualifier descriptor describes information about a high-speed capable device that would
// change if the device were operating at the other speed. If not highspeed capable stall this request.
uint8_t const* tud_descriptor_device_qualifier_cb(void)
{
  return (uint8_t const*) &desc_device_qualifier;
}

// Invoked when received GET OTHER SEED CONFIGURATION DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete
// Configuration descriptor in the other speed e.g if high speed then this is for full speed and vice versa
uint8_t const* tud_descriptor_other_speed_configuration_cb(uint8_t index)
{
  (void) index; // for multiple configurations

  // other speed config is basically configuration with type = OHER_SPEED_CONFIG
  memcpy(desc_other_speed_config, desc_configuration, CONFIG_TOTAL_LEN);
  desc_other_speed_config[1] = TUSB_DESC_OTHER_SPEED_CONFIG;

  // this example use the same configuration for both high and full speed mode
  return desc_other_speed_config;
}

#endif // highspeed

// Invoked when received GET CONFIGURATION DESCRIPTOR
// Application return pointer to descriptor
// Descriptor contents must exist long enough for transfer to complete
uint8_t const * tud_descriptor_configuration_cb(uint8_t index)
{
  (void) index; // for multiple configurations

  // This example use the same configuration for both high and full speed mode
  return desc_configuration;
}

//--------------------------------------------------------------------+
// String Descriptors
//--------------------------------------------------------------------+

// String Descriptor Index
enum {
  STRID_LANGID = 0,
  STRID_MANUFACTURER,
  STRID_PRODUCT,
  STRID_SERIAL,
};

// array of pointer to string descriptors
char const *string_desc_arr[] =
{
  (const char[]) { 0x09, 0x04 }, // 0: is supported language is English (0x0409)
  "TinyUSB",                     // 1: Manufacturer
  "TinyUSB Device",              // 2: Product
  NULL,                          // 3: Serials will use unique ID if possible
};

static uint16_t _desc_str[32 + 1];

// Invoked when received GET STRING DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete
uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
  (void) langid;
  size_t chr_count;

  switch ( index ) {
    case STRID_LANGID:
      memcpy(&_desc_str[1], string_desc_arr[0], 2);
      chr_count = 1;
      break;

    case STRID_SERIAL:
      chr_count = board_usb_get_serial(_desc_str + 1, 32);
      break;

    default:
      // Note: the 0xEE index string is a Microsoft OS 1.0 Descriptors.
      // https://docs.microsoft.com/en-us/windows-hardware/drivers/usbcon/microsoft-defined-usb-descriptors

      if ( !(index < sizeof(string_desc_arr) / sizeof(string_desc_arr[0])) ) return NULL;

      const char *str = string_desc_arr[index];

      // Cap at max char
      chr_count = strlen(str);
      size_t const max_count = sizeof(_desc_str) / sizeof(_desc_str[0]) - 1; // -1 for string type
      if ( chr_count > max_count ) chr_count = max_count;

      // Convert ASCII string into UTF-16
      for ( size_t i = 0; i < chr_count; i++ ) {
        _desc_str[1 + i] = str[i];
      }
      break;
  }

  // first byte is length (including header), second byte is string type
  _desc_str[0] = (uint16_t) ((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));

  return _desc_str;
}

/*
 * HID report descriptors
 */
#ifdef KEYBOARD_SHARED_EP
static const uint8_t shared_report[] = {
#    define SHARED_REPORT_STARTED
#else
static const uint8_t keyboard_report[] = {
#endif
#ifndef KEYBOARD_SHARED_EP
        TUD_HID_REPORT_DESC_KEYBOARD(),
#else
        TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(REPORT_ID_KEYBOARD)),
#endif
#ifndef KEYBOARD_SHARED_EP
};
#endif

#ifdef MOUSE_ENABLE
#    ifndef MOUSE_SHARED_EP
static const uint8_t mouse_report[] = {
#    elif !defined(SHARED_REPORT_STARTED)
static const uint8_t shared_report[] = {
#        define SHARED_REPORT_STARTED
#    endif
    HID_USAGE_PAGE ( HID_USAGE_PAGE_DESKTOP      ),
    HID_USAGE      ( HID_USAGE_DESKTOP_MOUSE     ),
    HID_COLLECTION ( HID_COLLECTION_APPLICATION  ),
#    ifdef MOUSE_SHARED_EP
        HID_REPORT_ID(REPORT_ID_MOUSE),
#    endif
    HID_USAGE      ( HID_USAGE_DESKTOP_POINTER )                   ,\
    HID_COLLECTION ( HID_COLLECTION_PHYSICAL   )                   ,\

      HID_USAGE_PAGE  ( HID_USAGE_PAGE_BUTTON  )                   ,\
        HID_USAGE_MIN   ( 1                                      ) ,\
        HID_USAGE_MAX   ( 5                                      ) ,\
        HID_LOGICAL_MIN ( 0                                      ) ,\
        HID_LOGICAL_MAX ( 1                                      ) ,\
        /* Left, Right, Middle, Backward, Forward buttons */ \
        HID_REPORT_COUNT( 5                                      ) ,\
        HID_REPORT_SIZE ( 1                                      ) ,\
        HID_INPUT       ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ) ,\

#    ifdef MOUSE_EXTENDED_REPORT
            // Boot protocol XY ignored in Report protocol
            HID_REPORT_COUNT(0x02),
            HID_REPORT_SIZE(0x08),
            HID_INPUT(HID_CONSTANT),
#    endif
            // X/Y position (2 or 4 bytes)
            HID_USAGE_PAGE(0x01),    // Generic Desktop
            HID_USAGE(0x30),         // X
            HID_USAGE(0x31),         // Y
#    ifndef MOUSE_EXTENDED_REPORT
            HID_LOGICAL_MIN(-127),
            HID_LOGICAL_MAX(127),
            HID_REPORT_COUNT(0x02),
            HID_REPORT_SIZE(0x08),
#    else
            HID_LOGICAL_MIN(-32767, 2),
            HID_LOGICAL_MAX(32767, 2),
            HID_REPORT_COUNT(0x02),
            HID_REPORT_SIZE(0x10),
#    endif
            HID_INPUT(HID_DATA | HID_VARIABLE | HID_RELATIVE),

#    ifdef POINTING_DEVICE_HIRES_SCROLL_ENABLE
            HID_COLLECTION(0x02),
            // Feature report and padding (1 byte)
            HID_USAGE(0x48),     // Resolution Multiplier
            HID_REPORT_COUNT(0x01),
            HID_REPORT_SIZE(0x02),
            HID_LOGICAL_MIN(0x00),
            HID_LOGICAL_MAX(0x01),
            HID_PHYSICAL_MINIMUM(1),
            HID_PHYSICAL_MAXIMUM(POINTING_DEVICE_HIRES_SCROLL_MULTIPLIER),
            HID_UNIT_EXPONENT(POINTING_DEVICE_HIRES_SCROLL_EXPONENT),
            HID_FEATURE(HID_DATA | HID_VARIABLE | HID_ABSOLUTE),
            HID_PHYSICAL_MINIMUM(0x00),
            HID_PHYSICAL_MAXIMUM(0x00),
            HID_REPORT_SIZE(0x06),
            HID_FEATURE(HID_CONSTANT),
#    endif

            // Vertical wheel (1 or 2 bytes)
            HID_USAGE(0x38),     // Wheel
#    ifndef WHEEL_EXTENDED_REPORT
            HID_LOGICAL_MIN(-127),
            HID_LOGICAL_MAX(127),
            HID_REPORT_COUNT(0x01),
            HID_REPORT_SIZE(0x08),
#    else
            HID_LOGICAL_MIN(-32767, 2),
            HID_LOGICAL_MAX(32767, 2),
            HID_REPORT_COUNT(0x01),
            HID_REPORT_SIZE(0x10),
#    endif
            HID_INPUT(HID_DATA | HID_VARIABLE | HID_RELATIVE),
            // Horizontal wheel (1 or 2 bytes)
            HID_USAGE_PAGE(0x0C),// Consumer
            HID_USAGE(0x0238, 2),  // AC Pan
#    ifndef WHEEL_EXTENDED_REPORT
            HID_LOGICAL_MIN(-127),
            HID_LOGICAL_MAX(127),
            HID_REPORT_COUNT(0x01),
            HID_REPORT_SIZE(0x08),
#    else
            HID_LOGICAL_MIN(-32767, 2),
            HID_LOGICAL_MAX(32767, 2),
            HID_REPORT_COUNT(0x01),
            HID_REPORT_SIZE(0x10),
#    endif
            HID_INPUT(HID_DATA | HID_VARIABLE | HID_RELATIVE),

#    ifdef POINTING_DEVICE_HIRES_SCROLL_ENABLE
            HID_COLLECTION_END,
#    endif

        HID_COLLECTION_END,
    HID_COLLECTION_END,
#    ifndef MOUSE_SHARED_EP
};
#    endif
#endif

#ifdef JOYSTICK_ENABLE
#    ifndef JOYSTICK_SHARED_EP
static const uint8_t PROGMEM JoystickReport[] = {
#    elif !defined(SHARED_REPORT_STARTED)
static const uint8_t PROGMEM SharedReport[] = {
#        define SHARED_REPORT_STARTED
#    endif
    HID_USAGE_PAGE(0x01),     // Generic Desktop
    HID_USAGE(0x04),          // Joystick
    HID_COLLECTION(0x01),     // Application
#    ifdef JOYSTICK_SHARED_EP
        HID_REPORT_ID(REPORT_ID_JOYSTICK),
#    endif
        HID_COLLECTION(0x00), // Physical
#    if JOYSTICK_AXIS_COUNT > 0
            HID_USAGE_PAGE(0x01), // Generic Desktop
            HID_USAGE(0x30),      // X
#        if JOYSTICK_AXIS_COUNT > 1
            HID_USAGE(0x31),      // Y
#        endif
#        if JOYSTICK_AXIS_COUNT > 2
            HID_USAGE(0x32),      // Z
#        endif
#        if JOYSTICK_AXIS_COUNT > 3
            HID_USAGE(0x33),      // Rx
#        endif
#        if JOYSTICK_AXIS_COUNT > 4
            HID_USAGE(0x34),      // Ry
#        endif
#        if JOYSTICK_AXIS_COUNT > 5
            HID_USAGE(0x35),      // Rz
#        endif
#        if JOYSTICK_AXIS_RESOLUTION == 8
            HID_LOGICAL_MIN(-JOYSTICK_MAX_VALUE),
            HID_LOGICAL_MAX(JOYSTICK_MAX_VALUE),
            HID_REPORT_COUNT(JOYSTICK_AXIS_COUNT),
            HID_REPORT_SIZE(0x08),
#        else
            HID_LOGICAL_MIN(-JOYSTICK_MAX_VALUE, 2),
            HID_LOGICAL_MAX(JOYSTICK_MAX_VALUE, 2),
            HID_REPORT_COUNT(JOYSTICK_AXIS_COUNT),
            HID_REPORT_SIZE(0x10),
#        endif
            HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE),
#    endif

#    ifdef JOYSTICK_HAS_HAT
            // Hat Switch (4 bits)
            HID_USAGE(0x39), // Hat Switch
            HID_LOGICAL_MIN(0x00),
            HID_LOGICAL_MAX(0x07),
            HID_PHYSICAL_MINIMUM(0),
            HID_PHYSICAL_MAXIMUM(315, 2),
            HID_UNIT(0x14),  // Degree, English Rotation
            HID_REPORT_COUNT(1),
            HID_REPORT_SIZE(4),
            HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE | HID_NULLSTATE),
            // Padding (4 bits)
            HID_REPORT_COUNT(0x04),
            HID_REPORT_SIZE(0x01),
            HID_INPUT(HID_CONSTANT),
#    endif

#    if JOYSTICK_BUTTON_COUNT > 0
            HID_USAGE_PAGE(0x09), // Button
            HID_USAGE_MIN(0x01),
            HID_USAGE_MAX(JOYSTICK_BUTTON_COUNT),
            HID_LOGICAL_MIN(0x00),
            HID_LOGICAL_MAX(0x01),
            HID_REPORT_COUNT(JOYSTICK_BUTTON_COUNT),
            HID_REPORT_SIZE(0x01),
            HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE),

#        if (JOYSTICK_BUTTON_COUNT % 8) != 0
            HID_REPORT_COUNT(8 - (JOYSTICK_BUTTON_COUNT % 8)),
            HID_REPORT_SIZE(0x01),
            HID_INPUT(HID_CONSTANT),
#        endif
#    endif
        HID_COLLECTION_END,
    HID_COLLECTION_END,
#    ifndef JOYSTICK_SHARED_EP
};
#    endif
#endif

#ifdef DIGITIZER_ENABLE
#    ifndef DIGITIZER_SHARED_EP
static const uint8_t PROGMEM DigitizerReport[] = {
#    elif !defined(SHARED_REPORT_STARTED)
static const uint8_t PROGMEM SharedReport[] = {
#        define SHARED_REPORT_STARTED
#    endif
    HID_USAGE_PAGE(0x0D),            // Digitizers
    HID_USAGE(0x01),                 // Digitizer
    HID_COLLECTION(0x01),            // Application
#    ifdef DIGITIZER_SHARED_EP
        HID_REPORT_ID(REPORT_ID_DIGITIZER),
#    endif
        HID_USAGE(0x20),             // Stylus
        HID_COLLECTION(0x00),        // Physical
            // In Range, Tip Switch & Barrel Switch (3 bits)
            HID_USAGE(0x32),         // In Range
            HID_USAGE(0x42),         // Tip Switch
            HID_USAGE(0x44),         // Barrel Switch
            HID_LOGICAL_MIN(0x00),
            HID_LOGICAL_MAX(0x01),
            HID_REPORT_COUNT(0x03),
            HID_REPORT_SIZE(0x01),
            HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE),
            // Padding (5 bits)
            HID_REPORT_COUNT(0x05),
            HID_INPUT(HID_CONSTANT),

            // X/Y Position (4 bytes)
            HID_USAGE_PAGE(0x01),    // Generic Desktop
            HID_USAGE(0x30),         // X
            HID_USAGE(0x31),         // Y
            HID_LOGICAL_MAX(0x7FFF, 2),
            HID_REPORT_COUNT(0x02),
            HID_REPORT_SIZE(0x10),
            HID_UNIT(0x13),          // Inch, English Linear
            HID_UNIT_EXPONENT(0x0E), // -2
            HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE),
        HID_COLLECTION_END,
    HID_COLLECTION_END,
#    ifndef DIGITIZER_SHARED_EP
};
#    endif
#endif

#if defined(SHARED_EP_ENABLE) && !defined(SHARED_REPORT_STARTED)
static const uint8_t PROGMEM shared_report[] = {
#endif

#ifdef EXTRAKEY_ENABLE
    HID_USAGE_PAGE ( HID_USAGE_PAGE_DESKTOP           ),
    HID_USAGE      ( HID_USAGE_DESKTOP_SYSTEM_CONTROL ),
    HID_COLLECTION ( HID_COLLECTION_APPLICATION       ),
        HID_REPORT_ID(REPORT_ID_SYSTEM),
        HID_USAGE_MIN(0x01),    // Pointer
        HID_USAGE_MAX_N(0x00B7,2), // System Display LCD Autoscale
        HID_LOGICAL_MIN(0x01),
        HID_LOGICAL_MAX_N(0x00B7,2),
        HID_REPORT_COUNT(1),
        HID_REPORT_SIZE(16),
        HID_INPUT(HID_DATA | HID_ARRAY | HID_ABSOLUTE),
    HID_COLLECTION_END,
    TUD_HID_REPORT_DESC_CONSUMER(HID_REPORT_ID(REPORT_ID_CONSUMER)),
#endif

#ifdef PROGRAMMABLE_BUTTON_ENABLE
    HID_USAGE_PAGE(0x0C),            // Consumer
    HID_USAGE(0x01),                 // Consumer Control
    HID_COLLECTION(0x01),            // Application
        HID_REPORT_ID(REPORT_ID_PROGRAMMABLE_BUTTON),
        HID_USAGE(0x03),             // Programmable Buttons
        HID_COLLECTION(0x04),        // Named Array
            HID_USAGE_PAGE(0x09),    // Button
            HID_USAGE_MIN(0x01), // Button 1
            HID_USAGE_MAX(0x20), // Button 32
            HID_LOGICAL_MIN(0x00),
            HID_LOGICAL_MAX(0x01),
            HID_REPORT_COUNT(32),
            HID_REPORT_SIZE(1),
            HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE),
        HID_COLLECTION_END,
    HID_COLLECTION_END,
#endif

#ifdef NKRO_ENABLE
    HID_USAGE_PAGE(0x01),        // Generic Desktop
    HID_USAGE(0x06),             // Keyboard
    HID_COLLECTION(0x01),        // Application
        HID_REPORT_ID(REPORT_ID_NKRO),
        // Modifiers (8 bits)
        HID_USAGE_PAGE(0x07),    // Keyboard/Keypad
        HID_USAGE_MIN(0xE0), // Keyboard Left Control
        HID_USAGE_MAX(0xE7), // Keyboard Right GUI
        HID_LOGICAL_MIN(0x00),
        HID_LOGICAL_MAX(0x01),
        HID_REPORT_COUNT(0x08),
        HID_REPORT_SIZE(0x01),
        HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE),
        // Keycodes
        HID_USAGE_PAGE(0x07),    // Keyboard/Keypad
        HID_USAGE_MIN(0x00),
        HID_USAGE_MAX(NKRO_REPORT_BITS * 8 - 1),
        HID_LOGICAL_MIN(0x00),
        HID_LOGICAL_MAX(0x01),
        HID_REPORT_COUNT(NKRO_REPORT_BITS * 8),
        HID_REPORT_SIZE(0x01),
        HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE),

        // Status LEDs (5 bits)
        HID_USAGE_PAGE(0x08),    // LED
        HID_USAGE_MIN(0x01), // Num Lock
        HID_USAGE_MAX(0x05), // Kana
        HID_REPORT_COUNT(0x05),
        HID_REPORT_SIZE(0x01),
        HID_OUTPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE | HID_NON_VOLATILE),
        // LED padding (3 bits)
        HID_REPORT_COUNT(0x01),
        HID_REPORT_SIZE(0x03),
        HID_OUTPUT(HID_CONSTANT),
    HID_COLLECTION_END,
#endif


#ifdef LIGHTING_ENABLE
    TUD_HID_REPORT_DESC_LIGHTING( REPORT_ID_LIGHTING_LAMP_ARRAY_ATTRIBUTES )
#endif

#ifdef SHARED_EP_ENABLE
};
#endif

#ifdef RAW_ENABLE
static const uint8_t raw_report[] = {
    TUD_HID_REPORT_DESC_GENERIC_INOUT(RAW_EPSIZE),
};
#endif

#ifdef CONSOLE_ENABLE
static const uint8_t console_report[] = {
    HID_USAGE_PAGE_N(0xFF31, 2), // Vendor Defined (PJRC Teensy compatible)
    HID_USAGE(0x74),         // Vendor Defined (PJRC Teensy compatible)
    HID_COLLECTION(0x01),    // Application
        // Data to host
        HID_USAGE(0x75),     // Vendor Defined
        HID_LOGICAL_MIN(0x00),
        HID_LOGICAL_MAX(16, 0x00FF),
        HID_REPORT_COUNT(CONSOLE_EPSIZE),
        HID_REPORT_SIZE(0x08),
        HID_INPUT(HID_DATA | HID_VARIABLE | HID_ABSOLUTE),
    HID_COLLECTION_END,
};
#endif

/*
 * Device descriptor
 */
tusb_desc_device_t const device_desc =
{
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
#if VIRTSER_ENABLE
    .bDeviceClass       = TUSB_CLASS_MISC,
    .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol    = MISC_PROTOCOL_IAD,
#else
    .bDeviceClass       = 0,
    .bDeviceSubClass    = 0,
    .bDeviceProtocol    = 0,
#endif
    .bMaxPacketSize0    = FIXED_CONTROL_ENDPOINT_SIZE,

    .idVendor           = VENDOR_ID,
    .idProduct          = PRODUCT_ID,
    .bcdDevice          = DEVICE_VER,

    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
#ifdef HAS_SERIAL_NUMBER
    .iSerialNumber          = 0x03,
#else // HAS_SERIAL_NUMBER
    .iSerialNumber          = 0x00,
#endif // HAS_SERIAL_NUMBER
    .bNumConfigurations = 0x01
};

#ifndef USB_MAX_POWER_CONSUMPTION
#    define USB_MAX_POWER_CONSUMPTION 500
#endif

#ifndef USB_POLLING_INTERVAL_MS
#    define USB_POLLING_INTERVAL_MS 1
#endif


#define  CONFIG_TOTAL_LEN  (TUD_CONFIG_DESC_LEN + TUD_HID_INOUT_DESC_LEN)
/*
 * Configuration descriptors
 */
static const uint8_t config_desc[] = {
{
  // Config number, interface count, string index, total length, attribute, power in mA
  TUD_CONFIG_DESCRIPTOR(1, TOTAL_INTERFACES, 0, CONFIG_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, USB_MAX_POWER_CONSUMPTION),


#ifndef KEYBOARD_SHARED_EP
  // Interface number, string index, protocol, report descriptor len, EP In address, size & polling interval
  TUD_HID_DESCRIPTOR(KEYBOARD_INTERFACE, 0, HID_ITF_PROTOCOL_KEYBOARD, sizeof(keyboard_report), KEYBOARD_EPIN_ADDR, KEYBOARD_EPSIZE, USB_POLLING_INTERVAL_MS),
#endif

#ifdef RAW_ENABLE
  // Interface number, string index, protocol, report descriptor len, EP Out & In address, size & polling interval
  TUD_HID_INOUT_DESCRIPTOR(RAW_INTERFACE, 0, HID_ITF_PROTOCOL_NONE, sizeof(raw_report), RAW_EPOUT_ADDR, RAW_EPIN_ADDR, RAW_EPSIZE, USB_POLLING_INTERVAL_MS),
#endif

#if defined(MOUSE_ENABLE) && !defined(MOUSE_SHARED_EP)
  TUD_HID_DESCRIPTOR(MOUSE_INTERFACE, 0, HID_ITF_PROTOCOL_MOUSE, sizeof(mouse_report), MOUSE_EPIN_ADDR, MOUSE_EPSIZE, USB_POLLING_INTERVAL_MS),
#endif

#ifdef SHARED_EP_ENABLE
#ifdef KEYBOARD_SHARED_EP
  TUD_HID_DESCRIPTOR(SHARED_INTERFACE, 0, HID_ITF_PROTOCOL_KEYBOARD, sizeof(shared_report), SHARED_EPIN_ADDR, SHARED_EPSIZE, USB_POLLING_INTERVAL_MS),
#else
  TUD_HID_DESCRIPTOR(SHARED_INTERFACE, 0, HID_ITF_PROTOCOL_NONE, sizeof(shared_report), SHARED_EPIN_ADDR, SHARED_EPSIZE, USB_POLLING_INTERVAL_MS),
#endif
#endif

#ifdef CONSOLE_ENABLE
  TUD_HID_DESCRIPTOR(CONSOLE_INTERFACE, 0, HID_ITF_PROTOCOL_NONE, sizeof(console_report), CONSOLE_EPIN_ADDR, CONSOLE_EPSIZE, USB_POLLING_INTERVAL_MS),
#endif


#ifdef MIDI_ENABLE
  8, TUSB_DESC_CONFIGURATION, AC_INTERFACE, 2, TUSB_CLASS_AUDIO, AUDIO_SUBCLASS_CONTROL, AUDIO_FUNC_PROTOCOL_CODE_UNDEF, 0,
  TUD_MIDI_DESCRIPTOR(AC_INTERFACE, 0, MIDI_EPIN_ADDR, MIDI_EPOUT_ADDR, MIDI_STREAM_EPSIZE),
#endif


#if defined(JOYSTICK_ENABLE) && !defined(JOYSTICK_SHARED_EP)
  TUD_HID_DESCRIPTOR(JOYSTICK_INTERFACE, 0, HID_ITF_PROTOCOL_NONE, sizeof(joystick_report), JOYSTICK_EPIN_ADDR, JOYSTICK_EPSIZE, USB_POLLING_INTERVAL_MS),
#endif

#if defined(DIGITIZER_ENABLE) && !defined(DIGITIZER_SHARED_EP)
  TUD_HID_DESCRIPTOR(DIGITIZER_INTERFACE, 0, HID_ITF_PROTOCOL_NONE, sizeof(digitizer_report), DIGITIZER_EPIN_ADDR, DIGITIZER_EPSIZE, USB_POLLING_INTERVAL_MS),
#endif
};


static const USB_Descriptor_Configuration_t config_desc = {

#ifdef VIRTSER_ENABLE
    /*
     * Virtual Serial
     */
    .CDC_Interface_Association = {
        .Header = {
            .Size               = sizeof(USB_Descriptor_Interface_Association_t),
            .Type               = DTYPE_InterfaceAssociation
        },
        .FirstInterfaceIndex    = CCI_INTERFACE,
        .TotalInterfaces        = 2,
        .Class                  = CDC_CSCP_CDCClass,
        .SubClass               = CDC_CSCP_ACMSubclass,
        .Protocol               = CDC_CSCP_ATCommandProtocol,
        .IADStrIndex            = NO_DESCRIPTOR,
    },
    .CDC_CCI_Interface = {
        .Header = {
            .Size               = sizeof(USB_Descriptor_Interface_t),
            .Type               = DTYPE_Interface
        },
        .InterfaceNumber        = CCI_INTERFACE,
        .AlternateSetting       = 0,
        .TotalEndpoints         = 1,
        .Class                  = CDC_CSCP_CDCClass,
        .SubClass               = CDC_CSCP_ACMSubclass,
        .Protocol               = CDC_CSCP_ATCommandProtocol,
        .InterfaceStrIndex      = NO_DESCRIPTOR
    },
    .CDC_Functional_Header = {
        .Header = {
            .Size               = sizeof(USB_CDC_Descriptor_FunctionalHeader_t),
            .Type               = CDC_DTYPE_CSInterface
        },
        .Subtype                = 0x00,
        .CDCSpecification       = VERSION_BCD(1, 1, 0),
    },
    .CDC_Functional_ACM = {
        .Header = {
            .Size               = sizeof(USB_CDC_Descriptor_FunctionalACM_t),
            .Type               = CDC_DTYPE_CSInterface
        },
        .Subtype                = 0x02,
        .Capabilities           = 0x02,
    },
    .CDC_Functional_Union = {
        .Header = {
            .Size               = sizeof(USB_CDC_Descriptor_FunctionalUnion_t),
            .Type               = CDC_DTYPE_CSInterface
        },
        .Subtype                = 0x06,
        .MasterInterfaceNumber  = CCI_INTERFACE,
        .SlaveInterfaceNumber   = CDI_INTERFACE,
    },
    .CDC_NotificationEndpoint = {
        .Header = {
            .Size               = sizeof(USB_Descriptor_Endpoint_t),
            .Type               = DTYPE_Endpoint
        },
        .EndpointAddress        = (ENDPOINT_DIR_IN | CDC_NOTIFICATION_EPNUM),
        .Attributes             = (EP_TYPE_INTERRUPT | ENDPOINT_ATTR_NO_SYNC | ENDPOINT_USAGE_DATA),
        .EndpointSize           = CDC_NOTIFICATION_EPSIZE,
        .PollingIntervalMS      = 0xFF
    },
    .CDC_DCI_Interface = {
        .Header = {
            .Size               = sizeof(USB_Descriptor_Interface_t),
            .Type               = DTYPE_Interface
        },
        .InterfaceNumber        = CDI_INTERFACE,
        .AlternateSetting       = 0,
        .TotalEndpoints         = 2,
        .Class                  = CDC_CSCP_CDCDataClass,
        .SubClass               = CDC_CSCP_NoDataSubclass,
        .Protocol               = CDC_CSCP_NoDataProtocol,
        .InterfaceStrIndex      = NO_DESCRIPTOR
    },
    .CDC_DataOutEndpoint = {
        .Header = {
            .Size               = sizeof(USB_Descriptor_Endpoint_t),
            .Type               = DTYPE_Endpoint
        },
        .EndpointAddress        = (ENDPOINT_DIR_OUT | CDC_OUT_EPNUM),
        .Attributes             = (EP_TYPE_BULK | ENDPOINT_ATTR_NO_SYNC | ENDPOINT_USAGE_DATA),
        .EndpointSize           = CDC_EPSIZE,
        .PollingIntervalMS      = 0x05
    },
    .CDC_DataInEndpoint = {
        .Header = {
            .Size               = sizeof(USB_Descriptor_Endpoint_t),
            .Type               = DTYPE_Endpoint
        },
        .EndpointAddress        = (ENDPOINT_DIR_IN | CDC_IN_EPNUM),
        .Attributes             = (EP_TYPE_BULK | ENDPOINT_ATTR_NO_SYNC | ENDPOINT_USAGE_DATA),
        .EndpointSize           = CDC_EPSIZE,
        .PollingIntervalMS      = 0x05
    },
#endif

};

/*
 * String descriptors
 */

#define USB_DESCRIPTOR_SIZE_LITERAL_U16STRING(str) \
    (sizeof(USB_Descriptor_Header_t) + sizeof(str) - sizeof(wchar_t)) // include header, don't count null terminator

static const USB_Descriptor_String_t PROGMEM LanguageString = {
    .Header = {
        .Size                   = sizeof(USB_Descriptor_Header_t) + sizeof(uint16_t),
        .Type                   = DTYPE_String
    },
    .UnicodeString              = {LANGUAGE_ID_ENG}
};

static const USB_Descriptor_String_t PROGMEM ManufacturerString = {
    .Header = {
        .Size                   = USB_DESCRIPTOR_SIZE_LITERAL_U16STRING(USBSTR(MANUFACTURER)),
        .Type                   = DTYPE_String
    },
    .UnicodeString              = USBSTR(MANUFACTURER)
};

static const USB_Descriptor_String_t PROGMEM ProductString = {
    .Header = {
        .Size                   = USB_DESCRIPTOR_SIZE_LITERAL_U16STRING(USBSTR(PRODUCT)),
        .Type                   = DTYPE_String
    },
    .UnicodeString              = USBSTR(PRODUCT)
};

// clang-format on

#if defined(SERIAL_NUMBER)
// clang-format off
static const USB_Descriptor_String_t PROGMEM SerialNumberString = {
    .Header = {
        .Size                   = USB_DESCRIPTOR_SIZE_LITERAL_U16STRING(USBSTR(SERIAL_NUMBER)),
        .Type                   = DTYPE_String
    },
    .UnicodeString              = USBSTR(SERIAL_NUMBER)
};
// clang-format on

#else // defined(SERIAL_NUMBER)

#    if defined(SERIAL_NUMBER_USE_HARDWARE_ID) && SERIAL_NUMBER_USE_HARDWARE_ID == TRUE

#        if defined(__AVR__)
#            error Dynamically setting the serial number on AVR is unsupported as LUFA requires the string to be in PROGMEM.
#        endif // defined(__AVR__)

#        ifndef SERIAL_NUMBER_LENGTH
#            define SERIAL_NUMBER_LENGTH (sizeof(hardware_id_t) * 2)
#        endif

#        define SERIAL_NUMBER_DESCRIPTOR_SIZE                                            \
            (sizeof(USB_Descriptor_Header_t)                     /* Descriptor header */ \
             + (((SERIAL_NUMBER_LENGTH) + 1) * sizeof(wchar_t))) /* Length of serial number, with potential extra character as we're converting 2 nibbles at a time */

uint8_t SerialNumberString[SERIAL_NUMBER_DESCRIPTOR_SIZE] = {0};

void set_serial_number_descriptor(void) {
    static bool is_set = false;
    if (is_set) {
        return;
    }
    is_set = true;

    static const char        hex_str[] = "0123456789ABCDEF";
    hardware_id_t            id        = get_hardware_id();
    USB_Descriptor_String_t* desc      = (USB_Descriptor_String_t*)SerialNumberString;

    // Copy across nibbles from the hardware ID as unicode hex characters
    int      length = MIN(sizeof(id) * 2, SERIAL_NUMBER_LENGTH);
    uint8_t* p      = (uint8_t*)&id;
    for (int i = 0; i < length; i += 2) {
        desc->UnicodeString[i + 0] = hex_str[p[i / 2] >> 4];
        desc->UnicodeString[i + 1] = hex_str[p[i / 2] & 0xF];
    }

    desc->Header.Size = sizeof(USB_Descriptor_Header_t) + (length * sizeof(wchar_t)); // includes header, don't count null terminator
    desc->Header.Type = DTYPE_String;
}

#    endif // defined(SERIAL_NUMBER_USE_HARDWARE_ID) && SERIAL_NUMBER_USE_HARDWARE_ID == TRUE

#endif // defined(SERIAL_NUMBER)

