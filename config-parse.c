#define _POSIX_C_SOURCE 200809L
#include "config-parse.h"
#include <core/log.h>
#include <core/int.h>
#include <core/math.h>
#include <core/util.h>
#include <core/vector.h>
#include <errno.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define MODULE_NAME "config"

#define CONFIG_PARSE_STATES_LIST    \
    X_(STATE_ERROR)                 \
    X_(STATE_IN_NOTHING)            \
    X_(STATE_IN_KEY)                \
    X_(STATE_IN_VALUE)              \
    X_(STATE_IN_SECTION)            \

#define X_(name) name,
enum config_parse_state {
    CONFIG_PARSE_STATES_LIST
    CONFIG_PARSE_N_STATES
};
#undef X_

static_assert(CONFIG_PARSE_N_STATES <= 32,
    "Too many states (cannot assign each a bit in a u32)");
#define X_(name) name##_MASK_ = 1U << name,
enum config_parse_state_mask {
    CONFIG_PARSE_STATES_LIST
};
#undef X_

#define X_(name) #name,
static const char *const config_parse_state_strings[] = {
    CONFIG_PARSE_STATES_LIST
};
#undef X_

#undef CONFIG_PARSE_STATES_LIST

struct config_parse_ctx {
    u32 line_number;

    char curr_char;
    char prev_char;
    bool escaped;
    bool comment;

    struct config_parse_global_ctx *global_ctx_p_;
};

struct config_option_intermediate {
    char key[CONFIG_KEY_MAX_LEN];
    char value[CONFIG_VALUE_MAX_LEN];
    char section[CONFIG_SECTION_MAX_LEN];
};

enum config_parse_global_ctx_flag {
    FLAG_LINE_HAS_SECTION_TAG = 1,
    FLAG_LINE_HAS_KEY_VALUE,
    FLAG_KEY_VALUE_READY,
    FLAG_SKIP_INCREMENT_CHAR_INDEX,
    FLAG_DROP_LINE,
    FLAG_VALUE_IN_SINGLE_QUOTATION,
    FLAG_VALUE_IN_DOUBLE_QUOTATION,
    FLAG_ESCAPED_LINE_BREAK,
    CONFIG_PARSE_GLOBAL_CTX_FLAG_MAX
};

static i32 match_options(struct config *cfg_o,
    const VECTOR(struct config_option_intermediate) intermediate_options);
static i32 try_write_value(union config_value *o,
    const char value_buf[CONFIG_VALUE_MAX_LEN],
    enum config_type desired_value_type);

static void handle_in_nothing(struct config_parse_ctx *ctx);
static void handle_in_key(struct config_parse_ctx *ctx);
static void handle_in_value(struct config_parse_ctx *ctx);
static void handle_in_section(struct config_parse_ctx *ctx);

/* This function, along with the `ctx_*` getters/setters
 * have the `struct config_parse_global_ctx`
 * definition available (everything above doesn't) */
static VECTOR(struct config_option_intermediate)
    read_options(const char *file_path, enum config_parse_ret *ret);

/* The parse state getter and setter (`ctx_*`) definitions
 * should be right here, but to improve readability they were moved lower
 * (beneath the last exported API function - `config_snprintf_section_and_key`)
 */

enum config_parse_ret
config_parse(const char *config_file_path, struct config *o)
{
    u_check_params(config_file_path != NULL && o != NULL);
    if (o->n_options > CONFIG_MAX_N_OPTIONS) {
        s_log_error("o->n_options (%u) > CONFIG_MAX_N_OPTIONS (%u)",
            o->n_options, CONFIG_MAX_N_OPTIONS);
        return CONFIG_PARSE_ERR_INVALID_ARG;
    }
    for (u32 i = 0; i < o->n_options; i++) {
        if (!(o->options[i].type >= 0 && o->options->type < CONFIG_N_TYPES)) {
            s_log_error("Invalid type in option %i: %i",
                i + 1, o->options[i].type);
            return CONFIG_PARSE_ERR_INVALID_ARG;
        }
    }

    enum config_parse_ret ret = CONFIG_PARSE_SUCCESS;
    VECTOR(struct config_option_intermediate) option_intermediates =
        read_options(config_file_path, &ret);
    if (option_intermediates == NULL || ret != CONFIG_PARSE_SUCCESS) {
        s_log_error("Failed to read config options from file \"%s\"",
            config_file_path);
        return ret;
    }

#ifndef CGD_BUILDTYPE_RELEASE
    s_log_debug("++++ read_options OK ++++");
    for (u32 i = 0; i < vector_size(option_intermediates); i++) {
        s_log_debug("key/value %u: \"%s\" = \"%s\"", i,
            option_intermediates[i].key, option_intermediates[i].value);
    }
    s_log_debug("+++++++++++++++++++++++++");
#endif /* CGD_BUILDTYPE_RELEASE */

    i32 n_matched_options = match_options(o, option_intermediates);
    vector_destroy(&option_intermediates);

    if (n_matched_options < 0)
        return CONFIG_PARSE_ERR_SYNTAX; /* Found duplicate keys */
    else if (n_matched_options == 0)
        s_log_warn("No matched options in configuration file \"%s\"",
            config_file_path);
    else /* if (n_matched_options > 0) */
        s_log_debug("Matched %i option(s) from file \"%s\"",
            n_matched_options, config_file_path);

    return 0;
}

