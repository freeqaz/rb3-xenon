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

### 5.0 ★ The lane died mid-flight; the six fixers' work was recovered

The lane lead terminated on repeated API 529s **while all six Opus fixers were
still running**. Their worktrees survived on disk with live branches. Recovery,
re-measured against a fresh baseline pickle at main `913f7ebd` (**30,101**, so
main had advanced +8 from the 30,093 in §1) rather than against any worker's
self-report:

| branch | committed | uncommitted (rescued) | verified strict Δ |
|---|---|---|--:|
| `laneAG-b3a` | none | 6 files, band3 vocal/player | (build-broken, see below) |
| `laneAG-b3b` | 2 commits | `Campaign.cpp/.h` | **+2** |
| `laneAG-b3c` | 3 commits | `SigninScreen.cpp` | **+14** |
| `laneAG-sysa` | none | `Geo.cpp`, `Mtx.h` | (build-broken) |
| `laneAG-sysb` | 1 commit | `rndobj/Utl.cpp` | **+5** |
| `laneAG-sysc` | 2 commits | `BandCharacter.cpp`, `CharClip.cpp` | **+11** |

All four self-reports were **accurate** (`+10/+2/+2`, `+5`, `+8/+3`, `+2`), which
is worth recording because two workers elsewhere the same day reported wrong
numbers. Merging all four into `laneAG-bodies`: **30,101 → 30,133, GAINED 32,
LOST 0** — exactly `2+14+5+11`, i.e. the four branches are fully independent and
additive.

★ **The rescued *uncommitted* work did not compile.** Two independent
signatures: `PerfectOverdriveTracker.cpp(92)` ambiguous `stlpmtx_std::max`
(C2780/C2782) on `laneAG-b3a`, and on `laneAG-b3b` a `map`→`hash_map` typedef in
`Campaign.h` that breaks `BandProfile.cpp(949)` and `TourDescPanel.cpp(214,216)`.
So a dying lane's uncommitted buffer is a **lead, not a patch** — it is by
definition a snapshot taken mid-edit, and it must be applied file-by-file with a
measurement each, never wholesale. It was still worth rescuing: each was tagged
`laneAG2-wip-<branch>` before its branch was reset, so nothing is lost.

**Operational rule this establishes:** commit-or-tag every fixer worktree the
moment a lane looks unstable. The four branches that *had* committed contributed
+32; the two that had not contributed 0 despite comparable effort.

### 5.1 ★★ NEW: the 78–96% band structurally excluded the cheapest tier

§1's funnel starts at the **78–96 flip band**. That choice is defensible for
body-ports but it is *arithmetically incapable* of containing the cheapest work
in the binary, and this was not noticed:

> `S = 1` means **exactly ONE differing immediate operand** and nothing else.
> With `IMM_DIFF = 1`, such a function scores `(1 - 1/(100·N))·100` — i.e.
> **99.9x%**, always. It can never appear in an 78–96 band.

Inverting the penalty for all 961 named paired sub-100 functions (f32 round-trip
succeeded for **961/961**, zero failures) gives this histogram — and the largest
single tier in the entire binary is `S=1`:

| S | count | meaning |
|--:|--:|---|
| **1** | **42** | exactly one differing immediate |
| 100 | 31 | exactly one inserted/deleted instruction |
| 660 | 28 | (systemic) |
| 60 | 27 | exactly one replaced instruction |
| 120 | 23 | two replaces |
| 600 | 23 | (systemic) |
| 2 | 17 | two differing immediates |
| 300 | 17 | (systemic) |
| 200 | 14 | two insert/deletes |
| 5 | 11 | one differing register |

After the lane's own triage (cluster < 4, no STL stride family, `N ≥ 8`,
never-attempted), the routable near-free pool is:

- **S ∈ {60,100,120,160,180,200}: 82 functions** — the tier §1 was aiming at.
- **S ∈ {1,2,5}: 44 functions** — the tier §1 could not see.

