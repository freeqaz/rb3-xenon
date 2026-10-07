#!/usr/bin/env bash
# native_asan.sh -- run EVERY native target under AddressSanitizer and fail on any
# report. (Lane W16-UO, 2026-10-07.)
#
# WHY: W16-UL found, under ASan and valgrind, an out-of-bounds read
# (rndobj/Text.cpp segmentLength), a stack use-after-scope
# (rndobj/AmbientOcclusion.cpp Tessellate) and allocation-family mismatches
# (KerningTable new[]/delete; class DELETE_OVERLOAD frees of global-new blocks;
# ~ObjectDir's free() of operator-new blocks). None of them is visible to
# native_build_gate.sh or native_health.sh: the program behaves until the heap
# layout shifts. This script is the instrument that sees them, and it exits
# nonzero while any of them, or any new one, is present.
#
# USAGE
#   tools/native_asan.sh [--no-build] [--only NAME[,NAME...]] [--selftest] [WORKTREE]
#     --no-build   run the existing native/build-asan binaries as they are
#     --only       run a subset (reported as PARTIAL, rc 3)
#     --selftest   compile three tiny programs (heap overflow, new[]/delete,
#                  use-after-scope) plus a clean control under ASan and require
#                  this script's own report parser to count 1, 1, 1 and 0; then exit
#
# ENV
#   RB3_ASSETS, RB3_ARK_REF, RB3_SONGS_DTA, RB3_MID_*  as native_health.sh
#   NATIVE_ASAN_TIMEOUT  per-target timeout in s (default 1800; rb3-render is the
#                        long one)
#   NATIVE_ASAN_JOBS     ninja -j for the build (default 8)
#
# BUILD: native/build-asan, configured with -DRB3X_SANITIZE=address (see
# native/CMakeLists.txt for the flags and why -mllvm -asan-globals=0 is needed).
#
# RUNTIME OPTIONS, and why:
#   halt_on_error=0       with -fsanitize-recover=address one run reports every
#                         finding, not just the first
#   suppress_equal_pcs=0  ASan otherwise reports one faulting PC once per run, and
#                         the PC of a bad copy is __asan_memcpy's, shared by every
#                         call site: with Tessellate's phase-1 use-after-scope
#                         present, its phase-3 one was never printed (W16-UO S1)
#   detect_leaks=0        LeakSanitizer is OFF. Every driver exits with its engine
#                         state still allocated (as retail never tears down), so
#                         exit-time leak reports measure the harness, not a defect.
#                         Unload leaks are gated by rb3-render's ul-venue-freed.
#   alloc_dealloc_mismatch=1, new_delete_type_mismatch=1  (ASan's defaults, stated)
#   `ulimit -d unlimited` this box caps data at 32 GB, which the shadow map exceeds
#
# EXIT
#   0  every target ran to its completion line, rc 0, and ASan reported nothing
#   1  ASan reported something, or a target crashed / hung / failed / stopped early
#   2  could not run at all (no native/, configure or build failed)
#   3  ran, but not everything: a subset (--only) or a target UNRUNNABLE (missing
#      input, no GPU). Not clean -- not tested.
#
# The last line of every run is
#   NATIVE_ASAN_RESULT verdict=<CLEAN|FINDINGS|FAIL|PARTIAL|INCOMPLETE|UNRUNNABLE>
#     targets=N ran=N clean=N reports=N distinct=N rc=N
set -u

G() { command grep -a "$@"; }

