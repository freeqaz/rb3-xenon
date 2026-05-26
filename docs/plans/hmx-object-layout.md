# Plan: correct `Hmx::Object` layout from 0x2c (dc3-derived) → 0x28 (RB3 retail)

**Impl agent model: Opus.** Foundational, high-blast-radius, binary-reconstruction
work. **Must run SOLO** — `Object.h` is included by 372 TUs and 198 subclasses;
no other source-editing agent (including the `src/system/math/*` track) may run
concurrently, or merge/rebuild races will produce false diffs.

## Background (verified, commit bb7f530 + this investigation)

RB3 retail `Hmx::Object` is **0x28 bytes**; our header (md5 `c6774ad9…`, **byte-identical
to dc3-decomp's** `Object.h`) models it as **0x2c** AND in a different member order.
dc3-decomp marks `system/obj/Object.cpp` **`Matching`** at 0x2c against *its own*
(newer) target — so this is a genuine RB3-vs-DC3 version divergence, exactly the
CLAUDE.md provenance caveat.

**rb3-Wii does NOT have the 0x28 layout** (verified: rb3-Wii `Object.h` md5 `331878c2…`
≠ ours; it is the *older* RB2-era layout — `class Object : public ObjRef`, inline
`TypeProps mTypeProps`, `std::vector<ObjRef*> mRefs`). So the fix is **NOT** "adopt
rb3-Wii wholesale." The 0x28 layout sits *between* the two siblings and **must be
reconstructed from the 360 binary**.

## 1. Target 0x28 layout — reconstructed from the binary

Ctor `lbl_82737FE8` (`build/45410914/asm/auto_03_82672130_text.s` ~L252628) and
dtor `fn_82738050` (same file, ~L252660), read directly. Object subobject base = `this`.

| Off  | Ctor store                              | Dtor evidence                                   | Member (current 0x2c name) |
|------|-----------------------------------------|-------------------------------------------------|----------------------------|
| 0x0  | `stw r9` = vtable `0x8201EE8C`           | rewritten to base vtbl `lbl_82000980+0xC`        | Object/ObjRefOwner vptr    |
| 0x4  | `stw r7` = vtable `0x8200E64C`           | `addi r3,this,0x4; bl fn_82263E30` (String dtor) | **`String mNote`** (vptr-first String; dtor frees mStr). spans 0x4..0xc |
| 0x8  | `stw r11`=0                              | (part of mNote)                                 | mNote.mStr                 |
| 0xc  | `stw r3` (self)                          | (part of mNote)                                 | mNote 3rd word             |
| 0x10 | `stw r11`=0                              | `lwz r3,0x10; if!=0 bl fn_82260570`(DataArray::Release) | **`DataArray *mTypeDef`** |
| 0x14 | `stw [0x82C411B0]` (default str ptr)     | `lwz r4,0x14; strlen; bl fn_82798278`(free)      | **`const char *mName`** (= gNullStr default) |
| 0x18 | `stw [0x82C411B0]`                        | —                                               | ptr member (mDir or mTypeProps-like) |
| 0x1c | `stw r11`=0                              | —                                               | ptr member (=0)            |
| 0x20 | ring self-loop (`stw r10=this+0x20` @0x20,@0x24) | `addi r3,this,0x20; bl fn_82451A48` (frees 0xc nodes, resets self-loop) | **`ObjRef mRefs` ring head** (next@0x20, prev@0x24) |
| 0x24 | (ring prev)                              | —                                               | mRefs.prev                 |
| 0x28 | **— no store —** (last store is 0x24)    | **— never touched —**                           | **DOES NOT EXIST**         |

**Hard facts (do not re-derive):** size = **0x28** (last ctor store at 0x24);
`mRefs` ObjRef ring is at **0x20** (next/prev self-loop; dtor `fn_82451A48` frees
0xc-byte ring nodes); a String (`mNote`) is at **0x4**; `mTypeDef` (DataArray*,
ref-counted release) at **0x10**; `mName` (char*, strlen+freed) at **0x14**; there
is **no member at 0x28** (dc3's `MsgSinks *mSinks // 0x28` is absent or folded).

**Still uncertain (impl agent resolves via objdiff iteration):** exact identity of
0x18/0x1c (likely `mDir`/`mSinks` or `mTypeProps`+`mDir`), and the exact 3-word
shape of the String at 0x4 vs a possible separate ring at 0xc. Reconstruct these
by diffing `Object.cpp`'s **own** target, not by guessing.

### Current 0x2c layout (for the diff)
`vptr@0x0, ObjRef mRefs@0x4 (0xc), mTypeProps@0x10, mTypeDef@0x14, String mNote@0x18,
mName@0x20, mDir@0x24, mSinks@0x28` → 0x2c. The target **moves `mRefs` to 0x20**,
**moves `mNote` to 0x4**, **drops one trailing pointer** (net −4).

## 2. The minimal edit (`src/system/obj/Object.h`, ~L1183-1207)

Reorder/resize the member block inside `class Object` so the C++ struct lays out as
the table above, dropping one 4-byte pointer to reach 0x28. Concretely (subject to
objdiff confirmation of 0x18/0x1c):

```cpp
String mNote;        // 0x4  (was 0x18)
DataArray *mTypeDef; // 0x10 (was 0x14)
const char *mName;   // 0x14 (was 0x20)
ObjectDir *mDir;     // 0x18 (was 0x24)
TypeProps *mTypeProps; // 0x1c (was 0x10)  -- OR mSinks; resolve by diff
ObjRef mRefs;        // 0x20 (was 0x4)
// mSinks: dropped to reach 0x28 -- confirm which trailing ptr is absent
```
Leave all `#ifdef HX_NATIVE` members/methods textually intact. `Object.cpp`'s ctor
init-list (`mTypeProps(nullptr), mTypeDef(nullptr), mName(gNullStr), mDir(nullptr),
mSinks(nullptr); mRefs.DetachSelf()`) needs editing **only** if a member is removed —
update the init-list and any `mSinks`/`Sinks()` uses accordingly.

## 3. Execution + verification sequence

1. **Confirm solo run.** No other agent editing `src/`. `git status` clean except this work.
2. **Pin `Object.cpp`.** Add `"system/obj/Object.cpp": "NonMatching"` to the `engine`
   block of `config/45410914/objects.json`. Add a `splits.txt` `.text` range covering
   the ctor/dtor (`0x82737FE8`..end of dtor `0x82738160`); widen to the full Object
   TU span if the cluster is known. Add ctor/dtor `fn_<addr>`→MSVC-mangled entries to
   `scripts/target_symbol_map.json` (`0x82737FE8` → `??0Object@Hmx@@QAA@XZ`,
   `0x82738050` → `??1Object@Hmx@@UAA@XZ`).
3. **`touch config/45410914/config.yml && ninja`** (forces re-SPLIT; emits
   `build/45410914/asm/Object.s` + target `obj/Object.obj` + compiled
   `src/system/obj/Object.obj`).
4. **objdiff the ctor first** (smallest, fully analyzed). Iterate header field
   offsets until `??0Object@Hmx@@QAA@XZ` is byte-equal, then the dtor. Use the
   store table above as the target oracle — each store offset is a member position.
5. **Verify no compile regression (blast radius).** `ninja` must rebuild all 372
   Object.h-dependent TUs with **0 new errors** (currently ~19 engine TUs are wired;
   confirm the count doesn't drop and no new failures appear in ninja output).
6. **Verify native build.** `cd native && cmake --build build` — must succeed and
   `./build/rb3-dta <songs.dta>` must still load headlessly. `native/src/*` reads
   Object via the engine headers (no hardcoded 0x2c offset constants were found in
   `native/src/`, but `SnapshotRing`/`NullifyAllRefs` in `Object.cpp` hardcode
   `kNextOffset = sizeof(void*)` and `kSentinelOffset = 3*ptr` relative to **ObjRef**,
   not Object — those are ObjRef-relative and unaffected by the Object reorder; re-confirm).
7. **Patchers / shims.** No script in `scripts/` hardcodes an Object offset; the 7
   obj-patchers operate on symbol names, not struct offsets — unaffected. Re-confirm
   by grepping `scripts/` for `0x28`/`0x2c`/`mSinks`.

## 4. Match verification & expected delta

After Object's ctor/dtor match: re-confirm `MasterAudio.cpp`'s
`SetupTrackChannel` (`fn_82759A78`, was **98.8%** — pure +4 member shift) jumps to
~100% (modulo unfixable ICF/reloc noise), and `SetupBackgroundChannel`
(`fn_82759BC0`, **93.4%**) loses its +4 shift (ternary control-flow + regswap remain
as separate work). Run `ninja`, read `build/45410914/report.json` /`progress.json`
for `matched_functions` — expect Object's 2 fns (+ likely several previously-blocked
MasterAudio member fns) to register. **Quantified:** the −4 correction unblocks
member-access codegen across **198 Object subclasses**; immediate measurable delta is
small (only pinned TUs diff), but it removes the single largest systemic blocker.

## 5. Rollback

This is **one commit**. If it regresses any compile or the native build:
`git revert <sha>` (or `git checkout -- src/system/obj/Object.h src/system/obj/Object.cpp
config/45410914/objects.json config/45410914/splits.txt scripts/target_symbol_map.json`),
then `touch config.yml && ninja` to restore. Do **not** force-push or hard-reset.

## 6. Risks

- **Native-build breakage** — highest-probability regression. The native engine
  exercises the ring/ref machinery heavily (`ReplaceRefs`, `SnapshotRing`,
  `NullifyAllRefs`). Moving `mRefs` to 0x20 changes `&mRefs` but those routines use
  ObjRef-relative offsets; still, re-run the headless `rb3-dta` smoke test.
- **Subclass offset assumptions** — any subclass or shim that hardcodes a post-base
  member offset (rather than using `this->member`) will silently shift. Grep
  `src/`/`native/` for literal `0x2c`/`0x28` near Object usage before finalizing.
- **vtable/RTTI emission** — reordering bases/members can change emitted vtable/`??_R`
  RTTI; the dtor self-diff and ctor self-diff catch this (watch the `??_7Object@Hmx@@…`
  vtable symbol).
- **STL container size** — `mNote` is a `String` (FixedString+TextStream, 0x8) not a
  std container, so no `sizeof` surprise; but confirm the at-0x4 String shape is 0x8,
  not 0xc, against the ctor stores (the 0x8/0xc stores must belong to mNote OR a
  distinct member — resolve by diff, do not assume).
- **0x18/0x1c identity** — if the dropped member is `mTypeProps` (not `mSinks`),
  `HasTypeProps`/`mTypeProps` code in `Object.cpp` breaks; confirm via the dtor
  (`fn_82260570` at 0x10 is `mTypeDef`; `mTypeProps`/`mSinks` release sites tell which
  survives) before deleting a field.

## References
- Target ctor: `build/45410914/asm/auto_03_82672130_text.s` `lbl_82737FE8` (~L252628)
- Target dtor: same file, `fn_82738050` (~L252660); ring-free `fn_82451A48`
  (`auto_03_82324884_text.s` L386543), String dtor `fn_82263E30`
  (`auto_03_82260000_text.s` L4970), DataArray::Release `fn_82260570` (L458)
- MasterAudio ctor cross-check (Object @ +0x8, HxAudio vptr @0x30): `MasterAudio.s` `fn_82758380`
- Current header: `src/system/obj/Object.h` L1149-1474; ctor/dtor `src/system/obj/Object.cpp` L90-119
- dc3 (0x2c, Matching): `../dc3-decomp/config/373307D9/objects.json` (Object.cpp = Matching), `../dc3-decomp/src/system/obj/Object.h` (identical to ours)
- rb3-Wii (older, different layout — NOT a drop-in): `../rb3/src/system/obj/Object.{h,cpp}`
- Post-mortem #2 (the 0x28 finding): `docs/plans/match-first-fn.md` L341-417
