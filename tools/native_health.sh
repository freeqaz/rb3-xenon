#!/usr/bin/env bash
# Collected NATIVE health measurement: LINK + RUNTIME + SCATTER, one line.
#
# WHY THIS EXISTS
# ---------------
# Native progress was measured by ONE instrument, `tools/native_build_gate.sh`,
# and that instrument asserts only that 18 targets LINK. Its own header says so:
# it proves targets link, not that they RUN. An executable that segfaults on the
# first instruction passes it.
#
# Real runtime instruments already existed -- rb3-render (~27 Gate() sites),
# rb3-milo (12), rb3-ark (11, with a REAL negative control) -- but they were
# MANUAL, UNSCHEDULED and UNCOLLECTED. Nothing ran them, nothing recorded their
# numbers, and nothing noticed when one stopped being runnable at all. Lane
# S5-NATIVE measured exactly that: rb3-render has been unable to reach 24 of its
# 27 gates on this machine because the host's Vulkan ICD is broken, and no
# artifact anywhere recorded the fact.
#
# ✅ RESOLVED 2026-09-10 (lane N1-GPUGATES): the NVIDIA module was reloaded on a
# package update -- no reboot -- and rb3-render now runs to completion. Verified
# FUNCTIONALLY, not by version string: vkCreateInstance returns VK_SUCCESS and
# enumerates 2 physical devices.
#
# ⛔ And the first run on a working GPU immediately refuted this script's own
# selftest: `render-stale` was being credited as a WORKING CONTROL over a
# PRE-EXISTING RED, because the pair it checked was the WRONG PAIR. See the
# delta-scored handpose block below -- that defect is the single most important
# thing this file has produced, and it was invisible while the GPU was broken.
#
# `tools/scatter_audit.py` had the same disease in the other direction: it still
# RAN, and its headline count had drifted 42 -> 47 unnoticed, because the 42 was
# recorded in a dated plan doc (docs/plans/x10-band-geometry-2026-08-03.md:188)
# that nothing re-reads.
#
# So this script does three things the link gate does not:
#   1. RUNS EVERY native target (all 18, since W16-PE) on real data and FAILS on
#      a crash, a hang, a nonzero exit, a [FAIL] line, or a run that exits 0
#      without printing its completion line; counts gate verdicts where the
#      target prints them. (Before W16-PE it ran 4 of 18, which is how
#      rb3-vocal2/rb3-harmony crashed for two months with every check green.)
#   2. Has a --selftest that DEMONSTRATES the negative controls go red.
#      A runtime gate nobody has shown able to FAIL is worth nothing, and this
#      repo's ledger of vacuous gates is long (the native gate itself once
#      counted stale executables off disk and could not fail; report_result's
#      only gate into COMPLETE had never once fired).
#   3. Emits ONE machine-readable line, so the numbers can be diffed against a
#      committed artifact instead of living in a lane's transcript.
#
# WHAT IT DELIBERATELY DOES NOT DO
#   It is NOT wired into CI and NOT wired into the default ninja build. CI has
#   zero native coverage today, and adding a slow (cold: ~6 min) or
#   environment-dependent edge to every build is its own harm. See the cadence
#   recommendation in docs/decomp/NATIVE_HEALTH.md.
#
# USAGE
#     tools/native_health.sh [project_dir] [--selftest] [--skip-link]
#
#     --selftest   additionally exercise every negative control and REQUIRE each
#                  to go red. A control that stays green is a HARD FAIL: it means
#                  the instrument it guards is vacuous.
#     --skip-link  skip the link gate (it dominates runtime on a cold tree).
#                  The verdict then self-labels, exactly like the link gate's own
#                  PARTIAL: it can never report a bare PASS.
#
#     RB3_ASSETS   data dir (default: ~/code/milohax/rb3/orig-assets/xbox-zip)
#     RB3_ARK_REF  independently-extracted reference file for rb3-ark's
#                  byte-exactness gate (default: the extracted-xbox-full copy)
#     RB3_MILOHAX  root for the chart/dta inputs below (default ~/code/milohax)
#     RB3_MID_VICARIOUS / RB3_MID_PILLS / RB3_MID_CENTERFOLD / RB3_SONGS_DTA
#                  per-input overrides; see the RUNTIME section for which target
#                  reads which. An absent input makes that target UNRUNNABLE
#                  (nodata), never a pass.
#     NATIVE_HEALTH_TIMEOUT  per-target wall bound in seconds (default 120);
#                  exceeding it is a FAIL (hang).
#
# EXIT CODES -- deliberately the SAME vocabulary as native_build_gate.sh, so the
# two can be read by one reader:
#     0  everything asked for was measured, and all of it is healthy.
#     1  something is BROKEN: the link gate failed, a runtime target crashed /
#        hung / exited nonzero / failed a gate / stopped before its completion
#        line, or (under --selftest) a negative control did NOT go red.
#     2  the script COULD NOT RUN AT ALL (bad option, no such dir, no native/).
#     3  it RAN and does NOT VOUCH FOR FULL COVERAGE -- a runtime instrument was
#        UNRUNNABLE for a verified environmental reason (no GPU, absent assets),
#        or --skip-link was passed. NOTHING IS KNOWN TO BE BROKEN; something was
#        NOT TESTED. Absence of evidence, and a distinct fact from both.
#
# THE SUMMARY LINE
#     NATIVE_HEALTH_RESULT verdict=... link=... runtime=... selftest=... rc=...
#     Keys, order and spelling are the contract; add fields at the END only.
#     Appended by W16-PE: runtime_crashed=<N targets that died on a signal>
#     runtime_failed=<name:kind,...|none>, kind in
#     crash|hang|exit|gatefail|nocomplete|nogates.

