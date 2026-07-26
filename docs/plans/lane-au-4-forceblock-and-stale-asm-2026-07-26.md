# laneAU-4 — sizing two inherited veins: the `DECOMP_FORCE*` sweep and the stale-`.s` trap

Sizing lane. Worktree `/home/free/tmp/wt-laneAU-4`, branch `laneAU-4`, base
`24f508fb`. Numbers derive from the on-disk carve
(`build/45410914/config.json` + `config/45410914/symbols.txt`), the current
`config/45410914/splits.txt`, `scripts/target_symbol_map.json`, and the COFF
symbol tables of our compiled `.obj` files.

★ **Build provenance (this matters — the first pass got it wrong).** The first
version of this document read the `.obj` files **reflinked from main's build
dir**, which carried another lane's uncommitted in-flight edits. Every
obj-derived number has since been re-measured after a **clean full build in
this worktree** (`git checkout -- config/45410914/symbols.txt`;
`rm -f build/45410914/{report.cache,target_symbol_renames.stamp}`;
`touch config/45410914/config.yml`; `./tools/ninja-locked` → **1,037 edges,
EXIT=0**, log `~/tmp/rb3_build_laneAU4.log`). Effects:

| result | pre-build | post-build |
|---|--:|--:|
| Item A 30-symbol emission probe | 25 defined / 5 unemitted | **unchanged, 0 of 30 moved** |
| Item A PAYS intersection (§A.4) | 0 | **0** |
| WRONG-UNIT worklist (§A.5) | 308 | ★ **290** |
| TU0-era map survivors (§B.4) | 26 | **26** |
| Item B orphan census | 8,638 / 90 named | **unchanged** (not obj-derived) |

The 18 vanished WRONG-UNIT rows were **all anonymous-namespace symbols**
(`?A0x1e5d0754`, `?A0x055cc49d`, `?A0x147d8002`, `?A0xf8b42a02`) — the
`anon_ns` post-compile patcher rewrites those hashes, so a stale obj carries
names the map cannot match. 0 rows were *added*, i.e. staleness produced purely
additive false positives. ★ **Lesson: never run a COFF-symbol analysis against
a reflinked build dir — build first.**

**Map provenance.** `scripts/target_symbol_map.json` drifted between this
lane's base and main `620bfb21` (**374 added, 547 renamed in place**), so every
map-derived result was re-derived against *both* versions. They agree exactly:
the WRONG-UNIT set is the **identical 290 rows** under both, and §B.4's 26
survivors are identical under both. The drift lands entirely in PAYS
(19,309 → 19,683). Verified, not assumed.

Source is byte-identical between `24f508fb` and `620bfb21`
(`git diff --stat 24f508fb 620bfb21 -- src/` is empty — those commits changed
only the map, docs and one script), so these objs are what current main
produces.

| item | verdict |
|---|---|
| **A — `DECOMP_FORCE*` sweep** (laneAR §9 item 2) | ★**DO NOT FUND. Measured yield ceiling = 0 today, ≤5 ever.** The premise is wrong. |
| **B — stale `.s` trap** (laneAR §10) | Real, larger than reported (**8,638** orphans, not one file), but the *decisive* claim — "a live tool reads them" — is **narrower than feared**. Guard landed. |

---

## Item A — the `DECOMP_FORCE*` sweep is not a vein

### A.1 The site census (reconciled)

`src/decomp.h:32` gates the whole family on `!defined(__MWERKS__)`, so under
MSVC X360 **every one of these macros expands to nothing**. That part of the
framing is correct. The site count is not 145 but **143**:

| macro | sites |
|---|--:|
| `DECOMP_FORCEACTIVE` | 112 |
| `DECOMP_FORCEFUNC` | 17 |
| `DECOMP_FORCEBLOCK` | 10 |
| `DECOMP_FORCEFUNC_TEMPL` | 2 |
| `DECOMP_FORCEDTOR` | 2 |
| `DECOMP_FORCELITERAL` | **0** |
| **total** | **143** |

The inherited count of 145 came from a naive `grep "DECOMP_FORCEFUNC("`, which
also matches `src/decomp.h`'s own
`#define DECOMP_FORCEDTOR(module, cls) DECOMP_FORCEFUNC(...)` line and SongDB's
`// DECOMP_FORCEBLOCK above is a no-op` comment. Neither is a call site.

### A.2 ★ 112 of the 143 sites cannot pair a function *even in principle*

