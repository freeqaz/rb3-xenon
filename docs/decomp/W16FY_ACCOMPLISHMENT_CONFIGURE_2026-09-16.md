# W16-FY — `Accomplishment::Configure`: 32 function-local static Symbols, `xlast_id`, and a metric that pays 0 for the real work

Date: 2026-09-16 · branch `w16-fy` · worktree off main `37e9cb40`
Target: `?Configure@Accomplishment@@QAAXPAVDataArray@@@Z`, unit `default/Accomplishment`,
source `src/band3/meta_band/Accomplishment.cpp`, retail body at `0x82594ef8`.

## Headline

| measure | baseline | after | delta |
|---|---:|---:|---:|
| row `fuzzy_match_percent` | 35.36068 | **99.99226** | **+64.63 pp** |
| row `match_percent_normalized` | 36.63003 | **99.99226** | +63.36 pp |
| row base size | 1,248 B | 2,592 B | target is 2,584 B |
| whole `matched_functions` | 44,098 | 44,120 | **+22** |
| whole `matched_code` | 4,162,044 | 4,162,748 | **+704 B** |
| whole `matched_code_percent` | 40.616930 | 40.623795 | +0.006865 |
| whole `masked_equal_functions` | 23,289 | 23,311 | +22 |
| whole `fuzzy_match_percent` | 50.446686 | 50.468063 | +0.021377 |

All figures measured in this lane's own worktree, full `./tools/ninja-locked` build,
read by exact key from `build/45410914/report.json` on the shipped **graded
`name_check`** ruler (`provenance.diff_config` confirms `functionRelocDiffs=name_check`).
A revert-and-rebuild in the same worktree reproduced the baseline **exactly**
(44,098 / 4,162,044), so the delta is 100% attributable to the source change.

## ⚠ The most important result is the one that contradicts the headline

`tools/icf_alias_check.py --worktree … --baseline-report …` on the newly-matched set:

```
matched (100%): 22
  REAL-BODIED :    0  (0.0%)
  STUB-FOLD   :   22  (100.0%)
  CONTENT-FOLD:   22  (100.0%)  <- paired by masked BYTE SIGNATURE  [22 of them <= 44B]
  NAME-PAIRED :    0  (0.0%)
VERDICT: ICF-ALIAS INFLATION (22 stub-folds of 22) -- zero real-bodied anchors
exit 1
```

**Every one of the +704 B is bookkeeping.** The 22 rows are a contiguous run
`fn_82595A50 … fn_82595CF0`, step `0x20`, each exactly 32 B — the EH funclets that
exist *only because retail's `Configure` has function-local statics*. Our object
never emitted counterparts, so they sat at `fuzzy` 0; now they pair byte-for-byte.

Conversely **the 2,584 B of genuinely reconstructed body earns 0 bytes**, because
`matched_code` keys on `fuzzy == 100` and the row stops at 99.99226 behind a single
relocation-name charge. This is the RESIDUAL-1 shape in its sharpest form yet: the
metric paid for the side effect and refused to pay for the work.

> ⚠ Instrument trap I walked into and caught: the first run of the honesty audit was
> `… | tail; echo "EXIT=$?"`, which printed `EXIT=0` — **that is `tail`'s exit code**,
> the vacuity CLAUDE.md documents. Redirected to a file and tested `$?` on the next
> line: the real exit is **1**. A verdict line saying INFLATION next to a green rc is
> exactly the shape that gets relayed upstream as a pass.

## What settled temporaries-vs-local-statics (the brief's lead hypothesis was wrong in form)

The brief reasoned: retail emits 32 `??0Symbol@@QAA@PBD@Z` inside the body, and **no
`??_B` static guards were found**, therefore retail builds `Symbol` **temporaries**
from string literals at each call site.

**That inference fails, and the retail bytes say the opposite.** Every one of the 32
ctor sites is wrapped in this shape:

```
lwz  r11, lbl_82DFEE58@l(r30)   ; ONE guard word, shared
clrlwi. r9, r11, 31             ; test this static's bit
bne  .L_…                       ; already constructed -> skip
ori  r11, r11, 0x1              ; set the bit
stw  r11, lbl_82DFEE58@l(r30)
mr   r3, r29                    ; &the static Symbol slot
addi r4, r11, lbl_820A3FAC@l    ; the string literal
bl   fn_827C0728                ; Symbol::Symbol(const char*)
```

Successive sites test `0x1, 0x2, 0x4, … 0x80000000`. Measured over all 32 sites the
**union of the guard bits is `0xFFFFFFFF`, popcount exactly 32** — a single guard word
fully consumed. That packed bitfield *is* MSVC's function-local-static guard: MSVC
packs up to 32 local statics per guard int, one bit each, which is precisely why no
per-variable `??_B` symbol appears. **A temporary needs no guard at all**, so 32
distinct guard bits refute the temporaries reading outright.

Storage slots descend `0x82DFEE54, 0x82DFEE50, 0x82DFEE4C, …`, one 4-byte `Symbol`
per static, confirming real static storage rather than stack temporaries.

**Independent second witness, from a signal that shares no arithmetic with the first:**
the 22 newly-pairing 32-byte EH funclets above. Local statics inside an `/EHsc`
function generate exactly this funclet population; temporaries do not. Two unrelated
instruments agree.

⇒ the fix is 32 function-local `static Symbol`, the `BandSongMetadata.cpp` (W16-FU)
house shape. **Order is load-bearing** — MSVC assigns guard bits in order of first
use, so the declarations must stay in retail's sequence.

## The five source defects

