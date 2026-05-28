# RTTI + vtable transitivity (production)

**Tools:** `tools/fingerprint_match.py rtti` and `tools/fingerprint_match.py vtable`
**Status:** production (2026-05-28). Productionizes the POCs at
`tools/exploratory/rtti_vtable_combined.py` (→ `rtti`),
`tools/exploratory/rtti_walk.py` (the RTTI scanner inside `rtti`), and
`tools/exploratory/vtable_transitivity.py` (→ `vtable`).
**Companion:** `docs/plans/exploratory-techniques.md` §1.2-1.4 (the original
POCs + precision methodology), `docs/decomp/callgraph-triangulation.md` (the
sibling productionized technique whose pattern these mirror).

## What it does

Both subcommands transfer dc3's named vtable slots onto rb3's anonymous
functions, by class-name identity:

- rb3 class `Foo`'s vtable slot *N* is some anonymous `fn_<addr>`.
- dc3's `??_7Foo@@6B@` slot *N* is a **named** function (from the leaked `.map`).
- Therefore rb3 `fn_<addr>` IS dc3's slot-*N* function.

The only valid-when condition is that the slot positions line up — i.e. no
engine drift inserted/removed a virtual between dc3's and rb3's versions of the
class. The two subcommands differ in how they discover rb3's vtables:

### `rtti` — walks rb3's non-standard X360 RTTI (the strong technique)

jeff's heuristic `.rdata` scanner finds only 342 rb3 vtables. RB3 actually has
1,396 RTTI TypeDescriptors. Walking RTTI recovers **1,321 vtables (3.8×)**.

**X360 RTTI layout (empirically derived; band.exe does NOT use the classic
`0x19930522` Complete-Object-Locator signature):**

- **TypeDescriptor:** `vptr(4) + spare(4) + ".?AV<CLASS>@@\0"` (or `.?AU` for structs).
  We find TDs by scanning for that string whose preceding `-8` dword is a
  `.text`-range vptr.
- **COL (Complete Object Locator), precedes the vtable at `vt_va − 4`:**
  - `+0x00 .. +0x08` = **three zero dwords** (this is where the `0x19930522`
    signature lives on desktop x86 MSVC — it is *absent* here).
  - `+0x0C` = `pTypeDescriptor`.
  - `+0x10` = pSelf / pClassHierarchyDescriptor.
  We find COLs by locating a big-endian pointer to a TD that is preceded by
  exactly 12 zero bytes.
- **Vtable:** `vt_va − 4` stores the COL pointer; `vt_va + 0` is slot 0. We find
  vtables by locating big-endian pointers to a COL; the vtable begins at `ref+4`
  and runs until the first non-`.text` dword.

This layout logic is preserved bit-for-bit from `tools/exploratory/rtti_walk.py`
(now `_rtti_walk_rb3` in `fingerprint_match.py`). It recovers 1,321 vtables /
1,317 named classes, exactly matching the POC.

### `vtable` — pairs jeff's heuristic vtables via known slots (the fallback)

The pre-RTTI technique. Reads jeff's 342 `vftable_<addr>` records from rb3's
`*_rdata.s`, resolves the slots we already know (via `unified_id`), finds the
dc3 vtable whose named slots align at the same indices, and transfers the
remaining slots. **Subsumed by `rtti`** (which finds 3.8× more vtables) but kept
as an orthogonal fallback — 22 of its 37 hits are also found by `rtti`, but 15
are unique (jeff's scanner caught vtables whose RTTI did not survive, e.g. some
templated/forward-ref classes).

## How to run

```bash
venv/bin/python3 tools/fingerprint_match.py rtti
#   -> writes unified_id_rtti.json (HIGH-tier, NEW-only)
venv/bin/python3 tools/fingerprint_match.py rtti --include-low
#   -> also writes unified_id_rtti_low.json (LOW-tier, ~41% precision)
venv/bin/python3 tools/fingerprint_match.py vtable
#   -> writes unified_id_vtable.json (NEW-only)
```

