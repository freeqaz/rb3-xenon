# W16-AX — EventAnim re-homes, the TimeConversion carve (refuted), doubled headings (closed)

Lane W16-AX. Worktree `~/tmp/wt-w16-ax`, branch `w16-ax`, based on main `ba734115`.
Files owned and touched: `config/45410914/splits.txt` only. No `src/`, no
`scripts/target_symbol_map.json`, no `scripts/symbol_aliases.json`.

**Headline: +2 functions / +296 B, 0 rows fell out, `total_code` unchanged.**
Two of the three briefed items turned out to rest on premises the in-tree record
and retail bytes refute; the lane's most valuable output is arguably the
refutations and one hard map defect, not the bytes.

| measure | before (`ba734115`) | after (`1fd90d38`) | delta |
|---|---:|---:|---:|
| `matched_functions` | 43,485 | 43,487 | **+2** |
| `matched_code` | 4,032,352 B | 4,032,648 B | **+296 B** |
| `matched_code_percent` | 39.355362 | 39.358253 | +0.002891 pp |
| `fuzzy_match_percent` | 49.661087 | 49.665497 | +0.004410 pp |
| `total_code` | 10,246,004 | 10,246,004 | 0 (pure reattribution) |
| `total_functions` | 69,217 | 69,217 | 0 |

Ruler `name_check` (read from `report.json` `provenance.diff_config`), objdiff
`5a51cd51fe0a353f`, tool commit `a5f0ea903ec1`. Baseline reproduced the brief's
figures exactly before any edit.

## 0. Instrument setup (why the numbers are trustworthy)

A fresh worktree's reflinked target objs are **pre-renamer**, so every retail
mangled name reads "absent" until the tree is built — a vacuity that agrees with
whatever you expect. Before reading a single COFF symbol:

- full build to rc=0, then a second build doing **zero compile edges** (only the
  always-run validators/report) — settled.
- `[renamed-check] 25819/29363 map names present in 3114 target objs = 87.9%`.
- independent assertion: **30,052** distinct mangled names across the 3,114
  target objs, above the 27,000 bar. Negatives on COFF lookups are therefore
  real negatives.

Every "which obj defines this name" answer below comes from a COFF symbol-table
read that distinguishes **defined** (section number > 0) from a mere
**undefined reference** (section 0) — a reference does not pair.

One instrument was built and then **discarded as invalid**: reconstructing a
row's absolute address by walking the unit's `.text` blocks in splits order and
mapping `report.json`'s cumulative `address` offset onto them. It placed
`ObjList<EventCall>::operator=` at `0x824c9618` where the map says `0x824c9ab8`,
and the unit's blocks total 43,248 B against 42,256 B of rows. Block-order
concatenation is not a valid address model here. **All ownership conclusions in
this document are keyed on the symbol NAME**, never on a reconstructed or
`.s`-column address — the trap CLAUDE.md records for multi-block units.

## 1. EventAnim / EventTrigger re-homes — +2 fns / +296 B

### 1.1 The shape of the defect

`EventTrigger.cpp:`'s blocks are **interleaved inside** `EventAnim.cpp:`'s
address span, leaving EventAnim two gaps:

- gap 1 `0x824C95F0–0x824C9918` — EventTrigger holds three blocks
- gap 2 `0x824C99F4–0x824C9B28` — EventTrigger holds two blocks

Both units use **bare** headings (`EventTrigger.cpp:`, `EventAnim.cpp:`).

### 1.2 Inventory — map name x defining obj

| address | size | map name (abbreviated) | EventAnim.obj | EventTrigger.obj | verdict |
|---|---:|---|---|---|---|
| `0x824C95F0` | 488 B block | **no map row** — 3 anonymous fns + 8 B EH prefix | — | — | unprovable, left |
| `0x824c97d8` | 116 B | `list<ProxyCall@EventTrigger>::list(...)` | absent | **DEFINED** | see §1.5 — map is WRONG |
| `0x824c9878` | 156 B | `_M_splice_insert_dispatch<EventCall@EventAnim>` | **DEFINED** | absent | **move** |
| `0x824c99f8` | 188 B | `list<EventCall@EventAnim>::operator=` | **DEFINED** | absent | **move** |
| `0x824c9ab8` | 108 B | `ObjList<EventCall@EventAnim>::operator=` | **DEFINED** | absent | **move** |
| `0x824c9b78` | — | `RndAnimatable::operator delete` | DEFINED | DEFINED | already in EventAnim's own block; no action |

