#include <cstddef>
#include <cstdint>
#include <cstring>

#include "keyboard.h"
#include "usb_descriptor.h"

#include <gtest/gtest.h>

namespace {
void expect_serial_descriptor_string(const char *expected)
{
    set_serial_number_descriptor();

    const auto  *descriptor = reinterpret_cast<const USB_Descriptor_String_t *>(SerialNumberString);
    const size_t expected_chars = std::strlen(expected);

    EXPECT_EQ(sizeof(USB_Descriptor_Header_t) + (expected_chars * sizeof(wchar_t)), descriptor->Header.Size);
    EXPECT_EQ(DTYPE_String, descriptor->Header.Type);

    for (size_t i = 0; i < expected_chars; ++i) {
        EXPECT_EQ(static_cast<wchar_t>(expected[i]), descriptor->UnicodeString[i]);
    }
}
} // namespace

TEST(UsbSerialNumber, WeakDefaultReturnsConfiguredSerial)
{
    EXPECT_STREQ(SERIAL_NUMBER, usb_descriptor_get_serial_number_ascii());
}

TEST(UsbSerialNumber, BuildsUsbStringDescriptorFromWeakDefault)
{
    expect_serial_descriptor_string(SERIAL_NUMBER);
}

TEST(UsbSerialNumber, ConvertsBytesToUppercaseHexString)
{
    char buffer[9] = {};
    const uint8_t id[] = {0x12, 0xAB, 0x00, 0xF0};

    EXPECT_EQ(8U, usb_descriptor_bytes_to_hex_string(buffer, sizeof(buffer), id, sizeof(id)));
    EXPECT_STREQ("12AB00F0", buffer);
}

TEST(UsbSerialNumber, HexStringConversionTruncatesToCompleteBytes)
{
    char buffer[6] = {};
    const uint8_t id[] = {0x12, 0xAB, 0x00, 0xF0};

    EXPECT_EQ(4U, usb_descriptor_bytes_to_hex_string(buffer, sizeof(buffer), id, sizeof(id)));
    EXPECT_STREQ("12AB", buffer);
}

TEST(UsbSerialNumber, HexStringConversionHandlesEmptyInputs)
{
    char buffer[9] = {'x'};
    const uint8_t id[] = {0x12};

    EXPECT_EQ(0U, usb_descriptor_bytes_to_hex_string(buffer, 0, id, sizeof(id)));
    EXPECT_EQ('x', buffer[0]);

    EXPECT_EQ(0U, usb_descriptor_bytes_to_hex_string(buffer, sizeof(buffer), NULL, sizeof(id)));
    EXPECT_STREQ("", buffer);

    EXPECT_EQ(0U, usb_descriptor_bytes_to_hex_string(buffer, sizeof(buffer), id, 0));
    EXPECT_STREQ("", buffer);
}
