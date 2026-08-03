# X4d — the venue root loads, and the wall was four bytes

**Date:** 2026-08-03
**Predecessor:** [X4c](x4c-init-audit-2026-08-02.md) "the init audit, and why `kNewGfx` was never the thing that emptied the frame"
**Branch:** `x4d-venue-root`, rebased onto `main`
**Engine:** `milo-native-engine` pinned at **`138e1606…`**, **zero engine edits**

---

## Verdict

★ **A REAL RB3 VENUE ROOT LOADS AND RENDERS.** `world/venue/small_club/small_club_01`
returns `rc=0` with every gate green — root `small_club_01` as a **`WorldDir`**,
114 meshes, 96 drawn, 38.92% coverage, 111930 distinct colours. The frame is
recognisably RB3's small club: brick walls, plank floors, the bar, the staircase,
stacked chairs, ceiling beams, railings.

★ **And it brings its own lighting.** `environ: scene's own 'geom_norim.env'` —
a **real shipped `RndEnviron`**, where X4a had to synthesize one because venue
*props* ship none. This is the milestone the charter named, and it is the first
real RB3 lighting in the port.

★ **The root cause was four bytes, and it was not any of the things four
milestones of documents predicted.** Not factory misses. Not `band3`. Not
`ObjectDir::Init`. `ObjectDir::PostLoad` guards its proxy-load branch with
`ShouldSaveProxy(bs)`, which is **strictly weaker** than the retail guard
`IsProxy() && !mProxyFile.empty()`. The extra disjunct fires for a proxy dir with
an **empty** `mProxyFile`, constructing a `DirLoader` on an empty path, handing
it the **parent's live stream**, and letting its `LoadHeader` eat 4 bytes that
belong to the next object.

**One-line fix. The 675 factory misses were never the wall — and they are still
all there, all recovered, in the run that renders.**

Both E1 rider caveats are answered, one of them by refuting my own first
explanation (§6).

⚠ **Partial, stated plainly:** this is the venue's **architecture**. No
characters, no crowd, no band, no audio, no camera shots. §5 is the honest
per-subsystem table.

---

## 1. Gate results

| # | Gate | Result | Evidence |
|---|---|---|---|
| a | Full native gate, **fresh** (`rm -rf native/build` first), rc=0 | ✅ **PASS — 18/18** | `tools/native_build_gate.sh` → `NATIVE GATE: PASS (rc=0, 0 errors, 0 warnings, 18/18 target(s) verified)`, **every target `relinked this run`**. Cache seeded first with an explicit `cmake -S . -B build -DMILO_ENGINE_PATH=… -DDawn_DIR=…` per X4c's warning that the gate's own `cmake` line omits them and would let 3 targets silently SKIP while still reporting `PASS`. |
| b | Zero `milo-native-engine` edits | ✅ **PASS** | engine `HEAD` == pin `138e1606…`; **I made zero engine edits** and no engine change requests arose this milestone. ⚠ Disclosed for completeness: the engine working tree carries **one uncommitted edit that is not mine** — `src/platform/FxSendNative.cpp`, mtime **2026-06-16**, i.e. seven weeks old and present in X4a/X4b/X4c's gate runs too. It is `NativeEffectSlot::SyncParams` (audio EQ band params), off the load and render paths entirely, so it does not confound this milestone. Left untouched per the concurrent-agent rule. |
| c | Shared-`src/` edits `HX_NATIVE`-gated, X360 arm token-for-token | ✅ **PASS — VERIFIED BY REBUILD, at symbol granularity** | §1.2 — the whole-object byte-compare that prior lanes used is **not sufficient here**, and §1.2 explains why. |
| d | PNG determinism ×2 on every cited image | ✅ **PASS — 4/4** | venue `59c1997f41cb58ed…` ×2; rider posed `af45a675041ed7cf…` ×2; rider body-only bind `5147517876eb36d2…` ×2 |
| e | X3/X4a/X4b/X4c evidence non-regressed | ✅ **PASS — byte-identical** | X3 track cell `cbdb29fa95a5b574…` and X3 crowd bind cell `a2a69cee7094f152…` reproduce **exactly** the SHAs X4c recorded |

### 1.1 ★ The rebase re-gate caught a break on `main` that was not mine — and the reason it got there is a standing hazard

The charter's "rebase + re-gate immediately before landing" earned its keep. This
work gated **PASS 18/18** on its own base (`279804a8`). After rebasing onto `main`
@ `4bca6887` the same gate went **FAIL — 16/18, 14 error lines, 2 target defects**
(`rb3-render`, `rb3-milo`):