★ The one caveat, from project memory: an ARG-ONLY penalty across a *cluster* is
the signature of a **`target_symbol_map.json` mispair**, not a source bug. The
whole `S ∈ {1,2,5}` tier is ARG-ONLY by construction, so `csz=1` singletons are
the trustworthy part and any clustered member whose differing immediate is a
struct member offset should be **reported to the map owner, not fixed here**.

Tooling: **`scripts/harvest/nearfree_tier_worklist.py`** (this lane) — inverts the
penalty, applies all of the above filters, and joins `decomp.db` attempt counts.
It replaces hand-rolled report.json greps for this kind of routing.

Note also that `decomp.db`'s `AT_LIMIT` verdict is **advisory, not
authoritative**: `EditSetlistPanel::Exiting` was recorded `AT_LIMIT` after one
attempt and this lane's `b3c` fixer flipped it to 100%. 22 of the 104 near-free
candidates carry a prior verdict; they are ranked last, not excluded.

### 5.2 Round-2 fixer results — +100

Seven Opus fixers: the six recovered worktrees re-tasked on top of the integrated
30,133 tip, plus a seventh (`imm`) on the newly-visible `S ∈ {1,2,5}` tier.
Each measured its own baseline pickle *before* editing; the lane then re-merged
and re-measured everything against main.

| fixer | Δ | attempted | closed | headline cause |
|---|--:|--:|--:|---|
| `b3a` | **+14** | 11 + WIP | 10 | pointer null-check signedness (`cmpwi` vs `cmplwi`) — one cause, 5 functions |
| `sysa` | **+11** | 12 + WIP | 9 | the strcpy `cmplwi` lever (§5.4); `regswap ≠ at_limit` twice more |
| `b3b` | **+10** | 8 + WIP | 6 | `map`→`hash_map`; by-value class-param temp; named-local-vs-temporary |
| `imm` | **+16** | 14 | 12 (+7 bonus) | `BandCamShot::mTargets` is `ObjList`, not `ObjVector` |
| `b3c` | **+7** | 8 + WIP | 5 | `NetSession::OnMsg` virtual-base upcast; `VertVector::mCapacity` is `int` |
| `sysb` | **+7** | 9 + WIP | 6 | per-group `Device()` fetch; rndobj/world near-frees |
| `sysc` | **+3** | 10 + WIP | 3 | `CharClip::Transitions::Save` writes `BytesInMemory()` first |

**Whole-binary A/B, unit-agnostic, against current main (`68ceb23d`, 30,144):**

> **30,144 → 30,244. GAINED 103, LOST 3. Net +100.**

An intermediate integration of six of the seven branches measured `+97`, and the
identical `+97` came out against *two different* baselines (main at 30,101 before
`laneAE2` landed, and 30,144 after) — both a determinism control and proof that
this lane and `laneAE2`'s +43 are fully independent. Merging the seventh
(`laneAG-sysc`, whose round-2 commit landed *after* that merge) added exactly its
measured +3.

★ **Process note worth keeping:** the seventh branch was nearly dropped. A fixer
that is resumed after the coordinator has already integrated will commit *behind*
the merge, and the merge commit gives no hint that anything is missing. Re-check
`git merge-base --is-ancestor <branch-tip> HEAD` for **every** branch immediately
before the final measurement, not once at integration time.

The 3 LOST are `fn_822B6AA0/6AE4/6B10`, anonymous EH funclets in
`default/BandCamShot`. Verified against `/FAs` listings that our funclet frame
constants are **byte-identical before and after**: the base funclet objdiff now
pairs to `fn_822B6AA0` emits `addi r3,r11,0x158` while the target needs `0x1f0`.
The obj's COMDAT set changed (`ObjVector<Target>` instantiations replaced by
`ObjList` ones) and the *unnamed-symbol pairing order* shifted with it. This is
the known objdiff funclet over-subscription issue (`project_objdiff_fork`), not a
source defect and not recoverable from source.

