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
- ~~**`BandCamShot::Target` `class`→`struct`: net 0 (+1/−1).**~~ **SUPERSEDED —
  the *full* change measures +4.** The bare class→struct flip is indeed net 0,
  but combined with the real `Save` body + the two scatter force-emits it is a
  measured **+4**. See "★ Measured outcome — `BandCamShot::Target` cluster = +4"
  below.

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

---

# Round 2 (2026-07-26, resumed lane) — the per-unit filter folded into the funnel

The round-1 lane died on repeated API 529s with five fixer worktrees still live.
Round 2 recovered them first, then built the per-unit pairing filter into a
proper scanner and re-derived the funnel from a fresh 30,102 baseline.

## Worktree recovery (nothing lost)

| worktree / branch | state found | disposition |
|---|---|---|
| `wt-laneAE-bigbody` / `laneAE-bigbody` | committed `99409cb1` (Mic.cpp ChatReceiver) | **already landed on main** — byte-identical, no action |
| `wt-laneAE-hmxobj` / `laneAE-hmxobj` | committed `fc6dc200` (3 OBJ_CLASSNAME force-emits) | **already landed on main** — byte-identical |
| `wt-laneAE-misc` / `laneAE-misc` | committed `9210ed23` (synth_xbox emissions) | **already landed on main** — byte-identical |
| `wt-laneAE-camshot` / `laneAE-camshot` | **UNCOMMITTED** BandCamShot Save + `Target` `operator<<` + 2 scatter force-emits (Shockwave, PanelDir), never measured | committed as `4610b0bc` to preserve it, then A/B'd (below) |
| `wt-laneAE-vecdtor` / `laneAE-vecdtor` | nothing but `config/45410914/symbols.txt` | nothing to recover (the `??_E` result was a clean negative, already documented) |
| `wt-laneAE-emit` / `laneAE-emit` | committed, all content landed | merged main back in; the apparent "deletions" in its diff vs main were **other lanes' docs**, not its own work |

## ★ New tool — `scripts/harvest/scatter_pairing_scan.py`

Round 1 established that pairing is per-unit but did not *mechanise* it.
`unemitted_symbol_scan.py` answers "does **any** obj define this name?".
The new scanner answers the load-bearing question — "does the obj whose pinned
span **contains the target** define it?" — and splits the named-0% pool three
ways. Both scanners share `coff_defined_symbols`.

## ★ The funnel, per-unit filter applied (measured, baseline 30,102)

