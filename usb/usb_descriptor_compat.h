/*
 * Copyright 2026 Zhangqi Li (@zhangqili)
 *
 * This file contains the USB descriptor compatibility surface libamp needs
 * after dropping the vendored LUFA tree. It intentionally provides descriptor
 * constants, HID report item encoding, and packed descriptor POD types only.
 * It is not a USB stack and does not provide endpoint, control request, or
 * class driver runtime APIs.
 */

/*
                         LUFA Library
         Copyright (C) Dean Camera, 2021.

    dean [at] fourwalledcubicle [dot] com
                     www.lufa-lib.org
*/

/*
    Copyright 2021  Dean Camera (dean [at] fourwalledcubicle [dot] com)

    Permission to use, copy, modify, distribute, and sell this
    software and its documentation for any purpose is hereby granted
    without fee, provided that the above copyright notice appear in
    all copies and that both that the copyright notice and this
    permission notice and warranty disclaimer appear in supporting
    documentation, and that the name of the author not be used in
    advertising or publicity pertaining to distribution of the
    software without specific, written prior permission.

    The author disclaims all warranties with regard to this
    software, including all implied warranties of merchantability
    and fitness.  In no event shall the author be liable for any
    special, indirect or consequential damages or any damages
    whatsoever resulting from loss of use, data or profits, whether
    in an action of contract, negligence or other tortious action,
    arising out of or in connection with the use or performance of
    this software.
*/

#pragma once

#include <stddef.h>
#include <stdint.h>
#include "keyboard_def.h"

/* Core preprocessor, endian, and descriptor helper macros. */
#ifndef CONCAT
#    define CONCAT(x, y) x##y
#endif

#ifndef CONCAT_EXPANDED
#    define CONCAT_EXPANDED(x, y) CONCAT(x, y)
#endif

#ifndef CPU_TO_LE16
#    define CPU_TO_LE16(x) (x)
#endif

#ifndef CPU_TO_LE32
#    define CPU_TO_LE32(x) (x)
#endif

#define NO_DESCRIPTOR 0
#ifndef USB_CONFIG_POWER_MA
#    define USB_CONFIG_POWER_MA(mA) ((mA) >> 1)
#endif
#define USB_STRING_LEN(UnicodeChars) (sizeof(USB_Descriptor_Header_t) + ((UnicodeChars) << 1))
#define USB_STRING_DESCRIPTOR(String) \
    { .Header = { .Size = sizeof(USB_Descriptor_Header_t) + (sizeof(String) - 2), .Type = DTYPE_String }, .UnicodeString = String }
#define USB_STRING_DESCRIPTOR_ARRAY(...) \
    { .Header = { .Size = sizeof(USB_Descriptor_Header_t) + sizeof((uint16_t[]){__VA_ARGS__}), .Type = DTYPE_String }, .UnicodeString = {__VA_ARGS__} }

#define VERSION_BCD(Major, Minor, Revision) \
    CPU_TO_LE16((((Major)&0xFF) << 8) | (((Minor)&0x0F) << 4) | ((Revision)&0x0F))

#define LANGUAGE_ID_ENG 0x0409

/* Standard USB descriptor constants. */
#define USB_CONFIG_ATTR_RESERVED 0x80
#define USB_CONFIG_ATTR_SELFPOWERED 0x40
#define USB_CONFIG_ATTR_REMOTEWAKEUP 0x20

#define ENDPOINT_ATTR_NO_SYNC (0 << 2)
#define ENDPOINT_ATTR_ASYNC (1 << 2)
#define ENDPOINT_ATTR_ADAPTIVE (2 << 2)
#define ENDPOINT_ATTR_SYNC (3 << 2)

#define ENDPOINT_USAGE_DATA (0 << 4)
#define ENDPOINT_USAGE_FEEDBACK (1 << 4)
#define ENDPOINT_USAGE_IMPLICIT_FEEDBACK (2 << 4)

