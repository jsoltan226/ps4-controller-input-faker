#define _GNU_SOURCE
#include "cfg.h"
#define P_INTERNAL_GUARD__
#include "evdev.h"
#undef P_INTERNAL_GUARD__
#include "kbddev.h"
#include "monitor.h"
#include <core/int.h>
#include <core/log.h>
#include <core/util.h>
#include <core/vector.h>
#include <core/buildtype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include <signal.h>
#include <poll.h>
#include <unistd.h>
#include <linux/input.h>
#include <linux/input-event-codes.h>

#define MODULE_NAME "main"

static i32 init_signal_handler(void);
static void signal_handler(i32 sig_num);
static atomic_flag running = ATOMIC_FLAG_INIT;

static i32 handle_monitor_event(struct evdev_monitor *mon,
    VECTOR(struct evdev) *devices, VECTOR(struct pollfd) *poll_fds);

static i32 handle_device_event(struct evdev *dev, i32 kbddev_fd,
    u16 fake_keypress_keycode);
static i32 write_fake_event(i32 fd, u16 key_code);

#define pollfd_disconnected(pollfd) \
    (pollfd.revents & POLLERR       \
    || pollfd.revents & POLLHUP     \
    || pollfd.revents & POLLNVAL)

static void handle_fd_disconnect(VECTOR(struct evdev) *devices,
    VECTOR(struct pollfd) *poll_fds, u32 device_index);

static const char *buildtype = NULL;

int main(int argc, char **argv)
{
    (void) argc;
    (void) argv;
    if (buildtype == NULL) buildtype = get_cgd_buildtype__();

    i32 ret = EXIT_FAILURE;
    s_configure_log(LOG_INFO, stdout, stderr);

    struct cfg cfg = { 0 };
    if (read_config(&cfg)) /* On failure, default values will be used */
        s_log_warn("Couldn't read the config properly");
    s_set_log_level(cfg.log_level);

    if (init_signal_handler())
        goto_error("Failed to initialize the signal handler. Stop.");

    kbddev_t fake_keyboard = { 0 };
    if (kbddev_init(&fake_keyboard, cfg.fake_keypress_keycode))
        goto_error("Couldn't initialize the fake keyboard device. Stop.");

    VECTOR(struct evdev) devices =
        evdev_find_and_load_devices(EVDEV_MASK_PS4_CONTROLLER);
    if (devices == NULL)
        goto_error("Error while loading active event devices. Stop.");
    s_log_info("Loaded %u event device(s)", vector_size(devices));

    struct evdev_monitor mon = { 0 };
    if (evdev_monitor_init(&mon))
        goto_error("Failed to initialize the evdev monitor. Stop.");

    VECTOR(struct pollfd) global_poll_fds = vector_new(struct pollfd);
    vector_reserve(global_poll_fds, 1 + vector_size(devices));
    /* Init the monitor pollfd */
    vector_push_back(global_poll_fds, (struct pollfd) {
        .fd = mon.fd,
        .events = POLLIN,
    });

    /* Init the device pollfds */
    for (u32 i = 0; i < vector_size(devices); i++) {
        vector_push_back(global_poll_fds, (struct pollfd) {
            .fd = devices[i].fd,
            .events = POLLIN,
        });
    }

    (void) atomic_flag_test_and_set(&running);
    while (atomic_flag_test_and_set(&running)) {
        /* Block until either a monitor or device event occurs */
        i32 ret = poll(global_poll_fds, vector_size(global_poll_fds), -1);
        if (ret == -1) {
            if (errno == EINTR) { /* Interrupted by signal, try again */
                continue;
            } else {
                goto_error("Failed to poll on monitor and devices: %s",
                    strerror(errno));
            }
        }

        i32 n_handled = 0;
        if (n_handled >= ret) continue;

        /* Check the monitor fd */
        if (global_poll_fds[0].revents & POLLNVAL) {
            /* Something like this should never happen */
            s_log_fatal(MODULE_NAME, __func__,
                "The monitor device file descriptor became invalid");
        } else if (global_poll_fds[0].revents & POLLIN) {
            if (handle_monitor_event(&mon, &devices, &global_poll_fds))
                goto_error("Failed to handle monitor event. Stop.");

            n_handled++;
        }
        if (n_handled >= ret) continue;

        /* Check the device fds */
        for (u32 i = 1; i < vector_size(global_poll_fds); i++) {
            if (pollfd_disconnected(global_poll_fds[i])) {
                handle_fd_disconnect(&devices, &global_poll_fds, i - 1);
            } else if (global_poll_fds[i].revents & POLLIN) {
                handle_device_event(&devices[i - 1], fake_keyboard.fd,
                    cfg.fake_keypress_keycode);
            }
            n_handled++;
            if (n_handled >= ret)
                break;
        }
    }

    s_log_debug("Exited from the main loop, cleaning up...");
    ret = EXIT_SUCCESS;
err:
    atomic_flag_clear(&running);
    vector_destroy(&global_poll_fds);
    evdev_monitor_destroy(&mon);
    evdev_list_destroy(&devices);
    kbddev_destroy(&fake_keyboard);
    s_log_info("Cleanup OK, exiting with code %i", ret);
    return ret;
}

