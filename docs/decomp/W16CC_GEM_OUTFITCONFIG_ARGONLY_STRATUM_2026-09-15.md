# W16-CC — `default/band3/bandtrack/Gem`: the whole near-miss stratum is argument-only, and 16% of the gap is a file we never ported

**Lane:** W16-CC · **Date:** 2026-09-15 · **Branch:** `w16-cc` off `e74c37c3`
**Unit:** `default/band3/bandtrack/Gem` — a three-source merged TU
(`src/band3/bandtrack/Gem.cpp` `#include`s `bandobj/OutfitConfig.cpp` at line 617
and `band3/bandtrack/GemRepTemplate.cpp` at line 643).

**Bottom line: this unit has no source work in it.** Not "hard source work" —
*none*. The entire near-miss stratum is argument-only with zero instruction
differences, the fold gate admits none of the pairs it can adjudicate, and the
zero stratum is unpaired rather than unnamed. The one genuinely actionable
finding is that **16.0% of the unit's gap is `NowBar.cpp`, a file this tree has
never ported**, whose full source exists in the rb3-Wii oracle.

**Bytes landed: 0. Aliases installed: 0.** That is the result, not a shortfall —
see §7.

---

## 1. Baseline and provenance

Read from `build/45410914/report.json`, never inherited:

```
matched_functions 43774   matched_code 4073996   total_code 10247068
total_functions   69240   matched_code_percent 39.757675
fuzzy_match_percent 49.878334   masked_equal_functions 23201
provenance: functionRelocDiffs=name_check, ppc.calculatePoolRelocations=false,
            tool_commit a5f0ea903ec1, binary hash 5a51cd51fe0a353f
```

⚠ `provenance.diff_config` is a **list of `key=value` strings**, not a dict —
`.get()` on it raises `AttributeError`. Cost me one read.

Base-commit ambiguity was resolved before measuring: the dispatch said
`e74c37c34a6f`, the brief said `73139623f8f0`. Exactly one commit separates
them and it is docs-only (1 line in `ROADMAP_GAP_TO_TARGET_2026-09-01.md`), so
there is no metric-bearing delta — and the brief's unit figures then reproduced
to the digit, which is the real confirmation.

## 2. The unit's exact partition

| stratum | rows | bytes |
|---|---:|---:|
| `fuzzy == 100` (matched) | 146 | 13,780 |
| `99.0 <= fuzzy < 100` (near-miss) | 44 | 8,524 |
| `0 < fuzzy < 99` (mid-band) | **1** | **40** |
| `fuzzy == 0` | 31 | 7,920 |
| **total** | **222** | **30,264** |

`matched_code_percent` 45.532646, **gap 16,484 B**. The brief's claim that
"there is no mid-band grind" is literally true: the mid band is one 40-byte row.

