# Porting backlog, ranked by match-potential ÷ porting-cost

**Baseline:** 270 matched functions (`build/45410914/report.json`).
**Question this answers:** of the BinDiff clusters that are *absent* from `src/`,
which should the next (heavy-porting) phase tackle first — and is there a cheap
"Class 1" (copy-from-dc3 + wire) batch hiding in the backlog like the 92 wired so far?

**Headline (verified, see §0): the cheap lever really is tapped.** The absent
backlog is **211 clusters / 3188 BinDiff hits**, but **204 of those (3147 hits,
99% of the recoverable potential) are Microsoft XDK platform libraries that exist
in NO decomp tree** — not dc3, not rb3, nowhere. They are write-from-scratch
reverse-engineering of MS proprietary code (mostly the retail HLSL→Xenos shader
compiler). **Class 1 (cheap copy+wire) is just 7 TUs / 41 hits. Class 2 (rb3-game
Wii→360 port) is effectively 0** — no bandobj/band3 clusters survive in the absent
set (they're either already pinned or were never BinDiff clusters). The realistic
near-term delta from this backlog is **~30-40 matches**, all from the 7 Class-1 TUs;
everything beyond that is a multi-month proprietary-RE project, not a "porting" wave.

---

## 0. Method + the load-bearing correction

Per the verify-load-bearing-assumptions rule, the central claim ("Class 1 =
dc3-present-but-unwired-subdir, the next cheap win") was checked against ground
truth before ranking, and **it does not hold**. Procedure:

1. Extracted all 303 cluster `cpp=` names from `proposed_splits_bindiff.txt`.
2. Subtracted everything already wired (union of `objects.json` basenames + 
   `splits.txt` blocks) → **211 absent clusters** (brief said ~207; the +4 is
   rounding/earlier-pin drift, immaterial).
3. For each absent TU, resolved its `src=` path and checked existence:
   - at the exact dc3 path,
   - anywhere in `../dc3-decomp/src` by basename (case-insensitive),
   - anywhere in `../rb3/src` by basename.

**What the check revealed:** the bindiff `src=` paths are *aspirational canonical
paths* the fingerprint tool synthesized (always rooted at `../dc3-decomp/src/...`),
**not pointers to files that exist.** Concretely:

- Only **7** of 211 absent TUs exist in dc3 at all (by basename, anywhere).
- dc3's `src/xdk/{d3dx9,xgraphics,xaudio2,xhv2,xmic,xonline,d3d9i}` directories
  **exist but contain 0 `.cpp` files** — they are header/stub-only in dc3. Our
  `src/xdk/*` mirrors dc3 exactly (same dirs, same emptiness). So the XDK clusters
  were never decompiled in dc3 either.
- The handful of basename "hits" in dc3/rb3 are **false positives** — same filename,
  unrelated code. E.g. cluster `movie.cpp` = `xdk/d3d9i/movie.cpp` (Xbox D3D movie
  playback) but the only `movie.cpp` in the trees is Milo's `system/movie/Movie.cpp`;
  `compress.cpp` cluster = `xdk/xgraphics/compress.cpp` (DXT shader compress) vs
  Milo's `utl/Compress.cpp` (zlib wrapper); `system.cpp` cluster = `xdk/xaudio2`
  (LEAPCORE audio) vs `os/System.cpp`. rb3's `buffer.cpp`/`codec.cpp`/`scheduler.cpp`
  are network plugins, not the xWMA/xgraphics clusters of the same name.

**Why Class 1 looked big but isn't:** the 92 already-wired TUs were the *real*
dc3-present-unwired files (ogg/zlib/json-c/tomcrypt/soundtouch + Milo engine/os/utl
that dc3 had decompiled). Those are now all wired. What remains absent is the part
of the binary **dc3 also never decompiled** — the XDK. There is no second rsync to
do because there is no second source.

---

## 1. Class breakdown with counts + recoverable hits

| Class | What it is | TUs | Hits | Share of absent hits |
|---|---|---:|---:|---:|
| **1 — cheap (copy-from-dc3/already-present + wire)** | source exists in dc3 or already in our `src/`; pipeline = wire+pin like the 92 | **7** | **41** | 1.3% |
| **2 — rb3-game Wii→360 port** | source only in `../rb3` (bandobj/band3), needs idiom translation | **0** | **0** | 0% |
| **3 — genuinely absent (write-from-scratch / proprietary RE)** | exists in no tree | **204** | **3147** | 98.7% |

Class 3 decomposes by XDK family (this is where the match-potential lives, and why
it is unreachable cheaply):

| XDK family | TUs | Hits | Nature / recoverability |
|---|---:|---:|---|
| **shader compiler** (`d3dx9` + `xgraphics`) | 104 | **2324** | MS proprietary HLSL→Xenos JIT compiler, LTCG-built. The hardest target in the binary. No public source. |
| **xaudio2 / xWMA** (`xaudio2`) | 36 | 378 | MS proprietary WMA Pro decoder + LEAPCORE/XAPO audio. No public source. |
| **CRT** (`LIBCMT`) | 25 | 164 | MSVC C runtime. *Partially* recoverable — VS ships `crt/src`. Closest thing to recoverable in Class 3 (see §3 note). |
| **d3d9i** (Xbox D3D) | 12 | 93 | MS proprietary. |
| **voice/mic** (`xhv2`, `xmic`) | 12 | 89 | MS proprietary (XHV2 voice chat, ASAC mic codec). |
| **xdk-misc** (`xapilibi`, `xinput2`, `xmcore`) | 8 | 64 | MS proprietary. |
| **online** (`xonline`, `xparty`) | 7 | 35 | MS proprietary (XOnline marshalling, party). |

> Class 3 caveat — open-source XDK pieces are already done: the recoverable
> 3rd-party libs that *do* have upstream (oggvorbis, zlib, json-c, tomcrypt,
> soundtouch) are **all already wired** (zero remain in the absent set). They were
> the cheap wins inside the 92. What's left in Class 3 is the MS-only residue.

---

## 2. Class 1 — the cheap fast-follow batch (DO THIS NEXT)

Seven TUs, **41 hits**, are genuinely cheap. Four are *already byte-present in our
`src/`* and only need wiring (truly free, exactly the 92-pattern). Three exist in
dc3 and need a copy+wire. Ranked by hits ÷ cost:

| Rank | TU | Hits | dc3/src status | Pipeline | Cost | Notes |
|---:|---|---:|---|---|---|---|
| 1 | **keygen_xbox.cpp** | 17 | dc3 `src/keygen_xbox.cpp` (202 ln); not in our `src/` | copy to `src/`, add objects.json (root TU), pin `0x82706A20–0x8270735C` | trivial | density 94%, all good dc3 names. Highest single cheap win. KeyChain/mash crypto — leaf-ish, no Object layout. Note `src/KeyChain.h` already present (untracked). |
| 2 | **NetworkSocket_Win.cpp** | 5 | **already in `src/system/os/`** (68 ln) | wire-only: objects.json + pin `0x8251DF68–0x8251E864` | trivial | pure wiring; WinSock shim. |
| 3 | **Memory_Xbox.cpp** | 5 | dc3 `src/Memory_Xbox.cpp` (392 ln); not in our `src/` | copy + objects.json (root) + pin `0x82262D70–0x82263544` | trivial-mod | density 38%; XMemFree/PhysMemTypeTracker. Verify XMem* decls resolve. |
| 4 | **StreamNull.cpp** | 4 | **already in `src/system/synth/`** (29 ln) | wire-only + pin `0x826FBD98–0x826FC11C` | trivial | density 36%; ~2 real matchers (rest are STL templ). |
| 5 | **RhythmBattlePlayer.cpp** | 3 | **already in `src/system/hamobj/`** (881 ln) | wire-only + pin `0x82319810–0x82319A04` | trivial-mod | dc3 hamobj (RB3 has no equivalent gameplay, but the 3 cluster fns are STL `Find::operator` templ instantiations that match regardless). hamobj is currently 0-entry/unwired; this would be the first wired hamobj TU. |
| 6 | **FreeCamera.cpp** | 3 | **already in `src/system/world/`** (175 ln) | wire-only + pin `0x824DA640–0x824DAC3C` | trivial | density 100%, all 3 good names. Object subclass — confirm post-Object-layout-fix it isn't member-access-gated (it's a world camera; likely leaf math). |
| 7 | **KinectSharePanel.cpp** | 4 | dc3 `src/lazer/meta_ham/KinectSharePanel.cpp` (383 ln) | copy + objects.json (game) + pin `0x824E3B48–0x824E53E4` | low-skip | **dc3-game-specific** (Dance Central Kinect share UI). RB3 has no Kinect share; the 4 cluster fns are dtor/`{vector deleting destructor}`/RockCentralOpCompleteMsg dtors — *might* match as generic dtors but high risk of being DC3-only code that doesn't exist in RB3 at that address. **Verify the address actually belongs to RB3 before pinning; likely a false cluster — skip if it doesn't compile clean.** |

**Estimated Class-1 match delta: +25 to +35.** At the project's observed leaf-fn
hit rate, ranks 1-6 (37 hits) should land most of their good-named fns; keygen (17)
and FreeCamera (3, 100% density) are the most reliable. Rank 7 is speculative.
This batch closes essentially all remaining cheap value in the backlog.

**Provenance per CLAUDE.md:** all 7 are engine/CRT/dc3-game (the dc3 lane), so copy
from dc3, not rb3. None are rb3-game-logic, so no Wii→360 translation is needed —
which is also why none of them carry the bandobj-port.md friction (no MWCC pragmas,
no `revolution/`, no single-arg ObjPtr drift).

---

## 3. Class 2 — rb3-game Wii→360 ports: NONE in this backlog

There is **no Class 2 work in the absent set.** The rb3-game subsystems
(`bandobj/`, `band3/`, `net_band/`, `tour/`, `meta_band/`) that *do* need Wii→360
porting are **already pinned** in `splits.txt`/`objects.json` (BandDirector,
BandCharacter, BandWardrobe, CrowdAudio, StreakMeter, TrackPanelDir, VocalTrackDir,
RockCentral, MusicLibrary, OvershellSlot, etc.) — they are tracked in
`docs/plans/bandobj-port.md`, gated on the `Hmx::Object` 0x2c→0x28 layout fix, and
sourced from `../rb3`. They are **not** in this backlog because they aren't
*absent BinDiff clusters* — they're already wired (pin-without-source, awaiting the
port). Any "rank Class 2 by hits" request resolves to **bandobj-port.md**, not here:
BandCharacter 18, BandDirector 13, BandWardrobe 8 (counts from `porting-wave-1.md`),
order = BandLabel/StreakMeter → CrowdAudio → BandDirector → (defer BandCharacter).

