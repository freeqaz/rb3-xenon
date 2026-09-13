# Lane W8-B — the rows that are one or two charges from crossing

**Branch** `w8-near-crossings` · worktree `~/tmp/wt-w8-b` · base `2fc2552a`
(`git merge-base --is-ancestor 2fc2552a HEAD` asserted before the first edit).

Anti-vacuity first (FOLDPROVE-2): the worktree was **fully built** before any
name-keyed work — `./tools/ninja-locked`, `EXIT=0` — and
`scripts/verify_objs_patched.py --verify-manifest` returned **exit 0** over
**1,205 decomp + 3,085 target objects** (`tree_sha256=f2a641a7bb0e8929`).  Every
retail mangled name quoted below was read *after* that build, so the
reflinked-objs-are-pre-renamer trap cannot be hiding behind a negative.

The worktree's `report.json` is **identical to main's** on every headline key
(`matched_functions` 42,738 · `matched_code` 3,865,216 · `matched_code_percent`
37.724308 · `fuzzy_match_percent` 49.108723 · `total_functions` 69,219 ·
`total_code` 10,245,956), and `provenance.diff_config` carries
`functionRelocDiffs=name_check` + `ppc.calculatePoolRelocations=false` — the
graded ruler.  All percentages below are graded and **unrounded, read from
`report.json`**, never from `run_objdiff`'s display.

---

## 0. Headline

* **The census did not exist and now does**: 5,499 paired rows at
  `99.5 <= fuzzy < 100`, **734,308 B** — 7.17% of `total_code`, ~19% of
  everything currently matched.
* ★★ **The briefed `5/N` screen is EXACT, and it is exact per charged
  ARGUMENT, not per charge — and equally for `register` args as for
  relocation-NAME args.**  Verified on **33/33** single-kind rows with **zero
  deviation** (max |error| 0.002 charges).  A full mismatch
  (`insert`/`delete`/`replace`) costs **20×** that — measured exactly, 20.000.
* ⛔ **The biggest prize in the tree is not source work.**
  `?CountOrCreateExpandedDetails@NextSongPanel@@` is **12,220 B behind ONE
  instruction**, and that instruction is a *commuted commutative add*
  (`add r3,r11,r28` vs `add r3,r28,r11`).  Permuter-class, deferred by directive.
* ⛔ **W7-B's four named rows are NOT the instruction-level class they read
  as.**  All four reproduce to five decimals at this base, and all four are
  dominated by **relocation-NAME (fold) charges**, so they belong to W7-D's
  alias vein, not to a source-porting lane.
* ★★★ `?Handle@PlatformMgr@@` (3,112 B, 7 charges) is a **PROVEN NON-FOLD** by
  W7-D's own rule, and the follow-through **settles it: our source is RIGHT and
  `target_symbol_map.json` is WRONG**, on two of four rows proven from retail
  bytes with a map-independent anchor (an XAM entrypoint constant).  Not edited
  — the repair is a coupled 4-row map change and only 2 rows are proven.
* ⛔ `?Handle@CustomizePanel@@` (5,036 B, 1 charge) is **documented drained by
  four prior lanes in the source file itself**.  Not reopened.

---

## 1. Re-deriving W7-B's four (its figures were at its own base, four TUs ago)

W7-B said explicitly that its population figures were measured at its base
commit and that four TUs had changed since, so the first thing this lane did was
re-read all four from `report.json` at `2fc2552a`.

| row | unit | size | W7-B | **re-derived (graded, unrounded)** | mpn |
|---|---|---:|---|---|---|
| `?Load@OutfitConfig@@UAAXAAVBinStream@@@Z` | OutfitConfig | 1,120 | 99.88 | **99.87500** | 99.87500 |
| `?SetupScore@BandScoreboard@@QAAXXZ` | BandScoreboard | 556 | 99.89 | **99.89209** | 99.89209 |
| `??0RandomGroupSeqInst@@QAA@PAVRandomGroupSeq@@@Z` | Sequence | 444 | 99.91 | **99.90991** | 99.90991 |
| `??1StreakMeter@@UAA@XZ` | StreakMeter | 404 | 99.75 | **99.75247** | 99.75247 |

★ **All four reproduce exactly.**  The four changed TUs did not touch them, so
W7-B's handoff was sound — the caution was right to issue and wrong to worry
about.  (`?Load@RndMesh@@` also reproduces: 3,452 B, 88.19119.)

