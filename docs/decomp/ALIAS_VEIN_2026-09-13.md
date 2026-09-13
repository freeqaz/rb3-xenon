# The alias-blocked vein, adjudicated on retail bytes — lane W7-D, 2026-09-13

Branch `w7-alias-vein`, worktree `~/tmp/wt-w7-d`, based on main `3a6bfe40`
(asserted ancestor before editing), rebased onto `89a3ad8b` mid-lane to pick up
lane W7-C's map fix (§5). Ruler: **`name_check` (graded)**, resolved at runtime
from `report.json` `provenance.diff_config` — not assumed.

Baseline, taken after this lane's **first full build** (mandatory: a reflinked
worktree's target objs are pre-renamer, so every mangled-name lookup reads
"absent" until the renamer's pre-compile step has actually run, and any negative
taken before that is vacuous):

```
matched_functions   42,676
matched_code        3,859,912 B
matched_code_percent  37.672540
fuzzy_match_percent   49.090370
total_code          10,245,956
total_functions      69,219
masked_equal        22,937
```

`verify_objs_patched.py --verify-manifest` → `OK: 1205 decomp, 3084 target
objects match` (rc=0) before any edit.

## 0. Headline

**12 folds installed, +3,756 B measured across two legs, both predicted
exactly. One fabricated alias withdrawn.** The larger part of this lane is
refusals, and the refusals are the deliverable: three separately briefed targets
— the `_S_sort` group, `AddRef`/`Release`, and the largest byte-weighted pair in
the whole stratum — are **refuted on retail bytes**, and one of them
(`AddRef`/`Release`) would have cost a 15-file retype for provably zero bytes.

The generalisable finding:

> ★★★ **A family folds iff its relocation targets are TYPE-INDEPENDENT.** Every
> pair this lane installed relocates only to symbols that do not vary with the
> template parameter (`__savegprlr_29`, `Object::AddRef`, `MemAlloc`/`MemFree`,
> `MemOrPoolAllocSTL`, the `_Slist_node_base*` machinery a hashtable shares
> across every value type). Every family it refused carries a **per-`T` callee**
> — `list<T*>::insert → _M_create_node<T>`, `vector<T>::resize →
> _M_fill_insert<T>`, `~list<T> → _List_base<T>::clear` — and retail keeps
> **4, 48 and 11** of those respectively at **DISTINCT addresses**.

That is CD-7's relocation-restricted folding, re-derived independently and now
usable as a *targeting rule* rather than a post-hoc explanation: you can predict
whether an STL family is aliasable by reading its relocation list, before
building anything.

★★ **And byte-identity is necessary but NOT sufficient, for a second and
independent reason** (§5): an address that simply *is* the other function
produces identical bytes trivially. Before reading T1 bytes, ask **"could these
two have folded at all, given their layout?"** — this lane saw that failure mode
twice, once as `_S_sort`'s thunk-into-one-specific-`clear` and once as the
`CharBlendBone`/`CharTransDraw` map defect.

## 1. The stratum, regenerated rather than inherited

W6-C's `TIER45_BODIES_2026-09-11.md` sized the ALIAS-BLOCKED class at **31 rows /
24,016 B**, scoped to the native-linked >3-charge stratum. This lane re-derived
it whole-binary, because the briefed figures had to be tested literally and
because that scoping is not the one the work needed.

Method: for every named row with `0 < fuzzy < 100` in all 653 units that have one
(3,119 rows), `objdiff-cli diff --batch --include-instructions` at the graded
ruler — **without `--build`**, so nothing is rebuilt and no object is left
unpatched — then classify **every** charged instruction. A row is ALIAS-BLOCKED
iff *every* charge is a `diff_arg` whose differing typed_arg is a `Symbol`, i.e.
nothing a source change can reach.

