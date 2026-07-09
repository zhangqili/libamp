#include <gtest/gtest.h>

#include "analog.h"

TEST(Analog, RingBufferAverageUsesCurrentWindow)
{
    RingBuf ringbuf = {};

    ringbuf_push(&ringbuf, 10);
    EXPECT_EQ(5, ringbuf_avg(&ringbuf));

    ringbuf_push(&ringbuf, 30);
    EXPECT_EQ(20, ringbuf_avg(&ringbuf));

    ringbuf_push(&ringbuf, 50);
    EXPECT_EQ(40, ringbuf_avg(&ringbuf));
}

TEST(Analog, DirtyRingBufferFallsBackToDataScan)
{
    RingBuf ringbuf = {};
    ringbuf.datas[0] = 100;
    ringbuf.datas[1] = 300;
#ifdef OPTIMIZE_MOVING_AVERAGE_FOR_RINGBUF
    ringbuf.sum = 1;
    ringbuf.dirty = true;
#endif

    EXPECT_EQ(200, ringbuf_avg(&ringbuf));
}

TEST(Analog, GrayCodeChannelSelectAcceptsAllConfiguredChannels)
{
    for (uint8_t channel = 0; channel < ANALOG_CHANNEL_MAX; channel++) {
        analog_channel_select(channel);
    }

    SUCCEED();
}

TEST(Filter, HysteresisFilterIgnoresSmallChanges)
{
    constexpr HysteresisFilterValue h = FILTER_HYSTERESIS;
    constexpr HysteresisFilterValue initial = 1000;

    ASSERT_GT(h, 0);
    ASSERT_LT(h + 1, initial);

    HysteresisFilter filter;
    hysteresis_filter_init(&filter, initial);

    EXPECT_EQ(initial, hysteresis_filter(&filter, initial + h, h));
    EXPECT_EQ(initial + 1, hysteresis_filter(&filter, initial + h + 1, h));

    hysteresis_filter_init(&filter, initial);

    EXPECT_EQ(initial, hysteresis_filter(&filter, initial - h, h));
    EXPECT_EQ(initial - 1, hysteresis_filter(&filter, initial - h - 1, h));
}

TEST(Filter, HysteresisFilterHandlesValueBounds)
{
    constexpr HysteresisFilterValue h = FILTER_HYSTERESIS;

    HysteresisFilter filter;
    hysteresis_filter_init(&filter, 0);
    EXPECT_EQ(ANALOG_VALUE_MAX - h, hysteresis_filter(&filter, ANALOG_VALUE_MAX, h));

    hysteresis_filter_init(&filter, ANALOG_VALUE_MAX);
    EXPECT_EQ(h, hysteresis_filter(&filter, 0, h));
}

TEST(Filter, KalmanFilterMovesTowardMeasurements)
{
    KalmanFilter filter;
    kalman_filter_init(&filter, 0.001f, 10.0f, 500.0f, 0.001f);

    const float first = kalman_filter(&filter, 100.0f);
    const float second = kalman_filter(&filter, 100.0f);

    EXPECT_GT(first, 0.0f);
    EXPECT_LE(first, 100.0f);
    EXPECT_GT(second, first);
    EXPECT_LE(second, 100.0f);
}