# ------------------------------------------------------------------ parser --
# asan_reports LOG -> one line per report: "<kind>\t<first frame in our code>"
# "our code" = the first stack frame under /src/ or /native/ of a checkout
# (not /usr/, not the sanitizer runtime), so the signature names a source line.
asan_reports() {
    awk '
    /==[0-9]+==ERROR: AddressSanitizer: / {
        if (inrep) print kind "\t" (site == "" ? "?" : site)
        inrep = 1; site = ""
        k = $0; sub(/.*ERROR: AddressSanitizer: /, "", k); sub(/ on .*/, "", k)
        sub(/ \(.*/, "", k); kind = k; next
    }
    inrep && /^    #[0-9]+ / && site == "" {
        # needs a file:line in our tree: the runtime frames (operator delete,
        # __asan_memcpy) name only the binary, whose path is under native/ too
        if ($0 ~ /\/(src|native)\/[^ ]*\.(cpp|cc|c|h|hpp|inl):[0-9]+/ && $0 !~ /\/usr\//) {
            s = $0; sub(/^    #[0-9]+ 0x[0-9a-f]+ in /, "", s)
            n = split(s, parts, " ")
            loc = parts[n]; sub(/.*\/(src|native)\//, "", loc)
            fn = s; sub(/ \/.*$/, "", fn); sub(/\(.*$/, "", fn)
            site = fn " " loc
        }
    }
    /^SUMMARY: AddressSanitizer: / && inrep { print kind "\t" (site == "" ? "?" : site); inrep = 0 }
    END { if (inrep) print kind "\t" (site == "" ? "?" : site) }
    ' "$1"
}

if [ "${1:-}" = "--selftest" ]; then
    T="$(mktemp -d "${TMPDIR:-$HOME/tmp}/native_asan_selftest.XXXXXX")"
    cat > "$T/ovf.cpp" <<'EOF'
#include <cstdlib>
int main() { char *p = (char *)malloc(8); volatile char c = p[-1]; free(p); return c & 0; }
EOF
    cat > "$T/mix.cpp" <<'EOF'
int main() { int *p = new int[4]; delete p; return 0; }
EOF
    cat > "$T/scope.cpp" <<'EOF'
#include <vector>
struct P { unsigned a; float b; };
int main(int argc, char **) {
    std::vector<P> v; P *q;
    if (argc > 0) { P x; x.a = 1; x.b = 2; q = &x; } else { P y; y.a = 3; y.b = 4; q = &y; }
    v.push_back(*q); return (int)v.size() - 1;
}
EOF
    cat > "$T/clean.cpp" <<'EOF'
#include <vector>
int main() { int *p = new int[4]; delete[] p; std::vector<int> v(3); return v[2]; }
EOF
    ok=1
    for t in ovf:1 mix:1 scope:1 clean:0; do
        n="${t%%:*}"; want="${t##*:}"
        clang++ -fsanitize=address -fsanitize-recover=address -fsanitize-address-use-after-scope \
            -O0 -g -o "$T/$n" "$T/$n.cpp" > "$T/$n.build" 2>&1 || { echo "selftest: cannot build $n"; exit 2; }
        (ulimit -d unlimited 2>/dev/null; ASAN_OPTIONS=halt_on_error=0:suppress_equal_pcs=0:detect_leaks=0 "$T/$n" > "$T/$n.log" 2>&1)
        got="$(asan_reports "$T/$n.log" | wc -l)"
        printf '  selftest %-6s want %s report(s), parsed %s  %s\n' "$n" "$want" "$got" \
            "$(asan_reports "$T/$n.log" | head -1 | tr '\t' ' ')"
        [ "$got" -eq "$want" ] || ok=0
    done
    rm -rf "$T"
    if [ "$ok" = 1 ]; then echo "SELFTEST: PASS"; exit 0; fi
    echo "SELFTEST: FAIL -- the parser does not count what ASan prints"; exit 1
fi

# ------------------------------------------------------------------- args --
DIR=""; NOBUILD=0; ONLY=""
while [ $# -gt 0 ]; do
    case "$1" in
    --no-build) NOBUILD=1 ;;
    --only) ONLY="$2"; shift ;;
    -*) echo "native_asan.sh: unknown option $1" >&2; exit 2 ;;
    *) DIR="$1" ;;
    esac
    shift
