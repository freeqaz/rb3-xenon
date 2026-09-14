# Game-unit crossing grind — lane W15-C, 2026-09-14

Branch `w15-c`, worktree `~/tmp/wt-w15-c`, base main **`8995d566`**.
Ruler **`name_check` (graded)**, read at runtime from `report.json`'s
`provenance.diff_config` (22 keys) — never assumed.

Baseline, this worktree's own full build, verified a patch fixed point first
(`scripts/verify_objs_patched.py --verify-manifest` → `OK: 1205 decomp, 3083
target objects match`, `tree_sha256=ec26b2b6a49a5795`):

```
matched_functions   42,846      matched_code    3,892,688 B
matched_code_percent 37.992430  fuzzy           49.189335
masked_equal        22,997      total_code     10,245,956
```

That reproduces §7o's 09-14 figures exactly, so the tree is at main's tip state.

---

## 0. Headline

- **+6 matched functions / +4,252 B / +0.041500 pp**, in three separately
  measured and separately committed changes. Final state
  **42,852 / 3,896,940 B / 38.033930%**.
- **The largest prize in the worklist is a priced REFUSAL, not an opportunity.**
  `?CountOrCreateExpandedDetails@NextSongPanel@@` is **12,220 B behind ONE
  charge**, and that charge is commutative-`add` operand order — the class
  `crossing_worklist.py` proved inert by direct experiment. Nothing in this
  lane can collect it.
- **Three real bugs the metric mostly could not see**: a locale/language field
  mix-up sent to the Rock Central service, an **undefined** MWCC-mangled symbol
  in a live call path, and a wrong `Find<T>` element type.
- **Two map defects found by retail-byte adjudication.** One was proven and
  repaired (+272 B); the second is proven wrong but **deliberately not renamed**,
  because proving a name wrong is not the same as knowing the right one.
- **The five units' headline gaps badly misdescribe the work available.** Only
  **75 named sub-100 rows / 65,740 B** exist across all five; RockCentral's
  41,200 B gap is mostly *unidentified* anon rows, and NextSongPanel's is
  mostly relocation-name charges.

---

## 1. The worklist, as ranked

`python3 tools/crossing_worklist.py --adjudicate --ruler graded
--max-mismatch 999 --top 999` over the whole binary, then filtered to the five
briefed units. The run reported **RULER SPLIT: 0/3023 disagreements** — pricing
and grading agree, so the instrument is sound.

Per-unit gap decomposition, read independently from `report.json` *before*
reading the tool's ranking (so the two are a cross-check, not an inheritance):

| unit | gap | partial (0<fz<100) | anon @0 (unidentified) | mpn100/fz<100 (name charges) |
|---|---:|---:|---:|---:|
| RockCentral | 41,200 | 12,636 (78 rows) | **20,932** (105) | 7,472 (70) |
| VocalTrack | 24,568 | **20,232** (26) | 2,144 (12) | 1,624 (34) |
| NextSongPanel | 17,368 | 1,088 (12) | 3,636 (5) | **12,644** (5) |
| GemManager | 17,260 | 5,220 (21) | **10,660** (44) | 1,280 (16) |
| OvershellSlot | 16,444 | 11,596 (10) | 4,764 (18) | 84 (2) |

⇒ **The headline gap is not a work queue.** RockCentral and GemManager are
dominated by rows we have not *identified*, which no source edit can reach;
NextSongPanel is dominated by relocation-name charges. The only dense
"divergence in code we already hold" veins are VocalTrack and OvershellSlot.

Top of the ranked worklist (size-if-it-crosses, graded):