```
SoftParticles.cpp:90: error: use of undeclared identifier 'd'
    ASSERT_REVS(1, 0)
    obj/Object.h:1618: expanded from macro -- if (d.rev > rev1 ...)
```

Attribution, not assertion: `src/system/rndobj/SoftParticles.cpp` was **not** in
my change set and **includes none of my files**. It was last touched by
`4bca6887` itself — lane DI-1's `match(SoftParticles LOAD_REVS)`, landed while
this lane was running.

★ **The two rev macros are a pair, and only one was overridden.** `obj/Object.h`'s
`LOAD_REVS` declares the stack temp `BinStreamRev d`; its `ASSERT_REVS` reads `d`.
DI-1 switched that TU to the rb3-Wii `gRev`/`gAltRev` dialect — which declares no
`d` — leaving the inherited `ASSERT_REVS` dangling.

★★ **And it compiled on X360 purely by luck.** Lane CP-2 had already made the
`!HX_NATIVE` arm of `ASSERT_REVS` expand to **nothing** (`obj/Object.h:1648`,
"retail compiles ASSERT_REVS out entirely"), so the X360 build never instantiates
the reference. **Only the `HX_NATIVE` arm — which deliberately keeps the
version-mismatch warning — sees it.**

> ⚠ **Standing hazard:** an X360-only match lane can land this entire class of
> break completely green. The X360 build does **not** cover the native build for
> anything living only in an `HX_NATIVE` macro arm — and `ASSERT_REVS` is now
> empty on X360, so that arm is *invisible* there. Corollary rule: **whenever a
> TU overrides `LOAD_REVS`, it must override `ASSERT_REVS` too.**

⚠ **I fixed it, then dropped my own fix, and dropping it is the right outcome.**
My repair restored the gate to PASS 18/18 with X360 codegen provably unaffected.
But while gate 3 ran, `main` advanced again to `fb3ec931` (lane DI-2), which
rewrote this TU to 100% and **independently fixed the same break, more cleanly** —
`#pragma push_macro`/`pop_macro` around all four rev macros, `ASSERT_REVS` defined
empty. Rebasing onto it left two competing `#undef ASSERT_REVS` blocks in one
file. `main` is **no longer broken**, so keeping my commit would not be a fix — it
would be an unrequested behaviour change layered on another lane's landed work.
**X4d's final changeset therefore does not touch `SoftParticles.cpp` at all.**

★ **The finding survives the fix being dropped, and it is the part that matters.**
This is **not the first** instance; `main` already carries three prior repairs of
exactly this shape, each from a different match lane:

```
c833a0fe fix(native): instantiate operator<< for plain ObjPtr<T> -- unblocks the link DG-3 broke
dce343a1 fix(native): unbreak rb3-milo/rb3-render — DD-2's RndEnvAnim::Save needs an ObjOwnerPtr<> save instantiation
61162969 fix(native): define CacheWav + CacheResource, unblocking 7 native link targets
```

DI-1's makes **four**. That is a systemic gap, not an incident, and the cost lands
on whoever rebases next. Filed in §8.

### 1.2 ⚠ Gate (c): the whole-object byte-compare is not a sufficient method, and it nearly cost a false alarm

X4a/X4c verified "no X360 codegen change" by rebuilding the X360 `.obj` and
byte-comparing whole files, accounting for the embedded build-path string. That
worked for them because their worktree paths were the **same length**
(`laneX4A` → `laneX4C`). Applied naively here it produces a **599843-byte
difference on `Dir.obj`** and looks catastrophic. It is not.

Two traps, both real:

1. **The warm reflink cache.** The first "before" object I captured had been
   built in the *main* repo at `/home/free/code/milohax/rb3-xenon` and served to
   the worktree by CoW. Its embedded absolute path is a **different length** to
   the worktree's, so every later byte shifts and the whole file "differs". Both
   sides must be rebuilt **in the same worktree**. (Detectable: the 4 timestamp
   bytes at offsets 5-8 are identical only when both were built here.)
2. **File-offset diffing is meaningless once any section changes size.** `Dir.obj`
   grew by 8 bytes, so a positional `cmp` reports a cascade, not a defect.

The correct instrument is **per-symbol section content**:

| object | text symbols | symbols whose `.text` code differs | `.text` content multiset identical? |
|---|---|---|---|
| `Dir.obj` | 4305 / 4305 | **0** | ✅ yes (1546 sections) |
| `DirLoader.obj` | 1179 / 1179 | **0** (the single hit is the aggregated `.text` *section symbol*, an artifact of keying by name) | ✅ yes (470 sections) |

