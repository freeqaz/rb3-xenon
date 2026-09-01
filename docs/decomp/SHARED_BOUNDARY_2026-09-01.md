# Shared-boundary recon — lane SHAREDBOUND, 2026-09-01

Branch `shared-bound`, off `26bfd724`. Worktree `~/tmp/wt-sharedbound`.

**Verdict: the shared-boundary vein is REFUTED. All 29 `pin_audit` candidates
are genuine retail layout, and not one of them should have its pin moved.**
The lane's yield came from a *different* operation class discovered while
refuting it.

The refutation is structural (one measured quantity, computed over all 29),
empirically confirmed by an A/B on the strongest candidate, and one-directional
— the host is always the equal-or-better home, never the worse one.

---

## 0. What refutes the brief

Recorded first, per the standing rule.

**(a) The lane's headline size was ~30× too big, because it summed the wrong
population.** The brief sized the work as *"1,999 functions, 339 unmatched, 52
XDK ⇒ **287 in-scope unmatched functions**"*, taken from each candidate's
per-**unit** `mf/tf`. But a candidate's prize is its **cluster**, not its unit's
whole unmatched tail. Measured cluster sizes are **4–36 named entries**; the
in-scope own-attributed population across all 29 is **~200 rows**, of which
**~150 are already at `fuzzy == 100`**. `mf/tf` describes the unit a candidate
was *derived from*, and carries no information about the candidate.

**(b) `pin_audit`'s own action string already predicted the refutation, and it
was right.** Every INTERLEAVE row ships the text *"interleave risk (Object/
DirLoader precedent fe603cc: expect COMDAT interleave refutation unless per-fn
`.s` proves contiguity)"*. The detector is not making a claim it failed to
check — it is explicitly flagging a symptom and naming the prior refutation.
It fires on pin-vs-map **geometry** and is structurally unable to see the two
mechanisms in §2, both of which are already in the tree.

**(c) The 29 count reproduces exactly** (29 candidates / 47 filtered / 49
deferred), so nothing here is a stale-worklist artifact.

---

## 1. The structural refutation: GAINCELL = 0 across all 29

Re-homing an already-pinned address pays through exactly one channel:
**pairability transfer**. objdiff pairs target↔base **by name, per unit**, so
moving an address to another unit pays only if the *receiving* unit's base obj
defines a name the *host's* base obj does **not**. Anything else is
reattribution.

So the decisive quantity per candidate is the count of cluster symbols that
**our obj defines and the host's obj does not**. Call it GAINCELL.

```
GAINCELL, all 29 candidates ............................ 0
```

Computed with `pin_audit`'s own ownership logic (`head_classes` over the
compiled obj's defined symbols), **not** by guessing that the unit name is the
class name — that naive filter reported `own = 0` for `Rnd`, `SoundTouch`,
`Part`, `DirUnloader`, `TexRenderer`, `User` and `UIListProvider`, i.e. it
would have hidden a prize had one existed. It was corrected before the verdict.

The asymmetry is not merely absent, it runs **backwards**, often severely:

| candidate | cluster own | host obj defines | our obj defines | re-home would |
|---|---:|---:|---:|---|
| `Rnd` ← `Rnd_Xbox.cpp` | 44 | **43** | **0** | destroy 43 pairings |
| `Part` ← `rnddx9/CubeTex.cpp` | 11 | 11 | 0 | destroy 11 |
| `UIListProvider` ← `UIList.cpp` | 7 | 7 | 0 | destroy 7 |
| `GemTrainerPanel` ← `RGTrainerPanel.cpp` | 9 | 9 | 0 | destroy 9 |
| `SetlistToStorePanel` ← `MetaPanel.cpp` | 8 | 8 | 0 | destroy 8 |
| `NextSongPanel` ← `MetaPanel.cpp` | 6 | 6 | 0 | destroy 6 |
| `SoundTouch` ← `TDStretch.cpp` | 13 | 13 | 0 | destroy 13 |
| `DataArray` ← `DataNode.cpp` | 8 | 8 | 8 | exactly neutral (§3) |
| `ConnectionStatusPanel` ← `MetaMusicScene.cpp` | 9 | 9 | 9 | neutral |