### 1.1 ⛔ But the handoff's implied CLASS is wrong, and that changes who should work them

`mpn == fuzzy` on all four looked like "the penalties are instruction-level, so
source can fix them".  **That inference is false**, and the reason is a property
of `mpn` this tree had not written down precisely:

> **`mpn` excludes `register` argument diffs but CHARGES `symbol`
> (relocation-name) argument diffs.**

Measured, not assumed — the whole top-45 agrees and two rows settle it alone:
`?CountOrCreateExpandedDetails@NextSongPanel@@` has `{register: 2}` and
`mpn == 100.000` while `fuzzy == 99.99673`; `?Handle@OvershellSlot@@` has
`{symbol: 2}` and `mpn == fuzzy == 99.99569`.  CLAUDE.md's "`mpn` excludes
arg-only penalties" is therefore true only of the *register* half.

⇒ `mpn == fuzzy` does **not** mean "no arg charges".  Dumping the charged-site
list for the four rows shows what they actually are:

| row | charged sites | kinds |
|---|---:|---|
| `?Load@OutfitConfig@@` | 7 | **7 × symbol** — `ObjVector<TransformArrow>::resize` vs `ObjVector<MatSwap>::resize`, `operator>>(ObjOwnerPtr<CharClip>)` vs `operator>>(ObjPtr<RndDir>)`, `vector<TransformCrowd>::~vector` vs `vector<OldColorOption>::~vector`, … |
| `?SetupScore@BandScoreboard@@` | 3 | **3 × symbol** — `vector<ObjOwnerPtr<…>>::_M_erase` vs `vector<ObjPtr<RndMesh>>::_M_erase`; `~ObjRefConcrete<RndMesh,ObjectDir>` vs `~ObjPtr<RndMesh>` |
| `??0RandomGroupSeqInst@@` | — | symbol-dominated (same family) |
| `??1StreakMeter@@` | — | symbol-dominated (same family) |

These are template-instantiation pairs: **the ICF fold / wrong-callee stratum**,
addressable by an alias install with retail-byte proof, **not** by porting
source.  That is W7-D's vein and its rule governs them.

⚠ Note the forgiveness rule working correctly in the same listings: at
`?SetupScore@BandScoreboard@@` row [100] retail's callee is `fn_8227F138` — a
**placeholder** name — and the row reads `equal` even though we spell
`??0?$ObjPtr@VRndMesh@@@@QAA@PAVObject@Hmx@@@Z` there.  An unnamed retail callee
costs nothing; only a *named* one can charge.

---

## 2. The whole-binary census (this is the thing nobody had enumerated)

**Screen**: every row in `build/45410914/report.json` with
`99.5 <= fuzzy_match_percent < 100.0`.  Unpaired rows score `fuzzy == 0` and are
outside the screen by construction.  Every numeric is `int()`/`float()`-coerced
(report.json ships numbers as JSON strings, and protobuf-JSON omits defaults).
The enumeration is committed as a tool, not a dump — **`tools/near_crossing_census.py`**
(`--classify N` reads charged sites for the N largest; it refuses if
`report.json` does not declare its ruler, and it never passes `--build`).

| stratum | rows | bytes | % of `total_code` |
|---|---:|---:|---:|
| `mpn == 100` (register/branch-dest only) | 3,287 | 200,480 | 1.96% |
| `mpn < 100` (contains a name charge or a real instruction diff) | 2,212 | 533,828 | 5.21% |
| **total `fuzzy >= 99.5`** | **5,499** | **734,308** | **7.17%** |

Read the split with §1.1's correction in hand:

* the `mpn == 100` stratum is **permuter-class by construction** — its only
  charges are register-argument diffs, which no source spelling addresses.
  **200,480 B is not a source-work backlog.**
* the `mpn < 100` stratum is a **mixture**, and the top of it is overwhelmingly
  relocation-NAME charges (folds and wrong callees), not insert/delete.

Of the **45 largest rows** (charge kinds extracted from
`objdiff-cli diff --include-instructions` on the graded ruler, no `--build`, so
the patched tree was never disturbed):

| dominant kind | rows in top 45 |
|---|---:|
| `symbol` only | 21 |
| `register` only | 8 |
| mixed `symbol`+`register` | 9 |
| contains `insert`/`delete`/`replace` | 6 |
| contains `immediate` | 4 |

