# Same-Instrument patch — TU5 (retail v0.0.5.1) retarget

Status: **COMPLETE, byte-verified.** Worktree-only, nothing committed.
Date: 2026-07-07. Author: Opus engineer (TU5 migration keystone).

Retargets the RB3Enhanced "Same Instrument" static code-cave patch from the
rb3-xenon BASE (TU0, v0.0.0.1) binary to the RETAIL TU5 (v0.0.5.1) XEX
(`orig/45410914/default_tu5.xex`, title 45410914). All game addresses re-derived
and byte-verified against `band_tu5.exe` via the INGEST worktree's section-mapped
reader `tools/tu5_va.py` (flat `0x3000+VA` is WRONG on TU5 — never use it).

---

## 1. Deliverables (paths + hashes)

| Artifact | Path | sha256 |
|---|---|---|
| Patched TU5 XEX | `<wt>/orig/45410914/default_tu5_patched.xex` | `9c5965ad7df7e1d34f49501d6dbe1754520868c06156dfebd0ceb9f8707d1c6f` |
| — (sha1) | | `a9fa9a91863cbe727377420bd6debe2790ffeac1` (13,971,456 B) |
| TU5 patch.toml | `RB3Enhanced/patches/45410914_same_instrument_full_tu5.patch.toml` | `f03da9d27655f4c811087bcb9465e12cc5ef86a512610d43251e63521247a52f` |
| TU5 cave blob | `RB3Enhanced/build_patch/tu5/same_instrument_cave_tu5.bin` | `0832715b6230018b43f1a73fb5993139000e9a4e5cd99f11676b19ddd0949cec` (2800 B) |
| TU5 standalone obj | `RB3Enhanced/build_patch/tu5/SameInstrumentHooks_tu5.obj` | `7d1b00a985abf128df690d4f7e472b31e943c3186ee54a3fec69708e523dddc1` |

`<wt>` = `/home/free/code/milohax/rb3-xenon/.claude/worktrees/tu5-migrate`.
Orig TU5 xex sha256 `6639ce25745505b598480499ca53b421fdec5604d813f5ee2c8152ecdad2a5ea`.

### TU5-aware tooling (new files; base tools untouched)
- `RB3Enhanced/scripts/objcave_pack_tu5.py` — imports `objcave_pack.py`, overrides
  the address tables (detours / GAME_FN / GAME_CRT / ORIG_TARGET / DETOUR) and the
  cave base for TU5. Verbs identical to the base packer.
- `<wt>/tools/xex_binpatch_tu5.py` — SECTION-MAPPED XEX patcher. Maps VA → XEX file
  offset via per-section empirical deltas (see §4), not flat `0x3000+VA`.

---

## 2. Re-derived TU5 values (base values do NOT transfer)

### 2a. Detour targets (all prologue `mflr r12` confirmed on TU5, hookable)
| Symbol | BASE VA | **TU5 VA** | method |
|---|---|---|---|
| OvershellPartSelectProvider::IsActive (Layer A) | 0x8264B5F8 | **0x826684C0** | skeleton (first-128B); body diverges → detour is whole-fn override |
| OvershellPanel::ResolvePartWaitStates (Layer B) | 0x8259D948 | **0x825B6488** | skeleton HIGH (unique) |
| PlayerTrackConfigList::ProcessConfig (Layer C) | 0x8274ACF8 | **0x8276FA08** | skeleton HIGH; body confirms cfg+0x10/+0x20 |
| TrackWatcherImpl::RecalcGemList (centre) | 0x8276FBB0 | **0x82794740** | skeleton HIGH; body confirms +0x68/+0x50/+0x1c |