`.rdata` / `.data` / `.pdata` / `.xdata` multisets are identical too. **Every
X360 function body is byte-identical.** What actually changed:

- **`.debug$S`** — source line info. Expected: I added lines to the file.
- **COMDAT emission order** in `DirLoader.obj` — 4 swapped pairs of `.text`
  sections with byte-identical contents. A permutation, not codegen.
- ⚠ **MSVC's anonymous-namespace discriminator**, `?A0xf6109900` vs
  `?A0x996edca1`, on 3 symbols in `Dir.obj`
  (`??__EgPreloaded`, `??__FgPreloaded`, `?DeleteShared`). The hash is
  **per-TU and content-derived**, so *any* edit to a shared file — including a
  pure comment — renames these. Their code bytes are identical, and **none is a
  scored function** (`default/DirLoader` scores 134/208 and contains no `?A0x`
  symbol; `default/Dir` is not a scored unit name at all), so scoring is
  untouched. Recorded because it is invisible to the whole-file method and will
  recur for every future shared-`src/` edit.

---

## 2. ⛔ The defect: a proxy-load guard weaker than retail's

### 2.1 How it was found — an absolute oracle, chosen for what it is *not* invariant under

X4c's parting lesson was to pick instruments that are not invariant under the
defect being hunted. For a stream, the strongest available absolute check is
**the `ReadDead` scan distance**, added as `RB3_STREAM_AUDIT`
(`obj/DirLoader.cpp`). After a top-level object's `PostLoad` the stream must be
sitting exactly on the 4-byte `0xADDEADDE` marker, so:

```
dist == 4   -> that object's Load() consumed exactly the right bytes
dist  > 4   -> it UNDER-read by (dist - 4)
huge dist   -> it OVER-read past its own marker
```

Factory-miss slots are tagged separately (`MISS-SKIP`), because a large distance
there is *expected* — it is the payload of a class we could not construct — and
conflating the two is precisely how a real desync hides.

**The first run paid for the instrument by returning nothing.** Across the entire
venue load: **zero anomalies.** Every object that loaded consumed exactly the
right bytes. That is a strong negative result — it eliminates every object
`Load()` in the tree in one run, and redirects the search to the only remaining
place a byte can go missing: **the loader machinery that runs *between* objects.**

`RB3_STREAM_AUDIT=2` then gave the byte-exact transition:

```
STREAM_AUDIT: <POSTLOAD off=906603 'amp_fnr_bassman'          (WorldInstance)
              ...ReadDead consumes its marker, dist == 4 -> 906607
DirLoader: rev 3 < 7 in '' at stream offset 906611 (proxy 'amp_fnr_bassman')
STREAM_AUDIT: >PRELOAD  off=906611 'amp_fnr_bassman01'        (WorldInstance)
```

906603 → 906607 is a clean marker. Then **something read 4 bytes at 906607** and
`amp_fnr_bassman01` started 4 bytes late. The value that phantom read — `3` — is
not a dir revision at all: it is `amp_fnr_bassman01`'s own `WorldInstance` rev
(`ASSERT_REVS(3, 0)`, `world/Instance.cpp:173`). It read the next object's first
field and complained that it was not a milo header.

### 2.2 The guard

`src/system/obj/Dir.cpp`, tail of `ObjectDir::PostLoad`:

| | guard |
|---|---|
| **rb3-Wii (RB3 retail oracle)** `rb3/src/system/obj/Dir.cpp:475` | `if (IsProxy() && !mProxyFile.empty())` |
| **rb3-xenon** (before) | `ShouldSaveProxy(bs)` == `IsProxy() && (!mProxyFile.empty() \|\| InlineProxy(bs))` |

The `|| InlineProxy(bs)` disjunct makes it strictly weaker. The branch then does:

```cpp
DirLoader *dl = new DirLoader(mProxyFile, kLoadFront, nullptr,
                              InlineProxy(bs) ? &bs : nullptr, this, false, nullptr);
```

— an empty `FilePath` **and the parent's live stream**, so `DirLoader::LoadHeader`
runs a milo-header read wherever the parent happened to be.

**Measured across all 12 proxy dirs on this venue** (`PROXY_AUDIT`):

| proxy dir | `mProxyFile` | `InlineProxy` | retail guard | xenon guard |
|---|---|---|---|---|
| `female_base`, `male_base`, `crowd_female01..04`, `crowd_male01..04` (10) | non-empty | 1 | fire | fire — **agree** |
| `amp_fnr_bassman` | **empty** | 1 | **does not fire** | **fires** — ⛔ |