#define ENDPOINT_DIR_MASK 0x80
#define ENDPOINT_DIR_OUT 0x00
#define ENDPOINT_DIR_IN 0x80

#define EP_TYPE_MASK 0x03
#define EP_TYPE_CONTROL 0x00
#define EP_TYPE_ISOCHRONOUS 0x01
#define EP_TYPE_BULK 0x02
#define EP_TYPE_INTERRUPT 0x03

#define REQDIR_HOSTTODEVICE (0 << 7)
#define REQDIR_DEVICETOHOST (1 << 7)
#define REQTYPE_STANDARD (0 << 5)
#define REQTYPE_CLASS (1 << 5)
#define REQTYPE_VENDOR (2 << 5)
#define REQREC_DEVICE (0 << 0)
#define REQREC_INTERFACE (1 << 0)
#define REQREC_ENDPOINT (2 << 0)
#define REQREC_OTHER (3 << 0)

enum USB_DescriptorTypes_t {
    DTYPE_Device = 0x01,
    DTYPE_Configuration = 0x02,
    DTYPE_String = 0x03,
    DTYPE_Interface = 0x04,
    DTYPE_Endpoint = 0x05,
    DTYPE_DeviceQualifier = 0x06,
    DTYPE_Other = 0x07,
    DTYPE_InterfacePower = 0x08,
    DTYPE_OTG = 0x09,
    DTYPE_Debug = 0x0A,
    DTYPE_InterfaceAssociation = 0x0B,
    DTYPE_BOS = 0x0F,
    DTYPE_DeviceCapability = 0x10,
    DTYPE_Functional = 0x21,
    DTYPE_CSDevice = 0x21,
    DTYPE_CSConfiguration = 0x22,
    DTYPE_CSString = 0x23,
    DTYPE_CSInterface = 0x24,
    DTYPE_CSEndpoint = 0x25,
    DTYPE_SuperSpeedEndpointCompanion = 0x30,
    DTYPE_SuperSpeedIsoEndpointCompanion = 0x31,
};

enum USB_DeviceCapabilityTypes_t {
    USB_DCAPTYPE_WirelessUSB = 0x01,
    USB_DCAPTYPE_USB20Extension = 0x02,
    USB_DCAPTYPE_SuperSpeedUSB = 0x03,
    USB_DCAPTYPE_ContainerID = 0x04,
    USB_DCAPTYPE_Platform = 0x05,
    USB_DCAPTYPE_SuperSpeedPlus = 0x0A,
};

enum USB_Descriptor_ClassSubclassProtocol_t {
    USB_CSCP_NoDeviceClass = 0x00,
    USB_CSCP_NoDeviceSubclass = 0x00,
    USB_CSCP_NoDeviceProtocol = 0x00,
    USB_CSCP_ApplicationSpecificClass = 0xFE,
    USB_CSCP_VendorSpecificClass = 0xFF,
    USB_CSCP_VendorSpecificSubclass = 0xFF,
    USB_CSCP_VendorSpecificProtocol = 0xFF,
    USB_CSCP_IADDeviceClass = 0xEF,
    USB_CSCP_IADDeviceSubclass = 0x02,
    USB_CSCP_IADDeviceProtocol = 0x01,
};

/* HID class constants and report descriptor item encoding. */
enum HID_Descriptor_ClassSubclassProtocol_t {
    HID_CSCP_HIDClass = 0x03,
    HID_CSCP_NonBootSubclass = 0x00,
    HID_CSCP_BootSubclass = 0x01,
    HID_CSCP_NonBootProtocol = 0x00,
    HID_CSCP_KeyboardBootProtocol = 0x01,
    HID_CSCP_MouseBootProtocol = 0x02,
};

enum HID_DescriptorTypes_t {
    HID_DTYPE_HID = 0x21,
    HID_DTYPE_Report = 0x22,
};

