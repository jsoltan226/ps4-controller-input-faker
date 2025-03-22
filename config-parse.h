#ifndef CONFIG_PARSE_H_
#define CONFIG_PARSE_H_

#include <core/int.h>
#include <core/vector.h>
#include <stdbool.h>

#define CONFIG_KEY_MAX_LEN 256U
#define CONFIG_VALUE_MAX_LEN 1024U
#define CONFIG_SECTION_MAX_LEN 256U

#define CONFIG_MAX_N_OPTIONS 512U

enum config_type {
    CONFIG_TYPE_UNSET,
    CONFIG_TYPE_INT,
    CONFIG_TYPE_FLOAT,
    CONFIG_TYPE_BOOL,
    CONFIG_TYPE_STRING,
};

struct config_option {
    char key[CONFIG_KEY_MAX_LEN];
    enum config_type type;
    union config_value {
        i64 i;
        f64 f;
        bool b;
        char str[CONFIG_VALUE_MAX_LEN];
    } value;
    char section[CONFIG_SECTION_MAX_LEN];
};

struct config {
    u16 n_options;
    struct config_option options[];
};

i32 config_parse(const char *config_file_path, struct config *o);

#endif /* CONFIG_PARSE_H_ */