1. **32 `Symbols*.h` extern globals → 32 function-local `static Symbol`.** A
   pre-interned global emits no ctor in the body; its ctor lives in a `??__E` in
   another TU. This alone is ~350 of the 1,336 missing bytes.
2. **Missing field `xlast_id` → `mContextId`** (`this+0x84`), `FindData(int)`,
   positioned between `leaderboard` and `gamerpic_reward`. `mContextId` was already
   in our header, ctor-initialised to `-1`, with a `GetContextID()` accessor —
   `Configure` simply never wrote it.
3. **`gamerpic_reward` / `avatarasset_reward` were the wrong shape.** They were
   `FindData(int)`; retail does `FindArray` + an explicit `Node(1).Type() == kDataInt`
   check + `Node(1).Int()` with the default `NULL` source, storing to `0x74` / `0x78`.
   The comment already in the file described this shape correctly while the code
   under it did something else.
4. **Removed all three `reserve()` calls** — retail has none, in any of the three loops.
5. **`bool noMsgChannel = …; if (noMsgChannel)` materialised the bool**
   (`clrlwi` + `cntlzw` + `extrwi.` + inverted branch) where retail tests the
   `Symbol::operator==(const char*)` result directly (`clrlwi.` + `bne`).
   Inlining the condition moved the row **99.66718 → 99.99226**.

Items 1–4 reconcile the callee census *exactly*, which is what promoted this from a
guess to a closed account: `FindArray` 4→6, `DataNode::Int` 1→3, `FindData(int&)`
10→9, and the three base-only `reserve<>` calls disappear.

## The one remaining charge is a PROVEN ICF fold, not a defect

Instruction 43: target calls `push_back<vector<Symbol>>`, we call
`push_back<vector<ControllerType>>`. **Retail's `Configure` calls ONE address
(`0x822D16F8`) from all three loops** — the `ControllerType` loop *and* both `Symbol`
loops. Retail folded them.

Adjudicated with the repo's own instrument:

```
tools/icf_pair_adjudicate.py --chase \
  --survivor '?push_back@?$vector@VSymbol@@V?$StlNodeAlloc@VSymbol@@@stlpmtx_std@@@stlpmtx_std@@QAAXABVSymbol@@@Z' \
  --ours     '?push_back@?$vector@W4ControllerType@@V?$StlNodeAlloc@W4ControllerType@@@stlpmtx_std@@@stlpmtx_std@@QAAXABW4ControllerType@@@Z'
```

* **FLAT T1: REFUTED** — bodies byte-identical (112 B both) but relocation targets
  differ (per-`T` `_M_insert_overflow_aux`). This is the `_List_base<T>::clear` case
  CLAUDE.md describes: byte-similar bodies at distinct addresses do **not** refute ICF.
* **CHASED T1: PROVEN** — every differing slot resolves to a callee pair that itself
  folds (`_M_insert_overflow_aux`, `__uninitialized_copy`, `__uninitialized_fill_n`,
  allocator chain all `SLOT-FOLD-OK`).
* `--chasetest` control suite: **"selftest PASSED — the instrument can both pass and
  fail"** (REFUTED on the negative, PROVEN on the positive). The PASS is therefore a
  measurement, not the detector restating its input.

**No alias was added, deliberately.** `scripts/symbol_aliases.json` (1,658 groups) has
no group for this survivor. Closing it is worth the row's full **2,584 B**, but:
(a) an alias lifts the score *by construction*, so it is an integrity-sensitive edit
with its own mature tooling and four gate tests; (b) bundling it with source changes
makes the A/B uninterpretable — `ab_measure`'s `control_none_shape()` fires
`ALIAS_SUSPECT` only on *map-only* patches, so bundling defeats the guard. It belongs
in its own lane, with its own measurement. The adjudication above is the whole input
that lane needs.

## Verified, and deliberately not done

* **Verified:** the in-source `0x82594EF8` address comment is **correct** against
  `scripts/target_symbol_map.json` — unlike the neighbouring class flagged yesterday,
  this one was not stale.
* **Verified with a presence control:** every string literal was resolved out of
  `orig/45410914/band.exe` by parsing PE section headers; the first four resolve to
  the four field names our source already reads, so an absence would have been
  meaningful.
* **Not done:** the `push_back` ICF alias (above).
* **Not done:** the other 18 unmatched rows in `default/Accomplishment` (unit is
  71/89, `fuzzy` 84.4275). Out of scope for this lane.
* **rb3-Wii oracle was the defect again, twice.** It uses the `Symbols*.h` globals
  (wrong shape for retail-360) and it never reads `xlast_id` at all, though it
  declares it in `Symbols.h`. Our source was *verbatim* the oracle for both and was
  wrong both times. That is the fifth-plus instance in two days of "the oracle is the
  defect"; treat oracle text as a hypothesis, retail bytes as the ruler.

## Scanner traps hit in this lane (all caught, all cheap to re-hit)

1. `.fn` labels in `build/45410914/asm/*.s` use **UPPERCASE** hex (`fn_82594EF8`)
   while `scripts/target_symbol_map.json` keys are **lowercase** (`0x82594ef8`). A
   case-sensitive regex returns a clean `None` — a screen that cannot fire.
2. `command grep -c "static Symbol"` reported **33** declarations where there are
   **32** — the 33rd hit was the phrase inside my own explanatory comment. A count is
   not the thing you think it is; re-derive with a real pattern
   (`^\s*static Symbol (\w+)\("…"\);`) before trusting it.
3. The `| tail; echo $?` vacuity (above).
