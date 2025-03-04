#define _GNU_SOURCE
#include "evdev.h"
#include "jsdev.h"
#include <core/int.h>
#include <core/log.h>
#include <core/util.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/joystick.h>

#define MODULE_NAME "jsdev"

#define DEV_INPUT_DIR "/dev/input"

static i32 get_evdev_from_js(const char *js_rel_path,
    char *o_evdev_path, u32 evdev_path_buf_size);

i32 joystick_dev_load(struct joystick_dev *jsdev,
    const char *rel_path, bool grab_evdev)
{
    u_check_params(jsdev != NULL && rel_path != NULL);
    memset(jsdev, 0, sizeof(struct joystick_dev));
    jsdev->initialized_ = true;

    (void) snprintf(jsdev->path, u_FILEPATH_MAX,
        "%s/%s", DEV_INPUT_DIR, rel_path);
    jsdev->fd = open(jsdev->path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (jsdev->fd == -1)
        goto_error("Failed to open device %s: %s",
            jsdev->path, strerror(errno));

    if (ioctl(jsdev->fd, JSIOCGNAME(JOYSTICK_NAME_MAX_LEN), jsdev->name) < 0)
        s_log_warn("Failed to get joystick device name: %s", strerror(errno));
#ifndef CGD_BUILDTYPE_RELEASE
    else
        s_log_debug("New joystick device: %s", jsdev->name);
#endif /* CGD_BUILDTYPE_RELEASE */

    /* Load the evdev that feeds into this joystick device */
    char evdev_path[u_FILEPATH_MAX] = { 0 };
    if (get_evdev_from_js(rel_path, evdev_path, u_FILEPATH_MAX))
        goto_error("Failed to determine the path to the joystick device's "
            "\"%s\" (%s) corresponsing evdev", jsdev->name, jsdev->path);

    s_assert(!strncmp(evdev_path, "/dev/input/", u_strlen("/dev/input/")),
        "Invalid event device path \"%s\"", evdev_path);
    if (evdev_load(evdev_path + u_strlen("/dev/input/"),
        &jsdev->evdev, EVDEV_TYPE_AUTO))
    {
        goto_error("Failed to load the joystick's (\"%s\" - %s) "
            "event device (%s)", jsdev->name, jsdev->path, evdev_path);
    }

    if (grab_evdev) {
        if (joystick_grab_evdev(jsdev))
            goto err; /* Any errors are logged by `joystick_grab_evdev` */
    }

    return 0;

err:
    joystick_dev_destroy(jsdev);
    return 1;
}

i32 joystick_grab_evdev(struct joystick_dev *jsdev)
{
    u_check_params(jsdev != NULL && jsdev->initialized_ &&
        jsdev->evdev.initialized_);
    if (jsdev->grabbed) {
        s_log_error("Joystick's (\"%s\" - %s) event device (\"%s\" - %s) "
            "already grabbed - not doing anything!",
            jsdev->name, jsdev->path, jsdev->evdev.name, jsdev->evdev.path);
        return 0;
    }

    /* Get exclusive access to the event device */
    if (ioctl(jsdev->evdev.fd, EVIOCGRAB, 1) < 0) {
        s_log_error("Failed to grab joystick's (\"%s\" - %s) "
            "event device (\"%s\" - %s): %s",
            jsdev->name, jsdev->path,
            jsdev->evdev.name, jsdev->evdev.path,
            strerror(errno)
        );
        return 1;
    }
    jsdev->grabbed = true;

    return 0;
}

i32 joystick_release_evdev(struct joystick_dev *jsdev)
{
    u_check_params(jsdev != NULL && jsdev->initialized_ &&
        jsdev->evdev.initialized_);

    if (!jsdev->grabbed) {
        s_log_error("Joystick's (\"%s\" - %s) event device (\"%s\" - %s) "
            "already released - not doing anything!",
            jsdev->name, jsdev->path, jsdev->evdev.name, jsdev->evdev.path);
        return 0;
    }

    /* Release the evdev */
    if (ioctl(jsdev->evdev.fd, EVIOCGRAB, 0)) {
        s_log_error("Failed to release joystick's (\"%s\" - %s) "
            "event device (\"%s\" - %s): %s",
            jsdev->name, jsdev->path,
            jsdev->evdev.name, jsdev->evdev.path,
            strerror(errno)
        );
        return 1;
    }
    jsdev->grabbed = false;

    return 0;
}

void joystick_read_event(struct joystick_dev *jsdev)
{
    u_check_params(jsdev != NULL && jsdev->initialized_ && jsdev->fd >= 0);

    struct js_event ev = { 0 };
    i32 n_bytes_read = 0;
    while (n_bytes_read = read(jsdev->fd, &ev, sizeof(ev)), n_bytes_read > 0) {
        if (n_bytes_read == -1) {
            if (errno == EINTR)
                continue; /* Interrupted by signal; try again */

            s_log_error("Failed to read from joystick device \"%s\" (%s): %s",
                jsdev->name, jsdev->path, strerror(errno));
            return;
        } else if (n_bytes_read == 0) {
            return; /* No events left to read */
        } else if (n_bytes_read != sizeof(struct js_event)) {
            s_log_fatal(MODULE_NAME, __func__,
                "Read %i bytes from joystick device \"%s\" (%s), "
                " expected size is %i. "
                "The linux input driver is probably broken...",
                n_bytes_read, jsdev->name, jsdev->path, sizeof(struct js_event)
            );
        } else {
            s_log_info("Device \"%s\" (%s) received a new event %u",
                jsdev->name, jsdev->path, ev.type);
        }
    }
}

void joystick_dev_destroy(struct joystick_dev *jsdev)
{
    if (jsdev == NULL || !jsdev->initialized_)
        return;

    if (jsdev->grabbed) {
        (void) joystick_release_evdev(jsdev);
        /* Any errors are printed by `joystick_release_evdev`;
         * no need to do that here */
    }
    jsdev->grabbed = false;

    if (jsdev->fd != -1) {
        close(jsdev->fd);
        jsdev->fd = -1;
    }
    memset(jsdev->path, 0, u_FILEPATH_MAX);
    memset(jsdev->name, 0, u_FILEPATH_MAX);
}

static i32 get_evdev_from_js(const char *js_rel_path,
    char *o_evdev_path, u32 evdev_path_buf_size)
{
    memset(o_evdev_path, 0, u_FILEPATH_MAX);

    char sysfs_path[u_FILEPATH_MAX] = { 0 };
    (void) snprintf(sysfs_path, u_FILEPATH_MAX,
        "/sys/class/input/%s/device/", js_rel_path);

    DIR *dir = opendir(sysfs_path);
    if (dir == NULL) {
        s_log_error("Failed to open directory \"%s\": %s",
            sysfs_path, strerror(errno));
        return -1;
    }

    struct dirent *entry = NULL;
    while (entry = readdir(dir), entry != NULL) {
        if (!strncmp(entry->d_name, "event", u_strlen("event"))) {
            (void) snprintf(o_evdev_path, evdev_path_buf_size,
                "/dev/input/%s", entry->d_name);
            closedir(dir);
            return 0;
        }
    }

    s_log_error("No event devices matching the joystick \"%s\" were found.",
        js_rel_path);
    closedir(dir);
    return 1;
}
