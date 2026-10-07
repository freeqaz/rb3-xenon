# W16-UO — the native targets under AddressSanitizer: every finding fixed (2026-10-07)

Branch `w16-uo`, worktree `~/tmp/wt-w16uo`, based on main `c200efb55`.
Takes up the memory errors W16-UL recorded and left alone
(`docs/decomp/W16UL_VENUE_CELL_SIDE_AND_UNLOAD_LEAK_2026-10-07.md` §2.5, §6): the
out-of-bounds read at `Text.cpp:733`, the stack use-after-scope at
`AmbientOcclusion.cpp:1256`, and the mismatched `new[]`/`delete` pairs.

## 0. Headline

- **All 18 native targets now run clean under ASan:** 0 reports, every target to its
  completion line, rb3-render's 414 gates all PASS (§3).
- **Eight finding sites and four fixes** (§1); one further site appeared only once a fix
  landed.
  - W16-UL's ASan run stopped at its first error and ran only rb3-render, so it saw three
    sites. Its valgrind run saw two more (rows 7, 8).
  - New here: three more use-after-scope sites in the same function, and the CharClip
    mismatch that the cascade fix exposed.
  - The sites, by fix:
    - `Text.cpp` `segmentLength`: a 1-byte read before the string.
    - `AmbientOcclusion.cpp` `Tessellate`: **four** use-after-scope sites, not one (phase
      1 and the three phase-3 edges).
    - `KerningTable`: `new Entry[]` freed with `delete`.
    - **One allocation-family defect behind every other mismatch.**
- **How the allocation-family defect arose.**
  - In retail the global `operator new` *is* MemAlloc, so a class overload, the global
    operators and `~ObjectDir`'s frees reach one allocator.
  - Natively there are two: libstdc++'s `new`/`delete`, and `malloc`/`free` behind
    MemAlloc/MemFree.
  - The `TrackWidgetImp` `DELETE_OVERLOAD` freed global-new blocks with `free`, and so did
    `~ObjectDir`'s cascade.
  - Once the cascade freed with `::operator delete` instead, `CharClip`'s hand-written
    MemAlloc `operator new` mismatched the other way round.
  - The fix makes every native `Hmx::Object` allocation the global `operator new`.
- **A sanitizer check that fails without each fix** (§2): `tools/native_asan.sh`, with
  a `RB3X_SANITIZE=address` CMake option.
  - Sabotage S1/S1b builds this branch's tooling over main's `src/`. S1b reports all 8
    sites, each at the line its fix names. S1 reported 5, because of the equal-PC trap
    below.
  - Run 1, taken before the CharClip fix, is the control for that fix.
- **X360 Δ0 on every key**, including Δfuzzy (§4). The first, unconditional form of the
  Tessellate fix cost Tessellate 74.68 → 73.47 and was moved behind `HX_NATIVE`.
- Two traps in the instrument, both measured (§2.2):
  - at `-O1`, ASan cannot see a use-after-scope like Tessellate's;
  - in recover mode, ASan prints an equal faulting PC only once, so one finding can
    hide another.

## 1. The findings and fixes

Counts are rb3-render report counts from sabotage S1b (§2.3), the run that shows every
site at once. All other 17 targets were clean in every run, with or without the fixes.

| # | site (main's line) | ASan kind | S1b reports | fix | X360 build |
|---|---|---|---:|---|---|
| 1 | `Text.cpp:733` `segmentLength` | heap-buffer-overflow, 1 byte before the string | 3 | test the bound before the byte | `HX_NATIVE` only |
| 2 | `AmbientOcclusion.cpp:1256` `Tessellate` phase 1 | stack-use-after-scope (`FacePriority`) | 200 | `AO_BRANCH_TEMP` | token-identical |
| 3–5 | `AmbientOcclusion.cpp:1528`, `1543`, `1558` phase 3 | stack-use-after-scope (`RndMesh::Face`) | 10 each | `AO_BRANCH_TEMP` | token-identical |
| 6 | `Font.cpp:35` `~KerningTable` | alloc-dealloc-mismatch (`operator new []` vs `operator delete`) | 151 | `delete[]` | `HX_NATIVE` only |
| 7 | `MemMgr.cpp:203` `MemFree` ← `*WidgetImp::operator delete` | alloc-dealloc-mismatch (`operator new` vs `free`) | 2,433 | class overloads use the global operators | native macros only |
| 8 | `Dir.h:634` `FlushDeferredFrees` | alloc-dealloc-mismatch (`operator new` vs `free`) | 156 | cascade frees with `::operator delete` | `HX_NATIVE` only |
| (9) | `FlushDeferredFrees` on `CharClip` blocks | alloc-dealloc-mismatch (`malloc` vs `operator delete`) | 4,524 in run 1 | `NativeObjAlloc` | token-identical |