```
12220 B  mm=1     fz= 99.997  ARITH_COMMUTE  NextSongPanel  CountOrCreateExpandedDetails
 9276 B  mm=2 sym fz= 99.996  SYMBOL         OvershellSlot  Handle
 8948 B  mm=1378  fz= 72.265  STRUCTURAL     VocalTrack     UpdateScrolling
 4676 B  mm=8     fz= 99.957  REGALLOC       RockCentral    RecordAccomplishmentData
 2188 B  mm=139   fz= 90.468  STRUCTURAL     VocalTrack     RebuildHUD
 1932 B  mm=1 sym fz= 99.990  SYMBOL         RockCentral    RecordScore
 1584 B  mm=8 sym fz= 99.404  REGALLOC       GemManager     UpdateLeftyFlip
 1396 B  mm=46    fz= 99.289  IMMEDIATE      OvershellSlot  OnMsg(ButtonDownMsg)
 1296 B  mm=30    fz= 94.105  STRUCTURAL     RockCentral    Handle
  896 B  mm=3 sym fz= 99.924  IMMEDIATE      GemManager     GemManager(ctor)
  788 B  mm=2 sym fz= 99.949  SYMBOL         RockCentral    SyncSetlists
  768 B  mm=1 sym fz= 99.974  SYMBOL         RockCentral    GetAllSonglists
  768 B  mm=1 sym fz= 99.974  SYMBOL         RockCentral    GetClosedBattles
  528 B  mm=12 sym fz= 99.470  REGALLOC       NextSongPanel  FinishLoad
  512 B  mm=1 sym fz= 99.961  SYMBOL         RockCentral    GetTickerInfo
  512 B  mm=4     fz= 97.617  STRUCTURAL     VocalTrack     Poll
  508 B  mm=1 sym fz= 99.961  SYMBOL         GemManager     PollHelper
  444 B  mm=2 sym fz= 99.910  SYMBOL         GemManager     UpdateArpeggios
  392 B  mm=2 sym fz= 99.898  SYMBOL         VocalTrack     CreateLyric
  328 B  mm=1 sym fz= 99.939  SYMBOL         GemManager     _M_insert_overflow_aux<Gem>
  292 B  mm=1 sym fz= 99.932  SYMBOL         VocalTrack     GetCurrentPlate
  272 B  mm=1 sym fz= 99.926  SYMBOL         OvershellSlot  UpdateFriendsList
```

### 1a. An instrument error I made, and the control that caught it

My first charge extraction globbed `~/tmp/crossing_worklist/diffs/*.json`
directly. That **bypasses the tool's per-entry input-signature validation** and
served me stale diffs from other builds: it reported `?Handle@VocalTrack@@` at
fuzzy 98.4 (my `report.json` has it at 100, i.e. absent from the sub-100 list)
and `?Handle@RockCentral@@` at 32.8 (real: 94.105). Caught only because those
contradicted `report.json`. Re-done by **importing `crossing_worklist` and
calling its own `diff_many`**, which validates `_cw_cache`, ruler, instrument
and `inputs`. The validated population is **75 rows / 65,740 B**, which
reconciles exactly with the report (83 named sub-100 rows minus the 8 at
fuzzy 0, which `load_rows` excludes by construction).

⇒ **Do not re-derive a cache read. Import the tool.**

---

## 2. The fold-vs-wrong-callee instrument

Most remaining rows hinge on one question: is a `[sym]` charge an ICF
fold-alias (irreducible) or a genuine wrong callee (fixable)? I built a
binary-wide fan-out census over every charged relocation-name site
(**2,327 distinct target callees / 5,012 sites**; `/home/free/tmp/w15c_foldpairs.json`).

**The principle:** a fold survivor is *one retail body* wearing *one arbitrary
survivor name*, so it pairs with **many unrelated** our-callees. A genuine
wrong callee pairs **1:1**.

| target callee | sites | distinct our-callees | verdict |
|---|---:|---:|---|
| `StlNodeAlloc<_List_node<int>>::ctor` | 132 | **98** | FOLD |
| `push_back<vector<ChatReceiver*>>` | 251 | **85** | FOLD |
| `reserve<vector<Dep*>>` | 43 | **22** | FOLD |
| `vector<int>::ctor` | 21 | 11 | FOLD |
| `push_back<pair<VocalPhrase*,VocalPart*>>` | 74 | 3 | FOLD (the §4a archetype) |
| `_M_push_back_aux_v<deque<LyricPlate*>>` | 3 | 2 (both pointer-deques) | FOLD |
| `_Param_Construct<Char3D>` | 2 | 2 (both Gem ctors) | FOLD |
| `IsScrolling@UIList` | 1 | 1 | **1:1 → adjudicate** |
| `clear@_List_base<PassiveMessage*>` | 3 | 1 | **1:1 → adjudicate** |
| `Find<BandLabel>@ObjectDir` | 2 | 1 | **1:1 → adjudicate** |
| `PlayableBy@VocalNote` | 1 | 1 | **1:1 → adjudicate** |
| `Init@Movie` | 27 | 1 | **1:1 → adjudicate (see §5)** |
| `GetCacheName@CacheXbox` | 1 | 1 | 1:1, but row is regalloc-walled |

