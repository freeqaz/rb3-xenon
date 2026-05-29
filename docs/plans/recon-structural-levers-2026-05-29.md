# Structural-lever recon (non-CamShot)

Survey of high-leverage structural levers in the rb3-xenon decomp, **excluding
CamShot blast radius** (the `camshot-port` agent owns CameraShot.h/.cpp +
CameraManager + BandCamShot + BandDirector follow-ons). Goal: identify the next
DC3-newer-than-RB3 mismatch like CamShot's 232-byte layout skew, ranked by
unlock potential.

Data taken from `build/45410914/report.json` at HEAD `261b66a`
(`matched_functions: 1815 / 65,770`). Method described in the prompt; every
finding is backed by an objdiff sample (binary-anchored) plus a source/header
cross-reference. Confidence labels are conservative.

## TL;DR ranking

| # | Lever | Confidence | Unlock estimate | Effort |
|---|---|---|---|---|
| **1** | **`StlNodeAlloc::deallocate` calls `MemOrPoolFreeSTL` with debug args** (we have 5-arg signature with `__FILE__`/`__LINE__`/`name`; target compiled with 2-arg `_MemFree(size, ptr)` direct path) | HIGH | **~53 fns**, 24 units (any near-miss STL container method) | EASY — one header edit (`src/system/utl/StlAlloc.h` lines 75-84); same fix kills both `allocate` and `deallocate`. **Smoke-test risk:** rb3-Wii uses 3-arg `_MemOrPoolFreeSTL(size, FastPool, ptr)`. Wrong pool tag = corrupted memory. Need to verify retail's allocator dispatcher signature first. |
| **2** | **`LightPreset::EnvironmentEntry` / `EnvLightEntry` / `SpotlightEntry` / `Keyframe` struct sizes diverge from retail** (DC3 added/removed fields; rb3-Wii has the original) | HIGH | **~17 fns** in LightPreset alone + downstream callers (LightPresetManager TBD) | MEDIUM — port struct layouts from rb3-Wii; need careful Ghidra struct dump because rb3-Wii's sizes also don't match retail (RB3 retail is BIGGER than both — Keyframe is 0xE0 in retail vs 0x6C rb3-Wii vs 0xAC ours; EnvironmentEntry is 0x68 in retail vs 0x14 rb3-Wii vs 0x2C ours). |
| **3** | **`TheNgStats->mMats++` and other NgStats counters in source that retail RB3 stripped** | MEDIUM (1 example confirmed across 7 `Select`s, 23 source touches in 7 files) | **~7-15 fns** across Shader + Cam + Rnd_Xbox + Env_NG + Rnd_NG + VelocityBuffer + SpotlightDrawer | EASY — gate with `#ifdef NGSTATS_DEBUG` or delete. Affects only Xbox/360 rendering path. |
| **4** | **`SAVE_REVS`/`INIT_REVS` save-format version mismatches** (Part = `0x29` vs target `0x25`; MatAnim = `7` vs target `4`; save schemas diverge too — base writes `Key<Color>`/`Key<float>`, target writes `Key<Vector3>`/`int`/`Key<Quat>`) | MEDIUM (4 confirmed Save/Load methods 50-99%; 489 REVS markers exist project-wide and many likely match DC3 not RB3) | **~10-30 fns** project-wide (per-class Save+Load+ctor+SetFrame all touch these literals) | MEDIUM-HARD per class — need to enumerate retail RB3 rev for each class via Ghidra, then port Wii's older Save body for the ones that changed schema. Only worth doing 1-2 high-value classes (Part, MatAnim) opportunistically. |
| **5** | **MSVC C++ EH funclet emission asymmetry** (dtk emits target-side catch handlers as separate `fn_` entries; our base side bakes them into the parent. Result: 257 fns @ 99.85-100% across 55 units that *cannot* hit 100%) | HIGH (dtk SPLIT behavior, NOT a source-side fixable bug — DC3 has the same issue at smaller scale) | **0 fn unlock** but currently masks the unit-level fuzzy% by ~10-15pp on EH-heavy units (MidiParser unit: 70/148 at 100, 35 stuck at 99.9%) | NOT FIXABLE from source. Action item: **classify these in scoring** (target-only funclets should not count against unit). Either patch objdiff scorer or filter `fn_` entries with the `subi r31, r12, 0xXX` prologue out of report. |

