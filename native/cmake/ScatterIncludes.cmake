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

    # ---- COMMENTS ARE NOT DIRECTIVES ---------------------------------------
    # !! This scanner used to match `#if`/`#endif` ANYWHERE on a line, so PROSE
    # INSIDE A COMMENT was parsed as a live directive. That is not theoretical:
    # `6c087cbd` (2026-08-14) added, to rndobj/TexRenderer.cpp, the comment
    #
    #     // #ifdef HX_NATIVE and the match build never defines it.
    #
    # which pushed an unmatched HX_NATIVE frame onto the stack and never popped
    # it. The `#include "math/mtx.cpp"` 212 lines below was therefore classified
    # CONDITIONAL-and-HX-guarded instead of UNCONDITIONAL -- and that is the one
    # bucket this module deliberately ignores in SILENCE: not pruned, and not
    # warned about either. mtx.cpp was then compiled standalone AS WELL AS being
    # emitted from TexRenderer's TU, and rb3-milo / rb3-render died with 17
    # duplicate definitions. A COMMENT-ONLY COMMIT BROKE THE NATIVE LINK.
    #
    # Two independent defences, because one regex should not be load-bearing:
    #
    #  1. Strip /* ... */ blocks, then require every directive to be preceded by
    #     a newline and whitespace ONLY. A real directive always starts its line;
    #     `// #ifdef X` and `foo(); // #endif` no longer match. CMake regexes
    #     have no multiline `^`, hence the explicit leading \n (and the prepended
    #     one, so a directive on line 1 is still seen).
    #  2. The #if/#endif stack must balance at EOF. If it does not, the scan is
    #     KNOWN-UNRELIABLE for this file and says so LOUDLY -- see below. That is
    #     the general net: it fires for any future desync cause, not just this
    #     one, which a comment-stripping fix alone would not.
    string(REGEX REPLACE "/\\*[^*]*\\*+([^/*][^*]*\\*+)*/" "" _content "${_content}")
    set(_content "\n${_content}")
    # Only the directives we care about: any #if flavour, #endif, and
    # #include of a .cpp. Ordinary header includes are not matched.
    string(REGEX MATCHALL
           "\n[ \t]*#[ \t]*(if[a-z]*[^\n]*|endif|include[ \t]*\"[^\"]*\\.cpp\")"
           _dirs "${_content}")

    set(_stack "")      # one entry per open #if: 1 if it mentions HX_NATIVE
    set(_u "")
    set(_c "")
    set(_h "")
    foreach(_d IN LISTS _dirs)
        if(_d MATCHES "^\n[ \t]*#[ \t]*if")
            if(_d MATCHES "HX_NATIVE")
                list(APPEND _stack 1)
            else()
                list(APPEND _stack 0)
            endif()
        elseif(_d MATCHES "^\n[ \t]*#[ \t]*endif")
            list(LENGTH _stack _n)
            if(_n GREATER 0)
                list(REMOVE_AT _stack -1)
            endif()
        elseif(_d MATCHES "#[ \t]*include[ \t]*\"([^\"]+)\"")
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

    # ---- ANTI-VACUITY: did the stack balance? ------------------------------
    # A non-empty stack at EOF means this scanner's model of the file is WRONG,
    # so every include it classified as conditional is unreliable -- exactly the
    # failure that shipped a broken native link on 2026-08-14. Only worth saying
    # for a file that actually has a .cpp include, since that is the only case
    # where the misclassification can change a decision.
    list(LENGTH _stack _depth)
    if(_depth GREATER 0)
        set(_any "${_u}" "${_c}")
        if(_any MATCHES "[^;]")
            message(WARNING
                "[scatter] ${_file}: #if/#endif do not balance (depth ${_depth} at EOF), "
                "so this file's scatter-includes may be MISCLASSIFIED as conditional and "
                "silently left un-pruned -- which is how a duplicate definition reaches "
                "the link. Usual cause: preprocessor-looking text inside a comment.")
        endif()
    endif()

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
            if(_res AND _res MATCHES "${RB3_SCATTER_PLATFORM_GUEST_RE}")
                # A platform-only guest: shadowed by rb3_scatter_multihost, so
                # nothing it reaches is emitted through it.
            elseif(_res)
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

