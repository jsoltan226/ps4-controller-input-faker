#include "log.h"
#include "int.h"
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>

#define MODULE_NAME "log"

static FILE *out_log_file = NULL, *err_log_file = NULL;
static bool user_fault = NO_USER_FAULT;
static s_log_level current_log_level = LOG_INFO;

void s_log(s_log_level level, const char *module_name, const char *fmt, ...)
{
    if (level < current_log_level)
        return;

    if (err_log_file == NULL) {
        s_set_log_err_filep(stderr);
        s_log_warn("The error log file was unset; setting it to stderr");
    }
    if (out_log_file == NULL) {
        s_set_log_out_filep(stdout);
        s_log_warn("The out log file was unset; setting it to stdout");
    }

    FILE *fp = level >= LOG_WARNING ? err_log_file : out_log_file;

    if (level == LOG_WARNING) fprintf(fp, "WARNING: ");
    else if (level == LOG_ERROR) fprintf(fp, "ERROR: ");

    fprintf(fp, "[%s] ", module_name);

    va_list vArgs;
    va_start(vArgs, fmt);
    vfprintf(fp, fmt, vArgs);
    va_end(vArgs);
    fprintf(fp, "\n");
}

noreturn void s_log_fatal(const char *module_name, const char *function_name,
    const char *fmt, ...)
{
    fprintf(err_log_file, "[%s] FATAL ERROR: %s: ", module_name, function_name);

    va_list vArgs;
    va_start(vArgs, fmt);
    vfprintf(err_log_file, fmt, vArgs);
    va_end(vArgs);
    fprintf(err_log_file, "\nFatal error encountered. Calling abort().\n");

    s_close_out_log_fp();
    s_close_err_log_fp();
    abort();
}

i32 s_set_log_out_file(const char *file_path)
{
    out_log_file = fopen(file_path, "wb");
    if (out_log_file == NULL) {
        s_log_error("Failed to open out log file '%s': %s", file_path, strerror(errno));
        return 1;
    }

    return 0;
}

i32 s_set_log_out_filep(FILE *fp)
{
    if (fp == NULL) {
        s_log_warn("Not changing out log file to NULL", NULL);
        return 1;
    }
    out_log_file = fp;
    return 0;
}

i32 s_set_log_err_file(const char *file_path)
{
    err_log_file = fopen(file_path, "wb");
    if (err_log_file == NULL) {
        s_log_error("Failed to open error log file '%s': %s", file_path, strerror(errno));
        return 1;
    }

    return 0;
}

i32 s_set_log_err_filep(FILE *fp)
{
    if (fp == NULL) {
        s_log_warn("Not changing error log file to NULL", NULL);
        return 1;
    }
    err_log_file = fp;
    return 0;
}

void s_set_log_level(s_log_level new_log_level)
{
    current_log_level = new_log_level;
}

s_log_level s_get_log_level(void)
{
    return current_log_level;
}

void s_set_user_fault(bool is_user_fault)
{
    user_fault = is_user_fault;
}

bool s_get_user_fault(void)
{
    return user_fault;
}

void s_close_out_log_fp(void)
{
    if (out_log_file != NULL
        && out_log_file != stdout
        && out_log_file != stderr
        && out_log_file != stdin
    ) {
        fflush(out_log_file);
        fclose(out_log_file);
        out_log_file = NULL;
    }
}

void s_close_err_log_fp(void)
{
    if (err_log_file != NULL
        && err_log_file != stdout
        && err_log_file != stderr
        && err_log_file != stdin
    ) {
        fflush(err_log_file);
        fclose(err_log_file);
        err_log_file = NULL;
    }
}
