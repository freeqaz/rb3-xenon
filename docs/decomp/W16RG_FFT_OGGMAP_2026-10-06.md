# W16-RG — FFT AltiVec bodies and the TU5 OggMap mogg validator (2026-10-06)

Branch `w16-rg`, off main `f883478b5`. Scope: the 0% rows of
`src/system/synth_xbox/FFT.cpp` and the anonymous 0% rows of the
`system/synth/OggMap` unit (`fn_82BB1FD8`, `fn_82BB1DC8`, `fn_82BB1980`).

All row figures are `fuzzy_match_percent` from `build/45410914/report.json`
(ruler `name_check`) after a full `./tools/ninja-locked`, unless marked
`run_objdiff` (same graded ruler).

## Results

| row | size | before | after |
|---|---:|---:|---:|
| `?fft_altivec@@YAHPAM0KJ0@Z` | 3,044 | 0 | 44.02 |
| `?fft_recursive@@YAHPAMKJ0@Z` | 2,128 | 0 | 45.43 |
| `?fft_real_forward_altivec@@YAHPAMJ0@Z` | 964 | 0 | 76.98 |
| `?SetKey@OggMap@@SAXPBD@Z` (was `fn_82BB1600`, in HAQManager) | 224 | 0 | 99.82 (mpn 100) |
| `?SetupCypher@@YAXH@Z` (was `fn_82BB1788`, in HAQManager) | 460 | 0 | **100** |
| `?ReadData@OggMap@@UAAHH@Z` (was `fn_82BB1980`) | 516 | 0 | **100** |
| `?ReadMap@OggMap@@AAA_NAAVBinStream@@@Z` (was `fn_82BB1DC8`) | 516 | 0 | **100** |
| `?OpenMogg@OggMap@@AAA_NPBD@Z` (was `fn_82BB1FD8`) | 1,004 | 0 | 97.51 |
| `?Validate@OggMap@@QAA_NPBDAA_N@Z` (was `fn_82BB2418`) | 180 | 0 | **100** |

OggMap unit: 17 / 18 functions at 100, 2,500 / 3,728 B matched (main's report:
8 / 14, 668 / 2,964 B; the denominators differ because two blocks moved in from
HAQManager, see below).

Whole-binary A/B, whole branch: **+8 functions / +1,792 B / +0.017491 pp
code / +0.060471 pp fuzzy** (see "Measurement").

## FFT (`src/system/synth_xbox/FFT.cpp`)

The three rows were declared and called but had no definition, so they read 0%.
The bodies are DC3's listing reconstructions, inserted verbatim, plus
`__vsel`/`__vaddfp`/`__vsubfp` declarations (a declaration is all MSVC needs to
emit the opcode). Retail sizes agree (2,128 / 3,044 / 964 B).

The resulting scores equal DC3's own scores for the same bodies against DC3's
retail: `fft_altivec` 44.02 here vs 42.47 in DC3, `fft_recursive` 45.43 vs
45.42, `fft_real_forward_altivec` 76.98 vs 76.98. These are inherited
residuals, and DC3's lanes have already recorded their negative results in the
source comments. Not ground further here. One probe: swapping the `perm_d` /
`perm_e` declarations in `fft_real_forward_altivec` (retail keeps `perm_d` at
0xa0 and `perm_e` at 0xb0, and we have them the other way round) changed only
the store order, not the slot assignment (76.975 → 76.996). It was reverted to
keep the body identical to DC3.

Side observation, not acted on: `fft_matrix_forward_columnwise` scores 79.39 here
but 84.90 in DC3 with a byte-identical body (the diff is comments only), so RB3's
retail differs from DC3's there. It was not a 0% row and is outside this brief.

**Native:** `synth_xbox/*` is excluded from the native build as platform-only
(`native/CMakeLists.txt`, "12 platform-only guests"), so the VMX intrinsics need
no portable fallback.

## OggMap (`src/system/synth/OggMap.{h,cpp}`)

### Identification

* **Class shape.** Retail RTTI (COL at `0x821F0C2C`) gives `OggMap` exactly one
  base, `OggValidatorFileSource`. Slot 0 of that base is the pure
  `ReadData(int)` override, which is `fn_82BB1980`. `OggMap`'s virtual dtor takes
  slot 1. The base declares **no** destructor: a base virtual dtor added an
  unwind state to `OggMap`'s ctor/dtor that retail does not have (~OggMap fell
  to 88% with one).
* **What TU5 added.** A mogg validator, with no oracle in rb3-Wii (pre-TU5) or
  DC3. The rows were written from the retail listing, using `VorbisReader`'s
  member-based copy of the same mogg logic as the template:
  * `SetKey`: hex key into file-static AES state. It is called from outside the
    unit (`0x8272C800`) with the RB1 mogg key.
  * `SetupCypher`: the v12+ key derivation (`KeyChain::getKey`,
    `ByteGrinder::GrindArray`, key-mask XOR, `ctr_start`, the two `{ha %d n}`
    hashes).
  * `ReadData`: read, AES-CTR decrypt, then rewrite `HMXA` pages to `OggS` with
    words 12/20 XORed by the hashes, into `mStream`.
  * `ReadMap`: a validating seek-table read.
  * `OpenMogg`: the header parse plus cipher setup.
  * `Validate`: drives an `OggValidator` over `mStream`.
* **Members.** `mGran` 0x4, `MemStream mStream` 0x8, `FileStream *mFile` 0x28,
  `bool mEncrypted` 0x2c, `OggValidator *mValidator` 0x30, and the lookup vector
  at 0x34. `sizeof` stays 0x40, so `VorbisReader`'s embedded `mOggMap` and its
  following offsets are unchanged.

