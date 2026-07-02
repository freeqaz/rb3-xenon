# Reconcile duplicate TambourineManager.cpp port — 2026-07-02

## Collision
Two independent efforts ported `band3/game/TambourineManager.cpp` and both landed on `main`:
- **Owner** `f99d7bb` — ported TU, wired it, carved it out of `DepthBuffer3D.cpp`'s
  splits container, pinned 5 fns. BUG: appended DepthBuffer3D carve fragments WITHOUT
  shrinking/removing the original `DepthBuffer3D` span lines → 3 pdata + 3 text self-overlaps.
  Also emitted the new unit under the **bare** header `TambourineManager.cpp:` (no
  `band3/game/` prefix) which does NOT match objects.json's key → orphaned split (never
  reported as a built unit).
- **Wave-5** `1f53196..3d7b28e` — a second CoW-worktree port of the SAME TU, ff-merged on
  top. Added a 2nd objects.json entry, a 2nd (correctly-named `band3/game/`) splits block
  holding only TambourineGems, and 2 more overlapping lines to the DepthBuffer3D block
  (`.pdata E7A8-E908`, `.text BAC0-D6F0`). Brought region to 4 pdata + 4 text overlaps.
- **Earlier pin** `fb68f85d` — added the UPPERCASE map key `0X826DBAA8` (TambourineGems)
  as an ExactInstr anchor, before either port. Owner later added the lowercase `0x826dbaa8`
  duplicate.

Source `src/band3/game/TambourineManager.cpp` is IDENTICAL across owner and HEAD (412 lines,
no source-level collision — the ff-merge kept one file). Renamer `scripts/obj_target_symbol_renamer.py`
lowercases every key (`k.lower().removeprefix("0x")`, `int(,16)` → `fn_%08X`), so `0X826DBAA8`
and `0x826dbaa8` normalize to the identical `fn_826DBAA8` — the dup is harmless but redundant.

## Baseline (before reconcile)
- `measures.matched_functions = 10933`
- unit `default/band3/game/TambourineManager` = 1/1 (TambourineGems, matched)
- unit `default/DepthBuffer3D` = 29/129 (holds the other 4 Tambourine fns as unmatched fn_ via overlap)
- splits self-check: **4 pdata / 4 text overlaps**

## Canonical decisions
- **Unit name:** `band3/game/TambourineManager.cpp` (matches objects.json + report unit
  `default/band3/game/TambourineManager` + sibling band3/game TUs). The bare `TambourineManager.cpp:`
  block is the mis-named one → deleted, its full 5-fn carve folded into the canonical block.
- **objects.json:** collapse the two `band3/game/TambourineManager.cpp` entries to one.
- **Map casing:** keep lowercase (13417 lowercase vs 253 uppercase; renamer lowercases anyway),
  drop the exact-dup uppercase `0X826DBAA8`. Restore the owner's full 5 lowercase pins so the
  carved target ranges all get their correct names (Gems/Succeed/Fail/Swing/HandleButtonDown).

## Original DepthBuffer3D span (from f99d7bb~1, pristine)
```
.pdata 8222E610-8222E9B0   (as E610-E7A8 + E7A8-E9B0)
.text  826D9F60-826DBAA8 + 826DBAC0-826DE1E0   (gap BAA8-BAC0 = TambourineGems)
```

## Final disjoint carve (0 overlaps, contiguous, no gaps)
DepthBuffer3D.cpp = original MINUS the 5 Tambourine fns:
  pdata: E610-E830, E838-E860, E868-E8F8, E900-E908, E910-E9B0
  text:  D9F60-BAA8, BAC0-C7C0, CAC8-CB88, CDF4-D580, D6A0-D6F0, D7F8-E1E0
band3/game/TambourineManager.cpp = the 5 fns:
  pdata: E830-E838, E860-E868, E8F8-E900, E908-E910
  text:  BAA8-BAC0(Gems), C7C0-CAC8(Succeed), CB88-CDF4(Fail), D580-D6A0(Swing), D6F0-D7F8(HandleButtonDown)

## Results
While investigating, a concurrent agent landed the splits+objects half of this
reconciliation as `f9d212c` ("config: fix TambourineManager/DepthBuffer3D split
corruption") — its disjoint carve is byte-for-byte the plan above (DepthBuffer3D =
original minus the 5 Tambourine fns; all 5 folded into the canonical
`band3/game/TambourineManager.cpp:` block; bare block + dup objects.json entry
removed). A later concurrent commit `f83045e` committed the two authorized stray
files (`configure.py`, `tools/project.py` — fork-wibo deploy / WIBO_FS_CACHE /
wrapper default). This left only the **symbol-map dedup** for this pass:

- Removed the exact-duplicate uppercase key `0X826DBAA8` (TambourineGems); kept the
  lowercase `0x826dbaa8` (dominant convention: 13417 lowercase vs 253 uppercase, and
  the renamer lowercases every key anyway so both normalize to `fn_826DBAA8` — proven
  no-op). Kept `0x826dd6f0` (HandleButtonDown). The 3 size-mismatched fns
  (Succeed/Fail/Swing) remain unpinned per the wave-5/`f9d212c` decision — their
  target ranges are carved to TambourineManager but show as unpaired fn_ (0%); not a
  regression (they were 0% under DepthBuffer3D before).

### Composed verify (fresh_report.sh, renamer re-run)
- splits self-check: **0 pdata / 0 text overlaps** (global)
- `measures.matched_functions = 10934` (>= 10933 required; = post-f9d212c baseline)
- unit `default/band3/game/TambourineManager` = 2/5: TambourineGems **100%**,
  HandleButtonDown **100%** (report-normalized from cli-direct 99.22 reloc residue);
  fn_826DC7C0/CB88/D580 unpaired 0%.
- unit `default/DepthBuffer3D` = 29/125 (was 29/129; lost the 4 Tambourine fns, matched unchanged)
- matched-name set (fuzzy>=99.99) **IDENTICAL** before/after — 0 net delta, 0 foreign regressions.
