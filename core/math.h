#ifndef CORE_MATH_H_
#define CORE_MATH_H_

#include "int.h"
#include "pixel.h"
#include "shapes.h"
#include <stdbool.h>

#define u_min(a, b) (a < b ? a : b)
#define u_max(a, b) (a > b ? a : b)
#define u_clamp(x, min, max) (u_min(u_max(x, min), max))

/* The simplest collision checking implementation;
 * returns true if 2 rectangles overlap
 */
static inline bool u_collision(
    const rect_t *r1,
    const rect_t *r2
)
{
    return (
        r1->x <= r2->x + r2->w &&
        r1->x + r1->w >= r2->x &&
        r1->y <= r2->y + r2->h &&
        r1->y + r1->h >= r2->y
    );
}

/* Converts a fixed point 16.16 number to float 32 */
static const inline f32 u_fp1616_to_f32(const i32 num)
{
    f32 ret = (f32)num;
    ret = ret / (f32)(1 << 16);
    return ret;
}

static inline void u_rect_from_pixel_data(
    const struct pixel_flat_data *data,
    rect_t *o
)
{
    o->x = 0;
    o->y = 0;
    o->w = data->w;
    o->h = data->h;
}

#endif /* CORE_MATH_H_ */
