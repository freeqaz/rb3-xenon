cmake_minimum_required(VERSION 3.16)
project(xexp-apply CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Path to XenonRecomp checkout. Override with -DXENONRECOMP_ROOT=/abs/path
# if not at the default sibling location.
get_filename_component(_default_xenon
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../XenonRecomp" ABSOLUTE)
set(XENONRECOMP_ROOT "${_default_xenon}" CACHE PATH
    "Path to XenonRecomp source checkout")

if(NOT EXISTS "${XENONRECOMP_ROOT}/XenonUtils/xex_patcher.cpp")
    message(FATAL_ERROR
        "XenonRecomp not found at ${XENONRECOMP_ROOT}. "
        "Pass -DXENONRECOMP_ROOT=/path/to/XenonRecomp")
endif()

set(_xu "${XENONRECOMP_ROOT}/XenonUtils")
set(_tp "${XENONRECOMP_ROOT}/thirdparty")

# Sanity-check submodules.
foreach(_required
        "${_tp}/libmspack/libmspack/mspack/lzxd.c"
        "${_tp}/tiny-AES-c/aes.c"
        "${_tp}/TinySHA1/TinySHA1.hpp")
    if(NOT EXISTS "${_required}")
        message(FATAL_ERROR
            "Missing ${_required}. Run "
            "`git submodule update --init` in ${XENONRECOMP_ROOT}.")
    endif()
endforeach()

add_executable(xexp-apply
    main.cpp
    "${_xu}/xex_patcher.cpp"
    "${_xu}/memory_mapped_file.cpp"
    "${_tp}/libmspack/libmspack/mspack/lzxd.c"
    "${_tp}/tiny-AES-c/aes.c"
)

target_include_directories(xexp-apply PRIVATE
    "${_xu}"
    "${_tp}/libmspack/libmspack/mspack"
    "${_tp}/tiny-AES-c"
    "${_tp}/TinySHA1"
)

# libmspack's lzxd.c expects HAVE_LIMITS_H / HAVE_MEMORY_H type defines via
# system.h. Pull in the minimal config it needs without a full ./configure.
target_compile_definitions(xexp-apply PRIVATE
    HAVE_LIMITS_H=1
    HAVE_MEMORY_H=1
    HAVE_STDLIB_H=1
    HAVE_STRINGS_H=1
    HAVE_STRING_H=1
    HAVE_INTTYPES_H=1
    HAVE_STDINT_H=1
    HAVE_SYS_TYPES_H=1
    HAVE_OFF_T_64=1
)

if(NOT WIN32)
    # xbox.h has `be<uint32_t>` (a struct with a ctor) inside anonymous unions —
    # a Microsoft extension that GCC rejects unless we opt in.
    target_compile_options(xexp-apply PRIVATE
        -Wno-unused-parameter
        $<$<COMPILE_LANGUAGE:CXX>:-fms-extensions>
    )
endif()