static i32 init_signal_handler(void)
{
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

    return 0;
err:
    return 1;
}

static void signal_handler(i32 sig_num)
{
    if (sig_num == SIGUSR1 || sig_num == SIGINT || sig_num == SIGTERM)
        atomic_flag_clear(&running);
}

static i32 handle_monitor_event(struct evdev_monitor *mon,
    VECTOR(struct evdev) *devices, VECTOR(struct pollfd) *poll_fds)
{
    VECTOR(char *) created = NULL;
    VECTOR(char *) deleted = NULL;
    if (evdev_monitor_read(mon, &created, &deleted))
        goto_error("Evdev monitor read failed");

    for (u32 i = 0; i < vector_size(created); i++) {
        struct evdev new_dev = { 0 };
        if (strncmp(created[i], "event", u_strlen("event"))) {
            /* Not an evdev */
        } else if (evdev_load(created[i], &new_dev, EVDEV_MASK_PS4_CONTROLLER))
            ;//s_log_debug("Failed to load event device %s", created[i]);
        else {
            s_log_info("New device: \"%s\" (%s), type %s",
                new_dev.name[0] ? new_dev.name : "n/a",
                new_dev.path, evdev_type_strings[new_dev.type]
            );
            vector_push_back((*devices), new_dev);
            vector_push_back((*poll_fds), (struct pollfd) {
                .fd = new_dev.fd,
                .events = POLLIN
            });
        }
        u_nfree(&created[i]);
    }
    vector_destroy(&created);

    for (u32 i = 0; i < vector_size(deleted); i++) {
        for (u32 j = 0; j < vector_size(*devices); j++) {
            s_assert(!strncmp((*devices)[j].path,
                "/dev/input/", u_strlen("/dev/input/")),
                "Invalid event device path \"%s\"", (*devices)[j].path);
            if (!strcmp(deleted[i],
                    (*devices)[j].path + u_strlen("/dev/input/"))
            ) {
                s_log_info("Removed device: %s", deleted[i]);
                evdev_destroy(&((*devices)[j]));
                vector_erase((*devices), j);
                /* Skip the first element (the monitor fd) */
                vector_erase((*poll_fds), j + 1);
                break;
            }
        }
        u_nfree(&deleted[i]);
    }
    vector_destroy(&deleted);

    return 0;

err:
    if (created != NULL) {
        for (u32 i = 0; i < vector_size(created); i++)
            u_nfree(&created[i]);
        vector_destroy(&created);
    }
    if (deleted != NULL) {
        for (u32 i = 0; i < vector_size(deleted); i++)
            u_nfree(&created[i]);
        vector_destroy(&created);
    }

    return 1;
}

