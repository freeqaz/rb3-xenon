#!/usr/bin/env bash
# =============================================================================
# build_xbox_ossp.sh  --  Strategy B, Lane K
# XDK-free full compile+link recipe for RB3Enhanced.dll (Xbox 360 / PPCBE).
#
# Mirrors RB3Enhanced/Makefile's CFLAGS_X / LFLAGS_X / LIBS_X, MINUS the XDK:
#   - drops  -FI xbox_intellisense_platform.h   (XDK intellisense force-include)
#   - drops  -Zi / -Fd                          (PDB debug info; not needed)
#   - replaces $(XEDK)/include/xbox              with reconstructed OSS headers
#   - replaces $(XEDK)/lib/xbox/*.lib            with Lane L reconstructed import libs
#
# Toolchain: MSVC-X360 PPC 16.00.11886.00 cl.exe/link.exe under wibo.
# Machine target: 0x01F2 (POWERPCFP / PPCBE).
#
# Usage:
#   ./build_xbox_ossp.sh compile     # compile every TU it can, log per-TU result
#   ./build_xbox_ossp.sh link        # attempt full link (needs Lane L libs)
#   ./build_xbox_ossp.sh all         # compile + link
# Env overrides:
#   XDK_OSS   = Lane H reconstructed xtl.h header dir   (default: <staging>/H-headers/xdk-oss)
#   IMPORTLIB = Lane L reconstructed import-lib dir     (default: <staging>/importlib-stub)
# =============================================================================
set -u

# ---- fixed paths -----------------------------------------------------------
MILO=/home/free/code/milohax
# RB3E source root. Default = the main RB3Enhanced checkout (byte-identical for
# concurrent agents who set no RB3E_SRC). Override to build from a worktree, e.g.
#   RB3E_SRC=/home/free/code/milohax/rb3e-civetweb-wt ./K-link/build_xbox_ossp.sh all
# This changes only the *source* root; objs/PE/map still land under $STAGE.
RB3E=${RB3E_SRC:-$MILO/RB3Enhanced}
XENON=$MILO/rb3-xenon
WIBO=$MILO/wibo/build/release/wibo
CC=$XENON/build/compilers/X360/16.00.11886.00/cl.exe
LINK=$XENON/build/compilers/X360/16.00.11886.00/link.exe

STAGE=$XENON/tools/oss-xbox-build/K-link
OBJ=$STAGE/obj
LOGS=$STAGE/logs
STUBS=$STAGE/stubs

# ---- dependency inputs (stub if Lane H / Lane L not ready) -----------------
LIBCMT=$XENON/src/xdk/LIBCMT               # CRT headers (present today)
# Lane H: xtl.h + group headers. The reconstructed set that compiles 51/51
# lives in H-headers/xdk-oss ($XENON/src/xdk has no xtl.h — with it, the 17
# xtl.h TUs FAIL and the link silently reuses stale objs).
XDK_OSS=${XDK_OSS:-$XENON/tools/oss-xbox-build/H-headers/xdk-oss}
# Lane L reconstructed import libs. The ONLY two real XEX import modules that
# resolve at load are xam.xex + xboxkrnl.exe (see finish/K-link.json).
IMPORTLIB=${IMPORTLIB:-$XENON/tools/oss-xbox-build/L-importlibs}

mkdir -p "$OBJ" "$LOGS"

# ---- CFLAGS: Makefile CFLAGS_X minus XDK force-include / PDB ---------------
# Makefile: -c -Zi -nologo -W3 -WX- -Ox -Os -D _XBOX -D RB3E_XBOX -D RB3E -D NDEBUG
#           -GF -Gm- -MT -GS- -Gy -fp:fast -fp:except- -Zc:wchar_t -Zc:forScope
#           -GR- -openmp- -FI<xbox_intellisense_platform.h> -Fd<...> -I include
CFLAGS=(
  -c -nologo -W3 -WX- -Ox -Os
  -D _XBOX -D RB3E_XBOX -D RB3E -D NDEBUG
  -GF -Gm- -MT -GS- -Gy -fp:fast -fp:except-
  -Zc:wchar_t -Zc:forScope -GR- -openmp-
  # civetweb private config (harmless global — only civetweb sources + civetweb.h
  # test these macros; none collide with RB3E/CRT/XDK-OSS identifiers). Global is
  # beneficial so net_http_civet.c sees USE_WEBSOCKET when it includes civetweb.h.
  -D NO_SSL -D NO_CGI -D NO_FILES -D NO_FILESYSTEMS -D NO_CACHING
  -D NO_THREAD_NAME -D USE_WEBSOCKET -D USE_STACK_SIZE=65536
  # NO_FILESYSTEMS forces two external hooks (civetweb.c:3537/16669 C1189);
  # bodies live in source/civetweb/external_*.inl (phase-0 v4 Class F).
  -D MG_EXTERNAL_FUNCTION_mg_cry_internal_impl
  -D MG_EXTERNAL_FUNCTION_log_access
  -TC
)
# include order: RB3E game headers -> stdint shadow -> Lane H xtl.h -> CRT
INCS=(
  -I "$RB3E/include"
  -I "$STUBS"
  -I "$XDK_OSS"
  -I "$LIBCMT"
)

