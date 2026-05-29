# Engine near-misses are STRUCT-OFFSET bugs, not permuter-class (2026-05-29)

**Status:** active sweep. **Key finding that reframes the engine fuzzy-gap queue.**

## The finding (verified, decisive)

The engine "near-miss" functions (80–99.99% normalized, the
`scripts/permuter_targets.py` queue) are **NOT permuter-class**. Classifying the
top 60 by their actual objdiff `instructions[].match_type` diffs:

- **82% (49/60) are pure struct-field-offset mismatches** — a single
  `addi/lwz/stw/lfs/...` instruction with a different *immediate offset* than the
  target (e.g. target `addi r3,r3,0x38` vs ours `0x3c`). This is a **C++ header
  struct-layout bug**, fixable by resizing/reordering a member — and it
  **cascades** (one base/sub-struct size error shifts every later field access).
- A few are `imm_other` (constants) or `offset_imm+register` (offset + regswap).
- **0/30 of the highest band (99.5–100%) had any source-codegen diff** the
  permuter could touch. The permuter correctly reports "all mismatches are noise"
  and improves nothing. **This supersedes `[[feedback-fuzzy-gap-needs-permuter]]`
  for the engine high band** — the right tool is header-layout fixes, not the
  permuter. (The permuter is still right for genuine regswap/scheduling residuals,
  a small minority.)

### Why the permuter can't help & why this is matchable
`matched_functions` counts **normalized** (reloc-name-insensitive) 100%. A
function at 99.94% normalized with `0` instruction `match_type!=equal` under
`functionRelocDiffs=none` but a lone `diff_arg` on an `addi` immediate = a struct
field at the wrong byte offset. No source permutation changes a struct's layout;
only the header does. There is **NO Ghidra type info** (stripped binary) — diagnose
from the offset deltas + the function's logic + the header.

## The diagnosis/fix loop (per function, run in an isolated worktree)

```
# 1. near-misses in a unit:
python3 -c "import json;d=json.load(open('build/45410914/report.json'));u=next(x for x in d['units'] if x['name']=='default/<UNIT>');[print(round(f.get('match_percent_normalized') or 0,3),f['name'],(f.get('metadata') or {}).get('demangled_name','')) for f in sorted(u['functions'],key=lambda f:-(f.get('match_percent_normalized') or 0)) if 80<=(f.get('match_percent_normalized') or 0)<100]"
# 2. a function's diffs (look for match_type != "equal"; target vs base offsets):
bin/objdiff-cli diff -p . -u default/<UNIT> '<MANGLED>' -c functionRelocDiffs=none -f json --include-instructions
# 3. map offset -> struct member (read the .cpp logic + header); find the +/-N byte bug
# 4. fix header (prefer LEAF class; flag shared-base for review)
# 5. ./tools/ninja-locked 2>&1 | tee /tmp/build.log | tail -20
# 6. re-diff: normalized_match_percent == 100 ? flipped.
# 7. NO-REGRESSION: measures.matched_functions after >= before, else git checkout the header.
```

## Delta distribution (top 120 near-misses, target − base offset)

`+4` (120), `−4` (85), `−8` (76), `−12` (29), `−16` (27), `+16` (21), `+8` (19),
`−40` (13) … The deltas cluster by unit (each ≈ one struct's bug). Mixed signs
(+4 AND −4) prove it is **per-struct, not one universal Object-base error**.

## Wave plan (worktree-isolated, Sonnet agents; orchestrator lands verified wins)

**Wave 1 (launched 2026-05-29, branches `sweep-smoke/2/3/4/5`):**
CharClip (−4 mFramesPerSec cluster), EventTrigger (+8 Anim/ProxyCall nested
structs), Geo (geometry math structs), Instance+CharBoneDir, LightHue+LightPreset+
Env_NG.

**Wave 2 (ready, units NOT in wave 1, ranked by offset-clean near-misses):**
Rnd_Xbox (5), Rnd (4), UIScreen (3), ContentMgr (3), MatAnim (3), Part (2),
system/rndobj/CubeTex (2), CacheMgr_Xbox (2).

## Guardrails (critical — struct changes cascade)
- Each agent works in its **own** `scripts/setup_worktree.sh` worktree (private
  build dir). Never touch main or other worktrees. Commit wins to the worktree
  branch; the orchestrator reviews + lands to main.
- **Verify global no-regression** (`measures.matched_functions` before/after) on
  EVERY rebuild — a struct change can break other functions.
- **Prefer leaf-class headers.** Shared-base fixes (Hmx::Object/Object.h, CharBones,
  ObjectDir, RndDrawable/RndTransformable, math Vector/Transform/Matrix/Color) have
  huge blast radius (100s of TUs + the native build) — do NOT land autonomously;
  flag for an orchestrator-coordinated cascade fix. If a single shared-base fix is
  correct it could flip MANY near-misses at once (high upside, high risk).
- Don't edit objects.json / splits.txt / symbols.txt (that's pinning, a separate
  lane).

## Refs
- `scripts/permuter_targets.py rank` — the fuzzy-gap queue (but see finding above).
- `tools/game_oracle_triage.py` — the game-code lane (separate; gated on pairing +
  the now-confirmed-UNMATCHABLE instrumentation, see
  `[[project-game-code-instrumentation]]`).
- Memories: `[[project-rb3-xenon-roadmap]]`, `[[feedback-fuzzy-gap-needs-permuter]]`
  (this finding refines it), `[[project-shared-index-commit-race]]`.
