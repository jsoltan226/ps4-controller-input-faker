#ifndef VECTOR_H_
#define VECTOR_H_
#include "static-tests.h"

#include "int.h"
#include "log.h"
#include <stdbool.h>
#include <stdlib.h>

struct vector_metadata__ {
    const u32 item_size;
    u32 n_items;
    u32 capacity;
};

/* Used for a more clean declaring of vector variables */
#define VECTOR(T) T *

/* Create a new vector of type `T` */
#define vector_new(T) ((T *)vector_init(sizeof(T)))
void * vector_init(u32 item_size);

/* Get the element at `index` from `v` */
/* We are using C, so we can forget about array bounds checking :) */
#define vector_at(v, index) (v[index])

/* Append `item` to `v` */
#define vector_push_back(v, ...) do {                                   \
    if (v == NULL)                                                      \
        s_log_fatal("vector", "vector_push_back", "invalid parameters");\
    v = vector_increase_size__(v);                                      \
    v[vector_size(v) - 1] = __VA_ARGS__;                                \
} while (0)
void * vector_increase_size__(void *v);

/* Remove the last element from `v` */
#define vector_pop_back(v) do { v = vector_pop_back__(v); } while (0)
void * vector_pop_back__(void *v);

/* Insert `item` to `v` at index `at` */
#define vector_insert(v, at, ...) do {                                  \
    if (v == NULL || (i32)at < 0 || at > vector_size(v))                \
        s_log_fatal("vector", "vector_insert", "invalid parameters");   \
    v = vector_increase_size__(v);                                      \
    vector_memmove__(v, at, at + 1, vector_size(v) - at);               \
    v[at] = __VA_ARGS__;                                                \
} while (0)
/* Won't do anything if the vector doesn't have enough spare capacity */
void vector_memmove__(void *v, u32 src_index, u32 dst_index, u32 nmemb);

/* Remove element from `v` at index `at` */
#define vector_erase(v, at) do { v = vector_erase__(v, at); } while (0)
void * vector_erase__(void *v, u32 at);

/* Return the pointer to the first element of `v` */
#define vector_begin(v) (v)

/* Return the first element of `v` */
#define vector_front(v) (v ? v[0] : 0)

/* Return the pointer to the element immidiately after the last one in `v` */
void * vector_end(void *v);

/* Return the last element */
#define vector_back(v) (vector_at(v, vector_size(v) - 1))

/* Check whether `v` is empty */
bool vector_empty(void *v);

/* Return the size of `v` (number of elements) */
#define vector_size(v) (((struct vector_metadata__ *)v)[-1].n_items)

/* Return the allocated capacity of `v` */
u32 vector_capacity(void *v);

/* Shrink `capacity` to `size` */
#define vector_shrink_to_fit(v) do { v = vector_shrink_to_fit__(v); } while(0)
void * vector_shrink_to_fit__(void *v);

/* Reset the size of `v` and memset is to 0,
 * but leave the allocated capacity unchanged */
void vector_clear(void *v);

/* Works the same as `realloc()`, but takes into account the vector metadata,
 * although note that `vector_realloc__(NULL, new_capacity)` won't do anything! */
/* This function is not intended to be used by the user */
void * vector_realloc__(void *v, u32 new_capacity);

/* Increase the capacity of `v` to `count`
 * (if `count` is greater than the capacity of `v`) */
#define vector_reserve(v, count) do {                                   \
    if (v == NULL)                                                      \
        s_log_fatal("vector", "vector_reserve", "invalid parameters");  \
    if (count > vector_size(v))                                         \
        v = vector_realloc__(v, count);                                 \
} while (0)

/* Resize `v` to `new_size`,
 * cutting off any elements at index greater than `new_size` */
#define vector_resize(v, new_size) do { v = vector_resize__(v, new_size); } while (0)
void * vector_resize__(void *v, u32 new_size);

#define vector_copy vector_clone
void * vector_clone(void *v);

/* Destroy the vector that `v_p` points to. */
#define vector_destroy(v_p) do {        \
    vector_free__(&(**v_p));            \
    *(v_p) = NULL;                      \
} while (0)
void vector_free__(void *v);

#endif /* VECTOR_H_ */
