#ifndef P_KEYBOARD_H_
#define P_KEYBOARD_H_

#include "window.h"
#include <core/pressable-obj.h>

#define P_KEYBOARD_N_KEYS 43

struct p_keyboard;
enum p_keyboard_keycode;

struct p_keyboard * p_keyboard_init(struct p_window *win);

void p_keyboard_update(struct p_keyboard *kb);

const pressable_obj_t * p_keyboard_get_key(const struct p_keyboard *kb,
    enum p_keyboard_keycode code);

void p_keyboard_destroy(struct p_keyboard **kb_p);

/* Defined at the bottom for better readability */
#define P_KEYBOARD_KEYCODE_LIST \
    X_(KB_KEYCODE_ENTER)        \
    X_(KB_KEYCODE_SPACE)        \
    X_(KB_KEYCODE_ESCAPE)       \
    X_(KB_KEYCODE_DIGIT0)       \
    X_(KB_KEYCODE_DIGIT1)       \
    X_(KB_KEYCODE_DIGIT2)       \
    X_(KB_KEYCODE_DIGIT3)       \
    X_(KB_KEYCODE_DIGIT4)       \
    X_(KB_KEYCODE_DIGIT5)       \
    X_(KB_KEYCODE_DIGIT6)       \
    X_(KB_KEYCODE_DIGIT7)       \
    X_(KB_KEYCODE_DIGIT8)       \
    X_(KB_KEYCODE_DIGIT9)       \
    X_(KB_KEYCODE_A)            \
    X_(KB_KEYCODE_B)            \
    X_(KB_KEYCODE_C)            \
    X_(KB_KEYCODE_D)            \
    X_(KB_KEYCODE_E)            \
    X_(KB_KEYCODE_F)            \
    X_(KB_KEYCODE_G)            \
    X_(KB_KEYCODE_H)            \
    X_(KB_KEYCODE_I)            \
    X_(KB_KEYCODE_J)            \
    X_(KB_KEYCODE_K)            \
    X_(KB_KEYCODE_L)            \
    X_(KB_KEYCODE_M)            \
    X_(KB_KEYCODE_N)            \
    X_(KB_KEYCODE_O)            \
    X_(KB_KEYCODE_P)            \
    X_(KB_KEYCODE_Q)            \
    X_(KB_KEYCODE_R)            \
    X_(KB_KEYCODE_S)            \
    X_(KB_KEYCODE_T)            \
    X_(KB_KEYCODE_U)            \
    X_(KB_KEYCODE_V)            \
    X_(KB_KEYCODE_W)            \
    X_(KB_KEYCODE_X)            \
    X_(KB_KEYCODE_Y)            \
    X_(KB_KEYCODE_Z)            \
    X_(KB_KEYCODE_ARROWUP)      \
    X_(KB_KEYCODE_ARROWDOWN)    \
    X_(KB_KEYCODE_ARROWLEFT)    \
    X_(KB_KEYCODE_ARROWRIGHT)   \


#define X_(name) name,
enum p_keyboard_keycode {
    P_KEYBOARD_KEYCODE_LIST
};
#undef X_

#define X_(name) #name,
static const char *const p_keyboard_keycode_strings[] = {
    P_KEYBOARD_KEYCODE_LIST
};
#undef X_

#undef P_KEYBOARD_KEYCODE_LIST

#endif /* P_KEYBOARD_H_ */