The non-Levers (tested and rejected):
- **`Geo::Intersect` / `BSPFace::Update`** (8 near-misses, 80-94%) — pure
  reg-alloc + math-reorder issues. AtLimit class. **Skip.**
- **`MemTracker::MemDiffEntry` STL templates** (6 near-misses 56-82%) — looks
  like `mHeap` is `int` in source but compared as 64-bit pair in target. Could
  be a `__int64` typedef divergence; would need Ghidra struct dump to confirm.
  Touches only `MemTracker.cpp`. **Defer — too narrow.**
- **`CharClip::InGroups` / `OnHasGroup`** (78-83%) — `ObjRefOwner::RefOwner()`
  virtual-call asymmetry. The `OBJREF_VIRTUAL` macro is already correctly empty
  for the X360 build (per prior decomp work). Target's virtual call is on a
  *different* object than what source suggests. **Skip — likely
  iterator-codegen, not struct.**
- **`Shader.cpp` `ShaderType` enum values diverge** (`kMultimeshShader` = 12
  ours, 18 retail) — real lever, but only 7 affected fns and **part of Lever 3
  blast radius** (RndShader::Select is the diff source for both NgStats and
  enum value). Fix Lever 3 first; re-measure ShaderType impact. Listed inline
  in §Lever 3.

---

## Background data

**Survey scope:** 836 near-misses (80-99.9% normalized) in the whole binary.
After excluding `Band*` / `Cam*` units = **790 outside CamShot blast radius**.
Of those:
- **257** are 40-48 byte catch-handler funclets at 99.85-100% (Lever 5 — not
  source-fixable).
- **212** are named (demangled) functions in the 50-99% band — the real
  structural pool.

**Per-unit density (top 15, named near-misses 50-95%, non-CamShot):**

| count | unit |
|---|---|
| 17 | `default/LightPreset` ← Lever 2 |
| 8 | `default/Geo` |
| 8 | `default/Shader` ← Levers 3+4 |
| 7 | `default/mdct` |
| 7 | `default/CharClip` |
| 6 | `default/EventTrigger` ← Lever 1 (list<Anim>/list<HideDelay>) |
| 6 | `default/MemTracker` |
| 5 | `default/MidiParser` |
| 4 | `default/psy` |
| 4 | `default/BlockMgr` ← Lever 1 (list<BlockRequest>/list<AsyncTask>) |
| 4 | `default/Part` ← Lever 4 |
| 3 | `default/Console` ← Lever 1 (list<Breakpoint>) |
| 3 | `default/LightHue` |
| 3 | `default/Archive` |
| 3 | `default/Rnd_Xbox` ← Lever 3 |

---

## Lever 1: `StlNodeAlloc` allocator signature divergence

### Class/TU
- File: `src/system/utl/StlAlloc.h` (lines 63-84)
- Affects every container method that touches `StlNodeAlloc::allocate` or
  `::deallocate` — vector, list, map, deque, set across the engine.

### Symptom
Consistent pattern across `_List_base::clear`, `_M_node_alloc`,
`_M_allocate_and_copy`, `_M_insert_overflow_aux` near-misses:

```
TARGET:  li r3, 0x24
         bl  fn_82798278           # MemFree(size, ptr) — 2 args
BASE:    li r4, 0x0                # extra inserted instructions
         mr  r4, r29
         bl  ?deallocate@?$StlNodeAlloc<...>@@QBAXPAU<...>@@I@Z  # name + 5 args
```

