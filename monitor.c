#define _GNU_SOURCE
#include "monitor.h"
#include <core/int.h>
#include <core/log.h>
#include <core/util.h>
#include <core/vector.h>
#include <errno.h>
#include <string.h>
#include <poll.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <linux/limits.h>
#include <libudev.h>

#define MODULE_NAME "monitor"

#define DEV_INPUT_DIR "/dev/input"

i32 evdev_monitor_init(struct evdev_monitor *o)
{
    u_check_params(o != NULL);

    o->udev = udev_new();
    if (o->udev == NULL)
        goto_error("Failed to create the udev context");

    o->mon = udev_monitor_new_from_netlink(o->udev, "udev");
    if (o->mon == NULL)
        goto_error("Failed to create the udev monitor");

    i32 ret = udev_monitor_filter_add_match_subsystem_devtype(o->mon,
        "input", NULL);
    if (ret != 0)
        goto_error("Failed to set the filter in udev: %s", strerror(ret));

    ret = udev_monitor_enable_receiving(o->mon);
    if (ret != 0)
        goto_error("Failed to enable udev events: %s", strerror(ret));

    o->fd = udev_monitor_get_fd(o->mon);
    if (o->fd < 0)
        goto_error("Failed to get the udev fd: %s", strerror(o->fd));

    s_log_debug("Initialized a udev monitor with fd %i", o->fd);
    return 0;

err:
    evdev_monitor_destroy(o);
    return 1;
}

i32 evdev_monitor_poll_and_read(struct evdev_monitor *mon, i32 delay_sec,
    VECTOR(char *) *o_created, VECTOR(char *) *o_deleted)
{
    u_check_params(mon != NULL);


    struct pollfd poll_fd = {
        .fd = mon->fd,
        .events = POLLIN
    };
retry_poll:;
    i32 ret = poll(&poll_fd, 1, delay_sec);
    if (ret < 0) {
        if (errno == EINTR) /* Interrupted by signal */
            goto retry_poll;

        s_log_error("Failed to poll on monitor fd: %s", strerror(errno));
        if (o_created != NULL) *o_created = NULL;
        if (o_deleted != NULL) *o_deleted = NULL;
        return 1;
    } else if (ret == 0) {
        /* No events available */
        if (o_created != NULL) *o_created = vector_new(char *);
        if (o_deleted != NULL) *o_deleted = vector_new(char *);
        return 0;
    } else {
        return evdev_monitor_read(mon, o_created, o_deleted);
    }
}

i32 evdev_monitor_read(struct evdev_monitor *mon,
    VECTOR(char *) *o_created, VECTOR(char *) *o_deleted)
{
    u_check_params(mon != NULL);

    VECTOR(char *) created = NULL;
    if (o_created != NULL) created = vector_new(char *);

    VECTOR(char *) deleted = NULL;
    if (o_deleted != NULL) deleted = vector_new(char *);

    struct udev_device *dev = NULL;
    char *duped_path = NULL;
    while (dev = udev_monitor_receive_device(mon->mon), dev != NULL) {
        const char *path = udev_device_get_devnode(dev);
        if (path == NULL) { /* A sysfs entry with no device node */
            udev_device_unref(dev);
            dev = NULL;
            continue;
        }

        /* Strip off the `/dev/input/` prefix */
        if (strncmp(path, "/dev/input/", u_strlen("/dev/input/")))
            goto_error("Invalid evdev path received: %s", path);

        duped_path = strdup(path + u_strlen("/dev/input/"));
        s_assert(duped_path != NULL, "Failed to duplicate string");

        const char *action = udev_device_get_action(dev);
        if (action == NULL)
            goto_error("Failed to get action performed on udev device");

        if (!strcmp(action, "add")) {
            if (created != NULL) vector_push_back(created, duped_path);
            else u_nfree(&duped_path);
        } else if (!strcmp(action, "remove")) {
            if (deleted != NULL) vector_push_back(deleted, duped_path);
            else u_nfree(&duped_path);
        }

        udev_device_unref(dev);
        dev = NULL;
    }

    if (o_created != NULL) *o_created = created;
    if (o_deleted != NULL) *o_deleted = deleted;
    return 0;

err:
    if (duped_path != NULL)
        u_nfree(&duped_path);
    if (dev != NULL) {
        udev_device_unref(dev);
        dev = NULL;
    }
    if (created != NULL) {
        for (u32 i = 0; i < vector_size(created); i++)
            free(&created[i]);
        vector_destroy(&created);
    }
    if (deleted != NULL) {
        for (u32 i = 0; i < vector_size(deleted); i++)
            free(&deleted[i]);
        vector_destroy(&deleted);
    }

    if (o_created != NULL) *o_created = NULL;
    if (o_deleted != NULL) *o_deleted = NULL;
    return 1;
}

void evdev_monitor_destroy(struct evdev_monitor *mon)
{
    if (mon == NULL)
        return;

    s_log_debug("Destroying udev monitor...");
    /* Both udev_..._unref functions always return NULL */
    if (mon->mon != NULL) {
        (void) udev_monitor_unref(mon->mon);
        mon->mon = NULL;
    }
    if (mon->udev != NULL) {
        (void) udev_unref(mon->udev);
        mon->udev = NULL;
    }

    mon->fd = -1;
}
