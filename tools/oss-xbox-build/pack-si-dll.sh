#!/usr/bin/env bash
# pack-si-dll.sh — turn a freshly-linked from-source RB3Enhanced PE into a
# loadable, devkit-signed XEX2 DLL for the RB3DX Xbox 360 console, and validate it.
#
# Pipeline (see BUILD-AND-DEPLOY.md + docs/plans/http-bringup-and-rb3eloader-fix-2026-07-15.md):
#   [1] xex2pack --compress none  — synthesize a raw, unsigned XEX2 from the PE with a
#       correct-VA import block (--import-map).                        (python)
#   [2] ../xex-patcher/tools/pack-loadable.sh  — extract the raw base, fix_thunks
#       (repair the type-1 import thunks the packer emits wrong), then the native
#       xex-patcher rebuilds the FileDataDescriptor + page-hash chain + import-digest
#       chain + HeaderHash + devkit RSA signature, and gates on xexlint.  (native)
#   [3] xextool -m d -c c  — LZX-compress + recompute page hashes + devkit re-sign.
#       *** REQUIRED for the module to LOAD ***: the raw container from [1]+[2] is
#       byte-correct (valid imports + signature, xexlint-green) but the console's
#       XexLoadImage REJECTS the uncompressed xex2pack container at image-map time.
#       Only the xextool-compressed container loads (proven on HW 2026-07-15). This
#       step needs wine; it is the last non-native piece until xex-patcher grows an
#       LZX-normal writer (see ../xex-patcher/analysis/03-lzx-normal-compression.md).
#
# Usage:
#   pack-si-dll.sh [PE] [OUT.dll]
#   pack-si-dll.sh --deploy [PE] [OUT.dll]     # also FTP to the console after xexlint passes
#
#   PE       default: K-link/RB3Enhanced.exe (the from-source build output)
#   OUT.dll  default: <staging>/RB3Enhanced.fromsource.dll
#
# Env: XBOX=192.168.8.180 (deploy target). Needs python3 + capstone. NO wine.
set -euo pipefail

DEPLOY=0
if [ "${1:-}" = "--deploy" ]; then DEPLOY=1; shift; fi

MILO=/home/free/code/milohax
XENON=$MILO/rb3-xenon
PACK=$MILO/xex-patcher/tools/pack-loadable.sh   # wine-free envelope: base + thunks + sign + xexlint
PK=$XENON/tools/xex2pack/xex2pack.py
MAP=$XENON/docs/plans/strategy-b/checkpoints/finish/ordinal-map.json
PE=${1:-$XENON/tools/oss-xbox-build/K-link/RB3Enhanced.exe}
OUT=${2:-$XENON/tools/oss-xbox-build/RB3Enhanced.fromsource.dll}
XBOX=${XBOX:-192.168.8.180}
W=$(mktemp -d)
trap 'rm -rf "$W"' EXIT

[ -f "$PE" ]   || { echo "PE not found: $PE" >&2; exit 2; }
[ -x "$PACK" ] || { echo "pack-loadable.sh not found/executable: $PACK" >&2; exit 2; }

# Entry point MUST be re-derived from the PE every relink (it moves).
ENTRY=$(python3 - "$PE" <<'PY'
import struct,sys
d=open(sys.argv[1],'rb').read(); e=struct.unpack_from('<I',d,0x3C)[0]
opt=e+4+20; aoe=struct.unpack_from('<I',d,opt+16)[0]; ib=struct.unpack_from('<I',d,opt+28)[0]
print(hex(ib+aoe))
PY
)
echo "== [0] PE=$PE  entry=$ENTRY =="

echo "== [1] xex2pack (compress=none, import-map) =="
python3 "$PK" --pe "$PE" --out "$W/boot.xex" --import-map "$MAP" \
  --entry "$ENTRY" --base 0x84000000 --pe-name RB3Enhanced.exe --compress none >/dev/null

echo "== [2] pack-loadable (base + fix_thunks + devkit sign + xexlint) — wine-free =="
"$PACK" "$W/boot.xex" "$W/raw.dll"

# [3] Compress + devkit re-sign via xextool. REQUIRED: the raw wine-free
# container from step [2] has a byte-correct import table + valid signature but
# is REJECTED by the console's XexLoadImage at image-map time; only the
# xextool-produced compressed container loads (proven on HW 2026-07-15, see
# docs/plans/http-bringup-and-rb3eloader-fix-2026-07-15.md). This is the one
# step that still needs wine until LZX-normal is implemented natively in
# xex-patcher. Base image bytes (incl. the fixed import thunks) are preserved.
XEXTOOL="$XENON/tools/oss-xbox-build/xextool/xextool.exe"
echo "== [3] compress + devkit re-sign (xextool -m d -c c) — makes it LOAD =="
[ -f "$XEXTOOL" ] || { echo "xextool not found: $XEXTOOL (needed for a loadable XEX)" >&2; exit 3; }
command -v wine >/dev/null || { echo "wine not found (needed by xextool for the compress step)" >&2; exit 3; }
WINEDEBUG=-all wine "$XEXTOOL" -m d -c c -o "$(winepath -w "$OUT")" "$W/raw.dll" 2>/dev/null \
  || { echo "xextool compress/re-sign FAILED" >&2; exit 3; }
echo "== [3b] re-verify compressed output with xexlint =="
python3 "$MILO/xex-patcher/tools/xexlint.py" "$OUT" | tail -1

if [ "$DEPLOY" = "1" ]; then
  echo "== [4] deploy to console $XBOX =="
  lftp -u xboxftp,xboxftp -e "set xfer:clobber on; set net:timeout 10; \
    put $OUT -o /Usb0/Games/rb3/RB3Enhanced.dll; bye" "$XBOX"
  echo "   deployed; on-drive sha:"
  lftp -u xboxftp,xboxftp -e "set net:timeout 10; get /Usb0/Games/rb3/RB3Enhanced.dll -o $W/chk; bye" "$XBOX" >/dev/null 2>&1
  sha256sum "$W/chk" | cut -c1-16
  echo "   launch with: python3 $XENON/tools/oss-xbox-build/xbdm_cmd.py $XBOX 'magicboot title=\\Device\\Mass0\\Games\\rb3\\default.xex directory=\\Device\\Mass0\\Games\\rb3'"
fi
echo "== done =="