Defaults (all overridable; same artifacts the POCs used):
- `--unified unified_id.json` — anchor oracle *and* the NEW-only filter set.
- `--rb3-exe orig/45410914/band.exe` — rb3 unxex'd PE (RTTI scan; `rtti` only).
- `--rb3-asm build/45410914/asm` — rb3 disassembly (`vftable_` in `*_rdata.s`; `vtable` only).
- `--dc3-map ../dc3-decomp/orig/373307D9/ham_xbox_r.map` — dc3 vtable RVAs.
- `--dc3-exe ../dc3-decomp/orig/373307D9/ham_xbox_r.exe` — dc3 vtable contents.
- `--dc3-objects ../dc3-decomp/config/373307D9/objects.json` — obj→cpp enrichment.
- `--rb3-symbols config/45410914/symbols.txt` — function sizes.
- `rtti` only: `--out unified_id_rtti.json`, `--out-low unified_id_rtti_low.json`,
  `--include-low` (default off).

No build is run; pure read-only data analysis (~10-20 s, dominated by the
RTTI scan over band.exe).

## Output schema + confidence

Records are **schema-compatible with `unified_id.json`** (`rb3_fn`, `rb3_addr`,
`size`, `source`, `dc3_name`, `dc3_name_demangled`, `dc3_obj`,
`dc3_inline_only`, `bindiff_src`, `similarity`, `confidence`, `algorithm`) plus
technique-specific provenance:

- `rtti`: `source="rtti"`, `rtti_tier` (HIGH/LOW), `rtti_class`, `rtti_slot`.
- `vtable`: `source="vtable"`, `vt_class`, `vt_dc3_vtable`, `vt_slot`,
  `vt_pairing_evidence`.

`bindiff_src` is populated (dc3 `.cpp` resolved from the map obj) so the
existing wave tooling consumes these records unchanged. dc3 `HamX@`→`BandX@`
class substitution (same `substitute_dc3_class_names` helper as
`merge_bindiff`/`triangulate`/`gen_target_map`) is applied to `dc3_name`.

| Technique | confidence / similarity | Intent |
|---|---|---|
| `rtti` HIGH | 0.80 / 0.80 | manual-review oracle, **below** gen_target_map's 0.95 default |
| `rtti` LOW | 0.40 / 0.40 | reject; opt-in only |
| `vtable` | 0.80 / 0.80 | manual-review oracle, below the auto-merge default |

## Precision (re-measured 2026-05-28 — NOT parroted from the POC)

Cross-verified against the `unified_id.json` overlap (proposals where the
address is *also* already mapped, so the existing dc3_name is the ground truth):

- **`rtti` HIGH: 59/83 = 71.1%** (POC: 59/83 = 71.08% — faithful).
- **`rtti` LOW: 44/107 = 41.1%** (POC: 41.12% — faithful).
- **`vtable`: 0/0** — no overlap to verify against, by construction (the
  technique only proposes for fns *not* already in unified_id, so its overlap
  set is empty). The POC reported the same; treat as unverified-but-structural.

### HIGH does NOT clear the ≥90% default-on bar

