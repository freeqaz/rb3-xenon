# Codegen-Iteration Targets (Post-Stage-3)

State: 290 matched / 66,102 fns (0.44%) across 338 report units. 69 TUs are
"partial match" (some fns match, some don't). This doc ranks those 69 TUs by
yield-per-effort for a codegen-iteration agent, surfaces the recurring residue
patterns, and gives an honest ceiling estimate.

`matched_functions` counts NORMALIZED match == 100% — i.e. tolerant of
relocation-name noise but **not** of register-swaps, LINKER_MERGED ICF, or
data-relocation differences. So the counter ticks only when bytes match modulo
reloc names.

## Headroom snapshot

- 69 partial-match TUs, 1450 fns total, 266 matched, 1184 unmatched.
- 215,388 unmatched bytes in partial TUs. **But 157,388 (73%) of that lives in
  1,015 functions at 0% match** — most of those are big, structurally-different
  ports (no source iteration will help). The interesting work is in the ~169
  fns at 1–99.99%, totalling ~58,000 bytes.

Band breakdown of the 1,184 unmatched fns in partial TUs:

| band       | count |    bytes |
|------------|------:|---------:|
| 0%         |  1015 |  157,388 |
| 1–50%      |    37 |    5,828 |
| 50–80%     |    24 |   10,148 |
| 80–95%     |    50 |   17,540 |
| 95–99%     |    25 |   11,380 |
| 99–99.99%  |    33 |   11,004 |

## Recurring residue patterns (from ~30 objdiff-cli samples)

The diff residue in our near-misses is dominated by a few patterns. Many "bug"
fixes would tick multiple functions across a TU:

1. **LINKER_MERGED / ICF (UNFIXABLE).** The target linker merged identical
   small comdat funs (often STL helpers, `__cdecl` stub bodies). Shows up
   everywhere `list<T>` / `vector<T>` member-template instantiations are
   involved (BlockMgr 6 fns, Gen 2 fns, DepthBuffer3D 4 fns, json_object 2,
   sharedbook, codebook, inflate 18 calls). **Cannot fix from source.**
2. **ADDRESS_RELOCATION_NOISE (UNFIXABLE).** lis/addi data refs that target a
   different but equivalent globals. Already counted as "noise" in fuzzy match
   but the normalized counter still rejects. Endemic to any function with
   string/static refs (Instance, UIScreen, LightHue, MasterAudio, info, Gen).
3. **REGISTER_SWAP (mostly UNFIXABLE).** Volatile-register pairings (f0↔f13,
   f10↔f11, r10↔r11) where the optimizer picked the alternate. Floating-point
   shows up heavily in Rot, Geo, LightHue. Callee-saved swaps (r29↔r30,
   r30↔r31) are MaybeFixable but usually mean a local declaration-order tweak.
4. **CONTROL_FLOW illegal↔bl (LIKELY FIXABLE).** Recurring pattern in any TU
   where dtk hasn't pinned/identified the BL target — shows as `<illegal>`
   versus a real `bl`. **This is a renamer / target_symbol_map gap, not a
   codegen diff**; not actionable from source.
5. **OFFSET_SWAP small (LIKELY FIXABLE, single-fn).** `(0x4,0x8)` shows up in
   Rot.cpp `MakeScale`/`Set`/`Multiply` — Vector3 `y`/`z` access order in our
   C++ doesn't match target's ordering. Source reassociation may fix; pure
   "swap two statements" experiments.
6. **PROLOGUE_MISMATCH (UNFIXABLE-from-source).** Target prologue saves
   r24-r31, ours r25-r31 (one extra local). Means we have a different
   local-variable lifetime than the original. In small fns sometimes coaxable
   by re-rolling source loops; in large fns (sharedbook 407 ins, codebook,
   mdct_init) it's path-of-least-resistance to accept.
7. **BOOL_MASK / ALLOCA_MISMATCH (rare).** One BOOL_MASK in `inflate`; one
   ALLOCA_MISMATCH in `vorbis_book_decodevs_add`. Pattern docs exist; one-line
   fixes when they appear, but not common.

## Top TUs ranked by yield-per-effort

Categories below: H/M/L = fixability of the *near-miss* residue (90-99% band),
not the whole TU.

### Tier S: high yield, near-trivial source edits (priority)

| TU (path)                              | matched / total | near-miss fns | dominant residue                |
|----------------------------------------|----------------:|--------------:|---------------------------------|
| Rot (math/Rot.cpp)                     |  12/39          | 5 @ 96-99%    | OFFSET_SWAP(0x4,0x8) + reg-swap |
| Geo (math/Geo.cpp)                     |   9/39          | 4 @ 80-99%    | reg-swap + 1 OFFSET_SWAP        |
| Instance (world/Instance.cpp)          |   9/48          | 6 @ 99-99.9%  | almost all AtLimit (LINKER_MERGED + reloc-noise) — **except** 1-2 that may tick with renamer-side gap fixes |
| DepthBuffer3D (gesture/)               |   1/135         | 5 @ 99-99.9%  | LINKER_MERGED on `__uninitialized_copy/fill_n` — only ~1 of 5 likely tickable |
| Gen (rndobj/Gen.cpp)                   |   3/83          | 4 @ 95-99.9%  | LINKER_MERGED + reloc-noise; **`ResetInstances`/`ListDrawChildren` 99.8%** are the only viable |

### Tier A: medium yield, recurring fixable pattern

| TU                          | near-miss residue                                   |
|-----------------------------|----------------------------------------------------|
| keygen_xbox                 | `parseHex16` 98.6% volatile reg-swap, marginal; `random` 83% deeper changes |
| LightHue                    | `??0LightHue` 98.9%, `TranslateColor` 99.4% (1 OFFSET_SWAP 0x58,0x74 — single-fn fix possible) |
| BlockMgr (os/)              | 6 STL `list<T>::*` fns all stuck at 75-85% on LINKER_MERGED — uninfluenced by source |
| CacheMgr_Xbox               | both near-misses 99%+ LINKER_MERGED — AtLimit, not worth |
| framing (oggvorbis/)        | `ogg_sync_reset` 50% / `ogg_sync_clear` 57% — small, CONTROL_FLOW illegal↔bl is a *renamer-map gap* |
| floor1                      | 1 fn (`floor1_free_info`) at 99.94% — single instruction tweak, probably AtLimit |
| MasterAudio                 | `SetupTrackChannel` 99.99% AtLimit (linker reloc), `SetupBackgroundChannel` 95% — 1 fn possibly tickable |

### Tier B: low yield (skip unless chasing %)

`TexBlender`, `floor1`, `TDStretch`, `psy`, `mdct`, `inflate`, `sharedbook`,
`codebook`, `json_object`, `linkhash`, `printbuf`, `sprintbuf`, `AsyncFile`,
`JoypadClient`, `UIScreen`, `HDCache`, `ScrollSelect`, `Memory_Xbox`. All
either AtLimit (95-99% LINKER_MERGED+reloc-noise — see psy 99.4% AtLimit
verdict, MasterAudio 99.6% AtLimit, JoypadClient `SendRepeat` 99.5% AtLimit,
yy_create_buffer 96.2% AtLimit) or 0%-stuck with structurally-different code
that needs a real port, not iteration.

## 3-5 highest-yield specific targets

1. **Rot.cpp OFFSET_SWAP cluster — `MakeScale` / `Set(Quat,Vector3)` /
   `MakeRotQuat` / `Multiply(Vector3,Quat)`** (4 fns @ 94-99% normalized).
   Recurring `(0x4,0x8)` offset-swap means the source statement ordering of
   `y/z` writes diverges from target. **One-pattern fix could tick 2-4 fns at
   once.** Each fn ~120-260 B. Total possible yield: +4 functions, ~700 bytes.

2. **Geo.cpp + Rot.cpp `MakeEuler` (20% match) / `Multiply(Plane,Transform)`
   (40%) / `Intersect(Transform,Plane,Ray)` (22%)** — these have substantial
   CONTROL_FLOW residue suggesting **the dc3 source for these specific fns
   diverges from RB3's**. Not low-effort; need to cross-check rb3-Wii
   equivalents and pick the closer source. Possible +3 if the cross-check
   yields cleaner ports.

3. **DepthBuffer3D STL `_M_*` instantiations (5 fns @ 99.8-99.96%).** All
   blocked on LINKER_MERGED for the `__uninitialized_*` calls. If the target
   symbol map ([[matched_functions metric insight]]) can be extended so the
   merged-target symbols are recognised as equivalent to our compiled
   instantiations, **this single map update may tick 5 fns**. Yield-per-effort
   is the highest in the analysis if the renamer can be extended.

4. **LightHue::TranslateColor (99.4%)** — single `OFFSET_SWAP(0x58,0x74)`. One
   member-reorder experiment. +1 fn, 360 B.

5. **CSHA1::Update (95.4%)** — r29↔r30 callee-saved swap. Often fixable by
   re-rolling a `for` loop or moving a local-variable decl. +1 fn, 228 B.

## Ceiling estimate

Of the ~12,000 unmatched bytes in the **80-99.99% band** that look "near", I
estimate the **plausibly tickable subset is ~2,500-4,500 bytes / 8-15
functions** after excluding:

- AtLimit verdicts (psy, MasterAudio::SetupTrack, JoypadClient::SendRepeat,
  inflate, yy_create_buffer, vorbis_synthesis_headerin etc. — all explicitly
  flagged AtLimit by objdiff-cli).
- LINKER_MERGED clusters (BlockMgr 6 STL fns, Gen STL fns, DepthBuffer3D 5 STL
  fns — won't unstick from source alone; needs renamer/map work).
- Volatile-register swaps (f0↔f13, f10↔f11 across Rot/Geo) — confirmed
  Unfixable by pattern detector.

**The 99-99.99% band (11,004 bytes / 33 fns) is mostly LINKER_MERGED +
ADDRESS_RELOCATION_NOISE — i.e. AtLimit.** Don't chase that band; it will not
yield to source iteration. The 80-95% band (17,540 bytes / 50 fns) is where
the 8-15 plausible wins live, but each is a ~half-day-of-careful-source-edit
investment.

**Practical takeaway:** codegen-iteration on already-wired NonMatching TUs is
worth **+8-15 functions / ~3-5k bytes max**. That moves us from 290 to ~305.
The bigger lever post-Stage-3 remains **(a)** the renamer/target_symbol_map
extension to convert ~5 LINKER_MERGED-blocked DepthBuffer3D / BlockMgr clusters
into matches, and **(b)** heavy bandobj/band3 ports (Stage-3 BandDirector).

## Recommended order for the iteration agent

1. **Sonnet, single TU at a time (parallel-safe with build-lane mutex):**
   - Rot.cpp OFFSET_SWAP cluster (4 fns, one recurring pattern). Read the dc3
     source for each, try statement-order swaps for `y`/`z` writes, rebuild,
     check if multiple fns tick at once.
   - LightHue::TranslateColor offset-swap (1 fn).
   - CSHA1::Update local-reorder (1 fn).
2. **Opus, deep per-fn (only if (1) yields):**
   - Geo.cpp `MakeEuler` / `Multiply(Plane)` / `Intersect(Transform,Plane,Ray)`
     — cross-check dc3 source vs rb3-Wii source, pick the variant closer to
     target asm. These are 20-40% match — substantial code-shape differences.
3. **Defer / skip until renamer extended:**
   - All STL `_M_create_node` / `__uninitialized_*` / `_M_fill_insert` clusters
     (BlockMgr, Gen, DepthBuffer3D). They're blocked on LINKER_MERGED which is
     a target-side ICF issue, not source codegen. Fix via target_symbol_map.
4. **Stop chasing %:**
   - All AtLimit fns (objdiff-cli verdict says so). Investing here is
     mathematically zero-yield.

**Model split:** Sonnet handles Tier S/A items where the residue is a known
pattern (OFFSET_SWAP, simple reg-swap, declaration reorder) — they're
mechanical edits with fast feedback. Opus only on Geo/Rot's MakeEuler/Intersect
class — those need cross-referencing dc3 vs rb3-Wii source to pick the right
base, and the diff is large enough that pattern-matching won't help.

**Build-lane mutex still applies** — single ninja writer; codegen-iteration is
intrinsically build-bound (every experiment = ninja + objdiff-cli check), so
parallelization across TUs gains little. Run one agent serially.
