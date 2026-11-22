# W16-UE: byte order in native-compiled source (2026-10-07, finished 2026-11-22)

> **STATUS (2026-11-22, branch `byteorder`):** the 7 findings of the interrupted
> 2026-10-07 pass were re-verified on main `af50f101a`; all 7 still held and all 7
> are fixed, plus 2 new ones from the finished census (#8, #9) and one site the WIP
> pass missed inside #3. Fixes are in commit `b99110d70`. The census covered the
> typed families listed under "Census coverage"; what it did not cover is listed
> there too. The behavioural gates the WIP plan called for (`w16ue_phase.cpp`) were
> **not** written; see "Not done".

W16-UB (`W16UB_LP64_LITERAL_AUDIT_2026-10-07.md` §2 #13/#14, §9) fixed two byte-order
bugs incidentally (DataFile `LoadDtz`'s size trailer, CDReader `CDReadExternal`'s
seek words) and did not sweep the class. Native runs little-endian; retail and the
shipped data are big-endian. This lane sweeps the sources the native targets
compile for multi-byte values read from or written to disc, archive, stream,
network or save data without the endian-aware path (`BinStream::ReadEndian` /
`WriteEndian`, which are host-aware natively: `BinStream.cpp` swaps when
`!mLittleEndian` under `HX_NATIVE`), and fixes each real one in an `HX_NATIVE` arm.
All fixes assume a little-endian native host, as the existing native arms do
(`VorbisReader` `Decrypt`, `SHA1` `blk0`, `ChunkStream`).

## History

- 2026-10-07, worktree `~/tmp/wt-w16ue`, branch `w16-ue` from main `a0d283292`
  (commit `2d4bc50e8`): live-line map, regex families, typed sweep stopped at 57 of
  777 TUs by a server reboot, 7 findings, no edits. Baseline native gate on that
  tree: `NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0`.
  That worktree is left as it was.
- 2026-11-22, worktree `~/tmp/wt-byteorder`, branch `byteorder` from main
  `af50f101a`: re-verification, fixes, finished typed census (this text).

## Instruments (2026-11-22; scratch under `~/tmp/byteorder/`)

The 10-07 compdb pointed at the old worktree and a six-week-old file set, so it
was regenerated rather than resumed.

1. **Compdb** `~/tmp/byteorder/cdb`: `cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON` on
   this worktree's `native/`, 5,010 commands, deduplicated to one command per file:
   **1,000 TUs** (853 `src/`, 83 `native/`, 64 `../milo-native-engine`). One command
   per file means a file compiled with different `-D` sets by different targets is
   parsed once (the 10-07 pass kept one per (file, -D/-I) set: 777 TUs then).
2. **Typed matchers** (clang-query, `run_one.sh` / `run_six.sh` / `run_seven.sh`,
   parser `parse.py` → `kind|file:line|A0 type|A1 type`). clang parses only
   preprocessor-live code, so every hit is on a natively live line; no separate
   live-line map was needed this time.
   - `q.txt` (the 10-07 matchers, unchanged): (1) `Read/Write/ReadAsync/…/Recv/Send`
     arg 0, (2) `FileRead/FileWrite/read/write/recv/send` arg 1,
     (3) `memcpy/memmove/memcmp`, (4) `*(T *)charOrVoidPtr`,
     (5) `((T *)charOrVoidPtr)[i]`, T not byte-sized. **989 rows**
     (kind 1: 251, 2: 19, 3: 622, 4: 85, 5: 12), all 1,000 TUs done.
   - `q6.txt` (new): a char/void pointer **cast into a scalar-pointer variable**
     (`T *p = (T *)buf;` or `p = (T *)buf;`), which kinds 4/5 cannot see because the
     deref happens later. Control: it finds `Bitmap.cpp` `bytepair` (#3) and
     `twobytes` (#4). **81 rows.**
   - `q7.txt` (new): every natively compiled call to an explicit swap
     (`EndianSwap*`, `SwapBytes`, `SwapData`, `EndianSwapBitmap`,
     `__builtin_bswap*`, `_byteswap_*`, `hton*/ntoh*`). **32 rows** after the fixes.
   - Discrimination: `OSCMessenger.cpp` (known positive) returns its five kind-4
     word loads; three of the 67 TUs with zero hits (`TempoMap.cpp`,
     `json_object.c`, `Shuttle.cpp`) were re-run with stderr and parse cleanly
     (`0 matches`, no errors), so an empty file is a real zero, not a parse failure.

## Findings

### Fixed (commit `b99110d70`, all `HX_NATIVE` arms or macros that expand to the old X360 tokens)

| # | site | defect natively (re-verified on `af50f101a`) | reach | fix |
|---|---|---|---|---|
| 1 | `utl/BeatMap.cpp` `WriteWav` | RIFF is little-endian; every size field and every sample was `EndianSwap`ped unconditionally, right only on a BE host | `GameMic` `do_record` → `~GameMic` | `WAV_LE(x)` = identity natively, `EndianSwap(x)` on X360; sample swap skipped natively |
| 2 | `synth/VorbisReader.cpp` `DoRawSeek` | `*(int *)mNonce = EndianSwap(byte / 16)`. Re-checked: `tomcrypt/ctr.c` increments `ctr[0]` first (lines 61-63 and 128-133), so the block index is a little-endian counter; X360's swap-then-BE-store writes it LE. Natively the same tokens wrote it BE, so decryption after any seek used the wrong counter. The old comment ("stored big-endian") was wrong on X360 too | `VorbisReader::Seek` → `DoRawSeek` on an encrypted mogg | plain store natively; comment corrected |
| 3 | `rndobj/Bitmap.cpp` `DecodeDxtColor` `color0/color1`, `DecodeDxt3Alpha` `bytepair[j]`, **and `RndBitmap::DxtColor`'s inline DXT3 `alphaData[yRemainder]`** (missed on 10-07) | X360 DXT words are 8-in-16 byte-swapped and read with a BE halfword load. Re-checked in `../milo-native-engine`: `TextureConvert.cpp` `ByteSwapDXT` and `Rnd_Wgpu_RB3.cpp` `ByteSwapDXT16` both swap a *copy* (`workBuf` / `work`), and `RndTex_Native.cpp` `RndBitmap::Load` reads the pixels raw, so `RndBitmap` memory keeps X360 order | `RndBitmap::PixelColor` on any shipped DXT1/DXT3 bitmap | `SwapBytes()` on each word load natively. `DecodeDxt5Alpha` is byte-level with an explicit swizzle: unchanged |
| 4 | `rndobj/Bitmap.cpp` 16 bpp `PixelColor` read (`SwapBytes(*(u16 *)uc)`) and `ConvertColor` write (`EndianSwap` on store) | 16 bpp pixels are LE in memory on both hosts (shipped bitmaps; `LoadBmp` reads BMP rows raw and sets the alpha bit in byte `i + 1`, the LE high byte). X360 needs the swap, LE does not | `PixelColor`/`SetPixelColor` on a 16 bpp bitmap | plain load natively; store swap skipped natively |
| 5 | `synth/OggMap.cpp` `ReadData` | XORs `gMagicHashA/B` into page bytes 12–15/20–23 as host words; `VorbisReader`'s native `Decrypt` already bswaps the hashes for this reason, OggMap did not | latent: `OggMap::Validate` needs `OggValidator::Create`, declared and never defined (W16-UB #12) | `__builtin_bswap32` of each hash natively |
| 6 | `utl/OSCMessenger.cpp` `Poll` / `SendOSCFloat` | OSC is a big-endian wire format; int/float args were read and sent in host order | none natively (`Connect` needs Holmes and a real socket) | `OSC_WIRE32(x)` = bswap natively, `(x)` on X360 |
| 7 | `os/Timer.h` native `__load/__store{word,short}bytereverse` | emulate PPC `lwbrx`/`stwbrx` (load/store LE data) with an unconditional bswap, which is a BE load on an LE host; their own comment said "no-ops on little-endian" | no caller in `src/`, `native/` or the engine (re-grepped) | plain `memcpy` loads/stores |
| 8 | `band3/game/GameMic.cpp` `GameMic::Update` playback (**new**, census kind 1) | `mStoredAudio` holds `SetInputFile`'s raw `.wav` samples (`WaveFileData::Read` into `Buffer()`), which are LE; every sample was swapped after the read | `GameMic::SetInputFile(filename)` playback | swap loop compiled on X360 only |
| 9 | `synth/WavReader.cpp` sample loop (**new**, census kind 1) | `mInWaveFileData->Read(mRawInputBuffer, …)` reads LE `.wav` samples, then every sample is swapped into `mInputBuffers` | every `.wav` stream read through `WavReader` | `WAV_SAMPLE(s)` = identity natively, the old shift-or on X360 |

### Checked, not a defect

From the 10-07 pass (still true on `af50f101a` where re-read):

- `rndobj/Utl.cpp` `EndianSwapBitmap` (called by `Rnd::CreateDefaultTexture`,
  `Rnd.cpp:1622`): word load, reverse, store is the same byte permutation on
  either host.
- `bandobj/BandFaceDeform` `Delta` / `BandHeadShaper::AddFrame`: the first halfword
  is loaded with `bs >> (short &)` (endian-aware) and read back as `*(u16 *)d`.
- `utl/Chunks.cpp` / `WaveFile.cpp`: `bs >>` on a stream whose flag the caller sets.
- `utl/ChunkStream.cpp` `DecompressMemHelper`, the 0x810 chunk header, and
  `WriteChunk`'s size word: native arms already drop the swaps.
- `char/CharBonesSamples.cpp`, `rndobj/Mesh.cpp:400-446`, `os/AsyncFile.cpp`,
  `os/HDCache.cpp:370`: existing native arms.
- `utl/Locale.cpp` `FastSort`, `rndobj/Font.h` `Key`, `Color`/`Color32` packing,
  `ByteGrinder`, `MemHeap`/`trie` headers: in-memory values, not data.
- `synth/VorbisReader.cpp` native `Decrypt`, `math/SHA1.cpp` native `blk0`:
  correct.

New on 2026-11-22, from the typed rows:

- `rndobj/HiResScreen.cpp` `BmpCache::SetPixelColor` stores
  `(a<<24)|(r<<16)|(g<<8)|b` as a host word and `GetPixelColor` reads bytes
  `[3]=a [2]=r [1]=g [0]=b` (BMP's BGRA). Natively the two agree and the file is a
  correct BMP; on X360 they do not (a BE store puts `a` at byte 0). Not a native
  defect. The whole path is dev-only (`TakeShot` is gated
  `MILO_DEBUG && HX_NATIVE`). Retail's own behaviour was not checked against
  `band.exe`.
- `os/HolmesClient.cpp:859`: `WriteEndian` — endian-aware.
- `synth/tomcrypt/ctr.c` word XOR, `PrefabMgr.cpp` `Randomize`,
  `BandPatchMesh.cpp`, `CharBones*.cpp`, `CharMirror.cpp`, `AmbientOcclusion.cpp`,
  `Geo.cpp`, `Object.cpp`, `Str*`, `Pool.cpp`, `AllocInfo.cpp`, `filterdesign.cpp`,
  `PlayerDiffIcon.cpp`, `BitCrushEffect.cpp`, `WahEffect.cpp`, `UTF8.cpp`: in-memory
  reinterpretation, never data.
- `synth/MicNull.cpp`, `synth/StandardStream.cpp:864`, `GameMic.cpp:42/46/70/73`:
  host-order PCM in memory (the record path's swap belongs to `WriteWav`, #1).
- `meta_band/BandMachineMgr.cpp` `SyncMachineMsg`: byte buffer whose contents are
  written with `bs <<` elsewhere.
- `os/NetworkSocket.cpp:68`: XNet DNS result, 360-only path (native has the socket
  stub).

### Recorded, not fixed

- `utl/EncryptXTEA.cpp` and `utl/BinkIntegration.cpp:111` (`EndianSwap` of Bink IO
  words): host-order key/keystream and an LP64 width defect in XTEA (`unsigned long
  v1/v2`). Only consumer is the BINKIO callbacks, which nothing natively enters
  (W16-UB §4.3), and there is no native Bink SDK to check against.
- `native/src/main_render.cpp` (6 explicit swaps): native-only tool code, not read.
- `../milo-native-engine` rows (64 TUs swept; e.g. `AudioDevice.cpp`,
  `PipelineDiskCache.cpp`): host-side caches and device buffers, not read in
  detail; engine fixes belong in that repo.
- Quazal (`network/ObjDup/SessionStateDDL.h`): out of scope per the standing
  directive.

## Census coverage

Covered:

- All 1,000 natively compiled TUs (one command per file) through matcher kinds 1–7
  above. Every row with a multi-byte buffer type in `src/` was read by hand
  (kinds 1/2: 37 `src/` rows after dropping byte/void buffers; kind 3: 76 `src/`
  rows with a multi-byte side, outside stlport/oggvorbis/json-c/speex/tomcrypt, the
  data-path ones read individually; kinds 4/5: every `src/` row; kind 6: every
  `src/` row; kind 7: every `src/` row).
- The structural readers on the 10-07 "next" list (ark header, MIDI, DataFile
  binary, save buffers, NetMessage, SongData, sample loaders, Bitmap save paths)
  go through `BinStream >>`/`<<`, which is host-aware natively; the typed sweep
  found no raw multi-byte `Read`/`Write` among them. `WavReader` (#9) and
  `GameMic` (#8) were the raw ones.

Not covered (blind spots of these matchers):

- Byte buffers read raw and later reinterpreted through a **struct** pointer
  (`Hdr *h = (Hdr *)buf; h->size`): kind 6 matches only scalar pointees.
- Values passed through a `union` or `memcpy` into a local multi-byte scalar from
  a byte buffer where both memcpy arguments are byte/void typed (kind 3 rows with
  two byte-ish sides were dropped unread: 198 of the 622 kind-3 rows).
- Code compiled natively only under a `-D` set other than the first command seen
  for that file (the compdb was deduplicated by file).
- Files `#include`d as `.cpp` into another TU are covered only where included.
- Shift-assembly of multi-byte values from bytes (`b[0]<<24|…`) was not swept;
  it is host-neutral by construction unless the byte order assumed is wrong for
  the format, which is a format question, not a host one.
- `native/` and engine rows were swept but triaged only as far as listed above.

## X360 match build

Every change is inside `#ifdef HX_NATIVE` / `#ifndef HX_NATIVE`, or a macro whose
X360 expansion is the previous token sequence (`WAV_LE(x)` → `EndianSwap(x)`,
`OSC_WIRE32(x)` → `(x)`, `WAV_SAMPLE(s)` → `(((s) << 8) | ((s) >> 8))`).
Expected Δ0. Result: see "Results".

## Results

*(filled in below)*

## Not done

- The behavioural gates (`native/src/w16ue_phase.cpp` in `rb3-render`,
  `--no-w16ue`, with a sabotage build per swap) were not written. The planned
  checks were: #1 write a WAV natively and parse it with an independent LE reader;
  #2 a seeked decode of a shipped v0x0B mogg must match the tail of the full
  decode (W16-TM's `DecodeMogg` rig); #3 `PixelColor` on a shipped DXT1/DXT3
  texture vs the engine's `ByteSwapDXT` + `DecompressDXT*`; #4 a 16 bpp BMP
  fixture through `LoadBmp`; #5 `ReadData` on a v0x0E mogg, check the first page's
  Ogg CRC; #8/#9 a known `.wav` through `WavReader`. The fixes rest on reading the
  code and the engine, not on a run.
- The blind spots listed under "Census coverage".
