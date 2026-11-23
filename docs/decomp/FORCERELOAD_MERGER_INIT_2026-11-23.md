# FileMerger::Merger::mForceReload: retail, the native init, and the "splash stream" (2026-11-23)

Lane `forcereload`, branched from `sanitize` (since landed as `0ddb8585b`). It follows up row 10 / §4 of
`SANITIZE_NATIVE_ASAN_UBSAN_2026-11-23.md`. That lane backed out a native `mForceReload = false` init because,
in its runs, the splash screen's stream "never started" with the init in place.

**Result.** The init is right and is now landed natively (`FileMerger.h`, both ctors, `HX_NATIVE` only). The
stream starts in every run of every arm. What the byte changes is *which* shell-music loop gets picked, through
the RNG. The sanitize lane's marker, a UBSan report in `Keygen_Stub`, fires only for v12–16 moggs, so it read
"the v11 loop was picked" as "no stream". There is no second native bug behind this one.

## 1. Retail readers and writers of 0x29

These come from a whole-`.text` scan of band.exe for `stb`/`lbz` with displacement 0x29 (27 functions in all),
bounded by `.pdata` and filtered to Merger code. The script is `~/tmp/forcereload/scan29.py`. It does not
catch indexed (`stbx`) or `addi`-then-0 forms.

| VA | function | access |
|---|---|---|
| `0x8239117C` | `FileMerger::NeedsLoading` | **the only read**: `mLoaded != mSelected` (or `mSelected != loading` while pending) `\|\| mForceReload` |
| `0x823918B0` | `FileMerger::Select(Symbol, const FilePath &, bool)` | store of the bool argument (the inlined `SetSelected`) |
| `0x82392514` | `FileMerger::AppendLoader` | store 0 |
| `0x8232E2D8` | `BandWardrobe::SelectExtra` | store 0 |
| `0x822813C0`, `0x82281430` | `BandCharacter::SetDircuts` | store 0 (`r20 = li 0` at `0x822812AC`) |

Places that do not touch it:

- `Merger(Hmx::Object *)` `0x8232DCF8` stores 0x28 and 0x2a (`0x8232DD58`/`5C`) and skips 0x29.
- The copy ctor `0x8232E6D0` constructs the members and then calls `operator=` `0x8232DE80` (`0x8232E7A0`),
  which copies 0x28 and 0x2a and not 0x29.
- The sort, swap and `_M_insert_overflow` helpers all go through those two functions. So the flag stays with a
  vector slot when mergers are sorted. When the vector reallocates, the flag is replaced by the new block's
  contents.

## 2. What value retail sees

- **How mergers are built.** `FileMerger::PreLoad` (`0x823952B8`) calls `operator>>(BinStream&, ObjVector<Merger>&)`
  (`0x82395258`). That calls `ObjVector::resize` (`0x823951E8`), which builds a stack temp
  `Merger(mOwner)` and then calls `vector::resize` → `_M_fill_insert` (`0x82394C30`) →
  `_M_insert_overflow_aux` (`0x82331FD8`). That path copy-constructs every element from the temp.
- **Where the byte comes from.** Because of the copy path above, the temp's own 0x29 (stack garbage) is never
  copied. Each element's 0x29 is whatever its new heap block already held.
- **The allocator.** The block comes from `MemOrPoolAllocSTL` `0x827BD208` (`bl` at `0x82332048`). For
  `size > 0x80` it calls `MemAlloc(size, 0)` `0x827BCD38` → `MemHeap::Alloc` `0x827BCA78`. Otherwise
  (a one-merger vector, 0x64 B) it calls `PoolAlloc` `0x827BB0E8`. None of these call `memset` (`0x8282AE30`),
  `XMemSet` or `XMemSet128`.
- **Heap fill.** MemHeap fills free blocks with 0xDEADDEAD only when `mDebugLevel >= 1`. The Xbox heaps in
  `config/mem.dta` (`main` 105,000,000 and `char` 32,000,000) set no `debug` key, so the level is 0 and there is
  no fill. The heaps' backing store is one CRT `malloc` per heap (`AddHeap`).
- **The value.** On the 360 the byte is the previous occupant's byte at that offset, or the heap's initial
  contents if the block was never used.
- **What is not settled.** A boot-time block has been through earlier allocations, so the value cannot be
  derived from the code alone. It is deterministic for a given boot, and nothing here observed it on hardware
  or in Xenia.
- **Why `false`.** It is the only value that does not depend on heap history. Every retail writer except
  `Select`'s argument stores it, and it is the value of a never-used block. Natively, a plain (non-ASan) build
  already saw 0 in 210 of 210 traced mergers (fresh glibc memory). The init makes that unconditional.

## 3. Why the byte changed the "splash stream"

The runs used a temporary env-gated trace build (patch: `~/tmp/forcereload/trace_instrumentation.patch`, not
committed). Its `FR_FORCE=<n>` sets the byte in the copy ctor, so `FR_FORCE=0` behaves exactly like the fix.

