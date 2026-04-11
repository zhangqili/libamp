/*
 * Copyright (c) 2024 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "usbd_user.h"
#include "packet.h"
#include "usb_descriptor.h"
#include "lamp_array.h"

#if defined(MTP_ENABLE)
#include "usbd_mtp.h"
#endif

#if defined(MTP_ENABLE) || defined(GAMEPAD_ENABLE)
#define USBD_MSOS_VENDOR_CODE 0x20

static const char msosv1_string_descriptor[] = {
    0x12,                       /* bLength */
    0x03,                       /* bDescriptorType */
    'M', 0, 'S', 0, 'F', 0, 'T', 0, '1', 0, '0', 0, '0', 0, /* qwSignature "MSFT100" */
    USBD_MSOS_VENDOR_CODE,      /* bMS_VendorCode */
    0x00                        /* bPad */
};
#endif

#if defined(GAMEPAD_ENABLE)
//#include "usbd_gamepad.h"
#include "gamepad.h"
#define USB_MSOSV1_COMP_ID_FUNCTION_XINPUT_DESCRIPTOR_INIT(bFirstInterfaceNumber) \
    bFirstInterfaceNumber,                          /* bFirstInterfaceNumber */\
    0x01,                                           /* reserved1 */            \
    'X', 'U', 'S', 'B', '2', '0', 0x00, 0x00,       /* compatibleID[8] */      \
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* subCompatibleID[8] */   \
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00              /* reserved2[6] */

static const uint8_t msosv1_compat_id_descriptor[] = {
    USB_MSOSV1_COMP_ID_HEADER_DESCRIPTOR_INIT(1),
    USB_MSOSV1_COMP_ID_FUNCTION_XINPUT_DESCRIPTOR_INIT(XINPUT_INTERFACE)
};

static struct usb_msosv1_descriptor msosv1_desc = {
    .string = (const uint8_t *)msosv1_string_descriptor,
    .vendor_code = USBD_MSOS_VENDOR_CODE,
    .compat_id = msosv1_compat_id_descriptor,
    .comp_id_property = NULL
};
#endif


static const uint8_t *device_descriptor_callback(uint8_t speed)
{
    UNUSED(speed);

    return (const uint8_t *)&DeviceDescriptor;
}

static const uint8_t *config_descriptor_callback(uint8_t speed)
{
    UNUSED(speed);

    return (const uint8_t *)&ConfigurationDescriptor;
}

static const uint8_t *device_quality_descriptor_callback(uint8_t speed)
{
    UNUSED(speed);

#ifdef CONFIG_USB_HS
    ///////////////////////////////////////
    /// device qualifier descriptor
    ///////////////////////////////////////
    static const uint8_t device_quality_descriptor[] = {0x0a,
        USB_DESCRIPTOR_TYPE_DEVICE_QUALIFIER,
        0x00,
        0x02,
        0x00,
        0x00,
        0x00,
        0x40,
        0x01,
        0x00};
    return device_quality_descriptor; 
#else
    return NULL;
#endif
}

static const uint8_t *other_speed_config_descriptor_callback(uint8_t speed)
{
    UNUSED(speed);

    return NULL;
}

static const char *string_descriptors[] = {
    (const char[]){ 0x09, 0x04 }, /* Langid */
    MANUFACTURER,                    /* Manufacturer */
    PRODUCT,                        /* Product */
    SERIAL_NUMBER,                 /* Serial Number */
#if defined(MTP_ENABLE)
    "MTP Interface",                 /* MTP Interface String */
#endif
};

static const char *string_descriptor_callback(uint8_t speed, uint8_t index)
{
    (void)speed;
#if defined(MTP_ENABLE) || defined(GAMEPAD_ENABLE)
    if (index == 0xEE) {
        return msosv1_string_descriptor;
    }
#endif
    if (index >= (sizeof(string_descriptors) / sizeof(char *))) {
        return NULL;
    }
    return string_descriptors[index];
}

const struct usb_descriptor usb_descriptor = {
    .device_descriptor_callback = device_descriptor_callback,
    .config_descriptor_callback = config_descriptor_callback,
    .device_quality_descriptor_callback = device_quality_descriptor_callback,
    .other_speed_descriptor_callback = other_speed_config_descriptor_callback,
    .string_descriptor_callback = string_descriptor_callback,
#if defined(GAMEPAD_ENABLE)
    .msosv1_descriptor = &msosv1_desc,
#endif
};

