# W16-OO — sub-100 rows in `src/system/{os,utl,obj,meta,ui,math,synth,midi,flow,track}` (2026-10-03)

**Branch** `w16-oo`, started off main `e5586e330`, rebased onto `2d4643b0b` (the W16-ON merge) with no conflicts.
**Ruler** `name_check` (graded, from `report.json` `provenance.diff_config`). The permuter was not run.
No alias was added or removed: `git diff main w16-oo -- scripts/symbol_aliases.json` is empty, so there was
nothing for `icf_pair_adjudicate.py --chase` to adjudicate. `src/network`, the Quazal block and
`band3/bandobj/char/world/beatmatch` (lane W16-ON) were not edited.

## 1. Population

Rows with `fuzzy < 100` whose unit's `base_path` (from `objdiff.json`) lies under the ten directories, on main's
`report.json` at `e5586e330`: **642 rows / 125,144 B**. That reproduces the brief's "about 120 KB".

| dir | rows | bytes |
|---|---:|---:|
| synth | 120 | 25,120 |
| os | 90 | 20,776 |
| utl | 109 | 18,456 |
| obj | 65 | 17,996 |
| ui | 84 | 13,460 |
| math | 34 | 11,440 |
| meta | 45 | 6,144 |
| track | 24 | 5,428 |
| midi | 37 | 3,164 |
| flow | 34 | 3,160 |