The brief's framing assumed bandobj-class clusters were sitting absent waiting to be
ranked here. They aren't — the BinDiff fingerprinter did not emit them as standalone
`cpp=` clusters at density≥3% (their dc3 name maps come from pinned `.text` spans,
not this file). So this section is intentionally empty, and the heavy game-port work
is owned by the existing bandobj plan.

---

## 4. Class 3 — the XDK residue (honest disposition)

98.7% of the absent match-potential. **Do not schedule this as a porting wave.**

- **Shader compiler (2324 hits, 104 TUs)** — this is the embedded retail HLSL→Xenos
  shader compiler (`D3DXShader::*`, `XGRAPHICS::*`: CParse, CProgram, CShaderProgram,
  instr, predication, regopt, scheduler, the CFG/SSA/liveness passes). It is MS
  proprietary, was LTCG/LTGC-inlined into the retail XEX (the project's stated hard
  case), and has no public source. Matching it = re-deriving an optimizing compiler
  from PPC asm. This is a research project measured in months-to-years, not a wave.
  *If ever attempted, it has its own internal Rosetta stone: the d3dx9 front-end
  (CParse/CTokenize) and the xgraphics back-end (CFG/SSA) are separable, and the
  low-span/high-density leaf TUs (linkedlist 45, vector 12, hashtable 11, dlist 9,
  the `LST_*`/`HASHTABLE_*` C-style containers) are the only tractable entry points.*