**Pool closure.** Reproducing §1's funnel exactly requires a third clustering axis
(`delta_shape`, which needs `--with-sizes` COFF parsing); a two-axis reproduction
of the same band gives 439 members at 30,101 (§1 measured 433 at 30,093 — the
difference is that §1's "named" included `$`-bearing thunk symbols, which is worth
stating since it is easy to get 327 instead by filtering them). Against that band:
**28 members reached strict-100, 372 remain.** The *near-free* tiers drained much
harder, which is the real signal: `S ≥ 60` never-attempted went **82 → 45**
(S=60: 20→7, S=100: 24→14), and `S ∈ {1,2,5}` went **44 → 29**.

### 5.3 ★★ Corrections to the decoder — S=5 is not a register, and report.json rounds

Two of the brief's own claims were wrong, both found by measurement:

1. **`S=5` is NOT "one differing register."** It is one differing **opcode**
   (`diff_op`), or five differing immediates. Of the five `S=5` targets: one was a
   single `ble`→`beq` from a signed `(int)size()` cast (closed with one word), one
   a branch-polarity `diff_op`, and three were 4–5 member-offset immediates.
   **None was the `at_limit` shape** — do not classify `S=5` as at_limit.
2. **`report.json` rounds `99.953` up to `100.0`**, so a strict count taken as
   `match_percent_normalized == 100.0` can credit a function that is not a byte
   match (`DataFunc::Quasiquote` in this lane: 1 residual commutative `add` swap).
   Relevant whenever lane counts are summed.
3. **Branch-target-only diffs are not scored** under `functionRelocDiffs=none`:
   `ResolvePartWaitStates` reached strict 100.0 while still showing
   `beq 0x2b8` vs `beq 0x2a0`. Corollary: `report.json`'s S is *smaller* than
   `objdiff-cli`'s raw S, which counts reloc/branch penalties. Price from
   `report.json` (that is the metric), but expect the raw diff to look worse.

### 5.4 ★★ The byte-copy loop: `extsb.` vs `cmplwi`, settled — and its sweep priced at zero

This is the lane's most transferable *mechanism*, and it is a case study in two
fixers reaching opposite conclusions on the same instruction.

`sysc` classified it **`build_env`/unreachable** after trying `/J`, `/O2`,
`unsigned char`, and an `(unsigned int)` cast (all gave `extsb.` or `mr.`), noting
retail uses `cmplwi rX,0` in its byte loops and **zero** of the ~98 enclosing
functions was matched anywhere in the tree. `sysa`, independently, **flipped two
functions with it** (`String::insert` 98.98→100, `String::operator=(FixedString)`
97.5→100, whole-binary +6/0).

**Resolution: retail is not calling `strcpy` at all**, so no flag can reach the
intrinsic. Verified on the retail `cl.exe` (16.00.11886.00, `/O1 /Oi /GR /EHsc /TP`)
with the loop held identical:

| temp declared as | compare emitted |
|---|---|
| `char` | `extsb. r8,r9` |
| `unsigned char` | `mr. r8,r9` ← sysc's result |
| `int` | `extsb` + `cmpwi` (5 instrs) |
| **`unsigned int`** | **`cmplwi r9,0`** ← retail |

★ **But declared width alone is not sufficient** (sysc's correction, and the part
most likely to mislead a later reader): the temp must be a named `unsigned int`
**read twice — once as the store's source, once as the test — in separate
statements.** `while ((c = (*d++ = *s++)) != 0)` folds `c` away and still emits
`mr.`. The working form:

```cpp
static void CopyStrZ(char *dest, const char *src) {
    --src;                 // MUST precede --dest: the first pointer decremented
    --dest;                // gets the lower register (else a 7-instr r10/r11 swap)
    unsigned int c;
    do {
        c = (unsigned char)*++src;   // separate statements, so `c` is a real
        *++dest = (char)c;           // 32-bit temp read twice
    } while (c != 0);
}
```
Keep it `static`/file-local so it inlines and emits no unpaired COMDAT. Do **not**
use `#pragma function(strcpy)` (forces a real `bl strcpy`, also not retail's shape).
Leave `strncpy` alone — it isn't inlined and already matches.

**★ There are THREE byte-loop shapes, not two.** Classifying every site in the
target asm: **DELTA 127 · XOR 95 · UPDATE 49.**

| shape | asm | status |
|---|---|---|
| **UPDATE** | `lbzu`/`cmplwi`/`stbu`/`bne` | **fixable** by `CopyStrZ` |
| **XOR** | `lbz`/`clrlwi`/`xor`/`stb`, ours has an extra trailing `clrlwi` before the `stb` | **walled** — MSVC 16.00.11886 unconditionally normalises a byte-xor result before `stb`; retail doesn't. Defeated 20 variants |
| **DELTA** | one src induction pointer + a constant `dest-src` delta: `addi r11,r31,0x544; subf r11,r3,r11; lbz r10,0(r3); cmplwi r10,0; stbx r10,r11,r3; addi r3,r3,1; bne` | **walled**, one instruction wide |

★ **The "compared register == stored register" test does NOT discriminate DELTA
from UPDATE** — DELTA passes it. The sound test adds `stbx`+`subf` (DELTA) vs
`stbu` (UPDATE).

DELTA is walled by a genuine conflict between the two requirements, measured over
six variants against a 97.5% baseline: exact `CopyStrZ` → 87.7% (right `cmplwi`,
wrong addressing); named temp + post-increment → 87.7%; single-index → 89.2%;
hand-written delta in two spellings → 87.3% each (MSVC canonicalises them). Any
named temp forces two lock-step pointers; hand-writing the delta reassociates to
`(r31-src)+0x544` instead of retail's `(r31+0x544)-src`. **The open question is
exactly one instruction wide:** `while ((*d++ = *s++) != 0)` with `unsigned char*`
reproduces DELTA **24/24 instructions**, sole diff `mr. r9,r10` vs `cmplwi r10,0`.
How do you get `cmplwi` when the tested value is an assignment-expression result?
Casts fold; naming it changes the addressing.

> ★★ **DO NOT commission a follow-up lane for the UPDATE recipe — its sweep is
> empty.** All UPDATE-shape sites were checked tree-wide against `report.json`:
> **zero** are in functions at ≥90%; every remaining one is in a function at 0% or
> unpaired, blocked by something else. The lever is real, already banked (+2), and
> has no residue. This is the `site count ≠ defect count` trap caught *before*
> paying for it: 127+95+49 = 271 sites, yield 2.

`sysc`'s four DELTA targets (`BandCharacter::OnGroupOverride`/`OnChangeFaceGroup`/
`OnPortraitEnd`, `BandDirector::PickDist`) stay walled but are **reclassified from
`build_env` to "reachable mechanism, wrong addressing form"** — the compiler
demonstrably *can* emit `cmplwi`, so "unreachable" was wrong. Each is worth +1
behind that single unresolved instruction.

### 5.5 Reusable levers banked this round

- **Pointer null-check signedness.** Retail null-checks object pointers with
  signed `cmpwi`; `if ((int)p)` reproduces it. Plain `p` gives `cmplwi`. One cause,
  5 functions in `b3a` alone; idiom already in-tree at `bandobj/StreakMeter.cpp:168`.
- **By-value class-param temp.** A *target-only* `stw <sym>, <slot>(r31)` dead store
  in a slot later reused by another local = a `Symbol` temporary materialised for a
  by-value class parameter. Trigger with a `const T &` parameter plus an explicit
  `T(x)` temporary at the call site. ★ **Scope it TU-locally** — widening a public
  signature added the temp at every cross-TU call site and measured +1 instead of +3.