Stable in EventTrigger (`list<Anim>` and `list<HideDelay>` both 75.8% sz=76),
BlockMgr (`list<AsyncTask>`, `list<BlockRequest>`), Console
(`list<Breakpoint>`), Gen (`list<RndGenerator::Instance>`). Also drives the
~10pp gap on every `vector<T>::_M_allocate_and_copy` outside LightPreset.

### Hypothesis
Our `StlAlloc.h` (inherited from DC3) defines:
```cpp
void deallocate(pointer ptr, size_type count) const {
    int size = count * sizeof(T);
    const char *name = gStlAllocNameLookup ? typeid(pointer).name() : gStlAllocName;
    MemOrPoolFreeSTL(size, ptr, __FILE__, 0x40, name);  // 5-arg debug form
}
```

rb3-Wii's `StlAlloc.h` is much simpler:
```cpp
void deallocate(pointer ptr, size_type count) const {
    _MemOrPoolFreeSTL(count * sizeof(T), FastPool, ptr);  // 3-arg
}
```

Retail RB3 looks like it inlined the deallocator to a **2-arg
`MemFree(size, ptr)` direct call** — `fn_82798278` is a 2-arg dispatcher
(`if (size > 0x80) Function_82797AA0(ptr); else Function_82795DA0()` — i.e.
"large-vs-pool free").

### Confidence
**HIGH (binary evidence + cross-source corroboration)**:
- Target asm consistently shows 2-arg call signature with `r3=size, r4=ptr`
  and immediate `li r3, <N>` before the call (decompiled
  `fn_82798278` confirms it's pool-aware free).
- Both rb3-Wii and our build differ from retail (rb3-Wii has 3-arg,
  we have 5-arg) — but rb3-Wii's is structurally closer. The dc3-newer-than-RB3
  caveat in CLAUDE.md applies directly here: dc3-decomp added the
  `__FILE__`/`__LINE__`/`name` debug parameters that retail lacked.

### Verification step before editing
Decompile `fn_82798278` from Ghidra (already done — confirmed 2-arg
size+ptr signature). Then decompile one of the *target's* concrete
`StlNodeAlloc::deallocate` instantiations (if any escape inlining) to see
exactly what wrapper sits between vector/list and `fn_82798278`. If retail
inlines all the way and just calls `MemOrPoolFree(size, ptr)` directly,
**replace the whole function body** with:

```cpp
void deallocate(pointer ptr, size_type count) const {
    MemOrPoolFree(count * sizeof(T), ptr);
}
```

where `MemOrPoolFree` is rb3-Wii's simpler dispatcher. (Don't add `name` arg
even though the macro exists — retail stripped it.)

### Estimated unlock
- **53 named near-miss fns** in the 50-99% band across **24 units**.
- Plus probable improvement on many `_M_allocate_and_copy` and `_List_base`
  fns that are currently at 84-95% (the allocate side has the same pattern).
- **Bounded estimate: +20 to +45 matched fns** depending on how many also have
  secondary issues (struct size, Save schema). Some, like the LightPreset
  Keyframe vector ops, will need BOTH this fix and Lever 2.

### Effort tier
**EASY (1 header edit + rebuild)**. Same fix pattern as the
`OBJREF_VIRTUAL` macro from earlier work — define a behavioral toggle, ship
the simpler retail variant. The only risk is the smoke-test: if retail's
deallocator dispatcher (FastPool? heap?) differs from rb3-Wii's, wrong tag =
wrong-pool free = memory corruption at runtime. **Must verify pool routing
via Ghidra** (is `fn_82798278` the only deallocator entry, or are there
per-pool variants?) before committing.

### References
- `~/.local/bin/objdiff-cli diff --include-instructions -1 build/45410914/obj/EventTrigger.obj -2 build/45410914/src/system/rndobj/EventTrigger.obj '?clear@?$_List_base@UAnim@EventTrigger@@V?$StlNodeAlloc@UAnim@EventTrigger@@@stlpmtx_std@@@stlpmtx_std@@QAAXXZ'`
- Source: `src/system/utl/StlAlloc.h:75-84`
- rb3-Wii reference: `~/code/milohax/rb3/src/system/utl/StlAlloc.h:85-87`
- Ghidra: `FUN_82798278` = retail's `MemFree(size, ptr)` dispatcher
  (decompiled via `tools/ghidra/ghidra-decompile.py --binary
  "/default.xex-35adb6" "FUN_82798278"`)
