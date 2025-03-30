#include "vector.h"
#include "int.h"
#include "log.h"
#include "math.h"
#include "util.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

typedef struct vector_metadata__ vector_meta_t;

#define MODULE_NAME "vector"

#define VECTOR_DEFAULT_CAPACITY 8

#define get_metadata_ptr(v) \
    ((vector_meta_t *)(((u8 *)v) - sizeof(vector_meta_t)))

#define element_at(v, at) (((u8 *)v) + (at * get_metadata_ptr(v)->item_size))

void * vector_init(u32 item_size)
{
    void *v = malloc(sizeof(vector_meta_t) + (item_size * VECTOR_DEFAULT_CAPACITY));
    s_assert(v != NULL, "malloc() failed for vector");
    memset(v, 0, sizeof(vector_meta_t) + (item_size * VECTOR_DEFAULT_CAPACITY));

    vector_meta_t *metadata_ptr = (vector_meta_t *)v;
    *(u32*)(&metadata_ptr->item_size) = item_size; /* Cast away `const` */
    metadata_ptr->n_items = 0;
    metadata_ptr->capacity = VECTOR_DEFAULT_CAPACITY;

    u8 *const vector_base = ((u8 *)v) + sizeof(vector_meta_t);
    return vector_base;
}

void * vector_increase_size__(void *v)
{
    if (v == NULL) return NULL;

    vector_meta_t *meta = get_metadata_ptr(v);

    if (meta->n_items >= meta->capacity) {
        u32 new_cap = meta->capacity;
        if (new_cap > 0)
            new_cap *= 2;
        else
            new_cap++;

        v = vector_realloc__(v, new_cap);

        /* `meta` might have been moved by `realloc()` */
        meta = get_metadata_ptr(v);
        meta->capacity = new_cap;
    }

    meta->n_items++;
    return v;
}

void * vector_pop_back__(void *v)
{
    if (v == NULL) return NULL;

    vector_meta_t *meta = get_metadata_ptr(v);
    /* Do nothing if the vector is empty */
    if (meta->n_items == 0) return v;

    meta->n_items--;
    memset(element_at(v, meta->n_items), 0, meta->item_size);

    if (meta->n_items <= (meta->capacity / 2)) {
        v = vector_realloc__(v, meta->capacity / 2);
        meta = get_metadata_ptr(v);
    }

    return v;
}

void vector_memmove__(void *v, u32 src_index, u32 dst_index, u32 nmemb)
{
    u_check_params(v != NULL);

    if (src_index == dst_index || nmemb == 0)
        return;

    vector_meta_t *meta = get_metadata_ptr(v);
    /* Don't do anything of the vector doesn't have enough spare capacity */
    if (src_index + nmemb > meta->n_items || dst_index + nmemb > meta->capacity)
        return;

    memmove(
        element_at(v, dst_index),
        element_at(v, src_index),
        nmemb * meta->item_size
    );
}

bool vector_empty(void *v)
{
    return v == NULL ? true : get_metadata_ptr(v)->n_items == 0;
}

u32 vector_capacity(void *v)
{
    return v == NULL ? 0 : get_metadata_ptr(v)->capacity;
}

void * vector_end(void * v)
{
    if (v == NULL) return NULL;
    vector_meta_t *meta = get_metadata_ptr(v);
    return ((u8 *)v) + (meta->n_items * meta->item_size);
}

void * vector_shrink_to_fit__(void *v)
{
    if (v == NULL) return NULL;

    vector_meta_t *meta = get_metadata_ptr(v);
    v = vector_realloc__(v, meta->n_items);

    meta = get_metadata_ptr(v);
    meta->capacity = meta->n_items;

    return v;
}

void vector_clear(void *v)
{
    if (v == NULL) return;

    vector_meta_t *meta = get_metadata_ptr(v);

    memset(v, 0, meta->n_items * meta->item_size);
    meta->n_items = 0;
}

void * vector_erase__(void *v, u32 index)
{
    u_check_params(v != NULL);

    vector_meta_t *meta = get_metadata_ptr(v);

    if (index >= meta->n_items) return v;

    /* Move all memory right of `index` one spot to the left, and delete the last spot */
    vector_memmove__(v, index + 1, index, meta->n_items - index - 1);
    return vector_pop_back__(v);
}

void * vector_realloc__(void *v, u32 new_cap)
{
    /* Unfortunately if `v` is NULL we do not know the element size,
     * and so we cannot make it work like realloc(NULL, size) would
     */
    if (v == NULL) return NULL;

    void *new_v = NULL;

    vector_meta_t *meta_p = get_metadata_ptr(v);
    new_v = realloc(meta_p,
        (new_cap * meta_p->item_size) + sizeof(vector_meta_t));


    s_assert(new_v != NULL, "realloc() failed!");
    meta_p = new_v;
    meta_p->capacity = new_cap;

    return ((u8 *)new_v) + sizeof(vector_meta_t);
}

void * vector_resize__(void *v, u32 new_size)
{
    u_check_params(v != NULL);

    vector_meta_t *meta = get_metadata_ptr(v);

    /* Clean up the items that are to be cut off */
    if (new_size < meta->capacity)
        memset(element_at(v, new_size), 0,
            (meta->capacity - new_size) * meta->item_size
        );

    v = vector_realloc__(v, new_size);
    if (v == NULL)
        return NULL;

    meta = get_metadata_ptr(v);
    meta->n_items = u_min(new_size, meta->n_items);

    return v;
}

void * vector_clone(void *v)
{
    if (v == NULL) return NULL;

    vector_meta_t *meta_p = get_metadata_ptr(v);

    void *new_v = vector_init(meta_p->item_size);
    if (new_v == NULL) return NULL;

    new_v = vector_realloc__(new_v, meta_p->capacity);

    memcpy(get_metadata_ptr(new_v), meta_p,
        (meta_p->capacity * meta_p->item_size) + sizeof(vector_meta_t)
    );

    return new_v;
}

void vector_free__(void *v)
{
    if (v == NULL) return;

    vector_meta_t *meta_ptr = get_metadata_ptr(v);

    /* Reset the entire vector, including the metadata */
    memset(meta_ptr, 0, sizeof(vector_meta_t) + meta_ptr->capacity);
    free(meta_ptr);
}
