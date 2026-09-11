# PINCHANNEL — lane W5-D: the pin channel W4-B said was undrained

**Branch** `w5-pinchannel` · **worktree** `~/tmp/wt-w5-d` · **base** `a8fdfd65`
(asserted `git merge-base --is-ancestor` before the first edit)

**Composed result: +9 matched functions / +540 B / +0.005272 pp** over four A/B
runs, every leg at a `symbols.txt` split fixed point, 0 regressions in any run.

W4-B closed MAPVEIN4 with *"every remaining item is blocked on PINNING, not
identification. The map channel here is close to drained; the pin channel is
not."* That reading is correct, and one of its own refusals was refused for a
reason that does not survive contact with the pin channel — see §4.

**Anti-vacuity check before any name-keyed work** (FOLDPROVE-2): the worktree
was FULLY BUILT first; `verify_objs_patched.py --verify-manifest` returned
**exit 0** over 1,205 decomp + 3,085 target objects, and retail mangled names
were confirmed resolvable in the target objs. A reflinked tree reads every
retail name as absent, and that failure agrees with a "this name does not
exist" prior.

## 0. Per-item table

| # | item | old pin | new pin | pairability check | pre-registered | **measured** | kept |
|---|---|---|---|---|---|---|---|
| 1 | `??_G` CreditsPanel ↔ ContentLoadingPanel | `CreditsPanel.cpp 0x82614098-0x826140EC`; `ContentLoadingPanel.cpp 0x827AF108-0x827AF15C` | **swapped** + both map names swapped | PASS after move — each obj defines its own `??_G` | +2 / +168 B | **+5 / +208 B** | KEPT |
| 2 | `Object::AddRef` / `::Release` | `DirLoader.cpp` (tail-of-block / mid-block) | `Object.cpp` (boundary move + 3-way carve) | PASS — `Object.obj` defines both | +2 / +252 B | **+0 / +0 B** | KEPT (accuracy) |
| 3 | `list<EventSink>::insert` + its creator | `EventTrigger.cpp 0x82749630-0x82749694` | `Msg.cpp`, + name `0x82749550` | PASS — `Msg.obj` defines both | +2 / +172 B | **+2 / +172 B** exact | KEPT |
| 4 | 9 fan-in-1 `??_G` rows → the RndWind / NetCacheMgrXbox chain | `NetCacheMgr_Xbox.cpp` 936-B island; `MeshAnim.cpp` 84-B island | `Wind.cpp` / `NetCacheMgr_Xbox.cpp`, 2 map renames, 1 alias withdrawn | PASS — `Wind.obj`, `NetCacheMgr_Xbox.obj` define theirs | +4 / +320 B (fallback +3/+236 stated) | **+2 / +160 B** | KEPT |
| 5 | jeff 158-site truncation (W4-C H1) | — | — | — | n/a | **characterised, nothing deployed** | n/a |

**Composition witness.** Each run's leg A reproduces the previous run's leg B
exactly: 42627 → 42632 → 42632 → 42634 → 42636. `matched_code` likewise
3,847,588 → 3,847,796 → 3,847,796 → 3,847,968 → 3,848,128.

## 1. Item 1 — CreditsPanel ↔ ContentLoadingPanel: the pin WAS the blocker

W4-B called this the best-evidenced rename in its `??_G` census and refused it
on the pairability gate. It was right to refuse and right about the cause: the
two units **interleave**, each holding a lone 0x54-byte `.text` block inside the
other's territory.

Retail-byte proof, map-independent — a `??_G` scalar-deleting destructor calls
its own class's `??1`, and the `bl` target is read off the decoded body:

```
fn_82614098 -> bl fn_82613A20 = ??1ContentLoadingPanel@@UAA@XZ
fn_827AF108 -> bl fn_827AEA68 = ??1CreditsPanel@@EAA@XZ
```

