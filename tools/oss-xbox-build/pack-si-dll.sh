#!/usr/bin/env bash
# pack-si-dll.sh — turn a freshly-linked from-source RB3Enhanced PE into a
# loadable, devkit-signed XEX2 DLL for the RB3DX Xbox 360 console, and validate it.
#
# Pipeline (proven on hardware — see docs/plans/si-hw-fix/CRASH-2same-instrument…md
# "SI-LOADABLE-RECIPE"): xex2pack (basic compress, correct-VA import block from the
# PE via --import-map) -> xextool -e d -c b (flat base) -> fix_thunks (repair the
# type-1 import thunks the packer emits wrong) -> splice fixed base back -> xextool
# -m d -c c (recompress + rehash + devkit re-sign) -> xexlint (offline gate).
#
# Usage:
#   pack-si-dll.sh [PE] [OUT.dll]
#   pack-si-dll.sh --deploy [PE] [OUT.dll]     # also FTP to the console after xexlint passes
#
#   PE       default: K-link/RB3Enhanced.exe (the from-source build output)
#   OUT.dll  default: <staging>/RB3Enhanced.fromsource.compressed.dll
#
# Env: XBOX=192.168.8.180 (deploy target). Needs wine + python3 + capstone.
set -euo pipefail

DEPLOY=0
if [ "${1:-}" = "--deploy" ]; then DEPLOY=1; shift; fi

MILO=/home/free/code/milohax
XENON=$MILO/rb3-xenon
XP=$MILO/xex-patcher/tools                       # xextool.exe, fix_thunks.py, xexlint.py
PK=$XENON/tools/xex2pack/xex2pack.py
MAP=$XENON/docs/plans/strategy-b/checkpoints/finish/ordinal-map.json
PE=${1:-$XENON/tools/oss-xbox-build/K-link/RB3Enhanced.exe}
OUT=${2:-$XENON/tools/oss-xbox-build/RB3Enhanced.fromsource.compressed.dll}
XBOX=${XBOX:-192.168.8.180}
W=$(mktemp -d)
trap 'rm -rf "$W"' EXIT

[ -f "$PE" ] || { echo "PE not found: $PE" >&2; exit 2; }

# Entry point MUST be re-derived from the PE every relink (it moves).
ENTRY=$(python3 - "$PE" <<'PY'
import struct,sys
d=open(sys.argv[1],'rb').read(); e=struct.unpack_from('<I',d,0x3C)[0]
opt=e+4+20; aoe=struct.unpack_from('<I',d,opt+16)[0]; ib=struct.unpack_from('<I',d,opt+28)[0]
print(hex(ib+aoe))
PY
)
echo "== [0] PE=$PE  entry=$ENTRY =="

echo "== [1] xex2pack (basic compress, import-map) =="
python3 "$PK" --pe "$PE" --out "$W/boot.xex" --import-map "$MAP" \
  --entry "$ENTRY" --base 0x84000000 --pe-name RB3Enhanced.exe --compress basic >/dev/null

echo "== [2] flat base XEX (xextool -e d -c b) =="
wine "$XP/xextool.exe" -e d -c b -o "$W/work.xex" "$W/boot.xex" >/dev/null 2>&1

echo "== [3] dump faithful base (xextool -b) =="
wine "$XP/xextool.exe" -b "$W/base_in.bin" "$W/boot.xex" >/dev/null 2>&1

echo "== [4] repair import thunks (fix_thunks) =="
python3 "$XP/fix_thunks.py" "$W/boot.xex" "$W/base_in.bin" "$W/base_fixed.bin"

echo "== [5] splice fixed base into flat XEX =="
python3 - "$W/work.xex" "$W/base_fixed.bin" <<'PY'
import struct,sys
wp,fp=sys.argv[1],sys.argv[2]
work=bytearray(open(wp,'rb').read()); fix=open(fp,'rb').read()
soh=struct.unpack('>I',work[8:12])[0]
work[soh:soh+len(fix)]=fix
open(wp,'wb').write(work)
print(f"   spliced {len(fix)} base bytes at file offset 0x{soh:X}")
PY

echo "== [6] recompress + rehash + devkit re-sign (xextool -m d -c c) =="
wine "$XP/xextool.exe" -m d -c c -o "$OUT" "$W/work.xex" >/dev/null 2>&1
echo "   -> $OUT ($(stat -c%s "$OUT") bytes, sha $(sha256sum "$OUT" | cut -c1-16))"

echo "== [7] xexlint (offline gate — must PASS before hardware) =="
( cd "$XP" && python3 "$XP/xexlint.py" "$OUT" )

if [ "$DEPLOY" = "1" ]; then
  echo "== [8] deploy to console $XBOX =="
  lftp -u xboxftp,xboxftp -e "set xfer:clobber on; set net:timeout 10; \
    put $OUT -o /Usb0/Games/rb3/RB3Enhanced.dll; bye" "$XBOX"
  echo "   deployed; on-drive sha:"
  lftp -u xboxftp,xboxftp -e "set net:timeout 10; get /Usb0/Games/rb3/RB3Enhanced.dll -o $W/chk; bye" "$XBOX" >/dev/null 2>&1
  sha256sum "$W/chk" | cut -c1-16
  echo "   launch with: python3 $XENON/tools/oss-xbox-build/xbdm_cmd.py $XBOX 'magicboot title=\\Device\\Mass0\\Games\\rb3\\default.xex directory=\\Device\\Mass0\\Games\\rb3'"
fi
echo "== done =="
