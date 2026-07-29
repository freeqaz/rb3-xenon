# laneBE — pricing the gap channel + the scope_map dropped-function fix (2026-07-29)

Inputs: `docs/plans/attribution-frontier-census-2026-07-29.md` (laneBA),
commit `dead67a0` (laneGAPFILL), commit `60f84426` (laneTIGHTGAP, branch
`tightgap`, **not on main**), `scripts/harvest/diffunit_gap_funnel.py`.

Worktree `/home/free/tmp/wt-laneBE-1` (branch `laneBE-1`), baseline
`166e6268` = **39,382** matched functions. Scratch: `/home/free/tmp/laneBE/`.
Main tree was never built in and never modified — **including no scope_map cache
rebuild on main.** That rebuild was deliberately *not* done: main's
`tools/scope_map.py` does not yet carry the fix (`ef734539` is on `laneBE-1`), so
rebuilding there would merely refresh the buggy cache. **Coordinator: run
`venv/bin/python tools/scope_map.py build` on main AFTER landing `ef734539`.**
The fix is verified in-worktree (§3.1).

**Mission 3 (the 2,200-near-miss campaign) was descoped mid-lane** by the
coordinator and handed to a dedicated lane (Lane BF). Nothing in that band was
touched by this lane — no near-miss function body was read, edited, or diffed.
See §4 for the one pointer worth carrying over.

## 0. TL;DR

1. **laneBA's "one unpriced channel" was already priced *and executed* by
   laneTIGHTGAP** three hours before laneBA published. Its geometry reproduces
   here byte-for-byte (104 gaps / 83,124 B / 975 fns / 25 named). Price: **+107**
   honest, **+87** distinct base pairings, from a raw **+186** (42% inflation).
   The 59 gaps laneTIGHTGAP dropped are worth **~0** (measured, by its own A/B).
   **Channel closed. Action: land `tightgap`.**
2. **★ But pricing it turned up a much larger sibling channel that nobody had
   measured: the same tight (≤4 KB) different-unit gaps *outside* the legacy
   `0x82800000–0x82D00000` window.** 638 gaps / 2,705 fns / 324 KB.
   Measured by full whole-binary A/B, twice, and reproduced a third time:

   | direction | strict gain | losses | **net `matched_functions`** | over-subscription inflation |
   |---|--:|--:|--:|--:|
   | all-LEFT | +561 | 1 | **+560** (39,382 → **39,942**) | **0.2%** |
   | all-RIGHT | +522 | 5 | +517 (→ 39,899) | 0.8% |
   | union of distinct flippable target fns | **600** | — | — | — |

   ★ Note the headline in commit `5ed03592` reads "+561 (39,943)"; the correct
   net figure is **+560 (39,942)** — +561 gains against 1 loss. Gross gains and
   net delta are both stated correctly in that commit's body table; only the
   subject line conflated them. `matched_code` +21,424 B (3,431,832 →
   3,453,256).

   This **exceeds laneBA's published whole-pool ceiling (+25 … +85) by 7–22×**,
   and it is essentially inflation-free — unlike the in-window pool, where 42%
   of the raw gain was many-to-one funclet absorption.
3. **★★ The honesty caveat, quantified: 80.5% of the 600 flip regardless of
   which neighbour claims them** (483 of 600 flip under LEFT *and* RIGHT). For
   those, the score carries **no evidence about the owner** — the attribution is
   a coin flip. This is a policy decision, not a measurement gap. §2.4.
4. **`tools/scope_map.py` defect fixed and committed** (`ef734539`): 5,256
   catch-all functions recovered to their true VAs; they land
   **vendor 4,299 / unknown 303 / engine 213 / thirdparty 191 / xdk 120 /
   crt 75 / game 55**. 0 regressions, 0 oracle-tier demotions.
