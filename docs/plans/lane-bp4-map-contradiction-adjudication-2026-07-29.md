# Lane BP-4 — ICF map-contradiction adjudication, and what the worklist was actually measuring

Date: 2026-07-29 · branch `laneBP4` · worktree `~/tmp/wt-bp4` (from `b78c702d`)
Owner of `scripts/target_symbol_map.json` for wave BP.

Baseline in this worktree, measured (fresh full build, `report.cache` removed):
`matched_functions 40830`, `masked_equal_functions 1514`,
**honest proxy `matched − masked_equal = 39316`** — identical to main's headline,
so the worktree is a clean A-leg.

---

## 0. Headline

The 200-row worklist `docs/plans/lane-bo8-icf-map-contradictions.json` yields
**zero repoints**, and that is the correct answer rather than a failure: 193 of
the 200 rows are **not map defects at all**. But the investigation built to
adjudicate them produced three *new* systematic map-defect channels that do
carry real defects, two of them large and mechanically enumerable.

| bucket | rows | meaning |
|---|---|---|
| fold refuted — retail kept the bodies distinct | **193** | not a map defect; leave |
| genuine unfolded-identical twins | **4** | undecidable by bytes; leave, marked |
| unreadable (no size in symbols.txt) | **3** | leave, marked |
| **repointed from this worklist** | **0** | — |

New channels found while adjudicating (details in §3–§6):

| channel | population | confidence |
|---|---|---|
| Save/Load stream-direction contradictions | 18 (8 at a false 100%) | proven per row |
| adjustor-thunk name permutation | 110 same-class (85 body-corroborated) | proven per row |
| phantom classes absent from the binary | 13 classes / 62+ entries | proven by string test |

---

## 1. The worklist asks one question; the defects live in a different one

A row of the worklist is an ICF equivalence class carrying ≥2 names that
`target_symbol_map.json` places at *different* VAs. Two readings compete:

1. the fold is **real** ⇒ at most one of those VAs holds the shared body, so
   ≥N−1 entries name a VA holding something else — a map defect;
2. the fold is **false for RB3** ⇒ retail kept the bodies apart and *our* build
   collapsed them (dc3 is a newer engine; `derived` folds are conditional on our
   codegen) — a source/codegen divergence, and repointing would make it worse.

`scripts/harvest/icf_contradiction_adjudicate.py` discriminates them by reading
the **retail bytes at each mapped VA out of band.exe** and comparing pairwise.

### The measurement trap that nearly inverted the answer

My first cut compared bodies with link-patched fields **masked**, and got 119
"twins" — i.e. mostly-real folds. That reading is wrong. Two copies of the *same*
function at two addresses have different `bl` displacement bytes purely because
the field is PC-relative; masking hides that, but it equally hides a genuinely
*different callee*. Masked-equality cannot separate "identical code the linker
failed to fold" from "same skeleton, different callee" — and those two lead to
opposite adjudications.

Fix: **resolve** branch fields to their absolute target instead of masking them,
leaving every other field byte-exact (`norm_body`). Normalized-equal then means
the bodies call the same things and hold the same immediates — true twins.

Re-run with the corrected test:

```
SKELETON_ONLY       118   same skeleton, DIFFERENT resolved callees -> retail did NOT fold
FOLD_REFUTED_SIZE    60   retail bodies differ in LENGTH
FOLD_REFUTED_BODY    15   same length, different masked bodies
TWIN_EXACT            3   byte-identical at two addresses
TWIN_TRUE             1   identical once branches resolved
UNREADABLE            3
```

So the `derived` source systematically **over-claims** folds: it requires
identical reloc target *names in our build*, but our build inlines/collapses
dtors and shares helper targets where retail emitted distinct ones. 43 of the
200 rows are pure ≤16-byte stub classes (`li r3,0; blr` shapes) unioned across
the whole binary — one row carries 58 mapped VAs and 3008 members with retail
sizes from 4 to 9276 bytes. Union-find over `derived` bodies over-merges badly.

### Why this lens cannot see the real defects

The known-bad anchor is **row 141**: `0x82690A10` → `?Save@SetUserTrackTypeMsg@@`
and `0x82690B28` → `?Save@SetUserDifficultyMsg@@`, both 76 bytes, both present in
our build, **both reading 100.0%**. My verdict for that row was SKELETON_ONLY —
the right answer to the folding question and a useless one, because the defect is
that `0x82690B28` is not a `Save` at all.

Two orthogonal axes, and the worklist only probes the first:

- **T1 fold-consistency** — what the worklist measures. 193/200 refuted.
- **T2 name-vs-body agreement** — where the defects are. Untested by the worklist.