/* CDC ACM and CDC networking constants. */
enum CDC_Descriptor_ClassSubclassProtocol_t {
    CDC_CSCP_CDCClass = 0x02,
    CDC_CSCP_NoSpecificSubclass = 0x00,
    CDC_CSCP_ACMSubclass = 0x02,
    CDC_CSCP_ECMSubclass = 0x06,
    CDC_CSCP_NCMSubclass = 0x0D,
    CDC_CSCP_ATCommandProtocol = 0x01,
    CDC_CSCP_NoSpecificProtocol = 0x00,
    CDC_CSCP_VendorSpecificProtocol = 0xFF,
    CDC_CSCP_CDCDataClass = 0x0A,
    CDC_CSCP_NoDataSubclass = 0x00,
    CDC_CSCP_NoDataProtocol = 0x00,
    CDC_CSCP_NCMDataProtocol = 0x01,
};

enum CDC_DescriptorTypes_t {
    CDC_DTYPE_CSInterface = 0x24,
    CDC_DTYPE_CSEndpoint = 0x25,
};

enum CDC_CSInterface_SubTypes_t {
    CDC_DSUBTYPE_CSInterface_Header = 0x00,
    CDC_DSUBTYPE_CSInterface_CallManagement = 0x01,
    CDC_DSUBTYPE_CSInterface_ACM = 0x02,
    CDC_DSUBTYPE_CSInterface_Union = 0x06,
    CDC_DSUBTYPE_CSInterface_EthernetNetworking = 0x0F,
    CDC_DSUBTYPE_CSInterface_NCM = 0x1A,
};

/* USB Audio 1.0 and MIDI streaming constants used by USB-MIDI descriptors. */
enum Audio_Descriptor_ClassSubclassProtocol_t {
    AUDIO_CSCP_AudioClass = 0x01,
    AUDIO_CSCP_ControlSubclass = 0x01,
    AUDIO_CSCP_ControlProtocol = 0x00,
    AUDIO_CSCP_AudioStreamingSubclass = 0x02,
    AUDIO_CSCP_MIDIStreamingSubclass = 0x03,
    AUDIO_CSCP_StreamingProtocol = 0x00,
};

enum AUDIO_DescriptorTypes_t {
    AUDIO_DTYPE_CSInterface = 0x24,
    AUDIO_DTYPE_CSEndpoint = 0x25,
};

enum Audio_CSInterface_AC_SubTypes_t {
    AUDIO_DSUBTYPE_CSInterface_Header = 0x01,
    AUDIO_DSUBTYPE_CSInterface_InputTerminal = 0x02,
    AUDIO_DSUBTYPE_CSInterface_OutputTerminal = 0x03,
    AUDIO_DSUBTYPE_CSInterface_Mixer = 0x04,
    AUDIO_DSUBTYPE_CSInterface_Selector = 0x05,
    AUDIO_DSUBTYPE_CSInterface_Feature = 0x06,
    AUDIO_DSUBTYPE_CSInterface_Processing = 0x07,
    AUDIO_DSUBTYPE_CSInterface_Extension = 0x08,
};

enum Audio_CSInterface_AS_SubTypes_t {
    AUDIO_DSUBTYPE_CSInterface_General = 0x01,
    AUDIO_DSUBTYPE_CSInterface_FormatType = 0x02,
    AUDIO_DSUBTYPE_CSInterface_FormatSpecific = 0x03,
};

enum Audio_CSEndpoint_SubTypes_t {
    AUDIO_DSUBTYPE_CSEndpoint_General = 0x01,
};

enum MIDI_JackTypes_t {
    MIDI_JACKTYPE_Embedded = 0x01,
    MIDI_JACKTYPE_External = 0x02,
};

/* Mass Storage, Image/MTP, DFU, and vendor helper constants. */
enum MSC_Descriptor_ClassSubclassProtocol_t {
    MSC_CSCP_MassStorageClass = 0x08,
    MSC_CSCP_SCSITransparentSubclass = 0x06,
    MSC_CSCP_BulkOnlyTransportProtocol = 0x50,
};

enum IMAGE_Descriptor_ClassSubclassProtocol_t {
    IMAGE_CSCP_ImageClass = 0x06,
    IMAGE_CSCP_StillImageSubclass = 0x01,
    IMAGE_CSCP_PIMA15740Protocol = 0x01,
};

