#define S_LOG_LEVELS_LIST_DEF__
#include <core/log.h>
#undef S_LOG_LEVELS_LIST_DEF__
#include "cfg.h"
#include "key-codes.h"
#include "config-parse.h"
#include <core/util.h>
#include <core/int.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MODULE_NAME "cfg"

#define CONFIG_FILE_NAME "ps4-controller-input-faker.ini"

#define X_(name_, value_) { .name = #name_, .value = value_ },
const struct config_enum_value log_level_possible_values[] = {
    S_LOG_LEVELS_LIST
};
#undef X_
#undef S_LOG_LEVELS_LIST

#define X_(name_, value_) { .name = #name_, .value = value_ },
const struct config_enum_value keycode_possible_values[] = {
    EV_KEY_LIST
};
#undef X_

static void assign_values_from_config(struct cfg *o,
    struct config *options);

i32 read_config(struct cfg *o)
{
    u_check_params(o != NULL);

    static struct config_option options[] = {
        {
            .key = "fake_keypress_keycode",
            .type = CONFIG_TYPE_ENUM,
            .enum_info = {
                .possible_values = keycode_possible_values,
                .n_possible_values = u_arr_size(keycode_possible_values),
            },
        },
        {
            .key = "log_level",
            .type = CONFIG_TYPE_ENUM,
            .enum_info = {
                .possible_values = log_level_possible_values,
                .n_possible_values = u_arr_size(log_level_possible_values),
            },
        },
    };
    struct config cfg = {
        .options = options,
        .n_options = u_arr_size(options),
    };

    const filepath_t config_file_path_templates[] = {
        "./" CONFIG_FILE_NAME,
        "/usr/local/etc/" CONFIG_FILE_NAME,
        "/etc/" CONFIG_FILE_NAME,
        /* XDG_CONFIG_HOME */ "%s/" CONFIG_FILE_NAME,
        /* HOME */ "%s/.config/" CONFIG_FILE_NAME,
        /* HOME */ "%s/." CONFIG_FILE_NAME,
    };
    static const char *
    template_envvars[u_arr_size(config_file_path_templates)] = {
        NULL,
        NULL,
        NULL,
        "XDG_CONFIG_HOME",
        "HOME",
        "HOME",
    };
    for (u32 i = 0; i < u_arr_size(config_file_path_templates); i++) {
        filepath_t path = { 0 };
        const char *envvar = { 0 };
        if (template_envvars[i] && (envvar = getenv(template_envvars[i]),
            envvar == NULL))
                continue;
        else if (template_envvars[i] && envvar != NULL) {
            snprintf(path, sizeof(filepath_t),
                config_file_path_templates[i], envvar);
            path[sizeof(filepath_t) - 1] = '\0';
        } else {
            memcpy(path, config_file_path_templates[i],
                sizeof(filepath_t) - 1);
        }

        enum config_parse_ret ret = config_parse(path, &cfg);
        switch (ret) {
        case CONFIG_PARSE_SUCCESS:
            s_log_debug("Successfully parsed config file at path \"%s\"",
                path);
            assign_values_from_config(o, &cfg);
            return 0;
        case CONFIG_PARSE_ERR_OPEN_FILE:
            s_log_debug("Couldn't open config file at path \"%s\"",
                path);
            continue;
        default:
        case CONFIG_PARSE_ERR_READ_FILE:
        case CONFIG_PARSE_ERR_INVALID_ARG:
        case CONFIG_PARSE_ERR_SYNTAX:
            goto_error("Error (%i) while parsing config file \"%s\"",
                ret, path);
        }
    };

    s_log_warn("No config file found in any of the possible paths");
err:
    s_log_warn("Using default config values");
    assign_values_from_config(o, NULL);
    return 1;
}

static void assign_values_from_config(struct cfg *o,
    struct config *options)
{
    if (options == NULL) {
        static const struct cfg default_cfg = {
            .fake_keypress_keycode = FAKE_KEYPRESS_KEYCODE_DEFAULT,
            .log_level = LOG_LEVEL_DEFAULT,
        };
        memcpy(o, &default_cfg, sizeof(struct cfg));
        return;
    }


    bool matched_fake_keypress_keycode = false,
         matched_log_level = false;

    for (u32 i = 0; i < options->n_options; i++) {
        if (!strncmp("fake_keypress_keycode", options->options[i].key,
            CONFIG_KEY_MAX_LEN) &&
            options->options[i].matched)
        {
            matched_fake_keypress_keycode = true;
            o->fake_keypress_keycode = options->options[i].value.e;
        } else if (!strncmp("log_level", options->options[i].key,
            CONFIG_KEY_MAX_LEN) &&
            options->options[i].matched)
        {
            matched_log_level = true;
            o->log_level = options->options[i].value.e;
        }
    }

    if (!matched_fake_keypress_keycode)
        o->fake_keypress_keycode = FAKE_KEYPRESS_KEYCODE_DEFAULT;
    if (!matched_log_level)
        o->log_level = LOG_LEVEL_DEFAULT;
}
