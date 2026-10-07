# W16-UC: every native file read resolves the way retail resolves it (2026-10-07)

Branch `w16-uc`. Follows W16-UA (`W16UA_NATIVE_RETAIL_BOOT_MACROS_2026-10-07.md` §4),
which found that 16 of the 27 files in the preinit config read came from host TEXT
files in a sibling checkout. W16-UA fixed that one case (`.dta` → `gen/*.dtb`) and
left the `.milo` → `.milo_xbox` path and every other door unaudited.

## Summary

- **Audit.** Every place a native target can get file bytes from, and every
  name rewrite before that, is listed in §1 and §2. Each one was checked against
  retail's rule and measured on all 18 targets (§3).
- **Fixes** (§4), all native-only:
  - Loose files must lie inside the disc image. The `.dta` fix covered one case
    of a general hole: any name the ark lacked under `../../system/run` was read
    from the host tree two levels above the data dir. On the pre-fix binary a
    planted host milo loaded and `rb3-milo` printed `ALL GATES PASSED`.
  - Device paths (`devkit:/…`) follow retail's `FileIsLocal` rule. They fail on
    the host door, as on a console without that device; they used to be looked
    up in the archive.
  - `gSystemRoot` is retail's relative `../../system/run`. It had been resolved
    against the working directory.
  - The milo cache mode is set unconditionally, as in TU5.
  - The archive name uses retail's constant platform.
  - The DTA overlay dir is opt-in instead of guessed from the working directory.
- **Check.** `tools/native_file_audit.py` reads an `strace` of each run plus the
  engine's own file ledger, and puts every host open in exactly one class (§5).
  A read outside the disc image that is not allow-listed fails the run. So does
  a `.milo`/`.dta` name mapped differently from retail's rule. `native_health.sh`
  runs all 18 targets under it and adds an escape probe and three controls. All
  three controls go red.
- **Result:** 18 of 18 targets clean, and 0 host reads outside the disc image
  beyond declared inputs and the allow-list. All 426 name mappings equal
  retail's. Both gates pass (§7).

## 1. The doors: where a native read can get bytes from

Retail has one door for a relative read. Its `FileIsLocal` (`os/File_Win.cpp`) is
`strlen(FileGetDrive(file)) > 1`. A plain path has no drive, so it is not local,
and `NewFile` builds an `ArkFile`. The only host files a console opens are on the
disc: `gen/main_xbox.hdr` and the `main_xbox_N.ark` parts.

| # | door | code | retail equivalent | finding |
|---|---|---|---|---|
| 1 | archive lookup | `ArkFile` ctor → `Archive::GetFileInfo(FileMakePath(".", name))` | same code | shared engine code, unchanged |
| 2 | archive bytes | `NativeArkRead` (`platform/ArkRead_Native.cpp`), `pread` on `<data>/gen/main_xbox_N.ark` | BlockMgr reads `d:\gen\main_xbox_N.ark` | on the disc; part names come from the `.hdr` |
| 3 | archive header | `Archive::Read` → `FileStream(kReadNoArk)` → `<data>/gen/main_xbox.hdr` | `d:\gen\main_xbox.hdr` | on the disc; **base name used `TheLoadMgr.GetPlatform()`, retail uses the constant `kPlatformXBox`** (fixed, §4.5) |
| 4 | host file | `AsyncFileNative::_OpenAsync` (`fopen` of a qualified path) | `AsyncFile_Win` on the device | reached only when native `FileIsLocal` says local; see the rows below |
| 4a | absolute path | `FileIsLocal`: `file[0]=='/'` | none (a retail device path is the nearest thing) | how a harness hands the engine a declared input (MIDI charts). Allowed only when declared (§5) |
| 4b | device path | `devkit:/locale_keep.dta` | retail rule: local, so the device door | **native sent it to the archive** (fixed, §4.2) |
| 4c | overlay dir | `NativeOverlayExists` | none | **auto-detected from `<data>/../native/dta` or the cwd's `native/dta`** (made opt-in, §4.6) |
| 4d | loose file | `NativeLooseFileExists` | none (DLC arrives on a device) | **could resolve outside the data dir** (fixed, §4.1) |
| 5 | stat / enumerate / mkdir / delete | `FileGetStat`, `FileEnumerate`, `FileMkDir`, `FileDelete` | same, on `d:` or a device | now refuse device paths with no host fallback; `FileEnumerate` under `UsingCD` uses the archive |
| 6 | driver `fopen` | dumps, PNGs, the `--config-dump` file, `rb3-ark`'s `ARK_REF` reference | none | outputs, plus one declared reference input |
| 7 | process | loader, libc, GPU stack (Dawn → Vulkan → driver) | none | allow-listed by category (`tools/native_file_audit_allow.txt`) |