enum MTP_Descriptor_ClassSubclassProtocol_t {
    MTP_CSCP_ImageClass = IMAGE_CSCP_ImageClass,
    MTP_CSCP_StillImageSubclass = IMAGE_CSCP_StillImageSubclass,
    MTP_CSCP_PIMA15740Protocol = IMAGE_CSCP_PIMA15740Protocol,
};

enum DFU_Descriptor_ClassSubclassProtocol_t {
    DFU_CSCP_ApplicationSpecificClass = USB_CSCP_ApplicationSpecificClass,
    DFU_CSCP_DFUSubclass = 0x01,
    DFU_CSCP_RuntimeProtocol = 0x01,
    DFU_CSCP_DFUProtocol = 0x02,
};

enum DFU_DescriptorTypes_t {
    DFU_DTYPE_Functional = 0x21,
};

enum DFU_Descriptor_Attributes_t {
    DFU_ATTR_CAN_DNLOAD = (1 << 0),
    DFU_ATTR_CAN_UPLOAD = (1 << 1),
    DFU_ATTR_MANIFESTATION_TOLERANT = (1 << 2),
    DFU_ATTR_WILL_DETACH = (1 << 3),
};

enum USB_MSOS20_DescriptorTypes_t {
    USB_MSOS20_DTYPE_SetHeader = 0x00,
    USB_MSOS20_DTYPE_SubsetHeaderConfiguration = 0x01,
    USB_MSOS20_DTYPE_SubsetHeaderFunction = 0x02,
    USB_MSOS20_DTYPE_FeatureCompatibleID = 0x03,
    USB_MSOS20_DTYPE_FeatureRegProperty = 0x04,
    USB_MSOS20_DTYPE_FeatureMinResumeTime = 0x05,
    USB_MSOS20_DTYPE_FeatureModelID = 0x06,
    USB_MSOS20_DTYPE_FeatureCCGPDevice = 0x07,
    USB_MSOS20_DTYPE_FeatureVendorRevision = 0x08,
};

enum USB_MSOS20_PropertyDataTypes_t {
    USB_MSOS20_PROPERTY_TYPE_REG_SZ = 0x01,
    USB_MSOS20_PROPERTY_TYPE_REG_EXPAND_SZ = 0x02,
    USB_MSOS20_PROPERTY_TYPE_REG_BINARY = 0x03,
    USB_MSOS20_PROPERTY_TYPE_REG_DWORD_LITTLE_ENDIAN = 0x04,
    USB_MSOS20_PROPERTY_TYPE_REG_DWORD_BIG_ENDIAN = 0x05,
    USB_MSOS20_PROPERTY_TYPE_REG_LINK = 0x06,
    USB_MSOS20_PROPERTY_TYPE_REG_MULTI_SZ = 0x07,
};

enum USB_WebUSB_URLSchemes_t {
    USB_WEBUSB_URL_SCHEME_HTTP = 0x00,
    USB_WEBUSB_URL_SCHEME_HTTPS = 0x01,
};

#ifndef WEBUSB_URL_TYPE
#    define WEBUSB_URL_TYPE 0x03
#endif
#ifndef WEBUSB_URL_SCHEME_HTTP
#    define WEBUSB_URL_SCHEME_HTTP USB_WEBUSB_URL_SCHEME_HTTP
#endif
#ifndef WEBUSB_URL_SCHEME_HTTPS
#    define WEBUSB_URL_SCHEME_HTTPS USB_WEBUSB_URL_SCHEME_HTTPS
#endif

#define HID_RI_DATA_SIZE_MASK 0x03
#define HID_RI_TYPE_MASK 0x0C
#define HID_RI_TAG_MASK 0xF0

#define HID_RI_TYPE_MAIN 0x00
#define HID_RI_TYPE_GLOBAL 0x04
#define HID_RI_TYPE_LOCAL 0x08

