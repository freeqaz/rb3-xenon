# Porting Wave 1 — prioritized next ~12 TUs that yield matches

**Baseline:** `matched_functions: 24` (`build/45410914/report.json`). All 24 are
Object-INDEPENDENT leaf code (Rot 8, Geo 7, SHA1/mtx/Key/Rand2/FileChecksum, +
MasterAudio's 2 arg-struct wrappers). The pattern is proven: **leaf / non-Object
code matches readily; member-accessing `Hmx::Object` subclasses are blocked by
the 0x2c→0x28 layout bug** (`docs/plans/hmx-object-layout.md`).

**Pipeline per TU:** wire into `objects.json` (`NonMatching`) → pin `.text` in
`splits.txt` → `gen_target_map` renames the cluster's `fn_<addr>` targets →
`touch config.yml && ninja` → trivial fns register. Match-potential below =
count of bindiff records whose `rb3_addr` falls inside the proposed `.text`
range, all with trustworthy dc3 names (these become auto-gen renames).

## Surprise finding (changes the brief's framing)

The **Bucket B sources are already in `src/`** (copied from dc3 in a prior
session, byte-identical to dc3's `Matching` versions, but **never wired into
`objects.json` or `splits.txt`**). So Bucket B is mostly *wiring*, not porting.

Conversely, **Bucket A bandobj subclass TUs are NOT in `src/` at all** — the
entire `src/system/bandobj/` tree (headers + cpp) is missing; those .cpp files
live only in `../rb3` (rb3-Wii), with deep Rnd/world/Loader/Task transitive
deps. They are **heavy Wii→360 ports**, far costlier than the brief implied. The
already-present Object subclasses with pins are `Console.cpp` (RndConsole) and
`ShaderOptions.cpp` (both from dc3, already compiling) — those are the realistic
near-term Bucket A wins, plus engine bandobj from dc3 is unavailable.

## Ranked table

| TU | Tree | Bucket | Match-potential (bindiff / good-name in range) | In src? | Dep footprint | Cost | Model |
|----|------|--------|---|---|---|---|---|
| oggvorbis/framing.c | dc3 | B | 15 / 15 (94% density) | yes (identical) | self-contained `oggvorbis/`; only needs `-I oggvorbis` | trivial | Sonnet |
| oggvorbis/bitwise.c | dc3 | B | 9 / 9 (100%) | yes (identical) | same | trivial | Sonnet |
| oggvorbis/info.c | dc3 | B | 7 / 7 (100%) | yes (identical) | codec.h+codec_internal.h (present) | trivial | Sonnet |
| oggvorbis/sharedbook.c | dc3 | B | 10 / 10 (100%) | yes | self-contained | trivial | Sonnet |
| oggvorbis/block.c | dc3 | B | 10 / 10 (100%) | yes | codec_internal, window, registry (present) | trivial | Sonnet |
| oggvorbis/codebook.c | dc3 | B | 8 / 8 (100%) | yes | self-contained | trivial | Sonnet |
| oggvorbis/mdct.c | dc3 | B | 10 / 10 (100%) | yes | self-contained (math) | trivial | Sonnet |
| oggvorbis/psy.c | dc3 | B | 18 / 18 (90%) | yes | masking,lpc,smallft,scales (present) | trivial | Sonnet |
| utl/Pool.cpp | dc3 | B | 3 / 3 (100%) | yes (identical) | Pool.h (zero includes) | trivial | Sonnet |
| utl/VarTimer.cpp | dc3 | B | 6 / 6 (100%) | yes (identical) | os/Timer.h (present) | trivial | Sonnet |
| obj/DataFlex.c | dc3 | B | 5 / 5 (100%) | yes (identical) | DataFlex.h→DataFile_Flex.h; `<unistd.h>` absent (see risk) | trivial-mod | Sonnet |
| synth/DelayEffect.cpp | dc3 | B | 5 / 5 (100%) | yes (identical) | xdk/xaudio2/xaudio2.h (present), Decibels (present) | trivial | Sonnet |
| synth/FlangerEffect.cpp | dc3 | B | 3 / 3 (100%) | yes (identical) | same as Delay | trivial | Sonnet |
| synth/CompressionEffect.cpp | dc3 | B | 4 / 4 (100%) | yes (identical) | same as Delay | trivial | Sonnet |
| synth_xbox/FFT.cpp | dc3 | B | 12 / 12 (100%) | yes (identical) | FFT.h, vectorintrinsics.h (present) | trivial-mod | Sonnet |
| os/BlockMgr.cpp | dc3 | B | 21 / **good-name N/A** (was 10 emitted; 53% density) | yes (DIFFERS from dc3) | heavy: CDReader,Archive,Block,HDCache,MemMgr,System,DataFunc (all present) | moderate | Opus |
| rndobj/Console.cpp | dc3 | A | 5 / 3 | yes, **compiles** | already wired+pinned | (post-fix re-diff) | Sonnet |
| rndobj/ShaderOptions.cpp | dc3 | A | 4 / 2 | yes, **compiles** | already wired+pinned | (post-fix re-diff) | Sonnet |
| bandobj/BandCharacter.cpp | rb3 | A | 18 / 18 | **NO** (whole bandobj tree missing) | very heavy: Character, RndDir, CharDriver + ~30 headers | heavy | Opus |
| bandobj/BandDirector.cpp | rb3 | A | 13 / 13 | **NO** | heavy: RndPollable, RndDrawable, Group, PostProc, Loader, world/Dir, Task | heavy | Opus |
| bandobj/BandWardrobe.cpp | rb3 | A | 8 / 8 | **NO** | heavy (shares BandDirector dep set) | heavy | Opus |

(Density >100% in `proposed_splits_bindiff.txt` = multiple symbols per emitted
fn, e.g. CRT save/restore stubs — ignored here.)

## Bucket B — port NOW (matchable, not layout-gated)

These don't inherit `Hmx::Object`, so they're unaffected by the pending fix and
can land immediately, in parallel with the Object work. ~110 candidate match
records across the set; even at the leaf-fn hit-rate we've seen, this is the
single biggest near-term delta.

**Batch B1 — oggvorbis (ONE worker, serialize internally).** All 8 ogg files
share **one** include-path edit: add `"/I src/system/oggvorbis"` to
`tools/defines_common.py` (no `ogg.h` is resolvable today; dc3 has this path).
That shared edit means the ogg files must be wired by a **single agent** (they
all touch `defines_common.py` + `objects.json`). Sources are byte-identical to
dc3 `Matching`, so this is pure wiring + pinning. Pin ranges are spatially
clustered `0x82BF7CB0..0x82C066EC` and mostly disjoint — **resolve the one
overlap**: `smallft.c` (…DFF0–F574) vs `mdct.c` (…FF480–C006D4) overlap; pin
mdct from `0x82BFF574` or drop smallft from this wave. Highest yield in the
project: framing(15)+psy(18)+mdct(10)+block(10)+sharedbook(10)+codebook(8)+
bitwise(9)+info(7). **Model: Sonnet** (mechanical).

**Batch B2 — utl/obj/synth leaf TUs (ONE worker, disjoint from B1).** Pool,
VarTimer, DataFlex, DelayEffect, FlangerEffect, CompressionEffect, FFT. All
byte-identical to dc3 already in `src/`; deps present. Splits regions are
disjoint from each other and from ogg (`0x82748B90`, `0x827AEBE0`, `0x827B3FE8`,
`0x82B47B38`, `0x82B81B08..82554`). Touches `objects.json` only (no
`defines_common` edit). **Model: Sonnet.** Note Delay/Flanger/Compression share
one .text cluster `0x82B81B08..0x82B82554` — pin as a 3-file contiguous block.

**Batch B3 — BlockMgr.cpp (ONE worker, can overlap B1/B2).** Highest single-TU
count (21) but **moderate**: rb3-xenon's copy **DIFFERS from dc3** (already
hand-touched) and the include set is heavy (CDReader/Archive/HDCache/MemMgr —
all present, but real engine, not leaf). Range `0x82518970..82519BBC` disjoint.
Note one entry in-range is a non-BlockMgr `_M_create_node` (STL list) — expect
~10 BlockMgr fns to be the realistic matchers. **Model: Opus** (needs judgment
on the dc3-vs-local divergence; cross-check rb3-Wii per provenance caveat).

