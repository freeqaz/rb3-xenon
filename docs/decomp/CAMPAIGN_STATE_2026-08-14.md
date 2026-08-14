# Campaign state — 2026-08-14

> **Second edition, rewritten end-of-session by lane CONSOLIDATE-1 at `3eb85dfd`.**
> The first edition (`1ebe3bf3`, 04:05) was written after ~20 lanes; **27 more
> landed after it** and corrected it twice in flight (`3f5b03fc`, `8ea4be91`).
> This edition replaces it wholesale. §8 lists what it corrected and why.

**Who this is for:** a cold pickup weeks from now. It exists so you inherit
conclusions instead of re-deriving them, and so you do not re-fund a vein that
was measured empty.

**Conventions.**

- **MEASURED** = a number I or a named lane read off `report.json`, retail
  bytes, or an executed A/B. *Inferred* = reasoned, not read. Every figure below
  is measured unless marked *inferred*.
- ⚠ **Do not read an absolute score out of this file.** `total_code`,
  `total_functions` and every percentage move when pins change; the two headline
  keys are computed on different rulers. **Read `report.json`** and read its
  `provenance` block — it self-declares its ruler.
- All scores here are on the **shipped graded ruler**
  (`functionRelocDiffs=name_check`), pinned to commit **`3eb85dfd`**.

---

## 0. Thirty-second orientation

| you want to know | go to |
|---|---|
| where the score actually is, and what the ceiling is | **§1** |
| whether there is a big lever left (**no**) | **§2** |
| what is still worth working | **§3** |
| what is drained — do not re-fund | **§4** |
| what will lie to you (~35 instrument failures, by mode) | **§5** |
| how to adjudicate a row honestly | **§6** |
| the eight source-invisible mechanisms | **§7** |

---

## 1. Measured state at `3eb85dfd`

Read by exact key from a `report.json` regenerated on a settled worktree
(**0 compile edges on two consecutive passes**), ruler `name_check`:

| key | value |
|---|---|
| `matched_functions` | **44,404** |
| `masked_equal_functions` | 22,897 |
| **honest** (`matched − masked_equal`) | **21,507** |
| `matched_code` | **3,725,560 B** |
| `matched_code_percent` | **36.098064** |
| `fuzzy_match_percent` | 48.57869 |
| `total_code` / `total_functions` | 10,320,664 / 69,227 |
| alias validator | **VALIDATE: PASS** — 1,322 map-consistent, 202 tolerated, 0 contradicted, **1,528 groups** |

### 1a. The ceiling — **re-measured today at 62.87%, and it moved**

`tools/noobj_census.py` re-run on this tree (join self-validates: rows sum to
`total_functions`, bytes to `total_code`, zero dropped):

| class | units | rows | bytes | % `total_code` |
|---|---|---|---|---|
| PAIRABLE (has base obj) | 1,047 | 54,672 | 6,488,248 | **62.87%** |
| UNPAIRABLE — no source | 230 | 4,454 | 2,106,356 | 20.41% |
| UNPAIRABLE — `auto_*` | 1,811 | 10,101 | 1,726,060 | 16.72% |

⇒ **`matched_code` 3,725,560 B is 57.42% of the reachable surface**, not 36.1%
of it.

⛔ **NOOBJ-1's 63.10% (2026-08-13) is stale by 0.23 pp after one day.** That is
the point of the standing rule, not an exception to it — **re-measure the
ceiling, never inherit it.** AUTOID-1's further ~1.75 pp scaffold discount (105
PAIRABLE units whose base objs define only 1–2 symbols) was **not re-measured
today**; applying it *inferentially* gives an honest ceiling ≈ **61.1%**. Treat
61–63% as the band and re-derive if a decision hangs on it.

Both unpairable classes stay closed: **229 of 230** no-source units are `xdk/*`
Microsoft vendor (writing them is out of scope, and they are already 100%
*mapped*); only **8.9%** of `auto_*` is attributable-and-portable.
⛔ Do not stub either into compiling — that buys pairable rows at 0% with no
content.

### 1b. ★ The decomposition that matters — where the 2.76 MB of headroom is

Every pairable row, partitioned by the two rulers. Self-validates (69,227 rows /
10,320,664 B), and stratum A reproduces `matched_code` **to the byte**, which is
a live re-confirmation of DB-4's rule that `matched_code` keys on `fuzzy == 100`:

| stratum | rows | bytes | % `total_code` | % pairable |
|---|---|---|---|---|
| **A** matched (`fuzzy == 100`) | 38,636 | 3,725,560 | 36.098% | 57.42% |
| **B** `mpn == 100`, `fuzzy < 100` — reloc-name / arg-only | 5,768 | 784,012 | 7.597% | 12.08% |
| **C** `mpn < 100`, `fuzzy > 0`, **named** — ordinary near-miss | 1,140 | 537,656 | 5.210% | 8.29% |
| **C′** same, anonymous | 1,491 | 61,460 | 0.596% | 0.95% |
| **D** `fuzzy == 0`, **anonymous** — unpaired *inside compiled units* | 7,279 | **1,329,264** | 12.880% | 20.49% |
| **D′** `fuzzy == 0`, named | 358 | 50,296 | 0.487% | 0.78% |
| — UNPAIRABLE | 14,555 | 3,832,416 | 37.133% | — |

**Gap to the pairable ceiling: 2,762,688 B.** Read the shape:

- **D is the single largest block — 48.1% of the gap** — and it is **absence of
  identification, not divergence**: 7,279 rows in units we already compile that
  carry no map name, so they cannot pair at any quality of source. Extremely
  diffuse: **752 units, top 25 = 20.5%**, largest unit 24,100 B.
- **B is 28.4% of the gap** and is ~84% irreducible (§2).
- **C is 19.5% of the gap** — the genuine "write better source" surface,
  1,140 rows, **no row larger than 8,948 B**.

### 1c. What the whole session moved — 47 lanes

Measured, not summed: I built and read `report.json` at the day-boundary commit
`56344fe7` (2026-08-13 23:53) and at `3eb85dfd`, **same ruler, same
`total_code`, both settled**, so these absolutes are directly comparable.