void config_snprintf_value(char *buf, u32 buf_size,
    const union config_value *val, enum config_type val_type)
{
    u_check_params(buf != NULL && val != NULL &&
        val_type > CONFIG_TYPE_UNSET && val_type < CONFIG_N_TYPES);

    switch (val_type) {
    case CONFIG_TYPE_INT:
        snprintf(buf, buf_size, "%li", val->i);
        break;
    case CONFIG_TYPE_FLOAT:
        snprintf(buf, buf_size, "%lf", val->f);
        break;
    case CONFIG_TYPE_BOOL:
        strncpy(buf, val->b ? "true" : "false", buf_size);
        break;
    case CONFIG_TYPE_STRING:
        strncpy(buf, val->str, buf_size);
        break;
    default:
        /* Not possible */
        break;
    }
}

void config_snprintf_section_and_key(char *buf, u32 buf_size,
    const struct config_option *option)
{
    snprintf(buf, buf_size, "%s%s%s",
        option->section, option->section[0] ? "." : "",
        option->key);
}

/* Parse state getters and setters */
static inline i32 ctx_get_n_read(const struct config_parse_ctx *ctx);
static inline u32 ctx_get_char_index(const struct config_parse_ctx *ctx);
static inline char ctx_read_key(const struct config_parse_ctx *ctx, u32 index);
static inline u32 ctx_get_key_strlen(const struct config_parse_ctx *ctx);
static inline void ctx_write_key(struct config_parse_ctx *ctx,
    u32 index, char c);
static inline char ctx_read_value(const struct config_parse_ctx *ctx, u32 index);
static inline u32 ctx_get_value_strlen(const struct config_parse_ctx *ctx);
static inline void ctx_write_value(struct config_parse_ctx *ctx,
    u32 index, char c);
/* static inline char ctx_read_section(const struct config_parse_ctx *ctx,
    u32 index); */
static inline u32 ctx_get_section_strlen(const struct config_parse_ctx *ctx);
static inline void ctx_write_section(struct config_parse_ctx *ctx,
    u32 index, char c);
static inline void ctx_reset_section(struct config_parse_ctx *ctx);
static inline void ctx_set_state(struct config_parse_ctx *ctx,
    enum config_parse_state new_state);
static inline bool ctx_get_flag(const struct config_parse_ctx *ctx,
    enum config_parse_global_ctx_flag flag);
static inline void ctx_set_flag(struct config_parse_ctx *ctx,
    enum config_parse_global_ctx_flag flag, bool value);
static inline const char *
ctx_get_file_path(const struct config_parse_ctx *ctx);
static inline void ctx_increment_char_index(struct config_parse_ctx *ctx);

#define syntax_error(msg) do {                                              \
    s_log_error("Syntax error in file \"%s\" on line %u: %s",               \
        ctx_get_file_path(ctx), ctx->line_number, msg);                     \
    ctx_set_state(ctx, STATE_ERROR);                                        \
    return;                                                                 \
} while (0)

#define is_printable_char(char) (char >= '!' && char <= '~')
#define is_whitespace(char) (char == ' ' || char == '\t')

