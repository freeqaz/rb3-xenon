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
#   XDK_OSS   = Lane H reconstructed xtl.h header dir   (default: rb3-xenon/src/xdk)
#   IMPORTLIB = Lane L reconstructed import-lib dir     (default: <staging>/importlib-stub)
# =============================================================================
set -u

# ---- fixed paths -----------------------------------------------------------
MILO=/home/free/code/milohax
RB3E=$MILO/RB3Enhanced
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
XDK_OSS=${XDK_OSS:-$XENON/src/xdk}         # Lane H: xtl.h + group headers
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
  for src in source/*.c; do
    total=$((total+1))
    local base; base=$(basename "$src" .c)
    local log="$LOGS/${base}.log"
    "$WIBO" "$CC" "${CFLAGS[@]}" "${INCS[@]}" -Fo"$OBJ/${base}.obj" "$src" >"$log" 2>&1
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
  echo "----" | tee -a "$LOGS/compile_summary.txt"
  echo "compiled $ok/$total, failed $fail" | tee -a "$LOGS/compile_summary.txt"
}

link_full() {
  # wibo parses argv tokens beginning with '/' as options and silently DROPS
  # absolute obj/lib paths as LNK4044 'unrecognized option; ignored' (a probe
  # that passed absolute paths linked ZERO objs). Fix: cd into $STAGE and pass
  # obj/lib names RELATIVE, with LIB=$IMPORTLIB in the env.
  local nobj; nobj=$(ls "$OBJ"/*.obj 2>/dev/null | wc -l)
  echo "linking $nobj game objs + crt/xapi/xonline + xam/xboxkrnl import libs..."
  mkdir -p "$STAGE/tmpdir"
  ( cd "$STAGE" \
    && TMP="$STAGE/tmpdir" TEMP="$STAGE/tmpdir" LIB="$IMPORTLIB" \
       "$WIBO" "$LINK" "${LFLAGS[@]}" \
         -OUT:RB3Enhanced.exe -IMPLIB:RB3Enhanced.imp \
         obj/*.obj crt/crt.obj xapi/xapi_oss.obj xonline/xonline_stub.obj \
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
  all)     compile_all; link_full ;;
  *) echo "usage: $0 {compile|link|all}"; exit 1 ;;
esac
