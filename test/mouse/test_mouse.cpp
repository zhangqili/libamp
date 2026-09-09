#include <gtest/gtest.h>

#include "keyboard.h"
#include "mouse.h"
#include "test_fixture.h"

TEST(Mouse, Buffer)
{
    mouse_report_clear();
    mouse_report_add(MK_EVENT(MOUSE_COLLECTION|(MOUSE_LBUTTON<<8), KEYBOARD_EVENT_NO_EVENT, NULL));
    mouse_report_add(MK_EVENT(MOUSE_COLLECTION|(MOUSE_RBUTTON<<8), KEYBOARD_EVENT_NO_EVENT, NULL));
    mouse_report_add(MK_EVENT(MOUSE_COLLECTION|(MOUSE_WHEEL_DOWN<<8), KEYBOARD_EVENT_NO_EVENT, NULL));
    mouse_report_send();
    MouseReport* mouse= (MouseReport*)shared_ep_send_buffer;
    EXPECT_EQ(mouse->buttons, BIT(0)|BIT(1));
    EXPECT_EQ(mouse->v, -1);
}
