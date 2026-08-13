# `RndMat::SyncProperty` crosses to 100% — and the residual was NOT property order

Lane SYNCPROP-1, 2026-08-13. Closes the follow-up lead METAMAT-1 flagged in
`docs/decomp/metamaterial-does-not-exist-in-rb3-retail-2026-08-13.md`.

**RESULT: `?SyncProperty@RndMat@@UAA_NAAVDataNode@@PAVDataArray@@HW4PropOp@@@Z`
(4,808 B) went fuzzy 98.186 / mpn 99.118 → 100.0 / 100.0. All 1,202 instructions
equal. Whole-binary Δ = +1 matched / +1 honest / +4,808 `matched_code` bytes /
+0.046586 pp code%.**

---

## ⛔ The briefed hypothesis was REFUTED, in the first measurement

METAMAT-1 wrote: *"The residual is very likely the property **order**."* It is not,
and the instrument that says so is unambiguous. `diff_inspect --mode diagnose` on
the baseline:

```
diff_arg instructions: 224
  Explained by root causes: 224
    Offset shifts:     0 arg diffs
    Register swaps:  216 arg diffs
    Symbol relocs:     0 arg diffs      <-- THE ANSWER
    Branch dests:      8 arg diffs
  Unexplained:         0
```

**`Symbol relocs: 0`.** Every one of the ~45 property `Symbol` references — the
`.rdata` string pointer, the static-`Symbol` storage, the guard bit, the
`??0Symbol@@QAA@PBD@Z` call — was already byte-identical *and in the same
instruction index* as retail. A wrong property order cannot produce that; it
would shift every subsequent block and light up the reloc column. The order was
already right, and had been since before this lane opened.

★ **Generalisable:** on a `SYNC_PROP`-style dispatcher, `Symbol relocs == 0` is a
one-line refutation of any ordering hypothesis. Read it before opening the row.

## What the residual actually was: three source defects, found on retail bytes

The 1,212-instruction baseline decomposed as 216 regalloc-swap args + 8
branch-dest + **exactly 10 surplus instructions in our build, 0 deletes**.

### 1. A surplus `SYNC_SUPERCLASS(Hmx::Object)` — worth all 10 instructions

Retail's body simply ends. Read straight out of `orig/45410914/band.exe` via
`tools/xex_string_at.py` (Python, never `grep` — binary-blind here), at the end of
`fn_82436488` (`0x82436488 + 4808 = 0x82437750`):

```
82437740  4b ff f6 e0   b     <common false exit>
82437744  38 60 00 00   li    r3, 0            <- return false
82437748  38 3f 00 c0   addi  r1, r31, 0xc0    <- epilogue
8243774c  48 3f 1b 4c   b     __restgprlr_*
```

There is **no `bl ?SyncProperty@Object@Hmx@@`** anywhere in the body, and objdiff
reported `0 delete / 0 diff_op`, so nothing was hiding elsewhere. `SYNC_SUPERCLASS`
expands to `if (parent::SyncProperty(...)) return true;` = 4× `mr` arg setup +
`bl` + `clrlwi`/`subic`/`subfe`, plus it gave the property list a **second exit
path**, which is why a `li r3,0; b` in the *first* property block could not fold
into the common tail. Removing the one line killed both insert clusters.

**Where it came from — our own history, not a divergence.** DC3 (newer) has
`RndMat : BaseMaterial` and correctly ends the list `SYNC_SUPERCLASS(BaseMaterial)`.
When BASEMAT-2 (`9ea37046`) merged `BaseMaterial` *into* `RndMat`, that line was
mechanically rewritten to name the new base — `Hmx::Object` — rather than
re-adjudicated against retail. rb3-Wii, the RB3-*era* oracle, ends its
`BEGIN_PROPSYNCS(RndMat)` with a bare `END_PROPSYNCS` and no `SYNC_SUPERCLASS` at all.

★ **And the 216-instruction register swap DISSOLVED with it** — r26↔r28 and
r25↔r27 across the whole body, gone, zero permuter work. Exactly the documented
pattern (`fixable-liveness.md`): the superclass call forced `_val`/`_prop`/`_i`/
`_op`/`this` to stay live to the very end of the function, which changed the
callee-saved assignment everywhere. **A `REGISTER_SWAP` label on a sub-100 row is
a symptom; do not open it as a regalloc problem.**

### 2. `next_pass` does not dirty the material

After `bl ??$PropSync@VRndMat@@`, retail branches to **`0x12c0`, the epilogue**,
returning PropSync's result directly — *not* to the shared
`clrlwi./beq/andi./mDirty |= 2` tail at `0x0ac` that every `SYNC_MAT_PROP` uses.
rb3-Wii agrees literally: `SYNC_PROP(next_pass, mNextPass)`, no dirty flag.

⚠ Written out as an explicit `{ static Symbol _s("next_pass"); ... }` block rather
than `SYNC_PROP`, because `Mat.cpp` does **not** carry
`/DRB3_SYNCPROP_LOCAL_STATIC`, so the live `SYNC_PROP` overload takes a `Symbol`
*variable*, not a string literal. Check the gate before reaching for that macro.

### 3. `cull` syncs as a **bool** — and the old code was a REAL MEMORY BUG