#define HID_RI_DATA_BITS_0 0x00
#define HID_RI_DATA_BITS_8 0x01
#define HID_RI_DATA_BITS_16 0x02
#define HID_RI_DATA_BITS_32 0x03
#define HID_RI_DATA_BITS(DataBits) CONCAT_EXPANDED(HID_RI_DATA_BITS_, DataBits)

#define _HID_RI_ENCODE_0(Data)
#define _HID_RI_ENCODE_8(Data) , ((Data)&0xFF)
#define _HID_RI_ENCODE_16(Data) _HID_RI_ENCODE_8(Data) _HID_RI_ENCODE_8((Data) >> 8)
#define _HID_RI_ENCODE_32(Data) _HID_RI_ENCODE_16(Data) _HID_RI_ENCODE_16((Data) >> 16)
#define _HID_RI_ENCODE(DataBits, ...) CONCAT_EXPANDED(_HID_RI_ENCODE_, DataBits(__VA_ARGS__))
#define _HID_RI_ENTRY(Type, Tag, DataBits, ...) (Type | Tag | HID_RI_DATA_BITS(DataBits)) _HID_RI_ENCODE(DataBits, (__VA_ARGS__))

#define HID_IOF_CONSTANT (1 << 0)
#define HID_IOF_DATA (0 << 0)
#define HID_IOF_VARIABLE (1 << 1)
#define HID_IOF_ARRAY (0 << 1)
#define HID_IOF_RELATIVE (1 << 2)
#define HID_IOF_ABSOLUTE (0 << 2)
#define HID_IOF_WRAP (1 << 3)
#define HID_IOF_NO_WRAP (0 << 3)
#define HID_IOF_NON_LINEAR (1 << 4)
#define HID_IOF_LINEAR (0 << 4)
#define HID_IOF_NO_PREFERRED_STATE (1 << 5)
#define HID_IOF_PREFERRED_STATE (0 << 5)
#define HID_IOF_NULLSTATE (1 << 6)
#define HID_IOF_NO_NULL_POSITION (0 << 6)
#define HID_IOF_VOLATILE (1 << 7)
#define HID_IOF_NON_VOLATILE (0 << 7)
#define HID_IOF_BUFFERED_BYTES (1 << 8)
#define HID_IOF_BITFIELD (0 << 8)

