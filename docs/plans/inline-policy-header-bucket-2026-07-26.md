# Inlined-by-us / out-of-line-in-retail — sizing the header-inline bucket

**Lane T, 2026-07-26.** Worktree `~/tmp/wt-laneT-headerinline`, branch
`laneT-headerinline`, base `349087a8` (28,238 strict).

Follow-on to `docs/plans/scatter-include-inlining-collapse-2026-07-26.md` §6, which
drained the scatter-include lever and named this one as "the real vein next door":

> `??0FilePath@@QAA@PBD@Z` (76 objs), `ObjRefConcrete<…>::~` (93 objs) and
> `ObjRefConcrete<…>::SetObjConcrete` (70 objs) are inlined by us everywhere and
> called out-of-line by retail — a **force-multiplier shape** with far more
> instances than this lane's.

This lane built the whole-binary candidate generator that the §6 note asked for,
measured the bucket, and A/B'd the top candidates. **The bucket is real and ~3×
the scatter one, but its per-candidate economics are poor** — see §5.

---

## 1. The shape

A function is defined in a header (or otherwise visible for inlining), so MSVC
`/O1 /Ob2` inlines it into every consuming TU of our build. Retail emitted it
**out of line** and *calls* it. Two consequences per consuming caller:

1. retail has a `bl` where we have an inlined body → body/frame divergence;
2. if the callee could throw, our now-local copy is provably nothrow at the call
   site, so MSVC **deletes the caller's EH cleanup funclets** — and the
   `__unwindtable$` symbol with them.

Both halves are observable straight out of COFF (no disassembly heuristics), via
the `__unwindtable$<mangled>` primitive documented in the scatter doc §2.1.

### 1.1 ★ Retail does not fold identical COMDATs across TUs

Load-bearing structural fact, established from `scripts/target_symbol_map.json`:
retail emits a **separate out-of-line copy per instantiation per translation
unit**. `??_G?$ObjRefConcrete@VObject@Hmx@@VObjectDir@@@@UAAPAXI@Z` (the
vector-deleting dtor thunk) exists at **three independent VAs** —
`0x82368580` (DataFunc.cpp), `0x8228d528` (BandDirector.cpp), `0x823348e8`
(BandCharDesc.cpp) — three copies of a byte-identical template body. Likewise
`~ObjRefConcrete<T,ObjectDir>` has ~26 named out-of-line instances across ~15
units.

So this is not "retail had one shared copy". Retail simply did not inline the
callee, per-TU, everywhere. That is what makes the shape a *policy* divergence
rather than a linker artifact.

## 2. The candidate generator — `scripts/harvest/header_inline_policy_scan.py`

```
venv/bin/python scripts/harvest/header_inline_policy_scan.py \
    --repo . --json ~/tmp/laneT/hip.json --top 35 --min-sites 2
```

Needs a built tree (`build/45410914/report.json`, `build/45410914/src`,
`build/45410914/obj`). Runs in ~5 s; imports its COFF primitives from
`scripts/harvest/funclet_cascade_rank.py` rather than duplicating them.

Method: for every pinned unit that has both a dtk target obj and one of our objs,
take each function defined on **both** sides and diff the set of **call**
relocations — relocs landing on a `bl` (opcode 18 with LK), not all relocs.
Callees present on the target side and absent on ours are *target-only callees*:
retail called something we did not. Aggregate by callee.

### 2.1 ★★ The per-site precision filter is what makes this honest

The raw "target-only callee" signal is dominated by ordinary body divergence. The
discriminator is **per-site**, not per-callee:

> We can only have *inlined* the callee if its body is in **this caller's own
> TU**. If our obj for that caller does not define the callee, the missing `bl`
> is body divergence (or a symbol-map mispair) — a different lever entirely.

Applying that test cuts the pool by 72% (§3). It is also self-validating: the
`NOT-IN-TU` class (callee defined in *none* of our objs) drops to exactly **0**
workable sites, as it must.

### 2.2 Negative results the filter produces immediately

Two of the three symbols §6 named as the headline instances are **not** defects:

* **`??0FilePath@@QAA@PBD@Z`** — already carries `__declspec(noinline)` in
  `src/system/utl/FilePath.h:13` (from the original dc3 scaffold `8b286231b1`).
  It is emitted with EH in **76/76** objs and has exactly **1** workable defect
  site binary-wide. The "76 objs" figure was **fan-out, not a defect count.**
* **`??1String@@UAA@XZ` / `?PathName@@YAPBDPBVObject@Hmx@@@Z`** — headline hits on
  the unfiltered ranking (9 and 13 non-strict sites) that are **already defined
  out-of-line** in `Str.cpp` / `Object.cpp`. We cannot be inlining them. Their
  missing `bl`s are body divergence — for `PathName` specifically the known
  `END_HANDLERS` / `MILO_NOTIFY` stripping divergence already documented at
  `src/system/obj/Object.h:1081-1099`.

Anyone ranking this bucket without the per-site filter will fund these first.

## 3. ★ Bucket size, measured

Over **776 pinned units / 11,178 paired parent functions**:

| class (where the callee's body lives) | callees | non-strict sites | of which EH | **in-TU non-strict** | **in-TU EH** |
|---|--:|--:|--:|--:|--:|
| HEADER-INLINE (defined in ≥2 of our objs) | 644 | 500 | 247 | **142** | **64** |
| SINGLE-TU (defined in exactly 1) | 414 | 344 | 172 | 52 | 21 |
| NOT-IN-TU (defined in none) | 135 | 159 | 51 | **0** | **0** |

The last two columns are the §2.1-filtered truth. **The workable bucket is 142
caller functions** (194 counting the SINGLE-TU/scatter overlap), of which **64
also lost every funclet** (the SHARP half, where an EH cascade is available on
top of the parent itself).

### 3.1 It is long-tailed, which is the economic problem

107 HEADER-INLINE callees carry ≥1 workable site; only **14** carry ≥2. That is
1.3 sites per callee. There is no head to attack: the top candidate is 8 sites,
and the median candidate is a single function. Compare the scatter lever's
~11 candidate parents — this bucket is **~13× larger in sites** but is spread
across ~107 independent source edits, each needing its own whole-binary A/B.

Sites-per-callee histogram (HEADER-INLINE, workable):

```
1 site : 93 callees      3 sites : 3      8 sites : 1
2 sites:  7 callees      4 sites : 2     10 sites : 1
```

### 3.2 ★ The two axes that actually price this bucket

**Axis 1 — callee fan-out = blast radius.** How many of our objs emit the callee,
i.e. how many TUs change codegen if it is forced out of line:

| callee fan-out | workable sites |
|---|--:|
| 2–4 objs (contained) | **55** |
| 5–19 objs | 41 |
| 20–69 objs | 13 |
| 70+ objs (fleet-wide) | 33 |

**Axis 2 — STL template vs real code.** STL instantiation-swarm members have a
measured ~0/6 historical flip rate and are a common home for symbol-map mispairs:

| | callees | sites | EH sites | of which low-fan-out |
|---|--:|--:|--:|--:|
| STL template | 63 | 71 | 22 | 43 |
| non-STL | 44 | **71** | **42** | 12 |

These two axes are **anti-correlated**, and that is the core economic finding:
the contained-blast-radius half is overwhelmingly STL (43 of 55 low-fan-out sites),
i.e. the historically dead class; while the non-STL half — the 71 sites with real
bodies and 42 EH cascades — sits mostly behind callees with 5+ TU fan-out.

**There is no low-risk / high-yield quadrant.** The honestly fundable pool is
~71 non-STL sites, nearly all of which require perturbing 5–279 TUs to chase 1–2
functions each.

### 3.3 ★★ Contamination: the caller side is half STL, and STL here means mispair

Axis 2 above classifies the *callee*. Classifying the **caller** cuts harder.
Deduplicated to distinct caller functions:

| | count |
|---|--:|
| distinct workable caller functions (all classes) | 162 |
| caller is itself an STL template instantiation | **90** |
| caller is real (non-STL) code | 72 |
| real caller **and** ≥1 real (non-STL) callee | **65** |

**56% of the workable sites are STL-instantiation callers.** That population is
both the historically dead flip class and the known home of symbol-map mispairs.

The mispairs are directly visible once you read the pairs. The best low-fan-out
non-STL candidate on the ranking, `??1EventCall@EventAnim@@QAA@XZ` (fan-out 5, 2
sites, both EH), turns out to have **both** of its sites inside
`list<ProxyCall<EventTrigger>>::clear` / `::erase`. A `list<ProxyCall>` does not
destroy an `EventAnim::EventCall`; this is the classic mispair tell, not a lever
instance. Several survivors of the 65 fail the same smell test on inspection —
`?DevHostname@@YAPBDVSymbol@@@Z` ← `?MainThread@@YA_NXZ`, and
`??1?$ObjRefConcrete@VBandCamShot@@…` ← `??8Symbol@@QBA_NPBD@Z` are not real call
edges.

> **Realistic fundable pool after both filters and a mispair smell test:
> ~30–50 caller functions**, not 500 and not 142. (Mispair repair is a
> single-owner lane — these are reported, not applied, here.)

### 3.4 The top of the ranking

| callee | our objs | w/EH | in-TU non-strict | in-TU EH |
|---|--:|--:|--:|--:|
| `??$MakeString@PBD@@YAPBDPBD0@Z` | 115 | 0 | 10 | 5 |
| `?SetObjConcrete@?$ObjRefConcrete@VObject@Hmx@@VObjectDir@@@@…` | 70 | 0 | 8 | 6 |
| `??3BinStream@@SAXPAX@Z` (operator delete) | 279 | 0 | 4 | 4 |
| `??5BinStream@@QAAAAV0@AA_N@Z` (`operator>>(bool&)`) | 94 | 0 | 4 | 3 |
| `??1?$ObjRefConcrete@VRndTransformable@@VObjectDir@@@@UAA@XZ` | 93 | 93 | 3 | 3 |
| `??$__uninitialized_copy@PBUEyeDesc@CharEyes@@PAU12@@stlpmtx_std@@…` | 2 | 2 | 3 | 1 |
| `??$_Copy_Construct@UNavItem@HamNavProvider@@@stlpmtx_std@@…` | 2 | 2 | 3 | 0 |
| `?Load@?$ObjRefConcrete@VRndTex@@VObjectDir@@@@QAA_N…` | 52 | 0 | 2 | 2 |
| `??1EventCall@EventAnim@@QAA@XZ` | 5 | 5 | 2 | 2 |

"our objs" is the fan-out — how many TUs would change codegen if the callee is
forced out of line. **That column is the risk, and it dwarfs the payoff** in
every high-ranking row: 70–279 TUs perturbed to chase 4–10 sites.

## 4. Constructs (inherited, not re-derived)

From the scatter doc §5c, measured on a control:

| construct | restores `bl`? | restores funclets? |
|---|---|---|
| `#pragma optimize("g", off)` / `("", on)` around the body | yes | **yes** |
| opaque `extern` throwing edge in the body | yes | **yes** |
| `__declspec(noinline)` | yes | no (~61.7% plateau) |
| `#pragma auto_inline(off)` / `inline_depth(0)` around body | yes | no |
| `throw(...)` on decl or definition | no | no |
| moving the `#include` to the top of the TU | no | no |

**The inliner and the nothrow analysis are independent axes.** Suppressing
inlining is not merely insufficient — it is the wrong axis when funclets are
involved. For a non-template callee, moving the definition into a `.cpp` hits
both axes and is lower-risk than the pragma (it does not change the callee's own
optimization level).

## 5. Per-candidate whole-binary A/B results

All measured against baseline **28,238**, full rebuild after every header edit
(`rm -f build/45410914/report.cache && ./tools/ninja-locked`), verdict from
`report.json match_percent_normalized == 100.0` only, checked **unit-agnostically**
as well as unit-keyed.

### 5.1 Candidate A — `ObjRefConcrete<T1,T2>::SetObjConcrete`, `#pragma optimize("g", off)`

Top-ranked non-STL candidate: 70-obj fan-out, **8** workable in-TU sites, **6**
with every funclet deleted, sites spread over 8 distinct units.

| | value |
|---|--:|
| strict after | **28,238** |
| net (unit-keyed) | **0** |
| gained / lost | 0 / 0 |
| net (unit-agnostic) | **0** |

**Result: exactly neutral. Not one of the 8 predicted sites flipped, and nothing
regressed.** Stronger than that — comparing full per-function match% across the
whole binary, **0 of 69,393 functions changed percentage at all.**

### 5.1.1 ★★ Why: `#pragma optimize` does NOT apply to a template body

Zero movement across 69k functions is not "the lever is weak", it is "the
construct did nothing". Settled with a single-obj byte test rather than
inference (`BandCamShot.obj`, pragma vs baseline):

| | result |
|---|---|
| obj md5 | **differs** — the pragma is not being ignored outright |
| function symbols byte-identical | **12,122** |
| function symbols differing | **0** |
| differing symbols | 51, **all of them data globals** (`?gModTime@@3KA`, `?TheWorld@@3PAVWorldDir@@A`, …) — section-layout shift only |

And specifically for the predicted site `?EndAnim@BandCamShot@@UAAXXZ`: **byte
identical** between the two builds — same 80 words, same 8 `bl`s, and no
`__unwindtable$`. The `bl ?SetObjConcrete@?$ObjRefConcrete@VRndEnviron@@…` visible
in that function **was already present at baseline**; it is a *different
instantiation* (`<RndEnviron,ObjectDir>`) that we always called out-of-line, and
not the `<Object,ObjectDir>` the candidate was selected on. Reading it as a
restored call was an error caught only by the byte test.

> **Conclusion: `#pragma optimize("g", off)` around a template definition in a
> header has no effect on codegen in consuming TUs.** MSVC evaluates the
> optimize-pragma state at the point of *instantiation* (end-of-TU, ambient
> state), not at the point of definition. The pragma only reaches the callee's own
> out-of-line COMDAT copy — which is why the obj changes at all — and never
> reaches the inline decision at the call site.

**This materially narrows the scatter doc §5c construct table.** The only
construct proven to restore both the `bl` *and* the caller's funclets was
validated on a **non-template** callee (`DataNode`'s copy ctor, hosted in a
`.cpp`). It does **not** transfer to templates. Since ObjRefConcrete, MakeString
and the entire STL population of this bucket are templates, **the one working
construct is unavailable for the majority of the bucket's candidates.**

Reverted; not committed. Revert verified byte-identical (`BandCamShot.obj` md5
back to `9cdcb891…`) and whole-binary strict back to exactly 28,238.

## 6. ★★ Verdict and pricing for a follow-up wave

### 6.1 The funnel, end to end

The headline number and the fundable number differ by **~60×**:

| filter | callers remaining |
|---|--:|
| raw "retail calls a callee we don't" (non-strict) | ~500 |
| §2.1 per-site: callee's body is in the caller's own TU | **162** |
| − **§6.1.1 kill filter: callee is already strict-100 somewhere** | **46** |
| − callers that are STL template instantiations (dead class + mispair home) | 17 |
| − callers with no **non-template** callee (the pragma is inert on templates, §5.1.1) | **8** |
| − visible symbol-map mispairs among those 8 | **~5–7** |

Every step of that funnel is measured, not estimated, except the last.

#### 6.1.1 ★★ The kill filter — the single most important result of this lane

> **If the callee is itself already strict-100 in some unit, there is no
> inline-policy divergence to correct.**

Retail emitted the out-of-line COMDAT **and** inlined the same function at other
call sites. An in-class/header definition already reproduces *both* behaviours —
which is exactly why the callee matches. Forcing it out of line can only lose: it
deletes the COMDAT from every consuming obj (killing the paired instance) and
removes the inlining retail actually performed.

This was found the expensive way, by a lane sub-agent measuring
`??5BinStream@@QAAAAV0@AA_N@Z` moved to `BinStream.cpp`: **−4 whole-binary**
(0 gained, 4 lost, unit-agnostic identical), the losses being three real callers
plus the callee's own 84-byte 100.0% instance in `default/BandCharacter`.

Applying it to the census:

| | callees | sites |
|---|--:|--:|
| workable before | 156 | 194 |
| **survive the kill filter** | **42** | **50** |
| killed | 114 | 144 |

**Validation: all four candidates this lane A/B'd — `SetObjConcrete`,
`MakeString<const char*>`, `BinStream::operator>>(bool&)`,
`BinStream::operator delete` — are killed by this filter.** It is now implemented
in the scanner (`callee_strict`), so a future wave never re-picks them.

### 6.2 The two structural reasons this bucket is worse than its size suggests

1. **The only working construct does not reach most of the bucket.**
   `#pragma optimize("g", off)` is proven inert on template bodies (§5.1.1), and
   **106 of 156 workable callees / 136 of 194 sites are templates.** For those, the
   only remaining option is a genuine out-of-line definition (explicit
   specialization hosted in a `.cpp`), which is invasive and untested at scale.
2. **Risk and yield are anti-correlated (§3.2).** The contained-blast-radius
   callees (2–4 objs) are 43/55 STL — the ~0/6 historical flip class. The non-STL
   sites sit behind callees with 5–279 TU fan-out. There is no low-risk /
   high-yield quadrant.

### 6.3 Measured flip rate

| candidate | construct | sites predicted | flipped | whole-binary net |
|---|---|--:|--:|--:|
| `ObjRefConcrete<T1,T2>::SetObjConcrete` (70 objs) | `#pragma optimize("g", off)` at file scope | 8 (6 EH) | **0** | **0** (inert, §5.1.1) |
| `BinStream::operator>>(bool&)` (94 objs) | body moved out-of-line to `BinStream.cpp` | 4 (3 EH) | **0** | **−4** |
| `BinStream::operator delete` (279 objs) | `#pragma optimize("g", off)` inside the class body | 4 (4 EH) | **0** | **0** (inert, §5.2.1) |

| `MakeString<const char*>` (115 objs) | `optimize("g",off)` on an explicit **specialization** | 10 (5 EH) | **0** | **−1** |
| `MakeString<const char*>` | `__declspec(noinline)` on the specialization | 10 (5 EH) | **0** | **0** |
| `MakeString<const char*>` | definition moved to `MakeString.cpp` (body invisible) | 10 (5 EH) | **0** | **−1** |

**Measured flip rate: 0/26 sites across four candidates and six constructs,
perturbing 70, 94, 115 and 279 TUs; best case 0, worst case −4.** Not a single
predicted caller moved by any amount in any variant — the `MakeString` callers
were bit-identical to baseline to 6 decimals.

### 5.3 Candidate B — `MakeString<const char*>` (sub-agent, own worktree)

The three variants above extend §5.1.1 in an important way. Variant 1 wrapped a
full **explicit specialization** — a concrete, non-template function — precisely
to dodge the instantiation-point problem, and it was **still inert at every call
site**; its only effect was to deoptimize the callee's own body and break its one
pairing (−1). Variant 2 kept the identical emission-forcing mechanism minus the
pragma and scored 0/0, cleanly isolating the −1 to the pragma rather than to the
substitute odr-use.

Variant 3 is the strongest negative in the lane: the body was moved out of the
header entirely, so a `bl` was *structurally forced* — and still nothing moved,
because the callers never called that instantiation at all (§6.3.2).

Two mechanical notes for future candidates: an explicit specialization cannot be
explicitly instantiated (`error C3416`), so a specialization-based construct on a
callee that has a forced-instantiation site requires replacing that line (an
`extern`-linkage function-pointer odr-use works and is match-neutral, proven by
variant 2's 0/0); and `__declspec(noinline)` on an explicit specialization
declared after the primary template appears to be silently ignored by this
compiler.

### 5.2 Candidate C — the two `BinStream` symbols (sub-agent, own worktree)

`operator>>(bool&)` moved out of the class into `BinStream.cpp` measured
**28,238 → 28,234 (−4)**, 0 gained / 4 lost, unit-agnostic identical (no anon
`fn_` re-homing at all). Lost: `?OldLoadProxies@RndDir@@`,
`?Load@BandSongMetadata@@`, `?LoadFixed@TourBand@@`, and the callee's own
instance `??5BinStream@@QAAAAV0@AA_N@Z` in `default/BandCharacter`.

That regression is what produced the kill filter (§6.1.1): the callee was an
84-byte target function **already at 100.0%**. Retail both emits the COMDAT and
inlines at the other sites; our in-class definition already reproduced that, so
the change was a loss in both directions rather than a funclet trade.

`operator delete` was not worth a cycle once two facts were established: the
target `??3BinStream@@SAXPAX@Z` is **4 bytes at 100.0%** in `default/MeshDeform`
(retail's operator delete *is* the tail-branch thunk we already emit), and the
symbol comes from the `MEM_OVERLOAD` macro at `src/system/utl/MemMgr.h:284`,
which **already carries `__declspec(noinline)`** deliberately — the in-repo
comment at `MemMgr.h:266-275` documents that retail ICF-folds every identical
`MemFree(p)` body into one thunk (`fn_82709EE0`) and that noinline is what
reproduces it. Its four census "callers" do not delete a `BinStream` at all; they
reach the symbol only through that ICF fold.

#### 5.2.1 ★ Second construct-level negative: `#pragma optimize` inside a class body

Wrapping the hand-expanded `MEM_OVERLOAD(BinStream, 0x55)` in
`#pragma optimize("g", off)` **inside the class body** was accepted by MSVC X360
with no diagnostic (no C4177) and was **silently inert**: the emitted COMDAT for
`??3BinStream@@SAXPAX@Z` in `MeshDeform.obj` is still `48000000`, a relocated
tail-branch — with `/Og` genuinely off MSVC would emit a framed `bl`+`blr`.
Whole-binary 28,238, byte-identical 100% set.

> **Playbook addendum: `#pragma optimize` is ignored inside a class body — it must
> be at file scope.** Combined with §5.1.1 (ignored for template bodies even *at*
> file scope), the one construct the scatter lane proved is far narrower in reach
> than its write-up implies: it works only on a **non-template, file-scope,
> non-in-class** definition.

### 6.3.1 ★★ Fan-out is blast radius, never yield

The premise that made this bucket look like a force multiplier — "`MakeString` is
in 115 objs, `ObjRefConcrete::~` in 93, `FilePath` ctor in 76" — is a
**category error**. Those numbers count *our* COMDAT emissions. Only functions
that `report.json` actually **pairs** are scored, and:

> **Every one of the 30 surviving candidates is paired in exactly ONE unit
> (`PAIR = 1`)** — including the 61-obj `?Multiply@@YAXABVVector3@@…` and the
> 52-obj `?Load@?$ObjRefConcrete@VRndTex@@…`.

So the callee side of any candidate here can contribute **at most ±1**, no matter
how many TUs it is emitted into. Fan-out is purely the number of TUs whose
codegen you perturb — i.e. *risk*. Measured on `MakeString<const char*>`: 115
emissions, 1 paired unit, maximum possible score +1, **measured score −1**.

The scanner now prints a `PAIR` column beside `DEFOB` so this cannot be misread
again.

### 6.3.2 A false-positive mode in the per-site test

The §2.1 test ("our obj defines the callee ⇒ we could have inlined it") is
satisfied by a **forced explicit instantiation**. `src/system/bandobj/BandCharacter.cpp:2716`
carries a hand-added `template const char *MakeString<const char *>(const char *, const char *);`;
deleting it and rebuilding the single obj makes the symbol vanish entirely. Our
BandCharacter source never calls that instantiation, so 2 of its 10 "sites" were
false positives.

Confirmed independently by a full relocation-table scan of `Crowd.obj` (10,308
relocations): **zero** references to `??$MakeString@PBD@…` in any variant —
including the variant where the body was moved to a `.cpp` and a `bl` was
therefore structurally forced. Crowd inlines `MakeString<int>`, a *different*
instantiation; the `<const char*>` COMDAT emission was incidental.

**Addendum for future use of this scanner:** treat a site as real only if the
caller's obj has a relocation *to* the callee in the target, and discount callees
whose in-TU definition comes from a forced explicit instantiation.

### 6.4 Recommendation — DE-FUND

**This bucket is drained. Do not fund a wave.** The complete surviving candidate
list after all measured filters is **8 caller functions**, several of which are
visibly symbol-map mispairs:

| caller | callee |
|---|---|
| `??0RndParticleSys@@IAA@XZ` | `?UpdateSphere@RndParticleSys@@UAAXXZ` |
| `?BuildTransform@CamShotFrame@@QBAX…` | `?Multiply@@YAXABVVector3@@ABVTransform@@AAV1@@Z` |
| `?Draw3DChars@WorldCrowd@@IAAXXZ` | `?Normalize@@YAXABVVector3@@AAV1@@Z` |
| `?Handle@TourProgress@@UAA…` | `?GetNumSongsForCurrentGig@TourProgress@@QBAH…` |
| `?ResetFilter@MusicLibrary@@QAAXW4FilterType@@@Z` | `?RemoveLastSongFromSetlist@MusicLibrary@@QAAXXZ` |
| `?SelectNode@MusicLibrary@@QAAX…` | `?OnMsg@MusicLibrary@@QAA…ABVPrim…` |
| `?SetFrameEx@HamCamShot@@MAAXMM@Z` | `?StaticClassName@EventAnim@@SA?AVSymbol@@XZ` ← mispair |
| `?resize@?$ObjList@UPropTriggerDefn@FlowTrigger@@@@QAAXI@Z` | `??0Anim@EventTrigger@@QAA@PAVObject@Hmx@@@Z` |

Eight functions, each needing an invasive `.cpp`-hosting edit plus its own
whole-binary A/B at ~25 min per cycle under load, with a measured flip rate of
**0/16** so far and one candidate at **−4**. That is not fundable against the
veins currently open.

The `MusicLibrary` pair is the least-bad starting point if anyone insists — both
callees are fan-out 1 (emitted in exactly one obj), so blast radius is a single
TU. `?Multiply@@YAXABVVector3@@ABVTransform@@AAV1@@Z` (61 objs) and
`?Normalize@@YAXABVVector3@@AAV1@@Z` are the math-header inlines and are the only
entries with a plausible force-multiplier story left, but both are fleet-wide.

### 6.5 Mispairs observed (reported, not applied — map repair is single-owner)

Pairs from the workable set that are not real call edges and should be treated as
`target_symbol_map.json` defects rather than lever instances:

| caller | "callee" |
|---|---|
| `?clear@?$_List_base@UProxyCall@EventTrigger@@…` / `?erase@?$list@UProxyCall@…` | `??1EventCall@EventAnim@@QAA@XZ` |
| `?DevHostname@@YAPBDVSymbol@@@Z` | `?MainThread@@YA_NXZ` |
| `??1?$ObjRefConcrete@VBandCamShot@@VObjectDir@@@@UAA@XZ` | `??8Symbol@@QBA_NPBD@Z` |
| `?SetFrameEx@HamCamShot@@MAAXMM@Z` | `?StaticClassName@EventAnim@@SA?AVSymbol@@XZ` |
| `?_M_fill_insert_aux@?$vector@H…` (from scatter doc §5b) | `?GetCurrentSortName@MusicLibrary@@…` |

### 6.6 Durable outputs

* `scripts/harvest/header_inline_policy_scan.py` — re-runnable in ~5 s. Two
  reusable filters, both of which generalise well beyond this lever to any
  "retail calls something we don't" question:
  * the **per-site test** (§2.1) — is the callee's body in *this* caller's TU;
  * the **kill filter** (§6.1.1) — is the callee already strict-100 somewhere.
    This one is cheap, decisive (156 → 42 callees), and would have pre-killed
    every candidate this lane spent a build cycle on.
* **`#pragma optimize` reaches far less code than assumed** — ignored for template
  bodies even at file scope (§5.1.1) and ignored entirely inside a class body
  (§5.2.1). It works only on a non-template, file-scope, non-in-class definition.
  This corrects the scope of the scatter doc §5c table and closes off a whole
  family of speculative fixes.
* **"Retail emits it out-of-line" and "we inline it" are not opposites.** Retail
  routinely does *both* for the same function. A callee that already matches is
  evidence the header definition is *correct*, not evidence of a defect — the
  inverted intuition that made this bucket look like a force multiplier.
* The funnel in §6.1 is the template for pricing any future "N inline sites!"
  claim: fan-out is not defect count, and the filters cut 500 → 8.
