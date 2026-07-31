# ScatterIncludes.cmake -- teach the native build about the decomp's
# scatter-includes.
#
# WHY THIS EXISTS
# ---------------
# To reproduce retail's COMDAT placement, this decomp contains ~250 "scatter
# includes": a .cpp that `#include`s ANOTHER .cpp so the includee's functions
# are emitted from the includer's translation unit. Example:
#
#     src/system/obj/DataFile.cpp:767   #include "obj/TextFile.cpp"
#
# Those are LOAD-BEARING for the X360 match and must not be removed.
#
# The X360 decomp build never links -- it only compiles objects for objdiff --
# so a scatter-included file being compiled BOTH standalone AND inside its
# includer is structurally invisible there. The native build is the only place
# it surfaces, as a duplicate-definition link error.
#
# Historically this was patched reactively, one incident at a time, by wrapping
# the include in `#if !HX_NATIVE` in the shared source. That works but is
# incidental: it requires whoever adds a scatter-include to know the native
# build exists. It has already been missed twice (LogFile, then TextFile).
#
# THE RULE
# --------
# For each target, independently:
#
#   1. Take the target's own source list S.
#   2. Follow UNCONDITIONAL scatter-include edges transitively from every
#      member of S. Anything reached is already emitted by a TU in S.
#   3. Remove those reached files from S.
#
# Deriving it PER TARGET is what makes it correct. A blanket exclusion list
# would be wrong in both directions:
#   * an includee whose includer is NOT in this target must be KEPT, or the
#     target loses those symbols entirely (undefined references);
#   * an includee whose includer IS in this target must be DROPPED, or the
#     symbols are defined twice.
#
# We deliberately only act on UNCONDITIONAL includes. CMake is not a
# preprocessor and must not guess at `#if` conditions. A conditional include is
# left alone -- which is exactly right for the existing `#if !HX_NATIVE`
# guards, since those are inert natively and the includee therefore DOES need
# to be compiled standalone. Any OTHER conditional whose includee is also a
# target source is reported at configure time rather than silently guessed at.
#
# Both mechanisms enforce the same invariant (exactly one definition per
# symbol) and cannot conflict: the guard makes the edge inert, and this module
# only prunes for edges that are active. The guard is now optional; a new
# scatter-include needs no native-side action at all.

# Quoted-include search roots, mirroring the targets' include directories.
# The union across all targets is used: resolution only ever matters when the
# resolved path is itself one of the target's sources, so a root that a given
# target does not actually pass is harmless.
set(RB3_SCATTER_ROOTS
    "${REPO_ROOT}/src"
    "${REPO_ROOT}/src/system"
    "${REPO_ROOT}/src/band3"
    "${REPO_ROOT}/src/network"
    "${CMAKE_CURRENT_SOURCE_DIR}/src")

# Scan one .cpp for `#include "....cpp"` directives, split by whether the
# directive sits inside any `#if`/`#ifdef`/`#ifndef` block, and (for the
# conditional ones) whether an enclosing condition mentions HX_NATIVE.
# Memoized in a GLOBAL property -- 13 targets share ~200 sources.
function(_rb3_scatter_scan _file _out_uncond _out_cond _out_cond_hx)
    string(MAKE_C_IDENTIFIER "_rb3_scat_${_file}" _key)
    get_property(_cached GLOBAL PROPERTY "${_key}_set")
    if(_cached)
        get_property(_u GLOBAL PROPERTY "${_key}_u")
        get_property(_c GLOBAL PROPERTY "${_key}_c")
        get_property(_h GLOBAL PROPERTY "${_key}_h")
        set(${_out_uncond}  "${_u}" PARENT_SCOPE)
        set(${_out_cond}    "${_c}" PARENT_SCOPE)
        set(${_out_cond_hx} "${_h}" PARENT_SCOPE)
        return()
    endif()

    # Re-run configure when a scatter-include is added, moved or removed.
    set_property(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
                 APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_file}")

    file(READ "${_file}" _content)
    string(REPLACE ";" "\\;" _content "${_content}")
    # Only the directives we care about: any #if flavour, #endif, and
    # #include of a .cpp. Ordinary header includes are not matched.
    string(REGEX MATCHALL
           "#[ \t]*(if[a-z]*[^\n]*|endif|include[ \t]*\"[^\"]*\\.cpp\")"
           _dirs "${_content}")

    set(_stack "")      # one entry per open #if: 1 if it mentions HX_NATIVE
    set(_u "")
    set(_c "")
    set(_h "")
    foreach(_d IN LISTS _dirs)
        if(_d MATCHES "^#[ \t]*if")
            if(_d MATCHES "HX_NATIVE")
                list(APPEND _stack 1)
            else()
                list(APPEND _stack 0)
            endif()
        elseif(_d MATCHES "^#[ \t]*endif")
            list(LENGTH _stack _n)
            if(_n GREATER 0)
                list(REMOVE_AT _stack -1)
            endif()
        elseif(_d MATCHES "^#[ \t]*include[ \t]*\"([^\"]+)\"")
            set(_inc "${CMAKE_MATCH_1}")
            list(LENGTH _stack _n)
            if(_n EQUAL 0)
                list(APPEND _u "${_inc}")
            else()
                list(APPEND _c "${_inc}")
                if("1" IN_LIST _stack)
                    list(APPEND _h "${_inc}")   # a deliberate HX_NATIVE guard
                endif()
            endif()
        endif()
    endforeach()

    set_property(GLOBAL PROPERTY "${_key}_u" "${_u}")
    set_property(GLOBAL PROPERTY "${_key}_c" "${_c}")
    set_property(GLOBAL PROPERTY "${_key}_h" "${_h}")
    set_property(GLOBAL PROPERTY "${_key}_set" TRUE)
    set(${_out_uncond}  "${_u}" PARENT_SCOPE)
    set(${_out_cond}    "${_c}" PARENT_SCOPE)
    set(${_out_cond_hx} "${_h}" PARENT_SCOPE)
