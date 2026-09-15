# W16-BJ — are W16-BG's 10 "mis-pinned" EH funclets mis-pinned, or ICF-folded survivors correctly homed?

Lane **W16-BJ**, 2026-09-15. Worktree `~/tmp/wt-w16-bj`, branch `w16-bj`, based on main
`9d2ec6df55ee`. Scope: `config/45410914/splits.txt` `.text` lines only, plus one new tool and
this record. **No map row, no alias group, no `src/` file touched.**

Baseline (measured in this worktree, build 1+2, `BUILD rc=0`, zero-work settle on build 2),
every one of the seven briefed figures confirmed literally:

| key | briefed | measured |
|---|---:|---:|
| `matched_functions` | 43,548 | **43,548** |
| `matched_code` | 4,039,828 | **4,039,828** |
| `matched_code_percent` | 39.4242 | **39.424236** |
| `total_functions` | 69,240 | **69,240** |
| `total_code` | 10,247,068 | **10,247,068** |
| `masked_equal_functions` | 23,076 | **23,076** |
| `fuzzy_match_percent` | 49.69595 | **49.69595** |

Ruler `name_check`, objdiff **4.2.9 `5a51cd51fe0a353f`** — both read out of `report.json`'s
`provenance`, not assumed. Renamer liveness (the reflinked-worktree trap): **60,980** mangled
`?…` names across the built target objs, far above the brief's 27,000 floor, so every "absent"
below is a real absence. Row-set baseline proven set-identical to `~/tmp/rows_w16bg_main.json`
(`CROSSED IN 0 / FELL OUT 0`), so this tree is a true main baseline.

---

## 0. The literal test of BG's 9-vs-10 count

BG's prose (§2 note 3 and §6 item 1) says **9 of 24** funclets are pinned to a unit other than
their parent's. **Its own table shows 10** — rows #1, 2, 3, 7, 10, 13, 15, 16, 17, 19.

Recomputed from scratch (parents re-derived from retail bytes, pinned units re-derived from
`splits.txt`, no figure inherited): **10**. BG's table is right and BG's prose undercounts by one.

**Cause, and it is benign:** rows #1 and #2 (`0x822FD26C`, `0x822FD2B0`) share *both* their parent
(`fn_822FC508`) and their pinned unit (`VocalTrackDir`), so deduping by *(parent, pinned unit)*
pair gives 9 distinct pairings for 10 affected funclets. The prose counted pairings and the table
counted rows. No measurement is wrong; the sentence is. **The row count is 10.**

---

## 1. The adjudication: the ICF rival hypothesis is REFUTED

**Expectation I pre-registered before measuring** (and it was wrong, which is the most useful line
in this document): I expected *mostly HOMED*, i.e. BG's filed defect to be largely not a defect.
Reasoning: MSVC emits each `__unwind$N` as its own COMDAT, retail has `/OPT:ICF`, and laneAP
selected these 24 precisely *because* each had an exact relocation-masked byte twin — the most
stereotyped funclet bodies in the tree, hence the population most likely to fold. A fold survivor
is placed with the *surviving* TU's contribution while still being referenced by the *other* TU's
FuncInfo, which reproduces BG's observation exactly under a one-parent attribution.

**Two independent instruments refute it.**

### 1.1 Fan-in is 1, unanimously — and the instrument is proven able to see a fold

All **8,541** `0x19930522` FuncInfos in `orig/45410914/band.exe` (reproducing CLAUDE.md's
independent count exactly), walked for UnwindMap `action` and TryBlockMap → HandlerType
`addressOfHandler`: **25,784** distinct unwind targets + **537** catch targets = **26,321**
funclets, BG's two totals reproduced exactly.

> **Every one of the 26,321 funclet targets has fan-in exactly 1.** No funclet in the image is
> referenced by two FuncInfos. Histogram: `{1: 26321}`.

A fold could still hide from that if the *FuncInfo records themselves* folded, so two parents
shared one FuncInfo VA. Tested by counting 8-byte EH-prefix sites (`{__CxxFrameHandler,
&FuncInfo}`, handler word `0x82829530`; the function starts 4 bytes after the FuncInfo word) per
FuncInfo:

| clean prefix sites per FuncInfo | count |
|---:|---:|
| 1 | 8,003 |
| 2 | 537 |
| 5 | **1** |

The **537** two-site records reconcile **exactly** with the 537 catch targets found independently
by the TryBlockMap walk — they are catch funclets carrying their own prefix, which is BG's note 1.
So 8,540 of 8,541 FuncInfos have exactly one non-funclet parent.