void usbd_hid_get_report(uint8_t busid, uint8_t intf, uint8_t report_id, uint8_t report_type, uint8_t **data, uint32_t *len)
{
    UNUSED(busid);
    UNUSED(intf);
    UNUSED(report_id);
    UNUSED(report_type);
    UNUSED(data);
    UNUSED(len);
    switch (intf)
    {
#ifdef LIGHTING_ENABLE
    case SHARED_INTERFACE:
        switch (report_id)
        {
        case REPORT_ID_LIGHTING_LAMP_ARRAY_ATTRIBUTES:
            {
                *len = lamp_array_get_lamp_array_attributes_report(*data);
            }
            break;
        case REPORT_ID_LIGHTING_LAMP_ATTRIBUTES_RESPONSE:
            {
                *len = lamp_array_get_lamp_attributes_report(*data);
            }
            break;
        default:
            (*data[0]) = 0;
            *len = 1;
            break;
        }
        break;
#endif
    default:
        (*data[0]) = 0;
        *len = 1;
        break;
    }
}

void usbd_hid_set_report(uint8_t busid, uint8_t intf, uint8_t report_id, uint8_t report_type, uint8_t *report, uint32_t report_len)
{
    UNUSED(busid);
    UNUSED(report_type);
    if (intf == KEYBOARD_INTERFACE
#if defined(SHARED_EP_ENABLE) && !defined(KEYBOARD_SHARED_EP)
    || intf == SHARED_INTERFACE
#endif
    )
    {
        if (report_len == 2)
        {
            if (report_id == REPORT_ID_KEYBOARD || report_id == REPORT_ID_NKRO) {
                g_keyboard_led_state.raw = report[1];
            }
        }
        else if (report_len == 1)
        {
            g_keyboard_led_state.raw = report[0];
        }
    }
#ifdef LIGHTING_ENABLE
    if (intf == SHARED_INTERFACE)
    {
        switch (report_id) {
            case REPORT_ID_LIGHTING_LAMP_ATTRIBUTES_REQUEST: {
                lamp_array_set_lamp_attributes_id(report);
                break;
            }
            case REPORT_ID_LIGHTING_LAMP_MULTI_UPDATE: {
                lamp_array_set_multiple_lamps(report);
                break;   
            }
            case REPORT_ID_LIGHTING_LAMP_RANGE_UPDATE: {
                lamp_array_set_lamp_range(report);
                break;   
            }
            case REPORT_ID_LIGHTING_LAMP_ARRAY_CONTROL: {
                lamp_array_set_autonomous_mode(report);
                break;
            }
            default: {
                break;
            }
        }
    }
#endif
}

/* Store example melody as an array of note values */
    
#ifndef KEYBOARD_SHARED_EP
static volatile bool keyboard_state;
USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t keyboard_buffer[KEYBOARD_EPSIZE];

static void usbd_hid_keyboard_in_callback(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    UNUSED(busid);
    UNUSED(ep);
    UNUSED(nbytes);
    keyboard_state = USB_STATE_IDLE;
}

static struct usbd_interface keyboard_intf;
static struct usbd_endpoint keyboard_in_ep = {
    .ep_cb = usbd_hid_keyboard_in_callback,
    .ep_addr = KEYBOARD_EPIN_ADDR};
#endif

#if defined(MOUSE_ENABLE) && !defined(MOUSE_SHARED_EP)
static volatile bool  mouse_state;
USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t mouse_buffer[MOUSE_EPSIZE];

static void usbd_hid_mouse_in_callback(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    UNUSED(busid); UNUSED(ep); UNUSED(nbytes);
    mouse_state = USB_STATE_IDLE;
}

static struct usbd_interface mouse_intf;
static struct usbd_endpoint mouse_in_ep = {
    .ep_cb = usbd_hid_mouse_in_callback,
    .ep_addr = MOUSE_EPIN_ADDR};
#endif

#ifdef SHARED_EP_ENABLE
static volatile bool  shared_state;
USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t shared_buffer[SHARED_EPSIZE];

