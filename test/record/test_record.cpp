#include <gtest/gtest.h>

#include "record.h"

extern "C" void loop_array_init(LoopArray *arr, LoopArrayElement *data, uint16_t len);

TEST(Record, LoopArrayPushGetWrapAndMax)
{
    LoopArrayElement data[3] = {};
    LoopArray array = {};
    loop_array_init(&array, data, 3);

    loop_array_push_back(&array, 10);
    loop_array_push_back(&array, 20);
    loop_array_push_back(&array, 30);

    EXPECT_EQ(30, loop_array_get(&array, 0));
    EXPECT_EQ(20, loop_array_get(&array, 1));
    EXPECT_EQ(10, loop_array_get(&array, 2));
    EXPECT_EQ(0, loop_array_get(&array, 3));
    EXPECT_EQ(30, loop_array_max(&array));

    loop_array_push_back(&array, 40);

    EXPECT_EQ(40, loop_array_get(&array, 0));
    EXPECT_EQ(30, loop_array_get(&array, 1));
    EXPECT_EQ(20, loop_array_get(&array, 2));
    EXPECT_EQ(40, loop_array_max(&array));
}
