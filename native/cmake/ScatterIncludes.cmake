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
