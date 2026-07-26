# Lane AH — the layout oracle, and the `S=1` tier it unlocks (2026-07-26)

Branch `laneAH-layout`, worktree `~/tmp/wt-laneAH-layout`. Baseline **30,244**
(main `750ee3d8`).

Two halves. Part 1 replaces a *known-bad* layout oracle with the compiler itself.
Part 2 spends that on a tier of functions that every previous lane was
structurally briefed **not** to look at.

---

## 1. The oracle: `cl.exe /d1reportSingleClassLayout<Class>`

### 1.1 What was wrong before

Every lane read class layout from one of two sources, and both are the *same*
source:

1. the hand-written `// 0xHEX` comments trailing member declarations in our
   headers, and
2. the orchestrator MCP tool `lookup_struct_offset`, which answers out of
   `struct_db.sqlite` — which `tools/struct_db.py` **parses from those very
   comments**.

So `lookup_struct_offset` looked like an independent oracle and was in fact a
mirror. And the comments are wrong in places (§4). A silently-wrong oracle is
worse than a missing one: it answers confidently and the lane never re-checks.

### 1.2 What replaces it

`cl.exe /d1reportSingleClassLayout<Name>` is an undocumented MSVC flag that
**works through the wibo-wrapped X360 compiler**. For every class whose name
*starts with* `<Name>` it prints:

* the real `sizeof`,
* every member at its real byte offset, nested by base class,
* `<alignment member> (size=k)` rows — **padding, explicitly labelled**,
* the vtable slot by slot, with the class that actually supplies each slot,
* the `this` adjustor per virtual — what a call site subtracts from `r3`.

The last two are things the header comments never encoded at all, and they are
exactly what a base-sub-object-adjust mismatch (`subi r3, r29, 0x1c` vs `0x64`)
is measuring.

### 1.3 Wiring (all committed)

* **`scripts/harvest/class_layout_report.py`** — lifts the exact compile command
  from `ninja -t commands` (so include path, PCH, `/O1 /Oi /GR /EHsc` all match
  the real build), strips the objcache prefix and `/showIncludes`, redirects
  `/Fo` to a scratch temp, adds the flag, and parses the output.
  * `--exact` (the flag is a *prefix* match: `...LayoutSynth` also reports
    `SynthSample`), `--offset 0x118` ("which member is here?"), `--json`,
    `--raw`, `--tu <path.cpp>`, `--check-header`, `--fix-header`,
    `--project-dir <worktree>`.
  * Auto-resolves a TU in which the class is complete: declaring header →
    same-stem `.cpp` → smallest compiled `.cpp` that includes the header.
  * ★ For a class declared in a header shared by many TUs, auto-resolution can
    land on a TU where the class is incomplete — **pass `--tu`** rather than
    concluding the class does not exist.
  * Costs one TU compile (10 s – 15 min depending on TU size and machine load);
    writes only to a scratch temp, so it is safe to run during a build.
* **`lookup_struct_offset` (MCP)** — now consults the compiler first
  (`verify=true` by default, plus a `project_dir` so a worktree gets *its own*
  headers) and returns a **VERIFIED** answer with the real `sizeof`. Every
  comment-derived fallback — exact, range, RB2 DWARF, and not-found — is
  suffixed with a loud **UNVERIFIED** banner that names the known-bad headers
  and prints the command to get ground truth. Reports are cached per
  (class, project_dir, header mtime).
* **`/struct-info`** — leads with the compiler report; the struct DB is demoted
  to a cross-check for the inheritance chain and `#if`-gated layout forks (which
  the compiler report flattens away).
* **`/stack-layout`** — gains the base-register split table that separates a
  stack-slot immediate (`r1`/frame-aliased `r31`) from a class-offset immediate
  (object pointer / `sizeof` / sub-object adjust). That fork is the whole triage
  problem in the `S=1` tier.
* **`CLAUDE.md`** — "ask the compiler, not the comments" under Struct + vtable.

Live demonstration of the difference: `Synth + 0x60`. The comment-derived path
answers *"No field found at offset 0x60 in Synth or its parent classes."* The
compiler answers *"inside `Synth::mCommonBank` at +8 (member starts at 0x58),
type `ObjDirPtr<ObjectDir>`."*

---

## 2. The `S=1` tier