⚠ **Limit of the heuristic, stated because it nearly bit me.** `SystemLocale`
showed **2** distinct our-callees and would read "fold-ish" by a naive ≥2 rule
— yet fixing it measured **+3,980 B**, i.e. it was unambiguously a wrong
callee. High fan-out (≥3, many sites) is strong fold evidence; **2 is not**.
Measurement outranks the heuristic.

---

## 3. Rows OPENED

### 3.1 ✅ `SystemLanguage()` → `SystemLocale()` ×4 — RockCentral (`43e70cab`)

Four rows, **each with exactly one charge, and the same charge**:

```
TGT  bl ?SystemLocale@@YA?AVSymbol@@XZ
SRC  bl ?SystemLanguage@@YA?AVSymbol@@XZ
```

| row | size |
|---|---:|
| `?RecordScore@RockCentral@@` | 1,932 |
| `?GetAllSonglists@RockCentral@@` | 768 |
| `?GetClosedBattles@RockCentral@@` | 768 |
| `?GetTickerInfo@RockCentral@@` | 512 |

**Fold refuted on mechanism, not opinion.** `src/system/os/System.cpp:194-195`:
`SystemLanguage()` returns `gSystemLanguage`, `SystemLocale()` returns
`gSystemLocale`. Two different globals ⇒ two different relocations ⇒ MSVC
cannot fold them (it folds only COMDATs identical *including* relocations).

**Positive control inside the same TU and build:** line 410
(`GetSongFullOffer`) already used `SystemLocale()` and carries **no** such
charge, while all four `SystemLanguage()` sites carry exactly one each. The
charge appears iff we call `SystemLanguage`.

The rb3-Wii **dev** oracle uses `SystemLanguage()` at all seven sites — another
instance of **retail bytes outranking the oracle**.

**Deliberately not changed:** line 237 (`OnMsg(ServerStatusChangedMsg)`) —
field is `locale` and it is probably the same defect, but that row sits at
fuzzy 94.0 with 61 charges and carries **no** SystemLocale charge, so there is
no evidence here; and line 1812 (`AddBuildInfoToDP`) whose field is **`h_lang`**,
where the language genuinely is correct. *A blind sweep of this file would have
broken 1812.*

| | Δfns | Δbytes | Δcode% |
|---|---:|---:|---:|
| **predicted** | +4 | +3,980 | +0.038845 |
| **measured** | **+4** | **+3,980** | **+0.038847** |

Attribution: +4 `default/RockCentral` (768→772), zero collateral. All four rows
now read **fuzzy 100.00000** in `report.json`.

**Behavioural:** on any console whose language and locale differ (eng/gbr),
every score, ticker, songlist and battle query reported the wrong field.

### 3.2 ✅ Two wrong callees — VocalTrack + NextSongPanel (`c1a1531e`)

**(a) An UNDEFINED MWCC symbol on a live path.** `VocalTrack::CreateLyric`
called `PlayableBy__9VocalNoteCFi` — a **Wii CodeWarrior** mangling — through
`extern "C"`, under a comment claiming `VocalNote::PlayableBy` was unavailable
here. The comment is false three ways: it *is* declared
(`beatmatch/VocalNote.h:40`), it *is* defined (`VocalNoteList.cpp:653`), and
`PlayableBy__9VocalNoteCFi` has **no definition anywhere in the tree** — it
survives only because the matching build never links.
`docs/decomp/sympair-queue.tsv:428` had already flagged this row and nobody
acted on it.

**(b) Wrong `Find<T>` element type.** `NextSongPanel::FinishLoad` used
`Find<UILabel>("highscore_1.lbl")`; retail's callee is
`??$Find@VBandLabel@@@ObjectDir@@`. Behaviourally identical (BandLabel derives
UILabel **first**, so the `+0x1BC` poke lands on the same address), and the
0x1BC field stays a byte poke because BandLabel's own members start at 0x238.
Corrected a stale in-source note claiming the symbol "reads as
`Find<RndAnimatable>`".

| | Δfns | Δbytes |
|---|---:|---:|
| **predicted** | +0 | +0 |
| **measured** | **+1** | **+0** |

**My prediction was wrong, and the mechanism is instructive.** I assumed
`FinishLoad`'s 11 remaining register charges would hold it below both rulers.
They do not: `mpn` **excludes register arg diffs**, so `FinishLoad`'s
`mpn = 99.96212` was held below 100 by the **relocation-name charge alone**.
Removing it took `mpn` to 100 (+1 function) while `fuzzy` stayed short
(registers *do* charge fuzzy) — so the row pays a function and zero bytes.
`CreateLyric` moved neither ruler, consistent with its residual being a fold
(folds charge *both*).

