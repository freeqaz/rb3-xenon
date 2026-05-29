# Pin-Wave #2 — Handoff (Opus-planned, Sonnet-executable)

**Baseline:** HEAD `d0bff00`, **509 matched functions** (report.json
`measures.matched_functions`), 90,236 matched bytes, total_functions 65,216.

**Goal:** Apply a pre-computed, de-risked set of **47 TU pins** to grow matched
functions. Same mechanical pipeline that wave-#1 used to land +115 (394→509).

This doc is **self-contained**: the exact pin set is embedded below in §4. You do
not need to re-run the ranker — the spans here are already trimmed and
overlap-checked. (If you want to regenerate from scratch: `venv/bin/python
tools/pin_candidates.py rank` then re-apply the §3 curation rules.)

---

## 1. How wave-2 candidates were chosen (context, not action)

`tools/pin_candidates.py rank` merges 4 oracle JSONs (bindiff `unified_id.json`,
callgraph `unified_id_callgraph.json`, RTTI `unified_id_rtti.json`, vtable
`unified_id_vtable.json`) → consensus tiers S/A/B/C → source-present gate → drops
already-pinned TUs. That produced **68 NEW-pinnable TUs / 371 oracle fns**.

The raw ranker output ranks by *oracle-fn count*, which is **the wrong signal for
pinning**. The correct signal is **density** = oracle_fns / fns_in_span. Low
density means the oracle fns are *scattered* across a wide span that also contains
many *other* TUs' functions. `.text` is per-TU contiguous (no LTCG reordering),
so a low-density span is a loose hull that (a) steals address space and (b)
**overlaps** the tight, real clusters sitting inside it. Concretely the ranker's
PlatformMgr_Xbox span (1.7% density, 122 KB, 1124 fns) *contains* ContentMgr,
VirtualKeyboard, JoypadMsgs, FlowCommand, PlatformMgr — pinning it would collide
with all of them. This is exactly the ICF/interleave-collision class that caused
wave-#1's 14 drops.

## 2. The span-trimming insight (why §4 is pre-computed)

Several of the **highest-value** TUs (FlowNode 39 fns, MidiInstrument 28,
MultiTempoTempoMap 20, MatAnim 16, StoreOffer 9) had a *tiny* (56–216 byte)
**existing pin from a neighboring TU sitting inside their proposed span** — e.g.
`BinkReader.cpp [0x82B87A60,0x82B87AAC)` lands inside MultiTempoTempoMap's span;
`Movie.cpp [0x82472160,…)` lands inside FlowNode's. These are almost certainly
ICF-folded singletons whose canonical copy physically lives in the larger TU's
cluster (or adjacent-cluster boundary spill). dtk requires **disjoint** `.text`
ranges, so these block a full-span pin.

**Resolution (already applied in §4):** trim the span's `end:` down to the
**function-symbol boundary just before the first conflicting existing pin**. This
keeps 88–95% of the span for the big TUs and recovers them. The trims are noted
inline (`# TRIM …`). The trim endpoints were snapped to real `symbols.txt`
function starts, so dtk won't reject "ends within symbol".

## 3. Curation rules that produced §4 (so you can re-derive / extend)

From the 68 ranker candidates, the final 47 were selected by:
1. **Drop loose hulls** (density < 7%): MetaMusicManager, ThreeDSound, UIPanel,
   PlatformMgr_Xbox, UILabelDir, FlowPickOne. Their oracle fns are real but
   scattered — the dense inner clusters get pinned separately.
2. **Drop wave-#1 ICF/compile failures** (re-pin won't help): Fur, Fur_NG,
   MotionBlur, SoftParticles, EnvelopeGenerator, GainEffect, MidiChannel,
   HamPartyJumpData, SongUtl, xboxmem (ICF); HttpReqCurl, WebSvcMgrCurl,
   AudioDucker, Voice (compile — need curl headers / Fader API). **See §6 if you
   want to retry these post-jeff-fix.**
3. **Trim spans** that overlap an existing splits.txt pin to the clean prefix (5
   TUs, §2).
