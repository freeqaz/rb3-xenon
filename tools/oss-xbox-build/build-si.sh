#!/usr/bin/env bash
# build-si.sh — ONE command from edited RB3Enhanced C source to a loadable,
# devkit-signed XEX DLL — optionally deployed to the console and launched.
#
# Pipeline (see BUILD-AND-DEPLOY.md for details):
#   [1] K-link/build_xbox_ossp.sh all   compile 51/51 TUs + link PE (wibo/MSVC-X360)
#       - hard-fails on ANY compile error (no stale-obj links)
#   [2] pack-si-dll.sh                  PE -> loadable XEX2 DLL (wine-free, xexlint-gated)
#   [3] --deploy: boot Aurora, wait for FTP, push DLL, verify sha
#   [4] --launch: launch RB3 with XBDM notify capture (implies --deploy)
#
# Usage:
#   ./build-si.sh                 # build + pack only
#   ./build-si.sh --deploy        # ... + push to console (leaves it in Aurora)
#   ./build-si.sh --launch        # ... + launch RB3 and watch for load/ALIVE
#
# Env: XBOX=<console ip>   (default 192.168.8.180, used by xbox.sh + FTP)
#      WATCH=<seconds>     (default 75) launch-watch window; exits early on ALIVE
set -euo pipefail

HERE=$(cd "$(dirname "$0")" && pwd)
XBOXSH=/home/free/code/milohax/xex-patcher/tools/xbox.sh
MILO=/home/free/code/milohax

# Per-source-tree isolation, mirroring K-link/build_xbox_ossp.sh: a non-default
# RB3E_SRC builds into K-link/out-<basename>/, so pack THAT tree's PE and give the
# DLL a distinct name (RB3Enhanced.fromsource.<basename>.dll) — otherwise a
# worktree pack would overwrite the default DLL. Default (no RB3E_SRC) keeps the
# byte-identical path + a no-arg pack-si-dll.sh call.
RB3E=${RB3E_SRC:-$MILO/RB3Enhanced}
if [ "$(realpath "$RB3E")" = "$(realpath "$MILO/RB3Enhanced")" ]; then
  PE=""; DLL=$HERE/RB3Enhanced.fromsource.dll
else
  name=$(basename "$RB3E")
  PE=$HERE/K-link/out-$name/RB3Enhanced.exe
  DLL=$HERE/RB3Enhanced.fromsource.$name.dll
fi

DEPLOY=0; LAUNCH=0
case "${1:-}" in
  --deploy) DEPLOY=1 ;;
  --launch) DEPLOY=1; LAUNCH=1 ;;
  "") ;;
  *) echo "usage: $0 [--deploy|--launch]"; exit 1 ;;
esac

echo "== [1/4] compile + link (build_xbox_ossp.sh all) =="
"$HERE/K-link/build_xbox_ossp.sh" all

echo "== [2/4] pack (pack-si-dll.sh -> $DLL) =="
if [ -n "$PE" ]; then
  "$HERE/pack-si-dll.sh" "$PE" "$DLL"      # non-default tree: explicit PE + suffixed DLL
else
  "$HERE/pack-si-dll.sh"                    # default: byte-identical no-arg invocation
fi

if [ "$DEPLOY" = "1" ]; then
  echo "== [3/4] deploy to console =="
  "$XBOXSH" aurora
  "$XBOXSH" wait-ftp
  "$XBOXSH" deploy "$DLL"
else
  echo "== [3/4] deploy skipped (pass --deploy) =="
fi

if [ "$LAUNCH" = "1" ]; then
  echo "== [4/4] launch RB3 (watching XBDM notify for load + ALIVE) =="
  "$XBOXSH" launch-watch "${WATCH:-75}"
else
  echo "== [4/4] launch skipped (pass --launch) =="
  echo "== done: $DLL =="
fi