set -uo pipefail

emit() {  # verdict link link_ver link_exp link_skip runtime rt_ran rt_tot
          # gates_pass gates_fail unrunnable selftest sc_unlinked sc_b sc_multi rc
    echo "NATIVE_HEALTH_RESULT verdict=$1 link=$2 link_verified=$3 link_expected=$4" \
         "link_skipped=$5 runtime=$6 runtime_ran=$7 runtime_total=$8" \
         "gates_pass=$9 gates_fail=${10} unrunnable=${11} selftest=${12}" \
         "scatter_unlinked=${13} scatter_dirb=${14} scatter_multihost=${15} rc=${16}" \
         "handpose_controls=${17:--} handpose_baseline_fail=${18:--}" \
         "runtime_crashed=${19:--} runtime_failed=${20:--}"
}

SELFTEST=0
SKIP_LINK=0
DIR=""
for arg in "$@"; do
    case "$arg" in
        --selftest)  SELFTEST=1 ;;
        --skip-link) SKIP_LINK=1 ;;
        -*) echo "native_health: unknown option $arg" >&2
            emit UNRUNNABLE - 0 0 0 - 0 0 0 0 - - - - - 2; exit 2 ;;
        *)  DIR="$arg" ;;
    esac
done
[ -n "$DIR" ] || DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DIR="$(cd "$DIR" 2>/dev/null && pwd)" || {
    echo "native_health: no such dir" >&2
    emit UNRUNNABLE - 0 0 0 - 0 0 0 0 - - - - - 2; exit 2; }
[ -d "$DIR/native" ] || {
    echo "native_health: no native/ under $DIR" >&2
    emit UNRUNNABLE - 0 0 0 - 0 0 0 0 - - - - - 2; exit 2; }

# grep is a shell FUNCTION shimming to `ugrep -I` in interactive shells here and
# is BINARY-BLIND -- false negatives only, shaped exactly like a decisive
# negative. Pin the real one and always pass -a.
G() { command grep -a "$@"; }

LOGDIR="${TMPDIR:-$HOME/tmp}"; mkdir -p "$LOGDIR"
SLUG="$(printf '%s' "$DIR" | cksum | cut -d' ' -f1)"
ASSETS="${RB3_ASSETS:-$HOME/code/milohax/rb3/orig-assets/xbox-zip}"
ARK_REF="${RB3_ARK_REF:-$HOME/code/milohax/rb3/orig-assets/extracted-xbox-full/songs/gen/songs.dtb}"

echo "=== NATIVE HEALTH ==="
echo "tree:    $DIR"
echo "assets:  $ASSETS"

# ------------------------------------------------------------------ LINK ----
link_verdict="SKIPPED"; link_ver=0; link_exp=0; link_skip=0; link_rc=0
if [ $SKIP_LINK -eq 0 ]; then
    echo
    echo "--- link gate (tools/native_build_gate.sh) ---"
    GLOG="$LOGDIR/native_health_link_$SLUG.log"
    "$DIR/tools/native_build_gate.sh" "$DIR" > "$GLOG" 2>&1
    link_rc=$?
    # Parse the CONTRACT line, never the prose verdict. The full-pass prose is
    # `PASS  (rc=0, ...` and the incomplete one is `PASS (INCOMPLETE: ...` --
    # one space apart, and that has been mis-relayed upstream before.
    rline="$(G '^NATIVE_GATE_RESULT ' "$GLOG" | tail -1)"
    if [ -z "$rline" ]; then
        echo "  the link gate emitted NO NATIVE_GATE_RESULT line -- it did not run to"
        echo "  completion. Refusing to report a verdict its own contract does not support."
        echo "  log: $GLOG"
        emit UNRUNNABLE - 0 0 0 - 0 0 0 0 - - - - - 2; exit 2
    fi
    echo "  $rline"
    for kv in $rline; do case "$kv" in
        verdict=*)  link_verdict="${kv#verdict=}" ;;
        expected=*) link_exp="${kv#expected=}" ;;
        verified=*) link_ver="${kv#verified=}" ;;
        skipped=*)  link_skip="${kv#skipped=}" ;;
    esac; done
    echo "  log: $GLOG"
else
    echo
    echo "--- link gate SKIPPED (--skip-link) -- this run does NOT vouch for linkage ---"
fi

