# Lane BO-8 — the 21,314 funclets at 100%: benign fold or over-count? (2026-07-29)

> **Verdict: the funclet half of `matched_functions` is SUBSTANTIALLY SOUND, and the
> rot is already disclosed.** The supply-backed funclet population is the *cleanest*
> stratum in the whole binary — **2.4% divergent**, against **5.5%** for named
> bodies. The bad credit is concentrated almost entirely in the **1,517 symbols
> objdiff already flags as `masked_equal`**, which measure **27.4% divergent —
> 11× the backed-funclet rate**. Two instruments that share no code agree on which
> symbols those are.
>
> **The mission's premise is refuted: populating the ICF alias map cannot decide
> this, because it cannot change anything.** Measured, not argued — growing the map
> from **3 groups to 1,408** moved `matched_functions`, `matched_code`,
> `fuzzy_match_percent` and `masked_equal_functions` by **exactly 0**. The map is
> dead code on the report path.
>
> **Honest floor: `matched_functions` 40,540 → 37,490 – 38,098** (a band, not a
> number; the headline over-states by **6.0% – 7.5%**). Recommended quotation in §6.

**Baseline:** worktree `/home/free/tmp/laneBO8/wt-icf`, branch `laneBO8-icf` from main
`7833c23d`. Full `./tools/ninja-locked` before any obj-derived measurement;
`report.cache` dropped; **no split-forcing build was run**, so this lane never
touched the `symbols.txt`-drift ±2 hazard. In-worktree `matched_functions` = **40,540**.

**Tools committed by this lane**
- `scripts/harvest/icf_alias_map_build.py` — builds the ICF alias map from four sources.
- `scripts/harvest/funclet_credit_audit.py` — the repeatable decision pass.
- `scripts/icf_alias_groups.json` — the populated map (generated, regenerable).
- `docs/plans/lane-bo8-icf-handverified.json` — hand-verified alias groups (proof required).
- `docs/plans/lane-bo8-icf-map-contradictions.json` — the 200-item map-mispair worklist.

**Reproduce end to end** (no build needed beyond a current `report.json`):

```bash
python3 scripts/harvest/icf_alias_map_build.py --sources dc3,laneab,hand,derived \
    --stats --out scripts/icf_alias_groups.json \
    --contradictions-out docs/plans/lane-bo8-icf-map-contradictions.json        # ~12 s
python3 scripts/harvest/reloc_correspondence.py --census --out ~/tmp/cen_perm.ndjson
python3 scripts/harvest/reloc_correspondence.py --census --strict-consistency \
    --out ~/tmp/cen_cons.ndjson                                                 # ~10 min each
python3 scripts/harvest/funclet_credit_audit.py \
    --census ~/tmp/cen_perm.ndjson --census-strict ~/tmp/cen_cons.ndjson        # seconds
```

---

## 0. What was actually asked, and why it decomposes into two questions

The brief: 21,314 funclet-like symbols sit at 100% (52.9% of `matched_functions`);
99.9% were paired only by the byte fallback; 89.1% carry a normalized-away
relocation-target difference; benign ICF folding or real over-count?

Those three facts describe **two independent defects**, and the whole difficulty of
the question comes from conflating them.

| | question | instrument |
|---|---|---|
| **AXIS 1 — SUPPLY** | did we actually *compile* a distinct body for this credit? | objdiff's own `masked_equal` disclosure, validated here by a compile-out A/B |
| **AXIS 2 — IDENTITY** | does the body we compiled point at the *same things* retail's does? | `reloc_correspondence.py --census` (retail-byte content oracle) |

A pass-2b surplus credit is unsupported *whatever* its relocations say. A
reloc-divergent credit is wrong *even though* a distinct body backs it. They are
disjoint deductions and this lane measures them jointly (§3).

---

## 1. ★ The premise is wrong: the ICF alias map cannot decide this

