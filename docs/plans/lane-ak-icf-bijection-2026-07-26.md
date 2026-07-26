# Lane AK — the oracle lane, and the seam it actually found

**Date:** 2026-07-26 · **Branch:** `laneAK-oracle` · **Baseline:** 30,300 strict
**Result: 30,300 → 32,150 = +1,850 strict, LOST set empty, by-name lost 0.**

The lane was chartered to attack the pool every other attribution channel routed
to *an external oracle* (rb3-Wii, DC3, BinDiff, the Ghidra TU5 bank). The honest
outcome is inverted:

> **All four external-oracle channels measured ~dead (string/assert +17, Ghidra
> TU5 bank +1, BinDiff +0, zero-evidence splits holes 2 defects in 110). The
> +1,850 came from a discard pile inside our own existing tooling.**

---

## 1. ★ The funnel, measured before anything was applied

`scripts/harvest/oracle_funnel_scan.py` (new, committed). objdiff pairs
target ↔ base **by symbol name inside one unit**; a target function with no
`scripts/target_symbol_map.json` entry stays anonymous `fn_<VA>` and cannot pair.

| stage | n | why it drops |
|---|--:|---|
| raw `fn_<VA>` rows in `report.json` | **47,500** | |
| − vendor `auto_` spans in 0x828–0x82C | −10,672 | XDK + Quazal, hard-skipped |
| − already 100% | −15,495 | anonymous funclets already positionally paired; naming them measured −13 |
| **in scope** | **21,333** | |
| − units with no compiled base obj | −11,496 | `auto_` carve spans: no obj can emit the name, so a map entry can *never* pay. Splits/source work — a different lane |
| − unit emits no unpaired non-`__unwind$` symbol | −1 | |
| **workable** | **9,836** | in 627 units |

**Composition of the 9,836** (`scripts/harvest/oracle_ceiling_scan.py`, new):

| | n | % | meaning |
|---|--:|--:|---|
| IDENTICAL | 1,919 | 19.5% | reloc-masked byte-identical to a symbol in the right obj → naming pays **strict** |
| SAME_SIZE | 6,060 | 61.6% | same size, different bytes → naming pays **partial credit only** |
| NO_SIZE | 1,857 | 18.9% | body is not in our obj at all |

and 2,029 of the 9,836 are EH funclets (79.4% real / 20.6% funclet, discriminator
= first instruction is `subi/addi rX, r12, imm`; 100% precision on the MasterAudio
calibration set).

**★ The strict ceiling of the entire identification channel is therefore ~1,919,
not 9,836.** Four fifths of the pool is body divergence wearing an identification
costume — no oracle can fix it, because the *name* is not what is missing.

---

## 2. ★★ The seam: an "ambiguous" discard pile

`size_order_automap.py` and `homing_gen4.py` both anchor on reloc-masked byte
identity — and both accept a target function only if it is byte-identical to
**exactly one** compiled function. `homing_gen4.py:172` counts the rest as
`drop_name_ambiguous` and throws them away.

Of the 1,919 IDENTICAL VAs, **only 199 are byte-unique. 1,720 were being
discarded.** That discard counter is the entire vein — precisely the shape repo
memory warns about ("a tool that COMPUTES a correct result then DISCARDS it").

**Why the ambiguity does not matter.** objdiff pairs by *name* and then compares
*bytes*. If a set of target VAs and a set of compiled symbols all share the same
reloc-masked bytes, **any bijection between them scores 100% on every pair**.
Which name lands on which VA is a question of true identity, not of match
percent. So the whole equivalence class is harvestable *without resolving the
identity at all* — and an oracle can refine the assignment later without changing
the score.

`scripts/harvest/icf_class_bijection.py` (new) emits that bijection under the
real constraints: per-unit candidate names only, never a name already paired in
the unit, never an already-mapped VA, never a `__unwind$` compiler ordinal,
already-100% anonymous funclets out of the pool by construction.