Row 9 appeared only after fix 8, and is part of the same defect (§1.4).

### 1.1 `segmentLength` (row 1)

- `for (; c6[i2 - 1] == ' ' && i1 < i2; ...)` reads the byte before testing the bound.
  For a segment of only spaces that starts at byte 0, the trim walks `i2` down to 0
  and then reads `c6[-1]`.
- **Retail does the same read.** In `WrapText` (`0x82457F70`, where this is inlined),
  retail loads `lbz r11, -0x1(r11)` and compares `cmpwi r4, 0x20` before
  `cmpw r9, r11; bge`. WrapText is 84.29% matched, so our source order alone would not
  have settled this; the retail bytes do.
- **The result cannot depend on the byte.** When `i1 < i2` is false the `&&` is false
  whatever was read. Swapping the operands natively changes no outcome, only the
  out-of-bounds read. On X360 the source keeps retail's order.

### 1.2 `Tessellate` (rows 2–5)

- Each branch of an `if` declares its own block-scoped temporary, a pointer is left
  pointing at it, and the pointer is dereferenced after the block closes:
  - phase 1, `FacePriority fp` / `fp2` → `priorities.push_back(*pFP)`;
  - phase 3, `RndMesh::Face tmpA` / `tmpB` → `newFaces.push_back(*pFace)`, three times.
- MSVC keeps those slots live, so retail pushes the value just computed. In C++ it is
  undefined, and natively ASan reads the copy as a use of a dead object.
- W16-UL recorded one site because its ASan run stopped at the first report. Fixing
  phase 1 exposed phase 3 (run 1, §3); S1b shows all four at once.
- **Fix:** `AO_BRANCH_TEMP(T, name, live)`. Natively it is `T &name = live`, a
  reference to one object declared before the `if` (`fpLive`, `edgeLive`) that
  outlives the branches. On X360 it is `T name`, token-identical to main.
- **The first, unconditional form cost the match build.** Hoisting one object for
  both builds read Δfuzzy −0.000580 pp: Tessellate 74.68 → 73.47, `fn_82492CEC` 99.8 →
  99.4, `fn_82492D14` 99.8 → 99.3 (§4). The scoped temporaries are closer to retail's
  frame, so the X360 source keeps them.

### 1.3 `KerningTable` (row 6)

- `Load` allocates `mEntries = new Entry[n]`, and both `Load` and the dtor free it with
  `delete`. (`SetKerning` already used `delete[]`.)
- **Retail mixes them too, harmlessly.** Retail's `Load` calls `??2@YAPAXI@Z`
  (`0x827BD2F0`) to allocate and `fn_8240DDB0`, an ICF-folded scalar `??3`, to free.
  Both reach MemAlloc/MemFree. `Load` is 100% matched with `delete`.
- Natively `new[]` and `delete` are different libstdc++ operators. Fix:
  `KERNING_DELETE_ENTRIES` is `delete[]` natively and `delete` on X360.

### 1.4 One allocation family for `Hmx::Object` (rows 7–9)

**Retail has one allocator.** Retail's global `operator new`/`delete`
(`utl/MemMgr.cpp`) are `MemAlloc(size, 0)`/`MemFree`. So a class overload, the global
operators and `~ObjectDir`'s frees may be mixed freely, and the code does mix them:

- the `TrackWidgetImp` family declares `DELETE_OVERLOAD` (MemFree) alone and is
  allocated by the global `new` (`TrackWidget::SyncImp`);
- `ClipGraphGenerator` has no class overload at all, so the global `new` allocates it;
- most `Hmx::Object` classes use `OBJ_MEM_OVERLOAD` (MemAlloc);
- `~ObjectDir`'s native cascade runs each dtor and frees the block itself, whatever
  allocated it (`DeleteObjects` phase 2, `ObjDirPtr::operator=`, `FlushDeferredFrees`).

