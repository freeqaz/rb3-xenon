# W16-T — the `DataResultList` funclet lead, `FriendsProvider`, and the `Accelerometer` naming row

**Lane W16-T (Opus), 2026-09-14.** Worktree `~/tmp/wt-w16-t`, branch `w16-t`, off `main` at `94b0ddae`.
Takes the four source-shaped leftovers from W16-J's `W16D_BLOCKED_LIST_ESCALATION_2026-09-14.md`
(§"Deliberately NOT done"). Every number is read from `build/45410914/report.json` after a **full**
`./tools/ninja-locked` with `rc` tested from the log file, ruler **`name_check`** (`objdiff.json`
`options.functionRelocDiffs = name_check`, objdiff 4.2.8 `032122696555`), and every price is a
**set-diff of the `fuzzy == 100` row set**, never a Δcount and never a mismatch count.

**Lane baseline (build 1, full, rc=0):** `matched_functions` **43,272** / `matched_code` **3,960,812 B**
/ `matched_code_percent` 38.65732 / fuzzy 49.479626, `total_functions` 69,216, `total_code` 10,245,956.

---

## Item 1 — the two `DataResultList` funclets: **REFUTED. Our source is already correct; the charge is an objdiff funclet-PAIRING artifact**

**The lead as briefed.** W16-J's naming of `0x8250b510` = `??1DataResultList@@UAA@XZ` turned two funclet
call sites from forgiven into checked. Both read `fuzzy 99.5 / mpn 100`, 40 B:
`default/MetaPerformer` `fn_825800B0` (ours appeared to destroy a `Message` at frame `+0x84`) and
`default/RockCentral` `fn_824F9BAC` (ours appeared to destroy a `DataArrayPtr` at `+0x50`). The brief's
hypothesis: **a wrong local type in our port** — a real behavioural defect, the wrong destructor running
on unwind.

**That hypothesis is refuted on retail bytes. Our source declares the right type in both places.**

### The retail reading

An EH funclet unwinds one local of its parent, and MSVC emits it **inside the parent's own COMDAT**, so
the parent is the function contiguously preceding it. Both parents resolve exactly:

| retail funclet | retail parent (contiguous) | what the parent does with that slot |
|---|---|---|
| `fn_825800B0` (40 B) | `fn_82580068` (72 B) — a destructor | `stw r3, 0x84(r31)` stores `this`; then `addi r3,r3,0x1c; bl fn_8257FE60` destroys a sub-object at `+0x1c`; then `mr r3,r30; bl fn_8250B510` = `~DataResultList` on `this` |
| `fn_824F9BAC` (40 B) | `fn_824F9B68` (68 B) | `addi r3,r31,0x50; bl fn_8250B410` **constructs** a `DataResultList` local at frame `+0x50`, passes `&it` as arg 3 to `fn_824F9620`, then `addi r3,r31,0x50; bl fn_8250B510` destroys it |

The funclet reads **the very frame slot its parent writes** (`+0x84` written by `stw`, read by `lwz`;
`+0x50` constructed into, destroyed from) — so the parent attribution is not an inference, it is the
same slot on both sides.

### Our side — the matching funclets exist, with retail's exact bytes and retail's exact callee

