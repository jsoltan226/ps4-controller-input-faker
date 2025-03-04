#define _GNU_SOURCE
#include "ptime.h"
#include <core/int.h>
#include <core/log.h>
#include <core/util.h>
#include <time.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#define MODULE_NAME "time"

void p_time(timestamp_t *o)
{
    u_check_params(o != NULL);

    struct timespec ts;
    s_assert(clock_gettime(CLOCK_TAI, &ts) == 0,
        "Failed to get the current time (from CLOCK_TAI): %s",
        strerror(errno));

    o->s = ts.tv_sec;
    o->ns = ts.tv_nsec;
}

void p_time_get_ticks(timestamp_t *o)
{
    u_check_params(o != NULL);

    struct timespec ts;
    s_assert(clock_gettime(CLOCK_MONOTONIC, &ts) == 0,
        "Failed to get the current time (from CLOCK_MONOTONIC): %s",
        strerror(errno));

    o->s = ts.tv_sec;
    o->ns = ts.tv_nsec;
}

i64 p_time_delta_us(const timestamp_t *t0)
{
    if (t0 == NULL) return 0;

    timestamp_t o = { 0 }, t1 = { 0 };
    p_time_get_ticks(&t1);
    timestamp_delta(o, *t0, t1);

    return (o.s * 1000000) + (o.ns / 1000);
}

i64 p_time_delta_ms(const timestamp_t *t0)
{
    if (t0 == NULL) return 0;

    timestamp_t o = { 0 }, t1 = { 0 };
    p_time_get_ticks(&t1);
    timestamp_delta(o, *t0, t1);

    return (o.s * 1000) + (o.ns / 1000000);
}

i64 p_time_delta_s(const timestamp_t *t0)
{
    if (t0 == NULL) return 0;

    timestamp_t o = { 0 }, t1 = { 0 };
    p_time_get_ticks(&t1);
    timestamp_delta(o, *t0, t1);

    return o.s + o.ns / 1000000000;
}

void p_time_nanosleep(const timestamp_t *time)
{
    if (time == NULL) return;

    const struct timespec ts_req = {
        .tv_sec = time->s,
        .tv_nsec = time->ns,
    };
    (void) clock_nanosleep(CLOCK_TAI, 0, &ts_req, NULL);
}

void p_time_usleep(u32 u_seconds)
{
    const struct timespec ts_req = {
        .tv_sec = u_seconds / 1000000,
        .tv_nsec = (u_seconds % 1000000) * 1000
    };
    (void) clock_nanosleep(CLOCK_TAI, 0, &ts_req, NULL);
}

void p_time_msleep(u32 m_seconds)
{
    const struct timespec ts_req = {
        .tv_sec = m_seconds / 1000,
        .tv_nsec = (m_seconds % 1000) * 1000000
    };
    (void) clock_nanosleep(CLOCK_TAI, 0, &ts_req, NULL);
}

void p_time_sleep(u32 seconds)
{
    const struct timespec ts_req = {
        .tv_sec = seconds,
        .tv_nsec = 0
    };
    (void) clock_nanosleep(CLOCK_TAI, 0, &ts_req, NULL);
}