#define HID_RI_INPUT(DataBits, ...) _HID_RI_ENTRY(HID_RI_TYPE_MAIN, 0x80, DataBits, __VA_ARGS__)
#define HID_RI_OUTPUT(DataBits, ...) _HID_RI_ENTRY(HID_RI_TYPE_MAIN, 0x90, DataBits, __VA_ARGS__)
#define HID_RI_COLLECTION(DataBits, ...) _HID_RI_ENTRY(HID_RI_TYPE_MAIN, 0xA0, DataBits, __VA_ARGS__)
#define HID_RI_FEATURE(DataBits, ...) _HID_RI_ENTRY(HID_RI_TYPE_MAIN, 0xB0, DataBits, __VA_ARGS__)
#define HID_RI_END_COLLECTION(DataBits, ...) _HID_RI_ENTRY(HID_RI_TYPE_MAIN, 0xC0, DataBits, __VA_ARGS__)
#define HID_RI_USAGE_PAGE(DataBits, ...) _HID_RI_ENTRY(HID_RI_TYPE_GLOBAL, 0x00, DataBits, __VA_ARGS__)
#define HID_RI_LOGICAL_MINIMUM(DataBits, ...) _HID_RI_ENTRY(HID_RI_TYPE_GLOBAL, 0x10, DataBits, __VA_ARGS__)
#define HID_RI_LOGICAL_MAXIMUM(DataBits, ...) _HID_RI_ENTRY(HID_RI_TYPE_GLOBAL, 0x20, DataBits, __VA_ARGS__)
#define HID_RI_PHYSICAL_MINIMUM(DataBits, ...) _HID_RI_ENTRY(HID_RI_TYPE_GLOBAL, 0x30, DataBits, __VA_ARGS__)
#define HID_RI_PHYSICAL_MAXIMUM(DataBits, ...) _HID_RI_ENTRY(HID_RI_TYPE_GLOBAL, 0x40, DataBits, __VA_ARGS__)
#define HID_RI_UNIT_EXPONENT(DataBits, ...) _HID_RI_ENTRY(HID_RI_TYPE_GLOBAL, 0x50, DataBits, __VA_ARGS__)
#define HID_RI_UNIT(DataBits, ...) _HID_RI_ENTRY(HID_RI_TYPE_GLOBAL, 0x60, DataBits, __VA_ARGS__)
#define HID_RI_REPORT_SIZE(DataBits, ...) _HID_RI_ENTRY(HID_RI_TYPE_GLOBAL, 0x70, DataBits, __VA_ARGS__)
#define HID_RI_REPORT_ID(DataBits, ...) _HID_RI_ENTRY(HID_RI_TYPE_GLOBAL, 0x80, DataBits, __VA_ARGS__)
#define HID_RI_REPORT_COUNT(DataBits, ...) _HID_RI_ENTRY(HID_RI_TYPE_GLOBAL, 0x90, DataBits, __VA_ARGS__)
#define HID_RI_PUSH(DataBits, ...) _HID_RI_ENTRY(HID_RI_TYPE_GLOBAL, 0xA0, DataBits, __VA_ARGS__)
#define HID_RI_POP(DataBits, ...) _HID_RI_ENTRY(HID_RI_TYPE_GLOBAL, 0xB0, DataBits, __VA_ARGS__)
#define HID_RI_USAGE(DataBits, ...) _HID_RI_ENTRY(HID_RI_TYPE_LOCAL, 0x00, DataBits, __VA_ARGS__)
#define HID_RI_USAGE_MINIMUM(DataBits, ...) _HID_RI_ENTRY(HID_RI_TYPE_LOCAL, 0x10, DataBits, __VA_ARGS__)
#define HID_RI_USAGE_MAXIMUM(DataBits, ...) _HID_RI_ENTRY(HID_RI_TYPE_LOCAL, 0x20, DataBits, __VA_ARGS__)

/* Standard USB descriptor PODs. Field names follow LUFA-style descriptors. */
typedef struct {
    uint8_t bmRequestType;
    uint8_t bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} __PACKED USB_Request_Header_t;

typedef struct {
    uint8_t Size;
    uint8_t Type;
} __PACKED USB_Descriptor_Header_t;

typedef struct {
    USB_Descriptor_Header_t Header;
    uint16_t USBSpecification;
    uint8_t Class;
    uint8_t SubClass;
    uint8_t Protocol;
    uint8_t Endpoint0Size;
    uint16_t VendorID;
    uint16_t ProductID;
    uint16_t ReleaseNumber;
    uint8_t ManufacturerStrIndex;
    uint8_t ProductStrIndex;
    uint8_t SerialNumStrIndex;
    uint8_t NumberOfConfigurations;
} __PACKED USB_Descriptor_Device_t;

typedef struct {
    USB_Descriptor_Header_t Header;
    uint16_t USBSpecification;
    uint8_t Class;
    uint8_t SubClass;
    uint8_t Protocol;
    uint8_t Endpoint0Size;
    uint8_t NumberOfConfigurations;
    uint8_t Reserved;
} __PACKED USB_Descriptor_DeviceQualifier_t;

typedef struct {
    USB_Descriptor_Header_t Header;
    uint16_t TotalConfigurationSize;
    uint8_t TotalInterfaces;
    uint8_t ConfigurationNumber;
    uint8_t ConfigurationStrIndex;
    uint8_t ConfigAttributes;
    uint8_t MaxPowerConsumption;
} __PACKED USB_Descriptor_Configuration_Header_t;

