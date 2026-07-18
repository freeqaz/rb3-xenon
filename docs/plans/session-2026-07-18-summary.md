# Session summary — 2026-07-18 (near-miss cracks + identification stack)

**Result: main 15,364 → 17,445 strict matched (+2,081 across 26 landed
batches; sole "losses" = 16 spurious Flow-credited stubs re-credited honestly
to the new BandSwatch unit).** Every landing was gated by an isolated verify-worktree
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
| nearmiss crack w8 | `e1f0d6cb` | +7 | 17,339 |
| opus wave 9 (99-band top closed) | `07409503` | +1 | 17,340 |
| lane-C scanner-stack round 2 (fixed point) | `16816838` | +26 | 17,366 |
| lane-B nomatch near-pair vein (drained) | `29cf4bd5` | +35 | 17,401 |
| FMO mis-pin fix + scanner vbase filter | `4c11e0a7` | +0 | 17,401 |
| Rnd vtable residual +0xC (3 DC3 virtuals gated) | `ad9376d5` | +2 | 17,403 |
| naming wave (vtable positional alignment, +110 names) | `034a448a` | +6 | 17,409 |
| Flow.cpp re-pin → BandSwatch unit wired | `88bd166b` | +3 | 17,412 |
| lane-C round 3 (calibration ~0.031/name) | `23e6cbb8` | +5 | 17,417 |
| UIComponent verdict + StarDisplay ctor pin | `a2a96d57` | +1 | 17,418 |
| crack-fodder body-ports (named-at-0% pool) | `dea64514` | +3 | 17,421 |
| BandSwatch gameport (byte-pairing) | `6c98d9f7` | +22 | 17,443 |
| RndText::Style re-port 0x44→0x24 (WALL overturned) | `1d7e8356` | +2 | **17,445** |

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

1. ~~FileMergerOrganizer +212B~~ **CLOSED (`4c11e0a7`).** 0x8268f050 =
   `BandUser::NewLocalBandUser` (LocalBandUser, vbase, 0x110) — re-pointed;
   FMO NewObject does not exist (no NEW_OBJ factory, sizeof 0x3c). Scanner now
   rejects target-vbase/base-plain shape mismatches; re-run 38 scanned / 0
   drift / 5 shape-rejected. Match-inert (A/B 0/0).
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
5. ~~identification-nomatch residue~~ **Lane B DONE (+35, `29cf4bd5`) — vein
   DRAINED.** Near-pair ranking surfaced the ~60 tractable targets; residue is
   walls (reloc-count drift, deep divergence, tiny stubs, STL ICF). Do NOT
   re-run as a crack wave. ~~Rnd vtable 3 extra slots~~ **DONE +2
   (`ad9376d5`)** — retail vtable ends at slot 73; DC3-only
   CreateLargeQuad/DrawLargeQuad/SetVertShaderTex gated RND_DC3_VIRTUAL;
   DxRnd::UpdateScalerParams now paired = body-port lead. ~~Flow ~0xD0
   missing members~~ **DEAD — MISPAIR** (0x822ad1b0 = BandSwatch::Load by
   RTTI; the 0xD0 was vbase-offset delta 0x254−0x184 between unrelated
   classes; DC3 Flow.h byte-identical to ours). Real finding: Flow.cpp's
   splits.txt pin [0x822ACFE8,0x822AF178) is the BandSwatch/ColorPalette
   TU — wrong-TU pin, re-pin dispatched; real Flow cluster ~0x822a6xxx.
   ~~LyricPlate +0x40~~ **DEFER/WALL (confirmed)**: retail RndText::Style =
   0x24 (Ghidra memcpy-stride proof, ctor 0x82baf278) vs our DC3 0x44 (split
   colors + ObjPtr font + mKerning + mBlacklight); the fix is a GLOBAL Style
   change → ~100 access sites / 6 files / **272 matched fns** blast radius vs
   2 targets at ~99.9. Only path = dedicated rb3-Wii RndText re-port stream
   (recipe in ~/tmp/lyricstyle_patches/NOTES.md). Do NOT attempt as a bounded
   layout fix. **UPDATE: LANDED anyway (`1d7e8356`, +2)** — the 272-fn
   blast radius was theorized without building; a Style-pad probe proved
   LOST 0, so the re-port was safe. LESSON: prove blast radius empirically.
   ~~UIComponent-chain layout~~ **REFUTED (`a2a96d57`)** — ctor
   matches 100% so layout is byte-identical; Load residuals = per-class
   vbase-dispatch codegen WALL. Diagnostic rule: ctor-100% ⟹ wall. ALL
   lane-B structural leads now resolved.
6. ~~scanner re-run~~ **Lane C DONE (+26, `16816838`) — round-2 fixed point.**
   Re-run only after the named set grows ~100+ (marginal yield ~26/109).
7. ~~FMO re-pin + vbase filter~~ DONE (`4c11e0a7`, see lead 1).

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

## Close-out review (2026-07-18, post-run)

Three empirical Opus scout reviews sized the remaining value pools; synthesis +
next-focus ranking in **`docs/plans/review-2026-07-18-next-focus.md`**. TL;DR:
(1) body-port campaign on the mapped-but-0% pool (~250-290 oracle-backed cheap
ports; first wave DirLoader/Debug/MemHeap/Console/Env_NG/MeshAnim/TDStretch/
MidiSynth), (2) recarve the 5 warm mid-address auto blobs (Accomplishment
0x825F71A0 warmest, 32 mapped), (3) ≥99 fixwave round 2 REJECTED (band spent —
80% funclet mirage, 0/8 diagnose-sample wins).