A balanced-paren parse of every `DECOMP_FORCEACTIVE` argument list shows
**112 of 112 take nothing but string literals and `__FILE__`** (assert texts,
`printf` formats, `.dta` key names — `"mesh"`, `"%s_howto"`,
`"cymbalGemCount >= 0"`, `"ui/character_creator/image/%s_keep.png"`, …).

Their entire purpose is to keep **`.rdata` string data** alive against the MWCC
linker. Progress here is counted in **matched functions**. Forcing a string
literal into existence cannot pair a function. **78% of the pool is
structurally incapable of yielding anything**, before any measurement of the
rest.

That leaves **31 function-forcing sites** (17 + 10 + 2 + 2), one of which
(`SongDB.cpp`) laneAR already converted.

### A.3 ★ The premise is wrong: MSVC does not "silently drop" these COMDATs

laneAR §9 framed the vein as *"every TU where the Wii oracle recorded a scatter
COMDAT and the MSVC build silently drops it."* It does not.

MWCC's `force_active` exists to defeat **linker dead-stripping**. objdiff
compares **`.obj` files**, not the linked image, and MSVC emits every COMDAT it
generates into the `.obj` regardless of whether the linker would later keep it.
The MWCC failure mode does not reproduce.

Measured directly by reading the COFF symbol tables of our built objects
(section number > 0 = defined here), **after the clean build**: of 30 probed
forced symbols, **25 are already defined in the owning unit's `.obj`** — `?Showing@GemSmasher@@QBA_NXZ`,
`?GetMaxSlots@TrackConfig@@QBAHXZ`, `?GetUser@Player@@QBAPAVBandUser@@XZ`,
`?HasNewAwards@AccomplishmentProgress@@QBA_NXZ`, `?HasKeys@BandSongMetadata@@`,
all three `SelectDifficultyPanel` entries, both `Cmp` dtors, all four
`UIEventMgr` predicates, `?SetType@Fader@@`, both
`?$ObjPtrList@VFader@@VObjectDir@@` template members,
`?SetPattern@TrainerGemTab@@`, `?HasSticker@PatchLayer@@`,
`?GetSetlist@SetlistRecord@@`, `?GetOwner@SavedSetlist@@`. They are odr-used
elsewhere in their own TU, so MSVC emitted them anyway. The macro is redundant.

Only **5** forced symbols are genuinely absent from our objs (identical
pre- and post-build):

| TU | symbol not emitted |
|---|---|
| `src/system/synth/SlipTrack.cpp` | `Stream::SetStereoPair` |
| `src/band3/meta_band/ProfileMgr.cpp` | `Profile::HasCheated` |
| `src/band3/game/PresenceMgr.cpp` | `vector<Symbol>::insert` |
| `src/system/beatmatch/DrumMixDB.cpp` | `vector<TickedInfo<String>>::reserve` |
| `src/system/bandobj/LayerDir.cpp` | `operator<<(BinStream&, const ObjPtr<Hmx::Object>&)` |

### A.4 ★ The intersection that bounds real yield is **0**

For a forced symbol to convert into a match it needs all three of: a
`target_symbol_map.json` entry, that entry's VA inside a pinned `.text` span
owned by the same unit, and our obj **not** currently defining the name (the
`span_predictor.py` PAYS condition, and exactly the shape of laneAR's SongDB
+1: `0x826864a8 → ??1?$vector@VVocalNote@@…`, inside SongDB.cpp's own span,
absent from our obj until the MSVC re-statement was added).

Applying that test to all 31 function-forcing sites:

* every candidate that **has** a map entry inside its owning span is **already
  defined by our obj** (`objHasIt=Y` for `?SetPattern@TrainerGemTab@@`,
  `??6@YAAAVBinStream@@AAV0@ABVSectionInfo@Stats@@@Z`, the four
  `TickedInfo<String>` entries in `DrumMixDB.cpp`, …);
* every one of the **5 genuinely-unemitted** symbols has **zero** map entries
  anywhere inside its owning unit's pinned spans.

**PAYS count = 0.** With no map entry the target-symbol renamer never renames a
`fn_XXXXXXXX` to that name, so objdiff has nothing to pair against — forcing
emission changes nothing.

### A.5 The confirmation test can fail (control run) — and my first count of it was wrong

Per the honesty rule, the same test was run across the **whole tree**, so the
test demonstrably returns positives. It just returns none for the FORCE\*
population.

