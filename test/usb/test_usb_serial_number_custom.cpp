#include <cstddef>
#include <cstring>

#include "keyboard.h"
#include "usb_descriptor.h"

#include <gtest/gtest.h>

extern "C" size_t usb_descriptor_get_serial_number(char *buffer, size_t buffer_size)
{
    static constexpr const char kCustomSerial[] = "CUSTOM123";

    if (buffer == NULL || buffer_size == 0) {
        return 0;
    }

    size_t length = 0;
    while (kCustomSerial[length] != '\0' && length + 1 < buffer_size) {
        buffer[length] = kCustomSerial[length];
        length++;
    }

    buffer[length] = '\0';
    return length;
}

TEST(UsbSerialNumberCustom, StrongHookOverridesWeakDefault)
{
    EXPECT_STREQ("CUSTOM123", usb_descriptor_get_serial_number_ascii());
}