# --------------------------------------------------------------- RUNTIME ----
# EVERY native target is RUN here, not just the four that print the house
# `  [PASS]/[FAIL]` contract. (Lane W16-PE, 2026-10-06.)
#
# WHY: until W16-PE this section ran four targets (milo, ark, render, score2).
# The other 14 were linked by the gate and run by NOTHING. rb3-vocal2 and
# rb3-harmony segfaulted on their first frame from f3ec9592d (2026-08-03) until
# W16-PD found it by hand on 2026-10-03 -- two months during which every gate
# and every health run on this box stayed green. A target nobody runs is worth
# nothing, however well it links. The full set costs ~3 s wall on a warm tree
# (measured: render 1.3 s, frame 0.6 s, everything else <0.25 s), so there was
# never a cost reason to leave any of them out.
#
# WHAT EACH RUN MUST DO TO BE GREEN, in the order it is checked:
#   1. not HANG      -- bounded by `timeout` ($NATIVE_HEALTH_TIMEOUT, default
#                       120 s). rc 124 = HANG. (If TERM is ignored, `-k` sends
#                       KILL and the rc is 137, which reads as CRASH SIGKILL --
#                       still a FAIL, just labelled by the signal.)
#   2. not CRASH     -- rc > 128 means death by signal (coreutils timeout
#                       re-raises the child's signal on itself, so a SIGSEGV is
#                       rc 139 through the wrapper as well -- measured).
#   3. exit 0        -- any other nonzero rc is EXIT.
#   4. no `  [FAIL] ` line.
#   5. print its COMPLETION MARKER -- the line each driver prints only after its
#                       last stage. rc 0 is not enough on its own: rb3-midi
#                       `--list` is a real `return 0` that runs no gate at all,
#                       and the selftest below uses exactly that path as the
#                       control for this check.
#   6. --gated targets must also print >=1 `  [PASS] ` line.
#
# ⛔ SEMANTICS CHANGE vs. the pre-W16-PE run_target: a run that printed ZERO gate
# lines used to be classified UNRUNNABLE ("novgates") BEFORE its rc was looked
# at, so a target that segfaulted before its first [PASS] line made the whole
# health run INCOMPLETE (rc=3, "nothing is known to be broken") instead of FAIL.
# A crash is now a FAIL whatever was or was not printed. UNRUNNABLE is reserved
# for VERIFIED environmental reasons checked BEFORE the run: no executable, a
# declared input (--needs) absent from disk, or no GPU (rb3-render rc 2 /
# rb3-frame's adapter-init message).
#
# Inputs are DECLARED here instead of being left to each driver's hardcoded
# default, so a missing file is reported as `nodata` rather than read as a
# driver abort. All of them are real shipped data (the ark, retail songs.dta,
# real RB3/RB3DX charts) except rb3-score2 (arithmetic against M5),
# rb3-save (serialization round-trip) and rb3-frame (GPU clear), which take none.
echo
echo "--- runtime: every native target, run ---"
NB="$DIR/native/build"
MH="${RB3_MILOHAX:-$HOME/code/milohax}"
MID_VICARIOUS="${RB3_MID_VICARIOUS:-$MH/onyx/songs-grinnz/tool/vicarious/notes.mid}"
MID_PILLS="${RB3_MID_PILLS:-$MH/onyx/songs-cort/hurt/pills/notes.mid}"
MID_CENTERFOLD="${RB3_MID_CENTERFOLD:-$MH/rock-band-3-deluxe/_ark/songs/centerfold/centerfold.mid}"
SONGS_DTA="${RB3_SONGS_DTA:-$MH/rb3/orig-assets/extracted/songs/songs.dta}"
RT_TIMEOUT="${NATIVE_HEALTH_TIMEOUT:-120}"
gates_pass=0; gates_fail=0; rt_ran=0; rt_total=0; unrunnable=(); green=()
rt_crashed=0; rt_failed=()

