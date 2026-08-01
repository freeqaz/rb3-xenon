---
name: ab-measure
description: Measure a change's whole-binary A/B delta SAFELY. Runs the entire protocol (settle-to-zero, report cache wipes, strict key reads, forced re-split for map/splits changes, absent-vs-absent detection) and REFUSES on broken runs instead of reporting noise. Use for ANY "did my change help the metric?" question — never hand-run the checklist.
argument-hint: "[--from-dirty | --patch FILE | --pick REF | --revert REF]"
---

# ab-measure — whole-binary A/B, safe by default

Wraps `tools/ab_measure.py`. The tool IS the protocol; it cannot print an
absolute it did not measure this run, and it exits 2 with **no verdict** when
any precondition fails.

## Steps

1. **Be in a buildable linked worktree** (never main — the tool refuses main):
   ```bash
   scripts/setup_worktree.sh ~/tmp/<lane>/wt <branch>
   ```

2. **Run the measurement.** Most common: you edited source in the worktree and
   want to price exactly those edits:
   ```bash
   python3 tools/ab_measure.py --worktree <wt> --from-dirty
   ```
   Other modes:
   ```bash
   python3 tools/ab_measure.py --worktree <wt> --patch change.diff   # a diff file
   python3 tools/ab_measure.py --worktree <wt> --pick <ref>          # apply a commit
   python3 tools/ab_measure.py --worktree <wt> --revert <ref>        # revert a commit
   ```
   Options: `--restore` (revert the patch after measuring), `--name-check`
   (opt-in second ruler; small nc deltas mean nothing, ~0.05pp noise floor),
   `--jobs N` / `AB_NINJA_JOBS` (default 12).

3. **Read the verdict.**
   - Exit 0: quote Δmatched / Δmasked_equal / Δhonest / Δcode% and BOTH leg
     absolutes from the tool output (they were measured in this run).
     `result.json` in the printed run dir has the full evidence trail
     (recompile counts, renamer patched-count, per-unit regressions).
   - Exit 2 (REFUSED): **do not report any number from the run.** Fix the
     stated precondition and rerun. Common refusals: worktree dirty (commit or
     use `--from-dirty`), cannot settle (check for concurrent builds / weird
     mtimes), source patch with 0 leg-B recompiles (the file isn't compiled —
     absent-vs-absent), map patch with `0 files patched` (inert edit).

## Never do these manually anymore

The tool already does them, in the right order, with assertions: settle
builds, `rm -f build/45410914/report.{json,cache}`, `git checkout --
config/45410914/symbols.txt`, `touch config/45410914/config.yml` for map/splits
changes, recompile counting before any report read. Hand-running the checklist
is how six different footguns fired in one session — see CLAUDE.md
"Whole-binary A/B measurement".