`match_percent_normalized` is not a fuzzy score: `pct = (1 − S/(100·N))·100` in
f32 with `N = size/4`, so it inverts losslessly to (penalty `S`, instruction
count `N`). `S=1` is **one differing immediate operand** — and it always scores
99.9x%, so the "78–96 % flip band" heuristic every lane was briefed with
*structurally excluded the entire tier*.

`scripts/harvest/nearfree_tier_worklist.py --tiers 1` over the 892 named paired
sub-100 functions (N ≥ 8): **34 functions at `S=1`**, the largest single tier in
the binary (next: 660→26, 600→23, 100→19). 23 survive the cluster<4 filter.

★ **It is a layout tier wearing an immediate mask** — and the ratio over all 34
(33 conclusively bucketed) is:

| Root cause | Count | Share |
|---|---|---|
| **LAYOUT** (class/member offset, `sizeof`, vtable slot, base/vtordisp adjust) | **18** | 53 % |
| **MISPAIR** (`target_symbol_map.json` points at a different function) | **6** | 18 % |
| OTHER (2 reloc-noise at_limit, 2 ICF-fold, 1 field-*type* divergence) | 5 | 15 % |
| **CONSTANT** (a genuine wrong literal) | **2** | 6 % |
| **STACK** (frame slot) | 2 | 6 % |

So **only 2 of 34** are what the tier's name suggests. Routing this tier to
constant-hunting would have found 6 % of it. Routing it to **layout** finds 53 %,
and the *next* biggest bucket is a map-repair channel, not a source channel — see
§5 and §6.

This lane converted 4 of the 18 LAYOUT ones (plus the two STACK ones) into **+22**
whole-binary matches, because a class fix is fleet-wide: `~BandCharacter` alone
carried 9 further functions in its unit along with it.

---

## 3. Per-target results — **+22 measured** (30,244 → 30,266, 0 net regressions)

Five subagents: one Sonnet read-only triage, four Opus fixers in their own
worktrees. Every claim below was re-measured against **this lane's own** baseline
pickle, unit-agnostically, merge by merge.

| Merge | Claimed | Measured | Gained | Lost |
|---|---|---|---|---|
| `laneAH-synth` | +3 | **+3** | `~Synth`, `Synth::Terminate`, `SynthTerminate` | 0 |
| `laneAH-songmgr` | +2 | **+2** | `BandSongMgr::Terminate`, `fn_8257A9C8` | 0 |
| `laneAH-metapanel` | +5 | **+5** | `MetaPanel::Handle`+`OnMsg`+3 funclets, `SongPreview` ctor+`Terminate` | 2 (see below) |
| `laneAH-stack` | +12 | **+12** | `AccomplishmentProvider::Custom`, `CharServoBone::DoRegulate`, `~BandCharacter` + 9 in `BandCharacter` | 0 |

No gain ever appeared in a unit the corresponding diff did not touch, so the
stale-obj phantom did not fire on any leg.

### What each defect actually was

* **`Synth`** — retail has **no `mTrackLevels` instance slot** (DC3 added it) and
  `mZombieInsts` sits immediately before `mCommonBank`, not at 0x7c. Pinned from
  the retail destructor's *reverse-declaration-order* destroy sequence. Removing
  the misplaced list also made the pre-existing `mCommonBankPad_Dc3Deficit` pad
  unnecessary — the pad was a symptom, not a fix. Two body fixes fell out of the
  corrected offsets (`Terminate` ends with `RELEASE(mMidiInstrumentMgr)`, and
  `SynthTerminate()` has no leading `StopAllSounds()`).
* **`BandSongMgr`** — *not* the handed-down "24 bytes missing before 0x160".
  `mUpgradeMgr`/`mLicenseMgr` already matched; only `mContentAltDirs` was
  misplaced (ours 0x148, retail 0x160). Retail interposes a 12-byte vector of
  8-byte trivially-destructible elements at 0x148 plus 4 bytes at 0x154, and puts
  `mContentAltDirs`/`mMaxSongCount` *after* the manager pointers.
* **`SongPreview`** (behind `MetaPanel::OnMsg`) — `sizeof` was 128 vs retail 112.
  The DC3-only `mInitted` / `mPreviewDb` / `ObjPtr<TexMovie>` tail went behind
  `#ifdef HX_NATIVE`. Confirmed by the retail ctor storing *exactly* 0x2c…0x6d
  with nothing in 0x6e..0x74 and no `ObjPtr` ctor call.
* **`~BandCharacter`** — rb3-Wii's `float unk6d8` does not exist in retail 360
  (the ctor emits exactly two zero-stores in that region and `0x778+4+4+64 ==
  0x7c0` leaves no slot), and the waypoint precedes the flags. Nine extra
  functions in the unit came along.