# classify_run LOG MARKER GATED NAME -- cmd...
# Runs cmd under timeout, then sets CL_STATUS (OK|FAIL|UNRUNNABLE), CL_KIND
# (ok|hang|crash|exit|gatefail|nocomplete|nogates|nogpu), CL_WHY, CL_RC, CL_P, CL_F.
# Shared by the real runs AND the selftest controls, so a control that goes red
# proves THIS classifier fires -- not a copy of it.
classify_run() {
    local log="$1" marker="$2" gated="$3" name="$4"; shift 5
    # rc captured DIRECTLY. Never `prog | tail; echo $?` -- that reports TAIL's rc.
    timeout -k 10 "$RT_TIMEOUT" "$@" > "$log" 2>&1
    local rc=$?
    CL_RC=$rc
    CL_P=$(G -c '^  \[PASS\] ' "$log"); CL_P=${CL_P:-0}
    CL_F=$(G -c '^  \[FAIL\] ' "$log"); CL_F=${CL_F:-0}
    if [ "$rc" -eq 124 ]; then
        CL_STATUS=FAIL; CL_KIND=hang; CL_WHY="HANG -- still running after ${RT_TIMEOUT}s"; return
    fi
    if [ "$rc" -gt 128 ]; then
        local sig; sig="$(kill -l $((rc - 128)) 2>/dev/null)" || sig="$((rc - 128))"
        CL_STATUS=FAIL; CL_KIND=crash; CL_WHY="CRASH -- died on SIG$sig (rc=$rc)"; return
    fi
    if [ "$rc" -eq 2 ] && [ "$name" = "rb3-render" ]; then
        # main_render.cpp returns 2 for "NO GPU" specifically, BEFORE the
        # load/pose/draw/png gates. That is the environment, not the code.
        CL_STATUS=UNRUNNABLE; CL_KIND=nogpu; CL_WHY="rc=2 NO GPU; reached only $((CL_P + CL_F)) gates"; return
    fi
    if [ "$rc" -eq 1 ] && [ "$name" = "rb3-frame" ] \
       && G -q 'GpuDevice::Init FAILED\|adapter is the NULL backend' "$log"; then
        CL_STATUS=UNRUNNABLE; CL_KIND=nogpu; CL_WHY="no usable GPU adapter (main_frame.cpp's own message)"; return
    fi
    if [ "$rc" -ne 0 ]; then
        CL_STATUS=FAIL; CL_KIND=exit; CL_WHY="EXIT rc=$rc"; return
    fi
    if [ "$CL_F" -ne 0 ]; then
        CL_STATUS=FAIL; CL_KIND=gatefail; CL_WHY="rc=0 but $CL_F gate(s) FAILED"; return
    fi
    if ! G -Eq "$marker" "$log"; then
        CL_STATUS=FAIL; CL_KIND=nocomplete
        CL_WHY="rc=0 but never printed its completion line /$marker/ -- it stopped early"; return
    fi
    if [ "$gated" = 1 ] && [ "$CL_P" -eq 0 ]; then
        CL_STATUS=FAIL; CL_KIND=nogates; CL_WHY="rc=0 but ZERO gate lines -- it measured nothing"; return
    fi
    CL_STATUS=OK; CL_KIND=ok; CL_WHY="rc=0"
}

run_target() {  # [--gated] NAME MARKER [--needs PATH]... -- ARGV...
    local gated=0
    if [ "${1:-}" = "--gated" ]; then gated=1; shift; fi
    local name="$1" marker="$2"; shift 2
    local need missing=""
    while [ "${1:-}" = "--needs" ]; do
        need="$2"; shift 2
        [ -e "$need" ] || missing="${missing:-$need}"
    done
    [ "${1:-}" = "--" ] && shift
    rt_total=$((rt_total + 1))
    if [ ! -x "$NB/$name" ]; then
        printf '  %-10s %-12s -- no executable (the link gate is the authority on why)\n' UNRUNNABLE "$name"
        unrunnable+=("$name:nobinary"); return
    fi
    if [ -n "$missing" ]; then
        printf '  %-10s %-12s -- input absent: %s\n' UNRUNNABLE "$name" "$missing"
        unrunnable+=("$name:nodata"); return
    fi
    local log="$LOGDIR/native_health_${name}_$SLUG.log"
    classify_run "$log" "$marker" "$gated" "$name" -- "$NB/$name" "$@"
    case "$CL_STATUS" in
    UNRUNNABLE)
        printf '  %-10s %-12s -- %s\n' UNRUNNABLE "$name" "$CL_WHY"
        G '\[FAIL\]' "$log" | head -1 | sed 's/^/             /'
        unrunnable+=("$name:$CL_KIND"); return ;;
    esac
    rt_ran=$((rt_ran + 1))
    gates_pass=$((gates_pass + CL_P)); gates_fail=$((gates_fail + CL_F))
    if [ "$CL_STATUS" = FAIL ]; then
        printf '  %-10s %-12s -- %s; %s gate(s) passed, %s failed\n' FAIL "$name" "$CL_WHY" "$CL_P" "$CL_F"
        G '^  \[FAIL\] ' "$log" | head -5 | sed 's/^/             /'
        echo "             last output: $(G -v "^timeout: " "$log" | tail -n 1 | cut -c1-100)"
        rt_failed+=("$name:$CL_KIND")
        [ "$CL_KIND" = crash ] && rt_crashed=$((rt_crashed + 1))
    else
        if [ "$gated" = 1 ]; then
            printf '  %-10s %-12s -- rc=0, %s gate(s) passed\n' OK "$name" "$CL_P"
        else
            printf '  %-10s %-12s -- rc=0, ran to completion\n' OK "$name"
        fi
        green+=("$name")
    fi
    echo "             log: $log"
}

