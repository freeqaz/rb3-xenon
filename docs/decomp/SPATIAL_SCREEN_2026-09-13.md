# The spatial map-defect screen — lane W9-A, 2026-09-13

Branch `w9-spatial-screen`, worktree `~/tmp/wt-w9-a`, base main `1d560c2b`
(asserted ancestor before editing). Ruler: **`name_check` (graded)**, read from
`report.json`'s `provenance.diff_config`, not assumed. Tool:
**`tools/spatial_map_screen.py`**.

Baseline at this lane's first full build (full build first — a reflinked tree's
target objs are pre-renamer; the build's own `CHECK TARGET OBJS RENAMED` step
reported **25,523 / 29,034 map names present (87.9%)**, so no mangled-name
lookup here was vacuous):

```
matched_functions   42,739
matched_code        3,865,716 B
matched_code_percent  37.729187
fuzzy_match_percent   49.127808
total_code          10,245,956
```

---

## 0. Headline

W8-C closed with a handoff: *"the cheap screen is spatial — a row whose address
falls outside its class's `.text` cluster while sitting inside another class's
unbroken run."* Built, controlled, and run over the whole map.

- The screen **rediscovers W8-C's own case** (`0x824302B0`) when W8-C's fix is
  reverted, and is **silent on it** otherwise — the same code, two map states,
  opposite verdicts.
- It reduces **17,482 participating map rows to 39 fires** (0.22%), of which the
  ≥44 B stratum is 21.
- **It found the same defect class twice more, independently**, including one
  whose `splits.txt` hole is byte-exact — the circular name↔pin structure W8-C
  described is **not a one-off**.
- Measured FP rate: **8 of 12 adjudicated = 66.7%** (precision 33.3%);
  conservative bound counting all 9 unresolved as FP, **17/21 = 81.0%**.
- Two fixes landed: **+16 functions / +6,400 B** and a deliberate **Δ0**.

Final: `matched_functions` **42,755**, `matched_code` **3,872,116 B**,
`matched_code_percent` **37.791653**, `fuzzy` **49.133522**.

---

## 1. Definition

**Premise** (CLAUDE.md, verified): retail is `/O1` with **no LTCG**, so the
linker preserves TU spatial grouping in `.text`. A class's out-of-line member
functions form a contiguous cluster.

A map row fires when **all** of the following hold:

1. Its owner class `X` differs from the owner `Y` of both its immediate
   neighbours in address order, and `Y`'s run is unbroken for at least
   `flank_min` rows **on both sides** (default 3).
2. `X` and `Y` do not share an outermost scope (nested class, or one namespace).
3. `X` has a **home cluster** of at least `min_home` rows (default 3) — the most
   populous segment of `X`'s own addresses, cut at gaps > `cluster_gap`
   (0x8000).
4. The row lies **outside** that home cluster, at least `iso_min` (0x4000) away.

Clusters are derived **from the map itself**. `splits.txt` is never read.

### 1.1 Why this is not `map_lint.py --check class_mixing`

`check_class_mixing` is **pin-relative**: it walks `splits.txt` units and asks
which in-range map names have a foreign owner. W8-C's defect was
*self-consistent* — the pin had been moved to agree with the wrong name — so the
owner matched the unit's own class family and **the check cannot fire, by
construction**. That is precisely the vein a map-relative frame opens. The two
checks are complementary; neither subsumes the other.

### 1.2 Population and what is deliberately transparent

29,131 map rows → **17,482 participating**, 11,650 suppressed. A suppressed row
is *transparent*: neither a candidate nor a run-breaker, so an interleaved COMDAT
does not shred an otherwise-unbroken run.

| suppressed as | rows | why its placement carries no TU information |
|---|---:|---|
| `template-comdat` (`?$`) | 4,585 | emitted into any using TU, folded arbitrarily |
| `adjustor-thunk` (`@@$4…`) | 1,411 | **emitted in the DERIVED class's TU by design** |
| `compiler-generated-comdat` (`??_B/_D/_E/_G`) | 1,284 | guard / vbase / deleting dtors |
| `free-function` (`@@YA…`) | 1,239 | has no class scope at all |
| `not-cxx` | 1,062 | plain C / data labels |
| `arbitrary-name` | 965 | the map's OWN `_bijection_arbitrary` / `_icf_arbitrary` / `_denylist`, whose comments say identity here is UNRESOLVED |
| `data-or-vcall` (`??_C/_7/_8/_R/_9`) | 463 | `.rdata`, not `.text` |
| `template-scope`, `dynamic-init`, `stl-namespace`, other | 433 | — |