★ **Correction to this lane's own first report.** I initially reported
"43 positives across 25 units". **That number was wrong.** It came from a
hand-rolled `unit → build/45410914/src/<unit>.obj` path guess that silently
`continue`d whenever the obj could not be located (`.c` units, path-form
mismatches) — so it under-counted by skipping exactly the cases it could not
resolve. Re-run through `scripts/harvest/span_predictor.py`, which builds a
definer index over **every** compiled obj, **and after a real build**, the
honest figure is **290**. Full census (map `620bfb21`, 25,165 entries; the
base-map figures differ only in PAYS):

| class | count |
|---|--:|
| PAYS | 19,683 |
| UNPINNED | 5,192 |
| **WRONG-UNIT** | **290** |

Two methodology notes, both load-bearing:

* ★ **The classification is not a tautology.** `span_predictor` unions the
  record's own `tu` key into the candidate owner set, so passing the
  range-owning unit as `tu` makes every record self-confirm PAYS. Records were
  therefore submitted under a **sentinel `tu`** that can never match a splits
  header. A 400-record control of ordinary map entries came back **396 PAYS**,
  proving the harness can return either verdict.
* ★ **Tool defect found in `span_predictor.py`.** Its `matches()` builds
  `want = tu + '.cpp'`, so **every `.c` unit is systematically mis-classified
  WRONG-UNIT**. Uncorrected the census reads **550**; 242 of those are pure
  false positives concentrated in `json-c`, vorbis, zlib and tomcrypt
  (`json_object.c` 29, `psy.c` 19, `framing.c` 18, `res0.c` 18, …). An
  extension-agnostic stem comparison removes all 242 (post-build the raw count
  is 532 vs the corrected 290). Reported, not patched —
  `span_predictor` is shared landing-decision tooling and a change to it wants
  its own A/B. **Any lane reading a WRONG-UNIT count off this tool today is
  reading a number ~78% too high for C units.**

★ **And the characterisation was wrong too.** "Plain map mispairs" is not what
these are. Of the 290, **276 have no definer anywhere in the tree** — the
symbol is absent from every obj we compile (XDK entry points like
`XShaderPDBBuilder_AddSourceFile` and `ST_SetTrackingMode`, `__unwind$` funclets,
anonymous-namespace helpers, and game symbols whose home TU is not wired). Those
are a **source/wiring** worklist, not a repoint worklist. Only **3** have a
single unambiguous definer, and **11** have several.

The enumerated artifact is committed at
**`docs/plans/lane-au-4-wrongunit-map-worklist.json`** (290 rows: `va`, `name`,
`pinned_owner_unit`, `cls`, `n_definers`, `definer_units`, `unique_definer`,
`va_status`, `note`).

★ **The 3 "unambiguous repoint targets" cannot be repointed as they stand** —
all three are also in §B.4's interior list, i.e. the map VA is not a live
function start:

```
0x82276908  ?OnPlayerSaved@TrackerManager@@   pinned to CharBonesMeshes.cpp, defined in band3/game/TrackerManager
0x8237fbd8  ?SetCharacter@HamPlayerData@@     pinned to CharClip.cpp,        defined in system/hamobj/HamPlayerData
0x82699410  ?SetPaused@HamAudio@@             pinned to band3/game/Stats.cpp, defined in system/hamobj/HamAudio
```

They need **re-homing** (`homing_reverse.py`) to find the true TU5 VA, not a
unit correction. Across all 290, 13 are INTERIOR and 277 sit on a live function
start. Net actionable-for-repointing today: small, and it belongs to the map
lane rather than this sizing lane.

### A.6 The MSVC-equivalent mechanism, and its codegen risk

* **Call sites would compile if the gate were flipped.** The arity concern is
  unfounded: the no-op branch `DECOMP_FORCEBLOCK(module, ...)` and the MWCC
  branch `(module, params, ...)` both consume the existing 3-argument calls,
  every site parenthesises its params, and no site has an unprotected top-level
  comma. The MWCC expansions are plain C++ with no MWCC-only syntax.
* **But flipping the gate would not achieve the goal for `DECOMP_FORCEFUNC`.**
  Its expansion is a *call* (`dummy->Showing();`). Under `/O1 /Ob2` MSVC inlines
  the call into the dummy and is not thereby obliged to emit a standalone
  out-of-line COMDAT. The construct that reliably forces emission is **taking
  the member's address** — `&Class::Method` — which is precisely what the two
  already-correct `DECOMP_FORCEBLOCK` sites do
  (`ProfileMgr.cpp:113` `&Profile::HasCheated`, `PatchDir.cpp:180`
  `&PatchLayer::HasSticker`). For templates the equivalent is an explicit
  instantiation.