⇒ **the near-crossing frontier is a NAMING/FOLD frontier, not a porting one.**

### 2.1 The same split over the **200** largest rows (261,740 B), classified exactly

Charge kinds were then extracted for the 200 largest rows in the band — 36% of
the band's bytes — and the result is sharper than the top-45 sample suggested:

| class | rows | bytes | share of the 200 |
|---|---:|---:|---:|
| **`symbol`-only** (relocation-NAME / fold) | **138** | **163,812** | **62.6%** |
| `mixed` | 31 | 48,700 | 18.6% |
| `register`-only (permuter-class) | 29 | 43,604 | 16.7% |
| **`insert`/`delete`/`replace`-only** (source-fixable) | **2** | **5,624** | **2.1%** |

Charge totals across those 200 rows: `symbol` 479 · `register` 370 ·
`immediate` 142 · `insert` 5 · `replace` 5 · `branch_dest` 4 · `delete` 2.

⛔⛔ **There are exactly TWO pure-instruction rows in the 200 largest, and one of
them is the drained `CustomizePanel` (§4.2).**  The other is
`?RenderScene@NgSpotlightDrawer@@` (588 B, 1 × `replace`, §4.5).  So the entire
*source-porting* surface at the top of this band is **5,624 B, of which 5,036 B
is already closed as unreachable** — i.e. **≈588 B is live**.

⇒ **This band should not be briefed to a body-porting lane.**  It is an alias /
identification lane's worklist with a permuter tail.  That is the single most
actionable thing this census says, and it is exactly the shape CLAUDE.md's
"THE GAP IS MOSTLY NOT SOURCE WORK" finding predicts, now measured on the
near-crossing band specifically rather than the whole gap.

---

## 3. ★★ The pricing screen, calibrated — and it is exact

The brief carried "one relocation-name charge costs exactly `5/N` pp of fuzzy,
`N = size/4`".  Tested literally against 33 single-kind rows:

```
charges_implied = (100 - fuzzy) / (5/N),   N = target_size / 4
```

| kind | measured price | rows | max deviation |
|---|---|---:|---|
| `diff_arg` argument, `symbol` | **1.000 × (5/N) per charged ARGUMENT** | 25 | 0.002 |
| `diff_arg` argument, `register` | **1.000 × (5/N) per charged ARGUMENT** | 8 | 0.001 |
| `delete` (full mismatch) | **20.000 × (5/N)** = `100/N` | 1 | 0.000 |

Three refinements to the briefed rule, all measured:

1. ★ **The unit is the charged ARGUMENT, not the charged instruction.**
   `NextSongPanel` implies 2.00 charges and has **one** `diff_arg` row carrying
   **two** differing register arguments.  A one-row/two-arg site prices as two.
2. ★ **The screen is NOT specific to name charges.**  A `register` argument diff
   costs exactly the same `5/N`.  So the screen tells you the **argument count**
   and says nothing about kind — you must read the kind from the charged-site
   list, exactly as the brief warned, but for a different reason than stated.
3. ★ **A full mismatch is 20× an argument diff** (`100/N` vs `5/N`).  So a row
   implying "20 charges" may be **one** deleted instruction — which is precisely
   `?Handle@CustomizePanel@@`: 5,036 B, implied 20.00, actual **1 × delete**.
   Reading that as twenty name charges would badly misprice the row.

⛔ **Do NOT apply the model to rows containing `immediate` args.**  Fitting
`units = n_reg + n_sym + 20·n_insdel + x·n_imm` over the four mixed rows gives a
**negative** `x` on all four (−0.06 … −1.30), i.e. the model is wrong there, not
merely imprecise.  Two known causes: the denominator stops being `target_size/4`
once inserts exist (`?OnFileLoaded@BandDirector@@` has 956 diff rows against
954 target instructions), and W7-C's separate finding that an immediate keeps
most of its credit.  **The honest scope of the screen is: rows whose charges are
all `diff_arg`.**  On those it is exact.

---

## 4. Per-row work

### 4.1 ⛔ `?CountOrCreateExpandedDetails@NextSongPanel@@` — 12,220 B, ONE instruction, permuter-class

