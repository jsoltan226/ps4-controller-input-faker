#ifndef EVDEV_H_
#define EVDEV_H_

#include <core/int.h>
#include <core/util.h>
#include <core/vector.h>
#include <linux/input-event-codes.h>
#include <linux/input.h>

#ifdef CGD_CONFIG_PLATFORM_LINUX_EVDEV_PS4_CONTROLLER_SUPPORT
#define EVDEV_TYPES_LIST    \
    X_(EVDEV_UNKNOWN)       \
    X_(EVDEV_KEYBOARD)      \
    X_(EVDEV_MOUSE)         \
    X_(EVDEV_PS4_CONTROLLER)    \
    X_(EVDEV_PS4_CONTROLLER_TOUCHPAD) \
    X_(EVDEV_PS4_CONTROLLER_MOTION_SENSOR) \

#else
#define EVDEV_TYPES_LIST    \
    X_(EVDEV_UNKNOWN)       \
    X_(EVDEV_KEYBOARD)      \
    X_(EVDEV_MOUSE)         \

#endif /* CGD_CONFIG_PLATFORM_LINUX_EVDEV_PS4_CONTROLLER_SUPPORT */

#define X_(name) name,
enum evdev_type {
    EVDEV_TYPE_AUTO = -1,
    EVDEV_TYPES_LIST
    EVDEV_N_TYPES
};
#undef X_

#define EV_check_end_ (-1)
#define EV_max_n_checks_ 256

static const i32
evdev_type_checks[EVDEV_N_TYPES][EV_CNT][EV_max_n_checks_] = {
    [EVDEV_KEYBOARD] = {
        [0] = {
            EV_KEY, EV_check_end_
        },
        [EV_KEY] = {
            KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9,
            KEY_0, KEY_A, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G, KEY_H,
            KEY_J, KEY_L, KEY_M, KEY_N, KEY_O, KEY_P, KEY_Q, KEY_R, KEY_S,
            KEY_T, KEY_U, KEY_V, KEY_W, KEY_X, KEY_Y, KEY_Z,
            KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT,
            KEY_SPACE, KEY_ESC, KEY_ENTER, KEY_BACKSPACE, KEY_TAB, KEY_F21,
            EV_check_end_
        }
    },
    [EVDEV_MOUSE] = {
        [0] = {
            EV_KEY, EV_REL, EV_check_end_
        },
        [EV_KEY] = {
            BTN_LEFT, BTN_RIGHT, BTN_MIDDLE, EV_check_end_
        },
        [EV_REL] = {
            REL_X, REL_Y, REL_WHEEL, EV_check_end_
        },
    },
#ifdef CGD_CONFIG_PLATFORM_LINUX_EVDEV_PS4_CONTROLLER_SUPPORT
    [EVDEV_PS4_CONTROLLER] = {
        [0] = {
            EV_KEY, EV_ABS, EV_check_end_
        },
        [EV_KEY] = {
            /* Cross, circle, square, triangle */
            BTN_SOUTH, BTN_EAST, BTN_WEST, BTN_NORTH,

            /* L1, R1, L2, R2 */
            BTN_TL, BTN_TR, BTN_TL2, BTN_TR2,

            /* L3, R3 */
            BTN_THUMBL, BTN_THUMBR,

            BTN_SELECT, /* Share */
            BTN_START, /* Options */
            BTN_MODE, /* PS (home) button */

            EV_check_end_
        },
        [EV_ABS] = {
            ABS_X, ABS_Y, /* Left stick movement */
            ABS_RX, ABS_RY, /* Right stick movement */
            ABS_Z, ABS_RZ, /* L2 and R2 pressure (0 - 255) */

            /* D-pad (arrows on the left) */
            ABS_HAT0X, /* -1 for left, 1 for right and 0 for neutral (both) */
            ABS_HAT0Y, /* Same here but -1 for up and 1 for down */

            EV_check_end_
        },
    },
    [EVDEV_PS4_CONTROLLER_TOUCHPAD] = {
        [0] = {
            EV_KEY, EV_ABS, EV_check_end_
        },
        [EV_KEY] = {
            /* Touchpad press */
            BTN_TOOL_FINGER, BTN_TOOL_DOUBLETAP,
            BTN_TOUCH,
            BTN_LEFT,
            EV_check_end_
        },
        [EV_ABS] = {
            ABS_MT_POSITION_X, ABS_MT_POSITION_Y, /* Finger movement */
            ABS_MT_TRACKING_ID, /* Touch ID */
            EV_check_end_
        },
    },
    [EVDEV_PS4_CONTROLLER_MOTION_SENSOR] = {
        [0] = {
            EV_ABS, EV_check_end_
        },
        [EV_ABS] = {
            ABS_X, ABS_Y, ABS_Z, /* Accelerometer */
            ABS_RX, ABS_RY, ABS_RZ, /* Gyroscope */
            EV_check_end_
        },
    },
#endif /* CGD_CONFIG_PLATFORM_LINUX_EVDEV_PS4_CONTROLLER_SUPPORT */
};

static const u32 ev_max_vals[EV_CNT] = {
    [EV_SYN] = SYN_MAX,
    [EV_KEY] = KEY_MAX,
    [EV_REL] = REL_MAX,
    [EV_ABS] = ABS_MAX,
    [EV_MSC] = MSC_MAX,
    [EV_SW] = SW_MAX,
    [EV_LED] = LED_MAX,
    [EV_SND] = SND_MAX,
    [EV_REP] = REP_MAX,
    [EV_FF] = FF_MAX,
    [EV_PWR] = 0,
    [EV_FF_STATUS] = FF_STATUS_MAX,
    [EV_MAX] = 0,
};
union ev_bits_max_size__ {
    u64 syn[u_nbits(SYN_CNT)];
    u64 key[u_nbits(KEY_CNT)];
    u64 rel[u_nbits(REL_CNT)];
    u64 abs[u_nbits(ABS_CNT)];
    u64 msc[u_nbits(MSC_CNT)];
    u64 sw[u_nbits(SW_CNT)];
    u64 led[u_nbits(LED_CNT)];
    u64 snd[u_nbits(SND_CNT)];
    u64 rep[u_nbits(REP_CNT)];
    u64 ff[u_nbits(FF_CNT)];
    u64 pwr[1];
    u64 ff_status[u_nbits(FF_STATUS_MAX + 1)];
};

#define X_(name) #name,
static const char *const evdev_type_strings[] = {
    EVDEV_TYPES_LIST
};
#undef X_

#define MAX_EVDEV_NAME_LEN   512
struct evdev {
    bool initialized_;
    i32 fd;
    enum evdev_type type;
    char path[u_FILEPATH_MAX];
    char name[MAX_EVDEV_NAME_LEN];
};

/* `evdev_load_available_devices()` will fail
 * if less than this fraction of devices load successfully */
#define MINIMAL_SUCCESSFUL_EVDEVS_LOADED 0.5f

/* Returns NULL on failure */
VECTOR(struct evdev) evdev_find_and_load_devices(enum evdev_type type);

/* Used internally by `evdev_load_available_devices`.
 * Returns 0 on success and non-zero on failure */
i32 evdev_load(const char *rel_path, struct evdev *out, enum evdev_type type);

/* Frees the memory and closes the file descriptors
 * associated with all devices in `*evdev_list_p`.
 * Also destroys the vector itself, and sets the value
 * of `*evdev_list_p` to `NULL`. */
void evdev_list_destroy(VECTOR(struct evdev) *evdev_list_p);

/* Frees resources associated with just the evdev `e`. */
void evdev_destroy(struct evdev *e);

#endif /* EVDEV_H_ */
