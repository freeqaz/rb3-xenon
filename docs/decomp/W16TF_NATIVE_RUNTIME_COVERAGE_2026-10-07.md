# W16-TF: native runtime coverage of in-scope gap rows (2026-10-07)

Branch `w16-tf`, worktree `~/tmp/wt-w16tf`, cut from main `e0cca0a7f`.

**Brief.** `CAMPAIGN_STATE_2026-10-07b.md` §6 lever 3. Native compiles 99.0% of the
in-scope gap, but only about 7,564 B of it was known to run under a gate (W16-SH), and
`tools/native_runtime_rank.py` had not been rerun since W16-SH linked 83 files.
`tools/native_stub_census.py` also missed two stub files (§6 lever 8). Fix the census,
rerun the ranking, and gate the highest-ranked in-scope gap code native can execute.

## 0. Headline

| | value |
|---|---|
| census `STUB_RE` | lists `bandtrack_link_stubs.cpp` (26 definitions) and `w16sh_link_support.cpp` (10), and now **refuses** on any unlisted `native/src` object |
| ranking tool defect found | rb3-render's profile was **0 bytes** (it ends in `_exit()`), so nothing it ran was ever counted |
| in-scope gap rows executed, tool as it was | 37 rows / 23,828 B |
| ... with the rb3-render profile fixed (same code) | **59 rows / 35,996 B** |
| ... after this lane's gates | **74 rows / 43,716 B** (+15 rows / +7,720 B, all under a gate) |
| native behaviour bug fixed | `MakeBSPTree` was `return false;` under `HX_NATIVE`: every `kVolumeBSP` mesh lost its collision tree |
| new gates | 31 in rb3-render (`native/src/w16tf_phase.cpp`), all pass; 2 sabotage controls fail exactly as predicted |
| X360 build | **Δ0** on every key (`ab_measure`, 17 leg-B recompiles) |

"In-scope" is the campaign's ring (IN-CORE + IN-SOON + IN-RB3ENG, scaffold and
unpairable units dropped), not the tool's old `src/band3 + src/system` filter. "Executed"
means the function was entered at least once by one of the 18 targets in
`native_health.sh`'s argv.

## 1. `native_stub_census.py`

- `STUB_RE` now lists the two files. Run read-only on main's `native/build`, the
  per-file counts are 26 and 10, which is W16-TA §4.3's `nm` patch exactly.
- New guard: every `native/src/*.o` on a link edge must match `STUB_RE` or a new
  `NON_STUB_RE` (the `main_*` drivers, `bandtrack_phase`, `w16sh_phase`, `w16tf_phase`,
  `score_engine`, `cc5_stub_probe`). Otherwise exit 3, naming the objects.
- Control: the patched tool with the old `STUB_RE` refuses on main's build and names
  exactly `bandtrack_link_stubs.cpp.o` and `w16sh_link_support.cpp.o`. With the fix it
  passes (18 targets, 7,952 rows).

The census still cannot see the class this lane found in §3: a stub written as an
`#ifdef HX_NATIVE` arm inside the real TU. That class has no file to list.

## 2. `native_runtime_rank.py`

**The defect.** `main_render.cpp` ends with `_exit()` (static GPU caches segfault in
their destructors). `_exit` skips clang's profile writer, which runs from `atexit`. The
first run this lane made reported rb3-render `rc=0 OK profraw=1`, but the file was 0
bytes, and the tool credited rb3-render with nothing. rb3-render hosts the W16-PX and
W16-SH gates and links most of W16-SH's 83 files, so every earlier ranking undercounted
the one target with the most code.

Fixes:
- `main_render.cpp` calls a weak `__llvm_profile_write_file()` before `_exit`. It is
  null unless the target is built with `-fprofile-instr-generate`.
- New self-checks (exit 3): a target left an empty profile, or a target compiles repo
  `src/` objects but counted none. Both fired on the old profiles, for rb3-render only.
  rb3-frame compiles only `main_frame.cpp`, so its 0 is correct and is exempted by
  that rule, not by name.
