# Permuter-sweep struct-cascade map (2026-05-29)

A 4-batch Ghidra-guided permuter sweep over **151 near-miss functions**
(80–99.9%, in the Ghidra symbol map, in units *not* owned by the active
struct-layout worktrees) produced **one** committable permuter win
(`CompressionEffect::Process` +0.54%, landed `cd1c521`) and reverted 2
semantic-breaking variants (FFT `reference_elimination`, Cache_Xbox
`argument_swap`).

**Conclusion: these near-misses are overwhelmingly struct-layout-class, not
permuter-class** — confirming the base-class-layout-wall thesis. The sweep's
real yield is this triage map. Each cluster below is a *single shared cause*
that, once fixed, should cascade across all listed functions. Ranked by
estimated leverage (functions × confidence).

Diff class read via:
`bin/objdiff-cli diff -p . -u default/<UNIT> '<MANGLED>' -f json --include-instructions`

## Cross-cutting causes (HIGH leverage — fix once, cascade many)

### 1. DataArray vtable-call dispatch pattern  ★ highest fan-out
Target calls `DataArray::Sym`/`DataArray::Node` through a **vtable indirection**
(`lwz r11,0x0,rX; slwi r10,rN,3; add r4,r10,r11; bctrl`), while our base emits a
**direct call**. Seen across many game TUs:
- `EventTrigger`: `PropSync(ProxyCall)` 97.35%, `PropSync(Anim)` 98.97%
- `CharClip`: `PropSync(NodeVector)` 96.69%, `PropSync(BeatEvent)` 95.94%
- `CharClipSet`: `SyncProperty` 98.53%
- `UIScreen`: `OnMsg` 89.69%
- `MidiParser`: `PushIdle`, `InsertIdle` (combined with offset skew below)
- `DataFile`: `DataWriteFile` 88.76%
This is a `DataArray` base-class layout / virtual-dispatch issue. Resolving the
DataArray member access pattern likely lifts a broad swath of game code. **Start
here.**

### 2. UIManager / UIComponent base layout skew
- `UITransitionHandler::FinishValueChange` 89.37% — `TheUI->field` at `+0x10`
  (base) vs `+0x28` (target) → **+24** into UIManager
