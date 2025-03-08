#define _GNU_SOURCE
#include "keyboard.h"
#include <core/log.h>
#include <core/int.h>
#include <core/util.h>
#include <core/pressable-obj.h>
#include <core/vector.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/poll.h>
#include <sys/ioctl.h>
#include <linux/limits.h>
#include <linux/input.h>
#include <linux/input-event-codes.h>
#define P_INTERNAL_GUARD__
#include "keyboard-evdev.h"
#undef P_INTERNAL_GUARD__
#define P_INTERNAL_GUARD__
#include "evdev.h"
#undef P_INTERNAL_GUARD__

#define MODULE_NAME "keyboard-evdev"

static void read_keyevents_from_evdev(i32 fd,
    pressable_obj_t keys[P_KEYBOARD_N_KEYS],
    bool updated_keys[P_KEYBOARD_N_KEYS]);

i32 evdev_keyboard_init(struct keyboard_evdev *kb)
{
    memset(kb, 0, sizeof(struct keyboard_evdev));
    kb->kbdevs = evdev_find_and_load_devices(EVDEV_MASK_KEYBOARD);
    if (kb->kbdevs == NULL)
        goto err;

    kb->poll_fds = vector_new(struct pollfd);
    vector_reserve(kb->poll_fds, vector_size(kb->kbdevs));

    for (u32 i = 0; i < vector_size(kb->kbdevs); i++) {
        vector_push_back(kb->poll_fds, (struct pollfd) {
            .fd = kb->kbdevs[i].fd,
            .events = POLLIN,
            .revents = 0
        });
    }

    return 0;

err:
    evdev_keyboard_destroy(kb);
    return 1;
}

void evdev_keyboard_destroy(struct keyboard_evdev *kb)
{
    if (kb == NULL) return;

    if (kb->kbdevs != NULL) {
        for (u32 i = 0; i < vector_size(kb->kbdevs); i++) {
            if (kb->kbdevs[i].fd != -1) close(kb->kbdevs[i].fd);
        }
        vector_destroy(&kb->kbdevs);
    }
    if (kb->poll_fds != NULL) vector_destroy(&kb->poll_fds);

    /* All members are already reset */

}

void evdev_keyboard_update_all_keys(struct keyboard_evdev *kb,
    pressable_obj_t pobjs[P_KEYBOARD_N_KEYS])
{
    u_check_params(kb != NULL && kb->kbdevs != NULL && pobjs != NULL);

    u32 n_poll_fds = vector_size(kb->poll_fds);

try_again:
    if (poll(kb->poll_fds, n_poll_fds, 1) == -1) {
        if (errno == EINTR) /* Interrupted by signal */
            goto try_again;

        s_log_error("Failed to poll() on keyboard event devices: %s",
            strerror(errno));
        return;
    }

    bool updated_keys[P_KEYBOARD_N_KEYS] = { 0 };
    for (u32 i = 0; i < n_poll_fds; i++) {
        if (!(kb->poll_fds[i].revents & POLLIN)) continue;
        read_keyevents_from_evdev(kb->kbdevs[i].fd, pobjs, updated_keys);
    }

    /* When a key is pressed, it gets a "pressed" (1) event
     * and in read_keyevents_from_evdev we update our pressable_objs accordingly.
     *
     * However, when the key is being held, it only starts getting
     * "held" (2) events after some amount of time (which might be several ticks).
     *
     * During that moment, the pressable_obj is not updated in the above loop,
     * and so the key is reported to be "down" for many ticks,
     * which should not happen.
     *
     * Therefore, we need to update the keys
     * that are held (but not getting any events) ourselves.
     */

    for (u32 i = 0; i < P_KEYBOARD_N_KEYS; i++) {
        if (!updated_keys[i] && (pobjs[i].pressed || pobjs[i].up))
            pressable_obj_update(&pobjs[i], pobjs[i].pressed);
    }
}

static void read_keyevents_from_evdev(i32 fd,
    pressable_obj_t keys[P_KEYBOARD_N_KEYS],
    bool updated_keys[P_KEYBOARD_N_KEYS])
{
    struct input_event ev = { 0 };
    i32 n_bytes_read = 0;
    while (n_bytes_read = read(fd, &ev, sizeof(struct input_event)),
        n_bytes_read > 0
    ) {
        if (n_bytes_read <= 0) { /* Either no data was read or some other error */
            return;
        } else if (n_bytes_read != sizeof(struct input_event)) {
            s_log_fatal(MODULE_NAME, __func__,
                    "Read %i bytes from event device, expected size is %i. "
                    "The linux input driver is probably broken...",
                    n_bytes_read, sizeof(struct input_event));
        }

        if (ev.type != EV_KEY) return;

        i32 i = 0;
        enum p_keyboard_keycode p_kb_keycode = -1;
        if (ev.code >= KEY_1 && ev.code <= KEY_9) {
            p_kb_keycode = (ev.code - KEY_1) + KB_KEYCODE_DIGIT1;
        } else {
            do {
                if (linux_input_code_2_kb_keycode_map[i][1] == ev.code) {
                    p_kb_keycode = linux_input_code_2_kb_keycode_map[i][0];
                    break;
                }
            } while (++i < P_KEYBOARD_N_KEYS);
        }

        if (p_kb_keycode == -1) return; /* Unsupported key press */

        /* ev.value = 0: key released
         * ev.value = 1: key pressed
         * ev.value = 2: key held
         */
        pressable_obj_update(&keys[p_kb_keycode], ev.value > 0);
        updated_keys[p_kb_keycode] = true;

        /*
         * s_log_debug("Key event %i for keycode %s",
         * ev.value, p_keyboard_keycode_strings[p_kb_keycode]);
         */
    }
}