Each of the three moved names appears **exactly once** in the whole report, in
`default/EventTrigger`, at **fuzzy 0** — unpairable by construction, because
`EventTrigger.obj` cannot define an EventAnim symbol. That is the defect: not a
source divergence, a wrong home.

The brief predicted one move; the inventory found **three**.

### 1.3 The moves, predicted and measured

| commit | range | size | predicted | measured |
|---|---|---:|---|---|
| `39a33838` | `0x824C9878–0x824C9914` | 156 B | +1 fn / +156 B | **+0 / +0** — row went 0 → fuzzy **99.74359** |
| `53cf9167` | `0x824C99F8–0x824C9AB4` | 188 B | Δ0 likely (reasoning from 1a) | **+1 fn / +188 B** |
| `1fd90d38` | `0x824C9AB8–0x824C9B24` | 108 B | +1 fn / +108 B | **+1 fn / +108 B**, exact |

Two of three predictions were wrong, in **opposite** directions — worth recording
as calibration. 1a was wrong about the prize, not the home: the row moved from
unpairable to a 99.74 near-match, and `matched_code` is all-or-nothing per row,
so a 99.74 row pays nothing. 1b was wrong the favourable way: I generalised 1a's
fold-alias residual to a body that in fact agrees with retail exactly.

`EventTrigger`'s denominator shrank exactly as it should at each step
(338 → 337 rows, 42,256 → 42,100 B after 1a). Set-diff named the two crossing
rows and reported **FELL OUT: 0 rows** at every step.

### 1.4 The `.pdata` cycle (expected, not a failure)

Each move's first build returned **rc=1** with the split-guard reporting *"THE
SPLIT REWROTE ITS OWN INPUT"*. This is the documented behaviour, not a defect:
`.pdata` is **derived output**, re-derived once per `.text` block on every split
run. Moving a `.text` line changes which TU owns the function, so dtk re-derived
the matching `.pdata` range (for 1a: `0x82213E18–0x82213E20`) onto EventAnim by
itself. The second build is the fixed point (rc=0) and a third build does zero
work. I hand-edited **no** `.pdata` line at any point.

### 1.5 The residual on `0x824c9878`, and a hard map defect

After the move the splice row sits at **fuzzy 99.74359 / mpn 99.74359** with
**exactly 2 charged sites**, both relocation-name `bl` arguments (37 of 39
instructions equal). Priced from `report.json`'s charged-site list, not from a
mismatch count.

**Site [8]** — target `??$?0H@?$StlNodeAlloc@V?$_List_node@H@...`, base
`get_allocator<_List_base<EventCall@EventAnim>>`. Both bodies are **4 bytes,
`4e800020` (a bare `blr`), with zero relocations**, and the target spelling is
defined identically in **18** of our objs. Identical bytes with no relocations is
exactly the condition MSVC's `/OPT:ICF` folds on — and exactly the
"relocation-free thunk" class GROUNDED-1 records as **irreducible**: the fold is
provable, but which spelling the call site meant was destroyed by ICF itself.

**Site [13]** — target `list<ProxyCall@EventTrigger>::list(...)` (i.e. the symbol
at `0x824c97d8`), base `list<EventCall@EventAnim>::list(...)`. objdiff labels
this `TEMPLATE_INSTANTIATION_MISMATCH`/`LikelyFixable`. **It is neither a fold
nor a source defect — the map row is wrong**, and that is settled on retail
bytes:

> Retail's body at `0x824c97d8` (target obj section `/22`, size 116 = the report
> row size) carries three relocations: `__savegprlr_28` at `+0x4`,
> `__restgprlr_28` at `+0x70`, and at `+0x58` a call to
> **`?insert@?$list@VEventCall@EventAnim@@...`** — the **EventAnim** instantiation
> of `list::insert`.

A `list<ProxyCall@EventTrigger>` range-constructor cannot call
`list<EventCall@EventAnim>::insert`. So `0x824c97d8` **is** EventAnim's
instantiation, misnamed as the EventTrigger twin. This is the MPNGAP-1
adjudication method — *does the named callee's signature match the call site?* —
returning a clean answer, and it is why the row reads 99.83 rather than 100:
two of its three relocations agree and one does not.

