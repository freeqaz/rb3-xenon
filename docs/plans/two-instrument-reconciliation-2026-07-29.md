# The two-instrument reconciliation — when the string anchor and the RTTI span disagree (laneBO2 lane C, 2026-07-29)

laneBL's `docs/plans/tu-pin-wave-2026-07-29.md` §6.1 flagged two rows where the
class-name string anchor lands **outside** the RTTI-derived span
(`BandFaceDeform`, `ReviewDisplay`) and deferred them with a one-sentence
hypothesis:

> *"the RTTI span comes from vtable slots (which for a derived class can sit in a
> base's TU) while the class-name literal is in the TU's own `StaticClassName`."*

This document is the measured reconciliation. **laneBL's stated hypothesis is
REFUTED on both rows**, and it is refuted *by construction*, not by accident —
see §2. The mechanism that actually fired is different on each row, which is the
point: there is no single tie-breaker, only a decision procedure.

Everything below was read out of `orig/45410914/band.exe` (imagebase
`0x82000000`) with `scripts/harvest/tu_locate/vt_rtti_scan.py` (retail COL →
vtable → slot list) plus capstone disassembly. Oracles corroborate only.

---

## 1. The candidate mechanisms

| | mechanism | which side moved |
|---|---|---|
| (a) | laneBL: a derived class's vtable slot resolves into a **base's** TU | RTTI |
| (b) | §3.0: an **ICF fold** puts a class-owned slot in an unrelated TU | RTTI |
| (c) | §5.3: the 88-byte `?StaticClassName@<Class>@@` COMDAT is grouped into a linker **scatter block** | string |
| (d) | §3.1: the span simply **under-covers at the head/tail** and the anchor is a real TU body just outside it | neither — the *span* is short |

(d) is not in laneBL's list and is the one that fired on `BandFaceDeform`.

---

## 2. ★★ `foldN` kills (a) and (b) in one measurement

For a slot VA *v*, define **`foldN(v)` = the number of distinct classes whose
retail vtables contain *v*** (computable directly from `vt_rtti_scan.py`'s
`vtables.json`).

* `foldN(v) ≥ 2` ⇒ *v* is shared — an inherited base implementation, or an ICF
  fold. It is **not** a locator. This is mechanism (a) **and** (b) at once: both
  are "the slot is not uniquely this class's".
* `foldN(v) == 1` ⇒ *v* appears in exactly one class's vtable, so it is that
  class's own, un-folded override. **An override cannot live in the base's TU**
  — the base's translation unit cannot define a function that overrides itself.

★ **So (a) is not a separate mechanism at all. It is the `foldN ≥ 2` case, and
laneBD's `owned_slots` count was already supposed to exclude it.** The reason
laneBL's hypothesis reads plausibly is that the failure it describes is real —
it just cannot coexist with a correctly-computed "class-owned" slot.

Measured on both rows: **every slot that sets an endpoint has `foldN == 1`.**

```
BandFaceDeform   vt 0x820201AC (21 slots)   un-folded own slots, all foldN=1:
  [0] 822C8380  76   [4] 822C8130  48   [5] 822C8168 252   [6] 822C8A48 352
  [7] 822C8600 324   [8] 822C7768 100   [9] 822C8310 108  [10] 822C89C8 120
  -> all inside laneBD's RTTI span 0x822C7768..0x822C8BA8.  Span is SOUND.

ReviewDisplay    RndDrawable vt 0x82031FFC  [5] 8231EC58 240, [18] 8231E588 96,
                                            [19] 8231F188 260   (all foldN=1)
                 RndPollable vt 0x82031FDC  [0] 8231EC08 80, [1] 8231E6A8 68
```
The shared slots in the same vtables are exactly the ones you would expect and
they carry huge `foldN`: `0x826C3888` (the ICF empty stub) `foldN=858`,
`0x823591E8` `foldN=973`, `Object::SetName 0x8275A5C0` `foldN=584`.

**Verdict: (a) REFUTED on both rows. (b) REFUTED on both rows.**

---

## 3. ★★ The scatter filter — a self-contained test on the STRING side

Independently reproduced from `band.exe` (and matching the lead's numbers
exactly). Enumerate every `.text` site referencing a class's own name literal:

```
classes with .?AV descriptor : 1,127
class-name code sites        :   513
in a scatter block           :   348  (68 %)
isolated                     :   165  (32 %)
scatter blocks (>=3 classes) :    37
```
Cluster the 513 sites by 4 KB window. **A window holding ≥ 3 distinct classes'
own-name references is a linker scatter block** — the `?StaticClassName@<Class>@@`
header-macro COMDATs of unrelated classes grouped together.

