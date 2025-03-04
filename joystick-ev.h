#ifndef JOYSTICK_EV_H_
#define JOYSTICK_EV_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include "evdev.h"
#include "monitor.h"
#include <core/vector.h>

void handle_monitor_event(struct evdev_monitor *mon,
    VECTOR(struct evdev) devices);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* JOYSTICK_EV_H_ */
