#define CONFIG_USB_HS
#define MAX_ENDPOINTS 16
#define MTP_ENABLE
#define GAMEPAD_ENABLE
#define WEBUSB_ENABLE
#define LIGHTING_ENABLE

#include "usb_descriptor.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

#include <gtest/gtest.h>

static uint16_t read_le16(const uint8_t *data)
{
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

static uint32_t read_le32(const uint8_t *data)
{
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

static_assert(sizeof(USB_Descriptor_Header_t) == 2);
static_assert(sizeof(USB_Descriptor_Device_t) == 18);
static_assert(sizeof(USB_Descriptor_Configuration_Header_t) == 9);
static_assert(sizeof(USB_Descriptor_Interface_t) == 9);
static_assert(sizeof(USB_Descriptor_Interface_Association_t) == 8);
static_assert(sizeof(USB_Descriptor_Endpoint_t) == 7);
static_assert(sizeof(USB_Descriptor_DeviceQualifier_t) == 10);
static_assert(sizeof(USB_Descriptor_BOS_t) == 5);
static_assert(sizeof(USB_Descriptor_DeviceCapability_Header_t) == 3);
static_assert(sizeof(USB_Descriptor_BOS_PlatformCapability_t) == 20);
static_assert(sizeof(USB_Descriptor_WebUSB_URL_t) == 3);
static_assert(sizeof(USB_Descriptor_SSEndpointCompanion_t) == 6);
static_assert(sizeof(USB_Descriptor_SSISOEndpointCompanion_t) == 8);
static_assert(sizeof(USB_HID_Descriptor_HID_t) == 9);
static_assert(sizeof(USB_CDC_Descriptor_FunctionalHeader_t) == 5);
static_assert(sizeof(USB_CDC_Descriptor_FunctionalCallManagement_t) == 5);
static_assert(sizeof(USB_CDC_Descriptor_FunctionalACM_t) == 4);
static_assert(sizeof(USB_CDC_Descriptor_FunctionalUnion_t) == 5);
static_assert(sizeof(USB_CDC_Descriptor_FunctionalEthernet_t) == 13);
static_assert(sizeof(USB_CDC_Descriptor_FunctionalNCM_t) == 6);
static_assert(sizeof(USB_Audio_Descriptor_Interface_AC_t) == 9);
static_assert(sizeof(USB_Audio_Descriptor_StreamEndpoint_Std_t) == 9);
static_assert(sizeof(USB_MIDI_Descriptor_AudioInterface_AS_t) == 7);
static_assert(sizeof(USB_MIDI_Descriptor_InputJack_t) == 6);
static_assert(sizeof(USB_MIDI_Descriptor_OutputJack_t) == 9);
static_assert(sizeof(USB_MIDI_Descriptor_Jack_Endpoint_t) == 5);
static_assert(sizeof(USB_DFU_Descriptor_Functional_t) == 9);
static_assert(sizeof(USB_MSOS20_Descriptor_SetHeader_t) == 10);
static_assert(sizeof(USB_MSOS20_Descriptor_ConfigurationSubsetHeader_t) == 8);
static_assert(sizeof(USB_MSOS20_Descriptor_FunctionSubsetHeader_t) == 8);
static_assert(sizeof(USB_MSOS20_Descriptor_CompatibleID_t) == 20);
static_assert(sizeof(USB_MSOS20_Descriptor_RegistryProperty_Header_t) == 8);
static_assert(sizeof(MIDI_EventPacket_t) == 4);
static_assert(sizeof(USB_XInput_Descriptor_HID_t) == 16);

TEST(UsbDescriptorCompat, FutureClassConstants)
{
    EXPECT_EQ(0x09, DTYPE_OTG);
    EXPECT_EQ(0x0A, DTYPE_Debug);
    EXPECT_EQ(0x0F, DTYPE_BOS);
    EXPECT_EQ(0x10, DTYPE_DeviceCapability);
    EXPECT_EQ(0x24, DTYPE_CSInterface);
    EXPECT_EQ(0x25, DTYPE_CSEndpoint);
    EXPECT_EQ(0x30, DTYPE_SuperSpeedEndpointCompanion);
    EXPECT_EQ(0x31, DTYPE_SuperSpeedIsoEndpointCompanion);

    EXPECT_EQ(0x05, USB_DCAPTYPE_Platform);
    EXPECT_EQ(0x03, WEBUSB_URL_TYPE);
    EXPECT_EQ(0x00, WEBUSB_URL_SCHEME_HTTP);
    EXPECT_EQ(0x01, WEBUSB_URL_SCHEME_HTTPS);

    EXPECT_EQ(0x08, MSC_CSCP_MassStorageClass);
    EXPECT_EQ(0x06, MSC_CSCP_SCSITransparentSubclass);
    EXPECT_EQ(0x50, MSC_CSCP_BulkOnlyTransportProtocol);

    EXPECT_EQ(0x06, IMAGE_CSCP_ImageClass);
    EXPECT_EQ(0x01, IMAGE_CSCP_StillImageSubclass);
    EXPECT_EQ(0x01, IMAGE_CSCP_PIMA15740Protocol);
    EXPECT_EQ(IMAGE_CSCP_ImageClass, MTP_CSCP_ImageClass);
    EXPECT_EQ(IMAGE_CSCP_StillImageSubclass, MTP_CSCP_StillImageSubclass);
    EXPECT_EQ(IMAGE_CSCP_PIMA15740Protocol, MTP_CSCP_PIMA15740Protocol);

    EXPECT_EQ(0xFE, DFU_CSCP_ApplicationSpecificClass);
    EXPECT_EQ(0x01, DFU_CSCP_DFUSubclass);
    EXPECT_EQ(0x01, DFU_CSCP_RuntimeProtocol);
    EXPECT_EQ(0x02, DFU_CSCP_DFUProtocol);
    EXPECT_EQ(0x21, DFU_DTYPE_Functional);
    EXPECT_EQ(0x0F, DFU_ATTR_WILL_DETACH | DFU_ATTR_MANIFESTATION_TOLERANT | DFU_ATTR_CAN_UPLOAD | DFU_ATTR_CAN_DNLOAD);

    EXPECT_EQ(0x06, CDC_CSCP_ECMSubclass);
    EXPECT_EQ(0x0D, CDC_CSCP_NCMSubclass);
    EXPECT_EQ(0x01, CDC_CSCP_NCMDataProtocol);
    EXPECT_EQ(0x0F, CDC_DSUBTYPE_CSInterface_EthernetNetworking);
    EXPECT_EQ(0x1A, CDC_DSUBTYPE_CSInterface_NCM);

    EXPECT_EQ(0x00, USB_MSOS20_DTYPE_SetHeader);
    EXPECT_EQ(0x01, USB_MSOS20_DTYPE_SubsetHeaderConfiguration);
    EXPECT_EQ(0x02, USB_MSOS20_DTYPE_SubsetHeaderFunction);
    EXPECT_EQ(0x03, USB_MSOS20_DTYPE_FeatureCompatibleID);
    EXPECT_EQ(0x04, USB_MSOS20_DTYPE_FeatureRegProperty);
    EXPECT_EQ(0x02, USB_MSOS20_PROPERTY_TYPE_REG_EXPAND_SZ);
}

TEST(UsbDescriptor, DeviceDescriptorFields)
{
    EXPECT_EQ(sizeof(USB_Descriptor_Device_t), DeviceDescriptor.Header.Size);
    EXPECT_EQ(DTYPE_Device, DeviceDescriptor.Header.Type);
    EXPECT_EQ(VERSION_BCD(2, 1, 0), DeviceDescriptor.USBSpecification);
    EXPECT_EQ(USB_CSCP_NoDeviceClass, DeviceDescriptor.Class);
    EXPECT_EQ(USB_CSCP_NoDeviceSubclass, DeviceDescriptor.SubClass);
    EXPECT_EQ(USB_CSCP_NoDeviceProtocol, DeviceDescriptor.Protocol);
    EXPECT_EQ(FIXED_CONTROL_ENDPOINT_SIZE, DeviceDescriptor.Endpoint0Size);
    EXPECT_EQ(VENDOR_ID, DeviceDescriptor.VendorID);
    EXPECT_EQ(PRODUCT_ID, DeviceDescriptor.ProductID);
    EXPECT_EQ(DEVICE_VER, DeviceDescriptor.ReleaseNumber);
    EXPECT_EQ(0x01, DeviceDescriptor.ManufacturerStrIndex);
    EXPECT_EQ(0x02, DeviceDescriptor.ProductStrIndex);
    EXPECT_EQ(0x03, DeviceDescriptor.SerialNumStrIndex);
    EXPECT_EQ(FIXED_NUM_CONFIGURATIONS, DeviceDescriptor.NumberOfConfigurations);
}

TEST(UsbDescriptor, BackendNeutralExtraDescriptors)
{
    EXPECT_EQ(sizeof(USB_Descriptor_DeviceQualifier_t), DeviceQualifierDescriptor.Header.Size);
    EXPECT_EQ(DTYPE_DeviceQualifier, DeviceQualifierDescriptor.Header.Type);
    EXPECT_EQ(VERSION_BCD(2, 0, 0), DeviceQualifierDescriptor.USBSpecification);
    EXPECT_EQ(USB_CSCP_NoDeviceClass, DeviceQualifierDescriptor.Class);
    EXPECT_EQ(USB_CSCP_NoDeviceSubclass, DeviceQualifierDescriptor.SubClass);
    EXPECT_EQ(USB_CSCP_NoDeviceProtocol, DeviceQualifierDescriptor.Protocol);
    EXPECT_EQ(FIXED_CONTROL_ENDPOINT_SIZE, DeviceQualifierDescriptor.Endpoint0Size);
    EXPECT_EQ(FIXED_NUM_CONFIGURATIONS, DeviceQualifierDescriptor.NumberOfConfigurations);
    EXPECT_EQ(0, DeviceQualifierDescriptor.Reserved);

    static constexpr const char kDefaultWebUSBURL[] = "emi-keyboard-configurator.vercel.app";
    EXPECT_EQ(WEBUSB_URL_DESCRIPTOR_LENGTH, WebUSBURLDescriptor.Header.Size);
    EXPECT_EQ(WEBUSB_URL_TYPE, WebUSBURLDescriptor.Header.Type);
    EXPECT_EQ(WEBUSB_URL_SCHEME_HTTPS, WebUSBURLDescriptor.Scheme);
    EXPECT_EQ(0, std::memcmp(WebUSBURLDescriptor.URL, kDefaultWebUSBURL, sizeof(kDefaultWebUSBURL) - 1));

    EXPECT_EQ(MSOS20_TOTAL_LENGTH, sizeof(MSOS20DescriptorSet));
    EXPECT_EQ(316U, sizeof(MSOS20DescriptorSet));
    EXPECT_EQ(MSOS20_SET_HEADER_LENGTH, read_le16(&MSOS20DescriptorSet[0]));
    EXPECT_EQ(USB_MSOS20_DTYPE_SetHeader, read_le16(&MSOS20DescriptorSet[2]));
    EXPECT_EQ(0x06030000UL, read_le32(&MSOS20DescriptorSet[4]));
    EXPECT_EQ(sizeof(MSOS20DescriptorSet), read_le16(&MSOS20DescriptorSet[8]));

    constexpr size_t kXInputSubsetOffset = MSOS20_SET_HEADER_LENGTH;
    EXPECT_EQ(MSOS20_FUNCTION_SUBSET_HEADER_LENGTH, read_le16(&MSOS20DescriptorSet[kXInputSubsetOffset]));
    EXPECT_EQ(USB_MSOS20_DTYPE_SubsetHeaderFunction, read_le16(&MSOS20DescriptorSet[kXInputSubsetOffset + 2]));
    EXPECT_EQ(XINPUT_INTERFACE, MSOS20DescriptorSet[kXInputSubsetOffset + 4]);
    EXPECT_EQ(MSOS20_XINPUT_FUNCTION_SUBSET_LENGTH, read_le16(&MSOS20DescriptorSet[kXInputSubsetOffset + 6]));
    EXPECT_EQ(0, std::memcmp(&MSOS20DescriptorSet[kXInputSubsetOffset + 12], "XUSB20", 6));

    constexpr size_t kWebUSBSubsetOffset = kXInputSubsetOffset + MSOS20_XINPUT_FUNCTION_SUBSET_LENGTH;
    EXPECT_EQ(WEBUSB_INTERFACE, MSOS20DescriptorSet[kWebUSBSubsetOffset + 4]);
    EXPECT_EQ(MSOS20_WINUSB_FUNCTION_SUBSET_LENGTH, read_le16(&MSOS20DescriptorSet[kWebUSBSubsetOffset + 6]));
    EXPECT_EQ(0, std::memcmp(&MSOS20DescriptorSet[kWebUSBSubsetOffset + 12], "WINUSB", 6));
    EXPECT_EQ(USB_MSOS20_PROPERTY_TYPE_REG_MULTI_SZ, read_le16(&MSOS20DescriptorSet[kWebUSBSubsetOffset + 32]));

    constexpr size_t kMTPSubsetOffset = kWebUSBSubsetOffset + MSOS20_WINUSB_FUNCTION_SUBSET_LENGTH;
    EXPECT_EQ(MTP_INTERFACE, MSOS20DescriptorSet[kMTPSubsetOffset + 4]);
    EXPECT_EQ(MSOS20_MTP_FUNCTION_SUBSET_LENGTH, read_le16(&MSOS20DescriptorSet[kMTPSubsetOffset + 6]));
    EXPECT_EQ(USB_MSOS20_PROPERTY_TYPE_REG_EXPAND_SZ, read_le16(&MSOS20DescriptorSet[kMTPSubsetOffset + 12]));

    EXPECT_EQ(BOS_TOTAL_LENGTH, sizeof(BOSDescriptor));
    EXPECT_EQ(57U, sizeof(BOSDescriptor));
    EXPECT_EQ(BOS_HEADER_LENGTH, BOSDescriptor[0]);
    EXPECT_EQ(DTYPE_BOS, BOSDescriptor[1]);
    EXPECT_EQ(sizeof(BOSDescriptor), read_le16(&BOSDescriptor[2]));
    EXPECT_EQ(2, BOSDescriptor[4]);
    EXPECT_EQ(BOS_WEBUSB_PLATFORM_CAPABILITY_LENGTH, BOSDescriptor[5]);
    EXPECT_EQ(DTYPE_DeviceCapability, BOSDescriptor[6]);
    EXPECT_EQ(USB_DCAPTYPE_Platform, BOSDescriptor[7]);
    EXPECT_EQ(WEBUSB_VENDOR_CODE, BOSDescriptor[27]);

    constexpr size_t kMSOS20BOSOffset = BOS_HEADER_LENGTH + BOS_WEBUSB_PLATFORM_CAPABILITY_LENGTH;
    EXPECT_EQ(BOS_MSOS20_PLATFORM_CAPABILITY_LENGTH, BOSDescriptor[kMSOS20BOSOffset]);
    EXPECT_EQ(DTYPE_DeviceCapability, BOSDescriptor[kMSOS20BOSOffset + 1]);
    EXPECT_EQ(USB_DCAPTYPE_Platform, BOSDescriptor[kMSOS20BOSOffset + 2]);
    EXPECT_EQ(0x06030000UL, read_le32(&BOSDescriptor[kMSOS20BOSOffset + 20]));
    EXPECT_EQ(sizeof(MSOS20DescriptorSet), read_le16(&BOSDescriptor[kMSOS20BOSOffset + 24]));
    EXPECT_EQ(MSOS20_VENDOR_CODE, BOSDescriptor[kMSOS20BOSOffset + 26]);
}

TEST(UsbDescriptor, ConfigurationDescriptorFields)
{
    EXPECT_EQ(sizeof(USB_Descriptor_Configuration_t), sizeof(ConfigurationDescriptor));
    EXPECT_EQ(sizeof(USB_Descriptor_Configuration_Header_t), ConfigurationDescriptor.Config.Header.Size);
    EXPECT_EQ(DTYPE_Configuration, ConfigurationDescriptor.Config.Header.Type);
    EXPECT_EQ(sizeof(USB_Descriptor_Configuration_t), ConfigurationDescriptor.Config.TotalConfigurationSize);
    EXPECT_EQ(TOTAL_INTERFACES, ConfigurationDescriptor.Config.TotalInterfaces);
    EXPECT_EQ(8, TOTAL_INTERFACES);
    EXPECT_EQ(1, ConfigurationDescriptor.Config.ConfigurationNumber);
    EXPECT_EQ(NO_DESCRIPTOR, ConfigurationDescriptor.Config.ConfigurationStrIndex);
    EXPECT_EQ(USB_CONFIG_ATTR_RESERVED | USB_CONFIG_ATTR_REMOTEWAKEUP, ConfigurationDescriptor.Config.ConfigAttributes);
    EXPECT_EQ(USB_CONFIG_POWER_MA(500), ConfigurationDescriptor.Config.MaxPowerConsumption);
}

TEST(UsbDescriptor, InterfaceAndEndpointAssignments)
{
    EXPECT_EQ(0, KEYBOARD_INTERFACE);
    EXPECT_EQ(1, RAW_INTERFACE);
    EXPECT_EQ(2, SHARED_INTERFACE);
    EXPECT_EQ(3, AC_INTERFACE);
    EXPECT_EQ(4, AS_INTERFACE);
    EXPECT_EQ(5, MTP_INTERFACE);
    EXPECT_EQ(6, XINPUT_INTERFACE);
    EXPECT_EQ(7, WEBUSB_INTERFACE);

    EXPECT_EQ(ENDPOINT_DIR_IN | 1, KEYBOARD_EPIN_ADDR);
    EXPECT_EQ(ENDPOINT_DIR_IN | 2, RAW_EPIN_ADDR);
    EXPECT_EQ(ENDPOINT_DIR_OUT | 2, RAW_EPOUT_ADDR);
    EXPECT_EQ(ENDPOINT_DIR_IN | 3, SHARED_EPIN_ADDR);
    EXPECT_EQ(ENDPOINT_DIR_IN | 4, MIDI_EPIN_ADDR);
    EXPECT_EQ(ENDPOINT_DIR_OUT | 4, MIDI_EPOUT_ADDR);
    EXPECT_EQ(ENDPOINT_DIR_IN | 5, MTP_EVT_EPIN_ADDR);
    EXPECT_EQ(ENDPOINT_DIR_IN | 6, MTP_IN_EPIN_ADDR);
    EXPECT_EQ(ENDPOINT_DIR_OUT | 6, MTP_OUT_EPOUT_ADDR);
    EXPECT_EQ(ENDPOINT_DIR_IN | 7, XINPUT_EPIN_ADDR);
    EXPECT_EQ(ENDPOINT_DIR_OUT | 7, XINPUT_EPOUT_ADDR);

    EXPECT_EQ(KEYBOARD_EPIN_ADDR, ConfigurationDescriptor.Keyboard_INEndpoint.EndpointAddress);
    EXPECT_EQ(RAW_EPIN_ADDR, ConfigurationDescriptor.Raw_INEndpoint.EndpointAddress);
    EXPECT_EQ(RAW_EPOUT_ADDR, ConfigurationDescriptor.Raw_OUTEndpoint.EndpointAddress);
    EXPECT_EQ(SHARED_EPIN_ADDR, ConfigurationDescriptor.Shared_INEndpoint.EndpointAddress);
    EXPECT_EQ(MIDI_EPIN_ADDR, ConfigurationDescriptor.MIDI_Out_Jack_Endpoint.Endpoint.EndpointAddress);
    EXPECT_EQ(MIDI_EPOUT_ADDR, ConfigurationDescriptor.MIDI_In_Jack_Endpoint.Endpoint.EndpointAddress);
    EXPECT_EQ(MTP_EVT_EPIN_ADDR, ConfigurationDescriptor.MTP_EventEndpoint.EndpointAddress);
    EXPECT_EQ(MTP_IN_EPIN_ADDR, ConfigurationDescriptor.MTP_DataInEndpoint.EndpointAddress);
    EXPECT_EQ(MTP_OUT_EPOUT_ADDR, ConfigurationDescriptor.MTP_DataOutEndpoint.EndpointAddress);
    EXPECT_EQ(XINPUT_EPIN_ADDR, ConfigurationDescriptor.XInput_INEndpoint.EndpointAddress);
    EXPECT_EQ(XINPUT_EPOUT_ADDR, ConfigurationDescriptor.XInput_OUTEndpoint.EndpointAddress);
    EXPECT_EQ(WEBUSB_INTERFACE, ConfigurationDescriptor.WebUSB_Interface.InterfaceNumber);

    EXPECT_EQ(KEYBOARD_EPSIZE, ConfigurationDescriptor.Keyboard_INEndpoint.EndpointSize);
    EXPECT_EQ(RAW_EPSIZE, ConfigurationDescriptor.Raw_INEndpoint.EndpointSize);
    EXPECT_EQ(SHARED_EPSIZE, ConfigurationDescriptor.Shared_INEndpoint.EndpointSize);
    EXPECT_EQ(MIDI_STREAM_EPSIZE, ConfigurationDescriptor.MIDI_Out_Jack_Endpoint.Endpoint.EndpointSize);
    EXPECT_EQ(512, MIDI_STREAM_EPSIZE);
    EXPECT_EQ(MTP_DATA_EPSIZE, ConfigurationDescriptor.MTP_DataInEndpoint.EndpointSize);
    EXPECT_EQ(512, MTP_DATA_EPSIZE);
    EXPECT_EQ(XINPUT_EPSIZE, ConfigurationDescriptor.XInput_INEndpoint.EndpointSize);
}

TEST(UsbDescriptor, HidReportDescriptors)
{
    static constexpr uint8_t kKeyboardPrefix[] = {
        0x05, 0x01, // Usage Page (Generic Desktop)
        0x09, 0x06, // Usage (Keyboard)
        0xA1, 0x01, // Collection (Application)
    };

    static constexpr uint8_t kUsage16[] = {HID_RI_USAGE(16, 0x0238)};
    static_assert(sizeof(kUsage16) == 3);
    EXPECT_EQ(0x0A, kUsage16[0]);
    EXPECT_EQ(0x38, kUsage16[1]);
    EXPECT_EQ(0x02, kUsage16[2]);

    ASSERT_GE(sizeof(KeyboardReport), sizeof(kKeyboardPrefix));
    EXPECT_EQ(0, std::memcmp(KeyboardReport, kKeyboardPrefix, sizeof(kKeyboardPrefix)));
    EXPECT_EQ(68U, sizeof(KeyboardReport));
    EXPECT_EQ(34U, sizeof(RawReport));
    EXPECT_EQ(561U, sizeof(SharedReport));
    EXPECT_EQ(sizeof(KeyboardReport), ConfigurationDescriptor.Keyboard_HID.HIDReportLength);
    EXPECT_EQ(sizeof(RawReport), ConfigurationDescriptor.Raw_HID.HIDReportLength);
    EXPECT_EQ(sizeof(SharedReport), ConfigurationDescriptor.Shared_HID.HIDReportLength);
}

TEST(UsbDescriptor, RawUsageDefaults)
{
    EXPECT_EQ(0xFF60, RAW_USAGE_PAGE);
    EXPECT_EQ(0x61, RAW_USAGE_ID);
}
