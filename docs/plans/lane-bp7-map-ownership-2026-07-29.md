# Lane BP-7 — map ownership: stream-direction, phantom drain, and a new decisive channel

Date: 2026-07-29 · branch `laneBP7` · worktree `~/tmp/wt-bp7` (from `391e8a39`)
Sole owner of `scripts/target_symbol_map.json` for this lane.

Baseline in this worktree, measured (full build, `report.cache` removed,
`symbols.txt` restored): `matched 40870`, `masked_equal 1509`,
**honest proxy 39361** — identical to main's headline, so a clean A-leg.

---

## 0. Headline

| part | applied | matched Δ | masked_equal Δ | honest Δ | predicted? |
|---|---|---|---|---|---|
| A+B stream-direction repoints | 5 | 0 | 0 | 0 | yes, exactly |
| C phantom-class drain | 34 deletes | **−26** | 0 | −26 | yes, exactly |
| D duplicate names | 2 deletes | −2 | 0 | −2 | yes, exactly |
| E StaticClassName cycles (**new channel**) | 5 repoints | 0 | 0 | 0 | yes, exactly |
| F BP-2b RndText fragment | 3 deletes + 2 repoints | 0 | 0 | 0 | yes, exactly |
| **lane total** | **51 rows** | **−28** | **0** | **−28** | |

Every per-part prediction was stated before its build and held exactly. The −28
is entirely deliberate false-credit drain (C −26, D −2); the other 46 row changes
are metric-neutral attribution fixes.

**Pricing note that contradicts the brief, stated plainly.** The brief expected
"honest proxy stable or rising while false credit drains". That is
arithmetically impossible for a deletion wave. Verified directly: the report
carries a per-function `masked_equal` flag (exactly 1509 functions have it), and
**zero** of the deleted false-100s carried it — they were *name*-paired, not
byte-fallback. So `matched` and `matched − masked_equal` fall by the same
amount. The honest proxy is an **upper bound on matched**, not a correctness
measure; draining false credit necessarily lowers both. The right reading is
that the over-count is now 28 smaller.

---

## 1. Part A — the brief's row 1 is refuted; only row 2 was a defect

The brief asked to repoint **both** `0x82690A10` and `0x82690B28` to
`Save`/`Load@ComponentFocusNetMsg`, on the grounds that BP-5's port created those
COMDATs. Both halves of that premise fail:

1. **Wrong obj.** Both VAs are owned by `NetGameMsgs.cpp`'s span
   `[0x82690868,0x82692818)`. BP-5's carve starts at `0x8269F378`. objdiff pairs
   **per unit**, and `NetGameMsgs.obj` contains *zero* ComponentFocusNetMsg
   symbols — not even an undefined external. The repoint would have unpaired
   both VAs and read 0%.
2. **`0x82690A10` was already right.** Its two `bl` targets are `??6`
   (`operator<<`); it is a genuine Save.

### What the two VAs actually are: a 6-way ICF hub

RTTI is decisive. Each ctor installs a vtable; each vtable's `??_R4` Complete
Object Locator names its class:

| ctor | vtable | `??_R4` type descriptor |
|---|---|---|
| 0x82690950 | 0x820DC034 | `.?AVSetUserTrackTypeMsg@@` |
| 0x82690A68 | 0x820DC0E4 | `.?AVSetUserDifficultyMsg@@` |
| 0x8269F3F8 | 0x820A6174 | `.?AVComponentFocusNetMsg@@` |

**All three** carry `slot[1] = 0x82690A10` and `slot[2] = 0x82690B28`. And all
six candidate COMDATs (3 classes × Save/Load) are **masked-identical** — the only
differing field is the relocation (`??5` vs `??6`). So the *method* half is
provable and the *class* half is not.

Applied: `0x82690B28  ?Save@SetUserDifficultyMsg@@ → ?Load@SetUserDifficultyMsg@@`.
Class half preserved per BP-4's own scoping. Spatial corroboration: the two
classes lay out identically, ctor + 0xC0 → serializer
(0x82690950→0x82690A10, 0x82690A68→0x82690B28), so each VA sits inside its own
class's COMDAT group.