### 2b. Direct-call game functions (GAME_FN)
| Symbol | **TU5 VA** | provenance |
|---|---|---|
| GameGemDB::Duplicate | **0x827932C8** | skeleton HIGH (body verified) |
| GameGemDB::GetDiffGemList | **0x827931C8** | leaf, BYTE-IDENTICAL to base (`lwz r11,0(r3);slwi;lwzx;blr`) |
| GameGemList::CopyFrom | **0x8278E168** | skeleton HIGH |
| BandUser::SetOvershellSlotState | **0x8268BAF0** | body verified: `stw r4,0x20(r3); li r4,1; b 0x8268B220` |
| OvershellPanel::UpdateAll | **0x825B70D0** | skeleton HIGH from base fn_8259E5B0 |
| BandUserMgr::GetBandUserFromSlot | **0x82682B60** | RB3Enhanced general port = already TU5 (body: `(slot+5)<<4` index) |
| MemFree | **0x827BC430** | RB3Enhanced general port = already TU5 (body: allocator free) |

### 2c. CRT prologue/epilogue helpers (GAME_CRT)
`__savegprlr_14 = 0x82829220`, `__restgprlr_14 = 0x82829270`, **stride 4** (block is
`std rN,-disp(r1)` on Xenon). Full r14..r31 tabulated by the packer. Cross-checked:
RecalcGemList calls savegprlr_27 @0x82829254, IsActive calls savegprlr_25 @0x8282924C,
the packed hooks call savegprlr_27/28/29 @0x82829254/58/5C.

### 2d. Data literal baked into the obj — UNCHANGED
`PORT_THEBANDUSERMGR = 0x82E023B8` is the **same on TU5** (verified: 106 `lis 0x82e0;
lwz …,0x23b8` references on TU5). This is the ONLY version-specific value baked into
the compiled obj, so the obj is otherwise version-independent (see §3).

### 2e. Struct offsets — ALL IDENTICAL to base (byte-verified on TU5 bodies)
- BandUser `mOvershellState` @ **0x20** (`lwz r11,0x20(r23); cmpwi cr6,r11,0xb` in
  ResolvePartWaitStates 0x825B6488).
- TrackWatcherImpl `mGemList` @ **0x1c**, `mSongData` @ **0x50**, `mTrack` @ **0x68**
  (RecalcGemList 0x82794740: `lwz r4,0x68; lwz r3,0x50; bl GetGemList; stw r3,0x1c`).
- SongData `mTrackDifficulties` @ **0x50**, `mGemDBs` @ **0xb0** (GetGemList 0x82770730:
  `lwz r11,0x50(r3); lwz r9,0xb0(r3)`).
- PlayerTrackConfig `mTrackType` @ **0x10**, `mTrackNum` @ **0x20** (ProcessConfig body).
- Reference-only TU5 VAs: SongData::GetGemList **0x82770730** (was 0x8274BB38),
  BandUser::UpdateData **0x8268B220** (was 0x8266D2B8).

Because 2d + 2e are all TU5-identical and everything in 2a-2c is resolved by the
PACKER (undef externals in the obj), the same standalone source compiles to a
functionally identical obj for both builds.

---

## 3. Compile (XDK-free, DO-faithful)

Recompiled the standalone obj for TU5 with the stage-2 recipe **plus**
`-D SI_STANDALONE_PATCH`:

```
wibo cl.exe -c -nologo -W3 -WX- -Ox -Os -GF -Gm- -MT -GS- -Gy -fp:fast -fp:except- \
  -Zc:wchar_t -Zc:forScope -GR- -openmp- -D _XBOX -D RB3E_XBOX -D SI_STANDALONE_PATCH \
  -I include -I build_patch/tu5 -I <rb3-xenon>/src/xdk/LIBCMT \
  -TC -Fobuild_patch/tu5/SameInstrumentHooks_tu5.obj source/SameInstrumentHooks.c
```

Exit 0, 0 warnings. Reloc histogram REL24 37 / REFHI 11 / REFLO 12 / PAIR 23 /
ADDR32 6 = identical to the base standalone obj (stage-4). The two objs differ only
in COFF symbol/string-table ordering + timestamp; they are functionally identical.
**No source `#ifdef` or new header set was needed** — the version delta is entirely
in the packer tables, not the C.