* `#pragma comment(linker, "/include:...")` is **not** an option: it affects the
  link, and objdiff compares `.obj`s.
* **Codegen risk is low but nonzero and asymmetric.** The correct pattern (the
  SongDB precedent) adds a new file-scope function that references the target;
  it does not alter inlining decisions at existing call sites. The wrong pattern
  — sprinkling calls in the hope of forcing emission — adds live call sites that
  can perturb `/Ob2` inlining in the host TU. Given a ceiling of 0, neither is
  worth the exposure.

### A.7 Call: **do not fund**

Honest ceiling: **0 functions today**, and **≤5 ever** — one per genuinely
unemitted symbol, each capped at ±1 (★ fan-out is blast radius, never yield).
Realising even those 5 requires first *homing* each symbol to a retail VA and
adding a map entry, which is the existing homing channel's work; the macro
contributes only a 5-item hint list. There is no "sweep" here.

If anyone still wants the 5, the order is: home the symbol
(`scripts/harvest/homing_reverse.py`) → confirm PAYS
(`scripts/harvest/span_predictor.py`) → *then* add an MSVC re-statement in the
SongDB shape. Forcing first is wasted work.

---

## Item B — the stale-`.s` trap

### B.1 Blast radius: 8,638 orphaned `.s`, not one file

dtk (jeff) writes one `.s` per split unit as a **reference-only side effect**.
`build.ninja`'s split rule declares only `build/45410914/config.json` as its
output (`grep -c "45410914/asm" build.ninja` → **0**), jeff does a plain
`File::create` per unit with no directory sweep (`jeff/src/cmd/xex.rs`), and no
script anywhere cleans the directory. A unit removed from `splits.txt` therefore
leaves its `.s` behind permanently, frozen at the geometry current when it was
last written.

Measured against dtk's own live manifest (`build/45410914/config.json`,
4,174 units — 884 pinned + 3,290 autogenerated):

| | count |
|---|--:|
| `.s` files on disk | **12,812** |
| orphans (unit root absent from live `config.json`) | **8,638** |
| — `auto_*` blobs | 8,548 |
| — **named units** | **90** |
| named orphans predating the Jul-15 TU5 flip | 18 |

### B.2 ★ mtime is NOT a usable freshness proxy — and duplicate names collide

**72 of the 90 named orphans carry today's date.** `splits.txt` was rewritten
between two split runs minutes apart, so a `.s` whose mtime is ten minutes old
can be entirely wrong.

Worse, the same unit exists twice under different path forms, because the unit
naming scheme changed from path-qualified to flat:

| unit | live | orphan |
|---|---|---|
| `Faders` | `asm/Faders.s` — 21:17, 108,025 B, first fn `fn_822E4500` | `asm/system/synth/Faders.s` — 21:07, 2,319 B, first fn `fn_82310E50` |
| `VocalPlayer` | `asm/VocalPlayer.s` — 21:17, 621,897 B, first fn `fn_8269CCA8` | `asm/band3/game/VocalPlayer.s` — 21:07, 617,115 B, first fn `fn_826E3820` |

(Not all diverge — `LayerDir`'s two copies share a first `.fn` — so this check
is capable of returning "clean", i.e. it is not a tautology.)

The only sound discriminator is **membership in the live `config.json` unit
list**. laneAR's `HamPlayerData.s` (2026-06-11, pre-TU5-flip) is one of the 18
genuinely ancient ones, but keying a guard on age would miss the other 72.

*Reflink caveat, checked:* the worktree's `build/45410914/` is a CoW reflink of
main's and **mtimes are preserved** — `HamPlayerData.s` reads 2026-06-11 in both
trees, matching laneAR's independent observation on main. The mtime figures
above are therefore honest; the classification does not rely on them regardless.

### B.3 ★ Does a live tool read them? — narrower than feared

The full tool surface (`scripts/`, `tools/`, `configure.py`,
`tools/project.py`, `.claude/skills/`, `scripts/orchestrator/`,
`scripts/analysis/diff_inspect.py`) was swept. **No tool anywhere validates a
`.s` against `splits.txt`, `config.json`, `config.yml`, or any split stamp.**
But the exposure splits three ways:

**(a) The agent-facing reader — `scripts/analysis/diff_inspect.py` — is
currently NOT exposed.** `_find_target_asm_file` (lines 1393-1446, reached from
the `/compare-asm` skill and MCP `run_diff_inspect`) derives `unit_name` from
`report.json`, tries 8 direct candidate paths, and only then falls back to a
brute-force `glob("**/*.s")` where the first file containing `.fn <symbol>`
wins. That fallback is the dangerous path. **Measured: 0 of 4,174 report.json
units fail all 8 candidate paths**, and the first existing candidate is the live
flat `asm/<unit>.s`. The fallback is unreachable today. ★ This is a partial
refutation of the inherited "active source of wrong answers across every lane"
framing — the hazard is latent (one unit losing its `.s` re-arms it), not live.

**(b) Structurally gated readers (safe).** `tools/map_lint.py`,
`tools/pin_audit.py`, `scripts/find_truncated_splits.py`, `scripts/map_verify.py`
and `scripts/recarve/scan.py` only ever open a `.s` for a unit they read out of
`splits.txt`, so an orphan is unreachable. `map_lint`/`pin_audit` are the
closest thing to a freshness auditor in the repo, but they are linters you must
run deliberately — they are on no read path.

**(c) Ungated whole-tree globbers — this is where the contamination is real.**
`scripts/grind/classify_funclets.py:126` globs `**/*.s` and **writes the results
into `decomp.db` via `UPDATE functions SET …`** — it ingests all 8,638 orphans.
`scripts/recarve/funclets.py:49-63` globs everything and caches funclet VAs;
its `newest = max(mtime)` is a *cache-invalidation key*, not a freshness gate.
`tools/fn_resolver.py:1055-1082` (used by `scripts/grind/enrich.py`) opens
`asm_dir/{basename}.s`. Also ungated: `tools/ghidra/build_symbol_map.py`,
`tools/fingerprint_match.py`, `tools/locator.py`, and ~14 one-off
`scripts/harvest/*` scanners that hardcode `build/45410914/asm/{base}.s`.
(`tools/topo_locate.py`, `tools/field_offset_gate.py` and the
`tools/exploratory/*` scanners read the **Wii** asm tree and are unaffected.)

### B.4 TU0-era map survivors: 26 real, addressed

Live function starts were taken from `config/45410914/symbols.txt` (69,218
`.text` functions, regenerated by the same 21:17 split) and intersected with the
24,791 `target_symbol_map.json` entries. **60 map VAs are not a live function
start**; 34 of those are benign `__savefpr_*`/`__restfpr_*`/`__savegpr_*` chain
entry points (one blob, many legal entries). That leaves **26 real defects**:

**7 PHANTOM — no live function contains the VA at all:**

```
0x82266fc0  ?clear@?$_Rb_tree@VString@@U?$less@VString@@@stlpmtx_std@@…
0x8234a450  ?SetMeshVerts@WorkVerts@BandPatchMesh@@QAAXXZ
0x824a4ad0  ?LoadShaderBuffer@RndShaderProgram@@QAAXAAVBinStream@@HAAPAVRndShaderBuffer@@@Z
0x8251a338  ?Poll@UsbMidiGuitar@@SAXXZ
0x827e7190  ?PushIdle@MidiParser@@AAAXMMHVSymbol@@@Z
0x827e7ae8  ?InsertDataEvent@MidiParser@@AAAXMMABVDataNode@@@Z
0x827eb398  ?ParseNote@MidiParser@@QAAXHHE@Z
0x82bbab10  ?AddTempoInfoPoint@MultiTempoTempoMap@@QAA_NHH@Z
0x82c4cf38  dosilence
```

**17 INTERIOR — the VA lands mid-function (inert; the renamer finds no `fn_`
symbol to rename):**

