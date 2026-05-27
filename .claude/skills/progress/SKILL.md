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