static void usbd_hid_shared_in_callback(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    UNUSED(busid);
    UNUSED(ep);
    UNUSED(nbytes);
    shared_state = USB_STATE_IDLE;
}

static struct usbd_interface shared_intf;
static struct usbd_endpoint shared_in_ep = {
    .ep_cb = usbd_hid_shared_in_callback,
    .ep_addr = SHARED_EPIN_ADDR};
#endif

#ifdef RAW_ENABLE
static volatile bool  raw_state;
USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t raw_in_buffer[RAW_EPSIZE];
USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t raw_out_buffer[RAW_EPSIZE];

static void usbd_hid_raw_in_callback(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    UNUSED(busid);
    UNUSED(ep);
    UNUSED(nbytes);
    raw_state = USB_STATE_IDLE;
}

static void usbd_hid_raw_out_callback(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    UNUSED(busid);
    UNUSED(ep);
    UNUSED(nbytes);
    usbd_ep_start_read(0, RAW_EPOUT_ADDR, raw_out_buffer, 64);
    packet_process(raw_out_buffer, sizeof(raw_out_buffer));
}

static struct usbd_interface raw_intf;

static struct usbd_endpoint raw_in_ep = {
    .ep_cb = usbd_hid_raw_in_callback,
    .ep_addr = RAW_EPIN_ADDR};
static struct usbd_endpoint raw_out_ep = {
    .ep_cb = usbd_hid_raw_out_callback,
    .ep_addr = RAW_EPOUT_ADDR};
#endif

#ifdef CONSOLE_ENABLE
static volatile bool  console_state;
USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t console_buffer[CONSOLE_EPSIZE];

static void usbd_hid_console_in_callback(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    UNUSED(busid); UNUSED(ep); UNUSED(nbytes);
    console_state = USB_STATE_IDLE;
}

static struct usbd_interface console_intf;
static struct usbd_endpoint console_in_ep = {
    .ep_cb = usbd_hid_console_in_callback,
    .ep_addr = CONSOLE_EPIN_ADDR};
#endif

#ifdef MIDI_ENABLE
static volatile bool  midi_state;
USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t midi_in_buffer[4];
USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t midi_out_buffer[4];

static void usbd_midi_bulk_out(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    (void)busid;
    (void)ep;
    (void)nbytes;
}

static void usbd_midi_bulk_in(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    (void)busid;
    (void)ep;
    (void)nbytes;

    midi_state = USB_STATE_IDLE;
}

static struct usbd_interface midi_in_intf;
static struct usbd_interface midi_out_intf;

static struct usbd_endpoint midi_out_ep = {
    .ep_addr = MIDI_EPOUT_ADDR,
    .ep_cb = usbd_midi_bulk_out
};
static struct usbd_endpoint midi_in_ep = {
    .ep_addr = MIDI_EPIN_ADDR,
    .ep_cb = usbd_midi_bulk_in
};
#endif

#ifdef VIRTSER_ENABLE
#include "usbd_cdc.h"
static struct usbd_interface cdc_cmd_intf;
static struct usbd_interface cdc_data_intf;

static void usbd_cdc_acm_bulk_out(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    UNUSED(busid);
    UNUSED(ep);
    UNUSED(nbytes);
    // TODO
}

static void usbd_cdc_acm_bulk_in(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    UNUSED(busid);
    UNUSED(ep);
    UNUSED(nbytes);
}

static struct usbd_endpoint cdc_out_ep = {
    .ep_addr = ENDPOINT_DIR_OUT | CDC_OUT_EPNUM,
    .ep_cb = usbd_cdc_acm_bulk_out
};
static struct usbd_endpoint cdc_in_ep = {
    .ep_addr = ENDPOINT_DIR_IN | CDC_IN_EPNUM,
    .ep_cb = usbd_cdc_acm_bulk_in
};
#endif

#if defined(JOYSTICK_ENABLE) && !defined(JOYSTICK_SHARED_EP)
static volatile bool  joystick_state;
USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t joystick_buffer[JOYSTICK_EPSIZE];

static void usbd_hid_joystick_in_callback(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    UNUSED(busid);
    UNUSED(ep);
    UNUSED(nbytes);
    joystick_state = USB_STATE_IDLE;
}

