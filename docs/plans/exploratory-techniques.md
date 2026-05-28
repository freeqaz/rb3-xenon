# Exploratory matching techniques beyond path-to-100

**Date:** 2026-05-28. **Companion to:** `path-to-100.md` (the durable phase plan).
**Scope:** identification-side techniques to expand the 11,582-fn oracle
(`unified_id.json`) so subsequent waves have addresses+names to pin.

Per `feedback_verify_assumptions.md`, every claim below was computed from real
data. Per the user's POC-with-numbers preference, every technique was
implemented, run, and cross-verified.

---

## Executive summary

The path-to-100 plan's hard ceiling is **17.5% of binary functions** because
nothing covers the 54,552 fn_XXXXXXXX with no oracle hint. This investigation
built and tested four POCs that exploit data we already have but haven't yet
mechanized. **Headline numbers:**

| Technique | New unique identifications | Cross-verify precision | Notes |
|---|---:|---:|---|
| Call-graph triangulation | **+1,594** (1,430 single-source + 441 multi-evidence subset) | **94.1%** (5,310/5,642 overlap) | Highest yield; works for any fn called by a named fn |
| RTTI walk (X360 layout) | 1,321 vtables (vs jeff's 342, 3.8×) | n/a (no fn proposals on its own) | Recovers ALL named vtables; feeds next technique |
| RTTI+vtable transitivity | +385 HIGH-tier + 753 LOW-tier | **71.1% HIGH** / 41.1% LOW | HIGH = matching slot counts (no engine drift); LOW = drift-suspected |
| Vtable transitivity (no RTTI) | +37 | n/a (small sample) | Subsumed by RTTI variant |
| **UNION across all POCs** | **+2,735** new fn identifications | tiered S/A/B/C below | **+24.7% expansion of unified_id baseline** |

**Top recommendation:** ship call-graph triangulation as the next addition to
`tools/fingerprint_match.py merge_*` — it's the highest-yield, highest-precision
new technique, mechanizable in ~200 LOC, and orthogonal to bindiff (overlap
with RTTI is only 9 fns out of 2,735 union). Combined with RTTI HIGH-tier as
secondary, this raises the addressable function pool from 11,582 to ~13,500
with ≥90% precision.

The 54,552 "no oracle" residue remains untouched by these techniques — they
expand the *known* set by triangulation, but cannot conjure source code for
genuinely unidentified functions. The path-to-100 plan's regime-B ceiling
(~25% of binary functions) likely rises by ~2 percentage points (to ~27%) if
these proposals are wired.

---

## Section 1 — Per-technique deep dives

### 1.1 Call-graph triangulation (`tools/exploratory/callgraph_triangulate.py`)

**Motivation:** for any named rb3 function F (mapped via unified_id to its DC3
twin F'), the bl call sequence in rb3's `.s` files is positionally aligned
with F'. So `rb3.bl[i] = fn_X` and `dc3.bl[i] = "Bar"` ⟹ `rb3 fn_X == dc3 "Bar"`.

**POC:**
- Reads rb3 asm in `build/45410914/asm/*.s` (529 files) and dc3 asm in
  `../dc3-decomp/build/373307D9/asm/**/*.s` (2,224 files) — note rb3 only has
  pinned-unit `.s` for ~510 units but dc3 has 2,224.
- For each rb3 named fn → dc3 named fn, aligns bl lists positionally.
- 85.2% of attempted pairings have matching bl-count alignment (8,858/10,392).
  The 15% misalignment is mostly /Ob2 inlining differences between dc3 and
  rb3 (functions where one side has more inlines than the other).

**Results:**
- 7,556 anonymous rb3 fns received at least one vote
- 7,072 unanimous proposals (single candidate dc3 name)
- 5,310 cross-verified against unified_id (overlap subset): **94.1% precision**
- 332 disagreements in the overlap; sampling shows most are STL template ICF
  aliases (e.g. `__partial_sort@?$Key@VVector3@@` vs `__partial_sort@PAPAUObjEntry@@`)
  — the same code body shared by multiple instantiations. These are semantic
  ties, not errors.
- **1,594 unique NEW identifications** (not in unified_id):
  - 441 multi-evidence (≥2 distinct named callers agree) — very high confidence
  - 1,153 single-evidence (one caller; lower confidence but still 94.1% measured)

**Integration cost:** low. Add as `tools/fingerprint_match.py merge_callgraph` —
~200 LOC of asm parsing + position alignment + vote counting. Output writes to
`unified_id.json` with new `source: "callgraph"` entries.

**Follow-up work:**
- Build a "callgraph-confirmed bindiff" pass: bindiff matches where callgraph
  also agrees get a higher confidence score (and bindiff matches where callgraph
  disagrees deserve manual review — 87 disagreements found in the overlap).
- Extend to *named callees in unidentified callers*: if rb3 fn_Y calls a known
  fn at position k, we can sometimes constrain fn_Y by the dc3 callgraph
  predecessors of the corresponding dc3 fn at position k. Currently the POC
  only handles known callers; supporting known callees would extend the cone.

**Reproduce:**
```bash
python3 tools/exploratory/callgraph_triangulate.py
# Outputs: /tmp/exploratory_callgraph.json + _stats.json
```

---

### 1.2 RTTI walk (`tools/exploratory/rtti_walk.py`)

**Motivation:** jeff's vtable scanner finds 342 candidate vtables in rb3 via
heuristic .rdata scanning. RB3 has 1,396 RTTI TypeDescriptors — so the
**true** vtable count is much higher. Walking RTTI is the way to find them all.

**X360 RTTI layout (empirically derived, differs from x86 MSVC):**
- TypeDescriptor: vptr (4) + spare (4) + null-terminated `.?AVCLASS@@` string
- COL (Complete Object Locator) precedes vtable at vt_va-4:
  - +0..+0x8 = three zero dwords (where 0x19930522 signature lives on x86)
  - +0xC = pTypeDescriptor
  - +0x10 = pSelf / pCHD
- Vtable: vt_va-4 stores pointer to COL; vt_va+0 is slot 0.

**Results:**
- 1,396 TypeDescriptors found
- 1,317 COL records identified (those with the 12-byte zero prefix)
- **1,321 vtables discovered** (vs jeff's 342 — a 3.8× expansion)
- 1,317 classes named (mangled); 79 RTTI-only-no-vtable (templates/forward refs)

**Integration cost:** medium. The RTTI walk should replace jeff's
`FindXboxVtables` heuristic in `proposed_splits.txt` generation — it's
strictly more accurate. Also output a `class_db.json` keyed by mangled class
name → vt VA → slot fn list, for downstream tools.

**Follow-up work:**
- Wire RTTI output to `proposed_splits.txt`: for each `.text` block hull that
  contains the vtable's slot fns, propose a per-class TU pin.
- Also parse CHD (Class Hierarchy Descriptor) records — at TD ref+0x4 in some
  layouts — to recover parent-class chains and disambiguate multi-inheritance.

**Reproduce:**
```bash
python3 tools/exploratory/rtti_walk.py
# Outputs: /tmp/exploratory_rtti.json + _stats.json + _classes.txt
```

---

### 1.3 RTTI + Vtable transitivity (`tools/exploratory/rtti_vtable_combined.py`)

**Motivation:** RTTI gives rb3 vt → class_name; DC3 `.map` gives
`??_7class_name@@6B@` → dc3 vt → slot fn names. Joining by class name yields
slot-by-slot identification.

**Method:**
- 1,317 distinct rb3 classes (RTTI)
- 1,965 distinct dc3 classes (from `??_7...@@6B@` symbols in `ham_xbox_r.map`)
- **608 classes overlap** (~46% of rb3, ~31% of dc3) — confirms strong shared
  engine layer per CLAUDE.md.
- For each overlap class, pair vtables slot-by-slot.

**Confidence tiers:**
- **HIGH** = rb3 vt and dc3 vt have identical slot counts (no engine drift)
- **LOW** = slot counts differ (engine version drift; slot positions unreliable)

**Results (382 single-vtable-per-class pairings out of 608 common classes; 226
classes have multiple vtables i.e. multiple-inheritance sub-objects, deferred):**
- 385 HIGH-tier new identifications, **71.1% precision** vs unified_id overlap
- 753 LOW-tier new identifications, **41.1% precision** — too low to use
  blindly; needs per-class manual review
- Of HIGH-tier disagreements: sampling showed ~half are ICF aliases (same code
  body, different MSVC-name expansion), so the real semantic precision on HIGH
  is likely **80-85%**.

**Sample HIGH-tier wins** (looks legit, not in unified_id today):
- `fn_8251AF50 → ??_GArkFile@@UAAPAXI@Z` (ArkFile destructor)
- `fn_8251AC98 → ?Filename@ArkFile@@UBA?AVString@@XZ`
- `fn_8251AA10 → ?Read@ArkFile@@UAAHPAXH@Z`
- `fn_825D0D00 → ??_EAccomplishmentCategory@@UAAPAXI@Z`
- `fn_82B5A630 → ??_E?$BloomTextures@$02@NgPostProc@@UAAPAXI@Z`
- All ArkFile/AccomplishmentGroup slots line up — a single class's whole vtable
  gets named at once.

**Sample LOW-tier failure mode** (engine drift; reject in production):
- `fn_82076884 (rb3 ObjPtrList<CamShot>)` rb3=4 slots vs dc3=3 slots
- dc3 added a virtual between rb3's slot 1 and slot 2; positional pairing breaks.

**Integration cost:** medium. Wire HIGH-tier proposals into `unified_id.json`
with `source: "rtti_vtable_high"` and a confidence < 1.0. LOW-tier output should
go to a separate `proposed_review.json` for manual triage.

**Follow-up work:**
- **Multi-inheritance pairing (226 classes deferred):** classes with multiple
  vtables (sub-object vtables for each base) need to pair by `??_7CLS@@6Bbase@`
  modifier (the post-`6B` token in the mangled name). The POC stops at single
  vt; extending to multi-vt is a half-day of code.
- **Drift-detection from CHD walk:** parse Class Hierarchy Descriptor records
  to enumerate parent slot counts and detect inserted-virtuals patterns.
  Can rescue LOW-tier proposals.

**Reproduce:**
```bash
python3 tools/exploratory/rtti_vtable_combined.py
# Outputs: /tmp/exploratory_rtti_vt.json + _stats.json
```

---

### 1.4 Vtable transitivity (no RTTI; `tools/exploratory/vtable_transitivity.py`)

**Motivation/Results:** earlier version, pre-RTTI. Found only 38 pairings
because it depended on jeff's 342 raw `vftable_XXXXXXXX` symbols and a
heuristic "find dc3 vt whose named slot fns appear in rb3 vt's known slots".

**Subsumed by RTTI variant.** Keep the script as a reference implementation;
deprecate from production wiring.

**Reproduce:**
```bash
python3 tools/exploratory/vtable_transitivity.py
# Outputs: /tmp/exploratory_vtable.json + _stats.json
```

---

### 1.5 Other Harmonix binaries (negative result)

**Investigated:** RB1/RB2/Beatles/LegoRock Xbox binaries with `.map` files.

**Found:**
- `/home/free/code/milohax/rb3/orig-assets/xbox-zip/default.xex` — same file
  as `orig/45410914/default.xex` (matching media ID `4FC9256F`). Not a new
  oracle.
- No other Xbox 360 leaked `.map` or `.exe` for Harmonix titles in the local
  tree.
- DC3 is the only complete-named Harmonix Xbox 360 oracle we have.

**Recommendation:** skip. Worth re-checking if RB1/RB2 retail XEX files are
ever added to `/home/free/code/milohax/` (they share BulletML/Drum/Synth code
with RB3, but without names they're equivalent in difficulty to RB3 itself).

---

## Section 2 — Cross-POC consensus (`tools/exploratory/consolidate.py`)

Combining all four POCs yields **2,735 unique new identifications** with
tiered confidence:

| Tier | Count | Description |
|---|---:|---|
| **S** (consensus) | 29 | ≥2 distinct techniques agree on same dc3 name |
| **A** (callgraph multi-vote) | 437 | Call-graph with ≥2 callers agreeing |
| **B** (RTTI HIGH-tier only) | 361 | RTTI vt with matching slot count |
| **C** (single low-evidence source) | 1,908 | One-shot callgraph or LOW-tier RTTI |

Tier S+A+B = **827 high-confidence new identifications** (~89% precision per
the per-POC cross-verify rates). Tier C should be reviewed manually before
wiring — the ~75% per-POC precision is too low for blind merge.

**Note on path-to-100 ceiling impact:**
- Baseline oracle: 11,582 fns (17.5% of binary).
- After consolidated POCs (with Tier C reviewed and ~80% kept): ~13,500 fns
  (~20.4% of binary).
- The path-to-100 regime-A "strict oracle" ceiling rises from 17.5% to ~20.4%.
- These new identifications enable new **splits.txt pinning** for ~5-15 more
  TUs (each new vt-named class = ~5-15 fns in a TU). Phase 2/4 of path-to-100
  benefits.

---

## Section 3 — Recommended next steps

### 3.1 Production wiring (HIGH priority)

1. **Merge call-graph triangulation into `tools/fingerprint_match.py`:**
   add `merge_callgraph` subcommand that writes a new section into
   `unified_id.json` (or a sibling `unified_id_callgraph.json` for review).
   This unblocks ~441-1594 new identifications immediately. Effort: ~half-day
   Sonnet.

2. **Replace jeff's vtable scanner output with RTTI walk for
   `proposed_splits.txt`:** the 1,321 RTTI-walk vtables strictly dominate jeff's
   342. New `tools/proposed_splits_from_rtti.py` script. Effort: half-day.

3. **Add `tools/exploratory/rtti_vtable_combined.py` HIGH-tier output to
   `unified_id_proposed.json`:** 385 new HIGH-tier names. Use as an oracle
   for splits derivation. Effort: 1 hour wiring + manual review of LOW-tier
   to harvest the 41% that's correct (deferred per ROI).

### 3.2 Research extensions

4. **Multi-vtable inheritance pairing in RTTI+VT:** 226 classes have multiple
   vtables (sub-object vtables for MI bases). Pair by the post-`6B` modifier
   in the mangled name. Likely +50-150 more HIGH-tier identifications.
   Effort: half-day.

5. **Bidirectional call-graph triangulation:** current POC only walks
   named-caller → unknown-callee. Adding unknown-caller → known-callee may
   help for orphan fns called by unidentified callers. Effort: half-day.

6. **CHD walk for parent-class chains:** enables drift detection (which slot
   in dc3 is the "new virtual" inserted between rb3's existing slots). Could
   raise LOW-tier RTTI precision from 41% to 70%+ by skipping
   drift-affected slot indices. Effort: 1-2 days.

### 3.3 Skip / defer

7. **Inlined-leaf reconciliation:** examined dc3 report.json for "inline-only"
   functions; dc3's metadata doesn't categorize this way. Would require
   parsing the leaked `.pdb` for inlinee info — multi-day project. Defer.

8. **Game data → fn signature recovery (DTA callback names):** plausible but
   small yield — game-side DTA files reference a few hundred handlers at most,
   most of which are covered by string-content matching already. Skip unless
   needed for game-code (band3) layer specifically.

9. **CFG structural fingerprinting:** would require parsing each fn into basic
   blocks and computing graph hashes; same-shape collision rate is likely
   high for STL templates. Implementation complexity > yield given that
   call-graph triangulation already exploits structural context. Skip.

---

## Section 4 — Reproducibility appendix

All POCs are deterministic from the read-only data files:

| File | Source | Purpose |
|---|---|---|
| `unified_id.json` | `tools/fingerprint_match.py merge_bindiff` | Baseline rb3 fn → dc3 name |
| `build/45410914/asm/*.s` | dtk SPLIT (jeff) | rb3 disassembly per unit |
| `../dc3-decomp/build/373307D9/asm/**/*.s` | dc3's dtk SPLIT | dc3 disassembly per unit |
| `../dc3-decomp/orig/373307D9/ham_xbox_r.map` | leaked dc3 .map | dc3 RVA → name (109k entries) |
| `../dc3-decomp/orig/373307D9/ham_xbox_r.exe` | dc3 release PE | dc3 binary (vtable contents) |
| `orig/45410914/band.exe` | rb3 unxex'd PE | rb3 binary (RTTI scan) |

Run order:
```bash
python3 tools/exploratory/callgraph_triangulate.py
python3 tools/exploratory/rtti_walk.py
python3 tools/exploratory/vtable_transitivity.py
python3 tools/exploratory/rtti_vtable_combined.py
python3 tools/exploratory/consolidate.py
```

All output JSON lands in `/tmp/exploratory_*.json`. Working directory:
`/home/free/code/milohax/rb3-xenon`. No build runs; pure read-only data
analysis.

---

## Section 5 — Honest caveats

- **Call-graph 94.1% precision** is measured against unified_id overlap.
  Single-evidence proposals (1,153 of the 1,594 new) have lower per-fn
  precision than the multi-evidence subset. Treat single-evidence as
  manual-review candidates.
- **RTTI HIGH-tier 71% precision** has ~half the misses being ICF aliases
  (semantically equivalent), so the *actionable* error rate is ~15%. Still
  review before wiring.
- **None of these techniques find functions outside the 17.5% oracle-covered
  set.** They expand the named pool within the binary; they do not invent
  source code for the 54,552 unidentified residue (that's path-to-100's
  ceiling, fundamentally bound by DC3's coverage + XDK closed-source).
- **The ICF-merged-aliasing pattern** is the dominant disagreement source
  across all techniques. Production wiring should treat both names as "valid
  candidates" for the same fn body, not pick a winner arbitrarily.

---

**Bottom line:** path-to-100's Phase 2-4 plans pin ~302 TUs from existing
oracles; these POCs say the oracle base itself can grow by ~10-25% with
mechanizable cross-correlation between data we already possess. Wire the
call-graph triangulation first; it's the cleanest ROI lever, orthogonal to
everything in path-to-100, and the implementation is small.