```
0x82276908  interior of 0x822768f8  ?OnPlayerSaved@TrackerManager@@QAAXPAVPlayer@@@Z
0x82319960  interior of 0x82319950  ??$Find@VRndParticleSys@@@ObjectDir@@…
0x82319a08  interior of 0x823199d8  ??$Find@VHamLabel@@@ObjectDir@@…
0x8237fbd8  interior of 0x8237fbd4  ?SetCharacter@HamPlayerData@@QAAXVSymbol@@@Z   ← laneAR's example, confirmed
0x8245ee08  interior of 0x8245edc8  ??$__uninitialized_copy@PAUBone@CharBones@@…
0x824c4500  interior of 0x824c44a8  ??$New@VRndTransProxy@@@Object@Hmx@@SAPAVRndTransProxy@@XZ
0x824f2e04  interior of 0x824f2dc8  ?OnMsg@BandUI@@QAA?AVDataNode@@ABVServerStatusChangedMsg@@@Z
0x82693e34  interior of 0x82693e24  ?Length@@YAMABVVector3@@@Z
0x82699410  interior of 0x82699260  ?SetPaused@HamAudio@@UAAX_N@Z
0x826c48f8  interior of 0x826c44f8  ?Enabled@DirectInstrument@@QBA_NXZ            ┐
0x826c4908  interior of 0x826c44f8  ?SetVolume@DirectInstrument@@QAAXH@Z          │ laneAO §"Request 5"
0x826c4910  interior of 0x826c44f8  ?NoteOn@DirectInstrument@@QAAXH@Z             │ residue, confirmed
0x826c4958  interior of 0x826c44f8  ?IsLoaded@DirectInstrument@@QAA_NXZ           │ (5 /Ob2-inlined accessors)
0x826c4970  interior of 0x826c44f8  ??$Find@VMidiInstrument@@@ObjectDir@@…        ┘
0x826eece8  interior of 0x826eece0  ?GetLane@TrainerGemTab@@QBAHH@Z
0x8282ef50  interior of 0x8282ef30  __u64tod
0x82b3f7c0  interior of 0x82b3f740  ?Draw@DirectionGestureFilterDoubleUser@@UAAX…
```

All 26 are **inert today** — none can cost a match, because the renamer has no
`fn_<VA>` symbol to act on. Their harm is the laneAO/laneAR failure mode:
a wrong identity that manufactures a false source lead. 14 of the 17 interior
entries were already known (laneAO's 5 `DirectInstrument` + laneAR's
`HamPlayerData`); the **7 phantoms and the remaining interior entries are new**.
Note the `MidiParser` cluster (3 phantoms in one class) and
`?Poll@UsbMidiGuitar@@` — both plausibly recoverable with
`scripts/harvest/homing_reverse.py`, since our objs define those symbols.
Sizing only; no repair attempted here.

### B.5 Guard landed: `scripts/prune_orphan_asm.py`

Patching N ungated readers is the wrong shape — there are at least 20 of them
and new one-off scanners appear every wave. **Deleting the orphans fixes every
reader at once.** The files are gitignored, are not ninja inputs
(`grep -c "45410914/asm" build.ninja` → 0), and the next split regenerates every
live one, so deletion is free.

`scripts/prune_orphan_asm.py` classifies strictly by membership in
`build/<title>/config.json`'s unit list — never by mtime (§B.2). It is
**dry-run by default**; `--apply` deletes, `--check` exits 1 if any orphan
exists (usable as a cheap assertion in any wrapper), `--list` dumps all paths,
`--project-dir` targets a worktree. It refuses to run if `config.json` is
missing or lists no units.

Verified in this worktree (reports 8,638 / 90 named, matching the analysis
above, exit 1 under `--check`) and end-to-end on a synthetic tree: it kept the
live flat `Faders.s`, deleted the stale nested `system/synth/Faders.s` duplicate
and `HamPlayerData.s`, removed a stale `auto_*`, kept the live `auto_*`, and
pruned emptied directories.

★ Run `--apply` in your own worktree, or in main only when no lane has a live
`ninja` in flight — the split rule rewrites this directory.

---

## Refuted / corrected

1. ★ **"`DECOMP_FORCEBLOCK` ⇒ the MSVC build silently drops the COMDAT" is
   wrong.** objdiff compares `.obj`s, and MSVC emits every COMDAT it generates
   into the `.obj`; MWCC's dead-strip problem does not reproduce. 22 of 27
   probed forced symbols are already defined by our objs.
2. ★ **`grep -rln DECOMP_FORCEBLOCK src/` does not "enumerate the vein."** 78%
   of the family forces string data only, and the intersection that bounds real
   yield is 0.
3. **Site count is 143, not 145** — the extra two are `src/decomp.h`'s own
   `FORCEDTOR→FORCEFUNC` definition and a comment in `SongDB.cpp`.
   `DECOMP_FORCELITERAL` has **zero** call sites.
4. ★ **The stale-`.s` trap is bigger in files but smaller in live impact than
   framed.** 8,638 orphans, not one — but `diff_inspect.py`'s dangerous
   brute-force fallback is measurably unreachable today (0/4,174 units). The
   real contamination is `scripts/grind/classify_funclets.py`, which globs the
   whole tree and writes tags into `decomp.db`.
5. ★ **"Check the file post-dates the TU5 flip" is not a sufficient guard.**
   72 of 90 named orphans are same-day. Only `config.json` membership works.