1. **The byte decides whether empty mergers reload.** On the splash path every traced merger has `sel=''` and
   `loaded=''`, so `NeedsLoading` returns the byte. With the byte unset (0xBE under ASan, or under
   `GLIBC_TUNABLES=glibc.malloc.perturb=65`), 76 (ASan) or 208 (perturb) of 210 queue a `NullLoader`. With it
   false, none do.
2. **The reloads draw from the RNG.** The extra async loads add four `FileMergerOrganizer::StartLoad` batches.
   Each one draws once: `gGenderChirality = RandomInt() & 1` (retail `0x823DA6F4` `bl RandomInt`
   `0x824F2F60`). Counting `Rand::sRand` draws before `MetaPanel::Load` gave 1107 with the byte unset and 1103
   with it false. The diff of the callers' backtraces is exactly 4 × `FileMergerOrganizer::StartLoad` ←
   `FileMergerOrganizerLoader::PollLoading`.
3. **The draws change the pick.** `Rand::Seed` is fixed, so the pick is deterministic. `MetaPanel::PickLoopIndex`
   returns loop **7** (a 3,060,673 B buffer, mogg **v16**) with the byte unset and loop **1** (2,482,157 B,
   **v11**) with it false.
4. **Only one of those reaches the marker.** In `VorbisReader::CheckHmxHeader`, v12–16 take the
   `HvDecrypt`/keygen branch, which is where the `Keygen_Stub.cpp:63` UBSan report comes from. v11 takes the
   `gRB1Key` CTR branch, which never touches it.
5. **The stream starts either way.** `MetaMusic::Start` → `StandardStream::Play` runs by frame ~14 in both
   cases, and the header is parsed in both.

## 4. Runs

All runs are `RB3_APP_RUN=1`, 300 frames, through `no610m.sh` with `VK_ICD_FILENAMES=/nonexistent` (Dawn Null).
Each one ended `RESULT: APP RAN 300 FRAMES screen=splash_screen` with rc 0. For an ASan binary `ulimit -d
unlimited` is required: without it ASan's shadow reservation fails with ENOMEM (rc 1, a first batch lost to it).

Trace build, 5 runs per arm. The table was identical in every run of an arm:

| arm | mergers reloading | loop buffer | mogg ver | `stream_play` | Keygen UBSan reports |
|---|---:|---:|---:|---:|---:|
| plain, default malloc | 0 | 2,482,157 | 11 | 1 | n/a |
| plain, perturb 0xBE | 208 | 3,060,673 | 16 | 1 | n/a |
| plain, perturb 0xBE + `FR_FORCE=0` | 0 | 2,482,157 | 11 | 1 | n/a |
| ASan+UBSan, unset | 76 | 3,060,673 | 16 | 1 | 8 |
| ASan+UBSan, `FR_FORCE=0` | 0 | 2,482,157 | 11 | 1 | 0 |

Fixed binaries (commit `e4a5f4578` before the rebase, now `397fe2a7c`; no trace), 5 runs per arm: see §4.1.

### 4.1 Fixed build

- **plain, default malloc:** 5/5 `APP RAN 300 FRAMES`, rc 0.
- **plain, perturb 0xBE:** 5/5 `APP RAN 300 FRAMES`, rc 0. This is the arm the init exists for. The trace
  build's equivalent arm (`FR_FORCE=0`) picked loop 1 (v11) and started the stream in 5/5 runs.
- **ASan+UBSan:** 5/5 `APP RAN 300 FRAMES`, rc 0. Each run has 14 `runtime error` lines, against 17 in the
  unset trace arm. That is 0 `FileMerger::NeedsLoading` invalid-bool reports (1 before) and 0
  `Keygen_Stub.cpp` reports (consistent with the v11 pick).

## 5. X360 Δ

This was measured as the reverse of the fix: `tools/ab_measure.py --revert e4a5f4578` on `sanitize` + the fix (pre-rebase hash; now `397fe2a7c` on main `56393ad26`, not re-measured after the rebase).
The ruler was `functionRelocDiffs=name_check` and objdiff `sha256:ec3badc0f33323e9`. Leg B recompiled 2,421
MSVC objs over 3 settle iterations, so the header did reach the X360 TUs.

- Δmatched **+0**, Δmasked_equal +0, Δhonest +0, Δcode% +0.000000 pp, Δcode_bytes +0, Δfuzzy +0.000000 pp.
  Units at 100%: 635 → 635 (mpn), 548 → 548 (fuzzy). The `none` control was also +0.
- Leg A: matched 55,268, honest 29,908, code 59.665310%.
- Run dir: `~/tmp/wt-forcereload/.ab_measure_runs/20261123-003552-forcereload-reverse-1078155/`.

Both inits sit under `HX_NATIVE`, so the change is codegen-neutral on the 360 by construction, and the
measurement agrees.

## 6. Not done

- The value retail actually holds was not read on hardware or in Xenia (§2).
- The v11 loop was not checked for audible output beyond `Play` and the header parse.
- `native_health.sh --selftest` was not run, per the brief.
- No change to `operator=`. Retail does not copy the flag, and the native build keeps that, so a sort leaves
  the flag with its slot as on the 360.
