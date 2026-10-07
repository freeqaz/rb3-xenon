# W16-UB: X360 literal sizes and offsets in native-compiled source (2026-10-07)

W16-TY found `KerningTable` clearing a literal 0x80 bytes of a 32-pointer table:
0x80 bytes on X360, 0x100 on a 64-bit host, so half its bucket heads stayed
uninitialised natively (`W16TY_VIA_DC3_UNENTERED_ROWS_2026-10-07.md` §5.3). It named
a likely sibling at `rndobj/VelocityBuffer.cpp:105` (memset 0xa4). This lane audited
every source the native targets compile for literals that assume the 32-bit Xbox
layout and fixed each real one in an `HX_NATIVE` arm.

## Summary

- **Population.** 2,606 `src/` files reach the native build (ninja's dep log over every
  native object, plus the compile list); 1,960 of them contribute preprocessed lines.
  Every hit below was filtered to lines that survive the native preprocessor.
- **Instruments.** Three, all driven by the native compile commands:
  1. a clang `-E` pass per TU recording which `(file, line)` pairs are live natively;
  2. `clang-query` AST matchers per TU for literal-length `memset/memcpy/memmove/memcmp`,
     literal-width `Read*/Write*` stream calls, and `(cast)ptr ± literal`, reporting the
     compiler's types for each argument;
  3. a `-fsyntax-only` pass re-enabling the warnings the native flags silence
     (`-Wpointer-to-int-cast`, `-Wint-to-pointer-cast`, `-Wvoid-pointer-to-int-cast`,
     `-Wnontrivial-memcall`, `-Warray-bounds`, `-Winvalid-offsetof`, …).
- **Findings.**
  - §2: 16 defects fixed in `HX_NATIVE` arms, 24 edited sites in all (MemHeap 6, EditSetlistPanel 3, MemTrack and NetStream 2 each).
    Two of them are byte order, not width, and two are wrong on both builds.
  - §3: 7 sites already handled by earlier native arms, verified.
  - §4: every other hit has a recorded reason, including each of the coordinator's DC3
    candidates.
- **Behaviour.** 7 native gates in `rb3-render` (§5). A sabotage build putting the X360
  arms back natively fails the six behaviour gates (§6).
- **X360.** `tools/ab_measure.py`: Δ0 on every key (§7).

## 1. Method and counts

| instrument | scope | raw | natively live | after type filter |
|---|---|---:|---:|---:|
| regex: `mem*` calls | 1,960 files | 435 | 340 | 70 with a literal length |
| regex: `(cast)x + literal` | 1,960 files | 80 | 55 | — |
| `clang-query` (3 matchers) | 767 TUs | 331 | 241 | 186 not byte-typed |
| `-fsyntax-only` warnings | 767 TUs | 1,819 | (compiler-live) | 118 in the cast/memcall/bounds families |

- The regex pass came first and was **incomplete**: it missed `*(RndCam**)((u8*)&TheRnd + 0xA4)`
  in `PostProc_NG.cpp`. The AST matcher found the same class with types attached, and
  the active-line filter correctly excluded that site (it is in the `#else` of a native
  stub).
- An early run of the regex pass printed **0 hits**. `xargs command grep` cannot run a
  shell builtin, so every file failed silently. The rerun used `/usr/bin/grep`.
- The first preprocessor pass **failed on all 4,054 TUs** (it kept the `-MF` argument).
  After the fix: 1,797 unique TUs (by file and `-D`/`-I` set), 0 failures, 1,960 files.
  That matches the 1,960 files in the dep-derived list.
- 76 TUs gave `clang-query` no matches. Two were rerun with stderr visible:
  `0 matches.` and no `error:`. All 767 compile in the native build with these commands.
- `-Wshorten-64-to-32` (1,161 hits: `size_t`/`ptrdiff_t` to `int`) was **not** examined
  site by site. It is implicit narrowing of sizes and differences, not a literal layout
  assumption.

## 2. Fixed (HX_NATIVE arms)

`X360 tokens` means the `#else` arm keeps the original source text. MemHeap's literal
`3` becomes `FREE_BLOCK_WORDS`, which expands to `3` on X360.