| key | day open `56344fe7` | now `3eb85dfd` | Δ |
|---|---|---|---|
| `matched_functions` | 44,355 | 44,404 | **+49** |
| `masked_equal_functions` | 22,889 | 22,897 | +8 |
| honest | 21,466 | 21,507 | **+41** |
| `matched_code` | 3,575,380 | 3,725,560 | **+150,180 B** |
| `matched_code_percent` | 34.642925 | 36.098064 | **+1.455139 pp** |
| `total_code` | 10,320,664 | 10,320,664 | 0 |

★ **47 lanes bought +150,180 B and +49 matched functions.** And **~53% of those
bytes are alias-forgiveness bookkeeping** — INCOMPLETE-1 (+60,700 B) and
INSTALL-1 (+19,136 B) alone — not source work. That single row is the strongest
evidence for §2.

---

## 2. ★★★ The core finding — every large lever was sized this session, and they all deflate

Each row was a headline candidate at some point today. Each was measured, and
each shrank. **Quote the closing number, never the opening one.**

| lever | opened as | closed at | who closed it |
|---|---|---|---|
| ICF-alias forgiveness | "unaudited exposure, 221 failing groups" | **720,992 B total, but 82.51% (594,904 B) already PROVEN on retail bytes**; unproven residue 124,168 B; **129,360 pair-bytes irreducible** (which name the call site meant was destroyed by ICF itself) | ALIASAUDIT-1 `df90b49f`, GROUNDED-1 `f4e26fcc` |
| the `mpn`/`fuzzy` ruler gap | "a queue that did not exist an hour ago" | **a STRICT SUBSET of MPNGAP-1's stratum, `\|B \ A\| = 0`**; instruction defects **0% by construction**; realisable upper bound **109,708 B = 16.3% of the gap / 1.06% of `total_code`** ⇒ **~84% irreducible** | RULERGAP-1 `3eb85dfd` |
| `TEMPLATE_ARGS` | "446,280 B of wrong template arguments" | **51.0% is fold** (343 pairs / 227,560 B); the wrong-callee (C) vein is **4.1% of bytes / 18,240 B** — and CVEIN-1 then **overturned the reading itself**: (C) is mostly folds, our source spelling is already right, and the collectable union is **15,752 B, fully covered 9,768 B** | TEMPLATE-1 `5a0a26f2`, CVEIN-1 (in `40db9762`) |
| "313 wrong-callee source defects" | 40,608 B queue | **54% (100 pairs / 21,912 B) fold-shaped**, 32% size-differs, 14% same-size-differs; of the fold-shaped 100, **87 refuted / 7 proven**; the requalified 213 (18,696 B) yielded **+9,560 B = 51.1%** | WRONGCALL-3 `1ad913dc`, FOLDPROVE-2 `554a15ac`, WRONGCALL-4 `045b393d` |
| "202 that can ONLY be a wrong callee" | 202 airtight defects | **~0 wrong callees** — SOURCE_SUSPECT **1 of 168 (0.6%)** against a shuffled null of 42.9%, and that one does not survive inspection | SRCCAND-1 `f8d2d76e` |
| the 74 MAP_SUSPECT rows | a map-rotation vein | **74/74 map-CORRECT**, **0 provable rotations**, cycle closure a **dead lever** (the untreated population closes cycles at the same rate); the `+0x60` lever is a **proven fold with no source edit to make** | MAPSUS-1 `b574f653` |
| identification tooling | "the identification wall" | **~209 functions / ~21,164 B = ~0.2% of `total_code`**, and IDENT-1 took most of it (20 names, +19 matched / +5,000 B); GRIND-1 took the band3 remainder (+10 / +3,084 B) | IDENT-1 `65154fb0`, GRIND-1 `d91954af` |
| "we do not hold the body" | **11.57%** of `total_code` (5,363 rows / 1,194,012 B) | genuine write surface **15 units / 5,348 B = 0.052%** | BODYWRITE-1 `ac6d4d62` → GRIND-1 `d91954af` |
| epilogue over-carve | a splitter defect class | **162 runs / 246 blocks / 9,968 B**, of which **2 live rows** (of 3 named-head runs); **1 closed** (+1 fn / +120 B), 1 priced-and-deferred, **244 gated behind the map, not the splitter** | SPLITBLOCK-1 `57e8437e` |
| compiled-but-unpinned units | "drainable wholesale" | **5 rows / 544 B** (1.35% of the orphan bytes); **68.3% of the class's owned functions are Dance Central code with no RB3 counterpart** | PINSRC-1 `6cb87d90` |
| orphan pins | 267 rows / 38,096 B | **re-homable 59 rows / 2,972 B** — ~13× smaller | PINHOME-1 `8e6eb9be` |
| factory-registration order | a systematic seam | **17 retail multi-lists** (two independent enumerations agree, so the census is bounded by the *binary*); **5 differed, all MEMBERSHIP, 0 pure ORDER**; remainder = 4 ports + 1 missing function | REGORDER-1 `6f00c6e6` |
| unsuffixed double literals | a codegen class | **3 of 73**, **0 crossable** (Δ0 measured) | DBLLIT-1 `dffb0454` |
| surviving `MILO_*` emissions | 2,326 source sites | **4 in the whole binary**, exactly **1 actionable** | MILOKEEP-1 `2c7c31bd` |

### ⇒ There is no big lever left.

The remaining 2.76 MB of headroom (§1b) is **three ordinary things, none of
which is a trick**:

1. **1.33 MB of unidentified rows inside units we already compile** (D) — 752
   units, no concentration, and naming carries the documented downside that it
   converts *forgiven* placeholder call sites into *checked* ones.
2. **784 kB of relocation-name charges** (B) — ~84% fold/map noise, realisable
   **109,708 B**.
3. **538 kB of ordinary named near-misses** (C) — real source divergence, one
   row at a time, largest row 8,948 B.