- Project memory: this is the same kind of fix as
  `Hmx::Object 0x28 layout via OBJREF_VIRTUAL` and
  `KeyChain namespace not class` — header-level retail/debug-form
  divergence with concentrated payoff.

---

## Lever 2: `LightPreset` sub-struct layouts diverged in DC3

### Class/TU
- `src/system/world/LightPreset.h:25-145` (inherited from DC3 with our
  ObjVector/ObjPtr conversions)
- 17 named near-misses 50-95% in `default/LightPreset` unit (top of the table).
- **Also touches LightPresetManager** (not yet measured separately — same
  struct types).

### Symptom
Target compiled with **larger** sub-structs than ours:
| struct | rb3-Wii | DC3/ours | retail (target) |
|---|---|---|---|
| `EnvironmentEntry` | 0x14 (20) | 0x2C (44) | **0x68 (104)** |
| `EnvLightEntry` | 0x28 (40) | 0x48 (72) | TBD via Ghidra |
| `SpotlightEntry` | 0x20 (32) | 0x44 (68) | TBD |
| `Keyframe` | 0x6C (108) | 0xAC (172) | **0xE0 (224)** |

Direct evidence:
- `_M_allocate_and_copy<EnvironmentEntry>` target stride =
  `mulli r3, r4, 0x68` (104), base = `mulli r3, r4, 0x2c`
- `_M_insert_overflow_aux<Keyframe>` target =
  `li r10, 0xe0`, `mulli r11, r23, 0xe0` (224); base = `0xb0` (176)
- `OnViewKeyframe` target = `mulli r10, r10, 0xe0`; base = `0xb0`

### Hypothesis
**RB3 retail has additional fields** in every keyframe sub-struct that BOTH
rb3-Wii AND DC3 stripped, but in opposite directions:
- rb3-Wii dropped them (Wii memory budget? engine simplification?).
- DC3 added different fields than retail (Dance Central had different
  staging needs).

The CamShot pattern repeats: **neither rb3-Wii nor DC3 is correct for retail**;
retail sits *between or beyond both*. Need to derive from Ghidra struct
analysis on the retail binary directly.

### Confidence
**HIGH for the symptom (size mismatch is bit-stable in 3 separate
functions), MEDIUM-LOW for the fix shape**. The 84% match pattern on
`_M_allocate_and_copy` of *three different sub-structs at the same pct, same
size* is the strongest "shared struct layout" signal in the whole near-miss
table — it pattern-matches CamShot's signature exactly.

### Estimated unlock
- 17 named near-misses in LightPreset (50-95% band)
- Plus ~20 vector-template near-misses in the 99-99.9% band that will close
  once Keyframe stride is correct
- LightPresetManager likely benefits too (shares all 4 types)
- **Bounded estimate: +20 to +50 matched fns**. Same blast-radius profile
  as CamShot's 4+long-tail unlock.

### Effort tier
**HARD (similar to CamShot port)**. The retail struct fields need to be
reverse-engineered from Ghidra — neither rb3-Wii nor DC3 has them. Expected
workflow:
1. Run `python3 tools/ghidra/ghidra-decompile.py` on `LightPreset::Keyframe`'s
   ctor and `LightPreset::CacheFrames` (already done — see Ghidra output
   pinpointing fields at offsets `+0x60` `mDuration`, `+0xb0`/`+0xb4`
   accumulators, `+0xb8` cached time, etc.).
2. Reconstruct each sub-struct's full field list (offsets + types) from
   asm patterns at known callsites (`OnViewKeyframe`, `ApplyState`,
   `AnimateState`, `Animate`).
3. Update headers, port `Save`/`Load` to match retail rev (Lever 4 likely
   applies here too).