4. **Greedy by density**, dropping any candidate whose (trimmed) span still
   overlaps an already-selected denser one: dropped **ContentMgr** (32%) because
   it overlaps the denser **FlowCommand** (33%). *ContentMgr is a good follow-up:
   try pinning its prefix `[0x8250afe8,0x8250b558)` after FlowCommand lands — see
   §6.*

## 4. THE PIN SET — apply these 47 (the actual work)

**`jeff_blocked` (22 of 47, tagged `# JEFF-BLOCKED`)**: the span touches a
residual `symbols.txt` overlap region. **Attempt them anyway** — the endpoints
are on clean symbol boundaries. Worst case dtk rejects the range or a couple
internal functions render `<illegal>` and just don't match (no harm). Do **not**
skip them; only drop on an actual dtk/build error.

### 4a. objects.json entries

Add each below to **`config/45410914/objects.json`** as `NonMatching`. Group by
path prefix: `system/hamobj/*` → the **`hamobj`** group's `objects` dict;
**everything else** (`system/*`) → the **`engine`** group's `objects` dict.

The 3 hamobj entries: `system/hamobj/FilterVersion.cpp`,
`system/hamobj/DancerSequence.cpp`, `system/hamobj/Pose.cpp`.
The other 44 are all `engine` (the `rel=` path is in each §4b comment).

### 4b. splits.txt entries

The complete, ready-to-paste blob (also saved at `/tmp/wave2_splits.append`).
Append to **`config/45410914/splits.txt`**. Pin **only `.text`** — dtk back-fills
the matching `.pdata` on the next SPLIT.

