# Session summary — 2026-07-18 (near-miss cracks + identification stack)

**Result: main 15,364 → 17,339 strict matched (+1,975, ZERO regressions across
14 landed batches).** Every landing was gated by an isolated verify-worktree
whole-binary A/B (strict SET delta keyed `(unit, name)` + fuzzy sum; `LOST`
must be empty). One agent-recommended change was rejected by that gate (see
"Gate saves" below) — the process worked exactly as designed.

## Scoreboard (chronological)

| landing | commit | Δ strict | main |
|---|---|---|---|
| nearmiss crack w1 (40-80% <400B) | `40688918` | +6 | 15,370 |
| nearmiss crack w2 | `4fa04926` | +3 | 15,373 |
| nearmiss crack w3 | `9b88b27b` | +3 | 15,376 |
| structural leads ×3 | `8886591c` | +6 | 15,382 |
| nearmiss crack w4 (400-800B) | `c6d0b969` | +3 | 15,385 |
| **global correlator sweep** | `366709b9` | **+1,493** | 16,878 |
| ICF base-side disambiguation | `01c1414f` | +118 | 16,996 |
| target-twin disambiguation (Lever 3) | `1000b661` | +234 | 17,230 |
| nearmiss crack w5 (99-band) | `d7aacbbd` | +63 | 17,293 |
| nearmiss crack w6 | `ac0b1763` | +7 | 17,300 |
| nearmiss crack w7 (NewObject family) | `dcb2444c` | +29 | 17,329 |
| sizeof-oracle scan | `c48f6527` | +3 | 17,332 |
| nearmiss crack w8 | `e1f0d6cb` | +7 | **17,339** |

(Plus docs-only: PlatformMgr dead-lever banner `92dad991`, scoreboard updates.)

## The big one: identification stack (+1,845)

`tools cost ~2 hours, paid +1,845 strict:`
1. **Global reloc-masked correlator sweep (+1,493)** — ran
   `scripts/harvest/tu5_reloc_masked_correlate.py` globally (all 573 paired
   units) instead of per-family: 1,495 CLEAN 1↔1 byte-identity map entries
   (unmapped target `fn_` ↔ unique compiled base symbol, byte-identical after
   masking COFF relocs, size-equal). The earlier "SongSort real_clean=0"
   conclusion was unit-local truth wrongly generalized, and the jeff
   Class-2/4 carve merges had created new clean pairs. **Lesson: re-run cheap
   global scans after carve/tooling levers land.** Driver preserved:
   `scripts/harvest/tu5_correlate_global_driver.py`.
2. **Base-side MULTI disambiguation (+118)** — reloc-destination NAME-sequence
   discriminator (`tu5_icf_disambiguate.py` + `tu5_reloc_seq.py`). Key
   correction: the "5,722 ambiguous" pool was ~4/5 `__unwind$`/`__ehhandler$`
   COMDAT pollution (they carry CODE+function flags in MSVC X360 objs).
3. **Target-twin mirror (+234)** — same discriminator from the target side;
   922 coincidental masked-byte matches correctly REJECTED (the precision
   gate). Scanner: `tu5_target_twin_disambiguate.py`.

**The old 15,804 "ceiling" is OBSOLETE** (it measured TU5-flip recovery scope
only; we overshot it by +1,535). Identification residue: ~5,300 nomatch =
genuine body divergence (body-port lane).

## Near-miss crack waves (99-band pivot, w5-w8: +106)

Proven loop: mine fresh `report.json` band → Sonnet agent per target in an
isolated worktree (`setup_worktree.sh`), mandatory source-vs-oracle gate →
coordinator harvests real `git diff`s → ONE combined verify-worktree A/B →
path-limited land. Win classes that recurred (transferable taxonomy):
- **Layout proofs from allocator immediates**: a pinned factory's
  `li r3,N` = ground-truth `sizeof` (PracticePanel 0x94, HighFive 0x30,
  Voice 0x74, FlowNode-derived +16). Scanner landed:
  `scripts/harvest/newobject_sizeof_scan.py` (38 classes: 35 exact, 3 drift).
- **DC3-only member strip**: Debug Crucible block (sizeof 0x144→0x100),
  UIListElementDrawState fabricated 16B overlay.
- **TU5 field/vtable reorders**: LoadMgr 3 fields → tail; BandUser TU5 virtual
  precedes IsParticipating; TrackInterface GetTrackIcon/UserName slots swapped;
  Game::Properties AllowOverdrivePhrases at Prop+0x5.
