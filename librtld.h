#ifndef P_LIBRTLD_H_
#define P_LIBRTLD_H_

#include <core/int.h>

/* Platform LibRtLd - library runtime loader
 *
 * This API provides a way to load dynamic libraries at runtime
 * as well as retrieve function pointers (symbols) from them.
 *
 * It's basiacally a wrapper around a given platform's dynamic loading API -
 * `libdl` on linux and `libloaderapi` on windows.
 */

/* Represents a handle to a shared library */
struct p_lib;

/* Just loads the library, doesn't look up any symbols.
 * Returns NULL if the library could't be loaded.
 *
 * This function provides more control over the library file name;
 * it allows you to control various parameters used in constructing
 * the actual file name of the library.
 *
 * If `suffix` is NULL, the platform's default extension will be used.
 * Obviously, for linux and android it's ".so", and for windows - ".dll".
 *
 * If `prefix` is NULL, here too the platform's default will be used.
 * However there's a catch - if the libname doesn't begin with the prefix,
 * the function will try to load the file without the prefix included
 * if the one with it is not found. So for example, on both linux and windows,
 * the default prefix is "lib", so when attempting to load "test"
 * (not "libtest"!), the function will first try to load "test.so", and then,
 * if that fails, will look for "libtest.so".
 *
 * If `version_string` is NULL, nothing is inserted in it's place.
 * It's a bit special, as depending on the platform, it will be put
 * in different places in the string.
 *
 * On linux, the template for the filename is as follows:
 *  <prefix><filename><ext>.<version_string>
 * On windows, it's like this:
 *  <prefix><filename>-<version_string><ext>
 *
 * Notice the '-' in windows and '.' in linux - they won't get inserted there
 * at all if `version_string` is NULL.
 *
 * Example: libname: "vulkan", prefix: NULL, suffix: NULL, version_string: "1"
 *  on linux will try to load "vulkan.so.1" and, if that fails, "libvulkan.so.1"
 *  and on windows "vulkan-1.dll" and then "libvulkan-1.dll".
 *
 * I Hope that makes it a little bit clearer.
 *
 */
struct p_lib * p_librtld_load_lib_explicit(const char *libname,
    const char *prefix, const char *suffix, const char *version_string);

/* Load the shared library named `libname` and from it retrieve all symbols
 * whose names are in the NULL-terminated array `symnames`.
 *
 * The function will append the platform's default extension to the libname -
 * `.so` for linux/android and `.dll` for windows, as well as the prefix
 * ("lib" for both platforms, so e.g. "png" -> "libpng.so" on linux)
 *
 * Returns NULL on failure - either if the library itself can't be loaded,
 * or if AT LEAST ONE of the symbols in `symnames` fails to be retrieved.
 */
struct p_lib * p_librtld_load(const char *libname, const char *const *symnames);

/* (Try to) load a symbol named `symname` from the shared library `lib`.
 *
 * The function first tries to look up the symbol in the library's sym list.
 * If nothing matching the `symname` is found there, it proceeds to trying
 * to load it from the shared library file itself.
 *
 * Then, if no symbol can be retrieved (most likely it doesn't exist),
 * NULL is returned. Otherwise, the function pointer is stored in `lib`'s
 * symbol list, and then returned.
 */
void * p_librtld_load_sym(struct p_lib *lib, const char *symname);

/* Closes and frees all memory associated with what `lib_p` points to,
 * and sets the value of the handle (`*lib_p`) to NULL.
 */
void p_librtld_close(struct p_lib **lib_p);

#endif /* LIBRTLD_H_ */
