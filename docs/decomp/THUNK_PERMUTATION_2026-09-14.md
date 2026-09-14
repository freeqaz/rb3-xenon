# The thunk-name permutation: 304 vtordisp thunk rows and 18 body rows repaired from retail VTABLE geometry, 116 thunk pins re-homed, and the "duplicate name" blocker was the other half of the cycle

**Lane W15-D (Fable escalation) · 2026-09-14 · branch `w15-d` off `main` `c90f107c` · worktree `~/tmp/wt-w15-d`**
**Ruler: `functionRelocDiffs=name_check` (graded, from `objdiff.json`; `report.json` provenance read, not assumed).**
**Baseline on this tree before any edit: 42,854 fns / 3,894,380 B / 38.008945 % / fuzzy 49.200146, `total_code` 10,245,956, `total_functions` 69,219.**

## 0. Result in one line

**Certified by `tools/ab_measure.py --revert cffe6f7a` (both legs settled, both at a `symbols.txt` split fixed point, leg B `renamer_patched=1822`): the commit is worth `Δmatched=+218  Δhonest=+218  Δcode_bytes=+4,056  Δcode%=+0.039589pp`, `units at 100% (mpn) 165 → 166`, `Δfuzzy −0.002786pp`, `total_code` unchanged (the tool prints the revert direction, −218/−4,056; leg A = with commit 43,072 / 38.048534 %, leg B = reverted 42,854 / 38.008945 %).** Pre-registered: thunks **+209 / +2,508** exactly, bodies 0 ± cascade; measured thunks came in at +210 (one extra from the `Copy@BandTrack` empty-fold alias added after pre-registration, predicted +1/+12 at that point), and the body/caller cascade added the remaining ~+1.5 kB net **including** the predicted downside — `?Save@BandLabel@@UAA` (100 B) fell from a false 100 to 4.0 %. `[control none] Δ−28 B` against `−4,056` on name_check: the wrong-callee-fix shape; the patch carries splits, so `ab_measure` correctly marks the alias-shape control NOT_APPLICABLE. `tools/icf_alias_finder.py --validate`: PASS, 0 contradicted.

## 1. What the two briefs got wrong, and why