---

## 4. TU5 XEX section-mapping (the load-bearing fix)

TU5's XEX is "basic"-format: the stored image is section-mapped. The XEX packs each
PE section's raw bytes at `dtk_off + delta`, where `delta` is constant WITHIN a
section but differs BETWEEN sections. Measured this run:

- `.text` delta = **+0x6200** (this is the source of the spike's uniform −0x8000
  drift; flat `0x3000+VA` lands mid-function).
- `.data` delta = **+0xD400**.

`xex_binpatch_tu5.py` recovers each section's delta by anchoring a unique non-zero
24-byte signature near the section start, then `xex_off(VA) = dtk_off(VA) + delta`.
The mapping is self-checking: every detour site must pre-read `7D8802A6` and every
cave word must pre-read `0` — both held on apply (proving the map).

---

## 5. Cave — corrected location

**Cave base = 0x82C8A000** (in TU5 `.data`, zero run 0x82C89DB8..0x82C8DB6F = 15,799 B).
Chosen because it is **file-backed** (required for a *binary* XEX patch) and
**unreferenced** (0 `lis`/mem materializations into 0x82C89000-0x82C8E000 from
`.text` → not a live global; safe from data corruption).

> **CORRECTION of the INGEST cave.** The INGEST proposed `0x82C55010` "BINK-section
> tail, 29,684 B file-backed zeros". That is WRONG: section-mapped, 0x82C55010 reads
> BINK **decoder code** (`38c00008 …`), not zeros — it is the INGEST's own −0x8000
> drift applied to the cave address. The genuine BINK/BINKBSS zero gap is at
> **0x82C5D010..0x82C643A0** (~29.5 KB, which matches the "29,684 B" figure) — but
> that gap is a load-zeroed VIRTUAL region **NOT stored in the section-mapped XEX
> file**, so it cannot host a *binary* file patch (only a memory-applied .patch.toml
> could target it). Hence the file-backed `.data` cave.

Cave layout (packer): blob 0x82C8A000..0x82C8AAF0 (2800 B). Hook entries
IsActiveHook@0x82C8A080, ResolveWaitStatesHook@0x82C8A0C0, RecalcGemListHook@0x82C8A590,
ProcessConfigHook@0x82C8A8B0; flag gSameInstrumentEnabled@0x82C8AAA0; memset thunk +
4 orig-trampolines near the tail.

---

## 6. Byte-verification table (from the PATCHED xex, section-mapped)

| Detour site (TU5 VA) | orig word | orig disasm | patched | real fn entry? |
|---|---|---|---|---|
| IsActive 0x826684C0 | 0x7D8802A6 | mflr r12 | `b 0x82C8A080` (48621BC0) | ✅ |
| ResolvePartWaitStates 0x825B6488 | 0x7D8802A6 | mflr r12 | `b 0x82C8A0C0` (486D3C38) | ✅ |
| ProcessConfig 0x8276FA08 | 0x7D8802A6 | mflr r12 | `b 0x82C8A8B0` (4851AEA8) | ✅ |
| RecalcGemList 0x82794740 | 0x7D8802A6 | mflr r12 | `b 0x82C8A590` (484F5E50) | ✅ |

- Cave hooks decode as valid prologues (e.g. `IsActiveHook: mflr r12; stw r12,-8(r1);
  stwu r1,-0x60(r1)`); their `bl` into the CRT block resolve to the TU5 savegprlr
  table (0x82829254/58/5C).
- Orig-trampolines decode `mflr r12 ; b target+4` → 0x826684C4 / 0x825B648C /
  0x8276FA0C / 0x82794744 (replicate the overwritten first instr, then re-enter the
  real body — so `*Orig()` calls behave).