**Concurrency:** B1, B2, B3 run **fully concurrent** — disjoint source dirs
(`oggvorbis/` vs `utl/`+`synth/`+`obj/` vs `os/`), disjoint splits regions,
disjoint `objects.json` blocks. **Only B1 edits `defines_common.py`**; B2/B3 must
NOT touch it. If you'd rather avoid even that one shared file, have B1 land the
include-path edit first (a 1-line change), then B2/B3 start — but since they
don't touch that line, true concurrency is safe with a clean merge.

## Bucket A — after the Object fix lands (layout-gated)

The 0x2c→0x28 `Hmx::Object` fix (`docs/plans/hmx-object-layout.md`, runs SOLO)
**must precede** all of these — they all `#include "obj/Object.h"` and inherit
its layout. Once the layout is stable, the ports themselves parallelize.

**A1 — re-diff the already-compiling subclasses (no port needed).** `Console.cpp`
(RndConsole, 5/3 in range) and `ShaderOptions.cpp` (4/2) are already wired,
pinned, and compiling. Post-fix, simply `ninja` + `gen_target_map` and let
member-access fns register. **Zero new source work; Sonnet** to verify and
nudge. Do these **first** post-fix — they're the cheapest confirmation that the
−4 correction actually unblocks subclass member access.

**A2 — bandobj subtree port (rb3-Wii → 360).** BandCharacter (18), BandDirector
(13), BandWardrobe (8) are the highest-count Bucket A targets, but **the entire
`src/system/bandobj/` tree is absent** and these .cpp exist only in `../rb3`.
Each drags ~30 transitive headers (Character/RndDir/CharDriver for BandCharacter;
RndPollable/RndDrawable/Group/PostProc/Loader/world::Dir/Task for BandDirector;
BandWardrobe shares BandDirector's set). This is **heavy Wii→360 work**, not a
verbatim copy. **Serialize the shared-header port first:** one Opus agent ports
the common bandobj header set + its rndobj/world/utl deps (BandDirector and
BandWardrobe both need them), THEN BandCharacter / BandDirector / BandWardrobe
.cpp ports can run concurrently (disjoint .cpp, disjoint splits regions
`0x8226C738`, `0x8227C378`, `0x8231CCB0`). **Model: Opus** for all three (Wii→360
judgment, dc3-vs-rb3 reconciliation). Defer A2 unless A1 confirms the fix pays
off — it's the most expensive item in the wave.

## Concurrency / serialization summary (for batching workers)

- **Run now, 3 concurrent workers:** B1 (oggvorbis, owns the `defines_common.py`
  ogg include edit), B2 (utl/obj/synth leaf), B3 (BlockMgr). Disjoint dirs +
  splits + objects blocks. Only B1 touches `defines_common.py`.
- **Object fix (solo)** runs on its own lane (already planned). No source-editing
  agent — including B1/B2/B3 if they're still live — may run during it, since it
  rebuilds 372 Object.h-dependent TUs.
- **After Object fix:** A1 (Console + ShaderOptions re-diff, Sonnet, cheap) →
  then optionally A2 (bandobj). A2 internally serializes: shared-header port
  first (1 Opus agent), then BandCharacter / BandDirector / BandWardrobe .cpp
  concurrent.

## Risks / notes

- **smallft.c ↔ mdct.c range overlap** in the proposal — fix the pin boundary
  before wiring both (or drop smallft from this wave).
- **DataFlex.c `<unistd.h>`** isn't on the include path in either tree, yet dc3
  builds it `Matching` — the include is likely behind an untaken `#ifdef` or
  wibo-provided; trivial-but-verify. FFT.cpp similarly uses
  `xdk/LIBCMT/vectorintrinsics.h` (present) — confirm it compiles before pinning.
- **BlockMgr local copy diverges from dc3** — do NOT blindly re-copy dc3; diff
  the local edits and cross-check rb3-Wii intent (provenance caveat).
- Don't re-propose anything already in `splits.txt` (MasterAudio, the math
  TUs, the pinned subclasses, RockCentral, etc.).
</content>
</invoke>
