#ifndef CONFIG_PARSE_H_
#define CONFIG_PARSE_H_

#include <core/int.h>
#include <core/util.h>
#include <core/vector.h>
#include <assert.h>
#include <stdbool.h>


enum config_type {
    CONFIG_TYPE_UNSET,
    CONFIG_TYPE_INT,
    CONFIG_TYPE_FLOAT,
    CONFIG_TYPE_BOOL,
    CONFIG_TYPE_STRING,
    CONFIG_TYPE_ENUM,
    CONFIG_N_TYPES,
};

struct config_option {
#define CONFIG_KEY_MAX_LEN 256U
    char key[CONFIG_KEY_MAX_LEN];

    /* Since this struct is large and is likely to be used in an array,
     * optimize it's size to be aligned with the page size
     * (4096 on most systems).
     * Since the section string is the least useful one here,
     * "cut out" space from it so that it can be used to store the
     * `type` and `matched` members */
#define CONFIG_SECTION_MAX_LEN (256U - 24U)
    char section[CONFIG_SECTION_MAX_LEN];

    union config_value {
        i64 i;
        f64 f;
        bool b;
#define CONFIG_VALUE_MAX_LEN 512U
        char str[CONFIG_VALUE_MAX_LEN];
        i64 e;
    } value;
    struct config_enum_info {
        const struct config_enum_value {
            const char *name;
            const i64 value;
        } *possible_values;
        u32 n_possible_values;
    } enum_info;

    enum config_type type;

    bool matched;
};
static_assert(sizeof(struct config_option) == 1024,
    "Invalid size of struct config_option: %s");

/* It's a bit unreasonable to create huge arrays of large structs,
 * especially since the program is unlikely to have
 * a lot of configuration options, and so oftentimes `config.n_options`
 * reaching above this limit is caused by some error
 * (e.g. a typo and/or an integer underflow),
 * and so a restriction is placed.
 *
 * Note, however, that it might be changed or removed entirely in the future
 * as the applications grow in complexity and require more config options.
 */
#define CONFIG_MAX_N_OPTIONS 512U

struct config {
    u16 n_options; /* must be in the range <0, CONFIG_MAX_N_OPTIONS> */
    struct config_option *options;
};

/* The maximum size of a "full key" string (a.k.a. "<section>.<key>") */
#define CONFIG_FULL_KEY_MAX_LEN \
    (CONFIG_SECTION_MAX_LEN + CONFIG_KEY_MAX_LEN + u_strlen("."))

/* Parses the configuration file `config_file_path`,
 * according to the `key`, `section` and `type` fields of `cfg->options`,
 * storing the results in their respective `value` members.
 *
 * If no value corresponding to a given option's key is found in the file
 * (the sections must also match), the `matched` field is set to false.
 *
 * Note that when no options in `cfg` are found and matched at all,
 * a warning will be printed, but 0 will still be returned.
 * It's up to the user to check which options were or weren't matched
 * by checking the value of `matched` field.
 *
 * Returns 0 on success and one of the follwing error codes on failure:
 * The possible errors are:
 *  -1: File couldn't be opened (invalid path, permission denied, etc)
 *  -2: Error while reading the file
 *  1: Invalid config struct/option layout in `cfg`
 *      or `n_options` > `CONFIG_MAX_N_OPTIONS`
 *  2: Incorrect syntax in the config file
 */
enum config_parse_ret {
    CONFIG_PARSE_SUCCESS = 0,
    CONFIG_PARSE_ERR_OPEN_FILE = -1,
    CONFIG_PARSE_ERR_READ_FILE = -2,
    CONFIG_PARSE_ERR_INVALID_ARG = 1,
    CONFIG_PARSE_ERR_SYNTAX = 2,
} config_parse(const char *config_file_path, struct config *cfg);

void config_snprintf_value(char *buf, u32 buf_size,
    const union config_value *val, enum config_type val_type);
void config_snprintf_section_and_key(char *buf, u32 buf_size,
    const struct config_option *option);

#endif /* CONFIG_PARSE_H_ */