static void handle_in_nothing(struct config_parse_ctx *ctx)
{
    if (ctx->escaped && ctx->curr_char != '\\')
        return;

    if (ctx->comment) {
        ctx_set_flag(ctx, FLAG_DROP_LINE, true);
        return;
    }

    switch (ctx->curr_char) {
    case '[':
        if (ctx_get_flag(ctx, FLAG_LINE_HAS_SECTION_TAG)) {
            syntax_error("More than one section tags (\"[...]\") "
                    "in a single line");
        }
        ctx_reset_section(ctx);
        ctx_set_state(ctx, STATE_IN_SECTION);
        ctx_set_flag(ctx, FLAG_LINE_HAS_SECTION_TAG, true);
        break;
    case ']':
        syntax_error("Section tag closing (']') "
                "without an opening ('[') in the same line");
    case '=':
        syntax_error("Assignment ('=') to nothing (Empty key)");
    default:
        if (is_printable_char(ctx->curr_char)) {
            s_log_debug("printable char: %#x ('%c')",
                ctx->curr_char, ctx->curr_char);
            ctx_set_state(ctx, STATE_IN_KEY);
            ctx_write_key(ctx, 0, ctx->curr_char);
        }
        /* Whitespaces, newlines and other non-printable characters
         * should be ignored */
        break;
    }
}

static void handle_in_key(struct config_parse_ctx *ctx)
{

    if (ctx->escaped) {
        ctx_write_key(ctx, ctx_get_key_strlen(ctx), ctx->curr_char);
        return;
    }
    if (ctx->comment)
        syntax_error("Comment inside a key");

    switch (ctx->curr_char) {
    default:
        ctx_write_key(ctx, ctx_get_key_strlen(ctx), ctx->curr_char);
        break;
    case '[':
        syntax_error("Section tag opening ('[') inside a key");
    case ']':
        syntax_error("Section tag closing (']') inside a key");
    case '=':
        if (ctx_get_flag(ctx, FLAG_LINE_HAS_KEY_VALUE)) {
            syntax_error("More than one key-value pair "
                    "in a single line");
        }
        ctx_set_flag(ctx, FLAG_LINE_HAS_KEY_VALUE, true);

        /* Strip off the whitespace(s) around the '=' */
        u32 j = ctx_get_char_index(ctx) - 1;
        while (j > 0 && is_whitespace(ctx_read_key(ctx, j)))
            j--;
        if (j == 0)
            syntax_error("Assignment ('=') of nothing (Empty value)");

        if (ctx_read_key(ctx, j) == '\\') /* Respect the escape char */
            j++;
        ctx_write_key(ctx, j + 1, '\0');

        ctx_increment_char_index(ctx); /* Always skip the '=' char itself */
        while (ctx_get_char_index(ctx) <= ctx_get_n_read(ctx) &&
                is_whitespace(ctx->curr_char))
            ctx_increment_char_index(ctx);

        /* Null-terminate the key */
        ctx_write_key(ctx, ctx_get_key_strlen(ctx), '\0');

        /* Skip advancing to the next character in the char loop
         * since we have already done that */
        ctx_set_flag(ctx, FLAG_SKIP_INCREMENT_CHAR_INDEX, true);

        ctx_set_state(ctx, STATE_IN_VALUE);

        break;
    }
}

static void handle_in_value(struct config_parse_ctx *ctx)
{
    /* Special characters lose their meaning when they are escaped
     * or in a comment, so we handle them separately from normal ones */
    /* Also, being escaped and in a comment is by definition mutually exclusive
     * and for us means that different paths should be taken */
#define CHR_ESCAPED ((u16)(255 + 1))
#define CHR_COMMENT ((u16)(255 + 2))
    u16 chr = (u16)ctx->curr_char;
    if (ctx->escaped) chr = CHR_ESCAPED;
    else if (ctx->comment) chr = CHR_COMMENT;
    switch (chr) {
    default:
    case CHR_ESCAPED:
        ctx_write_value(ctx, ctx_get_value_strlen(ctx), ctx->curr_char);
        break;
    case '[':
        syntax_error("Section tag opening ('[') inside a value");
    case ']':
        syntax_error("Section tag closing (']') inside a value");
    case '\'':
        ctx_set_flag(ctx, FLAG_VALUE_IN_SINGLE_QUOTATION,
            !ctx_get_flag(ctx, FLAG_VALUE_IN_SINGLE_QUOTATION));
        break;
    case '"':
        ctx_set_flag(ctx, FLAG_VALUE_IN_DOUBLE_QUOTATION,
            !ctx_get_flag(ctx, FLAG_VALUE_IN_DOUBLE_QUOTATION));
        break;
    case '\n':
    case CHR_COMMENT: /* A comment cuts off the rest of the line */
        if (ctx_get_flag(ctx, FLAG_VALUE_IN_SINGLE_QUOTATION))
            syntax_error("Unmatched single quote");
        if (ctx_get_flag(ctx, FLAG_VALUE_IN_DOUBLE_QUOTATION))
            syntax_error("Unmatched double quote");

        /* If we are in a comment,
         * delete the whitespace preceding the comment character */
        const u32 value_len = ctx_get_value_strlen(ctx);
        const char comment_preceding_char =
            ctx_read_value(ctx, value_len - 1);
        if (ctx->comment && is_whitespace(comment_preceding_char)) {
            ctx_write_value(ctx, value_len - 1, '\0');
        } else {
            ctx_write_value(ctx, value_len, '\0');
        }
        ctx_set_state(ctx, STATE_IN_NOTHING);
        ctx_set_flag(ctx, FLAG_KEY_VALUE_READY, true);
        ctx_set_flag(ctx, FLAG_DROP_LINE, true);
        break;
    };
#undef CHR_ESCAPED
#undef CHR_COMMENT
}