done
[ -n "$DIR" ] || DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DIR="$(cd "$DIR" 2>/dev/null && pwd)" || { echo "native_asan.sh: no such dir"; exit 2; }
result() {  # verdict targets ran clean reports distinct rc
    echo "NATIVE_ASAN_RESULT verdict=$1 targets=$2 ran=$3 clean=$4 reports=$5 distinct=$6 rc=$7"
    exit "$7"
}
[ -d "$DIR/native" ] || { echo "native_asan.sh: $DIR has no native/"; result UNRUNNABLE 0 0 0 0 0 2; }

LOGDIR="${TMPDIR:-$HOME/tmp}"; mkdir -p "$LOGDIR"
SLUG="$(printf '%s' "$DIR" | cksum | cut -d' ' -f1)"
ASSETS="${RB3_ASSETS:-$HOME/code/milohax/rb3/orig-assets/xbox-zip}"
ARK_REF="${RB3_ARK_REF:-$HOME/code/milohax/rb3/orig-assets/extracted-xbox-full/songs/gen/songs.dtb}"
MH="${RB3_MILOHAX:-$HOME/code/milohax}"
MID_VICARIOUS="${RB3_MID_VICARIOUS:-$MH/onyx/songs-grinnz/tool/vicarious/notes.mid}"
MID_PILLS="${RB3_MID_PILLS:-$MH/onyx/songs-cort/hurt/pills/notes.mid}"
MID_CENTERFOLD="${RB3_MID_CENTERFOLD:-$MH/rock-band-3-deluxe/_ark/songs/centerfold/centerfold.mid}"
SONGS_DTA="${RB3_SONGS_DTA:-$MH/rb3/orig-assets/extracted/songs/songs.dta}"
TIMEOUT="${NATIVE_ASAN_TIMEOUT:-1800}"
NB="$DIR/native/build-asan"

echo "=== native_asan: $DIR ==="
echo "build:   $NB (RB3X_SANITIZE=address)"

# ------------------------------------------------------- target list sync --
# The run table below mirrors native_health.sh's run_target rows (same inputs,
# same completion markers). A target added there and not here would silently go
# unsanitized, so a name-set difference is a refusal, not a warning.
HEALTH_NAMES="$(G -E '^run_target( --gated)? +rb3-' "$DIR/tools/native_health.sh" \
    | sed -E 's/^run_target( --gated)? +([a-z0-9-]+).*/\2/' | sort | tr '\n' ' ')"
OUR_NAMES="rb3-ark rb3-crowd rb3-dta rb3-frame rb3-gem rb3-harmony rb3-hit rb3-midi rb3-milo rb3-render rb3-save rb3-score rb3-score2 rb3-score3 rb3-score4 rb3-song rb3-vocal rb3-vocal2 "
if [ "$HEALTH_NAMES" != "$OUR_NAMES" ]; then
    echo "REFUSED: native_health.sh runs [$HEALTH_NAMES]"
    echo "         native_asan.sh runs   [$OUR_NAMES]"
    echo "         add the missing row(s) to this script's run table"
    result UNRUNNABLE 0 0 0 0 0 2
fi

# ------------------------------------------------------------------ build --
if [ "$NOBUILD" = 0 ]; then
    BLOG="$LOGDIR/native_asan_build_$SLUG.log"
    if [ ! -f "$NB/CMakeCache.txt" ]; then
        cmake -S "$DIR/native" -B "$NB" -G Ninja -DCMAKE_C_COMPILER=clang \
              -DCMAKE_CXX_COMPILER=clang++ -DRB3X_SANITIZE=address > "$BLOG" 2>&1 \
            || { echo "configure FAILED, see $BLOG"; result UNRUNNABLE 0 0 0 0 0 2; }
    fi
    if ! G -q '^RB3X_SANITIZE:STRING=address$' "$NB/CMakeCache.txt"; then
        echo "REFUSED: $NB is not configured with RB3X_SANITIZE=address"
        result UNRUNNABLE 0 0 0 0 0 2
    fi
    echo "building (log $BLOG) ..."
    cmake --build "$NB" -- -k 0 -j "${NATIVE_ASAN_JOBS:-8}" >> "$BLOG" 2>&1
    brc=$?
    if [ "$brc" -ne 0 ]; then
        echo "build FAILED rc=$brc; first errors:"
        G -E 'error:|undefined reference' "$BLOG" | head -5 | sed 's/^/    /'
        result FAIL 0 0 0 0 0 1
    fi
