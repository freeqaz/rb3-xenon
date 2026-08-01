# Destructor map-row audit (lane CG-1, 2026-08-01)

Instruments for adjudicating `??1` / `??_G` / `??_E` rows in
`scripts/target_symbol_map.json`. Built because a destructor row can score
**fuzzy-100 while naming the wrong function**: objdiff hard-sets relocation
args to `None` (`report.rs:394`), and a `??_G` body's *only* discriminating
content is the `bl` to the real destructor. The metric is structurally blind
to exactly the bit that identifies the row.

## Files

- `retail_reader.py` — VA→file-offset via the **real PE section table** (never
  `va-0x82000000`, which is only valid for `.rdata` on this image), plus a
  PowerPC I-form branch decoder. `--selftest` runs positive controls, a
  fail-on-demand leg (out-of-section VA must return `None`), and hand-computed
  branch-decode vectors.
- `rtti_cohort.py` — builds the destructor-row cohort and tests each class for
  a retail `??_R0` TypeDescriptor name string. `--selftest` proves the class
  extractor can both hit and miss.
- `adjudicate.py` — the working discriminator: read the retail body, extract
  `bl` targets (recursively dethunked), and ask the map what they are.

## Results and traps (read before reusing)

**"Class absent from retail RTTI" is NOT by itself a defect signal.**
Untreated base rate across all 2,396 destructor rows: **29.2% absent**
(46.4% for plain `??1`). Non-polymorphic classes never get a TypeDescriptor,
so absence is the *norm*. It only becomes selective when restricted to
polymorphic (`??_G`/`??_E`) non-template classes: there **93.1% do have RTTI**
and the 6.9% residue is a real outlier set.

**RTTI-absence and non-virtual `??_G` mangling are the SAME fact, not two
instruments.** A `??_G...@@QAA...` (`Q` = public non-virtual) names a
non-polymorphic class, which is *why* it has no RTTI. Treating the agreement
of these two as corroboration manufactures fake consensus. Verified: our own
compiler **does** emit `??_GChannelData@@QAAPAXI@Z` (found in `MasterAudio.obj`),
so "non-virtual `??_G` is impossible" is FALSE.

**A vtable-reference detector for `??_G` is DEAD.** "Is this address referenced
by a data word?" fires on **58.7% of 1,000 random `.pdata` function starts**
vs 67.6% of the cohort — enrichment 1.15x. ⚠ The tempting null (shift each
address by +0x40 → 1.5%) makes it look 45x enriched. The null must reproduce
the state met in the wild (*function starts*), not an adjacent one.

**Forwarder thunks are structurally unidentifiable by the default metric.**
3,073 mapped rows (11.2%) have a forwarder body; **2,649 (86.2%) are
byte-identical to another thunk except the branch target, which objdiff
masks.** The largest single group is **1,055 rows** sharing
`lwz r11,-4(r3); add r3,r11,r3`. Any two in a group can be swapped with zero
metric movement.

**Dethunk must be recursive AND size-independent.** Adjustor thunks have **no
`.pdata` entry**, so a `.pdata`-driven size lookup returns `None` and the
dethunk silently bails — which made the thunk stratum read 0/17 and 0/367
("no information") rather than "instrument broken". Use a fixed-window read.
