#include <linux/input-event-codes.h>
#define _GNU_SOURCE
#include "jsdev.h"
#include "evdev.h"
#include "monitor.h"
#include "keyboard.h"
#include "key-codes.h"
#include <core/log.h>
#include <core/util.h>
#include <core/vector.h>
#include <keyboard-evdev.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include <sys/cdefs.h>
#include <sys/poll.h>
#include <unistd.h>
#include <signal.h>
#include <linux/input.h>

#define MODULE_NAME "main"

#define CONTROLLER_DEV "event31"
#define MOTION_SENSORS_DEV "event256"
#define TOUCHPAD_DEV "event257"

#define KEYBOARD_DEV "event3"

#define JOYSTICK_DEV "js0"

static void __attribute_maybe_unused__ controller_test(void);
static void __attribute_maybe_unused__ keyboard_test(void);
static void __attribute_maybe_unused__ hotplug_test(void);
static void __attribute_maybe_unused__ joystick_test(void);

static void signal_handler(i32 sig_num);

static void read_dev(i32 fd);
static void handle_ev(struct input_event *ev, struct evdev *kb_evdev);

atomic_flag running = ATOMIC_FLAG_INIT;

int main(int argc, char **argv)
{
    s_configure_log(LOG_DEBUG, stdout, stderr);

    struct sigaction sa;
    sa.sa_handler = signal_handler;
    sa.sa_flags = 0;
    sigfillset(&sa.sa_mask);
    if (sigaction(SIGUSR1, &sa, NULL))
        goto_error("Failed to register SIGUSR1 handler: %s", strerror(errno));
    if (sigaction(SIGTERM, &sa, NULL))
        goto_error("Failed to register SIGTERM handler: %s", strerror(errno));
    if (sigaction(SIGINT, &sa, NULL))
        goto_error("Failed to register SIGINT handler: %s", strerror(errno));

    //controller_test();
    //keyboard_test();
    hotplug_test();
    //joystick_test();
    return EXIT_SUCCESS;

err:
    return EXIT_FAILURE;
}

static void __attribute_maybe_unused__ controller_test(void)
{
    struct evdev controller = { 0 };
    if (evdev_load(CONTROLLER_DEV, &controller, EVDEV_PS4_CONTROLLER))
        goto_error("Failed to load the main controller device.");
    s_log_debug("Opened controller as fd %i", controller.fd);

    struct evdev motion_sensors = { 0 };
    if (evdev_load(MOTION_SENSORS_DEV, &motion_sensors, EVDEV_PS4_CONTROLLER_MOTION_SENSOR))
        goto_error("Failed to load the motion sensors device.");
    s_log_debug("Opened sensors as fd %i", motion_sensors.fd);

    struct evdev touchpad = { 0 };
    if (evdev_load(TOUCHPAD_DEV, &touchpad, EVDEV_PS4_CONTROLLER_TOUCHPAD))
        goto_error("Failed to load the touchpad device.");
    s_log_debug("Opened touchpad as fd %i", touchpad.fd);

    struct pollfd poll_fds[] = {
        (struct pollfd){ .fd = controller.fd, .events = POLLIN },
        (struct pollfd){ .fd = motion_sensors.fd, .events = POLLIN },
        (struct pollfd){ .fd = touchpad.fd, .events = POLLIN },
    };

    (void) atomic_flag_test_and_set(&running);
    while (atomic_flag_test_and_set(&running)) {
        i32 ret = poll(poll_fds, u_arr_size(poll_fds), 1);
        if (ret == -1) {
            if (errno == EINTR) /* Interrupted by signal */
                continue;
            else if (errno == EAGAIN)
                break;
            s_log_error("Failed to poll on controller: %s", strerror(errno));
            atomic_flag_clear(&running);
            break;
        } else if (ret > 0) {
            for (u32 i = 0; i < u_arr_size(poll_fds); i++) {
                if (poll_fds[i].revents & POLLERR ||
                    poll_fds[i].revents & POLLNVAL ||
                    poll_fds[i].revents & POLLHUP)
                {
                    s_log_debug("fd %i disconnected, exiting...",
                        poll_fds[i].fd);
                    atomic_flag_clear(&running);
                    break;
                } else if (poll_fds[i].revents & POLLIN) {
                    read_dev(poll_fds[i].fd);
                }
            }
        }
    }

err:
    if (touchpad.fd != -1) close(touchpad.fd);
    if (motion_sensors.fd != -1) close(motion_sensors.fd);
    if (controller.fd != -1) close(controller.fd);
}

