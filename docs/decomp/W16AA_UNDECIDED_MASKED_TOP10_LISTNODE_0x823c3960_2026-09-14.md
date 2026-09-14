# W16-AA — the UNDECIDED_MASKED top-10, and naming the `list<ConstraintSystem>` node creator

**Lane:** W16-AA (opus) · **Branch:** `w16-aa`, off main `196fe28c` · **Worktree:** `~/tmp/wt-w16-aa`
objdiff ruler `name_check` (graded), read from `report.json` `provenance` on every leg below.

| | matched_functions | matched_code | matched_code_percent | fuzzy_match_percent |
|---|---:|---:|---:|---:|
| lane baseline (`196fe28c`) | 43,301 | 3,988,808 | 38.930557 | 49.512030 |
| lane final | **43,302** | **3,988,880** | 38.931263 | 49.512733 |
| **Δ** | **+1** | **+72 B** | +0.000706 | +0.000703 |

Commits: `83a65ed6` (three instruments), `17578db8` (Item 2 naming).
**Zero alias withdrawals. Zero source edits. One map row added.**

---

## Headline

**The brief's hypothesis does not survive contact with the bytes, and the reason is mechanical.**
The lane was dispatched on the premise that `UNDECIDED_MASKED` aliases hide wrong callees, to be exposed by
identifying the unnamed branch destination (MAPID-1's lever). Across every row of the top-10 that could be
adjudicated on retail bytes, **the retail evidence CONFIRMS the fold rather than refuting it.** No membership
was withdrawn, because none deserved to be.

The mechanism is a single sentence: **`w16s_alias_census.py` computes its `/OPT:ICF` closure on OUR build,
but the fold it is adjudicating happened in RETAIL's build.** Where retail folded the *callees* and our `/O1`
output does not, the closure under-merges, the parent pair falls out of `FOLD_CONFIRMED_CLOSURE`, the retail
comparison masks the one differing word, and the row lands in `UNDECIDED_MASKED`. That class is therefore
mostly **a reporting gap in the instrument, not an accuracy exposure in the alias file.**

Corollary, and it is the reusable part: **a channel-(a) discriminator where retail calls ONE address at the
offset our two spellings disagree is positive evidence FOR the fold**, not absence of evidence. It says the
two callees our build keeps apart are one function in retail — which is exactly what the membership asserts,
one level down.

---

## The instrument: two masking channels, and which one the brief's lever reaches

`retail_compare` adjudicates a relocated word **only** when it is a `b`/`bl` with `AA=0` *and* the map names
the decoded destination. So masking has two channels:

| channel | what it is | closable by naming? |
|---|---|---|
| **(a)** | `b`/`bl`, destination UNNAMED in `target_symbol_map.json` | **yes** — this is the brief's lever |
| **(b)** | ANY non-branch relocation (HA16/LO16 data ref, load/store displacement) — the 6-bit opcode is verified and the destination is **never consulted at all** | **no**, structurally |

`tools/w16aa_masked_diff.py` splits a membership's discriminators between them.

### ⚠ A size-ranked top-10 is NOT representative of its class — measured

| population | channel (a) only | channel (b) present | already-named |
|---|---:|---:|---:|
| the brief's size-ranked top-12 | 9 (75%) | **2 (17%)** | 1 |
| **all 643 UNDECIDED_MASKED** | **636 (98.9%) / 40,332 B (97.5%)** | **6 (0.9%) / 892 B** | 1 |

I began from the top-12 believing ~1 in 4 of the class was unreachable by naming. The population says **0.9%**.
The two channel-(b) rows are the two *largest* rows in the class, so size-ranking concentrates them ~19×.
⇒ Never brief a class's composition off its head; the head is selected on the very axis that biases it.

**But reachability of the census VERDICT is not reachability of a BUG.** 98.9% of the class can in principle
be moved out of `UNDECIDED_MASKED` by naming destinations; on the adjudicated evidence below, doing so would
move them to `FOLD_CONFIRMED`, not to a withdrawal.

---

## ⛔ A methodological error I made, caught before it reached the map

My first reading of gi=444 / gi=1302 / gi=514 was **"the discriminating callees have different sizes in our
build, therefore they cannot fold, therefore the alias forgives a wrong callee — withdraw."** That reasoning
is **wrong, and it is the STLPORT-1 disease in a new dress**: the sizes compared were **OURS**, and the fold
claim is about **RETAIL's** build. Our `FormatString::operator<<` family is 152–264 B; retail's is 124–216 B.
Our bodies license no inference at all about what retail's linker folded.

Two independent retail instruments then refuted the withdrawal outright (below). Had I acted on the size
reading I would have withdrawn ~5 sound memberships. **A tool's confident "cannot fold" is the claim most
worth auditing, because a withdrawal closes a vein nobody reopens.**

A second slip, caught by an internal contradiction rather than by luck: `w16aa_masked_diff.py` first picked
each side's **longest** COMDAT variant, while the census picks the first variant that compares **EQ to
retail**. On gi=109 that produced `masked_bytes_equal=False, reloc_shape_equal=False` — *impossible* for a
genuine `UNDECIDED_MASKED`, which is how it was caught. It had reported 5 channel-(b) discriminators where
the truth is 1 already-named one. **Diffing a pair the census never compared describes nothing.**

---

## Per-row findings

### gi=444 (116 B) + gi=1302 (112 B) — `MakeString` / `FormatString::operator<<` — **FOLD CONFIRMED**

Retail `0x827bb730` (map: `MakeString<const char*,int,int,int,int,int>`) calls **`0x827c40e8` six times in a
row**; retail `0x823abd20` (map: `MakeString<int,float,int>`) calls `0x827c40e8`, `0x827c4240`, `0x827c40e8`.
`0x827c40e8` is unnamed, 124 B, and calls `_snprintf`.

Reconstructing the type→address mapping from **all 35 map-named retail `MakeString` rows** (pairing each
instantiation's template arguments against its `operator<<` call sequence, windowed between the
`FormatString(const char*)` ctor and `Str()`) gives a **conflict-free** table:

| template arg | retail `operator<<` | observations |
|---|---|---:|
| `H` (int) · `PBD` (const char\*) · `VSymbol` · `_N` (bool) · `PAD` (char\*) | **`0x827c40e8`** | 19 + 8 + 2 + 2 + 1 = **32** |
| `M` (float) | `0x827c4240` | 24 |
| `VString` · `VFilePath` | `0x827c42c0` | 3 + 1 |

⇒ **retail folded the scalar/pointer `operator<<` overloads into one body.** The retail FormatString cluster
holds only ~4 distinct `operator<<`-shaped bodies (`0x827c40e8` 124 B, `0x827c4240` 128 B, `0x827c42c0` 124 B,
`0x827c4168` 216 B) against our **14** C++ overloads — so heavy folding is not a hypothesis, it is forced.

Both memberships follow immediately. gi=1302's two spellings, `MakeString<Symbol,float,const char*>` and
`MakeString<int,float,int>`, expand under the table to the identical call sequence
`40e8, 4240, 40e8` — **which is exactly the sequence retail's body contains**. gi=444's two spellings both
expand to six calls to `40e8`. The bodies are identical in retail; the fold is real.

Residual, stated rather than hidden: `_K` (unsigned long long) is the one type in gi=444's folded spelling
never directly witnessed. It is scalar, and every witnessed scalar lands on `40e8`; the partition
(scalar/pointer → `40e8`, float → `4240`, class-with-`Str()` → `42c0`) predicts it. Inferred, not observed.

### gi=1050 (116 B) — `NetLoaderXbox` / `NetLoaderStub` destructors — **FOLD CONFIRMED**

Channel (b): the discriminators are the two `??_7…@6B@` vtable pointers (HA16 at +0x18, LO16 at +0x24), which
the census cannot adjudicate at all. Adjudicated here directly instead — and the answer needs no data
resolution, because retail supplies a **map-named witness**:

```
??_GNetLoaderStub@@UAAPAXI@Z  @0x827d00a8  +0x1c  ->  bl 0x827cfcb0  =  ??1NetLoaderXbox@@UAA@XZ
```

`NetLoaderStub`'s own mapped scalar-deleting destructor calls the address the map names
`NetLoaderXbox::~NetLoaderXbox`. Two independent map rows agree that the two destructors are **one address**.
Both classes exist in retail (`NetLoaderStub` has 3 mapped members). The membership is sound.

### gi=95 (432 B) — `TeleportTarget@BandCamShot` / `@HamCamShot` — **NOT a retail fold; sound identification; LEAVE**

Channel (b), and the only row whose literal claim is false. The seven discriminators are all
**function-local statics whose mangled names embed the enclosing function** — `?msg@?4??TeleportTarget@…`,
the guard `?$S10@`/`?$S11@`, and `??__Fmsg@…`. Two distinct function scopes own two distinct static objects,
so the two bodies cannot be one function.

Retail settles it without needing that argument: **`HamCamShot` does not exist in RB3.** The map holds 165
`CamShot@@` rows, **all `BandCamShot`, zero `HamCamShot`**, and exactly one `TeleportTarget` row —
`0x822b2e20 = ?TeleportTarget@BandCamShot@@…`. `Ham*` is the Dance Central lineage our tree compiles from the
DC3 engine source; retail does not contain it. So there is no pair of retail functions to fold.

**It is nonetheless a sound *identification*** — our two COMDATs are masked-equal with identical relocation
shape, differing only in those scope-local statics: the same code under two class names, and retail's single
body at `0x822b2e20` is that code.

**Not withdrawn, deliberately.** The only referrers of the `HamCamShot` spelling are other `HamCamShot`
methods (`HamCamShot::EndAnim` +0xbc, `HamCamShot::Reteleport` +0x444) — no `BandCamShot` method calls it, so
the membership forgives nothing live today. That is precisely the class CLAUDE.md forbids pruning
("do NOT prune the classes that currently forgive 0 — they become live as porting advances; a prior prune
cost +94,616 B to reverse").

### gi=730 ×2 (120 B each) — `ObjDirItr<T>` constructors — **retail supports cross-`T` folding**

Discriminator is `?Advance@?$ObjDirItr@V<T>@@@@AAAXXZ`; retail's destination `0x823d13e8` (212 B) is unnamed.
Retail names two callers of it:

```
0x823d1a78  ??0?$ObjDirItr@VCharClipGroup@@@@QAA@PAVObjectDir@@_N@Z   -> 0x823d13e8
0x823d1af0  ??E?$ObjDirItr@VCharClip@@@@QAAAAV0@XZ                    -> 0x823d13e8
```

Two **different** `T` reaching one `Advance` ⇒ `ObjDirItr<T>::Advance` folds across `T` in retail. Meanwhile
`?Advance@?$ObjDirItr@VCharPollable@@@@` survives separately at `0x823d1528`, also 212 B — so the folding is
partial, and this family's fold classes are real but not universal. That is positive evidence for the
membership's *kind* of claim; the specific `T`s here (`DepthBuffer3D`, `HamSupereasyData`) are Ham/DC3 types,
so they are in gi=95's situation. **Left alone.**

### gi=514 / gi=513 / gi=230 / gi=231 (724 B) — Accomplishment/Goal comparators — **UNRESOLVED, hand to a map lane**

Discriminators are `??RAccomplishmentCmp` vs `??RGoalCmp` (and `AccomplishmentCategoryCmp`), reached through
unnamed `0x825f32a8` / `0x825f3640` / `0x825f33d0`. Positive evidence exists: **`0x825f32a8` (176 B) is called
by both `<AccomplishmentCmp>` and `<GoalCmp>` algorithm instantiations**, which is what a folded comparator
looks like. But the cluster's map is internally inconsistent and I stopped rather than build on it.

**⚠ Two instruments disagree here, and acting on the first would have corrupted two correct map rows.**
Caller populations look like a clean swap — all 7 callers of `??RAccomplishmentCmp@@`(`0x825f7000`) are
`<AccomplishmentCategoryCmp>` instantiations, and all 7 callers of
`??RAccomplishmentCategoryCmp@@`(`0x825f6f20`) are `<AccomplishmentCmp>` instantiations; unanimous, 14 sites,
opposite directions. A comparator is called by the algorithm instantiated on it, so that reads as decisive.

**Sizes refute it, exactly:**

| spelling | our `fn_size` | retail size @ mapped address |
|---|---:|---|
| `??RAccomplishmentCmp@@` | 104 | **104** @`0x825f7000` |
| `??RAccomplishmentCategoryCmp@@` | 112 | **112** @`0x825f6f20` |
| `??RGoalAlpaCmp@@` | 52 | **52** @`0x82555e68` |

3 of 3 exact on the **current** assignment. So the comparator rows are right and the inconsistency lives in
the cluster's **algorithm-instantiation** names (near-identical templates, exactly what an oracle-derived
generator mis-assigns). Also: `??RGoalCmp@@` (ours, 144 B) **is absent from the map entirely**, and
`0x825f32a8` is 176 B, matching neither of our comparators.

⇒ The discriminator's identity depends on names in a cluster demonstrably unreliable. **No alias touched, no
map row touched.** Per the brief's own rule: a masked comparison is not evidence against a fold, only absence
of evidence for one.

### gi=109 (136 B) — **sound; the `UNDECIDED` label is a representative-selection artifact**

Its single discriminator is at an **already-named** destination (`0x82710808`), and the two targets are
**CO-FOLDED in our own closure**. It is in the class only because the closure's representative (longest
variant) and the retail comparison's representative (first EQ variant) are different variants of the
survivor, which has 2. Nothing to identify; nothing to withdraw.