# ---- LFLAGS: proven recipe, see finish/K-link.json (0 unresolved externals) ---
# Only xam.xex + xboxkrnl.exe resolve at XEX load; XAPILIB/XNET/XONLINE are
# build-time static libs, not XEX import modules -> the game-fn stubs + xapi/
# xonline OSS objs cover them. -BASE:0x84000000 (MSVC-X360 default base is
# 0x88000000; we want the intended XEX load base). -NODEFAULTLIB + explicit
# crt/xapi/xonline objs. -MAP emits RB3Enhanced.map (hook VAs the SI Xenia
# harness needs). -RELEASE is intentionally DROPPED: under wibo it invokes the
# unstubbed imagehlp!CheckSumMappedFile (rc=134) AFTER the PE is written but
# BEFORE the .map is flushed, yielding an empty map. The PE checksum is
# irrelevant here -- xex2pack recomputes it -- so dropping -RELEASE gives a
# clean rc=0 link AND a complete map.
LIBS_X=(xam.lib xboxkrnl.lib)
LFLAGS=(
  -NOLOGO -INCREMENTAL:NO -ERRORREPORT:PROMPT
  -MACHINE:PPCBE
  -STACK:262144,262144
  -BASE:0x84000000
  -OPT:REF -OPT:ICF
  -dll -entry:_DllMainCRTStartup -XEX:NO -FIXED:NO -NODEFAULTLIB
  -MAP:RB3Enhanced.map
)

cd "$RB3E" || exit 2