* **`AccomplishmentProvider::Custom`** — a stack **slot-coalescing** delta, not a
  frame-size delta (both 0x90). Retail gave a sret temp its own slot; we merged it
  into a later-scoped `int`. Hoisting two `int` declarations one scope level up
  extends their live range so MSVC stops merging.
* **`CharServoBone::DoRegulate`** — not stack at all: we read `pred.mLastPos`
  where retail reads `pred.mPos`. It *looked* like a +0x14 frame shift only
  because `sizeof(Vector3)` is **16** here (trailing SIMD pad), so
  `ClipPredict::mAng` lands at 0x1c and `mLastPos` at 0x20.

### Negatives, recorded honestly

* **`MetaPanel` lost 2** (`??_GMetaPanel`, `MetaPanel::NewObject`) for a net +5.
  Those two want vbase 0xEC / `sizeof` 0x114, mutually consistent but contradicted
  by `Handle` (170 instructions, 9 vbase-relative refs → 0xDC) and by `OnMsg`.
  The padded-to-0x114 alternative was measured explicitly: **−2 net**. Kept the +5.
* **`HamLabel::Count` — at_limit, nothing committed.** The f30/f31 lead was wrong:
  both sides save exactly f30+f31 at identical slots and both frames are 0xa0. All
  five mismatches reduce to one allocator decision. Zero size delta, zero
  insert/delete — the one shape where at_limit is legitimate.
* **`Character::CollideListSubParts` should not be virtual** —
  `BandCharacter::OnCamTeleport` dispatches `Teleport` through primary-vtable slot
  11 while ours uses 12, so that entry occupies a slot retail lacks, and the
  comment at `src/system/char/Character.h:89-93` calling it harmless is
  **disproven**. But simply de-virtualising measured **+13/−8 = net −7** (lost 8
  `default/Character` functions). Reverted — this needs a dedicated `Character`
  vtable-order lane, not a one-line deletion.
* **`ClipCollide::Save`** — the handed-down "SAVE_REVS(1,0)→(3,0) + four ObjPtr→
  4-byte" fix would have **corrupted correct source**. See §6.

---

## 4. Headers whose `// 0xHEX` comments were proven wrong

★ The comments are not merely stale — they are **another platform's or another
game's offsets, copy-pasted**. Two distinct provenances, both confirmed:

| Header | Wrong | Provenance |
|---|---|---|
| `src/system/char/CharEyes.h` | **46 of 46** | copied verbatim from `../dc3-decomp`, whose `ObjPtr` is 20 bytes vs our 12 |
| `src/system/track/TrackDir.h` | **30**, drift non-uniform +0x62…+0xa0 | rb3-Wii |
| `src/system/bandobj/BandCharacter.h` | essentially the whole tail from ~0x4xx | rb3-Wii (`unk6c0 // 0x6c0` is really 0x750) |
| `src/system/bandobj/PatchDir.h` | 5 (`mLayers` 0x194 → real 0x1e4, …) | rb3-Wii |
| `src/system/char/ClipCollide.h` | **11 of 11** (`mReports` 0x2c → 0x28 … `mMode` 0x90 → 0x7c) | Wii-derived (`Object` 0x1c vs our 0x28), error accumulating; byte-identical bad comments in dc3-decomp |
| `src/system/char/CharClip.h` | 5, uniform **+8** | — |
| `src/system/char/CharUtl.h` (`ClipPredict`) | 3 | assumes `sizeof(Vector3)==12`; also wrong in dc3-decomp *and* rb3-Wii |
| `src/system/hamobj/HamLabel.h` | 2 (`mCountKeys` 0x168 → 0x238) | — |
| `src/band3/meta_band/SaveLoadManager.h` | 5, uniform **+4** | — |
| `src/system/synth/Synth.h` | 4 | — |
| `src/system/meta/SongPreview.h` | 2 | — |
| `src/system/flow/FlowWhile.h:39` | 1 | — |

**Fixed in place this lane:** `CharEyes.h` (46, via `--fix-header`),
`TrackDir.h` (30), `ClipCollide.h` (11), `SaveLoadManager.h` (5), plus the lines
each fixer moved in `Synth.h`, `SongPreview.h`, `BandSongMgr.h`,
`BandCharacter.h`, `MetaPanel.h`. All comment-only: each was A/B'd and measured
0 gained / 0 lost.

