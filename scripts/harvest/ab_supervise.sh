#!/bin/bash
# Supervised composed whole-binary A/B in a worktree.
#   Usage: setsid nohup scripts/harvest/ab_supervise.sh <worktree> <log> & disown
# Why a supervisor: external pkill sweeps kill long builds mid-flight (silent
# log stop, no OOM). ninja resumes incrementally, so a retry loop converges.
# Monitor by MARKER (grep "fresh_report.sh: done" / "SUPERVISOR: complete"),
# never by PID — pgrep -f on the worktree path self-matches the monitor shell.
WT="$1"; LOG="$2"
cd "$WT" || exit 1
rm -f build/45410914/target_symbol_renames.stamp
touch config/45410914/config.yml
for attempt in 1 2 3 4 5 6; do
    echo "=== SUPERVISOR attempt $attempt $(date '+%H:%M:%S') ===" >> "$LOG"
    NINJA_JOBS=6 tools/fresh_report.sh >> "$LOG" 2>&1
    rc=$?
    if [ $rc -eq 0 ] && tail -20 "$LOG" | grep -q "fresh_report.sh: done"; then
        echo "SUPERVISOR: complete after attempt $attempt" >> "$LOG"
        exit 0
    fi
    echo "SUPERVISOR: attempt $attempt exited rc=$rc, retrying in 5s" >> "$LOG"
    sleep 5
done
echo "SUPERVISOR: FAILED after 6 attempts" >> "$LOG"
exit 1