- `UIScreen::OnMsg` 89.69% — UIManager vtable + DataArray (see #1)
- `UIButton::OnMsg` 80.58% — `ButtonDownMsg` offset `0xe0`→`0xd0` (**−16**)
One UIManager/UIComponent member-offset fix may cover all three.

### 3. JoypadData struct off by +8
- `UsbMidiKeyboard::Poll` 85.51% — JoypadData member `0x6c`→`0x74` (**+8**)
- `UsbMidiGuitar::Poll` 98.71% — same `0x6c`→`0x74` (**+8**), plus swapped
  `0xb`↔`0xc` byte fields
Shared `JoypadData` (or `JoypadClient`) layout. Note `JoypadClient::Poll`
(batch 2) showed PlatformMgr member `0x20`→`0x54` (**+52**) — possibly related.

### 4. Scalar-deleting-dtor sub-object size mismatches
`subi rX, r3, 0xNN` in a `scalar deleting dtor` encodes the sub-object offset;
a wrong constant = a base-class/embedded-member size mismatch:
- `CharNeckTwist` dtor: target `0x58` vs base `0x24` → base **0x34 too small**
- `FlowIf` dtor: target `0x30` vs base `0x90` → base **0x60 too large**
- `FlowSound` dtor: off **−40**
- `DancerSequence` dtor: off −36 (largely resolved by main's BinStream fix)
- `FlowOutPort` dtor: off −100 (0x64)

## Self-contained per-TU clusters

| unit | offset/cause | functions |
|---|---|---|
| **Rnd_Xbox (DxRnd)** | **−4 at high offsets** (`0x39c→0x398`, `0x39b→0x397`, `0x3a0→0x3a4`), extra field ~`0x2f0`, + unresolved XDK symbols (`D3DDevice_*`, `MemOrPoolAlloc`) | 7 fns: DxRnd ctor, DoPostProcess, Present, SetupGamma, SetDefaultRenderStates, BackBuffer-family (some already 100%) |
| **SampleZone** | **`sizeof(SampleZone)` 0x1c target vs 0x50 base** — 52 bytes too big (easy, high-value) | `MidiInstrument::operator=`, `SampleZone::Save` |
| **MidiParser** | **+16** (`0x88→0x98`, `0x8c→0x9c`, `0x30→0x40`, `0xa4→0xb4`) + DataArray (#1) | PushIdle, InsertIdle, ClearManagedParsers |
| **Geo (Box)** | **+4** (`0x10↔0x14`) on Box/Triangle/Polygon FP loads | Multiply(Box), Intersect(Segment,Triangle), Clip(Polygon,Ray) |
| **StoreOffer** | **+0x80** (`0xc0` vs `0x40`) array-member access | PackFirstLetter |
| **DateTime** | **+4** (`0x50→0x54`) local temp; also `SystemLocale()` vs `SystemLanguage()` callee + extra return arms (retail stripped a case) | Format, ToCode |
| **MasterAudio** | vtable slot **0xbc vs 0xc0** (−4) + r27↔r28 reg swap | SetupBackgroundChannel |
| **SynthSample360** | −4/−8 + wrong vtable dispatch | LengthMs |
| **MoveParent** | **+12** (0x0c) + wrong callee | CacheLinks |
| **CharIKFoot** | **+52** (0x34) + ~19 missing instructions | ctor |
| **Debug** | Debug class layout **+4** + a missing store | StartLog |

## Not struct — other classes (lower priority / different tooling)

- **anon-namespace hash / wrong-TU pairing** (splits.txt fix, not source):
  `CDReader` fns use hash `0x7f36a62b` while dc3 uses `0xa5cb6096`; the target
  fns belong to a *different* anonymous namespace. Re-pair in splits.txt.
- **Regalloc-class permuter-plateau** (candidates for regswap pattern / m2c /
  manual, NOT struct): `keygen_xbox` (asciiDigitToHex, shuffle2, parseHex16,
  memcpy_cs ~95–97%), `SHA1::Update`, `Rot` (MakeScale/Set@Quat/MakeRotQuat
  ~94%, FPR scheduling), `MicNull` (`extsh` sign-extension from `Rand::Int`
  return type), `ScrollSelect::SendScrollSelected`.
- **Different code path / API** (source-intent merge, not offset): `RndGroup::
  SortDraws` (PoolAlloc+placement-new vs ObjPtrList::insert), `Rnd::
  CreateDefaults` (vtable `New<>` for RndCam/RndEnviron), `BinStream::Write`
  (DebugNotifyOncePrinter block + extsb), `DataFile::ParseArray` (PoolAlloc
  signature), `SynapseAPO` dtor (OggFree vs operator delete).
- **asm-misnest residue**: `RndMultiMeshProxy` ctor — vtable symbol-label
  mismatches (`lbl_` vs `??_7`), see `project_jeff_asm_misnest`.

## Caveat on measurements
All 4 sweep worktrees were rebased onto moving main during the run, so some
agent-reported "regressions to 0%" were stale-incremental-build artifacts, NOT
real (verified: `EventTrigger::Anim` ctor is healthy at 99.43% on main). Treat
the *offset deltas* above as the signal; re-measure % against current main
before acting.

## Refs
- `docs/plans/next-wave-onediff-clusters.md` (sibling: CharFaceServo, Instance/
  SharedGroup, MemStream, etc. — disjoint from this set)
- `docs/plans/engine-baseclass-layout-bugs.md` (the foundational base-class bugs)
- Memory `project_engine_baseclass_layout_wall`, `feedback_fuzzy_gap_needs_permuter`
