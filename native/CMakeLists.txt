cmake_minimum_required(VERSION 3.16)
project(rb3-xenon-native CXX C ASM)

# Requires Clang — the Milo engine is decompiled MSVC code and needs MSVC
# compatibility flags (-fms-extensions, -fms-compatibility) to compile.
if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(WARNING "rb3-xenon native build expects Clang; got ${CMAKE_CXX_COMPILER_ID}")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

set(REPO_ROOT ${CMAKE_SOURCE_DIR}/..)

# --- MSVC-compat + decomp flags (mirrors dc3-decomp native build) ---
set(MS_COMPAT_FLAGS
    -fms-extensions
    -fms-compatibility
    -fms-compatibility-version=19.29
    -fdelayed-template-parsing
    -D__GNUC_STDC_INLINE__
    -D__GCC_ATOMIC_TEST_AND_SET_TRUEVAL=1
)
set(ATOMIC_COMPAT_FLAGS
    -D__GCC_ATOMIC_BOOL_LOCK_FREE=2
    -D__GCC_ATOMIC_CHAR_LOCK_FREE=2
    -D__GCC_ATOMIC_WCHAR_T_LOCK_FREE=2
    -D__GCC_ATOMIC_CHAR16_T_LOCK_FREE=2
    -D__GCC_ATOMIC_CHAR32_T_LOCK_FREE=2
    -D__GCC_ATOMIC_SHORT_LOCK_FREE=2
    -D__GCC_ATOMIC_INT_LOCK_FREE=2
    -D__GCC_ATOMIC_LONG_LOCK_FREE=2
    -D__GCC_ATOMIC_LLONG_LOCK_FREE=2
    -D__GCC_ATOMIC_POINTER_LOCK_FREE=2
)
set(DECOMP_FLAGS
    -ferror-limit=0
    -fno-omit-frame-pointer
    -w
    "SHELL:-include ${CMAKE_SOURCE_DIR}/src/msvc_compat.h"
)

# --- Engine core: the subset needed for DTA parsing ---
# obj (DataNode/DataArray/Object), utl (MemMgr/Str/Symbol/streams),
# os (File/Debug/System), math (already known-good).
file(GLOB ENGINE_OBJ  ${REPO_ROOT}/src/system/obj/*.cpp)
file(GLOB ENGINE_UTL  ${REPO_ROOT}/src/system/utl/*.cpp)
file(GLOB ENGINE_OS   ${REPO_ROOT}/src/system/os/*.cpp)
file(GLOB ENGINE_MATH ${REPO_ROOT}/src/system/math/*.cpp)
set(ENGINE_SOURCES ${ENGINE_OBJ} ${ENGINE_UTL} ${ENGINE_OS} ${ENGINE_MATH})

# Drop platform-specific TUs — native shims replace them.
list(FILTER ENGINE_SOURCES EXCLUDE REGEX "_(Xbox|Win|Wii|X360)\\.cpp$")
list(FILTER ENGINE_SOURCES EXCLUDE REGEX "(Dx9|DX9|Wgpu|Gpu|D3D|Xenon)")
# Case-variant duplicates (dc3 ships both Vec.cpp/vec.cpp etc.) — keep lowercase.
list(FILTER ENGINE_SOURCES EXCLUDE REGEX "/(Vec|Mtx)\\.cpp$")

# Flex-generated DTA lexer (C source).
set(DTA_LEXER ${REPO_ROOT}/src/system/obj/DataFlex.c)

# --- Native platform shims (from dc3-decomp) ---
file(GLOB PLATFORM_SHIMS ${CMAKE_SOURCE_DIR}/src/platform/*.cpp)
set(NATIVE_SHIMS
    ${CMAKE_SOURCE_DIR}/src/xdk_shims.cpp
    ${CMAKE_SOURCE_DIR}/src/native_link_glue.cpp
    ${CMAKE_SOURCE_DIR}/src/thunk_stubs.cpp
    ${CMAKE_SOURCE_DIR}/src/native_job_stubs.cpp
    ${PLATFORM_SHIMS}
)

add_executable(rb3-dta
    ${CMAKE_SOURCE_DIR}/src/main_dta.cpp
    ${ENGINE_SOURCES}
    ${DTA_LEXER}
    ${NATIVE_SHIMS}
    ${CMAKE_SOURCE_DIR}/src/dta_link_stubs.s
)
target_compile_options(rb3-dta PRIVATE ${MS_COMPAT_FLAGS} ${ATOMIC_COMPAT_FLAGS} ${DECOMP_FLAGS})
target_compile_definitions(rb3-dta PRIVATE HX_NATIVE=1 MILO_DEBUG=1 _DEBUG=1)
# native/include shadows libc++ __wrap_iter (MSVC STL iterator->pointer compat).
target_include_directories(rb3-dta SYSTEM BEFORE PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_include_directories(rb3-dta PRIVATE
    ${CMAKE_SOURCE_DIR}/src          # native shims + stl/ STLport-style shim headers
    ${REPO_ROOT}/src
    ${REPO_ROOT}/src/system
)
find_package(Threads REQUIRED)
find_package(ZLIB REQUIRED)
target_link_libraries(rb3-dta PRIVATE Threads::Threads ZLIB::ZLIB)

# Rendering/MIDI/synth/Win32 symbols referenced by compiled-but-unused engine
# vtables are satisfied by weak no-op stubs in src/dta_link_stubs.s (none are
# on the DTA-parse path).