The brief says the question is "undecidable until the ICF alias map is populated —
and once it is, the decision is a 3-second batch pass." **It is not, and it is not.**

`icf_aliases.map` feeds objdiff's `MappingConfig::symbol_equivalences`, which is
consumed in exactly one place that matters: `reloc_eq`
(`../objdiff/objdiff-core/src/diff/code.rs:874-897`).

```rust
let relax_reloc_diffs = diff_config.function_reloc_diffs == FunctionRelocDiffs::None;
…
if left_reloc.relocation.flags != right_reloc.relocation.flags { return false; }
if relax_reloc_diffs { return true; }          // <-- returns BEFORE any name lookup
```

`objdiff-cli report generate` **hardcodes** `function_reloc_diffs: None`
(`../objdiff/objdiff-cli/src/cmd/report.rs:392-398`; `objdiff.json` sets no key at
all). So on the report path `reloc_eq` returns `true` on flag equality and never
reaches the `symbol_equivalences` fallback. The map is dead code there. It is also
**not** referenced by `pair_funclets_by_bytes`, `funclet_signature`, or
`matching_symbols` — so it cannot change *pairing* either.

**Measured control.** Generated a 1,408-group / 9,325-line map (469× the live one),
swapped it in, re-ran `report generate` over identical objects:

| measure | 3-group map | 1,408-group map |
|---|--:|--:|
| `matched_functions` | 40,540 | **40,540** |
| `masked_equal_functions` | 1,517 | **1,517** |
| `matched_code` | 3,569,916 | **3,569,916** |
| `fuzzy_match_percent` | 40.791737 | **40.791737** |

Zero on every axis. **Populating the map is worth doing — §5 — but it is an oracle
for analysis tools and for `objdiff-cli diff` noise, never a lever on the headline
and never a prerequisite for this audit.**

The real reason the question looked undecidable is the same one lane BH already
solved: `orig/45410914/band.exe` is the **decompressed retail PE**, so "does our
callee correspond to retail's" is a *byte comparison against retail's actual body*,
not an inference about `target_symbol_map.json`. **Folded bytes ARE our bytes**, so
the content oracle dissolves ICF without any alias map at all.

---

## 2. AXIS 1 — supply. The compile-out control, re-run at HEAD

`pair_funclets_by_bytes` **pass 2b** (`../objdiff/objdiff-core/src/diff/mod.rs:1554`)
pairs overflow target funclets **many-to-one** onto a base funclet another target
already owns, deliberately not marking it used:

```rust
let Some(&partner) = right_indices.first() else { continue };
for &l_idx in left_indices.iter() {
    if left_used.contains(&l_idx) { continue; }
    matches.push(SymbolMatch { left: Some(l_idx), right: Some(partner), …, masked_pairing: true });
    left_used.insert(l_idx);
    // Intentionally do NOT touch `right_used`
}
```

`matched_functions` counts **target** symbols (`report.rs:790, 841`), so N targets
sharing one base body credit **N**. Passes 1, 2 and 3 all consume their base partner
and are 1:1; **pass 2b is the only many-to-one path**, which is why "two target
symbols sharing one base partner index" is an exact detector.

**Control (this lane, current HEAD).** A private objdiff build with pass 2b behind an
env gate, both legs over identical objects:

| leg | `matched_functions` | `masked_equal_functions` |
|---|--:|--:|
| pass 2b ON | 40,540 | 1,517 |
| pass 2b OFF | 39,074 | 0 |
| **real inflation** | **1,466** | reported 1,517 |

The disclosure is a **conservative upper bound**, 51 high (0.13%): those 51 re-pair
onto a genuinely unused base funclet under pass 3 and still score 100%, so their
credit was *mis-attributed*, not *unsupported*. This reproduces the figure recorded
in `decomp-state-2026-07-19.md` (39,743 → 38,210, reported 1,582, gap 49) at a new
baseline — **the mechanism and its magnitude are stable, and the field tracks it.**