typedef struct {
    USB_Descriptor_Header_t Header;
    uint8_t InterfaceNumber;
    uint8_t AlternateSetting;
    uint8_t TotalEndpoints;
    uint8_t Class;
    uint8_t SubClass;
    uint8_t Protocol;
    uint8_t InterfaceStrIndex;
} __PACKED USB_Descriptor_Interface_t;

typedef struct {
    USB_Descriptor_Header_t Header;
    uint8_t FirstInterfaceIndex;
    uint8_t TotalInterfaces;
    uint8_t Class;
    uint8_t SubClass;
    uint8_t Protocol;
    uint8_t IADStrIndex;
} __PACKED USB_Descriptor_Interface_Association_t;

typedef struct {
    USB_Descriptor_Header_t Header;
    uint8_t EndpointAddress;
    uint8_t Attributes;
    uint16_t EndpointSize;
    uint8_t PollingIntervalMS;
} __PACKED USB_Descriptor_Endpoint_t;

typedef struct {
    USB_Descriptor_Header_t Header;
    uint16_t TotalLength;
    uint8_t TotalDeviceCaps;
} __PACKED USB_Descriptor_BOS_t;

typedef struct {
    USB_Descriptor_Header_t Header;
    uint8_t CapabilityType;
} __PACKED USB_Descriptor_DeviceCapability_Header_t;

typedef struct {
    USB_Descriptor_Header_t Header;
    uint8_t CapabilityType;
    uint8_t Reserved;
    uint8_t PlatformCapabilityUUID[16];
    uint8_t CapabilityData[];
} __PACKED USB_Descriptor_BOS_PlatformCapability_t;

typedef struct {
    USB_Descriptor_Header_t Header;
    uint8_t Scheme;
    char URL[];
} __PACKED USB_Descriptor_WebUSB_URL_t;

typedef struct {
    USB_Descriptor_Header_t Header;
    uint8_t MaxBurst;
    uint8_t Attributes;
    uint16_t BytesPerInterval;
} __PACKED USB_Descriptor_SSEndpointCompanion_t;

typedef struct {
    USB_Descriptor_Header_t Header;
    uint16_t Reserved;
    uint32_t BytesPerInterval;
} __PACKED USB_Descriptor_SSISOEndpointCompanion_t;

typedef struct {
    USB_Descriptor_Header_t Header;
    wchar_t UnicodeString[];
} __PACKED USB_Descriptor_String_t;

/* HID descriptor PODs. */
typedef struct {
    USB_Descriptor_Header_t Header;
    uint16_t HIDSpec;
    uint8_t CountryCode;
    uint8_t TotalReportDescriptors;
    uint8_t HIDReportType;
    uint16_t HIDReportLength;
} __PACKED USB_HID_Descriptor_HID_t;

typedef uint8_t USB_Descriptor_HIDReport_Datatype_t;

/* CDC ACM and CDC networking descriptor PODs. */
typedef struct {
    USB_Descriptor_Header_t Header;
    uint8_t Subtype;
    uint16_t CDCSpecification;
} __PACKED USB_CDC_Descriptor_FunctionalHeader_t;

typedef struct {
    USB_Descriptor_Header_t Header;
    uint8_t Subtype;
    uint8_t Capabilities;
    uint8_t DataInterface;
} __PACKED USB_CDC_Descriptor_FunctionalCallManagement_t;

typedef struct {
    USB_Descriptor_Header_t Header;
    uint8_t Subtype;
    uint8_t Capabilities;
} __PACKED USB_CDC_Descriptor_FunctionalACM_t;

typedef struct {
    USB_Descriptor_Header_t Header;
    uint8_t Subtype;
    uint8_t MasterInterfaceNumber;
    uint8_t SlaveInterfaceNumber;
} __PACKED USB_CDC_Descriptor_FunctionalUnion_t;

typedef struct {
    USB_Descriptor_Header_t Header;
    uint8_t Subtype;
    uint8_t MACAddressStringIndex;
    uint32_t EthernetStatistics;
    uint16_t MaxSegmentSize;
    uint16_t NumberMCFilters;
    uint8_t NumberPowerFilters;
} __PACKED USB_CDC_Descriptor_FunctionalEthernet_t;