# Order and membership mirror native_build_gate.sh's KNOWN_TARGETS, so a target
# added there without a row here is visible as a count mismatch (runtime_total
# vs link_expected) on the summary line.
run_target         rb3-dta     '^Done\. Showed [0-9]+ song'  --needs "$SONGS_DTA"      -- "$SONGS_DTA"
run_target --gated rb3-song    '^RESULT: ALL GATES PASSED'   --needs "$ASSETS"         -- "$ASSETS"
run_target --gated rb3-midi    '^RESULT: ALL GATES PASSED'   --needs "$ASSETS"         -- "$ASSETS"
run_target         rb3-gem     '^Done\.$'                    --needs "$MID_PILLS"      -- "$MID_PILLS"
run_target         rb3-hit     '^Done\.$'                    --needs "$MID_PILLS"      -- "$MID_PILLS"
run_target         rb3-score   '^Done\.$'                    --needs "$MID_PILLS"      -- "$MID_PILLS"
run_target --gated rb3-score2  '^RESULT: OK'                                           --
run_target         rb3-score3  '^Done\.$'                    --needs "$MID_VICARIOUS"  -- "$MID_VICARIOUS"
run_target         rb3-score4  '^Done\.$'                    --needs "$MID_VICARIOUS"  -- "$MID_VICARIOUS"
run_target         rb3-vocal   '^  all-off \(\+6\) '         --needs "$MID_VICARIOUS"  -- "$MID_VICARIOUS"
run_target         rb3-vocal2  '^Done\.$'                    --needs "$MID_VICARIOUS"  -- "$MID_VICARIOUS"
run_target         rb3-harmony '^Done\.$'                    --needs "$MID_CENTERFOLD" -- "$MID_CENTERFOLD"
run_target         rb3-crowd   '^=== M12 complete'           --needs "$MID_VICARIOUS"  -- "$MID_VICARIOUS"
run_target         rb3-save    '^=== ALL ROUND-TRIPS OK'                               --
run_target --gated rb3-ark     '^RESULT: ALL GATES PASSED'   --needs "$ASSETS" --needs "$ARK_REF" -- "$ASSETS" "$ARK_REF"
run_target         rb3-frame   '^rb3-frame: OK '                                       -- "$LOGDIR/native_health_frame_$SLUG.png"
run_target --gated rb3-milo    '^RESULT: ALL GATES PASSED'   --needs "$ASSETS"         -- "$ASSETS" ui/track/gen/tracksystem_meshes.milo_xbox
run_target --gated rb3-render  '^RESULT: ALL GATES PASSED'   --needs "$ASSETS"         -- "$ASSETS" "$LOGDIR/native_health_render_out_$SLUG"

