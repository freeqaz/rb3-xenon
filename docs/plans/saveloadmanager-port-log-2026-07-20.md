# SaveLoadManager port — LOG / handoff

Worktree `~/tmp/wt-slm-port`, branch `slm-port`. Pin 0x8254B070..0x82553F28,
**405 target fns**. Base = carve-pilot pin+wiring (cherry-picked).

## RESULT (verified)
- TU **compiles** against Xbox headers.
- **27 / 405 strict** (`match_percent_normalized==100`), **24.67% fuzzy**.
- Whole-binary **+27** (baseline 18,924 → 18,951), **named-LOST == 0**
  (clean A/B: reverted 5 changed files to stub, full `ninja-locked`,
  `rm report.cache` before each report.json — baseline exactly 18,924; the +27
  equals the unit gain, so the 3 shared-header edits regressed nothing).

## Commits
1. compile against Xbox headers (source + 3 shared headers)
2. reveal 3 byte-exact (IsAutosaveEnabled, OnMsg(ProfileSwappedMsg), ManualSave)

## Port adaptations (MWCC/Wii → MSVC/X360)
- `MemcardMgr_Wii.h` → `MemcardMgr.h`; `SelectDevice(Profile*,Hmx::Object*,int,bool)`
  arg order (Xbox reorders Wii's `(profile,bool,sink,pad)` → `(profile,sink,pad,bool)`).
- `ObjectDir::sMainDir` → `ObjectDir::Main()`; `ThePlatformMgr.mGuideShowing` → `.GuideShowing()`.
- brace `int sz` case clauses (MSVC C2360); `(uint)`→`(unsigned int)`;
  `_MemAllocTemp(x,0)` → `(_MemAllocTemp)(x,0)` (bypass in-tree 5-arg debug macro).
- `MakeString<SaveLoadMode>(...,(SaveLoadMode)mMode)` (header `mMode` is int).
- **FixedSizeSaveable.h**: dropped DC3 leak `#include "meta_ham/HamMemcardAction.h"`
  (stub Save/LoadMemcardAction collided with RB3's real ones) → forward decls.
- **WiiProfileMgr.h**: decl-only `NeedsLoading/PreLoad/NeedsSave/SetLocked/SaveSize(int)`.
- **MemcardMgr.h**: decl-only `IsDisableWriting/DisableWriting` (absent on retail Xbox).
- SaveLoadManager.cpp: `extern Symbol song_info_cache_*` (missing from Symbols headers).
- Helper `Save/LoadMemcardAction` ctors take `std::vector<BandProfile*>*` (header dropped
  the Wii `,unsigned short` — correct: this STLport's 2nd param is the *allocator*, not a
  size-type, so the 2-param form is inapplicable; plain vector is **0xc bytes**, 3 ptrs).

## THE WALL (why only 27/405) — three layers

### 1. Class layout in SaveLoadManager.h disagrees with retail (FORCE-MULTIPLIER)
Ground truth = retail ctor **fn_825521E0** (Ghidra TU5 :8002). `Init()` (fn_82552450)
does `new(0xb0)` so **sizeof=0xb0**. Ctor member inits vs header:
- `mActivated` byte **@0x18**, `mInitialLoadNotDone` byte **@0x19**  (header says 0x1c/0x1d).
  → implies retail MsgSource primary base ends at 0x18, not 0x1c (engine base — risky to touch).
- `mMode..mSaveProfiles-region` ints zeroed **0x20..0x3c** (matches header order).
- `DataArrayPtr` ctor **@0x40** (header `unk44`@0x44); `String` ctor **@0x48**.
- Only **ONE** vector `reserve(4)` @0x34 (Wii/header have two: mUploadProfiles+mSaveProfiles).
  With 0xc-byte vectors, one vector@0x34 fills 0x34-0x40 exactly → retail has a single
  profiles vector before the DataArrayPtr; the 2nd vector (if any) sits later. **The
  header's two-adjacent-8-byte-vectors model is wrong.**
- Hmx::Object virtual base **@0x88** (header labels 0x88 `mTimer`). Retail mTimer must be
  earlier or absent; 0x88 is the shared vbase.
Fixing this layout (reconstruct every offset from ctor + a few member-accessing fns) is the
single biggest lever: it would correct member offsets across ~all bodies AND the funclet
frame sizes (see §3). NOT attempted here — needs multi-function Ghidra cross-ref and may
touch the engine MsgSource base size (whole-binary risk → own gated A/B).

### 2. Bodies diverge Wii → retail (platform rewrite)
Retail Xbox rewrote the platform paths the Wii source encodes: `MemcardMgr_Xbox` (real
`SelectDevice`, no `IsDisableWriting`), song-info-cache states (0x14-0x22, a Wii RAM-cache
concept), `TheWiiProfileMgr` (a near-empty 360 stub), autosave-disable. Reloc-masked
similarity of my named bodies vs their best target: most real bodies **0.2-0.6** (Start,
AutoSave, IsReasonToUpload, GetDialogOpt1, DisableAutosave, AutoSaveNow...). These need
per-function reconstruction from Ghidra TU5, not verbatim Wii logic. Byte-exact today:
IsAutosaveEnabled, OnMsg(ProfileSwappedMsg), ManualSave (the 3 revealed).

### 3. 187 near-miss funclets (95-99.9% normalized) are DOWNSTREAM of §1/§2
The bulk of unmatched are 40-48 byte `__unwind$` funclets (DataNode/temp dtor cleanup for
DataArrayPtr-returning fns like GetDialogMsg). Their ONLY diff is the parent frame-pointer
immediate, e.g. `subi r31,r12,0xf0` (retail) vs `0x90` (mine) — my ported parent has a
different stack frame. They flip in BATCHES when their parent body matches (frame size =
locals/spills = body correctness). Do NOT chase funclets individually.

## Measurement notes
- objdiff **positionally pairs** anonymous target `fn_` to base fns, so the report counts
  matches without a map entry. `run_objdiff`/MCP pairs by NAME → a named body needs its
  target `fn_` mapped in scripts/target_symbol_map.json to be inspectable per-symbol.
- `tools/reveal_sweep.py --units default/band3/meta_band/SaveLoadManager` = byte-exact
  self-validating reveal (found 3 unique; rest ambiguous tiny fns or divergent).
- Byte-similarity correlation of divergent bodies is UNRELIABLE (misfired Start→Init).

## Recommended next steps (priority order)
1. **Reconstruct the class layout** from ctor fn_825521E0 + Activate/Poll/GetDialogMsg
   member accesses; fix SaveLoadManager.h offsets & member set. Verify sizeof=0xb0,
   DataArrayPtr@0x40, single-vector-before-it. Gated whole-binary A/B (may touch MsgSource).
2. With layout correct, port big bodies largest-first from Ghidra TU5 (NOT verbatim Wii):
   ctor fn_825521E0 (0x170), SetState fn_82550880 (0x1000, ~106-case), the 0x8254CC98
   (0x19C0) & 0x82553490/0x82552660 giants (GetDialogMsg/Opt1/Poll). Each body match flips
   its funclet cluster.
3. Re-run reveal_sweep after each body to auto-name new byte-exact fns.

## RESOLVED — layout reconstructed (2026-07-24, bodyport-w2, commit on branch bodyport-w2)

The §1 "class layout" wall is CLOSED. Ground truth = ctor fn_825521E0 (Ghidra
TU5) + objdiff full listing of `??0SaveLoadManager@@QAA@XZ`. Key corrections to
the wave-1 hypotheses:

- **mActivated IS at 0x18** (header "// 0x1c" comment was stale) — our MsgSource
  base already compiles to the right size; NO engine base edit needed. MsgSource
  non-virtual subobject ends at 0x18 (vbptr@0, mSinks list@0x4, mEventSinks@0xc,
  mExporting@0x14) with 4B tail pad that the derived class reuses. Retail base
  ctor is `PropertyEventProvider` (derives from MsgSource, same layout) — objdiff
  normalizes the `bl` so no source change needed there.
- **vector<BandProfile*> is 8 bytes** here (pointer-specialized STLport impl:
  _M_start + _M_finish only). Retail has a SINGLE 8-byte vector@0x34 followed by a
  4-byte member@0x3c (unk3c) — NOT two vectors and NOT one 12-byte vector. Wii's
  mUploadProfiles+mSaveProfiles pair collapses to one vector + one 4B slot. This
  8-byte-vector fact is the root of the uniform -4 shift from 0x3c onward.
- **No Timer member** — 0x88 is the shared Hmx::Object vbase. Dropped mTimer +
  Stop/Restart calls.
- Tail: byte@0x58, int@0x6c/0x70, byte@0x74(mRequestFlags)/0x75, int@0x78/0x7c,
  mAction@0x80, uninitialized 4B@0x84 (pads vbase to 0x88). sizeof = 0xb0.

**Result:** ctor 75.4% -> 99.99% (1 residual reg-scratch diff at the vector
reserve address — permuter-class). Whole-binary +5 / 0 lost. The 184 funclets
stay at 99.9% — each is `subi r31,r12,imm` off by the PARENT frame size, so they
only flip once the divergent parent bodies (Start/AutoSave/Poll/SetState/
GetDialogMsg/Opt1/Poll giants) are reconstructed from Ghidra TU5 (NOT verbatim
Wii — retail rewrote the platform paths). **The layout is now correct, so a
body-port wave on those parents is unblocked and each parent flip cascades its
funclet cluster.** That is the recommended wave-3 SLM task.