- Rows carry their scope ring (`scripts/native_scope_map.py` + the DC3-path split, as in
  W16-OV's `scope_ledger2.py`), and the tool prints executed rows/bytes per ring.
  Cross-check: VIA-DC3 gap 198,548 B equals W16-TA's figure to the byte. IN-SCOPE
  202,240 B is W16-TA's 202,324 B minus W16-TB's +84 B, which landed in between.

After the fix rb3-render writes a 9.8 MB profile and enters 3,636 repo `src/` functions.

### 2.1 Coverage by ring (report.json at main `e0cca0a7f`, `name_check`)

| ring | gap rows | gap B | exec, blind tool | exec, profile fixed | exec, after gates |
|---|---:|---:|---:|---:|---:|
| **IN-SCOPE** | 551 | 202,240 | 37 / 23,828 | 59 / 35,996 | **74 / 43,716** |
| IN-CORE | 133 | 43,888 | 15 / 11,748 | 24 / 14,324 | 39 / 22,044 |
| IN-SOON | 274 | 100,256 | 20 / 10,296 | 22 / 11,012 | 22 / 11,012 |
| IN-RB3ENG | 144 | 58,096 | 2 / 1,784 | 13 / 10,660 | 13 / 10,660 |
| VIA-DC3 | 472 | 198,548 | 4 / 5,536 | 30 / 21,524 | 30 / 21,524 |

Join tiers for the 551 in-scope rows: 256 have a native definition the tool can join
(sig 242, arity 12, name 2), 276 are anonymous `fn_`, 17 have no native definition and
2 did not parse. Of the 256 joinable rows, **197 rows / 143,828 B were linked but never
entered** before this lane. Ranked by file, the largest are `VocalTrack.cpp` 11,532
(`UpdateScrolling` alone 8,948), `BandPatchMesh.cpp` 7,528, `CustomizePanel.cpp` 5,428,
`GemManager.cpp` 5,404, `Geo.cpp` 5,212, `BandDirector.cpp` 4,464 and
`SaveLoadManager.cpp` 4,096. TSV: rerun the tool (`--out`).

## 3. Native behaviour fix: `MakeBSPTree`

`src/system/math/Geo.cpp` compiled `MakeBSPTree` as `return false;` under `HX_NATIVE`,
because its body named STLport-only things (`stlpmtx_std::less<BSPFace>`, `_S_sort`).
`RndMesh` (`rndobj/Mesh.cpp:1245`) builds a BSP for every `kVolumeBSP` mesh and, on
false, frees the tree and sets the volume to `kVolumeEmpty`. Natively, every such mesh
lost its collision volume.

Only the sort is platform-specific. The STLport specialisation is a stable merge sort by
descending `area`; libc++'s `list::sort` is also stable, so with the same comparator the
order is identical, including equal-area ties. The native arm is now
`faces.sort([](a, b) { return a.area > b.area; })`, and the rest of the body is shared.
The X360 translation unit sees the same tokens as before.

## 4. Gates (rb3-render, `native/src/w16tf_phase.cpp`)

The phase runs after W16-SH's in the default mode `native_health.sh` uses; `--no-w16tf`
opts out. It covers the 15 newly executed rows:

| gate(s) | reference | gap rows (B, fuzzy) |
|---|---|---|
| geo-bsp-fixture | signed volume of each test mesh by the divergence theorem: cube 8, concave L prism 3 (validates the fixtures) | — |
| geo-bsp-build/classify/check/seg-* | analytic inside test on fixed + 4,000 seeded points per mesh; `CheckBSPTree` true for the bounding box, false for a box inside the solid; segment entry parameter and face plane | MakeBSPTree 1,580 (97.27), BSPFace::Update 612 (99.47) |
| geo-clip-copy/inplace/empty | shoelace area and half-plane membership | Clip 512 (97.61) |
| geo-tri-box | inside, far, face-plane separated, edge-axis-only separated, crossing | Intersect(Triangle, Box) 848 (97.16) |
| geo-seg-tri | hit parameter, back-face flag, outside, past the end, parallel | Intersect(Segment, Triangle) 432 (97.19) |
| geo-ray-box | slab entry/exit | Intersect(origin, dir, Box) 188 (93.94) |
| geo-sphere-grow | minimal enclosing sphere | Sphere::GrowToContain 372 (99.46) |
| geo-frustum-persp/ortho | the frustum's half-spaces written out independently | operator>(Sphere, Frustum) 372 (99.89) |
| geo-plane-xform | transformed points lie on the transformed plane; sides kept | (Multiply(Plane, Transform) was already executed) |
| rot-make-rot-quat | half-angle quaternion about v1 x v2, plus parallel and opposite | MakeRotQuat 248 (89.42) |
| interp-linear-dta/atan/invexp | closed forms | LinearInterpolator::Reset(DataArray) 220, ATanInterpolator::Reset 220, InvExpInterpolator::Eval 100 |
| key-interp-tangent | numerical derivative of the cubic Hermite curve | InterpTangent 280 (99.57) |
| utf8-to-ascii | Latin-1 kept, others substituted, truncation at len - 1 | UTF8toASCIIs 128 (96.25) |
| sfs-fill/missing/wrong-type/unterminated | literal expected strings from the placeholder grammar | SuperFormatString(const char*, const DataArray*, bool) 1,608 (94.33) |

**Prediction against measurement.** The tolerances and expected values were written
before the first run. All 31 passed on the first run, so two sabotage controls were run
(both reverted). Predictions were written before each run.
- **S1**, the pre-lane native stub restored: predicted that exactly `geo-bsp-build-cube`
  and `geo-bsp-build-L` fail. Measured: those 2, `RESULT: FAILED (2 gate failure(s))`.
- **S2**, four defects at once (Clip keeps the wrong side; the 9 edge-axis tests skipped;
  the right frustum plane's sign flipped; `{{` not unescaped): predicted that the three
  clip gates, `geo-tri-box` (edge-axis case only), `geo-frustum-persp` and `sfs-fill` fail.
  Measured: exactly those 6.

**A limit S2 exposed:** the BSP gates stayed green with `Clip` broken, although `Clip`
ran 33 times in the phase. These two meshes do not depend on `Clip`'s result in
`MakeBSPTree`'s split arm or in `CheckBSPTree`'s straddle arm. `Clip` is gated by the
`geo-clip-*` gates alone.

No gate exposed a behaviour difference in a sub-100 row. Every row above is codegen-only
on these inputs, which fits W16-PK's finding that executed rows already behave like
retail.

### 4.1 Rows deliberately not gated

- `Intersect(Vector3, Vector3, Triangle, float&)` (296 B, mpn 100). It reads the triangle
  at byte offsets `0x10/0x14/0x18` and `0x20/0x24/0x28`, i.e. `{frame.x.y, frame.x.z,
  frame.y.x}` and `{frame.y.z, frame.z.x, frame.z.y}`, not the edge vectors. Retail
  (`0x824F04F0`) does the same loads, so this is retail's behaviour and an analytic
  Moller-Trumbore reference would be wrong. Its only caller is `AmbientOcclusion`'s
  `kdTree<Triangle>::Intersect`.
- `Multiply(Vector3, Quat, Vector3)` (192 B, 85.73). Native compiles its own
  `HX_NATIVE` arm, so a gate would observe native-only code, not the X360 body.
- `stlpmtx_std::_S_sort<BSPFace>` (424 B). X360-only; native sorts with libc++ (§3).

## 5. Gates and X360

**X360.** Prediction: Δ0 on every key. The only match-build input touched is Geo.cpp,
and the edit moves an `#ifndef HX_NATIVE` boundary without changing the X360 tokens.
`tools/ab_measure.py --worktree ~/tmp/wt-w16tf --revert HEAD --jobs 8` (leg A with the
lane commit, leg B reverted; `name_check`, objdiff-cli `c1b7d95240a35cd6`):

```
leg A: matched=54896 masked=25210 honest=29686 code%=59.240520  (recompiles: 0, settled)
leg B: matched=54896 masked=25210 honest=29686 code%=59.240520  (recompiles: 17, split=0, patch_steps=6, settle iterations: 2)
Δmatched=+0  Δmasked_equal=+0  Δhonest=+0  Δcode%=+0.000000pp  Δcode_bytes=+0
Δfuzzy=+0.000000pp   (legA 64.289185 -> legB 64.289185)
units at 100% [mpn ruler]: legA 620 -> legB 620
```

The prediction held. Leg A's 54,896 is W16-TA's 54,894 plus W16-TB's +2.

**Census on this branch's native build:** passes the new guard, 18 targets, 7,952 rows,
994 unique authored stub symbols, the same set as on main's build. Without the two new
files it is 965, which is W16-TA's `stubs.json` set exactly (0 symbols differ either
way). The two files add 36 definitions, 7 of which another stub file also defines. So
965 + 36 − 7 = 994. W16-TA's "956 + 36 = 992" counted live symbols and did not subtract
the overlap. `MakeBSPTree` is not in the stub set, as expected: its stub was an
in-TU `#ifdef`, which the census cannot see.

**`tools/native_health.sh`** on this worktree. rb3-render ran 88 gates: 56 before the
W16-TF phase, the phase's 31, and `all-cells-rendered`. 94 (W16-SH) + 31 = 125.

```
NATIVE_HEALTH_RESULT verdict=PASS link=PASS link_verified=18 link_expected=18 link_skipped=0 runtime=PASS runtime_ran=18 runtime_total=18 gates_pass=125 gates_fail=0 unrunnable=none selftest=SKIPPED scatter_unlinked=16 scatter_dirb=0 scatter_multihost=15 rc=0 handpose_controls=- handpose_baseline_fail=- runtime_crashed=0 runtime_failed=none
```

**`tools/native_build_gate.sh`**, run after the last code change:

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

## 6. Not done

- The 197 joinable-but-unentered in-scope rows outside these files are untouched. The
  biggest (`VocalTrack::UpdateScrolling` 8,948, `BandPatchMesh`, `GemManager::SetupGems`
  3,404, `CustomizePanel::Handle` 5,036) need a populated track or panel to drive, not a
  pure-function call.
- VIA-DC3 rows were ranked but not gated.
- I did not change `native_stub_census.py` to see `#ifdef HX_NATIVE` stub arms (§1).
- Correction to this lane's first commit message: it says 32 gates; the phase has 31.
  The 32nd `[PASS]` line after the phase header is rb3-render's own `all-cells-rendered`.

## Reproduce

```
CMAKE_BUILD_PARALLEL_LEVEL=12 python3 tools/native_runtime_rank.py <worktree> --out rank.tsv --json rank.json
python3 tools/native_stub_census.py native/build stubs.json --authored
```