5. **Stale-artifact trap fixed and committed** (`03c11d1d`): shared
   `scripts/harvest/live_units.py`; `classify_funclets.py` corrected from
   31,066 to **16,992** funclets (1.83× over-count) — which reproduces laneBA's
   independently-measured 16,992 exactly.

## 1. Mission 1a — the 104 tight in-window gaps: already priced

### Geometry reproduces exactly

`scripts/harvest/diffunit_gap_funnel.py` on the baseline splits (the
non-inflating tool; **not** `autocarve_funnel.py`, which inflates 1.99×):

```
pinned .text blocks 5813 | raw gaps 2393 | interior 881 | DIFFERENT-UNIT 1512
```

Cutting to ≤4 KB, ≥1 function, `va_lo` inside `0x82800000–0x82D00000`:

**104 gaps / 83,124 B / 975 fns / 25 named** — identical to laneTIGHTGAP's
live re-derivation, and one gap off laneGAPFILL's original 105/83,476/978/25
(one gap was consumed by intervening landings).

### The subtraction, shown

Classifying all 975 by disassembly shape, reading **only live** dtk target `.s`
files (intersected with `objdiff.json`'s `target_path` set — 8,939 of the
12,730 `.s` files in the warm worktree are stale orphans and must be dropped;
see §3.2). Funclet test = `subi r31, r12, IMM` then `mflr` — laneBA's and
`classify_funclets.py`'s signature.

| class | fns | bytes | can it flip? |
|---|--:|--:|---|
| `R_real` — >5 insn, real body | 606 | 66,532 | only if our source already emits it |
| `T_trivial` — ≤5 insn (accessors, thunks, tail forwarders) | 208 | 2,544 | yes, as crumb supply |
| `B_eh_funclet` — EH unwind funclet | 115 | 4,880 | yes, as crumb supply |
| `U_breadcrumb` — coverage stub | 21 | 420 | **unfixable, worth 0** |
| named (not resolvable from anonymous asm) | 25 | — | — |

Crumb supply = 208 + 115 + 21 = **344**.
laneTIGHTGAP measured **+87 distinct base pairings** ⇒ whole-pool conversion
**87 / 344 = 25.3%**. Landed `matched_functions` was **+107**, i.e. a residual
1.23× over-subscription factor on top of the distinct-pairing count.

Cross-check against laneBA's independent byte-twin supply measurement (59.1% of
in-scope crumbs have a reloc-masked twin somewhere in the tree): applied to the
**153** crumbs inside the 45 gaps laneTIGHTGAP actually kept, 0.591 × 153 = **90
predicted vs 87 measured**. Two independent methods agree to 3%.

### The residue is worth ~0

Splitting the 104 by what laneTIGHTGAP claimed:

| | gaps | fns | `R_real` | `B_eh_funclet` | `T_trivial` | `U_breadcrumb` |
|---|--:|--:|--:|--:|--:|--:|
| **claimed** (45) | 45 | 383 | 227 | **82** | 68 | 3 |
| **dropped** (59) | 59 | 592 | 379 | 33 | 140 | 18 |

The honest filter selected the funclet-rich gaps exactly as theory predicts:
the 45 kept gaps hold 39% of the functions but **71% of the EH-funclet supply**.
The 59 dropped gaps were dropped *because laneTIGHTGAP's own A/B measured zero
honest yield in them*. Their remaining supply is 33 funclets + 18 breadcrumbs
(the latter unfixable). **Upper bound ≤ +33, measured 0.**