Charge classes (W16-NA's classifier, `~/tmp/w16na/cls.py`): STRUCT_INSDEL 404 / 76,132 · REG_ONLY 34 / 15,512 ·
NAME_ONLY 90 / 9,956 · IMMEDIATE 83 / 9,688 · OPCODE:replace 13 / 5,252 · NAME+REG 8 / 4,880 · STACK_REG 4 / 2,332 ·
other 5 / 1,392.

Most of the large rows were already opened by W16-NA/ND/NL/NS (scheduling/register residue: `ObjectDir::Save`,
`UIListDir::BuildDrawState`, `Locale::Init`, `Song::SyncState`, `PlatformMgr::Poll`, `EQEffect::SetParameter` …).
This lane worked, in order: W16-OL's one-sided-block scan (35 rows with a ≥3-instruction run only one side has);
a scan for retail-only `bl Release@Object` against our `SetObjConcrete` (the open-coded null release); the
charged pairs of every NAME_ONLY / IMMEDIATE / STACK_REG row with ≤ 8 charges; and the unmentioned
STRUCT_INSDEL rows with ≤ 8 charges.

## 2. Whole-binary A/B

`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16oo-ab --patch <git diff main w16-oo>` on a fresh
`scripts/setup_worktree.sh` worktree at main `2d4643b0b`. Patch kinds: map + source + splits, 9 paths. Forced
re-split on both legs; both read at a `symbols.txt` fixed point after 0 extra splits. Run dir
`~/tmp/wt-w16oo-ab/.ab_measure_runs/20261003-033040-branch-2174282/`, `rc=0`, tree restored and verified by the tool.

```
leg A: matched=53220 masked=25132 honest=28088 code%=57.320602  (recompiles: 0, settled)
leg B: matched=53224 masked=25132 honest=28092 code%=57.324430  (recompiles: 105, split=1, patch_steps=7, settle iterations: 2)
Δmatched=+4  Δmasked_equal=+0  Δhonest=+4  Δcode%=+0.003828pp  Δcode_bytes=+392
Δfuzzy=+0.001647pp   (legA 63.541973 -> legB 63.543620)
unit improvements: MemMgr 25->26, MidiInstrument 111->112, StreamReceiver 10->11, MidiInstrumentMgr 13->14
unit net (ALL units) = +4   vs whole-binary Δmatched = +4   (no unit regressions)
units at 100% [mpn]: 523 -> 524 (MidiInstrumentMgr, MATCHED_ROSE); pairable units 1733 -> 1732
```

**Prediction, written before the run:** the in-tree row diff against main's `report.json`: **+4 fns / +392 B**,
0 rows down. **Measured: +4 / +392 B, identical.** I also predicted Δhonest +3, expecting the StorePanel funclet
to count as masked_equal; it measured **+4** (Δmasked_equal 0), so that guess was wrong. Leg A equals main's own
`report.json` (53,220 / 5,874,096 B). The `none` control reads +192 B and is NOT_APPLICABLE, because the patch
carries source. Pairable units −1 is the vendor `auto_03_82BC6C04_text` unit folding into its neighbour once the
MemMgr pin is gone.

The fuzzy-only gains are larger than the byte count: ThreadMemStack +13.4 pp, MemTruncate +9.4, MemFree +9.7 and
CheckOut +9.1 move rows that `.bss` order (§5.6) or the missing player id (§5.1) still hold below 100.

## 3. What changed, and why (each is one commit)

Behaviour and layout fixes:

| row(s) | B | before → after | defect, read off retail bytes |
|---|---:|---|---|
| `ThreadMemStack` | 348 | 71.60 → 85.05 | Retail addresses the null stack, `gThreadBuf`, the current index and `gNumThreads` off **one** `.bss` anchor (0x82E069A8 +0/+0x48/+0x440/+0x444): internal statics. Ours were externals. `gThreadIds` is `.data` at 0x82C78E0C initialised `{ -1, 0, 0, 0, 0, 0 }`; ours was zeroed `.bss`. Retail also holds `gMemStackLock` with a **`CritSecTracker`** (EH frame `subi r31,r1,0xa0`, unwind funclet 0x827BB9F4, one shared `Exit` tail); ours called Enter/Exit by hand. |
| `MAX_BUF_THREADS` | — | Δ0 | **6** in retail, not DC3's 32: `gThreadIds` is 6 words (sDefaultHeap follows at 0x82C78E24) and `gThreadBuf` fits 6 × 0x48 between the null stack and `gHeaps`. Capacity fix; the metric does not see it. |
| `MemTruncate`, `MemFree` | 284 + 188 | 87.73 → 97.10, 87.77 → 97.45 | Retail built **MemHeap and MemMgr as one TU** (already recorded in MemHeap.cpp). Without `MemHeap::Free`/`Truncate` visible, our compiler reloads `gNumHeaps` after every call; retail keeps it in a register. Match build only: MemMgr.cpp now includes MemHeap.cpp's method bodies (`RB3_MEMHEAP_METHODS_ONLY` skips its reunification and scatter-include tail). **Predicted before the build** (the reload disappears) and measured. |
| `MemCurrentHeap` | 104 | 60.77 → 100 | Retail reads the top of the heap stack in place (one `ThreadMemStack(false)`, no `bl GetCurrentHeapNum`). This compiler will not inline `GetCurrentHeapNum()` there; moving its definition first was **measured inert**. Same behaviour, spelled out. |
| `StorePanel::CheckOut` | 260 | 46.60 → 55.66 | Ours called the non-virtual `StoreProfile()` placeholder, which **returns NULL**, and dereferenced it. Retail calls `StoreUser()` (vslot 0x44) and `LocalUser::GetPadNum` (slot 0). See §5 for the half not written. |
| `MidiInstrumentMgr::UnloadInstrument` | 88 | 69.09 → 100 | Retail 0x82716248 open-codes `mInstrument = 0` as `if (mObject) { Release(this); mObject = 0; }` (`ObjPtr::ReleaseObjConcrete`). |
| `StreamReceiver::WriteData` | 84 | 99.76 → 100 | Retail copies with `XMemCpy`, as Poll's overflow copy in the same TU already does. |
| `??_GMidiInstrument` | 76 | 99.74 → 100 | Retail's deleting dtor calls `MemFree` directly: `OBJ_MEM_OVERLOAD_INLINE_DEL`. |
| `fn_827B4E2C` (StorePanel funclet) | 40 | 99.5 → 100 | follows CheckOut |

Map / splits:

| change | effect |
|---|---|
| **0x82BC6B70 is not `::operator delete`.** Dropped the map name `??3@YAXPAX@Z` and the MemMgr `.text` pin 0x82BC6B70–0x82BC6C04 (lane AD's map+splits fixpoint `6c48bb674` pinned it on that name). | Retail bytes: a locked bit-pool free off 0x82E4CEF0 with **177 callers, all vendor** (0x82BC1364–0x82C0F804, beside `LEAPFX::CAudioADPCM`). Global `operator delete` is retail's 4-byte `b MemFree` at 0x8240DDB0, already a proven folded member of the `??3BinStream@@SAXPAX@Z` group, so the map named one spelling at two addresses. dtk drops the derived `.pdata` line; the block returns to `auto_03_82BC46B8_text`. `total_code` unchanged (10,247,792); the MemMgr row (148 B at 2.57%) was never matchable. |

Codegen-neutral correctness: `synth/SynthSample.cpp`'s `SampleAlloc` is now file-static. Two TUs defined an
external `?SampleAlloc@@YAPAXH@Z` with different bodies; the external one is synth_xbox's physical allocator
(retail 0x82B6C7A0, `PhysicalAllocTracked(size, 4, "SampleData(phys)")`). The generic `MemAlloc(size, 0)` helper
is the one retail ICF-folds into `operator new` (0x827BD2F0, `li r4,0; b MemAlloc`). Δ0.

### 3.1 Side effect on `scripts/symbol_aliases.json` (not edited)

`tools/icf_alias_finder.py --validate` passes on both trees with 0 contradicted (main: 1798 map-consistent /
310 tolerated; branch: 1798 / 311). The difference is group `0x8240ddb0` (`??3BinStream@@SAXPAX@Z`). On main it
was **CONTRADICTED and exempted**, and its `contradiction_exempt` text names this exact defect ("the
`??3@YAXPAX@Z` row at 0x82bc6b70 (177 callers) is the vendor CRT delete — a genuine map NAME COLLISION").
With the name dropped, only the survivor is named in the target objects. The group now classifies honestly as
TOLERATED STALE_SPELLING (its junk `CharEyeDartRuleset`/`OggFree` spellings), so **the exemption is inert and
can be deleted**. It was left in place so this lane's alias diff stays empty.

## 4. Rows that went down

**None** on the final tree (in-tree row diff against main's `report.json`, `~/tmp/w16na/rowdiff.py`).
One intermediate regression was found and fixed: compiling MemHeap's bodies into MemMgr dropped ThreadMemStack's
unwind funclet `fn_827BB9F4` 100 → 99.9. That exposed ThreadMemStack's missing `CritSecTracker` (the funclet's
`subi r31, r12, 0xa0` is retail's EH frame), and the fix restored it.

## 5. Tried, reverted, or left (negative results)

1. **`StorePanel::CheckOut`'s player id.** Retail's last `XboxPurchaser` argument is
   `TheNet.mServer && mServer->IsConnected() ? mServer->GetPlayerID(pad) : 0` (Server vslots 0x14 / 0x1c,
   `Net::mServer` at 0x34, compiler-verified). `net/Net.h` cannot be included from this engine TU: it pulls
   band3's `meta_band` headers (`Difficulty` redefined; `obj/ObjMacros.h` replaces the handler macros, so every
   `HANDLE_ACTION` symbol goes undeclared). `src/network` is out of scope, so a lighter header could not be
   added. The id is still 0; the exact sequence is written next to the body.
2. **`??_GTrackWidgetImpBase` / `??_GMatWidgetImp` / `??_GImmediateWidgetImp` (68–76 B, 99.7).** Retail calls the
   class `operator delete` stub (folded to 0x8240DDB0); ours inlines it to `bl MemFree` although the classes
   declare the `noinline` `DELETE_OVERLOAD`. The compiler listing confirms the inline. Adding `NEW_OVERLOAD` to
   `TrackWidgetImpBase` (TrackWidget, which has both, is 100) did **not** move the `??_G` rows and dropped
   `TrackWidget::SyncImp` 100 → 97.7. Reverted.
3. **`UsbMidiGuitar::Poll` (1,244 B, 4 charges).** The program-change sum `(b0xa>>7) + (b0xb>>6&2) + (b0xc>>5&4)`
   loads the two masked bytes into swapped registers. Reordering the terms and grouping them were both inert.
   Register assignment; reverted.
4. **`Archive::GetFileInfo` (432 B).** Retail returns a literal `1` and lets the not-found path fall into the
   shared `return false` tail. Dropping our inner `return false` fixed the true path but merged two tails
   retail keeps apart (97.92 → 97.13). Testing not-found first was worse (94.68). This is tail-merge layout,
   the residue W16-NL recorded; reverted.
5. **`NewFile` (424 B, 72.2).** Already recorded in the source: deleting the `gNullFiles` branch costs −4 fns
   (it is our only instantiation of NullFile's vtable). New fact: **no vtable word in the retail image points at
   0x82347528**, the address the map calls `NullFile::Write`, so that map name is itself suspect. Left.
6. **The remaining MemMgr/MemHeap residue is `.bss` order.** Retail's one-TU layout is null stack @0x82E069A8,
   `gThreadBuf` +0x48, `gHeaps` +0x200, current index +0x440, `gNumThreads` +0x444, `gTimeStamp` +0x448 (MemHeap
   Init's 0x82E06DF0), two unknown words, `gNumHeaps` +0x454, `gCheckConsistency` +0x458, an unknown word, then the
   externals `gMemLock` / `gMemStackLock`. Ours is index, numHeaps, threadBuf, heaps, … null stack. It is neither
   declaration order nor its reverse, and three retail names are unknown, so it was not fitted. That ordering
   keeps MemFree / MemTruncate / MemAllocSize / ThreadMemStack / MemFindHeap / MemPrintOverview / MemInit off 100.

## 6. Leads left (not done)

- **`vector<FlowMathOp>` rows at 0xC stride** (`_M_insert_overflow_aux` 328, `resize` 128, `_M_fill_insert` 112,
  `__uninitialized_fill_n` / `__uninitialized_copy` 96 each, UIList's `_M_allocate_and_copy` 96). This is not a
  struct defect: `_Param_Construct<FlowMathOp>` matches 100% at our 0x34 layout, and retail's 0xC bodies call
  `_M_erase<ObjOwnerPtr<Waypoint>>` (a 12-byte element). They are other 12-byte vectors misnamed `FlowMathOp`, so
  this is identification work.
- **InlineHelp dtor / `SyncLabelsToConfig` (340 + 308 B).** Retail deletes the label vector's elements through a
  fixed subobject at +0x214; ours goes through the vbptr to `Hmx::Object` at +0x218 (the vtordisp sits at +0x214).
  Our UILabel layout is **compiler-verified identical to retail** (size 0x24C; `??1UILabel` and `??_GUILabel` at
  100). So the retail element type is not the UILabel we declare, and needs identification.
- **Misnamed map rows found** (renaming needs an identification first):
  - `??0NetLoaderRef@@QAA@ABU0@@Z`: retail's body is a `list<T>::erase(iterator)` on 0x14-byte nodes.
  - `ResMgr<void>::Get`: retail's body is a lookup plus a byte decrement in a 0x1000-stride table, returning 1.
- **World code compiled into `obj/`/`synth/` units** (`PropSync.cpp` includes `world/Dir.cpp`, and MidiSynth
  includes PropSync): `operator>>(BinStream&, WorldDir::BitmapOverride&)` (380 B) has retail's open-coded
  `if (mObject) { Release; = 0 }` where ours calls `SetObjConcrete`. That is `ReleaseObjConcrete()` in
  `world/Dir.cpp` (W16-ON's directory). Also `??0WorldDir` (1,024 B) and `??0MatOverride` (92 B).
- **`Hmx::Object::AddRef`/`Release` (252 B)** remain the retype + alias adjudication recorded in
  `OBJECTCPP_TU_2026-09-11.md` §5.
- **Pointer-element `_M_fill_insert` family** (Synth::Init 840, VorbisReader::Poll 784, StandardStream::Init 672,
  ArkFilesInit 540, BlockMgr::Init 344, UIGridProvider 340, StreamNull 288, TrackDir::PollActiveWidgets 260,
  UIListSlot::StartScroll 128): each row's only charge is `vector<T*>::_M_fill_insert` against retail's
  `vector<Object*>` survivor. This is the cycle-assumed-leaf class W16-JE/W16-OK parked on purpose. Not aliased.
- **TU5/DX-only code with no source** (W16-MC already classed these): `fn_82C40098` (1,372 B) is the dynamic
  initializer of a song-update hash table (`./songs/updates/<song>/…_update.mid` plus hash words at 0x82C66408),
  storing `gNullStr` into ~340 slots; plus the OggMap mogg-decryption rows.

## 7. Gates

(filled in at the end)