4. Smoke test: load an in-game lights .milo asset via the native engine and
   confirm rendering doesn't break.

### References
- objdiff: `OnViewKeyframe` 79.5%, `_M_insert_overflow_aux<Keyframe>` 84%,
  `_M_allocate_and_copy<EnvironmentEntry>` 83.4%, `_M_allocate_and_copy<SpotlightEntry>` 84%, `_M_allocate_and_copy<EnvLightEntry>` 84%
  (all in the cluster `('default/LightPreset', 83.9, 100)` — 3 vector ops
  at the *exact* same pct% same size, a fingerprint of shared struct skew).
- Headers: `src/system/world/LightPreset.h:25-145` (ours),
  `~/code/milohax/rb3/src/system/world/LightPreset.h:25-145` (rb3-Wii),
  `~/code/milohax/dc3-decomp/src/system/world/LightPreset.h:25-160` (DC3,
  matches ours).
- Ghidra: `?CacheFrames@LightPreset@@IAAXXZ` decompiles to show stride 0xE0
  for `mKeyframes` and inner vector strides 0x58 / 0x2C / 0x68 / 0x10.
- **Parallel agent note:** the `camshot-port` worktree is doing this exact
  workflow for `CamShot`. Pick LightPreset up *after* CamShot lands so the
  decomp workflow + Ghidra-driven struct reconstruction template is proven.

---

## Lever 3: `TheNgStats` debug instrumentation in source, stripped in retail

### Class/TU
- 23 occurrences across 7 source files:
  - `src/system/rndobj/Shader.cpp` (12 — RndShaderSimple, Particles,
    Multimesh, Standard, PostProc, DrawRect, UnwrapUV, Velocity,
    VelocityCamera, DepthVolume, Fur, SyncTrack)
  - `src/system/rnddx9/Cam.cpp` (1)
  - `src/system/rnddx9/Rnd_Xbox.cpp` (1)
  - `src/system/rndobj/Env_NG.cpp` (4)
  - `src/system/rndobj/Rnd_NG.cpp` (1+)
  - `src/system/rndobj/VelocityBuffer.cpp` (1)
  - `src/system/world/SpotlightDrawer.cpp` (3)

### Symptom
Every `RndShader*::Select` method has 5 inserted instructions on the base
side that read+increment+writeback `TheNgStats->mMats` (offset 0x18):

```
INSERT (base only): lis r11, ?TheNgStats@@3PAVNgStats@@A
                    lwz r11, ?TheNgStats@@3PAVNgStats@@A, r11
                    lwz r10, 0x18, r11
                    addi r10, r10, 0x1
                    stw r10, 0x18, r11
```

Target has zero NgStats references in these `Select` methods. Source code in
`Shader.cpp` is unconditional `TheNgStats->mMats++` — no `#ifdef` guard.

### Hypothesis
DC3 (where we inherited `Shader.cpp` from) kept the NgStats counters as live
profiling code. Retail RB3 either:
(a) Built with NgStats stripped via a release-build `#ifdef` we don't have,
(b) Removed the counters in source for the retail branch.

Note: `TheNgStats` itself **does exist** in the retail binary (Ghidra finds
the global symbol), so this isn't a "global pointer doesn't exist" issue —
just unconditional-vs-conditional increment.

### Confidence
**MEDIUM-HIGH for source-side fix scope (cleanly localised); MEDIUM for
which mechanism retail used**. The 7 `RndShader::Select` methods all show
the same insertion pattern, which is a strong "shared upstream cause" signal.
But we can't tell from the binary alone whether retail removed source-side or
preprocessor-gated. Either way, the source edit is `// TheNgStats->mMats++;`
or wrapped in `#ifdef NGSTATS_DEBUG`.

### Estimated unlock
- **~7-15 fns**. The 7 RndShader::Select methods (currently 71-93%) should
  jump to 90-99%. Plus possible knock-on improvements in the other 16 NgStats
  callsites (Cam, Env_NG, etc. — measure separately).
