# laneBL — converting laneBD's located spans into landed matches (2026-07-29)

Mission: take the **42 HIGH-confidence pin-ready spans** laneBD left in
`scripts/harvest/tu_locate/located_spans.json` (504 currently-unscoreable
functions) and turn them into measured, zero-loss matches; then go back to
locating the **65 TUs** laneBD could not place.

Predecessor: `docs/plans/wii-oracle-tu-location-2026-07-29.md` (laneBD — located
76 of 141, wired 3 for a coordinator-verified **+71**). Everything in §1 of that
doc about the *seam* — that these TUs are not missing but swallowed by
over-broad neighbouring pins — is confirmed here at scale.

Baseline for every measurement below, taken in a fully-built worktree at main
`7cfaa3d9`: `matched_functions` **39,736**, strict-100 by name **39,523**,
`fuzzy_match_percent` 39.68776.

---

## 0. TL;DR

* **31 TUs/carves pinned, wired, ported and measured: +599 gains / −38 losses
  (net +561)**, all seven lanes complete. **All but one of the losses is the retirement of a *false* 100 %** — a
  `target_symbol_map.json` entry bound to a target VA that is not that function,
  scoring only as a shape (§4). The single genuine cost found so far is
  `?MaxPhraseScore@VocalPart@@QBAMXZ`, an 8-byte ICF fold at `0x826EE518` that
  travelled with `BandPerformer`'s span and does not re-pair. Twelve of them are
  pure ADDs into unclaimed space and are structurally incapable of a loss; those
  alone are **+174**.

  For scale: laneBD's proof-of-concept was +71 from 3 TUs, and it priced the
  whole 42-span worklist's headroom at 504 currently-unscoreable functions. This
  wave has realised most of that headroom and then exceeded it, because the spans
  under-covered (§3) — `ChordShapeGenerator` alone came to **+55** across two
  carves, against a worklist estimate of 77 for a row laneBD had demoted to LOW.
* **Seven sub-lanes**, one buildable worktree each (`scripts/setup_worktree.sh`),
  each running the full recipe per TU: confirm identity from the retail PE →
  refine the span → carve or add in `splits.txt` → wire in `objects.json` → port
  the rb3-Wii/DC3 source → recover `fn_<VA>` ↔ mangled pairings with
  `size_order_automap.py` → double full build → unit-agnostic A/B.
* **Four names were proved to be PHANTOMS** (three donors + `MoveGraph`) — classes that do not exist in RB3
  retail at all, whose entire pin was foreign code. laneBD found the first
  (`SongDifficultyDisplay`); this wave found two more (`FlowEventListener`,
  `MoveMgr`) and, crucially, **the predictor that finds them is already in
  laneBD's data**.
* **The confidence ladder in `located_spans.json` has a systematic reduction
  bug**, which suppressed the single largest item in the worklist. Fixing the
  reduction (attribute string edges to individual `.pdata` functions, not to a
  `[min,max)` cluster) promoted `ChordShapeGenerator` from LOW to actionable and
  located **four TUs laneBD never placed at all**.
* **The LOW bucket's headline 532 functions is largely not actionable** — 265 of
  them are the controller family, and three of those five TUs have *zero*
  selective literals and *zero* unclaimed space. That is now measured, not
  suspected, and it should stop anyone funding a third string/RTTI channel there.
* **`located_spans.json`'s spans under-cover, and its `snapped` bounds can
  over-extend.** Every single TU in this wave needed its span corrected. The
  corrections are systematic and are written up in §3 as rules.

---

## 1. The ledger

All deltas are strict-100 **by-name multiset**, double full build, `report.cache`
removed before every read, measured in the lane's own worktree against the
baseline above. Losses are itemised in §4.

| lane | TU | span (as landed) | donor | gains | losses |
|---|---|---|---|--:|--:|
| A | `band3/tour/TourDesc` | `0x82367D60..0x82368FB8` | `system/obj/DataFunc.cpp` | **+58** | −1 |
| A | `band3/tour/TourProperty` + `TourWeightManager` | (2 carves) | `system/obj/DataFunc.cpp` | **+14** | −2 |
| B | `band3/meta_band/FaceHairProvider` | `0x8266F380..0x8266F730` | `Leaderboard.cpp` | **+6** | 0 |
| B | `band3/meta_band/CharProvider` | `0x82666648..0x82667FE0` (see §3.0/§4.2) | `system/rndobj/PropKeys.cpp` | **+26** | −11 |
| B | `band3/meta_band/BandStoreOffer` | `0x8266E4E8..0x8266EC88` | `Leaderboard.cpp` | **+9** | −1 |
| **C** | `band3/meta_band/LockStepMgr` | `0x825AB138..0x825AB3B8` + `0x825AB408..0x825ACA70` (**ADD**) | — | **+65** | 0 |
| C | `band3/meta_band/UGCPurchasePanel` | `0x8263E6A0..0x8263F2F0` + `0x8263F318..0x8263F910` (**ADD**) | — | **+25** | 0 |
| C | `system/beatmatch/SlotChannelMapping` | `0x82793F8C..0x82794730` (**ADD**) | — | **+23** | 0 |
| C | `band3/game/HitTracker` | `0x826E34E0..0x826E3808` (**ADD**) | — | **+12** | 0 |
| C | `system/utl/LogFile` | `0x827CBEF8..0x827CC150` (**ADD**) | — | **+7** | 0 |
| C | `band3/meta_band/Asset` | `0x825EF708..0x825EFB34` (**ADD**) | — | **+7** | 0 |
| D | `band3/game/RealGuitarGemPlayer` | `0x826EBE10..0x826EC860` (2 blocks) | `VocalPlayer.cpp` | **+30** | 0 |
| D | `band3/game/BandPerformer` | `0x826ED658..0x826EDFE4`+ | **PHANTOM** `system/flow/FlowEventListener.cpp` | **+24** | 0 |
| D | `band3/game/PracticeSectionProvider` | reassembled from 3 pins + 3 gaps | `PlayerTrackConfigList`/`Tracker`/`GemTrack` | **+19** | 0 |
| D | `band3/game/CrowdRating` | `0x826EE8B8..0x826EEB08` | `VocalPlayer.cpp` | **+4** | 0 |
| E | `system/ui/UIGridProvider` | `0x82817260..0x82817A64` | `system/ui/UIPicture.cpp` | **+19** | −1 |
| E | `system/bandobj/BandButton` | `0x823438C8..0x82345034` | `BandHighlight.cpp` | **+31** | −8 |
| E | `system/bandobj/UnisonIcon` | `0x822D2D58..0x822D38B0` | **PHANTOM** `MoveMgr.cpp` | **+24** | −1 |
| F | `system/bandobj/ChordShapeGenerator` | 4 unclaimed runs (**ADD**) | — | **+13** | 0 |
| G | `band3/meta_band/SongSetlistProvider` | `0x825BC6F8..0x825BC8FC` (**ADD**) | — | **+3** | 0 |
| G | `system/dsp/SndAnalysis` | `0x82B816F0..0x82B81DD8` (**ADD**) | — | **+4** | 0 |
| D | `band3/game/KeysFx` | `0x826F3E98..0x826F47B0` (located 708 B → true **2,328 B**) | `GuitarFx.cpp` | **+5** | 0 |
| F | `ChordShapeGenerator` (2nd carve) | out of the foreign `Mesh.cpp` pin | `Mesh.cpp` | **+42** | 0 |
| G | `band3/meta_band/InputMgr` | `0x825B0518..0x825B22A8` (**4 donors**) | `AppInlineHelp`/`CalibrationPanel`/`StreamRenderer`/`OvershellPanel` | **+34** | 0 |
| C | `band3/bandtrack/DrumTrackWatcherImpl` (bonus) | `0x827800B0..0x827808C0` (**ADD**) | — | **+5** | 0 |
| A | `band3/tour/TourPerformerLocal` + `TourPerformerRemote` | (2 carves) | `system/obj/DataFunc.cpp` | **+23** | −2 |
| B | `band3/game/BandUserMgr` | `0x826826A8..0x82684E40` (+ unclaimed head/tail) | `system/rndobj/PropKeys.cpp` | **+38** | 0 |
| A | `band3/tour/TourGameRules` + `TourGameModifier` | `0x82365E68..0x823660F8`, `0x823699B4..0x82369B28` (**ADD**) | — | **+10** | 0 |
| E | `system/bandobj/DialogDisplay` | `0x82329B20..0x82329FD8` (mostly unclaimed) | `CharUpperTwist.cpp` | **+5** | 0 |
| | **31 TUs / carves** | | | **+599** | **−38** |

Per lane: A **+105/−5**, B **+79/−12**, C **+144/−0**, D **+90/−8**, E **+80/−10**,
F **+60/−3**, G **+41/−0** — **net +561**. **All seven lanes complete.** Composition re-verified: **28 new units, zero cross-branch
`.text` overlaps**, and every branch passes `overlap_check.py` individually.

