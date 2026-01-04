#include <gtest/gtest.h>

#include "extra_key.h"

extern uint8_t shared_ep_send_buffer[64];

TEST(ExtraKey, Buffer)
{
    AdvancedKey key;

    extra_key_event_handler({CONSUMER_COLLECTION|(CONSUMER_AUDIO_VOL_DOWN<<8), KEYBOARD_EVENT_KEY_DOWN,false,&key});
    consumer_key_buffer_send();
    EXPECT_EQ(shared_ep_send_buffer[0], REPORT_ID_CONSUMER);
    EXPECT_EQ(*(uint16_t*)&shared_ep_send_buffer[1], AUDIO_VOL_DOWN);

    extra_key_event_handler({CONSUMER_COLLECTION|(CONSUMER_AUDIO_VOL_UP<<8), KEYBOARD_EVENT_KEY_DOWN,false,&key});
    consumer_key_buffer_send();
    EXPECT_EQ(shared_ep_send_buffer[0], REPORT_ID_CONSUMER);
    EXPECT_EQ(*(uint16_t*)&shared_ep_send_buffer[1], AUDIO_VOL_UP);

    extra_key_event_handler({CONSUMER_COLLECTION|(CONSUMER_AUDIO_VOL_UP<<8), KEYBOARD_EVENT_KEY_UP,false,&key});
    consumer_key_buffer_send();
    EXPECT_EQ(shared_ep_send_buffer[0], REPORT_ID_CONSUMER);
    EXPECT_EQ(*(uint16_t*)&shared_ep_send_buffer[1], 0);

    extra_key_event_handler({SYSTEM_COLLECTION|(SYSTEM_WAKE_UP<<8), KEYBOARD_EVENT_KEY_DOWN,false,&key});
    system_key_buffer_send();
    EXPECT_EQ(shared_ep_send_buffer[0], REPORT_ID_SYSTEM);
    EXPECT_EQ(*(uint16_t*)&shared_ep_send_buffer[1], SYSTEM_WAKE_UP);

}