- **Named local vs unnamed temporary.** Target `addi rN, r31, off` where base has
  `mr rN, r3` right after a ctor `bl` ⇒ retail used a **named local** (MSVC
  rematerialises the address instead of reusing the ctor's returned `this`). Must be
  wrapped in an **explicit scope** or the dtor slides (95.3 / 97.8 / 100 on `OnBack`).
- **For-loop rotation as a guard shape.** Retail's `srawi. r9,r9,2 / beqlr` entry
  guard is the *rotated latch of a real `for` loop*; a hand-written
  `if (size()==0) return;` gets MSVC's standalone zero-test peephole (`clrrwi.`).
- **One iterator variable, reassigned.** With two locals the first inlined
  `vector::erase` schedules its arg setups in the opposite order.
- **`regswap ≠ at_limit`, twice more.** `FileCacheFile::Seek` presented as 23
  regswap instructions incl. a 10-instruction r30↔r31 cascade on `this`. Root
  cause: retail keeps the result in **r3** through the tail; rewriting a ternary
  as statements returning `ret` gave 94.9 → 100% and **the whole cascade dissolved
  by itself.** Same story on `CharClip::Transitions::Save`, where an
  18-instruction "REGISTER_SWAP" was entirely downstream of one statement.
- **Per-TU rev-struct** (not a revision-constant fold): retail folds both rev words
  onto one base register at +0/+4, which only happens for internal-linkage
  `align(4)` file-scope statics, never for `DECLARE_REVS`/`INIT_REVS` class statics.
- **Absence of a string from the whole-binary `.rdata` dump is a clean oracle** for
  "retail lacks this DEV code path" — that is what settled `GetDLCMotd` on an
  842-instruction function (`MainHubPanel::Handle`, S=120, N=842).
- ★ **`kState_ChoosePartWarn` does not exist on the disc.** `ResolvePartWaitStates`
  needed `kState_ChooseDiff` (0xc) where we had `ChoosePartWarn` (0x4e) — so the
  confirm-loop the `HX_NATIVE` block just above works around was never shipped.

### 5.6 ★ New tooling primitive: `/d1reportSingleClassLayout`

`cl.exe /d1reportSingleClassLayout<ClassName>` **works through the wibo-wrapped
X360 compiler** and prints exact member offsets, base/vbase placement, padding,
vtables, and `this` adjustors. It cracked `BandCamShot` and `Synth` in minutes and
is strictly better than the stale `// 0xHEX` header comments and
`lookup_struct_offset` for layout work. No build integration needed — re-run the
TU's `ninja -t commands` line with the flag and a scratch `/Fo`. **Recommend wiring
it into the `struct-info` and `stack-layout` skills.**

### 5.7 Hand-offs to single-owner channels (diagnosed, NOT applied)

Map (`scripts/target_symbol_map.json`):
- `?HasCampaignKey@Campaign@@QBA_NVSymbol@@@Z` points at the address of
  **`Campaign::GetCampaignKey`** (target body is the hashtable `_M_find` +
  `return it->second / 0`, no bool normalisation). `GetCampaignKey` has no entry at
  all. Fixing the pair turns a false 48.9% into a real target.
- `?GetHarmony@Stats@@QBAHXZ` (`default/band3/game/Stats`, N=2, S=60) is a mispair:
  the target is `lfs f1, 0xb8(r3); blr` — a **float** getter at +0xb8 — but the
  mapped symbol mangles an `int` return. Correcting the source would change the
  mangled name and lose the pairing, so it needs a map re-anchor.
- Possible cross-class pairing to check: `?SetFrameEx@HamCamShot@@MAAXMM@Z` is
  listed inside the `default/BandCamShot` unit at 56%.

Splits:
- **`??8Head@BandCharDesc@@QBA_NABV01@@Z` is a target-carve truncation, not a
  source defect.** Target 332 bytes vs base 336; instructions 0–82 are
  byte-identical and the sole diff is a base-only trailing `blr`. The span is 4
  bytes short — re-deriving it should flip the function for free.

Layout owner (all diagnosed via `/d1reportSingleClassLayout`, none applied):
`?OnMsg@MetaPanel` (`sizeof(SongPreview)` 16 bytes too big — DC3-only
`mInitted`/`mPreviewDb`/`ObjPtr<TexMovie>` tail); `?AddActiveWidget@TrackDir`
(188 extra bytes before `mActiveWidgets`); `?Terminate@BandSongMgr` (24 bytes
missing before `this+0x160`); `?Save@ClipCollide` (`SAVE_REVS(1,0)`→`(3,0)` **and**
four members that are `ObjPtr` (0xc) in ours are 4-byte in retail); `??1Synth`
(`0x7c`→`0x5c` plus three −4s); `??1BandCharacter` (uniform −4/−8 in 0x7c0–0x820);
`?SyncProperty@FlowWhile` (`subi r3,r29,0x1c` vs `0x64` — base-sub-object adjust).
Stack-layout leads (route to `/stack-layout`, not constant hunting):
`?Custom@AccomplishmentProvider` (`r1+0x54` vs `0x5c`),
`?DoRegulate@CharServoBone` (`r1+0x90` vs `0x7c`), `?Count@HamLabel` (f30/f31).

### 5.8 Walls recorded (3-attempt cap reached, shape known)

- **`GetPlayerContributionString` @ DeployCountTracker + StreakTracker** (S=200,
  one shared cause, byte-identical 2 target-only instructions). Retail keeps a
  NULL-initialised `Stats*` local that ends up dead; **MSVC /O1
  dead-store-eliminates our reproduction.**
- **`VocalTrack::IncrementVolume`** — retail inlines `GetSynapseEnabled()` as
  `lbz; subic; subfe`, the 0/1 normalisation MSVC emits only for a **non-bool**
  byte. Making the member a `char` did not produce the idiom and cost a match.
- **`Player::LocalSetEnabledState`** — target `srawi. r11,r11,3` vs our
  `clrrwi. r11,r11,3`: retail computes the signed quotient, MSVC peepholes ours
  into a low-bit mask test.
- **`main`** — target's call to `App::Run` is encoded `bcl 20,lt` (B-form
  branch-always-with-link) where we emit `bl`; same target, identical semantics,
  all 18 other instructions match. **Only one such instruction in the entire
  binary** ⇒ toolchain/link artifact, not source-reachable.