**The single largest size-if-it-crosses bet in the binary**, and larger than
every row on W7-B's list combined.  `fuzzy 99.99673 / mpn 100.0`;
**3,054 of 3,055 rows equal**; the entire deficit is one row:

```
[1284]  T 0x2d1c   add  r3, r11, r28
        B          add  r3, r28, r11
```

`r11` is `ptr->mNodes` (`lwz r11,0(r27)` / `lwz r11,0(r11)`), `r28` is a
strength-reduced byte induction variable (`slwi r28,r29,3` hoisted before the
loop, `addi r28,r28,8` each turn).  The source is
`ptr.Node(count++) = DataArrayPtr(label, cur);` inside the
`InqGoalsAcquiredForSong` loop (`NextSongPanel.cpp:547-554`), reaching
`DataArray::Node(int) { return mNodes[i]; }` (`obj/Data.h:506`).

**Both sides compute the same value in the same registers; only the operand
order of a commutative `add` differs**, and the two source-visible inputs are
materialised by identical, `equal` instructions immediately before it.  This is
an operand-order tie-break in the scheduler, i.e. exactly the permuter's domain,
which is deferred by standing directive.  **Not touched.**

⚠ Worth flagging for whoever un-defers the permuter: `mpn` is already 100 here,
so crossing this row is worth **+12,220 B and +0 functions** — it is invisible
to any `matched_functions`-driven ranking.

⛔ **Do not "fix" it by editing `DataArray::Node`.**  `obj/Data.h` is included
tree-wide; the one candidate spelling (`return i[mNodes];`, which flips the AST
operand order) would cascade to every `Node()` call in the binary to buy one
instruction in one function.  The blast radius is the wrong shape for the prize.

### 4.2 ⛔ `?Handle@CustomizePanel@@` — 5,036 B, ONE `delete`, DRAINED

`fuzzy 99.92057`, one charged site: retail has `clrlwi r11,r11,24` at [530] and
we do not — a bool zero-extension after the `subic`/`subfe` materialisation of
`HasLicense(...) != 0`.

**`src/band3/meta_band/CustomizePanel.cpp` carries ~250 lines of in-file record
from four prior lanes (DQ-1, RESIDUAL-1, RESIDUAL-2, W11b-CUSTOMIZE) closing
this exact instruction.**  Measured INERT there: arm `!= 0`, `? true : false`,
`!!`, `(unsigned char)`, an extra `bool`-parameter boundary, `bool HasLicense` +
arm `!= 0`, `bool HasLicense { return X != 0; }` + bare arm,
`bool HasLicense { int r = X; return r; }`, and a `DataNode(bool)` overload.
Measured WORSE: if/else bodies (+5 insert), an int→bool helper (+4 insert),
`return (int)TheSongMgr.HasLicense(s)` (+3 insert / +5 delete).  W11b also
**refuted** RESIDUAL-2's own "mask only at a phi" rule.

⇒ **Not reopened.**  Reading the in-tree record cost two tool calls and saved a
whole leg; this is CLAUDE.md's "READ THE IN-TREE RECORD FIRST" paying out
literally.

### 4.3 ★ `?Handle@PlatformMgr@@` — 3,112 B, 7 charges, PROVEN NON-FOLD

`fuzzy 99.95501`, 7 × `symbol`, and the pairs are startling because they look
like a **permutation of one callee set**:

| site | retail names | we call |
|---|---|---|
| [417] `0x10c4` | `?InviteUserParty@PlatformMgr@@` | `?ShowUserFriendsUI@PlatformMgr@@` |
| [448] `0x1140` | `?ShowOfferUI@PlatformMgr@@` | `?ShowUserPartyUI@PlatformMgr@@` |
| [514] `0x1248` | `?ShowUserFriendsUI@PlatformMgr@@` | `?InviteUserParty@PlatformMgr@@` |
| [544] `0x12c0` | `?IsUserAGuest@PlatformMgr@@` | `?ShowOfferUI@PlatformMgr@@` |
| [600] `0x13a0` | `??1?$ObjPtr@VMoggClip@@@@` | `?EnableXMP@PlatformMgr@@` |
| [643] `0x144c` | `??$?0H@?$StlNodeAlloc@V?$_List_node@H@…` | `?CheckMailbox@PlatformMgr@@` |
| [661] `0x1494` | same `StlNodeAlloc` ctor | `?RunNetStartUtility@PlatformMgr@@` |