**Verdict: the channel laneBA flagged as "the only item that could change the
verdict" is closed on measurement, at +107. It is not a lane; it is a `git
merge` of branch `tightgap`.**

## 2. Mission 1b — ★ the channel nobody measured: tight gaps *outside* the window

Pricing 1a required deriving the whole different-unit gap set, which made the
out-of-window sibling visible for free. laneAL declined the different-unit class
wholesale; laneAM swept it with a static exclusive-signature predictor and left a
residue it priced at **96 fundable functions ≈ +25** (T=1) or **+85** at argmax
with 28% fake. laneBA adopted that as the whole-pool ceiling.

### 2.1 The pool

Same cut (≤4 KB, ≥1 fn, DIFFERENT-UNIT), `va_lo` **outside** the legacy window:

**638 gaps / 2,705 fns / 324,336 B.** Composition: 1,585 `R_real`, 595
`B_eh_funclet`, 401 `T_trivial`, 50 `U_breadcrumb`, 74 named/unresolved.
Crumb supply = 401 + 595 + 50 = **1,046**.

Static prediction, using the in-window whole-pool conversion measured in §1:
1,046 × 25.3% ≈ **+265 distinct pairings**, ≈ +325 landed. That already
contradicted laneBA's ceiling by 4×, so it was measured rather than published.

### 2.2 Measured, all-LEFT

Full A/B discipline: baseline strict-set snapshot; `diffunit_gap_apply.py --dir
left` (audit clean, 638 planned / 638 applied / 0 skipped); `git checkout --
config/45410914/symbols.txt`; `touch config.yml`; stamps removed;
`./tools/ninja-locked` twice; `rm -f report.cache` before every report read.

```
matched_functions 39,382 -> 39,942     net delta +560
matched_code      3,431,832 -> 3,453,256   (+21,424 B)
strict set by (unit,name):  +561 / -1
strict set by (name only):  +561 / -1
```

Single loss: `default/RockCentral :: fn_8250CBA4`. The whole run was repeated
from a clean re-apply after the all-RIGHT leg and landed on **39,942** again,
bit-for-bit — `diffunit_gap_apply.py --audit` reports 0 overlaps, 0 inversions,
0 duplicate blocks, 0 sectionless blocks.

### 2.3 The over-subscription test — this pool is clean

laneTIGHTGAP's inflation test: a unit cannot honestly hold more 100%-matched
functions than its **compiled base obj defines function symbols** (its
`MemcardMgr` case: 62 symbols, 464 credited matches). Re-implemented here over
the COFF symbol tables of all compiled objs:

| | raw gain | obj-fn-symbol-capped | inflation | units exceeding their obj |
|---|--:|--:|--:|--:|
| all-LEFT | 561 | **560** | **0.2%** | 1 (`RockCentral`) |
| all-RIGHT | 522 | 518 | 0.8% | 3 |

The test is not vacuous — it is the same test that caught the in-window pool's
42%. **The out-of-window pool is genuinely ~inflation-free**, because the gains
are spread thin (max 22 functions in any one unit) rather than dumped into one
absorbing unit.

### 2.4 ★★ The honesty discriminator: run it the other way

The decisive question is not "does it score" but "does the score know who owns
it". So the identical experiment was run with the opposite fixed rule.

```
all-RIGHT: matched_functions 39,382 -> 39,899   +522 / -5
```

Comparing the two flip sets **by function name**:

| | n |
|---|--:|
| flips under LEFT | 561 |
| flips under RIGHT | 522 |
| **flips under BOTH (direction-independent)** | **483** |
| LEFT-only | 78 |
| RIGHT-only | 39 |
| **union — distinct target fns flippable at all** | **600** |
| **direction-independent fraction** | **80.5%** |

**80.5% of the yield is attribution-blind.** Under objdiff's normalized diff a
28–44 B EH funclet / thunk is byte-identical to every other one, and ICF-folded
COMDAT bodies genuinely exist in *both* neighbours' objs — so either fence unit
is a valid byte supplier. For those 483 functions the splits.txt attribution is
a coin flip that the metric cannot punish.

Composition of the 600 (union):

| class | n | in the direction-independent 483 |
|---|--:|--:|
| `B_eh_funclet` | 354 | 267 |
| `R_real` | 236 | 212 |
| named / unresolved | 10 | 4 |

The 236 `R_real` flips are the interesting ones: a 30-instruction body cannot
match by accident, so **our source genuinely reproduces those bytes** — they
were simply unpinned. Their direction-independence (212/236) is the ICF story,
not a fakeness story: retail folded one copy of a template/inline helper that
several TUs each emit.

### 2.5 Why laneAM's predictor underprices this by ~7×

Not a bug in laneAM — a category difference, and it is the reusable lesson:

* laneAM's predictor scores **attribution evidence** (does this gap have an
  *exclusive* signature pointing at one neighbour?). It refuses ambiguity.
* objdiff scores **byte reproduction**. Pass 2b (`mod.rs:1533`) permits
  many-to-one pairing onto an already-consumed base funclet, and normalized diff
  masks relocations. **A crumb therefore needs no exclusive signature at all.**

So both numbers are correct about different quantities: **+25 is the honestly
*attributable* yield; +561 is the *reproducible* yield.** Any future lane
quoting a gap-channel ceiling must say which one it means.

### 2.6 Recommendation, and what is committed

`all-LEFT` is committed on `laneBE-1` as the landable candidate, for three
reasons: it is a **fixed rule, not an outcome argmax** (so it has none of the
selection bias behind laneAM's measured 28%-fake figure at T=0); it beats RIGHT
on both yield (+561 vs +522) and losses (1 vs 5); and it has a real mechanistic
prior — **MSVC X360 emits an EH funclet immediately *after* its parent
function**, so a crumb in a gap fenced on the left by unit X is more likely X's
tail than the next unit's head. The measurement corroborates that prior.

**The coordinator should decide whether the project accepts 483 coin-flip *unit*
attributions in exchange for +561 reproducible matches**, given the `_splits_fill`
doctrine that coin-flip attributions be recorded UNRESOLVED. Three options:

1. **Land all-LEFT (+561, 1 loss).** Best yield; records 483 ambiguous owners.
2. **Land only the `R_real` subset (~+236).** Every one is a genuine body our
   source already reproduces; ambiguity is ICF's, not ours. Most defensible.
3. **Land nothing; record the pool as UNRESOLVED at +561 known value.** Honest,
   forfeits the largest measured single-lever this side of the near-miss band.

## 3. Mission 2 — the two tool defects

### 3.1 `tools/scope_map.py` dropped/mis-addressed functions — FIXED (`ef734539`)

laneBA's diagnosis confirmed and made precise. The defect is in
`load_functions()`'s **catch-all/auto-unit branch only** (the pinned-source
branch's `base + rel` arithmetic is exact and was left alone). Every function
that `obj_target_symbol_renamer.py` renamed from `fn_<addr>` to its MSVC mangled
name stops matching `FN_ADDR_RE` and was assigned a **synthetic** address
(`last_anchor + named_off`) instead of its true VA — so it was absent from the
tier denominators at its real address.

Fix: two new resolvers consulted before the synthetic fallback —
`load_target_symbol_name2addr()` (inverts `scripts/target_symbol_map.json`,
dropping ICF-ambiguous names) and `load_symbols_txt_name2addr()` (exact-name
lookup in `config/45410914/symbols.txt`).

Verified by a controlled A/B on a frozen report snapshot (to isolate the code
change from concurrent build churn):

| metric | value |
|---|--:|
| catch-all named functions examined | 5,377 |
| resolved via `target_symbol_map.json` | 5,167 |
| resolved via `symbols.txt` fallback | 209 |
| still unresolvable | **1** (`__MERGED_fn_*`, ICF-merged) |
| **new true-VA addrs added** | **5,256** |
| stale synthetic addrs retired | 5,255 |
| code bytes moved to the correct address | ~2.2–2.3 MB |
| matched-function regressions | **0** |
| oracle-tier (game/engine/thirdparty/crt) demotions | **0** |

**Where the 5,256 landed** (laneBA predicted vendor/unknown/engine — confirmed):

| tier | fns |
|---|--:|
| vendor | 4,299 |
| unknown | 303 |
| engine | 213 |
| thirdparty | 191 |
| xdk | 120 |
| crt | 75 |
| game | **55** |

Only 55 land in the priority tier — so this defect was **inflating the game
tier's apparent completion by a negligible amount**, but was hiding 2.3 MB of
vendor mass. Live cache rebuild lands at **68,756** entries (vs `report.json`'s
69,378 `total_functions`; the ~620 difference is the pre-existing, documented
`dedup=True` collapse of ICF folds in `build` mode, unchanged by this fix).

### 3.2 Stale `auto_03_*` artifacts — FIXED (`03c11d1d`)

New shared helper **`scripts/harvest/live_units.py`** —
`live_target_paths()` / `live_unit_names()` / `filter_live()`, sourced from
`objdiff.json`.

Wired in:

* `scripts/grind/classify_funclets.py` — **12,951 → 2,855** `.s` files scanned
  (10,096 stale dropped); funclet count corrected **31,066 → 16,992**, a 1.83×
  over-count. ★ 16,992 is *exactly* laneBA's independently-measured figure —
  strong mutual corroboration of both the fix and laneBA's census.
* `tools/icf_alias_finder.py` — 165,440 symbols indexed cleanly post-filter.

Audited, no change needed: `scripts/harvest/gap_content_evidence.py` already
carries its own mtime-based stale filter.

**Follow-ups, deliberately not touched** (~80 grep hits; most glob the *distinct*
`build/45410914/src/**/*.obj` per-TU mirror, a related but separate staleness
question): `scripts/{obj_guard_patcher,dump_vtable,residue_census,tu_wiring_byunit,tu_wiring_rank,truncation_audit,find_truncated_splits,check_objects_json,map_verify}.py`,
`scripts/recarve/{funclets,scan}.py`,
`tools/{map_lint,global_fuzzy_index,dc3_content_match,dc3_residual_rank,game_content_match,pin_audit}.py`,
`tools/exploratory/{callgraph_triangulate,vtable_transitivity}.py`.

**The staleness is worse than laneBA measured.** laneBA reported 4,618 orphan
`.s` files against 2,395 live units; a *warm reflinked worktree* carries main's
whole accumulated history — **12,730 files on disk, 3,791 usable, 8,939 stale
(70%)**. Any obj/asm-derived scan in a worktree must filter, not just on main.

## 4. Mission 3 — descoped, pointer only

Reassigned to Lane BF mid-lane. **No near-miss function body was read, edited,
or diffed by this lane.** One pointer worth carrying: the honesty tooling built
here is directly reusable by any lane touching splits —
`/home/free/tmp/laneBE/oversub.py` implements laneTIGHTGAP's over-subscription
test (credited matches vs COFF-defined function symbols per unit) and
`/home/free/tmp/laneBE/snap.py` produces the unit-agnostic strict-set snapshots
used for zero-loss A/B. Both are worth promoting into `scripts/harvest/` if a
third lane needs them.

## 4b. ★ Landing note — this branch must be RE-DERIVED, not merged

`laneBE-1` is based on `166e6268` (39,382). While this lane ran, main advanced to
`d112d2ef` (**39,522**) — **`tightgap` landed as +109**, plus laneBODYPORT +31.
So §1's recommendation ("land `tightgap`") is already satisfied, and its measured
price (+107 here, +109 as landed) is confirmed by the coordinator's own A/B.

Consequence for §2: the 638 out-of-window claims in `5ed03592` are **geometrically
disjoint** from what `tightgap` landed (that lane touched only gaps *inside*
`0x82800000–0x82D00000`; every gap in this commit has `va_lo` outside it), but
`config/45410914/splits.txt` will still conflict **textually** because both
rewrite overlapping regions of the same file and dtk re-derives all `.pdata`
lines on every split run.

**Do not merge this branch's splits.txt.** Re-derive instead — it is two commands
and one build, and it is the only way to get honest numbers against the current
baseline:

```bash
scripts/setup_worktree.sh ~/tmp/wt-land ~/tmp/wt-land-branch   # from current main
cd ~/tmp/wt-land && ./tools/ninja-locked
python3 scripts/harvest/diffunit_gap_funnel.py --worktree $PWD --out gaps.json
python3 - <<'PY'
import json; W=(0x82800000,0x82D00000)
g=[x for x in json.load(open('gaps.json'))
   if x['size']<=4096 and x['n_fns']>0 and not (W[0]<=x['va_lo']<W[1])]