W15-B declined the six `Save`-vs-`??_E` rows because for 5 of 6 "the implied correct name already occupies another map address". **Every one of those occupying rows is itself a misnamed thunk** — `?SyncProperty@BandTrack@@$4` at `0x8234ebc8` branches to `0x826c3888`, the ICF survivor of every empty `blr` body in the binary (W14-B's control); `?SetType@RndMultiMeshProxy@@$4` at `0x82481390` branches to the class's own deleting destructor; `??_EBandTrack@@$4` sits at `0x827f4460`, inside UILabel's vtable run, branching to `?Highlight@UILabel`. The collision was not a blocker; it was the signature of a permutation, and the names on the far side were in play too. Adjudicating by destination *name* — both W14-B's and W15-B's method — cannot resolve a cycle whose every node is wrong.

The instrument that is independent of every map name is the **vtable**. Retail `.rdata` carries 2,220 `??_R4` Complete Object Locators (my indexer found exactly 2,220 — the count CLAUDE.md records — a free control); each COL's `offset` field identifies the subobject, and the run of `.text` pointers after it is the vtable. `scripts/harvest/class_layout_report.py --all-classes --json` gives, for the same class and subobject offset, which method each slot holds and which slots are vtordisp thunks. Joining the two by `(class, subobject offset, slot)` names every thunk in the binary without reading a single map name; the expected spelling is then taken **verbatim from the symbol our compiled objs define** (so a rename can never introduce an unpairable name by construction). 280 classes needed a layout; 269 came from ~90 `--all-classes` compiles (13 s each), 11 have no TU (`XboxSession` family, `Label3d`, `CharTransCopy`, `BandSong`).

### Controls (an instrument that cannot fail is not one)

| control | result |
|---|---|
| retail vtable slot count vs compiler slot count | **721 vtables agree**; 6 disagree (`TourSavable`, `PracticePanel` 20 vs 21; `Server`, `PlatformMgr@Callback`, `DxTexRenderer` ×2) — a real class-layout divergence vein, **not chased** |
| W15-B's 788 name-agreements | 733 AGREE, 9 proven folds, 36 undecidable (no layout), **10 WRONG** (§3) |
| planted sabotage: a real method of the right class on the wrong address (`?SetType@Waypoint@@$4` on the dtor-thunk address) | flips `AGREE → WRONG` with own-slot `0x823dc040` (the true SetType thunk); restore → `AGREE` |
| my own first census | scored the dtor thunks as "no expectation" because my regex assumed `Method@Class` and `??_EClass` has no `@`; the sabotage control came back `NO_EXPECTATION` instead of `WRONG` and **that is how the bug was found** |
| my own first apply | merged blocks and cut a block twice from a stale lookup, producing overlapping `.text` blocks in 52 units; dtk refused (`overlaps with previous split`); I re-implemented jeff's `.pdata` derivation (`split.rs:1049`) as a pre-flight check — HEAD 0 overlaps, my candidate 52, fixed candidate 0 |

⚠ W15-B's `probe.py` decodes the X360 `.pdata` second word as bits 2..23; the packed format is `PrologLen:8 | FunctionLength:22 (instructions) | flags`, i.e. `((f>>8)&0x3FFFFF)*4`. Its §3 conclusions survive because it fell back to next-entry distance, but the decoder is wrong and I hit it: the first extent check read all 116 moved thunks as "inside a function" (lengths of 2,048 and 27,136 B), the correct decode reads **0 inside, 116 standalone**.

## 2. Item 1 — the six rows and their partners

Compiler order of the `Hmx::Object` subobject vtable (identical for every class with the `Object` vbase): `{dtor}, RefOwner, Replace, IsDirPtr, ClassName, SetType, Handle, SyncProperty, Save, Copy, Load, PreSave, PostSave, Print, Export, SetTypeDef, SetName, DataDir, PreLoad, PostLoad, FindPathName`.

| addr | W14-B/W15-B name | retail vtable slot (class, subobject off, slot) → compiler says | new name | action | pre-reg Δ |
|---|---|---|---|---|---|
| `0x8234ebb8` | `?Save@BandTrack@@$4PPPPPPPM@A@AAXAAVBinStrea` | ('BandTrack', 'Object', 0, 'BandTrack::{dtor}') | `??_EBandTrack@@$4PPPPPPPM@A@AAPAXI@Z` | rename | +1 fn/+12 B |
| `0x82352810` | `?Copy@BandTrack@@$4PPPPPPPM@A@AAXPBVObject@H` | ('BandTrack', 'Object', 7, 'BandTrack::SyncProperty') | `?SyncProperty@BandTrack@@$4PPPPPPPM@A@AA_NAA` | rename | +1 fn/+12 B |
| `0x8234ebc8` | `?SyncProperty@BandTrack@@$4PPPPPPPM@A@AA_NAA` | ('BandDirector', 'Object', 2, 'BandDirector::Replace') | `?Copy@BandTrack@@$4PPPPPPPM@A@AAXPBVObject@H` | rename + alias group (10 folded) | +1 fn/+12 B |
| `0x822ee730` | `?Load@BandTrack@@$4PPPPPPPM@A@AAXAAVBinStrea` | ('GemTrackDir', 'Object', 19, 'GemTrackDir::PostLoad') | `?PostLoad@GemTrackDir@@$4PPPPPPPM@A@AAXAAVBi` | rename | +1 fn/+12 B |
| `0x822ee710` | `?PostLoad@GemTrackDir@@$4PPPPPPPM@A@AAXAAVBi` | ('GemTrackDir', 'Object', 8, 'GemTrackDir::Save') | `?Save@GemTrackDir@@$4PPPPPPPM@A@AAXAAVBinStr` | rename | 0 |
| `0x823dd010` | `?Highlight@Waypoint@@$4PPPPPPPM@A@AAXXZ` | ('Waypoint', 'Object', 10, 'Waypoint::Load') | `?Load@Waypoint@@$4PPPPPPPM@A@AAXAAVBinStream` | rename | +1 fn/+12 B |
| `0x822cb008` | `?Load@Waypoint@@$4PPPPPPPM@A@AAXAAVBinStream` | ('BandLeadMeter', 'Object', 18, 'BandLeadMeter::PreLoad') | `?PreLoad@BandLeadMeter@@$4PPPPPPPM@A@AAXAAVB` | rename + re-pin Waypoint.cpp→home | +1 fn/+12 B |
| `0x82481518` | `?Save@RndMultiMeshProxy@@$4PPPPPPPM@A@AAXAAV` | ('RndMultiMeshProxy', 'Object', 5, 'RndMultiMeshProxy::SetType') | `?SetType@RndMultiMeshProxy@@$4PPPPPPPM@A@AAX` | rename | +1 fn/+12 B |
| `0x82481390` | `?SetType@RndMultiMeshProxy@@$4PPPPPPPM@A@AAX` | ('RndMultiMeshProxy', 'Object', 0, 'RndMultiMeshProxy::{dtor}') | `??_ERndMultiMeshProxy@@$4PPPPPPPM@A@AAPAXI@Z` | rename | +1 fn/+12 B |
| `0x822bac80` | `?Save@CrowdMeterIcon@@$4PPPPPPPM@A@AAXAAVBin` | ('CrowdMeterIcon', 'Object', 19, 'CrowdMeterIcon::PostLoad') | `?PostLoad@CrowdMeterIcon@@$4PPPPPPPM@A@AAXAA` | rename | +1 fn/+12 B |
| `0x822bae88` | `?PostLoad@CrowdMeterIcon@@$4PPPPPPPM@A@AAXAA` | ('CrowdMeterIcon', 'Object', 8, 'CrowdMeterIcon::Save') | `?Save@CrowdMeterIcon@@$4PPPPPPPM@A@AAXAAVBin` | rename | 0 |
| `0x822bacb0` | `?PreLoad@CrowdMeterIcon@@$4PPPPPPPM@A@AAXAAV` | ('CrowdMeterIcon', 'Object', 9, 'CrowdMeterIcon::Copy') | `?Copy@CrowdMeterIcon@@$4PPPPPPPM@A@AAXPBVObj` | rename | +1 fn/+12 B |
| `0x822bae58` | `?Copy@CrowdMeterIcon@@$4PPPPPPPM@A@AAXPBVObj` | ('CrowdMeterIcon', 'Object', 18, 'CrowdMeterIcon::PreLoad') | `?PreLoad@CrowdMeterIcon@@$4PPPPPPPM@A@AAXAAV` | rename | 0 |
| `0x827f4460` | `??_EBandTrack@@$4PPPPPPPM@A@AAPAXI@Z` | ('UILabel', 'RndHighlightable', 0, 'UILabel::Highlight') | `?Highlight@UILabel@@$4PPPPPPPM@A@AAXXZ` | rename + re-pin VocalTrackDir.cpp→home | +1 fn/+12 B |


Mechanisms: every in-place thunk rename below 100 pays **+1 fn / +12 B** (the row's `mpn` is also 98.333, so it counts on both rulers); rows already at 100 are neutral (their name set is a permutation the same obj defines); `0x8234ebc8` needed the **alias**, not a rename — it is one address filling BandTrack [8,9,10], RndMultiMeshProxy [8,9,10], Waypoint's `Highlight` slot, BandDirector's `Replace`, BandSong's `Copy`, OutfitConfig's `PostSave`, WorldInstance's `PreSave` (11 spellings, one branch destination, the MSVC `/OPT:ICF` condition exactly). Its target is the empty-body survivor, so reaching 100 additionally required adding `?Copy@BandTrack@@UAA…` to the existing `0x826c3888` group — evidence is the vtable slot, not a byte comparison (vacuous for a `blr`). `0x822cb008`'s partner `0x822ca588` is an **r4-shape** thunk (`lwz r11,-4(r4); subf r4,r11,r4`) — a hidden-struct-return method's thunk carrying a `PreLoad` name, which is impossible; W15-B's 3-word r3 scan never saw the 504 r4 thunks at all.

## 3. Item 2 — the 236 disagreements, partitioned

| W15-B bucket | n | verdicts (this lane) |
|---|---:|---|
| AGREE | 788 | 733 **AGREE (map correct)**; 36 **UNDECIDABLE (no compiler layout / vtable key)**; 9 **PROVEN FOLD → alias**; 6 **RENAMED + RE-PINNED**; 4 **RENAMED** |
| SAME_CLASS_dtor_family | 155 | 134 **AGREE (map correct)**; 17 **UNDECIDABLE (no compiler layout / vtable key)**; 2 **PROVEN FOLD → alias**; 1 **UNDECIDABLE (unparsable))**; 1 **RENAMED** |
| SAME_CLASS_diff_method | 47 | 40 **RENAMED**; 4 **UNDECIDABLE (no compiler layout / vtable key)**; 3 **WRONG, not acted (holder conflict / no unit)** |
| DIFF_CLASS | 34 | 13 **RENAMED**; 8 **RENAMED + RE-PINNED**; 5 **AGREE (map correct)**; 3 **WRONG, not acted (holder conflict / no unit)**; 3 **UNDECIDABLE (no compiler layout / vtable key)**; 2 **PROVEN FOLD → alias** |

Whole two-shape census (1,806 thunks: 1,302 r3 + 504 r4):

| verdict | n |
|---|---:|
| AGREE (map correct) | 1273 |
| RENAMED | 188 |
| UNDECIDABLE (no compiler layout / vtable key) | 181 |
| RENAMED + RE-PINNED | 116 |
| WRONG, not acted (holder conflict / no unit) | 29 |
| PROVEN FOLD → alias | 15 |
| UNDECIDABLE (method not vtordisp in own vtable)) | 2 |
| UNDECIDABLE (unparsable)) | 1 |
| UNDECIDABLE (no layout)) | 1 |