## 2. The name rewrites before a door is chosen

| rewrite | where | rule | finding |
|---|---|---|---|
| `/eng/` → language, `/og/` → `/ng/` | `FileLocalize` (in `NewFile`, read mode) | retail-ported body | shared code; region NA keeps `eng` |
| `.dta` → `dir/gen/base.dtb` | `CachedDataFile`, inlined into `DataReadFile` and `DataLoader` | applied unless `FileIsLocal`, or the name already holds `.dtb` | depends on native `FileIsLocal`, i.e. on rows 4a–4d. **214 recorded (213 mapped; 1 left as-is, `rb3-dta`'s absolute declared input), all equal to retail's** |
| `.milo` → `dir/gen/base.milo_xbox` | `DirLoader::CachedPath` | applied when the cache mode is on; TU5 turns it on unconditionally in `ObjectDir::Init` | **native turned it on only `if (UsingCD())`** (fixed, §4.4). **212 recorded (164 mapped, 48 already `gen/*.milo_xbox`), all equal** |
| name → archive key | `FileMakePath(".", name)` | lowercases, `\` → `/`, collapses `.`/`..` | shared code |
| qualification | `FileQualifiedFilename` | retail: `FileMakePath("d:", name)`; native: data dir, or overlay | the data dir is the disc root |
| `FileSystemRoot()` | `FileInit` | retail: `FileMakePath(".", "../../system/run")` = `../../system/run` | **native `realpath`ed it against the cwd** (fixed, §4.3) |

The archive keys its 43 engine-system files as `../../system/run/...`, for
example `../../system/run/config/gen/default.dtb`. Retail never resolves those
paths on a filesystem, so `..` cannot escape anything there. Native qualifies
the same string as `<data>/../../system/run/...`, which climbs two levels out of
the disc image. Rows 4d and the old `gSystemRoot` are the two places where that
mattered.

## 3. What every target actually opens

Each target ran under `strace` (open-family syscalls only, seccomp-filtered),
with `RB3_FILE_LEDGER` set. The table is the final health run on the committed
branch. Classes are defined in §5. "ark" is engine lookups ok/miss. "dta"/"milo"
count recorded name mappings.

| target | host opens | disc | input | output | allowed | **outside** | ark | dta | milo | audit |
|---|---:|---:|---:|---:|---:|---:|---|---:|---:|---|
| `rb3-dta` | 8 | 0 | 1 | 1 | 6 | **0** | 0/0 | 1 | 0 | PASS |
| `rb3-song` | 18 | 11 | 0 | 1 | 6 | **0** | 1/0 | 1 | 0 | PASS |
| `rb3-midi` | 18 | 11 | 0 | 1 | 6 | **0** | 3/0 | 0 | 0 | PASS |
| `rb3-gem` | 9 | 0 | 2 | 1 | 6 | **0** | 0/0 | 0 | 0 | PASS |
| `rb3-hit` | 8 | 0 | 1 | 1 | 6 | **0** | 0/0 | 0 | 0 | PASS |
| `rb3-score` | 8 | 0 | 1 | 1 | 6 | **0** | 0/0 | 0 | 0 | PASS |
| `rb3-score2` | 6 | 0 | 0 | 0 | 6 | **0** | – | 0 | 0 | PASS |
| `rb3-score3` | 9 | 0 | 2 | 1 | 6 | **0** | 0/0 | 0 | 0 | PASS |
| `rb3-score4` | 8 | 0 | 1 | 1 | 6 | **0** | 0/0 | 0 | 0 | PASS |
| `rb3-vocal` | 8 | 0 | 1 | 1 | 6 | **0** | 0/0 | 0 | 0 | PASS |
| `rb3-vocal2` | 8 | 0 | 1 | 1 | 6 | **0** | 0/0 | 0 | 0 | PASS |
| `rb3-harmony` | 8 | 0 | 1 | 1 | 6 | **0** | 0/0 | 0 | 0 | PASS |
| `rb3-crowd` | 8 | 0 | 1 | 1 | 6 | **0** | 0/0 | 0 | 0 | PASS |
| `rb3-save` | 7 | 0 | 0 | 1 | 6 | **0** | – | 0 | 0 | PASS |
| `rb3-ark` | 20 | 11 | 1 | 2 | 6 | **0** | 29/0 | 30 | 0 | PASS |
| `rb3-frame` | 1,423 | 0 | 0 | 4 | 1,419 | **0** | – | 0 | 0 | PASS |
| `rb3-milo` | 18 | 11 | 0 | 1 | 6 | **0** | 76/0 | 60 | 18 | PASS |
| `rb3-render` | 1,442 | 11 | 0 | 7 | 1,424 | **0** | 327/0 | 122 | 194 | PASS |

- "disc" = 11 = `main_xbox.hdr` plus the ten parts. No target opens any other
  file on the disc image, and none opens a loose disc file.
- Every archive lookup in all 18 runs hit. The extension census of the 436 hits:
  217 `.milo_xbox`, 204 `.dtb`, 10 `.mogg`, 5 `.mid`.
- "input" is what the harness declared with `--needs`: the MIDI charts, the
  text `songs.dta` that `rb3-dta` parses, and `rb3-ark`'s independently
  extracted `songs.dtb` reference. They are not shipped-disc reads and are
  not meant to be. The charts are RB3DX/onyx songs that are not on the disc.
- "allowed" is the loader and libc (6 opens per process), plus the GPU stack for
  `rb3-frame`/`rb3-render`. The GPU stack's count varies by a file or so between
  runs (`rb3-render`: 1,441 in one run, 1,442 in the next). It also covers the executable under test and its
  RPATH library probes (`self`, granted per run).
- The ten drivers with no disc (`dta`, `gem`, `hit`, `score*`, `vocal*`,
  `harmony`, `crowd`) open no game file except their declared input. The config
  they need is **compiled in** (`crowd_config_dta.h`, `scoring_config_dta.h`).
  That is not a file read, so this check cannot see it (§8).

**Prediction that failed.** Going in, I expected the full-target sweep to find
more host reads of W16-UA's kind, because several systems resolve names under
`../../system/run`. It found none. With W16-UA's `.dta` fix in place, the first
traced run on the unmodified (pre-W16-UC) code read nothing outside the disc
except the GPU/loader files, which only needed allow-list lines. Every defect
in §4 is latent in today's 18 runs: none of them reads a wrong file today. Each
is a wrong rule that a planted file, a different working directory or a
different driver order turns into a wrong read, and §4 shows the planted case.

## 4. Fixes

### 4.1 Loose files must be on the disc image (`platform/File_Native.cpp`)

`NativeLooseFileExists` treats a name as a loose host file when the archive lacks
it and `stat(<data>/<name>)` finds a regular file. W16-UA added the
`.dta` → `gen/*.dtb` exception. A name under `../../system/run/` still climbs out
of the data dir.

**Prediction:** on the pre-fix binary, a milo planted at
`<data>/../../system/run/ui/gen/planted.milo_xbox` would load from the host.
**Result**, with `<data>` = `~/tmp/w16uc_plant/a/disc`, whose `gen/` is a symlink
to the real disc's, and the plant being a byte copy of the shipped
`ui/track/gen/tracksystem_meshes.milo_xbox`:

```
rb3-milo rc=0
RESULT: ALL GATES PASSED (0 gate failure(s))
  engine LOCAL (FileIsLocal said host) loose          2
FILE-AUDIT plant-pre: FAIL -- 2 read(s) outside the disc image and not on the allow-list:
    OUTSIDE [ledger HOST] /home/free/tmp/w16uc_plant/system/run/ui/gen/planted.milo_xbox  (ok)
    OUTSIDE [strace] /home/free/tmp/w16uc_plant/system/run/ui/gen/planted.milo_xbox  (ok)
```

So the run was green while reading off the disc. The fix requires the qualified
path's `realpath` to lie inside the data dir's `realpath`. After it, the same
name is looked up in the archive, misses, and fails, as on a console:

```
  OK    escape-probe -- <data>/../../system/run/... was asked of the archive (miss) and the planted host copy was not read
```

The `.dta` rule from W16-UA stays. The containment rule is the general one.

### 4.2 Device paths take the device door (`File_Native.cpp`, `AsyncFile_Native.cpp`)

`rb3-render` asks for `devkit:/locale_keep.dta` (a dev override in `Locale`). Under
retail's rule `devkit` is a device, so the read goes to `AsyncFile`, and a retail
console has no devkit drive, so the open fails. Native sent it to the archive:
`ARK devkit:/locale_keep.dta miss`. The outcome was the same (null file), but
the door was wrong. Under native qualification, a device path would also have
reached `fopen` as a path relative to the cwd.

Now `FileIsLocal` applies retail's rule (`LOCAL devkit:/locale_keep.dta device`).
`AsyncFileNative` refuses a device path before touching the filesystem
(`HOST r devkit:/locale_keep.dta fail nodevice`), and so do
`FileGetStat`/`FileEnumerate`/`FileMkDir`/`FileDelete`. `d:` is one character,
so it is still not a device, which is retail's rule too. `rb3-milo` gains a
`device-path-is-device` gate: local, and the open fails.

### 4.3 `gSystemRoot` (`src/system/os/File.cpp`, `FileInit`)

Native replaced retail's `strcpy(gSystemRoot, FileMakePath(gExecRoot, "../../system/run"))`
with `realpath("../../system/run")` against the **working directory**. Whenever
that existed, the result was an absolute host path, and native `FileIsLocal`
sends every absolute path to the host. So `FileSystemRoot()`-based loads
(rndobj cylinder/sphere, `chartest.milo`, shaders) would read whatever tree sat
two levels above the cwd, and the result changed with the cwd. Its other branch
probed a dc3-scaffold layout, `<data>/extracted/(..)/(..)/system/run`, which
nothing in this tree produces.

None of the 18 runs reads through `FileSystemRoot()`. I checked by running the
pre-fix `rb3-render` from a cwd where `../../system/run` exists: no host read
and no `LOCAL` record. The `../../system/run/...` lookups it does make come
from DTA `#include` paths, which resolve in the ark. The native branch is gone,
and native now runs retail's line. `rb3-milo`'s new `file-system-root` gate
reads `../../system/run`.

### 4.4 The milo cache mode (`src/system/obj/Dir.cpp`, `ObjectDir::Init`)

TU5 calls `DirLoader::SetCacheMode(true)` unconditionally. Native did it only
`if (UsingCD())`. Every current driver calls `SetUsingCD(true)` before
`ObjectDir::Init`, so no run was affected. A driver doing it the other way round
would ask the archive for `foo.milo`, which no disc holds. Now unconditional.
`rb3-milo`'s `milo-cached-path` gate checks
`ui/track/tracksystem_meshes.milo` → `ui/track/gen/tracksystem_meshes.milo_xbox`.
The ledger's `MAP milo` records check every load in every run (§5).

### 4.5 The archive name (`platform/System_Native.cpp`, `NativeArchiveInit`)

`gen/main_%s` was built from `PlatformSymbol(TheLoadMgr.GetPlatform())`. Retail's
`ArchiveInit` uses the constant `kPlatformXBox`, so native now does too.

### 4.6 The overlay dir (`platform/System_Native.cpp`, `NativeDetectOverlayDir`)

Overlay files shadow the archive, so an overlay read is by definition a read
from outside the disc. The overlay was guessed from `<data>/../native/dta` or
the cwd's `native/dta`, so the same binary could read different game data
depending on where it was started. No current driver calls the native
`SystemPreInit(argc, argv)` that runs the detection, so no run was affected. It
is now opt-in through `RB3_OVERLAY_DIR`. The audit fails any run that reads it
unless the overlay is allow-listed for that run.

## 5. The check

`tools/native_file_audit.py check` takes two records of one run:

- an **strace** of the process tree (`open`, `openat`, `openat2`, `creat`, plus
  `chdir`/`fchdir` so relative paths resolve; `-y` so a dirfd prints its path).
  It sees every open whoever made it: engine, driver, libc, GPU stack.
- the engine's **ledger** (`RB3_FILE_LEDGER=<path>`,
  `native/src/platform/FileLedger_Native.cpp`), one line per event:
  - `ARK <key> ok|miss`: `ArkFile` ctor;
  - `HOST r|w <path> ok|fail [nodevice]`: `AsyncFileNative`;
  - `LOCAL <name> absolute|device|overlay|loose`: whenever `FileIsLocal` sends a
    name to the host;
  - `MAP milo|dta <requested> <opened>`: `DirLoader` and
    `DataReadFile`/`DataLoader`.

  The hooks in engine TUs are `#ifdef HX_NATIVE` and call a weak symbol, so a
  target that links no platform shims still links.

Every host open goes into exactly one class:

| class | meaning | judged |
|---|---|---|
| `DISC` (`ARK_HDR`, `ARK_PART`, `DISC_LOOSE`) | under `--assets`, lexically or after `realpath` | ok |
| `INPUT` | a path declared with `--input` (`native_health` passes its `--needs` list) | ok |
| `OUTPUT` | opened write-only | reported, not judged |
| `ALLOW:<cat>` | a line of `tools/native_file_audit_allow.txt`, or a per-run `--allow-glob` | ok |
| `OUTSIDE` | anything else, **whether the open succeeded or not** | **violation** |

A failed open counts because the attempt is the bug. W16-UA's 16 reads succeeded
only because a sibling checkout happened to be there. Every `MAP` record is also
recomputed with retail's rule: `FileGetPath`/`FileGetBase`/`FileGetExt`,
`CachedPath` under an always-on cache mode, and `CachedDataFile` under retail's
device rule, all re-implemented in Python. A mismatch is a violation. Exit codes:
0 clean, 1 violation, 2 cannot judge. Code 2 means no trace, an empty trace, or
`--expect-disc` on a run that read no disc file or made no archive read, which
would make a clean result vacuous.

The allow-list holds no game data. Its header says so: a config, milo,
texture, chart or song list never belongs there.

**In `native_health.sh`:** every `run_target` runs under `strace` with a
per-target ledger. Its `--needs` become `--input`s, and `$ASSETS` becomes
`--assets` with `--expect-disc`. A new FILE SOURCES section judges all of them
(an `OUTSIDE` read adds `<t>:file-audit` to `runtime_failed`). It then runs the
**escape probe** of §4.1 as a positive check: no outside read, and the ledger
must show the planted name asked of the archive and missed, or the probe is
counted as unrunnable rather than passed. No `strace` makes the section
UNRUNNABLE (rc 3), never a pass.

**Controls** (selftest; each requires the base target green and its own audit
clean first, the house pair rule):

| control | what is planted | must |
|---|---|---|
| `file-planted` | `rb3-milo` loads the planted milo by its absolute host path: a real engine read (`FileIsLocal` → `AsyncFile`) of a file outside the disc image | audit rc 1, naming the file on both the strace and the ledger record |
| `file-undeclared` | `rb3-ark`'s own positive trace, re-judged without declaring `ARK_REF` | audit rc 1, naming `ARK_REF`. Shows inputs pass because they are declared, not because they were on the command line |
| `file-namemap` | `rb3-milo`'s own ledger with one `.milo` request rewritten to open the unmapped name, which is what a native cache mode left off produced (§4.4) | audit rc 1 on `[name map milo]` |

```
  RED   file-planted -- audit rc=1, the planted read named on both records (control WORKS)
  RED   file-namemap -- audit rc=1: [name map milo] char/main/shared/prefabs.milo  (engine opened char/main/shared/prefabs.mil... (control WORKS)
  RED   file-undeclared -- audit rc=1 on rb3-ark's own trace, naming ARK_REF (control WORKS)
```

The first time I ran the checker on a real trace it failed `rb3-ark` on
`ARK_REF`, before it was declared. That is the `file-undeclared` control
happening by accident, and it is why the declaration path exists. The first
full sweep also failed `rb3-frame`/`rb3-render` on 18 GPU-stack paths each
(`/etc/egl`, `/etc/xdg/vulkan`, `/usr/share/nvidia`, `/opt/cuda`, …). Those are
now allow-listed under `gpu`, each by name.

`rb3-milo` gains three gates (`file-system-root`, `milo-cached-path`,
`device-path-is-device`), so the runtime gate count goes from 274 to 277.

## 6. The X360 build

Five engine TUs changed: `os/File.cpp`, `os/ArkFile.cpp`, `obj/Dir.cpp`,
`obj/DirLoader.cpp` and `obj/DataFile.cpp`. All new code is under
`#ifdef HX_NATIVE`. In `File.cpp` and `Dir.cpp`, the one retail statement that
used to sit in an `#else` now sits outside the conditional. I preprocessed each
TU with the X360 compile's own flags plus `/EP /P` (cl 10224 under wibo), once
as edited and once as `HEAD` placed next to it, and compared with blank lines
removed. **All five are byte-identical** (`cmp` rc 0; 20,446 / 19,501 / 43,188 /
19,894 / 20,069 non-blank lines). The raw line counts differ, so identical
output also rules out a `__LINE__`-dependent token. I did not run an X360 build
(memory was short, and this is the stronger check).

## 7. Results

Measured on branch `w16-uc`, worktree `~/tmp/wt-w16uc`, base `8d3d5800e`, one
build at a time:

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
  file sources: 18 target(s) clean, 0 with reads outside the disc image, 0 not judgeable
  OK    escape-probe -- <data>/../../system/run/... was asked of the archive (miss) and the planted host copy was not read
NATIVE HEALTH: PASS  (rc=0, link 18/18, 277 runtime gate(s), selftest=PASS)
NATIVE_HEALTH_RESULT verdict=PASS link=PASS link_verified=18 link_expected=18 link_skipped=0 runtime=PASS runtime_ran=18 runtime_total=18 gates_pass=277 gates_fail=0 unrunnable=none selftest=PASS scatter_unlinked=13 scatter_dirb=0 scatter_multihost=12 rc=0 handpose_controls=3/3 handpose_baseline_fail=1 runtime_crashed=0 runtime_failed=none
```

Selftest: 16 controls red, 0 green, 0 skipped. `render-perturb` reads `INERT`,
which is its documented design (`native_health.sh` `hp_probe ... INERT`). Both
config views are still EQUAL (63,069 lines), and 17 of 17 targets print
`boot-macros`.

## 8. Not done

- **Compiled-in config.** The ten drivers without a disc get scoring/crowd
  config from `crowd_config_dta.h` and `scoring_config_dta.h`. Both headers
  were cut from the host TEXT extraction (`extracted/config/scoring.dta`), the
  same source class W16-UA found differing from the `.dtb` on backslashes. That
  is not a file read, so this check cannot see it. Nobody has compared those
  blocks with `config/gen/scoring.dtb` read under retail's macros
  (`tools/retail_boot_config.py` could do it).
- **Title-update archive.** Retail `ArchiveInit` mounts
  `<TitleContentPath>/gen/patch_xbox.hdr` ahead of the disc when a title update
  is installed. The data set is the disc only, and native mounts only
  `main_xbox.hdr`, so whether a TU5 patch archive overrides any disc file is
  unaudited.
- **Writes.** A relative write (`FILE_OPEN_WRITE`) is qualified into the data
  dir, so it lands inside the disc image. Retail's `d:` is read-only, so the
  write would fail there. No run writes through the engine (0 `HOST w` records),
  so I left it.
- **`gHostFile`** (DirLoader's dev host-file option) still sets `TheArchive = nullptr`
  around a load. No driver sets it.
- `rb3-save` writes `/tmp/rb3_save_rt3.bin` (`OUTPUT`, not judged; the house
  rule prefers `~/tmp`).
- Empty names: native `FileIsLocal("")` returns true and retail's returns false.
  `NewFile` never passes one, so I left it.