★ **The single 5-site record is the POSITIVE CONTROL, and it is why the absence of folding is a
measurement rather than a blind spot.** FuncInfo `0x820A1EF0` has five parents
(`0x825896C0`, `0x825896E8`, `0x82589710`, `0x825A6000`, `0x82687998`) — and `maxState=1` with
`action=0x00000000`, i.e. a degenerate no-cleanup record owning **zero funclets**. That is exactly
the `.xdata` shape that folds across TUs. The scanner therefore **can** detect FuncInfo folding;
it found the one instance in the image, and that instance touches no funclet. (House rule: a
witness that cannot discriminate cannot clear anything.)

⇒ **Funclet → parent fan-in is 1 for all 26,321 funclets.** Mechanistically coherent: MSVC emits
funclets inside the parent's COMDAT, so a funclet cannot fold independently of its parent, and
parents doing real cleanup work have distinct FuncInfos. **BG's one-parent attribution is complete,
not a first-hit artifact.**

### 1.2 Geometry: every funclet lies inside its own parent's contiguous funclet run

For each of the 24, the distance from the parent's extent end to the funclet start, and what
occupies it:

- `+0` — the funclet immediately abuts the parent's body (11 rows).
- `+8` — a catch funclet's own 8-byte EH prefix (3 rows: #9, #15, #16).
- `+68 … +544` — **filled entirely by other funclets of the SAME parent**, in every single case.
  Not one intervening ordinary function anywhere in the 24. Example #7: parent `0x82450B60`
  is followed by 16 of its own funclets before `0x82451BC0`.

This is precisely W16-BF §3's geometry, where retail's contiguous COMDAT
`0x82270E60–0x822716E8` (parent `App::App` + all eight of its funclets, one FuncInfo) was cut by
`splits.txt` at `0x822715B0`. A funclet sitting inside its parent's contiguous funclet run cannot
have been emitted by a different TU.

⇒ **The filed defect is real. A pin that separates a funclet from its parent is cutting a
contiguous retail COMDAT group mid-run — the BF §5 false-pairing shape, not an ICF artifact.**

---

## 2. Fan-in adjudication table, all 24 (the 14 in-unit rows are the control)