**Applying W7-D's rule to the first four — and it settles them.**  The map places
them at four *distinct, consecutive* addresses `0x82514CB8 / D00 / D48 / D90`,
exactly `0x48` apart, and reading the retail bodies (`build/45410914/asm/PlatformMgr.s`,
keyed on `.fn fn_<ADDR>`, never the synthetic address column) shows all four are
**byte-identical 17-instruction wrappers differing in exactly one field — the
final `bl`**:

```
mflr r12 / stw r12,-8(r1) / std r31,-0x10(r1) / stwu r1,-0x60(r1)
lwz r11,0(r4) / mr r31,r3 / mr r3,r4 / lwz r11,0(r11) / mtctr r11 / bctrl   ; u->GetPadNum()
mr r4,r3 / mr r3,r31
bl <fn_8251BFA8 | fn_8251CA08 | fn_8251BFE8 | fn_8251C118>                  ; THE ONLY DIFFERENCE
addi r1,r1,0x60 / lwz r12,-8(r1) / mtlr r12 / ld r31,-0x10(r1) / blr
```

That is W7-D's rule in its cleanest possible form: **the relocation target varies
per member, so the family CANNOT fold — and retail's own layout confirms it, by
keeping four identical-but-for-one-`bl` bodies at four separate addresses instead
of one.**  These charges are therefore **NOT forgiveable noise**; something is
genuinely wrong, and it is either our dispatch order or the map.

The last three sites ([600]/[643]/[661]) are the opposite case and almost
certainly real folds: they are `void f(this)` slots where we call `EnableXMP`,
`CheckMailbox`, `RunNetStartUtility` — and `PlatformMgr_Xbox.cpp:195-196` defines
`CheckMailbox` and `RunNetStartUtility` as **literally empty** (`{}`), which folds
with every other trivial body in the image and leaves an arbitrary survivor name
(here an `ObjPtr<MoggClip>` dtor and an `StlNodeAlloc` ctor).

#### ★★★ SETTLED: OUR SOURCE IS RIGHT AND THE MAP IS WRONG — proven on retail bytes, map-independently

The four wrappers are distinguished only by their inner `bl`, so identifying the
*inner* callees identifies the wrappers.  Reading them in full
(`build/45410914/asm/MoggClipMap.s`) gives two **airtight, map-independent**
identifications — each anchored on an argument *constant*, which no map row can
influence:

**`fn_8251CA08` is `PlatformMgr::ShowOfferUI(int)`.**
```
bl fn_82514988          ; IsSignedIn(padNum)   [map-named, corroborating only]
clrlwi. r11, r3, 24 / beq
li r6, -0x1 / li r5, 0x0 / li r4, 0x4 / mr r3, r31 / bl fn_8283D728
```
That is `XShowMarketplaceUI(padNum, 4, 0, -1)`, and
`XSHOWMARKETPLACEUI_ENTRYPOINT_CONTENTLIST_BACKGROUND == 0x0004`
(`src/xdk/xapilibi/xbase.h:211`) — the literal 4/0/−1 triple our
`ShowOfferUI(int)` passes at `PlatformMgr_Xbox.cpp:237-239`.  No other
`PlatformMgr` method has that signature.

**`fn_8251BFE8` is `PlatformMgr::ShowPartyUI(int)`.**
```
li r30, 0x1             ; unsigned long ret = 1;
bl fn_82514988 / clrlwi. / beq
mr r3, r31 / bl fn_82B54168 / subic r11,r3,1 / subfe r30,r11,r3
.L: clrlwi / cntlzw / extrwi r3, r11, 1, 26      ; return ret == 0;
```
The `ret = 1` default plus a `return ret == 0` tail is unique to
`ShowPartyUI` (`PlatformMgr_Xbox.cpp:162-175`); every sibling returns `void`.

Propagating up through the wrappers:

| retail addr | **true identity (proven)** | `target_symbol_map.json` says | Handle slot | **we call** |
|---|---|---|---|---|
| `0x82514D00` | **`ShowOfferUI(const LocalUser*)`** | `?IsUserAGuest@PlatformMgr@@` ⛔ | [544] | `ShowOfferUI` ✅ |
| `0x82514D48` | **`ShowUserPartyUI(const LocalUser*)`** | `?ShowOfferUI@PlatformMgr@@` ⛔ | [448] | `ShowUserPartyUI` ✅ |

