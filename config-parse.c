#define _POSIX_C_SOURCE 200809L
#include "config-parse.h"
#include <core/log.h>
#include <core/int.h>
#include <core/math.h>
#include <core/util.h>
#include <core/vector.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define MODULE_NAME "config"

enum config_parse_state {
    STATE_FAIL = -1,
    STATE_IN_NOTHING,
    STATE_IN_KEY,
    STATE_IN_VALUE,
    STATE_IN_SECTION,
    CONFIG_PARSE_STATE_MAX
};

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
};

enum config_parse_global_ctx_flag {
    FLAG_LINE_HAS_SECTION_TAG = 1,
    FLAG_LINE_HAS_KEY_VALUE,
    FLAG_KEY_VALUE_READY,
    FLAG_SKIP_INCREMENT_CHAR_INDEX,
    FLAG_DROP_LINE,
    CONFIG_PARSE_GLOBAL_CTX_FLAG_MAX
};

static i32 match_options(struct config *cfg_o,
    const VECTOR(struct config_option_intermediate) intermediate_options);
static i32 try_assign_option(struct config_option *o,
    const struct config_option_intermediate *intermediate_option);
static bool can_value_be_type(const char value_buf[CONFIG_VALUE_MAX_LEN],
    enum config_type desired_type);

static void handle_in_nothing(struct config_parse_ctx *ctx);
static void handle_in_key(struct config_parse_ctx *ctx);
static void handle_in_value(struct config_parse_ctx *ctx);
static void handle_in_section(struct config_parse_ctx *ctx);

/* Everything below has the `struct config_parse_global_ctx`
 * definition available (everything above doesn't) */
static VECTOR(struct config_option_intermediate)
    read_options(const char *file_path);

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
static inline void ctx_set_state(struct config_parse_ctx *ctx,
    enum config_parse_state new_state);
static inline bool ctx_get_flag(const struct config_parse_ctx *ctx,
    enum config_parse_global_ctx_flag flag);
static inline void ctx_set_flag(struct config_parse_ctx *ctx,
    enum config_parse_global_ctx_flag flag, bool value);
static inline const char *
ctx_get_file_path(const struct config_parse_ctx *ctx);
static inline void ctx_increment_char_index(struct config_parse_ctx *ctx);

i32 config_parse(const char *config_file_path, struct config *o)
{
    u_check_params(config_file_path != NULL && o != NULL);
    if (o->n_options > CONFIG_MAX_N_OPTIONS) {
        s_log_error("o->n_options (%u) > CONFIG_MAX_N_OPTIONS (%u)",
            o->n_options, CONFIG_MAX_N_OPTIONS);
        return 1;
    }

    VECTOR(struct config_option_intermediate) option_intermediates =
        read_options(config_file_path);
    if (option_intermediates == NULL)
        goto_error("Failed to read config options from file \"%s\"",
            config_file_path);

#ifndef CGD_BUILDTYPE_RELEASE
    s_log_debug("++++ read_options OK ++++");
    for (u32 i = 0; i < vector_size(option_intermediates); i++) {
        s_log_debug("key/value %u: \"%s\" = \"%s\"", i,
            option_intermediates[i].key, option_intermediates[i].value);
    }
    s_log_debug("+++++++++++++++++++++++++");
#endif /* CGD_BUILDTYPE_RELEASE */

    i32 n_matched_options = match_options(o, option_intermediates);
    if (n_matched_options < 0)
        goto_error("Failed to match options from config file \"%s\"",
            config_file_path);
    else if (n_matched_options == 0)
        s_log_warn("No matched options in configuration file \"%s\"",
            config_file_path);
#ifndef CGD_BUILDTYPE_RELEASE
    else /* if (n_matched_options > 0) */
        s_log_debug("Matched %i option(s) from file \"%s\"",
            n_matched_options, config_file_path);
#endif /* CGD_BUILDTYPE_RELEASE */

    vector_destroy(&option_intermediates);
    return 0;

err:
    if (option_intermediates != NULL)
        vector_destroy(&option_intermediates);
    return 1;
}

#define syntax_error(msg) do {                                              \
    s_log_error("Syntax error in file \"%s\" on line %u: %s",               \
        ctx_get_file_path(ctx), ctx->line_number, msg);                     \
    ctx_set_state(ctx, STATE_FAIL);                                         \
    return;                                                                 \
} while (0)

#define is_printable_char(char) (char >= '!' && char <= '~')
#define is_whitespace(char) (char == ' ' || char == '\t')