Two filters were added only after seeing them dominate the raw output, and both
are structural rather than tuned: adjustor thunks and free functions. Before
them the screen produced 120 fires, 29 of which were those two classes alone.

### 1.3 Owner extraction is local, and NOT `map_lint.mangled_classes`

`map_lint`'s helper is right for its own purpose and wrong for this one, twice:

- it falls back to a **signature argument type** when a symbol has no class
  scope, so the free function `?ReplaceSubdir@@YAXPAVObjectDir@@0@Z` is
  attributed to `ObjectDir` — whose TU says nothing about where a free function
  was emitted;
- it **mis-parses `??_E`/`??_G`**: `??_GRndPostProc@@` yields owner
  `GRndPostProc`, the special-name prefix letter glued onto the class. Those
  phantom owners shred otherwise-unbroken runs.

Both are real defects in a shared helper (see Handoffs); this lane did not patch
it, to keep the screen's behaviour self-contained and auditable.

---

## 2. The controls — they run on EVERY scan, and they can fail

A scan whose negative would close a vein must assert a known positive **in the
same run**. W7-B's first detector could not rediscover its own case; W8-B's
oracle scan returned a clean decisive "0 sites" out of a tab-vs-space bug. So
`--selftest` is not the gate; every invocation is.

| control | what it requires |
|---|---|
| **positive** | reverts W8-C's two map rows in memory and REQUIRES `0x824302B0` to fire |
| **negative** | REQUIRES `0x824302B0` silent on the unmutated map |
| **vacuity** | floors on participating population (12,000) and total suppressed (5,000), and REQUIRES each of four named structural filters to have suppressed something |

Any failure exits **3** and prints **no findings** — a broken screen must not be
able to report a reassuring empty result.

The fixture is the *real* historical state, not a guess: `git show
2fc2552a:scripts/target_symbol_map.json` differs from the current map in exactly
**three** rows, two of them W8-C's (`0x824302b0` was `?Save@RndEnviron@@`,
`0x82407e58` was absent).

**Proof the controls can fail** — `--break-control` sabotages one and each is
measured to exit 3, against a clean run at 0:

```
break=positive rc=3    break=negative rc=3    break=vacuity rc=3    clean rc=0
```

Known-positive detail as reported by a live run:

```
CONTROL positive PASS 0x824302B0 flagged with W8-C reverted
  (owner=RndEnviron host=RndPostProc flank=5 dist=0x1ea60 host_lacks_Save=True)
```

---

## 3. The measured false-positive rate

**A detector without a measured FP rate is a heuristic that confirms whatever
you point it at.** So:

**Sample stratum, fixed BEFORE adjudicating: every fire of size ≥ 44 B — 21 of
the 39.** The reason is an instrument limitation, not a convenience: below ~40 B
a retail body is 2–6 generic instructions (`li r3, 0x2d; blr`,
`lwz r3, 0xc(r3); blr`, `addi r3, r3, 0x40; b …`) and **cannot discriminate
between candidate names** — those are the canonical ICF fold shapes. Including
them would measure the instrument's blind spot, not the screen.

Verdicts are assigned on **retail bytes** (the `.s` body at the address: its
callees resolved through the map, its `this`-relative offsets, its vtable
writes), checked against compiler-authoritative class hierarchies from our
headers. **A "fold" verdict counts as FP** — conservative, because the screen's
claim is "candidate map defect" and a fold does not vindicate it.

| verdict | n | of stratum |
|---|---:|---:|
| **TRUE POSITIVE** — retail body is demonstrably not the named function | **4** | 19.0% |
| **FALSE POSITIVE** — retail body demonstrably IS the named function | **8** | 38.1% |
| **UNRESOLVED** — no decisive evidence either way | **9** | 42.9% |