- **xWMA/XAudio2 (378 hits)** — MS proprietary WMA Pro decoder. Same story.
- **LIBCMT (164 hits)** — the one Class-3 family with a *partial* external source:
  the MSVC C runtime ships source under the Visual Studio install (`VC/crt/src`).
  The retail XEX statically linked the X360 LIBCMT, so functions like
  `strtol`/`pow`/`_controlfp`/`_initterm`/the `__savegpr`/`__savefpr` stubs could in
  principle be matched against the shipped CRT source rather than written from
  scratch. **This is the only Class-3 sub-batch worth a feasibility spike** (highest
  hits-per-effort in the proprietary set), but it still requires sourcing the exact
  X360-toolchain CRT version and is out of scope for a "porting" phase. Treat as a
  separate, optional investigation — not part of the heavy-porting wave.

---

## 5. Concurrency / serialization guidance

The build-lane mutex (one `ninja` writer) is unchanged. For the Class-1 batch:

- **Source copying is concurrent.** The 3 copy-in TUs (keygen_xbox, Memory_Xbox,
  KinectSharePanel) touch disjoint files (`src/keygen_xbox.cpp`, `src/Memory_Xbox.cpp`,
  `src/lazer/meta_ham/KinectSharePanel.cpp`) and the 4 wire-only TUs touch no source
  at all. All 7 text edits (copy + objects.json entries + splits.txt pins) can be
  drafted in one pass.