★★ **The entire `DataFunc.cpp` block `0x82366274..0x8236955C` (0x32E8) is now
drained — it was 100 % foreign, five `band3/tour` TUs' worth.** `DataFunc.cpp`
retains 92 other `.text` blocks, so no empty-unit trap. This is the single
strongest confirmation of laneBD's seam thesis in the wave: an over-broad pin did
not merely *contain* a foreign TU, it contained nothing else at all.

`BandUserMgr` is worth singling out: it is the row where 36 functions were already
matched *inside* the span under the donor, so it was the wave's hardest test of
the by-name gate — and it came in at **+38 with zero losses**, i.e. every one of
those 36 legitimately migrated unit and kept scoring.

Three of the 31 (`SongSetlistProvider`, `SndAnalysis`, `InputMgr`) are TUs
**laneBD never located** — they came out of the re-reduction in §5.
`DrumTrackWatcherImpl` was a LOW row promoted by the same reduction.

★★ **Twelve of the 31 are pure ADDs into unclaimed space** — no donor shrinks, no
empty-unit risk, and structurally **zero possibility of a loss**. They account for
**+174 of the +599** and carry **0 of the 38 losses**: `LockStepMgr` +65,
`UGCPurchasePanel` +25, `SlotChannelMapping` +23, `ChordShapeGenerator` (first
carve) +13, `HitTracker` +12, `LogFile` +7, `Asset` +7, `DrumTrackWatcherImpl` +5,
`SndAnalysis` +4, `SongSetlistProvider` +3, and `TourGameRules`+`TourGameModifier` +10.

**laneBD's §4b list of "genuinely unclaimed, pin-ready" rows was by a wide margin
the best yield-per-unit-of-risk in the whole worklist, and it should be drained
first in any future wave.** It also finishes fastest: lane C converted seven of
them in one session while the carve lanes were still on their second TU.

### Composition

The branches were checked for composability (read-only, against `main`'s
`splits.txt`): **zero cross-branch `.text` overlaps** across all new units, and
each lane's donor edits are disjoint from every other lane's. `land.sh`'s
line-union is therefore safe here; no hand-resolution is needed, which matters
because union-merging a *genuine* splits overlap has cost 81 real losses before.

---

## 2. ★★ Phantom donors — and the predictor that finds them

laneBD found that `system/hamobj/SongDifficultyDisplay.cpp`'s pin was 100 %
foreign because **the class does not exist in RB3 retail** (no RTTI type
descriptor, no name string anywhere in the 66 k-function binary). It filed this
as a curiosity. It is not a curiosity — it is a **repeatable test with a
predictor**, and it fired twice more in this wave.

| donor | predicted by | verified in `orig/45410914/band.exe` |
|---|---|---|
| `system/hamobj/SongDifficultyDisplay.cpp` | laneBD §4b-bis: 0 class-owned vtable slots, 0 ctor sites | no `.?AVSongDifficultyDisplay@@`, no name string |
| `system/flow/FlowEventListener.cpp` | same signature | no `.?AVFlowEventListener@@` and no `.?AU…`; the strings `FlowEventListener`, `FlowEvent`, `EventListener` occur **zero** times; the only `*Flow*` type descriptor in the binary is `.?AVEnterFlowMsg@?A0x5b3730ba@@` |
| `system/bandobj/MoveMgr.cpp` | same signature | the byte string `MoveMgr` occurs **zero** times (also searched `.?AVMoveMgr`, `MoveMgr.cpp`, `moveMgr`, `move_mgr`); `MoveGraph` likewise 0 |

**The predictor: laneBD's RTTI instrument declining to place a claimer — 0
class-owned vtable slots AND 0 ctor sites — means the claimer's class is not in
the binary.** laneBD listed exactly three such donors (§4b-bis:
`SongDifficultyDisplay`, `MoveMgr`, `FlowEventListener`). All three are now
confirmed phantoms. The instrument was not failing; it was reporting absence, and
its *refusals* are as informative as its placements.

★ **Caveat — it is the TWO-channel test that is decisive, not RTTI alone.** A
class that is never instantiated, or that is non-polymorphic, has no COL either;
absent RTTI on its own does not prove absence. The confirming channel is the raw
**byte-string** search (the class name as a literal), and it is the cheaper of the
two — run it first. For `FlowEventListener` the RTTI sweep found zero `.?AV` and
zero `.?AU` descriptors, *and* the complete set of typedescs matching `Flow` is a
single entry (`.?AVEnterFlowMsg@?A0x5b3730ba@@`), which rules out the
anonymous-namespace form by construction; the string `FlowEventListener` occurs
nowhere at all. Both channels, then the verdict.

Both `MoveMgr` and `MoveGraph` are Dance-Central vocabulary — consistent with the
standing note that RB3 retail has no Flow system either. A `MoveGraph` map entry
bound to `0x822D35B8` inside the `UnisonIcon` carve was removed for the same
reason: `fn_822D35B8` is `bl 0x8227A9C8; blr` and `0x8227A9C8` is
`?StaticClassName@UnisonIcon@@SA?AVSymbol@@XZ` — it is `UnisonIcon::ClassName`,
not `MoveGraph::ClassName`.

**Action for the next lane:** the phantom test is cheap (a byte-string + RTTI
search of `band.exe`) and it should be run on *every* donor before a carve. When
a donor is a phantom, its `target_symbol_map.json` entries are all wrong by
construction and must be removed or repointed — see §4.

---

## 3. ★ Span-refinement rules (every TU in this wave needed one)

laneBD's §7.2 warned that located spans under-cover at both ends. That is
confirmed — and this wave adds the mechanism and two corrections.

### 3.0 ★★ But they can also OVER-cover — ICF folds inflate the located hi

**Do not treat "the span under-covers" as a rule to apply blindly.**
`FaceHairProvider`'s located hi was inflated by roughly **2×**: its vtable slots 1
and 8 (`Text` / `DataSymbol`, 116 B each) are **ICF-folded across the whole
`*Provider` family** and physically live inside `MakeupProvider.cpp`'s pin. The
RTTI instrument dutifully reported them as class-owned slots, so the span
stretched to reach them. Carving to the located hi would have **stolen ~900 B**
from a unit that legitimately owns it.

The asymmetry is systematic and worth internalising:
* **Under-covering** comes from what the instruments *cannot see* — free
  functions, STL template instantiations, `??__F` runs, `except_data`.
  `CharProvider` and `BandUserMgr` each under-covered by **29 %**.
* **Over-covering** comes from what the instruments see *too well* — an
  ICF-folded slot is a real class-owned slot that is not in the TU.

So a vtable slot lands inside the span **only if the function is not folded**.
Check fold status before letting a slot set an endpoint.

### 3.1 The head extends by leaf helpers and the `OBJ_CLASSNAME` accessor

★ **The head marker is consistent enough to search for directly**: the
`OBJ_CLASSNAME` local-static `Symbol` accessor — a guard-bit test, a
`static Symbol("<Class>")`, the class-name literal — immediately followed by its
own `??__F` guard-clear. Lane E hit it as the true head on `BandButton` (+40 %
span growth) and used it to place `DialogDisplay`'s head 0x78 *below* the located
lo. Search for the class-name literal's referencing function and walk back one
`??__F`.

Neither instrument sees a free function with no vtable slot and no distinctive
string. `TourDesc`'s span needed **+0x788 at the head**
(`GetTourStars{Bronze,Silver,Gold}GoalValue`, the `Get*Goal` / `Has*` /
`Get*ForGigNum` accessors, `Cleanup`, and the whole `TourDescEntry` ctor/dtor
group). `BandButton`'s needed +0x88 for its `OBJ_CLASSNAME` local-static `Symbol`
accessor plus that accessor's own `??__F` guard-clear and a 4-byte thunk.

### 3.2 The tail extends by the TU's own EH funclets — identify them by FRAME

`RealGuitarGemPlayer`'s true hi is `0x826EC860`, not the located `0x826EC7F0`:
`0x826EC7F0` (68 B) and `0x826EC834` (40 B) are the **ctor's** two EH funclets,
proved by frame correspondence (`subi r31, r12, 0xa0` matching the ctor's own
`addi r1, r31, 0xa0` epilogue) and by the second one loading the object from
`0xb4(r31)` and calling `GemPlayer::~GemPlayer` — the partially-constructed
unwind path. `UnisonIcon`'s tail extended **+0x380** because fourteen of its
vtable slots live above the reported end. `BandButton`'s extended **+0x4D0** for
six slots.

### 3.3 ★ `snapped` is NOT safe as a low bound — it snaps onto funclets