| wave | applied | strict | cum |
|---|--:|--:|--:|
| 1 — exact byte-class bijection | 1,595 | **+1,581** (99.1% conversion) | 31,881 |
| 2 — cross-unit duplicate names | 182 | **+180** | 32,061 |
| 3 — near-identity, 1 differing word | 385 | **+25** (6.5%) | 32,086 |
| 4 — rescan | 0 | — | fixpoint |
| 5 — compose arm D's unique entries | 314 | **+64** | 32,150 |

**0 regressions. 0 by-name losses. 0 gains in a unit no fragment touched** (so no
stale-obj phantom). 322 units paid.

### 2.1 Two secondary findings worth keeping

* **`tu5_map_apply_fragment.py`'s global name-collision assert is over-strict.**
  It refuses a fragment whose *value* is already a mapped value anywhere. But
  pairing is per-unit, so the same mangled name legitimately sits at two VAs in
  two different units. All 182 wave-2 entries were cross-unit duplicates (0 were
  same-unit — checked before applying) and they paid +180. Add an opt-in flag
  rather than relaxing the default.
* **Naming a VA can knock out a *neighbour*.** Wave 3's single regression was
  `RockCentral fn_824F70A0`, an anonymous funclet that had been positionally
  paired at 100% and lost its partner when a sibling VA in the same unit took a
  name. Dropping that unit's 7 entries removed the regression *and* recovered one
  extra match. Expect this hazard to grow as the identity threshold is relaxed.

### 2.2 Independently confirmed

Arm D swept `size_order_automap` tree-wide from its own cold baseline and landed
**+681** on the same binary. On the 626 VAs where our fragments overlap we agree
on the *name* only 166 times — the other 460 are adjustor/vcall thunks where we
each picked a different member of the same ICF class **and both score**. That is
the bijection argument reproduced by an independent agent that was not told to
look for it.

Arm D also re-measured the tier precisions on the post-dtk-swap state, and found
the more useful re-framing:

| tier | name precision | strict yield |
|---|--:|--:|
| EXACT | 99.10% | 99.0% |
| STRONG | 79.80% | 20.5% |
| WEAK, identity ≥90 | — | 92.8% |
| WEAK, identity 50–90 | — | 40.4% |
| WEAK, identity <50 | 26.0% (tier-wide) | **0.0%** |

> **Name precision does not predict yield; reloc-masked byte identity does,
> monotonically.** `size_order_automap.py`'s tier boundary should be redrawn on
> identity, not on size+order. The <50-identity band is a measured pure no-op.

---

## 3. The three external oracles — all measured, all ~dead

| oracle | result | why |
|---|--:|---|
| **String / assert (rb3-Wii + DC3)** | **+17** | see §3.1 |
| **Ghidra TU5 bank** | **+1** | **circular** — see §3.2 |
| **BinDiff r2** | **+0** | 291 of its 299 high-confidence IDs are already in the map; 8 unlanded |

### 3.1 The file/line assert premise is REFUTED

Retail RB3-360 embeds **zero** Milo/game source-path strings. Three independent
confirmations:
* `src/system/os/Debug.h:112` — the retail-configured `MILO_ASSERT(cond, line)`
  is literally `((void)(cond))`; `__FILE__`/line/`#cond` exist only under
  `HX_NATIVE`. `MILO_FAIL` is `((void)(__VA_ARGS__))`.
* `orig/45410914/band.exe` contains 161 distinct `.cpp` path strings and **every
  one is Quazal/NetZ middleware or the XDK shader compiler** — not one Milo or
  band3 path.
* `kAssertStr` (`"File: %s Line: %d Error: %s\n"`, VA `0x82086960`) is dead data:
  exactly one function references it, and no `__FILE__` string exists to feed it.

Yield of the file/line channel on the workable pool: **2 VAs**, both Quazal.