### gi=563 (112 B) — not adjudicated

Discriminator `__destroy_range_aux<…>` at unnamed `0x8246ebe0`; deeper pair is the scalar-deleting destructor
of `Key<vector<Color>>` vs `Key<vector<Vector2>>`. Left for a lane that reaches the `Key<>` family; no map
witness was available at the cost of the other rows.

---

## Item 2 — naming `0x823c3960`, the `list<ConstraintSystem>` node creator

W16-Y identified this body and **declined to name it**, on two open questions. Both are settled on retail
bytes (72 B extent, decoded):

```
+0x18  li  r3,24                          (8 B _List_node_base + 16 B ConstraintSystem)
+0x20  bl  0x827bd208  ?MemOrPoolAlloc@@YAPAXH@Z                       <- allocator question
+0x30  addi r3,r3,8
+0x34  bl  0x823958a8  ??$_Copy_Construct@UIKTarget@CharIKHand@@…      <- copy-ctor question
```

Fan-in over the whole `.text`: **exactly one caller**, `?insert@?$list@UConstraintSystem@CharBlendBone@@…`
@`0x823c3ac8` — confirming W16-Y.

Our build calls `MemOrPoolAllocSTL` and `_Copy_Construct<ConstraintSystem>`. **Both differences are already
forgiven by installed alias groups** (`MemOrPoolAllocSTL` is a folded member of survivor `MemOrPoolAlloc`, the
debug-stripped 1-arg fold from `utl/MemMgr.h`; our `_Copy_Construct` spelling is a folded member of survivor
`_Copy_Construct<CharIKHand::IKTarget>`). The remaining two relocations are the unnamed
`__savegprlr_29`/`__restgprlr_29` helpers, which objdiff forgives as placeholder targets. Body: **MASKED-EQ,
72 B vs retail's 72 B**.