⇒ **On both settled slots our dispatch is CORRECT and the map row is WRONG.**
By elimination the remaining two (`0x82514CB8` ↔ [417], `0x82514D90` ↔ [514])
are `ShowUserFriendsUI` and `InviteUserParty` — exactly what we call there — so
all four charges are almost certainly map defects, and the map's four names sit
on these bodies in a **permutation**.  Note the corroborating detail that
`?ShowUserPartyUI@PlatformMgr@@QAA_NPBVLocalUser@@@Z` has **no map entry at
all**: the namer had five identically-shaped wrappers and four names to place.

⚠ **This is only visible because the callee name is charged.**  The dispatch
comparison at each slot loads a `Symbol` through a **`lbl_`** data relocation on
retail's side, and `name_check` *forgives* placeholder targets — so a genuinely
crossed symbol→handler mapping would be **invisible** here.  The name charge is
the only channel through which this class of defect can surface at all, which is
CLAUDE.md's "naming pays in BUG EXPOSURE, not bytes" running in reverse.

**Still not edited, and here is the reason it would be wrong to.**  The repair is
four coupled `target_symbol_map.json` rows and only **two are proven**.  Renaming
`0x82514D00` to `ShowOfferUI` while `0x82514D48` still carries that name puts one
spelling at two addresses — precisely the state
`comdat_fold_gate.py` refuses on, and it would break the map's bijection.  A
partial fix is not a smaller fix here, it is a different and worse one.  Complete
the cycle first by identifying `fn_8251BFA8` and `fn_8251C118` (their XAM stubs
are `lbl_82E11E98 + 0x4` / `+ 0x10`), then land all four together with a forced
re-split A/B.  **Handoff, §6.**

⚠ **Side-finding, unrelated to the charge and worth a separate lane:** retail's
`ShowOfferUI` / `ShowPartyUI` bodies have **no `sXShowCallback` / NUI branch at
all** — `fn_8251CA08` calls `XShowMarketplaceUI` unconditionally after the
sign-in check.  Our source carries an `if (sXShowCallback(ul)) XShowNui…UI(…)`
arm in all three (`PlatformMgr_Xbox.cpp:142-175, 227-240`).  RB3 predates Kinect;
that arm looks like inherited DC3 code, and those three functions cannot match
while it is present.

### 4.4 The fold stratum at the top of the census

The remaining large rows are single- or double-`symbol` charges whose pairs have
the shape CLAUDE.md records as "what a fold looks like":

| row | size | charges | retail names vs ours |
|---|---:|---:|---|
| `?Handle@OvershellSlot@@` | 9,276 | 2 | `clear@_List_base<PassiveMessage*>` / `RemoveUser@OvershellSlot`; `StlNodeAlloc<_List_node<int>>::ctor` / `ToggleMuteStatus@SessionUsersProvider` |
| `?Handle@Rnd@@` | 6,416 | 1 | `StlNodeAlloc<_List_node<int>>::ctor` / `TakeShot@HiResScreen` |
| `?Handle@OvershellPanel@@` | 5,768 | 2 | `OnMsg(ConnectionStatusChangedMsg)` / `OnMsg(ServerStatusChangeMsg)`, `OnMsg(NetStartUtilityFinishedMsg)` |
| `?Handle@Game@@` | 5,428 | 1 | `TypeDef@Object@Hmx` / `TotalBasePoints@SongDB` |
| `?Handle@Tour@@` | 3,492 | 2 | `??0Tour@@` / `InitializeTour@Tour@@`; `GetConclusionText@Tour@@` / `GetTourGigGuideMap@Tour@@` |
| `?GenerateMacros@ShaderOptions@@` | 3,084 | 70 | one pair, 69×: `vector<pair<VocalPhrase*,VocalPart*>>::push_back` / `vector<ShaderMacro>::push_back` |

**Not installed.**  Every one needs retail-byte proof through
`tools/comdat_fold_gate.py`, and that gate consumes a triage worklist built by
`scripts/wrong_callee_triage.py`, which requires a **second `report.json` built
at `functionRelocDiffs=none`** — a build-config change this lane deliberately did
not make.  The standing hazard is decisive here: **an unproven alias lifts the
score BY CONSTRUCTION and the `none` control CANNOT catch a fabricated one**, so
guessing is strictly worse than leaving the bytes on the table.  Handoff, §6.