fi

ulimit -d unlimited 2>/dev/null || echo "warning: could not lift the data limit; the shadow map may fail"
export ASAN_OPTIONS="halt_on_error=0:suppress_equal_pcs=0:detect_leaks=0:alloc_dealloc_mismatch=1:new_delete_type_mismatch=1:symbolize=1"

# -------------------------------------------------------------------- run --
total=0; ran=0; clean=0; nrep=0; partial=0; incomplete=0; failed=0
ALLREP="$LOGDIR/native_asan_reports_$SLUG.tsv"; : > "$ALLREP"

run() {  # NAME MARKER [--needs PATH]... -- ARGV...
    local name="$1" marker="$2"; shift 2
    local missing=""
    while [ "${1:-}" = "--needs" ]; do [ -e "$2" ] || missing="$2"; shift 2; done
    [ "${1:-}" = "--" ] && shift
    if [ -n "$ONLY" ] && ! printf ',%s,' "$ONLY" | G -q ",$name,"; then partial=1; return; fi
    total=$((total + 1))
    if [ ! -x "$NB/$name" ]; then
        printf '  %-10s %-12s -- no executable in %s\n' FAIL "$name" "$NB"; failed=1; return
    fi
    if [ -n "$missing" ]; then
        printf '  %-10s %-12s -- input absent: %s\n' UNRUNNABLE "$name" "$missing"; incomplete=1; return
    fi
    local log="$LOGDIR/native_asan_${name}_$SLUG.log"
    local t0=$SECONDS
    timeout -k 10 "$TIMEOUT" env RB3_ASSETS="$ASSETS" "$NB/$name" "$@" > "$log" 2>&1
    local rc=$? dt=$((SECONDS - t0))
    local reps; reps="$(asan_reports "$log")"
    local n=0; [ -n "$reps" ] && n="$(printf '%s\n' "$reps" | wc -l)"
    [ -n "$reps" ] && printf '%s\n' "$reps" | sed "s/^/$name\t/" >> "$ALLREP"
    local why=""
    if [ "$rc" -eq 2 ] && [ "$name" = rb3-render ]; then
        printf '  %-10s %-12s -- rc=2 NO GPU\n' UNRUNNABLE "$name"; incomplete=1; return
    fi
    if [ "$rc" -eq 1 ] && [ "$name" = rb3-frame ] && G -q 'GpuDevice::Init FAILED\|adapter is the NULL backend' "$log"; then
        printf '  %-10s %-12s -- no usable GPU adapter\n' UNRUNNABLE "$name"; incomplete=1; return
    fi
    ran=$((ran + 1)); nrep=$((nrep + n))
    if [ "$rc" -eq 124 ]; then why="HANG after ${TIMEOUT}s"
    elif [ "$rc" -gt 128 ]; then why="CRASH rc=$rc"
    elif [ "$rc" -ne 0 ]; then why="EXIT rc=$rc"
    elif G -q '^  \[FAIL\] ' "$log"; then why="$(G -c '^  \[FAIL\] ' "$log") gate(s) FAILED"
    elif ! G -Eq "$marker" "$log"; then why="no completion line /$marker/"
    fi
    if [ "$n" -eq 0 ] && [ -z "$why" ]; then
        printf '  %-10s %-12s -- 0 ASan reports, rc=0, completed (%ss)\n' CLEAN "$name" "$dt"
        clean=$((clean + 1))
    else
        failed=1
        printf '  %-10s %-12s -- %s ASan report(s)%s (%ss)\n' FINDINGS "$name" "$n" "${why:+; $why}" "$dt"
        [ -n "$reps" ] && printf '%s\n' "$reps" | sort | uniq -c | sort -rn | head -12 \
            | sed 's/\t/  /; s/^/             /'
    fi
    echo "             log: $log"
}