static void handle_in_nothing(struct config_parse_ctx *ctx)
{
    if (ctx->curr_char == ']' && !ctx->escaped)
        syntax_error("Section tag closing (']') "
                "without an opening ('[') in the same line");
    if (ctx->curr_char == '[' && !ctx->escaped) {
        if (ctx_get_flag(ctx, FLAG_LINE_HAS_SECTION_TAG)) {
            syntax_error("More than one section tags (\"[...]\") "
                    "in a single line");
        }
        ctx_set_state(ctx, STATE_IN_SECTION);
        ctx_set_flag(ctx, FLAG_LINE_HAS_SECTION_TAG, true);
    }

    if (ctx->curr_char == '=' && !ctx->escaped)
        syntax_error("Assignment ('=') to nothing");

    if (ctx->comment) {
        ctx_set_flag(ctx, FLAG_DROP_LINE, true);
        return;
    }

    if (is_printable_char(ctx->curr_char)) {
        s_log_debug("printable char: 0x%x ('%c')", ctx->curr_char, ctx->curr_char);
        ctx_write_key(ctx, 0, ctx->curr_char);
        ctx_write_key(ctx, 1, '\0');
        ctx_set_state(ctx, STATE_IN_KEY);
    }
}

static void handle_in_key(struct config_parse_ctx *ctx)
{
    if (ctx->curr_char == '[' && !ctx->escaped)
        syntax_error("Section tag opening ('[') inside a value");
    if (ctx->curr_char == ']' && !ctx->escaped)
        syntax_error("Section tag closing (']') inside a value");

    if (ctx->comment)
        syntax_error("Comment inside a key");

    if (ctx->curr_char == '=' && !ctx->escaped) {
        if (ctx_get_flag(ctx, FLAG_LINE_HAS_KEY_VALUE)) {
            syntax_error("More than one key-value pair "
                    "in a single line");
        }
        ctx_set_flag(ctx, FLAG_LINE_HAS_KEY_VALUE, true);
        ctx_set_state(ctx, STATE_IN_VALUE);

        /* Strip off the whitespace around the '=' */
        u32 j = ctx_get_char_index(ctx) - 1;
        while (j > 0 && is_whitespace(ctx_read_key(ctx, j)))
            j--;
        if (j == 0)
            syntax_error("Empty key");

        if (ctx_read_key(ctx, j) == '\\') /* Respect the escape char */
            j++;
        ctx_write_key(ctx, j + 1, '\0');

        ctx_increment_char_index(ctx); /* Always ignore the '=' char itself */
        while (ctx_get_char_index(ctx) <= ctx_get_n_read(ctx) &&
                is_whitespace(ctx->curr_char))
            ctx_increment_char_index(ctx);

        /* Skip advancing to the next character in the for loop
         * since we have already done that */
        ctx_set_flag(ctx, FLAG_SKIP_INCREMENT_CHAR_INDEX, true);

        ctx_write_key(ctx, ctx_get_key_strlen(ctx), '\0');

        return;
    }

    ctx_write_key(ctx, ctx_get_key_strlen(ctx), ctx->curr_char);
}

static void handle_in_value(struct config_parse_ctx *ctx)
{
    if (ctx->curr_char == '[' && !ctx->escaped)
        syntax_error("Section tag opening ('[') inside a key");
    if (ctx->curr_char == ']' && !ctx->escaped)
        syntax_error("Section tag closing (']') inside a key");

    if ((ctx->curr_char == '\n' && !ctx->escaped) || ctx->comment) {
        const u32 value_len = ctx_get_value_strlen(ctx);
        const char comment_preceding_char = ctx_read_value(ctx, value_len - 1);

        /* Delete the whitespace preceding the comment character */
        if (ctx->comment && is_whitespace(comment_preceding_char)) {
            ctx_write_value(ctx, value_len - 1, '\0');
        } else {
            ctx_write_value(ctx, value_len, '\0');
        }
        ctx_set_state(ctx, STATE_IN_NOTHING);
        ctx_set_flag(ctx, FLAG_KEY_VALUE_READY, true);
        ctx_set_flag(ctx, FLAG_DROP_LINE, true);
        return;
    }

    ctx_write_value(ctx, ctx_get_value_strlen(ctx), ctx->curr_char);
}

static void handle_in_section(struct config_parse_ctx *ctx)
{
    if (ctx->comment)
        syntax_error("Comment inside a section tag");

    if (ctx->curr_char == '[' && !ctx->escaped)
        syntax_error("Section tag opening ('[') "
                "inside of an existing section tag");
    if (ctx->curr_char == '\n' && !ctx->escaped)
        syntax_error("Non-closed section tag (\"[...]\")");
    if (ctx->curr_char == ']' && !ctx->escaped)
        ctx_set_state(ctx, STATE_IN_NOTHING);
}