Corroborating: the cluster rows are **already matched under the host** —
`SoundTouch/TDStretch` 16/16 at `fuzzy 100`, `ConnectionStatusPanel` 9/9,
`UIListProvider` 7/7, `DirUnloader/PatchDir` 7/7, `Rnd/Rnd_Xbox` 53/69,
`SetlistToStorePanel` 91/95. There is no unpaired prize to collect.

⚠ `Scheduler` ×2 is XDK (`xgraphics/.../sched.cpp`, `scheduler.cpp`), out of
scope by standing directive, and is the only pair whose host obj does not exist
(so it has no gain channel either).

---

## 2. Why the host obj defines the symbols — two mechanisms, both already in tree

Every cluster symbol is a **COMDAT-class** symbol. Sampled across the set they
are, without exception, `$4PPPPPPPM@A@` MI adjustor thunks (12 B), `??_E` /
`??_G` / `??_D` implicit deleting destructors, `??0`/`??1` ctors/dtors, and
`?ClassName@`/`?StaticClassName@` — i.e. Milo's `OBJ_CLASSNAME` inlines. These
are emitted into **every TU that uses the class**; the linker keeps one copy,
and retail's copy lives in whichever TU's section contribution won. The address
therefore genuinely belongs to the host TU. **The pin is correct.**

On top of that, the tree already carries a deliberate **source-side
remediation** of this exact situation:

```
src/system/obj/DataNode.cpp:929:  #include "obj/DataArray.cpp"
```

**254 `#include "*.cpp"` sites exist tree-wide.** 8 of the 29 candidates have
the host TU explicitly including the owning unit's `.cpp` — `DataNode←DataArray`,
`UISlider←PanelDir`, `Spotlight←ColorPalette`, `MetaMusicScene←ConnectionStatusPanel`,
`UIListState←UIButton`, `PracticePanel←ChordbookPanel`, `CharIKFingers←SongSectionController`,
`FxSendMeterEffect←FxSendWah`, `SongSortByReview←SongSortByPlays`.

⇒ **The house fix for a shared boundary is the source-side one — make the host
TU compile the neighbour's code so the host's base obj can pair the target rows
— not the pin-side one.** It is already applied where it was needed. The
detector cannot see it, so these candidates are *solved work reported as open*.

---

## 3. Empirical confirmation: the strongest candidate measures exactly 0

Inference was not trusted on its own. The best structural case in the set was
A/B'd end-to-end.

`DataArray ← DataNode` is the strongest because it is a genuine adjacent-boundary
case, not a scatter: `DataNode.cpp`'s last-but-two `.text` block
`0x8274BBE4..0x8274C4E0` (0x8FC B) sits **immediately** before `DataArray.cpp`'s
pin at `0x8274C4E0`, and the **block-purity screen passes 8/8** — every named
entry in it is a `DataArray` method, zero foreign. Both objs define all 8.

Operation tested: move that whole block `DataNode → DataArray` (a whole existing
block boundary, so no carving, and `DataNode` keeps 6 other blocks so the
last-block-drain rule does not apply). `.text` only — `.pdata` was left alone.

**Predicted Δmatched = 0, Δcode_bytes = 0. Measured exactly that:**

```
Δmatched=+0  Δmasked_equal=+0  Δhonest=+0  Δcode%=+0.000000pp  Δcode_bytes=+0
unit improvements:  +12  default/DataArray  (41->53)
unit REGRESSIONS:   -12  default/DataNode   (67->55)
[control none] Δmatched_code=+0 B
```

The unit view *is* the mechanism: 12 rows moved between units, whole-binary net
exactly zero. The `none` ruler at +0 confirms no collateral code movement, which
the purity screen predicted.