Both dtors have retail `bl` fan-in **exactly 1** (W4-B's fold discriminator),
so this is a transposition, not an ICF fold survivor. Spatial corroboration:
`0x82614098` sits between `?SetType@ContentLoadingPanel@@$4…` and
`?FinishLoad@ContentLoadingPanel@@UAAXXZ`; `0x827AF108` between
`?SetType@CreditsPanel@@$4…` and `?FinishLoad@CreditsPanel@@EAAXXZ`.

Pairability re-checked **in the post-move state, before renaming**: each obj
defines its own `??_G`. Map injectivity preserved — 2 duplicate names before and
after, the same 2 W4-B recorded as pre-existing.

★ **`.pdata` demonstrated derived, again.** I edited `.text` only; the split
swapped exactly one 8-byte `.pdata` record each way (`0x82228F90-0x82228F98`
and `0x82240DC8-0x82240DD0`) — one record per moved body, **zero funclets**,
which is what the pre-registration assumed. Neither unit drained a block, so no
entry had to be deleted.

### 1.1 ★ The prediction miss is the finding: `??_E` collateral, opposite sign

Pre-registered **+2 / +168 B** (two 84-B rows at fuzzy 99.7619, each carrying
exactly one charged relocation-name site). Measured **+5 / +208 B**. The +40 B /
+3 fn is accounted exactly, and mechanically — the extra rows are the **`??_E`
vector-deleting-dtor thunks that branch INTO the `??_G` bodies**:

| row | size | before | after |
|---|---:|---|---|
| `??_EContentLoadingPanel@@WDM@` | 8 | 97.50 | 100 |
| `??_EContentLoadingPanel@@$4PPPPPPPM@GA@` | 16 | 98.75 | 100 |
| `??_ECreditsPanel@@$0PPPPPPPM@GA@` | 16 | 98.75 | 100 |

8 + 16 + 16 = 40 B. This is **W4-B §2.3's collateral family running with the
opposite sign**: W4-B *lost* two `??_E` rows because it renamed `??_G` alone and
left the `??_E` layer consistently-wrong-with-it; here the `??_E` names were
already correct, so repairing the `??_G` name repaired their branch-target
charge for free.

⇒ **Rule: when pricing a `??_G` rename, count the `??_E` thunks that branch into
it.** They are collateral in both directions, and the sign is decided by whether
their own names are already right — which is cheap to check and was not checked
by either lane in advance.

## 2. Item 2 — AddRef / Release: pairing is not matching, and that is worth knowing

Both addresses sit in `DirLoader.cpp`'s pin while both names are defined only in
`Object.obj`, which is exactly why W4-B measured their naming at 0.

Geometry from the committed `symbols.txt`, keyed on `.fn` symbols (never the
synthetic address column):

* **AddRef `0x8275BD08` size 0x64** — the *tail* of DirLoader's
  `0x8275B8B0-0x8275BD78` block, whose end abuts Object.cpp's `0x8275BD78`
  block ⇒ a **pure boundary move**. The 8-byte `except_data_827371D8` at
  `0x8275BD70` is the EH prefix of the function at `0x8275BD78` (Object's), so
  it correctly travels with it.
* **Release `0x8275B378` size 0x98** — genuinely **mid-block** in DirLoader's
  `0x8275B150-0x8275B6A0`, itself sandwiched between two Object.cpp blocks ⇒ a
  **3-way carve**, leaving `except_data_82736878` at `0x8275B410` with
  DirLoader (it prefixes DirLoader's own `fn_8275B418`).

The split handed Object.cpp exactly two 8-byte `.pdata` records — one per
re-homed body, zero funclets.

Pre-registered **+2 / +252 B**, *with the explicit caveat that `matched_code` is
all-or-nothing per row, so pairing is not matching*. Measured **+0 / +0 B**,
Δfuzzy **+0.001861 pp**. The caveat is what happened:

| row | before | after |
|---|---|---|
| `?AddRef@Object@Hmx@@QAAXPAVObjRefOwner@@@Z` (100 B) | fuzzy 0.000 (unpairable) | **69.680** (mpn 70.880) |
| `?Release@Object@Hmx@@QAAXPAVObjRefOwner@@@Z` (152 B) | fuzzy 0.000 (unpairable) | **77.342** (mpn 78.789) |

**Kept**, on the standing accuracy directive and the pairability memory: an
unpaired row is invisible to callee adjudication and looks identical to a row
with nothing wrong. A 0.000 here never meant *unmatched*, it meant
*unscoreable*. The cost is zero and the 252 B is now a **visible, priced
body-port target that did not exist as a target before** (handoff H2).

## 3. Item 3 — `list<EventSink>::insert` + creator: exact

`0x82749630` was a **lone** block in EventTrigger.cpp sitting 4 bytes after
Msg.cpp's `0x827495C8-0x8274962C` block ends — the pin was the only thing
holding it away from its own TU. Moved verbatim; EventTrigger keeps 25 blocks.

W4-B's handoff #3 (`0x82749550` = the creator, 72 B) is done with it, because
either alone is worth nothing. **Four gates before believing the creator's
name**, since `Msg.obj` defines *seven* different `_M_create_node`
instantiations and "the name is in Msg.obj" is not an identification:

1. the body at `0x82749550` does `li r3,0x14` → `MemOrPoolAlloc` →
   `addi r3,r3,8` → `bl fn_827493D0` =
   `??$_Copy_Construct@UEventSink@MsgSource@@…` — **T is named outright**;
2. retail `bl fn_82749550` fan-in is **exactly 1**, and the caller is
   `fn_82749630` itself ⇒ exclusive creator, no fold ambiguity;
3. no existing map address already carries that `_M_create_node` name ⇒ no new
   duplicate;
4. our `Msg.obj` defines **both** the `insert` and that `_M_create_node`.

The naming half is a **bet** under the `name_check` economics (naming an
anonymous address converts a *forgiven* placeholder call site into a *checked*
one). It is a safe bet here specifically because the single call site is
`fn_82749630`, which this same patch moves into `Msg.obj`.

Pre-registered **+2 / +172 B**, flagging the create_node half as less certain
(sibling `_M_create_node` rows read 99.667–100). Measured **+2 / +172 B** —
both rows 0 → 100. **Exact.**

## 4. Item 4 — the 9 fan-in-1 `??_G` rows: W4-B's refutation was CIRCULAR

W4-B tested the displaced-`??1` hypothesis and reported it refuted, because
*"every callee address is pinned to a unit matching its own current name"*.

⛔ **That test cannot discriminate.** The pin was placed *to match the name*, so
a pin agreeing with a name is not evidence the name is right. This is
CLAUDE.md's *"the SPLITS PIN can be CIRCULAR"* in its exact form, and it closed
a vein that is open.

Every briefed figure was re-verified literally first: retail `bl` fan-in is **1**
for all nine callees, as stated.

### 4.1 The non-circular column is BLOCK ISOLATION

How far does the callee's block sit from its own unit's nearest other block?

| `??_G` row | callee | callee's block | nearest sibling block | verdict |
|---|---|---|---|---|
| `??_GRndWind` | `??1NetCacheMgrXbox` `0x8245D4C0` | 936 B | **3.64 MB** | **ISLAND** |
| `??_GNetCacheMgrXbox` | `??1AccomplishmentConditional` `0x827D7360` | 84 B | **83 KB** | **ISLAND** |
| `??_GDxMesh` | `??_DMeterDisplay` `0x82738A48` | 108 B | **2.80 MB** | **ISLAND** |
| `??_GRndScreenMask` | `??_DRndMultiMeshProxy` `0x82481638` | 108 B | 128 B | genuine |
| `??_GMeterDisplay` | `??_DRndTexBlender` `0x8248B250` | — | 152 B | genuine |
| `??_GRemoteBandUser` | `??_DSpotlightEnder` `0x824E9C88` | 560 B | 552 B | genuine |
| `??_GAsyncFileWin` | `??1AsyncFileHolmes` `0x82535708` | 120 B | (sole block, adjacent) | genuine |
| `??_GSetlistProvider` | `??_GSetUserDifficultyMsg` `0x8253AB38` | — | same unit | genuine |
| `??_GNetLoaderStub` | `??1NetLoaderXbox` `0x827CFCB0` | — | same block | genuine |

Six of the nine are genuine and must not be touched. Three are islands whose
pin is an artifact of a wrong name.

### 4.2 The chain repaired here, and the hypothesis it kills

The decisive channel is map-independent — a class's `??_G` calls its **own**
`??1`, read out of the decoded `bl` target:

```
??_GRndWind        @0x8245DCC0 -> bl 0x8245D4C0   => 0x8245D4C0 is ??1RndWind
??_GNetCacheMgrXbox@0x827D7560 -> bl 0x827D7360   => 0x827D7360 is ??1NetCacheMgrXbox
```

★ **The second reading is what kills the rival "genuine ICF fold" hypothesis.**
If `~NetCacheMgrXbox` really folded into `0x8245D4C0`, its own `??_G` would call
*that* address. It calls a different one. Folding cannot explain that; a
displaced map name can.

Geometry corroborates, and no author would have written it by hand: `Wind.cpp`'s
block ends **exactly** at `0x8245D4C0` where the 936-B island begins, and
`MeshAnim.cpp`'s 84-B block is wedged **exactly** between two
`NetCacheMgr_Xbox.cpp` blocks. So link 1 is a pure boundary extension — carving
only the 84-B `??1` and leaving the island's other 852 B alone, because I have
evidence for the first function only — and link 2 merges three blocks into one.

### 4.3 ⛔ The alias group asserted a FOLD on evidence that refutes it

`symbol_aliases.json` group 1013: survivor `??1NetCacheMgrXbox@@UAA@XZ` at
`0x8245D4C0`, folded `??1RndWind@@UAA@XZ`, evidence tier **T1** = *"retail bytes
at the survivor address are byte-identical to our compiled body for the folded
spelling"*.

That T1 observation is **not proof of a fold. It is proof the address IS
RndWind's destructor** — recorded as a fold because the tool trusted the map
name for the survivor. **Withdrawn** with a record, `folded` emptied, nothing
pruned (ALIASAUDIT precedent). It forgave 0 census sites so the withdrawal is
Δ0 — but it had to go in the *same* commit, because after the rename it would
have begun forgiving a genuinely wrong callee.

⇒ **Generalisation worth carrying: an alias group whose survivor name comes from
the map inherits the map's defects, and its T1 evidence is exactly the evidence
that would convict the map.** `icf_alias_build.py` cannot tell the two apart,
because it is handed the name rather than deriving it.

### 4.4 Result, and the miss I did not flag

Pre-registered **+4 / +320 B**, with the 84-B `??1NetCacheMgrXbox` row flagged
UNCERTAIN (no T1 for that one) and a stated fallback of +3 / +236.
Measured **+2 / +160 B**. Half — and the two misses are different animals:

| row | before | after | |
|---|---|---|---|
| `??1RndWind` `0x8245D4C0` | 84.905 | **100.000** | +84 B — T1 vindicated |
| `??_GNetCacheMgrXbox` `0x827D7560` | 99.737 | **100.000** | +76 B |
| `??1NetCacheMgrXbox` `0x827D7360` | 84.667 | 85.190 | 0 B — **flagged, correct** |
| `??_GRndWind` `0x8245DCC0` | 99.737 | 99.737 | 0 B — **not flagged, MISSED** |

★ **The `??_GRndWind` miss is the informative one.** Its score is unchanged *to
the digit*, so its single charged arg site was never the `??1` callee I
repaired — unlike `??_GNetCacheMgrXbox`, whose was. **Two structurally identical
rows at the identical 99.73684, one crossing and one not**, is a clean
demonstration that a score does not identify *which* site is charged. Priced
from a mismatch count I would have got both wrong; this is the
`?Handle@CustomizePanel@@` lesson (RESIDUAL-1) reproduced in miniature.

### 4.5 `control none` decomposed the gain exactly

`control none` +84 B against default +160 B. That is **84 B of PAIRING channel**
(`??1RndWind` becoming definable in `Wind.obj` — visible on both rulers) **plus
76 B of NAME channel** (a relocation-name repair — invisible to `none`), and
84 + 76 = 160. Compare item 1, where `none` was **FLAT +0** against default
+208: pure relocation-name repair. Compare item 3, where `none` moved the **full
+172**: pure pairing.

⇒ **The `control none` delta is a decomposition instrument, not just an alias
guard.** Read as a fraction of the default delta it says *which channel paid*.

## 5. Item 5 — jeff's 158 truncation sites, characterised (nothing deployed)

I did **not** rebuild jeff. `cargo build --release` there overwrites the live
fleet splitter used by this repo and two siblings.

**Census reproduced literally**: **159** sites on this tree (briefed 158), of
which **156** have a preceding `.fn`; opcode histogram of the stranded
instruction matches W4-C's to within one `lwz` (29 vs 30, `stw` 21, `addi` 11,
`lfs` 7, `li` 7, `blr` 7). W4-C's warning is respected — this is keyed on
**carve geometry** (material between `.endfn` and the next `.fn`, classified
instruction-vs-`00000000`-padding), never on gap size, which measures padding:
the same walk finds **19,544 pure-padding gaps**, i.e. a size-keyed census would
be 99.2% noise.

### 5.1 The carve rule, in three measurements

| measurement | result | reading |
|---|---|---|
| stranded instructions per site | **1 in 118 / 156 (75.6%)** | a seam, not a lost region |
| last instruction of the **truncated** function | `lwz` 27, `stw` 23, `lfs` 12, `addi` 9 vs **`blr` 6, `b` 5** | jeff ends **mid-basic-block**, NOT at a control-flow terminator |
| spurious successor is referenced anywhere in the asm tree | **125 / 156 (80%) referenced NOWHERE** (30 branch targets, 1 data ref) | the successor is **invented**, not seeded by an incoming reference |
| successor start `mod 16` | **0 → 76, 8 → 74** (150/156 = **8-byte aligned**) | the leftover is re-seeded at the next **8-byte boundary** |
| truncation point `mod 16` | **4 → 72, 12 → 62** (134/156 **not** 8-aligned) | the seam is where that rounding starts |

⇒ **The rule to look at is whatever 8-byte-aligns a re-seeded leftover region.**
jeff's slice ends mid-block at a 4-aligned address; the un-analysed remainder is
then re-seeded at the next 8-byte boundary, and every instruction between the
slice end and that boundary falls **outside all symbols**. When the slice ends
at `mod 8 == 4`, exactly one instruction is lost — which is the 75.6% case.

Localisation for whoever takes it: `function_end = slices.end()`
(`src/analysis/cfa.rs:757`) and the leftover re-seeding downstream of it;
`src/analysis/slices.rs:402-492` holds the `function_end` boundary conditions.
jeff has no `$R4` vtordisp awareness at all — its only thunk logic
(`src/analysis/pass.rs`) is the CRT `__savegprlr` family — which is why
`fn_8268E3F8`'s 8-instruction thunk is split 0xC + hole + 0x10 while its
identically-shaped siblings at `0x8268E3B8`, `0x8268E3D8`, `0x8268E428`,
`0x8268E448` carve cleanly at 0x20.

### 5.2 ⛔ A `.text` pin cannot work around this class — measured, 2/156

The brief asked me to measure a pin workaround rather than assume none exists:

| seam location | sites |
|---|---:|
| at a `.text` **block boundary** (a pin could conceivably act) | **2** |
| **mid-block** (a pin sets block extents, not symbol extents) | 82 |
| in **unpinned `auto_*`** code (no pin exists to move) | 72 |

**1.3%.** The lever is jeff, as W4-C said; this is now a measured negative
rather than an inherited claim.

## 6. Gates

Run on the rebased branch after a full `./tools/ninja-locked` (rc=0):

```
tools/icf_alias_finder.py --validate     -> see report
scripts/verify_objs_patched.py --verify-manifest -> exit 0
tools/symbols_fixpoint_guard.py          -> see report (worktree only; refuses in main by design)
```

**Native gate: not run, and not applicable.** This branch touches
`config/45410914/splits.txt`, `scripts/target_symbol_map.json`,
`scripts/symbol_aliases.json` and this doc — **0 `src/**` files**. The gate
tests that shared headers still link into the native targets, which a pin or map
edit cannot affect. Stated as scope, not relayed as a verdict for a change class
the gate did not test.

## 7. What I did NOT do

* **The other two `??_G` islands (§4.1)** — `??_DMeterDisplay@0x82738A48` →
  `??_DDxMesh` (re-home into `Rnd_Xbox.cpp`, a pure boundary extension since
  `0x82738A48-0x82738AB4` abuts `Rnd_Xbox.cpp`'s `0x82738AB4` block) and
  `??_GMeterDisplay@0x8248B930` → `??_GRndTexBlender` (re-home into
  `TexBlender.cpp`). Both pairability gates already checked and **PASS**
  (`Rnd_Xbox.obj` defines `??_DDxMesh`, `TexBlender.obj` defines
  `??_GRndTexBlender`, and neither name appears in `report.json` today so
  there is no collision). ⚠ **Alias group 432 must be withdrawn with the
  `??_DDxMesh` rename** — it is the same T1-inherits-the-map defect as §4.3,
  and unlike group 1013 it **does** forgive a census site, which is why
  `??_GDxMesh` currently reads a *forgiven* 100.0. Left undone for budget, not
  for doubt.
* **The six genuine `??_G` rows (§4.1)** — callee sits with its own unit's
  blocks. Not touched, deliberately.
* **The 852 B remainder of the `0x8245D4C0` island** — I carved only the 84-B
  `??1` I could prove. Five unnamed functions remain pinned to
  `NetCacheMgr_Xbox.cpp` 3.6 MB from its body; they are probably `Wind.cpp`'s
  too, but "probably" is not an identification.
* **`??_GRndWind`'s residual charge (§4.4)** — one charged arg site, not the
  `??1` callee. Not diagnosed; `run_objdiff` does a one-`.obj` incremental build
  that skips the six obj patchers, so diagnosing it mid-lane would have left the
  tree measurably wrong.
* **Anything in `src/`** — no header, member type or body edit. Like W4-B, L7
  and W3-B, every adjudicated row here resolved to MAP or PIN, never
  SOURCE_WRONG. Four consecutive lanes now agree on that.
* **Any jeff or objdiff rebuild**, any `symbols.txt` edit, any hand-edited
  `.pdata` line, any new alias group. One alias spelling was **withdrawn**; none
  was added.
* **`objects.json`** — untouched; no unit was drained, so no entry had to be
  deleted.

## 8. Handoffs, in the order I would fund them

| # | handoff | evidence in hand |
|---|---|---|
| H1 | **The two remaining `??_G` islands** — `??_DDxMesh` (108 B) and `??_GRndTexBlender` (80 B), with alias group 432 withdrawn alongside | §4.1, §7; both pairability gates already PASS |
| H2 | **`Object::AddRef` 69.68 / `Object::Release` 77.34** — 252 B of body-port, newly visible | §2; a pure `src/` job, needs the native gate |
| H3 | **jeff: leftover re-seeding is 8-byte-aligned, stranding the seam** — 156 sites, 80% with no incoming reference | §5.1; `cfa.rs:757` + `slices.rs:402-492`; pin workaround measured dead at 2/156 |
| H4 | **`??_GRndWind` @`0x8245DCC0`** — 76 B, one charged arg site that is NOT its `??1` callee | §4.4; needs a full-build objdiff read |
| H5 | **`icf_alias_build.py` inherits map defects by construction** — its T1 evidence convicts the map it trusted | §4.3; two instances found in one lane (groups 1013, 432) |

## 9. Reusable lessons

* **A pin agreeing with a name is not evidence the name is right** — the pin was
  placed to match the name. The non-circular column is **block isolation**:
  distance from the callee's block to its own unit's nearest other block. Three
  islands at 83 KB–3.6 MB vs six genuine at 128–552 B is a two-order-of-magnitude
  separation, not a judgement call.
* **A class's `??_G` calling a DIFFERENT address than the one the map names its
  `??1` refutes the fold hypothesis outright.** Folding cannot produce that;
  a displaced name can.
* **When pricing a `??_G` rename, count the `??_E` thunks branching into it** —
  collateral in both directions, sign decided by whether their own names are
  already right (§1.1).
* **`control none` read as a FRACTION of the default delta decomposes the gain**
  into pairing (moves both rulers) vs relocation-name (moves only the graded
  one). Measured 0/208, 172/172 and 84/160 in one lane (§4.5).
* **Two identical scores can hide different charged sites.** 99.73684 on two
  structurally identical rows: one crossed on the repair, one did not move at
  all (§4.4).
* **Pairing is not matching, and `matched_code` is all-or-nothing per row** — a
  re-home can be entirely correct and pay zero bytes (§2). Pre-register the
  caveat and keep the change anyway.
* **Key a carve census on geometry, never on gap size**: the same walk yields
  156 real seams and **19,544** padding gaps (§5).