The compiler also **confirmed the corrected annotations**, not just the old wrong
ones: `sizeof(BandSongMgr)` = 0x1a4 exactly as predicted, with every rewritten
offset verbatim in the report; and `sizeof(ClipCollide)` = 0x80 with
`Hmx::Object` = 0x28, `ObjPtr<T>` = 12, `String` = 12 — which independently kills
the *other* half of the handed-down `ClipCollide::Save` diagnosis ("four members
are 4-byte in retail"). Our wrapper sizes were right all along; the address just
belongs to a different class (§6).

★ **Systematic pattern worth a scripted sweep**: `ObjPtr<T>` is **4 bytes on
rb3-Wii** and **12 on X360**; DC3's is **20**. Any header whose comments were
pasted from either sibling and that has an `ObjPtr` member early in the class has
*every* later comment wrong by the accumulated per-`ObjPtr` delta. That is a
mechanical scan (`--check-header` over the tree), not 12 hand investigations.

---

## 5. Corrections to received wisdom

* ★ **"`S=1` has 0 map mispairs" is FALSE.** The 14-target sample said 0; over the
  full 34, **6 (≈18 %) are likely or possible symbol mispairs**, and they are
  concentrated in *structurally generic skeletons* — `NewObject` factories, tiny
  single-base-ctor constructors, single-container-op forwarders. Those score
  99.9x % while calling a semantically unrelated function (`TrackDir::AddActiveWidget`
  → a `VectorRemove<RndDrawable*>` on `RndDir::mPolls`; `CharIKHand::NewObject` →
  `FlowRun::FlowRun`). **Verify the pairing before fixing a constant on top of it.**
  Two of this lane's four handed-down "layout defects" (`ClipCollide::Save`,
  `TrackDir::AddActiveWidget`) were mispairs, and `FlowWhile::SyncProperty` was a
  third.
* ★ **"`vector<T*>` is 8 bytes in this tree" did not reproduce.** Measured 12
  bytes in every instance checked: `Synth::mMics` (0x48…0x54),
  `PatchDir::mStickersLoading`, and the retail `BandSongMgr` vector at 0x148.
  `_Vector_base` here is `_M_start` / `_M_finish` / `_AllocProxy _M_end_of_storage`
  = 12 with an empty allocator. Do not assume 8; measure per instantiation.
* **`sizeof(Vector3)` is 16, not 12** (trailing SIMD pad, `src/system/math/Vec.h`).
  This one fact produced a false "stack shift" diagnosis, and it is mis-assumed in
  dc3-decomp's and rb3-Wii's headers too.
* **A `sizeof` baked into a constant is still a layout bug.** `CharClip::AllocSize`
  emits `sizeof(CharClip)` as an immediate; the 16-byte delta is the class being
  16 bytes off, not a magic number to edit. A "CONSTANT"-looking immediate must be
  traced to its source expression before it is classified.
* **`/stack-layout`'s `/Z7` CodeView variable labelling did not work** on either
  target tried (no source names emitted; one 16-byte slot mis-typed as `Vector3`),
  and the MCP `run_diff_inspect mode="stack-layout"` **timed out both times**.
  The CLI flag is `--stack-layout`, *not* `--mode stack-layout` — the MCP tool's
  documented spelling is rejected by the backing script. Do not treat this skill as
  load-bearing for the `S=1` tier; the retail-constructor reconstruction is what
  actually cracked all four layout targets.
* **`/d1reportSingleClassLayout` does emit for `SaveLoadManager`.**
  `docs/plans/slm-setstate-reconstruction.md` records "no output"; the TU is simply
  large enough that under fleet load it exceeds a 900 s timeout. Given ~50 min it
  reports the full layout (and proved the +4 stale comments).
* **Tool limitation found:** MSVC dumps a class only in the TU where its layout is
  first *completed*. For `FixedString` that happens in the **PCH**
  (`decomp_pch.cpp`), not in `Str.cpp`, so auto-resolution reports nothing.
  Retrying against `src/system/decomp_pch.cpp` works — worth adding as a fallback.
* **Runtime is the only real ergonomic cost.** The report recompiles the whole TU
  and prints nothing until it exits: ~10 min for `BandSongMgr.cpp`, ~50 min for
  `SaveLoadManager.cpp` under fleet load. Always run it in the background with a
  generous timeout; do not read a timeout as "the class is not reported".
