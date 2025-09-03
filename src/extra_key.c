/*
 * Copyright (c) 2025 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "extra_key.h"
#include "driver.h"

static ExtraKey consumer_buffer = {
    .report_id = REPORT_ID_CONSUMER,
};
static ExtraKey system_buffer = {
    .report_id = REPORT_ID_SYSTEM,
};

void extra_key_event_handler(KeyboardEvent event)
{
    switch (event.event)
    {
    case KEYBOARD_EVENT_KEY_UP:
        switch (KEYCODE_GET_MAIN(event.keycode))
        {
        case CONSUMER_COLLECTION:
            consumer_buffer.usage = 0;
            g_keyboard_report_flags.consumer = true;
            break;
        case SYSTEM_COLLECTION:
            system_buffer.usage = 0;
            g_keyboard_report_flags.system = true;
            break;
        default:
            break;
        }
        break;
    case KEYBOARD_EVENT_KEY_DOWN:
        switch (KEYCODE_GET_MAIN(event.keycode))
        {
        case CONSUMER_COLLECTION:
            consumer_buffer.usage = CONSUMER_KEYCODE_TO_RAWCODE(KEYCODE_GET_SUB(event.keycode));
            g_keyboard_report_flags.consumer = true;
            break;
        case SYSTEM_COLLECTION:
            system_buffer.usage = KEYCODE_GET_SUB(event.keycode);
            g_keyboard_report_flags.system = true;
            break;
        default:
            break;
        }
        break;
    case KEYBOARD_EVENT_KEY_TRUE:
        switch (KEYCODE_GET_MAIN(event.keycode))
        {
        case CONSUMER_COLLECTION:
            if (!consumer_buffer.usage)
            {
                consumer_buffer.usage = CONSUMER_KEYCODE_TO_RAWCODE(KEYCODE_GET_SUB(event.keycode));
            }
                break;
        case SYSTEM_COLLECTION:
            if (!system_buffer.usage)
            {
                system_buffer.usage = KEYCODE_GET_SUB(event.keycode);
            }
        default:
            break;
        }
        break;
    case KEYBOARD_EVENT_KEY_FALSE:
        break;
    default:
        break;
    }
}

int consumer_key_buffer_send(void)
{
    return hid_send_extra_key((uint8_t*)&consumer_buffer, sizeof(ExtraKey));
}

int system_key_buffer_send(void)
{
    return hid_send_extra_key((uint8_t*)&system_buffer, sizeof(ExtraKey));
}
