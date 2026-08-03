# Lane EB-3 — working the COMPLETABLE frontier: findings, negative results, declines

Companion to `UNIT_COMPLETION_FRONTIER_EB3_2026-08-03.md` (the re-census).
Tree: `2e589b9b`, settled worktree. **No source change landed** — everything below
is measurement. Read the declines: three of them would each have cost a lane a day.

## ★★★ FINDING 1 — the COMPLETABLE bucket contains MISATTRIBUTED rows, and they sort to the TOP

The census ranks "cheapest COMPLETABLE" by **highest fuzzy%**. I worked that list from
the top and the first two entries were unreachable for reasons the ranking cannot see.
Worse, several blockers are not body divergences at all: **the retail function at the
pinned address belongs to a different class than the name assigned to it.**

| unit | blocker | witness that it is FOREIGN | ICF-immune? |
|---|---|---|---|
| `system/gesture/SkeletonDir` | `TestClip` (8 B) | retail reads `this+0x7c74`, ours `this+0x248` — a **31,276-byte** layout gap in a class whose DC3 header is byte-identical to ours | yes (offset, not a reloc) |
| `HamDriver` | `Clear` (12 B) | retail loads a **global** into r3 (discarding `this`) and tail-calls `ContextWrapperPool::FailAllContexts`; DC3's `HamDriver::Clear` is `mLayers.Clear()`, identical to ours | partly — see note |
| `FilterQueue` | `CancelJob` (52 B) | retail stores `1.0f`/`0.0f`x3 into `this+0x3c..0x48` (**our `CancelJob` has no float stores at all**) and reads a vector at `this+0x280` vs our `0xc` | **yes** |
| `FlowQueueable` | `Deactivate(bool)` (168 B) | retail **consumes an FPR argument** (`fmr f31, f1`) and calls `Keys<Vector3>::Add` / `Keys<Vector2>::Add`. `Deactivate(bool)` has no float parameter | **yes** |

⚠ **I deliberately did NOT rest any of these on the tail-call callee name.** objdiff loads
**784 ICF equivalence entries**, and MSVC folds same-shape `vector<T>::erase` bodies, so
"target calls `vector<RndText::Line>::erase`, we call `vector<FilterInputFrame>::erase`"
is exactly the alias hazard — it proves which body you EQUAL, not whose you ARE. The
witnesses in the table are **argument class, float-store presence, and member offsets**,
none of which an ICF fold can manufacture.

★ `FlowQueueable` is a **multi-row** unit, so misattribution is **not** confined to the
single-row carve slivers. That was my initial hypothesis and it is too narrow.

**Sub-population sizing.** 5 of 39 COMPLETABLE units are single-row (a whole `.cpp`
pinned to 8-52 bytes): `FilterQueue`, `StorePurchaser`, `HamDriver`,
`system/gesture/SkeletonDir`, `auto_03_82402F68_text`. Three of the four I examined
were foreign. **These are pin/naming defects; "completing" them from source would be
metric-fitting** and is the reason I landed nothing on them.

## ★★★ FINDING 2 (negative, high-confidence) — STATEMENT-ORDER PERMUTATION IS INERT

I tested the "recover retail's source statement order" lever on four functions with
**ten source variants**, each rebuilt and diffed. **Every semantically-equivalent variant
produced BYTE-IDENTICAL output.**

| function | variants tried | result |
|---|---|---|
| `DSP::LowpassCoefficients` | `[1][0][2]` reorder; `double` temp; `float` temp; control | 3 byte-identical, 1 (double temp) *structurally worse* (87 vs 88 instrs) |
| `IPP::Add_InPlace` | `f2[i] += f1[i]`; `f2[i] = f2[i] + f1[i]`; `float t = f2[i]; f2[i] = f1[i] + t` | all 3 byte-identical |
| `FIRFilter::setCoefficients` | hoist `resultDivFactor` above `lengthDiv8`/`length` | byte-identical |

⇒ **MSVC X360 /O1 normalises independent scalar/member assignment order before
scheduling.** The residual in all three is FPR/GPR *scheduling* (which of two independent
loads issues first; whether an `frsp` can overwrite its source in place). That is the
permuter class, and **the permuter is OFF by standing directive** — so these rows are
walls, not backlog.

