#include "config-parse.h"
#include <core/log.h>
#include <core/util.h>
#include <stdlib.h>
#include <string.h>

#define MODULE_NAME "config-test"

#define CONFIG_FILE_PATH "tests/config.test.ini"

static struct config_option options[] = {
    (struct config_option) {
        .key = "test",
        .type = CONFIG_TYPE_STRING,
    },
    (struct config_option) {
        .key = "test_int",
        .type = CONFIG_TYPE_INT,
    },
    (struct config_option) {
        .key = "test_float",
        .type = CONFIG_TYPE_FLOAT,
    },
    (struct config_option) {
        .key = "test_bool",
        .type = CONFIG_TYPE_BOOL,
    },
    (struct config_option) {
        .key = "test_string",
        .type = CONFIG_TYPE_STRING,
    },
    (struct config_option) {
        .key = "test_section_1",
        .section = "section",
        .type = CONFIG_TYPE_STRING,
    },
    (struct config_option) {
        .key = "test_section_2",
        .section = "section.subsection",
        .type = CONFIG_TYPE_STRING,
    },
};
static struct config cfg = {
    .options = options,
    .n_options = u_arr_size(options)
};

static const union config_value expected_values[u_arr_size(options)] = {
    (union config_value) { .str = "SUS" },
    (union config_value) { .i = -7894253 },
    (union config_value) { .f = 3.14159265359 },
    (union config_value) { .b = true },
    (union config_value) { .str = "string" },
    (union config_value) { .str = "section_test" },
    (union config_value) { .str = "subsection_test" },
};

int main(void)
{
    s_configure_log(LOG_DEBUG, stdout, stderr);

    if (config_parse(CONFIG_FILE_PATH, &cfg)) {
        s_log_error("Failed to parse config. Stop.");
        s_log_info("Test result is FAIL");
        return EXIT_FAILURE;
    }

    /* Let this be reused */
    char full_key_buf[CONFIG_FULL_KEY_MAX_LEN] = { 0 };

    bool found_mismatch = false;
    for (u32 i = 0; i < u_arr_size(options); i++) {
        if (options[i].type == CONFIG_TYPE_UNSET) {
            memset(full_key_buf, 0, CONFIG_FULL_KEY_MAX_LEN);
            config_snprintf_section_and_key(full_key_buf,
                CONFIG_FULL_KEY_MAX_LEN, &options[i]);

            s_log_error("Unmatched value: %s", full_key_buf);
            found_mismatch = true;
        } else if (memcmp(&cfg.options[i].value, &expected_values[i],
                sizeof(union config_value)))
        {
            memset(full_key_buf, 0, CONFIG_FULL_KEY_MAX_LEN);
            config_snprintf_section_and_key(full_key_buf,
                CONFIG_FULL_KEY_MAX_LEN, &options[i]);

            char expected[CONFIG_VALUE_MAX_LEN] = { 0 };
            char got[CONFIG_VALUE_MAX_LEN] = { 0 };
            config_snprintf_value(expected, CONFIG_VALUE_MAX_LEN,
                &expected_values[i], options[i].type);
            config_snprintf_value(got, CONFIG_VALUE_MAX_LEN,
                &options[i].value, options[i].type);

            s_log_error("mismatched value %s%s%s: expected %s, got %s",
                options[i].section, options[i].section[0] ? "." : "",
                options[i].key,
                expected, got);
            found_mismatch = true;
        }
    }
    if (found_mismatch) {
        s_log_error("Not all options matched correctly.");
        s_log_info("Test result is FAIL");
        return EXIT_FAILURE;
    }

    for (u32 i = 0; i < u_arr_size(options); i++) {
        memset(full_key_buf, 0, CONFIG_FULL_KEY_MAX_LEN);
        config_snprintf_section_and_key(full_key_buf,
            CONFIG_FULL_KEY_MAX_LEN, &options[i]);

        char value_str[CONFIG_VALUE_MAX_LEN] = { 0 };
        config_snprintf_value(value_str, CONFIG_VALUE_MAX_LEN,
            &options[i].value, options[i].type);

        s_log_debug("cfg.%s: %s", full_key_buf, value_str);
    }

    s_log_info("Test result is OK");
    return EXIT_SUCCESS;
}