# -------------------------------------------------------------- SELFTEST ----
# Does each negative control actually go RED? The PAIR is the control: the
# positive run above must be green FIRST, or a red here proves nothing (it would
# just be the same breakage twice). Same discipline as the house rule "check a
# witness CAN DISCRIMINATE before it BLOCKS work".
selftest="SKIPPED"
hp_ok=0; hp_tot=0; hp_base="-"; hp_ctl="-"
if [ $SELFTEST -eq 1 ]; then
    echo
    echo "--- selftest: do the negative controls GO RED? ---"
    st_ok=0; st_bad=0; st_skip=0
    # THE PAIR IS THE CONTROL, and this has to be ENFORCED rather than merely
    # documented. If the target's POSITIVE run was not green, a red here proves
    # nothing -- it is the same breakage observed twice, and would be scored as
    # "the control works" while the control was never actually the cause. (This
    # is exactly the absent-vs-absent trap ab_measure.py refuses on.)
    was_green() {
        local x; for x in ${green[@]+"${green[@]}"}; do [ "$x" = "$1" ] && return 0; done
        return 1
    }
    probe() {  # label, base-target, then argv
        local label="$1" base="$2"; shift 2
        if ! was_green "$base"; then
            echo "  SKIP  $label -- $base's POSITIVE run was not green, so a red here"
            echo "        would prove nothing (same breakage twice, not the control)."
            st_skip=$((st_skip + 1)); return
        fi
        local log="$LOGDIR/native_health_selftest_${label}_$SLUG.log"
        "$@" > "$log" 2>&1
        local rc=$?
        local f; f=$(G -c '^  \[FAIL\] ' "$log"); f=${f:-0}
        if [ "$rc" -ne 0 ] && [ "$f" -gt 0 ]; then
            echo "  RED   $label -- rc=$rc, $f gate(s) failed (control WORKS)"
            st_ok=$((st_ok + 1))
        else
            echo "  GREEN $label -- rc=$rc, $f gate(s) failed -- THE CONTROL DID NOT FIRE."
            echo "        The instrument it guards is VACUOUS until this is fixed."
            st_bad=$((st_bad + 1))
        fi
        echo "        log: $log"
    }
    if [ -x "$NB/rb3-ark" ] && [ -d "$ASSETS" ] && [ -f "$ARK_REF" ]; then
        probe ark-corrupt rb3-ark "$NB/rb3-ark" "$ASSETS" "$ARK_REF" songs/gen/songs.dtb --corrupt
    else
        echo "  SKIP  ark-corrupt -- binary or assets absent"; st_skip=$((st_skip + 1))
    fi
    if [ -x "$NB/rb3-milo" ] && [ -d "$ASSETS" ]; then
        probe milo-badpath rb3-milo "$NB/rb3-milo" "$ASSETS" ui/track/gen/NO_SUCH_FILE.milo_xbox
    else
        echo "  SKIP  milo-badpath -- binary or assets absent"; st_skip=$((st_skip + 1))
    fi
    # A gate is worth nothing until something has shown it can go red, and this
    # one is newly wired, so it gets its control in the same commit. No assets
    # guard: rb3-score2 needs none. --force-divergent perturbs the M5 side only,
    # so the expected red is 2 of 3 gates with scenB-longest-streak surviving.
    if [ -x "$NB/rb3-score2" ]; then
        probe score2-divergent rb3-score2 "$NB/rb3-score2" --force-divergent
    else
        echo "  SKIP  score2-divergent -- binary absent"; st_skip=$((st_skip + 1))
    fi
    # ---- RUNTIME CLASSIFIER CONTROLS (W16-PE) ------------------------------
    # The all-targets runtime section above is only worth something if its
    # classifier is shown to go red on each failure class it claims to catch.
    # Each control runs a REAL target binary through the SAME classify_run and
    # must land in the SPECIFIC class named -- "it went red somehow" is not
    # enough, because a crash misread as e.g. `nocomplete` would hide the
    # signal. Same pair rule as above: the target's positive run must be green.
    #   crash-segv    rb3-harmony with an 16 KiB stack limit -> a real SIGSEGV
    #                 in the real process (5/5 deterministic, rc 139 through
    #                 timeout). It dies before main prints anything, so it proves
    #                 signal death is classified CRASH regardless of output; the
    #                 mid-run case was demonstrated by source injection in W16-PE
    #                 (docs/decomp/W16PE_NATIVE_RUNTIME_ALL_TARGETS_2026-10-06.md).
    #   exit-nonzero  rb3-crowd asked for a part the chart lacks -> its real
    #                 "track not found; abort." path, rc 1.
    #   nocomplete    rb3-midi --list -> a real `return 0` that runs no gate and
    #                 never prints RESULT. Without the completion-marker check
    #                 this run would read as healthy.
    ctl() {  # label base expect-kind marker gated -- cmd...
        local label="$1" base="$2" expect="$3" marker="$4" gated="$5"; shift 5
        if ! was_green "$base"; then
            echo "  SKIP  $label -- $base's POSITIVE run was not green, so a red here"
            echo "        would prove nothing (same breakage twice, not the control)."
            st_skip=$((st_skip + 1)); return
        fi
        local log="$LOGDIR/native_health_selftest_${label}_$SLUG.log"
        classify_run "$log" "$marker" "$gated" "$base" "$@"
        if [ "$CL_STATUS" = FAIL ] && [ "$CL_KIND" = "$expect" ]; then
            echo "  RED   $label -- classified $CL_KIND: $CL_WHY (control WORKS)"
            st_ok=$((st_ok + 1))
        else
            echo "  GREEN $label -- expected class '$expect', got $CL_STATUS/$CL_KIND ($CL_WHY)."
            echo "        THE CONTROL DID NOT FIRE as specified; the runtime classifier is suspect."
            st_bad=$((st_bad + 1))
        fi
        echo "        log: $log"
    }
    if [ -x "$NB/rb3-harmony" ] && [ -e "$MID_CENTERFOLD" ]; then
        ctl crash-segv rb3-harmony crash '^Done\.$' 0 -- \
            bash -c 'ulimit -s 16; exec "$@"' _ "$NB/rb3-harmony" "$MID_CENTERFOLD"
    else
        echo "  SKIP  crash-segv -- binary or chart absent"; st_skip=$((st_skip + 1))
    fi
    if [ -x "$NB/rb3-crowd" ] && [ -e "$MID_VICARIOUS" ]; then
        ctl exit-nonzero rb3-crowd exit '^=== M12 complete' 0 -- \
            "$NB/rb3-crowd" "$MID_VICARIOUS" "PART NOSUCH"
    else
        echo "  SKIP  exit-nonzero -- binary or chart absent"; st_skip=$((st_skip + 1))
    fi
    if [ -x "$NB/rb3-midi" ] && [ -d "$ASSETS" ]; then
        ctl nocomplete rb3-midi nocomplete '^RESULT: ALL GATES PASSED' 1 -- \
            "$NB/rb3-midi" "$ASSETS" --list
    else
        echo "  SKIP  nocomplete -- binary or assets absent"; st_skip=$((st_skip + 1))
    fi
    # ---- THE THREE RB3_HANDPOSE_* CONTROLS, DELTA-SCORED -------------------
    # ⛔⛔ `rc != 0 && failures > 0` IS NOT A VALID CREDIT FOR THESE, and scoring
    # them that way reported a WORKING CONTROL over a PRE-EXISTING RED for the
    # whole life of this script. Measured the first time the box had a working
    # GPU (lane N1-GPUGATES, 2026-09-10): `render-stale` was credited on rc=1
    # with 7 failures while the UNPERTURBED --hand-audit baseline was ALREADY
    # rc=1 with 5. The control was scored on breakage it did not cause.
    #
    # Two independent defects produced that, both now measured:
    #  1. The positive rb3-render run above does NOT pass --hand-audit, so it
    #     contains ZERO handpose gates. `was_green` therefore compared two
    #     DIFFERENT CONFIGURATIONS -- the absent-vs-absent trap wearing the
    #     pair-check as a costume. The pair rule was right; its subject was not.
    #  2. The DEFAULT cell set cannot host this control at all:
    #     ui/track/gen/tracksystem_meshes is 130 STATIC meshes with no skeleton,
    #     so four handpose gates are structurally vacuous there (4 of those 5
    #     baseline failures). Only a cell with a real figure can be a baseline.
    #
    # So: baseline on a cell that HAS a figure, and credit a control ONLY for
    # failures it ADDS OVER that baseline. Measured on crowd_female01:
    #     PERTURB +0  re-composes by construction -- INERT BY DESIGN
    #     PUBLISH +1  the bone LEAVES the COMPOSED population (6 -> 5)
    #     STALE   +2  stale-but-COMPOSED; worst dev == the injected 1.0 on
    #                 bone_L-hand.mesh, which is X18's entire reason to exist
    HP_CELL="char/crowd/gen/crowd_female01.milo_xbox"
    if [ -x "$NB/rb3-render" ] && [ -d "$ASSETS" ] \
       && ! printf '%s\n' "${unrunnable[@]+"${unrunnable[@]}"}" | G -q 'rb3-render:nogpu'; then
        hp_blog="$LOGDIR/native_health_hp_baseline_$SLUG.log"
        "$NB/rb3-render" "$ASSETS" "$LOGDIR/native_health_hp_baseline_out_$SLUG" \
            "$HP_CELL" --hand-audit > "$hp_blog" 2>&1
        hp_base=$(G -c '^  \[FAIL\] ' "$hp_blog"); hp_base=${hp_base:-0}
        hp_bp=$(G -c '^  \[PASS\] ' "$hp_blog"); hp_bp=${hp_bp:-0}
        if [ "$hp_bp" -eq 0 ]; then
            # Same anti-vacuity rule as run_target: a run that emitted no verdict
            # measured NOTHING, and every delta taken against it is meaningless.
            echo "  SKIP  handpose controls -- the --hand-audit baseline emitted NO gate"
            echo "        lines; it measured nothing, so no delta against it can mean anything."
            st_skip=$((st_skip + 3)); hp_tot=3
        else
            echo "  handpose baseline ($HP_CELL): $hp_bp pass / $hp_base KNOWN-RED"
            G '^  \[FAIL\] ' "$hp_blog" | cut -c1-96 | sed 's/^/          known-red: /'
            hp_probe() {   # label, ENV VAR, expectation: RED | INERT
                local label="$1" var="$2" expect="$3"
                local log="$LOGDIR/native_health_hp_${label}_$SLUG.log"
                # ⚠ argv written out in full. main_render.cpp:4965's loop ends in a
                # bare `else pos.push_back(argv[i])`, so a misspelled flag becomes a
                # POSITIONAL arkPath and the renderer draws the WRONG THING at rc=0.
                env "$var=1.0" "$NB/rb3-render" "$ASSETS" \
                    "$LOGDIR/native_health_hp_${label}_out_$SLUG" "$HP_CELL" \
                    --hand-audit > "$log" 2>&1
                local f d act
                f=$(G -c '^  \[FAIL\] ' "$log"); f=${f:-0}; d=$((f - hp_base))
                act=$(G -c "$var=.* ACTIVE" "$log"); act=${act:-0}
                hp_tot=$((hp_tot + 1))
                if [ "$expect" = "RED" ]; then
                    if [ "$d" -gt 0 ]; then
                        echo "  RED   $label ($var) -- +$d gate(s) OVER BASELINE (control WORKS)"
                        hp_ok=$((hp_ok + 1)); st_ok=$((st_ok + 1))
                    else
                        echo "  GREEN $label ($var) -- delta $d over baseline: THE CONTROL DID NOT FIRE."
                        echo "        The gate it guards is VACUOUS until this is fixed."
                        st_bad=$((st_bad + 1))
                    fi
                else
                    # INERT BY DESIGN -- and "nothing happened" is ALSO what a
                    # silently-ignored env var looks like, so delta==0 alone would
                    # pass for a DELETED feature. The activation line is the
                    # witness that the perturbation was actually APPLIED.
                    if [ "$act" -gt 0 ] && [ "$d" -eq 0 ]; then
                        echo "  INERT $label ($var) -- APPLIED (activation line present) and delta 0:"
                        echo "        the DOCUMENTED prediction (main_render.cpp:2821-2833). SetLocalXfm"
                        echo "        re-composes, so the tag stays COMPOSED. A control on the"
                        echo "        MEASUREMENT, not on the verdict."
                        hp_ok=$((hp_ok + 1)); st_ok=$((st_ok + 1))
                    elif [ "$act" -eq 0 ]; then
                        echo "  GREEN $label ($var) -- NO activation line: the env var never reached the"
                        echo "        code. VACUOUS -- this arm would pass for a DELETED feature."
                        st_bad=$((st_bad + 1))
                    else
                        echo "  GREEN $label ($var) -- delta $d, but the documented prediction is 0."
                        echo "        The mechanism has CHANGED; re-derive it before trusting either."
                        st_bad=$((st_bad + 1))
                    fi
                fi
                echo "        log: $log"
            }
            hp_probe render-perturb RB3_HANDPOSE_PERTURB INERT
            hp_probe render-publish RB3_HANDPOSE_PUBLISH RED
            hp_probe render-stale   RB3_HANDPOSE_STALE   RED
        fi
    else
        echo "  SKIP  handpose controls (PERTURB/PUBLISH/STALE) -- rb3-render cannot"
        echo "        reach its pose gates here. NOT a pass: these are UNDEMONSTRATED."
        st_skip=$((st_skip + 3)); hp_tot=3
    fi
    hp_ctl="$hp_ok/$hp_tot"
    if   [ $st_bad -gt 0 ]; then selftest="FAIL"
    elif [ $st_ok -eq 0 ];  then selftest="VACUOUS"   # nothing was demonstrated
    elif [ $st_skip -gt 0 ]; then selftest="PARTIAL"
    else                         selftest="PASS"; fi
    echo "  selftest: $selftest ($st_ok red, $st_bad stuck-green, $st_skip skipped)"