Everything valuable in this lane came from building T2 probes.

---

## 2. The `??5`/`??6` proof (T2, and it is airtight)

Retail `0x82690B28` disassembles to two calls with `r3 = stream, r4 = &field` —
the shape *shared* by `operator<<` and `operator>>`. Resolving the two `bl`
targets:

```
0x827cb778 = ??5@YAAAVBinStream@@AAV0@AAVHxGuid@@@Z
0x827c52e8 = ??5BinStream@@QAAAAV0@AAVString@@@Z
```

`??5` is `operator>>`. The body reads an HxGuid at `this+4` and a String at
`this+0x14`: it is **unambiguously a Load**. It reads 100% only because objdiff
runs `functionRelocDiffs=None`, so a `bl` to the wrong stream operator is
invisible to the score. This is the at-100% defect class
(memory: `project_correctness_vs_metric`) with a mechanical detector.

Discriminator validated on both axes before use:
`?ReadEndian@BinStream@@` (0x827C5058) and `?WriteEndian@BinStream@@`
(0x827C5098) are **distinct VAs**, so neither is an ICF fold and the read-side
label is genuine; and **zero** VAs are claimed by both a `??5` and a `??6` name
(56 and 68 distinct VAs respectively). The 498-AGREE majority below is itself the
polarity proof — a swapped `??5`/`??6` convention would produce ~498
contradictions instead of 18.

---

## 3. Channel 1 — `scripts/harvest/saveload_direction_scan.py`

Scans all 761 `?Save@`/`?Load@` map entries, resolves every `bl` through the map,
and classifies the body's direction from the callee manglings.

```
AGREE         498
NO_EVIDENCE   171
MIXED          74
CONTRADICT     18      <- retail body is the OPPOSITE direction from its name
```

**8 of the 18 sit at a false 100.0%.** Representative rows:

| VA | pct | declared | observed | mapped name |
|---|---|---|---|---|
| 0x82690B28 | 100.0 | WRITE | READ ×2 | `?Save@SetUserDifficultyMsg@@` |
| 0x8242E3E8 | 100.0 | WRITE | READ | `?Save@ColorKeys@@` |
| 0x82344238 | 0.0 | WRITE | READ ×36 | `?Save@BandHighlight@@` |
| 0x828182A8 | 1.83 | READ | WRITE ×26 | `?Load@UIFontImporter@@` |

A CONTRADICT proves the **method** half of the name wrong; it does not by itself
settle the **class** half.

---

## 4. The PropAnim `Keys` family — a proven cluster shift, deliberately NOT applied

The Keys Loads form a contiguous run of five 80-byte functions at
`0x8242E3E8/438/488/4D8/528` (exact 0x50 spacing), all base-chaining to
`?Load@PropKeys@@`. Identifying each slot by the `Key<T>` element type of the
serialisation helper it calls:

| VA | mapped as | evidence says |
|---|---|---|
| 0x8242E3E8 | `?Save@ColorKeys@@` | `Load@FloatKeys` |
| 0x8242E438 | `?Load@FloatKeys@@` | `Load@BoolKeys` |
| 0x8242E488 | `?Load@BoolKeys@@` | `Load@Vector3Keys` |
| 0x8242E4D8 | `?Load@Vector3Keys@@` | `Load@ColorKeys` |
| 0x8242E528 | `?Load@SymbolKeys@@` | `Load@SymbolKeys` ✓ |

The Save band is shifted too (`0x8242AA00` mapped `?Load@ColorKeys@@` is a
`Save@FloatKeys`; `0x8242AEB8` mapped `?Save@FloatKeys@@` is a
`Save@ObjectKeys`; `0x8242B7A8` mapped `?Load@QuatKeys@@` is a
`Save@Vector3Keys`). All 14 names have real 80B COMDATs in `PropAnim.obj`, so the
COMDAT gate passes; `?Save@ObjectKeys@@` and `?Save@Vector3Keys@@` currently have
COMDATs but **no map entry**, so they are unpaired.

**Map-free corroboration.** Our own obj gives ground-truth helper sizes:
`Key<float>` reader 108B, `Key<bool>` 112B, `Key<Symbol>` 116B — all unique — and
retail's helpers at those slots are exactly 108/112/116B. That independently
proves slots 1 and 2, and independently proves `0x8242E3E8` is **not** ColorKeys
(its element is 8 bytes; a `Key<Color>` element is 20).