static struct usbd_interface joystick_intf;
static struct usbd_endpoint joystick_in_ep = {
    .ep_cb = usbd_hid_joystick_in_callback,
    .ep_addr = JOYSTICK_EPIN_ADDR};
#endif

#if defined(DIGITIZER_ENABLE) && !defined(DIGITIZER_SHARED_EP)
static volatile bool  digitizer_state;
USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t digitizer_buffer[DIGITIZER_EPSIZE];

static void usbd_hid_digitizer_in_callback(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    UNUSED(busid);
    UNUSED(ep);
    UNUSED(nbytes);
    digitizer_state = USB_STATE_IDLE;
}

static struct usbd_interface digitizer_intf;
static struct usbd_endpoint digitizer_in_ep = {
    .ep_cb = usbd_hid_digitizer_in_callback,
    .ep_addr = DIGITIZER_EPIN_ADDR};
#endif


#if defined(MTP_ENABLE)
static struct usbd_interface mtp_intf;
#endif

#ifdef GAMEPAD_ENABLE
static volatile bool xinput_state;
USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t xinput_in_buffer[XINPUT_EPSIZE];
USB_NOCACHE_RAM_SECTION USB_MEM_ALIGNX uint8_t xinput_out_buffer[XINPUT_EPSIZE];

static int xinput_vendor_class_request_handler(uint8_t busid, struct usb_setup_packet *setup, uint8_t **data, uint32_t *len)
{
    Gamepad xinput_report;

    memset(&xinput_report, 0, sizeof(Gamepad));
    xinput_report.report_size = 20;

    memcpy(*data, &xinput_report, sizeof(Gamepad));
    *len = sizeof(Gamepad);
    return 0;
}

static void usbd_xinput_in_callback(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    UNUSED(busid);
    UNUSED(ep);
    UNUSED(nbytes);
    xinput_state = USB_STATE_IDLE;
}

static void usbd_xinput_out_callback(uint8_t busid, uint8_t ep, uint32_t nbytes)
{
    UNUSED(busid);
    UNUSED(ep);
    UNUSED(nbytes);

    usbd_ep_start_read(0, XINPUT_EPOUT_ADDR, xinput_out_buffer, XINPUT_EPSIZE);
    gamepad_out_callback((GamepadOutReport*)xinput_out_buffer);
    // struct xinput_out_report *out_report = (struct xinput_out_report *)xinput_out_buffer;
}

static struct usbd_interface xinput_intf = {
    .vendor_handler = xinput_vendor_class_request_handler,
};
static struct usbd_endpoint xinput_in_ep = {
    .ep_cb = usbd_xinput_in_callback,
    .ep_addr = XINPUT_EPIN_ADDR
};
static struct usbd_endpoint xinput_out_ep = {
    .ep_cb = usbd_xinput_out_callback,
    .ep_addr = XINPUT_EPOUT_ADDR
};
#endif

static void usbd_event_handler(uint8_t busid, uint8_t event)
{
    UNUSED(busid);
    switch (event)
    {
    case USBD_EVENT_RESET:
        keyboard_state = USB_STATE_IDLE;
#ifdef SHARED_EP_ENABLE
        shared_state = USB_STATE_IDLE;
#endif
#ifdef RAW_ENABLE
        raw_state = USB_STATE_IDLE;
#endif
#ifdef MIDI_ENABLE
        midi_state = USB_STATE_IDLE;
#endif
#if defined(MOUSE_ENABLE) && !defined(MOUSE_SHARED_EP)
        mouse_state = USB_STATE_IDLE;
#endif
#ifdef CONSOLE_ENABLE
        console_state = USB_STATE_IDLE;
#endif
#if defined(JOYSTICK_ENABLE) && !defined(JOYSTICK_SHARED_EP)
        joystick_state = USB_STATE_IDLE;
#endif
#if defined(DIGITIZER_ENABLE) && !defined(DIGITIZER_SHARED_EP)
        digitizer_state = USB_STATE_IDLE;
#endif
#ifdef GAMEPAD_ENABLE
        xinput_state = USB_STATE_IDLE;
#endif
        break;
    case USBD_EVENT_CONNECTED:
        break;
    case USBD_EVENT_DISCONNECTED:
        break;
    case USBD_EVENT_RESUME:
        break;
    case USBD_EVENT_SUSPEND:
        g_keyboard_is_suspend = usb_device_is_suspend(0);
        break;
    case USBD_EVENT_CONFIGURED:
        memset(raw_out_buffer, 0, sizeof(raw_out_buffer));
        usbd_ep_start_read(0, RAW_EPOUT_ADDR, raw_out_buffer, RAW_EPSIZE);
#ifdef GAMEPAD_ENABLE
        memset(xinput_out_buffer, 0, sizeof(xinput_out_buffer));
        usbd_ep_start_read(0, XINPUT_EPOUT_ADDR, xinput_out_buffer, XINPUT_EPSIZE);
#endif
        break;
    case USBD_EVENT_SET_REMOTE_WAKEUP:
        break;
    case USBD_EVENT_CLR_REMOTE_WAKEUP:
        break;
    case USBD_EVENT_SOF:
        break;
    default:
        break;
    }
    usbd_event_handler_user(busid, event);
}