> **Rule: a class-name anchor locates a TU only if it is ISOLATED. If ≥ 3
> distinct classes reference their own names from the same 4 KB window, none of
> those sites locates anything.**

★ This is a **partial refutation of laneBL §5.3 in the useful direction.**
laneBL concluded "treat an 88-byte class-name anchor as an existence proof only",
i.e. discard them all. That over-corrects: 165 of 513 are isolated and *do*
locate. The filter needs no RTTI, no pins and no oracle — one pass over the
string-xref table. Tool: `scripts/harvest/tu_locate/str_scatter.py`.

Applied to the two rows:

```
BandFaceDeform  8227A564  window 8227A000 holds 20 classes  -> SCATTER, discard
                          (BandCamShot, BandConfiguration, BandIKEffector,
                           BandLeadMeter, BandRetargetVignette, BandScoreboard,
                           BandSongPref, BandStarDisplay, CharKeyHandMidi,
                           ChordShapeGenerator, CrowdMeterIcon, ...)
                822C72D8  window 822C7000 holds  1 class    -> ISOLATED, locator
ReviewDisplay   8231E48C  window 8231E000 holds  1 class    -> ISOLATED
                8231F5F4  window 8231F000 holds  2 classes  -> ISOLATED
```

**So the filter resolves `BandFaceDeform` outright and is SILENT on
`ReviewDisplay`** (two survivors, and it never separates survivors).

---

## 4. Per-row verdicts

### 4.1 `BandFaceDeform` — mechanism (c) on the §5.3 anchor, (d) on the real one

laneBL §5.3 listed two anchors, `0x8227A528` and `0x822C7298`, and treated the
88-byte one as the shape to distrust. Both halves of that are right, but for
`0x822C7298` the "584 B, not 88 B" note in the assignment was the real tell:

* `0x8227A528` (88 B, claimed by `BandCharacter.cpp`, **316 KB** from the RTTI
  span) disassembles as the textbook `?StaticClassName@BandFaceDeform@@`:
  guard-bit test, `Symbol::Symbol(&static, "BandFaceDeform" @0x8200FEE0)`,
  return. It sits in the 20-class `0x8227A000` scatter block. **Mechanism (c) —
  the string side wandered. Existence proof only, never a locator.**
* `0x822C7298` (584 B, unclaimed) is **`BandFaceDeform::DeltaArray::AppendDeltas`**.
  Proof, from the disassembly, four independent ways:
  1. it holds the `"BandFaceDeform"` pointer in a callee-saved register across
     the loop and passes it to `0x827BD230` (`MemMgr`) — i.e. it is the
     `const char *name` **pool-allocation tag** of the oracle's
     `MemResizeElem(void*&, int&, void*, int, int, const char*)`, not a class-name
     `Symbol` at all;
  2. four function-local statics are updated at `-0x2f74/-0x2f70/-0x2f6c/-0x2f68`
     off `0x82CD0000` — exactly the oracle's `total`, `maxDelta`, `totalRuns`,
     `totalLength`;
  3. the inner loop packs **3 bytes per vertex** (`addi r29, r29, 3`) and tracks a
     running `fabs` maximum — the oracle's delta encoder verbatim;
  4. after pinning, `?Load@DeltaArray@BandFaceDeform@@` at `0x822C7128` (the
     function immediately below) came out **100 % reloc-masked byte-identical**
     on the first compile.

  **Mechanism (d) — the anchor is a real TU body `0x4D0` BELOW the RTTI lo. The
  two instruments never disagreed; the RTTI span under-covers at the head.**

★ Both instruments were partly right and the *apparent* disagreement was an
artifact of laneBL's §5.3 list conflating two anchors of completely different
shape under one row.

### 4.2 `ReviewDisplay` — mechanism (c), decided by the ORDER channel

The scatter filter is silent (both sites isolated). Disassembly separates them:

* `0x8231E450` (88 B, inside `StarDisplay.cpp`'s pin) **is**
  `?StaticClassName@ReviewDisplay@@` — guard test, `Symbol::Symbol(&static,
  "ReviewDisplay" @0x82031F30)`, return.
* `0x8231F5C8` (88 B, unclaimed) is **`ReviewDisplay::Init()`**: it *calls*
  `0x8231E450`, then registers the factory `0x8231F048` and calls
  `TheUI->InitResources`. `size_order_automap` independently paired it
  `?Init@ReviewDisplay@@SAXXZ` at 100 % byte identity.

So two functions reference the same literal and only one is the header macro.
**Mechanism (c): the `StaticClassName` COMDAT wandered into `StarDisplay`'s
neighbourhood.** The lead's typedesc-order bracket (lower bound `0x8231E8EC`
from the spatially-coherent `StarDisplay` anchor) independently rejects
`0x8231E450` and accepts `0x8231F5C8` — the two channels agree.