Eleven of twelve agree. The twelfth is the venue. Everything downstream — the
bogus `rev 3 < 7`, `Can't load old ObjectDir`, `version 41 > 3`, and the garbage
`String chars 774778671` that walks `ReadString` off a stack buffer into SIGSEGV
— is fallout from those 4 bytes.

`ShouldSaveProxy` is not wrong in itself. It is the correct guard on the **save**
path and is used that way at `Dir.cpp:680`. **Reusing it on the load path is the
defect**, and rb3-Wii's `ObjectDir::PostLoad` does not call it at all.

### 2.3 Why the crash was so far from the cause, and so loud about the wrong thing

The proximate SIGSEGV is `ChunkStream::ReadImpl` under
`operator>>(BinStream&, FilePath&)`. `FAIL: String chars 774778671 > 256` is
printed **and the read proceeds anyway** — `MILO_FAIL` is deliberately non-fatal
natively (`os/Debug.cpp:183`), so a diagnostic that correctly identifies a bad
length then smashes the stack with it. Frames 5+ of the backtrace are garbage
because the stack buffer is the thing that was overwritten. Worth fixing
independently; filed in §8.

---

## 3. ⛔ RETRACTED: `ObjectDir::Init` is not the venue defect

The charter named this as "a named lead worth checking early", and X4c filed it
as Tier-1 owed work. **It is innocent, and the disproof is mechanical.**

`ObjectDir::Init` (`obj/Dir.cpp:917-939`) registers ten `LoadMgr` **loader**
factories (`milo`, `milo_xbox`, …) all pointing at `DirLoader::New`. Those are
consulted **only** by `LoadMgr::AddLoader` / `ForceGetLoader`. But every path in
the `ObjectDir` graph bypasses that table and constructs `DirLoader` directly:

| path | site |
|---|---|
| top-level | `ObjDirPtr::LoadFile` → `obj/Dir.h:235` |
| **inline subdir** | `ObjDirPtr::LoadInlinedFile` → `obj/Dir.h:277` (takes `&bs` — structurally impossible through `AddLoader`, which only takes `(FilePath, LoaderPos)`) |
| **proxy subdir** | `obj/Dir.cpp:520` and `obj/Dir.cpp:1534` (the defect site) |
| plain subdir | `ObjectDir::LoadSubDir` → `obj/Dir.h:235` |

★ **Proof by construction:** `DirLoader::New` exists *only* as the factory thunk
`Init` registers. `nm` on the linked `rb3-render` shows **neither
`ObjectDir::Init` nor `DirLoader::New`** — both stripped by `--gc-sections`,
while `LoadMgr::AddLoader` survives. If any dir load routed through the factory
table, `DirLoader::New` could not have been stripped.

`DirLoader::LoadResources` — the one `AddLoader` call inside `DirLoader` — is
reachable only for `mRev <= 16`; RB3 venue milos are rev 25, so it never runs,
and its entries are texture/audio leaves for which the `FileLoader` fallback is
the *intended* result.

**What the omission does still cost** (real, but not this): the four DTA script
functions `load_objects` / `init_object` / `path_name` / `reserve_to_fit`
(`Dir.cpp:934-937`) are unregistered, and latently `MoveDir.cpp:399` and
`SyncSubDir` (`Dir.cpp:212`) would silently get a `FileLoader`. Left for a native
lane; **it does not bear on the venue**, which is what the charter asked.