static void __attribute_maybe_unused__ keyboard_test(void)
{
    struct keyboard_evdev kb = { 0 };
    kb.kbdevs = vector_new(struct evdev);
    kb.poll_fds = vector_new(struct pollfd);
    struct evdev tmp = { 0 };
    if (evdev_load(KEYBOARD_DEV, &tmp, EVDEV_KEYBOARD))
        goto_error("Failed to load the keyboard device");
    vector_push_back(kb.kbdevs, tmp);
    vector_push_back(kb.poll_fds, (struct pollfd) {
        .events = POLLIN,
        .fd = tmp.fd
    });

    pressable_obj_t keys[P_KEYBOARD_N_KEYS] = { 0 };

    while (!keys[KB_KEYCODE_Q].up)
        evdev_keyboard_update_all_keys(&kb, keys);

    s_log_info("Pressed 'Q', exiting...");

err:
    evdev_keyboard_destroy(&kb);
}

static void signal_handler(i32 sig_num)
{
    if (sig_num == SIGUSR1 || sig_num == SIGINT || sig_num == SIGTERM)
        atomic_flag_clear(&running);
}

static void read_dev(i32 fd)
{
    //s_log_debug("Received events on fd %i", fd);
    u8 buf[4096];
    while (read(fd, buf, 4096) > 0)
        ;
}

static void __attribute_maybe_unused__ hotplug_test(void)
{
    VECTOR(struct evdev) devices =
        evdev_find_and_load_devices(EVDEV_TYPE_AUTO);
    if (devices == NULL)
        goto_error("Failed to load event devices");
    for (u32 i = 0; i < vector_size(devices); i++) {
        s_log_info("New device: \"%s\" (%s), type %s",
            devices[i].name[0] ? devices[i].name : "n/a",
            devices[i].path, evdev_type_strings[devices[i].type]
        );
    }
    VECTOR(struct pollfd) poll_fds = vector_new(struct pollfd);
    for (u32 i = 0; i < vector_size(devices); i++) {
        vector_push_back(poll_fds, (struct pollfd) {
            .fd = devices[i].fd,
            .events = POLLIN
        });
    }

    VECTOR(char *) created = NULL;
    VECTOR(char *) deleted = NULL;

    struct evdev_monitor mon;
    if (evdev_monitor_init(&mon))
        goto_error("Failed to initialize the monitor.");

    (void) atomic_flag_test_and_set(&running);
    while (atomic_flag_test_and_set(&running)) {
        if (evdev_monitor_poll_and_read(&mon, 1, &created, &deleted))
            goto_error("Monitor poll failed");

        for (u32 i = 0; i < vector_size(created); i++) {
            struct evdev new_dev = { 0 };
            if (strncmp(created[i], "event", u_strlen("event"))) {
                /* Not an evdev */
            } else if (evdev_load(created[i], &new_dev, EVDEV_TYPE_AUTO)) {
                //s_log_debug("Failed to load event device %s", created[i]);
            } else {
                s_log_info("New device: \"%s\" (%s), type %s",
                    new_dev.name[0] ? new_dev.name : "n/a",
                    new_dev.path, evdev_type_strings[new_dev.type]
                );
                vector_push_back(devices, new_dev);
                vector_push_back(poll_fds, (struct pollfd) {
                    .fd = new_dev.fd,
                    .events = POLLIN
                });
            }
            u_nfree(&created[i]);
        }
        vector_destroy(&created);

        for (u32 i = 0; i < vector_size(deleted); i++) {
            for (u32 j = 0; j < vector_size(devices); j++) {
                s_assert(!strncmp(devices[j].path,
                    "/dev/input/", u_strlen("/dev/input/")),
                    "Invalid event device path \"%s\"", devices[j].path);
                if (!strcmp(deleted[i],
                        devices[j].path + u_strlen("/dev/input/"))
                ) {
                    s_log_info("Removed device: %s", deleted[i]);
                    evdev_destroy(&devices[j]);
                    vector_erase(devices, j);
                    vector_erase(poll_fds, j);
                    break;
                }
            }
            u_nfree(&deleted[i]);
        }
        vector_destroy(&deleted);

        i32 ret = poll(poll_fds, vector_size(poll_fds), 0);
        if (ret < 0) {
            if (errno == EINTR) /* Interrupted by signal */
                continue;
            goto_error("Failed to poll on device fds: %s", strerror(errno));
        } else if (ret == 0)
            continue; /* No events to be read */

        for (u32 i = 0; i < vector_size(poll_fds); i++) {
            if (poll_fds[i].revents & POLLERR ||
                poll_fds[i].revents & POLLHUP)
            {
                /* Device disconnected */
            } else if (poll_fds[i].revents & POLLIN) {
                if (devices[i].type != EVDEV_PS4_CONTROLLER &&
                    devices[i].type != EVDEV_PS4_CONTROLLER_TOUCHPAD)
                    continue;

                struct input_event ev;
                i32 n_bytes_read = 0;
                do {
                    n_bytes_read = read(poll_fds[i].fd, &ev, sizeof(ev));
                    if (n_bytes_read == -1 && errno == EINTR) {
                        continue; /* Interrupted by signal, try again */
                    } else if (n_bytes_read == -1) {
                        goto_error("Failed to read from fd %i: %s",
                            poll_fds[i].fd, strerror(errno));
                    } else if (n_bytes_read == 0) {
                        continue; /* No more events left to read */
                    } else if (n_bytes_read > 0 && n_bytes_read != sizeof(ev)) {
                        s_log_fatal(MODULE_NAME, __func__,
                            "Read %i bytes from event device, expected %i. "
                            "The linux input driver is probably broken...",
                            n_bytes_read, sizeof(struct input_event)
                        );
                    } else {
                        struct evdev *kb_evdev = NULL;
                        for (u32 i = 0; i < vector_size(devices); i++) {
                            if (devices[i].type == EVDEV_KEYBOARD) {
                                kb_evdev = &devices[i];
                                break;
                            }
                        }
                        if (kb_evdev == NULL)
                            goto_error("No keyboards attached!");

                        handle_ev(&ev, kb_evdev);
                    }
                } while (n_bytes_read > 0);
            }
        }
    }

err:
    for (u32 i = 0; i < vector_size(devices); i++)
        evdev_destroy(&devices[i]);
    vector_destroy(&poll_fds);
    vector_destroy(&devices);
    evdev_monitor_destroy(&mon);
}

