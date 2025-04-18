#include <gtest/gtest.h>

#include "keyboard.h"
#include "mouse.h"

extern uint8_t shared_ep_send_buffer[64];

TEST(Mouse, Buffer)
{
    mouse_buffer_clear();
    mouse_event_handler({MOUSE_COLLECTION|(MOUSE_LBUTTON<<8), KEYBOARD_EVENT_KEY_TRUE});
    mouse_event_handler({MOUSE_COLLECTION|(MOUSE_RBUTTON<<8), KEYBOARD_EVENT_KEY_TRUE});
    mouse_event_handler({MOUSE_COLLECTION|(MOUSE_WHEEL_DOWN<<8), KEYBOARD_EVENT_KEY_TRUE});
    mouse_buffer_send();
    Mouse* mouse= (Mouse*)shared_ep_send_buffer;
    EXPECT_EQ(mouse->buttons, BIT(0)|BIT(1));
    EXPECT_EQ(mouse->v, -1);
}
