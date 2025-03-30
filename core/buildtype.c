#include "buildtype.h"

const char *get_cgd_buildtype__(void)
{
    static volatile const char *cgd_buildtype__ =
#ifdef CGD_BUILDTYPE_RELEASE
    "CGD_BUILDTYPE_RELEASE__";
#else
    "CGD_BUILDTYPE_DEBUG__";
#endif /* CGD_BUILDTYPE_RELEASE */
    return (const char *)cgd_buildtype__;
}
