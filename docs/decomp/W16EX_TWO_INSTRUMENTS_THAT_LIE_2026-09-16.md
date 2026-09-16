# W16-EX — two instruments that lie, and a map that does not

**Lane subject: tooling that returns a confident wrong answer.** Not a byte lane.
Whole-binary Δ is **0 by construction and that is the correct outcome** — neither
patched tool has a build edge, and the one ruler-path edit is inert by design.
Pre-registration: `W16EX_PREREGISTRATION_2026-09-16.md`.

Standing rules this lane instantiates: *a metric that hides real bugs is worse than
a lower metric*; *a tool's confident "clean"/"unfixable" closes veins nobody
reopens*; *a gate that cannot fail is worth nothing*.

---

## 1. `class_layout_report.py` audited a header it had not measured — REPRODUCED

The brief labelled this UNVERIFIED (reported by W16-EV). It reproduces exactly.

```
# headers declaring MetaPerformer: ['src/meta_ham/MetaPerformer.h',
                                    'src/band3/meta_band/MetaPerformer.h']
# ... /d1reportSingleClassLayoutMetaPerformer src/band3/meta_band/MetaPerformer.cpp
=== header comment audit: src/meta_ham/MetaPerformer.h ===
  all // 0xHEX comments agree with the compiler
```

The layout is measured from the **live** band3 TU; the comments are audited in the
near-dead DC3-era **meta_ham** header. This matters because this script is the
**authoritative** layout oracle in this repo — the `// 0xHEX` comments and
everything derived from them (`struct_db.sqlite`, `lookup_struct_offset`) are
merely *derived* — so a false clean here is load-bearing, and `--fix-header` would
have **written** to the wrong file.

### Root cause — sharper than "it picks the wrong header"

`resolve_tu` and the header audit walked the **same candidate list under different
selection rules**, and disagreed exactly when the audit's pick had no compiled TU:

| path | rule | landed on |
|---|---|---|
| `resolve_tu` | scan **all** declaring headers for one with a compiled same-stem `.cpp` | `band3/meta_band/MetaPerformer.cpp` OK |
| `--check-header` | `find_header(...)[0]`, tiebreak *stem==class, then SHORTEST PATH* | `src/meta_ham/MetaPerformer.h` WRONG (27 chars vs 35) |

There is **no** `src/meta_ham/MetaPerformer.cpp`, which is precisely why the two
diverged: `resolve_tu` fell through to the live header, the audit did not.

### A SECOND, independent defect, found while confirming the first

`"all // 0xHEX comments agree with the compiler"` was printed whenever the bad-row
list was empty — **the same sentence for a real clean and for a run that compared
nothing**. Both silent-zero paths are live: `class_body_span` returning `None` bails
to `[]`, and a comment row whose identifier is absent from the member set was
skipped without a word. Measured on this very case:

| header audited | rows compared | rows unchecked | printed |
|---|---:|---:|---|
| `band3/meta_band/MetaPerformer.h` (live) | **32** | 0 | all agree |
| `meta_ham/MetaPerformer.h` (what the bug audited) | **1** | **43** | *"all comments agree"* |

=> The false clean was resting on a denominator of **one of forty-four**, in
language indistinguishable from the live header's genuine 32/32. This is the
vacuous-instrument family again (`all([])`; the grep-binary false negative; a
single-candidate gate that cannot fail).

### Fix

* **`choose_audit_header(headers, tu_src, tu_text)`** — PURE (so the pin is
  offline). A header is eligible only if the TU that produced the layout **reaches**
  it: same-dir same-stem, or a direct `#include` matched against every legal
  path-suffix spelling (include roots vary, so hardcoding them would rot). Anything
  other than exactly one survivor **REFUSES**, exit **4**, naming every candidate
  and both escape hatches. Deliberately **direct** includes only: resolving
  transitive inclusion needs the preprocessor, and guessing there would reintroduce
  the same silent mispick. *A real "I cannot answer" outranks a plausible wrong
  answer* — the same doctrine as the three-label contract already in this file.
* **`audit_header_counted()`** discloses `(seen, compared)`. `0 compared` prints
  **`NOTHING CHECKED`**, never a clean. `audit_header` survives as a wrapper so the
  two tree-wide sweeps keep working.
* `--header <path>` to disambiguate explicitly; `--ambiguity-census` for the blast
  radius (reproducible, so it cannot rot the way a pasted list would).

### The pins — and the proof they can fail

