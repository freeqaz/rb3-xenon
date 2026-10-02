# W16-NG — `ab_measure` measures a splits.txt heading rename (2026-10-02)

Branch `w16-ng` on main `17eef09f3`. Lane brief: `tools/ab_measure.py` refused every patch that renames a
`splits.txt` heading, and W16-NE had to measure with `RB3_ALLOW_UNRESOLVED_SPLITS=1`. Make such a patch
measure correctly without that override, keep refusing headings that really are unresolved, and prove both
with controls.

## 1. The defect

`apply_patch()` ran `python3 configure.py` by hand right after `git apply`, **before** the forced re-split.
`configure.py` builds its unit list from `build/45410914/config.json`, which is the *previous* split's output.
So it compared leg A's heading names against leg B's patched `objects.json`, and every renamed heading came
out as "no objects.json entry".

ninja already does this in the right order. Its generator edge is

```
build build.ninja objdiff.json: configure | build/45410914/config.json configure.py tools/project.py ...
```

That edge depends on the split's **output**, so a plain `ninja` after the same edit splits first and
configures second. The hand run was the only thing that reversed the order.

A second, smaller problem with the hand run: it dropped `$configure_args` (`--dtk/--objdiff/--wrapper`). In a
`~/tmp` worktree, a bare `configure.py` re-resolves the split and report commands through the `~/tmp/jeff` and
`~/tmp/objdiff` symlinks. Measured: 14 changed manifest lines, all tool command strings, and `objdiff.json`
byte-identical. The binaries are the same (identical sha256 `f0937c88…`), so no measurement was wrong because
of this, but leg B's manifest did not match leg A's.

## 2. The fix (`tools/ab_measure.py`)

- `apply_patch()` no longer runs `configure.py`. For a configgen patch it bumps the mtime of `configure.py`,
  which is a static input of the generator edge, so leg B's first ninja build regenerates the manifest in
  dependency order. The bump also covers configgen paths that are *not* static inputs of that edge
  (`tools/defines_common.py`, `tools/source_category.py`, `tools/scope_map.py`).
- New leg-B assertion: a configgen patch whose first leg-B build shows no `RUN configure.py` step is refused.
  Without that step, leg B would measure leg A's manifest.
- The check is still strict. If a heading is still unresolved after the split, ninja's configure step fails,
  and `_ninja()` turns that failure into a refusal at stage **`configure`** that names each heading.
- `RB3_ALLOW_UNRESOLVED_SPLITS` is removed from the environment of every build the tool runs, so a value set
  by the caller cannot let a unit that can never pair be priced.

Selftest: **102 PASS / 0 FAIL**, 8 new checks. Each new gate was shown to fail on a sabotaged copy:
- putting the hand run back fails the shape guard;
- deleting the leg-B gate fails both configgen-refusal checks.

## 3. Controls (whole binary, `name_check` ruler)

All runs: worktree off `w16-ng` at `666f1785d` (the fix), objdiff-cli `sha256:c1b7d95240a35cd6`, default
`name_check` report ruler. The controls reuse W16-NE's own change, because it has a published answer.

**Before the fix** (unmodified tool, `--revert 17eef09f3`): **REFUSED**, stage `subprocess`, with
`configure.py` rc=1 on 4 headings (`system/bandobj/MeterDisplay.cpp`, `system/rnddx9/Mat.cpp`,
`network/net/NetMessage.cpp`, `system/bandobj/MiniLeaderboardDisplay.cpp`). This reproduces W16-NE's first
attempt, in the reverse direction.

### Positive control: a known heading rename, measured to the published answer

`--revert 17eef09f3` reverts the W16-NE merge. Its first parent is `c8143e399`, the base W16-NE measured
against, so this run is the exact inverse of W16-NE's A/B. Before running it I predicted the exact negation of
W16-NE's five deltas.

| measure | leg A | leg B | Δ measured | W16-NE published Δ |
|---|---:|---:|---:|---:|
| matched_functions | 50,861 | 50,727 | **−134** | +134 |
| honest | 26,332 | 26,242 | **−90** | +90 |
| matched_code Δ | | | **−16,748 B** | +16,748 B |
| matched_code_percent | 53.768776 | 53.605335 | **−0.163441 pp** | +0.163441 pp |
| fuzzy_match_percent | 59.972584 | 59.879948 | **−0.092636 pp** | +0.092636 pp |

- **rc 0, status `measured`.** All five deltas are the exact negation, and both legs match W16-NE's legs to
  the last digit.
- **Order of leg B's first build:** `[1/2] SPLIT` → `[2/2] RUN configure.py`. First-iteration counts were
  `split=1 configure=1 msvc=63 renamer_patched=1853`.
- **The renamed units pair under their new names:**
  - `default/MeterDisplay` 0→70 and `default/system/bandobj/MeterDisplay` 71→0;
  - `default/Mat` 0→68 and `default/system/rndobj/Mat` 68→0.
- Both legs reached a split fixed point after 0 extra re-splits, and the worktree was restored clean.

### Negative control: a genuinely unresolved heading still refuses

This is the same revert patch with **one line changed**: the heading `+system/hamobj/MiniLeaderboardDisplay.cpp:`
becomes `…MiniLeaderboardDisplayTYPO.cpp:`. It therefore carries the 4 renames the old tool refused, plus one
heading that no `objects.json` entry can match. Prediction: refused at stage `configure`, naming only the
typo.

- **REFUSED, rc 2, stage `configure`.** Exactly one heading is named:
  `system/hamobj/MiniLeaderboardDisplayTYPO.cpp (no objects.json entry)`. The 4 legitimate renames resolved.
- **Leg B log order:** `[1/2] SPLIT` → `[2/2] RUN configure.py` → `ERROR: 1 splits.txt heading(s) …` →
  `ninja: error: rebuilding 'build.ninja': subcommand failed`.
- **The same run again with `RB3_ALLOW_UNRESOLVED_SPLITS=1` exported by the caller** (W16-NE's workaround):
  an identical refusal. The tool printed that it scrubbed the variable.
- In both runs, no deltas were reported and the tree was restored clean.

So the fix moves the configure step to after the split; it does not relax the check. The only heading that now
resolves is one the patched `splits.txt` *and* the patched `objects.json` agree on.

## 4. Not done

- I did not change `tools/project.py`. Its hard fail is correct; the fault was the order in which it was
  called.
- I did not change how `TreeGuard` restores the tree. After a heading-rename run, the restored worktree's
  next plain `ninja` re-splits and then re-configures in the same dependency order, so it heals itself the same
  way.