Both VAs are now registered in **`_bijection_arbitrary`**, which they were
missing despite being a textbook instance of that doctrine.

### Refuted lever: ICF aliases cannot be expressed

The map has four list-valued entries, so a per-VA *list of names* looks possible.
It is not: all four are metadata keys (`_bijection_arbitrary`, `_icf_arbitrary`,
`_denylist`, `_internal_linkage_allow`). `obj_target_symbol_renamer.py` does
`renames[sym_name].encode("ascii")`, so a list would crash. **One name per VA is
a hard constraint**; ICF aliasing is representable only in the metadata lists.

### Filed, not applied: the SetUser*Msg Dispatch question

`SetUserTrackTypeMsg`'s vtable `slot[3]` → `0x82691068`, whose body calls the VA
mapped `?SetDifficulty@BandUser@@`; `SetUserDifficultyMsg`'s `slot[3]` →
`0x826910F0`, calling the VA mapped `?SetControllerType@BandUser@@`. That looks
like a reciprocal swap, but the evidence chain runs through a **second** ICF hub
(`BandUser::Set*(Symbol)` is a 0x3c triplet in our build) and both Dispatch VAs
*and* both setter VAs are **already listed in `_bijection_arbitrary`**. So the
apparent swap is a known-arbitrary labelling, not a demonstrable defect.
**Undecidable without a non-ICF anchor — do not re-hunt.**

---

## 2. Part B — 18 CONTRADICT rows adjudicated, 4 applied

`saveload_direction_scan.py`: 765 rows, 18 CONTRADICT, 8 at a false 100%.