fi

# --------------------------------------------------------------- SCATTER ----
echo
echo "--- scatter audit (tools/scatter_audit.py) ---"
sc_unlinked="-"; sc_b="-"; sc_multi="-"
SJ="$LOGDIR/native_health_scatter_$SLUG.json"
if python3 "$DIR/tools/scatter_audit.py" > "$SJ" 2>"$SJ.err"; then
    read -r sc_unlinked sc_b sc_multi < <(python3 - "$SJ" <<'PY'
import json, sys, collections
r = json.load(open(sys.argv[1]))
T = set(r["targets"])
c = collections.defaultdict(set)
for row in r["direction_a"]:
    c[row["file"]].add(row["target"])
# "reaches NO native target at all" = flagged in DIRECTION A for EVERY target.
# This is the strictly-weaker decidable predictor x10 reported; the raw
# direction_a row count (~2100) is per-target and is NOT the headline.
unlinked = [f for f, ts in c.items() if ts >= T]
print(len(unlinked), len(r["direction_b"]), len(r["multi_host"]))
PY
)
    echo "  unlinked scatter guests (reach NO target): $sc_unlinked"
    echo "  direction B (excluded-but-emitted):        $sc_b"
    echo "  multi-host guests:                         $sc_multi"
    echo "  json: $SJ"
