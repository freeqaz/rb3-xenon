#!/usr/bin/env bash
# Lane X identity round-trip proof for xex2pack.
# 1. extract stock DLL basefile via idaxex
# 2. de-mangle idaxex's import-thunk rewrite -> raw basefile
# 3. repack (compress=none AND compress=basic) with xex2pack
# 4. re-extract & diff recovered PE == original basefile (byte-exact)
# 5. (optional) load each through xenia-headless to exercise its real XEX loader
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
TOOL="${XEX1TOOL:-/home/free/code/milohax/reverse-compiler-refs/idaxex/xex1tool/build/xex1tool}"
DLL="${STOCK_DLL:-/home/free/code/milohax/rb3-xenon/_rb3e07/rb3e07/RB3Enhanced.dll}"
XENIA="${XENIA:-/home/free/code/milohax/xenia/build/bin/Linux/Checked/xenia-headless}"
OUT="${1:-/home/free/code/milohax/rb3-xenon/tools/oss-xbox-build/X/work}"
mkdir -p "$OUT"; cd "$OUT"

echo "== [1] extract stock basefile (idaxex -b, post-import-rewrite) =="
"$TOOL" -b stock_basefile.pe "$DLL" >/dev/null

echo "== [2] restore raw import thunks =="
python3 "$HERE/deidax_thunks.py" "$DLL" stock_basefile.pe raw_basefile.pe

# The stock XEX's entry point is the ground truth for the boot images. It is NOT
# recoverable from the basefile: a basefile keeps link.exe's original ImageBase, so
# image_base+AddressOfEntryPoint is off by the rebase delta (0x8801B590 vs the true
# 0x8401B590 here). xex2pack shipped the wrong one for months because the only gate
# was the basefile diff below, which is blind to every header field.
WANT_ENTRY=$(python3 - "$DLL" <<'PY'
import struct,sys
x=open(sys.argv[1],'rb').read()
hc=struct.unpack('>I',x[0x14:0x18])[0]
for i in range(hc):
    k,v=struct.unpack('>II',x[0x18+i*8:0x18+i*8+8])
    if k==0x00010100: print("0x%08X"%v); break
PY
)
echo "   stock entry point = $WANT_ENTRY"

FAIL=0
for mode in none basic; do
  echo "== [3] pack (compress=$mode) =="
  # Identity leg: deliberately packs the idaxex-MANGLED basefile (its thunks are
  # idaxex's li r3/li r4 stubs), so the self-check would rightly reject it --
  # --no-verify. This leg proves basefile preservation only, nothing about
  # loadability; the boot leg below is the one that must be well-formed.
  python3 "$HERE/xex2pack.py" --pe stock_basefile.pe --out "rt_$mode.xex" \
      --from-xex "$DLL" --compress "$mode" --no-verify >/dev/null
  echo "== [4] re-extract & diff =="
  "$TOOL" -b "rt_recovered_$mode.pe" "rt_$mode.xex" >/dev/null
  if cmp -s stock_basefile.pe "rt_recovered_$mode.pe"; then
    echo "   PASS: recovered PE byte-identical ($mode)"
  else
    echo "   FAIL: recovered PE differs ($mode)"; FAIL=1
  fi

  # Bootable image uses the RAW (thunk-restored) basefile. The self-check runs by
  # default and exits nonzero on a bad digest chain or malformed import record.
  echo "== [4b] pack bootable + self-check ($mode) =="
  if python3 "$HERE/xex2pack.py" --pe raw_basefile.pe --out "boot_$mode.xex" \
        --from-xex "$DLL" --compress "$mode" >"pack_$mode.log" 2>&1; then
    echo "   PASS: self-check clean ($mode)"
  else
    echo "   FAIL: self-check rejected boot_$mode.xex -- see pack_$mode.log"; FAIL=1
  fi

  GOT_ENTRY=$(grep -o 'entry_point *= *0x[0-9A-Fa-f]*' "pack_$mode.log" | grep -o '0x[0-9A-Fa-f]*')
  if [ "$((GOT_ENTRY))" = "$((WANT_ENTRY))" ]; then
    echo "   PASS: entry point $GOT_ENTRY matches stock ($mode)"
  else
    echo "   FAIL: entry point $GOT_ENTRY != stock $WANT_ENTRY ($mode)"; FAIL=1
  fi

  # Independent cross-check with the project's own linter, when present. This is a
  # different implementation than xex2pack's self-check, so agreement is meaningful.
  XEXLINT="${XEXLINT:-/home/free/code/milohax/xex-patcher/tools/xexlint.py}"
  if [ -f "$XEXLINT" ]; then
    # The zero RSA signature is BY DESIGN for this packer (pack-loadable.sh
    # devkit-signs downstream), so it is the one expected reject.
    BAD=$(python3 "$XEXLINT" "boot_$mode.xex" 2>&1 | grep '\[FAIL\]' | grep -cv 'signature: RSA signature field is all zero' || true)
    if [ "$BAD" = "0" ]; then
      echo "   PASS: xexlint clean apart from the by-design zero signature ($mode)"
    else
      echo "   FAIL: xexlint reports $BAD unexpected reject(s) ($mode)"; FAIL=1
      python3 "$XEXLINT" "boot_$mode.xex" 2>&1 | grep '\[FAIL\]' | grep -v 'RSA signature field is all zero'
    fi
  else
    echo "   SKIP: xexlint not found at $XEXLINT ($mode)"
  fi
done

if [ -x "$XENIA" ]; then
  # INFORMATIONAL ONLY. xenia-headless asserts in XThread::GetCurrentThread once the
  # module is launched -- a harness limitation, not a property of our image. The old
  # criterion here ("Launching module" present AND "SetupLibraryImports" absent) was
  # a negative grep that reported PASS even though the process dumped core, and it
  # could never fail on a malformed image. It is not a gate; do not treat it as one.
  echo "== [5] xenia loader smoke (INFORMATIONAL -- not a gate) =="
  for mode in none basic; do
    timeout 40 "$XENIA" --target="$OUT/boot_$mode.xex" > "xenia_$mode.log" 2>&1 || true
    if grep -q "Launching module" "xenia_$mode.log"; then
      echo "   info: xenia reached 'Launching module' ($mode); see xenia_$mode.log"
    else
      echo "   info: xenia did NOT reach 'Launching module' ($mode) -- inspect xenia_$mode.log"
    fi
  done
fi

echo "== DONE (FAIL=$FAIL) =="
exit $FAIL