Applied 4 (plus Part A's row):

| VA | change | evidence |
|---|---|---|
| 0x82721370 | `Save@FxSendBitCrush` → `Load@` | body calls `ReadEndian`, then **`?Load@FxSend@@`** — a base-class Load chain, decisive on both halves |
| 0x8233B3E0 | `Save@BandList` → `Load@` | calls `ReadEndian`; the incumbent COMDAT is a **4-byte `blr` stub**, so its 2.94% was meaningless |
| 0x823C7E28 ↔ 0x823C7ED0 | PracticeSection Save/Load swap | 0x823C7E28 calls `ReadEndian` + `?Load@Object@Hmx@@` (base chain) + `ObjRefConcrete::Load`×2; its partner has no read-side callee at all |

**Held 13**, each with a reason. The size gate (retail size == our COMDAT size)
*failed* on all four applied rows — recorded honestly: our bodies differ in
length from retail, so these are source divergences and the repoints rest on
direction + base-chaining, not size. Notable holds:

- **3 PropAnim `Keys` rows** — a method flip is structurally wrong here (`Load`
  takes `BinStreamRev`, `Save` takes `BinStream`, so the flipped signature has no
  COMDAT). BP-4 §4 already showed this family needs a *cluster shift* with a
  class oracle, not a flip. Unchanged.
- **4 adjustor-thunk rows** at false 100% — these belong to the thunk channel,
  where the name is determined by the jump target; the saveload lens is the
  wrong tool.
- **0x828182A8 UIFontImporter** — resolved later by Part F (see §6).

---

## 3. Part C — phantom drain, with two corrections to the inherited evidence

### Correction 1: RndSpline is NOT a phantom, and it was the largest contributor

BP-4's test searched band.exe for the literal `"RndSpline"`. But
`src/system/rndobj/Spline.h` declares `class RndSpline` with
**`OBJ_CLASSNAME(Spline)`** — the short DTA name. The literal actually emitted is
`"Spline"`, which occurs **3 times**, the same density as controls (PropAnim 3,
BandList 3). RndSpline exists in RB3 retail; its 10 entries (7 at 100%) are
excluded from the drain.

Audited all 13: RndSpline is the **only** class whose `OBJ_CLASSNAME` literal
differs from its C++ name, so the other 12 stand. This is the trap that this
whole lane's best channel (§4) is built to avoid.

### Correction 2: "62+ poisoned entries" is really 41

11 of BP-4's rows were substring false positives — the phantom class appears as a
**parameter or template argument**, not as the owning class
(`?SetMatColorFlags@@` takes a `W4ColorModFlags@BaseMaterial@@`; several
`stlpmtx_std` templates are instantiated over `MoveRating@SkeletonClip` /
`CtrlPoint@RndSpline`). Replaced the substring test with a real parse of the
mangled qualification. 51 precise, minus RndSpline's 10 → **41**.

### Better root cause: DC3 leakage, not Wii-only classes

BP-4 attributed the phantoms to the rb3-Wii DEV oracle. Only 2 of 12 come from
there (`BandStoreUIPanel`, `WiiFriendsScreen`). The other **ten are DC3-exclusive
leakage from the dc3-decomp source tree**:

- `system/flow/*` (FlowNode, FlowIf, FlowCommand, FlowOnStop) — a DC3-newer subsystem
- `system/rndobj/{BaseMaterial, MetaMaterial}` — DC3-newer
- `system/char/ClipCollide` — DC3-newer
- `system/gesture/{SkeletonClip, FitnessFilterObj}` — **Kinect gesture**, never in Rock Band
- `system/hamobj/DancerSequence` — **Dance Central dancer object**, never in Rock Band

### Adjudication: 34 deleted, 7 held, 0 clean repoints

Before deleting, tried to *name the real owner* two ways:

- **`StaticClassName` bodies name their owner in a literal** — read directly:
  0x82319950 → `"MiniLeaderboardDisplay"` (agrees with BP-4 §9), 0x8240E0C0 →
  `"Movie"`, 0x822DC7B0 → `"Mesh"`, 0x8240E240 → `"MultiMesh"`, 0x82369BA8 →
  `"CharBone"`.
- **Adjustor thunks inherit their class from the jump target** when the *method*
  half agrees: 0x825734D0 → `ClassName@MetaPanel`, 0x823C7318 →
  `Copy@CharUpperTwist`. Where the method half *disagrees* the target name is
  itself unreliable (ICF) and was not used.

All 7 identified repoints were **held**: each either has no COMDAT in the obj of
the unit owning the VA (MiniLeaderboardDisplay needs its cluster carved out of
MetaPanel.cpp's mega-unit first — a bare repoint reads 0%) or its proposed name
is already mapped elsewhere.

---

## 4. Part E — NEW CHANNEL: `staticclassname_literal_scan.py`

**The strongest thing this lane produced.**

`OBJ_CLASSNAME(literal)` expands to a `StaticClassName()` that builds a `Symbol`
from `"literal"`. Every such body is **identical machine code except the one
relocation supplying the string pointer**. objdiff runs
`functionRelocDiffs=None`, so that field is invisible and *every* such body
matches *every* other at 100.0%. The whole family can be arbitrarily scrambled
across the map while reading perfectly clean — the at-100% defect class in its
purest form.

But the string is *in the body*. Disassemble the mapped VA, walk the `lis`/`addi`
pair, read the `.rdata` C string, compare to the `OBJ_CLASSNAME` argument the
mapped class declares in our own source. **No oracle, no build, no Ghidra.**

```
scanned 317 ?StaticClassName@ entries
  AGREE             290
  CONTRADICT         24     <- ALL 24 at a false 100.0%
  NO_SOURCE_LITERAL   3
```

The scan parses the real `OBJ_CLASSNAME` argument rather than assuming it equals
the class name — precisely the RndSpline trap from §3.

### Applied only the closed cycles (5 of 24)

```
3-cycle  0x82739158 -> 0x82738F08 -> 0x827382F8   (DxMesh / DxEnviron / DxCam)
2-cycle  0x82739208 <-> 0x82739288                (DxMovie / DxCubeTex)
```

**Closure is load-bearing, not decoration.** The Rnd/Dx/Ng renderer-layer split
means several classes share one DTA literal (`RndCubeTex`, `DxCubeTex`, … all
declare `OBJ_CLASSNAME(CubeTex)`), so literal→class is *one-to-many*. Only the
cycle constraint forces a unique assignment. The other 19 are open chains: their
proposed name currently sits on a VA outside the contradict set, so moving it
could break a correct pairing.

Metric-neutral by construction — each VA keeps its 100%, under the right name.

**Worklist left for the next owner:** the 19 open chains in
`~/tmp/bp7/scn.json`. They need the AGREE rows re-examined too, because an open
chain terminates on a VA the scan currently calls AGREE.

---

## 5. Part D — all three duplicate names resolved (3 → 1)

- **`?NodeCmp@@YAHPBX0@Z` — NOT a defect.** Already listed in the map's own
  `_internal_linkage_allow`. Verified independently: it is a static
  (internal-linkage) function, so each TU legitimately gets its own copy, and the
  two VAs have **different** retail sizes (0x14C at DataArray.cpp, 0x94 at
  BandWardrobe.cpp), each exactly matching a same-named COMDAT in that unit's own
  obj, both masked-equal to retail. Two genuine distinct functions. Left alone.
- **`?StaticClassName@Object@Hmx@@`** — 0x82271A90 builds `"Object"`, matching
  `Object.h:1614 OBJ_CLASSNAME(Object)`: **kept**. 0x8240DC38 builds `"CubeTex"`:
  **deleted** (owner MeshDeform.cpp supplies no `*CubeTex` COMDAT).
- **`?StaticClassName@RndCam@@`** — 0x8229CEB8 builds `"Cam"`, matching
  `Cam.h:24 OBJ_CLASSNAME(Cam)`: **kept**. 0x8240E940 builds `"DOFProc"`:
  **deleted**.

---

## 6. Part F — BP-2b's RndText fragment, and a cross-lane resolution

Adjudicated all 12 rows against the no-COMDAT-no-repoint gate, judging on the
fragment's justifications rather than the incumbent names (which BP-2b showed are
DC3-map hypotheses: retail has zero string hits for `blacklight`, `FontMap3d`,
`scroll_delay`, …).

Applied 5, held 7. The applied deletes must precede the applied repoints — a
repoint-first order trips the collision assert.

### The cross-lane resolution — neither lane could solve it alone

- This lane's Part B proved `0x828182A8` contradicts its name (`?Load@UIFontImporter@@`,
  but observes **WRITE ×26**) — yet had to **hold** it, because
  `?Save@UIFontImporter@@` was occupied by `0x82455928`.
