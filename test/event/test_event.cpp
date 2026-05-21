#include <gtest/gtest.h>

#include "event_buffer.h"
#include "event_cache.h"

namespace {

KeyboardEvent event_with_keycode(Keycode keycode)
{
    KeyboardEvent event = {};
    event.keycode = keycode;
    event.event = KEYBOARD_EVENT_KEY_DOWN;
    event.is_virtual = true;
    return event;
}

} // namespace

TEST(EventBuffer, EmptyPopReturnsZeroEvent)
{
    EventLoopQueueElm data[3] = {};
    EventLoopQueue queue;
    event_loop_queue_init(&queue, data, 3);

    const auto popped = event_loop_queue_pop(&queue);

    EXPECT_EQ(0, popped.event.keycode);
    EXPECT_EQ(0, popped.tick);
}

TEST(EventBuffer, PushPopPreservesOrderAcrossWraparound)
{
    EventLoopQueueElm data[4] = {};
    EventLoopQueue queue;
    event_loop_queue_init(&queue, data, 4);

    event_loop_queue_push(&queue, {event_with_keycode(KEY_A), 10});
    event_loop_queue_push(&queue, {event_with_keycode(KEY_B), 20});
    EXPECT_EQ(KEY_A, event_loop_queue_pop(&queue).event.keycode);

    event_loop_queue_push(&queue, {event_with_keycode(KEY_C), 30});
    event_loop_queue_push(&queue, {event_with_keycode(KEY_D), 40});

    EXPECT_EQ(KEY_B, event_loop_queue_pop(&queue).event.keycode);
    EXPECT_EQ(KEY_C, event_loop_queue_pop(&queue).event.keycode);
    EXPECT_EQ(KEY_D, event_loop_queue_pop(&queue).event.keycode);
}

TEST(EventBuffer, FullQueueDropsNewestElement)
{
    EventLoopQueueElm data[3] = {};
    EventLoopQueue queue;
    event_loop_queue_init(&queue, data, 3);

    event_loop_queue_push(&queue, {event_with_keycode(KEY_A), 1});
    event_loop_queue_push(&queue, {event_with_keycode(KEY_B), 2});
    event_loop_queue_push(&queue, {event_with_keycode(KEY_C), 3});

    EXPECT_EQ(KEY_A, event_loop_queue_pop(&queue).event.keycode);
    EXPECT_EQ(KEY_B, event_loop_queue_pop(&queue).event.keycode);
    EXPECT_EQ(0, event_loop_queue_pop(&queue).event.keycode);
}

TEST(EventCache, FindsAndRemovesOwnerScopedKeycodes)
{
    EventCacheListNode nodes[6] = {};
    EventCacheList list;
    event_forward_list_init(&list, nodes, 6);

    int owner_a;
    int owner_b;
    event_forward_list_insert_after(&list, &list.data[list.head], {event_with_keycode(KEY_A), &owner_a});
    event_forward_list_insert_after(&list, &list.data[list.head], {event_with_keycode(KEY_B), &owner_a});
    event_forward_list_insert_after(&list, &list.data[list.head], {event_with_keycode(KEY_A), &owner_b});

    EXPECT_TRUE(event_forward_list_exists_keycode(&list, &owner_a, KEY_A));
    EXPECT_TRUE(event_forward_list_exists_keycode(&list, &owner_b, KEY_A));

    event_forward_list_remove_first_keycode(&list, &owner_a, KEY_A);

    EXPECT_FALSE(event_forward_list_exists_keycode(&list, &owner_a, KEY_A));
    EXPECT_TRUE(event_forward_list_exists_keycode(&list, &owner_a, KEY_B));
    EXPECT_TRUE(event_forward_list_exists_keycode(&list, &owner_b, KEY_A));
}

TEST(EventCache, BufferKeepsFullWidthOwnerPointer)
{
    event_cache_init();
    int owner;

    event_cache_buffer_push(event_with_keycode(KEY_C), &owner);
    event_cache_add_buffer();

    EXPECT_TRUE(event_forward_list_exists_keycode(&g_event_buffer_list, &owner, KEY_C));
}