`located_spans.json` ships a `snapped [lo,hi)` "nearest real function boundary"
so `splits_move.py`'s mid-symbol gate passes first try. For
`RealGuitarGemPlayer` the snapped lo `0x826EBDE4` is **wrong**: `0x826EBDE4`,
`0x826EBDBC` and `0x826EBD94` are 40-byte EH funclets on an `r12-0x1d0` frame
destroying members at `+0x80/+0x90/+0xb0` — they unwind the function *before* the
span, and they already match at 99.9 % inside `VocalPlayer`. The correct lo is
laneBD's **unsnapped** value.

Rule: a funclet is a `.pdata` boundary, so `snapped` will happily snap onto one.
Before accepting a snapped lo, ask whose frame the function it snapped to
unwinds.

### 3.4 A guard word couples a `??__F` funclet to its TU — provably

`SongSetlistProvider`'s 32-byte funclet at `0x825BC808` clears bit 0 of
`0x82DFF5F8`, the same word `Text` sets at `0x825BC790` to guard its local-static
`Symbol` at `0x82DFF5F4`. `TourDesc`'s **19** `??__F` funclets at
`0x82368C20..0x82368EA8` clear 19 distinct bits of one guard word `0x82CBEC10`,
matching `Configure`'s 19 function-local statics exactly. This is a *proof* of
ownership, not a similarity score, and it is the cheapest way to settle a tail.

### 3.4-bis ★★ Do NOT reclaim a funclet run without checking `report.json` first

`ChordShapeGenerator`'s 29 property `Symbol`s share one guard word `0x82CBD38C`,
cleared bit-by-bit by exactly **29 32-byte `??__F` funclets** at
`0x822DE6AC..0x822DEA4C`. By the §3.4 rule those funclets are provably CSG's. They
are pinned to `CharLipSync.cpp` — **and they already score 100 % there**, via
anonymous byte pairing. Lane F deliberately did **not** carve them: the trade is
**±0 at best and 29 at risk**.

**Rule: ownership evidence tells you a funclet run is yours; `report.json` tells
you whether moving it is worth anything.** Check the second before acting on the
first. This is the one case in the wave where the correct action was to leave
provably-misattributed code where it sits.

### 3.5 ★ ICF mis-attribution: the tell is ISLAND DISTANCE, not byte count

`RealGuitarGemPlayer` is pinned as **two** `.text` blocks so `DepthBuffer3D.cpp`'s
interleaved 8-byte block at `0x826EBF20` stays with its owner. The first tell was
in the row: `claimers` read `['VocalPlayer.cpp:0x9d8', 'DepthBuffer3D.cpp:0x8']`
— a second claimer with a tiny byte count.

**But the general form is stronger and should go into the carve pre-flight: any
claimer pin that is an ISLAND — far from that unit's own other blocks — is an ICF
mis-attribution, regardless of size.** Under `/O1` with no LTCG, TU spatial
grouping is preserved, so a unit does not scatter a lone block megabytes from its
neighbours. `PracticeSectionProvider` proved it twice: `GemTrack` claimed
`0x826CF908..0x826CFB38` while its next block is at `0x82B93CE4`, and
`PlayerTrackConfigList` claimed `0x826CFBAC..0x826CFFB8` while its next is at
`0x8276F708`. Both sat mid-body of the real TU. `PracticeSectionProvider` was
therefore reassembled from **3 pins + 3 unpinned gaps**.

**Pre-flight addition: for each claimer, print the distance from the claimed
block to that unit's nearest other block.** A megabyte-scale distance is a
mis-attribution, not a donor.

### 3.6 Two cheap tells worth having in hand

* **Funclet identification without Ghidra.** A 32–48-byte function starting
  `subi r31, r12, <N>` *is* an EH funclet, and `<N>` identifies its parent via
  that parent's `addi r1, r31, <N>` epilogue. This one tell killed
  `RealGuitarGemPlayer`'s snapped lo, proved its tail extension, and settled
  `0x826CFEF0 = ~PracticeSectionProvider`.
* **Invert the string channel to find a tail.** Building the `lis`/`addi`
  constant-edge set over `.text` and inverting it (code VA → constants referenced)
  located `BandPerformer::Handle` at `0x826EDFF0`, `InitData` at `0x826D00C0`,
  `OnText` at `0x826D02C8` and `KeysFx::Poll` at `0x826F4250` in seconds each.

### 3.7 A TU can be far bigger than its `.cpp` — header-only classes

`PracticeSectionProvider.cpp` is 131 lines but the TU is the sole emitter of
`system/midi/MidiSectionLister.h`, which was **missing from our tree entirely**
and had to be ported before the TU would close. Do not size-check a span against
the oracle `.cpp`'s line count. The same effect made `KeysFx`'s true span
**2,328 B against a located 708 B** (`Poll` alone is 1,376 B).

---

## 4. Losses — every one accounted for, and why they are corrections

Total across the wave: **24 losses against 271 gains**. laneBD's +71 had zero, so
this needs an explicit account rather than a footnote.

Every loss in this wave falls into one class: **a `target_symbol_map.json` entry
that was binding a mangled name to a target VA that is not that function**, whose
100 % score therefore came from `pair_funclets_by_bytes` matching a *shape*, not
a reproduction. Carving the TU out reveals the mis-binding, the name stops
scoring, and the metric records a "loss" — while the program gets *more* correct,
not less. This is the exact inverse of the metric-farming the standing gate
guards against.

Three mechanisms produce them:

1. **Phantom-class map entries.** `FlowEventListener.cpp` had four map entries
   naming a class that does not exist; `size_order_automap`'s ground-truth column
   independently flagged three of the four as disagreements *before* they were
   touched. Repointed to `BandPerformer`'s real members.
2. **ICF folds.** `0x826EBFA8` was mapped as
   `??$__ucopy_aux@PAHPAH@stlpmtx_std@@…`; it is a single instruction,
   `b fn_826C7260`, and `fn_826C7260` is exactly slot 22 of `GemPlayer`'s
   120-slot vtable — i.e. `RealGuitarGemPlayer::Restart`, ICF-folded with the STL
   template. Likewise `0x822DD808`, mapped as
   `_Rb_tree<String, vector<ChallengeRow>>::swap`, is this TU's
   `_Rb_tree<unsigned short,…>::swap`.
3. **Adjustor thunks.** All 12-byte `subi r3,r3,N; b X` thunks are mask-identical,
   so `size_order_automap` cannot distinguish them and six `BandButton` VAs
   carried `BandHighlight`/`UIComponent` `$4` thunk names. This is the largest
   single loss cluster (8 of the 24).

### ★★ 4.1 Reusable recipe: vtable-slot alignment for adjustor-thunk ground truth

`size_order_automap` **provably cannot** resolve `$4PPPPPPPM@…` adjustor thunks —
they are all `subi r3,r3,N; b X`, mask-identical, so every proposal is a coin
flip. Derive them from the **compiler** instead of from similarity:

1. `TU_LOCATE_SCRATCH=… venv/bin/python scripts/harvest/tu_locate/vt_rtti_scan.py`
   → `vtables.json`: every `.?AV<Class>@@` → COL → vtable → slot list, read from
   `orig/45410914/band.exe`.
2. Dump **our own compiled** `??_7<Class>@@6B<Base>@@@` COMDATs from
   `build/45410914/src/<unit>.obj`. For the section the `??_7…` symbol defines,
   walk its relocation table — **10-byte records** (VA `u32`, symIdx `u32`, type
   `u16`) — and note **slot index = `reloc_va/4 − 1`**, because the section begins
   with the `??_R4` COL pointer. (`scripts/dump_vtable.py` prints only the first
   vtable, so this needs a full dump.)
3. **Pair vtables by slot count, not by offset** — our class sizes differ from
   retail's (`BandButton`'s 21-slot base sits at +552 in retail and elsewhere in
   our layout).
4. ★ **Establish fixed points before trusting anything.** Slots holding
   *externally-defined* functions must resolve to the same VA on both sides.
   `BandButton` / `UnisonIcon` / `DialogDisplay` each gave three —
   `[15] Object::SetTypeDef` `0x8275AB18`, `[16] SetName` `0x8275A5C0`,
   `[20] FindPathName` `0x8275A9D0` — plus the ICF empty-stub `0x826C3888` at
   `[11]/[12]/[14]` and `0x823591E8` at `[3]`. **If the anchors do not line up,
   the alignment is wrong — stop.**
5. Only then read off slot *i*: our symbol name ↔ retail slot VA. It is ground
   truth iff the VA is inside your span and the anchors held.
6. Cross-check against automap; **where they disagree, the vtable wins.** It
   caught automap assigning `0x82344ED8` to `PostLoad$4` when it is `Handle$4`,
   and mis-ordering 6 of `UnisonIcon`'s 15.

Yield: 11 (`BandButton`) + 15 (`UnisonIcon`) + 1 (`DialogDisplay`) thunk names,
**8 of them repointing WRONG-UNIT map entries**. This is the most transferable
technique the wave produced and belongs in the standard map-recovery step
alongside `size_order_automap.py`.