The *coarser* form does pay a little. Arm C's `string_content_join.py` (new)
joins target `fn_<VA>` to our obj's symbols on **referenced string-literal
identity alone**, needing no byte identity — which makes it complementary to
`content_join_propose.py`, that gates on byte identity and uses strings only as
an ICF tie-break. 1,626 of 8,733 VAs (18.6%) reference any string literal; 561
join; 342 applied; **+17 strict, 0 lost**. Marginal yield collapsed 5.9% → 1.4%
between rounds: **drained**. Its real payload is fuzzy — **+1.82pp**
(`fuzzy_match_percent` 33.660 → 35.482), i.e. 342 body-divergent retail functions
stopped reading a false 0% and became visible to body-port lanes.

### 3.2 ★ The Ghidra TU5 bank is not an oracle — it is a stale mirror of our map

17,590 real named functions (not the ~15.1k memory records; `Function_8XXXXXXX`
is a second auto-name form a naive `FUN_` filter misses). 1,080 have a VA the map
does not cover. Provenance of those 1,080:

| n | provenance | verdict |
|--:|---|---|
| 16,510 (93.9%) | VA in the current `target_symbol_map.json` | circular |
| 812 (4.6%) | VA in `tools/ghidra/rb3_symbol_map.full.json`, which is generated **from** the map | circular (stale Jul-18) |
| 192 | XBOXKRNL import thunks | not `.text` |
| 73 | our own name at a **different** VA | the `_icf_arbitrary` hazard |
| 3 | genuinely novel (`KeQuerySystemTime`, `KeTls*`) | vendor |

**Zero names of independent origin.** 406 VAs are named in both and *disagree* —
and Ghidra always holds the *older* spelling. `build_full_symbol_map.py` →
`apply_symbols` is a **one-way pipe**; nothing re-imports names out, so the bank
can never pay more than the drift between map generations. Salvage funnel:
17,590 → 1,080 → 268 in a workable unit → 221 real (not funclet) → 78 whose name
is not already mapped → 44 actually emitted by that unit's obj → **1 pays**.

**Treat the bank as a visualization of the map, not a source. Do not re-run this.**

### 3.3 The zero-evidence splits holes — measured, and it collapses

laneAC/laneAD explicitly handed this lane their residual: *"the 178 zero-evidence
holes and 5 UNPORTED holes are not splits problems — they need map coverage or
source (BinDiff / rb3-Wii / DC3 oracle)."* Re-derived at current state
(not trusted from the doc):