* ★ **A resumed fixer commits *behind* your merge with no hint.** `laneAH-songmgr`
  reported final, was merged, then resumed and landed an 11-comment commit
  afterwards. The `git merge-base --is-ancestor` re-check immediately before the
  final measurement is what caught it — run it *after* the last agent
  notification, not before.

---

## 6. Hand-offs to single-owner channels (diagnosed + measured, NOT applied)

**Map (`scripts/target_symbol_map.json`)**

1. ★ **`0x826fc818` is `?SetMic@Synth@@QAAXPBVDataArray@@@Z`** (unit
   `default/system/synth/Synth`, 256 bytes, currently reads 0 % as an unnamed
   stub). Adding the entry was measured: **+1, 0 losses.** Free.
2. ★ **`0x822C0D48` is `?Save@BandSongPref@@UAAXAAVBinStream@@@Z`**, not
   `?Save@ClipCollide@@…`. Proven via the vtable: that VA sits at `0x8201EE3C`
   inside a vtable that also holds `?ClassName@BandSongPref@@` and
   `?SyncProperty@BandSongPref@@`; `BandSongPref` has exactly four `Symbol`
   members and `ASSERT_REVS(3,0)`, landing at 0x28/0x2c/0x30/0x34 — byte-exact.
   **`ClipCollide` does not exist in RB3-360 retail at all** (no `"ClipCollide"`
   string, no `.?AVClipCollide@@` RTTI, none of its handlers). The real
   `BandSongPref::Save` body is **already committed** on this branch; with the map
   +  splits repair it measures **+1 more**.
3. `?AddActiveWidget@TrackDir@@QAAXPAVTrackWidget@@@Z` is mapped to **two**
   addresses: `0x823710b8` (a different function — `RndDir` poll removal) and
   `0x827de630`, which is an 8-byte `except_data` blob, not a function.
4. `?SyncProperty@FlowWhile@@…` at `0x82342680` is the ICF fold of
   `BandHighlight`/`RndMatAnim` `SyncProperty`; `?Copy@FlowWhile@@$4…` at
   `0x82402F68` is a 56-byte real body doing two superclass `Copy` calls, not a
   `$4` thunk. The 99.967 % there is a phantom.
5. `??_GMetaPanel` / `MetaPanel::NewObject` want a `MetaPanel` layout that
   `MetaPanel::Handle` and `OnMsg` both contradict — likely mispaired.

**Splits (`config/45410914/splits.txt`)**

6. Move `.text start:0x822C0D48 end:0x822C0DD0` out of the `ClipCollide.cpp:`
   block (~line 7277) into `BandSongPref.cpp:` (~line 10549). ★ Move **only that
   pin** — deleting the whole bogus `ClipCollide.cpp` unit is net 0, because
   `?ClassName@ClipCollide@@` currently reads 100 % as a phantom (generic body,
   differs only by a relocation that normalized diff ignores) and removing it
   costs −1. ★ Editing the `.pdata` line alone does nothing: dtk **re-derives and
   back-fills `.pdata` from the pinned `.text`**, silently restoring the entry.

**Remaining `S=1` layout work, grouped so one fix closes several**

* `BandProfile` — **2 functions** (`HasSeenHint`, `GetGameplayOptionsFromUser`),
  both +4, both downstream of one missing 4-byte member before `unk88`.
* A 5-member **`vtordisp` / scalar-deleting-destructor cluster** —
  `HamListRibbon` (+428), `HamScrollSpeedIndicator` (+192), `MeterDisplay` (+244),
  `CharFeedback` (−76), `RndRibbon` (+48). Same *pattern*, five different base
  chains.
* Singletons: `PatchDir` (−4), `CharClip` (−16 via baked `sizeof`), `FlowSound`
  (+32), `AnimTask` (+36), `MetaMusicManager` (−4), `ProfileMgr` (−4),
  `LocalePanel::Entry` (+4), `BandCharacter::OnCamTeleport` (vtable slot, see §3).
* Genuine constants (only **2** of 34): `PatchSelectPanel::Load` (grid column
  count 3 → 4) and `SongParser::AudioTrackUsed` (`kAudioTypeFake` ordinal 5 → 15).
* `String::operator==`/`!=` `(const FixedString&)` — **ICF fold**, not layout:
  retail folds them with the `String` versions, so they read the buffer at
  `String::mStr` (+8). `src/system/utl/Str.cpp:168-176` already documents and
  applies this exact treatment for `ToLower`/`ToUpper`/`ReplaceAll`.