| population (whole binary) | rows | bytes |
|---|---:|---:|
| named rows `0 < fuzzy < 100` | 3,119 | — |
| **ALIAS-BLOCKED (every charge a relocation NAME)** | **1,893** | **457,624** |
| … blocked by exactly ONE distinct pair (one adjudication crosses the row) | 1,647 | 346,820 |
| … blocked by >1 distinct pair | 246 | 110,804 |

⚠ **The `_S_sort` figure does not reproduce.** W6-C named it "11 rows / 4,712 B";
on this tree it is **8 rows / 3,424 B** (`FileMerger`, `Instance`, `UI`, `Rnd`,
`CharClipSet`, `Crowd`, `DirLoader`, `Mesh` — 424–432 B each, 4 charges each, one
distinct pair each). Main has moved since 09-11. Recorded, not smoothed over: a
prior lane's count is not inheritable, and this is the third consecutive wave in
which a briefed figure failed to reproduce.

## 2. The gates, and the vacuity found inside one of them

Each candidate pair (S = retail's spelling, F = ours) was put through:

1. **Strict T1** — our COMDAT for S and our COMDAT for F byte-identical **and**
   relocation-identical **compared by target NAME** (the linker's own `/OPT:ICF`
   condition, applied to two of our own COMDATs).
2. **Retail corroboration** — retail's body at `addr(S)` has the same size, every
   non-relocated word equal as a full 32-bit value, every relocated word equal in
   opcode/AA/LK.
3. **Injectivity** — the map places F at no other address.
4. **Population control** — no *other* retail address carries a
   branch-masked-identical body. If retail kept two shape-identical bodies apart,
   the family does not fold on shape and the alias would be fabrication.

Refusal census over the 1,155 single-pair candidates not already aliased:

| outcome | count |
|---|---:|
| size mismatch vs retail | 362 |
| **INJECTIVITY: map places F elsewhere** | 282 |
| **POPULATION: masked-identical retail siblings** | 251 |
| no COMDAT for F in our build | 71 |
| corroboration FAIL | 22 |
| passed all four gates | **167** (57,012 B) |
| … of those, **strict T1** | **69** |
| … … installed | **12** |
| … … refused as the zero-reloc stub class | 57 |

### 2.1 A fold-closure instrument, and why its aggregate specificity did not license its per-family use

To test pairs whose relocations differ only by template parameter, this lane
built an ICF **fold closure**: two COMDATs fold iff bytes are equal and every
relocation pair either names the same target or is itself a folding pair
(greatest fixed point, which is what an iterative folder converges to).

Validated both ways, because an instrument that answers "folds" to everything is
worthless:

* **negative control** — every pair of *distinct* retail addresses whose bodies
  are branch-masked-identical (retail proves it did **not** fold them):
  **779,735 pairs, 135 false positives = 0.017 %**;
* **positive control** — already-installed alias groups: **3,838 / 4,793
  confirmed**.

⛔ **And it is still WRONG on the one family that mattered.** 11 of those 135
false positives are `?insert@?$list`, precisely the family `AddRef`/`Release`
needed (§4.2). A 0.017 % aggregate specificity says nothing about a *chosen*
family — the same shape as the memory's "a per-stratum cost from a MIXED
population does not transfer to a pure one". **Retail's direct evidence
overrides the closure everywhere the two disagree**, and every verdict below is
the retail one.

## 3. INSTALLED — 12 folds, +3,756 B, both legs predicted exactly

### 3.1 Leg 1 — six type-independent folds (+2,960 B / +10 fns), first alias commit

| survivor address | family | folded spelling added | body |
|---|---|---|---|
| `0x8227d020` | `ObjPtrList<T>::Link` | `CharCollide` | 196 B, 3 relocs |
| `0x8240b050` | `RndMeshDeform::VertArray::SetSize` | `BandFaceDeform::DeltaArray::SetSize` | 80 B, 2 relocs |
| `0x8235c328` | `_Rb_tree<…>::_M_create_node` | `ScoreType` | 76 B, 1 reloc |
| `0x826e0950` | `_Rb_tree<…>::_M_create_node` | `unsigned → RndText::MeshInfo` | 100 B, 1 reloc |
| `0x8256ad18` | `hashtable::_M_initialize_buckets` | `Symbol → DataArray*` | 100 B, 3 relocs |
| `0x82656a00` | `__copy<SingerStats*>` | non-const iterator arm | 92 B, 3 relocs |

Worked example — `ObjPtrList<RndMesh>::Link` ≡ `ObjPtrList<CharCollide>::Link`:
both our COMDATs are **196 B, sha256 `d9bafec7b5ce340a`**, carrying the identical
three relocations **compared by target name** — `(4, __savegprlr_29)`,
`(56, ?AddRef@Object@Hmx@@QAAXPAVObjRefOwner@@@Z)`, `(192, __restgprlr_29)`.
Retail@`0x8227d020` is 196 B and corroborates word-for-word; fan-in 91;
`CharCollide` is placed nowhere else in the map; no masked-identical sibling
anywhere in retail. Every relocation is type-independent, so there is nothing
per-`T` that could keep the two apart.

★ **Three of the six landed in groups that ALREADY EXISTED** — `ObjPtrList<T>::Link`
at `0x8227d020` already carried ~20 folded spellings and was simply missing
`CharCollide`. That is the `STALE_SPELLING`-becomes-live mechanism CLAUDE.md
warns against pruning: as porting advances, new spellings enter a family whose
fold was proven long ago. It is also independent corroboration that these
families really do fold.

Predicted **+2,960 B / +9 fns**; measured **+2,960 B / +10 fns**
(Δcode% +0.028890 pp, Δfuzzy +0.000035 pp, 0 units off either ruler). The byte
figure — the one that was pre-registered from the row sizes — was exact; the
function count came in one higher than predicted because one further row crossed
`mpn` without being in the single-pair set.

### 3.2 Leg 2 — three pure-computation bodies + three destination-corroborated thunks (+796 B / +6 fns), second alias commit

| survivor | folded | body | evidence |
|---|---|---|---|
| `0x82667220` `__uninitialized_fill_n<CharProvider::CharacterEntry>` | `Key<Vector3>` arm | 60 B, **0 relocs** | total comparison — nothing masked |
| `0x8235c608` `_Rb_tree_base<pair<const int,float>>` ctor | `const MoveParent*` | 104 B, **0 relocs** | total comparison |
| `0x82557770` `hashtable::_M_find<Symbol>` | `Symbol → Lesson*` | 128 B, **0 relocs** | total comparison |
| `0x8240ddb0` `BinStream::operator delete` | `UIListCustomElement`, `UIListLabelElement`, `UIListSubListElement` | 4 B, 1 reloc | destination-level, §3.3 |

The three zero-relocation bodies are installed **against the house
`--install-min-relocs 1` default**, deliberately and narrowly. That filter's
stated rationale is that an unimplemented stub compiles to a bare 4-byte `blr`
with no relocations, so the zero-reloc class is exactly where the defect would
manufacture the identity that admits it. That rationale **cannot describe a
128-byte hash-probe loop**; the pool figure its docstring cites (7,535 COMDATs
byte-identical to a bare `blr`) is the 4-byte case. And with `relocs=[]` the
comparison is *total* — no name is masked — which is the anti-vacuity condition
`DATAINITFUNCS_2026-09-11.md` leaned on.

Predicted **+796 B / +6 fns**; measured **+796 B / +6 fns** (Δcode%
+0.007770 pp). Exact on both axes.

### 3.3 ⛔ The population control was VACUOUS for 4-byte bodies — caught, not relied on

The shape index behind gate 4 was built with `sz >= 8`, so **no 4-byte body was
ever indexed**. `shape[mask(thunk)]` therefore returned an empty sibling list for
every thunk in the tree — a gate that **cannot fail**, reading exactly like a
clean pass, and it had already admitted three pairs on that basis.

Caught by asking the index a question with a known answer: `0x8240ddb0` is a
4-byte `b MemFree` with fan-in 2,308, and the control reported **0 siblings**
while the index reported **0 four-byte entries in total**.

★ **The fix is not "lower the threshold to 4."** For a one-instruction body the
masked comparison is structurally the wrong instrument, because a PC-relative
branch's bytes encode a **displacement, not a destination** — the same trap
CLAUDE.md records for raw `memcmp`. Two 4-byte thunks with *identical bytes* at
different addresses call *different functions*. Measured on the map: **19 exact
byte-identical 4-byte bodies span 46 addresses**, mostly just shared
displacements rather than folds.

The three thunks were therefore re-adjudicated at **destination** level:
retail@`0x8240ddb0` decodes to `b 0x827bc430`, and the map names `0x827bc430`
`?MemFree@@YAXPAX@Z` — the exact symbol our relocation names.

⚠ **Stated limit on all twelve installs: identity does not ENTAIL folding.**
Retail holds a bare `blr` at **6 distinct addresses, unfolded**, so MSVC's ICF is
not exhaustive here. These rest on corroboration + injectivity, not on a theorem.
That same fact is why the 57 zero-reloc 4-byte candidates (§4.6) are refused
outright rather than withheld pending better evidence.

## 4. REFUSED — with the evidence, because a refusal is the deliverable here

### 4.1 ⛔ The `_S_sort` group — the brief's best-concentrated target — REFUSED

The handoff read: *"11 rows / 4,712 B on ONE survivor… one alias group would
cover all eleven rows — the cheapest thing a follow-up could do."*

What it actually is. Retail's survivor at `0x828043a8`, which the map names
`__destroy_aux<LocalePanel::Entry>`, is **4 bytes: `4bf144d8`** — a single
unconditional branch. Decoded, it is `b 0x82718880`, and `0x82718880` is
`?clear@?$_List_base@PAVSynthPollable@@…`.

So the survivor is a **tail-thunk into one specific `_List_base<T>::clear`**, and
our `~list<T>` spellings are tail-thunks into *their own* `clear<T>`. The fold is
real for a given `T` **iff our `clear<T>` also lives at `0x82718880`.**

It does not, for any `T` we have:

* `_List_base<T>::clear` has **41 map entries at 41 DISTINCT addresses** — the
  family does not fold across `T`, exactly as CLAUDE.md's CD-7 already records
  ("42 addresses, reloc-identical surplus 0, because its members differ in four
  `bl` targets (per-`T` node deallocators)"). Five of those bodies are
  branch-masked-identical across **11 addresses** that retail nonetheless kept
  apart.
* Of our **139** `~list<T>` COMDATs, every one is a 4-byte thunk to its own
  per-`T` `clear<T>`, and **not one** of those targets is placed at
  `0x82718880`. The only one the map can place at all is
  `~list<PassiveMessage*> → clear<PassiveMessage*>` @ `0x825df840` — a
  **different address**.

⇒ **REFUSED.** Strictly the verdict is *UNPROVABLE* rather than *refuted* for the
138 spellings whose `clear<T>` is unnamed in the map (missing evidence, the
`NEEDS_MAP_ID` class), but the one datum available points against, and the
family's 41-distinct-address structure makes the fold implausible. Installing it
would have been fabrication across 8 rows / 3,424 B.

★ Note what the *shape* of this target did: it presented as one survivor covering
many rows, which reads as leverage. The leverage was real and the fold was not —
concentration is a reason to adjudicate carefully, not a reason to believe.

### 4.2 `AddRef` / `Release` — REFUSED, and W6-B's stated premise is REFUTED

`OBJECTCPP_TU_2026-09-11.md` deferred this correctly but for a reason it
under-stated. Its sufficient condition was "(a) retype `mRefs` to
`std::list<ObjRefOwner*>` across 15 files **and** (b) an adjudication that
`list<ObjRefOwner*>::insert`/`::erase` genuinely fold into groups 1481 and 11",
and it characterised (b) as *"a real, provable fold — lists of 4-byte pointers
are byte-identical instantiations."*

**(b) is false, measured on retail bytes.**

*insert.* The map holds **five** `?insert@?$list@PA…` spellings (all pointer `T`,
all exactly 100 B) at **five distinct addresses**: `0x822b55e0` BandCamShot*,
`0x822b5728` RndDrawable*, `0x823b60c0` RndTransformable*, `0x823d14c0` Object*
(group 1481's survivor, fan-in 289), `0x827e5318` MidiParser*. Four of the five
are identical in **every word except one**, a `bl` at `+0x24`:

```
list<Object*>::insert      @0x823d14c0 : +24 -> 0x82520150   (64 B body)
list<RndDrawable*>::insert @0x822b5728 : +24 -> 0x822b4ca8   (72 B body, different)
```

That is the per-`T` node allocator (`_M_create_node<T>` on our side), and because
those callees are genuinely different functions — **different sizes, different
bodies** — retail did not fold the four inserts. Lists of 4-byte pointers are
therefore *not* byte-identical instantiations; they are byte-identical **except
at the one relocation that matters**.

*erase.* Group 11's survivor `0x82bb33d8` (`erase<Voice*>`) is **84 B**; the only
other pointer-`T` erase in the map, `0x82276828` (`erase<FileCache*>`), is
**96 B**. **Different sizes cannot fold** under any ICF.

⇒ `list<ObjRefOwner*>::insert`/`::erase` would receive their **own** addresses.
Aliasing them into groups 1481 and 11 is fabrication, and the 15-file `mRefs`
retype therefore bills **exactly zero** — which is what W6-B suspected and this
lane can now assert. **Do not fund the retype for metric reasons.** (It may still
be right as a *type-correctness* change; that is a different argument, and
CLAUDE.md's standing directive says accuracy can outrank the headline.)

### 4.3 ⛔ `Tour::SyncProperty` — 11,552 B, the largest pair in the stratum — REFUSED, and it is probably a MAP defect

The single highest byte-weighted adjudication available: retail spells
`?SyncProperty@Tour@@…` where **16 rows** of ours spell
`?SyncProperty@Object@Hmx@@…`, worth **11,552 B**. The rows are `SyncProperty`
overrides across unrelated classes (`ChordShapeGenerator` 2,644 B,
`RndTexRenderer` 1,380 B, `CalibrationPanel`, `SongSectionController`,
`DialogDisplay`, `CharBone`, …), each tail-calling its base.

It is not a fold:

* retail@`0x8235c2e0` is **72 B**; our `Object::SyncProperty` COMDAT is **720 B**
  — corroboration fails on size by 10×;
* `?SyncProperty@` has **407 map entries at 407 DISTINCT addresses** — the family
  never folds;
* `?SyncProperty@Object@Hmx@@…` is **absent from the map entirely**.

⇒ **REFUSED as an alias.** The likelier reading, offered as a hypothesis with its
evidence rather than a conclusion: `0x8235c2e0` (72 B, fan-in 19, a short generic
property-forwarding body) **is** `Hmx::Object::SyncProperty`, and the map's
`Tour::SyncProperty` name on it is **wrong** — the MPNGAP-1 shape, where our
source is right and the map is not. If so the fix is a map identification, and
the 720 B of our `Object::SyncProperty` against retail's 72 B is a *second*,
separate finding worth a body lane. Both are handed off (§6); neither is
something an alias may paper over.

### 4.4 The `vector<T>::resize` family — REFUSED (and this is what W6-C's `Geo::CheckBSPTree` actually is)

W6-C listed `Geo::CheckBSPTree` (1,336 B) among the ALIAS-BLOCKED rows and
described the cluster as `_S_sort` list-destructor charges. Read off the tree,
its 6 charges are neither: they are retail `?resize@?$vector@VStreakInfo@Stats@@…`
against our `?resize@?$vector@VVector2@@…`.

Our two `resize` COMDATs *are* byte-identical (116 B, sha256 `568582ba1bfd519b`)
and retail@`0x82656ac8` corroborates exactly — this one looks admissible right up
to the last gate. It fails because the relocations differ by name
(`_M_fill_insert<StreakInfo>` vs `_M_fill_insert<Vector2>`) and the chain does not
close: `?_M_fill_insert@?$vector@` has **91 map entries at 91 distinct
addresses**, and `?resize@?$vector@` has **78 entries at 78 distinct addresses**
of which **48 sit in 10 branch-masked-identical groups retail kept apart**.

⇒ shape identity is not sufficient; the family provably does not fold. **REFUSED.**
The same gate refused `_M_fill_insert<vector<Object*>>` ← `<vector<int>>`
(4,004 B / 12 rows) on the population control alone: closure said fold and
corroboration passed, but retail carries **three further masked-identical bodies
at distinct addresses**.

### 4.5 `OggFree` ← `BinStream::operator delete` — REFUSED on injectivity (6,112 B)

Byte-identical, relocation-identical by name, corroborated, 28 rows. It fails the
one gate that matters: **the map places `OggFree` at `0x8249b200` and the
survivor at `0x8240ddb0`** — two distinct addresses, so retail did not fold them.
The house sweep refuses it for the same reason; recorded here because it is the
most attractive-looking pair in the stratum and will be re-proposed otherwise.
(The validator's own EXEMPT note independently calls the `OggFree` row junk:
0 `bl` callers.)

### 4.6 The zero-relocation stub class — 57 pairs / 13,772 B REFUSED

57 of the 69 strict-T1 candidates are **4 bytes with zero relocations** — a bare
`blr`. This is the class the house `--install-min-relocs 1` filter exists to
withhold, and this lane found the direct retail evidence for why: **`4e800020`
(bare `blr`) occupies 6 DISTINCT retail addresses, unfolded.** A `blr`-identical
pair is therefore *no evidence of folding whatsoever*, quite apart from the stub
hazard. Refused, not withheld pending — the instrument cannot ever admit them.

## 5. WITHDRAWN — the `CharBlendBone`/`CharTransDraw` fabricated alias, and the generator defect behind it

Flagged mid-lane by the coordinator on behalf of lane W7-C
(`docs/decomp/SHADER_LAYOUT_2026-09-13.md` §7.1). W7-C fixed the **map** half —
re-homing `0x823c8908` to `CharTransDraw.cpp`, +1 fn / +316 B measured — and left
the **alias** half here. Adjudicated independently before acting, and the
independent evidence agrees and sharpens it.

The group claimed `?SetType@CharBlendBone@@UAAXVSymbol@@@Z` (survivor) folded
with `?SetType@CharTransDraw@@UAAXVSymbol@@@Z`.

**Its T1 byte evidence is CORRECT and its CONCLUSION is INVERTED.** Our two
bodies are identical *except* in their vbase displacement words:

```
ours, CharBlendBone : … 817effd0 … 386bffd0  816bffd0 … 357effcc … 388bffd0 …
ours, CharTransDraw : … 817effc8 … 386bffc8  816bffc8 … 357effc4 … 388bffc8 …
retail @0x823c8908  : … 817effc8 … 386bffc8  816bffc8 … 357effc4 … 388bffc8 …
```

The delta is exactly **8** — the `0x3c` vs `0x34` vbase split — and retail
carries the **CharTransDraw** form. So retail@`0x823c8908` **IS**
`CharTransDraw::SetType`; the bytes matched not because two functions folded but
because the address simply *is* the other function. And because the two bodies
**differ**, no ICF could ever have folded them: the question is foreclosed by
layout *before* any byte comparison is read.

Withdrawn with a `MAP_DEFECT_INVERTED_CONCLUSION` record; group kept with
`folded: []` per house convention, nothing pruned.

### 5.1 The generator inherits map defects BY CONSTRUCTION

The group's own `evidence` string states the rule that produced it:

> *"T1 = RB3 retail bytes **at the survivor address** are byte-identical (modulo
> relocated fields) to **our compiled body for the folded spelling**"*

That predicate cannot distinguish **"two functions folded"** from **"the map
misnamed this address"**. Whenever the map attaches name *A* to an address that
is really function *B*, the rule finds our body for *B* matching retail there and
emits "A ≡ B folded". ⇒ **`tools/icf_alias_build.py` converts every map
misidentification into a fabricated fold, automatically.** Lane W5-D flagged the
same mechanism; this is a further instance.

⛔⛔ **And the withdrawal ledger is NOT an input to the generator.**
`grep -n withdrawn tools/icf_alias_build.py` → **zero matches**. `--merge` only
carries hand-verified groups *forward* (additive); there is no suppression list.
So **all 563 groups in `symbol_aliases.json` carrying a `withdrawn` key are only
as durable as "nobody re-runs the generator"** — including the ten CONTRADICTED
folds a prior wave withdrew and the `FABRICATED_CLOSURE_NOT_PARTITION` set. This
is the reverse of the `a745039e` hazard (a prune that cost +94,616 B to undo):
here a regeneration would silently *re-fabricate*. **Recommend the generator read
`withdrawn` as a hard denylist keyed on (address, spelling).** Not done in this
lane — it is a tool change with its own A/B, and it is filed as handoff H1.

For *this* group specifically, regeneration is now blocked by the **map fix**,
not by the withdrawal: the differing words sit at COMDAT offsets
`0x90 / 0xa0 / 0xa4 / 0xc8`, none of which is in our relocation list, so the
generator's "modulo relocated fields" masking cannot hide them and the pairing
cannot re-derive while the map is correct. **The map fix is load-bearing; the
withdrawal record alone would not have held.**

## 6. Handoffs

1. **H1 — `tools/icf_alias_build.py` must read `withdrawn` as a denylist.**
   §5.1. Zero references today; 563 groups' worth of adjudication is unprotected
   against a regeneration. Keyed on (address, spelling), with a loud count of
   suppressions so a silent no-op is visible.
2. **H2 — `0x8235c2e0` is very likely `Hmx::Object::SyncProperty`, not
   `Tour::SyncProperty`** (§4.3). 72 B, fan-in 19, a short generic
   property-forwarding body; `?SyncProperty@Object@Hmx@@…` is absent from the map
   entirely, and the family has 407 entries at 407 distinct addresses. A map
   identification, worth up to **11,552 B across 16 rows** — the largest single
   item this lane found and the largest it refused.
3. **H3 — our `Hmx::Object::SyncProperty` is 720 B against retail's 72 B.**
   Independent of H2 and larger in consequence: a 10× base-class divergence on a
   method 16 rows tail-call. A body lane, not an alias one.
4. **H4 — the `_S_sort` cluster needs `clear<T>` map identifications, not an
   alias** (§4.1). 8 rows / 3,424 B. 138 of our 139 `~list<T>` thunks target a
   `clear<T>` that the map cannot place at all; until those addresses are named
   the fold is unprovable in either direction. This is the `NEEDS_MAP_ID` class.
5. **H5 — do NOT fund the `mRefs` retype for metric reasons** (§4.2). The alias
   half is refuted on retail bytes, so the 15-file retype bills exactly zero. It
   may still be correct as a *type-correctness* change; that argument has to be
   made on its own terms.
6. **H6 — 246 rows / 110,804 B are blocked by MORE THAN ONE distinct pair.**
   Out of scope here (this lane only adjudicated rows a single adjudication could
   cross). Every pair must be proven for the row to bill anything, so the
   effective price per byte is worse; rank before funding.

## 7. What this lane did NOT do

- **Did not install the sweep's 647 ADMITs.** `tools/ourside_fold_sweep.py` runs
  clean (verified — the W5-C fail-closed fix holds; `EXIT=0`, 682 pairs,
  647 ADMIT / 35 REFUSE covering 2,329 sites). It was **not** used as an install
  oracle, because almost all of its ADMITs are 4- and 8-byte thunks — the class
  where coincidental identity is cheap and where §3.3 shows the masked
  comparison is the wrong instrument. A blanket install there is the
  "unproven alias lifts the score by construction" hazard at scale.
- **Did not touch the 1,893-row / 457,624 B stratum beyond the single-pair
  slice.** 1,155 candidates adjudicated; 12 installed.
- **Did not re-run `icf_alias_build.py`** to observe empirically whether it
  re-derives the withdrawn group. The masking argument in §5.1 is analytic, from
  the COMDAT relocation offsets; an empirical confirmation would need the census
  and evidence inputs regenerated, and re-running a generator that ignores
  withdrawals is itself the hazard.
- **Did not run the native gate** — `src/` did not move. This lane touched only
  `scripts/symbol_aliases.json` and `docs/`. Stated rather than assumed: the
  gate's trigger condition is a change under `src/system/**`, `src/band3/**` or a
  shared header, and none applies.
- **Did not pursue the `Spotlight`/`RndEnviron` layout items** W6-C handed off;
  they are single-class body lanes, not alias work.

## 8. Gates

Run on the rebased branch after a full `./tools/ninja-locked` (rc=0):

```
icf_alias_finder.py --validate
  VALIDATE: PASS -- 1358 map-consistent, 235 tolerated, 0 contradicted, 1595 total
  CONTRADICTED (FATAL)       0

verify_objs_patched.py --verify-manifest
  [patch-state] OK: 1205 decomp, 3084 target objects match   (rc=0)
```

This lane touched `scripts/symbol_aliases.json` and `docs/decomp/` only, so the
native gate's trigger condition does not apply to its own changes. It was run
anyway, because the final rebase onto `11a38cee` brings five `src/system/**`
files in from lane W7-INLINE-BUDGET and the gate has caught `main` broken four
times:

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

18/18 with **0 SKIPs** — the rule with the track record, applied rather than
just reading `PASS`.

## 9. Per-change predicted vs measured

(Commits are named by position rather than sha: this branch was rebased three
times while lanes W7-C, W7-INLINE-BUDGET and W7-FILEMAKEPATH landed, and a sha
written into a doc does not survive that.)

| change | predicted | measured | commit |
|---|---|---|---|
| 6 type-independent folds | +2,960 B / +9 fns | **+2,960 B / +10 fns** | commit 1 of 3 |
| 3 zero-reloc bodies + 3 thunks | +796 B / +6 fns | **+796 B / +6 fns** | commit 2 of 3 |
| withdraw the fabricated `SetType` alias | **0** | **+0 B / +0 fns** | commit 3 of 3 |

Every byte figure was pre-registered from the row sizes before the A/B ran, and
both install legs were exact. `ALIAS_SUSPECT` fired on the two INSTALL legs, which is
**expected and carries no information on an alias-only patch**: forgiveness never
creates byte agreement, so the `none` control is flat *by construction*. It did
**not** fire on the withdrawal, because there the default ruler did not move
either (+0 / +0) — the alert keys on a default-up/`none`-flat divergence. CLAUDE.md
is explicit that this flatness is the hazard's **signature, not a clearance** —
the license for each install is the retail-byte evidence quoted per group in the
`evidence` field, never the control.

## 10. The one-line lesson

**Byte identity is necessary and never sufficient.** Two independent ways it
lies, both seen in this lane:

1. the bodies match but their **relocations are per-`T`**, so the linker never
   folded them (`resize`, `insert`, `~list` — 48, 4 and 11 retail addresses kept
   apart); and
2. the bodies match because the address **IS** the other function and the map
   said otherwise (`CharBlendBone`/`CharTransDraw`).

The cheap screen for (1) is *read the relocation list and ask whether any target
varies with `T`.* The cheap screen for (2) is *ask whether these two could have
folded at all, given their layout,* **before** reading a single byte.
