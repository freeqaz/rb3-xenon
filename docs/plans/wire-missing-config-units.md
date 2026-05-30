# Wiring the `warn_missing_config` units into objects.json (2026-05-30)

## What this was

`configure.py` printed ~248 `Missing configuration for <X>.cpp` warnings. Each is a
unit that dtk split out of the retail XEX (it has a `.text` range in
`config/45410914/splits.txt`, so it lands in `build/45410914/config.json`) but had
**no entry in `config/45410914/objects.json`** — so nothing was compiled to diff
against it. (Warning emitted in `tools/project.py:1070`, gated by
`config.warn_missing_config = True`.)

247 such units were "absent" (no objects.json entry); 1 more (`Utl.cpp`) is a
separate *pre-existing* class (see below).

## What was done

**235 units wired** into `objects.json` as `NonMatching`, each with its real
`src/`-relative path and correct lib (`engine` / `hamobj` / `band3` / `network` /
`xdk` / new `curl`). All 235 verified to **compile against main's current headers**;
the build stays green. Result: **248 → 13** warnings.

### Disambiguation (13 units, same basename in two dirs)
DC3 confirms both TUs are real in the binary; the single splits pin captured one.
Resolved by an Opus subagent that disassembled each candidate obj via
`objdiff-cli diff -2 <cand.obj> '<sym>' -f json-pretty --include-instructions`
(the freeqaz fork reads MSVC-X360 PPC-COFF that llvm-objdump can't) and
opcode-compared against the pinned target function in `build/45410914/asm/<U>.s`:
- **render → `rndobj`** (base `Rnd<X>`, not `Dx<X>`): Mesh, Tex, Env, Movie,
  MultiMesh, Mat, Cam, Lit
- **synth → `synth`**: FxSend, FxSendDelay, StreamReceiver
- **synth → `synth_xbox`**: Mic (real class only in synth_xbox), FxSendWah (target
  is the MI `FxSendWah360` ctor)

### Compile fixes made (subagents, in worktree → merged to main)
- **meta_ham/net_ham headers** (additive, no existing unit includes them): ported
  from `../dc3-decomp/src/lazer/meta_ham` + `lazer/net_ham` →
  `src/meta_ham/`, `src/net_ham/` (SkeletonIdentifier, MetagameStats, MetaPerformer,
  HamSongMgr, HamSongMetadata, Instarank, Playlist, SkillsAwardList, RCJobDingo).
  Unblocked `IdentityInfo.cpp`, `DanceRemixer.cpp`.
- **CamShot cluster**: `src/system/hamobj/HamCamShot.h` gained `public
  RndTransformable` + `std::vector<RndDrawable*> mGenHideVector` (CamShot itself
  dropped `RndTransformable` in commit `ee92785` for the retail layout, so
  `HamCamShot` carries it directly); `CameraShot.cpp:466` `WorldXfm()` call guarded
  `#ifdef HX_NATIVE`. Verified safe: HamCamShot.h is included only by 6 hamobj units
  (HamBattleData, RhythmBattle, Ham, HamCamShot, HamCamTransform, HamDirector), all
  0-matched in main → zero regression risk.
- **curl `sslgen.c`**: added a DC3-style `curl` cflags preset to `config.json`
  (`/TC /GS /D_XBOX360 /DCURL_STATICLIB`) + a `curl` lib in objects.json. The
  `/D_XBOX360` makes curl's `setup.h` include `config-xbox360.h` (defines
  `ssize_t`, `sread`/`swrite`, `HAVE_STRUCT_TIMEVAL`). DC3 builds this same file
  Matching, so the remaining ~60 curl/lib files can be added the same way later.

## The 13 still-deferred warnings (NOT wired) and why

**Fader cluster (9)** — `StandardStream, Sfx, Sequence, MoggClip, SongPreview,
HamAudio, Faders, Sound, MetaMusic`. These `.cpp` are DC3-sourced and use DC3's
**redesigned** `Fader`/`FaderGroup` API (`SetVolume`, `GetVolume`, `DuckedValue`,
`GetLevelTarget`, `FaderGroup::Save/Faders`). RB3's `Fader` is different: the
existing `src/system/synth/Faders.h` is a carefully RE'd RB3-retail layout with
offset annotations (`mVal // 0x1c`, `mMode // 0x40`; `Fader : public Hmx::Object`),
**identical to rb3-Wii's** `Faders.h` (`SetVal`/`GetVal`, no `SetVolume`). A
subagent's first attempt replaced `Faders.h` wholesale with DC3's version — that
broke `bandobj/CrowdAudio.cpp` (uses `mVal`/`CancelFade`/`SetMode`/`kInvExp`) and
would regress matched audio (MasterAudio/Synth/BinkClip). **Reverted.** Correct fix
= source these 9 from rb3-Wii (`../rb3/src/system/synth/...`) or hand-adapt DC3's to
RB3's Fader API, preserving the matched layout. This is decomp work, not config
wiring. (CLAUDE.md: "dc3-decomp is newer than RB3 … cross-check rb3-Wii.")

**`HolmesClient.cpp`, `NetCacheMgr.cpp` (2)** — DC3-sourced; reference base-class
members that concurrent commits deliberately **dropped to match RB3**:
`BinStream::ReadAsync` (removed in `9517690`) and `mServiceIDObtained` (removed in
`ff60e19`). Same pattern as the Fader cluster — needs RB3-correct source. Defer.

**`ssluse.c` (1)** — curl's OpenSSL TLS backend; needs `openssl/*.h` which exist in
**no** tree here, and **DC3 itself does not build it** (its objects.json wires only
`sslgen.c`). Skip until/unless OpenSSL headers are ported.

**`Utl.cpp` (1, pre-existing)** — *not* one of the 247. `objects.json` already
declares **two** `Utl.cpp` (`system/ui/Utl.cpp`, `band3/meta_band/Utl.cpp`), so
dtk's bare-basename `Utl.cpp` unit can't resolve to one (project.py's basename alias
goes ambiguous → `objects.get('Utl.cpp')` is None). Benign; a project.py
aliasing/qualified-unit-name limitation, independent of this task.

## How to pick up the deferred work
- Fader 9: `git -C ../rb3 show HEAD:src/system/synth/StandardStream.cpp` etc. as the
  RB3-API source; diff against the DC3 copies now in `src/`. Keep `Faders.h` as-is.
- HolmesClient/NetCacheMgr: grep the offending member, adapt to the current
  RB3-trimmed base class (commits `9517690`, `ff60e19`).
- Validation recipe used here: wire in a worktree
  (`scripts/setup_worktree.sh`), build the new objs with `-k 0`, then compare
  100%-matched symbols in `build/45410914/report.json` against a baseline at the
  **same commit** (concurrent commits to main otherwise show up as false
  "regressions" — that bit me; the worktree base was `fa9450d`, main was +15).