static i32 handle_device_event(struct evdev *dev, i32 kbddev_fd,
    u16 fake_keypress_keycode)
{
    struct input_event ev;
    i32 n_bytes_read = 0;

    do {
        n_bytes_read = read(dev->fd, &ev, sizeof(ev));
        if (n_bytes_read == -1 && errno == EINTR) {
            continue; /* Interrupted by signal, try again */
        } else if (n_bytes_read == -1 && errno == EAGAIN) {
            break; /* Non-blocking read would block - no events left */
        } else if (n_bytes_read == -1) {
            s_log_error("Failed to read from fd %i: %s",
                dev->fd, strerror(errno));
            return 1;
        } else if (n_bytes_read == 0) {
            continue; /* No more events left to read */
        } else if (n_bytes_read > 0 && n_bytes_read != sizeof(ev)) {
            s_log_fatal(MODULE_NAME, __func__,
                "Read %i bytes from event device, expected %i. "
                "The linux input driver is probably broken...",
                n_bytes_read, sizeof(struct input_event)
            );
        } else {
            const bool is_key_press = (ev.type == EV_KEY ||
                (ev.type == EV_ABS &&
                    (ev.code == ABS_HAT0X || ev.code == ABS_HAT0Y)
                )
            );
            if (!is_key_press)
                continue;

            if (write_fake_event(kbddev_fd, fake_keypress_keycode))
                return 1;
        }
    } while (n_bytes_read > 0);

    return 0;
}

static i32 write_fake_event(i32 fd, u16 key_code)
{
    /* Key down */
    struct timeval time;
    (void) gettimeofday(&time, NULL);
    struct input_event press_ev = {
        .type = EV_KEY,
        .code = key_code,
        .value = 1,
        .time = time,
    };
    if (write(fd, &press_ev, sizeof(struct input_event)) < 0) {
        s_log_error("Failed to write fake event to fd %i: %s",
            fd, strerror(errno));
        return 1;
    }

    /* Key up */
    (void) gettimeofday(&time, NULL);
    struct input_event release_ev = {
        .type = EV_KEY,
        .code = key_code,
        .value = 0,
        .time = time,
    };
    if (write(fd, &release_ev, sizeof(struct input_event)) < 0) {
        s_log_error("Failed to write fake event to fd %i: %s",
            fd, strerror(errno));
        return 1;
    }

    /* SYN report */
    (void) gettimeofday(&time, NULL);
    struct input_event syn_ev = {
        .type = EV_SYN,
        .code = SYN_REPORT,
        .value = 0,
        .time = time,
    };
    if (write(fd, &syn_ev, sizeof(struct input_event)) < 0) {
        s_log_error("Failed to write fake event to fd %i: %s",
            fd, strerror(errno));
        return 1;
    }

    return 0;
}

static void handle_fd_disconnect(VECTOR(struct evdev) *devices,
    VECTOR(struct pollfd) *poll_fds, u32 device_index)
{
    /* "di" - device index, "pi" - pollfd index */
    const u32 di = device_index;
    const u32 pi = device_index + 1; /* skip the monitor pollfd */

    /* Some kind of error occured on the fd
     * (this usually happens when the device is normally disconnected,
     * so nothing to worry about really) */
    s_log_debug("vector_size(poll_fds) = %u", vector_size(*poll_fds));
    if ((*poll_fds)[pi].revents & POLLERR) {
        s_log_info("Error on file descriptor %i (device %s - \"%s\"), "
            "disconnecting...",
            (*poll_fds)[pi].fd, (*devices)[di].path, (*devices)[di].name);
    }
    /* The device just disconnected, nothing super unusual */
    if ((*poll_fds)[pi].revents & POLLHUP) {
        s_log_info("File descriptor %i (device %s - \"%s\") disconnected",
            (*poll_fds)[pi].fd, (*devices)[di].path, (*devices)[di].name);
    }
    /* The fd is invalid (most likely a race condition/use-after-free) */
    if ((*poll_fds)[pi].revents & POLLNVAL) {
        s_log_fatal(MODULE_NAME, __func__,
            "File descriptor %i (device %s - \"%s\") became invalid",
            (*poll_fds)[pi].fd, (*devices)[di].path, (*devices)[di].name);
    }


    evdev_destroy(&((*devices)[di]));
    vector_erase((*devices), di);
    /* Skip the first element (the monitor fd) */
    vector_erase((*poll_fds), pi);
}