Acted thunks by shape: {'r3': 260, 'r4': 44}


### The 155 `??_E$4 → ??_G` rows are NOT ICF and NOT defects

Our compiled objs carry **10,090** symbol records where `??_EClass@@UAAPAXI@Z` is a **weak external whose default is `??_GClass@@UAAPAXI@Z`** (COFF storage class 105); exactly **one** real `??_E` body exists in the whole build (`??_EString@@UAAPAXI@Z`). Every `??_E$4` thunk we emit relocates against the `??_E` weak symbol (363/363), and the linker resolves it to `??_G`. So a retail `??_E$4` thunk branching to a `??_G`-named body is the compiler's own output; 190 such rows already score 100 with no alias group. W15-B's "probably ICF" was the right verdict for the wrong reason — nothing folded, nothing is wrong.

### Ten rows W15-B scored AGREE are misnamed thunk+body PAIRS

`?Load@HamLabel@@$4 → ?Load@HamLabel@@UAA` agrees with itself and both are `BandLabel::Load` by vtable; `HamLabel`, `PostProcer`, `RndSpline` have **no retail vtable at all** — they are Dance Central names transferred onto RB3 addresses. `HamLabel.cpp` (DC3 source, compiled here) is pinned onto retail `BandLabel::Load`/`Save` and matched at **100 %** because DC3's HamLabel *is* RB3's BandLabel renamed — the circular-pin hazard in its purest form. The coupled fix (rename + move `HamLabel.cpp`'s pins to `BandLabel.cpp`) is a handoff; I renamed `?Save@BandLabel@@UAA` in place (BandLabel.obj defines it) and it now reads **4.0 %** — our BandLabel::Save is *not* what HamLabel::Save matched, a real divergence that the wrong name had hidden at 100.

### 75 wrong-named thunk rows read fuzzy 100 today

objdiff pairs our `Copy` thunk with retail's `PreLoad` thunk by name, the 3-word bodies are identical, and the branch target is **placeholder-forgiven** because the destination body is unnamed. A wrong map name scored as a perfect match, 75 times (plus 7 bodies). This is CLAUDE.md's arg-blindness class, and it is why W14-B's "the six sit at 98.333, the correct ones at 100" partition holds only when the destination is named.

### Folds, proven and refused

26 alias groups installed, 47 folded spellings, evidence tier **VT1**: N retail vtable slots (class, subobject offset, slot) hold one address, the compiler assigns those slots to N methods, every member's thunk is the canonical body whose only relocation is the branch, and all branch to one destination. Two spellings were **withheld** because the validator found them with a second home in the map (`?PreSave@WorldInstance@@$4` at `0x824eb260`, `?GetLocalBandUser@RemoteBandUser@@$4` at `0x8268ed70`) — those two rows are suspects, not folds, until adjudicated. `tools/icf_alias_finder.py --validate`: **PASS — 1384 map-consistent, 243 tolerated, 0 contradicted, 1629 total**.

### 14 acted thunk rows still below 100 — each exposes a further wrong BODY name

`?Save@WorldDir@@$4` branches to a body the map calls `__uninitialized_copy<Grammar>`; `?Copy@EndingBonus@@$4` to `??2SpotlightDrawer`; `?SyncProperty@RndParticleSysAnim@@$4` to `operator<<(TextStream&, Color)`; `?PostLoad@UIButton@@$4` to `??1BinStreamRev`; `?Save@EndingBonus@@$4` to a body carrying a `$4` thunk name. Three are plausible genuine folds of trivial bodies (`IsLocal`→`IsDirPtr`, `CanSaveData`→`GetCrowdMeter`, `UserName`→`ContentPattern`: `li r3,K; blr`). Two read **50 %** (`?Handle@SynthEmitter@@$4`, `?PreLoad@UILabelDir@@$4`): our thunk shape differs from retail's (r3 vs r4 adjust), i.e. a **signature divergence** in our source — real-bug candidates. None chased.

## 4. Measured

**Certified by `tools/ab_measure.py --revert cffe6f7a` (both legs settled, both at a `symbols.txt` split fixed point, leg B `renamer_patched=1822`): the commit is worth `Δmatched=+218  Δhonest=+218  Δcode_bytes=+4,056  Δcode%=+0.039589pp`, `units at 100% (mpn) 165 → 166`, `Δfuzzy −0.002786pp`, `total_code` unchanged (the tool prints the revert direction, −218/−4,056; leg A = with commit 43,072 / 38.048534 %, leg B = reverted 42,854 / 38.008945 %).** Pre-registered: thunks **+209 / +2,508** exactly, bodies 0 ± cascade; measured thunks came in at +210 (one extra from the `Copy@BandTrack` empty-fold alias added after pre-registration, predicted +1/+12 at that point), and the body/caller cascade added the remaining ~+1.5 kB net **including** the predicted downside — `?Save@BandLabel@@UAA` (100 B) fell from a false 100 to 4.0 %. `[control none] Δ−28 B` against `−4,056` on name_check: the wrong-callee-fix shape; the patch carries splits, so `ab_measure` correctly marks the alias-shape control NOT_APPLICABLE. `tools/icf_alias_finder.py --validate`: PASS, 0 contradicted.

Per-unit attribution (leg A − leg B, from the archived reports):

| unit | Δfn | ΔB |
|---|---:|---:|

0 units moved in total; negative rows: 
Unit reaching 100 % (mpn): []


## 5. Handoffs

### Bodies proven misnamed by vtable geometry whose pin must move before they can pair

| addr | old name | vtable-proven name | unit today | cur fuzzy | acted |
|---|---|---|---|---|---|
| `0x822cbc10` | `?SetType@StreakMeter@@UAAXVSymbol@@@Z` | `?PostLoad@BandStarDisplay@@UAAXAAVBinStr` | StreakMeter.cpp | 0.0 | renamed |
| `0x8231e660` | `?PostLoad@StarDisplay@@UAAXAAVBinStream@` | `?PostLoad@ReviewDisplay@@UAAXAAVBinStrea` | StarDisplay.cpp | 99.82353 | renamed |
| `0x8232a148` | `?SetType@CharUpperTwist@@UAAXVSymbol@@@Z` | `?SetType@DialogDisplay@@UAAXVSymbol@@@Z` | CharUpperTwist.cpp | 99.93671 | renamed |
| `0x82340740` | `?Save@HamLabel@@UAAXAAVBinStream@@@Z` | `?Save@BandLabel@@UAAXAAVBinStream@@@Z` | BandLabel.cpp | 100.0 | renamed |
| `0x82344238` | `?Save@BandHighlight@@UAAXAAVBinStream@@@` | `?PreLoad@BandButton@@UAAXAAVBinStream@@@` | BandHighlight.cpp | 0.14035088 | renamed |
| `0x823c8fc0` | `??_ERndDrawable@@UAAPAXI@Z` | `??_GCharTransDraw@@UAAPAXI@Z` | Screenshot.cpp | 0.0 | renamed |
| `0x823ce6c0` | `?SetType@RndPropAnim@@UAAXVSymbol@@@Z` | `?SetType@CharNeckTwist@@UAAXVSymbol@@@Z` | MetaMusic.cpp | 99.93671 | renamed |
| `0x82406178` | `?SyncProperty@RndSpline@@UAA_NAAVDataNod` | `?PreLoad@RndDir@@UAAXAAVBinStream@@@Z` | Anim.cpp | 0.0 | renamed |
| `0x82429be8` | `??_ECharBlendBone@@UAAPAXI@Z` | `??_GRndPropAnim@@UAAPAXI@Z` | None | 0.0 | renamed |
| `0x8247df48` | `?SetType@RndSpline@@UAAXVSymbol@@@Z` | `?SetType@RndGenerator@@UAAXVSymbol@@@Z` | Line.cpp | 99.93671 | renamed |
| `0x824816a8` | `??_GRndScreenMask@@UAAPAXI@Z` | `??_GRndMultiMeshProxy@@UAAPAXI@Z` | ScreenMask.cpp | 99.45 | renamed |
| `0x82482350` | `??_GRndRibbon@@UAAPAXI@Z` | `??_GRndScreenMask@@UAAPAXI@Z` | Ribbon.cpp | 99.7 | renamed |
| `0x824d0898` | `??_GRndShockwave@@UAAPAXI@Z` | `??_GWorldDir@@UAAPAXI@Z` | Shockwave.cpp | 99.75 | renamed |
| `0x824e9ed0` | `??_GRemoteBandUser@@UAAPAXI@Z` | `??_GSpotlightEnder@@UAAPAXI@Z` | BandUser.cpp | 99.45 | renamed |
| `0x82605ca8` | `?SyncProperty@RndParticleSysAnim@@UAA_NA` | `?SyncProperty@BandStorePanel@@UAA_NAAVDa` | TrackWatcherImpl.cpp | 99.833336 | renamed |
| `0x8268c8d0` | `??_GRemoteUser@@UAAPAXI@Z` | `??_GRemoteBandUser@@UAAPAXI@Z` | BandUser.cpp | 100.0 | renamed |
| `0x8273e568` | `?SetType@DxMultiMesh@@UAAXVSymbol@@@Z` | `?SetType@DxMovie@@UAAXVSymbol@@@Z` | system/rnddx9/CubeTex.cpp | 99.93671 | renamed |
| `0x8273f8f0` | `?SetType@DxMovie@@UAAXVSymbol@@@Z` | `?SetType@DxMultiMesh@@UAAXVSymbol@@@Z` | system/rnddx9/Movie.cpp | 99.93671 | renamed |
| `0x8232ac50` | `??_GCharUpperTwist@@MAAPAXI@Z` | `??_GDialogDisplay@@UAAPAXI@Z` | CharUpperTwist.cpp | 100.0 | kept (at 100; re-pin first) |
| `0x82340670` | `?Load@HamLabel@@UAAXAAVBinStream@@@Z` | `?Load@BandLabel@@UAAXAAVBinStream@@@Z` | HamLabel.cpp | 100.0 | kept (at 100; re-pin first) |
| `0x824e9da8` | `?Save@PostProcer@@UAAXAAVBinStream@@@Z` | `?Save@SpotlightEnder@@UAAXAAVBinStream@@` | PostProcer.cpp | 100.0 | kept (at 100; re-pin first) |
| `0x825748e8` | `??_GHamIKEffector@@UAAPAXI@Z` | `??_GSetlistToStorePanel@@UAAPAXI@Z` | HamIKEffector.cpp | 100.0 | kept (at 100; re-pin first) |
| `0x8252a598` | `?ContentPattern@Callback@ContentMgr@@UAA` | `?UserName@NullLocalBandUser@@UBAPBDXZ` | AccomplishmentPanel.cpp | 100.0 | kept (at 100; re-pin first) |

### Thunk rows not acted (name-holder conflict or address outside every pin)

- `0x822c46c0` `?Load@BandIKEffector@@$4PPPPPPPM@A@AAXAA` → `??_EBandIKEffector@@$4PPPPPPPM@A@AAPAXI@Z`: holder 0x82445f88 skipped/not moving
- `0x822c57b8` `-` → `?Load@BandIKEffector@@$4PPPPPPPM@A@AAXAAVBin`: holder 0x822c46c0 skipped/not moving
- `0x823af2a8` `?Load@CharPollGroup@@$4PPPPPPPM@A@AAXAAV` → `?Load@CharWeightable@@$4PPPPPPPM@A@AAXAAVBin`: holder 0x823aee10 skipped/not moving
- `0x823b08b0` `-` → `?Load@CharPollGroup@@$4PPPPPPPM@A@AAXAAVBinS`: holder 0x823af2a8 skipped/not moving
- `0x82445f78` `-` → `?Load@RndTexRenderer@@$4PPPPPPPM@A@AAXAAVBin`: holder 0x827385b8 skipped/not moving
- `0x8247b7f0` `?Load@RndLine@@$4PPPPPPPM@A@AAXAAVBinStr` → `?Save@RndLine@@$4PPPPPPPM@A@AAXAAVBinStream@`: holder 0x824048b8 skipped/not moving
- `0x8247c3e0` `?Load@RndSpline@@$4PPPPPPPM@A@AAXAAVBinS` → `?Load@RndLine@@$4PPPPPPPM@A@AAXAAVBinStream@`: holder 0x8247b7f0 skipped/not moving
- `0x82697528` `-` → `?Handle@GamePanel@@$4PPPPPPPM@A@AA?AVDataNod`: address not in any unit
- `0x82812018` `-` → `?SetType@UILabelDir@@$4PPPPPPPM@A@AAXVSymbol`: address not in any unit
- `0x82812038` `-` → `?Save@UILabelDir@@$4PPPPPPPM@A@AAXAAVBinStre`: address not in any unit
- `0x82812048` `-` → `?Handle@UILabelDir@@$4PPPPPPPM@A@AA?AVDataNo`: address not in any unit
- `0x82812058` `-` → `?SyncProperty@UILabelDir@@$4PPPPPPPM@A@AA_NA`: address not in any unit
- `0x82812068` `-` → `?PostLoad@UILabelDir@@$4PPPPPPPM@A@AAXAAVBin`: address not in any unit

### 181 thunks with no expectation (class has no compiler layout or its vtable key could not be matched), by class

Label3d (11), EventTrigger (10), CharKeyHandMidi (9), CharClipGroup (9), PatchRenderer (8), BandWardrobe (8), CharTransCopy (8), UIFontImporter (8), UITrigger (8), MsgSource (7), MidiParser (6), PassiveMessagesPanel (5), JoinInvitePanel (4), RKTrainerPanel (4), PracticePanel (4), CampaignCareerLeaderboardPanel (3), AuditionSessionPanel (3), BandPreloadPanel (3), BandMatchmaker (3), BandSong (2), TourSavable (2), NetSession (2), SessionSearcher (2), Server (2), XboxServer (2), XboxSession (2), RockCentral (2), ProfileMgr (2), SaveLoadManager (2), ClosetMgr (2), UIEventMgr (2), .?AVOpenGateData@?A0x5b3730ba@@ (2), InputMgr (2), BandMachineMgr (2), Matchmaker (2), GameMode (2), StorePreviewMgr (2), TourDescPanel (2), TourProgress (1), CharBonesMeshes (1)


### Class-layout divergences from the slot-count control
`TourSavable@Object@` and `PracticePanel@Object@` (compiler 20 vs retail 21), `Server@Server@` (20 vs 19), `PlatformMgr@Callback@` (14 vs 3), `DxTexRenderer@RndTexRenderer@` (14 vs 9 and 14 vs 4). Compiler-vs-retail vtable slot counts are name-free evidence of a header divergence; a struct/vtable lane should open these.

### Two-homed spellings
`?PreSave@WorldInstance@@$4` (`0x824eb260`) and `?GetLocalBandUser@RemoteBandUser@@$4` (`0x8268ed70`) are named in the map at addresses the vtable does not confirm; the vtable puts those methods on the folded empty thunk `0x8234ebc8` and on `0x8268b8a8` respectively.

## 6. What I did NOT do
- Did not re-home any **body** pin (15 body renames below 100 are unpaired until a splits lane moves them; the 5 at 100 were left named as-is to keep their pairing, listed above).
- Did not adjudicate the 14 below-100 destination names, the two 50 % shape mismatches, or the 6 slot-count divergences.
- Did not touch `src/`; no native gate needed and none run.
- Did not name any of the 189 unnamed destination bodies (W14-B's body-first rule stands).
- Did not resolve the 181 no-expectation thunks (no compiler layout: 11 classes without a TU, plus vtable keys the layout JSON does not expose for some multiply-inherited classes).

## 7. Instruments (all under `~/tmp/w15d/`, reproducible)
`vt.py` (RTTI vtable index), `coffsyms.py` (COFF symbols + thunk relocation targets), `layouts.py` (batched `--all-classes` layouts), `census.py` (the join, with own-slot refutation and body expectations; `W15D_SABOTAGE` env for the control), `plan.py`/`apply.py` (permutation solver with injectivity-delta, pairing categories, block moves), `pdcheck.py` (jeff's `.pdata` derivation as a pre-flight). Build logs `~/tmp/rb3_build_w15-d*.log`; A/B log `~/tmp/w15d/ab_pick.log`.
