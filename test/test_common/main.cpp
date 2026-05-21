#include <gtest/gtest.h>

#include "keyboard.h"
#include "test_fixture.h"

class PerTestResetListener : public ::testing::EmptyTestEventListener {
public:
    void OnTestStart(const ::testing::TestInfo&) override {
        libamp_test_reset_environment();
    }
};

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);

    ::testing::UnitTest::GetInstance()->listeners().Append(new PerTestResetListener());

    return RUN_ALL_TESTS();
}
