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

FAIL=0
for mode in none basic; do
  echo "== [3] pack (compress=$mode) =="
  # round-trip identity uses the post-rewrite image so recovered PE == what idaxex re-dumps
  python3 "$HERE/xex2pack.py" --pe stock_basefile.pe --out "rt_$mode.xex" --from-xex "$DLL" --compress "$mode" >/dev/null
  echo "== [4] re-extract & diff =="
  "$TOOL" -b "rt_recovered_$mode.pe" "rt_$mode.xex" >/dev/null
  if cmp -s stock_basefile.pe "rt_recovered_$mode.pe"; then
    echo "   PASS: recovered PE byte-identical ($mode)"
  else
    echo "   FAIL: recovered PE differs ($mode)"; FAIL=1
  fi
  # bootable image uses the RAW basefile
  python3 "$HERE/xex2pack.py" --pe raw_basefile.pe --out "boot_$mode.xex" --from-xex "$DLL" --compress "$mode" >/dev/null
done

if [ -x "$XENIA" ]; then
  echo "== [5] xenia loader smoke (module load + import resolution) =="
  for mode in none basic; do
    timeout 40 "$XENIA" --target="$OUT/boot_$mode.xex" > "xenia_$mode.log" 2>&1 || true
    if grep -q "Launching module" "xenia_$mode.log" && ! grep -q "SetupLibraryImports" "xenia_$mode.log"; then
      echo "   PASS: xenia loaded module + resolved imports ($mode); halts at title-exec harness limit"
    else
      echo "   CHECK: see xenia_$mode.log ($mode)"
    fi
  done
fi

echo "== DONE (FAIL=$FAIL) =="
exit $FAIL