- **FP rate over the adjudicated subset: 8 / 12 = 66.7%** (precision 4/12 =
  **33.3%**).
- **Conservative bound, counting every unresolved fire as FP: 17 / 21 = 81.0%**
  (precision **19.0%**).

Both are quoted because the first alone would be optimistic — it is easy to
adjudicate the obvious true positives and leave the hard ones out of the
denominator. Neither figure is fitted: no threshold was moved after adjudication
began.

★ **Read the precision against the selectivity, not against 100%.** The screen
selects **21 rows out of 17,482** (0.12%) and at least 4 of them are real,
fixable identity defects — one of which paid +6,400 B. A 19–33% precision at
that selectivity is an enormous enrichment; it is not a "mostly wrong" detector.

### 3.1 What the false positives actually are — one mechanism, not noise

Every decisive FP is the same thing: **a class whose methods are split across
more than one TU**, so it legitimately has a member inside another class's run.

| addr | row | why it is legitimate |
|---|---|---|
| `0x82b5d278` | `?New@Synth@@` | calls `??0Synth360@@` — `Synth::New` is the factory returning the platform subclass; DC3 map: **same obj** (`synth_xbox:Synth.obj`) |
| `0x82469750` | `??1ReclaimableAlloc@@` | DC3: same obj (`rndobj:MultiMesh.obj`) |
| `0x82815870` | `??1UIListWidget@@` | DC3: same obj (`ui:UIListSlot.obj`) |
| `0x82688298` | `??1MultiplayerAnalyzer@@` | destroys `vector<Data@MultiplayerAnalyzer>` — a **nested** type; decisive |
| `0x8232bfb0` | `??0Message@@` | `PoolAlloc` + `DataArray` ctor + `DataNode::operator=` is a Message ctor |
| `0x826b94b0` | `??1VocalNoteList@@` | destroys `vector<VocalNote>` |
| `0x825e0328` | `??0PassiveMessageQueue@@` | calls `??0Timer@@`; ctor shape (probable, not decisive) |
| `0x826ac0e8` | `?Poll@GemTrainerLoopPanel@@` | calls `?Poll@UIPanel@@`, its own base (probable) |

The `min_home ≥ 3` rule already removed the *larger* benign class — a
**companion class with no TU of its own** (`TourDescEntry` in `TourDesc.cpp`,
`ChunkStream::ChunkInfo`, `CharClip::Transitions`), whose map name is entirely
correct. That single rule took the fire count from 91 to 39.

### 3.2 The 18 fires below 44 B are NOT counted, and NOT cleared

They are reported by the tool and excluded from the rate. All are 2–6
instruction accessors or tail-call thunks. Their names may well be arbitrary —
several are exactly the shapes ICF folds — but **retail bytes cannot say**, and
recording an unfalsifiable verdict as either TP or FP would be worse than
recording none.

---

## 4. Two independent corroborations of the screen

Neither shares arithmetic with the spatial rule.

### 4.1 DC3's leaked linker map gives per-symbol OBJECT FILE membership

`tools/dc3_map.py` resolves a mangled name to the `.obj` it was linked from —
direct TU ground truth for engine code. **Validated on the known positive
first:** `?Save@RndPostProc@@` → `rndobj:PostProc.obj`, `?Save@RndEnviron@@` →
`rndobj:Env.obj` — two distinct TUs, so the oracle discriminates on the case the
screen was built from.

Over the 39 fires: **coverage 16/39** (RB3-only game classes are absent from
DC3), of which **12 DIFF-TU (corroborated)** and **4 SAME-TU (refuted)**. All
four refutations are in the FP table above.

⚠ DC3 is *newer* than RB3 and a method can move TUs between them, so this is
corroboration, never adjudication. Retail bytes outrank it, as in W8-C §3.

### 4.2 Fires are 21.2× enriched on `splits.txt` orphan blocks

Independent because the screen **never reads `splits.txt`**. Defining an *orphan
block* as a unit's `.text` block more than 0x40000 from that unit's
largest-by-bytes cluster (1,017 such blocks over 315 units):