void usb_init(uint8_t busid, uintptr_t reg_base)
{
    usbd_desc_register(0, &usb_descriptor);
    
#ifndef KEYBOARD_SHARED_EP
    usbd_add_interface(0, usbd_hid_init_intf(0, &keyboard_intf, KeyboardReport, sizeof(KeyboardReport)));
    usbd_add_endpoint(0, &keyboard_in_ep);
#endif

#if defined(MOUSE_ENABLE) && !defined(MOUSE_SHARED_EP)
    usbd_add_interface(0, usbd_hid_init_intf(0, &mouse_intf, MouseReport, sizeof(MouseReport)));
    usbd_add_endpoint(0, &mouse_in_ep);
#endif

#ifdef RAW_ENABLE
    usbd_add_interface(0, usbd_hid_init_intf(0, &raw_intf, RawReport, sizeof(RawReport)));
    usbd_add_endpoint(0, &raw_in_ep);
    usbd_add_endpoint(0, &raw_out_ep);
#endif

#ifdef SHARED_EP_ENABLE
    usbd_add_interface(0, usbd_hid_init_intf(0, &shared_intf, SharedReport, sizeof(SharedReport)));
    usbd_add_endpoint(0, &shared_in_ep);
#endif

#ifdef CONSOLE_ENABLE
    usbd_add_interface(0, usbd_hid_init_intf(0, &console_intf, ConsoleReport, sizeof(ConsoleReport)));
    usbd_add_endpoint(0, &console_in_ep);
#endif

#ifdef MIDI_ENABLE
    usbd_add_interface(0, &midi_out_intf);
    usbd_add_interface(0, &midi_in_intf);
    usbd_add_endpoint(0, &midi_out_ep);
    usbd_add_endpoint(0, &midi_in_ep);
#endif

#ifdef VIRTSER_ENABLE
    usbd_add_interface(0, usbd_cdc_acm_init_intf(0, &cdc_cmd_intf));
    usbd_add_interface(0, usbd_cdc_acm_init_intf(0, &cdc_data_intf));
    usbd_add_endpoint(0, &cdc_out_ep);
    usbd_add_endpoint(0, &cdc_in_ep);
#endif

#if defined(JOYSTICK_ENABLE) && !defined(JOYSTICK_SHARED_EP)
    usbd_add_interface(0, usbd_hid_init_intf(0, &joystick_intf, JoystickReport, sizeof(JoystickReport)));
    usbd_add_endpoint(0, &joystick_in_ep);
#endif

#if defined(DIGITIZER_ENABLE) && !defined(DIGITIZER_SHARED_EP) 
    usbd_add_interface(0, usbd_hid_init_intf(0, &digitizer_intf, DigitizerReport, sizeof(DigitizerReport)));
    usbd_add_endpoint(0, &digitizer_in_ep);
#endif

#if defined(MTP_ENABLE)
    usbd_add_interface(0, usbd_mtp_init_intf(0, &mtp_intf, (uint8_t*)&ConfigurationDescriptor.MTP_Interface, 
        sizeof(ConfigurationDescriptor.MTP_Interface) + sizeof(ConfigurationDescriptor.MTP_EventEndpoint) + sizeof(ConfigurationDescriptor.MTP_DataOutEndpoint) + sizeof(ConfigurationDescriptor.MTP_DataInEndpoint),
        ConfigurationDescriptor.MTP_DataOutEndpoint.EndpointAddress, 
        ConfigurationDescriptor.MTP_DataInEndpoint.EndpointAddress, 
        ConfigurationDescriptor.MTP_EventEndpoint.EndpointAddress));
#endif
#ifdef GAMEPAD_ENABLE
    usbd_add_interface(0, &xinput_intf);
    usbd_add_endpoint(0, &xinput_in_ep);
    usbd_add_endpoint(0, &xinput_out_ep);
#endif
    usbd_initialize(busid, reg_base, usbd_event_handler);
    /*
    while (!usb_device_is_configured(busid))
    {
        
    }
    */
}

