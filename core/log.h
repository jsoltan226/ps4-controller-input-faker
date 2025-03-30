#ifndef LOG_H_
#define LOG_H_

#include <stdbool.h>
#include <stdio.h>
#include <stdnoreturn.h>
#include "int.h"

#define S_LOG_LEVELS_LIST   \
    X_(LOG_FATAL, 0)        \
    X_(LOG_ERROR, 1)        \
    X_(LOG_WARNING, 2)      \
    X_(LOG_INFO, 3)         \
    X_(LOG_DEBUG, 4)        \


#define X_(name, value) name = value,
enum s_log_level {
    S_LOG_LEVELS_LIST
};
#undef X_

#ifndef S_LOG_LEVELS_LIST_DEF__
#undef S_LOG_LEVELS_LIST
#endif /* S_LOG_LEVELS_LIST_DEF__ */

void s_log(enum s_log_level level, const char *module_name,
    const char *fmt, ...);

#ifndef NDEBUG
#define s_log_debug(...) \
    s_log(LOG_DEBUG, MODULE_NAME, __VA_ARGS__)
#else
#define s_log_debug(...)
#endif /* NDEBUG */

#define s_log_info(...) \
    s_log(LOG_INFO, MODULE_NAME, __VA_ARGS__)

#define s_log_warn(...) \
    s_log(LOG_WARNING, MODULE_NAME, __VA_ARGS__)

#define s_log_error(...) \
    s_log(LOG_ERROR, MODULE_NAME, __VA_ARGS__)

noreturn void s_log_fatal(const char *module_name, const char *function_name,
    const char *fmt, ...);

#define s_assert(expr, /* msg on fail */...) do {                   \
    if (!(expr)) {                                                  \
        s_log_error("Assertion failed: '%s'", #expr);               \
        s_log_fatal(MODULE_NAME, __func__, __VA_ARGS__);            \
    }                                                               \
} while (0);

void s_set_log_level(enum s_log_level new_log_level);
enum s_log_level s_get_log_level(void);

i32 s_set_log_out_file(const char *file_path);
i32 s_set_log_out_filep(FILE *fp);
#define s_set_log_file s_set_log_out_file
#define s_set_log_filep s_set_log_out_filep

i32 s_set_log_err_file(const char *file_path);
i32 s_set_log_err_filep(FILE *fp);

void s_close_out_log_fp(void);
void s_close_err_log_fp(void);

/* This is used in the error handling system to determine
 * whether the error was the user's fault
 * (e.g a non-existent file was given as input)
 * to decide if usage should be printed after the error message
 */
#define YES_USER_FAULT  true
#define NO_USER_FAULT   false
void s_set_user_fault(bool is_user_fault);
bool s_get_user_fault(void);

/* A shortcut for configuring logging, especially in tests */
#define s_configure_log(level, outfilep, errfilep) do { \
    s_set_log_level(level);                             \
    s_set_log_out_filep(outfilep);                      \
    s_set_log_err_filep(errfilep);                      \
} while (0);

#endif /* LOG_H_ */