typedef struct {
    USB_Descriptor_Header_t Header;
    uint8_t Subtype;
    uint16_t NCMVersion;
    uint8_t NetworkCapabilities;
} __PACKED USB_CDC_Descriptor_FunctionalNCM_t;

/* Audio 1.0 and USB-MIDI streaming descriptor PODs. */
typedef struct {
    USB_Descriptor_Header_t Header;
    uint8_t Subtype;
    uint16_t ACSpecification;
    uint16_t TotalLength;
    uint8_t InCollection;
    uint8_t InterfaceNumber;
} __PACKED USB_Audio_Descriptor_Interface_AC_t;

typedef struct {
    USB_Descriptor_Endpoint_t Endpoint;
    uint8_t Refresh;
    uint8_t SyncEndpointNumber;
} __PACKED USB_Audio_Descriptor_StreamEndpoint_Std_t;

typedef struct {
    USB_Descriptor_Header_t Header;
    uint8_t Subtype;
    uint16_t AudioSpecification;
    uint16_t TotalLength;
} __PACKED USB_MIDI_Descriptor_AudioInterface_AS_t;

typedef struct {
    USB_Descriptor_Header_t Header;
    uint8_t Subtype;
    uint8_t JackType;
    uint8_t JackID;
    uint8_t JackStrIndex;
} __PACKED USB_MIDI_Descriptor_InputJack_t;

typedef struct {
    USB_Descriptor_Header_t Header;
    uint8_t Subtype;
    uint8_t JackType;
    uint8_t JackID;
    uint8_t NumberOfPins;
    uint8_t SourceJackID[1];
    uint8_t SourcePinID[1];
    uint8_t JackStrIndex;
} __PACKED USB_MIDI_Descriptor_OutputJack_t;

typedef struct {
    USB_Descriptor_Header_t Header;
    uint8_t Subtype;
    uint8_t TotalEmbeddedJacks;
    uint8_t AssociatedJackID[1];
} __PACKED USB_MIDI_Descriptor_Jack_Endpoint_t;

/* DFU descriptor PODs. Runtime and DFU mode share this functional descriptor. */
typedef struct {
    USB_Descriptor_Header_t Header;
    uint8_t Attributes;
    uint16_t DetachTimeout;
    uint16_t TransferSize;
    uint16_t DFUVersion;
} __PACKED USB_DFU_Descriptor_Functional_t;

/* Microsoft OS 2.0 descriptor set PODs. Variable-length fields follow these headers. */
typedef struct {
    uint16_t Length;
    uint16_t DescriptorType;
    uint32_t WindowsVersion;
    uint16_t TotalLength;
} __PACKED USB_MSOS20_Descriptor_SetHeader_t;

typedef struct {
    uint16_t Length;
    uint16_t DescriptorType;
    uint8_t ConfigurationValue;
    uint8_t Reserved;
    uint16_t TotalLength;
} __PACKED USB_MSOS20_Descriptor_ConfigurationSubsetHeader_t;

typedef struct {
    uint16_t Length;
    uint16_t DescriptorType;
    uint8_t FirstInterface;
    uint8_t Reserved;
    uint16_t SubsetLength;
} __PACKED USB_MSOS20_Descriptor_FunctionSubsetHeader_t;

typedef struct {
    uint16_t Length;
    uint16_t DescriptorType;
    char CompatibleID[8];
    char SubCompatibleID[8];
} __PACKED USB_MSOS20_Descriptor_CompatibleID_t;

typedef struct {
    uint16_t Length;
    uint16_t DescriptorType;
    uint16_t PropertyDataType;
    uint16_t PropertyNameLength;
} __PACKED USB_MSOS20_Descriptor_RegistryProperty_Header_t;

/* USB-MIDI event packet. */
typedef struct {
    uint8_t Event;
    uint8_t Data1;
    uint8_t Data2;
    uint8_t Data3;
} __PACKED MIDI_EventPacket_t;