int usb_send_shared_ep(uint8_t *buffer, uint8_t size)
{
#ifdef SHARED_EP_ENABLE
    if (shared_state == USB_STATE_BUSY)
    {
        return 1;
    }
    shared_state = USB_STATE_BUSY;
    memcpy(shared_buffer, buffer, size);
    int ret = usbd_ep_start_write(0, SHARED_EPIN_ADDR, shared_buffer, SHARED_EPSIZE);
    if (ret < 0)
    {
        shared_state = USB_STATE_IDLE;
        return 1;
    }
    return 0;
#else
    UNUSED(buffer); UNUSED(size);
    return 1;
#endif
}

int usb_send_keyboard(uint8_t *buffer, uint8_t size)
{
#ifndef KEYBOARD_SHARED_EP
    UNUSED(size);
    if (keyboard_state == USB_STATE_BUSY)
    {
        return 1;
    }
    keyboard_state = USB_STATE_BUSY;
    memcpy(keyboard_buffer, buffer, KEYBOARD_EPSIZE);
    int ret = usbd_ep_start_write(0, KEYBOARD_EPIN_ADDR, keyboard_buffer, KEYBOARD_EPSIZE);
    if (ret < 0)
    {
        keyboard_state = USB_STATE_IDLE;
        return 1;
    }
    return 0;
#else
    return usb_send_shared_ep(buffer, size);
#endif
}

int usb_send_raw(uint8_t *buffer, uint8_t size)
{
#ifdef RAW_ENABLE
    UNUSED(size);
    if (raw_state == USB_STATE_BUSY)
    {
        return 1;
    }
    raw_state = USB_STATE_BUSY;
    if (size > 0 && size <= 64)
    {
        memset(raw_in_buffer, 0, 64);
        memcpy(raw_in_buffer, buffer, size);
    }
    else
    {
        return 1;
    }
    int ret = usbd_ep_start_write(0, RAW_EPIN_ADDR, raw_in_buffer, 64);
    if (ret < 0)
    {
        raw_state = USB_STATE_IDLE;
        return 1;
    }
    return 0;
#else
    UNUSED(buffer); UNUSED(size); return 1;
#endif
}

int usb_send_midi(uint8_t *buffer, uint8_t size)
{
#ifdef MIDI_ENABLE
    if (midi_state == USB_STATE_BUSY)
    {
        return 1;
    }
    midi_state = USB_STATE_BUSY;
    memcpy(midi_in_buffer, buffer, size);
    int ret = usbd_ep_start_write(0, MIDI_EPIN_ADDR, midi_in_buffer, 4);
    if (ret < 0)
    {
        midi_state = USB_STATE_IDLE;
        return 1;
    }
    return 0;
#else
    UNUSED(buffer); UNUSED(size); return 1;
#endif
}

int usb_send_mouse(uint8_t *buffer, uint8_t size)
{
#if defined(MOUSE_ENABLE)
#if !defined(MOUSE_SHARED_EP)
    if (mouse_state == USB_STATE_BUSY) return 1;
    mouse_state = USB_STATE_BUSY;
    memcpy(mouse_buffer, buffer, size);
    int ret = usbd_ep_start_write(0, MOUSE_EPIN_ADDR, mouse_buffer, MOUSE_EPSIZE);
    if (ret < 0)
    {
        mouse_state = USB_STATE_IDLE;
        return 1;
    }
#else
    return usb_send_shared_ep(buffer, size);
#endif
#endif
    return 0;
}

