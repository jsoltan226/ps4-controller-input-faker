#ifndef EVDEV_MONITOR_H_
#define EVDEV_MONITOR_H_

#include <core/int.h>
#include <core/vector.h>

/* The purpose of this class is to monitor /dev/input
 * for creation or deletion of files (event devices).
 *
 * The state should be checked with `monitor_poll` e.g. in a main loop
 * and creation/deletion of devices should be handled
 * before any action is performed on them. */
struct evdev_monitor {
    struct udev *udev;
    struct udev_monitor *mon;
    i32 fd;
    bool destroyed__;
};

/* Initializes a new /dev/input monitor `o`.
 * Returns 0 on success and non-zero on failure. */
i32 evdev_monitor_init(struct evdev_monitor *o);

/* Read all events from the monitor `mon`
 * and store a new vector with the affected devices' paths (as malloced strings)
 * in `o_created` and `o_deleted`, respectively.
 *
 * If, for example, `o_created` is NULL, file creation events will be ignored.
 * Same goes for `o_deleted`.
 *
 * If an error occurs, the function will return a non-zero value
 * and `*o_created` and `*o_deleted` won't store any results.
 *
 * Otherwise, 0 is returned and the user should free all the strings
 * in both `o_created` and `o_deleted`, as well as the vectors themselves.
 * Obviously this doesn't apply if NULL was passed instead of the vector. */
i32 evdev_monitor_read(struct evdev_monitor *mon,
    VECTOR(char *) *o_created, VECTOR(char *) *o_deleted);

/* Polls the monitor `mon` for any file creation or deletetion events
 * with the timeout `delay_sec` (0 means no waiting, -1 means wait indefinetly)
 *
 * If `o_created` is NULL, file creation events will be ignored.
 * Same goes for `o_deleted`.
 *
 * If an error occurs, the function will return a non-zero value
 * and `*o_created` and `*o_deleted` won't store any results.
 *
 * Otherwise, 0 is returned and the user should free all the strings
 * in both `o_created` and `o_deleted`, as well as the vectors themselves.
 * Obviously this doesn't apply if NULL was passed instead of the vector. */
i32 evdev_monitor_poll_and_read(struct evdev_monitor *mon, i32 delay_sec,
    VECTOR(char *) *o_created, VECTOR(char *) *o_deleted);


/* Destroys the monitor pointed to by `mon`. */
void evdev_monitor_destroy(struct evdev_monitor *mon);

#endif /* EVDEV_MONITOR_H_ */