#undef is_printable_char
#undef is_whitespace
#undef syntax_error

static i32 match_options(struct config *cfg_o,
    const VECTOR(struct config_option_intermediate) iopts)
{
    i32 n_matched = 0;
    for (u32 i = 0; i < cfg_o->n_options; i++) {
        for (u32 j = 0; j < vector_size(iopts); j++) {
            if (!strncmp(cfg_o->options[i].key, iopts[j].key,
                    CONFIG_KEY_MAX_LEN)) {
                if (try_assign_option(&cfg_o->options[i], &iopts[j]) == 0)
                    n_matched++;
            }
        }
    }

    return n_matched;
}

static i32 try_assign_option(struct config_option *o,
    const struct config_option_intermediate *intermediate_option)
{
    /* Short-hand */
    const struct config_option_intermediate *const iopt = intermediate_option;

    if (!can_value_be_type(iopt->value, o->type))
        return 1;

    strncpy(o->key, iopt->key, CONFIG_KEY_MAX_LEN);

    switch (o->type) {
        default: case CONFIG_TYPE_UNSET:
            goto_error("Value (\"%s\") is of unknown type", iopt->value);
        case CONFIG_TYPE_INT:
            o->value.i = strtoll(iopt->value, NULL, 0);
            break;
        case CONFIG_TYPE_FLOAT:
            o->value.f = strtod(iopt->value, NULL);
            break;
        case CONFIG_TYPE_BOOL:
            if (!strncasecmp("true", iopt->value, CONFIG_VALUE_MAX_LEN)
                || !strncmp("1", iopt->value, CONFIG_VALUE_MAX_LEN))
            {
                o->value.b = true;
            } else if (!strncasecmp("false", iopt->value,
                        CONFIG_VALUE_MAX_LEN)
                || !strncmp("0", iopt->value, CONFIG_VALUE_MAX_LEN))
            {
                o->value.b = false;
            }            break;
            break;
        case CONFIG_TYPE_STRING:
            strncpy(o->value.str, iopt->value, CONFIG_VALUE_MAX_LEN);
            break;
    }
    return 0;

err:
    return 1;
}

static bool can_value_be_type(const char value_buf[CONFIG_VALUE_MAX_LEN],
    enum config_type desired_type)
{
    /* If a value has any non-escaped quotation chars ("),
     * it can only be a string */
    const char *chr_p = value_buf;
    bool found_unescaped_quote = false;

    bool esc = false;
    do {
        if (*chr_p == '\\') {
            esc = !esc;
        } else if (*chr_p == '"' && !esc) {
            found_unescaped_quote = true;
            break;
        } else {
            esc = false;
        }
    } while (*(++chr_p));
    if (found_unescaped_quote)
        return desired_type == CONFIG_TYPE_STRING;

    /* Temporary variables */
    char *endp;
    union {
        i32 i32_;
        i64 i64_;
        f64 f64_;
    } ret;

    switch (desired_type) {
    default: case CONFIG_TYPE_UNSET:
        return false;
    case CONFIG_TYPE_INT:
        errno = 0;
        endp = NULL;
        ret.i64_ = strtoll(value_buf, &endp, 0);
        if (ret.i64_ == 0 && endp == value_buf)
            return false;
        return errno == 0;
    case CONFIG_TYPE_FLOAT:
        errno = 0;
        endp = NULL;
        ret.f64_ = strtod(value_buf, &endp);
        if (ret.f64_ == 0 && endp == value_buf)
            return false;
        return errno == 0;
    case CONFIG_TYPE_BOOL:
        return !strncasecmp(value_buf, "true", CONFIG_KEY_MAX_LEN) ||
            !strncasecmp(value_buf, "false", CONFIG_KEY_MAX_LEN) ||
            !strncasecmp(value_buf, "0", CONFIG_KEY_MAX_LEN) ||
            !strncasecmp(value_buf, "1", CONFIG_KEY_MAX_LEN);
    case CONFIG_TYPE_STRING:
        return true;
    }
}

#define is_whitespace(char) (char == ' ' || char == '\t')

struct config_parse_global_ctx {
    const char *file_path;
    char *line;
    u64 line_length;
    i32 n_read;
    u32 char_index;

    char key_buf[CONFIG_KEY_MAX_LEN + 1];
    char value_buf[CONFIG_VALUE_MAX_LEN + 1];

    enum config_parse_state state;