`ThreadCallInit` likewise does not bear on the venue: the venue load is fully
synchronous and never enters `DataLoader::LoadFile`'s spin. Its `gMainThreadID
== -1` assertion-disabling remains true and remains owed — **but it is not a
venue item**, and I am leaving it, as the charter permits.

---

## 4. ⭐ `BandCamShot`: the one-line shortcut is REFUTED

X4c established the precondition (the misses are top-level) but deliberately did
not land the base-class bind, because whether `ReadDead` absorbs the short read
was unmeasured and there was no clean signal. There is now.

`RB3_BIND_BANDCAMSHOT=1` (`native/src/milo_object_factories.cpp`, off by default)
binds `Symbol("BandCamShot")` to `CamShot::NewObject`.

| leg | result |
|---|---|
| **unregistered (default)** | **venue loads, rc=0**, 675 misses, every one recovered |
| base-class bound | **rc=134 SIGABRT** — `ASSERT_REVS WARNING: CamShot 'coop_all_b00.shot' version 50 > 2`, then `BinStream::operator>>(String) ABORT: bad size=73556334 at pos=16199107` |

It retires all 611 misses and **breaks the load**. It is strictly worse than
doing nothing.

★ **The reason generalises, and it is the useful part.** A base-class substitute
does not merely *under-read*. It **mis-interprets** the derived payload from the
very first field — it reads `BandCamShot`'s rev as a `CamShot` rev — then derails
**inside** `Load()` and walks a garbage `String` length off a cliff. Control
never reaches the `ReadDead` that would have re-synced.

> **`ReadDead` recovers objects that were SKIPPED. It cannot recover an object
> that was MIS-PARSED, because the parse never returns.**

That is exactly the difference between the factory-miss path — which is healthy,
675 misses, all recovered, venue renders — and the wrong-class path. So the
one-line shortcut for 90% of the misses is dead: `BandCamShot` needs its **real
TU**, or nothing. **A miss is the better outcome, and it is already the default.**

⇒ The `ScatterIncludes.cmake` dedupe lane is therefore **not** on the venue's
critical path either. It was ranked #3 to enable 618 misses; the venue does not
need them. It remains the right lane for *content* (camera shots, audio, band
characters), not for *loading*.

---

## 5. Per-subsystem verdict table

Measured on the run in `evidence/venue-root-LOADS.log`.

| subsystem | verdict | evidence |
|---|---|---|
| **Dir graph / stream** | ✅ **ALIVE** | root `small_club_01` as `WorldDir`; 675 factory misses **all recovered**; `rc=0`; zero `ReadDead` anomalies |
| **Geometry — `RndMesh`** | ✅ **ALIVE** | 114 meshes, 96 issued draws, 41132 verts, bbox (-224,-669,0)..(277,278,157) |
| **Materials — `RndMat`** | ✅ **ALIVE** | 113 of 114 meshes carry a real shipped `Mat` |
| **Textures — `RndTex`** | ✅ **ALIVE** | 93 meshes with a diffuse `Tex`; BC compression active |
| **`RndEnviron` / scene lighting** | ★ **ALIVE — REAL, first time** | `environ: scene's own 'geom_norim.env'`. X4a's was **SYNTHESIZED**; venue props ship none. |
| **Lights (`.lit`)** | ⚠ **PARTIAL** | `.lit` objects load, but their targets don't: 49 unresolved `player0` + 14 `player1..3` refs (`shadow_projected.lit`, `vocals_silhouette.lit`). Player-targeted lighting needs `BandCharacter`. |
| **Anchors (`.tp` TransProxy)** | ⚠ **PARTIAL** | 37 unresolved `player*` refs — same root cause |
| **Material fallback** | ⚠ **1 synthesized** | 1 of 114 meshes ships no `Mat` and got our neutral prelit grey. **That mesh's appearance is ours, not the asset's.** |
| **`MatAnim`** | ⚠ **rev warning** | `ASSERT_REVS WARNING: MatAnim 'bonus_beam.mnm' version 7 > 0 (or alt 0 > 7)` — loads, un-investigated |
| **PostProc** | ⬜ **UNREACHED — correctly** | `RndPostProc::Current()=(nil)`. Venue **roots** ship zero PostProc (X4a's archive scan); only `video_*` roots do. X4a proved postproc separately on `world/shared/fx`. Not a gap. |
| **Skinned characters** | ⬜ **UNREACHED** | **0 skinned meshes.** `BandCharacter` unregistered (4 misses) |
| **Crowd** | ⬜ **UNREACHED** | `WorldCrowd` is registered, but no crowd is instantiated in this root |
| **Audio / synth** | ⬜ **UNREACHED** | 55 misses across `Sfx`(23) `SynthSample`(18) `MoggClip`(6) `SynthFader`(5) `Parallel`/`RandomGroupSeq`(3); `synth/` gate deliberately closed |
| **Camera shots** | ⬜ **UNREACHED** | `BandCamShot` 611 misses; base-class bind **refuted** (§4) |

**What is in the frame, honestly:** the venue's static architecture, with its own
materials, textures and environment. It is a room, correctly lit by its own
`RndEnviron`. It is empty — no band, no crowd, no stage lighting animation.

---

## 6. The E1 riders

### 6.1 Framing — the whole character, and *why* it was cropped

Not a renderer defect. `main_render.cpp` carries **two** framing constants and
its own comment says one cannot serve both — `0.9` for the wide flat track piece,
`1.15` for the tall narrow figure. But those are attached to the **default X3
cells**; any **explicit arkPath** fell through to `{0.45, 0.30, 0.9}`. So the
character framed correctly as a default cell and cropped its head and hands when
rendered by path — which is exactly how X4c invoked it.

Fixed by making framing explicit rather than implicit: `--dist-scale`,
`--azimuth`, `--elevation` (driver-only; no shared `src/`).

`evidence/rider1-posed-full-framing.png` (`af45a675041ed7cf…`, ×2) contains the
**whole character at the posed beat**: head, face, both arms raised in a coherent
overhead reach, fingers, legs, feet. **The pose is correct** — `crowd_reaching_01`
at beat 4.0 is a two-armed overhead reach, and that is what renders. X4c's
headline is now verifiable, not merely "the spikes are gone".

### 6.2 The residual slivers — ⛔ **I was wrong once, and the control caught it**

**My first hypothesis was that they were the hand props.** A crowd character
ships **six** skinned meshes: the body plus five mutually-exclusive gesture props
(`horns`, `fist`, `clap`, `lighter`, `lighter.1`) which the game shows **one of**
per crowd member, and `rb3-render` draws **all six at once**. That is a real
finding and worth knowing — but it is **not** the slivers. Refuted directly with
`--only-mesh female_crowd_body`: the props vanish from the hands and **both
slivers remain** (`evidence/rider2-body-only-posed.png`).

The decisive control is **bind pose**. With **no clip applied at all**, the same
pale wedge sits at the crotch and the same streak runs down the shin — same
shape, same place (`evidence/rider2-body-only-BIND-slivers-present.png`,
`5147517876eb36d2…` ×2). And they are present in the **X3 baseline bind cell**
(`a2a69cee7094f152…`), recorded before any pose work existed and byte-identical
today (`evidence/rider2-X3-baseline-bind-slivers-present.png`).

⇒ **They are not skinning, not the pose, and not weight normalisation.** Answer,
in one line with its evidence:

> **The slivers are the LOD2 body asset's own surface content** — the mesh is
> `female_crowd_body01_lod02.mesh` with `female_crowd_body01_lod2_diff.tex`, the
> lowest-detail crowd body — **visible at bind pose with no clip, and present in
> the X3 baseline PNG that predates all pose work.** They deform *with* the leg
> when it bends, i.e. they are glued to the surface, so they are texture/material
> content on very low-poly geometry, amplified by the SYNTHETIC single-directional
> light this cell uses. Not a second instance of the truncation class.

Corroborating instrument readings, all clean: per-vertex weight sums
**0.998..1.000**, `nan=0`; palette invariant 72/72 bones resolved, worst
`|det−1|` **2.92e-04**; `BLENDINDICES` dense `0..nBones-1`, gcd 1, on all 6
meshes.

★ The generalisable bit: this cell has **no `RndEnviron`** and is lit by one
synthetic directional with no ambient occlusion, which is unusually good at
turning ordinary texture contrast into something that reads as a geometry defect.
**Now that a venue supplies real lighting, the right way to judge a character's
appearance is under the venue's environ** — that is a concrete next experiment,
not a platitude.

---

## 7. Retracted hypotheses, with evidence

1. ⛔ **"`ObjectDir::Init` never runs, so `.milo` loads silently get a plain
   `FileLoader`"** — the charter's named early lead and X4c §8's Tier-1 item.
   **Retracted.** The factory table is consulted only by `AddLoader`, and every
   dir-graph path constructs `DirLoader` directly. Proof by construction: `nm`
   shows `--gc-sections` stripped **`DirLoader::New` itself**, which is only
   possible if nothing routes through the table. §3.
2. ⛔ **"The 675 factory misses are the venue wall"** (X4a §3, carried by
   `band3-native-unblock-priority` §7 as items #2/#3). **Retracted by
   construction:** the venue now renders **with all 675 misses still present and
   unchanged**. Further, they are not even *reached* — `CreateObjects` logs all
   661 of the venue's misses up front, and `LoadObjs` was dying at
   `amp_fnr_bassman01` before touching a single miss slot (measured: **0**
   `MISS-SKIP` on the venue file against 661 "Can't make" lines).
3. ⛔ **"Bind `BandCamShot` to `CamShot::NewObject` and retire 611 misses in one
   line"** (`band3-native-unblock-priority` §4.3, "the cheapest high-value
   measurement left"). **Retracted by measurement:** `rc=134`. `ReadDead` cannot
   absorb a *mis-parse*, only a *skip*. §4.
4. ⛔ **My own first read of the defect: "the `HX_NATIVE` arm of
   `ObjectDir::InlineProxy` is DC3-shaped and decides differently from retail."**
   The two arms *are* differently shaped — native consults the DC3-era
   `mInlineProxyType` enum, retail the RB3 bool `mInlineProxy` (which does not
   even exist under `HX_NATIVE`) — and it looked like the answer. **Retracted:
   they compute the same value on this data.** All 12 proxies measured
   `InlineProxy=1, cached=1, AllowsInlineProxy=1`. The divergence is real but
   latent; the live defect is the *caller's* guard, one line away. Measuring the
   decision instead of reasoning about the shape is what separated them.
5. ⛔ **My own second hypothesis: "the native arm reads a 4-byte `InlineDirType`
   where retail reads a 1-byte bool"** (`Dir.cpp:1226`, `d.rev > 0x1B`). A
   genuine width divergence of exactly the class the charter warns about — and
   **not live**: measured `d.rev=27` (`0x1B`, *not* `> 0x1B`) on all 47 dirs, so
   the 1-byte arm is taken. **Latent hazard for any asset at rev ≥ 0x1C**, filed
   in §8, but it is not this defect.
6. ⛔ **My own third hypothesis: "the residual slivers are the five hand-prop
   meshes drawn simultaneously."** **Retracted** by `--only-mesh`: props off,
   slivers remain. The six-meshes-at-once finding is real and separately useful;
   it is just not the answer. §6.2.
7. ⚠ **Explicitly NOT claimed:** that the venue is *correct*. There is no retail
   ground truth in this loop. The claim is "loads, renders, and no detectable
   defect in what loaded" — §5 states what did not load at all.

---

## 8. Owed work / handoff

| item | why | owner |
|---|---|---|
| ⛔ **Match lanes are not covering the native build — 4th instance** | §1.1. `ASSERT_REVS` is **empty on X360** and non-empty only under `HX_NATIVE`, so a per-TU macro-dialect switch broke `rb3-render`/`rb3-milo` while the X360 gate stayed green. Three prior repairs of the same shape already exist on `main` (`c833a0fe`, `dce343a1`, `61162969`). **Add a native smoke build to the match-lane gate.** The TU sweep is done and is now clean: exactly 3 files `#undef LOAD_REVS` — `obj/dialect_object_push.h`, `ui/UILabel.cpp`, `rndobj/SoftParticles.cpp` — and all three now override `ASSERT_REVS` correctly. (Sweep run with a positive control, since DI-1's own commit records a vacuous-grep incident in this area.) | **match lanes / infra** |
| ⛔ **`MILO_FAIL` on a bad string length does not stop the read** | `BinStream::ReadString` prints `String chars N > 256` and then reads N bytes into a fixed stack buffer, smashing the frame. The diagnostic is *correct* and the code proceeds anyway. A native-only early-out turns a stack smash into a legible error, and would have made this milestone's crash self-describing. | X4e / native |
| ⚠ **The `InlineProxy` / `mInlineProxyType` shape divergence** | Latent, not live (§7.4). Native consults a DC3-era tri-state enum whose value is **never deserialized on the load path** (`mInlineProxyType` keeps its ctor default `kInlineCached`), while RB3 stores a bool. It agrees today by coincidence of the default. | native |
| ⚠ **4-byte `InlineDirType` read at `Dir.cpp:1226` for `d.rev > 0x1B`** | Retail reads a 1-byte bool at all revs. Not reached at rev 27; a rev ≥ 0x1C asset would over-read 3 bytes per proxy dir. | native |
| **MATCH DEBT: the `ShouldSaveProxy` guard** | The fix is `HX_NATIVE`-gated and the X360 arm is **unchanged**, so `default/Dir` keeps its current score. rb3-Wii `main/system/obj/Dir` is the oracle for an A/B. Probable match *gain*, unmeasured. | match lane |
| **Player-targeted lighting** | 49 + 14 unresolved `player0..3` refs from `.lit`/`.tp`. Needs `BandCharacter` — i.e. the `ScatterIncludes` lane, now correctly motivated by **content**, not by loading. | X4e |
| **`BandCamShot` real TU** | 611 shots. The shortcut is dead (§4); only the real TU will do. Behind the `ScatterIncludes` dedupe. | X4e |
| **Render a character under the venue's real environ** | The first time this is possible. Also the honest way to re-judge §6.2. | X4e |
| `ObjectDir::Init`'s four DTA funcs; `ThreadCallInit` | Real, still owed, **not venue items** (§3). | native |
| Carried, untouched | `ScatterIncludes.cmake` dedupe (807 dup-defs); 4 root defects in `BandCharacter.cpp` incl. the latent `Refs()` hang; `WorldInstance::PreLoad/PostLoad` rev ordering | — |

---

## 9. Evidence

Copied **outside** the worktree, and the worktree is being left in place:

| path | what |
|---|---|
| `/home/free/tmp/laneX4D/evidence/venue-small_club_01.png` | ★ the venue root, `59c1997f41cb58ed…` |
| `/home/free/tmp/laneX4D/evidence/venue-root-LOADS.log` | the `rc=0` run behind §5 |
| `/home/free/tmp/laneX4D/evidence/venue-SIGSEGV-before-fix.log` | the failure, for A/B |
| `/home/free/tmp/laneX4D/evidence/venue-desync-object-trace.log` | `RB3_STREAM_AUDIT=2`, the 906603→906611 arithmetic |
| `/home/free/tmp/laneX4D/evidence/venue-proxy-audit.log` | the 12-row proxy table of §2.2 |
| `/home/free/tmp/laneX4D/evidence/rider1-posed-full-framing.png` | rider 1, `af45a675041ed7cf…` |
| `/home/free/tmp/laneX4D/evidence/rider2-body-only-posed.png` | props off, slivers remain |
| `/home/free/tmp/laneX4D/evidence/rider2-body-only-BIND-slivers-present.png` | ★ the control, `5147517876eb36d2…` |
| `/home/free/tmp/laneX4D/evidence/rider2-X3-baseline-bind-slivers-present.png` | slivers predate all pose work |

Worktree: `/home/free/tmp/laneX4D/wt` (branch `x4d-venue-root`).

### Reproduce

```bash
cd /home/free/tmp/laneX4D/wt/native
cmake -S . -B build -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
      -DMILO_ENGINE_PATH=/home/free/code/milohax/milo-native-engine \
      -DDawn_DIR=/home/free/code/milohax/dc3-decomp-deps/dawn/lib/cmake/Dawn
cmake --build build --target rb3-render

# ★ the venue root
./build/rb3-render /home/free/code/milohax/rb3/orig-assets/xbox-zip OUT --frames 1 \
    world/venue/small_club/small_club_01/gen/small_club_01.milo_xbox

# the stream oracle (1 = anomalies only, 2 = full offset trace)
RB3_STREAM_AUDIT=1 ./build/rb3-render ... world/venue/.../small_club_01.milo_xbox

# the refuted BandCamShot bind
RB3_BIND_BANDCAMSHOT=1 ./build/rb3-render ...   # -> rc=134

# rider 1: whole character, posed
./build/rb3-render /home/free/code/milohax/rb3/orig-assets/xbox-zip OUT --frames 1 \
    --clips char/crowd/anim/gen/female_base.milo_xbox \
    --clip crowd_reaching_01 --beat 4.0 --dist-scale 1.35 \
    char/crowd/gen/crowd_female01.milo_xbox

# rider 2: the control -- body only, BIND pose, slivers still there
./build/rb3-render /home/free/code/milohax/rb3/orig-assets/xbox-zip OUT --frames 1 \
    --dist-scale 1.35 --only-mesh female_crowd_body \
    char/crowd/gen/crowd_female01.milo_xbox
```

⚠ Write flags out in full (X4c §7.5: zsh does not word-split unquoted expansions,
and the failure is silent in both directions).

---

## 10. Recommended X4e shape

1. ★ **When an instrument returns *nothing*, that is the measurement.** Zero
   `ReadDead` anomalies across an entire venue eliminated every object `Load()`
   in the tree in one run and pointed at the machinery *between* objects. A
   negative result from a well-chosen absolute oracle is worth more than a
   positive one from a vague one — but only if the oracle would have spoken.
   State in advance what a clean run would rule out.
2. ★ **Measure the decision, don't reason about the shape.** Two of my own
   hypotheses (§7.4, §7.5) were structurally real divergences between the native
   and retail arms — genuinely wrong-looking code — and both were *inert on this
   data*. One `printf` of the actual value separated them from the live defect,
   which was a plain boolean guard one line away and looked like nothing.
3. ★ **Prefer a control that removes the variable to an argument about it.** The
   sliver question was settled in one run by rendering **bind pose** — no clip at
   all. Everything else (weights, dets, histograms, prop isolation) was
   supporting detail.
4. **The venue milestone was never a build-system milestone.** Four documents
   costed it as 13 TUs + a `ScatterIncludes` lane + 14 registrations. It was one
   guard. The build-system lane is still right — for **content** (characters,
   shots, audio), which is what §5 shows is actually missing.