json.dump(g, open('gaps_out.json','w')); print(len(g))
PY
python3 scripts/harvest/diffunit_gap_apply.py --worktree $PWD --gaps gaps_out.json --dir left
# then the standard splits ritual: git checkout -- symbols.txt; touch config.yml;
# rm build/45410914/*.stamp report.cache; ninja-locked TWICE; snapshot; oversub.py
```

Expect close to +560 but **not identical** — `tightgap`'s landings changed the
fence set slightly, so a few gaps will have merged, split, or disappeared. The
two commits from Mission 2 (`ef734539`, `03c11d1d`) touch only `tools/` and
`scripts/` and **cherry-pick cleanly** onto current main; land those first and
independently of the splits question.

## 6. Coordinator follow-ups Q1–Q4 (second pass) — ★ RECOMMENDATION REVERSED TO **CLOSE**

The coordinator asked four questions before deciding LEFT-vs-conservative. The
answers **retract a claim of mine from §2.4** and change the recommendation from
"coordinator's call between three options" to **"close the channel"**.

### Q1 — the crosstab: shape × direction-determinacy

| shape (as classified in §1) | coin-flip | LEFT-only | RIGHT-only | determined | total | % determined |
|---|--:|--:|--:|--:|--:|--:|
| EH funclet | 267 | 59 | 28 | 87 | 354 | 24.6% |
| "real body" | 212 | 19 | 5 | 24 | 236 | **10.2%** |
| named/unresolved | 4 | 0 | 6 | 6 | 10 | 60.0% |
| **all** | **483** | 78 | 39 | **117** | **600** | **19.5%** |

**This is the opposite of the hoped-for result.** The conservative option is not
nearly free: the supposed real bodies are the *least* direction-determined class
in the set (10.2% vs 24.6% for funclets). On this table alone, "land only the
real bodies" would have meant landing 212 coin-flips to get 24 evidenced ones.

### Q2 — inherent or artifact? **INHERENT, 93.0%** — hypothesis confirmed

Test: compute each flipped function's reloc-masked `(size, sha1)` from the dtk
**target** obj, then ask whether the LEFT neighbour's compiled base obj **and**
the RIGHT neighbour's compiled base obj each define a funclet-like Code symbol
with the same signature. objdiff gates both sides on `is_funclet_like`, so that
is exactly the supply condition for a 100% normalized pairing. 528 of 600
resolved.

| coin-flips (428 resolved) | n | share |
|---|--:|--:|
| **both objs supply the bytes → inherent ambiguity** | **398** | **93.0%** |
| only one obj supplies → pairing artifact | 24 | 5.6% |
| neither supplies → unexplained | 6 | 1.4% |

**The test is not vacuous, and it validates itself on the control:** of the 100
resolved *direction-determined* functions, 88 show exactly the matching
**one-sided** supply (`L=Y R=n` 63, `L=n R=Y` 25). The supply model therefore
*predicts* determinacy, which is what a real mechanism does and a tautology does
not.

So the coordinator's reframing is correct as far as it goes: **ownership is
genuinely underdetermined by the binary, and neither attribution is a false
claim.** The 24 one-sided-supply-but-flips-both-ways cases (5.6%) are a genuine
pairing artifact and are flagged here as promised, but they are not the story.

### ★★ Q2's by-product: §2.4's "236 genuine real bodies" is RETRACTED

Asking *which base symbol class* supplies each target shape exposed a defect in
my own §1 classifier:

| target shape | resolved | median size | max size | supplied by |
|---|--:|--:|--:|---|
| `B_eh_funclet` | 330 | 40 B | 76 B | `__unwind$` 291, `__catch$` 27, none 12 |
| **`R_real`** | **192** | **32 B** | **32 B** | **`__unwind$` 192 (100%)** |
| named | 6 | 116 B | 180 B | none |

Every single `R_real` entry is **exactly 32 bytes / 8 instructions**, and 100% of
them are supplied by an `__unwind$` EH funclet. Disassembled, they are all one
shape — a **static-init guard-clear cleanup**:

```
stwu  r1, -0x60(r1)
lis   r11, lbl_82CBC68C@ha        ; load guard word
lwz   r11, lbl_82CBC68C@l(r11)
clrrwi r11, r11, 1                ; clear bit 0
lis   r10, lbl_82CBC68C@ha
stw   r11, lbl_82CBC68C@l(r10)    ; store back
addi  r1, r1, 0x60
blr
```

My §1 funclet detector only recognised the `subi r31, r12` + `mflr` prologue and
missed this second variant entirely. **There are therefore ZERO genuine real
bodies in the flip set — all 600 are EH funclets/cleanup boilerplate.** §2.4's
sentence "236 of the 600 are real >5-insn bodies our source already reproduces"
is withdrawn, and with it **option 2 ("land only the real bodies") does not
exist.** The decision is binary. (This also corroborates laneBA's warning that
the "static-init guard" and "coverage-breadcrumb stub" labels describe one
byte-identical class — and that a shape detector must be falsified against a
control, which mine was not.)

### Q3 — the tiebreaker: it exists, it is RAW diff, and it rejects **100%**

The relocation *is* the discriminating evidence, and normalized diff is exactly
what throws it away. Direct check on `fn_82275674` (a coin-flip claimed by
`PatchDir`), `run_diff_inspect --diff-mode raw`:

```
97.5% raw match — 4 of 8 instructions differ, all diff_arg:
  target: lis r11, lbl_82CBC68C
  base  : lis r11, ??_B?1??StaticClassName@RndAnimatable@@SA?AVSymbol@@XZ@51