else
    echo "  scatter_audit.py FAILED -- $(tail -1 "$SJ.err" 2>/dev/null)"
fi

# --------------------------------------------------------------- VERDICT ----
echo
runtime_verdict="PASS"
[ ${#unrunnable[@]} -gt 0 ] && runtime_verdict="INCOMPLETE"
[ "$gates_fail" -ne 0 ] && runtime_verdict="FAIL"
[ ${#rt_failed[@]} -gt 0 ] && runtime_verdict="FAIL"   # crash / hang / exit / nocomplete
[ "$rt_ran" -eq 0 ] && runtime_verdict="UNRUNNABLE"

rc=0; verdict="PASS"
if [ "$link_verdict" = "FAIL" ] || [ "$runtime_verdict" = "FAIL" ] \
   || [ "$selftest" = "FAIL" ] || [ "$selftest" = "VACUOUS" ]; then
    verdict="FAIL"; rc=1
elif [ $SKIP_LINK -eq 1 ] || [ "$link_verdict" = "INCOMPLETE" ] \
     || [ "$link_verdict" = "PARTIAL" ] || [ "$runtime_verdict" != "PASS" ] \
     || [ "$selftest" = "PARTIAL" ]; then
    verdict="INCOMPLETE"; rc=3
fi

case "$verdict" in
FAIL)
    echo "NATIVE HEALTH: FAIL -- something is BROKEN (rc=1)"
    for u in ${rt_failed[@]+"${rt_failed[@]}"}; do echo "    - runtime FAILED: $u"; done ;;
INCOMPLETE)
    echo "NATIVE HEALTH: INCOMPLETE -- ran, but does NOT vouch for full coverage (rc=3)"
    echo "  NOTHING IS KNOWN TO BE BROKEN; something was NOT TESTED:"
    [ $SKIP_LINK -eq 1 ] && echo "    - link gate skipped (--skip-link)"
    for u in ${unrunnable[@]+"${unrunnable[@]}"}; do echo "    - runtime UNRUNNABLE: $u"; done
    [ "$selftest" = "PARTIAL" ] && echo "    - a negative control could not be demonstrated" ;;
PASS)
    echo "NATIVE HEALTH: PASS  (rc=0, link $link_ver/$link_exp, $gates_pass runtime gate(s), selftest=$selftest)" ;;
esac

emit "$verdict" "$link_verdict" "$link_ver" "$link_exp" "$link_skip" \
     "$runtime_verdict" "$rt_ran" "$rt_total" "$gates_pass" "$gates_fail" \
     "$(if [ ${#unrunnable[@]} -gt 0 ]; then IFS=,; echo "${unrunnable[*]}"; else echo none; fi)" \
     "$selftest" "$sc_unlinked" "$sc_b" "$sc_multi" "$rc" "$hp_ctl" "$hp_base" \
     "$rt_crashed" \
     "$(if [ ${#rt_failed[@]} -gt 0 ]; then IFS=,; echo "${rt_failed[*]}"; else echo none; fi)"
exit $rc