static void handle_in_section(struct config_parse_ctx *ctx)
{
    if (ctx->comment) {
        syntax_error("Comment inside a section tag");
    }
    if (ctx->escaped) {
        ctx_write_section(ctx, ctx_get_section_strlen(ctx), ctx->curr_char);
        return;
    }

    switch (ctx->curr_char) {
    default:
        ctx_write_section(ctx, ctx_get_section_strlen(ctx), ctx->curr_char);
        break;
    case '[':
        syntax_error("Section tag opening ('[') "
                "inside of an existing section tag");
    case ']':
        ctx_set_state(ctx, STATE_IN_NOTHING);
        break;
    case '\n':
        syntax_error("Non-closed section tag (\"[...]\")");
    }
}

#undef is_printable_char
#undef is_whitespace
#undef syntax_error

static i32 match_options(struct config *cfg_o,
    const VECTOR(struct config_option_intermediate) iopts)
{
    i32 n_matched = 0;
    for (u32 i = 0; i < cfg_o->n_options; i++) {
        memset(&cfg_o->options[i].value, 0, sizeof(union config_value));
        cfg_o->options[i].matched = false;

        for (u32 j = 0; j < vector_size(iopts); j++) {
            /* Make sure that both the key and the sections match */
            if (!strncmp(cfg_o->options[i].key, iopts[j].key,
                    CONFIG_KEY_MAX_LEN) &&
                !strncmp(cfg_o->options[i].section, iopts[j].section,
                    CONFIG_SECTION_MAX_LEN))
            {
                if (cfg_o->options[i].matched) {
                    char full_key_buf[CONFIG_FULL_KEY_MAX_LEN] = { 0 };
                    config_snprintf_section_and_key(full_key_buf,
                        CONFIG_FULL_KEY_MAX_LEN, &cfg_o->options[i]);
                    s_log_error("Duplicate key: \"%s\"", full_key_buf);
                    return -1;
                } else if (!try_write_value(&cfg_o->options[i].value,
                        iopts[j].value, cfg_o->options[i].type))
                {
                    cfg_o->options[i].matched = true;
                    n_matched++;
                }
            }
        }
    }

    return n_matched;
}

static i32 try_write_value(union config_value *o,
    const char value_buf[CONFIG_VALUE_MAX_LEN],
    enum config_type desired_value_type)
{
    char *end_p = NULL; /* Temporary variable for `strtoXX` */

    switch (desired_value_type) {
    case CONFIG_TYPE_UNSET:
        break;
    case CONFIG_TYPE_INT:
        errno = 0;
        o->i = strtoll(value_buf, &end_p, 0);
        if (errno || (o->i == 0 && end_p == value_buf))
            return 1;
        break;
    case CONFIG_TYPE_FLOAT:
        errno = 0;
        o->f = strtod(value_buf, &end_p);
        if (errno || (o->f == 0.f && end_p == value_buf))
            return 1;
        break;
    case CONFIG_TYPE_BOOL:
        if (!strncasecmp("true", value_buf, CONFIG_VALUE_MAX_LEN)
            || !strncmp("1", value_buf, CONFIG_VALUE_MAX_LEN))
        {
            o->b = true;
        } else if (!strncasecmp("false", value_buf,
                    CONFIG_VALUE_MAX_LEN)
            || !strncmp("0", value_buf, CONFIG_VALUE_MAX_LEN))
        {
            o->b = false;
        } else {
            return 1;
        }
        break;
    case CONFIG_TYPE_STRING:
        (void) strncpy(o->str, value_buf, CONFIG_VALUE_MAX_LEN);
        break;
    default:
        s_log_fatal(MODULE_NAME, __func__,
            "Value (\"%s\") is of unknown type (%u)",
            value_buf, desired_value_type);
    }
    return 0;
}

