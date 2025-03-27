#include <gtest/gtest.h>

#include "key.h"

static int num;

extern "C" void key_up_cb(void*key)
{
    num = KEY_EVENT_UP;
}

extern "C" void key_down_cb(void*key)
{
    num = KEY_EVENT_DOWN;
}

TEST(KeyTest, Event)
{
    static Key key;
    key_attach(&key, KEY_EVENT_UP, key_up_cb);
    key_attach(&key, KEY_EVENT_DOWN, key_down_cb);
    key_update(&key, true);
    EXPECT_EQ(num, KEY_EVENT_DOWN);
    key_update(&key, false);
    EXPECT_EQ(num, KEY_EVENT_UP);
}

TEST(KeyTest, State)
{
    static Key key;
    key_update(&key, true);
    EXPECT_TRUE(key.state);
    key_update(&key, false);
    EXPECT_FALSE(key.state);
}