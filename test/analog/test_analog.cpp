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
    HysteresisFilter filter;
    hysteresis_filter_init(&filter, 100);

    EXPECT_EQ(100, hysteresis_filter(&filter, 101));
    EXPECT_EQ(107, hysteresis_filter(&filter, 110));
    EXPECT_EQ(107, hysteresis_filter(&filter, 105));
    EXPECT_EQ(97, hysteresis_filter(&filter, 94));
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