struct config_parse_global_ctx {
    const char *file_path;
    char *line;
    u64 line_length;
    i32 n_read;
    u32 char_index;

    char key_buf[CONFIG_KEY_MAX_LEN + 1];
    char value_buf[CONFIG_VALUE_MAX_LEN + 1];
    char section_buf[CONFIG_SECTION_MAX_LEN + 1];

    enum config_parse_state state;

    u32 flags;
};

#define is_whitespace(char) (char == ' ' || char == '\t')

#define goto_error_ret(code, ...) do {  \
    *ret = code;                        \
    s_log_error(__VA_ARGS__);           \
    goto err;                           \
} while (0)

static VECTOR(struct config_option_intermediate)
read_options(const char *file_path, enum config_parse_ret *ret)
{
    FILE *fp = NULL;

    VECTOR(struct config_option_intermediate) iopts = NULL;
    struct config_parse_global_ctx global = {
        .file_path = file_path,
        .state = STATE_IN_NOTHING
    };
    struct config_parse_ctx ctx = {
        .global_ctx_p_ = &global,
    };

    fp = fopen(file_path, "rb");
    if (fp == NULL) {
        goto_error_ret(CONFIG_PARSE_ERR_OPEN_FILE,
            "Couldn't open config file \"%s\" for reading: %s",
            file_path, strerror(errno));
    }

    iopts = vector_new(struct config_option_intermediate);

    global.flags = 0;
    while (global.n_read = getline(&global.line, &global.line_length, fp),
            global.n_read >= 0)
    {
        global.char_index = 0;
        ctx.line_number++;
        ctx.escaped = false;
        ctx.prev_char = '\0';

        /* Only increment `char_index`
         * if the `FLAG_SKIP_INCREMENT_CHAR_INDEX` isn't set,
         * and reset the flag's value back to false */
        for (global.char_index = 0;
            global.char_index < global.n_read;

            global.char_index +=
                    !ctx_get_flag(&ctx, FLAG_SKIP_INCREMENT_CHAR_INDEX),
                ctx_set_flag(&ctx, FLAG_SKIP_INCREMENT_CHAR_INDEX, false))
        {
            ctx.prev_char = ctx.curr_char;
            ctx.curr_char = global.line[global.char_index];
            if (ctx.prev_char == '\\')
                ctx.escaped = !ctx.escaped;
            else
                ctx.escaped = false;

            /* Non-escaped escape characters ('\') should always be skipped */
            if (ctx.curr_char == '\\' && !ctx.escaped)
                continue;

            ctx.comment =
                (is_whitespace(ctx.prev_char) || global.char_index == 0) &&
                (ctx.curr_char == ';' || ctx.curr_char == '#');

            if (ctx.escaped && ctx.curr_char == '\n')
                ctx_set_flag(&ctx, FLAG_ESCAPED_LINE_BREAK, true);

            switch (global.state) {
            case STATE_ERROR: *ret = CONFIG_PARSE_ERR_SYNTAX; goto err;
            case STATE_IN_NOTHING: handle_in_nothing(&ctx); break;
            case STATE_IN_KEY: handle_in_key(&ctx); break;
            case STATE_IN_VALUE: handle_in_value(&ctx); break;
            case STATE_IN_SECTION: handle_in_section(&ctx); break;
            default:
                s_log_fatal(MODULE_NAME, __func__,
                    "Invalid state: %i", global.state);
            }
            if (ctx_get_flag(&ctx, FLAG_DROP_LINE)) {
                s_log_debug("<<< DROP LINE ACK");
                goto done_line;
            }
        }
done_line:;
        if (ctx_get_flag(&ctx, FLAG_KEY_VALUE_READY)) {
            s_log_debug("<<< KEY/VALUE READY ACK");
            ctx_set_flag(&ctx, FLAG_KEY_VALUE_READY, false);

            struct config_option_intermediate tmp = { 0 };
            strncpy(tmp.key, global.key_buf, CONFIG_KEY_MAX_LEN);
            strncpy(tmp.value, global.value_buf, CONFIG_VALUE_MAX_LEN);
            strncpy(tmp.section, global.section_buf, CONFIG_SECTION_MAX_LEN);
            vector_push_back(iopts, tmp);
            s_log_debug("new key/value: %s = %s", global.key_buf, global.value_buf);
            memset(global.key_buf, 0, CONFIG_KEY_MAX_LEN);
            memset(global.value_buf, 0, CONFIG_VALUE_MAX_LEN);
        }
        /* Only reset flags on actual new lines */
        if (!ctx_get_flag(&ctx, FLAG_ESCAPED_LINE_BREAK)) {
            global.flags = 0;
            switch (global.state) {
            case STATE_IN_NOTHING:
                break;
            case STATE_ERROR:
                *ret = CONFIG_PARSE_ERR_SYNTAX;
                goto err;
            case STATE_IN_VALUE:
                s_log_fatal(MODULE_NAME, __func__,
                    "Unhandled new line in value");
            case STATE_IN_KEY:
                ctx_set_state(&ctx, STATE_ERROR);
                goto_error_ret(CONFIG_PARSE_ERR_SYNTAX,
                    "Syntax error in file \"%s\" on line %u: %s",
                    global.file_path, ctx.line_number,
                    "Key with no value");
            case STATE_IN_SECTION:
                ctx_set_state(&ctx, STATE_ERROR);
                goto_error_ret(CONFIG_PARSE_ERR_SYNTAX,
                    "Syntax error in file \"%s\" on line %u: %s",
                    global.file_path, ctx.line_number,
                    "Non-closed section tag (\"[...]\")");
            default:
                s_log_fatal(MODULE_NAME, __func__,
                    "Invalid state (%u)", global.state);
            }
        } else {
            s_log_debug("<<< ESCAPED LINE BREAD ACK");
            ctx_set_flag(&ctx, FLAG_ESCAPED_LINE_BREAK, false);
        }
    }

    /* Clean up */
    u_nfree(&global.line);

    if (ferror(fp)) {
        goto_error_ret(CONFIG_PARSE_ERR_READ_FILE,
            "Error while reading from \"%s\": %s",
            file_path, strerror(errno));
    }
    if (fclose(fp)) {
        goto_error_ret(CONFIG_PARSE_ERR_READ_FILE,
            "Couldn't close the file stream of \"%s\": %s",
            file_path, strerror(errno));
    }
    fp = NULL;

    return iopts;

err:
    ctx.global_ctx_p_ = NULL;
    if (global.line != NULL)
        u_nfree(&global.line);
    if (iopts != NULL)
        vector_destroy(&iopts);
    if (fp != NULL) {
        if (fclose(fp))
            s_log_error("Couldn't close the file stream of \"%s\": %s",
                file_path, strerror(errno));
        fp = NULL;
    }
    return NULL;
}

