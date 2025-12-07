/*
 * Copyright (c) 2024 Zhangqi Li (@zhangqili)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef FILTER_H_
#define FILTER_H_

#include "keyboard.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FILTER_DOMAIN_RAW           0
#define FILTER_DOMAIN_NORMALIZED    1

#define FILTER_TYPE_LOW_PASS        0
#define FILTER_TYPE_KALMAN          1

#ifndef FILTER_DOMAIN
#define FILTER_DOMAIN FILTER_DOMAIN_RAW
#endif

#ifndef FILTER_TYPE
#define FILTER_TYPE FILTER_TYPE_LOW_PASS
#endif

#ifndef FILTER_HYSTERESIS
#if FILTER_DOMAIN == FILTER_DOMAIN_RAW
#define FILTER_HYSTERESIS 3
#else
#define FILTER_HYSTERESIS A_NORM(0.01f)
#endif
#endif

// Alpha for lowpass filter, between 0 and 1, higher means more smoothing
#ifndef FILTER_LOWPASS_ALPHA
#define FILTER_LOWPASS_ALPHA 0.5f
#endif

typedef float FilterValue;

typedef struct __HysteresisFilter
{
    FilterValue state;
} HysteresisFilter;

typedef struct __LowpassFilter
{
    FilterValue state;
} LowpassFilter;

typedef struct __KalmanFilter
{
    FilterValue pos;
    FilterValue vel;

    FilterValue p00;
    FilterValue p01;
    FilterValue p10;
    FilterValue p11;

    FilterValue dt;
    FilterValue Q_pos;
    FilterValue Q_vel;
    FilterValue R;
} KalmanFilter;

void filter_reset(void);

static inline void hysteresis_filter_init(HysteresisFilter *filter, FilterValue initial_state)
{
    filter->state = initial_state;
}

static inline FilterValue hysteresis_filter(HysteresisFilter *filter, FilterValue value)
{
    if (value - FILTER_HYSTERESIS > filter->state)
        filter->state = value - FILTER_HYSTERESIS;
    if (value + FILTER_HYSTERESIS < filter->state)
        filter->state = value + FILTER_HYSTERESIS;
    return filter->state;
}

static inline void lowpass_filter_init(LowpassFilter *filter, FilterValue initial_state)
{
    filter->state = initial_state;
}

static inline FilterValue lowpass_filter(LowpassFilter *filter, FilterValue value)
{
    filter->state = filter->state * FILTER_LOWPASS_ALPHA + value * (1 - FILTER_LOWPASS_ALPHA);
    return filter->state;
}

static inline void kalman_filter_init(KalmanFilter *filter, FilterValue dt, FilterValue Q_pos, FilterValue Q_vel, FilterValue R)
{
    filter->pos = 0;
    filter->vel = 0;
    filter->p00 = 1;
    filter->p01 = 0;
    filter->p10 = 0;
    filter->p11 = 1;
    filter->dt = dt;
    filter->Q_pos = Q_pos;
    filter->Q_vel = Q_vel;
    filter->R = R;
}

static inline FilterValue kalman_filter(KalmanFilter *filter, FilterValue value)
{
    // x_pred = A * x_prev
    FilterValue pos_pred = filter->pos + filter->vel * filter->dt;
    FilterValue vel_pred = filter->vel;

    // P_pred = A * P_prev * A^T + Q
    FilterValue p00_temp = filter->p00 + filter->p10 * filter->dt;
    FilterValue p01_temp = filter->p01 + filter->p11 * filter->dt;
    
    FilterValue p00_pred = p00_temp + p01_temp * filter->dt + filter->Q_pos;
    FilterValue p01_pred = p01_temp;
    FilterValue p10_pred = filter->p10 + filter->p11 * filter->dt;
    FilterValue p11_pred = filter->p11 + filter->Q_vel;

    // S = H * P_pred * H^T + R
    FilterValue S = p00_pred + filter->R;
    
    // K = P_pred * H^T * inv(S). 
    // K = [p00_pred / S, p10_pred / S]^T
    FilterValue K_pos = p00_pred / S;
    FilterValue K_vel = p10_pred / S;

    // x = x_pred + K * (z - H * x_pred)
    FilterValue y = value - pos_pred;
    
    filter->pos = pos_pred + K_pos * y;
    filter->vel = vel_pred + K_vel * y;

    // P = (I - K * H) * P_pred
    FilterValue p00_new = (1.0f - K_pos) * p00_pred;
    FilterValue p01_new = (1.0f - K_pos) * p01_pred;
    FilterValue p10_new = p10_pred - K_vel * p00_pred;
    FilterValue p11_new = p11_pred - K_vel * p01_pred;

    filter->p00 = p00_new; filter->p01 = p01_new;
    filter->p10 = p10_new; filter->p11 = p11_new;

    return filter->pos;
}

#if FILTER_TYPE == FILTER_TYPE_LOW_PASS
typedef LowpassFilter Filter;
#elif FILTER_TYPE == FILTER_TYPE_KALMAN
typedef KalmanFilter Filter;
#endif

#ifdef __cplusplus
}
#endif

#endif