# ---- MULTI-HOST GUESTS -------------------------------------------------------
# rb3_scatter_prune handles "the guest is ALSO a standalone source". It cannot
# handle a guest that TWO OR MORE sources of one target reach, directly or
# transitively, through unconditional scatter-includes: each host emits its own
# copy, and both copies are strong. Measured on rb3-app (A0, 2026-11-22):
# BandSwatch.cpp -> rnddx9/Rnd_Xbox.cpp -> meta_band/AccomplishmentManager.cpp
# and PrefabMgr.cpp -> meta_band/AccomplishmentManager.cpp gave 129 duplicate
# definitions; MusicLibraryStore.cpp and SongSort.cpp -> meta_band/Utl.cpp 12.
#
# THE RULE: per target, for every guest with more than one host, the FIRST host
# in the target's source order keeps it. Every other host is compiled, for this
# target only, with an extra `-iquote` directory holding an EMPTY file at the
# include's own spelling. A quoted include searches the includer's directory,
# then -iquote, then -I, so the empty file shadows the guest in exactly those
# hosts and the guest is compiled exactly once. No source file changes and no
# scatter-include is removed or reordered (they carry the X360 COMDAT placement).
# If the includer's own directory already resolves the spelling, the shadow
# cannot win and configure stops rather than guess.
#
# PLATFORM-ONLY GUESTS: a guest under RB3_SCATTER_PLATFORM_GUEST_RE (rnddx9, the
# DX9 renderer the engine replaces; synth_xbox) is shadowed the same way in
# EVERY host, and nothing is followed through it. No native target compiles
# these standalone (native/CMakeLists.txt, L4 note); the first host to reach one
# natively is rb3-app's BandSwatch.cpp -> rnddx9/Rnd_Xbox.cpp, whose
# rnddx9/Mesh.cpp re-defines PackVector/FillCompressedVertex next to
# rndobj/Mesh.cpp's.
set(RB3_SCATTER_PLATFORM_GUEST_RE "/src/system/(rnddx9|synth_xbox)/")

function(_rb3_scatter_shadow _target _hi _host _includer _inc _why)
    set(_dir "${CMAKE_BINARY_DIR}/scatter_multihost/${_target}/h${_hi}")
    get_filename_component(_idir "${_includer}" DIRECTORY)
    if(EXISTS "${_idir}/${_inc}")
        message(FATAL_ERROR
            "[scatter] ${_target}: ${_includer} resolves \"${_inc}\" from its own "
            "directory, so a -iquote shadow cannot keep it out of ${_host} (${_why}).")
    endif()
    set(_shadow "${_dir}/${_inc}")
    set(_body "// Generated by native/cmake/ScatterIncludes.cmake for target ${_target}:\n// ${_why}\n")
    if(EXISTS "${_shadow}")
        file(READ "${_shadow}" _old)
    else()
        set(_old "")
    endif()
    if(NOT "${_old}" STREQUAL "${_body}")
        file(WRITE "${_shadow}" "${_body}")
    endif()
    get_property(_opts SOURCE "${_host}" PROPERTY COMPILE_OPTIONS)
    set(_flag "$<$<STREQUAL:$<TARGET_PROPERTY:NAME>,${_target}>:-iquote${_dir}>")
    if(NOT "${_flag}" IN_LIST _opts)
        set_property(SOURCE "${_host}" APPEND PROPERTY COMPILE_OPTIONS "${_flag}")
    endif()
endfunction()

function(_rb3_scatter_reach _root _out_edges)
    set(_queue "${_root}")
    set(_seen "")
    set(_edges "")
    while(_queue)
        list(POP_FRONT _queue _cur)
        if("${_cur}" IN_LIST _seen)
            continue()
        endif()
        list(APPEND _seen "${_cur}")
        _rb3_scatter_scan("${_cur}" _uncond _cond _cond_hx)
        foreach(_inc IN LISTS _uncond)
            _rb3_scatter_resolve("${_inc}" "${_cur}" _res)
            if(_res AND _res MATCHES "${RB3_SCATTER_PLATFORM_GUEST_RE}")
                list(APPEND _edges "${_cur}|${_inc}|${_res}|platform")
            elseif(_res)
                # includer|spelling|resolved
                list(APPEND _edges "${_cur}|${_inc}|${_res}")
                list(APPEND _queue "${_res}")
            endif()
        endforeach()
    endwhile()
    set(${_out_edges} "${_edges}" PARENT_SCOPE)
