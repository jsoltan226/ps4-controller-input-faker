#ifndef JSDEV_H_
#define JSDEV_H_

#include "evdev.h"
#include <core/int.h>
#include <core/util.h>

#define JOYSTICK_NAME_MAX_LEN 512

struct joystick_dev {
    bool initialized_;

    i32 fd;
    char path[u_FILEPATH_MAX];
    char name[JOYSTICK_NAME_MAX_LEN];
    bool grabbed;

    struct evdev evdev;
};

i32 joystick_dev_load(struct joystick_dev *jsdev,
    const char *rel_path, bool grab_evdev);

i32 joystick_grab_evdev(struct joystick_dev *jsdev);
i32 joystick_release_evdev(struct joystick_dev *jsdev);

void joystick_read_event(struct joystick_dev *jsdev);

void joystick_dev_destroy(struct joystick_dev *jsdev);

#endif /* JSDEV_H_ */