⚠ A harness note that made this trustworthy: my variant script has an **identity control**
(`ctrl` regenerates the file byte-for-byte). Without it, "the edit did nothing" and "the
edit was never applied" are indistinguishable — and the latter is the failure mode that
manufactures a fake negative result.

⛔ Do **not** re-fund "reorder the assignments to match retail" for this residual class.

## ★ FINDING 3 — the census's cheapness ranking is ANTI-correlated with tractability

Both 95-98% rows I opened were scheduling walls; the rows with *real, source-shaped*
defects were the **low**-% ones (`FilterQueue` 29%, `FlowQueueable` 0%, `MemcardMgr` 83%).
High fuzzy% means "the compiler agrees with us about almost everything", which is precisely
the state where what is left is *codegen*, not *source*. Rank by **defect signature**,
not by fuzzy%.

## Blocker-row signature census (all 39 COMPLETABLE units, 83 rows)

Built with `objdiff-cli diff -f json` per row; every label names **what was measured**.

| signature | rows |
|---|---:|
| `HEAVY_DIVERGENCE` | 28 |
| `OFFSET_ONLY` | 28 |
| `FOREIGN_BODY` | 11 |
| `SIZE_GAP` | 7 |
| `ORDERING_INS_DEL` | 4 |
| `FPR_ARG_CLASS_DIFFERS` | 3 |
| `REG_ONLY_SCHEDULING` | 2 |

`OFFSET_ONLY` (28) is dominated by 40-44 B **EH unwind funclets** whose single mismatch is
a parent-frame slot offset. `HEAVY_DIVERGENCE` (28) is the genuine source backlog.

### Per-unit (suspect first)

| unit | rows | signatures |
|---|---:|---|
| `DelayEffect` | 1 | FOREIGN_BODYx1 |
| `SoftParticleBuffer` | 1 | FOREIGN_BODYx1 |
| `Rnd_NG` | 1 | FOREIGN_BODYx1 |
| `HamDriver` | 1 | FOREIGN_BODYx1 |
| `system/gesture/DrawUtl` | 2 | OFFSET_ONLYx1, FOREIGN_BODYx1 |
| `HamRibbon` | 3 | OFFSET_ONLYx2, FOREIGN_BODYx1 |
| `PropertyEventProvider` | 3 | HEAVY_DIVERGENCEx2, FOREIGN_BODYx1 |
| `BoxMap` | 5 | HEAVY_DIVERGENCEx4, FOREIGN_BODYx1 |
| `HolmesClient` | 5 | OFFSET_ONLYx3, FOREIGN_BODYx2 |
| `FFT` | 6 | HEAVY_DIVERGENCEx4, SIZE_GAPx1, FOREIGN_BODYx1 |
| `FilterQueue` | 1 | FPR_ARG_CLASS_DIFFERSx1 |
| `PreloadPanel` | 1 | FPR_ARG_CLASS_DIFFERSx1 |
| `FlowQueueable` | 1 | FPR_ARG_CLASS_DIFFERSx1 |
| `OvershellPartSelectProvider` | 1 | HEAVY_DIVERGENCEx1 |
| `SHA1` | 1 | HEAVY_DIVERGENCEx1 |
| `FlangerEffect` | 1 | HEAVY_DIVERGENCEx1 |
| `PitchDetector` | 1 | HEAVY_DIVERGENCEx1 |
| `HamPhotoDisplay` | 1 | HEAVY_DIVERGENCEx1 |
| `StorePurchaser` | 1 | HEAVY_DIVERGENCEx1 |
| `EQEffect` | 1 | HEAVY_DIVERGENCEx1 |
| `system/meta/MemcardMgr` | 1 | HEAVY_DIVERGENCEx1 |
| `system/gesture/SkeletonDir` | 1 | HEAVY_DIVERGENCEx1 |
| `auto_03_82402F68_text` | 1 | HEAVY_DIVERGENCEx1 |
| `Mat_NG` | 4 | HEAVY_DIVERGENCEx2, OFFSET_ONLYx1, SIZE_GAPx1 |
| `MidiReader` | 4 | HEAVY_DIVERGENCEx3, OFFSET_ONLYx1 |
| `MicInputArrow` | 4 | OFFSET_ONLYx2, ORDERING_INS_DELx1, HEAVY_DIVERGENCEx1 |
| `StorePreviewMgr` | 5 | OFFSET_ONLYx3, HEAVY_DIVERGENCEx2 |
| `FileChecksum` | 1 | SIZE_GAPx1 |
| `PitchCorrectedVoice` | 1 | SIZE_GAPx1 |
| `system/synth/MoggClip` | 1 | SIZE_GAPx1 |
| `FftIpp` | 2 | SIZE_GAPx1, ORDERING_INS_DELx1 |
| `UIStats` | 4 | OFFSET_ONLYx3, SIZE_GAPx1 |
| `FIRFilter` | 1 | ORDERING_INS_DELx1 |
| `RealGuitarTrackWatcherImpl` | 2 | OFFSET_ONLYx1, ORDERING_INS_DELx1 |
| `FilterCoeffs` | 1 | OFFSET_ONLYx1 |
| `Main` | 1 | OFFSET_ONLYx1 |
| `ScrollbarDisplay` | 3 | OFFSET_ONLYx3 |
| `AccomplishmentPlayerConditional` | 6 | OFFSET_ONLYx6 |
| `IPP_basicmath_xbox` | 2 | REG_ONLY_SCHEDULINGx2 |

