# scripts/harvest — coordinator wave harvest/land tooling

Helpers the **coordinator** uses to land multi-agent wave results onto `main`.
(Previously lived in `/tmp`; moved here so they survive across sessions.)

Each wave lane runs in its own git worktree and produces a one-commit branch
`w<N>-<tu>`. Independent lanes all touch the same three *union* files, so a
plain rebase conflicts spuriously. These scripts resolve those unions
deterministically.

| Script | Role |
|---|---|
| `land.sh <worktree\|branch>` | Rebase one lane branch onto `main`, auto-resolving union files. Prints `READY:<branch>` (then `git merge --ff-only`) or `DEFER:<branch> <reason>`. |
| `resolve_json_union.py <wt> <relpath>` | Dict-union of git stage-2+stage-3 for `scripts/target_symbol_map.json` / `config/45410914/objects.json` (ours-first, then new-from-theirs; order preserved). |
| `resolve_splits_union.py <wt>` | Line-union of `config/45410914/splits.txt` (ours + lines theirs added vs base). |

## Land sequence (per wave — see the full SOP)

`docs/decomp/handoff/wave-loop-SOP-2026-06-20.md` is authoritative. In short:

1. For each cleared+honest lane: `scripts/harvest/land.sh <worktree>` → on
   `READY`, `git merge --ff-only <branch>` into main.
2. **Splits overlap self-check BEFORE building** (two independent lanes can pin
   overlapping ranges — the union scripts do NOT catch that):
   ```python
   import re; t=open('config/45410914/splits.txt').read()
   for k in ['pdata','text']:
     rs=sorted([(int(a,16),int(b,16)) for a,b in re.findall(rf'\.{k}\s+start:(0x[0-9A-Fa-f]+)\s+end:(0x[0-9A-Fa-f]+)',t)])
     print(k, sum(1 for i in range(1,len(rs)) if rs[i][0]<rs[i-1][1]),'overlaps')
   ```
3. **Composed verify** (only truth): `rm -f build/45410914/target_symbol_renames.stamp
   && touch config/45410914/config.yml && NINJA_JOBS=12 tools/fresh_report.sh`
   (re-run once; the splits-only divergence WARN is a known FP). Read
   `measures.matched_functions`.
