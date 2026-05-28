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

    # Ogg Vorbis codec — ogg.h / codec.h live here; needed by all oggvorbis/ TUs.
    "/I src/system/oggvorbis",

    # RB3 game code (band3/). Mirrors rb3-Wii's "-i src/band3" so that
    # #include "meta_band/Foo.h" resolves from within any band3/ TU.
    "/I src/band3",

    # RB3 network code (network/). Mirrors rb3-Wii's "-i src/network".
    "/I src/network",
]