★ And the RTTI span is wrong here **in the opposite direction from
`BandFaceDeform`**: laneBD's lo `0x8231E4C8` sits inside `StarDisplay`'s pin,
i.e. `ReviewDisplay` **over-covers at the head** while `BandFaceDeform`
**under-covers at the head**. Two rows, two opposite span errors, two different
string-side mechanisms.

---

## 5. ★ The reusable rule

Given a class-name anchor set `A` and an RTTI span `[lo,hi)` that disagree:

0. **Is the string channel even applicable?** Some classes have no class-name
   literal in the image at all (`CharMeshCacheMgr`, `TourPropertyCollection`,
   `DrumTrackWatcherImpl` — 0 sites each). Then steps 1–3 are inapplicable *by
   construction*, not weak. Go to 4.
1. **Enumerate ALL class-name code sites** — there is usually more than one, and
   laneBL's §5.3 list mixed shapes within a single row.
2. **Scatter filter** (`str_scatter.py`): discard any site whose 4 KB window
   holds ≥ 3 distinct classes' own-name references. One survivor ⇒ done.
3. **Shape test on the survivors.** An 88-byte `guard-test → Symbol::Symbol(&static,
   "<Class>") → return` body is `?StaticClassName@<Class>@@` and is an existence
   proof only. **Anything that is not that shape is a candidate TU body and must
   be identified by what it does, not by its size.** Where two survivors remain,
   the order bracket (`td_order.py`, ~4:1, weakest in its narrow regime)
   discriminates.
4. **Then, and only then, test the RTTI span** — compute `foldN` for every
   endpoint-setting slot. `foldN ≥ 2` ⇒ discard the slot (this covers laneBL's
   (a) and §3.0's (b) simultaneously). `foldN == 1` ⇒ the slot is a genuine
   own override and the span is sound at that end.
5. **Record which step fired.** On these two rows it was step 2 for
   `BandFaceDeform` and step 3 for `ReviewDisplay`; step 4 exonerated the RTTI
   side both times.

★ **The headline finding is not which hypothesis won — it is that neither
instrument is the tie-breaker in general. You must first ask which one is even
applicable.** `BandFaceDeform` and `ReviewDisplay` are exact inverses of each
other on that question, and three of the five rows in this lane had no string
channel at all.

---

## 6. Other refutations recorded by this lane

* **laneBL §6.1's `DrumTrackWatcherImpl` row is STALE.** laneBL's own lane C had
  already landed `0x827800B0..0x82780130` + `0x82780150..0x827808C0` in main
  `9df262c9`, and the unit sits at 6/10 strict, 97.37 % fuzzy. Its "1,576 B /
  4 fns, 98 % unclaimed" figure describes the *pre-laneBL* state. **Remaining
  headroom is porting (4 near-misses: 99.93 / 99.84 / 94.7 / 91.2 / 84.1 %), not
  pinning.** Re-pinning would have created exactly the §7bis silent
  duplicate-range corruption.
* **laneBL §9's unassigned `TourQuestGameRules` ADD `0x82365BC0..0x82365CCC` is
  REFUTED.** `TourQuestGameRules`' own two vtable slots (`foldN=1`, so provably
  its own) are `0x82365D60` and `0x82365DF0` — **above** that range and inside
  `FileMergerOrganizer.cpp`'s pin. The proposed range also overlaps laneBL's own
  `TourPropertyCollection` run at `0x82365BC0..0x82365C20`. Whoever takes
  `TourQuestGameRules` should carve `≈0x82365CD8..0x82365E3C` out of
  `FileMergerOrganizer.cpp`, not ADD at `0x82365BC0`.
* **`HamScrollSpeedIndicator` is a PHANTOM — a fifth one.** The byte string
  `HamScrollSpeedIndicator` occurs **zero** times in `band.exe`; so do
  `HamScroll` and `.?AVHamScrollSpeedIndicator@@`; it has **zero** vtables and
  **zero** un-folded own slots — laneBD §4b-bis's predictor firing exactly as
  documented. Its pin `0x8231EF28..0x8231F130` (520 B) is therefore 100 %
  foreign, and it demonstrably holds `ReviewDisplay` code:
  `?Init@ReviewDisplay@@` at `0x8231F5C8` registers the factory `0x8231F048`,
  which is inside that pin. `HamScrollSpeedIndicator.cpp` has a second block, so
  carving it does not fire the empty-unit trap.
