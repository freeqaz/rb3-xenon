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
#   1. RUNS the binaries and counts their gate verdicts.
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
#
# EXIT CODES -- deliberately the SAME vocabulary as native_build_gate.sh, so the
# two can be read by one reader:
#     0  everything asked for was measured, and all of it is healthy.
#     1  something is BROKEN: the link gate failed, a runtime gate failed, or
#        (under --selftest) a negative control did NOT go red.
#     2  the script COULD NOT RUN AT ALL (bad option, no such dir, no native/).
#     3  it RAN and does NOT VOUCH FOR FULL COVERAGE -- a runtime instrument was
#        UNRUNNABLE for a verified environmental reason (no GPU, absent assets),
#        or --skip-link was passed. NOTHING IS KNOWN TO BE BROKEN; something was
#        NOT TESTED. Absence of evidence, and a distinct fact from both.
#
# THE SUMMARY LINE
#     NATIVE_HEALTH_RESULT verdict=... link=... runtime=... selftest=... rc=...
#     Keys, order and spelling are the contract; add fields at the END only.

set -uo pipefail

emit() {  # verdict link link_ver link_exp link_skip runtime rt_ran rt_tot
          # gates_pass gates_fail unrunnable selftest sc_unlinked sc_b sc_multi rc
    echo "NATIVE_HEALTH_RESULT verdict=$1 link=$2 link_verified=$3 link_expected=$4" \
         "link_skipped=$5 runtime=$6 runtime_ran=$7 runtime_total=$8" \
         "gates_pass=$9 gates_fail=${10} unrunnable=${11} selftest=${12}" \
         "scatter_unlinked=${13} scatter_dirb=${14} scatter_multihost=${15} rc=${16}" \
         "handpose_controls=${17:--} handpose_baseline_fail=${18:--}"
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
# Each entry: [--no-assets] <target> <argv...>. Only the targets that emit the
# house `  [PASS]/[FAIL]` contract AND propagate the verdict to their exit code;
# the remaining 14 have ad-hoc printing and no common contract.
#
# ⚠ This comment used to end with "-- main_score2 in particular prints
# MATCH/DIVERGENT and returns 0 UNCONDITIONALLY, so it cannot be read as a gate".
# That was true when N1-GPUGATES wrote it and STALE from 2026-09-10, when
# L4-NATIVESCATTER gave main_score2.cpp the contract and a real --force-divergent
# flag. It is wired in below. Re-verified here before wiring, rather than trusted:
#   (none)              rc=0, 3 [PASS], 0 [FAIL]
#   --force-divergent   rc=1, 1 [PASS], 2 [FAIL]  (scenB-longest-streak still
#                       passes => targeted, not a blanket red)
#   --forcedivergent    rc=2  (unknown argument REFUSED, not silently ignored)
# rb3-score2 is pure arithmetic against the M5 score_engine transcription: it
# needs neither assets nor a GPU, which is why it is the one runtime instrument
# that can run on a box without the ~4 GB ark -- hence --no-assets, so the assets
# gate does not report it UNRUNNABLE (and drag the verdict to INCOMPLETE) for a
# dependency it does not have.
echo
echo "--- runtime gates ---"
NB="$DIR/native/build"
gates_pass=0; gates_fail=0; rt_ran=0; rt_total=0; unrunnable=(); green=()

# Count gate verdict lines, and REFUSE to call a zero-gate run a pass.
# A binary that prints nothing has not passed; it has not been measured.
run_target() {  # [--no-assets] name, then argv
    local need_assets=1
    if [ "${1:-}" = "--no-assets" ]; then need_assets=0; shift; fi
    local name="$1"; shift
    rt_total=$((rt_total + 1))
    if [ ! -x "$NB/$name" ]; then
        echo "  UNRUNNABLE $name -- no executable (the link gate above is the authority on why)"
        unrunnable+=("$name:nobinary"); return
    fi
    if [ "$need_assets" -eq 1 ] && [ ! -d "$ASSETS" ]; then
        echo "  UNRUNNABLE $name -- assets absent at $ASSETS (set RB3_ASSETS)"
        unrunnable+=("$name:noassets"); return
    fi
    local log="$LOGDIR/native_health_${name}_$SLUG.log"
    # rc captured DIRECTLY. Never `prog | tail; echo $?` -- that reports TAIL's rc.
    "$NB/$name" "$@" > "$log" 2>&1
    local rc=$?
    local p f; p=$(G -c '^  \[PASS\] ' "$log"); f=$(G -c '^  \[FAIL\] ' "$log")
    p=${p:-0}; f=${f:-0}
    if [ "$rc" -eq 2 ] && [ "$name" = "rb3-render" ]; then
        # main_render.cpp:5131 returns 2 for "NO GPU" specifically, BEFORE the
        # load/pose/draw/png gates. That is the environment, not the code, and
        # conflating it with rc=1 would report a broken build on a driverless box.
        echo "  UNRUNNABLE $name -- rc=2 NO GPU; reached only $((p + f)) of ~27 gates"
        echo "             $(G '\[FAIL\]' "$log" | head -1)"
        unrunnable+=("$name:nogpu"); return
    fi
    if [ $((p + f)) -eq 0 ]; then
        # Anti-vacuity: a run that emits no verdict has measured NOTHING.
        echo "  UNRUNNABLE $name -- rc=$rc but ZERO gate lines; it measured nothing"
        unrunnable+=("$name:novgates"); return
    fi
    rt_ran=$((rt_ran + 1))
    gates_pass=$((gates_pass + p)); gates_fail=$((gates_fail + f))
    if [ "$rc" -ne 0 ] || [ "$f" -ne 0 ]; then
        echo "  FAIL       $name -- rc=$rc, $p passed, $f FAILED"
        G '^  \[FAIL\] ' "$log" | head -5 | sed 's/^/            /'
    else
        echo "  OK         $name -- rc=0, $p gate(s) passed"
        green+=("$name")
    fi
    echo "             log: $log"
}

run_target rb3-milo "$ASSETS" ui/track/gen/tracksystem_meshes.milo_xbox
run_target rb3-ark  "$ASSETS" "$ARK_REF"
run_target rb3-render "$ASSETS" "$LOGDIR/native_health_render_out_$SLUG"
run_target --no-assets rb3-score2

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
    echo "NATIVE HEALTH: FAIL -- something is BROKEN (rc=1)" ;;
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
     "$selftest" "$sc_unlinked" "$sc_b" "$sc_multi" "$rc" "$hp_ctl" "$hp_base"
exit $rc