endfunction()

# Resolve a quoted include the way the native targets' -I search does:
# the includer's own directory first, then the include roots.
function(_rb3_scatter_resolve _inc _includer _out)
    get_filename_component(_dir "${_includer}" DIRECTORY)
    foreach(_root "${_dir}" ${RB3_SCATTER_ROOTS})
        if(EXISTS "${_root}/${_inc}")
            get_filename_component(_abs "${_root}/${_inc}" ABSOLUTE)
            set(${_out} "${_abs}" PARENT_SCOPE)
            return()
        endif()
    endforeach()
    set(${_out} "" PARENT_SCOPE)
endfunction()

# Remove, from <listvar>, every source that another source in the same list
# already emits via an unconditional scatter-include. Edits <listvar> in place.
function(rb3_scatter_prune _listvar _target)
    set(_norm "")
    foreach(_s IN LISTS ${_listvar})
        get_filename_component(_a "${_s}" ABSOLUTE)
        list(APPEND _norm "${_a}")
    endforeach()

    set(_cpp "")
    foreach(_s IN LISTS _norm)
        if(_s MATCHES "\\.cpp$")
            list(APPEND _cpp "${_s}")
        endif()
    endforeach()

    # Transitive closure over unconditional edges, starting from every source.
    set(_queue "${_cpp}")
    set(_seen "")
    set(_reached "")
    while(_queue)
        list(POP_FRONT _queue _cur)
        if("${_cur}" IN_LIST _seen)
            continue()
        endif()
        list(APPEND _seen "${_cur}")
        _rb3_scatter_scan("${_cur}" _uncond _cond _cond_hx)
        foreach(_inc IN LISTS _uncond)
            _rb3_scatter_resolve("${_inc}" "${_cur}" _res)
            if(_res)
                list(APPEND _reached "${_res}")
                list(APPEND _queue "${_res}")
            else()
                message(WARNING
                    "[scatter] ${_target}: cannot resolve #include \"${_inc}\" "
                    "from ${_cur} -- a duplicate definition may reach the link.")
            endif()
        endforeach()
        # Conditional edges we did not act on. An HX_NATIVE guard is a
        # deliberate, understood decision; anything else is worth surfacing.
        foreach(_inc IN LISTS _cond)
            if(NOT "${_inc}" IN_LIST _cond_hx)
                _rb3_scatter_resolve("${_inc}" "${_cur}" _res)
                if(_res AND "${_res}" IN_LIST _cpp)
                    message(WARNING
                        "[scatter] ${_target}: ${_cur} conditionally includes "
                        "\"${_inc}\", which is ALSO compiled standalone in this "
                        "target. If the condition is true natively this will be "
                        "a duplicate definition; guard it with #if !HX_NATIVE.")
                endif()
            endif()
        endforeach()
    endwhile()

    set(_drop "")
    foreach(_r IN LISTS _reached)
        if("${_r}" IN_LIST _cpp AND NOT "${_r}" IN_LIST _drop)
            list(APPEND _drop "${_r}")
        endif()
    endforeach()

    if(_drop)
        foreach(_d IN LISTS _drop)
            list(REMOVE_ITEM _norm "${_d}")
            file(RELATIVE_PATH _rel "${REPO_ROOT}" "${_d}")
            message(STATUS "[scatter] ${_target}: not compiling ${_rel} "
                           "standalone (emitted by a scatter-include)")
        endforeach()
        set(${_listvar} "${_norm}" PARENT_SCOPE)
    endif()
endfunction()

# add_executable(), with the scatter-include pruning applied to the source list.
macro(rb3_add_executable _name)
    set(_rb3_srcs ${ARGN})
    rb3_scatter_prune(_rb3_srcs "${_name}")
    add_executable(${_name} ${_rb3_srcs})
    unset(_rb3_srcs)
endmacro()
