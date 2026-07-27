# Three-address adjudication — CharEyes / CharSignalApplier / HamMove contest (2026-07-27)

Adjudication of the laneH vs laneDUPNAME conflict on three virtual addresses, plus
the independently-refused `??_GCharSignalApplier` @ 0x827b7e88. **Report only — no
map/splits changes landed on main.** All measurements in worktree
`~/tmp/wt-adjud-1` (branch `adjud-1` off main 560dffb3), full `./tools/ninja-locked`
baseline, cold `report.cache` on every read, `git checkout -- symbols.txt` +
`rm target_symbol_renames.stamp` + `touch config.yml` per hygiene rules.

Method: string operand read from the extracted PE (`orig/45410914/band.exe`) via
`scripts/harvest/localstatic_symbol_audit.py --json` (453-member family census);
definer sets by direct COFF symbol-table parse of our compiled objs (DEFINED
section > 0, not string-table presence); mangled names taken from COFF, never
authored; measured A/B of the full correction set.

## Verdict table

| VA | current map binding | reads | string operand (PE) | true identity | recommendation | confidence |
|---|---|---|---|---|---|---|
| 0x8236a128 | `?StaticClassName@CharSignalApplier@@` | strict-100 (fake) | **`CharEyes`** (unique in image) | `?StaticClassName@CharEyes@@SA?AVSymbol@@XZ` | repoint → CharEyes (laneH was right) | **HIGH** |
| 0x8229ceb8 | `?StaticClassName@CharEyes@@` | strict-100 (fake) | **`Cam`** (2 loaders in image) | `?StaticClassName@RndCam@@SA?AVSymbol@@XZ` | repoint → RndCam (laneDUPNAME's keep was wrong) | HIGH that CharEyes is false; MEDIUM-HIGH that RndCam is the true name |
| 0x82271a90 | `?StaticClassName@HamMove@@` | strict-100 (fake) | **`Object`** (unique in image) | `?StaticClassName@Object@Hmx@@SA?AVSymbol@@XZ` | repoint → Hmx::Object (laneZ was right; laneDUPNAME's revert wrong) | **HIGH** |
| 0x827b7e88 | unmapped (refused) | n/a | n/a (no string; discriminator = dtor callee) | **NOT** `??_GCharSignalApplier` — a UIPanel-derived class's ??_G | **refusal HOLDS**, upgraded from spatial-far to content-refuted | **HIGH** |

**Measured cost of the full three-repair set: 0 strict** (38,947 → 38,947 in the
worktree A/B; all three repaired sites still read normalized-100, now truthfully).
laneDUPNAME went 0-for-3 on these addresses.

## Per-address evidence

### 1. 0x8236a128 (owner unit `Char.cpp`, span 0x82369da8–0x8236a6a0)

- PE string operand: **`CharEyes`** — and it is the *only* function among the 453
  family members that loads `CharEyes`. The string discriminator is unambiguous.
- **No function in the retail image loads `CharSignalApplier`** — that COMDAT was
  /OPT:REF-discarded at retail link. The current binding names a function that
  does not exist in the binary.
- Definers: our `Char.obj` **defines** `?StaticClassName@CharEyes@@SA?AVSymbol@@XZ`
  (COFF sec 454) as well as the CharSignalApplier one (sec 678) — so the repoint
  re-pairs at 100 with zero cost, confirmed by the A/B.
- History: bindiff-r1 said CharIKFoot → a380ed69 string-verified it to CharEyes
  (laneH lineage) → laneDUPNAME e7b8ba85 moved it to CharSignalApplier purely to
  break the duplicate-name tie with 0x8229ceb8. It broke the tie in the wrong
  direction: the *other* dup member was the false one.

### 2. 0x8229ceb8 (owner unit `OutfitConfig.cpp`, span 0x8229cd68–0x8229d350)

- PE string operand: **`Cam`** — definitively **not** `CharEyes`. The laneDUPNAME
  binding kept here is false; the retired plan's instinct to remove this exact VA
  was directionally correct (the binding is a fake-100), but eviction is the wrong
  remedy (−1 strict, `pair_funclets_by_bytes` does not recover evicted family
  members). Repoint is free.
- Identity: exactly **two** functions in the image load `Cam`
  (0x8229ceb8 in OutfitConfig.cpp, 0x82738f08 in Rnd_Xbox.cpp) and exactly two
  classes declare the token `Cam` (RndCam, DxCam). COFF: `OutfitConfig.obj`
  defines **RndCam**'s StaticClassName but **not** DxCam's; `Rnd_Xbox.obj` defines
  both. The only consistent one-copy-per-name assignment is
  **RndCam @ 0x8229ceb8, DxCam @ 0x82738f08**. (Rests on our COMDAT emission
  mirroring retail's — same source, same flags; hence MEDIUM-HIGH, not HIGH.)
- Note the collateral falsehoods this exposes (out of scope, not adjudicated):
  0x8240e940 (`Rnd.cpp`) currently mapped RndCam but loads `DOFProc`;
  0x827382f8 mapped DxCam but loads `Mesh`; 0x82738f08 mapped DxEnviron but loads
  `Cam`. The Rnd/Dx family block is a rotation of fake-100s awaiting the same
  treatment.

### 3. 0x82271a90 (owner unit `HamMove.cpp`, span 0x822717c0–0x82271b08)

- PE string operand: **`Object`** — unique in the image. **No function anywhere in
  the retail image loads `HamMove`** (retail discarded HamMove's StaticClassName
  COMDAT). The HamMove binding is definitively false, DC-lineage name or not; the
  suspicious literal was confirmed.
- The unique `Object` loader means this VA **is** `Hmx::Object::StaticClassName`
  (retail linker kept the copy emitted by the HamMove TU — unremarkable COMDAT
  placement; the class name on the function does not have to match the owning TU).
- **The audit tool's `harmful=true` flag on this row is an artifact, not a fact.**
  The tool synthesizes its repair name from prose and produced the bogus
  *unqualified* `?StaticClassName@Object@@SA?AVSymbol@@XZ` (its own docstring
  warns about exactly this on exactly this VA). The COFF-sourced name is
  `?StaticClassName@Object@Hmx@@SA?AVSymbol@@XZ`, and `HamMove.obj` **defines it**
  (COFF sec 1735) — so the correct repair re-pairs at 100. Measured: no loss.
- laneZ (ca7b9803) had this right; laneDUPNAME reverted it to resolve the name
  collision with 0x8240dc38 (MeshDeform), again evicting the true member and
  keeping the false one — 0x8240dc38's Object@Hmx binding loads `CubeTex`
  (another fake-100, out of scope here).

### 4. 0x827b7e88 — `??_GCharSignalApplier@@UAAPAXI@Z`, refused `refuse-spatial-far`

**The refusal holds, and on stronger grounds than the refuser knew.**

- Spatial (the original ground): the VA is **unpinned**, sitting in the gap
  between ProfileMgr.cpp (ends 0x827b7dbc) and a HamMove.cpp scatter sliver
  (0x827b7ed0). Both definers of the symbol in our build (`CharSignalApplier.obj`,
  `OutfitConfig.obj` — n_definers=2; laneAR's 3 is stale) have their nearest spans
  ~583 KB and ~4.7 MB away. Nothing local claims it.
- Content (new, decisive): the ??_G family is itself a fake-100 generator (bodies
  identical mod relocs for every class with the vbase-at-+0x4C layout — our
  compiled `??_GCharSignalApplier` is word-for-word identical to the retail body
  except branch relocs). The only sound discriminator is the first `bl` target =
  the class destructor, and retail's is `fn_827B7CA0` (0x88 bytes), which
  **calls `??1UIPanel@@UAA@XZ` (0x82814358)** and destroys a 12-byte-stride
  vector via a `??1vector` body. CharSignalApplier derives from
  CharPollable/CharWeightable — **not UIPanel** — and our compiled
  `??1CharSignalApplier` is 0x80 bytes with different member destruction. The
  retail fn at 0x827b7e88 is the deleting destructor of some UIPanel-derived
  class, not CharSignalApplier's.
- Since the VA is unpinned, the refusal also costs nothing today; accepting the
  proposal would have required pinning the gap and would have manufactured a
  relocation-blind fake-100 of exactly the kind this contest is about.
- Collateral finding: `0x827b7ca0` is currently mapped
  `?FakeProfileFill@ProfileMgr@@QAAXXZ` — provably wrong (it is a destructor that
  stores vtable pointers and tail-calls `??1UIPanel`). Flagged for a future pass;
  not adjudicated here.

## Measured A/B (worktree `~/tmp/wt-adjud-1`)

- Leg A (baseline, branch base 560dffb3, cold cache): **38,947** matched_functions.
- Leg B (three repairs applied, COFF-sourced names, dup counterparts left in
  place): **38,947**. All three repaired sites read normalized-100; the dup-name
  counterparts (`RndCam` also at 0x8240e940, `Object@Hmx` also at 0x8240dc38)
  **coexist at 100 across units with no loss** — the global-injectivity premise
  that drove laneDUPNAME's tie-breaks is not mechanically required, at least for
  cross-unit duplicates in this family.
- (Baseline note: the worktree's cold count is 38,947 vs the 38,819 quoted for
  main — the delta is upstream of this contest; both legs here are self-consistent
  and differ by exactly 0.)

## What a reader should conclude

1. **None of the three addresses is undecidable.** The string operand + COFF
   definer evidence resolves all three cleanly, and the correction set is
   measured **0-cost and truth-restoring**: repoint 0x8236a128 → CharEyes,
   0x8229ceb8 → RndCam, 0x82271a90 → Hmx::Object (exact leg-B edit sits in
   `~/tmp/wt-adjud-1/scripts/target_symbol_map.json`, ready to land).
2. **laneDUPNAME's re-bijection tie-breaks were made blind to the string
   discriminator** and on these rows kept the false member and evicted the true
   one, twice, and invented a third falsehood once. Any future dup-name
   enforcement should consult `localstatic_symbol_audit.py` strings *before*
   choosing which duplicate to keep — and should first re-examine whether the
   injectivity constraint is needed at all, since both induced cross-unit dups
   measured free here.
3. **The `??_GCharSignalApplier` refusal stands** — now content-refuted, not just
   spatial. The ??_G family needs the same treatment as StaticClassName: the
   discriminator is the dtor callee chain, not body bytes.
4. The audit tool has one repair-name synthesis defect (namespaced classes yield
   bogus unqualified mangles, e.g. Hmx::Object) and its `harmful` flag inherits
   it. Verdicts/strings are sound; repair names must be COFF-sourced.
5. Follow-up vein (not landed): the Rnd/Dx StaticClassName rotation
   (0x8240e940, 0x8240dc38, 0x827382f8, 0x82738f08 and the ~24 remaining
   MISMATCH/ambiguous rows in the audit) — several look repairable by the same
   definer-set + spatial argument used for 0x8229ceb8.
