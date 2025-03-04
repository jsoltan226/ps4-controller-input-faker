#ifndef UTIL_H_
#define UTIL_H_

#include "log.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#define u_BUF_SIZE  1024
#define u_PATH_FROM_BIN_TO_ASSETS "../assets/"

#define u_FILEPATH_MAX 256
typedef const char filepath_t[u_FILEPATH_MAX];


#define goto_error(...) do {    \
    s_log_error(__VA_ARGS__);   \
    goto err;                   \
} while (0)

#define u_check_params(expr) s_assert(expr, "invalid parameters");

#define u_color_arg_expand(color) color.r, color.g, color.b, color.a

#define u_arr_size(arr) (sizeof(arr) / sizeof(*arr))

#define u_strlen(str_literal) (sizeof(str_literal) - 1)

#define u_nbits(x) ((((x) - 1) / (8 * sizeof(u64))) + 1)

#define u_rgba_swap_b_r(color) do {     \
    const register u8 tmp = color.b;    \
    color.b = color.r;                  \
    color.r = tmp;                      \
} while (0)

/* Free and nullify */
#define u_nfree(ptr_ptr) do {   \
    free(*(ptr_ptr));           \
    *(ptr_ptr) = NULL;          \
} while (0)

/* Zero-out and free */
#define u_zfree(ptr_ptr) do {                   \
    memset(*(ptr_ptr), 0, sizeof(**(ptr_ptr))); \
    free(*(ptr_ptr));                           \
} while (0)

/* Zero-out, free and nullify */
#define u_nzfree(ptr_ptr) do {                  \
    memset(*(ptr_ptr), 0, sizeof(**(ptr_ptr))); \
    free(*(ptr_ptr));                           \
    *(ptr_ptr) = NULL;                          \
} while (0)

#endif /* UTIL_H_ */