### 3.3 ✅ Map defect: `0x826662e0` is `FriendsProvider::Reload` (`5d079253`)

`?UpdateFriendsList@OvershellSlot@@` (272 B) had exactly one charge:
`TGT bl ?IsScrolling@UIList@@QBA_NXZ` vs `SRC bl ?Reload@FriendsProvider@@QAAXXZ`.
**Our source was right; the map was wrong.** Retail body at `0x826662e0`:

```
addi r3, r3, 0x2c
b    fn_8250CEE0        ; = DeleteAll<std::vector<Friend*>>(vector<Friend*>&)
```

Steps `this` to a `vector<Friend*>` at +0x2c and deletes all of it — that is
`Reload`. It **cannot** be `UIList::IsScrolling`, which mangles `QBA_NXZ`: a
public **const** member returning **bool** cannot tail-call a mutating
`DeleteAll(vector&)` returning void. Signature contradicts body — the
`Handle@GemPlayer` archetype.

**Priced before editing**, because a map edit's danger is un-pairing:
`UpdateFriendsList` is fz=mpn=99.92647 (crosses both rulers);
`?IsScrolling@UIList@@QBA_NXZ` is **8 B at fz=mpn=97.0**, i.e. contributes
**zero** `matched_code` and zero functions today, so un-pairing it can cost
nothing. Blast radius measured, not assumed: **exactly one** `bl` site to
`fn_826662E0` in the whole target asm, and `?Reload@FriendsProvider@@QAAXXZ`
was absent from the map (no duplicate-key hazard).

| | Δfns | Δbytes | Δcode% |
|---|---:|---:|---:|
| **predicted** | +1 | +272 | +0.002655 |
| **measured** | **+1** | **+272** | **+0.002653** |

`Δfuzzy = −0.000070 pp` because the 8-byte UIList row stopped pairing at a
**false** 97%. By the standing directive (accuracy over headline %), a drop
from a truer denominator is a win.

---

## 4. Rows DECLINED, with the reason

| row | size | why declined |
|---|---:|---|
| `CountOrCreateExpandedDetails@NextSongPanel` | **12,220** | mm=1, and it is `add r3,r11,r28` vs `add r3,r28,r11` — **ARITH_COMMUTE**, the class `crossing_worklist.py` proved inert by direct experiment (MSVC canonicalises commutative operand order; the source edit is a no-op). **ESCALATE** — largest single-charge prize in the tree. |
| `Handle@OvershellSlot` | **9,276** | mm=2. idx 1336 is a **proven fold** (`StlNodeAlloc<_List_node<int>>::ctor`, 98 distinct our-callees). Folds charge *both* rulers, so the row cannot cross however idx 689 is resolved. Same shape as the CustomizePanel refusal. |
| `UpdateScrolling@VocalTrack` | 8,948 | mm=**1378** across 121 insert/delete clusters, 392 register charges, 45% equal. Body divergence entangled with regalloc — the hardest class. I checked and **refuted** the obvious lead: the `MILO_DEBUG` dev-code hypothesis is already exploited (lines 169/303 carry the house `#if defined(MILO_DEBUG) && defined(HX_NATIVE)` guard). |
| `RecordAccomplishmentData@RockCentral` | **4,676** | mm=8, **all register** (r29↔r30 swap) + 1 operand reversal; `mpn` already 100. Pure permuter class. **ESCALATE.** |
| `RebuildHUD@VocalTrack` | 2,188 | mm=139, diffuse across 8 classes. |
| `UpdateLeftyFlip@GemManager` | 1,584 | mm=8 = 4 register + 2 insert/delete that are only the *alignment shadow* of those swaps (same `lis …@h` on both sides) + 1 fold + 1 offset. Register-walled. |
| `OnMsg(ButtonDownMsg)@OvershellSlot` | 1,396 | mm=46, 43 of them immediates — diffuse, no single construct named. |
| `Handle@RockCentral` | 1,296 | mm=30 (18 structural / 8 immediate / 4 opcode) — **zero** register and **zero** symbol charges, so it is all source construct. Genuinely the best remaining *source* candidate; not opened for budget. **Hand to a follow-up lane.** |
| `GemManager(ctor)` | 896 | mm=3. The two IMMEDIATE charges are a store-order swap of `unkb8` (0xc4) and `mBonusGems` (0xcc) — **both member-initializers, and MSVC emits those in DECLARATION order**, which is pinned by the retail offsets. Reordering the init list is inert: **there is no source lever.** The third charge is the proven `reserve` fold, capping the row below fuzzy 100 anyway. |
| `Poll@VocalTrack` | 512 | **Refuted in-source by lane W19-VOCAL**, with its measurement: the alternative spelling scores **97.617 → 95.195**. Residual is retail failing to coalesce a `clrlwi` normalization; our code is one instruction shorter. Do not retry. |
| `FinishLoad@NextSongPanel` residual | 528 | after §3.2 its remaining 11 charges are **register-only** — stopping there, as instructed. |
| `SyncSetlists@RockCentral` | 788 | mm=2, **both proven folds** (`vector<int>::ctor` 11 callees; `push_back<pair<VocalPhrase*,VocalPart*>>` — the exact §4a archetype). |
| `PollHelper@GemManager` | 508 | mm=1, proven fold (98-callee survivor). |
| `UpdateArpeggios@GemManager` | 444 | mm=2; idx 37 proven fold. idx 63 is `Init@Movie` — see §5. |
| `GetCurrentPlate@VocalTrack` | 292 | mm=1, fold (`_M_push_back_aux_v` over two unrelated pointer-deques). |
| `_M_insert_overflow_aux<Gem>@GemManager` | 328 | mm=1, fold (`_Param_Construct<Char3D>` pairs with two different Gem ctors). |
| `_S_sort<Symbol>@VocalTrack` | 424 | mm=4, all EH-handler relocations to a `_List_base` destructor — fold. |
| `SetupRealGuitarAreaStrumSections` / `Poll@NextSongPanel` / `ProcessStaticLyrics` | 344/304/312 | register-only (+1 commute each). `mpn` already 100 on two of them. |
| `PrepareNoteTubes` / `VocalTrack(ctor)` / `UpdateTubePlates` / `PollLyricAnimations` / `UpdateTambourineGems` / `GetNextLyricPlate` / `SyncAvailableSongs` / `OnMsg(ServerStatusChanged)` / `UpdateSetlist` / `DataPointToQString` / `AttemptRemoveUser` / `Entry@LocalePanel` | 1,160 / 868 / 772 / 744 / 744 / 516 / 944 / 1,024 / 1,268 / 460 / 300 / 308 | all mm ≥ 29, diffuse multi-class body divergence. Not single-construct rows; they need body ports, not instruction edits. |