* **`Mesh.cpp`'s micro-pin `0x8231EA20..0x8231EAA8` (136 B) is an island** —
  `RndMesh`'s own un-folded slots are all at `0x8241C598..0x824213E8`, ~1.0 MB
  away (§5.0's corollary). Same shape for the `MeshAnim.cpp` / `MatAnim.cpp` /
  `PropAnim.cpp` blocks interleaved through `BandFaceDeform`'s span: those three
  classes' own un-folded slots live at `0x8246DF90+`, `0x82461FD0+`, `0x82426CE0+`
  — 1.6–1.9 MB away — while `BandFaceDeform`'s slots `[4] [5] [6] [7] [10]` sit
  *inside* their pins.

---

## 7. Retail-vs-Wii-dev divergences added by this lane

* ★★ **`SAVE_OBJ` is suspect by default — FOURTH confirmed instance.**
  `BandFaceDeform::Save` is `SAVE_OBJ(BandFaceDeform, 0x129)` in the Wii dev tree
  (an unconditional `MILO_ASSERT(0)`); retail `0x822C7768` is a real
  `SAVE_REVS(0,0)` body — packed rev 0 through `BinStream::WriteEndian`, chain to
  `Hmx::Object::Save`, then stream `mFrames` from `this+0x28`. **4.0 % → 100 %.**
* ★ **A temporary that only names a member changes evaluation order.**
  `ReviewDisplay::DrawShowing`: the Wii source has
  `RndAnimatable *focus = mFocusAnim; focus->SetFrame(cond ? 1.0f : 0.0f, 1.0f);`
  Retail evaluates the ternary *before* loading the member, i.e. writes
  `mFocusAnim->SetFrame(...)` with no temporary. **91.0 % → 100 %.**
* ★ **Local-static `Symbol`s again** (laneBL §7's lever, third TU).
  `ReviewDisplay::Update` spells `review_anim` and `focus_anim` as
  function-local statics, not the `utl/Symbols*.h` globals: guard bits 0 and 1 of
  one guard word `0x82CBDE48`, storage `0x82CBDE44` / `0x82CBDE40`.
  **50.7 % → 100 %.** Both names *also* exist as globals (`Symbols4.h`,
  `Symbols2.h`), so §7's shadowing trap is live here — each declaration must sit
  immediately before its use.

---

## 8. What remains

* `BandFaceDeform` — the three-donor carve out of `MeshAnim.cpp` / `MatAnim.cpp`
  / `PropAnim.cpp` for slots `[4] [5] [6] [7] [10]` plus `?ClassName`/`?SetType`.
  Evidenced (all `foldN=1`) but **not loss-proof**; ~6 more functions.
  `0x822C7680..0x822C76E4` (`SongLayout.cpp`, 100 B) is
  `operator<<(BinStream&, const vector<DeltaArray>&)` — called by
  `BandFaceDeform::Save` — so that micro-pin is a mis-attribution too.
* `ReviewDisplay` — the `StarDisplay.cpp` tail carve `0x8231E450..0x8231E8EC`
  (10 of its 18 un-folded own slots, incl. `CopyMembers`/`Enter` and 8 thunks)
  and the whole phantom `HamScrollSpeedIndicator` pin. `ReviewDisplay::Poll`
  residual is one signed-vs-unsigned compare (`cmplwi` vs `cmpwi`) —
  permuter-class, permuter banned.
* `ReviewDisplay`'s offset-336 adjustor-thunk vtable was **declined**: its
  `[16]`/`[20]` fixed points hold (`0x8275A5C0` / `0x8275A9D0`) but `[15]` does
  not resolve to `Object::SetTypeDef`, so per §4.1 rule 4 the alignment is not
  trusted and the 13 thunk names were not guessed. *Name it or decline it.*
* `TourPropertyCollection` — `BandSongMetadata.cpp` `0x82365510..0x82365560` and
  `0x823658F8..0x82365974`, and the `LicenseMgr.cpp` island
  `0x82365668..0x8236570C`, all sit inside its bracket. `0x82365C20..0x82365CCC`
  is unclaimed and **declined** — it is between `TourPropertyCollection` and
  `TourQuestGameRules` and this lane could not name it.