**Not landed.** It is Δ0 churn asserting an ownership claim I cannot prove:
our `DataNode.cpp` really does `#include DataArray.cpp`, so "the tail is
DataArray's TU contribution" and "it is DataNode's" are both live hypotheses,
and the metric cannot separate them.

⚠ Procedural note for the next lane: the first A/B attempt was **REFUSED**
because the `.text` move changed which TU owns the derived `.pdata` entries, so
dtk rewrote `splits.txt` mid-split (the split modified its own input). The
recovery is exactly what the refusal text says — re-apply, run **one** build to
let dtk write the corrected `.pdata`, then measure the fixed point. dtk moved
`.pdata 0x8223B868..0x8223B8E0` `DataNode → DataArray` by itself, confirming
`.pdata` is derived output.

---

## 4. Operations enumerated and tested

A drained-vein verdict is only as broad as its operation set, so the set is
stated explicitly.

| # | operation | status | evidence |
|---|---|---|---|
| 1 | Re-home a cluster range host → owning unit | **REFUTED** | GAINCELL = 0 on all 29 (§1); A/B on best case = exactly Δ0 (§3) |
| 2 | Split a host pin to carve out the cluster | **REFUTED** | same gain channel as #1; strictly worse (carving risk, no extra upside) |
| 3 | Add a pin over unpinned code | **NOT APPLICABLE** | INTERLEAVE means the range is already inside a host pin; zero AUTO_BLOB rows among the 29 |
| 4 | Source-side: `#include` the neighbour's `.cpp` into the host TU | **ALREADY DONE** | 254 sites tree-wide; 8 of 29 candidates (§2) |
| 5 | Repair a *wrong* existing map name in a cluster | **NOT FOUND** | every cluster name adjudicated consistent; the host pairs them at 99.7–100 |
| 6 | Name a map-absent primary behind an *already-named* thunk | ✅ **PAYS** | §5 — the lane's entire yield |
| 7 | Body-port the genuinely divergent rows | **OUT OF LANE** | flagged in §6 for a bodyport lane |

Explicitly **not** done: XDK (`Scheduler` ×2) per standing directive; permuter;
any body-port.

---

## 5. The vein that did pay — a NAMED thunk whose TARGET is anonymous

This is the inverse of what THUNK3 drained (`c897bdd7` closed *unnamed thunk*;
this is *named thunk, unnamed target*), and it was found only because §1 forced
a look at what the cluster symbols actually are.

Scanning every tail-branch thunk body in `build/45410914/asm/*.s` — keyed on the
`.fn fn_<addr>` symbol, **never** the synthetic address column:

```
thunk-shaped functions (<=5 insns, tail `b fn_`) .... 3,186
map-NAMED thunk -> map-ABSENT target ................   297   (224 distinct targets)
  of which unique-candidate from the pin owner's obj    17    (5,504 B, all at fuzzy 0)
  ambiguous / no candidate / unpinned .................. 207
```

⚠ **The 207 are mostly not a backlog.** A target pointed at by thunks of several
different classes is the ICF-survivor case CLAUDE.md calls irreducible — the
folded name the call site meant was destroyed by the fold. Do not mine these by
heuristic.

### 5a. `0x82570650` = `?ClassName@AppLabel@@UBA?AVSymbol@@XZ` — landed, +1 fn / +48 B

The optional pickup, re-adjudicated on retail bytes from **both ends** rather
than inherited:

- `fn_825720B8` — which the map **already names**
  `?ClassName@AppLabel@@$4PPPPPPPM@A@BA?AVSymbol@@XZ` — is
  `lwz r11,-0x4(r4); subf r4,r11,r4; b fn_82570650`. It branches **directly to
  `0x82570650`**, so the map already asserts what that address is.
- `fn_82570650` is `mr r31,r3; bl fn_82570428; mr r3,r31; blr`, and `0x82570428`
  is map-named `?StaticClassName@AppLabel@@`. That is exactly
  `Symbol AppLabel::ClassName() const { return StaticClassName(); }` with the
  hidden `Symbol` return buffer in `r3`.