It is **not** an ICF fold, and this matters because the detector's label invites
that reading. The two instantiations are byte-identical in code, but their
relocation **targets** differ (per-`T` `_List_base` dtor, per-`T` `list::insert`,
per-`T` `__ehfuncinfo$`). MSVC folds only COMDATs identical *including*
relocations — this is the documented `_List_base<T>::clear` shape (42 addresses,
reloc-identical surplus 0).

**I did not act on it.** The fix is a map rename plus a companion splits move,
and `scripts/target_symbol_map.json` is W16-AY's file. Filed with full evidence
as `docs/decomp/W16AX_map_proposals.json` (AX-1, AX-2).

⚠ **Ordering hazard, stated explicitly in the proposal:** the two halves must
land together. The rename alone puts an EventAnim-spelled symbol in a unit whose
base obj cannot define it; the splits move alone puts an EventTrigger-spelled
symbol in EventAnim. Either alone takes the row from 99.83 to 0 — no byte loss
(a 99.83 row already contributes 0 `matched_code`) but a strict accuracy
regression. Combined potential for AX-1 + AX-2: **+2 fns / +272 B**.

### 1.6 What I deliberately did NOT move

- **`0x824C97D8–0x824C9878`** — see §1.5. Correct by the map today, provably
  wrong by retail bytes, but unfixable from inside my file ownership.
- **`0x824C95F0–0x824C97D8` (488 B)** — three **anonymous** functions,
  `fn_824C95F8` (388 B), `fn_824C977C` (40 B), `fn_824C97A4` (52 B), plus the
  8-byte EH prefix at the block start: 8+388+40+52 = 488, closing exactly on the
  block size. No map row names any of them, so they **cannot pair in any unit**
  and re-homing them is metric-neutral *and* unproven. Enclosure by the same
  unit on both sides is the heuristic measured at 66.24% precision (33.76% FP) —
  not a proof of membership.
- Checked and dismissed as a rival explanation for that block: **it is not a cut
  function**. `?Save@EventAnim@@` is 172 B from `0x824c9540`, ending `0x824c95ec`
  + 4 B padding = `0x824C95F0` exactly, and reads fuzzy 100. EventAnim's block
  boundary does not bisect anything.

## 2. The TimeConversion carve — PREMISE REFUTED, not executed

**The brief's premise is false, the in-tree record says so in the very sentence
the brief paraphrases, and executing the carve would have had zero upside.**

### 2.1 Inventory of `0x827C8C88–0x827C97C8` (the whole 2,880 B block)

The brief asked whether the block is a mis-pin larger than the TimeConversion
cluster. It is — it holds **three** TUs' worth of code, and the TimeConversion
cluster is larger than the 8 functions briefed (**14** named, not 8):

| sub-range | owner | contents |
|---|---|---|
| `0x827C8C88–0x827C90B8` | StringTable | `UsedSize`, `Size`, `~StringTable`, `StringTable(int)` + 5 anonymous |
| `0x827C90B8–0x827C9470` | **TimeConversion** | `MsToTick`, `MsToBeat`, `TickToMs`, `BeatToMs`, `BeatToTick`, `TickToBeat`, `SecondsToBeat`, `TickToSeconds`, `BeatToSeconds`, `OnSecondsToBeat`, `OnBeatToSeconds`, `OnBeatToMs`, `OnMsToTick`, `TimeConversionInit` (+ anon `fn_827C91A0`) |
| `0x827C9470–0x827C97C8` | **Locale** | `Terminate@Locale`, `FindDataIndex@Locale`, `DataSetLocaleVerboseNotify`, `OrderedLocaleChunk` ctor/dtor, `Localize` (+ 3 anonymous) |

### 2.2 The refutation

`src/system/utl/StringTable.cpp` **deliberately `#include`s** `utl/Locale.cpp`,
`utl/TimeConversion.cpp`, `utl/GlitchFinder.cpp` and `movie/Movie.cpp`, under an
in-source comment that states the mechanism exactly:

> `// COMDAT-scatter owner-TU includes (sw scatter-scan): retail linker`
> `// interleaved these owners' COMDATs into this TU's .text span.`

Consequences, each measured rather than assumed:

1. **`StringTable.obj` defines all 24 names in the block** — the TimeConversion
   ones and the Locale ones.
2. **All 14 named TimeConversion rows are already at `fuzzy == 100.0`**, 884 B
   total. So are all 6 Locale rows. There is **nothing to gain**.