endfunction()

function(rb3_scatter_multihost _listvar _target)
    set(_roots "")
    foreach(_s IN LISTS ${_listvar})
        get_filename_component(_a "${_s}" ABSOLUTE)
        if(_a MATCHES "\\.cpp$")
            list(APPEND _roots "${_a}")
        endif()
    endforeach()
    set(_guests "")
    set(_i 0)
    foreach(_r IN LISTS _roots)
        _rb3_scatter_reach("${_r}" _edges)
        set(_edges_${_i} "${_edges}")
        foreach(_e IN LISTS _edges)
            string(REPLACE "|" ";" _p "${_e}")
            list(GET _p 2 _g)
            list(LENGTH _p _np)
            if(_np GREATER 3)
                list(GET _p 0 _includer)
                list(GET _p 1 _inc)
                file(RELATIVE_PATH _grel "${REPO_ROOT}" "${_g}")
                file(RELATIVE_PATH _hrel "${REPO_ROOT}" "${_r}")
                _rb3_scatter_shadow("${_target}" "${_i}" "${_r}" "${_includer}" "${_inc}"
                    "${_grel} is a platform-only guest; ${_hrel} skips it natively")
                message(STATUS "[scatter] ${_target}: ${_hrel} skips platform-only guest "
                               "${_grel} (platform rule)")
                continue()
            endif()
            string(MAKE_C_IDENTIFIER "${_g}" _gk)
            if(NOT DEFINED _hosts_${_gk})
                list(APPEND _guests "${_g}")
                set(_hosts_${_gk} "")
            endif()
            if(NOT "${_i}" IN_LIST _hosts_${_gk})
                list(APPEND _hosts_${_gk} "${_i}")
            endif()
        endforeach()
        math(EXPR _i "${_i} + 1")
    endforeach()

    foreach(_g IN LISTS _guests)
        string(MAKE_C_IDENTIFIER "${_g}" _gk)
        list(LENGTH _hosts_${_gk} _nh)
        if(_nh LESS 2)
            continue()
        endif()
        list(GET _hosts_${_gk} 0 _keep)
        list(GET _roots ${_keep} _keeper)
        file(RELATIVE_PATH _grel "${REPO_ROOT}" "${_g}")
        file(RELATIVE_PATH _krel "${REPO_ROOT}" "${_keeper}")
        list(SUBLIST _hosts_${_gk} 1 -1 _others)
        foreach(_hi IN LISTS _others)
            list(GET _roots ${_hi} _host)
            file(RELATIVE_PATH _hrel "${REPO_ROOT}" "${_host}")
            foreach(_e IN LISTS _edges_${_hi})
                string(REPLACE "|" ";" _p "${_e}")
                list(LENGTH _p _np)
                list(GET _p 0 _includer)
                list(GET _p 1 _inc)
                list(GET _p 2 _res)
                if(_np GREATER 3 OR NOT "${_res}" STREQUAL "${_g}")
                    continue()
                endif()
                _rb3_scatter_shadow("${_target}" "${_hi}" "${_host}" "${_includer}" "${_inc}"
                    "${_grel} is emitted by ${_krel}; ${_hrel} skips it (multi-host rule)")
            endforeach()
            message(STATUS "[scatter] ${_target}: ${_grel} has more than one host; "
                           "${_krel} emits it, ${_hrel} skips it (multi-host rule)")
        endforeach()
    endforeach()
endfunction()

# add_executable(), with the scatter-include pruning applied to the source list.
macro(rb3_add_executable _name)
    set(_rb3_srcs ${ARGN})
    rb3_scatter_prune(_rb3_srcs "${_name}")
    rb3_scatter_multihost(_rb3_srcs "${_name}")
    add_executable(${_name} ${_rb3_srcs})
    unset(_rb3_srcs)
endmacro()