Pinned unit is derived from `splits.txt` `.text` ranges keyed on the **full heading path**
(never `basename()`). **Which answer is authoritative, and why:** `splits.txt` is the *input* that
determines dtk's carving and is the surface I edit; `report.json` unit membership is its *output*.
I validated they are equivalent — **31,383 of 31,383 `fn_<VA>` rows agree, 0 disagreements** — and
use `splits.txt` because it can also answer for funclets that have no report row, which
`report.json` cannot. (First attempt at that validation read 0/59,105 agreement because
`report.json`'s per-row `address` field is a **section-relative offset, not a VA**; that is a
silent vacuity and is now recorded as a blind spot in the tool's docstring.)

`fan` = distinct referencing parents. `gap` = parent extent end → funclet start.

| # | funclet | size | kind | fan | parent | parent unit | pinned unit | gap | fuzzy | mpn | mEq | verdict |
|---:|---|---:|---|---:|---|---|---|---:|---:|---:|:-:|---|
| 1 | `0x822FD26C` | 68 | unwind | 1 | `fn_822FC508` | *(unpinned `auto_*`)* | VocalTrackDir | +0 | 100 | 100 | Y | **ORPHAN** |
| 2 | `0x822FD2B0` | 68 | unwind | 1 | `fn_822FC508` | *(unpinned `auto_*`)* | VocalTrackDir | +68 | 100 | 100 | Y | **ORPHAN** |
| 3 | `0x82318BBC` | 44 | unwind | 1 | `fn_823187F0` | RGUtl | DepthBuffer3D | +400 | 100 | 100 | Y | **MIS-PINNED** |
| 4 | `0x8232380C` | 68 | unwind | 1 | `fn_82323690` | InstrumentDifficultyDisplay | *same* | +0 | 100 | 100 | Y | HOMED |
| 5 | `0x82323850` | 68 | unwind | 1 | `fn_82323690` | InstrumentDifficultyDisplay | *same* | +68 | 100 | 100 | Y | HOMED |
| 6 | `0x8235C974` | 40 | unwind | 1 | `fn_8235C678` | CharClip | *same* | +168 | 100 | 100 | Y | HOMED |
| 7 | `0x82451BC0` | 32 | unwind | 1 | `fn_82450B60` | PartLauncher | AccomplishmentPlayerConditional | +544 | 100 | 100 | Y | **MIS-PINNED** |
| 8 | `0x8246454C` | 40 | unwind | 1 | `fn_824644E0` | Graph | *same* | +0 | 100 | 100 | Y | HOMED |
| 9 | `0x824B1684` | 44 | catch | 1 | `fn_824B14E8` | LightPreset | *same* | +8 | 100 | 100 | Y | HOMED |
| 10 | `0x824F8244` | 44 | unwind | 1 | `fn_824F8208` | RockCentral | Watcher | +0 | 100 | 100 | Y | **MIS-PINNED** |
| 11 | `0x82527B48` | 32 | unwind | 1 | `fn_82527AF0` | VirtualKeyboard | *same* | +0 | 100 | 100 | Y | HOMED |
| 12 | `0x8253CC94` | 40 | unwind | 1 | `fn_8253CBD8` | MusicLibrary | *same* | +0 | 100 | 100 | Y | HOMED |
| 13 | `0x8263F2F0` | 40 | unwind | 1 | `fn_8263EDF0` | band3/meta_band/UGCPurchasePanel | band3/meta_band/BandProfile | +176 | 100 | 100 | Y | **MIS-PINNED** |
| 14 | `0x82675A44` | 40 | unwind | 1 | `fn_826759E0` | band3/game/Game | *same* | +0 | 100 | 100 | Y | HOMED |
| 15 | `0x82703AD0` | 40 | catch | 1 | `fn_82703A68` | system/rndobj/Utl | StandardStream | +8 | 100 | 100 | Y | **MIS-PINNED** |
| 16 | `0x82703B68` | 40 | catch | 1 | `fn_82703B00` | StandardStream | system/rndobj/Utl | +8 | 100 | 100 | Y | **MIS-PINNED** |
| 17 | `0x82709BF8` | 40 | unwind | 1 | `fn_82709BB0` | Sequence | DepthBuffer3D | +0 | 100 | 100 | Y | **MIS-PINNED** |
| 18 | `0x8274D430` | 40 | unwind | 1 | `fn_8274D198` | DataArray | *same* | +160 | 100 | 100 | Y | HOMED |
| 19 | `0x827799AC` | 40 | unwind | 1 | `fn_827796F8` | SongData | CharLipSync | +80 | **99.3** | 99.8 | Y | **MIS-PINNED** |
| 20 | `0x827AE070` | 40 | unwind | 1 | `fn_827ADFF8` | HeldButtonPanel | *same* | +0 | 100 | 100 | Y | HOMED |
| 21 | `0x82B5AE14` | 40 | unwind | 1 | `fn_82B5ADC0` | system/synth_xbox/Synth | *same* | +0 | 100 | 100 | Y | HOMED |
| 22 | `0x82B62D3C` | 40 | unwind | 1 | `fn_82B62CD8` | system/synth_xbox/FxSendCompress | *same* | +0 | 100 | 100 | Y | HOMED |
| 23 | `0x82B6F13C` | 44 | unwind | 1 | `fn_82B6F100` | Synapse_dsp | *same* | +0 | 100 | 100 | Y | HOMED |
| 24 | `0x82B7CC24` | 32 | unwind | 1 | `fn_82B7CAE8` | TourDescPanel | *same* | +0 | 100 | 100 | Y | HOMED |

**Verdicts: HOMED 14 · MIS-PINNED 8 · ORPHAN 2.**

- ✅ **Control result: all 14 in-unit rows read HOMED.** The classifier is not simply labelling
  everything mis-pinned.
- **My parent attribution agrees with BG's on 24 of 24 addresses**, derived independently.
- **The brief's ORPHAN prediction is confirmed**: rows #1/#2's parent `fn_822FC508` sits in
  `auto_03_822FC4F8_text`, an unpinned region with no base object, so **no pin move can pair
  them** — there is no heading to move them to. They are excluded from item 3.
- ⚠ **7 of the 8 MIS-PINNED rows read `fuzzy == 100` today**, i.e. they are being paid for by a
  false byte-twin in a unit that never emitted them. Only #19 reads 99.3/99.8 — which is the same
  signature BF §3 measured on its seven falsely-paired App funclets.

---

## 3. Tree-wide census (report only — nothing acted on outside the 8)

`tools/funclet_homing.py`, same classifier, whole image. Population: all 26,321 funclet targets.

| verdict | count | bytes | share of funclets |
|---|---:|---:|---:|
| **HOMED** | 24,193 | 936,384 | 91.91% |
| **MIS-PINNED** | **734** | **29,216** | 2.79% |
| **ORPHAN** (parent in unpinned `auto_*`) | 1,352 | 56,168 | 5.14% |
| **UNPINNED-FUNCLET** (funclet itself in a gap) | 42 | 1,676 | 0.16% |
| total | **26,321** | 1,023,444 | 100% |

### 3.1 Reconciliation against `report.json` — every discrepancy explained

| check | measured | verdict |
|---|---|---|
| report rows total vs `total_functions` | 69,240 = **69,240** | exact |
| retail funclet targets carrying an `fn_<VA>` report row | **26,321 of 26,321** | exact — every funclet is in the denominator, none missing |
| `masked_equal` **flagged rows** | **24,415** | re-measured (the inherited figure was 24,386) |
| `masked_equal_functions` **measure** | **23,076** | re-measured (inherited 22,886) |
| flag − measure | **1,339** | **exactly** the number of flagged rows with `mpn < 100` |
| flagged rows that are retail funclets | 24,341 | 99.70% |
| flagged rows that are **not** funclets | **74** | explained below |

The flag-vs-measure gap reproduces CLAUDE.md's documented superset relationship exactly: the
counter increments only inside `match_percent_normalized == 100.0` because it exists to discount
*credit*, so a flagged-but-99.x row is disclosed on the row and not in the measure. **1,339 is
not a discrepancy, it is that rule.**

The **74** non-funclet flagged rows are explained by objdiff's own predicate, not by a gap in my
census: `is_funclet_like` (`diff/mod.rs:880-898`) admits **any `fn_` + exactly 8 hex digits**, so
ordinary retail functions can pair through the same byte-signature fallback. 27 of the 74 are
**12 bytes**, only **1** carries its own EH prefix, and they cluster in stereotyped stubs
(`RockCentral` ×2, `BlockMgr` ×3, …). 74/24,415 = **0.30%**. ⚠ I did not individually adjudicate
all 74; the size/prefix distribution is the evidence, not a per-row proof.

### 3.2 The MIS-PINNED class, sized for a later lane

| | rows | bytes |
|---|---:|---:|
| MIS-PINNED at `fuzzy == 100` — **carried by a false twin** | **419** | **16,548** |
| MIS-PINNED below 100 | 315 | — |
| MIS-PINNED with no report row | 0 | — |

⇒ **419 rows / 16,548 B of today's `matched_code` rest on a funclet paired against a unit that did
not emit it.** That is the honest exposure of this class, and it is the number a follow-up lane
should price against. **This lane acts on 8 of the 734 and nothing more.**

⛔ **9 MIS-PINNED rows fall under a live concurrency bar and are FILED, not touched**: `0x8227A280`
(BandCamShot → BandCharacter), `0x8227A800` (Ham → BandCharacter, also inside W16-BH's
`0x8227A7A8–0x8227A948`), `0x8228732C` (BandCharacter → RockCentral), `0x8228C960` / `0x8228CA90`
(BandCharacter → BandDirector), `0x823F4A30` (UI → CharTaskMgr), `0x824EA034` / `0x824EA05C`
(Instance → BandUser), `0x8268B9EC` (BandUser → UI, inside W16-BI's `0x8268AEC8–0x8268C920`).
My census independently corroborates W16-BH's and W16-BI's surfaces.

---

## 4. Pre-registered prediction for the re-home (recorded BEFORE the edit)

All 8 MIS-PINNED rows of §2 are re-homed. In **every** case the funclet sits at the **start** of
its donor block; the correct boundary is determined by enumerating every `symbols.txt` occupant of
that block and asking whose parent each belongs to. That found the donor block is usually composed
**entirely** of the receiver's funclets, so the minimal well-formed edit is "delete the donor block,
extend the receiver's preceding block", which also repairs mis-pinned **siblings** of the same
parent. Consequently the 8 briefed funclets travel with 18 siblings — **26 rows in 9 block edits**.

⚠ **This is wider than the brief's "≤10 funclets", deliberately, and the reason is that the
narrower edit is not well-formed.** Moving 1 of the 3 funclets in `0x82318BBC–0x82318C40` would
require splitting a block whose every occupant belongs to RGUtl, and would leave two of the same
parent's funclets in a foreign unit. All 26 rows are independently adjudicated MIS-PINNED, all
9 edits stay inside the headings the brief authorised, and every row is priced below.

**Evidence for each prediction** is a relocation-masked body comparison (`bl` → top 6 bits;
D-form → top 16 bits; masked only at our object's relocated word offsets) of retail's funclet body
against every 4-byte-aligned window of every `.text` section of the receiving object.
✅ **The comparator passes a positive control on 8/8**: every row objdiff scores `fuzzy == 100`
today has a twin in its **donor**, and the one row at 99.3 has none — twin-present ⟺ fuzzy 100.

| edit | rows | donor → receiver | predicted |
|---|---:|---|---|
| #3 | 3 | DepthBuffer3D → RGUtl | **−2 fn / −88 B**: `0x82318BBC`, `0x82318BE8` fall out of 100 (RGUtl.obj has **no** twin); `0x82318C14` already 99.91, stays |
| #7 | 16 | AccomplishmentPlayerConditional → PartLauncher | **+5 fn / +200 B**: `0x82451A40`, `0x82451AA8`, `0x82451B10`, `0x82451B78`, `0x82451BE0` **cross to 100** (PartLauncher.obj has the twin, APC only a near-twin); 11 stay 100, now *earned* |
| #10 | 1 | Watcher → RockCentral | Δ0, 100 now earned |
| #13 | 1 | band3/meta_band/BandProfile → …/UGCPurchasePanel | **−1 fn / −40 B**: no twin in UGCPurchasePanel.obj |
| #15 | 1 | StandardStream → system/rndobj/Utl | Δ0, 100 now earned |
| #16 | 1 | system/rndobj/Utl → StandardStream | Δ0, 100 now earned |
| #17 | 1 | DepthBuffer3D → Sequence | Δ0, 100 now earned |
| #19 | 2 | CharLipSync → SongData | **Δ0** — see the refuted conditional below |
| **net** | **26** | | **`matched_functions` +2 · `matched_code` +72 B · `total_functions` / `total_code` unchanged** |

**Range: −3 … +2 functions (central +2).** The downside is realised if objdiff's greedy
funclet assignment over-subscribes a receiver. Checked: PartLauncher.obj defines **90**
funclet-like symbols for **53** targets after the move — no over-subscription — while
**RGUtl.obj defines only 2 for 11**, independently reinforcing #3's predicted fall-out. Upside
beyond +2 is possible but not predicted: freeing 16 targets out of AccomplishmentPlayerConditional
releases its object's funclets, which could let other APC rows pair better.

⛔ **The brief's pre-registered conditional on `fn_827799AC` has its CONDITION REFUTED.** The brief
asked: "if `SongData`'s compiled object has the byte-equal funclet, predict it crosses to 100
(+40 B / +1 fn)". Measured: **SongData.obj contains no relocation-masked twin** of `0x827799AC`
(nor of its sibling `0x827799D4`). So I predict it does **not** cross, and stays below 100 — Δ0 on
both measures, since a 99.3/99.8 row contributes to neither. It may fall further (to 0 if left
unpaired), which is still Δ0 on `matched_*`.

**The accuracy case does not depend on the sign.** Three rows are predicted to *lose* their 100:
that is the correct outcome, because it stops crediting `RGUtl` and `UGCPurchasePanel` for funclets
their objects do not emit, and **exposes** that our source for those parents does not reproduce
them (standing directive: a code% drop from a truer attribution is a win, and a metric that hides
real bugs is worse than a lower metric). PINHOME-1's rule applies — re-homing is **not**
metric-neutral, unlike adding a pin over `auto_*` code — which is why every row here is priced.

---

## 5. Measurement (recorded AFTER the edit)

### 5.1 Provenance

| | |
|---|---|
| worktree | `/home/free/tmp/wt-w16-bj`, branch `w16-bj` |
| baseline | main `9d2ec6df55ee`, rowset `~/tmp/bj/baseline_bj.json` (= `~/tmp/rows_w16bg_main.json`) |
| ruler | **graded** — `functionRelocDiffs=name_check`, read from `report.json`'s `provenance.diff_config`; objdiff 4.2.9 `5a51cd51fe0a353f` |
| build | full `./tools/ninja-locked`, `BUILD rc=0` (`~/tmp/rb3_build_w16bj_4.log`), then a settle build `rc=0` (`_5.log`, check/progress edges only) |
| edit surface | `config/45410914/splits.txt` only — 16 `.text` lines touched by hand (9 rewritten, 7 deleted); dtk then re-derived `.pdata` (16 removed / 9 added). Final `git diff --stat`: **18 insertions / 32 deletions**. `symbols.txt` did **not** drift. |

⚠ **The first build after the edit returned `rc=1`** on the split guard's *"THE SPLIT REWROTE ITS OWN
INPUT -- its output is not a fixed point of its input."* That is the documented `.pdata`
re-derivation (`.pdata` is derived output, never input), symmetric with the `.text` edit; the guard
states *"Recovery is one build"*, and one rebuild returned `rc=0`. Recorded because a lane reading
only the final `rc=0` would not know the guard had fired.

### 5.2 Whole-binary measures

```
matched_functions        43548 ->  43550     delta      +2     predicted +2    EXACT
matched_code           4039828 -> 4039860    delta     +32 B   predicted +72 B MISS -40 B
matched_code_percent  39.424236 -> 39.424545 delta +0.000309
total_functions          69240 ->  69240     delta      +0     as predicted
total_code            10247068 -> 10247068   delta      +0     as predicted
masked_equal_functions   23076 ->  23078     delta      +2
fuzzy_match_percent   49.695950 -> 49.694675 delta -0.001275
```

★ **The aggregate `fuzzy_match_percent` FELL, and that is the intended direction.** Three rows lose
a `masked_equal` false-twin credit outright, which drags the aggregate down harder than five genuine
crossings lift it. Standing directive: a code% drop from a truer attribution is a win.

### 5.3 Set-diff of the `fuzzy == 100` membership

`python3 tools/rowset_snapshot.py diff ~/tmp/bj/baseline_bj.json`:

```
CROSSED IN : 19 rows, 676 B
FELL OUT   : 18 rows, 644 B
NET bytes  : +32
```

**Decomposed** — the key is `unit::name`, so a re-homed row necessarily appears on *both* sides:

| class | rows | bytes |
|---|---:|---:|
| pure `unit::name` **key rename** (fell out of donor, reappeared under receiver) | 14 | 476 B on each side, net 0 |
| **genuine gains** (5 PartLauncher rows crossing 99.3/99.4 → 100) | 5 | **+200 B** |
| **genuine losses** (`RGUtl` ×2, `UGCPurchasePanel` ×1, `StandardStream`→`Utl` ×1) | 4 | **−168 B** |
| **net** | | **+32 B** ✅ reconciles to `matched_code` exactly |

**0 rows vanished and 0 appeared** across the whole funclet population (26,321 → 26,321), and
**0 collateral rows** — no row outside the 26 changed its `fuzzy == 100` membership. So the entire
delta is the move, with no offsetting noise.

### 5.4 Prediction vs measurement, per row (all 26)

| addr | size | donor → receiver | fuzzy before→after | mpn before→after | Δ B |
|---|---:|---|---|---|---:|
| `82318BBC` | 44 | DepthBuffer3D → RGUtl | 100 → **0** | 100 → 0 | −44 |
| `82318BE8` | 44 | DepthBuffer3D → RGUtl | 100 → **0** | 100 → 0 | −44 |
| `82318C14` | 44 | DepthBuffer3D → RGUtl | 99.91 → **0** | 99.91 → 0 | 0 |
| `82451A20` | 32 | APC → PartLauncher | 100 → 100 | 100 → 100 | 0 |
| `82451A40` | 40 | APC → PartLauncher | **99.3 → 100** | 99.8 → **100** | **+40** |
| `82451A68` | 32 | APC → PartLauncher | 100 → 100 | 100 → 100 | 0 |
| `82451A88` | 32 | APC → PartLauncher | 100 → 100 | 100 → 100 | 0 |
| `82451AA8` | 40 | APC → PartLauncher | **99.4 → 100** | 99.9 → **100** | **+40** |
| `82451AD0` | 32 | APC → PartLauncher | 100 → 100 | 100 → 100 | 0 |
| `82451AF0` | 32 | APC → PartLauncher | 100 → 100 | 100 → 100 | 0 |
| `82451B10` | 40 | APC → PartLauncher | **99.3 → 100** | 99.8 → **100** | **+40** |
| `82451B38` | 32 | APC → PartLauncher | 100 → 100 | 100 → 100 | 0 |
| `82451B58` | 32 | APC → PartLauncher | 100 → 100 | 100 → 100 | 0 |
| `82451B78` | 40 | APC → PartLauncher | **99.3 → 100** | 99.8 → **100** | **+40** |
| `82451BA0` | 32 | APC → PartLauncher | 100 → 100 | 100 → 100 | 0 |
| `82451BC0` | 32 | APC → PartLauncher | 100 → 100 | 100 → 100 | 0 |
| `82451BE0` | 40 | APC → PartLauncher | **99.3 → 100** | 99.8 → **100** | **+40** |
| `82451C08` | 32 | APC → PartLauncher | 100 → 100 | 100 → 100 | 0 |
| `82451C28` | 32 | APC → PartLauncher | 100 → 100 | 100 → 100 | 0 |
| `824F8244` | 44 | Watcher → RockCentral | 100 → 100 | 100 → 100 | 0 |
| `8263F2F0` | 40 | BandProfile → UGCPurchasePanel | 100 → **99.3** | 100 → **99.8** | −40 |
| `82703AD0` | 40 | StandardStream → system/rndobj/Utl | 100 → **99.5** | 100 → **100** | **−40 ⛔ the miss** |
| `82703B68` | 40 | system/rndobj/Utl → StandardStream | 100 → 100 | 100 → 100 | 0 |
| `82709BF8` | 40 | DepthBuffer3D → Sequence | 100 → 100 | 100 → 100 | 0 |
| `827799AC` | 40 | CharLipSync → SongData | 99.3 → 99.8 | 99.8 → 99.8 | 0 |
| `827799D4` | 40 | CharLipSync → SongData | 99.4 → 99.9 | 99.9 → 99.9 | 0 |

**Function-count reconciliation** (rows crossing the `mpn == 100` boundary):
`+5` (the five PartLauncher rows) `−3` (`82318BBC`, `82318BE8`, `8263F2F0`) = **+2 exactly.**

Per-edit against §4's pre-registration: **#3 −2 fn / −88 B EXACT · #7 +5 fn / +200 B EXACT ·
#10 Δ0 EXACT · #13 −1 fn / −40 B EXACT · #15 predicted Δ0, measured −40 B ⛔ MISS ·
#16 Δ0 EXACT · #17 Δ0 EXACT · #19 Δ0 EXACT.** Eight of nine edits priced exactly.

Two wording corrections to §4, neither affecting a number:
- #3 said `82318C14` *"already 99.91, stays"* — it did not stay, it fell to **0** (lost its pairing
  entirely). Byte and function impact are identical (a 99.91 row contributes to neither measure), so
  the prediction was right on both measures and wrong on the row state. Recorded rather than smoothed.
- #19's downside branch (*"may fall further, to 0 if left unpaired"*) did not occur: both rows
  **improved** slightly, 99.3 → 99.8 and 99.4 → 99.9, i.e. `SongData` pairs them better than
  `CharLipSync` did even though it cannot pair them exactly.

### 5.5 The one miss, explained — a structural limit of the predictor, not a pairing accident

`fn_82703AD0` **did** cross into `system/rndobj/Utl`. It reads **`mpn = 100.0`** — so it keeps its
`matched_functions` credit, which is exactly why the function delta is the predicted `+2` — and
**`fuzzy = 99.5`**. `matched_code` keys on `fuzzy == 100` **all-or-nothing per row**, so its 40 B are
withheld. **40 B is the entire shortfall**; no other row deviates.

Since `mpn = diff_score − arg_diff_score`, a row at `mpn == 100` with `fuzzy < 100`
**proves** every surviving penalty is argument-level — a relocation-name charge under `name_check`.
And that is precisely what the predictor cannot see:

> §4's evidence instrument was a **relocation-masked** body comparison — `bl` masked to its opcode,
> D-forms to their top 16 bits. It masks *exactly* the information `name_check` charges. So the
> comparator is an `mpn`-grade instrument: it can predict `matched_functions` and is
> **structurally incapable** of predicting `matched_code`. §4 priced #15 on `fuzzy` with it.

⚠ **Its 8/8 positive control was confounded, and the confound was invisible from the donor side.**
The control asserted "twin present ⟺ `fuzzy == 100`" against each row's **donor**. But in the donor
every one of those rows was a `masked_equal` **false byte-signature pairing** — which by
construction has an exact byte twin, because that is *why* objdiff paired it. So the biconditional
held there trivially and carried no information about the true home, where the twin exists but its
relocations name different symbols. **A control drawn from the population the instrument was tuned
on cannot discriminate** — same family as this tree's other vacuous-control incidents.

⇒ **Durable rule: a relocation-masked twin comparator predicts `matched_functions`, never
`matched_code`.** To price bytes, the comparator must compare relocation *target names*, i.e. do
what `name_check` does. State the predicted measure explicitly next time.

**Not run, deliberately:** the charged site on `fn_82703AD0` is not named here. `run_objdiff` (MCP)
performs a single-`.obj` incremental build that **skips the six obj patchers**, which would leave the
tree unpatched immediately before the `--verify-manifest` gate. The arg-only nature of the residual
is established analytically above (`mpn == 100 ∧ fuzzy < 100`), so naming the site is an improvement
in detail, not in certainty. **Filed** for a lane that can afford a full rebuild afterwards: it is a
live adjudicable signal — either our source names a different callee than retail, or a legitimate
fold-alias is missing — and it was **invisible while the row sat in `StandardStream` at a false 100**.
That is the "a metric that hides real bugs is worse than a lower metric" directive paying out.

### 5.6 Independent confirmation from the classifier

`python3 tools/funclet_homing.py --validate` re-run on the **post-edit** tree:

```
FuncInfos=8541 funclet targets=26321 .text pins=6685
fan-in (funclet -> #FuncInfos): {1: 26321}
verdicts: {'HOMED': 24219, 'MIS-PINNED': 708, 'ORPHAN': 1352, 'UNPINNED-FUNCLET': 42}
extra EH-prefix sites: 537 catch-funclet + 4 folded-parent (expect 537 + 4)
ambiguous FuncInfos: 1 (expect 1: the folded no-action record)
VALIDATE: PASS
```

**HOMED 24,193 → 24,219 (+26)** and **MIS-PINNED 734 → 708 (−26)**, exactly the 26 rows edited, with
`ORPHAN` and `UNPINNED-FUNCLET` unmoved. This is an instrument that shares no arithmetic with
`report.json` agreeing on the size of the move to the row.

### 5.7 Gates

Run in brief order, all in the worktree, native gate **last**:

| gate | result |
|---|---|
| full build `./tools/ninja-locked` | **`BUILD rc=0`** (`~/tmp/rb3_build_w16bj_4.log`); settle build `rc=0` (`_5.log`) |
| `python3 scripts/verify_ruler_agreement.py --check` | **rc=0** — all 4 keys OK, both entry points resolve `name_check` |
| `python3 scripts/verify_objs_patched.py --verify-manifest` | **rc=0** — 1,215 decomp + 3,114 target objects match, `tree_sha256=b73b5dd1c0f01157`; denylist OK |
| `python3 tools/icf_alias_finder.py --validate` | **rc=0** — `VALIDATE: PASS`, 1,404 map-consistent, 247 tolerated, **0 contradicted** of 1,652 |
| `python3 tools/funclet_homing.py --validate` (lane instrument) | **rc=0** — `VALIDATE: PASS` |
| `tools/native_build_gate.sh` | **rc=0**, verbatim: |

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

`skipped=0`, as required.

### 5.8 Commits on `w16-bj`

| sha | files | what |
|---|---|---|
| `0b1f75cf` | `tools/funclet_homing.py` | the reusable classifier; fan-in refutes the ICF hypothesis |
| `061aa923` | `docs/decomp/W16BJ_…md` | §0–§4, committed **before** the edit so §4 is a real pre-registration |
| `645e765a` | `config/45410914/splits.txt` | the 9 `.text` edits re-homing 26 funclets |
| (this) | `docs/decomp/W16BJ_…md` | §5 measurement |

### 5.9 NOT done — deliberately

1. **No tree-wide action.** Re-measured on the post-edit tree, the census now reads
   **HOMED 24,219 / 937,352 B · MIS-PINNED 708 / 28,248 B · ORPHAN 1,352 / 56,168 B ·
   UNPINNED-FUNCLET 42 / 1,676 B** (§3's pre-edit figures were 24,193 / 936,384 B and
   734 / 29,216 B — the 968 B moved transferred exactly, MIS-PINNED → HOMED, with the other two
   classes unmoved). Of the original 734, **419 rows / 16,548 B read `fuzzy == 100` on a false
   twin**. Brief says report only; the 26 moved here are the bounded pilot. The pilot's own result
   prices the remainder: of 26 rows, 5 gained bytes and 4 lost them, so a tree-wide sweep is an
   **accuracy play, not a byte play**, and should be budgeted as such.
2. **The 2 ORPHAN rows (`#1`, `#2`) were not moved.** Their parent `fn_822FC508` sits in the unpinned
   `auto_03_822FC4F8_text`; there is no pinned receiving unit, so no pin move can help. Pinning the
   parent's cluster is a different lane.
3. **The 8 anonymous parents were not named**, per the brief's item 4.
4. **9 census rows inside the concurrency bars were filed, not touched** — the `Ham.cpp:` / `UI.cpp:`
   / `BandUser.cpp:` headings, W16-BH's `BandCharacter.cpp:` receiver, and the address windows
   `0x8227A7A8–0x8227A948` (BH) and `0x8268AEC8–0x8268C920` (BI). Listed in §3.2.
5. **No map row, no alias group, no `src/` file was touched.** The whole lane is one `splits.txt`
   and two docs/tools files.
6. **The charged relocation-name site on `fn_82703AD0` was not named** — see §5.5 for why, and it is
   filed as the lane's single follow-up.
7. **`report.json`'s per-row `address` is still a section-relative offset**, not a VA. Recorded as a
   blind spot in `tools/funclet_homing.py`'s docstring; nothing keys on it.