compile_all() {
  local ok=0 fail=0 total=0
  : > "$LOGS/compile_summary.txt"
  # Prune objs whose source doesn't exist in THIS $RB3E: obj/ is shared across
  # RB3E_SRC targets and link_full globs obj/*.obj, so a worktree build's extra
  # TUs (civetweb.obj, civetweb_x360_shim.obj, net_http_civet.obj, ...) would
  # otherwise leak into a later default-checkout link (bit the SI build 2026-07-15;
  # same class as the stale _xdk_stubs.obj incident below).
  local o base
  for o in "$OBJ"/*.obj; do
    [ -e "$o" ] || break
    base=$(basename "$o" .obj)
    [ -f "source/$base.c" ] || [ -f "source/civetweb/$base.c" ] || [ "$base" = _xdk_stubs ] \
      || { echo "PRUNE stale $base.obj (no source in $RB3E)"; rm -f "$o"; }
  done
  # $STAGE/_xdk_stubs.c (XDK entrypoint shims) compiles with the same recipe —
  # it MUST be in this loop: a stale obj/_xdk_stubs.obj once shipped return -1
  # networking stubs long after the source was fixed.
  # source/*.c is non-recursive: it auto-globs source/civetweb_x360_shim.c and
  # source/net_http_civet.c (compiled -TC as C), but MISSES the vendored subdir TU
  # source/civetweb/civetweb.c — list it explicitly. That one TU compiles -TP (C++;
  # cl 16.00 -TC=C89 rejects civetweb's C99 mid-block/for-init decls — phase-0 v4).
  # Conditional: only when civetweb is vendored in $RB3E (worktree). The default
  # RB3E=main checkout has no source/civetweb/, so skip it (and the crt_civetweb
  # step below + its link obj) to keep the default OSS build working.
  local HAVE_CIVET=0 civet_srcs=()
  if [ -f "$RB3E/source/civetweb/civetweb.c" ]; then HAVE_CIVET=1; civet_srcs=(source/civetweb/civetweb.c); fi
  for src in source/*.c "${civet_srcs[@]}" "$STAGE/_xdk_stubs.c"; do
    total=$((total+1))
    local base; base=$(basename "$src" .c)
    local log="$LOGS/${base}.log"
    # wibo treats absolute /paths as cl options — pass sources relative to cwd
    case "$src" in /*) src=$(realpath --relative-to=. "$src") ;; esac
    # vendored civetweb TU: force C++ (-TP wins over the array's trailing -TC,
    # last-wins in cl) + expose its own dir for the #include "*.inl" siblings.
    local extra=()
    # -TP (C++, past C99 mid-block decls); vendored dir for the #include "*.inl"
    # siblings + external_*.inl hooks; civetweb_win32/ scoped redirect headers
    # (windows/winsock2/ws2tcpip/sal/direct/process + sys/*) that route the
    # _WIN32 branch to civetweb_x360_shim.h. This -I precedes INCS (hence LIBCMT)
    # so process.h/sys-stat shadow LIBCMT's win_types-pulling versions (v4 Class B).
    case "$src" in source/civetweb/civetweb.c) extra=(-TP -I "$RB3E/source/civetweb" -I "$RB3E/include/civetweb_win32") ;; esac
    "$WIBO" "$CC" "${CFLAGS[@]}" "${extra[@]}" "${INCS[@]}" -Fo"$OBJ/${base}.obj" "$src" >"$log" 2>&1
    local rc=$?
    if [ $rc -eq 0 ] && [ -f "$OBJ/${base}.obj" ]; then
      ok=$((ok+1))
      echo "OK    $base" | tee -a "$LOGS/compile_summary.txt"
    else
      fail=$((fail+1))
      # first error line for triage
      local firsterr
      firsterr=$(grep -m1 -iE 'error|fatal|cannot open' "$log" | head -c 160)
      echo "FAIL  $base    ${firsterr}" | tee -a "$LOGS/compile_summary.txt"
    fi
  done
  # civetweb CRT additions -> crt/crt_civetweb.obj (linked next to the prebuilt
  # crt.obj). Kept out of crt.c so the 51 game TUs' shared obj is untouched.
  # Only when civetweb is present; otherwise drop any stale obj so link_full's
  # existence-gate won't pull duplicate CRT symbols into the default build.
  if [ "$HAVE_CIVET" = 1 ]; then
    total=$((total+1))
    local cvsrc; cvsrc=$(realpath --relative-to=. "$STAGE/crt/crt_civetweb.c")
    "$WIBO" "$CC" "${CFLAGS[@]}" "${INCS[@]}" -Fo"$STAGE/crt/crt_civetweb.obj" "$cvsrc" \
        >"$LOGS/crt_civetweb.log" 2>&1
    if [ $? -eq 0 ] && [ -f "$STAGE/crt/crt_civetweb.obj" ]; then
      ok=$((ok+1)); echo "OK    crt_civetweb" | tee -a "$LOGS/compile_summary.txt"
    else
      fail=$((fail+1))
      echo "FAIL  crt_civetweb    $(grep -m1 -iE 'error|fatal' "$LOGS/crt_civetweb.log" | head -c 160)" | tee -a "$LOGS/compile_summary.txt"
    fi
  else
    rm -f "$STAGE/crt/crt_civetweb.obj"
  fi
  echo "----" | tee -a "$LOGS/compile_summary.txt"
  echo "compiled $ok/$total, failed $fail" | tee -a "$LOGS/compile_summary.txt"
  [ "$fail" -eq 0 ]
}

link_full() {
  # wibo parses argv tokens beginning with '/' as options and silently DROPS
  # absolute obj/lib paths as LNK4044 'unrecognized option; ignored' (a probe
  # that passed absolute paths linked ZERO objs). Fix: cd into $STAGE and pass
  # obj/lib names RELATIVE, with LIB=$IMPORTLIB in the env.
  local nobj; nobj=$(ls "$OBJ"/*.obj 2>/dev/null | wc -l)
  # civetweb CRT obj only when it was built this run (see compile_all HAVE_CIVET)
  local civet_obj=""
  [ -f "$STAGE/crt/crt_civetweb.obj" ] && civet_obj="crt/crt_civetweb.obj"
  echo "linking $nobj game objs + crt${civet_obj:+/civetweb}/xapi/xonline + xam/xboxkrnl import libs..."
  mkdir -p "$STAGE/tmpdir"
  ( cd "$STAGE" \
    && TMP="$STAGE/tmpdir" TEMP="$STAGE/tmpdir" LIB="$IMPORTLIB" \
       "$WIBO" "$LINK" "${LFLAGS[@]}" \
         -OUT:RB3Enhanced.exe -IMPLIB:RB3Enhanced.imp \
         obj/*.obj crt/crt.obj $civet_obj xapi/xapi_oss.obj xonline/xonline_stub.obj \
         "${LIBS_X[@]}" >"$LOGS/link_full.log" 2>&1 )
  local rc=$?
  echo "link rc=$rc -> $LOGS/link_full.log"
  if [ -s "$STAGE/RB3Enhanced.exe" ] && [ -s "$STAGE/RB3Enhanced.map" ]; then
    echo "  PE -> $STAGE/RB3Enhanced.exe ; MAP -> $STAGE/RB3Enhanced.map"
  else
    echo "  WARNING: missing RB3Enhanced.exe or RB3Enhanced.map"
  fi
}

case "${1:-all}" in
  compile) compile_all ;;
  link)    link_full ;;
  all)     compile_all || { echo "!! compile failures — refusing to link (stale objs would be silently reused). See $LOGS/compile_summary.txt"; exit 1; }
           link_full ;;
  *) echo "usage: $0 {compile|link|all}"; exit 1 ;;
esac
