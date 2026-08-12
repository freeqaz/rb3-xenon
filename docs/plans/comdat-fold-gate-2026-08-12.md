# The homonym oracle does not generalise; the comparator was the bottleneck

2026-08-12, lane R. Companions: `tools/homonym_index.py`,
`tools/comdat_fold_gate.py`, `docs/plans/dc3-homonym-index-2026-08-12.json`,
`docs/plans/comdat-fold-gate-2026-08-12.json`.

This lane was dispatched to sweep dc3's leaked `ham_xbox_r.map` for the class
FT3 fired on — one mangled name, two modules — and apply it across rb3-xenon's
whole `name_check` charge set. **The sweep refutes the premise.** The gain came
from somewhere else in the same lane: the gate's *comparator*, which was
refusing hundreds of pairs on an artifact of its own construction.

Measured at the lane's HEAD on pinned `objdiff-cli-B` (objdiff main `745b7e3`),
rendered alias map regenerated before every read, one `-o` path per arm:

| ruler | before | after | complete fns |
|---|---|---|---|
| `none` (control) | 42.220000% (4,357,396 B) | **42.220000%** (4,357,396 B) | **+0 / −0** |
| `name_check` | 32.462280% (3,350,332 B) | **32.544292%** (3,358,796 B) | **+78 / −0** |

Nothing lost on either ruler. The control does not move by a byte, which is what
a name-only change owes you. The `none`↔`name_check` gap narrows 9.757720 pp →
9.675708 pp.

## 1. The homonym class is 25 names, and the bound is structural

`tools/homonym_index.py` sweeps all 117,960 symbols dc3's leaked map names with
their owning `.obj`:

    rows parsed                       119,504
    code-section names                 77,927   (public 53,906, static 24,023)
    names at >1 code address            1,393
      from the Publics table                0
      from the Static symbols table     1,392
    after dropping __unwind$/__catch$/__ehhandler$/__tryblocktable$ labels
      HOMONYMS (real functions)            25

**Zero multi-address publics is the finding, and it is not a sample size.** A
duplicate public is a link error and COMDAT selection collapses the rest onto one
address, so a homonym can only ever arise between definitions with INTERNAL
linkage. The class is bounded by how often two TUs give a file-scope function the
same name — in a 77,927-function game, 25 times, and 11 of those are `??__E` /
`??__F` dynamic initialisers for file-scope statics two TUs both called
`sLicense`, `gFile`, `sRand`, `vecNegate0`.

Cross-checking the 25 against rb3:

| | count |
|---|--:|
| present in `scripts/target_symbol_map.json` at all | 5 |
| present in the `name_check` wrong-callee charge set | **1** |

The one is `??3@YAXPAX@Z` — the pair FT3 already fired on. **There is no second
one and there cannot be a meaningfully larger set.** Do not budget another sweep
against this oracle.

### The cross-game assumption, stated and tested

Using dc3's map as evidence about rb3's link assumes the two Xbox 360 MSVC-PPC
builds of the same Milo lineage share the property. That assumption is *not*
needed for the negative result above: the linkage rule that produces the zero is
a property of the MSVC linker, not of either game. It would have been needed only
to transfer a positive, and there is one positive to transfer — corroborated
directly against `orig/45410914/band.exe` by lane H (RB3's `0x82BC6B70` is
byte-identical, masked, over all 148 bytes to dc3's `xaudio2:baseswfilter.obj`
copy).

## 2. The gate was refusing on its own instrument

`tools/fold_thunk_gate.py` masks every relocation-CAPABLE field on both sides,
then — because the linked image cannot say which fields the linker actually
patched — requires the two sides to agree on the SET of masked offsets. Retail's
side marks every `li` / `addi` / `lwz` / `stw` as relocation-capable; our COFF
side lists only offsets carrying a real relocation record. The sets differ on any
body larger than a tail branch.

Pointed at the three untouched sub-classes as-is, that fired on **196 pairs / 436
sites** — refusals produced by the instrument, not the evidence — and the whole
sweep admitted 1 pair.

`tools/comdat_fold_gate.py` uses our COFF relocation table as the oracle for
which fields are relocated, because that is exactly what it is:

| our side at that offset | how it is compared |
|---|---|
| no relocation | the **full 32-bit word**. Strictly stronger than masking, and it is where intra-body loop branches and every literal constant live |
| branch relocation | opcode/AA/LK, then retail's destination **resolved through the map and name-compared** — not masked away |
| 16-bit-immediate relocation | **REFUSE**. A `lis`/`addi` in a linked image reconstructs a data VA we cannot name, so "equal after masking" is vacuous — it is how `??__EgNotifies` and `SystemConfig` pass as 12 identical bytes (11 pairs / 26 sites refused here) |

Fail-closed additions: size mismatch refuses (no padding trimming — a trim is a
guess about which end is padding); a spelling admitted against two survivor
addresses is refused at both; a spelling the alias file already places elsewhere
is refused. **The count of names sitting at more than one address in
`scripts/symbol_aliases.json` is 842 before this lane and 842 after.**

### Result

    ADMIT   33 pairs / 1,693 sites in 23 groups     REFUSE 1,015 pairs / 2,495 sites