- BP-2b independently proved `0x82455928` is **RndText::Save**, mis-pinned into
  UIFontImporter.cpp's span — i.e. that occupancy *was itself the defect*.

Combined, both VAs are forced: delete 0x82455928's claim, repoint 0x828182A8 to
`?Save@UIFontImporter@@`. This is the "yield lives in the SEAM" lesson again — a
hold in one lane was the missing key in another.

Held rows all carry their blocker: 0x8245ADC8 / 0x8245C2D8 are at a false 100%
but BP-2b rates them medium-high/medium *and* warns against assigning
`?SyncProperty@RndText@@` blindly (the retail body takes a vbase-normalised
`this` with a constant −0x194 adjustment, not a real vtordisp thunk). All 7
`candidate_additions` need either a splits pin this lane is forbidden to touch or
an unresolved mangled form.

---

## 7. Tools added

| tool | purpose |
|---|---|
| `scripts/harvest/staticclassname_literal_scan.py` | **new channel** — audit every `?StaticClassName@` entry against the class-name string its retail body builds; parses the real `OBJ_CLASSNAME` argument |
| `scripts/harvest/map_row_delete.py` | the third applier (repoint / insert / **delete**); same never-json.dump line-surgical discipline, asserts `old`, requires per-row `why`, asserts post-condition key count |
| `scripts/harvest/map_flag_arbitrary.py` | line-surgical appender for the list-valued metadata keys, which `map_repoint_apply.py` cannot touch; metric-inert by construction |

Fragments (each separately landable):
`docs/plans/lane-bp7-part{AB,C,D,E,F-*}-*.json`.

---

## 8. Deferred

- **The 110-row adjustor-thunk permutation channel** (mission part E, marked
  optional) was **not** worked. Budget went to the StaticClassName channel
  instead, which is strictly stronger: its discriminator is decisive and
  oracle-free, whereas the thunk channel's 85 "body-corroborated" rows rest on
  size agreement. The 4 thunk rows that surfaced in Part B are logged in §2.
- **No splits pins were touched** (lane constraint), which is what blocks most
  Part C and Part F held rows.