### 4.5 ⚠ `?RenderScene@NgSpotlightDrawer@@` — 588 B, the ONE live source-class row, and it resists

`fuzzy 99.59184`, 146 of 147 rows equal, one `replace`:

```
lwz   r11, sLights           ; begin
lwz   r10, 0x4(r8)           ; end
subf  r11, r11, r10          ; byte span
T:  srawi.  r11, r11, 3      ; -> ELEMENT COUNT (signed /8), test
B:  clrrwi. r11, r11, 3      ; -> span & ~7, test
beq   <skip>
```

Both are correct tests of "is `sLights` empty" (the span is always a multiple of
`sizeof(SpotlightEntry) == 8`); they differ in whether MSVC **materialised the
quotient**.  Our source already asks for it —
`int numLights = sLights.end() - sLights.begin();` then `if (numLights != 0 && …)`
(`src/system/world/SpotlightDrawer_NG.cpp:572`) — and MSVC folds the
`!= 0` test of a `>>3` into the cheaper mask because the quotient is dead
afterwards (r11 is clobbered four instructions later on **both** sides).

★ **Oracles were harvested rather than guessed.**  A binary-wide scan of every
target `.s`, keyed on the `.fn fn_<ADDR>` symbol (never the synthetic address
column), for `srawi. rX,rX,3` immediately followed by `beq`/`bne` finds **32
sites, 18 of them inside functions we already match at `fuzzy == 100`** —
`TrackPanel::SetSuppressTambourineDisplay/SetSuppressPlayerFeedback/AssignAndInitTracks/Exit`,
`Tracker::SetupDisplays/HandleAddPlayer`, `CharClip::Save/ListBones/Copy`,
`CharBones::ListBones`, `CharBonesSamples::LoadHeader`, `BandSongMgr::RankTier`,
`BandDirector::HarvestDircuts`, `StringTable::~StringTable`,
`Player::LocalSetEnabledState`, `MultiChannelMapping::FillChannelList`,
`CharClipDriver::SetBeatOffset`, `TrackPanel::GetTrackSlot`.

⛔ **But every oracle is the WRONG shape.**  Read three of them
(`TrackPanel.cpp:825`, `:839`, `Tracker`): each is
`for (int i = 0; i < mTrackSlots.size(); i++)`.  The quotient survives there
because it is compared against a **variable** (`i`), which MSVC cannot fold into
a mask.  Retail's `RenderScene` guard is a comparison against the **constant 0**
(the branch is `beq`, so the test is `== 0` — a `> 0` test would have emitted
`ble`), and no oracle in the tree exhibits a constant-0 test that keeps the
shift.  ⇒ **the 18 oracles do not license a fix here; they refute the obvious
one.**  Recorded as a handoff with the oracle list, **not** edited.

⚠ This is the same class as §4.1: a value that is dead after its own test, where
MSVC's choice between two equivalent encodings is a scheduling/materialisation
tie-break rather than a source-visible property.

---

## 5. Negatives, and what this lane did NOT do

* ⛔ **No source change was made, and therefore no A/B was run.**  Stating that
  plainly rather than manufacturing a measurement: every row this lane opened
  resolved to one of three closed categories — permuter-class (§4.1),
  documented-drained (§4.2), or needs-an-alias-gate-this-lane-did-not-run (§4.4)
  — and the one row with a genuine open defect (§4.3) needs a **map** repair
  whose dominant term is un-pairing.  A speculative edit would have been priced,
  not measured.
* ⛔ **Did not touch `obj/Data.h`** to chase §4.1's commuted `add` (blast radius
  ~281 PCH TUs plus every other `Node()` caller, for one instruction).
* ⛔ **Did not reopen `CustomizePanel`** — see §4.2's four-lane record.
* ⛔ **Did not install any ICF alias**, prove any fold, or edit
  `scripts/symbol_aliases.json` / `scripts/icf_alias_groups.json`.
* ⛔ **Did not edit `scripts/target_symbol_map.json`**, `splits.txt`,
  `objects.json`, `symbols.txt`, or any build config.
* ⛔ **Did not run `run_objdiff` / `run_diff_inspect` (MCP) or
  `objdiff-cli --build`** anywhere.  Every diff in this document came from
  `bin/objdiff-cli diff -p . -u <unit> <symbol>` **without** `--build`, precisely
  so the six obj patchers were never bypassed; the tree is still the verified
  fixed point it was built as.