⚠ **Correction to the framing this lane was briefed with.** The brief said the
remainder is "ordinary per-row matching on units we already compile —
divergence, not absence." That is **true of stratum C and false of the largest
block**: D is 2.5× C's size and is *absence of identification*. Both are
ordinary and neither is a lever; but a lane briefed on "divergence" will look in
the wrong stratum.

---

## 3. What is open

### 3a. The near-miss stratum (C) — the honest source-work frontier

**1,140 named rows / 537,656 B.** Standing rule: `matched_code` is
all-or-nothing per row, so rank by **size-if-it-crosses**, and the near-crossing
slice is where the odds are:

| fuzzy band | rows | bytes |
|---|---|---|
| [99, 100) | 196 | 75,864 |
| [95, 99) | 182 | 129,392 |
| [90, 95) | 144 | 70,104 |
| [75, 90) | 237 | 114,068 |
| [50, 75) | 195 | 93,624 |
| [0, 50) | 186 | 54,604 |

**fuzzy ≥ 95: 378 rows / 205,256 B (1.99% of `total_code`).**

Largest rows (size-if-it-crosses, with current fuzzy): `VocalTrack::UpdateScrolling`
8,948 B @ 72.25 · `DataInitFuncs` 8,068 B @ 70.53 · `MD5::transform` 6,068 B @
82.19 · `CSHA1::Transform` 5,856 B @ 55.69 · `CharacterCreatorPanel::Handle`
5,164 B @ 98.24 · **`CustomizePanel::Handle` 5,036 B @ 99.92057, ONE charged
site** · `VocalPlayer::Handle` 4,936 B @ 97.97 · `Spotlight::SyncProperty`
4,728 B @ 99.39.

> **🔲 PLACEHOLDER — lane INSTR-1.** A lane is censusing this `mpn < 100`
> stratum by root cause for the first time, concurrently with this write-up.
> **Its result is not in this document.** When it lands, paste its
> classification here and re-check the ranking above against it. Do not guess
> the answer from the size table — the whole point of §5 is that a plausible
> number is not a measured one.

### 3b. Named residuals with diagnoses recorded at their sites

- **`?Handle@CustomizePanel@@`** — **5,036 B, fuzzy == mpn == 99.92057, exactly
  ONE charged site** (a target-only `clrlwi r11,r11,24` on the has_license /
  has_patch bool tail). RESIDUAL-2 closed 2 of the 3 and characterised the
  third: a binary-wide scan finds the pattern at 12 sites, 4 of which we already
  match 100%, and those controls show **the mask is emitted ONLY AT A PHI**
  (in three it is literally a branch target). Retail's site has no label there;
  MSVC range-analyses the `subfe` result as 0/1 and elides every narrowing,
  including an explicit `(unsigned char)` cast. Nine source shapes inert, three
  worse. **Closing it pays the full 5,036 B + 1 fn.**