Retail passes `&this->mCull` directly (`addi r3, r26, 0x11c`) into
`?PropSync@@YA_NAA_NAAVDataNode@@…` — the **`bool&`** overload. Ours called the
`int&` overload at a different call site.

This is not merely a codegen mismatch. `BaseMaterial.h:414` declares
`unsigned char mCull; // 0x11c` — **one byte** — and `Mat.cpp` cast it
`(int &)mCull`. A `kPropSet` on `cull` therefore wrote **four** bytes,
`0x11c-0x11f`, clobbering:

| offset | member |
|---|---|
| 0x11c | `mCull` |
| 0x11d | `mPerPixelLit` |
| 0x11e | `mScreenAligned` |
| 0x11f | `mEnvironMapFalloff` |

Setting one material property silently corrupted three others. The header comment
already read *"stored as 1 byte in retail"* — the storage was known; the cast was
never fixed. rb3-Wii declares `bool mCull : 1`.

⚠ **Retail does NOT use rb3-Wii's `bool bit = mCull; … mCull = bit;` stack-temp
pattern** — it passes the member by reference. So the matching form is
`(bool &)mCull`, not a local. The member type was left `unsigned char`
deliberately: changing it would ripple into `Save`/`Load`/`Copy` (`bs << mCull`,
`d >> (int &)mCull`) for zero metric gain, and this lane has no retail evidence
about those.

★ Both #2 and #3 were found by reading *branch destinations*, after the body was
otherwise 100%. The pair cost only `fuzzy 99.991684` — but `matched_code` is
all-or-nothing per row, so those two instructions were worth the entire 4,808 B.
**`mpn` hit 100.0 while `fuzzy` sat at 99.991684**: the row had landed in the
"counted as a matched function, bytes withheld" class. `run_objdiff` displayed
"100.0%" by rounding at both stages. **Read `fuzzy_match_percent` out of
`report.json`; do not trust a displayed 100.**

## Measured result (`tools/ab_measure.py --from-dirty`)

Ruler read from `report.json` `provenance`: **`functionRelocDiffs=name_check`**
(the shipped default), objdiff-cli sha256 stable across both legs
(`6a4d96e3b7ecb6e4`).

| | leg A | leg B | Δ |
|---|---|---|---|
| `matched_functions` | 44,275 | 44,276 | **+1** |
| `masked_equal_functions` | 22,889 | 22,889 | **0** |
| **honest** (`matched − masked_equal`) | 21,386 | 21,387 | **+1** |
| `matched_code_percent` | 34.445960 | 34.492546 | **+0.046586 pp** |
| `matched_code` bytes | — | — | **+4,808** |
| `fuzzy_match_percent` | 48.382580 | 48.382996 | +0.000416 pp |

- `Δmasked_equal = 0`, so unlike METAMAT-1's result this is entirely in the
  **honest** channel — not disclosure.
- unit improvements: `default/Mat` 60→61; unit net (all units) **+1** == whole-binary
  Δmatched, so the gain is fully attributed with no offsetting regression.
- units at 100%: **252 → 252** (mpn), **119 → 119** (fuzzy) — 0 reached, **0 fell off**.
  `default/Mat` is 61/74, so this row did not complete a unit.

## Native

`tools/native_build_gate.sh`: **PASS, rc=0, 18/18 targets verified, 0 errors,
0 warnings, 0 SKIPs**, after seeding the cmake cache with all four absolute flags
(`rb3-frame`, `rb3-milo`, `rb3-render` all relinked — the three that silently SKIP
in a `~/tmp` worktree otherwise).

## Coupled files — censused, none required

No class name, signature or nested type changed, so the coupling that cost
BASEMAT-2 −248 B does not arise. Censused on full mangled forms anyway:
`scripts/target_symbol_map.json` has 3 `SyncProperty@RndMat` rows (still valid —
nothing was renamed, and the row matches at 100%), `scripts/symbol_aliases.json`
has 0.

## Serialization

`SyncProperty` is not half of a read/write pair, and **`RndMat::Save` / `Load` /
`Copy` were not touched on either side.** The `cull` fix was deliberately confined
to the propsync call site precisely so the `bs << mCull` / `d >> (int &)mCull`
serialization pair stays symmetric.

## Deliberately NOT done

- **`mCull` was not retyped to `bool`.** It stays `unsigned char`; only the propsync
  cast changed. Retyping would ripple into `Save`/`Load`/`Copy` with no metric gain
  and no retail evidence for those bodies.
- **`d >> (int &)mCull` in `BaseMaterial.cpp:219` was left alone.** It is the same
  `(int &)`-on-a-1-byte-member shape as the bug fixed here and is a **strong lead**,
  but it lives in `RndMat::Load`, a different function with its own retail bytes,
  and adjudicating it is a separate change. **Flagged, not guessed at.**
- **The two spans METAMAT-1 orphaned onto `MetaMaterial.cpp`** (`0x824382D4-0x8243833C`,
  `0x82578A90-0x82578C1C`) are untouched — still lane SPLITS-3's region.
- **`splits.txt` / `objects.json` untouched.** (The split re-derives `.pdata` lines
  on every run, which dirties `splits.txt` in any worktree; that churn was reset
  before the A/B so it could not ride into the patch.)
- **No permuter was run** — it was never needed; the regalloc swap was a symptom.
