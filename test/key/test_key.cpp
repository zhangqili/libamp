#include <gtest/gtest.h>

#include "key.h"
#include "advanced_key.h"

TEST(example, add) {
    double res;
    res = add_numbers(1.0, 2.0);
    EXPECT_EQ(res, 3.0, 1.0e-11);
}