Legs **G** and **H**, modelled on existing legs D and F: **offline, deterministic
and self-sabotaging** — each reconstructs the OLD rule and *requires it to give the
wrong answer*, so the leg cannot pass on a tool where the input happened to be
empty. Proven able to fail: reverting `choose_audit_header` to `hdrs[0]` turns G
**red** (it picks `meta_ham` and stops refusing). Selftest **7/7**.

### Blast radius

`--ambiguity-census`: **110 of 2,910** class names declared under `src/**` are
ambiguous *under `find_header`'s own predicate* (so it measures this tool's
ambiguity, not "ambiguity in C++"). Of those, **12** have >=2 candidates whose stem
equals the class name — for those the surviving tiebreak is `len(path)`, a
criterion with **no semantic content whatsoever**:

> `BPMDetect, DateTime, FIFOSampleBuffer, FIFOSamplePipe, Instarank, Key,
> MetaPerformer, MeterDisplay, MiniLeaderboardDisplay, PitchDetector, SoundTouch,
> Stream`

=> **Any past `--check-header` clean on one of the 110 is UNPROVEN, not clean.**

### The same defect in both tree-wide sweeps

`tools/header_offset_audit.py` took `hdrs[0]` behind a comment *rationalising it*
("Take the best hit"), and `tools/header_offset_audit_par.py`'s index returned
**one header per class**, making the ambiguity literally unrepresentable. Both
already held the TU and did not use it. Both now resolve against it and **report**
refusals — trading a false clean for a silent skip is the same disease. The par
index now returns ranked candidate **lists** (84 classes with >1 candidate; lower
than 110 because it excludes `stlport`/`xdk` — a different denominator, stated).

WARNING: the dangerous direction for a *sweep* is not the false clean but the
**false finding**: auditing header A against class B's layout can flag a
**correct** comment, and a human then "fixes" ground truth. That is the exact
silent-corruption shape `audit_header`'s own docstring already documents twice.

### One brief figure REFUTED-as-stale (not wrong)

The brief's *"the live band3 header had **16 wrong rows**"* does **not** reproduce:
today it is clean, **32/32 compared**. Reconciled — commit `d3901634` *"W16-EV: fix
30 stale // 0xHEX offset comments in 5 game headers"* lists `MetaPerformer.h 16`.
W16-EV's observation was true when measured and was repaired **by W16-EV's own
commit**. The figure is stale, not false. (Same shape as `CLAUDE.md`'s
CharEyes/SaveLoadManager note, which advertised defects that had already been
fixed — and which that note itself now flags.)

---

## 2. `target_symbol_map.json` — the file is clean; three censuses were not

The brief labelled its Task-2 figures **VERIFIED**. Re-measured at `2d9c4ead`,
**the two headline defects do not exist**, and both are artifacts of a census that
does not know the file's schema.

### The schema (three value kinds)

| shape | meaning |
|---|---|
| `"0xADDR": "?Mangled@@..."` | a real row — **the only thing here that is a NAME** |
| `"0xADDR": null` | deliberately unclaimed; the renamer **skips** it. 101 rows. A record, not rot. |
| `"_prefixed": [...] / "..."` | metadata, **never** address->name. `_icf_arbitrary`, `_bijection_arbitrary`, `_denylist`, `_denylist_unadjudicated` are **lists of ADDRESSES**; `_internal_linkage_allow` is a list of names; every `*_comment` is prose. 16 such keys. |

**The applied convention** — implemented identically by both consumers
(`obj_target_symbol_renamer.load_address_map`, `tools/map_name_injectivity.py`):
`key.lower().startswith("0x") and isinstance(v, str) and v`.

### Refutations

* **`"0x826101b8"` is NOT "an address masquerading as a symbol".** It is **never a
  VALUE anywhere in the file**. As a **KEY** it maps to a legitimate
  `hash_map<int,UIComponent*>` destructor. Its two "duplicate" appearances are
  memberships in `_icf_arbitrary` and `_bijection_arbitrary` — **lists of
  addresses**, the one place an address belongs. **Not a defect; "fixing" it would
  have corrupted a provenance record.** (It is the sole address in both lists; both
  carry the same *treat-as-UNRESOLVED* doctrine, so the union semantics is identical
  and there is nothing to reconcile. Left untouched: *do not fix a count into
  agreement*.)
* **`?NodeCmp@@YAHPBX0@Z x3`** is 2 real rows + 1 entry in `_internal_linkage_allow`
  — the allow-list that **licenses** the duplicate. `_internal_linkage_allow_comment`
  documents it as *measured* legitimate: a file-static qsort comparator in
  `DataArray.cpp` and `BandWardrobe.cpp`, both honest, both 100%. A static free
  function is not a COMDAT and is not deduped by the linker.