```
# Wave-2 curated pin set. Pin .text; dtk back-fills .pdata.
MidiInstrument.cpp:               # system/synth — density=13% o=28 TRIM ->0x822b3d28 (95%, conflict BustAMoveData)
	.text       start:0x822b0c60 end:0x822b3d28
CharCollide.cpp:                  # system/char — 17% o=2
	.text       start:0x822b76f8 end:0x822b7d54
CameraTilt.cpp:                   # system/gesture — 100% o=2 JEFF-BLOCKED
	.text       start:0x8230f100 end:0x8230f158
CharBoneOffset.cpp:               # system/char — 11% o=7
	.text       start:0x82391328 end:0x82391a28
CharWeightable.cpp:               # system/char — 20% o=5
	.text       start:0x8239c350 end:0x8239c8c8
FlowValueCase.cpp:                # system/flow — 8% o=2
	.text       start:0x823a71d8 end:0x823a7eac
CharIKFoot.cpp:                   # system/char — 9% o=4
	.text       start:0x823acf28 end:0x823ad664
FlowDistance.cpp:                 # system/flow — 100% o=1 JEFF-BLOCKED
	.text       start:0x823af410 end:0x823af454
CharBone.cpp:                     # system/char — 15% o=16
	.text       start:0x823c3ff0 end:0x823c5e44
FlowSlider.cpp:                   # system/flow — 50% o=2 JEFF-BLOCKED
	.text       start:0x823d8818 end:0x823d88a4
HiResScreen.cpp:                  # system/rndobj — 67% o=3 JEFF-BLOCKED
	.text       start:0x823e4548 end:0x823e4644
TexRenderer.cpp:                  # system/rndobj — 15% o=6 JEFF-BLOCKED
	.text       start:0x82430e18 end:0x82431948
Group.cpp:                        # system/rndobj — 7% o=9
	.text       start:0x8243f5d0 end:0x8244169c
CharNeckTwist.cpp:                # system/char — 22% o=3
	.text       start:0x8244ce30 end:0x8244d184
MatAnim.cpp:                      # system/rndobj — 14% o=16 TRIM ->0x82450950 (89%, conflict Line)
	.text       start:0x8244e768 end:0x82450950
FilterVersion.cpp:                # system/hamobj — 100% o=1 JEFF-BLOCKED  [hamobj group]
	.text       start:0x82462300 end:0x82462340
Ribbon.cpp:                       # system/rndobj — 18% o=2
	.text       start:0x8246f4f8 end:0x8246f960
Set.cpp:                          # system/rndobj — 100% o=1 JEFF-BLOCKED
	.text       start:0x8246f960 end:0x8246f9f0
FlowNode.cpp:                     # system/flow — 24% o=39 JEFF-BLOCKED TRIM ->0x82472160 (88%, conflict Movie)
	.text       start:0x824710a8 end:0x82472160
FlowQueueable.cpp:                # system/flow — 22% o=3
	.text       start:0x82474ba8 end:0x82474e9c
DancerSequence.cpp:               # system/hamobj — 10% o=3 JEFF-BLOCKED  [hamobj group]
	.text       start:0x824840e8 end:0x82484954
FlowOutPort.cpp:                  # system/flow — 100% o=2 JEFF-BLOCKED
	.text       start:0x824a5990 end:0x824a59d4
Shockwave.cpp:                    # system/rndobj — 12% o=2
	.text       start:0x824bd0c8 end:0x824bd6e8
FlowSound.cpp:                    # system/flow — 8% o=3
	.text       start:0x824e4af0 end:0x824e542c
PlatformMgr.cpp:                  # system/os — 12% o=5 JEFF-BLOCKED
	.text       start:0x825018a0 end:0x82501e5c
FlowCommand.cpp:                  # system/flow — 33% o=7 JEFF-BLOCKED
	.text       start:0x8250b558 end:0x8250b768
VirtualKeyboard.cpp:              # system/os — 22% o=2
	.text       start:0x825145b0 end:0x82514870
JoypadMsgs.cpp:                   # system/os — 19% o=6
	.text       start:0x8251eff8 end:0x8251f828
Pose.cpp:                         # system/hamobj — 100% o=1 JEFF-BLOCKED  [hamobj group]
	.text       start:0x825b67b0 end:0x825b6804
PhysicsVolume.cpp:                # system/world — 7% o=3
	.text       start:0x825e16d8 end:0x825e25cc
ColorPalette.cpp:                 # system/world — 29% o=8 JEFF-BLOCKED
	.text       start:0x8268c4a8 end:0x8268c844
BaseSkeleton.cpp:                 # system/gesture — 17% o=2
	.text       start:0x82693c20 end:0x826940a0
Stream.cpp:                       # system/synth — 50% o=2 JEFF-BLOCKED
	.text       start:0x826f41c0 end:0x826f42a4
DirUnloader.cpp:                  # system/obj — 33% o=3 JEFF-BLOCKED
	.text       start:0x82717a40 end:0x82717c24
StoreOffer.cpp:                   # system/meta — 12% o=9 TRIM ->0x82781bb8 (36%, conflict SpeechMgr)
	.text       start:0x82781500 end:0x82781bb8
MoviePanel.cpp:                   # system/meta — 11% o=5 JEFF-BLOCKED
	.text       start:0x8278ad10 end:0x8278be6c
StorePanel.cpp:                   # system/meta — 8% o=8
	.text       start:0x8278fe70 end:0x82790c24
ConnectionStatusPanel.cpp:        # system/meta — 40% o=3 JEFF-BLOCKED
	.text       start:0x82795448 end:0x82795630
FilePath.cpp:                     # system/utl — 100% o=2 JEFF-BLOCKED
	.text       start:0x82799a00 end:0x82799a60
UTF8.cpp:                         # system/utl — 40% o=7 JEFF-BLOCKED
	.text       start:0x827a7450 end:0x827a7dac
Chunks.cpp:                       # system/utl — 17% o=2
	.text       start:0x827b35e8 end:0x827b3c38
DisplayEvents.cpp:                # system/midi — 100% o=1 JEFF-BLOCKED
	.text       start:0x827ca570 end:0x827ca8d0
UITransitionHandler.cpp:          # system/ui — 24% o=5
	.text       start:0x827dc4e0 end:0x827dca48
UISlider.cpp:                     # system/ui — 9% o=6
	.text       start:0x827e4368 end:0x827e4f7c
UIListLabel.cpp:                  # system/ui — 19% o=4
	.text       start:0x827fa338 end:0x827facc4
DOFProc_NG.cpp:                   # system/rndobj — 14% o=3
	.text       start:0x82b5d040 end:0x82b5d764
MultiTempoTempoMap.cpp:           # system/utl — 81% o=20 JEFF-BLOCKED TRIM ->0x82b87a60 (88%, conflict BinkReader)
	.text       start:0x82b871f0 end:0x82b87a60
```

