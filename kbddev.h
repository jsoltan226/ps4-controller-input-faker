#ifndef KBDDEV_H_
#define KBDDEV_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <core/int.h>
#include <assert.h>
#include <stdbool.h>

typedef struct kbddev {
    i32 fd; /* The actual event device fd */

    /* Internal stuff; don't play around with it */
    bool dev_created__; bool destroyed__;
} kbddev_t;
static_assert(sizeof(kbddev_t) <= 8,
    "The size of struct kbddev must be smaller than or equal to "
    "the word size (64 bits - 8 bytes)");

/* Initializes a new fake keyboard device in `*kbddev_p`.
 * Returns 0 on success and non-zero on failure. */
i32 kbddev_init(kbddev_t *kbddev_p);

/* Destroys the fake keyboard device pointed to by `kbddev_p`. */
void kbddev_destroy(kbddev_t *kbddev_p);

#ifdef KBDDEV_INTERNAL_GUARD__
#include <linux/uinput.h>
#include <linux/input-event-codes.h>

static const u8 evdev_key_bits[] = {
    KEY_F21
};

#define KBDDEV_UINPUT_DEV_NAME "PS4 controller fake keyboard"
static const struct uinput_setup ds4_setup = {
    .name = KBDDEV_UINPUT_DEV_NAME,
    .id = { /* Values taken directly from a DS4 controller evdev */
        .bustype = BUS_BLUETOOTH,
        .vendor = 0x8100,
        .product = 0x09cc,
        .version = 0x8100,
    },
    .ff_effects_max = 0,
};

#endif /* KBDDEV_INTERNAL_GUARD__ */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* KBDDEV_H_ */