---

## 5. Leads handed on (proven wrong, NOT repaired)

**A. `0x825df840` is not `clear@_List_base<PassiveMessage*>`.** Its retail body
loads three distinct members (`0x38/0x3c/0x40`), makes a **virtual call through
vtable slot 0x1c**, then two more calls, and returns void. No list `clear()`
has that shape. It sits **0x58 before** `?AttemptRemoveUser@OvershellSlot@@`
(`0x825df898`), and our `RemoveUser`/`AttemptRemoveUser` are adjacent in
source — so it is almost certainly `?RemoveUser@OvershellSlot@@QAAXXZ`, which
is **absent from the map**.
**Not renamed on purpose:** our `RemoveUser` body does *not* match it (retail
has 4 calls and no `TheSaveLoadMgr` null check; ours has 6 calls and the
check), so renaming would pair it at a low score, and **proving a name wrong is
not knowing the right one**. The 9,276 B it sits behind is uncollectable
anyway (§4). This belongs to W15-B, together with the observation that
`AttemptRemoveUser` is itself at **fuzzy 14.84**.

**B. `Init@Movie@@SAXXZ` ↔ our `TickToMs@@YAMM@Z`, 27 sites, 1:1.** Twenty-seven
call sites where our `TickToMs(float)` pairs against a target symbol the map
names `static void Movie::Init()`. A 1:1 pairing at 27 sites is the signature
of a **systematic map error**, not a fold (a fold survivor fans out). If
`0x…` is really `TickToMs`, one map repair pays 27 sites. Not adjudicated here
for budget; hand to W15-B.

---

## 6. What I did NOT do

- Did not run the permuter (deferred by standing directive); the two escalation
  candidates above are exactly permuter-shaped.
- Did not open the diffuse ≥29-charge body rows — they are oracle-porting work,
  not crossing work, and VocalTrack's source is already the rb3-Wii port
  (2,782 lines vs the oracle's 2,777), so a source diff shows nothing.
- Did not touch RockCentral line 237 or 1812, or rename `0x825df840` — all
  three stated above with their evidence.
- Did not attempt the `auto_*`/anon strata that make up most of RockCentral's
  and GemManager's headline gaps; those need identification, not source.