| stage | n |
|---|--:|
| raw bracketed holes < 1 KB, in scope | 793 |
| − has mapped evidence | −633 |
| **zero-evidence (this lane's pool)** | **160** |
| − `n_carved == 0` (nothing to score) | −10 |
| − all-funclet (PE-grounded via `.pdata` + EH `_s_FuncInfo`) | −40 |
| **has ≥1 real function** | **110** |
| − no string literal on any VA | −91 |
| has ≥1 string | 19 |
| − generic/shared literal, or traces to a *third* unit | −17 |
| **confirmed actionable defect** | **2** |

**2 of 110 = 1.8%, against laneAC's 11.4% base rate on evidence-bearing holes —
so the naive projection of ~12 defects over-priced this pool ~6x.** Not a
fundable lane. Both confirmed defects were **invisible to the official
`autoid.json` pipeline** (its `min_strings=2` threshold; each had only one
qualifying string) and absent from `unified_id.json` entirely — found only by
manual sub-threshold grep against `../rb3/src`.

**Handoff to the splits lane (propose only — splits repair is not this lane's):**
* move `0x826146d8` / `0x82614788` from `CustomizePanel.cpp` to
  `band3/meta_band/ContentLoadingPanel.cpp` (string `loading_additional_progress`,
  3 call sites, all in `ContentLoadingPanel.cpp`'s `ContentMounted`/`ContentFailed`)
* move `0x826D6500` and its two adjoining helpers through `0x826d65e8` from
  `band3/game/TrackerDisplay.cpp` to `FocusTracker.cpp` (strings
  `focus_streak_length_multiplier` / `focus_streak_max_note_gap_ms`, rb3-Wii
  `FocusTracker.cpp:308-309`; target asm shape corroborates two `FindData` calls)

Two zero-evidence holes independently trace to `BandCamShot`/`HamCamShot` content
that lives in neither the assignee nor the enclosing unit — suggestive of a
genuinely orphaned CamShot scatter cluster, but not actionable as hole merges.
Confirmed noise not to re-pursue: `ui/startup/eng/startup_autosave_esrb_keep.milo`
recurs across 7 unrelated holes (classic shared-literal FP), and
`chars_dir` / `set_band_multiplier` / `trigger_disband_event` are `Symbols*.cpp`
project-wide constant-table strings.

The one cheap follow-up worth doing: lower `autoid`'s `min_strings` to 1 and
re-run **only over the zero-evidence hole candidate set**, not the whole binary.

---

## 4. Name spelling — the calibration was right, but the fix is structural

The brief predicted ~3 in 4 oracle-supplied names would need mangling correction.
**Across every arm, the measured number was ZERO** — because every arm took the
mangled name straight out of the COFF symbol table of our compiled obj
(`size_order_automap._ordered_funcs()`), never from prose, from an oracle's
spelling, or by hand-mangling.

> **The lesson is not "correct the spelling", it is "never author one."** A
> handoff should ship VAs plus a rule for reading the name out of the obj, not
> name strings. That sidesteps the `@@UAAXXZ`-vs-`@@M` class of failure entirely.

---

## 5. What is genuinely unreachable

Of the 9,836 workable in-scope VAs, after this lane and arm D:

* **~2,029 EH funclets.** Verified, not assumed: our objs emit `__unwind$337003`
  style **global compiler ordinals**, which shift with any source edit. There is
  no stable name to map. Permanently out.
* **~5,291 non-funclet VAs receive no proposal of any tier** (arm D's tree-wide
  number). These are the SAME_SIZE / NO_SIZE mass — **body divergence**, and the
  fix is body-port or source, not identification.
* **11,496 in-scope VAs in `auto_` carve spans with no compiled TU.** No obj emits
  any name for them, so no map entry can ever pay. That is splits/source work.

**The identification era really is over for strict scoring.** What remains of
value in *naming* is fuzzy: arm C's +1.82pp shows that naming body-divergent
functions converts them from an invisible false 0% into a visible, workable
body-port target. That is the right way to price a future naming wave.

---

## 6. Verification

**Six subagents** (2 Sonnet read-only measurement, 3 Opus verification arms each
in its own worktree from its own cold baseline, 1 Sonnet hole-pool measurement).
Every number above was re-checked against the lane lead's own baseline pickle
(`/home/free/tmp/laneAK_base_strict.pkl`, 30,300) in the lead's own worktree.
A/B protocol: settle build → snapshot → edit → rebuild, `rm -f report.cache` and
`touch config.yml` on every leg, strict = `match_percent_normalized == 100.0`
only, GAINED/LOST checked **unit-agnostically** and again by name only.

**A bug found mid-lane, worth flagging to other lanes:** `scripts/recarve/funclets.py`'s
default global "scan every `.s`, union all VAs" mode is corrupted by stale
overlapping auto-carve spans — the same VA appears in up to 3 `.s` files with
*different bytes*, and 1,077 of ~121,729 distinct VAs classify inconsistently
across files. Classify per-unit from the owning unit's own asm, never from the
global set. This moved the workable funclet count 2,106 → 2,029.

## 7. Tools added

| tool | role |
|---|---|
| `scripts/harvest/oracle_funnel_scan.py` | the honest funnel for the map-coverage channel |
| `scripts/harvest/oracle_ceiling_scan.py` | IDENTICAL / SAME_SIZE / NO_SIZE — the strict ceiling |
| `scripts/harvest/icf_class_bijection.py` | ★ the bijection over discarded ambiguous byte-classes |
| `scripts/harvest/nearidentity_bijection.py` | the 1-differing-word widening (near-exhausted) |
| `scripts/harvest/string_content_join.py` (arm C) | string-literal join needing no byte identity |
| `scripts/harvest/automap_tree_sweep.py` (arm D) | identity-banded tree-wide automap sweep |
