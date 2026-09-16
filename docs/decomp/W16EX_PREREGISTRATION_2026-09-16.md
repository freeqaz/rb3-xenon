# W16-EX pre-registration (written BEFORE any fix or measurement)

Lane subject: **tooling that returns a confident wrong answer**. Per the standing
rule that *a metric that hides real bugs is worse than a lower metric*, and that
*a tool's confident "unfixable"/"clean" closes veins nobody reopens*, this lane
lands on **correctness, not bytes**.

## Pre-registered predictions

1. **Task 1 reproduces.** `class_layout_report.py --check-header MetaPerformer`
   measures the layout from the LIVE TU (`src/band3/meta_band/MetaPerformer.cpp`)
   and audits the DEAD header (`src/meta_ham/MetaPerformer.h`), reporting clean.
   [CONFIRMED before this commit — transcript in ~/tmp/w16ex_repro_metaperformer.txt]

2. **Δ0 on the whole-binary metric is the EXPECTED and CORRECT outcome.**
   `class_layout_report.py` is an analysis tool with no build edge: it is not in
   `build.ninja`, does not emit an object, and is not part of the ruler. A
   non-zero delta from the Task-1 patch would mean I had changed something I did
   not intend to. **Do not read this lane's Δ0 as an inert patch.**

3. **The map-file edit (Task 2) is predicted Δ0 = 0 on every key**, because both
   consumers filter to `key.lower().startswith("0x") and isinstance(v, str)`.
   This is a *test of that filter*, not a scoring play: if a `_`-prefixed comment
   key were NOT filtered, the measurement would be non-zero or the build would
   crash. Measured with `tools/ab_measure.py --from-dirty` (forces the re-split
   and iterates to a `symbols.txt` fixed point; the file is a RULER PATH and an
   un-resplit map edit is INERT).

4. **Blast radius, pre-registered before the fix is written:** 110 of 2,910 class
   names declared in `src/**` are ambiguous under `find_header`'s OWN predicate,
   and **12** of those have >=2 candidates whose stem equals the class name — for
   those the surviving tiebreak is `len(path)`, which carries no semantic content.
   Any past `--check-header` clean on one of the 110 is **unproven, not clean.**

## Pre-registered REFUTATIONS of my own brief (Task 2)

The brief labels these VERIFIED. Re-measured at `2d9c4ead`, they are artifacts of
a census that does not know the file's schema:

- `"0x826101b8"` is **NOT** "an address masquerading as a symbol". It is never a
  VALUE anywhere in the file; as a KEY it maps to a legitimate `hash_map` dtor.
  Its two "duplicate" appearances are memberships in `_icf_arbitrary` and
  `_bijection_arbitrary`, which are **lists of addresses** — the one place an
  address belongs. **Not a defect. "Fixing" it would corrupt provenance records.**
- `?NodeCmp@@YAHPBX0@Z x3` is 2 applied rows + 1 entry in
  `_internal_linkage_allow` — the allow-list that *licenses* the duplicate, which
  `_internal_linkage_allow_comment` documents as measured-legitimate (a file-static
  qsort comparator in DataArray.cpp and BandWardrobe.cpp).
- The "convention-dependent 2/3/4" disagreement is not open: the convention is
  **already pinned in code and wired into ninja** as `tools/map_name_injectivity.py`,
  which reads exactly the applied rows. It reports **injective**.

**Predicted outcome of Task 2: no map-content defect exists to fix.** The real
gap is that the schema is undiscoverable, which is what produced three wrong
censuses in three lanes. Deliverable is therefore documentation + a pin, not a
data edit.
