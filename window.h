#ifndef P_WINDOW_H_
#define P_WINDOW_H_

#include <core/int.h>
#include <core/pixel.h>
#include <core/shapes.h>
#include <stdbool.h>

struct p_window;

#define P_WINDOW_FLAG_LIST                                                  \
    /** WINDOW TYPES **/                                                    \
    /* The default */                                                       \
    X_(P_WINDOW_TYPE_NORMAL, 0)                                             \
                                                                            \
    /* Only used for testing purposes.                                      \
     * Contradicts with `P_WINDOW_NORMAL` (obviously)                       \
     * and doesn't support any GPU acceleration. */                         \
    X_(P_WINDOW_TYPE_DUMMY, 3)                                              \
                                                                            \
    /** WINDOW AREA AND POSITIONING **/                                     \
    /* Makes `p_window_open` ignore the `x` or `y` fields                   \
     * specified in the `area` parameter, and instead sets                  \
     * the given coordinate so that the window spawns exactly               \
     * at the center of the screen */                                       \
    X_(P_WINDOW_POS_CENTERED_X, 10)                                         \
    X_(P_WINDOW_POS_CENTERED_Y, 11)                                         \
                                                                            \
    /** VSYNC **/                                                           \
    /* Makes `p_window_open` fail if vsync is not supported. */             \
    X_(P_WINDOW_VSYNC_SUPPORT_REQUIRED, 15)                                 \
                                                                            \
    /* A warning is logged by `p_window_open` if vsync is not supported,    \
     * but the initialization will still proceed.                           \
     *                                                                      \
     * If vsync is to be enabled in the future, call `p_window_get_meta`    \
     * to check whether it's actually supported.                            \
     *                                                                      \
     * Obviously it's mututally exclusive with the previous flag.           \
     *                                                                      \
     * This is the default. */                                              \
    X_(P_WINDOW_VSYNC_SUPPORT_OPTIONAL, 16)                                 \
                                                                            \
    /** GPU ACCELERATION FLAGS **/                                          \
    /* The fallback order is as follows:                                    \
     * Vulkan -> OpenGL -> Software (none) -> FAIL.                         \
     *                                                                      \
     * Note that these flags contradict each other                          \
     * (i.e. you can only pick one of them) */                              \
                                                                            \
    /* Use the standard fallback order. This is the default behavior        \
     * if none of the other acceleration flags are specified. */            \
    X_(P_WINDOW_PREFER_ACCELERATED, 20)                                     \
                                                                            \
    /* Fail instead of falling back to software */                          \
    X_(P_WINDOW_REQUIRE_ACCELERATED, 21)                                    \
                                                                            \
    /* Only (try to) initialize Vulkan */                                   \
    X_(P_WINDOW_REQUIRE_VULKAN, 22)                                         \
                                                                            \
    /* Only (try to) initialize OpenGL */                                   \
    X_(P_WINDOW_REQUIRE_OPENGL, 23)                                         \
                                                                            \
    /* Only (try to) initialize software rendering */                       \
    X_(P_WINDOW_NO_ACCELERATION, 24)                                        \
                                                                            \


#define P_WINDOW_POS_CENTERED_XY \
    (P_WINDOW_POS_CENTERED_X | P_WINDOW_POS_CENTERED_Y)

#define X_(name, id) name = 1 << id,
enum p_window_flags {
    P_WINDOW_FLAG_LIST
};
#undef X_

#define P_WINDOW_MAX_N_FLAGS_ (sizeof(enum p_window_flags) * 8)
#define X_(name, id) [id] = #name,
static const char *const p_window_flag_strings[P_WINDOW_MAX_N_FLAGS_] = {
    P_WINDOW_FLAG_LIST
};
#undef X_
#ifndef P_INTERNAL_GUARD__
#undef P_WINDOW_MAX_N_FLAGS_
#endif /* P_INTERNAL_GUARD__ */

enum p_window_acceleration {
    P_WINDOW_ACCELERATION_UNSET_ = -1,
    P_WINDOW_ACCELERATION_NONE,
    P_WINDOW_ACCELERATION_OPENGL,
    P_WINDOW_ACCELERATION_VULKAN,
    P_WINDOW_ACCELERATION_MAX_
};

/* Opens a new window named `title`,
 * with position and dimensions given in `area`,
 * and additional configuration parameters `flags`.
 *
 * Returns a handle to the new window on success,
 * and `NULL` on failure.
 */
struct p_window * p_window_open(const char *title,
    const rect_t *area, const u32 flags);

/* Only works for windows that use software rendering.
 * Returns the current back buffer on success and `NULL` on failure. */
enum p_window_present_mode {
    P_WINDOW_PRESENT_VSYNC = 0,
    P_WINDOW_PRESENT_NOW = 1,
};
struct pixel_flat_data * p_window_swap_buffers(struct p_window *win,
    const enum p_window_present_mode present_mode);

/* Closes, destroys and sets to `NULL` the window that `win_p` points to */
void p_window_close(struct p_window **win_p);

/* Retrieves information about the window `win` into `out`. */
struct p_window_info {
    rect_t client_area;
    rect_t display_rect;
    pixelfmt_t display_color_format;
    enum p_window_acceleration gpu_acceleration;
    bool vsync_supported;
};
void p_window_get_info(const struct p_window *win, struct p_window_info *out);

/* Sets the GPU acceleration mode in `win` to `new_acceleration_mode`.
 *
 * Destroys/deallocates everything associated with the previous acceleration
 * mode, and initialized the new one.
 *
 * If at any point something goes wrong, a non-zero value is returned
 * and the acceleration mode becomes `P_WINDOW_ACCELERATION_UNSET_`.
 * On success, 0 is returned.
 */
i32 p_window_set_acceleration(struct p_window *win,
    enum p_window_acceleration new_acceleration_mode);

#undef P_WINDOW_FLAG_LIST
#endif /* P_WINDOW_H_ */