| sub-class | ADMIT pairs/sites | REFUSE pairs/sites |
|---|--:|--:|
| `bijection_class` | 8 / 40 | 231 / 463 |
| `map_name_unresolved` | 6 / 11 | 131 / 438 |
| `residual` | 6 / 7 | 529 / 1,101 |
| `fold_thunk_naming` | 13 / 1,635 | 23 / 162 |
| `transposition` | 0 | 70 / 203 |
| `map_misassignment` | 0 | 27 / 84 |
| `wrapper_not_inlined` / `wrapper_inlined_by_us` | 0 | 4 / 44 |

Tiers: CF1 (retail's map places the folded spelling on the same body or nowhere)
20, CF2 (on a different body the image discredits — zero `.text` fan-in or no
`symbols.txt` extent) 12, CF3 (homonym) 1.

**All nine of `fold_thunk_gate.py`'s admitted pairs re-derive as ADMIT under the
stronger comparator — not one is refused.** The +1,358 that landed earlier today
stands on independent evidence. Four more pairs in that sub-class that FT refused
on the offset-set artifact are admitted here (`??3Loader` 22 sites and `??3Task`
10 onto `??3BinStream`, two `list<T>` destructors onto
`__destroy_aux<Entry@LocalePanel>`), and they carry far more weight per site
because each is the whole charge on a 40-byte scalar-deleting body: 36 sites
bought 36 complete functions, where the earlier 58 sites bought 42.

### What did not work

Making the branch-destination compare **alias-aware** — treating two callee names
as equal when an already-installed group places them at one address, which is
exactly what objdiff's `reloc_eq` does at report time — gained **zero** new
admits and cost one (a pair that then passed at a second survivor address and was
refused at both by the multi-address rule). The 87 branch-name refusals are
genuinely different callees, not aliasable ones. The semantics are correct and
were kept; the yield was nil. A second fixpoint round changed nothing.

## 3. The refusals are not "the callee has not matched yet"

The `different_function` census attributed 1,539 of its `cannot_adjudicate` sites
to "our callee has not matched yet — settled by progress on the callee, not a
better comparator". The brief flagged that as a claim to check. **It is not what
the refusal population is.** Cross-tabbing every refusal against our callee's own
`none` score at the VA the map gives it:

| refusal | pairs | sites | our F already 100% at `none` |
|---|--:|--:|--:|
| body size differs | 776 | 1,874 | **705** |
| branch destination named differently | 87 | 264 | 56 |
| branch destination unnamed in the map | 70 | 164 | 59 |
| unrelocated word differs | 31 | 60 | 27 |
| our objs disagree on the spelling | 17 | 53 | 13 |

788 refused pairs / 1,995 sites have a callee that is **already byte-exact
against its own map address** and is simply not the body at the survivor address.
Only ~64 pairs are waiting on an unmatched callee. A better comparator does not
reach these, and neither does progress on the callee.

Two shapes account for nearly all of them, and neither is an alias:

* **197 pairs where retail's two bodies are reloc-masked identical but their
  relocations differ** — `list<CharClip*>::insert` vs `list<Hmx::Object*>::insert`
  (both 100 B, 88 sites), `ObjPtrList<CharInterest>` vs `ObjPtrList<Hmx::Object>`
  (both 84 B). Identical shape, different type-specific callee, so `/OPT:ICF`
  correctly did *not* fold them and the map hung one name on each arbitrarily.
  This is the shape the wrong-callee triage already read correctly. **The repair
  is an edit to `target_symbol_map.json`, not an alias** — and that is a
  measurement-surface change on shared build state with 34 sha256 pins riding on
  the eval roster, so it stays unlanded here as it did there.
* **17 pairs / 246 sites where OUR compiled body for the charged callee is a
  4-byte `b <survivor>`** — a wrapper retail inlined and we did not. The triage
  binned exactly ONE of them (37 sites) as `wrapper_not_inlined`; the other 16
  pairs / 209 sites sit in `residual`, `map_name_unresolved` and
  `map_misassignment`, led by `~ObjPtr<RndTransformable>` → 141 sites.

  The mis-binning has a single cause worth fixing at the source.
  `scripts/wrong_callee_triage.py` decides "is our callee a thunk to retail's?"
  by reading the thunk **out of the retail image at the map's address for our
  spelling** — and that address is precisely the arbitrary fold-survivor parking
  the whole triage exists to distrust. For the 141-site pair the map parks
  `~ObjPtr<RndTransformable>` at `0x8271EEF0`, whose 4 bytes are
  `b SynthEmitter::Handle`; our own COMDAT is 4 bytes carrying one relocation
  that names `~ObjRefConcrete<RndTransformable,ObjectDir>` — the survivor —
  outright. Read the thunk from our object's relocation record and the class is
  17× larger. An inlining difference either way: not a wrong callee, and not
  reachable by aliasing.

## 4. Reproduce

```sh
python3 tools/homonym_index.py --json docs/plans/dc3-homonym-index-2026-08-12.json
python3 tools/comdat_fold_gate.py --subclass all \
    -o docs/plans/comdat-fold-gate-2026-08-12.json --install
python3 tools/gen_symbol_alias_map.py            # NOT a ninja edge; regenerate or you measure the old map
<pinned-objdiff> report generate -p . -c functionRelocDiffs=none      -o A.json && rm -f A.json.cache
<pinned-objdiff> report generate -p . -c functionRelocDiffs=name_check -o B.json && rm -f B.json.cache
```
