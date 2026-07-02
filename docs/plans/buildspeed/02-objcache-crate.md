# W1-B — `objcache`: Rust MSVC object cache (core crate, standalone)

Model: **opus**. Wave 1. Creates a NEW sibling repo. Does NOT touch rb3-xenon main's build
wiring (that happens in W3-A after W2-B's integration dry-run). This is CRITICAL
infrastructure — correctness under heavy concurrency beats every other property.

## Hard rules

- All scratch/worktrees/logs under `~/tmp` (NEVER `/tmp` — RAM tmpfs).
- Never mutate rb3-xenon main, wibo main, jeff, objdiff. You only create
  `/home/free/code/milohax/objcache` (new git repo) and read the others.
- MSVC objs are NEVER bit-stable across recompiles: exactly 4 bytes — the COFF FileHeader
  TimeDateStamp at offset 4 — differ between same-settings recompiles (verified by `cmp -l`,
  1 changed byte for a 2-second gap). ANY byte comparison needs a same-settings recompile
  control.
- If you test against real compiles, do it in a worktree created via
  `/home/free/code/milohax/rb3-xenon/scripts/setup_worktree.sh ~/tmp/wt-objcache objcache-dev`
  (buildable via btrfs reflinks) — never main. Build there with the worktree's own
  `./tools/ninja-locked`, tee logs to `~/tmp/objcache_*.log`.

## Problem statement (measured facts — trust these, they were verified)

rb3-xenon compiles ~745 TUs with MSVC X360 `cl.exe` 16.00.11886 under the wibo Windows-PE
loader. The ninja `msvc` rule (after wave-1's window #1 lands; see the "command shapes"
section) is a single command per TU. Costs: real compile 0.56-3.56 s/TU; front-end c1xx is
68-86% of that (measured via `/Bt`), so **preprocess-mode caching is rejected** — `/EP` alone
is 0.34 s = 68% of a 0.50 s compile. Direct-mode (dep-closure hashing) is the design:
hashing a TU's 170-header/1.2 MB closure takes ~4 ms. Python wrapper startup is ~45 ms —
hence Rust with a **~5 ms hit-path target**.

The cache pays off in: cold-cache A/B baseline builds (`setup_worktree.sh --cold-cache`),
reflink-failed worktree setups, any full re-log after a rule change, and N agents rebuilding
the same baseline. It does NOT speed the already-warm no-op path or novel edits.

Obj facts (all verified):
- Sole nondeterminism: COFF FileHeader TimeDateStamp, offset 4..8 (Machine=0x01F2 at 0,
  NumberOfSections at 2, TimeDateStamp at 4). Zero it on store → byte-stable entries.
- Sole absolute-path content: ONE embedded string = the /Fo output path as cl canonicalized it
  (`Z:\home\free\code\milohax\rb3-xenon\build\45410914\...\<unit>.obj`). Different worktree
  roots ⇒ that one string differs. objdiff/dtk ignore it (they diff sections/symbols/relocs);
  serving a foreign-root obj is match-safe ("option A"). Do NOT try to patch the string in the
  obj (length differences make that unsafe).
- Post-compile patchers (scripts/obj_anon_ns_patcher.py etc.) run as separate downstream ninja
  edges over `build/<ver>/src/**` and mutate objs in place — the cache must store the RAW
  compiler output (it wraps only the compile edge), and everything downstream just works.

## Where it lives

- New repo: `/home/free/code/milohax/objcache` (git init; plain log style, no trailer needed —
  it's a new repo, set the style: concise imperative subjects).
- Rust 2021, single binary crate `objcache` → `target/release/objcache`.
- Wired like jeff/objdiff prebuilt binaries: rb3-xenon's configure.py will reference the
  absolute prebuilt path (W3-A does that); no cargo edge in build.ninja; manual
  `cargo build --release` after source edits.
- Dependencies (keep the tree small; each must be justified): `blake3` (keyed hashing, SIMD),
  `memmap2` (zero-copy file hashing), `tempfile` (atomic publish), plus std. Avoid tokio —
  the hit path is sequential syscalls; async buys nothing at 5 ms scale. `rayon` optional for
  hashing the closure in parallel (measure first; 4 ms serial may already be fine).

## Command shapes it must wrap

After window #1 (W1-A), the msvc rule command is:

```
WIBO_FS_CACHE=1 WIBO_REWRITE_SHOWINCLUDES=1 /home/free/code/milohax/wibo/build/release/wibo \
  build/compilers/X360/16.00.11886.00/cl.exe $cflags /showIncludes /Fo$out $in
```

cwd = repo root (main or a worktree). `$cflags` contains only RELATIVE `/I` paths and flags
(e.g. `/I src/system/stlport /I src/xdk/LIBCMT /I src /I src/system ... /nologo /c /GR /O1
/Oi /EHsc /TP`). `$in` is a relative source path, `$out` a relative obj path. ninja parses
stdout with `deps = msvc`, `msvc_deps_prefix = "Note: including file:"`.

After wave 2 (PCH lands), two more rules exist: `msvc_pch_create` (adds
`/Yc"decomp_pch.h" /Fp$pch_out`) and `msvc_pch` (adds `/Yu"decomp_pch.h" /FI"decomp_pch.h"
/Fp$pch_file`). Design the crate so PCH-aware keying can be added in W2-B without reshaping
the store (see "PCH hooks" below).

Planned integration (W3-A wires it): the rule becomes
`objcache exec --fo $out -- <the full command above>`. Your CLI must accept exactly that:
everything after `--` is the verbatim child command; parse cflags/source/output from it
(also honor `--fo` as the authoritative output path). Env assignments are NOT part of the
child argv — W3-A will keep `WIBO_FS_CACHE=1 WIBO_REWRITE_SHOWINCLUDES=1` as env prefix words
before `objcache`; POSIX shells apply them to `objcache`, so **propagate your entire
environment to the child** (default `Command` behavior — just don't scrub it).

## Design (bake this in; deviate only with written justification in the repo's DESIGN.md)

### Key

`key = blake3(key_material)` where key_material serializes, in fixed order:
1. Compiler identity: for each of `cl.exe`, `c1xx.dll`, `c2.dll` under the compiler dir
   derived from the child argv (`build/compilers/X360/16.00.11886.00/`): content blake3.
   Cache these per (dev,inode,size,mtime) in a small sidecar so the hit path stats 3 files
   instead of hashing ~5 MB (hash once, revalidate by stat).
2. The exact cflags string (argv slice between the cl.exe path and `/showIncludes`, plus any
   flags after — i.e., everything except `/Fo<out>` and the source path; keep `/showIncludes`
   itself OUT of the key material or in it, just be consistent).
3. Relative source path (as given) + source content blake3.
4. The validated include closure: for each header in the manifest entry, its content blake3.
5. A cache format version integer (bump on any store-format change).

Deliberately EXCLUDED from the key: the wibo binary identity (byte-neutral to objs — verified
fork vs stock = 0 differing bytes), the absolute repo root (cross-root sharing is the point),
`/Fo` path, environment variables (none that affect codegen are set today; if
WIBO_COMPUTER_NAME / WIBO_PATH_MAP ever get set, they DO affect obj bytes — add a guard:
if either is present in the env, include their values in the key material).

### Manifest (ccache-style direct mode)

`manifest_key = blake3(compiler identity + cflags + relative source path)`.
A manifest maps that to a list of recorded closures: `[{closure: [(rel_path, blake3)...],
obj_key}]`. Hit = some recorded closure where EVERY file's current hash matches. Never serve
without validating every header hash (~4 ms for 170 files via mmap+blake3). On miss →
real compile → parse emitted `Note: including file:` lines → hash that closure → store obj →
append a new closure record (dedupe identical closures).

### Store layout & atomicity (the concurrency contract)

```
~/.cache/rb3-objcache/
  config.toml            # enabled flag, size cap, cache root override
  objects/ab/cdef.../    # content-addressed by obj_key
      obj                # timestamp-zeroed compiler output
      deps               # normalized "Note: including file:" payload lines (see Deps)
      meta               # cflags, source, sizes, creation info (debugging)
  manifests/ab/cdef...   # manifest files
  locks/                 # ONLY for manifest compaction / gc, never the hit path
```

- All writes: create in a tempfile in the SAME directory → fsync → `rename(2)` into place.
  Rename is atomic on btrfs; readers never see torn files.
- Concurrent same-key stores are benign: content is identical by construction (key covers all
  inputs); last rename wins.
- Manifest updates: read-modify-write via tempfile+rename is lossy under races (a concurrent
  closure record can be dropped) — acceptable (the dropped record re-appears on the next miss)
  BUT must never corrupt. Alternative (preferred): append-only record files
  `manifests/<mk>.d/<closure_hash>` — one file per closure, no read-modify-write at all;
  listing the dir enumerates candidates. Choose this unless measurement says dirent overhead
  matters.
- Fetch: try `hardlink(obj, $out)` first (same btrfs as the repos: ~0.1 ms). CAVEAT: the
  downstream patchers mutate objs **in place** — a hardlinked cache entry would be corrupted
  by the patcher writing through the link. Therefore hardlink is ONLY safe if the patchers
  replace the file (write-temp+rename) — they do NOT (they patch in place). **So: default to
  reflink copy** (`copy_file_range`/FICLONE, still ~free on btrfs, breaks the link) and treat
  hardlink as forbidden. Verify FICLONE works from `~/.cache` to the repo (same filesystem —
  check `stat -f`; if different fs, plain copy).
- Self-check on fetch: recompute blake3 of the fetched obj against obj_key metadata when
  `paranoid = true` in config (default true until W3-A's stress gate passes, then default
  false for speed; keep the flag).

### Deps capture & replay (ninja deps=msvc correctness)

- On miss: capture child stdout. Lines starting with `Note: including file:` are dep lines;
  everything else must pass through to your stdout UNMODIFIED and in order (warnings/errors).
  Store the dep payload paths NORMALIZED: strip the prefix, trim, lexically normalize, and
  **relativize under the current cwd** (paths under the repo root become root-relative;
  paths outside stay absolute). This normalization is load-bearing for W3-B (worktree seeding
  needs main's .ninja_deps to be root-relative) — today's deps are mixed 41-absolute /
  221-relative per TU (measured on MasterAudio.obj).
- Also ECHO the normalized dep lines (prefix re-attached) to stdout on the miss path so ninja
  records the same normalized form a hit would replay (hit/miss must be indistinguishable to
  ninja).
- On hit: write the stored dep lines to stdout as `Note: including file: <path>` (one per
  line, `\n`), then exit 0. ninja's CLParser trims whitespace; `\n` is fine.
- Exit code: mirror the child's exit code on miss; on any cache-internal error, fall through
  to a real passthrough compile (see Failure policy). Never exit nonzero because of a cache
  problem when the compile itself succeeded.

### Timestamp zeroing

On store, after reading the compiler's obj: verify bytes 0..2 == `F2 01` (IMAGE_FILE_MACHINE
0x01F2 PPCBE, little-endian on disk) as a sanity check, then zero bytes 4..8, then hash →
obj_key → publish. The obj written to `$out` on the MISS path is the compiler's own output
(leave its live timestamp alone — matching tooling tolerates it); the obj served on a HIT is
the zeroed stored copy. That means hit-objs and miss-objs differ in those 4 bytes — this is
fine (objdiff/dtk never read the header timestamp) but MUST be documented and is why the
verification protocol says "byte-identical modulo timestamp".

### Failure policy (graceful passthrough — non-negotiable)

Any error anywhere (cache dir unwritable, corrupt entry, lock timeout, parse failure, ENOSPC):
log one line to `~/.cache/rb3-objcache/objcache.log` (append, best-effort) and RUN THE REAL
COMPILE with untouched stdout/exit-code semantics. A build must never fail or stall because of
the cache. `objcache exec` with a missing/invalid config = passthrough with caching disabled.

### Toggle & config — NOT in the command string

The ninja rule string must stay constant (changing it costs a full re-log). Enable/disable via
`~/.cache/rb3-objcache/config.toml` (`enabled = true/false`) + env override `OBJCACHE=off|0`
→ passthrough. Ship `objcache on|off` subcommands that flip the config atomically.

### Eviction & tooling

- `objcache gc --max-size 10G`: LRU by entry atime/mtime, delete objects + orphaned manifest
  records. Never run implicitly on the hit path.
- `objcache stats`: hits/misses/bytes (maintain counters via per-process atomic increments to
  a stats file — use a separate small file per process flushed at exit, aggregated by `stats`,
  to avoid hit-path locking).
- `objcache verify <unit.obj> -- <command>`: runs the real compile to a temp path and byte-
  compares (modulo offset 4..8) against what the cache would serve — the building block for
  W2-B/W3-A verification.

### PCH hooks (design now, implement the keying in W2-B)

Under `/Yu`, cl restores front-end state from the .pch, so the emitted `/showIncludes` closure
does NOT include pch-covered headers — the closure alone under-keys msvc_pch TUs. Two candidate
strategies (W2-B decides by experiment):
(a) include `blake3(system.pch bytes)` in the key — simplest; requires the .pch to be
    byte-stable per header-set (likely embeds paths/timestamps — W2-B measures);
(b) include the pch-source closure: when the wrapper sees a `/Yc` compile, record its closure
    keyed by (pch source + cflags); `/Yu` compiles look that record up and mix it in.
For v1 (this task): detect `/Yc`/`/Yu` in argv and PASS THROUGH uncached (correct, conservative),
with the detection points structured so W2-B can slot the chosen strategy in.

## Testing (in this task, standalone)

1. Unit tests: key stability, manifest closure validation (mutate one header byte → miss),
   timestamp zeroing (feed a real obj from a worktree build), deps normalization (feed the
   captured stdout of a real compile — get one by running the wave-1 command by hand in a
   worktree), atomic publish (kill -9 mid-store leaves no visible partial entry).
2. Integration smoke (worktree): wrap 3 real TUs by hand (small/medium/large, e.g.
   src/system/obj/Object.cpp, src/system/os/Console.cpp, src/band3/meta_band/MusicLibrary.cpp):
   miss → compile+store; re-run → hit; `cmp` hit obj vs a fresh control compile — identical
   except bytes 4..8; dep lines byte-compare (normalized) between hit and miss runs.
3. Concurrency micro-stress: 64 parallel `objcache exec` of the SAME TU from a clean cache
   (all miss simultaneously) → exactly one entry, all 64 objs valid; then 64 parallel hits →
   all served, measure hit latency distribution (target: p50 ≤ 5 ms, p99 ≤ 20 ms).
4. `cargo build --release` clean; `cargo clippy` clean.

## Acceptance criteria

- Repo at /home/free/code/milohax/objcache with DESIGN.md (the above, plus your deviations),
  passing tests, release binary built.
- Measured hit path p50 ≤ 5 ms on the integration TUs (report the number).
- Passthrough proven: `chmod -R a-w` the cache dir → wrapped compile still succeeds and the
  obj is correct.
- PCH argv detection present (passthrough), structured for W2-B.
- Report: binary path, measured latencies, any design deviations, open questions for W2-B.

## Rollback

Nothing to roll back — the crate is unwired. Deleting the repo (or `objcache off`) is the
kill switch at every later stage.