**Why it was not applied.** `Key<Vector3>`, `Key<Color>` and `Key<Quat>` are all
20-byte elements with 168-byte readers *in our build too* (Vector3 pads to 16+4),
so size and stride cannot separate them. Worse, when I chased the last two slots
map-free, retail `0x8242E210` — which the map labels the `Key<Vector3>` reader —
calls the **Color** per-element reader, so the map's own labels inside that family
are self-inconsistent. Because the run must move as a **block** (any single
repoint collides on a name), the choice was all-or-nothing, and the fine
assignment rested on labels I had just shown unreliable. Applying it would have
risked manufacturing exactly the mispairs this worklist exists to find.

**Left for a follow-up lane** with a stronger class oracle — specifically the
RTTI/vtable route that worked for §7 and §8: read the Complete Object Locator of
each Keys class's vtable and take the `Save`/`Load` slots directly.

---

## 5. Channel 2 — `scripts/harvest/thunk_name_consistency_scan.py`

An MSVC `this`-adjustor thunk is three instructions and no logic:

```
lwz  r11, -4(r3)      ; load vbase/vtordisp displacement
subf r3, r11, r3      ; adjust this
b    BODY             ; tail-jump to the real override
```

Its mangled name is therefore **fully determined** by its jump target's name.
That makes the check *self-consistent* rather than oracle-dependent: even if
BODY's own name is wrong, the thunk must still agree with it. A disagreement is
an unambiguous internal contradiction in the map.

```
scanned 1741 adjustor-thunk map entries
  OK                 1159
  MISMATCH            320      (306 at a false 100%)
  TARGET_UNNAMED      252
  NOT_SIMPLE_THUNK      8
  TARGET_UNKNOWN        2
```

**Do not read 320 as 320 defects.** Partitioning by class identity:

- **cross-class 210** — mostly **ICF at the target**: the real body was folded
  with an identical body elsewhere and the map labelled the folded address with
  the other alias. Three target VAs receive 21, 19 and 17 thunks respectively —
  an unmistakable ICF-hub signature. The thunk name here is arguably *correct*;
  this is the `merged_call` artifact class, not a defect.
- **same-class 110** (107 at a false 100%) — genuine intra-class permutation,
  with explicit reciprocal pairs: `?Handle@TexMovie@@$4`→ClassName body *and*
  `?ClassName@TexMovie@@$4`→Handle body; likewise FreestylePanel, RndCamAnim,
  CharBonesBlender, CharBoneOffset, CamShot, Spotlight, WorldInstance,
  CharPosConstraint, ScoreDisplay.

**Which side is wrong — thunks or bodies?** Settled map-free: for **85 of the
110**, the retail body size at the target VA equals our COMDAT size for the name
the map gives that VA, so the *body* names are corroborated and the *thunk* names
are the wrong ones. (The 25 disagreements are genuine source divergences — e.g.
`?Copy@UIComponent@@` is 148B ours vs 500B retail — not naming errors.)

