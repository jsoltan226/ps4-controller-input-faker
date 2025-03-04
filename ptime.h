#ifndef P_TIME_H_
#define P_TIME_H_

#include <core/int.h>

typedef struct timestamp {
    i64 s;  /* seconds */
    i64 ns; /* nano-seconds */
} timestamp_t;

/* Store the amount of time elapsed from `t0` to `t1` in `o` */
#define timestamp_delta(o, t0, t1) do {     \
    (o).ns = (t1).ns - (t0).ns;             \
    (o).s = (t1).s - (t0).s;                \
    (o).ns += ((o).ns < 0) * 1000000000L;   \
} while (0)

/* Retrieve the current UNIX time into `o`.
 * Does not guarantee high precision. */
void p_time(timestamp_t *o);

/* Retrieve the value of a system performance counter into `o`.
 * Guarantees high precision, but is not in sync with UTC. */
void p_time_get_ticks(timestamp_t *o);

/* Get the time elapsed since `t0` */
i64 p_time_delta_us(const timestamp_t *t0);
i64 p_time_delta_ms(const timestamp_t *t0);
i64 p_time_delta_s(const timestamp_t *t0);

/* Wait for a given amount of time */
void p_time_nanosleep(const timestamp_t *time);
void p_time_usleep(u32 u_seconds);
void p_time_msleep(u32 m_seconds);
void p_time_sleep(u32 seconds);

#endif /* P_TIME_H_ */