- `PracticePanel::Poll` remainder · `CharIKHand::IKElbow` (`fsubs` operand order
  + a structural −0x10 frame delta) · `VocalTrackDir::PostLoad` ·
  `RndFlare::CalcScale` (CSE/scheduling divergence, now *exposed* rather than
  masked — see §5 #60).
- **`DataNode::operator==`** — priced at ~+472 B / +1 fn but blocked on a jeff
  **P1** relaxation. SPLITBLOCK-1 recommends **not** doing it: a fleet-shared
  binary change for 0.0046% of `total_code`.
- **`?MemOrPoolAllocSTL@@YAPAXH@Z` (28,624 B)** — SRCPORT-1's handoff. If
  retail's 1-arg STL form carries the temp guard, the fold is refuted and those
  bytes **withdraw**. This is an integrity risk, not a prize.

### 3c. Deliberately not funded

- **RULERGAP-1's 247,376 B `ALL_FOLD` stratum stays unaliased** — classified,
  not proven. Coordinator ruling, recorded in-commit.
- **141–143 `STALE_SPELLING` alias groups are tolerated, not pruned.** Pruning
  is measured harmful: `a745039e` restored 14 such at **+94,616 B**, and
  dropping UNWITNESSED groups cost **−6,652 B / 2 units off 100%**.
- **The permuter is OFF by user directive.** Permuter-class regalloc residue is
  therefore parked, not solved.

---

## 4. Drained — do not re-fund

Everything in §2's table is drained at its closing number. Additionally:

| vein | verdict | closing number |
|---|---|---|
| `mpn==100 / fuzzy<100` stratum (MPNGAP-1) | **depleted, not enriched** | real-defect class **10.7% vs 12.1% control = 0.88×** |
| REFUTED relocation-name pairs, **as bytes** | 71% ICF-fold-gated | trustworthy worklist ~2% / ~13,000 B |
| no-source XDK | needs Microsoft source | 229/230 units |
| `auto_*` attribution | mostly unportable | 8.9% attributable-and-portable; upper bound 5.51% |
| map naming (argument types), engine + network | **broadly sound** | SIGSCAN-1: **7 rows of 876** uncrossed engine rows; network **zero** |
| body-port, structural half | **not reachable** | **0 of 47,820 B**; residue is pure-`reg` permuter-class |
| `json_tokener` version skew | refuted | our 12 error strings incl. the `"nesting to deep"` typo ⇒ json-c 0.9, correct for 2010 |
| cycle closure in the map | **dead lever** | untreated population closes 9 two-cycles + 1 three-cycle at identical row count |
| 2,271 contiguous same-unit `.text` block pairs | **not a lever, do not sweep** | exactly **one** cut a live row |

**Retired rows** — documented negatives. ⚠ **Re-price before re-opening; see the
un-retirement rule in §6.** `?Handle@BandStorePanel@@` (only lever is
metric-fitting) · `?Interp@CameraShot@@` (association lives in shared math-header
inlines ⇒ engine-wide fan-out) · `?SyncProperty@BandWardrobe@@` (tried, inert).

**Standing DO-NOT warnings, each of which would break working code:**

- ⛔ `SongData::mRangeShifts` is **correctly** `map<int,float>` — `AddRangeShift(int,float)`
  indexes by int. Editing it to `map<CRC,float>` to collect 8,212 B breaks
  working code to satisfy a fold.
- ⛔ **BandTrack `GetTrackIcon`/`UserName` are transposed and deliberately NOT
  repaired** — both bodies already read 100 (name_check forgives the placeholder
  literal) and our vtable order mirrors the map's transposition, so a map-only
  swap measures **net −84**. The honest repair needs a parent vtable reorder.

---

## 5. ★★★ What will mislead you — ~35 instrument failures, by mode

⚠ **This session caught roughly 35, not the dozen the consolidation brief
expected.** They are grouped by *failure mode* rather than by lane, because the
mode is what you can check for. **Every one produced a clean, decisive-looking
number.**

### 5a. Vacuities that agree with your prior — the most dangerous class

A vacuity that confirms your hypothesis is the hardest kind to catch, because
nothing about the output looks wrong.

- **A fresh worktree's reflinked target objs are PRE-RENAMER**, so every retail
  mangled name reads *absent*. FOLDPROVE-2's cheap kill reported a **unanimous
  100/100 refuted — exactly what it was primed to expect**. Caught only by the
  **symbol count disagreeing with main's (69,438 vs 69,415)**. True answer: 7
  folds / +2,564 B. Same root cause makes `--validate` report **"0 OK, 1499
  failing"** in any unbuilt worktree — a 100% failure describing the *build
  state*. (`b19d6178`, `554a15ac`, `9168f776`)
- **`all()` over an empty list is `True`** ⇒ a jump-table census returned
  **37,752 blocks / 2.67 MB**. True: 246 blocks / 9,968 B. (`57e8437e`)
- **A confident vacuous `0/3086`** — a binary-wide `.s` scan keyed on the
  **synthetic address column instead of the `.fn` symbol**. Believed only after
  validation against a known positive, which found 12 sites. (`1f078361`)
- **A retail reader anchored `^\s+`** against dtk `.s` lines that begin with a
  `/* addr */` comment ⇒ **confident 0**. *(A separate instance from the one
  above — two different mechanisms, both returning a clean zero.)* (`dffb0454`)
- **A non-recursive `obj/*.obj` glob** missed **569 of 3,084** live target objs,
  so symbols defined only in a subdirectory read "unwitnessed, therefore inert".
  Had already cost **−6,652 B / 2 units off 100%**. (`9168f776`)
- **A census keyed on the wrong JSON field** (`diff_kind`, `None` for all 1,333
  rows; the real key is `match_type`). (`b81c03b8` / `0dfc1ec3`)
- **`nobody_contamination.py` failed to fire on the very case it was built
  from.** Fixing it moved UNDECIDABLE 16.4% → 42.8% — the honest cost of an
  instrument allowed to say "I don't know". (`ac6d4d62`)
- **grep is binary-blind** — PINSRC-1's zero-occurrence proof was run in Python
  *precisely because* the agent-shell grep would have returned the same answer
  vacuously. (`6cb87d90`)

### 5b. Tools that silently misreport

- ⛔⛔ **`run_objdiff` was OVERRIDING the shipped ruler, not duplicating it.**
  `objdiff-cli` applies `objdiff.json`'s options and then applies `-c` **last**,
  so `mcp_server.py`'s hardcoded `-c functionRelocDiffs=none` overrode
  `name_check` on every call, in four places. **7,157 rows disagreed**; **5,555
  rows / 674,936 B read `fuzzy == 100` under `none` but below 100 graded.**
  Live fixture `?Handle@OvershellSlot@@`: *"100.0%, 2319 all equal, 0 mismatches,
  Complete — No action needed"* vs graded *99.99569%, 2 `diff_arg`, AtLimit*.
  ★ **The MISMATCH COUNT is ruler-dependent, not just the percent** (0 / 2 / 641
  for one row). It mis-briefed `?Handle@CustomizePanel@@` to **three** lanes.
  Fixed: `scripts/analysis/ruler.py` resolves the ruler at runtime from
  `report.json`'s provenance. (`7286bfd1`, victims `348e3c7b`, `5a2ce6ba`)
- ⛔ **`ab_measure` forced ONE re-split per leg and under-reported a splits patch
  by the FULL byte amount.** jeff's Class-4 over-carve merge converges via
  `symbols.txt` feedback *across* re-splits. Known-answer fixture: **OLD +1 fn /
  +0 B vs NEW +1 fn / +120 B.** Invisible because `mpn` excludes the arg-only
  penalty, so `matched_functions` moved either way. Now iterated to a fixed
  point. (`57e8437e`, fixed `c8f74dcb`)
- ⛔⛔ **`ab_measure`'s run dir collided across concurrent lanes and applied one
  lane's patch into another's worktree.** `--run-root` defaulted to the
  *script's* location, not `--worktree`. Lane SIGSCAN-1's first run applied
  SRCPORT-1's `MemMgr.cpp` patch, **destroyed its own three edits, and printed a
  confident "A/B RESULT (MEASURED)" delta of 0** for a patch its own
  `[classify]` line had not named. Now keyed on the worktree, pid in the leaf,
  and **refuses on collision instead of sharing**. (`b29d4c05`)
- **`report.json` bills a phantom fat prize.** Shader's `fn_824A59D4` is billed
  **8,852 B — 73% of that unit's class-3 mass and its obvious first target —
  where the real body is 12 bytes and returns `true`.** Aggregate inflation is
  only 0.7%, so it never moves a headline; it just manufactures the fattest-looking
  prize in the unit you were about to open. `default/Shader` **falls out of the
  top 25 entirely** when re-ranked on asm extents. (`ac6d4d62`, generalised
  `d91954af`)
- **A source line-count proxy oversized the body-write surface ~10×**
  (52,012 B → 5,348 B). (`d91954af`)
- **A census that inflated itself by not subtracting shipped aliases** — three
  separate instances (top-150 split 80/20/50 → 64/49/37; a `.get("symbols")`
  read that forgave nothing, inflating 955 → 1,670 pairs; 190 → 174). Two were
  caught **only** because they contradicted another lane's published number.
  (`53f97dc9`, `5a0a26f2`, `abdbfd6b`)
- **`collect()` is LAST-WINS across duplicate COMDATs** — `?PreInit@Rnd@@` is
  defined in **nine** of our objs, so on a partially-built tree the tool returned
  a stale copy and **reported a landed source edit as inert**. *An obj newer than
  its source was still stale.* (`abdbfd6b`)
- **A dashboard % over a denominator with zero numerator** — 649,620 B of
  UNPINNED functions contributing exactly zero. And `scope_map.json`'s `matched`
  is the **arg-blind `mpn`** predicate while `matched_code` keys on `fuzzy`: the
  bucket breakdown was optimistic **by ~44×**. (`7c3b425e`)
- **A label that measured something other than what it said** — `OK (grounded)`
  meant **map-consistency**, not proof of folding, while carrying essentially all
  720,992 B. Proved non-load-bearing mechanically: **eight groups emptied of
  every folded spelling still land in that bucket and the count did not move.**
  Renamed `OK (MAP-CONSISTENT)`. (`f4e26fcc`)
- **A stale docstring describing a state the tree had already left** — the
  alias gate refused `??2@YAPAXI@Z` on a 12-byte body that is **8 bytes today**;
  the lever had been harvested nine commits earlier. (`5e2eb07b`)
- **A prose comment containing `#if` broke the native link.** `ScatterIncludes.cmake`
  matched `#if` anywhere on a line, so a comment-only `docs(src)` commit pushed
  an unmatched `HX_NATIVE` frame, reclassifying an `#include "math/mtx.cpp"` 212
  lines below into **the one bucket the module ignores in silence** ⇒ 17
  duplicate definitions. (`c61c4a32`)
- **A false pairing reads as a hard function.** `HasPart` at **fuzzy 53.2** was
  never hard — it was being diffed against retail's `Rank`. Inverse at the other
  end: a **"99.8%"** row whose instruction agreement was **7/9 = 77.8%** over a
  categorically wrong pairing. ⇒ *a mid-50s row can be a pairing bug; check that
  before grinding it.* (`8ea4be91`, `5a2ce6ba`)

### 5c. Screens that cannot fail (or nearly)

- **Full-signature uniqueness killed 0 of 100.** A signature that includes
  relocation names describes exactly what ICF folded away, so post-ICF it is
  **near-tautological**. ⚠ It is **not** a dead gate — a base-rate control shows
  **2.65% (1,588 of 60,009)** retail body signatures are multiply held, so it
  *can* fire, **and running that control is what made the 0 worth reporting.**
  Uniqueness on the **masked** body refuted **80 of 100**. (`554a15ac`)
- **An FP calibration scored against a stratum where our build folds by
  construction.** TEMPLATE-1's 5.8% (C)-gate FP was measured over **T1 alias
  pairs**, so it never sampled the population where our own codegen breaks the
  fold. INCOMPLETE-1 answered it by drawing 2,488 decoys **from the population
  being judged** (decoy 0/2,488 = 0.00% vs treatment 205/373 = 54.96%).
  (indicted by CVEIN-1 in `40db9762`; answered `5f44f30f`)
- **A transposition licence that cannot discriminate identical bodies.**
  MPNGAP-1 licensed landings on "both callees are `fuzzy == 100`, so their map
  names are validated by body match" — but an identical body matches at *either*
  address, so the assignment is a coin flip and "fixing" it lifts the metric by
  construction. **3 of 7 payable rows REFUSED; re-priced 2,020 B → 656 B.**
  (`3eb85dfd`)
- **A naive holdout that lied by 5×** because it sampled rows the map already
  names — the opposite of the population being named (0.52% → 2.76%; the gate
  actually used measures 0.33%). And **the retail-side multiplicity guard is
  structurally blind to its own dominant error mode**: `/OPT:ICF` has already
  folded an ambiguous group to one address, so **`retail_mult == 1` is exactly
  what a folded group looks like**. (`65154fb0`)
- **A tautology returning CORROBORATED for every pair** — `same(rn, rn)` hit an
  `rn == on ⇒ SAME` shortcut, producing **659 phantom rows**; removing it
  **flipped the headline class outright**. (`5a0a26f2`)
- **The right null that measured nothing** — 7 closed 2-cycles scored **0.000
  over 200 trials** against a random-rewiring null, which cannot close a cycle
  under *any* hypothesis. The **untreated-population** control settled it and
  killed the lever. (`b574f653`)
- **A control whose population is defined by the absence of what it measures** —
  shuffled trusted-stratum leg **0/86**, i.e. it cannot fire. Same lane shipped
  an `evaluate()` that takes an `fprs` argument and **never reads it**, and a
  subcommand that was **dead, not degraded** (it crashed before analysing
  anything). (`a869018a`)
- **"Does the map independently place F elsewhere" is structurally vacuous** —
  the map is **address-keyed**, so the answer is always "absent". No-evidence,
  not clearance. (`fa79f273`, `5a2ce6ba`)
- **`none`-flat is the ALIAS-SUSPECT signature, not a clearance** — the `none`
  ruler is structurally incapable of clearing an alias. The prescribed answer is
  a **pre-registered row-level prediction**, matched to the byte.

### 5d. Classifiers that understate obstruction

- ⛔ **A short-circuiting gate reports the FIRST gate that fired, not the full
  obstruction set — a bucket of "60" was really 19.** Re-evaluated
  non-short-circuited, **41 of the 60 are also residency-blocked, and 41 of 41
  have `addr(F) ≠ addr(S)`** — retail keeping two addresses *is* the definition
  of not folded. (`efebb9c6`)
- **Flat T1 as a *verdict* understates provability by 27 pp** (55.5% → 82.51%).
  Its vacuity guard is right as a guard and wrong as a verdict. (`f4e26fcc`)
- **A pricing denominator that hid its own control** — it counted sites
  `name_check` never charges, so a **1-of-1** row read as *"1 of 9, cannot
  cross"* — and that row was the lane's control. Realisable total moved **200 B
  → 3,468 B**: ★ **the bug was hiding candidates, not inventing them — the
  direction that closes veins silently.** (`c56c6e6f`)
- **A tier tag that was library-group membership, not a property of the file** —
  10 rows wrong, and hand-correcting them would have drifted straight back.
  **One of the reported mis-tags was the classifier** (`ZLibCompression.cpp`
  matched `/zlib/` against the *directory name*). (`e629a7f8`)

### 5e. Instruments that fail toward MANUFACTURING work — these damage correct code

Everything above fails toward *finding nothing*, which wastes a lane. These fail
the other way.

- **REGORDER-1 `6f00c6e6` had four, in the direction that gets correct code
  deleted:**
  - a **mis-transcribed VA** — `2188758400` read as `0x82756B40` instead of
    `0x8275CD80` (verified: `hex(2188758400) == 0x8275cd80`) — made the scanner
    answer **"NOT REGISTERED ANYWHERE IN RETAIL" for ALL 19 queries, including
    FOUR PROVEN POSITIVE CONTROLS.** That is precisely what a lane about to
    delete registrations wants to hear, and **only the controls caught it.**
  - **keying retail slots on symbol NAMES** dropped every unnamed slot and
    reported surplus registrations that do not exist;
  - comparing retail literals to **class names** invented ~20 phantom defects in
    one function (retail spells `RndFur` as **"Fur"**);
  - **best-overlap matching** paired retail's `Synth360::Init` against our
    `Synth::Init`, inventing a 5-registration defect.
  - ⚠ **And the inverse in the same lane:** treating an unnamed slot as a
    **wildcard** *hid* a real defect a predecessor therefore could not see. The
    fix for the four must not reintroduce this.
- **A screen that would have "found" half a stratum as a phantom source vein** —
  the retail-vs-ours relocation walk bottoms out at a **shared allocator edge**
  identical for every `T`, so it **reports a global constant as a per-pair
  verdict**, calling **51.4%** of retail-byte-proven folds DIFFERENT. Fix:
  compare our build against *itself*, so the allocator cancels. (`5a0a26f2`)
- **A discriminator that made "fix it" mean "break it"** — `relocs_agree`
  compares relocation target *names*, so for two template instantiations over
  layout-compatible types `NOT-T_F` is **true by construction**, routing folds
  into SOURCE_DEFECT. (`1ad913dc`)
- **Two classifier false positives that FABRICATED hits** — walking back from
  the param `(` made every `operator delete` / `` `vbase dtor' `` pair compare
  equal; an `rfind("__cdecl")` anchor read a mangled outline wrapper as a real
  function of that name and matched a real target. (`5b9778f3`)
- **A stale header comment that nearly manufactured a false contradiction
  against a correct alias** — `BeatMatchController`'s `// 0x2c` is stale by
  `+0xC`; the compiler puts `mHitSink` at `+0x38`. **That trap nearly fired.**
  (`5a2ce6ba`)
- **Two "independent" channels that were not — the only instrument failure this
  session that actually LANDED.** Pre-registered +224 B; **measured −1,248 B, 0
  crossed, seven template instantiations fell off 100.** Root cause: it read a
  callee **through** a map name and called two dependent channels independent.
  Reverted. (`f5df4fff`)
- **A wrong map name manufactured a phantom function — twice.** A prior lane
  concluded a 4-arg body "lives outside this object" and **wrote an `HX_NATIVE`
  stub for it**; that body **is** the function. (`3a1af7e3`, `5b9778f3`)
- **Substring-editing a signature inside a mangled name** left 7 function-local
  statics carrying an MSVC back-reference suffix ⇒ measured **−540 B instead of
  +1,204** (−1,744 + 1,204). ⇒ **mangled names are context-dependent; never
  substring-edit a signature inside one.** (`3a1af7e3`)
- **A "correct" fix that lowers the score** — `RndFlare::CalcScale` goes
  **99.08 → 95.3 when the literal is spelled correctly**: the wrong constant
  incidentally suppressed a CSE retail also does not perform. And the naive
  screen for that class is **90.6% FP**. (`dffb0454`)

### 5f. Prices that are structurally uncollectable

**`matched_code` is all-or-nothing per row**, so a headline byte figure can be
uncollectable *in principle*:

- `DataNode::operator==` — a 464 B row that pays **ZERO**. "Do NOT brief it as a
  +464 B prize." (`9544fa4c`)
- RESIDUAL-2 closed **2 of 3** sites for **+0 B**. (`1f078361`)
- BODYPORT-5's structural half: after exclusions **no row in 47,820 B is within
  reach of 100 — the whole vein is Δ0 bytes**. (`0dfc1ec3`)
- SIGSCAN-1's biggest apparent prizes (5,768 B, 3,564 B) are **1 of 274** and
  **1 of 122** charged sites. (`5b9778f3`)

### 5g. Controls that fired correctly — worth knowing, so they are not re-audited

- **The native gate is SOUND.** Reproduced in one worktree, one variable:
  `b81c03b8` **PASS 18/18** vs `0dfc1ec3` **FAIL 16/18** ⇒ it discriminates. The
  PASS leg is the control. (Its *report* was improved: a pure link failure
  printed a blank target list and matched 0 of 34 `multiple definition` lines
  because it grepped for `error:`.) (`c61c4a32`)
- **PINHOME-1's pre-`b29d4c05` `ab_measure` run was audited on four channels and
  is clean** — the run-dir collision did not touch it. (`8e6eb9be`)

---

## 6. Adjudication rules that survived

1. **`matched_code` is all-or-nothing per row** ⇒ rank by **size-if-it-crosses**,
   not by penalty count. And **price from asm extents, never from `report.json`
   sizes** — one function billed **8,852 B** has a **12-byte** body (§5b).
2. **Price a candidate from `report.json`'s charged-site list**, never from a
   `run_objdiff` mismatch count — that count is ruler-dependent and undercounts
   (§5b).
3. ★ **COMPENSATING PAIRS are the paying class.** Both sides wrong in
   mirror-image ways, so the bodies read 100 while call sites are charged, and
   **fixing either side alone REGRESSES.** The map-vs-source screen cannot
   express this third category. GameGem's 0x12 bit-getter ladder: the map named
   the run one position early, our header ended the 0x10 group with
   `mRealGuitar` instead of starting the 0x12 group with it; fixed together it
   pays **+2,676 B** (within a commit measuring +3,584 B). ⇒ **Before repairing
   any map row, ask what our own side does with it. The compensating pair is not
   rare here; it is the modal shape of the big rows.**
4. **Never alias a real bug; never "fix" source to satisfy a fold.** Both
   directions of the metric can be gamed, and this session found live instances
   of each (§4's DO-NOT list, §5e).
5. **Adjudicate on retail bytes — but a SHAPE argument from retail bytes is
   weaker than an IDENTITY one.** On `?SyncProperty@Tour@@`, retail's bytes are
   the bare unknown-property fallthrough with no property comparisons, which
   **reads as a decisive map defect**. It is not: rb3-Wii's independent
   `Tour.cpp` has that exact trivial body, so Tour syncs no properties and
   `Object::SyncProperty` ICF-folded into it. **Only the oracle settled it.**
   The retail-byte rule is right and **not sufficient alone**. (13 rows /
   9,884 B hung on this.)
6. ★ **A retirement is only valid on the tree it was measured on.**
   `?Handle@CustomizePanel@@` was correctly retired at "closing all three
   instructions buys ZERO bytes" — true *when priced*, because two ICF aliases
   were still charging the row. Other lanes landed both aliases, and the row
   **un-retired without anyone touching it.** When inheriting a documented
   negative, **re-measure the price even when you trust the diagnosis.**
7. **A map repair pays on TWO channels** — the call sites *and* the row's own
   re-pairing. GAMEROW-2's +5,236 B decomposes as +5,228 (call site,
   `name_check`-only) + 8 (the thunk's own re-pairing), which is why the `none`
   control moved by exactly +8.
8. **A charged pair names two symbols and the defect need not be either.**
   Inside a masked template class, body evidence **cannot constrain the
   name/address assignment at all** — the defect is often on a third symbol.
9. **Bodies equal under the same name ⇒ the map name is right.** The sound
   converse of a screen already proven vacuous in the other direction. But
   **a 100% row is NOT evidence a member type or template argument is right** —
   `insert_unique`/`_M_insert` are T-independent and score 100 against the wrong
   `T` by construction. The discriminator for `_Rb_tree<K,V>` is **node
   allocation size** (`li r3, 0x10 + sizeof(pair<const K,V>)`), and because
   different-size COMDATs cannot fold, that one immediate kills the fold
   hypothesis too.
10. **Pin neutrality is scoped to REATTRIBUTION.** Pinning previously-unpinned
    code is Δ exactly 0. **Re-homing** an already-pinned row from a unit whose
    obj cannot define its symbol to one that can is **not** neutral — measured
    **+3 fns / +428 B** — because objdiff pairs by name *within a unit*, so the
    row gains a base counterpart for the first time. ⚠ The unscoped claim lives
    in `PIN_WAVES_AND_DENOMINATOR_2026-08-09.md:22`; **CLAUDE.md carries neither
    half.**
11. **Run positive controls BEFORE acting on an absence, not after** (§5e), and
    **before believing any clean number, demonstrate the check CAN fire**
    (§5a, §5c). A shrinking census looks identical to good news.
12. **A predecessor's site note is a HYPOTHESIS, not a finding.** Five were
    relayed into briefs unverified and refuted by the receiving lane this
    session — every one caught because the lane was briefed to **test** the
    premise rather than execute it. **Keep briefing that way.**

---

## 7. The mechanism catalogue — eight causes, every one invisible to a source diff

Found by adjudicating retail bytes on rows that looked like scheduling noise.

1. **Code retail never had** — post-ship Harmonix fixes imported from
   **dc3-decomp (newer than RB3)**, or **rb3-Wii dev-build** code retail
   compiled out. Instances: an 8-slot recursion-safe prop-path pool; three
   `(angle != angle)` NaN guards; a dev-only `sDisableEyeClamping` read; a
   `str.empty()` early-out; a `LoadSafely+bool` test; an `sRandomOverride` hook;
   a `gNumHeaps` guard on `MemPushTemp`/`MemPopTemp`. **Whole functions too:**
   retail has **no `DataNode::Equal`** — `Equal(n,a,warn)` is a later refactor,
   and the comparison is inline in `operator==`.
2. **Declaration-point displacement** — MSVC emits a local static's guarded
   initializer **at its declaration point**; 17 hoisted `static Symbol`s pushed
   retail's opening block from instructions 4–18 to 196–208. ⛔ Censused as a
   one-off — *check*, don't sweep.
3. **Same value, different expression** — retail read the **parameter**
   `&color`; we read **the copy we had just stored**. Fixing it collapsed all 11
   mismatches to 68/68 equal.
4. **Flag polarity** — retail holds `dot > 0.0f` positive; two branch-opcode
   mismatches dissolved from a rename.
5. **Storage-class divergence** — retail uses function-local statics where our
   `Symbols.h` has globals.
6. **`/fp:fast` barriers** — parens are one; **a block-local named temp is
   another** (`sel * -eyeRot` reassociates to `-(sel * eyeRot)`).
7. **We inherit defects from the ORACLE** — five `if (mPreviewDesc)` guards that
   **rb3-Wii carries verbatim** while retail dereferences straight through.
   ⇒ ★ **oracle agreement is NOT clearance; only retail bytes catch this class.**
   ⚠ And the converse is also live (§6 rule 5): on `SyncProperty@Tour` only the
   **oracle** could settle what retail's bytes appeared to decide.
8. **The dead `this` home is a source-shape oracle** — MSVC `/O1` homes the
   vbase-adjusted `this` of an inlined **member** call into a dead stack slot,
   so its presence/absence witnesses **member vs non-member at an inlined call
   site**. Proven live: routing `CustomizePanel::Handle`'s `in_clothing_state`
   arm through a **file-static non-member** dropped both dead-home instructions
   with nothing else moving — and rb3-Wii spells it a const member, so this is a
   real RB3-360-vs-Wii divergence **neither oracle can see**.
   ⚠ **Its companion rule is now SHARPENED, and the naive form is a
   false-positive generator.** "A trailing `clrlwi rX,rX,24` says the callee
   returned `bool`" — but the mask is **emitted ONLY AT A PHI** (in 3 of 4
   controls it is literally a branch target). Its **absence** is therefore not
   evidence of non-`bool`: MSVC range-analyses a `subfe` result as 0/1 and
   elides every narrowing, including an explicit `(unsigned char)` cast.

★ **When retail shipped a real bug, reproduce it in the match build and keep the
fix under `#ifdef HX_NATIVE`.** Do not delete it — the native runtime is the
stated real goal. Same for dev-only paths and surplus factory registrations:
gate, don't remove.

---

## 8. Corrections this edition makes

**To the first edition of this file:**

1. **"~20 lanes"** → **47 lanes landed on 2026-08-14** (00:15 → 16:25).
2. **The ceiling is re-measured at 62.87%, not 63.10%** — it moved by 0.23 pp in
   one day (§1a).
3. **The `?Handle@CustomizePanel@@` retirement was already corrected once**
   (`8ea4be91`) and has moved **again**: the row is now **fuzzy == mpn ==
   99.92057 with ONE charged site**, not 99.76172 with three, after RESIDUAL-2
   closed two of them (§3b).
4. **"9 instruments that cannot fail"** → **~35 distinct instrument failures**,
   regrouped by failure mode, with two new modes: *classifiers that understate
   obstruction* (§5d) and *prices that are structurally uncollectable* (§5f).
5. **The REGORDER-1 factory-registration row is left as written (17 lists / 5
   differed / 12 identical after repair) but flagged**: the merge headline says
   "4 now exact" and the body says "12 of 17, up from 10", which do not
   reconcile (10 + 4 = 14). Not re-derivable without re-running
   `tools/reglist_diff.py`. **Quote 12; do not quote the chain.**

**To the consolidation brief this lane was given** — six corrections, each
verified against the in-tree record:

6. ⛔ **"~91% of the mpn/fuzzy gap is irreducible" is MPNGAP-1's, and it is an
   unshown round number.** No line of its own decomposition yields 91% (nearest
   derivable: 89.3% or 93.2%). **RULERGAP-1's own measurement gives 83.7%
   irreducible** — realisable upper bound **109,708 B = 16.3% of the gap**.
   Quote RULERGAP-1's figure, and do not attribute 91% to RULERGAP-1. *(Beware
   a same-shaped decoy: ICFPROVE-1's 91.4% is a different quantity — the share
   of charged pairs with a retail-side placeholder.)*