run rb3-dta     '^Done\. Showed [0-9]+ song'  --needs "$SONGS_DTA" -- "$SONGS_DTA"
run rb3-song    '^RESULT: ALL GATES PASSED'   --needs "$ASSETS" -- "$ASSETS"
run rb3-midi    '^RESULT: ALL GATES PASSED'   --needs "$ASSETS" -- "$ASSETS"
run rb3-gem     '^Done\.$'                    --needs "$ASSETS" --needs "$MID_PILLS" -- "$MID_PILLS"
run rb3-hit     '^Done\.$'                    --needs "$ASSETS" --needs "$MID_PILLS" -- "$MID_PILLS"
run rb3-score   '^Done\.$'                    --needs "$ASSETS" --needs "$MID_PILLS" -- "$MID_PILLS"
run rb3-score2  '^RESULT: OK'                 --needs "$ASSETS" --
run rb3-score3  '^Done\.$'                    --needs "$ASSETS" --needs "$MID_VICARIOUS" --needs "$MID_CENTERFOLD" -- "$MID_VICARIOUS" "PART DRUMS" "$MID_CENTERFOLD"
run rb3-score4  '^Done\.$'                    --needs "$ASSETS" --needs "$MID_VICARIOUS" -- "$MID_VICARIOUS"
run rb3-vocal   '^  all-off \(\+6\) '         --needs "$ASSETS" --needs "$MID_VICARIOUS" -- "$MID_VICARIOUS"
run rb3-vocal2  '^Done\.$'                    --needs "$ASSETS" --needs "$MID_VICARIOUS" -- "$MID_VICARIOUS"
run rb3-harmony '^Done\.$'                    --needs "$ASSETS" --needs "$MID_CENTERFOLD" -- "$MID_CENTERFOLD"
run rb3-crowd   '^=== M12 complete'           --needs "$ASSETS" --needs "$MID_VICARIOUS" -- "$MID_VICARIOUS"
run rb3-save    '^=== ALL ROUND-TRIPS OK'     --
run rb3-ark     '^RESULT: ALL GATES PASSED'   --needs "$ASSETS" --needs "$ARK_REF" -- "$ASSETS" "$ARK_REF" --config-dump "$LOGDIR/native_asan_syscfg_ark_$SLUG.txt"
run rb3-frame   '^rb3-frame: OK '             -- "$LOGDIR/native_asan_frame_$SLUG.png"
run rb3-milo    '^RESULT: ALL GATES PASSED'   --needs "$ASSETS" -- "$ASSETS" ui/track/gen/tracksystem_meshes.milo_xbox
run rb3-render  '^RESULT: ALL GATES PASSED'   --needs "$ASSETS" -- "$ASSETS" "$LOGDIR/native_asan_render_out_$SLUG"

distinct="$(cut -f2- "$ALLREP" | sort -u | G -c .)"
echo
if [ "$distinct" -gt 0 ]; then
    echo "--- distinct findings (kind, first frame in our code), runs that hit each ---"
    cut -f2- "$ALLREP" | sort | uniq -c | sort -rn | sed 's/\t/  /; s/^/  /'
    echo "  all reports: $ALLREP"
fi
if [ "$failed" = 1 ]; then
    v=FAIL; [ "$nrep" -gt 0 ] && v=FINDINGS
    result "$v" "$total" "$ran" "$clean" "$nrep" "$distinct" 1
fi
[ "$partial" = 1 ] && result PARTIAL "$total" "$ran" "$clean" "$nrep" "$distinct" 3
[ "$incomplete" = 1 ] && result INCOMPLETE "$total" "$ran" "$clean" "$nrep" "$distinct" 3
result CLEAN "$total" "$ran" "$clean" "$nrep" "$distinct" 0