| stage | count | bytes |
|---|---|---|
| target fns in `report.json` | 69,420 | |
| … at 0% | 36,004 | |
| … in scope (non `auto_03_`) | 8,538 | |
| … unmapped `fn_*` (other lane's pool) | 8,139 | |
| … map/tooling artifacts (`lbl_`/`merged_`/`$`) | 6 | |
| … **named and 0%** | **393** | |
| ↳ **NOWHERE** — no obj emits the name → needs **source** | **302** | 57,372 |
| ↳ **ELSEWHERE** — emitted, but by the wrong obj → **SCATTER-WIRE** | **41** | 5,784 |
| ↳ SAME-UNIT — pairable already → body divergence, not this lane | 50 | 2,792 |

**ELSEWHERE is the round-2 payload.** It is the pool where the source is already
correct and the *only* defect is which TU emits the COMDAT — precisely what the
round-1 `OBJ_CLASSNAME` wins fixed. Existence proof, re-verified: all three
landed wins are now emitted by **exactly** their landing obj
(`?StaticClassName@OvershellDir@@…` → `BandCharacter` only, etc.), and all three
flipped to 100%.

Composition of the 41: ~15 STL/template instantiations, ~14 inline in-class-body
methods (cheap ODR-use fix), ~12 out-of-line members (need the heavier
`#include "<owner>.cpp"` scatter-include). The `--classname` mode shows the
`OBJ_CLASSNAME` sub-vein is now nearly **drained**: 3 ELSEWHERE + 2 NOWHERE left.

### ★ True size of the class

| scope | NOWHERE | ELSEWHERE | unpairable total | units |
|---|---|---|---|---|
| fleet-wide (incl. XDK/Quazal) | 5,730 / 2,418,780 B | 48 / 8,240 B | **5,778 fns / 2,427,020 B** | 313 |
| in scope (excl. `auto_03_`) | 302 / 57,372 B | 41 / 5,784 B | **343 fns / 63,156 B** | 137 |

So the honest headline is **5,778 functions / 2.43 MB can never be claimed by a
name our build compiles** — but **94% of it is XDK + Quazal vendor code that is
hard-skipped**, leaving a real in-scope class of **343 functions / 63 KB**.
The per-unit filter then narrows the *cheap* part of that to **41**.

## ★ New sub-class found — duplicate target names inside one unit (MAP defect)

objdiff pairs by name within a unit, so if the **target** side carries the same
name twice, at most one can pair and the extras are structurally unclaimable no
matter what the source does. In scope there are **21 duplicate-name groups
covering 24 non-100% target functions (~2.3 KB)** — all ICF folds that the map
named identically at several VAs. Cross-tabbing against the funnel:

- NOWHERE: 0 of 302 affected
- ELSEWHERE: 0 of 41 affected
- **SAME-UNIT: 23 of 50 affected**

⇒ the "50 SAME-UNIT body-divergence rows" is really **27 body rows + 23 map
duplicates**. Worst offenders: `?ConfigPanels@VocalTrackDir@@QAAXXZ` (748 B, ×2),
`??_G?$ObjPtrList@VHamCamShot@@VObjectDir@@@@UAAPAXI@Z` (×3, `BandCamShot`),
`?OnFileExecRoot@@YA?AVDataNode@@PAVDataArray@@@Z` (×3, `File`),
`?StaticClassName@TexMovie@@…` / `?StaticClassName@SfxSeq@@…` (×2).
**Handoff to the map owner — deduplicate these VAs; not applied here.**

## Seed re-check — `ObjRefConcrete::Replace` is STILL not addressable

Re-verified against the current build: **zero** `ObjRef`-family rows appear
anywhere in the 302 NOWHERE / 41 ELSEWHERE lists, because those VAs are still
unmapped `fn_*`. Emitting `Replace` from source would still not pair. The seed
that opened this channel remains **blocked on a map repoint**, which is a
different lane's single-owner responsibility. Unchanged from round 1.

## ★★ Process trap — an inherited worktree's first build is NOT a baseline

Round 1's headline was **+9** with an "interaction gain no single worker saw"
(`?delete_and_clear@AllocInfoVec@@QAAXXZ` in `default/MemTracker`). Main's own
post-landing measurement said **+8**. Round 2 settled it decisively:

- my resumed `wt-laneAE-emit` baseline read **30,102**, stable across **three**
  `rm -f report.cache` + full-`ninja` legs — so double-building did *not* expose it
- the set-diff against main's `report.json` was exactly one entry: that same
  `('default/MemTracker', '?delete_and_clear@AllocInfoVec@@QAAXXZ')`
- deleting `build/45410914/src/system/utl/MemTracker.obj` and rebuilding dropped
  the count to **30,101** and made the worktree **zero-diff against main**

⇒ the orphaned worktree was carrying a **stale `.obj`** that ninja considered
current, so no amount of rebuilding would refresh it. **The "+9 interaction gain"
was a stale-obj phantom; the true landed figure is +8 and the true baseline is
30,101.**

A sibling round-2 worker hit the same thing harder: its first baseline read
**30,089**, making a naive A/B report **+16** for a change worth **+4** — 12 of
the "gains" were in units the diff never touched (`Task` ×5, `DataFunc` ×2,
`DirLoader` ×2, `DataUtl`, `ContentMgr_Xbox`, `HamNavList`).

**Rules this yields** (stronger than round 1's "build twice"):
1. Building twice is **not sufficient** in an inherited worktree — a stale obj
   survives it. Set-diff your baseline against a *known-good* tree (main), or
   delete the suspect objs and rebuild.
2. **A gain in a unit your diff does not touch is the tell for a stale obj**, not
   an interaction gain. Round 1 mis-read exactly that signal as a real win.
3. Require two *identical* baseline reads AND a cross-tree reconciliation before
   trusting an absolute number. Deltas measured within one worktree stay valid
   regardless, which is why per-worker deltas were still sound.
## ★ Measured outcome — `BandCamShot::Target` cluster = **+4 strict** (30,101 → 30,105)

**Branch:** `laneAE-camshot` · commits `4610b0bc` (rescued source) + this doc.
Measured in `/home/free/tmp/wt-laneAE-camshot` with main merged in, every leg
built **twice** (`rm -f report.cache` before each), harness kept *inside* the
worktree (`laneAE_ab.py`).

Baseline read **twice independently: 30,101 / 30,101**. Full change read **three
times: 30,105 / 30,105 / 30,105**. `config/45410914/symbols.txt` md5 held at
`4f2060e6…` across all five builds, so no dtk carve drift between legs.

```
base=30101  full=30105  NET=+4  (GAINED 5 / LOST 1)
 + default/BandCamShot  ??6@YAAAVBinStream@@AAV0@ABUTarget@BandCamShot@@@Z            (344 B)
 + default/BandCamShot  ??$?6UTarget@BandCamShot@@…@@YA…ABV?$list@UTarget@…@Z         (108 B)
 + default/PanelDir     ??4?$list@UTarget@BandCamShot@@…@stlpmtx_std@@QAA…@Z          (176 B)
 + default/PanelDir     ??4?$ObjList@UTarget@BandCamShot@@@@QAAXABV0@@Z               (108 B)
 + default/Shockwave    ??$_M_splice_insert_dispatch@U?$_List_iterator@UTarget@…@Z    (156 B)
 - default/BandCamShot  ??0Target@BandCamShot@@QAA@ABV01@@Z                           (248 B)
```

### Each of the four files maps 1:1 onto a gain — no dead weight

| file | change | gain it delivers |
|---|---|---|
| `bandobj/BandCamShot.h` | `class Target` → `struct Target` | prerequisite for **all** 5 (every gained name carries the `U` struct key) |
| `bandobj/BandCamShot.cpp` | real `BEGIN_SAVES` body + `operator<<(BinStream&, const Target&)` | `??6@…ABUTarget…` (344 B). **`Save` itself is not in the map** and scores nothing — but streaming `mTargets` is what odr-uses the operator and forces the COMDAT to emit. Load-bearing, not cosmetic |
| `bandobj/BandCamShot.cpp` | `sw_BandCamShotTargetListStream` | the `list<Target>` stream operator (108 B) |
| `ui/PanelDir.cpp` | `sw_BandCamShotTargetListAssign` | both PanelDir `operator=` rows (176 + 108 B) |
| `rndobj/Shockwave.cpp` | `sw_BandCamShotTargetListSplice` | `_M_splice_insert_dispatch` (156 B) |

The class→struct flip is **inseparable** from the 5 gains and from the 1 loss —
provable from the mangled names alone, without a build: every gained symbol
contains `UTarget@BandCamShot@@` (struct key) and the lost one contains `ABV01`
(class key) *on the same type*. There is therefore **no subset that takes the
gains without the loss**; the +5/−1 is a map-key conflict, not a source tradeoff.

### Fuzzy cost (small, and recoverable by the same map fix)

Whole-binary size-weighted fuzzy: **33.528053% → 33.523878%** (−0.0042 pp,
≈ −441 weighted bytes). Fully accounted for: lost `248 B × 100%` +
`1180 B × 92.014%` = 1,333 B against 892 B of gains. The 1180-byte
`operator>>` was at **92.014%** at baseline and is now **0%** — not a body
regression, purely unpaired by the key flip.

### ★ Handoff to the map owner — 2 renames, worth +1 strict and the fuzzy back

The map is internally inconsistent for **one type**: 11 of 13
`Target@BandCamShot` rows use the `U` (struct) key, and exactly **2** use `V`.
Our build now emits the `U` counterpart of both, **in the same unit**
(`default/BandCamShot`), verified by dumping the COFF symbol table of
`build/45410914/src/system/bandobj/BandCamShot.obj`:

| map row (`V`, currently 0%) | what we emit (`U`) | expected |
|---|---|---|
| `??0Target@BandCamShot@@QAA@ABV01@@Z` (248 B) | `??0Target@BandCamShot@@QAA@AB**U**01@@Z` | **+1 strict** — it was 100% at baseline and class-vs-struct does not change codegen, so this is a near-guaranteed flip ⇒ total **+5** |
| `??5@YAAAVBinStream@@AAV**V**Target@BandCamShot@@@Z` (1180 B) | `??5@YAAAVBinStream@@AAV0@AA**U**Target@BandCamShot@@@Z` | restores the **92.014%** pairing; a 1180 B function sitting 8 pts from a flip is then a genuine close-out candidate |

This is the *same* defect shape the lane already documented for `??_E`/`??_G`:
a map carrying a key the compiler cannot emit. **Not applied here** — map repair
is a single-owner channel.

### Process note — the stale-obj trap (new, cost one wrong number)

The first baseline read in this worktree was **30,089**, and the naive A/B
therefore said **+16 (17 gained / 1 lost)**. Twelve of those "gains" were in
units the change does not touch (`Task` ×5, `DataFunc` ×2, `DirLoader` ×2,
`DataUtl`, `ContentMgr_Xbox`, `HamNavList`). A **baseline re-measure control**
reproduced exactly those 12 against the *identical* tree — they were stale objs
left behind by the predecessor agent that died mid-build in this worktree, not
effects of the change. ⇒ **When picking up an inherited/orphaned worktree, the
first build is not a baseline.** Re-measure the baseline a second time and
require two identical reads before trusting any delta; a gain in a unit your
diff does not touch is the tell.
## ★ Round 2 — batch 1: the template / STL-instantiation COMDATs

**Date:** 2026-07-26 · **Branch:** `laneAE-misc` · **Baseline: 30,101 strict**
(the `30,102` quoted in the round-1 section above was a stale-obj artifact in an
orphaned worktree — a leftover `build/45410914/src/system/utl/MemTracker.obj`
manufacturing a phantom `('default/MemTracker','?delete_and_clear@AllocInfoVec@@QAAXXZ')`.
Main's own `report.json` and `docs/plans/decomp-state-2026-07-19.md` both read
**30,101**; that is the real landed number and the `+9` above is really `+8`.)

13 candidates, all "the landing unit's obj does not define the target's mangled
name". Measured whole-binary, unit-agnostic `(unit,name)` set diff, two identical
`report.json` reads per leg:

**30,101 → 30,107 = +6 strict, 0 losses.**

| landing unit | symbol | outcome |
|---|---|---|
| `default/Mic` | `vector<unsigned short>::_M_insert_overflow` | **FLIPPED** |
| `default/Mic` | `vector<unsigned short>::_M_fill_insert` | **FLIPPED** |
| `default/Mic` | `vector<int>::_M_insert_overflow` | **FLIPPED** |
| `default/Mic` | `vector<MoveParent const*>::_M_allocate_and_copy<MoveParent const**>` | **FLIPPED** |
| `default/VocalTrack` | `_Deque_base<pair<LightPreset::KeyframeCmd,float> >::_M_initialize_map` | **FLIPPED** |
| `default/band3/bandtrack/Gem` | `__median<AllocInfo*, bool(*)(AllocInfo* const&, AllocInfo* const&)>` | **FLIPPED** |
| `default/VocalTrack` | `vector<FileMerger::Merger>::push_back` | MAP MISPAIR |
| `default/VocalTrack` | `deque<PoolVoice>::push_back` | MAP MISPAIR |
| `default/VocalTrack` | `LightPreset::Keyframe::Save(BinStream&) const` | MAP MISLABEL |
| `default/Synapse_dsp` | `__destroy_range_aux<reverse_iterator<RhythmDetector::Frame*> >` | MAP MISPAIR |
| `default/PanelDir` | `~vector<char const*>` | MAP MISLABEL |
| `default/BandCharDesc` | `__copy_backward_ptrs<SkeletonClip::MoveRating*>` | MAP MISLABEL |
| `default/Text` | `operator>>(BinStreamRev&, Key<vector<Vector3> >&)` | MAP MISLABEL |

### ★ The reusable mechanism split (price this)

Emitting a scattered COMDAT from the landing TU needs **two different shapes**,
and picking the wrong one silently emits nothing:

1. **`inline` free-function / member templates** — `__median`,
   `__copy_backward_ptrs`, `__destroy_range_aux`, `vector<T>::_M_allocate_and_copy`.
   A call site is **inlined away** and no COMDAT appears. Only an
   **explicit instantiation** (`template <ret> ns::fn<Args>(params);`) forces the
   standalone COMDAT. `_M_allocate_and_copy` proved this: an `assign(first,last)`
   ODR-use compiled fine and emitted *nothing*; the explicit instantiation flipped it.
2. **out-of-line members of a class template** — `_M_insert_overflow`,
   `_M_fill_insert`, `_M_initialize_map`, `push_back`. These are declared in the
   class and defined in the `.c` body file, so a plain ODR-use helper
   (`ForceEmit_*`) is enough.

**Verification loop that saves whole builds:** build only the landing `.obj`
(`ninja build/45410914/src/<path>.obj`) and parse its COFF symbol table
(`coff_defined_symbols` in `scripts/harvest/unemitted_symbol_scan.py`) to confirm
the *exact* target name is now defined, before spending an A/B. 12 of 13 were
confirmed emitted this way in a few minutes.

**Trap:** transcribed mangled names are easy to truncate — a hand-typed
`@stlpmtx_std@@IAA…` (missing one `@stlpmtx_std@@`) made a *successful* emission
read as NOT-EMITTED. Always take the name from `report.json`, never retype it.

**Include-collision trap:** `gesture/SkeletonClip.h` is unincludable from
`BandCharDesc.cpp` — it pulls `hamobj/Difficulty.h`, whose `enum Difficulty`
collides with `band3/game/Defines.h`'s, which the `VocalTrack.cpp`
scatter-include already needs. Neither order helps. A minimal local declaration
of just the nested type (the trick already used in `synth_xbox/Synapse_dsp.cpp`)
is enough for an instantiation.

### ★ The 7 map defects — decoded from retail bytes (handoff, report-only)

Round 1 concluded "the dominant root cause is the map, not the source". Round 2
reproduces that at **7 of 13**. All were force-emitted, measured, then **reverted**
(commit `527c896a`) because a force-emit against a mislabeled VA only manufactures
a misleading partial pairing.

**Two map-rename-only flips — the correct name is ALREADY emitted in the right unit:**

| VA / current map name | what the bytes say | why |
|---|---|---|
| `?push_back@?$deque@UPoolVoice@@…` | `?push_back@?$deque@VRangeShift@VocalTrack@@V?$StlNodeAlloc@VRangeShift@VocalTrack@@@stlpmtx_std@@@stlpmtx_std@@QAAXABVRangeShift@VocalTrack@@@Z` | all 3 element-size immediates are `0x18` vs `0x24` for `PoolVoice`; `sizeof(VocalTrack::RangeShift)` = `0x18` (6 floats); target's callee is `?_M_push_back_aux_v@?$deque@VRangeShift@VocalTrack@@…` |
| `?push_back@?$vector@UMerger@FileMerger@@…` | `?push_back@?$vector@V?$deque@PAVTubePlate@@…` | single `addi` immediate differs by exactly 60 = `sizeof(FileMerger::Merger)` − `sizeof(deque<TubePlate*>)`; target's callee is `?_M_insert_overflow_aux@?$vector@V?$deque@PAVTubePlate@@…` |

`band3/bandtrack/VocalTrack.obj` already defines **both** correct names, so these
should be clean +2 from a two-line map edit with **no source change at all**.

**One needs map rename AND a source force-emit:**

`??$__destroy_range_aux@V?$reverse_iterator@PAUFrame@RhythmDetector@@@…` in
`default/Synapse_dsp`: both `subi` immediates are `0xc` in target vs `0x14` for
`RhythmDetector::Frame`, while *every* callee matches (the `vector<Vector3>`
destructor path). `sizeof == 0xc` **and** dtor `== ~vector<Vector3>` ⇒ the element
type is `stlpmtx_std::vector<Vector3>`, **not** `RhythmDetector::Frame`. (Note this
does *not* impeach the in-tree `Frame` layout comment — `0xc` is too small for any
`float` + `vector` layout.) We emit
`__destroy_range_aux<reverse_iterator<vector<Vector3>*> >` only from `ui/UIList.obj`
and `gesture/DepthBuffer3D.obj`, so after the rename someone must also
explicit-instantiate it at the tail of `synth_xbox/Synapse_dsp.cpp`.

**Four outright mislabels — the VA is unrelated code:**

| current map name / unit | what is actually at the VA |
|---|---|
| `??$?5V?$vector@VVector3@@…(BinStreamRev&, Key<vector<Vector3> >&)` · `default/Text` | a **16-byte adjustor thunk**: `lwz r11,-0x4(r3); subf r3,r11,r3; subi r3,r3,0x10; b ?Print@RndTransformable@@UAAXXZ`. Our `operator>>` is 72 B. ICF-folded vtordisp thunk |
| `??1?$vector@PBDV?$StlNodeAlloc@PBD@…` (`~vector<char const*>`) · `default/PanelDir` | calls `?FirstFrom@?$KeylessHash@PBDUEntry@ObjectDir@@@@AAAPAUEntry@ObjectDir@@PAU23@@Z` then `fn_82806F08`; 20 of 21 instructions differ. `ObjectDir` hash code, not a vector dtor |
| `??$__copy_backward_ptrs@PAUMoveRating@SkeletonClip@@…` · `default/BandCharDesc` | opens `subi r31, r12, 0x70` — the MSVC-X360 **EH funclet** prologue (`r12` = parent frame). A funclet, not a copy helper |
| `?Save@Keyframe@LightPreset@@QBAXAAVBinStream@@@Z` @ `0x82ba10c0` · `default/VocalTrack` | a **`_Deque_base` constructor / `_M_initialize_map`-family** function: `li r26,0xa` then `divwu r11,r4,r26`, buffer size `0x78`, `mulli r11,r10,0xc` ⇒ 10 elements of **12 bytes** per node; writes the full `0x28`-byte deque header (`0x0`–`0x24`) into `r3`. There is no `BinStream` operator`<<` call anywhere in it |

★ **The generalisable tell**, cheap and mechanical: for an STL specialization,
**every element-size immediate in the body is a fingerprint of `sizeof(T)`**. If
the immediates disagree with `sizeof` of the mapped `T` by a constant, the map
has the wrong `T` — and the target's *callee* names (which come from the same
map) usually name the right one outright. A scanner over
`diff_arg`-only sub-100 % STL rows that solves for `sizeof(T)` and compares
against `struct_db` would classify this whole class without a build. This is the
concrete instance of the `objdiff pct INVERTS` note that "ARG-ONLY clusters are
MAP MISPAIRS".

## ★★ Round-2 measured outcome — **+43 strict, 1 loss** (30,101 → 30,144)

Verified in `wt-laneAE-emit` after merging all four worker branches, against the
**corrected** 30,101 baseline pickle. Built three times; reads 2 and 3 identical
(30,144 / 30,144). **Every one of the 44 gains is in a unit the diff directly
touches** (the stale-obj tell was explicitly checked and came back 0).

| worker | branch | claimed | shape |
|---|---|---|---|
| camshot | `laneAE-camshot` | +4 (5 gained / 1 lost) | struct-key + Save + 2 scatter force-emits |
| batch 1 | `laneAE-misc` | +6 | template explicit instantiation / ODR-use |
| batch 2 | `laneAE-hmxobj` | +18 | inline ODR-use + scatter-include |
| batch 3 | `laneAE-bigbody` | +15 | scatter-include + unwired-owner wiring |
| **joint** | `laneAE-emit` | **+43 / −1** | **exactly additive — no interaction losses** |

Gains by unit: UISlider 10 · UIListState 7 · Mic 4 · VocalTrack 4 · BandCamShot 2 ·
GemSmasher 2 · Lit_NG 2 · PanelDir 2 · UI 2 · Gem 2 · GemManager 2 ·
BandHighlight 1 · CameraTilt 1 · PreloadPanel 1 · Shockwave 1 · UIListHighlight 1.
Sole loss: `??0Target@BandCamShot@@QAA@ABV01@@Z` — the predicted `V`-vs-`U`
class-key row (map defect, see the handoff below).

★**Additivity is the headline process result.** Four workers' deltas summed
4+6+15+18 = 43 and the joint build measured exactly +43. Emission fixes are
**local to the landing obj** and do not fight each other, unlike layout or
`virtual` changes. Round-1's fear that "adding a symbol perturbs inlining
fleet-wide" is real but *bounded*: it shows up as a same-unit partial regression
the worker can see and fix, not as fleet-wide collateral.

### ★ The premise was half wrong, and that is where the yield was

Round 1 framed this as "inline COMDATs only emitted where ODR-used". **`/O1`
implies `/Gy`, so EVERY function is a COMDAT and retail's linker scattered
out-of-line members too.** Roughly half the ELSEWHERE rows are ordinary
out-of-line members, and the fix for those is `#include "<owner>.cpp"` — which
delivered ~23 of the 43. Operational facts, measured:

- **A scatter-include does NOT require removing the owner from `objects.json`.**
  There is no link edge in `build.ninja`, so duplicate definitions across objs are
  harmless (`UIListWidget.cpp`/`PropSync.cpp` stay wired alongside `PanelDir.cpp`'s
  include of them).
- Only real requirement: rename `gRev`/`gAltRev` when both TUs use `SAVE_REVS`.
- The `PanelDir.cpp` → `UISlider.cpp` include paid **+10**, of which 8 were
  transitive `fn_*` COMDATs pulled in for free. **A scatter-include's yield is the
  whole COMDAT cluster, not the one row you targeted.**
- ★**New lever — when a scatter-include goes net-negative, MINIMISE the included
  set instead of abandoning it.** A `#ifndef RB3_TRACKCONFIG_SCATTER_MIN` guard
  exposing only the 3 scattered definitions recovered two `VocalTrack`
  regressions while keeping all 3 gains, and left `default/TrackConfig` itself
  byte-unchanged.
- **Two emission shapes, and picking wrong silently emits nothing:** `inline` free
  functions / member templates (`__median`, `__destroy_range_aux`,
  `_M_allocate_and_copy`) need an **explicit instantiation** — a call site gets
  inlined away and emits nothing; out-of-line members of a class template
  (`_M_insert_overflow`, `push_back`) are fine with a plain ODR-use helper.
- **Cheap pre-flight that avoids wasted A/Bs:** build only the landing `.obj` and
  parse it with `coff_defined_symbols`. 12 of 13 candidates confirmed in minutes.
  **Never retype a mangled name** — one hand-typed name missing a `@stlpmtx_std@@`
  made a successful emission read as NOT-EMITTED.

### ★ Second new lever — FMA contraction broken via named-object members

Retail emits 4×`fmuls` then 4×`fadds`; MSVC X360 `/O1` contracts plain float
locals into `fmadds`. Routing products through the **members of a named object**
breaks the expression tree so the backend peephole cannot re-fuse:
```cpp
Hmx::Color diff;
diff.red = (mEndsTint.red - mRootsTint.red);
diff.red = diff.red * fShell;   // separate fmuls, not fused into the later fadds
```
This took `NgFur::Shell` (816 B) from 96.85% → **100%** in one edit.
★**Measured negative: `#pragma fp_contract(off)` is inert at these flags** —
byte-identical output, no `C4068`. `docs/decomp/XBOX360_FLOATING_POINT_CODEGEN.md`
"Strategy 1" is wrong for this toolchain; **do not re-fund it**.

### Unwired owners confirmed (the VENDOR-UNWIRED vein is real)

`src/system/rndobj/Fur_NG.cpp` has real `NgFur::Prep`/`Shell` bodies and is
**absent from `objects.json`**; "emitted by `rtti`" was because
`src/xdk/LIBCMT/rtti.cpp` already scatter-includes it. Scatter-including it into
`Lit_NG.cpp` flipped `Prep` and moved `Shell` 0% → 96.85%, then the FMA lever
closed it. A read-only triage of the 302 NOWHERE rows confirmed **5 unwired-TU
rows**: `FxSendPitchShift360.cpp`, `FxSendSynapse360.cpp`, `DrawUtl.cpp`,
`PlatformMgr_Xbox.cpp` (file-level only; its pool row is a map mislabel), and
`ScrollbarDisplay` (needs a real class port — currently a 1-line fake stub in
`Band.cpp`). Strongest single remaining source row:
`FilterCoeffs::Low/HighpassCoefficients` (692 B combined) — our file is a 1-line
empty stub and dc3's is a same-unit drop-in.

### Honest residual size of the actionable class

Of the 302 NOWHERE rows: VECDTOR 25 (map defect, dead) · MAP-ARTIFACT 21 ·
STL-INST 96 · VENDOR-UNWIRED 93 (+~6 mis-tagged) → **67 rows / 12,332 B
remainder**, of which 11 are DEAD by an already-measured trap and ~9 are
SUSPECT-MISPAIR. **Honest actionable remainder: ~16 high-confidence rows /
~2,600 B.** Full triage: `docs/plans/lane-ae-nowhere-triage-2026-07-26.md`.

## ★ Map-owner handoffs from round 2 (report only — NOT applied)

1. **Guaranteed free flip.** `0x82813190` in `default/UIPanel` is mapped
   `?Enter@RndPollable@@UAAXXZ` (160 B) but the body is
   `static Message msg("finish_load"); HandleType(msg); …; mState = kDown` =
   **`?FinishLoad@UIPanel@@UAAXXZ`**, which has **no map entry at all**. Source
   order corroborates (`IsLoaded` 0x82813040 < 0x82813190 < `Enter` 0x82813258,
   matching `UIPanel.cpp`'s IsLoaded/FinishLoad/Exiting/Enter order). We already
   compile the body — a one-line repair.
2. `0X8240EA08` → `?Enter@RndPollable@@…` (40 B) is an **EH funclet**
   (`subi r31,r12,0x70` before `mflr`).
3. `0X8240F158` → `?PropExceptionID@PropKeys@@…` (8 B) is an **adjustor thunk**
   (`addi r3,r3,0x154; b …`).
4. ★**Cheap lint with a 2-for-2 hit rate:** the map has exactly **11 uppercase
   `0X`-prefixed keys** out of 21,767, and **both** that landed in this pool are
   defects (#2, #3). Audit all 11.
5. **Wrong template argument `T` — a whole new defect family.** Four rows where
   the map names the wrong element type, provable from `sizeof(T)` immediates:
   `?push_back@?$deque@UPoolVoice@@…` is really
   `deque<VocalTrack::RangeShift>` (all 3 element-size immediates `0x18`, not
   `0x24`; callee is the `RangeShift` `_M_push_back_aux_v`);
   `?push_back@?$vector@UMerger@FileMerger@@…` is really
   `vector<deque<TubePlate*> >` (one `addi` off by exactly
   `sizeof(Merger) − sizeof(deque<TubePlate*>)`);
   `__destroy_range_aux<reverse_iterator<RhythmDetector::Frame*> >` is really
   `vector<Vector3>` (`0xc` vs `0x14`). The first two are **rename-only ⇒ +2 with
   zero source change**. ★**Proposed scanner:** for any sub-100% STL row whose
   mismatches are `diff_arg`-only, solve the immediates for `sizeof(T)` and
   compare against the mapped `T` in `struct_db` — a constant disagreement means
   the map has the wrong `T`, and the target's callee names usually name the right
   one outright. Classifies the family with no build.
6. **Outright mislabels** (unrelated code at the VA, verified by decoding):
   `default/Text` `operator>>(BinStreamRev&, Key<vector<Vector3> >&)` → a 16-byte
   adjustor thunk tail-calling `?Print@RndTransformable@@UAAXXZ`;
   `default/PanelDir` `~vector<char const*>` → calls
   `?FirstFrom@?$KeylessHash@PBDUEntry@ObjectDir@@@@…`;
   `default/BandCharDesc` `__copy_backward_ptrs<SkeletonClip::MoveRating*>` → an
   EH funclet prologue; `default/VocalTrack`
   `?Save@Keyframe@LightPreset@@QBAXAAVBinStream@@@Z` @ `0x82ba10c0` → a
   `_Deque_base` ctor (10 × 12-byte elements, no `BinStream` call anywhere).
7. `?GetTrackOrder@TrackPanel@@…` has **no map entry for that exact mangling**;
   the map holds a `…_N@Z` (extra trailing `bool`) variant at `0x82b93c78`.
8. **`BandCamShot::Target` class-key inconsistency** — 11 of 13
   `Target@BandCamShot` rows use the `U` (struct) key, exactly 2 use `V`. We now
   emit the `U` counterpart of both **in the same unit** (verified by dumping
   `BandCamShot.obj`'s COFF symbol table). Renaming
   `??0Target@BandCamShot@@QAA@ABV01@@Z` → `…ABU01@@Z` recovers the lane's only
   loss (class-vs-struct does not change codegen) ⇒ **+1**, and renaming
   `??5@YAAAVBinStream@@AAV0@AAVTarget@BandCamShot@@@Z` → `…AAUTarget…` restores a
   1180 B function to its 92.014% pairing, 8 points from a flip.
9. **21 duplicate-target-name-in-unit groups** (24 fns) — deduplicate; see the
   round-2 funnel section.

### Tooling note (jeff / scattered units)

For scattered units, dtk's `fn_<VA>` symbol **names do not correspond to the VA of
the body they cover** (e.g. `.fn fn_82321590` in `CameraTilt.s` holds the body at
`0x82280F8C`). Pairing still works because the map and dtk agree on the *name*,
but the `.s` address column is **not** usable to sanity-check a map VA. Belongs in
`docs/plans/jeff-scattered-unit-addresses.md`.

### Not re-funded (measured negatives, round 2)

- `default/UILabel` ← `movie/Movie.cpp` scatter-include: `?IsLoading@Movie@@`
  reached only 99.8% while `fn_827F765C` fell 100 → 92.5. **Net −1, reverted.**
- `DefaultPhysicsManager` ctor (Gem) paired at only 16.96%, `VocalGuidePitch` ctor
  (TrackerDisplay) at 2.0% — both **reverted**: a whole-TU include is not worth
  sub-20% pairing. Their blocker is body divergence, not emission; a body-port
  lane can re-add the one-line include for free.
- `?QueueEnumJob@PlatformMgr@@` — target does `lwz r3,0x90(r3)` but
  `PlatformMgr.h` puts `mJobMgr` at `0x34`; a layout divergence, so emitting it
  cannot reach 100% as-is.