Of the 31 zero rows, **28 are anonymous / 7,676 B** (the brief's figure) and 3
are named: two `__destroy_range_aux` instantiations over anonymous-namespace
types (`?A0xf8e4b4b5` = `Unlockable`, `?A0x81ddebd1` = `Label`, 96 B each) and
`?FillHit@NowBar@@QAAXHH@Z` (52 B, newly unpaired by §6).

## 3. Stratum 1 — all 8,524 B is argument-only. Measured, not inferred.

Run `objdiff-cli diff --batch` over all 44 rows and sum the instruction
summaries:

```
rows 44   bytes 8524
total 2131   equal 2074   diff_arg 57
insert 0   delete 0   replace 0   diff_op 0
ARG_ONLY rows: 44 of 44
```

**Zero instruction-level differences exist anywhere in the stratum.** Every one
of the 8,524 bytes is withheld by 57 argument charges across 2,131 otherwise
identical instructions.

This is the trap CLAUDE.md documents from the other side. "N/N instructions
equal" is an *instruction*-level reading and does not include relocation-name
charges, which are *argument*-level and coexist with all-instructions-equal.
Here the two readings are maximally divergent: a mismatch count says the
stratum is finished, and the grader withholds 8,524 B.

Charge composition (57 total):

| class | charges |
|---|---:|
| callee-name (`Symbol`) over 41 distinct pairs / 50 sites | 50 |
| `Register` | 4 |
| `COMMUTATIVE_OP_ORDER` | 2 |
| `Signed` immediate | 1 |

Detector labels over the 44 rows: `TEMPLATE_INSTANTIATION_MISMATCH` 19 rows,
`WRONG_CALLEE` 23, `UNVERIFIABLE_PAIRING` 16, `REGISTER_SWAP` 1, none 1.
⚠ Per CLAUDE.md these labels are **restatements of the detector's input**, not
diagnoses — `TEMPLATE_ARGS_DIFFER` *is* what a fold looks like, and a
`WRONG_CALLEE` verdict on a reloc-name-only row is bit-for-bit the definition
of the thing it claims to have detected. I did not treat any of them as
evidence in either direction.

## 4. Fold adjudication — gate ADMIT 0 / REFUSE 7 / SKIP 34. No aliases installed.

The 41 distinct callee pairs were checked against
`scripts/symbol_aliases.json` first: **none is already forgiven.** 24 have no
group for the retail name at all; 17 have a group that lacks our spelling.

Adjudicating on retail bytes with `tools/comdat_fold_gate.py`:

- **7 pairs** had honest map addresses on both sides and could be adjudicated.
  **0 ADMIT, 7 REFUSE** — 4 size mismatches (different-size COMDATs cannot
  fold), 1 nested-fold contradiction (`??4TransformArea@@` vs
  `??4ObjList<OldMatOption>`, branch destination at offset 0x50 differs),
  1 fail-closed (two distinct COMDATs for `~VertVector`), 1 other.
- **34 pairs** were skipped because *our* spelling has no map address.

⛔ **I refused to infer a fold from that absence, and this is the load-bearing
methodological point of the lane.** "Callee absent from the map ⇒ fold-alias"
is the model CLAUDE.md records as refuted: it never measured folding, it
measured *identification coverage*, which is 41.7%. Treating those 34 as folds
would have installed 34 unproven aliases.

⚠ And a `none`-ruler control **cannot** catch that mistake. `none` ignores
relocation names, so it reads +0 against a fabricated alias **by
construction**. That flatness is the *signature* of the hazard, not a
clearance. The only instrument that discriminates here is retail bytes, and on
retail bytes the answer was 0-for-7.

### 4.1 The MeshAO/Patch fold, refuted twice over

The most attractive-looking candidate was `??4MeshAO@@` ↔ `??4Patch@BandCharDesc@@`.
Refuted on two independent grounds:

- **Bytes.** Our `??4Patch@BandCharDesc@@` is 132 B — exactly the extent at the
  map's `??4Patch` address. Our `??4MeshAO` is 96 B. A 96 B body cannot fold
  with a 132 B one.
- **Structure.** `Patch : public FixedSizeSaveable` has a vtable, `int mTexture`,
  `String mMeshName`, `float mRotation`. `MeshAO` has no base and no vtable:
  two `String`s and two `std::vector`s, 48 B. They are unrelated classes whose
  assignment operators cannot be the same code.

**No aliases were installed by this lane.** A failed fold proof was the
deliverable, and §7 prices it.

## 5. Stratum 1's register class is closed to source work — because *retail* is inconsistent

The 440 B of `Register` charges look like the one source-reachable class in the
stratum. It is not, and the control table is why:

| function | retail emits | ours | row |
|---|---|---|---|
| `Gem::Hit` | `lwzx r3,r29,r11` | same order | **100** |
| `Gem::Release` | `lwzx r3,r29,r11` | same order | **100** |
| `Gem::UpdateTailPositions` | `lwzx r3,r11,r29` | our order | charged |
| `Gem::PartialHit` | `lwzx r3,r11,r28` | our order | charged |

All four use the identical `mTails[i]->…` source construct. `lwzx rD,rA,rB`
computes `rA+rB` and is commutative, so operand order is a scheduling artifact,
not a semantic one — and **retail is the side that is inconsistent between its
own four call sites.** No single source spelling can fix two without breaking
the other two. Permuter is off by standing directive, so this class is closed.

⚠ I had initially hypothesised that `mTails[i]` indexing was a shared source
defect. **Refuted by the same table**: four sibling functions using the
identical construct sit at fuzzy 100.0000.

## 6. Stratum 2 — the zero rows are UNPAIRED, not unnamed

This reframes the brief's "28 anonymous rows awaiting identification".

**30 of the 31 zero rows have `base_size == 0`.** They are 100% `insert`: the
target has a body and *we emit nothing*. They are not bodies awaiting a name;
naming them buys a pairable row at 0% with no content, which is the
`ForceEmit_*` metric-fitting pattern the project forbids.

Evidence that identification does not pay here: the 7 largest unpaired rows
(3,820 B — half the stratum) have **zero same-size COMDATs** among our
`Gem.obj`'s 2,427. We do not emit those bodies at any name. Concretely,
`fn_822AB3E0` is 588 B while our `OutfitConfig` ctor is 1,008 B and its dtor
556 B; `fn_822A5A38` is 1,164 B with no same-size COMDAT at all.

### 6.1 `NowBar.cpp` is entirely unported — 16.0% of the gap

The one paired-and-divergent zero row, `?Register@OutfitConfig@@SAXXZ` (52 B),
turned out to be a **phantom**: the body carrying that name is not
`OutfitConfig::Register`.

Retail body at `0x82BAA660`, 52 B:

```
lwz   r10, 0x0(r3)     ; vector _M_start — member at offset 0x0
mr    r11, r4
lwz   r9,  0x4(r3)     ; _M_finish
subf  r9,  r10, r9
srawi r9,  r9, 2       ; size(), 4-byte elements
cmplw cr6, r4, r9
bge   cr6, -> li r3, 0
slwi  r11, r11, 2
lwzx  r3,  r11, r10    ; mSmashers[index]
mr    r4,  r5
b     0x82BAFF20       ; = ?FillHit@GemSmasher@@QAAXH@Z
```

That is `NowBar::FillHit(int,int)` with `FindSmasher` inlined and the
`MILO_ASSERT` compiled out, against `NowBar.h`'s
`std::vector<GemSmasher*> mSmashers; // 0x0`. The rb3-Wii oracle
(`../rb3/src/band3/bandtrack/NowBar.cpp:174`) is the same three lines.

★ **The anchor is map-independent**: the identification rests on the tail-call
destination, which is image ground truth, not on another map row that could
itself be wrong.

Widening to the band `0x82BAA400`–`0x82BAAF30`: **17 rows / 2,796 B, of which
13 rows / 2,644 B sit at fuzzy 0.** `NowBar.h` declares 13 member functions.
(Not an exact 1:1 — `FindSmasher` is inlined at its call sites — but the
coincidence is strong.) The 4 non-zero rows in the band are 32–40 B generic EH
funclets that pair by byte signature.

**2,644 B = 16.0% of this unit's 16,484 B gap is `NowBar.cpp`.**

Corroboration from three directions:

- `src/band3/bandtrack/NowBar.h` **exists** in our tree; `NowBar.cpp` **does
  not**, and is absent from both `config/45410914/objects.json` and
  `config/45410914/splits.txt`.
- No map row names any `NowBar::` method (the only hit is
  `?UpdateNowBar@GamePanel@@`).
- The map names `0x82baa3b8` (TrackConfig ctor) and `0x82baaf30`
  (`Gem::OnScreen`) and **nothing in between** — the whole band is anonymous.

**This is the hand-off.** The source exists in the oracle and is 243 lines.

### 6.2 The map repair, and a refutation of my own earlier claim

Landed as commit `e4b8181` — `0x82baa660`: `?Register@OutfitConfig@@SAXXZ` →
`?FillHit@NowBar@@QAAXHH@Z`.

⛔ **Retail has no out-of-line `OutfitConfig::Register` at all.** Our header:

```cpp
static void Init();
static void Register() { REGISTER_OBJ_FACTORY(OutfitConfig); }
```

`Register` is an inline header function, so `/Ob2` folds it into its one caller,
`Init`. The name was therefore fabricated *twice over*: a symbol that does not
exist as a body, placed on a body from an unported file.

★ **I had earlier written up `0x822AC058` as "the real `OutfitConfig::Register`",
proven by decoding branch displacements. That was WRONG and is retracted here.**
The body there calls `StaticClassName`, forms a pointer to `NewObject` via
`addi`, calls `Hmx::Object::RegisterFactory` — *and then makes three more calls
storing to the consecutive globals `0x82CBCC28/2C/30`*. Those three are
`sMat`, `sCam`, `sBandCharDesc`, and `OutfitConfig.cpp:460` is:

```cpp
void OutfitConfig::Init() {
    Register();
    sMat = Hmx::Object::New<RndMat>();
    sCam = Hmx::Object::New<RndCam>();
    sBandCharDesc = Hmx::Object::New<BandCharDesc>();
}
```

So `0x822AC058` is `Init` **with `Register` inlined into it**, and the map
already named it `?Init@OutfitConfig@@SAXXZ` — correctly. My error was a
category error: comparing retail's `Init` against our standalone `Register` and
reading the difference as "our `Register` is incomplete". It also generated a
follow-on plan (extend the splits pin over the unpinned gap
`0x822AC048`–`0x822AC110` to capture "the real Register") which is now moot —
there is nothing there to capture.

**A/B, map kind ⇒ forced re-split on both legs, renamer_patched=1830, both legs
at a `symbols.txt` split fixed point, objdiff-cli sha pinned across legs:**

```
leg A  43774 / 23201 masked / 39.757675%   fuzzy 49.878334
leg B  43774 / 23201 masked / 39.757675%   fuzzy 49.878330
Dmatched +0   Dcode% +0.000000pp   Dcode_bytes +0   Dfuzzy -0.000004pp
control `none` FLAT; tool classifies the patch a pure RE-name
units at 100%: 187 -> 187 (mpn), 167 -> 167 (all-rows-fuzzy)
```

Pre-registered before the run: Δ0 on matched, code bytes and code%, because the
row moves from mpn 0.769 (paired against the wrong body) to 0 (unpaired —
nothing defines `NowBar::FillHit`), and both are far below the 100 admission
gate. **Scored exactly.** The −0.000004 pp of fuzzy is the *withdrawal* of
credit for a coincidental resemblance between two unrelated functions, i.e. the
metric got slightly worse and slightly truer.

## 7. Prediction scorecard

| # | pre-registered | outcome |
|---|---|---|
| 1 | The near-miss stratum contains real codegen work | ❌ **REFUTED** — 0 insert/delete/replace across all 44 rows |
| 2 | `mTails[i]` indexing is a shared source defect | ❌ **REFUTED** — 4 siblings with the identical construct sit at 100 |
| 3 | The register class is fixable by a uniform source spelling | ❌ **REFUTED** — retail is the inconsistent side |
| 4 | `MeshAO`/`Patch` are an ICF fold worth an alias | ❌ **REFUTED** on both compiled bytes (96 vs 132 B) and class structure |
| 5 | The zero stratum is bodies awaiting identification | ❌ **REFUTED** — 30/31 are unpaired; we emit no such bodies |
| 6 | `0x822AC058` is the real `OutfitConfig::Register` | ❌ **REFUTED by me**, §6.2 — it is `Init` with `Register` inlined |
| 7 | The map rename measures Δ0 on matched / bytes / code% | ✅ **CONFIRMED exactly** |

Six refutations and one confirmation. **Five of the six refutations closed a
vein that looked fundable from the outside**, which is the yield of this lane.
The seventh being right is worth less than the six being wrong.

## 8. What I did NOT do, and why

- **Installed no aliases.** 34 of the 41 callee pairs are unadjudicable because
  our spelling has no map address. An unproven alias lifts `name_check` *by
  construction*, and the obvious control cannot catch it. 0 ADMIT was the
  honest answer.
- **Did not extend the splits pin** over `0x822AC048`–`0x822AC110`. The
  justification for it evaporated with §6.2, and the brief is explicit that
  pinning is not decomp and is a last resort in this unit.
- **Did not port `NowBar.cpp`.** It is a whole new TU (243 oracle lines, a new
  `objects.json` entry and a new `splits.txt` block) and out of scope for a lane
  briefed on closing an existing unit's gap. It is written up above so the next
  lane starts from the identification rather than re-deriving it.
- **Did not run the permuter** — off by standing directive. This matters because
  the register class in §5 is exactly where it would be pointed.
- **Did not touch** `GemTrackDir.cpp`, `VocalTrackDir.cpp`, `Game.cpp`,
  `BandStorePanel.cpp`, `StoreOfferProvider.cpp` (other lanes' surfaces).

## 9. Tooling

`tools/comdat_fold_gate.py` crashed on **every** real run with
`TypeError: int() can't convert non-string with explicit base`, because 51 of
the 1,657 groups in `scripts/symbol_aliases.json` carry `address: null`. Fixed
in `f4ee477` by skipping address-less groups — they contribute no address and
so cannot establish an equivalence class, which leaves the branch-destination
compare falling back to a literal name compare: **stricter, never looser**, so
the guard cannot manufacture an ADMIT. The count is reported rather than
swallowed. Selftest after the fix: `SELFTEST PASS`, 0 defects over 130 extents.

## 10. Recommendation for the next lane

**Port `NowBar.cpp`** (§6.1). It is the only place in this unit where bytes are
reachable by writing source:

- Source: `../rb3/src/band3/bandtrack/NowBar.cpp`, 243 lines, named functions.
- Header already present at `src/band3/bandtrack/NowBar.h` with offsets.
- Target band `0x82BAA400`–`0x82BAAF30`, 13 rows / 2,644 B at fuzzy 0.
- Needs an `objects.json` entry (`NonMatching`) and a `splits.txt` `.text` block.
- One anchor is already proven and landed: `0x82BAA660` = `?FillHit@NowBar@@QAAXHH@Z`.

⚠ Price it against the **asm extents**, not `report.json` sizes, and expect the
Wii→360 port (MWCC→MSVC) to be the real cost rather than the logic.

**Do not re-fund** strata 1 and 2 of this unit. They are drained, and §3–§6
record the evidence that closed each one.
