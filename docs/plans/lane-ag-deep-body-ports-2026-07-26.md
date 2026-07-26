# Lane AG — the deep-body-port residue (the ~45% every tooling lane declined)

**2026-07-26.** Worktree `~/tmp/wt-laneAG-bodies`, branch `laneAG-bodies`,
base `7dd6f685` (**30,093** strict). Six Opus fixers in independent worktrees
(`laneAG-{b3a,b3b,b3c,sysa,sysb,sysc}`) + one Sonnet pricing pass.

Prior art: `docs/plans/identical-pct-cluster-scan-2026-07-26.md` (the decoder),
`docs/plans/funclet-cascade-lever-2026-07-25.md` §9/§12–§31.

---

## 1. ★ The honest size of the fundable pool: 309 functions

Every previous statement of "how much deep-body work is left" was a share of a
pre-filtered denominator. This is the funnel built directly from
`build/45410914/report.json` at 30,093, with each step measured:

| step | filter | removed | remaining |
|---|---|--:|--:|
| 0 | named, paired (0 < pct < 100), N ≥ 8, **78–96 flip band** | — | **433** |
| 1 | drop VA-identified retail coverage-breadcrumb stubs | 1 | 432 |
| 2 | drop cluster size ≥ 5 on any of `pct` / `score_shape` / `delta_shape` | **100** | 332 |
| 3 | drop ARG-ONLY-only divergence | 0 | 332 |
| 4 | drop STL element-`sizeof` family (`_M_fill_insert`, `_M_insert_overflow_aux`, `__uninitialized_*`, `resize`, `push_back`) | 23 | **309** |

**The cluster-size filter removes the most — 100 of 433 (23%)**, an order of
magnitude more than stubs (1) or the STL family (23) combined. That is a direct
confirmation of the anti-predict rule (≥20 → 0/45; 10–19 → 0/13; 5–9 → 0/44;
3–4 → 3/77): the biggest single act of triage available in this pool is simply
**refusing every function that shares its penalty with four or more others.**

Whole named paired sub-100 pool for context (1,425 functions, f32 round-trip
inverted `S` losslessly for **all 1,425**, zero failures):

| band | all | N ≥ 8 |
|---|--:|--:|
| < 50 | 336 | 305 |
| 50–78 | 180 | 165 |
| **78–96** | **440** | **433** |
| 96–97.5 | 80 | 80 |
| 97.5–99.8 | 168 | 146 |
| > 99.8 | 221 | 219 |

78–96 band by area: **system/ 365, band3/ 61, network/ 4, xdk/ 1, other 2.**

### 1.1 ★★ The stub census does NOT apply to this lane

Project memory prices 17,771 retail coverage-breadcrumb stubs binary-wide
(13.7% of carved `.fn` symbols) and every pricing exercise so far has deducted
some share of them. Measured against the actual band:

> **0 of 433 (0.0%) of the 78–96 flip band is a genuine breadcrumb stub.**

Cross-referenced two independent ways — VA lookup through the inverted
`scripts/target_symbol_map.json` (58 pool hits binary-wide, 56 of them the exact
32-byte/8-instruction shape) and direct target-asm shape extraction from
`build/45410914/asm/`. Stubs are real and large, but they land at
**pct 17.5 / 25.0 / 37.5 / 50.0** — an 8-instruction body cannot produce a
78–96% score. The single band "hit" (`ChordbookPanel::SetFret`, S=2004) is a
VA/name-map artifact, not a stub.

**Rule: stop deducting the stub population from flip-band estimates. It is a
`<50%`-band phenomenon.** Run the census before pricing a *unit*; do not apply
it to a flip-band worklist.

### 1.2 The 78–96 band is where clustering is most trustworthy — and least useful

All 140 clustered band members are **STRUCTURAL**; **zero are ARG-ONLY**. That is
not luck: an ARG-ONLY penalty (a handful of `PENALTY_IMM_DIFF=1` / `REG_DIFF=5`
hits) is arithmetically incapable of dragging a function below 96%. So step 3 of
the funnel removes nothing.

The corollary is the useful half: **300 of the 440 band members are in no cluster
at all.** The band is dominated by singletons — i.e. by genuinely per-function
work. This is the quantitative statement of why this lane exists and why it is
the last one standing.

---

## 2. ★★ NEW WALL CLASS: inline-body edits in shared headers cost ~40 for 0

The single most transferable result of this lane, because it kills an entire
class of plausible-looking one-line fixes.

`Morph`'s `operator>><Weight>(BinStreamRev&, Key<Weight>&)` reads 94.44444 =
N=18, **S=100 — exactly one inserted instruction**, and the instruction is
legible without ambiguity:

```
    bl   ??5@YAAAVBinStream@@AAV0@AAVVector3@@@Z
+   mr   r3, r30            <-- BASE ONLY: reloads `bs`
    li   r5, 0x4
    addi r4, r31, 0xc
    bl   ?ReadEndian@BinStream@@QAAXPAXH@Z
```

Retail chains: it feeds the *return value* of the inner `operator>>` straight
into `ReadEndian`. We reload the original stream. The source is a three-line
inline in `src/system/math/Key.h`:

```cpp
inline BinStream &operator>>(BinStream &bs, Weight &w) {
    bs >> (Vector3 &)w;
    return bs;                 // <-- discards the inner call's return
}
```

The obvious fix — `return bs >> (Vector3 &)w;` — was applied and A/B'd
whole-binary. Result:

> **30,093 → 30,054. GAINED 0, LOST 39.** Reverting restored **exactly 30,093,
> LOST 0** — so the −39 is deterministic and real, not build noise.

Two independent lessons, both worth more than the target was:

1. **The intended target did not move at all** (94.44444 before and after). MSVC
   canonicalises the returned reference of an *inlined* helper back to the
   original object, so you cannot steer a chained-return through an inline. To
   reproduce retail's shape the helper would have to be genuinely out-of-line —
   which is a different (inline-policy) lever entirely.
2. **The 39 losses are in units with no relation to the edit** — `Archive`,
   `ContentMgr`, `Memcard`, `MemcardMgr_Xbox`, `MidiInstrument`, `Song`,
   `Splash`, `StoreOffer`, and the whole `system/synth/FxSend*` `SyncProperty`
   family. They did not fall to 0; they fell to **99.875 / 99.96296** — i.e.
   `S=1`, *one immediate*. That is the signature of COMDAT-ordering /
   scope-counter drift inside the obj, not of changed logic.

> **RULE. Changing the *body* of an `inline` function in a widely-included
> header is net-negative by default, at roughly −40/0, even when the change is
> semantically identical and even when the intended target does not move.** The
> existing "shared-header edits need a whole-binary A/B" guidance understates
> this: the prior should be *don't*, and the A/B is to confirm the damage, not
> to look for a win. Adding/removing declarations is not the same thing — this
> is specifically about perturbing an emitted inline COMDAT's contents.

---

## 3. NEW WALL CLASS: volatile-live-across-call regalloc divergence

`RndShaderMgr::SetTransform` (`default/MeshAnim`, 81.7%, N=23) is a 19-instruction
function whose target and base perform **identical work in identical order**. The
entire divergence is the register *class*:

| | target (retail) | base (ours) |
|---|---|---|
| frame | `stwu r1, -0xb0` | `stwu r1, -0xa0` |
| `this`, vtable held in | `r30` / `r31` (non-volatile, `std`/`ld` saved) | `r8` / `r9` (**volatile**) |
| the 4 ins/del | exactly the `std`/`ld` save+restore pair | — |

The source is a two-liner that matches the target's semantics exactly
(`mBoneCount = 0; SetVConstant4x3(kVS_WorldTransform, Hmx::Matrix4(xfm));`), and
`Hmx::Matrix4::Matrix4(const Transform &)` is only *declared* in `math/Mtx.h`,
so no in-TU body is visible.

Verified against our own object bytes rather than objdiff's rendering
(`build/45410914/src/system/rndobj/MeshAnim.obj`, `.text` + 0):

```
7d8802a6 mflr r12      9421ff60 stwu r1,-0xa0    81030000 lwz  r8,0(r3)
7c691b78 mr   r9,r3    4bffffe1 bl   Matrix4::Matrix4   81680030 lwz r11,0x30(r8)
```

**Our build really does keep volatile `r8`/`r9` live across a `bl`.** Retail, for
identical source, does not. The size delta is nonzero, so the corrected
`regswap ⇒ at_limit` rule does *not* dismiss it — but the only inserted/deleted
instructions **are** the callee-save pair, i.e. a consequence of the register
choice rather than independent structural evidence.

> **Shape to recognise: identical instruction stream, target uses non-volatiles +
> `std`/`ld` saves, base uses volatiles, and the entire frame delta is exactly
> the size of those saves.** Mark it `volatile-live-across-call` and move on —
> but flag it, because the mechanism is not understood and a general explanation
> would be worth far more than the individual function. (Candidate: some property
> of the callee's declaration/visibility that makes our compiler assume
> preservation.)

---

## 4. Routing rules as re-confirmed by this lane

- The decoder is the cheapest triage in the project. `S = round((100 - pct) * N)`,
  `N = size/4`; `PENALTY_INSERT_DELETE=100`, `PENALTY_REPLACE=60`,
  `PENALTY_REG_DIFF=5`, `PENALTY_IMM_DIFF=1`. `S=100` ⇒ exactly one
  inserted/deleted instruction; `S=60` ⇒ exactly one replace. **49 functions
  binary-wide sit at S=60 and 42 at S=100** — those 91 are the near-free tier and
  are enumerated by the pricing pass.
  ★ Force both sides of any round-trip through `struct.pack("<f", …)`;
  `report.json` stores an f32's shortest repr and Python parses it to a double.
- **Refuse cluster size ≥ 5.** Biggest single triage win available (−100 of 433).
- **`regswap ⇒ at_limit` only when the size delta is zero AND there is no
  insert/delete/diff_op anywhere.** §3 above is exactly the case that rule would
  have mis-killed on the "there are deletes" test and mis-*kept* on the
  "regswaps are never causal" test — read both halves.
- The stub census is a `<50%` phenomenon; do not deduct it from a flip-band pool.

---

## 5. Fixer results

*(filled in from the six per-target worktrees — see §5.1)*
