#include "key-codes.h"
#define P_INTERNAL_GUARD__
#include "evdev.h"
#undef P_INTERNAL_GUARD__
#include <core/log.h>
#include <core/util.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <linux/input-event-codes.h>

#define MODULE_NAME "evdev"

#define X_(name, id) [id] = #name, \

static const char *const ev_type_strings[EV_CNT] = {
    EV_TYPE_LIST
};
static const char * ev_strings[EV_CNT][sizeof(union ev_bits_max_size__) * 8] = {
    [EV_SYN] = { EV_SYN_LIST },
    [EV_KEY] = { EV_KEY_LIST },
    [EV_REL] = { EV_REL_LIST },
    [EV_ABS] = { EV_ABS_LIST },
    [EV_MSC] = { EV_MSC_LIST },
    [EV_SW]  = { EV_SW_LIST  },
    [EV_LED] = { EV_LED_LIST },
    [EV_SND] = { EV_SND_LIST },
    [EV_REP] = { EV_REP_LIST },
    [EV_FF] =  { ""          },
    [EV_PWR] = { ""          },
    [EV_FF_STATUS] = { ""    },
};
#undef X_

void evdev_print_caps(i32 fd)
{
    u64 ev_bits[u_nbits(EV_MAX)];
    if (ioctl(fd, EVIOCGBIT(0, sizeof(ev_bits)), ev_bits) < 0) {
        s_log_error("Failed to get supported event types from fd %i: %s",
            fd, strerror(errno));
        return;
    }

    printf("FD %i\n" "- EV_SYN (0x0)\n\n", fd);

    u32 i = 0;
    while (++i < EV_MAX) {
        if (!(ev_bits[i / 64] & (1ULL << (u64)(i % 64))))
            continue;

        printf("- %s (%#x)\n", ev_type_strings[i], i);

        /* EV_REP and upwards are useless to use and yet often
         * EVIOCGBIT fails for them */
        if (i >= EV_REP) {
            printf("\n");
            continue;
        }

        u64 bits[sizeof(union ev_bits_max_size__)];
        if (ioctl(fd, EVIOCGBIT(i, ev_max_vals[i]), bits) < 0) {
            s_log_error("Failed to get event bits from fd %i for %s: %s",
                fd, ev_type_strings[i], strerror(errno));
            return;
        }

        for (u32 j = 0; j < ev_max_vals[i]; j++) {
            if (bits[j / 64] & (1ULL << (u64)(j % 64)))
                printf("-- %s (%#x)\n", ev_strings[i][j], j);
        }
        printf("\n");
    }
}
