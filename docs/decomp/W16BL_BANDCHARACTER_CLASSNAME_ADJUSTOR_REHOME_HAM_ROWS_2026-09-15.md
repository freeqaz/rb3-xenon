# W16-BL — `0x82289748` is `BandCharacter::ClassName`'s adjustor thunk, and CY-1 moved it to `Line.cpp:` because a masked byte test on a thunk is content-free

Lane W16-BL. Worktree `~/tmp/wt-w16-bl`, branch `w16-bl`, based on main `312da843`.
Ruler `name_check` (shipped default), objdiff 4.2.9.

## 0. Baseline — verified in-tree, reproduces the briefed ledger figure exactly

Settled to a zero-work build first (build 0 did 397 edges — a fresh reflinked
worktree's split/renamer chain; build 1 did **5**, all always-run check/progress
phonies, `symbols.txt` clean). Only then read:

| key | value |
|---|---|
| `matched_functions` | 43,552 |
| `matched_code` | 4,040,380 |
| `matched_code_percent` | 39.42962 |
| `fuzzy_match_percent` | 49.699745 |
| `total_functions` | 69,240 |
| `total_code` | 10,247,068 |
| `masked_equal_functions` | 23,076 |
| rows at `fuzzy == 100` | 40,887 |

`rowset_snapshot.py` leg A vs the briefed baseline copy `~/tmp/rows_w16bl_base.json`
(a copy of `~/tmp/rows_w16bh_main.json`, original untouched): **40,887 rows both
sides, 0 gained, 0 lost.** The briefed baseline is therefore verified literally,
not inherited.

## 1. PRE-REGISTERED PREDICTION (written and committed BEFORE the build)

Edit: move splits `.text 0x82289748–0x82289754` from `Line.cpp:` to
`BandCharacter.cpp:` (by merging it back into the block CY-1 carved it out of),
and set map `0x82289748` → `?ClassName@BandCharacter@@$4PPPPPPPM@A@BA?AVSymbol@@XZ`.

| measure | before | predicted after | Δ |
|---|---|---|---|
| `matched_functions` | 43,552 | **43,553** | **+1** |
| `matched_code` | 4,040,380 | **4,040,392** | **+12 B** |
| `matched_code_percent` | 39.42962 | ≈39.429632 | ≈+0.000012 pp |
| `total_functions` | 69,240 | 69,240 | **0** (re-home, the row exists either way) |
| `total_code` | 10,247,068 | 10,247,068 | **0** |
| `masked_equal_functions` | 23,076 | 23,076 | **0** (the row pairs by NAME, not byte signature) |
| `default/Line` | 77 fns / 13,040 B / 66 matched / 8,360 B | 76 / 13,028 / 66 / 8,360 | −1 fn, −12 B denominator |
| `default/BandCharacter` | 619 fns / 70,680 B / 536 matched / 48,312 B | 620 / 70,692 / 537 / 48,324 | +1 fn, +12 B both sides |

Set-diff prediction: **exactly one GAINED row**,
`default/BandCharacter::?ClassName@BandCharacter@@$4PPPPPPPM@A@BA?AVSymbol@@XZ`
(12 B), and **zero LOST rows** — `fn_82289748` sits at `fuzzy 0` today so it was
never a member of the `fuzzy == 100` set.

If the new row lands below 100 the only possible charge is the relocation NAME of
the branch target (an adjustor thunk is `lwz`/`subf`/`b` and the `b` is the sole
relocated word); both sides spell `?ClassName@BandCharacter@@UBA?AVSymbol@@XZ`, so
I expect no charge.

## 2. Why the prediction was safe to make — three preconditions TESTED, not assumed

**(a) The address is pinned, and BH's write-up is wrong about it.** W16-BH §8.1
says `0x82289748` is "**UNPINNED** … it sits in the 12-byte gap between the
`BandCharacter.cpp:` blocks". It is not: it is pinned at `splits.txt:5162` under
`Line.cpp:`, and `default/Line` carries the row as `fn_82289748`, 12 B,
`fuzzy 0`. Read the file, not the write-up — this is exactly the error class the
brief's "test every briefed figure literally" rule exists for, and the brief
called it correctly.