| population | in an orphan block | rate |
|---|---:|---:|
| **fires** | 14 / 39 | **35.9%** |
| **null — all participating map rows** | 296 / 17,482 | **1.69%** |

**Enrichment 21.2×.** The null is what makes this meaningful: a raw "fires sit on
odd pins" observation would have confirmed itself.

⚠ This is correlation with a *related* defect signal, not a second FP rate — an
orphan block can itself be legitimate.

---

## 5. The fixes — predicted vs measured

Every A/B: `python3 tools/ab_measure.py --worktree ~/tmp/wt-w9-a --from-dirty`,
one change per run, ruler `name_check`, per-unit effects checked, committed
immediately.

| # | change | predicted | measured | per-unit |
|---|---|---|---|---|
| 1 | `0x827FFAD8` → `??0UIComponent@@` + name `0x823F9B50` + close the splits hole (**coupled**) | Δfns **[0,+2]**, Δbytes **[0,+1200]** | Δfns **+16**, Δbytes **+6,400**, Δcode% **+0.062466pp**, Δfuzzy **+0.005892pp** | unit net (ALL units) **+16** == whole-binary Δmatched; units@100 160→162, **0 fell off** |
| 2 | `0x826A75C8` → `??0Player@@` (map-only) | Δfns [0,+4], Δbytes [0,+1500], Δfuzzy **> 0** | Δfns **+0**, Δbytes **+0**, Δfuzzy **−0.000178pp** | 0 reached 100, **0 fell off**, both rulers |

### 5.1 Fix 1 — W8-C's defect, byte-for-byte the same shape

`0x827FFAD8` (496 B) was `??0RndTransformable@@IAA@XZ`, sitting in an unbroken
run of `UIComponent` methods 0x405008 from any other `RndTransformable` row.
Four independent adjudications:

1. the body calls `??0RndDrawable@@IAA@XZ`. `class RndTransformable : public
   virtual RndHighlightable` does **not** derive from `RndDrawable`;
   `class UIComponent : public RndDrawable, public RndTransformable, public
   RndPollable` does. Decisive alone.
2. it writes **five** vtable pointers (0x4/0x28/0xdc/0x170/0x174) — multiple
   inheritance, which `RndTransformable` does not have.
3. `??0UIComponent@@` was **absent from the map** while `??1UIComponent@@` sat
   at `0x828000E8` — the ctor was the one member missing from the run, exactly
   as `Save` was missing from `RndPostProc`'s.
4. the body does `addi r3, r30, 0x24` then `bl fn_823F9B50` — it builds its
   `RndTransformable` sub-object at offset 0x24, so **`fn_823F9B50` is the real
   `RndTransformable` ctor**. It was unnamed, inside Trans's own cluster, and it
   calls only `??0Object@Hmx@@` and **not** `??0RndDrawable@@`.

**And the pin was circular, exactly as in W8-C.** `UIComponent.cpp` carried

```
.text  start:0x827FE2D0 end:0x827FFAD8      <- stops AT the ctor
.text  start:0x827FFCC8 end:0x828012E0      <- resumes AFTER it
```

— a hole of 0x1F0, and 0x827FFAD8 + 496 = 0x827FFCC8 **exactly** — while
`Trans.cpp`'s **last** `.text` block was `0x827FFAD8 end:0x827FFCC8`, 0x405008
from every other Trans block. The wrong name justified the hole and the hole
made the wrong name score.

So the fix had to be coupled: a rename alone would have read 0% forever, because
Trans's target obj cannot define `??0UIComponent@@`. Spelling was read from our
compiled COFF after building (`??0UIComponent@@IAA@XZ`, protected), never
guessed.

★ **dtk then corroborated it without being asked.** The first A/B was **REFUSED**
by the split-guard: dtk re-derived `.pdata 0x82245AC0-0x82245AC8` out of
`Trans.cpp` and into `UIComponent.cpp` by itself, because `.pdata` is derived
output re-computed from `.text` ownership on every split. Recovery is one build,
then commit what dtk wrote — and what it wrote is an independent agreement, from
a tool that knows nothing of the argument.