static void __attribute_maybe_unused__ joystick_test(void)
{
    struct joystick_dev jsdev = { 0 };
    if (joystick_dev_load(&jsdev, JOYSTICK_DEV, true))
        goto_error("Failed to load joystick device");

    struct pollfd poll_fd = { .fd = jsdev.fd, .events = POLLIN };
    (void) atomic_flag_test_and_set(&running);
    while (atomic_flag_test_and_set(&running)) {
        i32 ret = poll(&poll_fd, 1, 1);
        if (ret == -1 && errno == EINTR)
            continue; /* Interrupted by signal */
        else if (ret == -1)
            goto_error("Failed to poll on joystick device \"%s\" (%s): %s",
                jsdev.name, jsdev.path, strerror(errno));
        else if (ret == 0)
            continue; /* No events ready to be read */
        else if (poll_fd.events & POLLIN)
            joystick_read_event(&jsdev);
    }

err:
    joystick_dev_destroy(&jsdev);
}

static void handle_ev(struct input_event *ev, struct evdev *kb_evdev)
{
    if (!((ev->type == EV_KEY) ||
        (ev->type == EV_ABS &&
         (ev->code == ABS_HAT0X || ev->code == ABS_HAT0Y))
    ))
        return;

    struct timeval time;
    gettimeofday(&time, NULL);
    struct input_event press_ev = {
        .type = EV_KEY,
        .code = KEY_F21,
        .value = 1,
        .time = time,
    };
    if (write(kb_evdev->fd, &press_ev, sizeof(struct input_event)) < 0) {
        s_log_error("Failed to write to the evdev \"%s\" (%s): %s",
            kb_evdev->name, kb_evdev->path, strerror(errno));
    }

    gettimeofday(&time, NULL);
    struct input_event release_ev = {
        .type = EV_KEY,
        .code = KEY_F21,
        .value = 0,
        .time = time,
    };
    if (write(kb_evdev->fd, &release_ev, sizeof(struct input_event)) < 0) {
        s_log_error("Failed to write to the evdev \"%s\" (%s): %s",
            kb_evdev->name, kb_evdev->path, strerror(errno));
    }

    gettimeofday(&time, NULL);
    struct input_event syn_ev = {
        .type = EV_SYN,
        .code = SYN_REPORT,
        .value = 0,
        .time = time,
    };
    if (write(kb_evdev->fd, &syn_ev, sizeof(struct input_event)) < 0) {
        s_log_error("Failed to write to the evdev \"%s\" (%s): %s",
            kb_evdev->name, kb_evdev->path, strerror(errno));
    }
}