| # | site | defect on a 64-bit host | native reach | check |
|---|---|---|---|---|
| 1 | `rndobj/VelocityBuffer.cpp` ctor | `memset(&mViewProjXfm, 0, 0xa4)`: 0xa4 = 0x8..0xac ends at the last byte of the 4-byte `mCam`. Natively `mCam` is 8 bytes, so its upper half stays uninitialised. Now clears through `&mCam + 1` | ctor linked in `rb3-milo`, `rb3-render` | `ub-velbuf-ctor` |
| 2 | `utl/MemHeap.cpp` (5 sites) | `FreeBlock {u32, u32, FreeBlock*}` is 3 words on X360 and 4 natively. The minimum block (`GetSizeWords`) is 3 words, so a freed minimum block spills `mNextBlock` into the next block's header. The debug fills start at `+3`, inside `mNextBlock` | latent: no native driver calls `SystemInit` → `MemInit`, so `gNumHeaps == 0` and `MemAlloc` uses `malloc` | `ub-memheap-minblock` |
| 3 | `utl/MemHeap.cpp` `Print` | walks the free chain as `(unsigned int *)curFreeBlock[2]`, a 32-bit read of `mNextBlock` | as #2 | `ub-memheap-minblock` (Print's FREE count) |
| 4 | `utl/MemTrack.cpp` `MemTrackStack` + `MemTrackInit` tail | `char ptrs[260]` = 65 × 4 holds 32 eight-byte pointers, yet the native-only Begin/End helpers index slots 0..64. The init loop stored through `(int)CharArrayArray + i`, truncating a PIE `.bss` address, with stride 4 | latent: `MemTrackInit` runs only from `MemInit` (above) | `ub-memtrack-stacks` (forked child) |
| 5 | `band3/meta_band/EditSetlistPanel.cpp` | `XStringVerify` buffers laid out by hand: two `STRING_DATA {WORD; WCHAR*}` 6 bytes apart in `new char[12]` (pointers at +2, +8), a 14-byte response read at +2, a 28-byte `XOVERLAPPED`. Natively the second record overwrites the first pointer's top bytes and runs 4 bytes past the block; Cleanup then `delete[]`s the damaged pointer. Now typed structs, sized by `sizeof` | latent: `--gc-sections` drops `VerifyStrings` from every native binary (checked with `nm`) | none: not linked |
| 6 | `band3/game/Game.cpp:687` | `*(bool *)((char *)midiParserMgr + 0x69) = b1`. The compiler's X360 layout puts `mPlaybackEnabled` at 0x69. Natively 0x69 is a different member, so the store corrupts it. Now `SetPlaybackEnabled` (new inline setter, `HX_NATIVE` only, in `MidiParserMgr.h`) | `Game` in the song/score drivers | `ub-layout` (premise only) |
| 7 | `band3/game/Game.cpp:1836` | `*(VocalGuidePitch **)(*(char **)((char *)this + 0x48) + 0x14)`. The compiler's X360 layout names `this+0x48` as `mUnkTU5GuidePitch` (the code comment calling it unmodelled is stale), and `+0x14` is its `mGuidePitch`. Natively both offsets differ: `mGuidePitch` aligns to 0x18 | audition mode only (`mUnkTU5_movieSync`) | `ub-layout` (premise only) |
| 8 | `band3/game/TrainerPanel.cpp:194` | `*(DataEventList **)((char *)parser + 0x18)` for `MidiParser::mEvents`; now the public `Events()` | trainer panels | `ub-layout` (premise only) |
| 9 | `band3/meta_band/NextSongPanel.cpp:86` | `*(float *)((char *)label + 0x1BC) = 1.0f`. 0x1BC is `UILabel::mAlpha` in the compiler's X360 layout; now `SetAlpha(1.0f)`, the same plain store | panel enter | `ub-layout` (premise only) |
| 10 | `rndobj/Bitmap.cpp` `LoadDIB` | bottom-up rows read into `(void *)((int)pixels + i * rowBytes)`, truncating the heap pointer | any BMP load | `ub-loadbmp-rows` |
| 11 | `os/NetStream.cpp` Read/Write | `v = (void *)((uint)v + bytes)` | no native socket backend | none |
| 12 | `synth/OggMap.cpp` `SetupCypher` | passes `masterKey`'s address through a DTA int to the letter DataFuncs (`Synth.cpp` `returnMasterKey`). Natively the address does not fit, and `Synth::InitSecurity` does not register those functions, so `masterKey` stayed uninitialised. Now `KeyChain::getMasher`, which is `VorbisReader::setupCypher`'s native arm | latent: `OggMap::Validate` → `OpenMogg` has no caller in the tree | none |
| 13 | `obj/DataFile.cpp` `LoadDtz` | **byte order.** The size trailer is little-endian; the X360 arm stores its bytes most-significant first, which is right only on a big-endian host | `DataNetLoader` (`.dtz` over the network) | `ub-loaddtz-trailer` |
| 14 | `os/CDReader.cpp` `CDReadExternal` | **byte order.** `((LONG *)&u)[1]` / `*(PLONG)&l` are the low and high words only on a big-endian host; on x86 the seek went to the high word | `ArkFile::GetFileHandle`, called only by `Movie.cpp`'s Bink path | `ub-cdreadexternal-seek` |
| 15 | `rndobj/Flare.cpp:213/216` | **not LP64; wrong on both builds.** Copies a 0x40-byte `Transform` into a 0x30-byte `Hmx::Matrix3` local and back, a 16-byte stack overrun. Natively it uses a `Transform` local (rotation rebuilt, translation carried through) | flares with an xfm texgen material | `ub-layout` (premise: `sizeof(Hmx::Matrix3)` = 0x30) |
| 16 | `utl/GlitchFinder.cpp:361` | **not LP64; wrong on both builds.** `buf[0x400] = '\0'` into `char buf[1024]`; natively `buf[0x401]` | glitch finder spew | none |

