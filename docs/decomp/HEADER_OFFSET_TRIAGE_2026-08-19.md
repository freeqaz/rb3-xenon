# Header-offset triage — closing the 76% un-triaged bucket, 2026-08-19

> **STATUS (2026-08-19): CURRENT.** Closes the open item left by
> `HEADER_OFFSET_AUDIT_2026-08-18.md` §3 ("the un-triaged bucket is 76% of the
> finding and is NOT a clean bill"). Tool: `tools/header_offset_triage.py`.
> ⛔ **The headline of the 08-18 audit is now substantially reframed: 91.3% of
> the disagreeing comments are inherited VERBATIM from a sibling decomp of a
> DIFFERENT BUILD.** They were never RB3-360 measurements. Read §3.

## 1. The un-triaged bucket is closed

The 08-18 audit joined headers to `report.json` units with a **substring
heuristic** that resolved only 178 of 740 classes. This replaces it with an
**exact, name-keyed join**: every MSVC mangled symbol carries its own immediate
class qualifier, so `?IsLoaded@MasterAudio@@QAA_NXZ` names class `MasterAudio`
with no path reconstruction at all.

★ That sidesteps *both* the substring heuristic **and** the bare-vs-nested
`basename()` hazard that broke four pinning lanes — there is no path to
reconstruct, so there is nothing to get wrong.

| bucket | 08-18 (substring) | **08-19 (exact join)** |
|---|---:|---:|
| **A — layout PROVEN** | 175 cls / 1,082 rows | **551 / 4,017** |
| **NO_WITNESS** (class-B risk) | 3 / 47 | **32 / 237** |
| ⚠ **UN-TRIAGED** | **562 / 4,178** | **0 / 0** |
| **UNADJUDICABLE** (new class) | — | 157 / 1,053 |
| total | 740 / 5,307 | **740 / 5,307** ✅ |

Self-validating: the buckets sum to the audit's own class and row totals exactly.

★ **UNADJUDICABLE is a bucket the 08-18 audit did not anticipate.** 157 classes
have **no identified member function in the RB3 binary at all** — overwhelmingly
`hamobj/`, `gesture/`, `flow/`, `meta_ham/`, i.e. Dance Central subsystems whose
headers were inherited from dc3-decomp. ⚠ The first hypothesis, *"this code is
absent from RB3"*, was **WRONG and was corrected before it was written down**:
`default/HamDirector` **is** a unit in `report.json` — but it holds **6 functions
/ 668 B** and its single mangled symbol is `OfflineCallback`, not `HamDirector`.
So the code is *pinned but unidentified*, not absent. Either way no name-keyed
instrument can adjudicate it.

## 2. The discriminator, and what it actually proves

★ **A class with a function scoring `fuzzy_match_percent == 100` has a provably
correct layout for every member that function touches** — any offset error
changes the instruction encoding and would break the byte match.

⛔ **Watch the circularity.** Our obj's displacements *are* our compiler's
offsets by construction, so "the compiler offset appears in our obj" proves
nothing on its own. The evidence is entirely carried by `fuzzy == 100`: **our
bytes equal retail's bytes**, so a `this`-relative access at offset K in a
matching body means *retail* puts the member at K.

### The displacement scan had to be rebuilt once

The first version counted **every** D-form displacement in the body. Measured
against nulls it was **not usable as a classifier**:

| test | whole-body | `this`-relative |
|---|---:|---:|
| compiler offset seen | 73.1% | 52.4% |
| commented offset seen | 51.0% | 33.8% |
| **NULL (+0x40)** | **29.9%** | **10.4%** |
| enrichment vs null | 2.44× | **5.01×** |

A body is full of stack accesses off `r1`, so a fabricated offset "appeared" 30%
of the time. **Using a 2.44× enrichment as a deterministic classifier is exactly
the refuted `callee absent from map ⇒ fold-alias` mistake (1.95×)** — an
enrichment ratio describing the *detector*, not the defect. The fix was to track
`this` (r3, propagated through `mr`, evicted on any clobber the tracker cannot
decode with certainty — **under-report rather than invent**).

⚠ **The null is still contaminated and the honest bound is wide.** Four nulls:

| null | fires | enrichment |
|---|---:|---:|
| compiler+2 (word-misaligned) | 1.7% | 30.4× |
| compiler+0x40 | 10.4% | 5.01× |
| random 4-aligned **in the class's own span** | **24.5%** | **2.14×** |
| compiler+0x400 (outside the class) | 0.0% | — (near-vacuous) |

The 24.5% null is contaminated **upward**: the findings JSON records only
*disagreeing* members, so an offset that is "not one of them" is usually a real,
**agreeing** member. True FP sits between 1.7% and 24.5% and **cannot be
tightened without the full member list**. Stated rather than papered over.

## 3. ★★★ THE REFRAMING: 91.3% OF THE COMMENTS ARE NOT RB3 MEASUREMENTS

The 08-18 audit framed every row as *"class A (stale comment) or class B (our
layout is wrong, comment came from retail RE)"*. **Class B's premise is wrong for
almost the whole population.** Censused by member name against both sibling
decomps:

| provenance of the disagreeing comment | rows | share |
|---|---:|---:|
| byte-identical to **dc3-decomp**'s comment | 2,559 | 48.2% |
| byte-identical to **rb3-Wii**'s comment | 2,287 | 43.1% |
| **provably inherited from a sibling decomp** | **4,846** | **91.3%** |
| our comment edited **away** from the oracle | 171 | 3.2% |
| no oracle header at all | 45 | 0.8% |

Where the member resolves in the oracle, the comment is inherited **96.0%**
(DC3) and **97.3%** (rb3-Wii) of the time.

⇒ **These comments describe a DIFFERENT BUILD.** DC3 is a *newer* Milo engine;
**rb3-Wii is a different CPU, compiler and ABI entirely** (MWCC/Wii vs MSVC/X360),
so its container and `ObjPtr` sizes are not ours. A comment/compiler disagreement
is therefore, by default, **a provenance artifact — not evidence that our layout
is wrong.** Class-B risk is far smaller than "740 classes disagree" implies.

Worked example: `rndobj/TexProc.h` shows deltas of +28→+44, which *looks* like a
class missing 0x1c bytes of members. It is not — our declaration matches DC3's
member-for-member, and **dc3-decomp's header carries the identical `// 0x40`
comments**. The gap is `ObjPtr` being 20 B in DC3 and **12 B** here, which 43
independently proven rows corroborate (`RndMultiMesh::mMesh` @ 0x24,
`CharServoBone::mRegulate` @ 0x9c).

★ **The 171 edited-away rows are the higher-authority set** (someone changed them
deliberately). In **16** of them the oracle *agrees with our compiler* and only
our comment is the outlier — e.g. `FileLoader::mStream` ours `0x1c`, compiler
`0x20`, DC3 `0x20`. Those are unambiguous class A. The other 155 are three-way
disagreements = genuine DC3-vs-RB3 engine differences.

## 4. What was applied

**2,032 comment rows in 370 headers**, restricted to the rigorous set (a
`fuzzy==100` body makes a `this`-relative access at the compiler's offset).

- `git diff`: **2,032 insertions / 2,032 deletions**, and **every changed line is
  a trailing `// 0xHEX` comment** — verified by filtering the diff, not asserted.
- Whole-binary metric, full build, same commit, one variable:
  **`matched_functions` 44,514 · `matched_code` 3,760,072 · `total_code`
  10,245,956 · `code%` 36.698110 — identical to the digit** before and after.
  Metric-neutral as predicted.

### The apply gate refuses rather than guesses

Every row is gated on the line still naming the member **and** the trailing
comment still holding the exact value the audit recorded. Refusals:

| refusal | rows |
|---|---:|
| no single trailing `0xHEX` comment | 31 |
| multi-offset array comment (`// 0x1c, 0x38, …`) | 3 |

⚠ **The first sabotage control DID NOT FIRE, and that was a defect in the
control, not a pass.** Corrupting `SongParser.h:251` changed nothing because that
row is not in the *proven* set, so the gate was never asked — *a check that
cannot fail proves nothing*. Re-run against a **proven** row (`mSink`, line 250)
the gate fired correctly: applied 2032→2031 with one
`comment value != audited value (header drifted)`.

★ **Those 31 refusals are mostly AUDIT false positives, not tool failures** — a
third FP class the 08-18 audit did not know it had:

- **dual-value comments**: `Object.h` `// 0x20 (native) / 0x18 (X360)` — the
  audit read the first value and "wanted" the X360 value the line **already
  states**.
- **multi-declarator lines**: `float mSlope, mB; // 0x14 0x18`.
- **annotated comments**: `// 0x38 - enum`, `// 0x58 / -0x8c`.

## 5. Not done — read before quoting this

- **The 32 NO_WITNESS classes are NOT cleared.** 17 are DC3-only
  (`hamobj`/`gesture`/`flow`); the 15 real ones (`RndRibbon` 14 rows, `TexProc`
  11, `DxTex` 6, `Scheduler` 5, `RndPostProcMgr` 4, …) have **no matching
  function**, so our layout is unconstrained by retail there. §3 lowers their
  prior; it does not settle them.
- **1,985 A_PROVEN rows were left alone** — the class has a witness but no
  matching body demonstrably touches *that* member. The class-level uniform-shift
  argument supports them; per-row proof does not. Not applied.
- ★ **A blind spot in the name-keyed join**: layout evidence is not limited to a
  class's *own* member functions — **any** matching function that touches the
  class proves its layout, and for a **nested** class the enclosing class's
  methods are the natural witnesses. `SongParser::DifficultyInfo` sits in
  NO_WITNESS while its enclosing `SongParser` has **42 functions at fuzzy==100**.
- ★★ **A second discriminator exists and is unexploited: structural
  impossibility.** `DifficultyInfo`'s comment puts `mActivePlayers` at `0x8`
  after a `std::vector`, implying an **8-byte vector**; the compiler reports
  STLport `vector` = **12 B** (`_M_start`/`_M_finish`/`_M_end_of_storage`). The
  comment is refuted **with no retail evidence at all**. Mechanising "commented
  gap < the member type's true size" would settle NO_WITNESS and UNADJUDICABLE
  rows that no name-keyed instrument can reach.
- No layout was changed and no code was touched by this lane.
