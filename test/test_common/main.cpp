#include <gtest/gtest.h>

#include "keyboard.h"

class GlobalEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        keyboard_init();
        g_keyboard.config.nkro = false;
    }

    void TearDown() override {
    }
};

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);

    ::testing::AddGlobalTestEnvironment(new GlobalEnvironment());

    return RUN_ALL_TESTS();
}
