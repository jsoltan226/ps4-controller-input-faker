#include "config-parse.h"
#include <core/log.h>
#include <stdlib.h>

#define MODULE_NAME "config-test"

static struct config cfg = {
    .n_options = 1,
    .options = {
        (struct config_option) {
            .key = "test",
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

    s_log_info("Test result is OK");
    return EXIT_SUCCESS;
}