- `verify` subcommand: all 675 words match toml; cave_region==cave.bin (0 mismatch,
  700 words); whole-file diff exact (592 changed words = writes, 83 zero-over-zero
  no-ops, 0 extra, header untouched).

---

## 7. Layer-A/B/C logic on TU5 (unchanged approach holds)

- **Layer A (IsActive)**: entry hookable (`mflr r12`); signature unchanged
  (`IsActive(int) const → bool` in r3). The hook wraps `IsActiveOrig` and forces
  `true` under the flag — a whole-function behaviour override, so the ~56% body
  divergence is irrelevant. Trampoline re-enters the real body.
- **Layer B (ResolvePartWaitStates)**: the C reimplements it (iterate 4 overshell
  slots, advance ChoosePartWait→ChooseDiff). All helpers pinned on TU5:
  GetBandUserFromSlot 0x82682B60, SetOvershellSlotState 0x8268BAF0 (state@0x20),
  UpdateAll 0x825B70D0; TheBandUserMgr 0x82E023B8. mOvershellState offset 0x20 and
  the state==11 gate byte-confirmed in the TU5 body.
- **Layer C (ProcessConfig)** + **centre (RecalcGemList)**: TWI offsets and SongData
  offsets byte-confirmed identical to base (§2e); the mGemList store offset 0x1c was
  byte-verified in the TU5 RecalcGemList body.

---

## 8. Caveats / follow-ups needing runtime or Ghidra-on-TU5

1. **Cave executability.** 0x82C8A000 is in `.data` (RW, not X). Xenia with
   `writable_code_segments = true` JITs guest code from the image regardless of
   `.data` NX, so it will execute there; this matches how the base cave (an unowned
   gap) ran. For a strict-NX / on-console target, a code cave in an EXECUTABLE
   section is needed — no file-backed executable zero run exists on TU5 (.text/BINK
   are dense). Options: (a) memory-only Xenia patch into the BINK/BINKBSS gap
   0x82C5E000 (executable, unowned, but not file-backable), or (b) locate .text
   slack / re-pad BINK via Ghidra-on-TU5. **Recommend a Xenia boot-spike**
   (`li r3,1; blr` @cave + `b cave` @0x826684C0) to confirm 0x82C8A000 executes and
   writes stick before trusting the full blob.
2. **`.data` cave safety.** 0x82C89DB8 zero run is unreferenced by `.text`
   (strong signal it is alignment padding), but a data-driven writer could still
   touch it. Ghidra-on-TU5 (add as a SEPARATE program; do not disturb the base on
   port 8002) would confirm no global is mapped there.
3. **TU5 basefile hashes** in the toml are empty (`TO-VERIFY`) — no Xenia on this
   machine. Fill from a Xenia boot log; the loader keys on title_id + is_enabled
   regardless.
4. **is_enabled = false** in the toml (safe default). The binary-patched xex already
   has the flag word = 1 and detours installed (it IS the enabled build).
5. Layer-A precise variant (drop only the RepresentSamePart rejection) and Layer-B
   participant-awareness remain Phase-2 items, unchanged from the base plan.

---

## 9. Base-patch note (not fixed here — flagged for the base team)

The RB3Enhanced `ports_xbox360.h` is a MIXTURE: the general ports (MemFree
0x827BC430, GetBandUserFromSlot 0x82682B60, TheBandUserMgr 0x82E023B8) are RETAIL-TU5
addresses, while the same-instrument block (IsActive 0x8264B5F8, etc.) holds
rb3-xenon-BASE addresses. Consequence: the BASE packer resolves MemFree /
GetBandUserFromSlot to TU5 VAs that are **mid-function on the base binary**
(base 0x827BC430 = `stw`, base 0x82682B60 = `stwu`). The base selftest does not
validate GAME_FN entry prologues, so this latent mismatch was not caught. It only
affects the clone-teardown (MemFree) and Layer-B (GetBandUserFromSlot) paths on the
BASE artifact; the TU5 artifact here uses correct TU5 VAs for both.
