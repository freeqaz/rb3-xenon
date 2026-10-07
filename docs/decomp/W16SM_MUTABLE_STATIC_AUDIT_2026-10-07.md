# W16-SM — whole-binary `--include-mutable` audit of writable statics (2026-10-07)

Base: main `eb5592461` (W16-SL). Worktree `~/tmp/wt-w16-sm`, branch `w16-sm-mutable-statics`.

## Method

`tools/const_value_audit.py --include-mutable` over all 1,046 units / 56,318 paired
functions, compared with a read-only run so only the writable-data mismatches
remain. Each retail value was then tied to the functions that read or write it,
using a lis-hi + D-form-lo xref scan over retail `.text`. A label was accepted
as a variable only when its retail readers and writers are that variable's
functions.

**Tool fix (blind spot).** `Coff.extent` returned `b""` for any of our `.bss`
symbols, so an `addi` (address-of) to a zero-initialised static had no width and
was skipped as `extent`. Those skips covered **11,805** operands, which is exactly
how co-addressed statics and arrays are reached. A `.bss` symbol now reads as
the zeros of its extent. Mutable-mode `extent` skips went from 11,820 to 15. The
fix surfaced 3 further rows (`kBlurTaps`, `kServerVer`, OggMap `gKey`), and none
of them is a plain value fix (see below). In read-only mode it adds two VALUE
rows, both sites where ours is `.bss` and retail's target is `.rdata`.

## Fixed (in scope)

| static | ours → retail | retail VA | readers / writers in retail |
|---|---|---|---|
| `RndMesh::sLastCollide` | 0 → **-1** | 0x82C6FC20 | `CollideShowing` (stw), `BandPatchMesh::ProjectPatches` |
| `gLoadCount` (Loader.cpp) | 0 → **1** | 0x82C78E2C | `LoadMgr::PollUntilLoaded` only |
| `MemHeapStack::sDefaultHeap` | *undefined* → **-1** | 0x82C78E24 | `GetCurrentHeapNum`, `MemCurrentHeap`, `MemAlloc`, `MemInit` (stw); DC3 `= -1` |
| `gHasPreconfig` (System.cpp, anon ns) | false → **true** | 0x82C71834 | `PreInitSystem`; DC3 `= true` |
| TexMovie `gSaveRev` | 8 → **5** (off `HX_NATIVE`) | 0x82C77238 | `TexMovie::Save`; Save writes rev 5 |
| UIFontImporter `gSaveRev` | 0x4000A → **9** | 0x82C793C0 | `UIFontImporter::Save` |
| `gRevs_{LabelNumberTicker, UIListDir, UIListWidget, MeterDisplay, MiniLeaderboardDisplay}` | {alt, rev} → **zero** | 0x82E07E60 / 0x82E079D4 / 0x82E07D10 / 0x82CBDD34 / 0x82CBDD14 | all past `.data`'s raw end (0x82CBC600), i.e. retail `.bss` |

TexMovie keeps rev 8 under `HX_NATIVE`. Its native Save still writes DC3's
`mIsLocalized`, which native Load reads only at `rev > 5`, so a bare 5 would
desync a native round-trip.

**Behaviour fix the audit exposed (not a static):** `CamShot::GetCam`'s
`__RTDynamicCast` target descriptor at 0x82C6CF4C names `.?AVPanelDir@@`.
Ours (from DC3) cast to `WorldDir`, which returns no
camera for a plain PanelDir. The row read 100 throughout because the descriptor
is a forgiven placeholder.

**A/B** (`ab_measure --from-dirty`, name_check): **Δ0 on every key**, 24 leg-B
recompiles, **0 of 68,884 rows changed**. The SL co-addressing regression did
not occur. The two internal statics that left `.bss` (`gLoadCount`,
`gHasPreconfig`) have no internal `.data` neighbour in their TU. The rest are
external or single aggregates. After the fix, the mutable audit clears all 13
targeted rows and adds no new flag.

## Real wrong values, OUT OF SCOPE (Xbox platform / XDK / Quazal) — not fixed

| where | static | ours → retail |
|---|---|---|
| `os/System_Xbox.cpp` | `gCallback` (Get/SetDiskErrorCallback) | NULL → `ShowDirtyDiscError` (0x8251B870) |
| `rnddx9/CubeTex.cpp`, `rnddx9/Rnd.cpp` | `sParticleDecl`, `sDepthRectDecl` terminator | Stream 0xFFFF → **0x00FF**: our `D3DDECL_END` (XDK header) is wrong |
| `rnddx9/Rnd.cpp` | `sDepthRectVerts` | 4 floats at +64/+108/+156/+160: 0.0 → 1.0 |
| `rnddx9/Rnd_Xbox.cpp`, `rnddx9/CubeTex.cpp` | `sMutableSkinnedVertexElements`, `sMutableVertexElement` | DC3's vertex layout; retail's element types/offsets differ throughout (e.g. elem 1 type 0x2A23B9 vs ours 0x1A23A6) |
| `rnddx9/Rnd_Xbox.cpp` | `CopyPostProcess::sCopyPostInited` | 0 → 1 (only CopyPostProcess reads/writes it; meaning unverified) |
| `rnddx9/ShaderMgr.cpp` | `TheDxShaderInclude` | dynamic init in ours; retail constant-initialises its vptr (0x821017D4) in `.data` |
| Quazal `UDPTransport` | `g_uiTransportWaitTime` | 0 → 10 |

## Flags that are NOT wrong values

- **RTTI descriptor reorders** — `GatherObjectsFromDir/Group<RndMesh>`
  (AmbientOcclusion) and `CharClipSet::LoadCharacter`. `??_R0` descriptors are
  writable `.data`, so mutable mode now compares them. Each side binds the same
  descriptor to the same register (r25 = RndMesh, r28 = Object, r27 =
  WorldInstance), and only the `lis/addi` (or `mr r5/r6`) order differs. The
  USE tracker keys `mr` rows by row index, which makes this a false SWAP.
- **`NextHashPrime` primes** — retail's table is **61 words with no 0
  terminator**. 0x82CA9074 begins `.?AVUGCNet_Server@@`'s TypeDescriptor, and
  the `!= 0` loop runs into its `spare` word. Ours (= DC3) has 62 with
  a trailing 0. Left as is, because replicating it would write a deliberate
  out-of-bounds read.
- **`RndSoftParticleBuffer::BlurSurface` `kBlurTaps`** — retail `.data` holds
  only the leading 0.1f, and the other nine are stored on first use (already
  documented in source). That is a partial-static-init codegen shape, not an
  initializer value.
- **`WaitingUserGate::OnMsg`** — the RTTI type-name string embeds the
  anonymous-namespace hash (`?A0x5b3730ba` retail vs ours). `anon_ns` rewrites
  symbol names, not string bytes, so no source fix exists.
- **`RockCentral` fn_824F6768** (anon, 80%) — ours `addi kServerVer` (a String
  object) is aligned against a retail `.rdata` string literal. This is
  misalignment in an unopened row.
- **`OggMap::SetKey`** — the `"%x"` literal and `gKey` are aligned crosswise.
  This is misalignment, not a value.

## Coverage bound

The audit sees only statics referenced by **paired** functions with an `@l`
operand (direct or via `addi`). A static read only by an unpaired or anonymous
row, or only through a pointer loaded from elsewhere, is not covered. A value
written by a dynamic initializer is invisible on both sides.
