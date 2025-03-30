#ifndef CFG_H_
#define CFG_H_

#include <core/log.h>
#include <core/int.h>
#include <linux/input-event-codes.h>

struct cfg {
#define FAKE_KEYPRESS_KEYCODE_DEFAULT (KEY_F21)
    u16 fake_keypress_keycode;

#define LOG_LEVEL_DEFAULT (LOG_DEBUG)
    enum s_log_level log_level;
};

i32 read_config(struct cfg *o);

#endif /* CFG_H_ */