- **`SyncProperty@BandWardrobe`** (the N=604 S=100 target) — *not* a
  `SYNC_SUPERCLASS`/macro-tail issue. One `mr r5,r11` insert + an r5↔r11 pair
  across 4 inlined `ForceBlink` arms. Pure arg-register selection ⇒ permuter-class,
  and the permuter is off by user directive.
- **`??1VorbisReader`** — 3-instruction scheduler rotation; the existing
  `auto &terminating` alias is load-bearing (removing it → 93.4%).
- **`??1StreamReceiver360`** — two base-only delete-expression **EH object-state
  spills** before each `Voice::~Voice()`. Source is byte-identical to DC3's.
- **`CheckRequirements@AccomplishmentSetlist`** (BOOL_MASK) and
  **`GetScaledFanValue@AccomplishmentManager`** (retail re-reads `mStart` inside
  the loop; we hoist it) — both fine-regalloc/permuter band.
- ★ **`jeff` mis-nested `.fn` blocks bite oracle reading.** A WIP note justified an
  arg-order swap by citing "target `fn_822C17D0` in `mtx.s` reads the Transform off
  r3", but that `.fn` block's bytes actually start at `0x822C16A4`. **Do not read
  argument registers off a `.fn` label without checking the address comment on the
  first instruction.**