**All 1,517 are funclet-shaped. Zero are named.** Confirmed by direct classification,
not assumed — named symbols pair by name, which consumes the partner.

---

## 3. AXIS 2 — identity, jointly with axis 1. ★ The decisive table

`reloc_correspondence.py --census` over all **40,540** functions at exactly
`match_percent_normalized == 100.0`, cross-tabulated against the per-item
`masked_equal` bit. Permissive oracle reading:

| population | n | CORRESPONDING | NO_RELOCS | **DIVERGENT** | UNRESOLVABLE | other | **div %** |
|---|--:|--:|--:|--:|--:|--:|--:|
| named body | 19,126 | 11,010 | 1,733 | **1,046** | 5,011 | 326 | **5.5%** |
| funclet, supply-**backed** | 19,897 | 13,533 | 0 | **487** | 5,563 | 314 | **2.4%** |
| funclet, pass-2b **surplus** | 1,517 | 538 | 0 | **416** | 563 | 0 | **27.4%** |
| **all** | **40,540** | 25,081 | 1,733 | **1,949** | 11,137 | 640 | **4.8%** |

**Read the third row.** The 1,517 symbols that objdiff's *pairing-supply* accounting
flags are **27.4% divergent** — **11.4× the backed-funclet rate** and **5.0× the
named-body rate**. Two instruments that share no code, no input and no author — a
Rust pairing-arity counter and a Python content oracle reading retail's real bytes —
independently single out the *same 1,517 symbols* as the defective ones.

**The finding is invariant to oracle strictness.** Under `--strict-consistency`
(which demands ≥2 *named, non-funclet* supporters and demotes everything else to
undecided, in both directions) the same three-way split reads:

| population | n | CORRESPONDING | **DIVERGENT** | UNRESOLVABLE | **div %** |
|---|--:|--:|--:|--:|--:|
| named body | 19,126 | 10,972 | **814** | 5,281 | **4.3%** |
| funclet, supply-**backed** | 19,897 | 5,057 | **111** | 14,415 | **0.56%** |
| funclet, pass-2b **surplus** | 1,517 | 84 | **73** | 1,360 | **4.8%** |

The surplus:backed divergence ratio moves from 11.4× to 8.6× — the *magnitude*
changes, the *verdict* does not. That is the same invariance test laneBH applied to
its own headline.

That convergence is the strongest single result in this lane, and it cuts both ways:

1. **It vindicates the disclosure.** `masked_equal_functions` is not a bookkeeping
   curiosity; it selects a population that is genuinely, measurably wrong.
2. **It exonerates the other 19,897.** The supply-backed funclet population is the
   **cleanest stratum in the binary** at 2.4% divergent — cleaner than named bodies
   (5.5%), cleaner than the tree average (4.8%). **The 21,314 are not, in bulk,
   over-count.** The brief's worry that half the headline might be fictitious does
   not survive contact with retail's bytes.

### 3a. Why "89.1% carry a relocation difference" and "2.4% divergent" are both true

They measure different things and the gap between them *is* the ICF confound.

- **89.1%** (lane BO-5) = at least one relocation whose *target symbol name* differs.
  Measured through `objdiff-cli diff` in its default `DataValue` reloc mode, which
  compares names.
- **2.4%** = at least one relocation whose target **body in retail** differs from
  ours.

A folded destructor makes the first true and the second false, every time. Retail
keeps one name per folded VA and our map records that one name, so ~9 of every 10
name differences here are the naming artifact, not a wrong callee. **This is the
single most important correction in the lane: a differing callee NAME is nearly
worthless as evidence; a differing callee BODY is decisive.**

### 3b. Reconciling with lane BO-1's 64.6%

BO-1 reports that **64.6%** of strict-100 matches score against *any member of their
equivalence class*. Set beside BO-5's 52.9% and this lane's 4.8% divergent, the three
are not in tension — they answer three different questions:

| question | measure | value |
|---|---|--:|
| Is the credit **ambiguous** (would a twin also have scored 100)? | BO-1's twin gate | 64.6% |
| Was the credit formed by **byte identity rather than name identity**? | funclet-like @100 | 52.8% |
| Is the credit **unsupported** by a distinct compiled body? | `masked_equal` | 3.7% |
| Is the credit **pointing somewhere else in retail**? | reloc-correspondence | 4.8% (perm.) |

**Ambiguity is not wrongness.** A 12-byte adjustor thunk whose masked body is shared
by ~1,600 symbols is maximally ambiguous, yet if our source really does emit that
thunk and retail really does have it there, the match is real. BO-1's 64.6% is a
statement about *evidential strength*, and it maps onto this census's **UNRESOLVABLE**
bucket (27.5% permissive / larger conservative) — the honest "the binary does not tell
us" bucket — not onto DIVERGENT. **Do not add BO-1's 64.6% to any over-count figure.**
Its correct use is as a *confidence discount*, exactly as this lane's undecidable
band is used in §6.

The two numbers do agree on the thing that matters: both say the *function count* is
dominated by short, low-information bodies. That is why §6 recommends quoting bytes
alongside functions — funclets are **52.8% of matched functions but only 22.6% of
matched code** (824,648 of 3,652,368 bytes), and the whole pass-2b surplus is **1.44%
of matched bytes** (52,748 B; modal size 32 B, 1,099 of the 1,517).

### 3c. `.pdata` trap — not inherited

