---
name: progress
description: Get decomp progress summary. Shows total/complete/at_limit counts, percentages, detected patterns, and top units with remaining work.
argument-hint: ""
allowed-tools: Bash(python3 scripts/get_progress.py)
---

# Progress Skill

Show overall decomp progress statistics.

## Steps

1. **Run the progress script:**
   ```bash
   python3 scripts/get_progress.py
   ```

2. **Present the results** — the script outputs a formatted markdown report with:
   - Overall function counts (total, complete, at_limit, remaining)
   - Detected pattern breakdown (merged, regswap, etc.)
   - Top 15 units with the most remaining work

⚠ This skill DISPLAYS state; it does not price a change. To measure whether a
specific change helped (Δmatched/Δhonest/Δcode%), never diff two progress
readings by hand — use `/ab-measure` (`tools/ab_measure.py`), which runs the
full A/B protocol and refuses broken runs.