★★ **I UNDERSHOT BY 8×, and the reason is the transferable part.** I priced this
off W8-C's analogous re-home, which measured Δ0/Δ0, and never checked the
**caller population** — which CLAUDE.md explicitly instructs and which I had
read. The 16 gaining units are all `UIComponent` **subclasses** (`UILabel`,
`UIList`, `UIPicture`, `UISlider`, `StarDisplay`, `MeterDisplay`, `ScoreDisplay`,
`ScrollbarDisplay`, `BandHighlight`, `CheckboxDisplay`, `InlineHelp`,
`InstrumentDifficultyDisplay`, `LabelNumberTicker`, `ReviewDisplay`, …): every
subclass ctor calls `??0UIComponent@@`, so under `name_check` each of those call
sites was charged a relocation-NAME mismatch against the wrong map name.
`RndEnviron::Save` had no such callers; a widely-inherited base ctor has 16.
**A wrong name is financed by its callers, and repairing it is paid for by
them too.**

`none`-ruler control **flat at +0 B** against +6,400 graded. With a
retail-byte-adjudicated, non-map-only patch that is the wrong-name-repair
signature, not the fabricated-alias one; `scripts/symbol_aliases.json` was not
touched, and grepping it for `0x827ffad8`, `0x823f9b50`, `uicomponent@@iaa` and
`rndtransformable@@iaa` returns **0 occurrences each**, so this fix could not
have been laundering a fold (the corollary hazard W7-C found).

Rows after: `??0UIComponent@@` 36.52 → **81.07**; `??0RndTransformable@@`
unpaired → **73.94**. Both are correctly paired for the first time and neither
is finished.

### 5.2 Fix 2 — decisive identification, deliberate Δ0

`0x826A75C8` (708 B, the largest fire) was `??0GemPlayer@@`. The body calls
`??0Performer@@` and `??0MsgSource@@` directly, matching `class Player : public
Performer, public MsgSource`. That alone is not enough — `/Ob2` could have
inlined Player's ctor into GemPlayer's — so the decisive half is that the body
**never calls `??0BeatMatchSink@@`** (present at `0x8277B5A0`), which
`class GemPlayer : public Player, public BeatMatchSink` is required to
construct. `??0Player@@` was absent from the map; `??1Player@@` sits at
`0x826A7A00`. **Third instance of one signature: ctor mis-named, dtor correctly
named right after, correct ctor name absent.**

No pin move was needed, unusually: our compiled `GemPlayer.obj` also defines
`??0Player@@` as a COMDAT copy, so `default/GemPlayer`'s base obj can supply the
name.

**My Δfuzzy prediction was wrong in sign.** I expected a UIComponent-style
cascade; `??0Player@@` has only **two** call sites in the whole binary
(`0x826C84F4`, `0x826A39B4`) against `??0UIComponent@@`'s 16. Same lesson from
the other side.

**Kept anyway.** The identification is decisive, the map is now truer, and the
cost is zero bytes, zero functions and 0.000178pp of fuzzy — the same call
MAPID-1 made when naming `0x827BCD38` cost −1,656 B. Standing directive is
accuracy over headline.

---

## 6. What this lane did NOT do

- **Did not adjudicate the 18 fires below 44 B** (§3.2). Not "clean" — *not
  testable* by retail bytes. Recording an unfalsifiable verdict would be worse
  than recording none.
- **Did not resolve 9 of the 21 fires in the sample stratum.** They are counted
  against the screen in the conservative bound and listed in §7, with the
  evidence gathered so far. In particular I did **not** claim
  `?Exit@RGTrainerPanel@@` (calls `?XBMEndCapture@D3D@@` and clears a D3D
  global, host D3D) as a true positive — suggestive is not decisive.
- **Did not fix `0x826B03D8`** (`??0TexMovie@@`, 224 B), although I regard it as
  adjudicated TP: `TexMovie : RndDrawable, RndPollable` cannot construct an
  `RGGemMatcher`, and the body never calls `??0RndDrawable@@`. The blocker is
  the exact hazard the task names: **`RGTrainerPanel.cpp` has no `.text` pin at
  all**, so the correct home does not exist yet and a rename alone would read 0%
  forever. Creating that pin is a pinning lane's job, not a rename.