7. ⛔ **"96.9% of the 11.57% figure is Quazal vendor" attaches the ratio to the
   wrong denominator.** 96.9% is of the **209,528 B `UNWRITTEN` sub-class**
   (103 units / 202,900 B), **not** of the 11.57% / 1,194,012 B class-3
   population. Independently corroborated here: `network/quazal` is only
   **13.4%** of the whole anonymous-in-compiled-units stratum, which is what you
   would wrongly conclude from the loose form.
8. ⛔ **"68.3% of the class is Dance Central code" is a share of OWNED
   FUNCTIONS** (66 of 162 units carrying 68.3% of 3,310 owned functions), not of
   units or bytes.
9. ⛔ **"the remaining headroom is ordinary per-row matching — divergence, not
   absence" is half right.** The largest block of the gap-to-ceiling is
   **1,329,264 B (48.1%) of ANONYMOUS rows inside units we already compile** —
   absence of identification. The divergence stratum is **537,656 B**, 2.5×
   smaller (§1b, §2).
10. ⛔ **"a function billed 8,852 B has a 12-byte body" is BODYWRITE-1's finding
    (`ac6d4d62`), generalised into the pricing rule by GRIND-1 (`d91954af`)** —
    it is not DATANODE-1's. DATANODE-1's own rule is a different one: a 464 B
    row can pay **zero** because `matched_code` is all-or-nothing.