3. The only sub-100 rows in `default/StringTable` are 7 rows / 1,112 B, and every
   one is either **anonymous** (`fn_827C8DE8`, `fn_827C8E50`, `fn_827C8EF8`,
   `fn_827C91A0`, `fn_827C9540`, `fn_827C96D8` — placeholder names, unpairable in
   *any* unit) or **`??0GlitchFinder@@QAA@XZ`** (340 B, which is the unit's
   *other* block at `0x827C2780` entirely). No TimeConversion row can gain.
4. The pattern is not a one-off: **170 source files / 252 scatter-include
   directives** use it tree-wide. Carving TimeConversion out would be
   inconsistent with 170 files.
5. Direct measurement of the risk: all **14 / 14** TimeConversion COMDAT bodies
   are **byte-identical AND relocation-target-name-identical** between
   `StringTable.obj` and `TimeConversion.obj`. So the carve would be **Δ0** —
   safe, and pointless.

### 2.3 The in-tree record already said this

W16-AT §6, which the brief cites, reads (emphasis added):

> "The TimeConversion cluster (`0x827C90B8`–`0x827C9370`) sits inside
> `StringTable`'s `.text` pin, so `TimeConversion.cpp`'s functions are scored
> against `StringTable.obj` **(they still match because both TUs end up in that
> base obj through the pairing)**. A splits re-home would be cleaner."

The brief dropped the parenthetical and re-framed *"would be cleaner"* — a
cosmetic preference — as a defect. This is the standing
**"read the in-tree record first"** failure mode, and it is why I tested the
premise before building on it.

### 2.4 Why "cleaner" is not worth it either

Beyond Δ0: I cannot make the carve *complete*. `StringTable.obj` would **still**
define all 14 symbols, because removing the `#include` is a `src/` edit barred to
this lane. The result would be a redundant configuration — `TimeConversion.obj`
supplying the target pairing while `StringTable.obj` compiles the same 14 bodies —
which is arguably less clean than today, plus a `StringTable` block split in two
and a new heading, for no measured benefit.

**Decision: not carved.** Had I carved it, the honest report would have been "Δ0,
split StringTable's block in three, fought a 170-file mechanism".

### 2.5 What evidence would reverse this

- Any TimeConversion row reading **< 100** that reaches 100 under
  `TimeConversion.obj`. There are none today: the only sub-100 row in the span is
  the anonymous `fn_827C91A0`, and an anonymous target name cannot pair in any
  unit. (W16-AT deliberately left it anonymous; its `SecondsToTick` name is
  convention-derived, not proved, and under `name_check` its call site is already
  forgiven.)
- A decision to remove the scatter-include from `StringTable.cpp` — a `src/`
  change. If that happens the carve becomes **mandatory**, not optional, and the
  884 B depends on it.
- Evidence that the scatter-include mechanism is being retired tree-wide.

## 3. Doubled headings (`UIStats` / `AccomplishmentProgress` / `Game`) — ALREADY FIXED

**Verified closed. The defect does not exist at `ba734115`; CLAUDE.md's
description of it as live is stale.**

Three independent instruments agree:

1. **Heading census** keyed on **full path** (never `basename()`): each unit has
   **exactly one** heading, and it is the path-qualified one —
   `band3/meta_band/UIStats.cpp:`, `band3/meta_band/AccomplishmentProgress.cpp:`,
   `band3/game/Game.cpp:`. **Zero exact-duplicate headings tree-wide.**
2. **`python3 scripts/verify_objs_patched.py --check`** — the tool CLAUDE.md says
   names doubled targets — reports rc=0 and
   **`0 object(s) declared by >1 unit`**, with
   `1051/1051 declared compiled objects pair with a target (100.0%)`
   (`relpath-only would reach 347`, i.e. the PAIRFIX `obj_pairing` rewrite is in
   place) and `tree is a fixed point of 6 post-compile passes`.
3. **The 23 remaining basename collisions are not doublings.** Checked
   explicitly: `0` cases where the same *path* appears twice. `Utl.cpp` ×4
   (`rndobj`/`rnddx9`/`obj`/`synth`), `Movie.cpp` ×2 (`rndobj`/`rnddx9`), the
   `FxSend*` family, etc. are genuinely distinct source files — the documented
   `Movie.obj` collision, not a defect.

**Fixing commit: `b341d7ab`, "splits: merge the three TUs that carried two
headings each", 2026-08-31, freeqaz** — an ancestor of `ba734115`, touching only
`config/45410914/splits.txt` (+52/−58). Its own message records that it derived
the population two independent ways (duplicate `base_path` in `objdiff.json`, and
bare+path-qualified pairs in `splits.txt`), both giving exactly three; that it
kept the **path-qualified** heading so the unit resolves by exact path rather
than through the alias; and that it was a **negative result**:

> `matched_functions 42274 -> 42273 (-1)`, `matched_code 3772560 -> 3772520 (-40 B)`
> — zero rows vanished or appeared, 7 changed score, the whole −40 being one
> 40-byte EH funclet that objdiff pairs by byte signature and which re-paired to a
> different byte-equal counterpart in the larger merged pool.

It was landed anyway, on accuracy. **No action required from this lane.**

A consistency check that my own edits are exactly what I believe: the brief
quoted these headings at `splits.txt` lines 8963 / 8905 / 9800; I measure
8957 / 8899 / 9794, a uniform **−6** = the 3 `.text` + 3 derived `.pdata` lines my
item-1 moves relocated from EventTrigger (which sits earlier in the file) to
EventAnim (later).

## 4. Gates

All run in the worktree, in the brief's order, after the last source change.

```
full build                                   rc=0   (~/tmp/rb3_build_w16ax_11.log)
scripts/verify_ruler_agreement.py --check    rc=0
scripts/verify_objs_patched.py --verify-manifest  rc=0
tools/icf_alias_finder.py --validate         N/A — no alias edits in this lane
tools/native_build_gate.sh                   see line below
```

`NATIVE_GATE_RESULT` line, verbatim:

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

Run after commit `ef0e6695`, no source change since; log `~/tmp/w16ax_native_gate.log`.
Full coverage: 18/18 verified, **0 SKIPs**, rc=0 — the 0-SKIP rule applied, not just the `PASS` word.


## 5. NOT done, and why

- **`0x824C97D8–0x824C9878` not re-homed** — retail bytes prove the map row is
  wrong (§1.5), but the fix needs a `scripts/target_symbol_map.json` rename that
  is W16-AY's file, and the splits half alone is accuracy-negative. Filed as
  proposal AX-1 with the ordering hazard spelled out. **Worth +116 B combined.**
- **No alias installed for the `blr` fold at site [8]** —
  `scripts/symbol_aliases.json` is W16-AY's. Filed as AX-2. An unproven alias
  lifts `name_check` by construction, so I supplied byte-level proof (4-byte
  `blr`, zero relocations, 18 identical definitions) rather than a `none`-ruler
  control, which cannot validate an alias.
- **`0x824C95F0–0x824C97D8` (488 B, 3 anonymous fns) left where it is** — no map
  name, unpairable in any unit, and spatial enclosure is a 66%-precision
  heuristic. Moving it would be metric-neutral and unproven.
- **TimeConversion not carved** — premise refuted (§2), Δ0 measured, and
  incompletable without a `src/` edit I am barred from.
- **The Locale cluster (`0x827C9470–0x827C97C8`) not carved either** — same
  mechanism, same verdict: all 6 named rows already at fuzzy 100 through
  `StringTable.cpp`'s scatter-include of `utl/Locale.cpp`. Reported as an
  inventory finding only.
- **`??0GlitchFinder@@QAA@XZ` (340 B, fuzzy 0) not investigated** — it is
  `StringTable`'s *other* block (`0x827C2780–0x827C28D4`) and a source matter in
  `GlitchFinder.cpp`, outside both my brief and my file ownership. Flagged: it is
  the single largest unmatched named row in the unit.
- **`fn_827C91A0` not named** — W16-AT's reasoning stands (convention-derived
  name, call site already forgiven under `name_check`); naming an anonymous
  address pays in bug exposure, not bytes, and I found no new evidence.
- **CLAUDE.md not edited** — its doubled-heading paragraph is stale (§3), but the
  file is outside this lane's ownership bars. Flagged for the coordinator.
- **No `src/` edits, no map edits, no alias edits, no commits to main, no push,
  no rebase.** No AI attribution trailer on any commit.

## 6. Commits

| sha | subject |
|---|---|
| `39a33838` | splits: re-home `0x824C9878` (splice_insert_dispatch<EventCall@EventAnim>) — predicted +156 B, measured Δ0, landed on accuracy |
| `53cf9167` | splits: re-home `0x824C99F8` (list<EventCall@EventAnim>::operator=) — **+1 fn / +188 B** |
| `1fd90d38` | splits: re-home `0x824C9AB8` (ObjList<EventCall@EventAnim>::operator=) — **+1 fn / +108 B** |