The spelling was **read from the built COFF, not guessed** — my first guess (`…PAU_List_node@…`) does not
exist; the real return type is `_List_node_base*`. (A fresh worktree's reflinked objs are pre-renamer; this
tree was fully built first.)

**Pre-registered: +1 function / +72 B**, caller unchanged in both directions (its `bl` targeted an unnamed
address and so was already uncharged; our `insert` relocates to precisely the name assigned, so naming turns
a forgiven site into a checked site that passes).
**Measured by set-diff: CROSSED IN 1 row / 72 B, FELL OUT 0.** Exact.

### ⚠ A plausible force multiplier that is NOT one

All **31** map-named retail `list<>::_M_create_node` rows call `MemOrPoolAlloc`; **none** call
`MemOrPoolAllocSTL`, while our build emits `MemOrPoolAllocSTL` in every one. That reads like a systematically
mis-ported `StlNodeAlloc` policy with wide blast radius. **It is not.** The two folded in retail and
`scripts/symbol_aliases.json` already records it. Do not re-open it as a source lever on the 31/31 count —
the count is real and its natural interpretation is wrong.

---

## What I did NOT do, and why

- **No alias withdrawals.** None of the top-10 earned one. Where retail could adjudicate it confirmed the
  fold; gi=95's membership is not a retail fold but forgives nothing live, and pruning inert memberships is
  forbidden.
