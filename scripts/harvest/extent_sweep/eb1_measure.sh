#!/bin/bash
# lane EB-1: the map-patch measurement protocol, by hand.
#
# ab_measure.py REFUSES any patch touching symbols.txt (it treats symbols.txt as
# derived split drift that must never be part of a measured change), so a carve
# cannot go through it.  This reproduces the parts that matter:
#
#   * BOTH legs pay an identical FORCED RE-SPLIT (rm the renamer stamp + touch
#     config.yml).  A map edit without a re-split is INERT, and -- worse -- the
#     build dir keeps the previously renamed target objs, so a baseline leg
#     silently reads the treatment.  Forcing it on leg A too is what stops that.
#   * report.json AND report.cache are wiped before the read (a stale cache
#     inflates).
#   * the DEFAULT target is settled to zero real edges FIRST, and then the report
#     build must itself do no compiles -- the graph has been measured producing a
#     second wave of dirtiness after a build finishes, and an unsettled read is
#     WRONG, not merely noisy.
#
# usage: eb1_measure.sh <snapshot-out.json> <log-tag>
set -uo pipefail
WT=/home/free/tmp/laneEB1/wt
OUT="$1"; TAG="$2"
LOG=/home/free/tmp/laneEB1/eb1_build_${TAG}.log
: > "$LOG"

cd "$WT" || exit 1
rm -f build/45410914/target_symbol_renames.stamp
touch config/45410914/config.yml

edges() { grep -E '^\[[0-9]+/[0-9]+\]' "$1" | grep -vcE '\] PROGRESS'; }

ok=0
for i in 1 2 3 4 5 6; do
  P=/home/free/tmp/laneEB1/eb1_pass_${TAG}_$i.log
  ./tools/ninja-locked > "$P" 2>&1 || { echo "BUILD FAILED pass $i"; tail -30 "$P"; exit 1; }
  cat "$P" >> "$LOG"
  d=$(edges "$P")
  R=/home/free/tmp/laneEB1/eb1_rep_${TAG}_$i.log
  rm -f build/45410914/report.json build/45410914/report.cache
  ./tools/ninja-locked build/45410914/report.json > "$R" 2>&1 || { echo "REPORT FAILED pass $i"; tail -30 "$R"; exit 1; }
  cat "$R" >> "$LOG"
  # the report edge itself is expected; anything ELSE means the graph was dirty
  r=$(grep -E '^\[[0-9]+/[0-9]+\]' "$R" | grep -vcE '\] (PROGRESS|REPORT)')
  echo "  settle pass $i: default_real_edges=$d  report_extra_edges=$r"
  if [ "$d" = "0" ] && [ "$r" = "0" ]; then ok=1; break; fi
done
[ "$ok" = "1" ] || { echo "REFUSED: could not reach a quiescent build in 6 passes"; exit 2; }

echo -n "  SPLIT ran: "; grep -cE '^\[[0-9]+/[0-9]+\].* SPLIT' "$LOG"
echo -n "  renamer patched: "; grep -aoE 'patched [0-9]+ (file|obj)' "$LOG" | tail -1
python3 /home/free/tmp/laneEB1/eb1_snap.py "$OUT"