★ Note the contrast in judgement: lane C **declined** +5 from five byte-identical
`$4PPPPPPPM@A@` thunks because it could not separate them; lane E **claimed**
seven of the same shape because it had named them from the vtable rather than
guessed. Both were right. The rule is *name it or decline it — never guess it.*

### 4.1-bis Isolating a loss from a repoint

Asked whether `UIGridProvider`'s single loss came from the carve or from the
`0x82817860` map repoint, lane E ran the full recipe **both ways**: repointed →
39,541 (19 gains / 1 loss); reverted → 39,540 (18 gains / **the same 1 loss**).
So the loss is caused by the carve and the repoint is worth **+1 free**. This is
the pattern to use whenever a commit bundles a carve with map edits — the two are
separable and should be measured separately.

### 4.2 The `CharProvider` 11 — adjudicated, and a correction worth keeping

The largest loss cluster after `BandButton` was defended per-name and
**independently verified by the coordinator** against main's
`build/45410914/asm/PropKeys.s`: the closed call graph rooted at `fn_82666648`
reproduces (`0x82666AC0` has `addi r31,r31,0x14` + `mulli r11,r31,0x14` and calls
`fn_82666798 → fn_82666648`; `0x826675F8` has `mulli r11,r11,0x14` and calls
`fn_826666E8` / `fn_82667138` / itself / `fn_826673E8`; `0x82666ED8` has
`mulli r11,r31,0x14` and calls `fn_82666B10` twice). **Accepted as retirement of
false 100 %s.**

★ **But the stated mechanism was wrong for two-thirds of the set, and that
correction matters more than the verdict.** The lane argued a uniform "a 0x14
element stride is impossible for the mapped `Key<T>` type". `src/system/math/Key.h`
is `Key<T> { T value; float frame; }`, so:

| type | size | `Key<T>` | 0x14 stride? |
|---|--:|--:|---|
| `Vector3` (x,y,z) | 12 | **16** | **impossible** ✔ refutes items 1, 2, 8 |
| `Quat` (x,y,z,w — `Mtx.h:173`) | 16 | **20 = 0x14** | **consistent** — proves nothing |
| `Color` (r,g,b,a — `Color.h`) | 16 | **20 = 0x14** | **consistent** — proves nothing |

So items **3, 5, 6, 7** rest entirely on **call-graph closure into the
`CompareCharacters` component**, and items **4, 9** on automap-EXACT byte identity
against our `CharacterEntry` / `~CharProvider` bodies. That evidence is strong and
was accepted — but a future lane re-deriving from "the stride is impossible" will
fail on `Key<Quat>`/`Key<Color>` and may wrongly reinstate those four bindings.

**General rule: element-stride arithmetic refutes a map binding only when the
sizes actually disagree. Check the arithmetic per type before leaning on it;
prefer call-graph closure or automap-EXACT byte identity as the discriminator.**

★ The corrected reading also identifies the *mechanism* for nine of the twelve
lane-B losses, and it is not ICF. `PropKeys`' `Key<Quat>` / `Key<Color>` sort
templates and `CharProvider`'s `CharacterEntry` templates share the **same 0x14
element stride**, so MSVC emits **reloc-masked byte twins** — distinct functions
with identical masked bodies. `pair_funclets_by_bytes` will pair either against
either. This is a third, distinct fake-match mechanism alongside phantom names
and ICF folds, and it is the one most likely to recur: **any two container
templates over same-sized elements are candidates.**

### 4.2-bis The `??_G` ICF-fold class — the commonest loss shape in the wave

Four of lane A's five losses are one mechanism: **a template `??_G` (scalar
deleting destructor) name that retail's linker folded onto the game class's own
`??_G`**. In each case the VA provably belongs to the new owner, and a VA can
carry only one name, so it is a **−1/+1 rename, not lost program**:

| VA | old name | proof of true owner |
|---|---|---|
| `0x82368580` | `??_G?$ObjRefConcrete@…` (76 B) | calls `fn_823684E8` = `~TourDesc` |
| `0x82368FC8` | `??_G?$StackString@$0IAA@@` (68 B) | materialises vtable `0x82040334` = `TourProperty`'s |
| `0x82369510` | `??_G?$ObjPtr@VObject@Hmx@@` (76 B) | calls `~TourWeightManager` |
| `0x823667C8` | `??_GDataThisPtr` (88 B) | vtable `0x8203F9F4` + `TourPerformerImpl::~` |

Lane E's `UIGridProvider` loss (`??_G?$ObjPtr@VRndMesh@@@@`) and lane F's
`0x822E11E8` (`??_G?$ObjOwnerPtr@VRndMesh@@`) are the same shape. **`??_G` names
in `target_symbol_map.json` should be treated as low-confidence by default** —
they are short, near-identical across every `ObjPtr`/`ObjRef`/`StackString`
instantiation, and therefore the most fold-prone symbols in the binary.

### 4.3 Honest caveat

**Any loss that turns out to be a real body (≥ 100 B, not funclet-shaped) is a
genuine regression and should block that TU**, not the wave. The `*Provider`
family is near-identical small classes, so the shape hazard is highest exactly
there — which is why the `CharProvider` set was adjudicated per-name rather than
in aggregate. The `BandButton` 8 were still under the same per-name review at
time of writing.

---

## 5. ★ Fixing the reduction bug in laneBD's confidence ladder

laneBD's string channel clusters all code→string edges for a TU's selective
literals and reports `[min, max)`. When a literal is shared (`sound`,
`song_select`, `chord`), the union balloons past 8 KB, swallows ≥ 3 claimers, and
the row is demoted to LOW. **The signal was never weak — the reduction threw the
resolution away.** Attributing each edge to its individual `.pdata` function and
ranking *functions* by how many selective literals they carry recovers it.

### 5.0 ★★ `Mesh.cpp`'s pin `0x822DF9A0..0x822E33A4` is FOREIGN

Found while extending `ChordShapeGenerator`, and it is the largest mis-attributed
pin the wave uncovered. `RndMesh`'s own RTTI (typedesc `0x82C6B88C`, 4 COLs)
resolves to vtables at `0x820608F4/EC/94/8C`, and **every one of their own-class
slots is at `0x82417DA8..0x8242xxxx` — 1.3 MB away** from the pin. Three blocks
(`0x822DFD88..0x822DFF20`, `0x822DFF28..0x822E1234`, `0x822E12B0..0x822E33A4`)
were carved out to `ChordShapeGenerator` for **+42**, and `Mesh.cpp` keeps all its
real blocks, so no empty-unit risk.

This is the §2 phantom test's weaker sibling and it should be run just as
routinely: **a donor whose own RTTI slots are nowhere near its pin does not own
that pin**, even when the class itself certainly exists.

### 5.1 `ChordShapeGenerator`: LOW → landed

Its LOW "span" `0x822DD290..0x822E325C` (24.5 KB) straddles **eight** pinned
units (`Mesh.cpp` 13,448 B, `CharLipSync.cpp` 3,172 B, `Font.cpp` 1,520 B,
`XLSPConnection.cpp`, `SpectralAnalysis.cpp`, `CharBonesSamples.cpp`,
`Synapse_dsp.cpp`, `BandProfile.cpp`) — it is not a TU at all. Per-function
reduction collapses it to **one** function:

```
0x822DD480..0x822DD664  484 B  UNCLAIMED  6/6 selective literals
    'chord' 'chord_L' '%s(%d)' '%s.mesh' '%s_%d' 'update_objects'
```

— which is also laneBA's independent `autoid` proposal at score 6/6 — sitting
inside a fully-unclaimed 6-function run. Lane F confirmed it three further ways
(RTTI COL → vtable `0x82024D4C`; the *only* `lis`/`addi` pair building that
vtable in the whole `.text` is at `0x822E0364` inside the 1,112-byte ctor; and
`fn_822DDC58` carries all 29 `SYNC_PROP` names while `fn_822E2FC8` carries the 4
`HANDLE` names and calls the four `On*` handlers) and landed it as a **pure ADD
into unclaimed space** — 13 functions, unit fuzzy 100.00 %, zero losses.

**General rule: a LOW row whose span overlaps ≥ 3 pins should be re-reduced
per-function before being written off.**

### 5.2 Four TUs located that laneBD never placed

From laneBD's still-unlocated 65, by the same reduction:

| TU | anchor fn | selective literals on it | owner | Wii fns/bytes | status |
|---|---|---|---|--:|---|
| `band3/meta_band/SongSetlistProvider` | `0x825BC828..0x825BC8FC` (212 B) | **2/2** `part_difficulty_screen` + `song_select_screen` — the only function in the binary carrying both | UNCLAIMED | 4 / 908 | **landed +3** |
| `system/dsp/SndAnalysis` | `0x82B81860..0x82B81BF8` (920 B) | **4/4** `boost`, `maxperiod`, `minperiod`, `numpeaksmin` | UNCLAIMED | 5 / 2,584 | **landed +4** |
| `band3/meta_band/InputMgr` | `0x825B0598` (88 B, `input_user_left`) + `0x825B0C38` (160 B, `input_mgr`) | 2 anchors | UNCLAIMED (≈1.9 KB / 14 fns) | 32 / 10,332 | located, assigned |
| `system/bandobj/ArpeggioShape` | `0x82356118..0x82356208` (240 B) | **5/5** `chord_label.txt`, `chord_shape.mat`, `chord_shape.mesh`, `fade.mnm`, `fret_numbers_chord.txt` | **`Rot.cpp`** micro-pin, currently mapped to `?OnOverlayPrint@Rnd@@…` at **11.7 %** — a mispair, so moving it costs nothing | 24 / 6,052 | located, refined, not ported |

★ `ArpeggioShape` was refined further: `0x82356118` is
`ArpeggioShapePool::ArpeggioShapePool(ObjectDir*, RndGroup*, int)` — it calls
`ObjectDir::FindObject` (`0x8227D5E8`) five times with exactly the five init-list
literals, then loops calling `CreateArpeggioShape`. The rest of the TU is
scattered across `FingerShape.cpp` (`0x82355098`), `GemTrackResourceManager.cpp`
(`0x82356050`) and an unclaimed run `0x82354CAC..0x82355044` — a multi-donor
untangle needing its own session.

★★ **CORRECTION to a hypothesis this lane issued.** I flagged a *second* `Rot.cpp`
micro-pin at `0x823556E0..0x82355738` (88 B) in the same block as also suspicious.
It is **not** stray: it is `?StaticClassName@RndMatAnim@@SA?AVSymbol@@XZ`, it
scores **100 %**, and its `'MatAnim'` string reference confirms it. **Do not move
it.** A scattered micro-pin in a foreign-looking neighbourhood is not evidence of
mis-attribution by itself — `Rot.cpp` legitimately owns ~30 scattered COMDATs.
The 88-byte class-name shape (§5.3) was the tell, and I should have applied my own
rule before proposing the move.

### 5.3 Twelve further single-literal anchors (leads, not locations)

`StorePackedMetadata` (124 Wii fns — the largest unlocated TU),
`StoreOfferContentsProvider` (31), `UISyncNetMsgs` (25), `VocalOverlay` (21),
`ChordPreview` (20), `NowBar` (16), `GameMic` (15), `ProfilePicture` (5), plus
precise anchors for `MultiSelectListPanel`, `ReviewDisplay`, `BandFaceDeform`,
`DialogDisplay`.

★ Note the recurring **88-byte** anchor (`ReviewDisplay` `0x8231E450`/`0x8231F5C8`,
`DialogDisplay` `0x82329B20`/`0x8232ABF8`, `BandFaceDeform`
`0x8227A528`/`0x822C7298`, `PatchRenderer` `0x822AE130`/`0x822AF0A0`). These are
`?StaticClassName@<Class>@@` header-macro COMDATs, which laneBD §6.3 already
refuted as a *locator* (the linker groups them into scatter blocks). Treat an
88-byte class-name anchor as an **existence proof only** — which is exactly what
the phantom test in §2 needs — and require a second, non-88-byte anchor before
pinning.

---

## 6. ★ What is NOT actionable — the LOW bucket, measured

laneBD priced its LOW bucket at 22 spans / 532 functions with a "do not act
without a third channel" caveat. Measuring the two things that would make a third
channel work — selective literals, and unclaimed space to put the code in —
shows most of it is unreachable by *any* refinement of the existing instruments:

| TU | unmatched | selective literals | unclaimed runs in span |
|---|--:|--:|---|
| `KeyboardController` | 96 | **0** | **none** |
| `ButtonGuitarController` | 76 | **0** | **none** |
| `RealGuitarController` | 52 | **0** | **none** |
| `JoypadGuitarController` | 29 | 1 (in `TrackWatcherImpl.cpp`) | none |
| `JoypadMidiController` | 12 | 1 (in `TrackWatcherImpl.cpp`) | none |

265 of the 532 sit in these five rows. Their Wii sources contain 6–10 literals
total, none selective; their spans are 100 % claimed; and the RTTI channel put
all five on the same overlapping block because their vtables are dominated by a
shared controller base. Also signal-less: `StoreArtLoaderPanel` (26),
`RKTrainerPanel` (10), `StandInProvider` (3), `Playback` (3), `AppScoreDisplay`
(1), `BandSong` (0), and `TourChallengeResultsPanel` (a 4-byte span — an artifact
that should be deleted from the worklist).

**Do not fund another string/RTTI channel for the controller family.** The one
signal that would work is *compile-and-byte-search*: port the Wii source
speculatively, compile it, and search retail `.text` for reloc-masked
byte-identical bodies. The strength is already established — laneBD's `UIProxy`
port produced 13 reloc-masked byte-identical functions on the first compile, and
this wave saw **17 of 26** for `UIGridProvider`. Nobody has yet turned it into a
locator. It inverts the pipeline (port → compile → locate → pin) which is the
right order when there is no positional signal at all.

### 6.0 ★ `MakeupProvider.cpp` is itself mis-pinned — its pin holds `OutfitProvider`

Found by lane B while separating the `Leaderboard.cpp` provider cluster. This is
the same class of defect as the phantom donors (§2) and the foreign `Mesh.cpp`
pin (§5.0), but with a *live* class: `MakeupProvider` exists, its pin simply
contains a different provider's code. It matters because `MakeupProvider.cpp`
appears as a second claimer on four separate rows of `located_spans.json`, so
anyone reasoning from those rows is reasoning from a bad label.

Lane B also returned refined spans for three sibling providers it did not have
budget to wire — these are pin-ready and should be taken next:

| TU | span |
|---|---|
| `EyebrowsProvider` | `0x8266EC88..0x8266F088` |
| `FaceTypeProvider` | `0x8266F088..0x8266F380` |
| `InstrumentFinishProvider` | ≈ `0x8266FAF0..0x8266FDC8` |

Note these supersede the `located_spans.json` rows, which for `FaceTypeProvider`
and `InstrumentFinishProvider` both anchored on the *same* 116-byte function
carrying only the generic literal `option` — the string channel cannot separate
those two, and the ICF-fold analysis above is why.

### 6.1 Rows that DO collapse — the next wave's worklist

| TU | was | anchor / unclaimed run | headroom |
|---|---|---|--:|
| `CurrentOutfitProvider` | LOW | 3 in-span anchors; runs `0x82670090..0x82670414` (900 B/3) and `0x82670700..0x82670A80` (896 B/4) | 41 |
| `CampaignGoalsLeaderboardChoicePanel` | MED | runs `0x825F45F0..0x825F4948` (856 B/3), `0x825F4298..0x825F4594` (764 B/6) | 63 |
| `BandFaceDeform` | MED | anchor `0x822C7298` (584 B, UNCLAIMED) — **outside** laneBD's span | 42 |
| `SetlistSortByLocation` | LOW | 0 anchors, but `0x825C4B10..0x825C5488` = 2,424 B / **22 fns** unclaimed | 33 |
| `ReviewDisplay` | MED | run `0x8231EAA8..0x8231EF28` (1,152 B/8) | 28 |
| `CharMeshCacheMgr` | LOW | `0x8239CCA8..0x8239D048` (928 B/4) | 20 |
| `MultiSelectListPanel` | LOW | anchor `0x82627170` (**2/2**), run `0x82626C00..0x82626E34` (564 B/4) | 18 |
| `SaveLoadStatusPanel` | LOW | `0x82631BB8..0x82631DA4` (492 B/3) | 18 |
| `RetryAudioPanel` | LOW | anchor `0x826303C8` UNCLAIMED, span 82 % unclaimed | 12 |
| `TourPropertyCollection` | LOW | `0x82365978..0x82365C20` (680 B/6), 82 % unclaimed | 12 |
| `DrumTrackWatcherImpl` | LOW | `0x82780298..0x827808C0` (1,576 B/4), **98 % unclaimed** | 9 |
| `AssetOffer` | MED | whole span `0x8266B520..0x8266B7CC` **100 % unclaimed** | 8 |

★ `BandFaceDeform` and `ReviewDisplay` are worth a note: their string anchor lands
**outside** the RTTI-derived span. Two instruments disagreeing is signal — the
RTTI span comes from vtable slots (which for a derived class can sit in a base's
TU) while the class-name literal is in the TU's own `StaticClassName`. Reconcile
before pinning either.

---

## 7. Retail-vs-Wii-dev divergences — additions to the standing list

laneBD established that `../rb3` is a **dev** build and every divergence costs a
whole function. This wave found these; each was worth at least one function and
several were worth 0 → 100.