## ⛔ FINDING 4 — the 1,174-row 40-byte funclet cohort is SIZED and is NOT a unit lever

Scanning `report.json` for the repeated "40 B at 99.9%" shape that recurs across the
multi-blocker units: **1,174 anonymous 40-byte rows in [99,100) fuzzy, across 262 units**
(mpn 99.9 x663, 99.8 x477). They are EH unwind funclets (`subi r31, r12, 0x70` prologue);
the residual is a **parent-frame slot offset** plus, in the sampled case, a *different
destructor callee* — i.e. the parent function's local set differs.

**Zero units are blocked solely by this cohort**, so it cannot complete a unit. It is a
~47 KB / ~1,174-function `matched_functions` opportunity if the *parents* were fixed, but
it is derived work, not an independent lever. ⚠ Note the sampled funclet's parent scores
**mpn 100** while destroying the wrong object — a fresh instance of "mpn is blind to
callee identity"; the funclet is the only thing disclosing the parent's defect.

⚠ Coercion trap, hit live: `functions[].size` in `report.json` is a **JSON string**. My
first cohort query returned `0 rows` — a clean, decisive-looking negative — purely because
`sz==40` never matched `'40'`. Same family as the `matched_code` string trap.

## Declines, with the witness for each

| unit | decline reason | is the witness capable of discriminating? |
|---|---|---|
| `SkeletonDir`, `HamDriver`, `FilterQueue`, `FlowQueueable` | blocker is a **foreign body** (table above) | Yes — offsets / float-store presence / FPR argument class. All differ between target and base and none is a masked reloc. |
| `FilterCoeffs`, `IPP_basicmath_xbox` | FPR/GPR **scheduling**; 10 source variants inert | Yes — the instrument is byte-level output of a real recompile, and its identity control proves it can register a change. |
| `Main` (`main`, 1 mismatch) | target is `bcl 20, lt, X`, ours `bl Y` — an **opcode-class** difference no C++ statement produces | Partly — I could not construct a source form that emits `bcl`; recorded as unexplained rather than as at_limit. |
| `EQEffect::Process` | 46-instruction regswap cascade + uniform `off:+/-24`; permuter class | Weak — the `off:24` could be a real layout defect that would dissolve the regswap. **Left open, not closed.** |
| `MemcardMgr::SaveLoadAllComplete` | **genuine and structural**: retail computes `r4 = this+0x20` as a constant; we go through a **vbtable displacement chain** (`*(*(this+4)+4) + this + 4`) | Yes — but the fix is an inheritance change to `MemcardMgr` with wide blast radius. **Best remaining single target; left for a lane with budget to A/B it.** |

## What I did NOT do

- **Landed no unit completion.** Every unit I opened was a wall or a misattribution; I
  will not fabricate padding or fake stores to close one.
- Did **not** run the native gate — no `src/` change survives, so it does not apply.
- Did **not** adjudicate whether the four foreign-body rows should be **repointed or
  unpinned** in the map. That is real map work and needs the retail-byte + address-
  neighbourhood protocol; note a straight repoint pays ~0 on the metric (masked reloc),
  so the payoff is unit membership only.
- Did **not** re-examine `MoggClip` (`51c3c615` refuted a fourth source form) or re-fund
  the `??_E` lever (refuted, 0 units).