- Shape corroboration: `ColorPalette::ClassName` is also exactly 48 B.
- `MetaPanel.obj` (base obj for the owning pin block `0x825704A8..0x82570C80`)
  defines `?ClassName@AppLabel@@UBA?AVSymbol@@XZ`.

**Predicted +1 fn / +48 B; measured +1 fn / +48 B, 0 regressions.** The 12 B
thunk was at `fuzzy 100` *before*, with its branch target a forgiven
placeholder; naming converted that site into a `name_check`-charged one and it
**stayed at 100**, which independently confirms our thunk branches to the name
installed. `none` control read `REAL_PAIRING +48 B`.

### 5b. Six more primaries — Δ0 bytes, +0.0148pp fuzzy, and one PREDICTION MISS

Took the 7 whose proposed class equals the pin-owning unit.

**I pre-registered "non-negative with zero regressions" and MISSED:**

```
7 entries: Δmatched=-1  Δcode=-12 B  Δfuzzy=+0.014889pp  1 regression (CharDriver 137->136)
6 entries: Δmatched=+0  Δcode=+0 B   Δfuzzy=+0.014839pp  0 regressions
```

The miss is CLAUDE.md's naming economics firing exactly: naming an anonymous
address converts a **forgiven** placeholder call site into a **checked** one, so
a wrong name buys a new charge. The lost row is 12 B — a thunk — so our source's
thunk branches to something other than `?Replace@CharDriver@@`, meaning either
that identification is wrong or `0x82376E40` is an ICF survivor. **`0x82376E40`
is left UNNAMED and is an open question, not a closed one.** Dropping that one
entry removed the whole penalty and kept essentially all the pairing gain, which
isolates it cleanly.

Landed (6): `0x824B7738 Copy@LightPreset`, `0x824A0870 Replace@EventTrigger`,
`0x8239E318 Load@CharCuff`, `0x823A3698 Load@CharFaceServo`,
`0x823BFB88 Load@CharIKFoot`, `0x823D6270 Load@CharGuitarString`.

**The headline is deliberately `Δcode = +0`.** This pays in identification, not
bytes (cf. MAPID-1) — 6 bodies that scored 0 because nothing could pair them now
pair and score partially, which is exactly why `fuzzy` moves and `matched_code`
does not. It hands 6 real divergences to a body-port lane; it manufactures no
credit.

---

## 6. Handoffs

- **`0x82376E40`** — a named `?Replace@CharDriver@@` thunk branches to it, but
  naming it costs a 12 B thunk row. Wrong identification, or ICF survivor.
  Worth one lane-hour with the both-ends method of §5a.
- **The 207 ambiguous thunk targets** — sized, characterized, and *not*
  recommended for heuristic mining.
- **`?Load@ColorPalette@@UAAXAAVBinStream@@@Z`** (`0x824DFEB0`, 244 B) sits at
  **fuzzy 61.8** inside `Spotlight`'s pin. That is a genuine source divergence,
  the only large one surfaced by this lane, and it is a **body-port** target —
  no pin change can help it.
- **`pin_audit` enhancement worth making:** the INTERLEAVE branch could compute
  GAINCELL itself (host-obj-defines vs our-obj-defines over the cluster) and
  auto-file the row as `F8_host_already_pairs`. That would have retired all 29
  of these without a lane. The data it needs is already loaded — `ObjStats.defined`
  for both units.

## 7. Reproduction

```bash
venv/bin/python tools/pin_audit.py --json ~/tmp/sb_worklist.json --quiet
# 29 candidates / 47 filtered / 49 deferred, unchanged by this lane (no pin was moved)
```

`pin_audit` after: **29 → 29**, by design. `baseline_matched` 42292 → 42293
(the AppLabel row); map entries 28947 → 28954 (+7 = 1 + 6).