- **PROPSYNCS-static (+54 of w5's +63)**: BandWardrobe SYNC_PROP*→_STATIC
  cascaded 51 flips. Vein now DRAINED — `Object.h`'s PCH macros are already
  inherently static; only `ObjMacros.h`-including TUs (2, both done) could flip.
- **EH-temp homing**: explicit `.Str()` on a discarded `StaticClassName()`
  temp forces retail's separate stack slot (one macro line → 10 FxSend*360
  factories + 10 funclets).
- **Logic-level finds**: Splash::EndSplasher calls `Resume()` not `Suspend()`
  (Ghidra-proven); Singer reads mLastFrameMicEnergy; StreamPlayer NewStream
  arg `true`; HttpGet enum true retail ordering.

## Structural leads (all 3 confirmed, +6)

- ChunkAllocator `MAX_FIXED_ALLOCS=32` (retail `new(0x80)`; 64 kept under
  `HX_NATIVE`).
- HttpGet::State retail order: Downloaded=3, Failed=4, FailedSend=5, Pending=6.
- SampleInst setters are NON-virtual in retail (no PlayableSample vtable
  slots) — resolved the SfxInst +0x28 this-adjust mystery.

## Gate saves (why the A/B discipline matters)

- **FlowNode shared-vbase pad refactor (w8): REJECTED.** Agent verified
  NewObject byte-identical, but my combined A/B caught it LOSING
  `??_GFlowDistance` (a second vbase perturbs dtor codegen). Landed
  per-class pads stay; refactor dropped.
- w7's "FlowAnimate/FlowDistance = phantom pins" verdict was WRONG — the
  sizeof-oracle scan proved real +16 drift next day. Independent instruments
  cross-check each other.

## Dead levers / walls documented (do NOT re-hunt)

- **PlatformMgr layout**: genuinely diverges (retail MsgSource-lineage,
  mConnected@0x26) but ZERO strict gain — accessors unpinned, global-reloc
  access offset-normalized. Banner in `PlatformMgr.h`.
- 99-band walls (regalloc/CSE single-register-swap class): Synth360::Terminate,
  PlayNextShot, AddDircut, OnEnterCloset (permuter exhausted 185 variants),
  BeginTiling, CalculateHandDest, ShapeDeltaBox, ListAnimChildren,
  TrainerGemTab::Draw, yylex, Keys::AtFrame, EventTrigger erase,
  FillCompressedVertex, SampleData::Load, ??_GCharServoBone + ~10 Flow ??_G
  (ICF mispair signature).
- STL-template near-misses (261 fns: `_M_fill_insert`/`push_back`/`_Rb_tree`/
  `insert_unique`) = ICF pairing artifacts, excluded from all sweeps.

## Banked leads (updated after opus wave 9, `07409503`, main 17,340)

1. ~~FileMergerOrganizer +212B~~ **RESOLVED: FALSE POSITIVE.** The pinned
   NewObject@0x8268f050 is a foreign virtual-base/LocalUser class; real FMO is
   0x3c (100% dtor proof). Follow-ups: re-pin the true FMO NewObject address;
   teach `newobject_sizeof_scan.py` to reject vbase-ctor callees.
2. ~~FlowSound pin~~ **DONE (net-0).** NewObject island carved from
   ContextChecker's span; zero yield because our build never emits
   `?NewObject@FlowSound` — the factory registration is unwired (that wiring is
   the surviving lead, non-deterministic).
3. ~~Mispinned Flow-family 5~~ **RESOLVED: COMDAT-placement wall.** VAs and map
   entries already correct; retail's linker placed the weak thunks in different
   TUs than our per-TU COMDAT emission. Nothing deterministic to fix.
4. ~~90-99 band top~~ **CLOSED for crack waves** (9/9 Opus WALL verdicts at
   98.7-98.9; new named wall class: MSVC strcpy-intrinsic terminator test
   `cmplwi` vs `extsb.` — not source-expressible). Permuter BANNED (user
   directive 2026-07-18: low yield + grinds the box).
5. ~5,300 identification-nomatch = genuine body divergence (Lane B in flight).
6. Re-run the three identification scanners after big body-port waves (Lane C
   in flight).

## Tooling landed this session

`scripts/harvest/`: `tu5_correlate_global_driver.py`,
`tu5_icf_disambiguate.py`, `tu5_reloc_seq.py`,
`tu5_target_twin_disambiguate.py`, `newobject_sizeof_scan.py`.

## Method reference

Coordinator (Fable) delegates: Opus for triage/tooling/keystones, Sonnet for
mechanical cracks; every wave = isolated worktrees + source-vs-oracle gate;
coordinator independently re-verifies EVERY diff with its own clean-worktree
whole-binary A/B before landing; path-limited commits only; scoreboard in
`docs/plans/tu5-p5-progress.md` updated per landing; memory updated in
`project_correlator_global_sweep_2026-07-18.md` +
`project_nearmiss_crack_campaign_2026-07-18.md`.