**Natively there are two families.** MemAlloc/MemFree are `malloc`/`free`
(`MemMgr.cpp:411`, `:203`), and the global operators are libstdc++'s
(`MemMgr.cpp:107`: "do NOT override global operator new/delete"). The cascade freed
with `free()`, which was right for `OBJ_MEM_OVERLOAD` objects and wrong for
`ClipGraphGenerator` (row 8). The widget deletes were wrong the other way round
(row 7).

**The rule natively:** every `Hmx::Object` allocation is the global `::operator new`,
and the cascade frees with `::operator delete`. In detail:

- `utl/MemMgr.h`, native block:
  - `OBJ_MEM_OVERLOAD` and `OBJ_NEW_OVERLOAD` allocate with `NativeObjAlloc`
    (= `::operator new`);
  - `MEM_OVERLOAD`, `MEM_ARRAY_OVERLOAD`, `NEW_OVERLOAD` and `DELETE_OVERLOAD` use the
    global operators.
- `obj/Dir.cpp` `NativeObjMemFree`: the free outside a cascade is `::operator delete`.
- `obj/Dir.h`: `FlushDeferredFrees` and `ObjDirPtr::operator=`'s immediate free use
  `::operator delete`.
- The **hand-written** `Hmx::Object` operators get the same rule: `CharClip` (new and
  delete), `PatchDir` (new and delete), `StarDisplay` and `MiniLeaderboardDisplay`
  (new only; they inherit the `OBJ_MEM_OVERLOAD` delete). Row 9 is `CharClip`: after
  fix 8 its MemAlloc block met the cascade's `::operator delete` 4,524 times.

**Why not make the global operators `malloc` instead.** That would also be "one family,"
but every pairing would then look legal to ASan, including `new[]`/`delete` (row 6),
so the check would lose that class of finding for good. Clang's sized deallocation would
also need every `operator new`/`delete` variant replaced. MemAlloc as `operator new` was
also rejected: native `MemRealloc` calls `realloc` on MemAlloc blocks (`gNumHeaps == 0`
natively).

**Left alone, measured:**
- Classes with `POOL_OVERLOAD` (`malloc` through `PoolAlloc`) include `Hmx::Object`s
  (`CharClipDriver`, `AnimTask`, `SeqInst`, …). If one were destroyed by a cascade it
  would mismatch. None was in any run: all 4,524 run-1 reports have the allocating
  frame `CharClip::operator new`, and the final runs are clean.
- Non-`Object` classes with hand-written MemAlloc/MemFree pairs (`ArkFile`,
  `ChunkStream`, `FileStream`, `RndMesh::Vert`, `AsyncFile`) are consistent within
  themselves and never reach the cascade.


## 2. The check: `tools/native_asan.sh`

### 2.1 What it does

- `native/CMakeLists.txt` gains `RB3X_SANITIZE` (default empty, so the normal build and
  the native gate never see it). With `-DRB3X_SANITIZE=address` it compiles every target
  and the engine with
  `-fsanitize=address -fsanitize-recover=address -fno-omit-frame-pointer
  -fsanitize-address-use-after-scope -mllvm -asan-globals=0 -g`.
  `-asan-globals=0` is W16-UL's workaround: without it `--gc-sections` keeps 567
  undefined references.
- `tools/native_asan.sh [--no-build] [--only …] [WORKTREE]`:
  - configures and builds `native/build-asan`;
  - lifts the data limit (`ulimit -d unlimited`);
  - runs all 18 targets with `native_health.sh`'s inputs and completion markers;
  - fails on any ASan report, crash, hang, nonzero rc, gate FAIL or missing completion
    line.
- It **refuses** when its target list differs from `native_health.sh`'s `run_target`
  rows, so a new target cannot go unsanitized unnoticed.
- `ASAN_OPTIONS=halt_on_error=0:suppress_equal_pcs=0:detect_leaks=0:alloc_dealloc_mismatch=1:new_delete_type_mismatch=1`.
- Each report is reduced to `kind<TAB>first frame with a file:line in our tree`; the
  distinct list is printed.