This is the load-bearing honest finding. The bar for an auto-merged oracle is
≥90% (callgraph's `multi` tier is 94%). **`rtti` HIGH's raw 71.1% is well
below it.** Per the productionization brief, the technique is therefore emitted
to its **own file** (`unified_id_rtti.json`, never auto-unioned into the symbol
map) at confidence **0.80**, which is sub-threshold for `gen_target_map`'s 0.95
default. It is a **manual-review / splits-derivation oracle**, not auto-merge
fodder. LOW (41%) is suppressed entirely unless `--include-low`.

**ICF-alias caveat.** Of the 24 HIGH disagreements, a substantial fraction are
not errors but **ICF aliases** — the same folded code body reached by multiple
mangled names:
- `??_E<Class>` (scalar deleting dtor) vs `??_G<OtherClass>` (vector deleting
  dtor): trivial dtors fold to one body.
- Folded template-container methods, e.g. `?RemoveKey@BoolKeys@@` vs
  `?RemoveRange@SymbolKeys@@`, `?SetKey@ColorKeys@@` vs `?RemoveKey@QuatKeys@@`
  — the `*Keys`/`*Vec` instantiations share a code body.

So the *semantic* (actionable) precision is higher than 71%, but it still does
not reach the 90% auto-merge bar. For these aliases, **both** mangled names are
valid candidates for the same body; don't treat the single winner as canonical.

## Orthogonality (re-measured 2026-05-28)

Against the existing callgraph oracle (`unified_id_callgraph.json`):

- `rtti` HIGH (384 recs): overlaps callgraph by **2** addresses.
- `vtable` (37 recs): overlaps callgraph by **0** addresses.
- Combined NEW set (`rtti` HIGH ∪ `vtable`) = **399 addresses beyond
  unified_id**, of which **397 (99.5%) are orthogonal** to the callgraph oracle.

This confirms the report's claim: vtable-transitivity identifies fns *via their
class's vtable slot*, which is a completely different signal from call-graph
position. The two oracles are nearly disjoint and additive.

## Hand spot-checks (reproduced from the report's sample wins)

All five of `exploratory-techniques.md` §1.3's sample HIGH wins reproduce
exactly in `unified_id_rtti.json`:

- `fn_8251AF50 → ??_GArkFile@@UAAPAXI@Z`
- `fn_8251AC98 → ?Filename@ArkFile@@UBA?AVString@@XZ`
- `fn_8251AA10 → ?Read@ArkFile@@UAAHPAXH@Z`
- `fn_825D0D00 → ??_EAccomplishmentCategory@@UAAPAXI@Z`
- `fn_82B5A630 → ??_E?$BloomTextures@$02@NgPostProc@@UAAPAXI@Z`

## Merge semantics (load-bearing)

Both subcommands write **NEW addresses only** — any rb3 address already in
`unified_id.json` is skipped, so the union with `unified_id.json` is
**collision-free by construction**.

**Difference from the POC's "new" definition (intentional):** the POC filtered
"new" only against addresses that *already carry a dc3_name*. The production
subcommands filter against **all** `unified_id` addresses, including ~24 entries
that came from `autoid` string-attribution and have `dc3_name=None`. Those
addrs ARE skipped here (matching `triangulate`'s strict address-based NEW-only
contract), even though `rtti` could in principle *enrich* them with a dc3_name.
This is why the production HIGH count is 384 (POC: 385) and LOW is 730 (POC:
753) — the delta is exactly those autoid-no-name overlaps. The subcommand
reports the skipped count (`skipped (addr already in unified_id, incl.
autoid-no-name…)`) for transparency.

Because confidence (0.80) is below `gen_target_map`'s 0.95 default, a naive
union is inert for auto-merge — these stay manual-review until a human promotes
specific entries. To feed the wave generators after review, union the file and
pass it as `--unified` (same recipe as `docs/decomp/callgraph-triangulation.md`).

## Known limitations

- **HIGH is below the auto-merge bar (71%).** Manual-review only; never
  auto-unioned. See the precision section.
- **Multi-inheritance classes deferred (226 classes).** Classes with multiple
  vtables (sub-object vtables per base) are skipped — pairing them needs the
  post-`6B` modifier in the mangled name. POC follow-up (`exploratory-techniques.md`
  §3.2.4) estimates +50-150 more HIGH hits; ~half-day of work.
- **No drift detection.** LOW-tier (mismatched slot counts) is rejected wholesale.
  A CHD (Class Hierarchy Descriptor) walk could detect *which* slot drifted and
  rescue the unaffected slots, raising LOW precision from 41% toward 70%+.
- **`vtable` is subsumed by `rtti`** (15 unique hits remain). Keep as a fallback;
  don't treat it as a primary oracle.
- **No new source code.** Like all the exploratory techniques, these expand the
  *named* pool within the binary; they cannot identify the ~54k functions with
  no oracle hint (the path-to-100 ceiling).