```

Our base symbol is the guard for `RndAnimatable::StaticClassName`; the retail
function clears an unrelated guard. Same shape, different object.

Scaled to the whole set by counting relocations inside each flipped target
function (a function with zero relocations is raw-safe by construction):

| shape | zero-reloc (raw-safe) | has relocs (raw-fragile) |
|---|--:|--:|
| EH funclet | 0 | 330 |
| 32 B guard-clear | 0 | 192 |
| named | 0 | 6 |
| **total** | **0** | **528** |

**Not one function of the +560 survives a raw-diff requirement.** The tiebreaker
the coordinator asked for is real and available — and applying it resolves the
483 coin-flips to **zero evidenced attributions**, not to a recoverable subset.

★ **The same test applied to work already on main:** of laneTIGHTGAP's landed
+109, 107 are in the in-window tight-gap set and 81 resolved — **0 of 81 are
raw-safe** (sizes 12/32/40/44/52/84/132 B). This is not a reason to revert it;
it is a statement that **the landed +109 and this +560 are the same channel with
the same property**, and the project should decide about them together.

### Q4 — re-derived and re-measured on current main

`laneBE-2` branched from main `37883122`; full build, then the standard splits
ritual, then a second full build.

| | pre-tightgap baseline | **current main** |
|---|--:|--:|
| baseline `matched_functions` | 39,382 | **39,520** |
| out-of-window tight gaps | 638 gaps / 2,705 fns | **638 gaps / 2,705 fns (unchanged)** |
| all-LEFT result | 39,942 | **40,080** |
| **net delta** | **+560** | **+560** (+561 / −1) |
| over-subscription inflation | 0.2% | 0.2% |
| splits audit | clean | clean |

**Identical.** The two pools are confirmed disjoint: laneTIGHTGAP touched only
in-window gaps, and the in-window remainder now measures **59 gaps / 592 fns** —
exactly the 59 gaps it dropped (§1). So the number is valid against the tree it
would land on, and none of my gaps were claimed by intervening work.

(Minor: my baseline reads 39,520 vs the coordinator's 39,522 — a 2-function
difference not chased down; it does not affect the delta, which is measured
within one tree.)

### ★ Recommendation: **CLOSE — do not land the splits claim**

Not because the matches are fake — under the project's official
`match_percent_normalized` metric they are real, and Q2 shows the ambiguity is
inherent rather than a defect. Because of what the +560 *is*:

1. **Zero real bodies.** All 600 are EH funclets and static-guard-clear cleanup
   boilerplate (Q2 by-product). Nothing here advances any TU toward being ported.
2. **Zero evidenced attributions.** 93% is inherently ambiguous, and the one
   available tiebreaker rejects 100% (Q3).
3. **It is farmable, and that is the real risk.** EH funclets are 24.5% of the
   binary (17,002 functions). A channel that credits `matched_functions` for
   generic cleanup shapes, at ~0.25 matches per crumb, with no attribution
   evidence, can be run repeatedly until the metric stops meaning "we reproduced
   this program".

**The principled line** — and it is already implicit in prior lanes' behaviour —
is *contiguity evidence*: laneAL's **interior** gaps (same unit fenced on both
sides) carry a real argument that the hole is that unit's own code, so absorbing
them is evidenced. **Different-unit** gaps carry no such argument. Recommend:
keep interior sweeps, close different-unit gap absorption.

**Two concrete proposals for the coordinator:**

* Adopt **raw-safety as a gate** on any future gap-absorption channel: a claimed
  crumb counts only if it still matches with `functionRelocDiffs` enabled. Cheap
  to compute (reloc count per target function — the Q3 script), and it cleanly
  separates "we reproduced this code" from "we reproduced this code *shape*".
* Record the 638-gap pool as **UNRESOLVED at a known value of +560** per the
  `_splits_fill` doctrine, so no future lane re-prices it from scratch. This
  document is that record.

The splits.txt for the claim is preserved on branch `laneBE-2` (re-derived and
valid against current main) **should the coordinator decide otherwise** — it is
committed but explicitly not recommended for landing.

## 5. Reproduction

```bash
scripts/setup_worktree.sh ~/tmp/wt-laneBE-1 laneBE-1
cd ~/tmp/wt-laneBE-1 && ./tools/ninja-locked          # dirty-obj reflink trap

# gap geometry (authoritative tool; NOT autocarve_funnel.py)
python3 scripts/harvest/diffunit_gap_funnel.py --worktree $PWD \
        --out /home/free/tmp/laneBE/gaps_all.json
# -> 104 in-window tight gaps / 975 fns ; 638 out-of-window tight gaps / 2,705 fns

# shape classification (live-asm-filtered)
python3 /home/free/tmp/laneBE/classify_gapfns.py $PWD <gaps.json>

# the A/B, per direction
python3 scripts/harvest/diffunit_gap_apply.py --worktree $PWD \
        --gaps /home/free/tmp/laneBE/gaps_tight_outwindow.json --dir left
git checkout -- config/45410914/symbols.txt
touch config/45410914/config.yml && rm -f build/45410914/*.stamp build/45410914/report.cache
./tools/ninja-locked && ./tools/ninja-locked
rm -f build/45410914/report.cache
python3 /home/free/tmp/laneBE/snap.py build/45410914/report.json out.json
python3 /home/free/tmp/laneBE/oversub.py $PWD base_snap.json out.json
```

Scratch (regenerable, not committed): `/home/free/tmp/laneBE/`.