- Last line: `NATIVE_ASAN_RESULT verdict=… targets ran clean reports distinct rc`.
  `rc` is 0 clean, 1 findings or failure, 2 cannot run, 3 partial or incomplete.
- `--selftest` builds four toy programs (heap overflow, `new[]`/`delete`, a
  use-after-scope shaped like Tessellate's, and a clean control). It requires the
  parser to count exactly 1, 1, 1 and 0.

### 2.2 Two traps in the instrument, both measured

- **`-O1` hides the use-after-scope.** The first version compiled at `-O1`. Its own
  selftest went red: the Tessellate-shaped toy reported at `-O0` and was silent at
  `-O1`, even with the object escaping through a `noinline` call. The sanitizer build
  now uses no `-O`, the same `-O0` the shipping native build uses.
- **Recover mode prints an equal PC once.** S1 reported phase 1's use-after-scope and
  none of phase 3's. Every one faults inside `__asan_memcpy` (the `push_back` copy),
  so they share one PC, and ASan's default `suppress_equal_pcs=1` printed only the
  first. That is how one finding hides another. With `suppress_equal_pcs=0`, S1b
  reports all four sites (200 + 10 + 10 + 10).
- **A clean run is not affected by suppression.** It only drops a report after an
  equal-PC report has already printed, so a run with 0 reports suppressed nothing.
- Also fixed before any result was used: the first parser took the ASan runtime frame
  (`operator delete (…/native/build-asan/rb3-render+0x…)`) as the site, because the
  binary's path contains `/native/`. It now requires a `file:line` frame.

### 2.3 It fails without each fix

| run | `src/` | rb3-render reports | distinct sites |
|---|---|---:|---|
| **S1** (old options) | main's (`c200efb55`) | 2,742 | 5: rows 1, 2, 6, 7, 8 — phase 3 hidden (§2.2) |
| **S1b** (`suppress_equal_pcs=0`) | main's | 2,973 | **8: rows 1–8**, at main's lines |
| **run 1** | fixes for rows 1, 2, 6, 7, 8 (row 2 unguarded), not row 9 or 3–5 | 4,525 | row 9 (4,524) and one phase-3 site (1532) |
| run 2 | every fix (Tessellate unguarded) | **0** | — |
| **final** | every fix, Tessellate guarded (`AO_BRANCH_TEMP`) | **0** | — |

- **S1** was a separate detached worktree with this branch's tooling and `src/` reverted
  to main (since removed). It ran the full `native_asan.sh`: build, then all 18 targets.
  Its 17 other targets were clean.
- **Prediction for S1** (`~/tmp/w16uo/S1_predictions.txt`, written before the build):
  six distinct findings at rows 1, 2, 3–5 (one entry), 6, 7, 8; no CharClip finding;
  other targets clean. **Measured:** five. Phase 3 was missing, which exposed the
  equal-PC trap. After the option change, S1b matched the prediction and resolved
  phase 3 into its three edges.
- The rows 7 and 8 breakdowns, from S1b's allocation stacks:
  - row 7 is `ImmediateWidgetImp` 2,017, `MultiMeshWidgetImp` 388, `CharWidgetImp` 16,
    `MatWidgetImp` 12;
  - row 8 is `ClipGraphGenerator` 156, the one class seen in a cascade with no class
    `operator new`.
- **Run 1** is the sabotage for the `CharClip` half of fix 7–9, and the evidence that
  phase 3 existed before S1b showed it.


## 3. Clean run

`tools/native_asan.sh` at the final source (`~/tmp/w16uo/asan_final.log`), build included:

```
  CLEAN      rb3-dta … rb3-milo   (17 targets, 0–5 s each)
  CLEAN      rb3-render   -- 0 ASan reports, rc=0, completed (158s)
NATIVE_ASAN_RESULT verdict=CLEAN targets=18 ran=18 clean=18 reports=0 distinct=0 rc=0
```

- Under ASan rb3-render printed **414 PASS, 0 FAIL**, `RESULT: ALL GATES PASSED`.
  That is the count W16-UL recorded for the normal build. `ul-venues-freed` read
  52 of 52 (4,030 created, 0 left alive), and `ul-cascade-delete-deferred` 1,339
  deferred frees, the same figure as W16-UL.
- The selftest passes (1, 1, 1, 0).
- **What the instrument covers.** These are the targets' default runs, with
  `native_health.sh`'s inputs. Code those runs do not reach is not sanitized.


## 4. X360 A/B

- **Prediction: Δ0 on every key.** Every `src/` edit is either inside `HX_NATIVE` or
  expands to the same tokens on X360 (`KERNING_DELETE_ENTRIES`, `AO_BRANCH_TEMP`, the
  `CharClip` `#ifdef` split).
- **Method.** `tools/ab_measure.py --worktree ~/tmp/wt-w16uo --patch <diff> --jobs 8`,
  where the patch is `git diff HEAD c200efb55 -- src/`, so leg B is this branch with
  main's `src/`. `--revert` was refused first: the commit could not be reverse-applied
  because a later commit edited `native_asan.sh`.

**First A/B: the unconditional Tessellate hoist** (`~/tmp/w16uo/ab2.log`):

```
leg A: matched=55027 masked=25227 honest=29800 code%=59.390015  (recompiles: 0, settled)
leg B: matched=55027 masked=25227 honest=29800 code%=59.390015  (recompiles: 1128, …)
Δmatched=+0  Δmasked_equal=+0  Δhonest=+0  Δcode%=+0.000000pp  Δcode_bytes=+0
Δfuzzy=+0.000580pp   (legA 64.380050 -> legB 64.380630)
```

- **The prediction failed on Δfuzzy.** Removing the fixes *gained* 0.000580 pp.
- A per-row diff of the two archived `report.json`s puts all of it in
  `AmbientOcclusion`:
  - `?Tessellate@RndAmbientOcclusion@@QAAXPAM0@Z` 74.68 → 73.47 with the fix;
  - `fn_82492CEC` 99.8 → 99.4;
  - `fn_82492D14` 99.8 → 99.3.
- So the fix was moved behind `HX_NATIVE` with `AO_BRANCH_TEMP` (§1.2).

**Second A/B, final source** (`~/tmp/w16uo/ab3.log`):

```
leg A: matched=55027 masked=25227 honest=29800 code%=59.390015  (recompiles: 0, settled)
leg B: matched=55027 masked=25227 honest=29800 code%=59.390015  (recompiles: 1128, split=0, patch_steps=6, settle iterations: 2)
Δmatched=+0  Δmasked_equal=+0  Δhonest=+0  Δcode%=+0.000000pp  Δcode_bytes=+0
Δfuzzy=+0.000000pp   (legA 64.380630 -> legB 64.380630)
units at 100% [mpn ruler]: legA 629 -> legB 629
```

- The `none` control was flat (+0 B).
- Leg B recompiled 1,128 objects, so the A/B is not absent-vs-absent.
- The tool restored the worktree and verified it.


## 5. Native gate, health and valgrind

**Native gate**, on the final source (`~/tmp/w16uo/gate1.log`):

```
layout:    LAYOUT_ODR_RESULT verdict=PASS x360_tus=1267 x360_failed=0 … native_tus=1960 native_failed=0 … rc=0
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

`tools/native_build_gate.sh` was run again as the lane's last action, after the doc
commit; that line is in the final report.

**Runtime**, normal (unsanitized) build: `tools/native_health.sh --skip-link`
(`~/tmp/w16uo/health1.log`). The link gate is the run above.

```
NATIVE_HEALTH_RESULT verdict=INCOMPLETE link=SKIPPED … runtime=PASS runtime_ran=18 runtime_total=18 gates_pass=511 gates_fail=0 unrunnable=none … runtime_crashed=0 runtime_failed=none retail_config=12/12 unhandled_calls=0
```

- `INCOMPLETE` / rc 3 is only `link=SKIPPED`.
- `gates_pass=511` is the figure W16-UL measured at `bcfdbc30f`. The allocator change
  moved no gate.

**valgrind**: W16-UL's exact configuration (default cells plus the bandtrack phase,
`--undef-value-errors=no --freelist-vol=4000000000`) on the normal build
(`~/tmp/w16uo/vg1.vglog`).

| | W16-UL (`~/tmp/w16ul/vg2.vglog`) | W16-UO |
|---|---|---|
| ERROR SUMMARY | 1,328 errors from 10 contexts | **4 errors from 4 contexts** |
| `Invalid read of size 1` (`segmentLength`) | 1 | 0 |
| `Mismatched free() / delete / delete []` | 5 (4 `*WidgetImp`, 1 `FlushDeferredFrees`) | 0 |
| `realloc() with size 0` / `posix_memalign() invalid size value: 0` | 4, all in `libnvidia-glcore` / `libnvidia-eglcore` during `dlopen` | the same 4 |
| rb3-render | `RESULT: ALL GATES PASSED` | `RESULT: ALL GATES PASSED`, rc 0 |

- **Prediction: 0 errors. Wrong:** 4. All 4 are inside NVIDIA's proprietary GL/EGL
  driver as it initialises (`NvGlEglApiInit`, `_dl_init`), with no frame of ours. W16-UL
  had the same 4, and I had not counted them when I wrote the prediction.
- Every context in our code is gone.
- valgrind covers what ASan cannot see inside `SafeReleaseFromRing` (§6). It reports no
  invalid write there.


## 6. Not done

- **LeakSanitizer is off** (`detect_leaks=0`). Every driver exits with its engine state
  still allocated, as retail never tears down, so exit-time leak reports would measure
  the harness. Unload leaks remain gated by rb3-render's `ul-venue-freed`.
- **Global-variable overflows are not checked** (`-asan-globals=0`, needed for the
  `--gc-sections` targets to link; §2.1).
- **`ObjRef::SafeReleaseFromRing` is still `no_sanitize("address")`** (`Object.h`). ASan
  is blind inside it, and that blind spot is how W16-UL's `ArpeggioShape` heap write
  hid from ASan. valgrind is the instrument for it (§5); I did not remove the
  attribute.
- **`POOL_OVERLOAD` `Hmx::Object`s** (`CharClipDriver`, `AnimTask`, `SeqInst`, …) would
  mismatch if a directory cascade ever destroyed one. None does on these runs (§1.4).
  I left the pool alone rather than change an allocator nothing exercised.
- **`native_asan.sh` is not wired into `native_build_gate.sh` or `native_health.sh`.**
  An ASan build is a second full native build (about 25 min at `-j8`, 2.3 GB). It is a
  separate check, to be run when native memory behaviour changes.
- **Merging and pushing** are left to the coordinator.


## Reproduce

```
# the sanitizer check (configures + builds native/build-asan, runs all 18 targets)
tools/native_asan.sh                       # last line NATIVE_ASAN_RESULT …; rc 0 = clean
tools/native_asan.sh --no-build --only rb3-render
tools/native_asan.sh --selftest            # the parser can fail: 1, 1, 1, 0

# by hand
cmake -S native -B native/build-asan -G Ninja -DCMAKE_C_COMPILER=clang \
      -DCMAKE_CXX_COMPILER=clang++ -DRB3X_SANITIZE=address
cmake --build native/build-asan -- -j8
ulimit -d unlimited
ASAN_OPTIONS=halt_on_error=0:suppress_equal_pcs=0:detect_leaks=0 \
  native/build-asan/rb3-render ~/code/milohax/rb3/orig-assets/xbox-zip <outdir>

# sabotage S1: this branch's tooling over main's src/, in a scratch worktree
git worktree add --detach ~/tmp/wt-sab <this branch>
git -C ~/tmp/wt-sab checkout c200efb55 -- src/
~/tmp/wt-sab/tools/native_asan.sh ~/tmp/wt-sab   # 8 distinct sites in rb3-render, rc 1

# X360
git diff HEAD c200efb55 -- src/ > ~/tmp/revert_src.diff
python3 tools/ab_measure.py --worktree <wt> --patch ~/tmp/revert_src.diff --jobs 8

# valgrind (W16-UL's configuration)
DEBUGINFOD_URLS=https://debuginfod.archlinux.org valgrind --undef-value-errors=no \
  --freelist-vol=4000000000 native/build/rb3-render <assets> <outdir> --no-w16sh --no-w16tf \
  --no-w16tj --no-w16tm --no-w16tr --no-w16ts --no-w16tw --no-w16ty --no-w16ub --no-w16uf \
  --no-w16uj --no-w16ul
```
