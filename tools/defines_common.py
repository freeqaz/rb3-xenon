# Game versions
DEFAULT_VERSION = 0
VERSIONS = [
    "45410914",  # 0
]

# Include paths
cflags_includes = [
    # C++ stdlib. STLport must come FIRST in the include path list so its
    # headers (<algorithm>, <vector>, ...) shadow the native CRT. Vendored
    # from dc3-decomp (already configured for X360 MSVC).
    "/I src/system/stlport",

    # C stdlib (Xbox 360 XDK CRT). Vendored from dc3-decomp; ships
    # math.h, stdio.h, etc. that the X360 cl.exe expects.
    "/I src/xdk/LIBCMT",

    # Project source — mirrors rb3-Wii's layout so #include "math/..." and
    # #include "os/..." resolve the same way under MSVC X360.
    "/I src",
    "/I src/system",
]
