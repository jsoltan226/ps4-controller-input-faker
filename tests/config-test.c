#include "config-parse.h"
#include <core/log.h>
#include <stdlib.h>

#define MODULE_NAME "config-test"

static struct config cfg = {
    .n_options = 5,
    .options = {
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
    }
};

int main(void)
{
    s_configure_log(LOG_DEBUG, stdout, stderr);

    if (config_parse("tests/config.test.ini", &cfg)) {
        s_log_error("Failed to parse config. Stop.");
        s_log_info("Test result is FAIL");
        return EXIT_FAILURE;
    }

    s_log_debug("cfg.test: \"%s\"", cfg.options[0].value.str);
    s_log_debug("cfg.test_int: %lli", cfg.options[1].value.i);
    s_log_debug("cfg.test_float: %lf", cfg.options[2].value.f);
    s_log_debug("cfg.test_bool: %s", cfg.options[3].value.b ? "true" : "false");
    s_log_debug("cfg.test_string: %s", cfg.options[4].value.str);

    s_log_info("Test result is OK");
    return EXIT_SUCCESS;
}