**★★ `std::map` where retail uses Harmonix's `hash_map`**

The rb3-Wii dev tree approximates Harmonix's `hash_map` as `std::map`. Retail does
not: the proof is that retail calls the container **default ctor out of line**
(`fn_8255D480` = `??0?$hash_map@VSymbol@@H…`), which `std::map` would not produce.
Worth `ChooseQuestFilters` 50.7 % → 100 % and `InqSongsInFilterData` 83 % → 97 %.
Any `std::map<Symbol, T>` inherited from the Wii oracle is a candidate.

**★★ TRAP: a block-scoped `static Symbol` can silently bind a global of the same
name**

`random`, `custom`, `filter_any`, `filter_dynamic_artist` … exist *both* as the
function-local statics retail emits *and* as globals. Writing
`static Symbol random("random")` inside a block therefore compiles cleanly while a
sibling scope binds the **global** instead — wrong codegen, no diagnostic. Noted
in-source in `src/band3/tour/TourPerformerLocal.cpp`. This is the sharpest edge in
the local-static lever below; expect it wherever a property name is also a
`Symbols*.h` global.

**★★ Redundant derived destructor declarations — a tree-wide scannable lever**

*The rb3-Wii dev headers declare a redundant derived `virtual ~Derived() {}`;
retail's headers do not.* With the redeclaration MSVC emits the **own-vptr store
at dtor entry** (`lis`/`addi ??_7Derived` + `stw r11,0(r3)`); retail's dtors go
straight to member teardown and only restore the **base** vptr. Confirmed
independently in three TUs; deleting five redeclarations was worth **+5**
(`~HitTracker` 88 → 100, `~MassChannelMapping` and `~MultiChannelMapping`
83.6 → 100, and `LockStepMgr::StartLock` 63.8 → 100 via the inlined
`BasicStartLockMsg` temporary).

This is the best force-multiplier the wave produced that nobody has swept: the
signature is mechanical (a dtor whose target lacks the own-vptr store while our
header redeclares the destructor), it applies to every polymorphic Milo class
ported from a Wii header, and it is **cheap to scan tree-wide**.

**★ Control-flow shapes (lane G measured the rivals, so these are specific)**
* **Shared-false tail on a switch.** Retail routes `default:` *and* a failing
  in-case guard to **one** `return false` placed *after* the switch. Writing
  `default: return false;` inside costs ~11 % — `IsValidButtonForShell`
  89.1 → **100**. Two rival formulations measured *worse* (85.5 %, 76.6 %), so the
  shape is exact, not a preference.
* **Direct bool expression, not accumulate-into-a-flag.** `bool x = A && B;`
  produces retail's `li 1` / conditional `li 0` / `clrlwi` in a volatile register;
  the oracle's flag form inverts the test and hoists into a callee-saved register.
  `AllowRemoteExit` 94.3 → **100**, `HasValidController` 79.3 → **100**.
* **Unsigned-range tricks are dev-only.** `ty - 3U <= 1 || ty == 1` in the oracle
  is `ty == 1 || (ty > 2 && ty <= 4)` in retail. `CheckTriggerAutoVocalsConfirm`
  97.7 → **100**.
* Micro-lever worth trying on any loop with a repeated divided bound: hoisting
  `int half = vlen / 2;` instead of leaving `vlen / 2` inline let MSVC
  strength-reduce an induction pointer as retail does (`FindCCPeak`
  93.3 → 95.7 %).

**★ A new wall class: hand-written VMX128**
`ShiftedDotProduct`'s fast path is **hand-vectorised VMX128 selected by its
fourth parameter** — which the Wii oracle marks `/*unused*/`, using a Gekko
paired-single `asm{}` block instead. It is not reconstructible from this oracle
and was left at 25 % deliberately. **Expect this in `system/dsp` and
`system/synth`**; it is a legitimate reason to stop, distinct from regalloc.

**Guards and arms retail drops**
* `UIGridProvider::SetListToData` — retail has **no** `if (child)` guard; the
  `SetSelectedSimulateScroll(idx % mWidth)` call is unconditional.
* `UIGridProvider::GetDataFromList` — no `if (child) … else return -1`; the body
  is unconditional.
* `UIGridProvider::GetSymbolFromList` — tests only `data < NumData()`, with no
  `data >= 0` half. **This one alone took the function 0 → 100.**
* `ChordShapeGenerator::MakeInvertedMesh` — no `LOADMGR_EDITMODE` Sync/SetMutable
  arm (71.6 % → 100 %).

**Save/Load revisions**
* `ChordShapeGenerator::Save` is a real `SAVE_REVS(1,0)` body mirroring `Load`,
  **not** `SAVE_OBJ(…, 0x43)` (1.5 % → 100 %). This is the third TU in the
  project where a Wii `SAVE_OBJ` is a real `SAVE_REVS` body in retail; treat
  `SAVE_OBJ` in the oracle as *suspect by default*.

**★ Local-static Symbols, and declaration POSITION as codegen**
* Retail spells `TourDesc::Configure`'s 19 `Symbol`s as **function-local
  statics**, not the globals of `utl/Symbols*.h`. The *declaration position* is
  load-bearing twice: it fixes the guard-bit numbering (and therefore all 19
  `??__F` bodies) and where the guard test lands in the flow.
* `ChordShapeGenerator::SyncProperty` needs **both**
  `/DRB3_SYNCPROP_LOCAL_STATIC` and `/DRB3_HANDLE_LOCAL_STATIC` — retail builds
  each of the 29 property `Symbol`s as a guarded function-local static
  (33.1 % → 100 %, a 2,644-byte / 661-instruction function).
* `SongSetlistProvider`'s `choosing` is a function-local `static Symbol`, not the
  centralised global.

**Dev-source bugs retail does not have**
* `ChordShapeGenerator::NameMesh` — retail has no `dynamic_cast<RndMesh*>` around
  `Dir()->FindObject(...)`, **hoists `counter = 1` out of the do-while** (the Wii
  dev source assigns it inside, which is a bug), keeps the new name in a separate
  variable assigned only after the loop, and inlines the `MakeString` into the
  `FindObject` call so `Dir()` is evaluated first (83.3 % → 100 %).

**Tree/API shape, not dev-vs-retail**
* `TheUI` is a **pointer** in RB3-360 (`extern UIManager *TheUI`), so the Wii
  source's `TheUI.` member syntax becomes `TheUI->` — hit in `BandButton`,
  `UIGridProvider` and `SongSetlistProvider` alike.
* Header-comment offsets are Wii-sized. `TourDesc`'s post-`vector` members sit
  +4 because MSVC's `std::vector` is 3 pointers. **Ask
  `scripts/harvest/class_layout_report.py` (the compiler), never the `// 0xHEX`
  comments.**
* `UnisonIcon.h` needs `#include "obj/ObjMacros.h"` for `DECLARE_REVS`/
  `INIT_REVS`: `obj/Object.h` defines a *different* two-argument `INIT_REVS` and
  wins by include order otherwise.

---

## 8. The honesty gate — and why `reloc_correspondence.py` was dropped

Standing requirement: gains must be *program*, not *metric*.

★ **`scripts/harvest/reloc_correspondence.py` is NOT usable as a per-wave gate.**
A single `--symbol` invocation **timed out at 10 minutes**; `--unit` across two
units does not finish in useful time. It was dropped mid-wave on the
coordinator's ruling. Do not plan a wave around it.

**The replacement is the size distribution of the gains, and it is both cheaper
and a stronger signal.** Calibration measured on the whole tree from
`report.json`:

| tree strict-100 baseline | |
|---|--:|
| n | 39,521 |
| **funclet-shaped** (`fn_<8hex>`/`__unwind$`/`__catch$`/`??__E`/`??__F`) | **53.4 %** |
| size min / p25 / median / mean / p75 / p95 / max | 4 / 32 / **40** / **88** / 88 / 288 / 6,900 |

