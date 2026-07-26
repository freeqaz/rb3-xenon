# Lane AE — the unemitted-symbol class

**Date:** 2026-07-26 · **Branch:** `laneAE-emit` · **Baseline:** 30,093 strict

## The idea

objdiff pairs target ↔ base **by symbol name**. So a target function whose
mangled name is not defined by *any* object our build compiles is **structurally
pinned at 0%** — no amount of body-porting, splits work, or map work can score
it, because there is nothing to pair it with. The only fix is a **source** change
that makes our build emit the symbol retail has.

Three lanes independently hit this and handed it off. The seed was the
`ObjRefConcrete<T>::SetObj` family: our build emits **122**
`?SetObj@?$ObjRefConcrete@…` symbols and **zero**
`?Replace@?$ObjRefConcrete@…` (verified below), so the retail bodies are
unclaimable by any name we compile.

## Scanner

`scripts/harvest/unemitted_symbol_scan.py`

1. Walks every `.obj` under `build/45410914/src/`, parses the COFF symbol table,
   and collects every symbol **defined** there (`SectionNumber > 0`). That is the
   authoritative *emitted set* — 923 objs, **747,867 distinct symbols**.
2. Walks `build/45410914/report.json` for every target function at
   `match_percent_normalized == 0.0`.
3. Excludes `fn_*` (unmapped — a different lane's pool: map coverage, not source),
   `__unwind$`/`$*`, and `auto_03_*` units (XDK vendor + Quazal, hard-skipped).
4. Reports the ones whose name is **not in the emitted set**, ranked by
   cluster size × function size.

It then applies two generalisations that turned out to carry the actual value:

- **FIX-SIG vs ADD-DECL.** If our build emits the same `scope::method` with a
  *different signature*, the bug is a **wrong declaration**, not a missing
  definition — and retail's mangled name is ground truth for the correct one.
- **Wrong-variant detector.** The tree carries two source lineages (`Band*`/
  `meta_band` from RB3, `Ham*`/`meta_ham` from DC3) and **retail contains both
  families**. A candidate whose symbol becomes an *emitted* symbol under a
  `Ham`↔`Band` substitution is a case where both variants exist in our tree and
  we instantiated the wrong one — the confirmed **inverse** of the usual
  "retail predates the DEV additions" trap.

## ★ The funnel (measured, not estimated)

| stage | count |
|---|---|
| target functions in report.json | 69,420 |
| … at 0% | 36,021 |
| … in scope (non-XDK/Quazal) | 8,555 |
| … unmapped `fn_*` (other lane's pool) | 8,140 |
| … **named and 0%** | **415** |
| … of which the name **is** emitted (body/pairing work, not ours) | 91 |
| … of which the name is **NOT** emitted — **this lane** | **324** |
| ↳ **FIX-SIG** (same method emitted with a wrong signature) | **51** |
| ↳ ADD-DECL (scope::method never emitted at all) | 273 |
| ↳ of which wrong `Ham`/`Band` variant selected | 6 |

Total unclaimable bytes in scope: **64,132**.

**Strategic number — the true size of the class.** Dropping the scope filter,
fleet-wide there are **5,755 unemitted named 0% functions / 2,430,664 bytes
across 307 units**. The class is therefore *dominated* by the XDK/Quazal vendor
spans (5,755 → 324 once those are excluded), which is why the in-scope lane is
small. That 5,755 is the honest headline for "how much of the binary can never
be claimed by a name we compile", but ~94% of it is out-of-scope vendor code.

### In-scope composition of the 324 (classified, adjudicated)

| bucket | count | bytes | note |
|---|---|---|---|
| VENDOR-UNWIRED | 99 | ~29,940 | LEAPCORE, `XGRAPHICS::Compress*`, `DxRnd`, `DSP::Synapse::*`, `Mic*Xbox`, `fft_*`, CRT `osfinfo`, `ctr_encrypt`. **Most of this is not truly absent** — real source sits *unwired* in dc3's `synth_xbox`/`rnddx9`. Treat as an unwired-TU vein, not a dead end |
| STL-INST | 86 | 12,512 | `_Rb_tree<…>::insert_unique`, `vector<…>::_M_insert_overflow_aux`, `_M_fill_insert_aux`, `__final_insertion_sort`. A prior lane landed +9 then **drained** it — price low |
| MAP-ARTIFACT | 30 | ~2,850 | `lbl_*`, `merged_*`, `__MERGED_*`, ≤8-byte fragments, and foreign namespaces (NUISPEECH, `FaceCore::MultiPoseDetector`, `TrueColor`, `_NUIP_`, XAUDIO2, ATL, `ST::`, XShaderPDBBuilder). **Not** source opportunities |
| VECDTOR (`??_E`) | 25 | 1,960 | **map defect, not a source gap** — see the measured result below |
| **ACTIONABLE** | **84** | **~17,220** | the fundable pool |

`_icf_arbitrary` (25 VAs) was checked against all 324 — **zero overlap**, no
exclusions needed.

**~1/3 of the ACTIONABLE rows are `SCATTER-WIRE`**, i.e. the owner TU ≠ retail's
landing unit, so the fix is a scatter-include, not a declaration. Two of them
(`PlatformMgr_Xbox.cpp`, `FxSendPitchShift360.cpp`) have real bodies in-tree but
are **not compiled at all** — they need wiring into `objects.json` before a
scatter-include can even apply.

Two classes are fake placeholder stubs inside `src/system/bandobj/Band.cpp`
(`PatchRenderer`, `ScrollbarDisplay`) where rb3-Wii has full
`RndTexRenderer`/`UIComponent`-derived headers ready to port — those need real
class ports, not declarations.

★**Trap (a) fired here**: `RndParticleSys::InitParticle` has **both** dc3 and
rb3-Wii agreeing with our 4-parameter source, yet retail's mangled name wants 3.
Both oracles are newer than retail simultaneously.

★**Ham/Band lineage flag fired on 3 rows**, all downgraded from clean fixes to
**SUSPECT MISPAIR** (map may be carrying DC3 `ham_xbox_r.map` names):
`ProfileMgr::GetProfileFromPad` (internally inconsistent with sibling
`GetShouldAutosave` in the same class, which resolves to `BandProfile*`),
`MetaPerformer::MetaPerformer(HamSongMgr const&)`,
`AppLabel::SetBestBattleScore(HamProfile*, int)`. The **reverse** case also
exists and is *not* a mispair: `HamScrollSpeedIndicator` in the `Dir` unit is a
genuinely shared, unrenamed DC3 class present verbatim at
`src/system/hamobj/HamScrollSpeedIndicator.h`.

## ★ The payload — FIX-SIG rows (retail's mangled name = ground truth)

These are provable source bugs. Selected rows:

| size | our declaration | retail's mangled name says |
|---|---|---|
| 244 | `void Hmx::Object::InitObject();` → emits `?InitObject@Object@Hmx@@QAAXXZ` | `?InitObject@Object@Hmx@@**U**AAXXZ` — **virtual** |
| 296 | `AddSink(…, SinkMode = kHandle)` | `…W4SinkMode@12@**_N**@Z` — a trailing `bool` |
| 2492 | `RndParticleSys::InitParticle(float, RndParticle*, Transform const*, PartOverride…)` | 3 params — the 4th is a DC3 **DEV addition** retail predates |
| 892 | `CharIKFoot::DoFSM(Transform&)` | `DoFSM(**Character***, Transform&)` |
| 188 | `FftIpp::FftReal(unsigned*, float&, float*, float&)` | `PIB`/`PIA` = **`__restrict`** pointers |
| 84 | `ProfileMgr::GetProfileFromPad(int)` → `BandProfile*`, non-const | `Q**B**A…PAV**HamProfile**@@` — **const**, returns `HamProfile*` |
| 172 | `PreviewDownloadCompleteMsg()` | `(bool, bool)` |
| 164 | `EventDialogDismissMsg()` / `(DataArray*)` | `(Symbol, Symbol)` |
| 136 | `ConnectionStatusChangedMsg(DataArray*)` | `(bool)` |
| 84 | we emit only `??_ECharServoBone@@**WBA@AA**…` / `$4PPPPPPPM@KM@AA…` (adjustor thunks) | plain `??_ECharServoBone@@**UAA**PAXI@Z` — retail's CharServoBone is **not** multiply-inherited where ours is |
| 112/108 | `sort`/`__final_insertion_sort` on `WidgetDrawSort@?A0x60bef9da` | `?A0x530db9db` — **`obj_anon_ns_patcher.py` does not neutralise anon-namespace hashes embedded in *template arguments*, only the symbol's own scope**. Tooling gap |

### The `Ham`/`Band` wrong-variant cluster (6 rows, ~1,108 B)

Retail has **both** families: 18 `HamCamShot` symbols and 46 `BandCamShot`,
1 `HamSongMgr` and 24 `BandSongMgr`, 2 `HamProfile` and 114 `BandProfile`.
Our tree has both too (`src/system/bandobj/BandCamShot.h`,
`src/system/hamobj/HamCamShot.h`; `src/meta_ham/…`, `src/band3/meta_band/…`).

Five of the six rows are `Target@**Band**CamShot` in retail vs
`Target@**Ham**CamShot` emitted by us — `operator<<`, `ObjList<Target>::operator=`,
`list<Target>::operator=`, `_M_splice_insert_dispatch<_List_iterator<Target>>`.
Two independent defects are implicated:
1. retail mangles it **`U`**`Target@BandCamShot` = **struct**; our
   `BandCamShot::Target` is declared `class` (would mangle `V`);
2. retail instantiates **`ObjList`/`std::list`** of `Target`; our `BandCamShot`
   holds an `ObjVector<Target>`.

The sixth row runs the other way: retail wants
`??0MetaPerformer@@QAA@ABV**HamSongMgr**@@PBD@Z`, which our
`src/meta_ham/MetaPerformer.h:38` declares *exactly*, but the wired definition is
`src/band3/meta_band/MetaPerformer.cpp:151` using `BandSongMgr`. Retail has only
**one** `HamSongMgr` symbol, so this is a single-signature fix at most — do not
rename `BandSongMgr` fleet-wide.

## ★ Necessary-but-not-sufficient: pairing is **per-unit**

Measuring the sibling pool (the 91 target functions at 0% whose name *is*
emitted somewhere): **83 of 91 are emitted in a different unit than the target
function's; only 8 are same-unit.** objdiff pairs target ↔ base **within a
unit**.

So emitting the symbol is necessary but not sufficient — it has to be emitted in
the unit where **retail's COMDAT physically landed** (the `unit` column of the
candidate list), and retail scattered COMDATs across TUs. A correct declaration
fix whose definition lives in a different TU measures 0 flips and is a **false
negative**.

The follow-up recipe when the natural owner TU ≠ the landing unit is the known
scatter-wiring trick: append `#include "<owner>.cpp"` inside the .cpp of the
landing unit, which pairs the whole COMDAT cluster at once (that vein previously
landed +52). The `BandCamShot::Target` cluster is a textbook case — its six
symbols land in four different units (`BandCamShot`, `PanelDir` ×2, `Shockwave`,
`BandUser`).

## Seed verification (`ObjRefConcrete`)

- our build emits **122** `?SetObj@?$ObjRefConcrete@…`, **0** `?Replace@?$ObjRefConcrete@…`
- `?Replace@` exists for 396 other ObjRef-family types (e.g. `ObjDirPtr<T>`)
- `ObjRef::Replace(ObjRef*, Hmx::Object*)` is pure virtual (`src/system/obj/Object.h:42`),
  so `ObjRefConcrete` must override it — retail does, we do not

**★ Blocked at the lane boundary.** `scripts/target_symbol_map.json` currently
contains **zero** entries naming `SetObj@…ObjRefConcrete`, so those VAs are
unmapped (`fn_*`). Emitting `Replace` from source would therefore *still* not
pair — the fix needs a **source change AND a map repoint**, and map repair is a
different lane's single-owner responsibility. **Handed off, not applied.**

## Handoffs to the map owner (report-only, per the single-owner rule)

1. `ObjRefConcrete::Replace` VAs are unmapped — needs map entries, not just source.
2. The map contains DC3-lineage names (`HamSongMgr`, `HamProfile`,
   `HamListRibbon`, `HamScrollSpeedIndicator`) — at least some are transfers from
   DC3's `ham_xbox_r.map` and may be wrong for RB3. `?AddSink@Object@Hmx@@…_N@Z`
   directly contradicts an in-tree comment
   (`src/system/obj/Object.h:1734`) that derives the *opposite* arity from real
   X360 call-site codegen. One of the two is wrong.
3. Map-artifact names in pinned spans: `lbl_*`, `merged_*`, `__MERGED_*`, and
   NUISPEECH/FaceCore/`_NUIP_`/XAUDIO2/ATL/XShaderPDBBuilder names appearing in
   RB3 game units.

## Tooling gap found

`scripts/obj_anon_ns_patcher.py` neutralises the `?A0x…` anonymous-namespace hash
of a symbol's own scope but **not** hashes embedded inside *template arguments*
(`??$sort@PAPAVUIListWidget@@UWidgetDrawSort@?A0x530db9db@@…`). Extending it
would pair the `AssetProvider` `sort`/`__final_insertion_sort` rows for free.

## ★ Measured result — `??_E` sub-class is a MAP DEFECT, not a source gap

**Clean negative, net 0, nothing committed** — and the most valuable finding of
the lane. The 25 in-scope (58 fleet-wide) `??_E<Class>@@UAAPAXI@Z` candidates
are **not** missing vector deleting destructors. MSVC-X360 names the *primary*
deleting destructor `??_G` and names its adjustor/vtordisp **thunks** `??_E`.

Evidence:
- our build: `??_G` = 1,915 symbols, **all primaries, zero thunks**; `??_E` =
  524, of which **517 are thunks** (`W..`, `$4PPPPPPPM@..`)
- the only 7 non-thunk `??_E` we emit are exactly the array-`new`'d types
  (`String`, `TaskTimeline`, `StreamSettings`, `FreestyleMoveFrame`,
  `kdTreeNode`, `OrderedLocaleChunk`). A genuine vector deleting dtor always
  carries the `rlwinm. rN,r4,0,30,30` (`flags & 2`) branch plus an
  `eh vector destructor iterator` call — 108 B for `String`. `??_EString` is at
  **100%**, driven by `new String[…]` in `src/system/rndobj/HiResScreen.cpp:41`
- **none of the 25 targets has that `flags & 2` branch** — all are the plain
  scalar shape at the canonical ICF fold sizes (68/76/84/88 B), reloc-masked
  byte-identical to `??_G` functions we already emit
- the map holds 59 `??_E` primaries, **58 of them at 0.0%**, and 509 `??_G`
  entries with **zero** thunks — the exact inverse of what the compiler emits.
  `tools/fingerprint_match.py:1027` already warns about `??_E`/`??_G` ICF aliases

Several are additionally attached to the **wrong class**, provable from the
callee `??1`: `??_EBandSongMgr`→`~CymbalSelectionProvider`,
`??_EPracticeChoosePanel`→`~GameMicManager`, `??_EPoseFatalities`→`~NgPostProc`,
`??_ECharBoneOffset`→`~CharIKSliderMidi`.

Experiments run (all whole-binary, unit-agnostic set diff off a 30,093 baseline):
forcing a `delete[]` use site on `RndVelocityBuffer` → **+0** (MSVC emitted
`??_G…@EAA…`, never a primary `??_E`); private-dtor alone → **+0**; a *positive*
control (`mFrame = 0` → `1`) → **−1** (harness names the right function); a
*negative* control (comment-only edit, real recompile) → **+0**.

### Handoff — 7 verified guaranteed flips for the map owner

Pure `??_E`→`??_G` rename, same class, byte-identical in-unit:
`0x82b72930` → `??_GRateTransposer@soundtouch@@UAAPAXI@Z` ·
`0x82704a88` → `??_GStandardStream@@UAAPAXI@Z` ·
`0x82b85510` → `??_GRndVelocityBuffer@@UAAPAXI@Z`

`??_E`→`??_G` **plus class re-attribution** (corroborated by the callee `??1`
*and* byte-identical in-unit):
`0x826697f8` → `??_GCymbalSelectionProvider@@UAAPAXI@Z` ·
`0x823cbdc0` → `??_GCharIKSliderMidi@@UAAPAXI@Z` ·
`0x82b899b8` → `??_GNgPostProc@@UAAPAXI@Z` ·
`0x82682210` → `??_GGameMicManager@@UAAPAXI@Z`

**Proposed global lint** (cheap, systematic): flag every `??_E` map entry whose
target body lacks the `flags & 2` branch, and every `??_G` map entry that has
one. That catches this whole defect class binary-wide.

Optional source fix, net-0 today and therefore uncommitted:
`src/system/rndobj/VelocityBuffer.h` should have
`virtual ~RndVelocityBuffer()` under `private:`, not `public:` — retail's
mangling is `EAA` (private virtual). Only worth taking together with an `EAA`
map rename.

## ★ Measured outcome — **+9 strict, 0 regressions** (30,093 → 30,102)

Verified by the lane lead against its **own** baseline pickle, with the full
tree merged, built **twice**, and both reads identical (30,102 / 30,102):

```
GAINED 9  LOST 0
 + default/BandCharacter  ?StaticClassName@OvershellDir@@SA?AVSymbol@@XZ
 + default/BandSwatch     ?StaticClassName@PatchRenderer@@SA?AVSymbol@@XZ
 + default/StarDisplay    ?StaticClassName@ReviewDisplay@@SA?AVSymbol@@XZ
 + default/FFT            ?FFTComplex@@YAHPAMJJ0@Z
 + default/FFT            ?fft_pingpong@@YAHPAMKJ0@Z
 + default/Synapse_dsp    ?Time2IirA@?A0xa7b3dd7d@@YAMMM@Z
 + default/Mic            ??1ChatReceiver@@QAA@XZ
 + default/Mic            fn_82B5E1E4
 + default/MemTracker     ?delete_and_clear@AllocInfoVec@@QAAXXZ
```

The workers individually reported +2 / +3 / +3 = +8; the joint build yields **+9**
(`AllocInfoVec::delete_and_clear` is an interaction gain no single worker saw).
This is why the lane lead re-measures rather than summing worker claims.

### The four winning shapes (price these high)

1. **Inline-COMDAT force-emit from the landing TU** (+3, ~100% reliable).
   `OBJ_CLASSNAME` defines `StaticClassName` *inline in the class body*, so MSVC
   only emits the COMDAT in a TU that odr-uses it — and nothing did. Retail
   scattered each COMDAT into a *different* TU's span, so force-emitting from
   the landing TU (`BandCharacter.cpp`, `StarDisplay.cpp`, `BandSwatch.cpp`)
   flips them. **A scanner over `OBJ_CLASSNAME`-bearing headers with no in-tree
   odr-use would find the rest mechanically** — recommended next tool.
2. **Missing definition with a Matching DC3 oracle** (+2 FFT, +1 ChatReceiver).
   Highest-volume shape, but only flips outright when retail and DC3 bodies
   agree; otherwise it lands a partial (FFT: three more went 0% → 70–81%).
3. **Anonymous-namespace *nesting*** (+1). New failure shape: right unit, right
   signature, wrong namespace nesting. We emitted
   `?Time2IirA@?A0xe1e61d40@Synapse@DSP@@…`; retail is
   `?Time2IirA@?A0xa7b3dd7d@@…` — the **global** anon namespace, not
   `DSP::Synapse::<anon>`. Compounding it, `scripts/obj_anon_ns_patcher.py`'s
   regex is `\?A0x([0-9a-fA-F]{8})@@`, which **only matches the global form**, so
   a nested anon namespace is doubly unpairable and the wired patcher silently
   does nothing. Always check nesting before assuming the patcher handles a
   `?A0x…` row.
4. **`__restrict` / const signature fixes** — real but rarely flip outright
   (`FftIpp::FftReal` 0% → 94.4%).

### ★ The dominant root cause: the map, not the source

**The single biggest finding of this lane is that most of this pool is not a
source gap at all.** Verified by *decoding the retail bytes at the VA*, not by
inference:

- 6 of 9 big-body candidates are **map label errors**
- all 25 `??_E` rows are a systematic **map defect** (see above)
- 4 more `Hmx::Object`-family rows are map mispairs

Confirmed mislabels (handoffs to the map owner — **not applied here**, per the
single-owner rule):

| VA / symbol | what is actually there |
|---|---|
| `?Load@StoreArtLoaderPanel@@UAAXXZ` | **`?Load@UIPanel@@UAAXXZ`** — literals `"load"/"proj_file"/"file"/"heap"`, `Message→HandleType→FindArray→FilePath::Set→GetCurrentHeapNum→PoolAlloc(0xa8)→DirLoader`; line-for-line our `src/system/ui/UIPanel.cpp:196`. **One-line map repair = immediate flip; we already emit the body** |
| `?FocusComponent@UIPanel@@**U**AAPAVUIComponent@@XZ` | the `U` should be **`Q`**. Self-refuting evidence: `StoreFocusComponent@CharacterCreatorPanel`, `StoreFocusComponent@CustomizePanel` and `SetFocusPanel@UIScreen` all matched at baseline **calling it through a direct `bl`** — impossible if retail's were virtual |
| `?AddSink@…_N@Z` @`0x82b881d8` | not AddSink at all — vtable-dispatched alloc, flag bits at `+0x188`, two floats via `lwa 0x4c/0x50` + `fcfid/frsp`, call to slot `0xfc`. Render code. **The in-tree comment at `src/system/obj/Object.h:1734` stands; our 4-param declaration is correct as-is** |
| `??0PreviewDownloadCompleteMsg@@QAA@_N0@Z` @`0x825c21d0` | builds `DataNode{r4, type 4 = kDataObject}` + `DataNode{clrlwi(r5), type 0 = kDataInt}` → `(Hmx::Object*, bool)`, not `(bool,bool)` |
| `??0EventDialogDismissMsg@@QAA@VSymbol@@0@Z` @`0x82803540` | both DataNodes are type 4 (`kDataObject`); `(Symbol,Symbol)` would be type 5 |
| `?ST_SetTrackingMode@@YAKW4ST_TRACKING_MODE@@@Z` @`0x8228f1a8` | 12 B; loads a global into r3 and tail-calls, **discarding its own parameter** |
| `?InitParticle@RndParticleSys@@` (3-arg) | target reads r7 → it **is** 4-arg. DC3's `ham_xbox_r.map` has only the 4-arg form. **Our `Part.h` is correct** |
| `?DoFSM@CharIKFoot@@` (2-arg) | target memcpy's 0x30 B into r4 and loads the character from `lwz r3,0x3c(this)` → 1-arg `DoFSM(Transform&)`, exactly our header |
| `?GetStarsToken@@YA?AVSymbol@@H@Z` | a **constructor** (vtable stores, sub-object ctors at +0x4/+0x10/+0x1c/+0x28, two `bdnz` init loops) |
| `?CheckNoFlashcardsCondition@…` | arg-less: `BandWardrobe::GetPlayMode()` → `DataGetMacro(Symbol)` → linear scan (stride 8, limit 0x18) → return index |
| `?InviteParty@PlatformMgr@@QAAXH@Z` | `(this, vector<UIScreen*>& out)`: `NumData()` → `BottomScreen()` → 2× `push_back` |

### ★ Measured negatives (do not re-fund these)

- **`virtual` on `Hmx::Object::InitObject`: −598.** It *paired* (0% → 88.85%) but
  slid vtable slots fleet-wide; the loss list is saturated with
  `?SetType@X@@UAAXVSymbol@@@Z` (`OBJ_SET_TYPE` virtuals). **Retail RB3's
  `Hmx::Object` does not have `InitObject` virtual.**
- **`virtual` on `UIPanel::FocusComponent`: −14.** Same shape.
- ⇒ **"DC3 has `virtual`, we don't" is a map-lineage artifact by default — 0 for
  2, costing −598 and −14. Route it to the map owner; do not spend a build.**
  This directly contradicts the general "missing-virtual force-multiplier"
  lever *on engine base classes*.
- **`FIFOSampleBuffer::ptrBegin` const:** retail has **both** the const
  `FIFOSampleBuffer` form and a non-const protected-virtual `FIFOProcessor`
  form. const-throughout = net 0 (symmetric swap); both overloads = **−8**
  (extra vtable slot shifts TDStretch/RateTransposer call sites); pure-virtual
  variant does not compile. Unresolved C++-shape puzzle, not a map error.
- **`BandCamShot::Target` `class`→`struct`: net 0 (+1/−1).** The port of
  `operator<<(BinStream&, Target const&)` hit **100% first try**, but the map
  holds **both** class-keys for the same type — `U` for the template
  instantiations, `V` for `??0Target@BandCamShot@@QAA@ABV01@@Z` (248 B,
  currently 100%). If the map owner normalises the `V` rows to `U`, this becomes
  a clean **+2**. Ready-made patch: `/home/free/tmp/laneAE_bandcamshot_struct.patch`.

### ★ Two process traps that nearly caused wrong landings

1. **`report.json` is not converged on the first read** after
   `rm -f report.cache && ninja`. A stable, reproducible phantom **−8 in
   soundtouch** appeared that also reproduced with the change *fully reverted*.
   Building twice and reading until two identical readings makes it vanish.
   **Any single-build A/B in this fleet is untrustworthy.**
2. **Two concurrent agents collided on a shared `~/tmp` A/B script path.** The
   overwritten script reported `+2 gained / 0 lost` for a change that was
   actually **−598**. Keep A/B harnesses *inside your own worktree*, never at a
   shared `~/tmp` path.

## Pricing note

Adding an out-of-line definition perturbs `/O1 /Ob2` inlining fleet-wide; a
sibling lane measured a whole class of "emit it out-of-line" fixes at **0/26
flips, worst −4**. Every candidate here was therefore A/B'd whole-binary against
a per-worker baseline pickle that had to read exactly 30,093 before any edit.