Applied this lane: only the **5 clean reciprocal pairs** (10 entries) where the
swap is reciprocal, the displacement tag is identical (so the swap stays inside
one sub-object), and both bodies are size-corroborated. Those need no name
synthesis — the correct names already exist at each other's VAs. The remaining
~100 need a name-synthesis step (rebuild the thunk mangling from the body name +
the VA's displacement tag) and are left for a follow-up.

---

## 6. Channel 3 — phantom classes (highest-yield, cheapest test)

`OBJ_CLASSNAME` embeds `#classname` as a string literal, so any class with a
`StaticClassName`/`ClassName` in retail **must** have its name string in the
binary. Of the 336 classes in the map exposing those methods, **13 have zero byte
occurrences anywhere in band.exe** — verified directly, against a 10-class
control group that all hit 2–5 times:

```
BandStoreUIPanel  BaseMaterial  ClipCollide  DancerSequence  FitnessFilterObj
FlowCommand  FlowIf  FlowNode  FlowOnStop  MetaMaterial  RndSpline
SkeletonClip  WiiFriendsScreen
```

They poison **62+ map entries** (RndSpline 13, FlowIf 8, FlowNode 8,
SkeletonClip 8, BandStoreUIPanel 8, BaseMaterial 6, DancerSequence 5, rest ≤2).
Root cause: the map was generated from the rb3-Wii **DEV** oracle, so it carries
Wii-only classes that the 360 retail build never had. Every such entry is a
certain mispair, detectable with a pure string test and no build.

Caveat for whoever drains this: these entries are currently *earning false
credit* (see §8), so removing them will **reduce** `matched_functions` while
improving correctness. Price it with Δ(matched − masked_equal), not the headline.

---

## 7. Part B — applied: `0x82630340` → `??1RetryAudioPanel@@UAA@XZ`

Held by lane BO-4 because `??1RetryAudioPanel` had no COMDAT; unblocked by
`RetryAudioPanel.cpp` landing in `4b424140`. Four independent channels agree:

1. **RTTI (decisive)** — all three vtables the retail body installs
   (`0x820CA194/0x820CA154/0x820CA0FC`) have Complete Object Locators whose type
   descriptor reads `.?AVRetryAudioPanel@@`, and the COL offsets `0x0`/`0x3c`
   match the body's literal `stw r11,0(r3)` / `stw r9,0x3c(r3)`.
2. **Hierarchy** — `RetryAudioPanel : VoiceoverPanel` and retail tail-branches to
   `0x8262F390 = ??1VoiceoverPanel@@UAA@XZ`; `NewAwardPanel : TexLoadPanel` and
   *our* `??1NewAwardPanel` correctly tail-calls `??1TexLoadPanel`. **Neither
   side's code is wrong — only the map's name→address binding.**
3. **Spatial** — the VA lies in RetryAudioPanel.cpp's span
   `0x82630228..0x82630690`, where every other named symbol is RetryAudioPanel;
   our COMDAT is `sel=1` (NO_DUPLICATES) so its retail copy must lie in its own
   TU's span. NewAwardPanel.cpp's span is ~36 KB away.
4. **Uniqueness** — a masked-byte search over all of retail `.text` returns
   exactly one hit for this body shape.

COMDAT gate **PASSES**: a real `0x44` COMDAT in
`build/45410914/src/band3/meta_band/RetryAudioPanel.obj`, matching
`symbols.txt size:0x44`.

Two provenance corrections: the brief's `op:"proposed-hold"` row does **not**
exist in `scripts/harvest/bo6_settype_fragments.json` (that file has no such op
and no such VA); the real provenance is
`docs/plans/lane-bo4-byte-search-locator-2026-07-29.md` §9.3. And the claimed
micropins around `[0x82630340,0x82630384)` are not there — the VA has a single
owner, RetryAudioPanel.cpp. Also, the premise "the current 100.0% is a
reloc-masked false match" is **stale**: since RetryAudioPanel.cpp landed, the
target-side name is *unpaired* (`fuzzy None`), so this repoint should be **+1**.

---

## 8. Part C — all 13 entries CONFIRMED, all 13 INERT, do not apply

Lane BO-1's `~/tmp/laneBO1/e_frag_full.json` (13 UISyncNetMsgs entries,
`0x8269F3F8`–`0x826A00D0`). Re-verified independently, per row: **13 CONFIRM, 0
REFUTE, 0 UNDECIDABLE.** Class identity is RTTI ground truth (COLs read
`.?AVComponentFocusNetMsg@@` etc.); Save/Load rows are pinned by vtable slot
(slot[1]=Save, slot[2]=Load) *and* by the `??5`/`??6` discriminator agreeing;
`Dispatch` bodies call exactly the static-local ctors the oracle says they
construct, with correct multiplicity. Oracle signatures in
`../rb3/src/band3/game/UISyncNetMsgs.{h,cpp}` match all six ctors exactly.

**But none of the 13 names has a COMDAT definition in our build.** Ten appear in
`NetSync.obj` only as `sec=0` UNDEFINED EXTERNAL references; the three
`??0NetComponent*Msg` inline ctors are absent entirely. Root cause:
`src/band3/game/UISyncNetMsgs.cpp` **does not exist in our tree** (only the `.h`)
and is not in `objects.json`. Applying the fragment would rename anonymous `fn_`
symbols to names our base obj cannot supply → unpaired before, unpaired after,
Δ = 0 by construction. Not harmful, just inert — and very likely why BO-1's
branch correctly priced out. The brief's claim that this fragment carries aliases
for `0x82690A10`/`0x82690B28` is inaccurate; it contains neither VA.

Two spin-off findings worth more than the fragment:

- **Splits mis-attribution.** `Performer.cpp`'s pinned span
  `0x8269F2C8–0x826A06A0` contains a whole foreign TU. Proposed: trim Performer
  to `0x8269F2C8–0x8269F378` and pin a new unit
  `band3/game/UISyncNetMsgs.cpp` at `0x8269F378–0x826A06A0`. The only
  already-mapped symbol inside the proposed span is
  `?Type@NetComponentPostScrollMsg@@`; no Performer symbol falls inside it.
  Porting that TU from the rb3-Wii oracle then makes all 13 live — 13 named
  functions plus ~25 EH funclets in the span.
