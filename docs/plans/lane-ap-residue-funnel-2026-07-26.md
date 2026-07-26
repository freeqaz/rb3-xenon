# laneAP — the "unreachable ~4,400" funnel: 40% of it was never a source problem (2026-07-26)

Baseline at lane start: **36,659** strict (`match_percent_normalized == 100.0`),
main HEAD `9b2d2737`. Baseline pickle `/home/free/tmp/laneAP/base2_strict.pkl`
(symmetric, build ×2).

Read first: `docs/plans/lane-am-diffunit-2026-07-26.md`,
`docs/plans/lane-an-pdata-parentage-2026-07-26.md`,
`docs/plans/lane-al-autocarve-2026-07-26.md`.

## The pool, restated honestly

laneAM measured **4,287 functions** in different-unit `.text` gaps that reached
100% under *neither* neighbouring unit in whole-binary probe builds, and closed
with: *"That residue is a source problem, not an attribution one."* laneAN
measured that `.pdata` parentage decides only 2.8% of it and agreed.

Reconstructed exactly from laneAM's own probe-leg pickles
(`laneAM_L_post_strict.pkl` / `laneAM_R_post_strict.pkl`), unit-agnostically by
name: **4,323 functions**. Of those, **27 are now strict-100 on current main**
(subsequent lanes' fills). **Live residue: 4,296.**

★ The pool is also *not* all unowned any more. Since laneAM measured it,
**1,264 of the 4,296 (29.4%) have been pinned into some unit** by laneAM's own
T1 fills and laneAN's micro-pin wave, and still do not match:

| current state | n | bytes |
|---|--:|--:|
| still unpinned | 3,032 | 427 KB |
| pinned, reads **0%** | 846 | 151 KB |
| pinned, reads **90–99.9%** | 385 | 16 KB |
| pinned, reads 50–89% | 4 | |
| pinned, no `report.json` entry | 29 | |

## ★★ The funnel, measured

### Cut 1 — what kind of code is it

| class | n | share | bytes | median size |
|---|--:|--:|--:|--:|
| **`NO_EH_PARENT`** (ordinary non-EH bodies) | **3,290** | 76.6% | 532 KB | 88 B |
| `FUNCLET_PARENT_IN_RESIDUE` (parent also unreachable — derivative) | 573 | 13.3% | 25 KB | 40 B |
| `FUNCLET_PARENT_PINNED` (parent pinned ⇒ **owner proven**) | 323 | 7.5% | 14 KB | 40 B |
| coverage-breadcrumb **stubs** | 110 | 2.6% | 7 KB | 32 B |

Two independent confirmations of the `NO_EH_PARENT` label: laneAN's `.pdata`
EH-parent map, and a bit-level funclet-prologue screen (`addi/subi rX, r12, imm`
as instruction 0) over the disassembly of all 7,265 gap functions — **0 / 3,290**
`NO_EH_PARENT` carry it, **890 / 895** of the `FUNCLET_*` classes do.

★**The stub class is 2.6%, not 13.7%** — this pool is markedly *under*-represented
in retail coverage breadcrumbs versus the binary-wide base rate. And the raw 110
is itself **23% noisy**: hand-classified from disassembly, only **85 are the
genuine 8-instruction breadcrumb shape**; 5 are real EH funclets and 18 are real
full-bodied functions (32–316 B, real FP math and `bl` calls) that
`/home/free/tmp/coverage_stub_syms.json` wrongly admits — its builder does not
apply the documented `is_coverage_stub()` rule that rejects any body containing
`bl`. **True stub share of this pool: 85/4,296 ≈ 2.0%.**

★**Correction to laneAN's `parent_map.json`:** it records a `kind:"unwind"`
"parent" for essentially *every* `.text` function with a `.pdata` record, not
only genuine funclets. "Has a `parent_map` entry" ≠ "is an EH funclet"; the
authoritative flag is `pdata_all.json`'s `is_prologue_screened_funclet`. All 85
true breadcrumbs have it `False`.

### ★★ Cut 2 — the one that reframes the pool: objdiff cannot score most of it

An anonymous `fn_<VA>` can only ever pair through `pair_funclets_by_bytes`, and
that gate admits only funclet-shaped symbols on the **base** side
(`__unwind$N` / `__catch$N` / `??__E*` / `??__F*`) — all small. Measured
empirically over **all 45,030 pinned anonymous `fn_` functions binary-wide**:

| size band | pinned anon | at 100% | rate |
|---|--:|--:|--:|
| ≤16 B | 2,992 | 3 | **0.1%** |
| 17–32 B | 10,854 | 8,128 | 74.9% |
| 33–44 B | 15,762 | 10,438 | 66.2% |
| 45–68 B | 3,295 | 1,367 | 41.5% |
| 69–84 B | 1,616 | 23 | **1.4%** |
| 85–128 B | 3,066 | 2 | **0.1%** |
| 129–512 B | 6,009 | 0 | **0.0%** |
| >512 B | 1,436 | 0 | **0.0%** |

**The scoreable window for an anonymous symbol is 17–68 B. Outside it the base
rate is 0.0–1.4%, over a 10,511-function sample that yields exactly 2 matches.**

Applying that to the residue:

| | n | share | bytes | share of bytes |
|---|--:|--:|--:|--:|
| **inside the window (17–68 B)** | **1,395** | 32.5% | 60 KB | 10.4% |
| **outside it (≤16 B or >68 B)** | **2,901** | **67.5%** | 518 KB | **89.6%** |

> ★ **Two thirds of the pool, and 90% of its bytes, are unreachable because
> objdiff has no candidate to pair them against — not because our source is
> wrong.** Only **94** of those 2,901 carry a name in
> `scripts/target_symbol_map.json`. For them the channel is not attribution and
> not body-porting; it is **identification** (a mangled name our build also
> defines), or an objdiff base-gate change (measured and declined by laneAN).

This is the correction to laneAM's "we compile no matching body at all". laneAM's
static predictor was funclet-shape-gated on the base side, so for every ordinary
body it had an **empty candidate set by construction** — absence of a hit there
proves nothing.

### ★★ Cut 3 — un-gated byte-twin test: we *do* compile 55% of it

Re-ran the signature test with the base-side funclet gate removed, over every
`cls` 2/3 Code symbol in all 1,024 compiled objs (455,994 symbols), with both
sides relocation-masked and canonicalised for COFF trailing-alignment padding.
Tool `/home/free/tmp/laneAP/w1/bytetwin2.py`.

Calibration: 60 currently-pinned, strict-100% functions pushed through the same
path — **59/60 byte-twin their own obj**. The single failure (`fn_825D4338`) is
conservative and understood: objdiff's *normalized* 100% is measurably looser
than exact masked-byte identity, so a MISS means "no byte-identical body", not
"no compiled body".

| class | HIT | MISS |
|---|--:|--:|
| `NO_EH_PARENT` (3,290) | **1,502** | 1,760 |
| `FUNCLET_PARENT_IN_RESIDUE` (573) | 502 | 71 |
| `FUNCLET_PARENT_PINNED` (323) | 251 | 72 |
| stubs (110) | 95 | 15 |
| **total (4,268 measured, 28 are pure alignment padding)** | **2,350** | 1,918 |

laneAM's gated predictor found **8** of the 3,290. Un-gated it is **1,502**.

A second worker reached **1,499 exact / 411 near / 1,380 absent** on the 3,290 by
an independent route (RTTI-decoded COMDAT index rather than signature hashing) —
1,499 vs 1,502 is agreement to 0.2%.

**Where the MISSes really are:** 473 of the 1,918 have a same-size near-twin
differing in ≤3 four-byte words (body divergence, not missing source). The
remaining **1,445 have no same-size candidate within 3 instructions** and are
heavily weighted large (694 at 129–512 B, 164 at >512 B, essentially all
`NO_EH_PARENT`). *That* is where "genuine source problem" honestly holds — and
it is **1,445 of 4,296 (34%)**, not the whole pool.

**The inverse hazard is real and was priced:** 1,101 of the 2,350 HITs have their
twin in >50 distinct objs (STL/stereotype shapes carrying no identity evidence).
Only **295 are unique-obj**, and 268 of those are also unique within the pool.

### Cut 4 — is the source available, and is it in scope?

Over 2,441 residue functions that any channel could attribute to a TU:

| | fns | note |
|---|--:|---|
| TU **already wired** in `objects.json` | **2,035 (83.4%)** | not a source-availability problem |
| TU only in `../rb3/src` (rb3-Wii) | 363 (14.9%) | 60 TUs — the real gap |
| TU only in `../dc3-decomp/src` | 4 | negligible |
| in-tree but unwired | 1 (`PlatformMgr_Xbox.cpp`) | |
| absent from all three trees | 38 (18 names) | `LockMessages.cpp`, `BandNetGameData.cpp`, … |
| wired-but-stubbed (<50% of oracle bytes) | 78 fns / 10 TUs | `Movie.cpp` r=0.15, `BandPatchMesh.cpp` r=0.11, `Loader.cpp` r=0.30, `BandCharacter.cpp` r=0.42 |
| unattributed | 849 | |

Scope: **in-scope 3,100 (94.2%)** of the 3,290; network-deprioritised 162,
Quazal 28, **XDK 0**. The hard-skipped vendor window contributes nothing here.

### Cut 5 — the ≤16 B class is a write-off, and 5% of it is not code

All 854 ≤16 B residue functions disassembled and shape-classified: **54.1%
virtual-base `this`-adjustor thunks**, 7.7% other tail-call thunks, 4.7% bare
`b <target>`, 4.6% non-virtual adjustors, 0.7% vtable-indirect glue — i.e. **~72%
are compiler-synthesized ABI glue, not portable source** — plus ~23% trivial
accessors. **41 (4.8%) are not functions at all**: 8 are pure `00000000`
alignment fill and 33 are truncated fragments of shared restore-helper code that
the gap carve wrapped a synthetic `fn_` symbol around. Those 41 should be
scrubbed from residue counts as tool artifacts. Base match rate for the band is
0.1% (3 of 2,992 binary-wide). **Write the class off.**

## ★ What I funded, and what it measured

**The identity-resolved un-gated byte twins.** Both workers independently ranked
this first, and where their lists overlapped (66 functions) they agreed on the
mangled name **66/66, zero disagreements**.

Audit before funding — rejected any candidate whose twin name was already mapped
at another VA (a duplicate mangled name inside one unit is a guaranteed
regression), whose `symbols.txt` size disagreed with the carve, or whose VA sits
inside a third unit's pinned span.

### Leg A — pure additions (no pin removed anywhere)

179 accepted: **179 map entries** (of which 5 *corrected* provably wrong existing
entries) + **173 `.text` micro-pin blocks across 91 units, 7,616 B added,
0 bytes removed**, `AUDIT CLEAN` via `micropin_apply.py`.

| | |
|---|--:|
| symmetric baseline (build ×2) | 36,659 |
| post (build ×2) | 36,810 |
| gained / lost | 156 / 5 |
| **net** | **+151** |
| targets reaching 100% | **154 of 179 (86%)** |

### ⚠ Leg B — moving mis-pinned byte twins: MEASURED FLOP, reverted

104 residue functions are byte-twins of a symbol in a unit **different from the
one they are currently pinned to**, and all of them currently score <100%, so
moving them looked like free upside (unlike laneAN's −34 repair, which moved
fills that *were* at 100%). Applied 77 audited `splits_move.py` moves + 99 map
entries, build ×2:

| | |
|---|--:|
| gained | 11 |
| lost | **10** |
| **net** | **+1** |
| targets reaching 100% | **10 of 99 (10%)** |

> ★ **Reverted.** 77 span moves of blast radius for +1 is not worth the splits
> fragmentation, and it independently reproduces laneAN's finding from a
> different direction: **moving code between units on byte-signature evidence is
> not free even when the moved functions contribute nothing.**
>
> ★ The 86% vs 10% hit-rate gap is the lesson: the leg-B population is dominated
> by 40–44 B EH funclets (the Waypoint↔VocalTrackDir cluster, 68 of 99), where
> masked equality zeroes every relocated instruction and therefore compares
> almost nothing. **Byte-twin evidence is strong for large, low-relocation-density
> bodies and near-worthless for small high-density ones** — exactly inverted from
> the intuition that small functions are easier.

The 10 moves that *did* convert were re-applied on their own (0 additional
losses).

### Landed total — measured twice, on two different baselines

Development leg, off the lane's own merge-base `9b2d2737`:

| | |
|---|--:|
| baseline (build ×2) | **36,659** |
| post (build ×2) | **36,820** |
| gained / lost | 166 / 5 |
| **net** | **+161** |

★**Composed re-verify.** Main advanced to `c25054e5` (37,058) mid-lane —
laneAQ's `>68 B` identity funnel works the *same* territory — so the union-rebase
was thrown away and the edit was **rebuilt from its JSON inputs on top of current
main** and re-measured from a fresh symmetric baseline. 4 map entries were
dropped as newly name-collided with laneAQ; 0 pins collided.

| | |
|---|--:|
| baseline at `c25054e5` (build ×2) | **37,058** |
| post (build ×2) | **37,215** |
| gained / lost | 162 / 5 |
| **net** | **+157** |
| gains that are laneAP targets | **160 of 162** |
| `overlap_check` | `.text` 0 overlaps, `.pdata` 0 overlaps |

The same 5 losses and the same 2 non-target gains appear in both legs.

> ⚠ **A trap for the next lane: do NOT let `land.sh` union-merge a splits diff
> that contains dtk's auto-derived `.pdata` back-fills.** Two lanes that each
> added `.text` pins get their independently-derived `.pdata` sub-ranges unioned,
> and the composed tree hard-fails the split (`Split 1:0x82208BA8..0x82208BB8
> overlaps with previous split` — 74 `.pdata` overlaps here). `READY:` from
> `land.sh` is **not** a verify. Rebuild the edit from its inputs on current main
> and let dtk re-derive `.pdata` once. `scripts/harvest/overlap_check.py` catches
> this with no build.

The 5 losses are anonymous funclets *inside units the diff touches*
(`Mesh` 2, `AppMiniLeaderboardDisplay` 2, `CharBoneDir` 1) — displaced funclet
pairings. One of the 2 non-target gains (`?Disable@CamShot@@QAAX_NH@Z`) is in a
unit the splits diff does not touch; its base obj was **not recompiled** in
either leg, so it is a target-side consequence of the global re-split
(`icf_aliases.map` and every target obj are regenerated on any splits edit), not
the stale-obj phantom.

## ★ What I refuted

1. **"The residue is a source problem."** (laneAM, endorsed by laneAN.) False for
   most of it. 67.5% of the pool cannot score for a reason that has nothing to do
   with source; and of the part that *can* be examined, we already compile a
   byte-identical body for 55%. The honest residual "we do not compile this"
   class is **1,445 functions (34%)**, not 4,300.
2. **"Only 8 of 3,290 have a twin in our objs."** An artifact of a base-side
   funclet-shape gate. Un-gated: 1,502.
3. **My own hypothesis that mis-pinned byte twins are recoverable by moving.**
   Measured at +1 net / 10 losses and reverted.
4. **`coverage_stub_syms.json` is 23% false-positive in this pool** (it does not
   apply the documented "no `bl`" rule).
5. **laneAN's `parent_map.json` over-reports funclets** — it has an entry for
   nearly every `.text` function; use `is_prologue_screened_funclet`.
6. **Caller/locality attribution is worthless here**: measured 0–38% accuracy
   against byte-identity ground truth (caller plurality: **0 of 7**). Only RTTI
   vtable membership is trustworthy (**77%, 27 of 35**). The `left_unit`/
   `right_unit` "locality prior" names the *caller's* TU, not the definer's.
7. **`unemitted_symbol_scan.py` does not address this pool.** Only 113 of 4,296
   (2.6%) have any map entry at all, and only 17 (0.4%) land in its flagged
   bucket. It cannot ask the question for 97.4% of the residue.

## ★ Also confirmed drained (free to re-check, costs no build)

`pdata_parent_owner.py actionable` on **current** splits returns **0 actionable
funclets across 0 units** — laneAN's micro-pin channel is at fixpoint. And
`pdata_parent_owner.py gaps` shows **601 gaps / 3,383 functions remaining
unowned tree-wide, every one `NO_EVIDENCE`** (2,176 with no EH parent, 1,207
with an unpinned parent). Re-run both after any splits wave; they are static.

## Residue for the next lane, named

1. **The 268-entry identity-resolved list minus what I landed.** 104 were
   rejected as already-pinned-elsewhere (leg B — do **not** re-fund the move) and
   28 as name-already-mapped-at-another-VA. That last class is a real defect
   channel: some of those existing entries are wrong, exactly like the 5 I
   corrected. Full list: `/home/free/tmp/laneAP/w1/bytetwin.json` (`actionable`).
2. **The 43 fan-out HITs** — one distinct base symbol name but present in >1 obj
   (template/ICF fan-out): identity certain, owner ambiguous. 25 are ≥68 B.
3. **The 1,101 ubiquitous HITs are NOT a work queue.** Twin in >50 objs = no
   identity evidence. Do not fund them.
4. **321 `FUNCLET_PARENT_PINNED` funclets with a *proven* owner that our obj
   still fails to emit.** For 549 of the 896 residue funclets the target's `bl`
   resolves to a named destructor, so each one states a concrete source defect —
   *"unit X's parent function is missing an EH cleanup destroying `T` at member
   offset `0xNNN`"*. Top callees: `??3BinStream@@SAXPAX@Z` 108,
   `??1ObjRef@@QAA@XZ` 95, `??1String@@UAA@XZ` 54, `??1Object@Hmx@@UAA@XZ` 46.
   Extracted to `/home/free/tmp/laneAP/funclet_diag.json`. This is a
   struct-layout / member-set worklist for a body-port lane, not attribution work.
5. **The 99.8/99.9% band is a fuzzy-pairing artifact, not a near-miss queue.**
   2,462 functions binary-wide sit at 99.0–100.0%, 1,643 of them exactly 40 B at
   99.9%. Diagnosed one (`fn_822D9680`, Waypoint): target destroys `RndDir` at
   `+0x1e0`, base destroys `ObjPtr<RndMat>` at `+0xc` — **objdiff pass 3 fuzzy-paired
   two unrelated funclets of the same shape**. Reaching 100% needs our obj to
   emit the *right* funclet (item 4), not a one-instruction fix. **Do not price
   this band as a cheap +1,600.**
6. **The one coherent port opportunity, and it is small.** 80 source-gap TUs
   total **406 functions / 57 KB = 11% of the residue bytes**; the largest single
   one is `StatMemberTracker.cpp` at 36 functions. The only coherent bloc is ten
   rb3-Wii-only display/panel classes — `StatMemberTracker, OvershellDir,
   PlayerDiffIcon, ClosetPanel, MicInputArrow, ScrollbarDisplay,
   ContentDeletePanel, ScoreDisplay, InstrumentDifficultyDisplay,
   ChooseColorPanel` — 165 `NO_EH_PARENT` fns but **262 residue functions of all
   classes inside their spans**, all RTTI-high-confidence, tight contiguous spans
   (0.4–5.8 KB), five already have headers in-tree, full rb3-Wii `.cpp` bodies
   exist. Spans in `/home/free/tmp/laneAP/w2/identity.json:source_gap_tus`.
   **~260 functions is the whole realistic port opportunity in this residue.**
7. **336 of 743 gaps (2,095 fns, 310 KB) have a dominant owner that is neither
   neighbour.** Probing left/right was structurally guaranteed to fail for them.
   Examples: gap `0x8236B000` (L=StarsDisplay, R=CharBonesMeshes) is
   RhythmDetector factory code; `0x823215E8` (L=CameraTilt, R=SpeechMgr) is
   `ScrollbarDisplay`; `0x82323490` (L=SpeechMgr, R=LabelShrinkWrapper) is
   `InstrumentDifficultyDisplay`.

## Reproducing

```bash
# funnel (static, no build)
python3 scripts/harvest/pdata_parent_owner.py actionable --json out.json   # 0 = drained
python3 scripts/harvest/pdata_parent_owner.py gaps --json gaps.json        # 601 / 3,383
python3 /home/free/tmp/laneAP/w1/bytetwin2.py    # un-gated byte-twin, ~15 s
```

Artifacts: `/home/free/tmp/laneAP/` — `residue_c2.json` (the 4,296 classified),
`w1/bytetwin.json` (4.6 MB: summary + all rows + the 268 actionable),
`w2/identity.json` (2.1 MB: RTTI attribution, TU ranking, source-gap TUs),
`w3/` (stub shape classification, ≤16 B census, unemitted overlap),
`funclet_diag.json`, `accepted2.json`, `pins_legA.json`, `moves_legB.json`
(the reverted leg), `mispinned.json`.