- **objects.json / splits.txt are shared files** — batch all 7 edits in a single
  agent to avoid merge conflicts on those two config files (they're small; one
  writer is simplest).
- **Compile-verification serializes** on the single build lane: after staging all 7,
  run `touch config.yml && ninja` **once**; trivial fns register together. If a TU
  fails to compile (Memory_Xbox XMem decls, KinectSharePanel DC3-only risk), drop it
  and re-run — do not let one failure block the other 6.
- **Pin-range disjointness:** all 7 proposed `.text` ranges are spatially disjoint
  from each other and from existing pins (keygen `0x827069…`, NetworkSocket
  `0x8251DF…`, Memory_Xbox `0x82262D…`, StreamNull `0x826FBD…`, RhythmBattlePlayer
  `0x823198…`, FreeCamera `0x824DA6…`, KinectSharePanel `0x824E3B…`). No overlap
  resolution needed.
- **FreeCamera is the only Object subclass** in the batch — if the `Hmx::Object`
  layout fix (`hmx-object-layout.md`) hasn't landed, its member-accessing fns may
  not match; wire it anyway (the 3 cluster fns look leaf), and re-diff after the fix.

**No worktrees available; everything verify-compiles on the one lane.** A build-lane
agent is currently running — this batch must wait for / coordinate with it; do not
start its `ninja` until the lane is free.

---

## 6. Honest total-effort estimate + realistic ceiling

| Tier | Effort | Realistic match delta |
|---|---|---|
| **Class 1 (7 TUs, wire+copy)** | hours — one batched agent, one `ninja` | **+25 to +35** (270 → ~300) |
| Class 2 (rb3-game) | tracked separately in bandobj-port.md; not this backlog | (n/a here) |
| **Class 3 — LIBCMT spike** (optional, 25 TUs) | days, contingent on sourcing exact X360 CRT | up to +164 *if* the CRT source is obtainable and matches; realistically a fraction |
| **Class 3 — shader compiler + xWMA + rest** (179 TUs) | months-to-years of proprietary RE; not a porting task | up to +2960 in theory, ~0 in practice for this phase |

**Bottom line for the next heavy-porting phase:**

1. **Run the Class-1 batch now** (§2). It's the only "free" matches left in the
   absent backlog — ~30 matches for a few hours of wiring. This is the entire
   actionable content of this document.
2. **The "heavy porting" phase the brief anticipated does not exist as framed.** The
   absent backlog is not 207 portable-from-a-sibling-repo TUs; it's 7 cheap ones and
   204 MS-proprietary XDK functions with no source anywhere. Once Class 1 is wired,
   **the porting lever is fully exhausted** and the next real lever is either (a) the
   already-planned bandobj/Object-layout game-code work (`bandobj-port.md`,
   `hmx-object-layout.md`), or (b) genuine matching-against-asm of code we *do* have
   wired (the 129 `NonMatching` TUs already in `objects.json`), or (c) the
   speculative LIBCMT-from-VS-source spike.
3. **Realistic ceiling of THIS backlog if "fully ported":** ~3188 matches in the
   abstract, but ~3147 of those require reverse-engineering Microsoft's shader
   compiler and codecs from scratch. The **practical** ceiling — what's reachable
   without a multi-month proprietary-RE effort — is **~40 matches** (Class 1, plus a
   long-shot on LIBCMT leaf stubs).

---

## Appendix — verification artifacts

- Absent set = `clusters(303) − wired(129 = objects.json ∪ splits.txt basenames)` = **211**.
- Existence probes: `find ../dc3-decomp/src -iname <cpp>` and `find ../rb3/src -iname <cpp>`
  for all 211 → 7 dc3-real (4 already in our `src/`), 0 rb3-real-and-relevant, 204 nowhere.
- dc3 `src/xdk/{d3dx9,xgraphics,xaudio2,xhv2,xmic,xonline,d3d9i}` = **0 .cpp each**
  (header/stub-only); our `src/xdk` mirrors it exactly.
- Class-3 family hit totals computed from `proposed_splits_bindiff.txt` `hits=`
  fields summed per `src=` subsystem.
</content>
</invoke>