#undef is_whitespace
#undef goto_error_ret

static inline i32 ctx_get_n_read(const struct config_parse_ctx *ctx)
{
    return ctx->global_ctx_p_->n_read;
}

static inline u32 ctx_get_char_index(const struct config_parse_ctx *ctx)
{
    return ctx->global_ctx_p_->char_index;
}

static inline char ctx_read_key(const struct config_parse_ctx *ctx, u32 index)
{
    u_check_params(index < CONFIG_KEY_MAX_LEN);
    return ctx->global_ctx_p_->key_buf[index];
}

static inline u32 ctx_get_key_strlen(const struct config_parse_ctx *ctx)
{
    return strnlen(ctx->global_ctx_p_->key_buf, CONFIG_KEY_MAX_LEN);
}

static inline void ctx_write_key(struct config_parse_ctx *ctx,
    u32 index, char c)
{
    s_log_debug("write key: %#x ('%c')\t @ %u", c, c, index);
    u_check_params(index < CONFIG_KEY_MAX_LEN);
    ctx->global_ctx_p_->key_buf[index] = c;
}

static inline char ctx_read_value(const struct config_parse_ctx *ctx,
    u32 index)
{
    u_check_params(index < CONFIG_VALUE_MAX_LEN);
    return ctx->global_ctx_p_->value_buf[index];
}