    u32 flags;
};

static VECTOR(struct config_option_intermediate)
read_options(const char *file_path)
{
    FILE *fp = NULL;

    VECTOR(struct config_option_intermediate) iopts = NULL;
    struct config_parse_global_ctx global = {
        .file_path = file_path,
    };
    struct config_parse_ctx ctx = {
        .global_ctx_p_ = &global,
    };

    fp = fopen(file_path, "rb");
    if (fp == NULL)
        goto_error("Couldn't open config file \"%s\" for reading: %s",
            file_path, strerror(errno));

    iopts = vector_new(struct config_option_intermediate);

    while (global.n_read = getline(&global.line, &global.line_length, fp),
            global.n_read >= 0)
    {
        global.flags = 0;
        global.char_index = 0;
        ctx.line_number++;

        /* Only increment `char_index`
         * if the `FLAG_SKIP_INCREMENT_CHAR_INDEX` isn't set,
         * and reset the flag's value back to false */
        for (global.char_index = 0;
            global.char_index <= global.n_read;

            global.char_index +=
                    !ctx_get_flag(&ctx, FLAG_SKIP_INCREMENT_CHAR_INDEX),
                ctx_set_flag(&ctx, FLAG_SKIP_INCREMENT_CHAR_INDEX, false))
        {
            ctx.curr_char = global.line[global.char_index];
            ctx.escaped = ctx.prev_char == '\\' && ctx.curr_char != '\\';
            ctx.comment =
                (is_whitespace(ctx.prev_char) || global.char_index == 0) &&
                (ctx.curr_char == ';' || ctx.curr_char == '#');

            if (ctx.escaped && ctx.curr_char == '\n') {
                if (global.state == STATE_IN_KEY ||
                    global.state == STATE_IN_SECTION ||
                    global.state == STATE_IN_NOTHING)
                {
                    goto_error("Escaped line break not in value");
                } else if (global.state == STATE_IN_VALUE) {
                    strncat(global.value_buf, "\n",
                        CONFIG_VALUE_MAX_LEN - strlen(global.value_buf));
                    goto done_line;
                }
            }

            switch (global.state) {
            default: case STATE_FAIL: goto_error("STATE_FAIL is set");
            case STATE_IN_NOTHING: handle_in_nothing(&ctx); break;
            case STATE_IN_KEY: handle_in_key(&ctx); break;
            case STATE_IN_VALUE: handle_in_value(&ctx); break;
            case STATE_IN_SECTION: handle_in_section(&ctx); break;
            }
            if (global.state == STATE_FAIL)
                goto err;
            if (ctx_get_flag(&ctx, FLAG_DROP_LINE)) {
                s_log_debug("<<< DROP LINE ACK");
                goto done_line;
            }

            ctx.prev_char = ctx.curr_char;
        }
done_line:;
        if (ctx_get_flag(&ctx, FLAG_KEY_VALUE_READY)) {
            s_log_debug("<<< KEY/VALUE READY ACK");
            ctx_set_flag(&ctx, FLAG_KEY_VALUE_READY, false);

            struct config_option_intermediate tmp = { 0 };
            strncpy(tmp.key, global.key_buf, CONFIG_KEY_MAX_LEN);
            strncpy(tmp.value, global.value_buf, CONFIG_VALUE_MAX_LEN);
            vector_push_back(iopts, tmp);
            s_log_debug("new key/value: %s = %s", global.key_buf, global.value_buf);
            memset(global.key_buf, 0, CONFIG_KEY_MAX_LEN);
            memset(global.value_buf, 0, CONFIG_VALUE_MAX_LEN);
        }
    }

    /* Clean up */
    u_nfree(&global.line);

    if (ferror(fp))
        goto_error("Error while reading from \"%s\": %s",
            file_path, strerror(errno));
    if (fclose(fp))
        s_log_error("Couldn't close the file stream of \"%s\": %s",
            file_path, strerror(errno));
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
    s_log_debug("write key: 0x%x ('%c')\t @ %u", c, c, index);
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
    s_log_debug("write value: 0x%x ('%c')\t @ %u", c, c, index);
    u_check_params(index < CONFIG_VALUE_MAX_LEN);
    ctx->global_ctx_p_->value_buf[index] = c;
}

static inline void ctx_set_state(struct config_parse_ctx *ctx,
    enum config_parse_state new_state)
{
    u_check_params(new_state >= STATE_FAIL &&
        new_state < CONFIG_PARSE_STATE_MAX);
    ctx->global_ctx_p_->state = new_state;
    s_log_debug("SET STATE: %u", new_state);
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