### Re-home of two HAQManager blocks (splits)

HAQManager's `.text` blocks `[0x82BB15F8,0x82BB1708)` and
`[0x82BB177C,0x82BB197C)` held `SetKey` and `SetupCypher`. Both address OggMap's
file statics (base `0x82E4BE10`) and the key-size static `0x82CAA4D4`, and they
sit between OggMap's own blocks. OggMap.cpp is now one contiguous range
`0x82BB15F8–0x82BB24D0`. `.pdata` was re-derived by the split, and the
split-guard's one expected "rewrote its own input" failure passed on rebuild.
HAQManager keeps `[0x82BB153C,0x82BB15E8)`, whose two helpers are not OggMap
code. Six map names were added for the anonymous rows (`scripts/target_symbol_map.json`).

### File-static layout: two rules, and one caveat

Retail's statics, as offsets from `gKey` (`0x82E4BE10`): `gKey` 0x0,
`gNonce` 0x20, `gCtr` 0x30, `gKeyMask` 0x340, `gMagicHashA` 0x350, a byte at
0x354, `gDecrypt` 0x355, `gMagicHashB` 0x358, a word at 0x35c, `gMagicA` 0x360,
`gMagicB` 0x364, `gKeyIndex` 0x368.

1. **MSVC lays out a TU's file statics in reverse declaration order** (natural
   alignment), so they are declared highest-address first.
2. **MSVC drops a static that no emitted code touches.** A `(void)x` reference
   does not keep it. Retail reserves 0x354 (byte) and 0x35c (word), but no
   surviving function in the unit touches them: whatever did was discarded by
   the linker, as `GetSongLengthSamples` is (absent from retail's range).
   Without them `gDecrypt` packs to 0x354 and `gMagicA/B/gKeyIndex` shift by 4.

⚠ **Caveat:** the two slots are kept by `OggMapReserveUnusedStatics()`, a
never-called function that is **not reconstructed retail code**; the source says
so at the definition. It emits a body in our object that retail does not have,
and that body pairs with nothing. Swapping the two bool declarations was inert
until both statics were kept.

### Row notes

* **ReadData → 100.** Retail's store order for the `OggS` rewrite is
  `out[2]`, `out[0]`, `out[1]`, `out[3]`, and the two branches end in separate
  `mStream.Write` calls (decrypted `out` vs raw `in`).
* **ReadMap → 100.** The monotonicity loop compares against `int prevPos/prevSamp`
  seeded with -1, using an unsigned index.
* **SetupCypher 98.04 → 100.** Prediction: hoisting `(int)masterKey ^ iEval` into
  a local before `i6 += 'A'` would move the `xor` ahead of the format-string
  `addi`. Measured: 115/115 instructions equal.
* **Validate → 100.**
* **OpenMogg 85.18 → 97.51.**
  * Two separate `version < 10` / `version > 15` tests (not `||`) let the merged
    failure block sit inline after the first test, as retail has it (87.2 → 97.6
    `run_objdiff`, together with the next bullet).
  * One `long long` temp is reused for all three 64-bit header reads, so the
    frame drops to retail's 0xb0.
  * Residual: retail shares the s64 slot with the `new FileStream` EH temp at
    0x58, places `hdrSize` at 0x60, and stores each `gMagicA/gMagicB/gKeyIndex`
    *before* reloading `mFile`. Tried and inert: the temp declared at function
    top, before `hdrSize`, or in the inner block. Tried and worse: reading via an
    inline helper (97.5, frame +0x10, one slot per inlined copy). DC3's
    `VorbisReader` (same mogg header read) records the same unsolved stack-temp
    shape at 99.987.
* **SetKey 99.82 (mpn 100).** Pure register allocation: retail hoists the `"%x"`
  pointer into r28 and the `gKey` base into r27, and we do the reverse. Two
  instructions differ. Retail's string at `0x8217F0CC` is a plain `"%x"`.
  * Inert: `byte` hoisted out of the loop; `unsigned int byte`.
  * Worse: `key.substr(...)` passed straight into `sscanf` (92.8).
  * The permuter is off by directive, so this stays recorded rather than swept.

## Native build

Neither TU is in the native build: `synth_xbox` is platform-only, and
`OggMap.cpp` is not listed. `VorbisReader.h` includes `OggMap.h`, so the header
change does reach native compiles. `tools/native_build_gate.sh` in the lane worktree:

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

## Measurement

`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16rg-ab --patch <git diff f883478b5..HEAD>`
(one patch for the whole branch, applied to a fresh worktree at main `908b3feb0`,
which is `f883478b5` plus a native-only pin; ruler `name_check`; both legs at a
`symbols.txt` split fixed point; leg B renamer patched 1,863 files):

```
leg A: matched=54702 masked=25213 honest=29489 code%=58.777473
leg B: matched=54710 masked=25216 honest=29494 code%=58.794964
Δmatched=+8  Δmasked_equal=+3  Δhonest=+5  Δcode%=+0.017491pp  Δcode_bytes=+1792
Δfuzzy=+0.060471pp
unit improvements:  +9  default/system/synth/OggMap  (8->17)
unit REGRESSIONS:   -1  default/HAQManager  (1->0)
```

The HAQManager −1 is a reattribution, not a regression. Its one matched row
before the change was a 40 B block inside the re-homed range, which now counts
in OggMap (OggMap's +9 includes it). The three FFT rows move fuzzy but not
`matched_code`, because no FFT row crosses 100.