- **Did not name `0x827c40e8` / `0x827c4240` / `0x827c42c0`** despite identifying all three. Naming is a bet:
  it converts forgiven call sites into checked ones. Our `operator<<` bodies are 152–264 B against retail's
  124–216 B, so **no row could cross even if named** — zero upside — while every spelling not in the proven
  set (`_K`, `I`, `J`, `K`, `PAX`, `_J`) would newly charge. Negative expected value. The identification is
  recorded above for a lane that first ports `FormatString::operator<<`.
- **Did not swap the Accomplishment/AccomplishmentCategory comparator map rows** — see the two-instrument
  conflict above; sizes say the current rows are correct.
- **Did not repair the Accomplishment/Goal algorithm-name cluster.** Real defect, but a map lane's job and far
  wider than this brief.
- **Did not run the permuter** (OFF by standing directive) and **made no source edits**.
- **gi=563 not adjudicated** — budget went to the rows with retail witnesses.

## Tools added

- `tools/w16aa_masked_diff.py` — locates a membership's discriminator and classifies its masking channel.
- `tools/w16aa_target_fold.py` — reduces a membership to "is the discriminating target pair co-folded?",
  entirely our-side. **Read its caveat**: it answers for OUR build, which is the wrong binary for this
  question — that is the lane's central finding, kept in the tool so the next reader cannot miss it.
- `tools/w16aa_adjudicate_dest.py` — which of our candidate COMDATs is the retail body at an address; both
  sides are function extents (`fn_raw`), so the STLPORT-1 one-sided reader artifact is out of reach.

## For an escalation lane

The productive instrument on this class is **not** naming the destination. It is the **retail-side fold
witness**: find a map-named retail function that must call spelling X, decode its call at the relevant site,
and see whether it lands on the same address as the survivor's. That settled gi=444 (35 rows, 32
observations), gi=1302 (an exact 3-call sequence match), gi=1050 (one deleting destructor) and gi=730 (two
`T`s, one `Advance`). It needs no map edit and carries no un-pairing risk. Mechanising it over all 643 rows
is the natural successor to this lane.