- **Independent corroboration of the §2 anchor.** `ComponentFocusNetMsg`'s vtable
  slot[1] → `0x82690A10` and slot[2] → `0x82690B28`. Slot[2] is the *Load* slot,
  so `0x82690B28` is a Load body — agreeing with the `??5` proof on the method
  and additionally pinning the class. **Not repointed**, because
  `?Load@ComponentFocusNetMsg@@` has no COMDAT in our build, so the RTTI-correct
  name would be inert *and* would surrender the two current false 100s.

---

## 9. BP-3's `0x82319C48` — MAP MISPAIR, repoint name identified, not applied

Verdict **(b) map mispair**: the VA is `?SetType@MiniLeaderboardDisplay@@`, not
`?SetType@BandStoreUIPanel@@`. Decisive evidence: its `StaticClassName` callee
(`0x82319950`) builds its Symbol from the literal at `0x82030C10`, which reads
`"MiniLeaderboardDisplay"` — and `BandStoreUIPanel` has **zero** byte occurrences
in band.exe (§6).

BP-3's "this-adjustor of 328" is **not** an anomaly: the body uses
`lwz r11,-0x144(r30)` / `addic. r11,r30,-0x148` plus a vbtable indirection, which
is MSVC's ordinary virtual-base displacement. Control: the correctly-mapped,
100%-matching `?SetType@AppMiniLeaderboardDisplay@@` at `0x8264C178` is *also*
exactly 0x13C and instruction-identical using `-0x178`/`-0x17C`. Compiler layout
(`/d1reportSingleClassLayout`): `UIPanel` and `BandStoreUIPanel` are both
sizeof 104 with adjustors −64 (reproducing BP-3's 64), while
`MiniLeaderboardDisplay`'s adjustors are −340 vs retail's −328 — a 12-byte
DC3-newer-vs-RB3 member delta, not a 264-byte chasm. So **not** (a).

BP-3's "100% nearby" is **refuted as evidence**: `StaticClassName`(88B) and
`ClassName`(48B) are part of the *same mispaired triplet*, and their 100% is
reloc-masked false credit — `OBJ_CLASSNAME` bodies are class-agnostic apart from
the string relocation, and those sizes fit any class. `NewObject` is 2.3 MB away,
not nearby. So the mispair is a contiguous triplet
`0x82319950 / 0x82319A50 / 0x82319C48`, not one narrow VA.

**Not applied**, deliberately: `MetaPanel.obj` contains
`?SetType@BandStoreUIPanel@@` (MetaPanel.cpp includes the header) and *zero*
MiniLeaderboardDisplay symbols, so a bare repoint would unpair the VA and trade
three false credits for an honest 0%. Epistemically right, but it needs the
`system/hamobj/MiniLeaderboardDisplay.cpp` cluster carved out of MetaPanel.cpp's
44-span mega-unit and pinned first. Filed as a paired splits+map change.

---

## 10. Map hygiene note

`scripts/target_symbol_map.json` already contains **3 duplicate names**
(`?StaticClassName@Object@Hmx@@`, `?StaticClassName@RndCam@@`, `?NodeCmp@@`, each
at two addresses). `tu5_map_apply_fragment.py`'s insert-time check would have
blocked these, so they entered by another path. `map_repoint_apply.py` therefore
asserts on the *delta* (introduce no NEW duplicate) rather than absolutely, and
prints a note — asserting absolutely would block every repoint forever and tempt
the next author to delete the check.

---

## 11. Tools added (all committed, all read-only analysis)

| tool | purpose |
|---|---|
| `scripts/harvest/icf_contradiction_adjudicate.py` | adjudicate the 200-row worklist; PE section-table reader with the `off(0x824DAAD0)==0x004CF8D0` anchor asserted before any read; `mask_body` vs `norm_body` tiers |
| `scripts/harvest/saveload_direction_scan.py` | `??5`/`??6` stream-direction audit of all 761 Save/Load entries |
| `scripts/harvest/thunk_name_consistency_scan.py` | adjustor-thunk name vs jump-target audit (1741 entries) |
| `scripts/harvest/map_repoint_apply.py` | in-place repoint/swap applier; never json.dumps the map; asserts `old` to fail loudly on a stale fragment; requires a per-row `why` |

Fragment with per-row justification:
`docs/plans/lane-bp4-map-repoints-2026-07-29.json` (11 ops).

band.exe addressing, re-verified rather than assumed: `.text` is RVA `0x270000` /
raw `0x264E00`, delta `0xB200`; `va − 0x82000000` is not a file offset for
`.text`. All tools parse the real section table and assert the anchor.
