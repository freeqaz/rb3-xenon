# dc3-drift virtual-signature flags — audited list (round 2, 2026-07-11)

Tool: `scripts/vsig_diff.py` (three-way virtual-signature differ; run it to
regenerate the raw list — 185 flags as of this date, `src/system` + `src/band3`
vs `../dc3-decomp` + `../rb3`).

This doc records the **audited verdicts** so nobody re-chases retired flags.
Two rounds of A/B (closeout25 a1, closeout26 a2) established the calibration:

> **"Retail follows rb3-Wii" is NOT a rule.** RB3-360 retail is TU5-era and
> follows dc3's newer shape at least as often as Wii's. Every flip must be
> verified against retail codegen (Ghidra call-site/body ABI, or objdiff
> pairing structure) BEFORE testing. A flag is only *testable* when an
> affected override/caller has a mapped retail pairing (near-miss in
> report.json) — rank by mapped surface first, verify second, flip last.

## Round-1 landings (closeout25 a1, net +1 strict + 1 byte-exact upgrade)

| Flag | Retail shape | Evidence |
|---|---|---|
| `DataThisPtr::Replace` | `void (ObjRef* from, Hmx::Object* to)` (Wii arity in the family slot) | body reads r4+r5, `gDataThis == from`; 92.14 → 100 |
| `Memcard::ShowDeviceSelector` | Wii param order `(ContainerId&, bool, Object*, int)` | base fn 0x825201A8 null-checks r6; override fn_82518458 r5=bool flag |

## Round-2 verdicts (closeout26 a2) — all testable flags verified, ZERO flips warranted

Every flag with mapped surface today verifies as **ours already correct**
(retail follows our/dc3 shape). Per-address evidence for each:

| Flag family | Ours (kept) | Wii said | Verdict + evidence |
|---|---|---|---|
| `SongMgr::AddSongData` container | `std::hash_map<int,SongMetadata*>&` | `std::map` (dc3 too!) | **ours correct, BOTH oracles wrong.** `BandSongMgr::AddSongData` (82.67) diff has insert=0 — every base instruction pairs 1:1 with target incl. our hashtable `_M_find` code; target has 27 *extra* instrs (see body-port lead below) |
| `PropKeys::Load` family (8 overrides) | `Load(BinStreamRev&)` | `Load(BinStream&)` | **ours correct.** BoolKeys/Vector3Keys/SymbolKeys::Load all 100 under `AAVBinStreamRev@@` mangling. Retires round-1 open lead #1 |
| `GetFileHandle` family (File/AsyncFile/BufFile/NullFile/FileCacheFile) | `bool (void*&)` | `int (DVDFileInfo*&)` | **ours correct.** `ArkFile::GetFileHandle` 100 under `_NAAPAX` |
| `SongMgr::Data` | `const SongMetadata* (int) const` | non-const return | **ours correct.** 100 under `PBVSongMetadata` |
| `SetADSR` family (SampleInst/StandardStream/StreamReceiver) | `(const ADSRImpl&)` | `(const ADSR&)` | **ours correct.** `StreamReceiver360::SetADSR` 99.94 under `ABVADSRImpl@@` (residual = regalloc) |
| `NodeSort/SongSortBy*::TextForNode` | `bool (...)` | `const char* (...)` | **ours correct.** `SongSortByStars::TextForNode` 100 under `_N` return |
| `Object::Replace` + 24 override flags (3WAY) | `void (ObjRef*, Object*)` | `void (Object*,Object*)` / dc3 `bool` | **ours correct** (wave-24 shape). 35/36 mapped Replace instances at 100 |
| `PropKeys::SetFrame` family | `(float,float,float)` | `(float,float)` | **ours correct** (round 1); Vector3Keys::SetFrame 98.49 mapped under `MMM` |
| MCFile/MCContainer/Memcard `MCResult` returns (~20 methods) | `MCResult` | `void`/`int` | **ours correct** (round 1, 100s across Memcard_Xbox) |
| `SongData::CalcSongPos` | both shapes declared | `(float)` only | already resolved in-tree: retail vtable slot is `CalcSongPos(float)`, proven by Game::Poll (see `src/system/beatmatch/HxSongData.h` comment) |
| `StreamReceiver::StartSendImpl` | `(uchar*,int,int)` only | Wii adds 5-arg overload | **leaning ours-correct** (weaker): whole StreamReceiver360 TU at 99.9x with our slot layout; Wii's 5-arg is ring-wrap handling of the 0xC000 Wii ring, retail uses 0x4000 flat buffers. Base `StreamReceiver::Poll` itself is unmapped — recheck only if it gets pinned and mismatches at a vcall |

### Mangle-only / codegen-neutral noise (do not chase as match levers)

Const-qualification (`RefOwner() const`, `Print() const`, `DOFProc::Set(const RndCam*)`,
`WorldInstance::ProxyFile`), enum renames (`SubdirAction` vs `Action`, `StoreError`
vs int returns, `Mic::Type`, `RndTex::AlphaCompress` vs bool), int-width aliases
(`u32`/`uint`/unsigned vs int returns, `int*` vs `void*`), `CopyType` vs
`Object::CopyType` (same type — differ-printing noise), `char const*` vs
`const char*` (pure noise), ptr-vs-ref `As*Keys`. These change mangled names
(map-entry spelling) but not code bytes.

### No mapped surface today (re-rank after synth/os pin waves)

`Memcard::CreateContainer` (callers SetDevice/ThreadCall_SearchForDevice
unmapped; ThreadCall_SaveGame 87.60 does NOT call it), `Stream::GetJumpBackTotalTime`,
`Stream/StandardStream::AddMarker` (by-value vs const-ref — real codegen if ever
mapped), `SynthSample::NewInst`, `SampleInst::StopImpl/SetStartProgress`,
`FixedSizeSaveableStream::FinishWrite/FinishStream`, `StorePanel::MakeNewOffer`,
NetworkSocket return-shape family (only mapped fn is `WinSockSocket::Accept` at 0%),
Mic buffer getters (bodies unmapped; `MicXbox::GetType` 0% is a port gap, not a flag),
Content/Cache/CacheMgr families.

## Spin-off leads (NOT signature flags)

1. **`BandSongMgr::AddSongData` body-port (82.67 → ~95+):** retail body = ours
   + 27 extra instructions: `Symbol` ctor, `map<int,float>::insert_unique`,
   `hash_map<int,SongUpgradeData*>::operator[]`, unmapped `fn_8255F488` /
   `fn_82586AB0` / `fn_82589208`(x2) / `fn_82783CD8` — TU5-era song-upgrade
   rating logic absent from both oracles. Requires reversing the extra block
   from Ghidra (target-only cluster at instr 136-150 + 18-21 + 32-39), then a
   regswap permuter pass (38 swap instrs).
2. **`MicNull::GetRecentBuf` (93.33):** 2-instruction address-formation quirk —
   retail keeps `this` in r11 and folds `+0x5a60` into the memcpy arg
   (`addi r4,r11,0x5a60`); we precompute. Permuter/source-shape nit.

## Vein status

**DRAINED at current mapped surface** (two rounds: +1 net strict, ~35 flags
retired with evidence). The constraint is mapped surface, not flag supply —
rerun `scripts/vsig_diff.py` + re-rank against report.json near-misses after
big pin/identification waves (synth and os families especially).
