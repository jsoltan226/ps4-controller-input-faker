#define P_INTERNAL_GUARD__
#include "evdev.h"
#undef P_INTERNAL_GUARD__
#include "joystick-ev.h"
#include "monitor.h"
#include <core/int.h>
#include <core/log.h>
#include <core/vector.h>
#include <stdatomic.h>

#define MODULE_NAME "joystick-ev"

void handle_monitor_event(struct evdev_monitor *mon,
    VECTOR(struct evdev) devices)
{
    VECTOR(char *) created = NULL;
    VECTOR(char *) deleted = NULL;
    if (evdev_monitor_poll_and_read(mon, 0, &created, &deleted))
        return;

    for (u32 i = 0; i < vector_size(created); i++) {
        struct evdev new_dev = { 0 };
        if (strncmp(created[i], "event", u_strlen("event"))) {
            /* Not an evdev */
        } else if (evdev_load(created[i], &new_dev, EVDEV_MASK_AUTO)) {
            s_log_debug("Failed to load event device %s", created[i]);
        } else {
            s_log_info("New device: \"%s\" (%s), type %s",
                new_dev.name[0] ? new_dev.name : "n/a",
                new_dev.path, evdev_type_strings[new_dev.type]
            );
            vector_push_back(devices, new_dev);
        }
        u_nfree(&created[i]);
    }
    vector_destroy(&created);

    for (u32 i = 0; i < vector_size(deleted); i++) {
        for (u32 j = 0; j < vector_size(devices); j++) {
            s_assert(!strncmp(devices[j].path,
                "/dev/input/", u_strlen("/dev/input/")),
                "Invalid event device path \"%s\"", devices[j].path);
            if (!strcmp(deleted[i], devices[j].path + u_strlen("/dev/input/"))) {
                s_log_info("Removed device: %s", deleted[i]);
                evdev_destroy(&devices[j]);
                vector_erase(devices, j);
                break;
            }
        }
        u_nfree(&deleted[i]);
    }
    vector_destroy(&deleted);
}