* **The count is not "convention-dependent" and open to taste.** The convention is
  already pinned **in code and wired into ninja**. The gate reports:

  ```
  [map-injectivity] OK: 29435 applied rows, 29434 distinct names, injective
                        (+1 enumerated internal-linkage exception(s))
  ```

  The only other duplicate (`??$__destroy_aux@ULevelData@@...` x2) is in `_denylist`,
  **not applied**, and is correctly reported informationally rather than as a failure.

=> **There was no map-content defect to fix.** The real gap was that the schema was
undiscoverable, which is what produced three wrong censuses in three lanes.

### What was changed instead

1. **`tools/map_name_injectivity.py --selftest`** gained a **schema-filter leg**.
   Every pre-existing leg ran on an already-parsed dict, so none of them pinned the
   one step all three lanes got wrong. The leg **self-sabotages**: it reconstructs
   the naive flatten and *requires* it to manufacture the phantom
   (`"0x82000040" in naive` -> True) and to over-count the licensed duplicate (3
   instead of 2). **14/14.**
2. **`_schema_comment`** added to the map itself, stating the three value kinds, the
   applied convention, the two refutations, and *"run the gate, do not re-derive
   this by hand"*. Placed first so it is read before anything else.

---

## 3. Measurement

`scripts/target_symbol_map.json` is a **RULER PATH**, so the edit was measured with
`tools/ab_measure.py --from-dirty` (which forces the re-split and iterates to a
`symbols.txt` fixed point — an un-resplit map edit is INERT, and one forced split
per leg under-reports bytes). **Predicted Δ0 before measuring**, because both
consumers filter on the `0x` key prefix; the measurement is therefore a **test of
that filter**, not a scoring play. A non-zero delta would have meant the
`_`-prefixed key was reaching a consumer.

```
  leg A: matched=43978 masked=23224 honest=20754 code%=40.320374
  leg B: matched=43978 masked=23224 honest=20754 code%=40.320374
  Delta matched=+0  masked_equal=+0  honest=+0  code%=+0.000000pp  code_bytes=+0
  Delta fuzzy=+0.000000pp   (legA 50.014150 -> legB 50.014150)
  units at 100% [mpn]:   189 -> 189  (0 reached 100, 0 fell off)
  units at 100% [fuzzy]: 169 -> 169  (0 reached 100, 0 fell off)
```

**And it is NOT absent-vs-absent.** Leg B reports `split=1` and
`renamer_patched=1830`, and both legs read at a `symbols.txt` split fixed point, so
the edited map really was driven through the ruler path and still moved nothing —
which is the whole point. The `none` control is flat as well.

### A hazard that fired on this lane, in real time

This doc was first written **into the worktree while the A/B was running**, and
`ab_measure`'s restore **deleted it**, naming it on the way out:

```
  D docs/decomp/W16EX_TWO_INSTRUMENTS_THAT_LIE_2026-09-16.md
  These were created DURING the run, so the restore removed them.
```

The brief warned about exactly this and it still happened — the warning was read
as a rule about *scratch files*, not about the deliverable. Write run-time
deliverables to `~/tmp` and copy them in afterwards. Credit where due: the tool
**told me**, by name, instead of silently losing it.

---

## 4. What this lane did NOT do

* **Did not touch `_icf_arbitrary`/`_bijection_arbitrary`/`_denylist` membership.**
  The brief correctly warns that a duplicate name is not automatically a bug (ICF
  folding and genuine template instantiations legitimately share spellings) and that
  these must be adjudicated on **retail bytes**. Nothing here was adjudicated on
  retail bytes, so nothing was removed.
* **Did not resolve transitive includes** in `choose_audit_header`. Refusal is the
  deliberate fallback; guessing is what caused the defect in the first place.
* **Did not re-run the tree-wide header sweep** under the fixed selection. That is
  the obvious follow-up and it is now *possible* (both sweeps report refusals), but
  it is a multi-hour compile campaign and its product would be a new census, not a
  fix. **The 110 ambiguous names remain UNPROVEN, not audited.**
* **Did not audit the remaining 109 ambiguous classes individually.** Each costs one
  TU compile (~4 min here, up to ~50 min under fleet load).
* **Did not verify that the two `MetaPerformer` classes cannot both be complete in
  one TU.** If they ever were, `parse()` keys `classes` by name and one would
  silently overwrite the other. Out of scope here; the refusal covers the reporting
  side, not the parsing side.