The census enumerates from `report.json` target symbols and from COFF symbol tables;
`.pdata` is never consulted for enumeration or for shape classification, in this
lane's tools or in `reloc_correspondence.py`. The frameless-leaf mis-bucketing that
bit four lanes is **not** present here. (`funclet_credit_audit.py` classifies shape by
objdiff's own `is_funclet_like` name predicate, reimplemented verbatim.)

---

## 4. VERDICT REQUESTED BY THE COORDINATOR — the unlanded +27

The held branch's +27 is described as "0 ported bodies scoring, 27 byte-paired
funclets", 100% reloc-masked twin, 96.3% funclet-shaped.

**Verdict: do not bank it on `Δmatched_functions`. Price it, and land only the
supply-backed remainder — if any.** The decision rule, which is now mechanical:

> **Price every landing by `Δ(matched_functions − masked_equal_functions)`, never by
> `Δmatched_functions`.** Both numbers are in every `report.json`. A landing whose
> honest delta is 0 is banking pass-2b surplus.

Applied to this shape specifically: a delta that is entirely funclet-shaped and
entirely byte-paired is *precisely* the population that splits 2.4% / 27.4% along the
supply line, and a set of 27 that arrives together in one span is far likelier to be
over-subscription overflow than 27 independently-backed funclets. Concretely, run in
the branch worktree:

```bash
rm -f build/45410914/report.cache && ./tools/ninja-locked
python3 - <<'EOF'
import json; m=json.load(open('build/45410914/report.json'))['measures']
print('honest =', m['matched_functions'] - m.get('masked_equal_functions',0))
EOF
```

against the same computation on the baseline. **If honest Δ ≈ 0, discard.** This is
also the standing gate this lane recommends for every future gap-absorption channel,
alongside laneBH's divergence-rate gate.

---

## 5. Deliverable 1 — the populated ICF alias map

`scripts/harvest/icf_alias_map_build.py`, ~12 s, four sources union-found by name
(folding is an equivalence relation):

| source | groups in | trust | what it is |
|---|--:|---|---|
| `dc3` | 2,882 / 12,844 names | ★★★ | **`../dc3-decomp/orig/373307D9/ham_xbox_r.map`** — a leaked *retail linker map* for the same Milo engine built by the same MSVC X360 toolchain with the same `/O1 /Oi /GR /EHsc`. Multiple names at one CODE address there are ICF folds **directly observed**. Caveat: dc3 is a newer engine build. |
| `laneab` | 25 / 69 | ★★★ | `docs/plans/laneAB-icf-tie-alternates-2026-07-26.json`, hand-verified tie groups |
| `hand` | 2 / 6 | ★★★ | `docs/plans/lane-bo8-icf-handverified.json` — BO-1's vtable-proved NetGameMsgs folds |
| `derived` | 3,321 classes over 338,471 symbols in 1,088 objs | ★★ | our own compiled objs: byte-identical bodies **with identical relocation shape AND identical relocation target names**. Conditional on our codegen. |

**Result: 1,407 anchored groups / 7,917 alias names** (vs the live map's **3 groups /
4 names** — a 469× increase), plus 2,509 un-anchored groups (15,373 names: real folds
where no member is in `target_symbol_map.json`; still valid `reloc_eq` buckets), plus
**200 CONTRADICTIONS**.

**Deliberate conservatism.** `derived` requires reloc *target names* to be equal, so
two bodies differing only by `bl A` vs `bl B` are **not** declared folded even when A
and B themselves folded. Computing that fixed point can only *add* groups, so the
emitted set stays sound. Funclets are excluded (their masked bodies are identical
binary-wide, so folding claims about them are vacuous); trailing COMDAT pad is
trimmed; bodies under 8 bytes are dropped as vacuous keys.

### 5a. ★ The contradictions list is the more valuable half

A **contradiction** is a class where our evidence says one body but
`target_symbol_map.json` places two or more members at *different* VAs. Either our
codegen diverges there, or **the map is mispaired**. 200 of them, and the channel
validated itself immediately:

> The very cluster lane BO-1 proved wrong by hand — `0x82690A10` / `0x82690B28`
> serving `Save`/`Load` for `ComponentFocusNetMsg`, `SetUserTrackTypeMsg` and
> `SetUserDifficultyMsg` — **appears in this list, derived mechanically and with no
> knowledge of BO-1's work.** Our own objs make `?Save@SetUserTrackTypeMsg` and
> `?Save@SetUserDifficultyMsg` byte+reloc identical (76 B, 2 relocs), and likewise
> the two `Load`s, while the map puts them at two different VAs.

Given the standing memory that **43% of one worklist and 76% of another were map
mispairs**, and that a mispaired pin can drive a "fix" that breaks correct source, a
200-item mechanically-derived mispair list is a first-class artifact. Sample:

```
??_G?$ObjPtr@VRndPropAnim@@@@UAAPAXI@Z            0x82665d98
??_G?$ObjRefConcrete@VRndPropAnim@@VObjectDir@@@@UAAPAXI@Z  0x8228ea88
```

**★ Handed to the `target_symbol_map.json` owner** (this lane does not edit that
file): `0x82690B28` is currently `?Save@SetUserDifficultyMsg@@UBAXAAVBinStream@@@Z`.
BO-1's vtable-slot proof says that body is **`Load`** — so the pin is wrong about the
**method** as well as arbitrary about the **class**.

### 5b. What the map is and is not for

- **Not** for `matched_functions` — measured 0 (§1). Do not wire it expecting movement.
- **Yes** for shrinking the census's UNDECIDABLE bucket, which is the dominant
  uncertainty in §6 and is dominated by exactly this ambiguity.
- **Yes** for `[sym]` noise in `objdiff-cli diff` runs in a name-comparing reloc mode.
- **Yes** as the discriminator that byte-search locators structurally lack — per
  BO-1, a locator cannot tell "my code is here" from "my code was folded into another
  TU's span," and a registered alias group is exactly that missing information.

---

## 6. THE HONEST FLOOR, and how to quote it

Two disjoint deductions from the in-worktree **40,540**:

| | permissive | conservative |
|---|--:|--:|
| `matched_functions` | 40,540 | 40,540 |
| − axis 1: pass-2b surplus (unsupported) | −1,517 | −1,517 |
| − axis 2: DIVERGENT among the *supported* | −1,533 | −925 |
| **= corrected count** | **37,490** | **38,098** |

**Quote the band: `matched_functions` honestly reads 37,490 – 38,098, i.e. the
headline over-states by 6.0% – 7.5%.** The permissive end is "how much *could* be
wrong"; the conservative end withdraws every verdict resting on the weakest
(consistency) oracle in both directions. **29.1% (permissive) to 53.5%
(conservative) of the population remains undecidable** — `.bss` statics and externs
with no bytes in either image. That is a property of the evidence, **not** a
suspicion; never fold it into the over-count, and never quote the evidenced fraction
(66.1% / 44.0%) as if the remainder were fake.

This **supersedes** the "over-counts ~4%" line in
`docs/plans/decomp-state-2026-07-19.md` §"QUOTE THE HONEST FLOOR". That figure was
correct *for axis 1 alone*, and axis 1 alone still reads 3.7% here. It is incomplete
because it also asserts "**No real decompilation is affected** — named-function
matches are structurally unreachable by this pass." True of axis 1; **false overall.**
Axis 2's divergence is *worse* among named bodies (5.5%) than among supply-backed
funclets (2.4%), and named bodies are where real decompilation lives. The clean claim
is narrower: *pass-2b surplus never touches named functions.*

### Recommended quotation going forward

1. **Per-build headline = `matched_functions − masked_equal_functions`** (currently
   **39,023**). It is in every `report.json`, needs no census, is cheap and monotone.
   Be honest about what it is: it corrects **axis 1 only**, so it is an *upper*
   bound on the audited band above, not a substitute for it. Quote it as
   "≥ 37,490 honest (39,023 before the identity correction)" — or just quote the band
   when a method can be stated.
2. **Quote matched *bytes* alongside functions.** Funclets are 52.8% of the function
   count but 22.6% of matched code; the function count is dominated by 32–48 B
   compiler-generated bodies with no source-level existence. `matched_code`
   (3,569,916 B) is the metric that tracks decompilation *work*.
3. **Price every landing by `Δ(matched_functions − masked_equal_functions)`.**
4. **Re-run the full audit per wave**, not per landing: `funclet_credit_audit.py` is
   seconds once a census exists, and the census is ~10 min.

---

## 7. Method, limits, and traps avoided

- **Traps avoided.** No `.pdata` enumeration (§3c). No `va - 0x82000000` mapping —
  the content oracle uses the parsed PE section table. No split-forcing build, so the
  `symbols.txt`-drift ±2 hazard is not in play; every A/B in this lane is a
  `report generate` over **byte-identical objects**, which is immune to it. No
  `class_layout_report.py`. Main was never written to.
- **`masked_equal_functions` is an upper bound**, measured 51 high of 1,517 (3.4%
  relative). The floor is therefore ~51 pessimistic on axis 1.
- **The census's own calibration is laneBH's**, including a falsification control
  (95.2% of CORRESPONDING verdicts flip when target relocations are rewritten) and a
  23-function positive control with zero false CORRESPONDING. This lane inherits
  those controls and adds no new oracle.
- **The conservative leg is not "the truth"** — it demotes weakly-supported verdicts
  in *both* directions, so it lowers DIVERGENT and raises UNDECIDABLE together. Quote
  the band.
- **Not measured here:** whether pass-3 fuzzy pairings that reach 100% pair the
  *right* funclet. They are 1:1 and supply-backed, so they cannot inflate the count;
  a wrong choice would show up in axis 2, where the backed population reads 2.4%.
- **`derived` is conditional on our codegen.** A class it emits is a claim about *our*
  build; if our body diverges from retail, retail may not have folded that pair. This
  is why `dc3` (directly observed) outranks it and why contradictions are quarantined
  rather than resolved automatically.