- **Did not fix `0x82605CA8`** (`?SyncProperty@RndParticleSysAnim@@`, 120 B, at
  fuzzy 99.83), also adjudicated TP: `RndParticleSysAnim : RndAnimatable` cannot
  call `?SyncProperty@StorePanel@@`, while `BandStorePanel : StorePanel` must.
  Left alone because `?SyncProperty@BandStorePanel@@` is absent from the map,
  there is a nearby adjustor thunk `0x82605FF8` still naming
  `RndParticleSysAnim`, and moving one without settling the other risks
  un-pairing a row that currently scores 99.83.
- **Did not chase the `?Init@Sfx@@` / `?Init@CharMeshHide@@` pair.** Both bodies
  are the identical Milo factory-registration shape differing only in two
  relocations, and `?Init@CharMeshHide@@` *already exists* at `0x8264D058`, so a
  rename would collide. Unwinding it needs the `StaticClassName` identities
  settled first — possibly a shifted region rather than one bad row.
- **Did not patch `tools/map_lint.py`**, despite finding two real defects in its
  `mangled_classes` (§1.3). Changing a shared helper mid-lane would have altered
  another tool's output while I was measuring with it.
- **Did not touch `scripts/symbol_aliases.json`**, and verified no alias group
  references any address or name involved in either fix.
- **Did not re-tune any threshold after adjudication began.** The FP rate is
  measured, not fitted (the failure lane W7-B explicitly flagged in itself).
- **Did not run the screen at `flank_min` 2 or 4 as the reported result.** Both
  are available (`--flank-min`); 3 was chosen before adjudication.

---

## 7. Handoffs

1. **Finish the two rows fix 1 opened.** `??0UIComponent@@` (496 B, **81.07**)
   and `??0RndTransformable@@` (512 B, **73.94**) are correctly paired for the
   first time and are now ordinary source-porting targets — 1,008 B between
   them, in the heavily-inherited base classes, so a completion may carry a
   caller cascade of its own. Price from `report.json`'s charged-site list, not
   from a mismatch count.
2. **`0x826B03D8` needs a `RGTrainerPanel.cpp` `.text` pin** before its rename
   can pay (§6). `TexMovie.cpp` carries **three** orphan blocks — `0x822B9308`
   (12 B), `0x824444E8` (64 B), `0x826B03D8` (224 B) — all three of which the
   screen fired on independently. Worth treating as one pinning task.
   ⚠ `0x824444E8` currently reads **fuzzy 100**, so removing that pin would
   *lose* 64 matched bytes; do not sweep the three together blindly.
3. **`0x822B9308`** (`?Reset@TexMovie@@QAAXXZ`, 12 B, fuzzy 0) has a signature
   contradiction worth an MPNGAP-1-style adjudication: the body reads `0(r4)`
   while the named signature (`QAAXXZ`) takes no arguments.
4. **`tools/map_lint.py --check class_mixing` cannot see this defect class** and
   should say so in its own docstring; and its `mangled_classes` has the two
   defects in §1.3.
5. **`tools/dc3_map.py` resolves its map path via a worktree-relative sibling.**
   It happened to work here only because `~/tmp/dc3-decomp` is a symlink to the
   real repo (md5-verified identical). That is the bug class fixed in `60837907`
   for `pin_from_symnames.py`, still live here, and its failure mode is shaped
   like a legitimate "not applicable".
6. **Re-run the screen after any pin wave.** Its input is the map, but its
   *value* is highest where map and pins have drifted together, and the
   orphan-block enrichment (§4.2) says those two drift as one.

---

## 8. Reproducing

```bash
python3 tools/spatial_map_screen.py                    # scan; controls run first
python3 tools/spatial_map_screen.py --selftest         # controls only
python3 tools/spatial_map_screen.py --break-control positive   # must exit 3
python3 tools/spatial_map_screen.py --json out.json --flank-min 2
```

The tool refuses (exit 3) and prints nothing if any control fails.