Every lane reports, per TU: **n**, **median**, **mean**, **% funclet-shaped**. A
batch that is body-weighted (high median, low funclet share) is program; a batch
that tracks or exceeds the tree's 53.4 % funclet share is boilerplate harvesting
and must be presented as such. Worked example — `CharProvider`: **n=26, median
136 B, mean 140.8 B, 0 % funclet-shaped**, against a tree median of 40 B and
53.4 % funclet. That is exactly the "body-weighted, not boilerplate" evidence the
relocation gate existed to find, obtained in seconds instead of never.
(laneBD's +71 was median 40 B / mean 134 B / zero funclets.)

★ Two spans in the remaining worklist are flagged as elevated metric risk and
must have their counts split into real bodies vs funclets before they are landed:
* `Quest` `0x8235AE68..0x8235B8BC` — **22 of its 27 `.pdata` functions are a
  uniform 32-byte `??__F` run** at `0x8235B4E4..0x8235B6C4`.
* `CampaignCareerLeaderboardPanel` — 10 uniform 32-byte + 6 40-byte functions of
  30.

The same split is owed on two *landed* TUs where a large funclet share is known
to exist: `TourDesc`'s +58 includes the 19 `??__F` funclets of §3.4, and
`SndAnalysis`'s +4 includes four consecutive 32-byte functions at
`0x82B81BF8`/`C18`/`C38`/`C58`. Those funclets are legitimately owned (the
guard-word coupling proves it) but must not be presented as ported bodies.

More generally, the panel TUs share a size fingerprint (68 / 48 / 32 / **316**
bytes recurring across `GameTimePanel`, `ParentalControlPanel`,
`InterstitialPanel`, `CampaignCareerLeaderboardPanel`) which is Milo `UIPanel`
boilerplate — near-identical bodies, i.e. exactly the family that can score 100 %
as a shape. For those, the size split is the only cheap check available, so
insist on it.

## 8bis. Two spin-off findings, handed off rather than pursued

Both came out of lane B and are recorded here because they are levers, not
laneBL work:

* ★★ **`User`/`BandUser` has a virtual the dev tree lacks — found twice,
  independently.** Lane D: `BandUser::IsNullUser()` is reached through a **virtual
  base** (`add` / `addi r3,r11,4` / vtable slot `0x70`), and the *same four*
  target-only instructions explain all five `BandPerformer` residuals. Lane G:
  where the Wii line is `mSessionMgr->HasUser(user)`, retail calls a virtual **on
  the user, passing the SessionMgr** (`lwz r11,0(r30); mr r3,r30; lwz r4,0x24(r29);
  lwz r11,0(r11); bctrl`), reaching it via `vbtbl[8]+4` when the pointer is a
  `LocalBandUser*` — i.e. RB3-360's `User`/`BandUser` has a
  `bool <slot0>(SessionMgr*)` virtual the dev tree does not model. Worth ≥ 8
  functions across two lanes, binary-wide blast radius, **needs a layout owner.**
  Two lanes converging on the same class from different symptoms is the strongest
  signal in the wave for what to fund next.
* ★ **`DataArrayPtr`'s ctor is out-of-line in retail, inline in our tree.** Retail
  calls `??0DataArrayPtr@@QAA@ABVDataNode@@@Z` at `0x8228D370`, but
  `src/system/obj/Data.h` defines every `DataArrayPtr` ctor inline, so `/Ob2`
  inlines them everywhere. This blocks `TourPerformerRemote::OnSynchronized` at
  67.7 % and **has the same shape in every TU that builds a `DataArrayPtr` from
  `DataNode`s** — a tree-wide inline-policy lever, exactly the class the
  force-multiplier finder was built for. Needs its own A/B.
* ★ **`BandUser::IsNullUser()` goes through a VIRTUAL BASE.** All five
  `BandPerformer` residuals (89.5–94.7 %) differ by the *same* four target-only
  instructions: retail reaches `IsNullUser()` via `add` / `addi r3,r11,4` / vtable
  slot `0x70`, where our `BandUser.h` models a plain call. Binary-wide blast
  radius, so lane D left it alone — it needs a layout owner, and it is the
  highest-leverage single item the wave surfaced.
* ★ **`AssetMgr` uses a linked chain, not `std::map`.** Retail walks a
  null-terminated singly-linked chain — head at `*(mgr+0x2c)`, next pointer at
  `*(node+0)`, `Asset*` at `*(node+8)` — while our source emits an `_Rb_tree`.
  This is a genuine **cross-TU container-shape lever**: it blocks
  `AssetProvider::Update` plus four constructors. Queued as its own lane; laneBL
  deliberately did not take it on.
* **dtk over-carve is a real ceiling, not a port defect.** 11 of `CharProvider`'s
  19 residuals are one source function that jeff split at an early `blr`. Those
  are certified `at_limit` and must not be ground on. (Consistent with the
  standing note that over-carve was drained 88 → 1; this is the residual tail of
  the same phenomenon, not a regression of it.)

---

## 7ter. ★ The empty-unit trap fired for real

Documented in CLAUDE.md but never observed until now. `InputMgr`'s carve took
`StreamRenderer.cpp`'s block — **its only `.text` range** — and `report.json`
hard-failed with `Failed to open obj/StreamRenderer.obj` exactly as predicted,
until the unit's whole `splits.txt` entry was deleted in the same edit. Four
donors were involved in that one carve (`AppInlineHelp`, `CalibrationPanel`,
`StreamRenderer`, `OvershellPanel`) and **all four micro-pins were
mis-attributions**: `InputStatusChangedMsg::Type()`+`??__F`,
`InputStatusChangedMsg`'s ctor, `InputMgr::OnMsg(LocalUserLeftMsg)`, and
`OvershellPanel` swallowing `Handle`'s 11 EH funclets *and* its 9 `??__F`
guard-clears.

Ownership was proved by the §3.4 guard-word rule: `Handle`'s 9 local statics set
bits 0–8 of `0x82DFF420` and the nine 32-byte funclets clear exactly those bits,
while the 11 EH funclets carry `addi r31,r12,-0xf0` = `Handle`'s frame size. Both
cheap tells (§3.4, §3.6) doing the whole job on one carve.

## 7bis. ★★ A silent `splits.txt` corruption class: dtk tolerates duplicate ranges

Lane C shipped **six commits containing 36 self-overlapping duplicate `.text`
ranges** without noticing. Its per-TU staging helper rebuilt `splits.txt` from
`git show HEAD:config/45410914/splits.txt` at each step — HEAD advances, so every
step re-appended the earlier units' blocks.

★ **The reason it went unnoticed is the finding: dtk silently tolerates
exact-duplicate ranges.** The build succeeds, the split succeeds, and the
measurement is unaffected (re-verified byte-identical after dedupe). There is
**no symptom at all** until `overlap_check.py` aborts, or until a *different*
lane adds a pin that collides with the duplicate — at which point the failure
surfaces in someone else's work.

Two rules follow:
1. **Never rebuild `splits.txt` from a moving `HEAD`** inside a per-TU loop.
2. **Run `overlap_check.py` before every commit, not just before every build.**
   The existing SOP places it before builds, which is exactly where it cannot
   catch this.

All seven branches were audited after the fix: `laneBL-A` 5,819 `.text` ranges /
0 overlaps, B 5,819/0, C 5,825/0, D 5,816/0, E 5,818/0, F 5,819/0, G 5,815/0 —
plus zero cross-branch overlaps.

## 8ter. Measured size distributions (the gate in practice)

Lane D, all five TUs, all 90 gains landing in its own units (no donor re-pairings):

| TU | n | min | median | mean | max | % funclet |
|---|--:|--:|--:|--:|--:|--:|
| `RealGuitarGemPlayer` | 31 | 4 | 68 | 80.8 | 304 | 16.1 % |
| `BandPerformer` | 26 | 12 | 66 | 93.7 | 396 | 15.4 % |
| `PracticeSectionProvider` | 22 | 8 | 64 | 124.0 | 912 | 31.8 % |
| `CrowdRating` | 5 | 32 | 76 | 191.2 | 668 | 0.0 % |
| `KeysFx` | 6 | 12 | 80 | 92.0 | 212 | 16.7 % |
| **lane D total** | **90** | 4 | **68** | **102.0** | 912 | **18.9 %** |
| `CharProvider` (lane B) | 26 | — | **136** | 140.8 | — | **0 %** |
| *tree baseline* | 39,521 | 4 | *40* | *88* | 6,900 | *53.4 %* |

Body-weighted on every axis: median 1.7× the tree's, funclet share about a third
of it.

★ **One honest qualification, from lane F.** Its 60 gains show median 44 B /
mean 195 B / **0 % funclet-shaped by name** — but **41 of the 60 carry anonymous
`fn_<8hex>` names**, small template and thunk COMDATs that came with the pin. The
substance is the named tail (`SyncProperty` 2,644 B, `Handle` 660, `NameMesh` 484,
`_Rb_tree::swap` 408, `Save` 268, `Copy` 252, `MakeInvertedMesh` 212, plus
`ClassName`/`SetType`/the four `On*` handlers). **"0 % funclet-shaped" is not the
same as "0 % boilerplate" when the names are anonymous** — report the named/
anonymous split alongside the funclet share.

★★ **Lane C's framing is the one to adopt, and it should be mandatory.** It
reported its +144 as **"76 ported bodies + 68 byte-paired EH funclets"** rather
than as 144 bodies:

| TU | n | median | mean | % funclet | named bodies | anon `fn_` funclets |
|---|--:|--:|--:|--:|---|---|
| `LockStepMgr` | 65 | 68 | 94 | 47.7 % | n=34, med 112, mean 144 | n=31, med 40 |
| `UGCPurchasePanel` | 25 | 40 | 101 | 68.0 % | n=8, med 234 | n=17, med 40 |
| `SlotChannelMapping` | 23 | 48 | 61 | 34.8 % | n=15, med 72 | n=8, med 40 |
| `HitTracker` | 12 | 42 | 65 | 25.0 % | n=9, med 68 | n=3, med 40 |
| `LogFile` | 7 | 76 | 81 | 28.6 % | n=5, med 88 | n=2, med 40 |
| `Asset` | 7 | 32 | 46 | 85.7 % | n=1 (116 B) | n=6, med 32 |
| `DrumTrackWatcherImpl` | 5 | 88 | 72 | 20.0 % | n=4, med 88 | n=1 |
| **total** | **144** | **44** | **84** | **47.2 %** | **n=76, med 92, mean 124** | **n=68, med 40** |

`Asset` (1 body + 6 funclets) is the weakest row in the wave and is labelled as
such; `UGCPurchasePanel` carries 17 funclets but also the wave's largest median
body (234 B). Lane B's four TUs: n=79, median 84 B, mean 116 B, **15 % funclet**.

★ Lane G's `SndAnalysis` is the wave's one honestly-bad row and is labelled as
such: its +4 is **4 x 32 B `??__F` funclets and zero real bodies** (both real
bodies missed strict — `FindCCPeak` 95.66 %, `RefinePeriod2` 99.977 %). Lane G
total: n=41, median 64, mean 102.7, 43.9 % funclet, 23 named bodies. Lane E:
n=80, median 44, mean 132.7, **15 % funclet, 68 of 80 named**. Lane A: n=105,
median 40, mean 87.9, 35.2 % funclet — framed as "105 = 68 named bodies + 37
anonymous funclets", named bodies alone median 76 B / mean 116.7 B / max 1,608 B.

★ Lane C also **declined +5 of pure metric**: five twelve-byte
`$4PPPPPPPM@A@` adjustor thunks that are byte-identical to each other and
separable only by a relocation the normalized diff masks. It left them unmapped
rather than guessing, and did not fabricate the unidentified XEX-import callee in
`UGCPurchasePanel::Enter`/`Exit`. That is the behaviour the gate is for.

Two independent lanes measured `reloc_correspondence.py` hanging (>8 min and
>10 min on `--unit`), corroborating the coordinator's finding and the decision to
drop it.

## 8quater. ★ The highest-value follow-up: reconstruct `UILabel`'s tail

`BandButton`'s three residuals (`PreLoad` 1,140 B, `DrawShowing` 76.1 %, `Update`
0.7 %) were **reduced rather than ported**, because their Wii bodies touch
`UILabel` members that are still buried inside `UILabel::mUnkTU5Tail[0xAC]` —
`mFitType`, `mWidth`, `mHeight`, `mLeading`, `mAlignment`, `mKerning`,
`mTextSize`, `mCapsMode`, `mLabelDir`, `mFontMatVariation` — plus
`RndText::GetFont` and `UIComponent::UpdateAndDrawHighlightMesh`, which do not
exist in the tree yet.