Rows 2 and 3 are one class in one file (6 edited sites). Rows 13 and 14 are the
byte-order cousins. Rows 15 and 16 are overruns on both builds; they surfaced in the
same census.

## 3. Already handled by an earlier native arm (verified, no change)

| site | literal | why it is right natively |
|---|---|---|
| `rndobj/Font.cpp` `KerningTable` ×3 | 0x80 | W16-TY `726d3bea5` (`sizeof(mTable)`) |
| `utl/AllocInfo.cpp:56` | `gMemTracker + 0x10` | the native arm's comment claims `mTimeSlice` sits at 0x10 on LP64; `ub-layout` checks it with `offsetof` |
| `rndobj/PostProc_NG.cpp:371,380-384` | `&TheRnd + 0xA4`, `this + 0x220`, `vtable + 0x40` | native `DoVelocity` is a stub; the `-E` filter shows these lines are not compiled natively |
| `utl/PoolAlloc.cpp` `FixedSizeAlloc` | free list in `int` words (`*cur = (int)next`) | native `PoolAlloc`/`PoolFree` call `malloc`/`free`, with a comment naming this defect |
| `bandobj/BandPatchMesh.cpp:53` | 0x3a / 0x2f / 0x40 | native arm derives them with `offsetof` (the `-Winvalid-offsetof` hits are that arm) |
| `synth/VorbisReader.cpp` `setupCypher` | `(int)masterKey ^ iEval` | native arm calls `KeyChain::getMasher` (#12 copies it) |
| `utl/ChunkStream.cpp` `DecompressMemHelper` | big-endian size word | native arm drops the `EndianSwap` |

## 4. Recorded reasons (not changed)

### 4.1 Coordinator's DC3 candidates

| candidate | our tree | verdict |
|---|---|---|
| `VelocityBuffer.cpp` memset 0xa4 | same literal | **real**, fixed (#1) |
| `ThreadCall_Win.cpp` `gData` 0xF0 | same literal: `memset(gData, 0, 0xF0)` over `ThreadCallData gData[12]`, which is 12 × 0x14 on X360 | **not in the native build**. Only `os/ThreadCall.h` appears in the native dep log; the `.cpp` is never compiled. If ported it would be real: natively `ThreadCallData` is 40 bytes (4 + pad + three 8-byte pointers + `int` + pad), so 0xF0 clears exactly half the array |
| `MetagameRank.cpp` ~300 (0x40) | **no such file** in `src/` | n/a |
| `HDCache.cpp` ~238 (0x80) | `char zeroPad[0x80]; memset(zeroPad, 0, 0x80)` | clean: byte array, same size on both |
| `ByteGrinder.cpp` ~99/126 (0x20) | `static bool usedUp[0x20]; memset(usedUp, 0, 0x20)` | clean: `bool` is 1 byte on both |
| `AllocInfo.cpp` ~204 | `MemAlloc(0x220008)` + trie at `+0x220000` | clean: the trie is a byte pool of 0x11-byte nodes holding `u32` indices, not pointers (`utl/trie.cpp` reads `u32`/`u8` at fixed node offsets). The `gMemTracker + 0x10` read is §3 |

### 4.2 Literal lengths and offsets that are correct natively

By compiler type (`clang-query`), these literals cover types whose size does not depend
on pointer width:
- byte, `char`, `bool` and `u8` arrays: string buffers, `ChunkID`, SHA-1/AES/Ogg
  buffers, `FixedSizeSaveable`, `HDCache`, `OSCMessenger`;
- `float`/`int` arrays: `Calibration` `unka4`/`unkb8`, `CharCollide` radii,
  `VibratoDetector`, `MemPoint`'s `int[16]` + `int` (0x44);
- `Transform`/`Matrix3`/`Matrix4` copies of 0x30/0x40, where `Vector3` is 16 bytes on
  both;
- `ReadEndian(&x, 4)` where `x` is `int`/`unsigned int`/enum;
- `RndMesh::Vert` 0x60, which W16-TY's `ty-fixture` checks;
- `ChunkInfo` 0x810, all `int`s;
- `XTEABlock` +8;
- `Triangle` + 0x10/0x20 (floats, no vptr);
- `BandHeadShaper`'s `Delta` (a packed byte stream);
- `MemHandle::Lock` + 0x10 (`MemHandleAlloc` is `{ptr, int}`, 0x10 natively);
- `UIStats`' 4-byte `unsigned int` log entries;
- `NetworkSocket`'s `XNDNS` + 8 (`int` fields);
- `Timer.h` byte-reverse helpers;
- the `nontrivial-memcall` sites, all `sizeof(*this)`/`sizeof(T)`.

### 4.3 Real but not fixed

| site | defect | why left |
|---|---|---|
| `utl/BinkIntegration.cpp` (15 sites) | BINKIO callbacks read and write the vendor struct at X360 offsets (0x40, 0x44, 0x70, 0x80, 0x84, 0xb4, memset 0x120) | BINKIO is owned by the RAD Bink SDK, which the native build lacks; `BinkFileOpen` and its siblings have no caller anywhere in `src/` or `native/` (linked, never entered). A native layout would have to come from a native Bink SDK |
| `synth/Synth.cpp:92` `returnMasterKey` | `memcpy((void *)(i3 ^ i2), masher, 0x40)`: script-supplied int as an address | native `InitSecurity` does not register it; its one native consumer (#12) no longer calls it |
| `band3/game/TrainerGemTab.cpp:169` | writes `mTails[5]` into `RndMesh *mTails[5]` (`-Warray-bounds`, both builds) | lands in `int unk90` on X360 and in `unk90` plus its 4 alignment bytes natively: the same logical slot, so benign on both. The header probably wants `mTails[6]`; that is a layout question for a matching lane |
| ~25 `if ((int)ptr)` null tests (`TrackPanelDir` ×8, `TrainerPanel` ×3, `StreakMeter`, `ScreenMask`, `HDCache`, `Msg`, `Object`, …) | truncating null test | wrong only for a non-null pointer whose low 32 bits are all zero. Retail spelling; left |
| `(int)p - (int)q` / `(int)p >= (int)q` / `==` (`SongParser` ×2, `Rnd.cpp:472`, `BandWardrobe:111`, `AmbientOcclusion:533`) | truncated pointer arithmetic | differences within one object are exact mod 2³²; a comparison could only flip across a 4 GB boundary inside one array |
| `Rnd::CompressTexture` returns `(int)desc` | pointer as ID | callers only store and compare the ID (`BandCharacter::mCompressedTextureIDs`) |
| `PerfectOverdriveTracker.cpp:289/293` | int passed as `const char *` vararg for `%d` | x86-64 passes both in a GPR and `%d` reads the low half |
| printing (`MessageTimer`, `OutfitConfig:332`, `Character.cpp:277` group name, `Graph.cpp:165`) | `%x` of a truncated pointer | cosmetic |
| `MemTracker::HashKey` `uint(ptr) / 8 % size`, `tomcrypt ctr.c` alignment test | truncation used as hash / low bits | correct by construction |
| `CharClip.cpp:779/783` | int offset carried in a `void *` and back | round-trips |
| `CharClip.cpp:958` | `(char *)offset` for `%s` in a `MILO_FAIL` | fail-path message, wrong on both builds |
| `NetworkSocket.cpp:56` | `XNetDnsLookup((int)name.c_str(), …)` | no native XNet; `NetworkSocket_Stub.cpp` serves the native socket API |
| `network/Core/NetZ.h:21`, `synth_xbox/Voice.h:43`, `rnddx9/Rnd.h:113` | int to pointer | Quazal (out of scope per the standing directive), XAudio2 and D3D9 platform code |

## 5. Native gates (`native/src/w16ub_phase.cpp`, `rb3-render`)

The phase runs after W16-TY's (`--no-w16ub` skips it). Each reference is independent
of the code under test:
- the members the literal was meant to cover, named so the compiler sizes them;
- the heap's own invariants: every live block keeps its header, and freeing everything
  restores the single initial free block;
- a payload built and read back through the engine's own serialiser and compressor.

Gates that could crash on a broken body run in a forked child (60 s alarm), so a
fault fails that gate rather than the run. Run on 2026-10-07 at `23c0cf256`, assets
`~/code/milohax/rb3/orig-assets/xbox-zip`, rc=0, `RESULT: ALL GATES PASSED (0 gate failure(s))`:

```
[PASS] ub-layout — MidiParserMgr::mPlaybackEnabled 0xe9 (X360 0x69), MidiParser::mEvents 0x40 (0x18), UILabel::mAlpha 0x340 (0x1BC), MemTracker::mTimeSlice 0x10 (AllocInfo's native arm reads 0x10), sizeof(Hmx::Matrix3) 0x30 (Flare copied 0x40)
[PASS] ub-velbuf-ctor — mViewProjXfm..mCam is 168 bytes here (0xa4 on X360); 0 left non-zero over 0xA5 fill, mCam null
[PASS] ub-memheap-minblock — sizeof(FreeBlock)=16, min block 4 words; after freeing every other one of 256: 0 live headers changed, 0 live words changed, free chain in bounds (129 nodes), Print listed 129 free blocks; after freeing all: one block, 16384 of 16384 words free
[PASS] ub-loaddtz-trailer — payload 128 bytes (low byte 0x80), 62 compressed; LoadDtz returned an array, re-serialised 128 bytes, identical
[PASS] ub-loadbmp-rows — 3x4 32 bpp bottom-up BMP loaded as 3x4, 0 of 12 pixels misplaced
[PASS] ub-cdreadexternal-seek — ark 0 handle positioned at 0x12345, expected 0x12345
[PASS] ub-memtrack-stacks — 64 nested file names pushed and popped, 0 pops restored the wrong name
```

- `ub-layout` is a premise check, not a behaviour check. It shows that each X360
  offset the old code used names a different member natively, so the old store/read
  hit the wrong field.
- The `ub-loaddtz-trailer` payload is padded until the serialised length's low byte is
  ≥ 0x80. That is the byte whose sign the big-endian assembly gets wrong.
- The BMP fixture's pixels encode their own coordinates, so a row swap or a short
  read shows up as misplaced pixels.

## 6. Sabotage: the X360 arms compiled natively

One build of `rb3-render` with these seven `#ifdef HX_NATIVE` lines changed to
`#ifdef W16UB_SABOTAGE_NEVER`:
- VelocityBuffer;
- MemHeap's `FREE_BLOCK_WORDS` and `Print`;
- DataFile `LoadDtz`;
- Bitmap `LoadDIB`;
- CDReader;
- the MemTrack struct.

It also restored the original `(int)CharArrayArray + i` init loop. Predictions were
written before the run; the source was reverted afterwards with `git checkout` in the
worktree.

| gate | predicted | measured |
|---|---|---|
| ub-layout | PASS (not sabotageable: premise only) | PASS |
| ub-velbuf-ctor | FAIL, 4 bytes non-zero, mCam not null | **FAIL**: `4 left non-zero over 0xA5 fill, mCam NOT null` |
| ub-memheap-minblock | FAIL (headers changed, or crash) | **FAIL**: child SIGSEGV |
| ub-loaddtz-trailer | FAIL (assert/abort) | **FAIL**: `decompSize > 0` and `pDecompBuf` asserts fired (non-fatal natively), inflate error −2, `LoadDtz returned null` |
| ub-loadbmp-rows | FAIL, SIGSEGV | **FAIL**: child SIGSEGV |
| ub-cdreadexternal-seek | FAIL, position 0 | **FAIL**: `positioned at 0x0, expected 0x12345` |
| ub-memtrack-stacks | FAIL, SIGSEGV at init | **FAIL**: child SIGSEGV |

`RESULT: FAILED (6 gate failure(s))`, rc=1. Every behaviour gate can fail. The one
prediction that was imprecise was LoadDtz: the decoded size went negative, and the
native `MILO_ASSERT` path logs and continues instead of aborting. The verdict was the
same.

## 7. X360 A/B

Prediction: Δ0 on every key. Every change sits in an `HX_NATIVE` arm, the X360 arm
keeps its original tokens, and MemHeap's `FREE_BLOCK_WORDS` expands to `3`.

Command:
`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16ub --revert 862e971a1 --label w16ub-revert-src`
(the source commit; the native-only commit `23c0cf256` touches nothing the X360 build
reads). Leg A has the fixes and leg B has them reverted, so the sign is inverted, which
does not matter for Δ0.

```
leg A: matched=54946 masked=25223 honest=29723 code%=59.314053  (recompiles: 0, settled)
leg B: matched=54946 masked=25223 honest=29723 code%=59.314053  (recompiles: 53, split=0, patch_steps=6, settle iterations: 2)
Δmatched=+0  Δmasked_equal=+0  Δhonest=+0  Δcode%=+0.000000pp  Δcode_bytes=+0
Δfuzzy=+0.000000pp   (legA 64.315216 -> legB 64.315216)
units at 100% [mpn ruler]: legA 611 -> legB 611  (Δ+0; 0 reached 100, 0 fell off; pairable units 1737->1737)
units at 100% [all-rows-fuzzy ruler]: legA 532 -> legB 532  (Δ+0; 0 reached 100, 0 fell off; pairable units 1737->1737)
[control none] Δmatched_code=+0 B Δcode%=+0.000000 (default ruler +0 B)
```

The ruler is `name_check` (from `objdiff.json`) and objdiff-cli was stable across legs
(`sha256:c1b7d95240a35cd6`). Leg B recompiled 53 TUs, mostly through `MidiParserMgr.h`,
so the patch was applied and compiled, not absent-vs-absent. The tool restored the
tree and verified it afterwards.

## 8. What was not done

- `-Wshorten-64-to-32` (1,161 sites) was counted, not triaged (§1).
- Byte order was not swept as a class. #13 and #14 surfaced through the subscript and
  stream censuses; other big-endian assumptions can remain.
- Rows 5–9, 11, 12 and 16 have no behaviour gate. 5, 11 and 12 are not linked or not
  reached natively. 6–9 need a live `Game`, `MidiParserMgr` or panel. `ub-layout` checks
  only their premise: the X360 offset names a different member natively.
- No `ASan` run. The stack overruns in #15/#16 were found by `-Warray-bounds` and the
  memcall types, not by a sanitizer.
