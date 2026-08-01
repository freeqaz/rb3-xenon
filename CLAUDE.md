# rb3-xenon — Claude Context

## ★ Think out loud (applies to every turn, every agent)

**Narrate your work in visible output as you go.** Say what you are about to do,
why you chose it over the alternative, what you expect to see, and then what you
actually saw — *before* moving on. Do not batch a silent run of ten tool calls
and emit only a conclusion.

**Why: the log is the deliverable.** This work is reviewed after the fact —
weeks later, by someone (or some agent) who was not there and cannot re-run the
session. The transcript is the only record of *why* a lever was chosen, what was
measured, and what was ruled out. Hidden reasoning is not recoverable; only the
visible trajectory and narration survive. So write for that reader: a correct
patch with no narration teaches nothing and cannot be audited, while a narrated
dead end stays useful forever — it stops the next lane from re-hunting a drained
vein.

What "out loud" means concretely:

- **Before a tool call / batch:** one line on intent — *"reading `splits.txt` to
  confirm the `.text` span before pinning"*, not silence.
- **Before a decision:** name the alternatives you rejected and the reason.
  *"Doing this in a worktree rather than main because the header change cascades
  to ~281 TUs."*
- **State your expectation first, then the result.** A prediction that fails is
  the most informative line in a transcript — call it out explicitly (*"expected
  +12, got 0 — so the cause is shared, not per-unit"*).
- **Surface surprises immediately**, especially ones that contradict a memory,
  a doc, or an earlier claim of yours. Say what changed your mind.
- **Say what you did NOT do** and why — deferred walls, skipped units, unverified
  assumptions. Silence reads as coverage.
- **Numbers with provenance:** quote the measured value and how it was measured
  (which leg, which build, cache cold/warm). "Improved" without a number is noise.

This applies to subagents too — a subagent's final report is the only thing that
survives it, so it must carry the reasoning, not just the verdict. Keep it prose
and proportional: a sentence or two per step, not an essay per tool call.

---

Decompilation of **Rock Band 3** for **Xbox 360** (PowerPC). Goal: matching
machine code from C++ source. Target binary: vanilla retail XEX, title ID
`45410914`, at `orig/45410914/default.xex` (not committed).

**Docs index: `docs/INDEX.md`** — audited master index of every doc under
`docs/` (2026-07-06): current references vs `[HIST]` frozen records, plus a
"known traps" box. Check it before trusting any doc's current-state claims;
stale/boilerplate docs carry a `> **STATUS (2026-07-06):**` banner.

**Optimization level: `/O1 /Oi /GR /EHsc` (+ implicit `/fp:fast`) — retail
size-optimized release, no LTCG. ✅ VERIFIED AGAINST RB3 RETAIL (lane CB-4,
2026-07-30), no longer inferred from dc3.** Every one of seven perturbations
scored *worse* and none scored better, so the "metric-fitted build config" hazard
never arose (that hazard only bites when you have to interpret a **gain**):

| evidence class | finding |
|---|---|
| `??_R4` Complete Object Locators in retail `.rdata` | **2,220** ⇒ **`/GR` ON** |
| `FuncInfo` magic `0x19930522`, `EHFlags == 1` (`FI_EHS_FLAG`) on **all 8,541** | **`/EHs*` synchronous** — not `/EHa`, not EH-off |
| Rich header: 401×`0x00AA` + 1,760×`0x00AB` plain C/C++ (a real `/GL` obj has machine `0x0000` and **no** `@comp.id`) | **no LTCG — now verified, not inferred** |
| `bl __security_check_cookie`: **12** sites in one 24 KB span (DC3: **599** over 6 MB) | **`/GS` OFF** for Harmonix code |
| whole-binary A/B (all legs on the correct 10224 compiler) | `/O2` **−13,813** matched / −29.00 pp · `/Ou` −14,255 / −29.17 pp · `/GS` −598 · `/Oi-` −465 · `/fp:precise` −47 · `/EHs` (dropping the `c`) −43 |

⚠ **The era-inference was right about the flags and wrong about the compiler
build** — RB3 retail used cl **10224**, dc3 used **11886** (see
`docs/decomp/xdk-11164-compiler.md`). Don't let the flags being confirmed
retro-justify the reasoning; it was the same reasoning either way.

★★★ **`/GR` can NEVER be settled by a match-% sweep: `/GR-` is `.text`-byte-identical.**
Only the `.rdata` COL evidence answers it — which is why the binary-evidence class
was mandatory rather than a nicety. ⚠ And RTTI **type-name strings are NOT a `/GR`
test**: `/GR-` drops `??_R4` 44→0 but `??_R0` only 73→40, because EH also emits
them. Two control failures shaped this result — the first `/GS` cookie detector
was **vacuous** (0 hits on a known-`/GS` object, because it assumed `lis`/`lwz`
adjacency and the compiler schedules two instructions between them) and was caught
before it could produce a false "retail has no `/GS`".

Also measured: `/Gy` and `/GF` are byte-identical to default (already on), `/Oy`
is a **no-op on PowerPC**, `/Ob2` is byte-identical to `/O1` (validating the claim
below), and `@comp.id` is unchanged across **all 22 flag legs** ⇒ the Rich header
encodes **no** flag information (a useful bound on that instrument).

⛔ **Still unsettled:** the `c` in `/EHsc` rests on the metric alone (−43); the
per-function EH instrument **failed to discriminate** (TIGHT 99 under both). And
the Rich header's 401 C vs 1,760 C++ objects implies a `/TC`//`/TP` split nobody
has examined.

★ **Per-TU flag heterogeneity is no longer unexplored — it is MEASURED** (lane
CF-4, 2026-08-01), with a control:

| finding | value |
|---|---|
| `/Od` region (Quazal NetZ) | **`0x82A6D168`–`0x82B54190`**, 5,782 fns / 933,792 B; 2,352 flagged `/Od` (61.8% of bytes) |
| separate `/Od` island **inside** HMX code | `keygen_xbox.s` @ `0x82724A90` |
| control: named HMX fns below `0x82A00000` flagged | **0 / 13,676** (MasterAudio 0/57, BandDirector 0/117, BandCharacter 0/165) |
| enrichment | ≈ **413×**, exactly one false positive |

⇒ **`/Od` is OBJECT-granular, not address-range** — the `keygen_xbox` island proves
it. Two corrections to this doc's own prior claims: the **`0x82A00000` boundary is
~450 KB too low**, and **"vendor ⇒ `/Od`" is FALSE** (zlib `inflate`/`deflate` are
textbook `/O1`).
⚠ The detector only became trustworthy once the **untreated-population control**
was run — the raw "unoptimized score" alone fires on any large `/O1` function under
register pressure, i.e. it confirms whatever you point it at.
⚠ Side-findings for a map lane, **flagged but unverified**: nine named engine units
each claim exactly one function inside the Quazal block, and `CameraManager.s`
claims 13 at `0x82B02748`–`0x82B031A0` — almost certainly bad `.text` pins reading
as false 0%. Also **4,364 addresses (3.58%) where two `.s` files disagree** (stale
TU0-era generations under `build/45410914/asm/`) — **any asm-wide scan must filter
to files newer than 2026-07-15** or the address axis is garbage.

**Crucially, no whole-program optimization** — TU spatial grouping in `.text` is
preserved (empirically: the MasterAudio.cpp cluster of 46 functions packs into
8 KB).

What `/O1` does: `/Ob2` aggressive within-TU inlining (so leaf math like SHA1's
K-constants disappear into callers), but *no cross-TU reordering or whole-program
inlining*. dc3's linker map tags ~32k functions `f i` — those are `/Ob2` inlines,
not LTCG magic.

★ **ICF is a LINKER option (`/OPT:ICF`), NOT a `/O1` effect** — this doc said
otherwise for months. **Retail RB3 does have it**, verified on `band.exe` (lane
CD-7, 2026-07-31): over 40,609 non-funclet `.pdata`-sized functions, bodies
identical **including call targets** show only **51 surplus copies — *below* three
random-offset nulls (115/158/170)** — while bodies identical **ignoring** call
targets survive **3,967 times in 1,061 groups**. That ~78× gap *is* the signature
of relocation-restricted folding; full folding would collapse both populations and
no folding would leave both intact. Cross-binary positive control: the same
scanner on **DC3 — proven ICF-ON by its leaked map** — produces the same
signature (28 reloc-identical vs 4,941 shape-identical surplus).

⇒ **MSVC folds only COMDATs identical *including relocations and associated
`.xdata`*.** So byte-*similar* bodies at distinct addresses are **expected and do
NOT refute ICF** — e.g. `_List_base<T>::clear` has 42 addresses, reloc-identical
surplus **0**, because its members differ in four `bl` targets (per-`T` node
deallocators). Folding is near-total in HMX code (**6** surplus / 32,580) and
**17× weaker in vendor/CRT code ≥ `0x82A00000`** (25 / 8,029) — a concrete
instance of the per-TU flag heterogeneity flagged as unexplored above, likely
`/Gy`-off monolithic non-COMDAT `.text`.

⚠ **Instruments structurally INCAPABLE of settling ICF** (same trap as `/GR`):
match-%/objdiff (`report.rs` masks reloc args — a folded callee and a wrong callee
score identically), and raw `memcmp` for duplicate bodies (**silently vacuous**:
PC-relative `bl` displacements differ at different addresses, so identical
functions are *not* identical bytes — this would "prove" ICF by finding nothing).
The instrument that works is relocation-normalized body hashing over
`.pdata`-authoritative extents, split reloc-identical vs shape-identical, against
a random-offset null.

⛔ **The "71.5% of `name_check` sites are ICF fold-aliases ⇒ NOISE" model does NOT
survive** — and for a reason *independent* of ICF being real. "Callee absent from
map ⇒ fold-alias" never measured folding; it measured **identification coverage**.
The map names 27,515 of 66,003 functions (**41.7%**), and a null shows **36.8%** of
*all* call sites have a callee absent from the map vs 71.6% in the charged stratum
— an enrichment of only **~1.95×**, used as if it were a deterministic classifier.
Retail-byte adjudication refuted **641 pairs / 2,131 sites** the census called
noise, and only **~28%** of the stratum is explained by a proven fold. The honest
name for that stratum is *"our callee has no identified retail address"* — a
**triage backlog**, not noise (3,776 distinct pairs, top 100 = 46.2% of sites).
⚠ This does **not** by itself reopen the `name_check` default decision; that rests
on the separate finding that name_check *aggregate* code% is build-unstable.

The asymmetry between binaries is **not** optimization level — it's that dc3 had
a leaked PDB/.map giving its functions names+addresses, while RB3's are
anonymous `fn_8XXXXXXX`. dc3 is therefore a **Rosetta Stone** for retail Milo
(same flags, named functions): match RB3 by transferring dc3's labels via
shared string content (`tools/fingerprint_match.py`) or structural similarity
(Ghidra+BinDiff).

## Source provenance (important)

Two sibling repos feed this one. Pick the right source per directory:

- **`src/system/` (Milo engine) ⟵ `../dc3-decomp`.** Dance Central 3 is the
  *same Milo engine*, already 360-ported (uses `RTL_CRITICAL_SECTION`,
  `xdk/XBOXKRNL.h`, etc. — not Wii's `revolution/OS.h`). Its engine headers and
  `.cpp` files compile under the 360 toolchain, so they are the correct base.
  Compiled with the same `/O1 /Oi /GR /EHsc` retail flags as us; the leaked
  PDB/.map (`ham_xbox_r.map`) gives it named functions — that's *our* asymmetric
  advantage, not a cleaner build.
- **`src/band3/`, `src/network/` (RB3 game code) ⟵ `../rb3` (rb3-Wii **DEV**
  decomp).** Important: `../rb3` is the Wii *development* build's decomp, not
  retail. It retains `MILO_ASSERT` source-path strings and named functions that
  the retail Xbox build stripped — a **richer source oracle** for cross-binary
  identification. Wii-targeted (MWCC PowerPC), needs Wii→360 porting.
- **`src/xdk/`, `src/system/stlport/`, toolchain ⟵ `../dc3-decomp`.**

**Caveat (from the project owner):** dc3-decomp is *newer* than RB3. Its engine
code may have subtle behavioral differences or version incompatibilities. When a
file misbehaves, cross-check against rb3-Wii's equivalent and merge intent — do
not assume dc3's version is correct for RB3.

⚠ **`MILO_DEBUG` is force-defined tree-wide (`src/macros.h:3`) and it does NOT
gate `MILO_ASSERT`** — the whole `MILO_*` family is `#ifdef HX_NATIVE`, which the
match build never defines (cflags carry **no `/D` at all**), so
`MILO_ASSERT(cond,line)` is just `((void)(cond))`. The force-define's only effect
is to switch ON rb3-Wii **dev-build** code that retail compiled out, so every
inherited `#ifdef MILO_DEBUG` is a suspect. **Fix per-site with the house
pattern** `#if defined(MILO_DEBUG) && defined(HX_NATIVE)` (keeps native
behaviour; see `os/Timer.h`, `obj/ObjMacros.h`, `utl/Loader.h`) — ⚠ **never
blanket-remove**: the measured whole-binary control is **−21** (some guards are
genuinely in retail). Details + census:
`docs/decomp/patterns/milo-debug-force-define.md`.

## Decomp priority: the GAME, not the engine

**Spend matching/porting effort on RB3's game layer (`src/band3/`, `src/network/`),
NOT on the Milo engine (`src/system/`).** The engine is effectively pre-solved:
DC3 is the same engine on the same platform (Xbox 360), and we verified that DC3's
already-decompiled engine **loads and renders RB3-360 `.milo_xbox` assets** with
zero rb3-xenon code (same texture tiling / vertex compression / endianness; DC3's
milo loaders keep backward-compat parse branches for RB3's older revisions). A
3-way `rndobj` cross-check shows RB3-360 ≈ DC3 on every divergence point (NgRnd,
BaseMaterial, MetaMaterial, atlas particles, FontMap3d, Matrix4, `rnddx9`); only
**rb3-Wii** is the outlier (its `rndwii`/GX branch). So the renderer, materials,
textures, mesh/skeleton load are all supplied by DC3 — the part that's actually
RB3-specific, and where decomp value concentrates, is the game code.

Full evidence + the asset-render experiment + the "bigger play" (a native target
that injects DC3 rndobj + only RB3 game code) are in
`docs/plans/engine-reuse-and-asset-rendering.md`.

## Git & worktrees (concurrent agents) **important**

**Assume other agents are working in the main repo right now.** The main
working tree is shared, so any command that mutates tracked files or the index
out from under them will *deeply break* concurrent work. Hard rules:

- **Never `git stash` in the main repo.** It silently yanks everyone's
  uncommitted changes. To compare a change against `HEAD` or another commit, do
  it in a worktree, not by stashing.
- **Never `git checkout`/`git restore`/`git reset --hard` *files* in the main
  repo** to discard or swap working-tree content. Another agent's in-flight edits
  to that file would be destroyed. (Switching branches is also off-limits in the
  shared tree — use a worktree.)
- **Do your isolated/experimental work in a git worktree.** A bare
  `git worktree add` is *unbuildable* here — the build inputs and toolchain are
  gitignored (`build/`, `orig/*`, `build.ninja`, `objdiff.json`). Use
  **`scripts/setup_worktree.sh [path] [branch]`** to get a buildable + diffable
  worktree in seconds via btrfs CoW reflinks: it reflinks `orig/` and
  `build/45410914/` (a *private* warm-cache build dir — never a symlink into
  main, so the worktree's build can't corrupt the shared one), symlinks the
  read-only toolchain, baks absolute tool paths into the worktree's
  `build.ninja` via `configure.py`, and primes ninja state. A fresh **warm**
  worktree also **seeds main's `.ninja_log`/`.ninja_deps`** so its first full
  `ninja` is a true 0-compile no-op instead of recompiling all ~745 objs. Seeding is gated: it only
  happens when main is clean (no `src/`/`config/` or `configure.py`/
  `tools/project.py` diffs), the worktree's msvc rule blocks are byte-identical
  to main's, and main's deps are uniformly repo-root-relative; if any gate
  fails it auto-skips with a printed reason and falls back to the old
  rm-and-rebuild path. (Portability requires the PCH `/Fp` path to stay
  repo-root-relative — see `tools/project.py`; an absolute `/Fp` would make the
  PCH command worktree-specific and re-trigger the full cascade.) Add
  `--cold-cache` for a guaranteed-clean A/B baseline — seeding is disabled on
  that path, so cold baselines stay honestly cold. Remove with
  `git worktree remove --force <path>`.
- **Put worktrees + all scratch under `~/tmp` (= `/home/free/tmp`), NEVER `/tmp`.**
  `/tmp` is a RAM-backed **tmpfs** (47 GB, shared across everything, fills fast —
  we hit "Disk quota exceeded" mid-build this way), *and* tmpfs has no btrfs
  reflink, so `setup_worktree.sh`'s CoW fast-path silently falls back to full
  ~660 MB copies there. `~/tmp` is on the **same btrfs as the repo** → CoW
  reflinks work (cheap, fast) and there's ~300 GB+ free. So
  `scripts/setup_worktree.sh ~/tmp/wt-foo foo`, build logs to
  `~/tmp/rb3_build_{task}.log`, etc. (The harness's own task/transcript files
  already live under `~/tmp` — follow suit for worktrees and logs.)
- The orchestrator MCP manages a pool of these worktrees
  (`scripts/orchestrator/worktree_pool.py`) for its agents; `setup_worktree.sh`
  is the same machinery you can drive by hand.

## Two build tracks

**1. X360 decomp-matching build** — compile-to-match the retail XEX (MSVC X360).

```bash
./tools/ninja-locked 2>&1 | tee ~/tmp/rb3_build_{task}.log
python3 configure.py     # regenerate build.ninja (after editing objects.json/splits.txt)
```

**ALWAYS `tee` the build output to a log file** (`~/tmp/rb3_build_{task}.log` or
similar — use `~/tmp`, not the RAM-backed `/tmp`; see the worktree note above).
Makes debugging easier.

dtk is the local **jeff** fork at `../jeff`; **objdiff is also a local fork**
at `../objdiff` (freeqaz/objdiff, with custom pattern-detector work and
normalized-diff changes). `configure.py` resolves BOTH to their **prebuilt
release binaries** (`<fork>/target/release/{dtk,objdiff-cli}`, absolute paths)
— there are no cargo build edges in build.ninja anymore. That absolute-path
parity (main and worktrees bake the identical command strings) is what enables
warm-worktree command-hash reuse.

The compiler wrapper is the **freeqaz/wibo fork** release binary at
`/home/free/code/milohax/wibo/build/release/wibo` (configure.py resolves it by
default; the old `build/tools/wibo` download edge is gone — it used to fetch
stock upstream wibo and silently disable the FS cache). configure.py
**hard-fails** on a wrapper binary lacking the fork's `WIBO_FS_CACHE` /
`WIBO_REWRITE_SHOWINCLUDES` feature bytes — intentional (a stock binary would
silently corrupt `deps = msvc` dependency tracking), not a bug.
`tools/transform_dep.py` is no longer in the msvc rule (wibo rewrites
`/showIncludes` output in-process).

**wibo staging discipline (important).** The live `build/release/wibo` is invoked
by *every* MSVC compile in main AND all worktrees, and now includes the
residual-perf merge (readlink storm fix, scoped negative-exists cache, in-process
stats reporter, `current_path()` memo). Because a stock/regressed wibo silently
corrupts obj bytes fleet-wide, **never overwrite `build/release/wibo` directly**:
rebuild to `build/staging/wibo`, run the byte gate (compile ≥3 TUs with the staged
vs live binary → 0 differing bytes beyond the COFF timestamp), then atomically
swap (`mv release/wibo <backup>; mv staging/wibo release/wibo`). The binary is not
a ninja input, so the swap triggers no recompiles — which is exactly why a bad one
goes unnoticed until objs are wrong. Rollback = `mv <backup> release/wibo`.

**objcache — shared content-addressed MSVC object cache.** Sibling Rust repo at
`/home/free/code/milohax/objcache` (rebuilt manually like jeff/objdiff:
`cargo build --release`). Every `msvc`/`msvc_pch`/`msvc_pch_create` rule is
prefixed with `objcache exec --fo $out -- <wibo> cl.exe …` (resolved to the same
absolute path in main and every worktree via `configure.py`'s `_find_local_fork`,
so command strings stay byte-identical → warm-worktree hits). configure.py
**hard-fails** if the binary is missing (`RB3_OBJCACHE_OPTIONAL=1` opts out to a
plain uncached-but-correct compile). Cache lives at `~/.cache/rb3-objcache`
(config, manifests, objects, stats).
- **What it buys:** a cold `rm -rf build/45410914/src && ./tools/ninja-locked` is
  now all-cache-hits — measured **3.5 s / 778 hits / 5 misses** vs a full ~5-min
  recompile. Cold A/B baselines and fresh worktrees are near-free.
- **Kill switch (no re-log):** `objcache off` (or `OBJCACHE=off` env) → the prefix
  stays in the rule but the binary passes straight through to the real compiler.
  `objcache on` re-enables. `objcache stats [--reset]`, `objcache gc --max-size 10G`.
- **Correctness model:** key = compiler-DLL identity + full cflags + source +
  validated dep-closure hashes (+ PCH identity for PCH TUs); any anomaly (e.g. a
  missing dep file) → passthrough, never a stale serve. Served objs are
  byte-identical to a real compile except (a) the 4-byte COFF timestamp (zeroed on
  hits) and (b) for cross-root hits, the single embedded `/Fo` path string — both
  match-irrelevant (whole-binary `matched_functions` holds equal through all-hits
  rebuilds). objcache also normalizes recorded deps to **repo-root-relative** (no
  absolute src paths in `ninja -t deps` → enables warm-worktree `.ninja_deps` seeding).
- **Resolved gotcha (7956af7):** `<memory.h>` used to have no real match on the
  include path, so wibo's case-insensitive resolve fell through to the game's
  `src/Memory.h` and recorded a lowercase dep that doesn't exist on Linux —
  leaving 5 soundtouch TUs perpetually ninja-dirty. Fixed by adding the CRT
  compat header `src/xdk/LIBCMT/memory.h` (defers to `string.h`); LIBCMT
  precedes `src` in the include order so it wins with exact casing. Lesson: a
  header include that only resolves case-insensitively will churn forever —
  give it a real exact-case target on the include path.

> **Editing the jeff/objdiff/wibo/objcache sources? Rebuild the release binary
> manually** — ninja no longer builds or tracks the tools:
> `cargo build --release` in `../jeff`, `../objdiff`, or `../objcache`;
> **`cmake --preset release64 && cmake --build --preset release64`** in `../wibo`.
> None of these binaries is an implicit ninja input, so a tool rebuild
> does not retrigger compiles (wibo is byte-neutral to objs — verified fork
> vs stock objs = 0 differing bytes; objcache serves timestamp-only-different objs).
>
> ★ **`release64`, NOT `release` — this doc said `release` for five months and it
> is the wrong architecture.** `CMakePresets.json`'s `release` preset uses the
> `i686-linux-gcc.cmake` toolchain (32-bit), but the deployed
> `build/release/wibo` is `ELF 64-bit x86-64`; `release64` uses
> `x86_64-linux-gcc.cmake`. Building `release` produces an i686 binary that has
> been broken since February — and chasing that breakage cost two lanes real
> budget on 2026-07-30, one of which mis-attributed it to a clang-22 upgrade.
> ⚠ Note the confusing layout: the *output directory* is `build/release/`
> regardless of which preset you used, so the path does not tell you the arch —
> check with `file build/release/wibo`.

**2. Native engine build** (`native/`, x86_64 Linux + clang) — runs the engine
on the host. Currently boots headlessly and loads RB3 `songs.dta`.

```bash
cd native && cmake -S . -B build -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build build && ./build/rb3-dta <songs.dta>
```

Full native-port build recipe, hard-won lessons, and link-stub regeneration are
in Claude's memory: `project_native_port.md`. Highlights: needs dc3's dual-target
`types.h` (LP64 int-width vs Xbox ILP32 long); `Symbol::Init()` mandatory before
interning; rb3-xenon's RB3 engine is more complete than DC3's so several dc3
`_Native` shims are redundant; ~74 rendering/MIDI/synth/Win32 symbols off the DTA
path are satisfied by weak stubs in `native/src/dta_link_stubs.s`.

**Architectural goal:** the native engine should eventually be **extracted into a
standalone `../milo-native-engine`** repo — a shared native Milo runtime consumed
by the per-game decomps (rb3-xenon, dc3-decomp, …) rather than each carrying its
own `native/`. For now it lives in `native/` and borrows from `../dc3-decomp`.

## Build wiring

- `tools/defines_common.py` — include paths. **STLport must come first**, then
  `src/xdk/LIBCMT` (C CRT), then `src`, `src/system`.
- `config/45410914/objects.json` — declares which `.cpp` files to compile and
  their match status. New files: add here as `NonMatching`.
- `config/45410914/splits.txt` — pins per-object section ranges so dtk emits
  per-unit target `.obj` + `.s` for objdiff to compare against. Pin **just
  `.text`** for a new cluster; on next `ninja` (after `touch config.yml`) dtk
  auto-derives and back-fills the matching `.pdata` range. Other sections
  (`.rdata`, `.data`) need manual pinning if the TU has them.
  ★`.pdata` lines are **derived output, not input**: every split run clears the
  entire `.pdata` split set and re-derives one range per `.text` block (jeff
  `split.rs:1035` `split_pdata`), then rewrites splits.txt. Measured: 54
  deleted `.pdata` lines regenerated byte-identical; a hand-made `.pdata`
  overlap silently healed. So move/edit only `.text` — `.pdata` follows
  automatically. If `.pdata` lines *don't* regenerate, the split run itself
  failed (check the log — e.g. symbols.txt-drift "ends within symbol"); the
  derivation never selectively skips.
  ★A move that drains a unit's **last `.text` block** must delete the unit's
  whole splits.txt entry in the same edit: an empty unit still emits a 42-byte
  `obj/<unit>.obj` — the split succeeds, then `report.json` hard-fails with
  `Failed to open <unit>.obj: Invalid COFF/PE section headers` (verified; with
  the entry removed the full build passes).
- `tools/project.py` — patched so objects in `objects.json` get a compile edge
  even without a `splits.txt` address range (compile-only scaffolding).
- `tools/fingerprint_match.py` — function-identification tool (extract / report
  / autoid / identify subcommands). Indexes all 66,838 RB3 functions by
  referenced strings/callees/constants; cross-refs strings against
  `../rb3/src` + `../dc3-decomp/src` to propose source-file mappings. Generates
  `fingerprints.json` + `autoid.json` (gitignored, regenerable). See
  `project_function_identification.md` in Claude's memory.
- `src/` include style mirrors rb3-Wii: `#include "math/Vec.h"` resolves via
  `/I src/system`. **Beware include shadowing:** `/I src` precedes
  `/I src/system`, so a file at `src/os/Foo.h` will shadow `src/system/os/Foo.h`.
  (A stub `src/os/Debug.h` once shadowed the real engine `Debug.h` and broke
  every macro-using header — don't reintroduce stubs at `src/` root.)
- **PCH (precompiled header, dc3 port).** 9 engine dirs (`hamobj synth flow
  gesture meta obj os utl movie`, ~281 TUs under `src/system`) compile through a
  `/Yc//Yu` PCH at `build/45410914/pch/system.pch`, built from
  `src/system/decomp_pch.h` (= only `obj/Object.h` + `os/Debug.h`). Config lives in
  `configure.py` (`config.pch_header/pch_source/pch_eligible_dirs`); the
  `msvc_pch_create`//`msvc_pch` rules + PCH edge + per-object eligibility switch are
  in `tools/project.py`. `deps="msvc"` makes ninja auto-track the PCH's headers, so
  touching `os/Debug.h` rebuilds the PCH and cascades to all eligible objs (proven
  staleness chain). **Matching-safe, gated:** whole-binary `matched_functions` is
  equal PCH-on vs PCH-off (W1-C worktree A/B + re-verified on main). NOTE: unlike
  dc3, `/FI"decomp_pch.h"` here is NOT strict-`.text`-byte-identical (it perturbs
  *untracked* helper/template inlining) — that is why `char rndobj world ui` are
  **excluded** (they regressed tracked matches, net -10); do not add them without a
  fresh 3-gate A/B. **`decomp_pch.h` is codegen-load-bearing — keep it sacred**
  (Object.h + Debug.h only; native-only edits must be `#ifdef HX_NATIVE`).
  **Instant disable:** `config.pch_eligible_dirs = set()` in `configure.py` +
  `python3 configure.py` → every TU reverts to the plain `msvc` rule, byte-identical
  to pre-PCH (`decomp_pch.h/.cpp` stay, unreferenced).

## Known issues / expected noise

- dtk SPLIT prints `WARN` lines about UTF-16 strings, `PpcRel`/`PpcAddr16`
  relocations, and unaligned symbols. These are **tolerated** — jeff was patched
  to downgrade asm-write failures to warnings (see jeff `src/cmd/xex.rs`), so
  `config.json` is still emitted and the build proceeds.
- `Progress: 0.00% matched` is the current baseline. Denominator is the **whole
  binary** (11,790,708 code bytes / 66,003 functions), so this is the honest
  dc3-comparable metric — there's no denominator gaming. Matches register only
  when a unit has both (a) pinned section ranges in `splits.txt` and (b) a
  compiled `.obj` that objdiff equates byte-for-byte with the dtk target `.obj`.
- **Case sensitivity:** dc3's tree has case-variant files (e.g. `vec.cpp` vs
  `Vec.cpp`) that collide on Windows but coexist on Linux. Use the name dc3's
  `objects.json` actually builds (lowercase `vec.cpp`, `mtx.cpp`).

## Matching phase (active)

The pipeline is proven end-to-end on `MasterAudio.cpp` (2026-05-26): pinning a
real `.text` range in `splits.txt` produces a dtk target `.obj` + per-object
`.s`. The remaining work is per-target: derive splits → port source so it
compiles → diff via objdiff. See `project_rb3_xenon_roadmap.md` Phase 5 and
`project_function_identification.md` in memory for state.

### Splits-bootstrap recipe (per new cluster)

1. Run `tools/fingerprint_match.py autoid` to get source-file proposals.
2. For each tight cluster (≥3% density, ≥3 corroborating strings, NOT
   `Symbols*.cpp` which is a systematic FP), compute `[min(fn), max(fn)+size)`
   = the `.text` span. Cross-check the strings against `../rb3/src` or
   `../dc3-decomp/src` to confirm cluster identity.
3. Add to `splits.txt`:
   ```
   FooBar.cpp:
       .text       start:0xAAAAAAAA end:0xBBBBBBBB
   ```
4. `touch config/45410914/config.yml && ninja`. dtk emits
   `build/45410914/asm/FooBar.s` + `build/45410914/obj/FooBar.obj` and
   auto-derives the matching `.pdata` range (back-filled into `splits.txt`).
   This is not a one-time backfill: `.pdata` is re-derived from the `.text`
   splits on **every** split run — never hand-edit or hand-carry `.pdata`
   lines (see the splits.txt bullet in Build wiring for the two ★ traps).

### Obj patchers (WIRED)

`scripts/` holds dc3's MSVC object patchers. They rewrite the COFF **symbol
table** (not machine code) of `.obj` files to neutralize build-environment-specific
naming that MSVC bakes in, so objdiff compares real code rather than naming noise.
They are **wired and active** in `configure.py:284-368` (`config.custom_build_steps`,
mirroring dc3 `configure.py:294-357`):

- **pre-compile** — `obj_target_symbol_renamer` rewrites the dtk-split *target* obj's
  anonymous `fn_<addr>` symbols to MSVC mangled names from
  `scripts/target_symbol_map.json`, so objdiff can pair target↔base by name. (Game
  entries are generated by `tools/gen_game_target_map.py` from the rb3-Wii oracle;
  without a map entry a pinned game TU reads a false 0%.)
- **post-compile** (on our compiled obj) — `anon_ns` (anonymous-namespace hashes,
  which MSVC derives from machine name + source path), `dynamic_init` (`??__E`
  STATIC→EXTERNAL), `guard` (`$S`→`??_B` static-init guards), `bool_mangle` (bool
  back-ref mangling), `atexit_scope` (`??__F` scope counters).

`regswap` + `transplant` exist in `scripts/` but are **not** in the wired list (enable
per-function when needed). The "guard-thunk wall" that drags game-unit fuzzy down
(retail emits `??__E`/`??__F`/guard thunks our objs don't pair) is what these address.

### Identification tooling

- `tools/fingerprint_match.py` — see Build wiring above. Generates the
  identification table used to derive splits.
- Cross-binary identification (planned): Ghidra + BinDiff transfer dc3's
  named functions (from leaked `ham_xbox_r.map`) onto RB3's anonymous
  `fn_8XXXXXXX` by structural similarity. BinDiff installed at
  `/usr/bin/bindiff`; BinExport plugin ships at `/opt/bindiff/extra/ghidra/`;
  XEXLoaderWV source cloned at `/home/free/code/milohax/XEXLoaderWV/` (needs
  rebuild for Ghidra 12.1 — installed prebuilt is 12.0.1).

## Orchestrator MCP, Ghidra MCP, skills

Ported from DC3 (2026-05-27). Both projects share the MSVC X360 toolchain so
most tooling transfers verbatim.

**Orchestrator MCP** (`.mcp.json` → `scripts/orchestrator/`):
- Server name: `decomp`. Backed by `decomp.db` (SQLite, 66k functions seeded
  from `build/45410914/report.json` via `scripts/ingest_report.py`).
- 11 tools: `report_result`, `query_functions`, `get_attempts`, `lookup_rb3wii`
  (greps `~/code/milohax/rb3/src` — RB3 Wii dev decomp, named functions),
  `lookup_dc3` (greps `~/code/milohax/dc3-decomp/src` — same compiler twin),
  `run_objdiff`, `run_analyze_function`, `run_diff_inspect`,
  `lookup_struct_offset`, `lookup_merged_symbol`, `mark_patch_result`.
- Worktree pool (`scripts/orchestrator/worktree_pool.py`) tracks per-agent
  worktrees in `decomp.db.worktrees`. Set up via `scripts/setup_worktree.sh`.
- Python env: symlinked `venv` → `../dc3-decomp/venv` (shared deps: `mcp`,
  `pyghidra-mcp`, etc.). Regenerate DB anytime with
  `venv/bin/python scripts/ingest_report.py build/45410914/report.json`.

**Ghidra MCP** (`tools/ghidra/pyghidra-service.sh`):
- Port **8002** (DC3 owns 8000, rb3-Wii owns 8001).
- Project at `ghidra_projects/RB3Xenon/RB3Xenon` (build via
  `tools/ghidra/import-xex.sh` — single-pass full analysis, no leaked .map).
- Uses VMX128 SLEIGH fork at `/home/free/code/milohax/ghidra/build/ghidra/`
  (same Ghidra build DC3 uses).
- Python client: `tools/ghidra/mcp_client.py` — default URL
  `http://127.0.0.1:8002/mcp`, session cache at
  `/tmp/claude/ghidra_mcp_session_rb3xenon.txt`.
- Sub-tools: `pcode_inspect.py`, `code_search.py`, `struct_check.py`,
  `ghidra-decompile.py`, `ghidra-search.py`, `ghidra-xrefs.py`,
  `ghidra-callgraph.py`, `batch_export.py`.

**Skills** (`.claude/skills/`, 24 total): batch-check, compare-asm, data-diff,
ghidra-{decompile,search,struct}, permute, progress, recon, refactor-staff,
resolve-vcall, stack-layout, struct-info, vtable, dc3-pair (primary engine
oracle — DC3 is the closest twin), rb3wii-pair (game-code oracle — richer
named-function source). All ported with port 8002 + title-ID 45410914
substitutions applied.

**Analysis engine** (`scripts/analysis/diff_inspect.py`, 1969 LOC): modes
`diagnose`, `clusters`, `regswaps`, `offsets`, `replaces`, `compare`,
`save_baseline`, `mismatches`, `stack-layout`, `asm_listing`. Backs the
`/compare-asm` + `/stack-layout` skills and the MCP `run_diff_inspect` tool.

**Struct + vtable** (`tools/struct_db.py`, `scripts/dump_vtable.py`):
`// 0xHEX` annotated headers → `struct_db.sqlite`; COFF `??_7*@@6B` vtables
decoded with `??_R4` RTTI Complete Object Locator parsing.

★ **Class layout: ask the compiler, not the comments.**
`scripts/harvest/class_layout_report.py <Class>` wraps
`cl.exe /d1reportSingleClassLayout<Class>` (an undocumented MSVC flag that works
through wibo) and prints real offsets, explicit padding rows, vtable slots with
the supplying class, and `this` adjustors. It is **authoritative**; the `// 0xHEX`
header comments — and `struct_db.sqlite`/`lookup_struct_offset`, which are
*derived from those comments* — are measurably wrong in places (`CharEyes.h`: 20
wrong offsets; `SaveLoadManager.h`: uniformly +4 stale). `lookup_struct_offset`
now consults the compiler by default (`verify=true`, `project_dir=<worktree>`) and
labels comment-derived answers **UNVERIFIED**. `--check-header` audits a header's
comments against the compiler; `--offset 0x118` answers "which member is here".

**MSVC pattern docs** (`docs/decomp/patterns/`, `docs/decomp/MSVC_X360_REGALLOC.md`,
`docs/decomp/TECHNICAL_NOTES.md`, `docs/decomp/PRAGMA_*.md`,
`docs/decomp/XBOX360_FLOATING_POINT_CODEGEN.md`): ported verbatim from DC3.
Same compiler, same flags → applies directly.

## Phase tracking

Memory files at `~/.claude/projects/-home-free-code-milohax-rb3-xenon/memory/`:
- `project_rb3_xenon_roadmap.md` — overall phase tracking + current state.
- `project_function_identification.md` — the fingerprint-match approach.
- `project_native_port.md` — native host engine build (separate from matching).
- `project_jeff_fork.md` — local jeff dtk fork (RB3-retail fixes).
- `feedback_verify_assumptions.md` — verify load-bearing claims via Opus
  subagent before committing to them (killed the `.xidata`-lever plan
  pre-emptively this way).
- `feedback_plans_with_refs.md` — cross-session plans must embed
  doc/file/URL references for cold pickup.
- `feedback_autonomy.md` — execute autonomously on rb3-xenon.

Live plan files (per-stream, written by planning agents) live at
`~/.claude/plans/rb3-xenon-*.md`.