**Reconstructing `UILabel`'s tail gates ~5 functions in `BandButton` alone and
presumably many more across the whole `UILabel` family.** It is the single
highest-value follow-up the wave identified, and it is a layout job rather than a
port, so it wants its own owner.

Two more blockers of the same character, recorded so nobody re-discovers them:
* **`CharKeyHandMidi` is refuted as tractable for a lane** (not as a location —
  its span is now known: `0x822D07F0..0x822D16F8` + `0x822D1768..0x822D2908`, 13
  owned vtable slots, and with `UnisonIcon` carved it is the only remaining
  claimant of the donor block). The port is blocked on **`CharIKFingers`**: this
  tree's version is DC3-derived with a different API (`kNumFingers` not
  `kFingerNone`, no `SetFinger`/`ReleaseFinger`, a different `FingerDesc`).
  Reconciling it touches an already-matching engine unit. Carve reverted cleanly.
* **`DialogDisplay` continues into `CharUpperTwist.cpp`'s pin**
  (`0x82329FD8..0x8232A60C`): three more owned slots and a second vtable
  materialisation at `0x8232A2D8`. `CharUpperTwist` is a *real* retail class, so
  that block is genuinely shared and needs a finer seam than a whole-block carve.

Two further dtk over-carves were confirmed (jeff-level ceilings, certify
`at_limit`, do not grind): 11 of `CharProvider`'s 19 residuals are one source
function split at an early `blr`; and `fn_828175C0`
(`UIGridSubProvider::UpdateExtendedText`, really 0x2C) is split into a 0x20
function + a spurious `except_data_827F21C8` + a 4-byte `bctr`, while all five of
its siblings match.

★ **An honest negative:** the redundant-derived-destructor lever (§7) **does not**
apply to lane E's TUs — `??1UIGridProvider` (104 B), `??1BandButton` (296 B) and
`??1UnisonIcon` already matched byte-identically. It is a real lever with a real
+5, not a universal one.

Build-order gotcha worth carrying: **`obj/ObjMacros.h` must be included before the
class header** in every ported TU, or `obj/Object.h`'s conflicting two-argument
`INIT_REVS(rev, alt)` wins and breaks `INIT_REVS`/`DECLARE_REVS`.

## 9. What remains

**Of the 42 HIGH spans:** 13 TUs landed here (plus laneBD's 3 = 16 of 45 rows
touched). Assigned-and-in-flight at time of writing: `BandUserMgr`,
`BandStoreOffer`, `LockStepMgr`, `SlotChannelMapping`, `UGCPurchasePanel`,
`HitTracker`, `LogFile`, `Asset`, `CrowdRating`, `KeysFx`, `TourPerformerLocal`,
`TourPerformerRemote`, `ArpeggioShape`, `InputMgr`, `CharKeyHandMidi`,
`DialogDisplay`.

**Unassigned HIGH remainder** (≈ 67 functions): `Quest` (15, but see §8),
`CampaignCareerLeaderboardPanel` (12, ditto), `GameTimePanel` (12),
`ParentalControlPanel` (10), `InterstitialPanel` (6), `ProfileAssets` (6),
`AssetStore` (6), `TourQuestGameRules` (2), `TourReward` (1). Four of these have
their headroom in *fully unclaimed* runs and so are clean ADDs with no loss risk:
`DialogDisplay` `0x82329B98..0x82329FD8` (7 fns, mean 151 B — the pick of them),
`ProfileAssets` `0x82655098..0x826552CC` (6), `KeysFx` `0x826F3EA8..0x826F4100`
(5), `TourQuestGameRules` `0x82365BC0..0x82365CCC` (4).

`PatchRenderer` was deliberately excluded — another lane has in-flight work on
`src/system/bandobj/PatchRenderer.h` and `BandSwatch.cpp` in main.

**Of the 65 unlocated:** 4 located (§5.2), 12 leads with a single anchor (§5.3),
and the controller family refuted as unreachable by the current instruments (§6).
laneBD's own corrections still stand: ≥ 12 TUs / 109 fns are census false
positives (`system/speex`, `bufstreamnand`, `rso_utl`).

## 10. Reproduction

```bash
cd /home/free/code/milohax/rb3-xenon              # main, read-only

# the sharpened string reduction (§5) -- read-only, ~1 min
cd scripts/harvest/tu_locate
TU_LOCATE_SCRATCH=~/tmp/laneBL/tu_locate ../../../venv/bin/python str_xref.py
#   -> 20,141 code->string edges, 12,693 distinct strings (identical to laneBD's;
#      only the REDUCTION differs)
# then, per TU: str_locate.wii_lits / lits_of / locate  ->  attribute each edge to
#   str_locate.fn_of(va) (the real .pdata table) instead of clustering min..max,
#   and rank functions by len(selective literals carried).
# unclaimed runs: walk str_locate.FNS, group maximal chains with claim_of(a) is None.

# honesty-gate calibration (§8)
venv/bin/python - <<'PY'
import json,re,statistics
r=json.load(open('build/45410914/report.json'))
F=re.compile(r'^(fn_[0-9a-fA-F]{8}|__unwind\$|__catch\$|\?\?__E|\?\?__F|__unwind__merged_)')
sz=[];fl=0
for u in r['units']:
    for f in u.get('functions',[]):
        if f.get('fuzzy_match_percent',0)>=100.0:
            sz.append(int(f.get('size') or 0)); fl+=bool(F.match(f['name']))
sz.sort(); n=len(sz)
print(n, f"{100*fl/n:.1f}% funclet", sz[n//2], f"{statistics.mean(sz):.1f}")
PY
```

Per-TU recipe, A/B protocol and the trap list are in `~/tmp/laneBL/BRIEF.md`
(the operating brief every sub-lane followed); the durable version of it is
laneBD's `docs/plans/wii-oracle-tu-location-2026-07-29.md` §7 plus §3 and §4
above.
