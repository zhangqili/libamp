#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "keyboard.h"
#include "storage.h"

TEST(Storage, StorageLittleFS)
{
    for (int i = 0; i < sizeof(g_keymap); i++)
    {
        ((uint8_t*)g_keymap)[i] = rand();
    }
    uint8_t buffer[sizeof(g_keymap)];
    memcpy(buffer, g_keymap, sizeof(g_keymap));
    keyboard_save();
    keyboard_recovery();
    EXPECT_THAT(buffer, testing::ElementsAreArray(((uint8_t*)g_keymap), sizeof(g_keymap)));
}
