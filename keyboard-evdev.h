#ifndef KEYBOARD_EVDEV_H_
#define KEYBOARD_EVDEV_H_

#include "keyboard.h"
#include <core/int.h>
#include <core/util.h>
#include <core/vector.h>
#include <core/pressable-obj.h>
#include <sys/poll.h>
#include <poll.h>
#include <linux/limits.h>
#include <linux/input-event-codes.h>
#define P_INTERNAL_GUARD__
#include "evdev.h"
#undef P_INTERNAL_GUARD__

struct keyboard_evdev {
    VECTOR(struct evdev) kbdevs;
    VECTOR(struct pollfd) poll_fds;
};

i32 evdev_keyboard_init(struct keyboard_evdev *kb);
void evdev_keyboard_destroy(struct keyboard_evdev *kb);

void evdev_keyboard_update_all_keys(struct keyboard_evdev *kb,
    pressable_obj_t pobjs[P_KEYBOARD_N_KEYS]);

static const i32 linux_input_code_2_kb_keycode_map[P_KEYBOARD_N_KEYS][2] = {
    { KB_KEYCODE_ENTER, KEY_ENTER },
    { KB_KEYCODE_SPACE, KEY_SPACE },
    { KB_KEYCODE_ESCAPE, KEY_ESC },
    { KB_KEYCODE_DIGIT0, KEY_0 },
    { KB_KEYCODE_DIGIT1, KEY_1 },
    { KB_KEYCODE_DIGIT2, KEY_2 },
    { KB_KEYCODE_DIGIT3, KEY_3 },
    { KB_KEYCODE_DIGIT4, KEY_4 },
    { KB_KEYCODE_DIGIT5, KEY_5 },
    { KB_KEYCODE_DIGIT6, KEY_6 },
    { KB_KEYCODE_DIGIT7, KEY_7 },
    { KB_KEYCODE_DIGIT8, KEY_8 },
    { KB_KEYCODE_DIGIT9, KEY_9 },
    { KB_KEYCODE_A, KEY_A },
    { KB_KEYCODE_B, KEY_B },
    { KB_KEYCODE_C, KEY_C },
    { KB_KEYCODE_D, KEY_D },
    { KB_KEYCODE_E, KEY_E },
    { KB_KEYCODE_F, KEY_F },
    { KB_KEYCODE_G, KEY_G },
    { KB_KEYCODE_H, KEY_H },
    { KB_KEYCODE_I, KEY_I },
    { KB_KEYCODE_J, KEY_J },
    { KB_KEYCODE_K, KEY_K },
    { KB_KEYCODE_L, KEY_L },
    { KB_KEYCODE_M, KEY_M },
    { KB_KEYCODE_N, KEY_N },
    { KB_KEYCODE_O, KEY_O },
    { KB_KEYCODE_P, KEY_P },
    { KB_KEYCODE_Q, KEY_Q },
    { KB_KEYCODE_R, KEY_R },
    { KB_KEYCODE_S, KEY_S },
    { KB_KEYCODE_T, KEY_T },
    { KB_KEYCODE_U, KEY_U },
    { KB_KEYCODE_V, KEY_V },
    { KB_KEYCODE_W, KEY_W },
    { KB_KEYCODE_X, KEY_X },
    { KB_KEYCODE_Y, KEY_Y },
    { KB_KEYCODE_Z, KEY_Z },
    { KB_KEYCODE_ARROWUP, KEY_UP },
    { KB_KEYCODE_ARROWDOWN, KEY_DOWN },
    { KB_KEYCODE_ARROWLEFT, KEY_LEFT },
    { KB_KEYCODE_ARROWRIGHT, KEY_RIGHT },
};

#endif /* KEYBOARD_EVDEV_H_ */