int usb_send_console(uint8_t *buffer, uint8_t size)
{
#ifdef CONSOLE_ENABLE
    if (console_state == USB_STATE_BUSY) return 1;
    console_state = USB_STATE_BUSY;
    memcpy(console_buffer, buffer, size);
    int ret = usbd_ep_start_write(0, CONSOLE_EPIN_ADDR, console_buffer, CONSOLE_EPSIZE);
    if (ret < 0)
    {
        console_state = USB_STATE_IDLE;
        return 1;
    }
#endif
    return 0;
}

int usb_send_joystick(uint8_t *buffer, uint8_t size)
{
#if defined(JOYSTICK_ENABLE)
#if !defined(JOYSTICK_SHARED_EP)
    if (joystick_state == USB_STATE_BUSY) return 1;
    joystick_state = USB_STATE_BUSY;
    memcpy(joystick_buffer, buffer, size);
    int ret = usbd_ep_start_write(0, JOYSTICK_EPIN_ADDR, joystick_buffer, JOYSTICK_EPSIZE);
    if (ret < 0)
    {
        joystick_state = USB_STATE_IDLE;
        return 1;
    }
#else
    return usb_send_shared_ep(buffer, size);
#endif
#endif
    return 0;
}

int usb_send_digitizer(uint8_t *buffer, uint8_t size)
{
#if defined(DIGITIZER_ENABLE)
#if !defined(DIGITIZER_SHARED_EP)
    if (digitizer_state == USB_STATE_BUSY) return 1;
    digitizer_state = USB_STATE_BUSY;
    memcpy(digitizer_buffer, buffer, size);
    int ret = usbd_ep_start_write(0, DIGITIZER_EPIN_ADDR, digitizer_buffer, DIGITIZER_EPSIZE);
    if (ret < 0)
    {
        digitizer_state = USB_STATE_IDLE;
        return 1;
    }
#else
    return usb_send_shared_ep(buffer, size);
#endif
#endif
    return 0;
}

int usb_send_xinput(uint8_t *buffer, uint8_t size)
{
#ifdef GAMEPAD_ENABLE
    UNUSED(size);
    if (xinput_state == USB_STATE_BUSY)
    {
        return 1;
    }
    xinput_state = USB_STATE_BUSY;
    if (size > 0 && size <= XINPUT_EPSIZE)
    {
        memset(xinput_in_buffer, 0, XINPUT_EPSIZE);
        memcpy(xinput_in_buffer, buffer, size);
    }
    else
    {
        return 1;
    }
    int ret = usbd_ep_start_write(0, XINPUT_EPIN_ADDR, xinput_in_buffer, size);
    if (ret < 0)
    {
        xinput_state = USB_STATE_IDLE;
        return 1;
    }
    return 0;
#else
    UNUSED(buffer); UNUSED(size); return 1;
#endif
}


__WEAK void usbd_event_handler_user(uint8_t busid, uint8_t event)
{
    UNUSED(busid);
    UNUSED(event);
}

#include "driver.h"
int hid_send_shared_ep(uint8_t *report, uint16_t len)
{
    return usb_send_shared_ep(report, len);
}

int hid_send_keyboard(uint8_t *report, uint16_t len)
{
    return usb_send_keyboard(report, len);
}

int hid_send_nkro(uint8_t *report, uint16_t len)
{
    return usb_send_shared_ep(report, len);
}

int hid_send_extra_key(uint8_t*report,uint16_t len)
{
    return usb_send_shared_ep(report, len);
}

int hid_send_mouse(uint8_t*report,uint16_t len)
{
    return usb_send_shared_ep(report, len);
}

int hid_send_joystick(uint8_t*report,uint16_t len)
{
    return usb_send_shared_ep(report, len);
}

int hid_send_raw(uint8_t *report, uint16_t len)
{
    return usb_send_raw(report, len);
}

int send_midi(uint8_t *report, uint16_t len)
{
    return usb_send_midi(report, len);
}

int send_remote_wakeup(void)
{
    return usbd_send_remote_wakeup(0);
}

int hid_send_gamepad(uint8_t *report, uint16_t len)
{
    return usb_send_xinput(report, len);
}