* ⛔ **Did not rebuild jeff, objdiff, wibo or objcache.**
* ⚠ **Did not classify all 5,499 rows** — charge kinds were extracted for the
  top 200 by size.  The tail is enumerated in the census but unclassified.
* ⚠ **Did not settle the `immediate` argument price** (§3) — the two-price model
  fails on those rows and this lane reports the failure rather than a fitted
  constant.

---

## 6. Handoffs, in priority order

1. ★★★ **`?Handle@PlatformMgr@@`, 3,112 B — a proven MAP defect, half-repaired
   and ready to finish.**  §4.3 settles `0x82514D00` = `ShowOfferUI` and
   `0x82514D48` = `ShowUserPartyUI` on retail bytes.  Remaining work: identify
   `fn_8251BFA8` and `fn_8251C118` (XAM stubs `lbl_82E11E98 + 0x4` / `+ 0x10`)
   to close the 4-cycle, then land all four `target_symbol_map.json` rows in one
   edit with a forced re-split A/B.  **Do not land a partial rename** — two of
   the four names would collide at two addresses.
1b. ⚠ **`PlatformMgr_Xbox.cpp`'s NUI arms** (§4.3 side-finding) — retail has no
   `sXShowCallback` branch in `ShowOfferUI`/`ShowPartyUI`/`ShowFriendsUI`.
   Separate, and it blocks those three functions independently of the map.
2. ★★ **The fold stratum, §4.4** — ~38 kB across six rows, all needing
   `comdat_fold_gate.py`.  The prerequisite is a `none`-ruler `report.json` so
   `wrong_callee_triage.py` can regenerate its worklist (the committed one is
   from 2026-08-12).  That prerequisite, not the proving, is the work.
3. ★ **`NextSongPanel`, 12,220 B** — the largest single permuter target in the
   binary, one commuted `add`, `mpn` already 100.  Park it against the permuter
   being un-deferred; do not spend source effort on it before then.
4. ⚠ **Re-derive, never inherit.**  This lane's census is measured at
   `2fc2552a`.  §1 shows W7-B's four rows survived four TU changes unchanged —
   but that was *checked*, not assumed, and it is the check that is transferable.

---

## 7. Reusable lessons

* ★★ **A screen that is exact can still be exactly the wrong instrument.**  The
  `5/N` rule reproduced to three decimals on 33 rows — and it still cannot tell
  a fold from a bug from a register swap, because `register` and `symbol`
  arguments price identically.  **Precision is not diagnosticity.**  Always read
  the kind from the charged-site list; the number only tells you *how many*.
* ★★ **`mpn == fuzzy` is not evidence of an instruction-level defect.**  `mpn`
  drops register args and keeps name args, so the two rulers agree exactly when
  the charges are names — which is the *most* fold-like case, not the least.
  This is what made W7-B's four rows read as porting work.
* ★ **W7-D's rule has a positive form, and retail's layout states it for you.**
  Four bodies identical but for one `bl`, sitting at four distinct addresses
  `0x48` apart, *is* the linker announcing that a per-member relocation blocked
  the fold.  You do not need the gate to rule a fold OUT — only to rule one IN.
* ★★★ **A scanner returned a clean, decisive `0 sites` that was pure vacuity —
  and it agreed with my prior.**  The oracle scan in §4.5 first reported *zero*
  `srawi.`-plus-branch sites binary-wide, which was entirely plausible ("that
  idiom must be rare") and would have closed the row as unreproducible.  The bug
  was that the "next line is a branch" test was `' beq' in line` while the asm
  separator is a **TAB**.  It was caught only by an **anti-vacuity assertion
  written into the scan itself** — *the known SpotlightDrawer site must appear* —
  which the fixed version prints (`sites = 32`, `known site present: True`).
  ⇒ **Any scan whose negative would close a vein must assert a known positive in
  the same run.**  Same family as the `grep`-binary and `all([])` traps, and it
  cost one tool call to catch versus a whole false conclusion.
* ★ **The in-tree record is the cheapest instrument in the tree.**  Two `sed -n`
  calls on `CustomizePanel.cpp` retired a 5,036 B candidate that four lanes had
  already closed, including one whose stated wall rule a later lane refuted.
