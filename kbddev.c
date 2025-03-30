#define _GNU_SOURCE
#define KBDDEV_INTERNAL_GUARD__
#include "kbddev.h"
#undef KBDDEV_INTERNAL_GUARD__
#include <core/int.h>
#include <core/log.h>
#include <core/util.h>
#include <errno.h>
#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <linux/input-event-codes.h>

#define MODULE_NAME "kbddev"

#define UINPUT_DEV_PATH "/dev/uinput"
#define UINPUT_DEV_FALLBACK_PATH "/dev/input/uinput"

i32 kbddev_init(kbddev_t *kbddev_p, u16 fake_keypress_keycode)
{
    u_check_params(kbddev_p != NULL);

    /* Open the uinput device */
    struct kbddev ret = {
        .fd = -1,
        .dev_created__ = false,
        .destroyed__ = false
    };
    ret.fd = open(UINPUT_DEV_PATH, O_WRONLY | O_NONBLOCK | O_CLOEXEC);
    if (ret.fd == -1) {
        if (errno == ENOENT) { /* No such file or directory */
            ret.fd = open(UINPUT_DEV_FALLBACK_PATH,
                O_WRONLY | O_NONBLOCK | O_CLOEXEC);
            if (ret.fd == -1) {
                goto_error("Failed to open the uinput device "
                    "using the fallback path (%s): %s",
                    UINPUT_DEV_FALLBACK_PATH, strerror(errno));
            }
        } else {
            goto_error("Failed to open the uinput device (%s): %s",
                UINPUT_DEV_PATH, strerror(errno));
        }
    }

    /* Set the relevant evdev bits */
    if (ioctl(ret.fd, UI_SET_EVBIT, EV_KEY) < 0) {
        goto_error("Failed to set EV_KEY bit on uinput device: %s",
            strerror(errno));
    }
    /*
    for (u32 i = 0; i < u_arr_size(evdev_key_bits); i++) {
        if (ioctl(ret.fd, UI_SET_KEYBIT, evdev_key_bits[i]) == -1) {
            goto_error("Failed to set KEY bit %#x on uinput device: %s",
                evdev_key_bits[i], strerror(errno));
        }
    }
    */
    if (ioctl(ret.fd, UI_SET_KEYBIT, fake_keypress_keycode) == -1)
        goto_error("Failed to set KEY bit %#x on uinput device: %s",
            fake_keypress_keycode, strerror(errno));

    /* Set up the fake device */
    struct uinput_setup setup = ds4_setup;
    if (ioctl(ret.fd, UI_DEV_SETUP, &setup) == -1)
        goto_error("uinput device setup failed: %s", strerror(errno));

    if (ioctl(ret.fd, UI_DEV_CREATE) == -1)
        goto_error("uinput device creation failed: %s", strerror(errno));
    ret.dev_created__ = true;

    s_log_debug("Created a fake keyboard device with fd %i", ret.fd);
    *kbddev_p = ret;
    return 0;
err:
    kbddev_destroy(&ret);
    *kbddev_p = ret;
    return 1;
}

void kbddev_destroy(kbddev_t *kbddev_p)
{
    if (kbddev_p == NULL || kbddev_p->destroyed__)
        return;

    s_log_debug("Destroying fake keyboard device...");
    if (kbddev_p->fd != -1) {
        if (kbddev_p->dev_created__) {
            if (ioctl(kbddev_p->fd, UI_DEV_DESTROY) == -1) {
                s_log_error("Failed to destroy uinput device: %s",
                    strerror(errno));
            }
            kbddev_p->dev_created__ = false;
        }
        close(kbddev_p->fd);
        kbddev_p->fd = -1;
    } else if (kbddev_p->dev_created__)
        s_log_fatal(MODULE_NAME, __func__,
            "The dev_created flag is set on a struct "
            "with an invalid file descriptor; possible memory corruption");

    kbddev_p->destroyed__ = true;
}