Walking the COFF symbol tables of our own compiled objects (`__unwind$NNNNNN` symbols, each in the same
section as its parent's external symbol):

| our funclet | its section's parent symbol | its relocation | vs retail |
|---|---|---|---|
| `__unwind$366621` (MetaPerformer.obj) | `??1PendingDataInfo@MetaPerformer@@QAA@XZ` | `??1DataResultList@@UAA@XZ` | **9/9 words identical** to `fn_825800B0` |
| `__unwind$325531` (RockCentral.obj) | `?RecordDataPointNoRet@RockCentral@@SAXAAVDataPoint@@H@Z` | `??1DataResultList@@UAA@XZ` | **9/9 words identical** to `fn_824F9BAC` |

And **both parents already score `fuzzy 100.0 / mpn 100.0`** in `report.json`
(`??1PendingDataInfo@MetaPerformer@@QAA@XZ` 72 B = retail's 72 B; `?RecordDataPointNoRet@RockCentral@@…`
68 B = retail's 68 B). Under `name_check` a parent at 100 means its own `bl ??1DataResultList`
**relocation name** already agrees with retail's. There is no wrong local type to fix.

### What is actually charged: an ambiguous byte-signature equivalence class

objdiff pairs these anonymous funclets by **funclet byte signature** (the mechanism behind
`masked_equal`), not by name — our side has no `fn_<addr>` spelling for them. Within one signature the
pairing is a many-to-many assignment, and it is resolved arbitrarily:

- MetaPerformer, signature `subi r31,r12,0x70` + `lwz r3,0x84(r31)`: retail has **1** such funclet;
  **our object has 4** (two calling `??1Message`, one `??1_String_base`, one `??1DataResultList`).
  objdiff chose a `Message` one. `objdiff-cli diff` confirms the single charge:
  `bl ??1DataResultList@@UAA@XZ` (target) vs `bl ??1Message@@UAA@XZ` (base), `diff_arg`, **all 10
  instructions equal**.
- RockCentral, signature `subi r31,r12,0x90` + `addi r3,r31,0x50`: retail has **5**
  (`fn_823F0F5C`, `fn_824F8F24`, `fn_824F9BAC`, `fn_82508E28`, `fn_826D0F70`); our object has ≥5
  (three `??1DataArrayPtr`, one `??1String`, one `??1DataResultList`). **All five retail rows read
  99.5** — i.e. the assignment is wrong for **0/5 correct**, which is what an arbitrary assignment
  inside an equivalence class looks like, and is not a property of `DataResultList`.

### Size of the real class (measured, whole binary)

Rows at `mpn == 100` **and** `fuzzy < 100` — counted in `matched_functions`, withheld from
`matched_code`:

| class | rows | bytes | pp of `total_code` |
|---|---:|---:|---:|
| all `mpn==100 & fuzzy<100` | 3,366 | 204,884 | 2.0000 |
| of which **size 40 (EH funclets)** | 1,842 | 73,680 | 0.7191 |

(Next sizes: 44 B ×909, 48 B ×389 — the same funclet shape with one or two extra instructions.)

### Verdict and what was NOT done

**No source change is correct here, and two of the three obvious "fixes" would be harmful:**

- Changing our local's type to `Message`/`DataArrayPtr` to chase the pairing would **introduce** the
  behavioural defect the brief was trying to remove, and would break the two parents that are at 100.
- An alias `??1Message` ≡ `??1DataResultList` is forbidden by the brief and is alias fabrication: the
  two destructors are genuinely different code, so it would be forgiveness bought with a false claim.
- Un-naming `0x8250b510` would hide the charge without changing anything; W16-J named it deliberately.

The defect is in the **funclet pairing**, which lives in `../objdiff` (a tiebreaker on the relocation
target name within a byte-signature equivalence class would resolve it deterministically). That is a
shared-toolchain change and explicitly outside this lane's scope. Recorded here with its size so a
tooling lane can price it: **up to 73,680 B at 40 B alone, 204,884 B for the whole class**, unreachable
by any amount of source work.

**Δ measured: 0 (no change made).** The deliverable of this item is the refutation itself — it closes a
lead that would otherwise have cost an escalation lane a correctness regression.

---

## Item 2 — `FriendsProvider`: container type corrected, then the bodies written. **LANDED, `faacef80`**

### The retail reading
W16-J read `std::vector<int> unk2c` as wrong and it is. The proof does not rest on the oracle:

- both the dtor (`0x826662F0`) and `Reload` (`0x826662E0`, an 8-byte tail call) pass `this+0x2c` to
  `fn_8250CEE0`, which the target symbol map names
  `??$DeleteAll@V?$vector@PAVFriend@@V?$StlNodeAlloc@PAVFriend@@@stlpmtx_std@@@stlpmtx_std@@@@YAX…`
  — i.e. `DeleteAll<vector<Friend*, StlNodeAlloc<Friend*>>>`;
- **independently**, the dtor's own deallocation arithmetic (`subf; srawi r11,r11,2; slwi r3,r11,2`)
  gives a 4-byte element, corroborating the element size without reading a single name.

`std::vector<int>` and `std::vector<Friend*>` are both 12 bytes (`_Vector_base` = 3 pointers), so this is
a **pure type correction: no layout moves**, and `mpn` is arg-blind to it. It was landed on correctness,
not on the metric. Confirmed by reading `MusicLibraryStore.obj` that a plain `std::vector<T*>` in this
tree already mangles with `StlNodeAlloc`, so no allocator spelling was needed.

### What changed
- `src/band3/meta_band/FriendsProvider.h` — `std::vector<int> unk2c` → `std::vector<Friend *> mFriends`,
  with `class Friend;` forward-declared.
- `src/band3/meta_band/FriendsProvider.cpp` — **new**: ctor, dtor and `Reload` (both of the latter are
  `DeleteAll(mFriends)`). MSVC auto-generated the scalar deleting dtor and the `W3` adjustor thunk.
- `config/45410914/objects.json`, `config/45410914/splits.txt` — three-carve pin: a new
  `FriendsProvider.cpp` heading taking `0x826661CC-0x82666290` and `0x826662E0-0x8266643C`, leaving the
  interleaved `__linear_insert<FlowNode*>` COMDAT where it was in `UIList`, and pulling the scalar
  deleting dtor out of the head of `BandLabel`'s pin.
- `scripts/target_symbol_map.json` — three rows added (`0x826662f0` dtor, `0x826663f0` `??_G`,
  `0x82666288` `??_E…W3`).

### Predicted vs measured
Pre-registered **344–352 B**. Measured by set-diff, full build rc=0:
**CROSSED IN 9 rows / +520 B, FELL OUT 0**; `matched_functions` 43272 → 43278 (+6),
`matched_code` 3960812 → 3961332 (+520). No `BandLabel` row fell out.

The overshoot is fully attributed: four EH funclets crossed (44+44+40 B) plus **`default/UIList::fn_827F8DA4`
(40 B), which is unrelated to this change** and crossed only because the funclet byte-signature assignment
shifted — Item 1's mechanism running in our favour for once. `default/FriendsProvider` ends 9/9 functions,
480/520 B; its single sub-100 row is `fn_8266625C`, which is Item 1's class.

---

## Item 3 — `0x82529ae8` is **NOT** `ReceiveUpstreamAccelerometerResponse`. **Name withdrawn, `b0307de8`, Δ 0**

Checked `scripts/symbol_aliases.json` first, as the brief requires: **0 groups touch this address**
(`command grep -c -a`, not the ugrep shim). So the reloc-name finding is believable, and the bytes refute
the name three independent ways:

1. **Signature.** `0x82529ae8` is a 4-byte tail-call stub `b fn_82532378`. `fn_82532378` zeroes a 4-byte
   stack struct, stores `r4` and `r5` into it as **halfwords** at +0 and +2, and calls `fn_8283FB90` =
   `XInputSetState`. That is `XINPUT_VIBRATION{WORD wLeftMotorSpeed; WORD wRightMotorSpeed}`, so the real
   signature is `(int, WORD, WORD)` — three arguments, not the map's `(int, unsigned char*)`.
2. **Call sites.** `Joypad.s` builds two motor speeds (`li r5,0xff; rlwimi r5,r30,8`, i.e.
   `(byte<<8)|0xff`) before the call; `Joypad_Xinput.s fn_82531FF8` is a preset switch branching to it
   with `{0,0}`, `{0,0x6000}`, `{0,0xFFFF}`. Both are rumble, neither is an accelerometer response.
3. **The named function cannot live there at all.** Our
   `ReceiveUpstreamAccelerometerResponse(int, unsigned char*)` (`Joypad_Xbox.cpp:128`) is a `MILO_LOG`-only
   body, and the whole `MILO_*` family is `#ifdef HX_NATIVE`, which neither our match build nor retail
   defines. In retail it compiles to a bare `blr`, which `/OPT:ICF` folds with every other empty function
   — it could not be a distinct tail-call thunk.

**No replacement name was installed**, and that is deliberate. Neither oracle has one: DC3 has
`JoypadSetVibrate(int,bool)` / `JoypadVibrate(int)` (an enable **flag**, not a motor-speed setter) and
rb3-Wii has nothing; `XInputSetState` appears in DC3 only as an XDK header declaration. Inventing a
mangled name to collect 4 B would be fabrication, and the next lane would read it as evidence. Left
anonymous, exactly as W16-J left `0x827BBA68`.

**Predicted Δ 0** from the caller population: the row is 4 B at fuzzy 40 / mpn 40, so it contributes 0 to
both measures, and all three callers sit at fuzzy 0 — the wrong name was financed by nobody.
**Measured exactly 0**: CROSSED IN 0, FELL OUT 0, 43278 / 3961332 unchanged. The measurement is **not
vacuous** — `fuzzy_match_percent` moved 49.483063 → 49.483044 (−0.000019), which is precisely this row
shedding its false 40% partial credit, proving the edit took effect.

---

## Item 4 — the three small rows: **two collected byte-exact (`581b5b32`), one read and left**

All three read **fuzzy 0**, and none of them was a near-miss: in each case our object never defined the
symbol, so objdiff had nothing to pair. Two were **unresolved externals in our own tree**.

### 4a — `?Release@VertexBufferData@DxMesh@@QAAXXZ` (retail `0x82737F30`, 68 B) — class (i), COLLECTED
Declared in `rnddx9/Mesh.h` since the DC3 port and never defined, while the inline `~VertexBufferData()`
two lines above the declaration calls it. Retail bytes give the body with no ambiguity:
`lwz r4,0(r3)` (arg = `this->buffer`, the first word) · `addi r3, <global 0x82E04B38>` (`TheDxRnd`) ·
`bl ?AutoRelease@DxRnd@@QAAXPAUD3DResource@@@Z` (a real call — not inlined) · `li r11,0; stw r11,0(r31);
stw r11,4(r31)` (zero **both** words). That is `DX_RELEASE(buffer); size = 0;`, and it independently
confirms the `VertexBufferData` layout in `Mesh.h` (`buffer` +0, `size` +4).

### 4b — `??0Shuttle@@QAA@XZ` (retail `0x826BBC08`, 32 B) — class (i), COLLECTED
`Shuttle::Shuttle` was declared in `game/Shuttle.h` with no definition, so `new Shuttle()` at
`Game.cpp:185` was unresolved. Added `src/band3/game/Shuttle.cpp`.

It is a **separate TU on the evidence, not a header inline**: `Game::Game` reaches the ctor with
`bl fn_826BBC08` at retail `0x823623EC`, and with no LTCG in this build a 32-byte ctor visible as `inline`
could not have survived `/O1 /Ob2` uninlined at that call site. The same call site corroborates the class
without reference to the body — `li r3,0x10` before `operator new` is `sizeof(Shuttle) == 16`, and the
result goes to `stw r3,0xe0(r30)`, matching `Shuttle *mShuttle; // 0xe0` in `Game.h`.

Body: `li r11,0; stb r11,8; stw r11,0xc; lfs f0,lbl_82000D78; stfs 0; stfs 4`, and `lbl_82000D78` is
`.float 0` (read from `build/45410914/asm/auto_00_82000400_rdata.s`), so both floats are `0.0f`. The
integer stores landing before the float ones is scheduling around the `lfs` latency, not a different
initialisation order. Pinned by a three-carve of `FreestylePanel`'s `.text`; the first build failed at
SPLIT by design, dtk re-derived FreestylePanel's `.pdata` end `0x82233DA0` → `0x82233D90`, and the retry
passed. `Shuttle.cpp` correctly gets **no** `.pdata` — a frameless leaf touches neither stack nor LR, so
it has no unwind record.

### Predicted vs measured (items 4a+4b)
Pre-registered **+1 fn / +32 B** — I called the `Release` edit **inert**, because `rnddx9/Mesh.cpp` has no
entry in `objects.json` and emits no `Mesh.obj`. **Measured +2 fns / +100 B**:
`+68 B default/Rnd_Xbox::?Release@VertexBufferData@DxMesh@@QAAXXZ`,
`+32 B default/Shuttle::??0Shuttle@@QAA@XZ`, FELL OUT 0; 43278 → 43280, 3961332 → 3961432.

★ **The failed prediction corrects a reading I had already written down.** `Rnd_Xbox.cpp:983` is
`#include "rnddx9/Mesh.cpp"` — a **scatter-include** — so `Mesh.cpp`'s code compiles *into* `Rnd_Xbox.obj`.
That is why that object carried the undefined reference and why defining the body paired the row. It also
**refutes my earlier inference** that `Rnd_Xbox.obj`'s mixed contents (`DxMesh` + `DxCam` + `NgEnviron` +
`AccomplishmentManager`) proved unit membership there was linker COMDAT placement: it is scatter-includes
(`AccomplishmentManager.cpp` is included at line 977). **Before concluding "COMDAT soup" from a mixed
symbol table, check the bottom of the TU for scatter-includes.**

### 4c — `fn_82654440` (76 B, `default/SessionUsersProviders`) — identity PROVEN, **NOT collected**
This is the one the brief called unexplained. It is explained now; what blocks it is naming, not reading.

Retail body, in full:
```
lwz  r10, 0x2c(r3)      ; mUsers.begin()   -- SessionUsersProvider::mUsers is at +0x2c
slwi r9,  r4, 2         ; index * 4
mr   r11, r5            ; save incoming arg3 before r5 is overwritten
cmplwi cr6, r5, 0
lwzx r10, r10, r9       ; BandUser *u = mUsers[index]
lwz  r9,  0x4(r10)      ; vbptr at u+4
lwz  r9,  0x4(r9)       ; vbtable[1]  -> displacement of the 1st virtual base
add  r10, r9, r10       ; -> the virtual-base subobject
lwz  r5,  0x2c(r10)     ; arg3 = that base's +0x2c   (const OnlineID *)
bne  cr6, .L470
li   r4,  0             ; arg2 = NULL
b    .L480
.L470:
lwz  r10, 0x4(r11)      ; vbptr of the incoming pointer
lwz  r10, 0xc(r10)      ; vbtable[3] -> displacement
add  r11, r10, r11
addi r4,  r11, 0x4      ; arg2 = the LocalUser subobject
.L480:
lis/addi r3, lbl_82CC9D1C    ; ThePlatformMgr
b    fn_8251C960             ; TAIL CALL
```
`fn_8251C960` is named in the map:
`?ShowGamercard@PlatformMgr@@QAA?AW4ShowGamercardResult@@PAVLocalUser@@PBVOnlineID@@@Z`. So the function
is, in substance, `SessionUsersProvider::<something>(int index, <user> *u)` →
`ThePlatformMgr.ShowGamercard(u, mUsers[index]->mOnlineID)`. The two null-checked vbtable walks are the
signature of implicit conversions **to a virtual base** (MSVC guards such an upcast with a null test),
which is consistent with `mOnlineID` living in a virtual base of `BandUser` at +0x2c. Our tree already has
the idiom verbatim at `src/band3/meta_band/SongRecord.cpp:233`:
`ThePlatformMgr.ShowGamercard(user, oid);`. Its only caller is `OvershellSlot` at `0x8249BEDC`, next to our
existing `OvershellSlot::ViewUserGamercard(int)`.

**Why it was left.** The row is **anonymous** (`fn_82654440`) — collecting it needs a `target_symbol_map`
name as well as a body, and no oracle supplies one (`ShowGamercard` does not appear anywhere in rb3-Wii).
The identity is proven but the **spelling** — method name, argument types, constness — would be invented,
and an invented mangled name is charged by `name_check` at every call site and would be read by the next
lane as evidence. That is precisely the fabrication refused in Item 3, so it is refused here too, for
76 B. An escalation lane that can pin the name (e.g. from a DC3 twin of `SessionUsersProvider`, or from
the `OvershellSlot` handler symbol that reaches it) starts from a complete reading and needs only the
name.

---

## Lane ledger

| | `matched_functions` | `matched_code` |
|---|---|---|
| lane baseline (`94b0ddae`) | 43,272 | 3,960,812 |
| after Item 2 (`faacef80`) | 43,278 | 3,961,332 |
| after Item 3 (`b0307de8`) | 43,278 | 3,961,332 |
| after Item 4 (`581b5b32`) | **43,280** | **3,961,432** |
| **lane net** | **+8** | **+620 B** |

Item 1 contributes 0 by construction and is the lane's largest deliverable anyway: it closes a lead that
would otherwise have cost an escalation lane a correctness regression.

## Deliberately NOT done

- **Item 1 was not "fixed".** Retyping the two locals would *introduce* the defect the brief hypothesised
  and would break two rows currently at 100. An alias `??1Message` ≡ `??1DataResultList` and un-naming
  `0x8250b510` were both rejected as metric fitting. The real defect is objdiff's funclet byte-signature
  pairing, in `../objdiff`, out of this lane's scope — sized here at **73,680 B** (40 B rows alone) /
  **204,884 B** (whole class) so a tooling lane can price it.
- **No replacement name for `0x82529ae8`.** Withdrawing a false name is evidence; inventing a true one is
  not. Left anonymous.
- **`fn_82654440` not collected** (76 B) — reading complete, blocked only on an unfabricable name.
- **`rnddx9/Mesh.cpp` was not added to `objects.json`.** It reaches the build through
  `Rnd_Xbox.cpp`'s scatter-include, so the `Release` row was collected without touching the wiring.
  Giving it its own compile edge would be a much larger change and was not needed.
- **No permuter, no aliases added, no `symbol_aliases.json` edit**, and no change to any file outside the
  worktree. `symbols.txt` was never hand-edited.

## Gates (run last, after the final source edit, in the worktree)

1. full build — `rc=0` (`~/tmp/rb3_build_w16t_8.log`)
2. `python3 scripts/verify_ruler_agreement.py --check` — `rc=0`, both objdiff-cli entry points resolve
   the same ruler (`functionRelocDiffs = name_check`, `combineDataSections/combineTextSections = true`,
   `ppc.calculatePoolRelocations = false`, read from `report.json` `provenance.diff_config`)
3. `python3 scripts/verify_objs_patched.py --verify-manifest` — `rc=0`,
   `1212 decomp, 3095 target objects match` (`tree_sha256=e5eb7d44b264f0d3`)
4. `tools/native_build_gate.sh` — `rc=0`:

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```