**(b) Our compiled object defines the name.** `objdiff` pairs target↔base by
name, so a rename whose name the base obj cannot define turns a matched row into
a permanent 0 (`pin-neutrality-scoped-2026-08-14.md`). Read from the COFF symbol
table of `build/45410914/src/system/bandobj/BandCharacter.obj` **after** a build
(a fresh worktree's reflinked objs are pre-renamer): 12,469 symbols, including
`?ClassName@BandCharacter@@$4PPPPPPPM@A@BA?AVSymbol@@XZ`. ✅

**(c) `Line.cpp:` does not drain.** It holds four `.text` blocks; removing one
leaves three, so the "a move that drains a unit's LAST `.text` block must delete
the whole entry, else `report.json` hard-fails on a 42-byte obj" trap does not
fire. Both headings are also unique — 0 exact-duplicate headings of 1,291, and
each of `Line.cpp:` / `BandCharacter.cpp:` resolves to exactly one heading, so
the bare-vs-path-qualified `basename()` trap does not apply.

## 3. The retail evidence: `0x82289748` is `BandCharacter::ClassName`'s adjustor

`tools/retail_body.py` over `orig/45410914/band.exe`:

```
82289748  lwz   r11, -4(r4)
8228974c  subf  r4, r11, r4
82289750  b     0x822896e0   ; ?ClassName@BandCharacter@@UBA?AVSymbol@@XZ
```

A `$4PPPPPPPM@A@` virtual-base adjustor thunk. The adjustment is applied to **r4,
not r3**, which is itself a consistency check rather than an oddity: the function
returns `Symbol` by value, so r3 carries the hidden return slot and `this` is in
r4. Control, on the row W16-BH repaired: `0x8227b050` is the identical shape
branching to `0x8227a910` = `?ClassName@BandSong@@UBA?AVSymbol@@XZ`.

**The destination IS the identification, and I re-derived that rather than
inheriting it.** Scanning the whole `.text` section (`0x82270000`, 10,342,400 B)
for the three-word pattern finds **504 adjustor thunks with 504 DISTINCT
destinations — zero collisions**. `/OPT:ICF` therefore cannot have folded two
adjustors together, and exactly one thunk targets `0x822896e0`: `0x82289748`.
(W16-BH reported 452 over a narrower extent and reached the same conclusion; the
figure differs, the verdict does not.)

Corroboration from the map's own neighbourhood, which needed no new work:
`0x82289718` is `??_EBandCharacter@@$4PPPPPPPM@HPE@AAPAXI@Z` and `0x82289758` is
`??_EBandCharacter@@WCGI@AAPAXI@Z` — the moved block sits **inside a run of
BandCharacter thunks**. Every `Band*` class in the map carries a
`$4PPPPPPPM@A@` adjustor beside its `ClassName` primary (BandDirector
`0x822970b8`/`0x822970e8`, BandSwatch, BandCamShot, BandCrowdMeter, …);
`BandCharacter`'s was the one missing.

## 4. What lane CY-1 got wrong, and why its instrument could not have known

`f592571a` (2026-08-02) did **not** pin this address fresh. It **carved** the
12 B out of BandCharacter's own contiguous `0x82289710–0x8228A23C` block,
splitting it into `0x82289710–0x82289748` + `0x82289754–0x8228A23C`, and moved
the middle to `Line.cpp:`. It never named the address — the map row stayed
`null` until today.

**The mechanism.** CY-1's own commit message records that `xbin_adjudicate` is
"structurally blind on 4/12/16 B thunks (no `RUNTIME_FUNCTION`)" and that it
substituted **SMALLPRED** — the same reloc-**masked** byte-identity test with the
dtk section size as length. On this stratum that test is *structurally
content-free*: `default/Line` holds a run of **ten** 12-byte
`RndLine::*$4PPPPPPPM@A@` adjustor thunks (`Highlight`, `??_E`, `ClassName`,
`SetType`, `Print`, `SyncProperty`, `Save`, `Handle`, `Copy`, `Load`), all at
100%, and every one is byte-identical to BandCharacter's ClassName adjustor
**except for the single word the mask removes** — the relocated branch
destination. A masked comparison on a single-`b` thunk compares 8 fixed bytes and
nothing else, so it "matches" `Line.cpp:` **by construction**.

This is the in-tree doctrine restating itself: the map's own
`_single_branch_thunk_misnames_comment` says "a single-`b` 4-byte thunk is
RELOCATION-MASKED BYTE-IDENTICAL to every other single-`b` thunk in the binary —
the branch destination IS the relocation — so the whole class collapses into ONE
equivalence class for any byte-identity bijection", and measured that cohort at
**88.9% defective** for the two bulk lanes. SMALLPRED's advertised 95.95%
positive rate was measured over its whole 3,458-row population, not over the pure
thunk stratum where it carries no information — CLAUDE.md's "a per-stratum rate
from a MIXED population does not transfer to a PURE one", instantiated.

**Three refutations were available at the time**, and are worth recording because
each is cheap and none required the retail disassembly:
1. the carve left a 12-byte **hole inside a contiguous range**. CY-1's drain gate
   checked only whether a move *emptied* a block — punching a hole in one was
   unguarded. A hole is a much stronger signal than a byte match is.
2. the branch destination (504/504 distinct) settles it outright.
3. the map neighbours on **both** sides are BandCharacter thunks.

## 5. Geometry choice, and the collateral measurement

I **merged** CY-1's two carved blocks back into one
`.text 0x82289710 end:0x8228A23C` rather than adding a third contiguous `.text`
line under `BandCharacter.cpp:`. Reasons, in order: it is the exact inverse of the
bad carve and therefore restores a **known-good pre-CY-1 geometry**; it
introduces **no new block boundary** for dtk to carve across (extra boundaries
are where the tail-merge / over-carve behaviour and "ends within symbol" failures
bite, and `symbols.txt` is simultaneously an input and an output of the split);
and fewer blocks means less of dtk's synthetic multi-block address distortion.
The conservative alternative (leave both lines, add a third) was held in reserve
in case the merge perturbed neighbouring carves — **it did not: 0 rows fell out**,
so the fallback was never needed.

**No `.pdata` regenerated**, which is a consistency check rather than a surprise:
a 12-byte thunk has no `RUNTIME_FUNCTION`, so the moved block never had a `.pdata`
range, and merging two adjacent `.text` blocks re-derived byte-identical ranges.
`symbols.txt` did not change on any build.

## 6. Prediction vs measurement — 5/5 headline keys and 8/8 per-unit figures

Builds: `touch config/45410914/config.yml` → full `./tools/ninja-locked` (rc=0,
11 edges: SPLIT + renamer + REPORT) → second full build **5 edges, all
always-run phonies** = `symbols.txt` fixed point. Ruler `name_check`, objdiff
4.2.9. Never `ninja <one>.obj`, never `objdiff-cli --build`.

| measure | before | PREDICTED | MEASURED | verdict |
|---|---|---|---|---|
| `matched_functions` | 43,552 | **43,553** | **43,553** | ✅ HIT |
| `matched_code` | 4,040,380 | **4,040,392** | **4,040,392** | ✅ HIT (+12 B) |
| `total_functions` | 69,240 | 69,240 | 69,240 | ✅ HIT |
| `total_code` | 10,247,068 | 10,247,068 | 10,247,068 | ✅ HIT |
| `masked_equal_functions` | 23,076 | 23,076 | 23,076 | ✅ HIT |
| `matched_code_percent` | 39.42962 | ≈39.429632 | **39.429737** | ⚠ see below |
| `fuzzy_match_percent` | 49.699745 | — | 49.699863 | — |
| `default/BandCharacter` | 619 fns / 536 / 70,680 B / 48,312 B | 620 / 537 / 70,692 / 48,324 | **620 / 537 / 70,692 / 48,324** | ✅ 4/4 |
| `default/Line` | 77 fns / 66 / 13,040 B / 8,360 B | 76 / 66 / 13,028 / 8,360 | **76 / 66 / 13,028 / 8,360** | ✅ 4/4 |

`rowset_snapshot.py diff` against the baseline copy:

```
CROSSED IN : 1 rows, 12 B
   +     12 B  default/BandCharacter::?ClassName@BandCharacter@@$4PPPPPPPM@A@BA?AVSymbol@@XZ
FELL OUT   : 0 rows, 0 B
NET bytes  : +12
```

The new row reads **`fuzzy 100.0 / mpn 100.0` with `masked_equal` unset** — it
pairs by NAME, exactly as predicted, so none of this rests on byte-signature
forgiveness. The one possible charge (the relocation *name* of the branch target,
the sole relocated word in an adjustor thunk) did not materialise because both
sides spell `?ClassName@BandCharacter@@UBA?AVSymbol@@XZ`.

⚠ **THE ONE PREDICTION MISS IS MINE AND IT IS ARITHMETIC.** I pre-registered the
code% delta as ≈+0.000012 pp; the true value is
`12 / 10,247,068 × 100 = +0.000117 pp`, so 39.42962 → **39.429737**. I slipped a
decimal converting a byte delta I had right into a percentage. The byte model was
exact on every key; the derived figure was 10× off. Recorded rather than
smoothed, because a pre-registration is worthless if its misses get quietly
rounded away.

## 7. Item 2 — the `Ham*` rows: 19 adjudicated, **0 renamed, 0 withdrawn**

### 7.1 The disputed count, settled

BH §6 says "**HamMove | 7** named map rows". Measured: the map has **19** rows
carrying a `Ham[A-Z]` token (the brief's figure, confirmed), of which the literal
`HamMove` appears in **4** and the strict token `HamMove` (excluding the distinct
class `HamMoveKey`) in **3**. **The brief is right and BH is wrong.** BH's table
is also internally inconsistent: its per-token counts (7+2+2+1+1) sum to **13**
while its prose claims "**9** named map rows".

Token census: `HamListRibbonDrawState` 4 · `HamMove` 3 · `HamIKEffector` 2 ·
`HamLabelCountDoneMsg` 2 · `HamListRibbon` 2 · `HamSupereasyMeasure` 2 ·
`HamCharacter` 1 · `HamMaster` 1 · `HamMasterLoader` 1 · `HamMoveKey` 1 ·
`HamNavProvider` 1.

### 7.2 Retail-string evidence, and what it does and does not license

No `Ham*` token appears anywhere in either retail image — no `.?AVHam…@@` RTTI
type-name and no bare substring, in `band.exe` or `default.xex`. The instrument
**discriminates**: the positive controls `.?AVBandCharacter@@`, `.?AVRndLine@@`,
`.?AVBandSong@@`, `.?AVCharacter@@` are all present.

But BH's own caveat is right and I am keeping it: **absence of an RTTI string is
not proof of a mislabel.** Retail carries no symbol names at all, and a
non-polymorphic `struct` never emits `??_R0` — so for the plain-struct template
rows the test is **vacuous in both directions**, which is the "a vacuity that
confirms your prior" hazard. What *is* decisive is the **positive** half:

| RB3 counterpart | in retail | DC3 twin | in retail |
|---|---|---|---|
| `.?AVBandLabelCountDoneMsg@@` | **PRESENT** | — | — |
| `.?AVBandIKEffector@@` | **PRESENT** | `.?AVHamIKEffector@@` | absent |
| `.?AVUILabelDir@@` / `.?AVUIListDir@@` | **PRESENT** | `.?AVHamListRibbon@@` | absent |
| `.?AVBandCharacter@@` / `.?AVBandLabel@@` | **PRESENT** | `.?AVHamCharacter@@` | absent |

For every *polymorphic* Ham name in the map, the RB3 counterpart's own RTTI
type-name string is in retail and the Ham one is not.

### 7.3 Four decisive identifications, obtained from retail bytes

- **`0x82340580`** builds `Symbol("count_done")` from `.rdata 0x82038648`, and
  **`0x82340940`** (the ctor) calls it — one class, two members 0x3C0 apart.
  The oracle `../rb3/src/system/bandobj/BandLabel.h:40` has
  `DECLARE_MESSAGE(BandLabelCountDoneMsg, "count_done")` with ctor
  `BandLabelCountDoneMsg(BandLabel *label)`; DC3's `hamobj/HamLabel.h:53` has the
  identical macro for `HamLabelCountDoneMsg`. **We carry both headers.** And
  `.?AVBandLabelCountDoneMsg@@` is PRESENT in retail — so this is direct
  retail-byte proof, not inference. Our `BandLabel.obj` defines **both** ctors and
  they are **raw-byte-identical** (164 B), their relocations differing *only* in
  the class spelling (`?Type@Band…`/`??_7Band…` vs `?Type@Ham…`/`??_7Ham…`);
  likewise the two `Type()` bodies (88 B) which share the *same* string COMDAT
  `??_C@_0L@LCDMNEMO@count_done?$AA@`.
- **`0x822c22c8`** — `__RTDynamicCast` destination TypeDescriptor `0x82c6c9d0`
  reads `.?AVBandIKEffector@@`. So `ObjPtr<T>::Replace` has **T =
  BandIKEffector**, not `HamCharacter`. (Its `bl` to
  `SetObjConcrete@?$ObjRefConcrete@VBandCharacter@@VObjectDir@@` is *not* a
  counter-argument — that body is byte-identical for every `T`, so ICF folded the
  family and the map holds the survivor's arbitrary name. The RTTI descriptor is
  the non-folded evidence.)
- **`0x82817a68`** — `__RTDynamicCast` from `.?AVObjectDir@@` to
  **`.?AVUILabelDir@@`**. So `ObjDirPtr<T>` has **T = UILabelDir**, not
  `HamListRibbon`; `0x82817ae8` (+0x80) is the adjacent `operator=` of the same
  `ObjDirPtr<T>`.
- **`0x823c35d8`** `PropSync` forms the property strings `"target"` and
  `"weight"` — an IK-effector constraint — and `.?AVBandIKEffector@@` is in
  retail while `.?AVHamIKEffector@@` is not.

### 7.4 ★ The binding constraint is PAIRING TOPOLOGY, not evidence

**Every one of those four identifications is un-actionable by a map edit alone,
and that is the finding worth carrying forward.** `objdiff` pairs target↔base by
name against the base obj of the unit the address is **pinned** to, and **16 of
the 19 rows are pinned to units whose base obj is `system/hamobj/*.obj` — DC3
source that defines only the DC3 spelling.** Renaming such a row converts a
matched row into a 0:

- `?Replace@?$ObjPtr@VBandIKEffector@@@@…` is defined by 4 objs — **not**
  `hamobj/HamIKSkeleton.obj`. Renaming `0x822c22c8` costs **−120 B**.
- `?PostLoad@?$ObjDirPtr@VUILabelDir@@@@…` is defined by 4 objs — **not**
  `hamobj/HamNavList.obj`. Renaming `0x82817a68` costs **−128 B**.
- `?Type@BandLabelCountDoneMsg@@…` is defined by `bandobj/BandLabel.obj` — **not**
  `hamobj/HamLabel.obj`. Renaming `0x82340580` costs **−88 B**.

`0x82340940` is the sole row whose **base obj does define** the RB3 name
(`bandobj/BandLabel.obj`), and it is still blocked — by **relocation coupling**.
Its instruction at +80 is `bl 0x82340580`; rename the ctor alone and our
Band-spelled relocation is compared against the map's still-Ham name for
`0x82340580`, producing a `diff_arg` charge and costing the whole **164 B**. This
is precisely the mechanism W16-BH measured in the opposite direction, where
naming `0x8227A910` charged the BandCharacter adjustor at instruction 2. Caller
and callee must be renamed **atomically**, and `0x82340580` cannot be renamed
without moving it.

⇒ **An identification lane and a pinning lane cannot be separated for a
cross-lineage mislabel: the map rename and the splits re-home are ONE atomic
transaction.** This is why the verdict on all 19 rows is KEEP-WITH-REASON rather
than RENAME, despite four of them being decisively identified. Nothing here is
evidence-limited; it is scope-limited, and my splits scope is `Line.cpp:` and
`BandCharacter.cpp:` only.

### 7.5 The two charged rows, priced

- **`0x82480018`** (340 B, `default/PartAnim`, **99.941**) — the single charge is
  one `diff_arg` at instruction 18: target `bl
  …_M_allocate_and_copy@…vector@URndPointTest@NgRnd@@…` vs our
  `…@PBUHamSupereasyMeasure@@…`. **The map contradicts itself** — a
  `vector<T>::operator=` must call `_M_allocate_and_copy` for the same `T`, and
  these two rows carry different `T`. The self-consistent rename is refused on
  measurement: **0 objs** in the tree define
  `??4?$vector@URndPointTest@NgRnd@@…QAAAAV01@ABV01@@Z`, while **3** define the
  Ham spelling (`TrackWatcherImpl.obj`, `PartAnim.obj`, `HamSupereasyData.obj`).
  The 340 B is **not collectable by any map edit**; it needs a source change
  (instantiate the RB3 `T`'s `operator=` in that TU), and the iterator
  const-ness differs too (`PAU` target vs `PBU` ours), so even the type fix may
  not close it. Filed, not attempted.
  ⚠ **Instrument note:** my first pass concluded "our tree has **no**
  `RndPointTest` symbols at all", which was a **false negative from the
  binary-blind `grep` shim** (`grep -rl … --include='*.obj'` routes through
  ugrep `-I`). The Python scan finds **99**. CLAUDE.md's most-cited hazard, and
  it produced exactly the shape it warns about — a decisive-looking negative that
  agreed with my prior. Caught only by re-running in Python.
- **`0x8279b2e0`** (8 B, `default/TrackWatcherImpl`, 97.000 fuzzy / 99.5 mpn) —
  `_STLP_alloc_proxy`, the 110-home arbitrary COMDAT lane CY-1 explicitly refused
  to move. It has **≥3 byte-equal twins in its own base obj**
  (`_STLP_alloc_proxy<GemInProgress>`, `<BeatMatchSink*>`, `<float>`), so **no
  unique identification exists even in principle**. Irreducible.

### 7.6 Per-row adjudication

| addr | map name (abbrev) | unit | base obj | sz | fuzzy | RB3 identification | our obj defines RB3 name? | verdict |
|---|---|---|---|---:|---:|---|---|---|
| `0x822c22c8` | `?Replace@?$ObjPtr@VHamCharacter@@@@UAAXPAVObjRef@@PA…` | `default/HamIKSkeleton` | `system/hamobj/HamIKSkeleton.obj` | 120 | 100.000 | ?Replace@?$ObjPtr@VBandIKEffector@@@@UAAXPAVObjRef@@PAVObject@Hmx@@@Z | no — defined by 4 other obj(s) | KEEP-WITH-REASON |
| `0x8230c200` | `??6@YAAAVBinStream@@AAV0@ABUNavItem@HamNavProvider@@…` | `default/HamNavProvider` | `system/hamobj/HamNavProvider.obj` | 96 | 100.000 | — not identifiable from bytes (plain-struct template: no RTTI on eit… | n/a | KEEP-WITH-REASON |
| `0x82340580` | `?Type@HamLabelCountDoneMsg@@SA?AVSymbol@@XZ` | `default/HamLabel` | `system/hamobj/HamLabel.obj` | 88 | 100.000 | ?Type@BandLabelCountDoneMsg@@SA?AVSymbol@@XZ | no — defined by 1 other obj(s) | KEEP-WITH-REASON |
| `0x82340940` | `??0HamLabelCountDoneMsg@@QAA@PAVUIComponent@@@Z` | `default/BandLabel` | `system/bandobj/BandLabel.obj` | 164 | 100.000 | ??0BandLabelCountDoneMsg@@QAA@PAVBandLabel@@@Z | **YES in base obj** | KEEP-WITH-REASON |
| `0x8239c508` | `??$__uninitialized_fill_n@PAUHamListRibbonDrawState@…` | `default/HamListRibbon` | `system/hamobj/HamListRibbon.obj` | 80 | 100.000 | — not identifiable from bytes (plain-struct template: no RTTI on eit… | n/a | KEEP-WITH-REASON |
| `0x823c3300` | `??6@YAAAVBinStream@@AAV0@ABVConstraint@HamIKEffector…` | `default/HamIKEffector` | `system/hamobj/HamIKEffector.obj` | 84 | 100.000 | (Band)IKEffector::Constraint `operator<<` | n/a | KEEP-WITH-REASON |
| `0x823c35d8` | `?PropSync@@YA_NAAVConstraint@HamIKEffector@@AAVDataN…` | `default/HamIKEffector` | `system/hamobj/HamIKEffector.obj` | 272 | 100.000 | (Band)IKEffector::Constraint `PropSync` | n/a | KEEP-WITH-REASON |
| `0x82480018` | `??4?$vector@UHamSupereasyMeasure@@V?$StlNodeAlloc@UH…` | `default/PartAnim` | `system/rndobj/PartAnim.obj` | 340 | 99.941 | — not identifiable from bytes (plain-struct template: no RTTI on eit… | n/a | KEEP-WITH-REASON |
| `0x82685e28` | `??$__uninitialized_copy@PAUHamMoveKey@@PAU1@@stlpmtx…` | `default/MoveDir` | `system/hamobj/MoveDir.obj` | 56 | 100.000 | — not identifiable from bytes (plain-struct template: no RTTI on eit… | n/a | KEEP-WITH-REASON |
| `0x826f7cc8` | `?_M_fill_insert_aux@?$vector@UHamListRibbonDrawState…` | `default/HamNavList` | `system/hamobj/HamNavList.obj` | 392 | 100.000 | — not identifiable from bytes (plain-struct template: no RTTI on eit… | n/a | KEEP-WITH-REASON |
| `0x82714858` | `??$_Destroy_Range@PAULocalizedName@HamMove@@@stlpmtx…` | `default/HamMove` | `system/hamobj/HamMove.obj` | 80 | 100.000 | — not identifiable from bytes (plain-struct template: no RTTI on eit… | n/a | KEEP-WITH-REASON |
| `0x827148b0` | `??$__uninitialized_copy@PAULocalizedName@HamMove@@PA…` | `default/HamMove` | `system/hamobj/HamMove.obj` | 96 | 99.792 | — not identifiable from bytes (plain-struct template: no RTTI on eit… | n/a | KEEP-WITH-REASON |
| `0x82715530` | `?_M_fill_insert@?$vector@ULocalizedName@HamMove@@V?$…` | `default/HamMove` | `system/hamobj/HamMove.obj` | 112 | 100.000 | — not identifiable from bytes (plain-struct template: no RTTI on eit… | n/a | KEEP-WITH-REASON |
| `0x8276e328` | `??0HamMasterLoader@@QAA@PAVHamMaster@@@Z` | `default/HamMaster` | `system/hamobj/HamMaster.obj` | 96 | 100.000 | — not identifiable from bytes (plain-struct template: no RTTI on eit… | n/a | KEEP-WITH-REASON |
| `0x82773920` | `?_M_insert_overflow_aux@?$vector@UHamListRibbonDrawS…` | `default/HamListRibbon` | `system/hamobj/HamListRibbon.obj` | 400 | 100.000 | — not identifiable from bytes (plain-struct template: no RTTI on eit… | n/a | KEEP-WITH-REASON |
| `0x827742b0` | `?push_back@?$vector@UHamListRibbonDrawState@@V?$StlN…` | `default/HamListRibbon` | `system/hamobj/HamListRibbon.obj` | 128 | 100.000 | — not identifiable from bytes (plain-struct template: no RTTI on eit… | n/a | KEEP-WITH-REASON |
| `0x8279b2e0` | `??0?$_STLP_alloc_proxy@PAUHamSupereasyMeasure@@U1@V?…` | `default/TrackWatcherImpl` | `system/beatmatch/TrackWatcherImpl.obj` | 8 | 97.000 | — not identifiable from bytes (plain-struct template: no RTTI on eit… | n/a | KEEP-WITH-REASON |
| `0x82817a68` | `?PostLoad@?$ObjDirPtr@VHamListRibbon@@@@QAAXPAVLoade…` | `default/HamNavList` | `system/hamobj/HamNavList.obj` | 128 | 100.000 | ?PostLoad@?$ObjDirPtr@VUILabelDir@@@@QAAXPAVLoader@@@Z | no — defined by 4 other obj(s) | KEEP-WITH-REASON |
| `0x82817ae8` | `??4?$ObjDirPtr@VHamListRibbon@@@@QAAAAV0@PAVHamListR…` | `default/HamNavList` | `system/hamobj/HamNavList.obj` | 300 | 100.000 | ??4?$ObjDirPtr@VUILabelDir@@@@QAAAAV0@PAVUILabelDir@@@Z | no — defined by 4 other obj(s) | KEEP-WITH-REASON |

"our obj defines RB3 name?" is read from the COFF symbol tables of all 1,204
compiled objects, after a build (reflinked objs are pre-renamer). `n/a` = no
RB3 name was established, so the question does not arise.

## 8. Closed: BH §8.6's `0x8227A528`

BH flagged, UNVERIFIED, that the 128-byte block at `0x8227A528` "may be
`BandFaceDeform`'s slot in the same `OBJ_CLASSNAME` run". **Already settled and
nothing to do:** the map has `0x8227a528` →
`?StaticClassName@BandFaceDeform@@SA?AVSymbol@@XZ`, 88 B, at 100 in
`default/BandCharacter`. It sits in a clean run of `?StaticClassName@Band*@@`
rows (`BandCamShot` `0x8227a1a8`, `BandConfiguration` `0x8227a228`,
`BandCrowdMeter` `0x8227a328`, `BandSongPref`, `BandIKEffector`,
`BandRetargetVignette`, **BandFaceDeform**, `BandLeadMeter`, `BandStarDisplay`,
`BandScoreboard`, `BandSong`) — BH's guess was right and the map already says so.

## 9. Item 3 — `_bijection_arbitrary` for `0x8227b050`: EDITED, Δ0 measured

**What the file said.** `_bijection_arbitrary` is a list of **1,013** VAs "whose
NAME was assigned by a bijection over a reloc-masked BYTE-IDENTICAL equivalence
class … the MATCH is true and byte-verified, but WHICH name belongs on WHICH VA
is NOT established", and it instructs any tool deriving identity from these
entries to treat them as **UNRESOLVED**. `0x8227b050` was a member.

**Why it needed the edit.** W16-BH *established* that identity on retail bytes,
and §3 above re-derives the warrant independently (504 thunks / 504 distinct
destinations ⇒ the destination is the identification). A stale "UNRESOLVED"
marker on a proven identity is an **active** hazard, not untidiness: the same
comment records that lane CF4-FIX had to delete an alias tier which cited this
very list as its warrant, and that "aliasing it destroys the evidence". Leaving
the row invites a future lane to re-adjudicate or alias an answer already known.

**Predicted Δ0 on all five headline keys**, because the list is advisory —
`obj_target_symbol_renamer.load_address_map` filters only JSON-null rows and
`_denylist`, never this list. **Measured** after `touch config/45410914/config.yml`
and two full builds to a `symbols.txt` fixed point: `matched_functions` 43,553 ·
`matched_code` 4,040,392 · `total_functions` 69,240 · `total_code` 10,247,068 ·
`masked_equal` 23,076 · code% 39.429737 — **bit-identical to the post-re-home
state**, and the set-diff is still exactly the one 12 B row in / 0 out. Δ0
confirmed by measurement, not asserted from the docstring (CLAUDE.md: "0 delta ⇒
inert is FALSE" cuts both ways — the point is to measure, and here the measured
value is the predicted one).

`_bijection_arbitrary`: 1,013 → 1,012 entries. One line, one list element.

## 10. ★ Sized follow-up the coordinator should own: the 18 `Ham*.cpp:` unit pins

This is the real vein behind Item 2, and it is **out of my scope** (splits
headings other than my two). `splits.txt` carries **18** `Ham*.cpp:` headings —
DC3 `src/system/hamobj/` source pinned onto RB3 `.text` — holding **33,708 B**,
of which only ~10,288 B is matched, and most rows are anonymous:

| unit | fns | matched | total_code | matched_code | code% | anon rows |
|---|---:|---:|---:|---:|---:|---:|
| `default/HamCamTransform` | 188 | 93 | 18,712 | 3,720 | 19.88 | 131 |
| `default/HamMove` | 41 | 25 | 5,552 | 3,200 | 57.64 | 20 |
| `default/HamNavProvider` | 22 | 6 | 2,332 | 140 | 6.00 | 21 |
| `default/HamListRibbon` | 9 | 4 | 1,068 | 652 | 61.05 | 6 |
| `default/HamIKSkeleton` | 9 | 3 | 760 | 120 | 15.79 | 7 |
| `default/HamCharacter` | 9 | 1 | 432 | 48 | 11.11 | 9 |
| `default/HamNavList` | 8 | 8 | 1,248 | 1,248 | 100.00 | 1 |
| `default/HamDirector` | 6 | 3 | 668 | 148 | 22.16 | 5 |
| `default/HamIKEffector` | 5 | 2 | 1,156 | 356 | 30.80 | 3 |
| `default/HamRibbon` | 4 | 2 | 456 | 216 | 47.37 | 0 |
| `default/HamSupereasyData` | 4 | 0 | 260 | 0 | 0.00 | 4 |
| `default/HamLabel` | 2 | 2 | 120 | 120 | 100.00 | 1 |
| `default/HamSongData` | 2 | 2 | 364 | 364 | 100.00 | 0 |
| `default/HamScrollSpeedIndicator` | 2 | 0 | 152 | 0 | 0.00 | 2 |
| `default/HamBattleData` · `HamCamShot` · `HamMaster` · `HamPhotoDisplay` | 1 each | 0/0/1/0 | 96/156/96/80 | 0/0/96/0 | — | 0/1/0/1 |

W16-BH proved the analogous `Ham.cpp:` pin was a DC3 scaffold and that its
`HamSong` row was really `BandSong`. The evidence in §7.2–7.3 says the same
disease runs through these 18: the RB3 counterpart's RTTI is in retail, the Ham
one never is, and the rows that *do* match match because an STL template on a
same-shaped struct compiles to identical code regardless of the struct's name.
⚠ I am **not** claiming all 33,708 B is mis-pinned — ~10,288 B genuinely matches
and a re-home is not neutral, so each unit needs its own adjudication.

### 10.1 The exact filed patch for the Label pair (Δ0 bytes, pure accuracy)

Ready to apply by a lane that owns `HamLabel.cpp:` / `BandLabel.cpp:`. It must be
**one atomic commit** — any subset loses bytes (§7.4):

1. `splits.txt`: **delete the entire `HamLabel.cpp:` entry.** Its only `.text`
   block is `start:0x82340580 end:0x823405F8` (120 B), so moving it drains the
   unit's last block and CLAUDE.md's rule applies — an emptied unit still emits a
   42-byte obj and `report.json` then hard-fails with `Failed to open
   HamLabel.obj: Invalid COFF/PE section headers`. Delete its `.pdata
   start:0x821FE790 end:0x821FE7A0` line with it.
2. `splits.txt`: add `.text start:0x82340580 end:0x823405F8` under
   `BandLabel.cpp:` (heading at line 12248).
3. map `0x82340580` → `?Type@BandLabelCountDoneMsg@@SA?AVSymbol@@XZ`
4. map `0x82340940` → `??0BandLabelCountDoneMsg@@QAA@PAVBandLabel@@@Z`
5. the block's second row, `fn_823405D8` (32 B, currently 100 via byte-signature
   pairing), moves with it — identify or leave anonymous; leaving it anonymous is
   free under `name_check`'s placeholder forgiveness.

Predicted: `default/HamLabel` disappears (−2 fns / −120 B from that unit),
`default/BandLabel` gains both rows, and whole-binary `matched_code` is **Δ0**
(both rows are at 100 now and should stay at 100), `total_code` Δ0. The value is
**accuracy plus draining a DC3 scaffold**, not bytes. ⚠ Unverified by me — no
build was run for it, because the edit is outside my scope.

## 11. Gates — all five, in the mandated order, in the worktree

1. **Full build** (`./tools/ninja-locked`) — **rc=0**. Five edges, all always-run
   phonies (`CHECK ICF-ALIAS MAP`, `CHECK SPLIT CURRENT`, `CHECK MAP
   NAME-INJECTIVITY`, `CHECK TARGET OBJS RENAMED`, `PROGRESS`): **zero compiles,
   zero SPLIT, zero REPORT** ⇒ a `symbols.txt` fixed point, and `symbols.txt` is
   clean in `git status`. No `ninja <one>.obj` and no `objdiff-cli --build` was
   ever run in this lane — both skip the six post-compile patchers and
   manufacture phantom regressions.
2. **`python3 scripts/verify_ruler_agreement.py --check`** — **rc=0**, grader
   config read from `report.json`'s own `provenance.diff_config`:
   `functionRelocDiffs=name_check`, `combineDataSections=true`,
   `combineTextSections=true`, `ppc.calculatePoolRelocations=false` all OK.
   "both objdiff-cli entry points resolve the same ruler."
3. **`python3 scripts/verify_objs_patched.py --verify-manifest`** — **rc=0**.
   `[denylist] OK: 6 denylisted address(es), 3 with a live map string, none named
   in 3113 target objects (495612 symbols scanned)`;
   `[patch-state] OK: 1215 decomp, 3113 target objects match
   2026-09-15T04:46:14Z (tree_sha256=67018c24fa136922)`.
4. **`python3 tools/icf_alias_finder.py --validate`** — **rc=0**,
   `VALIDATE: PASS -- 1404 map-consistent, 247 tolerated, 0 contradicted, 1652
   total`. **0 CONTRADICTED (FATAL)**. I touched no alias group, so this is a
   no-regression check rather than a result of mine.
5. **`tools/native_build_gate.sh`** — LAST action, verbatim:

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

`skipped=0` as required — full coverage, not an INCOMPLETE run relayed as PASS.
⚠ Run **twice**: once to capture the line above, then again *after* committing
this write-up, because CLAUDE.md records a **comment-only docs commit breaking
the native link** (`6c087cbd`, via `ScatterIncludes.cmake`'s unanchored `#if`
match). A `docs/decomp/*.md` file cannot enter any build, but the rule earned its
place, so the second run proves it rather than assuming it. Both runs:
`verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0`.

## 12. What I did NOT do, and why

1. **No rename or withdrawal of any `Ham*` row** — all 19 are KEEP-WITH-REASON.
   Four are decisively identified but blocked by pairing topology (§7.4); the
   rest are plain-struct template instantiations whose `T` the bytes cannot name.
   A withdrawal to placeholder was considered and rejected for every row: all 19
   pair at ≥97, so withdrawing costs the pairing and buys nothing — `name_check`
   already forgives placeholder *targets*, so the callers are not being charged.
2. **No splits edit outside `Line.cpp:` / `BandCharacter.cpp:`.** The correct fix
   for the Label pair, `ObjPtr<BandIKEffector>` and `ObjDirPtr<UILabelDir>` all
   require moving a `.text` block between headings I do not own. Filed in §10.1.
3. **No `src/` change.** `0x82480018`'s 340 B needs a source-side template
   instantiation (§7.5). Not attempted.
4. **No alias group touched**, per the bar. ⚠ Flagged for the alias owner:
   `symbol_aliases.json` mentions `ObjDirPtr@VUILabelDir` **97** times and
   `ObjDirPtr@VHamListRibbon` **17** times, so `0x82817a68`'s current 100 may
   rest on fold forgiveness across a family whose `T` I have now shown to be
   `UILabelDir` on retail bytes. That is an accuracy question for a lane that
   owns aliases, and it is exactly the class CLAUDE.md says to grep before
   believing a reloc-name find.
5. **Nothing owned by W16-BJ or W16-BK.** I touched no `UI.cpp:`/`UIColor.cpp:`
   heading and neither of the map rows `0x828023a0` / `0x82802418`; my map edits
   are `0x82289748` and one `_bijection_arbitrary` list element.
6. **`0x8276e328` (`HamMasterLoader` ctor) not identified.** Its `FilePath` string
   operand at `0x82000c55` is the empty string and I did not walk the `??_R4` of
   the vtable it stores at `0x8210a9c0`. Identifiable in principle by that route;
   I stopped because the row is at 100 in a `hamobj`-backed unit, so even a
   decisive answer is blocked by §7.4.
7. **`objects.json` untouched** — `system/hamobj/*.cpp` stay declared, as BH left
   them. Removing compile edges is a link-surface decision the native gate would
   have to price.

**What would change my verdicts.** For the four identified rows: nothing about
the *identification* — it rests on retail RTTI type-name strings and a
`__RTDynamicCast` destination, both read from `orig/45410914/band.exe`. What
would change the **action** is owning the splits heading, at which point §10.1 is
the patch. For the plain-struct rows, a decisive answer needs a channel the bytes
do not carry: a leaked RB3 map, or a caller whose own identified signature pins
the struct type. For `0x82480018`, a source instantiation of the RB3 `T`'s
`operator=` in that TU plus resolution of the `PAU`/`PBU` iterator const-ness.