11. ⚠ **`TEMPLATE_ARGS` (C) needs its later correction recorded, not its
    original.** The 4.1% / 18,240 B measurement stands; **the reading does
    not** — CVEIN-1 showed (C) is mostly **folds**, our source spelling is
    already right, and **18,240 B was never collectable** (row union 15,752 B;
    fully covered 9,768 B; the per-pair sum double-counts by 2,488 B).

**Bookkeeping corrections worth carrying:**

12. `scripts/symbol_aliases.json` now holds **1,528 groups / 15,284 folded
    spellings** (1,499 → 1,493 → 1,528 across the session).
    ⚠ **`report.json`'s `provenance.map_file_entries = 7174` is objdiff's own
    post-parse bind count — it is NOT an alias-group or spelling count.** Do not
    quote it as one.
13. `scripts/target_symbol_map.json` holds **29,006 rows**, 28,978
    non-placeholder = **41.86% of 69,227 functions** named.
14. **PINHOME-1's scoping corrects `PIN_WAVES_AND_DENOMINATOR_2026-08-09.md:22`,
    not CLAUDE.md.** CLAUDE.md carries neither the original claim nor the
    scoping — a real gap (§6 rule 10).
15. **SPLITBLOCK-1 states its live set both ways** in one commit ("2 were live"
    vs "saturates at 3"). Quote **"2 live rows of 3 named-head runs"**. Its two
    byte figures are different things: **246 blocks / 9,968 B** (tails) vs
    **159 anon-head runs / 23,072 B** (head + tails) — do not add them.
16. **BODYWRITE-1's "725 units / median 820 B" and IDENT-1's "5,363 rows" are
    different populations** (the 725-unit census is 6,973 rows = 5,363 plus
    1,492 rows at `0 < mpn < 100`). And **IDENT-1 landed 20 names for +19
    matched** — the one-off is `??0CamShotFrame@@`, a finding rather than a bad
    name.
17. **PINSRC-1's "+7 matched / +252 B" is only +1 honest** — six of the seven are
    unwind funclets pairing by byte signature.