static inline u32 ctx_get_value_strlen(const struct config_parse_ctx *ctx)
{
    return strnlen(ctx->global_ctx_p_->value_buf, CONFIG_VALUE_MAX_LEN);
}

static inline void ctx_write_value(struct config_parse_ctx *ctx,
    u32 index, char c)
{
    s_log_debug("write value: %#x ('%c')\t @ %u", c, c, index);
    u_check_params(index < CONFIG_VALUE_MAX_LEN);
    ctx->global_ctx_p_->value_buf[index] = c;
}

/*
static inline char ctx_read_section(const struct config_parse_ctx *ctx,
    u32 index)
{
    u_check_params(index < CONFIG_SECTION_MAX_LEN);
    return ctx->global_ctx_p_->section_buf[index];
}
*/

static inline u32 ctx_get_section_strlen(const struct config_parse_ctx *ctx)
{
    return strnlen(ctx->global_ctx_p_->section_buf, CONFIG_SECTION_MAX_LEN);
}

static inline void ctx_write_section(struct config_parse_ctx *ctx,
    u32 index, char c)
{
    u_check_params(index < CONFIG_SECTION_MAX_LEN);
    s_log_debug("write section: %#x ('%c') \t@ %u", c, c, index);
    ctx->global_ctx_p_->section_buf[index] = c;
}

static inline void ctx_reset_section(struct config_parse_ctx *ctx)
{
    memset(ctx->global_ctx_p_->section_buf, 0, CONFIG_SECTION_MAX_LEN);
}

static inline void ctx_set_state(struct config_parse_ctx *ctx,
    enum config_parse_state new_state)
{
    u_check_params(new_state >= 0 &&
        new_state < CONFIG_PARSE_N_STATES);
    s_assert(ctx->global_ctx_p_->state > 0 &&
        ctx->global_ctx_p_->state < CONFIG_PARSE_N_STATES,
        "Invalid current state: %u", ctx->global_ctx_p_->state);
    static const u32 legal_transitions[CONFIG_PARSE_N_STATES] = {
        [STATE_ERROR] = 0,

        [STATE_IN_NOTHING] = STATE_ERROR_MASK_
            | STATE_IN_NOTHING_MASK_
            | STATE_IN_KEY_MASK_
            | STATE_IN_SECTION_MASK_,

        [STATE_IN_KEY] = STATE_ERROR_MASK_
            | STATE_IN_VALUE_MASK_,

        [STATE_IN_VALUE] = STATE_ERROR_MASK_
            | STATE_IN_NOTHING_MASK_,

        [STATE_IN_SECTION] = STATE_ERROR_MASK_
            | STATE_IN_NOTHING_MASK_,
    };

    if (!(legal_transitions[ctx->global_ctx_p_->state] & 1 << new_state)) {
        s_log_fatal(MODULE_NAME, __func__,
            "Illegal state transition: %s -> %s",
            config_parse_state_strings[ctx->global_ctx_p_->state],
            config_parse_state_strings[new_state]);
    }

    ctx->global_ctx_p_->state = new_state;
    s_log_debug("SET STATE: %s", config_parse_state_strings[new_state]);
    s_log_debug("section \"%s\"", ctx->global_ctx_p_->section_buf[0] ?
        ctx->global_ctx_p_->section_buf : "(null)");
}

static inline bool ctx_get_flag(const struct config_parse_ctx *ctx,
    enum config_parse_global_ctx_flag flag)
{
    return (ctx->global_ctx_p_->flags & (1U << flag)) > 0;
}

static inline void ctx_set_flag(struct config_parse_ctx *ctx,
    enum config_parse_global_ctx_flag flag, bool value)
{
    if (flag == FLAG_KEY_VALUE_READY && value == true)
        s_log_debug(">>> KEY/VALUE READY!");
    u_check_params(flag >= 0 && flag < CONFIG_PARSE_GLOBAL_CTX_FLAG_MAX);
    if (value)
        ctx->global_ctx_p_->flags |= (1U << flag);
    else
        ctx->global_ctx_p_->flags &= ~(1U << flag);
}

static inline const char *
ctx_get_file_path(const struct config_parse_ctx *ctx)
{
    return ctx->global_ctx_p_->file_path;
}

static inline void ctx_increment_char_index(struct config_parse_ctx *ctx)
{
    ctx->global_ctx_p_->char_index++;
    ctx->curr_char = ctx->global_ctx_p_->line[ctx->global_ctx_p_->char_index];
}
