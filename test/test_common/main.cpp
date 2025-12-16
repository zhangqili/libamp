#include <gtest/gtest.h>

#include "keyboard.h"

class GlobalEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        extern uint8_t flash_buffer[LFS_BLOCK_SIZE*LFS_BLOCK_COUNT];
        memset(flash_buffer, 0xFF, sizeof(flash_buffer));
        keyboard_init();
        g_keyboard_config.nkro = false;
    }

    void TearDown() override {
    }
};

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);

    ::testing::AddGlobalTestEnvironment(new GlobalEnvironment());

    return RUN_ALL_TESTS();
}
