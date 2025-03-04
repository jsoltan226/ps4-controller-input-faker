#include "pixel.h"
#include "int.h"
#include "log.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>

#define MODULE_NAME "pixel"

i32 pixel_row_data_init(struct pixel_row_data *out, u32 w, u32 h)
{
    if (out == NULL) {
        s_log_error("invalid parameters");
        return 1;
    }

    out->rows = malloc(h * sizeof(pixel_t *));
    s_assert(out->rows != NULL, "malloc() failed for pixel data rows");
    for (u32 i = 0; i < h; i++) {
        out->rows[i] = calloc(w, sizeof(pixel_t));
        s_assert(out->rows != NULL, "calloc() failed for pixel data row %i", i);
    }

    out->w = w;
    out->h = h;
    return 0;
}

void pixel_row_data_destroy(struct pixel_row_data *data)
{
    if (data == NULL) return;

    if (data->rows != NULL) {
        for (u32 i = 0; i < data->h; i++)
            u_nfree(&data->rows[i]);

        u_nfree(&data->rows);
    }
    memset(data, 0, sizeof(struct pixel_row_data));
}

void pixel_data_row2flat(struct pixel_row_data *in, struct pixel_flat_data *out)
{
    if (in == NULL || out == NULL) return;

    out->buf = malloc(in->w * in->h * sizeof(pixel_t));
    s_assert(out->buf != NULL, "malloc() failed for pixel data buf");

    for (u32 y = 0; y < in->h; y++)
        memcpy(&out->buf[y * in->w], in->rows[y], in->w * sizeof(pixel_t));

    out->w = in->w;
    out->h = in->h;
}

void pixel_data_flat2row(struct pixel_flat_data *in, struct pixel_row_data *out)
{
    if (in == NULL || out == NULL) return;

    out->rows = malloc(in->h * sizeof(pixel_t *));
    s_assert(out->rows != NULL, "malloc() failed for pixel data rows");

    for (u32 y = 0; y < in->h; y++) {
        out->rows[y] = malloc(in->w * sizeof(pixel_t));
        s_assert(out->rows != NULL, "malloc() failed for pixel data row %i", y);
        memcpy(out->rows[y], &in->buf[y * in->w], in->w * sizeof(pixel_t));
    }

    out->w = in->w;
    out->h = in->h;
}