- Several won't hit 100% because of the **ShaderType enum value drift**
  (kMultimeshShader = 12 ours vs 18 retail) — that's a separate, smaller
  edit that pairs naturally with this fix.

### Effort tier
**EASY (mechanical mass-edit)**. Bracket the 23 sites with
`#ifdef NGSTATS_DEBUG` or delete them outright. The `#ifdef` form is
preferable because it preserves intent and is reversible. Add `NGSTATS_DEBUG`
to `tools/defines_common.py` so it can be selectively enabled for
development builds.

For the ShaderType enum drift: derive correct values from a few well-known
calls (`Cache(0x12, ...)` in `RndShaderMultimesh::Select` confirms
`kMultimeshShader = 0x12`, vs our 0xc). The whole enum needs re-ordering by
comparing the binary's `sShaderTypes[i] = "..."` callsites. ~30 min in
Ghidra.

### References
- objdiff: `?Select@RndShaderMultimesh@@MAAXPAVRndMat@@W4ShaderType@@_N@Z`
  71.1% — 5-instruction NgStats insertion at idx 29-36.
- Sources: `src/system/rndobj/Shader.cpp:282`,1235,1247,1262,1284,1297,1312,1328,1341,1355,1386,1401
- DC3 reference: `~/code/milohax/dc3-decomp/src/system/rndobj/Shader.cpp:277,1232,...` (same pattern; we inherited verbatim).
- rb3-Wii reference: N/A (Wii uses `rndwii/`, doesn't have `Shader.cpp`).
- Ghidra: `?TheNgStats@@3PAVNgStats@@A` exists as a global in retail
  binary — the symbol resolves, so this isn't a missing-extern issue.

---

## Lever 4: `INIT_REVS`/`SAVE_REVS` and Save-schema drift (per-class)

### Class/TU
- 489 `SAVE_REVS`/`INIT_REVS`/`LOAD_REVS` markers across `src/`. Specific
  confirmed mismatches:
  - `src/system/rndobj/Part.cpp:332,598`: ours `0x29` (41), target writes
    `0x25` (37)
  - `src/system/rndobj/MatAnim.cpp:46,74`: ours `7`, target writes `4`

### Symptom
On `RndParticleSys::Save` (77.4%, 1124 bytes):
```
TARGET:  li r11, 0x25    # save rev = 37
BASE:    li r11, 0x29    # save rev = 41
```

AND a complete **schema reorder/retyping** between the two — target serializes
`Key<float>` lists where we serialize `Vector2` lists; target has additional
fields between known anchors (stack frame is 0x368 vs 0x3b0 — 72 bytes
difference, but inverted from typical struct growth).

Similarly `RndMatAnim::Save` (78.6%): target writes `Key<Vector3>` then
`int`-list then `Key<Quat>`; base writes `Key<Color>` then `Key<float>` then
`Key<Vector3>`. **Entire animation-channel order changed.**

### Hypothesis
DC3 extended the Save formats with additional features (per-channel
versioning, extra anim types). RB3 retail = older format. Each class needs:
1. Set REV literal to the retail value (binary tells us exactly).
2. Comment out / `#ifdef DC3_REV_FORMAT` the new fields in Save body.
3. Ditto Load body (mirror the Save changes).
4. Likely shrink the struct field set too (e.g., `RndMatAnim` may have fewer
   keyframe lists in retail).

### Confidence
**HIGH on the existence of the divergence** (4 Save methods confirmed in the
50-99% band; literal REV values bit-stable). **LOW on the per-class fix
complexity** — each class's Save body may need 5-50 lines edited, plus the
struct's field set may shrink. Doing this for all 489 markers without
Ghidra-assisted verification per class would be expensive and error-prone.

### Estimated unlock
- ~10-30 fns project-wide if all REVS that diverge get aligned. Realistically,
  pick 2-3 high-value classes (Part, MatAnim, MeshAnim).
- **Per-class:** 1 Save + 1 Load + 1 ctor + 1 SetFrame ≈ 4-8 fns each.

### Effort tier
**MEDIUM (per class) — HARD if attempting all 489 markers**. Most of the cost
is per-class verification of the retail Save schema via Ghidra
`Save@<Class>@@UAA...`. Recommended: do Part and MatAnim opportunistically as
part of other work (e.g., when porting LightPreset for Lever 2 — Save touches
Keyframe sub-structs too, so the work compounds). **Do not attempt as a
standalone wave.**

### References
- objdiff: `?Save@RndParticleSys@@UAAXAAVBinStream@@@Z` (77.4%),
  `?Save@RndMatAnim@@UAAXAAVBinStream@@@Z` (78.6%),
  `?Save@SampleZone@@QBAXAAVBinStream@@@Z` (93.7%),
  `?Save@SampleData@@QBAXAAVBinStream@@@Z` (78.3%).
- Sources: `src/system/rndobj/Part.cpp:332,598`,
  `src/system/rndobj/MatAnim.cpp:46,74`.

---

## Lever 5: dtk SPLIT emits per-funclet `fn_` entries (not source-fixable)

### Class/TU
- jeff `dtk` (the local fork at `../jeff/`), specifically the
  `.text` section emission path.
- 257 affected near-miss "functions" across 55 units — every catch-handler
  funclet that retail's MSVC emitted gets a separate `fn_82xxxxxx` symbol.

### Symptom
Every C++ method compiled with `/EHsc` that has any non-trivial-destructor
auto produces multiple small (32-48 byte) catch-handler funclets. Their
prologue is uniformly:
```
subi r31, r12, 0xXX     # restore parent frame pointer via funclet pivot
mflr r12
stw  r12, -0x8(r1)
stwu r1, -0x60(r1)
... 1-3 instructions of body (e.g., bl <destructor>) ...
addi r1, r1, 0x60
lwz  r12, -0x8(r1)
mtlr r12
blr
```

Our base side **never emits these as separate symbols** (MSVC bakes them
into the parent's `.text$<comdat>` section per-COMDAT). dtk dumps them with
synthetic `fn_<addr>` names, which then appear in the report as functions
with 99.85-99.99% normalized match but no base-side equivalent — a fixed
~10pp drag on unit fuzzy%.

DC3 has the same problem at smaller scale (it has 174 .fn entries in
target asm vs 71 in its report — the report apparently filters funclets
elsewhere). The asymmetry between our 148-entry MidiParser unit (35
funclets in the report) and DC3's 71-entry MidiParser unit (0 funclet
near-misses) is the smoking gun.

### Hypothesis
Either:
- **jeff** emits the catch funclets as named .fn entries when dc3-tk did not
  (we should suppress them — they're not user functions),
- **objdiff** counts them and DC3's report.json ingestion strips them
  (we should mirror the filter in `scripts/ingest_report.py` or `tools/project.py`),
- Both.

### Confidence
**HIGH on the problem identification** (binary asm shows funclets in both
target objs; DC3's report has 71 fn entries for MidiParser, ours has 148;
the *extra* 77 are all 32-48 byte funclets at 99.9%).
**MEDIUM on which tool is the right place to fix it**.

### Estimated unlock
**Zero matched_function count change**. But the unit-level fuzzy% improves
dramatically: MidiParser would go from 35.5% → 47.3% on `matched_functions_percent`
alone (the 77 funclets are 47% of the 148 entries). Across 55 affected units
× ~5 funclets each = ~275 false-near-misses cleaned up. Removes the noise from
near-miss surveys (this very recon would have surfaced the real structural
clusters faster).

### Effort tier
**MEDIUM (jeff/dtk patch in Rust)**. The funclet detection is mechanical:
any `.text` symbol whose first instruction is `subi r31, r12, 0xXX` (the
SLEIGH-decodable `addi r31, r12, -<imm>`) AND whose total size is ≤ 64 bytes
AND ends in `blr` is a funclet. Either:
- Emit them as `.fn ... funclet` (a new local symbol kind) so they show up
  as data in objdiff, not as functions,
- Don't emit them as standalone symbols at all (fold into parent's COMDAT).

Or, lower-effort: modify `scripts/ingest_report.py` to filter `fn_*` whose
size ≤ 64 and pattern matches in the asm. Cleaner with full dtk fix.

### References
- objdiff: see any of the 257 fns in MidiParser (`fn_827C1D04`,
  `fn_827C1D2C`, ...) — all have `Base Size: 0` (no base equivalent).
- Source `build/45410914/asm/MidiParser.s:1747-1759` (sample funclet).
- DC3 report: MidiParser unit has 71 fns total (compare to ours' 148 —
  difference is funclets).
- jeff source: `~/code/milohax/jeff/src/` — section emission path.

---

## Appendix: top 20 LOWEST-pct non-CamShot named near-misses (50-99%)

These are the functions farthest from 100% that still have a real demangled
name. Useful as a hand-pick list if a future pass wants individual targets.

| pct | size | unit | demangled (truncated) |
|---|---|---|---|
| 52.9% | 68 | StringTable | StringTable::UsedSize() const |
| 56.2% | 128 | Synth | vector<XAUDIO2_SEND_DESCRIPTOR>::_M_allocate_and_copy |
| 56.3% | 264 | MemTracker | __unguarded_partition<MemDiffEntry*, ...> |
| 56.7% | 120 | Jukebox | __find<JukeboxItem*, int> |
| 57.6% | 1200 | FFT | fft_matrix_forward_columnwise |
| 58.7% | 1328 | StreakMeter | StreakMeter::SyncObjects() — note: StreakMeter is bandobj, will be touched by CamShot port |
| 59.2% | 284 | CharBoneOffset | CharBoneOffset::CharBoneOffset() |
| 60.6% | 128 | Console | RndConsole::Breakpoint::~Breakpoint — Lever 1 candidate |
| 61.4% | 5856 | SHA1 | CSHA1::Transform — almost certainly codegen, skip |
| 61.7% | 212 | MemTracker | __push_heap<MemDiffEntry*, ...> |
| 63.6% | 88 | BustAMoveData | operator>><BAMPhrase, ...> — Lever 1 + 4 |
| 64.4% | 124 | MultiTempoTempoMap | MultiTempoTempoMap::PointForBeat |
| 65.7% | 360 | BoxMap | BoxMapLighting::ApplyLight |
| 66.1% | 132 | Part | RndParticleSys::OnExplicitParts — Lever 4 adjacent |
| 66.7% | 72 | CacheMgr | CacheMgr::CreateCacheMgr |
| 67.8% | 248 | mtx | Multiply(Transform, Transform, Transform) — codegen, skip |
| 67.9% | 280 | bitwise | (unnamed wrapper) |
| 68.8% | 204 | Rnd_Xbox | DxRnd::SavePreBuffer — Lever 3 adjacent |
| 69.0% | 228 | MemTracker | __introsort_loop<MemDiffEntry*, ...> |
| 69.2% | 112 | Gen | RndGenerator::OnSetPath — Lever 1 + 4 |

---

## Recommended execution order

1. **Lever 5 first** (jeff or ingest_report filter) — cleans up the recon
   signal and stops wasting agent time on un-fixable funclets.
2. **Lever 1** (`StlNodeAlloc::deallocate` header edit) — one file change, broad
   effect across STL containers. Highest yield-per-effort.
3. **Lever 3** (NgStats `#ifdef` + ShaderType enum re-derive) — 23 small edits
   in 7 files. Self-contained. Run in parallel with Lever 1 (different files).
4. **Lever 2** (LightPreset sub-struct port) — wait for `camshot-port` to land
   so the struct-reconstruction methodology is proven; then apply the same
   workflow to LightPreset's 4 sub-structs.
5. **Lever 4** (per-class Save schema port) — only opportunistically; pair with
   Lever 2 when the LightPreset port also needs Save fixed for the new layout.

If only ONE lever can be done: **Lever 1**. It's the cheapest, lowest-risk,
broadest-impact item in the table.