## 5. Execution recipe (the mechanical loop)

**All source files already exist in `src/`** (copied from dc3 earlier) — verified
present for all 47. They are **not yet compiled** (absent from objects.json), so
expect some compile failures that need porting (the wave-#1 rate was ~88% clean).

1. **Apply** §4a (objects.json) + §4b (splits.txt).
2. `touch config/45410914/config.yml && ./tools/ninja-locked 2>&1 | tee /tmp/rb3_pinwave2.log`
   — ALWAYS via `ninja-locked`, ALWAYS `tee`. Never bare `ninja`.
3. **Triage failures** from the log:
   - **Compile error** (missing header / API): if a quick port (≤~10 min: add an
     include, rename `mStr`→`Str()`, `revolution/OS.h`→xdk, `operator new(unsigned
     long)`→`size_t`, 1-arg `Debug::Fail` — see
     `docs/plans/meta_band-port-breaking-changes.md`), do it. Otherwise **drop the
     TU** (remove from objects.json + splits.txt) and note it.
   - **"ends within symbol 0xXXXX"**: extend `end:` to the next symbol boundary
     (grep `symbols.txt` for the next `fn_…`/named symbol start). **"starts within
     symbol"**: pull `start:` back to the symbol start, or drop.
   - **"overlaps"/"assigned to multiple units"**: another pin claims that range —
     drop the lower-density TU.
   - **dtk SPLIT WARN** (UTF-16, PpcRel, unaligned): tolerated noise, ignore.
4. **Iterate** until the build is green (config.json emitted, report.json
   regenerated). Drop stubborn TUs rather than spending >~15 min each.
5. **Measure** (report.json methodology, NOT objdiff-cli raw):
   ```
   python3 -c "import json;m=json.load(open('build/45410914/report.json'))['measures'];print('matched_functions',m['matched_functions'])"
   ```
   Delta from **509** is the wave-2 yield.

## 6. Stretch / follow-ups (only if §4 lands fast)

- **ContentMgr.cpp** prefix `[0x8250afe8,0x8250b558)` (10 oracle fns, system/os)
  — pin *after* FlowCommand succeeds (they overlap; FlowCommand won density).
- **Retry wave-#1 ICF drops post-jeff-fix**: the phantom prune re-SPLIT may have
  unblocked some. Cheap to try single-fn pins: SongUtl, xboxmem, MotionBlur.
- **Recover the trims**: if the parallel jeff/splitting investigation (see §7)
  eliminates the residual overlaps, the trimmed TUs (MultiTempoTempoMap, FlowNode,
  MatAnim, MidiInstrument) can be re-pinned to full span for the tail functions.

## 7. Hard constraints (coordination — READ)

- **Do NOT `git commit`.** The orchestrator commits precisely (objects.json +
  splits.txt + symbols.txt only). Leave everything else.
- **Do NOT touch** the uncommitted permuter-port files: `.claude/skills/permute/
  SKILL.md`, `scripts/permuter_rb3xenon.py` (deleted), `decomp-synth.json`. Those
  belong to a parallel background actor.
- **Do NOT `git stash` / `git checkout` / `git restore` files** in this shared
  main tree (per CLAUDE.md).
- A parallel **Opus jeff/splitting agent** may be running, but it works on a
  *private jeff clone* + an isolated worktree and will **not** touch this tree's
  `../jeff` or build. You own `.ninja-build.lock` here; it won't contend.
- **Report back**: final matched_functions, the delta from 509, the list of TUs
  that pinned cleanly, and the list you dropped (with the one-line reason each).
