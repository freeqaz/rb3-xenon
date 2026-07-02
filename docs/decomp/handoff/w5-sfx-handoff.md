# WAVE-5 Sfx lane handoff (branch `w5-sfx`, worktree `/home/free/tmp/wt-w5-sfx`)

Commit: `fa82a1c` (single checkpoint). Base: main `5cb96d4`.

## What landed

Wired the pre-existing dc3-scaffold `src/system/synth/Sfx.cpp` into the build,
carved its 3 target functions out of the wired `StreamNull.cpp` container range,
reconciled the `.cpp` bodies toward the RB3-360 (Wii-shaped) retail, and pinned
the one function that reaches report-normalized-100.

Files changed (only these 4):
- `src/system/synth/Sfx.cpp` — compile fix + Load/UpdateVolume reconcile.
- `config/45410914/objects.json` — add `"system/synth/Sfx.cpp": "NonMatching"` to module **engine** (mirrors SfxMap.cpp).
- `config/45410914/splits.txt` — carve (see below).
- `scripts/target_symbol_map.json` — +3 entries (see below).

**`Sfx.h` was intentionally NOT touched** — the wired `SfxMap.cpp` includes it and
depends on the dc3-flavored `SfxMap` class (`Load(BinStreamRev&)`, the
`operator<<`/`operator>>(BinStreamRev&,…)` pair). Editing Sfx.h risks regressing
SfxMap's 2 already-matched fns. Verified after all changes: SfxMap `Save` 99.88%
size-exact + `operator<<` 100% unchanged; StreamNull own fns unchanged
(IsFinished/Resync 100%, ~StreamNull 99.26%, ctor 99.51%, all size-exact — same as
pre-carve).

## Per-id outcomes (objdiff-cli-direct, JSON to file)

| id (VA) | symbol | before | after | size (t/b) | verdict |
|---|---|---|---|---|---|
| 0x826FCC90 | `?Pause@Sfx@@QAAX_N@Z` | — | **99.75%** | 80/80 exact | **PIN** — AtLimit source-immune (LINKER_MERGED ICF: the one diff is `bl fn_826FCB80` = ICF-folded `SfxInst::Pause`). report-normalized-100. |
| 0x826FFB28 | `?Load@Sfx@@UAAXAAVBinStream@@@Z` | 43.49% | 75.84% | 304/304 exact | fuzzy-paired, CONFIRMED identity (size-exact). Ceiling = shared-infra divergence, see below. |
| 0x826FCBF8 | `?UpdateVolume@SfxInst@@UAAXXZ` | 53.08% | 53.08% | 148/192 | fuzzy-paired, CONFIRMED identity (addr+size). Blocked by MoggClip/SfxInst header drift. |

Strict claimed = **1** (Pause). Fuzzy-paired = **2** (Load, UpdateVolume) — both
confirmed identities (not guesses): Pause & Load are size-exact, UpdateVolume's
target size 0x94 == fn_826FCBF8 and it calls FaderGroup::GetVal in the Sfx cluster.

## Splits carve (StreamNull.cpp is wired — highest landing risk, verified clean)

StreamNull.cpp `.text [0x826FBD28,0x82700E18)` → split into 3 disjoint ranges,
handing 2 ranges to Sfx.cpp:
```
StreamNull.cpp:  .text 0x826FBD28..0x826FCBF8
                 .text 0x826FCCE0..0x826FFB28
                 .text 0x826FFC58..0x82700E18   (.pdata unchanged, keeps all unwind)
Sfx.cpp:         .text 0x826FCBF8..0x826FCCE0   (UpdateVolume + Pause, adjacent)
                 .text 0x826FFB28..0x826FFC58   (Load)
```
- Boundaries derived from real fn symbols (next-symbol addresses in symbols.txt).
- Replaced the stray scaffold `Sfx.cpp` split (`.text 0x824B3818..0x824B3874`,
  a 0x5C unmapped fn unrelated to Sfx) — it now falls back to a default gap unit.
- Sfx.cpp given **no .pdata** on purpose: the Sfx fns' unwind entries stay inside
  StreamNull's pdata range, avoiding an in-section overlap. Overlap self-check:
  `pdata 0 overlaps / text 0 overlaps`.

## Why Load stalls at 75.84% (the real ceiling — infra, not effort)

Structural fix already applied (ASSERT_REVS→Wii `if(rev>0xC)` shape) got it
size-exact. Remaining diffs are ONE shared-infrastructure divergence:

- **Target uses the Wii static-gRev vector reads**: it stores `rev` into a global
  (`SfxMap::gRev`/`sRev` at `lbl_82DA0017`) and calls the **2-arg**
  `operator>>(BinStream&, ObjVector<SfxMap>&)` (targets `fn_826FEE18`/`fn_826FFAD0`),
  passing `bs` directly.
- **xenon's shared BinStream/SfxMap infra is BinStreamRev-based**: the only
  ObjVector read operator is `operator>>(BinStreamRev&, vector<T>&)`, so my source
  must build a `BinStreamRev d` on the stack and call the **3-arg** BinStreamRev
  operator. That extra stack object drives the whole remaining delta: r30↔r31
  swap (`this`/`bs` allocation), frame 0x80 vs 0x70, and the different call targets.

Closing it requires giving `SfxMap`/`MoggClipMap` a `gRev`/`sRev` static + plain
`operator>>(BinStream&, ObjVector<…>&)` overloads (Wii style) — a **shared-header
serialization change** that would touch the wired `SfxMap.cpp` and every ObjVector
consumer. Out of scope / too risky for a port lane with the owner actively landing.
The LINKER_MERGED lines objdiff attributes here are partly mis-attributed (the
2-arg vs 3-arg operators are genuinely different fns, not ICF folds).

## Why UpdateVolume stalls at 53% (header drift, multiple blockers)

- xenon `MoggClip::SetVolume` is **virtual** → my base does a vtable dispatch
  (`mtctr`/`bctrl`); the target does a **direct `bl`** to a non-virtual
  `SetControllerVolume(float)` that xenon's MoggClip.h does not declare.
- The target iterates an **`mMoggClips` pointer-list member** (+4 stride) on
  SfxInst; xenon can't express the Wii `ObjPtrList<MoggClipMap>` because
  `MoggClipMap` is a plain class (not `Hmx::Object`) in xenon's shared header, so
  the scaffold iterates `mSfx->MoggClipMaps()` (value vector) instead → different
  offsets, prologue r28-r31 vs r29-r31, extra loads.
- Fixing needs shared MoggClip.h + MoggClipMap.h + SfxInst-layout changes. Weakest
  id in the worklist (bsim15-20); not worth the shared-header risk. Left as-is.

## What a lander must know
- Land order/JSON unions: objects.json add is in module **engine**; target_symbol_map
  add is 3 keys near 0x826FCBF8/0x826FCC90/0x826FFB28 (sorted). Splits: StreamNull
  now has 3 `.text` lines — the resolve_splits_union must keep them unit-attributed.
- Re-verify post-land: StreamNull.cpp still reports its ~18 matched (carve preserved
  its first range [0x826FBD28,0x826FCBF8)); SfxMap.cpp still 2/2.
- Pause is the only strict add. If policy wants map entries at true-100 ONLY, drop
  the Load + UpdateVolume keys (both are confirmed-identity fuzzy-paired, safe to
  keep as progress or drop — they report their real %; neither is a false identity).
