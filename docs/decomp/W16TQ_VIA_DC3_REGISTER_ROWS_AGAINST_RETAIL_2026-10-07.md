# W16-TQ: 49 VIA-DC3 register/reorder rows, read against retail

Lane W16-TQ, 2026-10-07, worktree `~/tmp/wt-w16tq`, branch `w16-tq`, base main `44f33e78b`.
Brief: `CAMPAIGN_STATE_2026-10-07c.md` §6 lever 1, VIA-DC3 part = §5 bucket **30** ("in W16-PU/QF
population, register/reorder class, no per-row read", 49 rows / 47,276 B). Executed rows first, then by
size. This is the VIA-DC3 twin of W16-TO, which read the in-scope part.

Ruler: the shipped graded `name_check`. Per-row numbers are `fuzzy_match_percent` / `match_percent_normalized`
from this worktree's `report.json` after a full `tools/ninja-locked`.

**Result: 49 / 49 EQUAL in behaviour. No source change is kept, so the branch carries this doc only.**
One alias finding for a name lane is recorded (§4.6); I did not edit any alias.

## 1. The population

The 49 rows are the rows of `~/tmp/w16tn-gap/dispo_via_rows.json` whose `dispo` starts `30 `. I re-derived
the count and bytes instead of copying them: 49 rows / 47,276 B, as §5 of 10-07c states. "Executed" is the
`count` column of `~/tmp/w16tn-gap/rank.tsv` (W16-TN's `native_runtime_rank.py` run), joined on the mangled
name. That gives **4 rows / 4,740 B executed**, the same four the brief names: `RndParticleSys::Load` (1,950
entries), `UILabel::LabelUpdate` (886), `Normalize` (Line, 327), `NgEnviron::Select` (2). 8 rows are
reorder-shaped (I1/I2), 29 are at `mpn` 100.

Baseline check before reading anything: the worktree built clean (`BUILD_RC=0`, `[pairing] 1056/1056`,
`[patch-state] OK: tree is a fixed point of 6 post-compile passes`). Its `report.json` equals 10-07c §1 on
the headline keys (`matched_code` 6,078,232, `matched_functions` 54,939). All 49 rows read the same `fuzzy`
as W16-TN's census (0 of 49 differ).

## 2. Method

W16-TO's method and tools (`~/tmp/w16to/`), copied to `~/tmp/w16tq/` and repointed at this worktree's
`orig/45410914/band.exe` (sha1 `5f3f667a…`, the clean TU5 PE). One `objdiff-cli diff --full-listing` per row
(graded ruler, no `--build`), then:

1. **Calls.** Multiset of `bl` targets per side. A differing callee that the ruler does not charge is an
   installed fold and was not looked at further. Placeholder callees (`fn_`), which the ruler forgives, were
   read by hand (§4.7).
2. **One-sided loads/stores, immediates and float constants.** Two rows had a one-sided float constant;
   both are named statics on our side (§3).
3. **String literals, by content, every line.** TO's `sympair.py` pairs symbols only on aligned lines, so a
   literal on an insert/delete line was never checked. I added a whole-listing check: the multiset of
   retail string bytes against our decoded `??_C@` names, per row. 70 literals; all agree (the two
   apparent misses were my length filter dropping retail's `""` at `0x82000C55` and `"0"` at `0x820730A8`).
4. **Value flow.** TO's `symeval6.py` had blind spots that showed up on these rows, so I hardened it. The
   result is `~/tmp/w16tq/symeval7.py` (64-byte by-reference window) and `symeval8.py` (16-byte window):
   - **`std` was not tracked and stack homes were compared as stores.** A float conversion goes
     `std rX → lfd → fcfid` through a stack slot, so every renumbered slot read as a value difference
     (`EvaluateChannel` 6, `RndPostProc::Save` 4). Stack-slot stores now update memory but are not
     compared.
   - **That alone would hide a wrong value passed by reference**, e.g. a stack `Vector3` handed to
     `Normalize(const Vector3&, Vector3&)`. So at each call, the contents of every stack object whose
     address is an argument are now part of the compared call. A slot an address-taken call may have
     written reads back as `('out', call, arg, offset)`, which does not depend on where the slot sits.
   - **A symbol's base register was part of its value** (`__real@3f800000@l(r9)` vs `(r10)`): dropped.
     `clrrwi rX,rY,0` is a copy. 64-bit `ld` and the update-form loads `lbzu/lhzu/lwzu` were untracked:
     now loads.
   - **The window size is a trade-off, so both are run.** 64 bytes catches a large by-reference temp
     (`XboxEnumeration::Poll`'s ~0x28-byte `EnumProduct`) but lets a neighbouring object's call output
     leak into a slot's name (`SetupXSection`). 16 bytes is the reverse. A row is cleared only when it is
     clean under **both** or was settled by hand. The flagged set is the same 13 rows under both.

   *Controls* (`~/tmp/w16tq/ctl.py`, one mutation in our column, `ctl_*.mdx`). Each must read a difference
   on a row that reads clean unmodified:

   | mutation | row | v7 (64 B) | v8 (16 B) |
   |---|---|---|---|
   | FP operand `fmuls f0,f13,f0` → `f13,f13` | `Normalize` | store-diff 1 | store-diff 1 |
   | compare immediate `cmpwi 1` → `2` | `NgEnviron::Select` | store-diff 1 | store-diff 1 |
   | call argument `li r4,1` → `0` | `UILabel::LabelUpdate` | 1 call | 1 call |
   | value behind a by-reference stack temp | `RndPostProc::Save` | 4 lines | 4 lines |
   | source of a stack conversion slot (`std r11` → `std r9`) | `EvaluateChannel` | store-diff 2 (baseline 1) | same |
   | 64-bit load offset `ld 0x0` → `0x8` | `XboxEnumeration::Poll` | 1 call | **0** |

   The last row is why both windows are required: v8 alone misses it, and the union catches all six.
5. **Then the residue was read by hand.** As in TK and TO, a charge that names no source construct is
   recorded as SCHED / HOME / LAYOUT / ANCHOR / NAME and not fought.

Classes (TO's, plus ANCHOR):
- **SCHED:** register, operand order, scheduling, loop-IV base or an extra copy. No call, store, compare or
  constant differs.
- **HOME:** a stack temp at another slot, same contents.
- **LAYOUT:** block placement or tail-merge only.
- **ANCHOR:** a file-scope static placed at another offset from the shared base; each variable is
  accessed consistently.
- **NAME:** the only other charge is a callee name of a fold group.

## 3. Result

What the evaluator reported after §2's fixes, and how each item was settled:

| row | what the evaluator reported | settled by |
|---|---|---|
| `CharCollide::Deform` `PathName`, `CharIKHand::IKElbow` `WorldXfm_Force`, `LabelNumberTicker::Poll` `UpdateDisplay`, `Impl::Draw` and `Spotlight::Copy` `SetObjConcrete`, `ConsumeData` `WriteData`, `InitInfo` `Symbol` ctor, `StoreOffer` `FindData` | args r7–r10 / f1–f4 differ | beyond the callee's arity: dead registers |
| `RndPostProc::Save` | `WriteEndian`'s r4 = `0x58`/`0x54(r1)` vs `0x5c(r1)` | HOME; the by-reference contents are equal |
| `DecodeThreadEntry` | `gReaders`/`gNewReaders` swapped | role-equal (§4.4) |
| `LightPreset::Load` | `sLoading`-based offsets differ | ANCHOR (§4.5) |
| `CharBonesSamples::EvaluateChannel` | `out[0]` stored as a lerp vs a load | linear-path artifact; per-arm read equal (§4.2) |
| `NgSpotlightDrawer::SetupXSection` | call arguments to calls 8–13 | stack layout; inputs and outputs equal (§4.1) |
| `Spotlight::BuildNGSheet` | constant 1.0 retail-only | retail `lbl_82C7120C` = 1.0 = our `kSheetFade` (`Spotlight.cpp:1545`) |
| `Synth::DrawMeterScale` | constant 0.2 retail-only | retail `lbl_820F4C60` = 0.2 = our `sMeterLayout[0]` (`Synth.cpp:380`) |
| `CharCollide::Deform` | one-sided `addi 0xc`/`subi 0x14` vs `0x8`/`0x10`, and an extra `fmr` | both give base `r5−8`; SCHED (§4.3) |

| # | row | size | fuzzy | mpn | exec | charges | verdict |
|---:|---|---:|---:|---:|---:|---|---|
| 1 | `RndParticleSys::Load` | 2,436 | 99.43 | 99.67 | 1,950 | 19 reg, 1+1 ins/del | EQUAL (SCHED: one `fmuls` two slots later; `SetObjConcrete<BandCharacter/CharLipSync>` vs `<RndTransformable/RndMat>` are uncharged folds) |
| 2 | `NgEnviron::Select` | 1,756 | 99.37 | 99.52 | 2 | 7 reg, 12 off, 1+1 ins/del | EQUAL (SCHED: the same eight fields stored and three loaded in another order; `li r29,2` moved; `push_back<ChatReceiver*>`/`<NgLight*>` uncharged fold) |
| 3 | `UILabel::LabelUpdate` | 464 | 99.57 | 100 | 886 | 10 reg | EQUAL (SCHED: r27/r28, W16-NS; setter/getter folds, W16-QF) |
| 4 | `Normalize` (Line) | 84 | 99.52 | 100 | 327 | 1 reg | EQUAL (SCHED: `fmuls` operand order) |
| 5 | `LightPreset::Load` | 3,024 | 99.71 | 99.71 | 0 | 17 off, 1+1 ins/del | EQUAL (ANCHOR, §4.5) |
| 6 | `CharIKHand::IKElbow` | 2,880 | 97.92 | 98.89 | 0 | 67 reg, 4+4 ins/del | EQUAL (SCHED: FP ops and one `cmplwi` moved; 62 calls equal) |
| 7 | `CharHair::SimulateInternal` | 2,472 | 99.95 | 100 | 0 | 3 reg | EQUAL (SCHED) |
| 8 | `CharLookAt::Poll` | 2,268 | 99.93 | 100 | 0 | 4 reg | EQUAL (SCHED: FP commutative, W16-RD) |
| 9 | `NgSpotlightDrawer::SetupXSection` | 2,104 | 76.45 | 79.24 | 0 | 94 reg, 115 off, 46+48 ins/del | EQUAL (SCHED + HOME, §4.1) |
| 10 | `CharIKFingers::SetName` | 2,076 | 99.23 | 99.23 | 0 | 2+2 ins/del | EQUAL (SCHED: two string `addi` one slot later; `"bone_R-index03.mesh"`, `"spot_R-ringfinger_tip.mesh"` equal by bytes) |
| 11 | `CharSleeve::Poll` | 1,980 | 99.80 | 100 | 0 | 10 reg | EQUAL (SCHED: FP commutative, W16-PU) |
| 12 | `CharEyes::LidTrackAndClampingUpdate` | 1,764 | 99.32 | 99.55 | 0 | 15 reg, 2 sym+reg, 1+1 ins/del | EQUAL (SCHED: 1.0/0.0 into f22/f21 swapped, one `fneg` moved) |
| 13 | `CharBones::ScaleAdd` | 1,756 | 99.95 | 100 | 0 | 2 reg | EQUAL (SCHED: FP commutative, W16-RD) |
| 14 | `CharBones::RotateTo` | 1,644 | 99.76 | 100 | 0 | 15 reg | EQUAL (SCHED) |
| 15 | `MetaMusic::UpdateMix` | 1,572 | 99.85 | 100 | 0 | 6 reg | EQUAL (SCHED: which product `fmadds` fuses, W16-NA; the evaluator is contraction-blind) |
| 16 | `CharCuff::DeformMesh` | 1,364 | 99.40 | 99.98 | 0 | 19 reg, 6 off | EQUAL (SCHED: stack fields `0x84`–`0x98` loaded in another order) |
| 17 | `Spotlight::BuildNGSheet` | 1,232 | 98.56 | 99.34 | 0 | 17 reg, 4 off, 1+1 ins/del | EQUAL (SCHED: one `Vector3` loaded/stored component-permuted; constant §3) |
| 18 | `CamShot::Shake` | 1,148 | 99.86 | 100 | 0 | 4 reg | EQUAL (SCHED) |
| 19 | `RndPostProc::Save` | 1,056 | 99.97 | 99.97 | 0 | 4 off, 4 slot `addi` | EQUAL (HOME: `WriteEndian` temp at `0x5c` vs `0x58`/`0x54`) |
| 20 | `CharCollide::Deform` | 1,052 | 98.88 | 99.58 | 0 | 14 reg, 5 off, 5 imm, 1 ins | EQUAL (SCHED, §4.3) |
| 21 | `StoreOffer::StoreOffer` | 948 | 99.68 | 100 | 0 | 15 reg | EQUAL (SCHED) |
| 22 | `CharBonesSamples::EvaluateChannel` | 944 | 85.77 | 87.10 | 0 | 35 reg, 24 off, 14+15 ins/del | EQUAL (SCHED + LAYOUT, §4.2) |
| 23 | `CamShot::SetPos` | 920 | 99.87 | 100 | 0 | 3 reg | EQUAL (SCHED) |
| 24 | `StandardStream::InitInfo` | 872 | 98.78 | 99.06 | 0 | 7 reg, 4 other, 1+1 ins/del | EQUAL (NAME + SCHED: `"max_slip"` equal by bytes; `_M_fill_insert<Object*>` vs `<ChannelParams*>` is W16-OX's pointer fill_insert fold, settled not admissible) |
| 25 | `LightPreset::Animate` | 772 | 99.84 | 100 | 0 | 3 reg | EQUAL (SCHED) |
| 26 | `Spotlight::Copy` | 716 | 99.64 | 100 | 0 | 10 reg | EQUAL (SCHED; placeholder `fn_8230C628` §4.7) |
| 27 | `CompressionEffect::Process` | 632 | 99.94 | 100 | 0 | 1 reg | EQUAL (SCHED) |
| 28 | `StandardStream::ConsumeData` | 616 | 99.58 | 100 | 0 | 12 reg | EQUAL (SCHED) |
| 29 | `BuildFromBSP` | 572 | 99.65 | 100 | 0 | 9 reg | EQUAL (SCHED) |
| 30 | `CamShot::StartAnim` | 552 | 99.93 | 100 | 0 | 1 reg | EQUAL (SCHED) |
| 31 | `RndFont::SetCharInfo` | 516 | 89.46 | 90.70 | 0 | 25 reg, 4 sym, 6+6 ins/del | EQUAL (SCHED: one `lhz`→`fcfid` conversion and two `stfs` scheduled elsewhere; compares equal once base registers are dropped) |
| 32 | `CharClipSet::LoadCharacter` | 480 | 98.17 | 98.33 | 0 | 2 reg, 1+1 ins/del | EQUAL (SCHED: `__RTDynamicCast` one slot later, same arguments and call order) |
| 33 | `XboxEnumeration::Poll` | 452 | 98.94 | 100 | 0 | 22 reg | EQUAL (SCHED: `this`/element pointer r29/r30; placeholder `fn_828404A8` §4.7) |
| 34 | `SfxInst::SfxInst` | 432 | 99.72 | 100 | 0 | 3 reg | EQUAL (SCHED) |
| 35 | `LabelNumberTicker::Poll` | 380 | 99.47 | 100 | 0 | 6 reg | EQUAL (SCHED) |
| 36 | `Movie::Impl::Draw` | 380 | 99.58 | 100 | 0 | 6 reg | EQUAL (SCHED: `clrrwi rX,rY,0` copy vs the value; placeholder `fn_8283D580` §4.7) |
| 37 | `NoteVoiceInst::NoteVoiceInst` | 364 | 97.55 | 97.77 | 0 | 1 reg, 3 off, 1+1 ins/del | EQUAL (SCHED: three `stb` to `0x35`–`0x37` in another order, one `lwz` moved) |
| 38 | `Synth::DrawMeterScale` | 344 | 98.95 | 100 | 0 | 11 reg, 7 sym | EQUAL (SCHED: r29/r30, r30/r31; `lbl_82C76B68` is a forgiven placeholder for `TheRnd`; constant §3) |
| 39 | `XboxEnumeration::Start` | 328 | 99.39 | 100 | 0 | 10 reg | EQUAL (SCHED; placeholder `fn_8283EB70` §4.7) |
| 40 | `VorbisReader::~VorbisReader` | 316 | 97.41 | 97.41 | 0 | 1 name, 1+1 ins/del | EQUAL (NAME + SCHED: `stb r28,0x11a(r3)` moved; the name is §4.6) |
| 41 | `DecodeThreadEntry` (VorbisReader) | 272 | 99.85 | 100 | 0 | 2 reg | EQUAL (SCHED, §4.4) |
| 42 | `StorePanel::CheckOut` | 260 | 96.52 | 96.91 | 0 | 3 reg, 1 off, 1+1 ins/del | EQUAL (SCHED: `lwz 0x7c(r27)` into another register one slot earlier) |
| 43 | `ctr_encrypt_fast` | 244 | 99.84 | 100 | 0 | 1 reg | EQUAL (SCHED) |
| 44 | `CharLipSyncDriver::Sync` | 224 | 96.32 | 96.41 | 0 | 1 off, 1+1 ins/del | EQUAL (SCHED: retail `lwz 0x48(r30)` = ours `lwz 0x8(r29)` with `r29 = r30+0x40`) |
| 45 | `Movie::Impl::DiscContentionPublish` | 196 | 98.78 | 100 | 0 | 10 reg | EQUAL (SCHED; placeholder `fn_8283D580` §4.7) |
| 46 | `Achievements::Submit` | 136 | 93.53 | 94.12 | 0 | 2 reg, 1+1 ins/del | EQUAL (SCHED: `addi r3,r1,0x50` two slots earlier) |
| 47 | `op9` (ByteGrinder) | 96 | 99.58 | 100 | 0 | 1 reg | EQUAL (SCHED) |
| 48 | `op6` (ByteGrinder) | 92 | 99.57 | 100 | 0 | 1 reg | EQUAL (SCHED) |
| 49 | `op0` (ByteGrinder) | 88 | 99.55 | 100 | 0 | 1 reg | EQUAL (SCHED) |

Totals: 49 rows / 47,276 B. By class: SCHED 43, ANCHOR 1 (`LightPreset::Load`), HOME 1 (`RndPostProc::Save`),
SCHED + HOME 1 (`SetupXSection`), SCHED + LAYOUT 1 (`EvaluateChannel`), NAME + SCHED 2 (`InitInfo`,
`~VorbisReader`). The four executed rows are SCHED.

## 4. Per row, where the read needed more than the tools

### 4.1 `NgSpotlightDrawer::SetupXSection` (2,104 B, 76.45): EQUAL (SCHED + HOME)

Retail saves r17–r31 and ours r20–r31. Retail's frame is `0x220` B and ours `0x200`, retail's body is
8 B longer, and almost every local sits at another offset. That matches W16-QQ's record ("by-value copies the same, stack placement differs"). The
16-byte evaluator lines the two sides up by naming every call output relative to its own argument:
- calls 1–7 (`GetLightPosition`, three `WorldXfm_Force`, two `Normalize`, `NGRadii`) are equal, args and
  by-reference contents;
- the `Normalize` calls 8 and 9 receive equal x/y/z (the only "difference" is a fourth float past the
  `Vector3`);
- call 10 differs only in its output slot's stale contents;
- the three `TheShaderMgr` constant calls pass registers `0x56`, `0x57`, `0x58` in that order on both sides,
  with equal `Vector4` contents;
- `SetXSectionTexture` is equal, and there is no store-to-object difference.

### 4.2 `CharBonesSamples::EvaluateChannel` (944 B, 85.77): EQUAL (SCHED + LAYOUT)

The linear evaluator crosses arms here, so I read each arm:
- **frac == 0, rotation:** both arms compute `f0` (short × 1/1638.4, or the float) and store it once.
  Retail tail-merges that store into the frac ≠ 0 float arm (`b 0x484`), ours into the frac == 0 float arm
  (`stfs f0,0(r30)` at idx 34). LAYOUT.
- **frac == 0, compressed vector:** `out[k] = s[k] × 1300/32767`, for k = 0, 1, 2 on both sides; retail
  converts `s[2]` first.
- **frac ≠ 0, rotation:** `(v0 + (v1−v0)·frac)·scale`. The slot numbers are swapped (`0x60`/`0x58` vs
  `0x50`/`0x60`), and `fsubs`/`fmadds` take the same values in mirrored registers.
- **frac ≠ 0, compressed vector** (idx 178–240): traced load by load, both sides build `sv0` at
  `0x60/0x64/0x68` = `s0[0..2]·k` and `sv1` at `0xa0/0xa4/0xa8` = `s1[0..2]·k`, then call
  `Interp(sv0, sv1, frac, dest)`.
- **quaternion arms:** identical.

### 4.3 `CharCollide::Deform` (1,052 B, ours 1,056): EQUAL (SCHED)

The one-sided immediates (`addi r7,r5,0xc` / `subi r9,r7,0x14` vs `0x8` / `0x10`) give the same base,
`r9 = r5 − 8`, on both sides. In the second `ctr` loop (idx 93–124, the `bdnz` target on both sides), the
per-iteration value is, on both sides,
`sqrt((v0·s+b0−cx)² + (v1·s+b1−cy)² + (v2·s+b2−cz)²) + acc`,
- where `v` is the 3-float field at `Q+0xc` and `s = (|v|−r)/|v|`;
- `b` is the bone position at `r4 + idx·0x60`;
- `c` is the 0.25-scaled centroid from the first loop.

Retail loads `v1` into f9 first and ours `v0`. That is why ours needs one extra `fmr f3,f6` to keep `v1`
live past the reuse of f6, and why ours is 4 B longer.

### 4.4 `DecodeThreadEntry` (VorbisReader, 272 B): EQUAL (SCHED)

Retail materialises `0x82E4C2A8` into r31 and `0x82E4C2A0` into r28. Ours puts `gNewReaders` in r31 and
`gReaders` in r28, and the two `lis` are issued in the opposite order. Alignment therefore paired retail
`C2A8` with our `gReaders`, which is the `MemTracker::DiffDump` trap from W16-TO §4.3. By role the two sides
agree:
- r31 is the list tested for empty and spliced from;
- r28 is the list spliced into, iterated and erased from.

So retail `gNewReaders` = `0x82E4C2A8` and `gReaders` = `0x82E4C2A0`, consistent with our declaration
order. All 68 instructions are otherwise identical.

### 4.5 `LightPreset::Load` (3,024 B): EQUAL (ANCHOR)

Retail addresses three statics off one base `lbl_82CC6E8C`: alt-rev `+0`, rev `+4`, `sLoading` `+6`. Ours
addresses them off `sLoading`: alt-rev `−2`, rev `+2`, `sLoading` `0`. Every access is consistent per
variable:
- 13 `lhz` of the rev;
- one `sth` each of rev and rev>>16;
- three `stb` of the flag.

This is the co-addressing record already in source (`LightPreset.cpp:28`, W16-HP) and W16-PY's anchor
census, which stopped after 3 variants. Not retried.

### 4.6 `VorbisReader::~VorbisReader` (316 B): EQUAL; an alias finding for a name lane

The one name charge is the `mPcmBuffers` destructor:
- **Retail** calls `0x82BB3BB8`, which the map names `vector<vector<unsigned short>>::~vector`
  (`symbol_aliases.json` group 697, `folded: []`).
- **Ours** is `vector<vector<short>>::~vector` (`VorbisReader.h:92`). It is a fold member of group 771,
  whose survivor is a different body at `0x82775950`.

Retail has two surviving bodies, so ICF did not consider them identical. Read from `band.exe`, they are
byte-identical except for their `bl` targets, and the difference sits two levels down:

| level | `0x82775950` chain | `0x82BB3BB8` chain |
|---|---|---|
| outer dtor | `+0x48` → `0x82773E70` | `+0x48` → `0x82BB3510` |
| `__destroy_range_aux` | `+0x2c` → `0x8269C800` | `+0x2c` → `0x8241B2D0` |
| element `??_G` | **12-byte element** (`li 0xc`, `divw`, `mulli 12`) | **2-byte element** (`srawi 1`, `slwi 1`) |

Our element is 2 bytes, so our destructor chain is byte-equivalent to `0x82BB3BB8`, not `0x82775950`. Our
`__destroy_range_aux<reverse_iterator<vector<short>*>>` has the same problem: it is a fold member of group
1879 at `0x82773E70`, the 12-byte chain. Both memberships look contradicted by retail bytes at depth 2.
(Group 771's T1 evidence masks relocations at depth 1, where the two bodies are identical. Its record says
"PROVEN on retail bytes" by W16-CU, so this is a claim against a recorded proof, and a name lane should
adjudicate it with `icf_pair_adjudicate.py --chase`.)

**Behaviour is unaffected.**
- `short` and `unsigned short` cannot be told apart in a destructor.
- In this TU `mPcmBuffers` elements are only written (`sth` of a clamped float, `VorbisReader.cpp:636`)
  and passed on as `void*` (`:541`), so the element's sign is not observable here.
- `VorbisReader.cpp` is not native-compiled (10-07c §5).

I did not edit either alias.

### 4.7 Placeholder callees, read by hand

The ruler forgives a `fn_` callee, so these were read from the split asm (keyed on `.fn fn_<addr>`):

| row | retail callee | what it is | ours |
|---|---|---|---|
| `Impl::Draw`, `Impl::DiscContentionPublish` | `fn_8283D580` | `lwz r11,0x100(r13); lwz r3,0x14c(r11)`: the current thread's id | `GetCurrentThreadId` |
| `Spotlight::Copy` (×3) | `fn_8230C628` | `this != &rhs`; pop nodes while longer (`0xc`-byte node free); assign element-wise | `ObjPtrList<T>::operator=` |
| `XboxEnumeration::Poll` | `fn_828404A8` | `CP_UTF8` (0xFDE9) special case; `cch == -1` ⇒ length+1; `cb == 0` ⇒ size query | `WideCharToMultiByte` |
| `XboxEnumeration::Start` | `fn_8283EB70` | shifts r4–r7 up, passes 0 as r4, tail-calls a kernel import | `XEnumerate` |
| `VorbisReader::VorbisReader` | `fn_8283D3C0` | kernel call `(h, 0)`; negative status ⇒ set last error, return 0; else 1 | `SetEvent` |

The argument registers at each site are equal on both sides (the evaluator compares them).

## 5. Why nothing is measured with `ab_measure`

No behaviour defect was found, so no source, map, splits, alias or `symbols.txt` change is kept. With no
patch, `tools/ab_measure.py` has nothing to price: its Δ would be 0 by construction. For the same reason
the native gate was not run. I also ran no spelling probes on the SCHED rows. The in-tree record for this
class is strongly negative (W16-RD: "inert in >99% of tries"; W16-TO's two probes were byte-identical), and
the brief asked for behaviour first.

## 6. What this says about lever 1

- **VIA-DC3 bucket 30 is drained for behaviour: 0 defects in 49 rows**, including the 4 executed. W16-TO
  found 0 in 57 in-scope rows. W16-TG's 2 in 70 (`Bloom_Blur`'s parameter swap) stays the only find in this
  class, so lever 1 as a whole has found 2 behaviour defects in 176 register/reorder rows.
- **TO's evaluator was not safe to reuse unchanged on these rows.** Its stack handling turned every
  conversion-slot renumbering into a value difference. The obvious fix, not comparing stack stores, would
  silently stop checking anything passed by reference. Use `~/tmp/w16tq/symeval7.py` + `symeval8.py`
  (both windows, union), `calldiff7.py` / `calldiff8.py`, `ctl.py`, and the whole-listing string check in
  §2 step 3. Like TK's and TO's, they are uncommitted (10-07c §6 lever 8).
- **Leftovers, with owners:**
  - `VorbisReader` destructor chain: groups 771 / 1879 memberships vs `0x82BB3BB8` / `0x82BB3510`,
    name lane (§4.6).
  - `StandardStream::InitInfo`: W16-OX's fill_insert fold (settled not admissible).
  - `LightPreset::Load`: ANCHOR (W16-PY).
- These rows go back to the permuter pilot's pool unchanged, now with a behaviour verdict.

## 7. Not done

- No alias edit (§4.6), and no re-adjudication of uncharged callee-name differences. As in TE, TI, TK and
  TO, the ruler's alias acceptance was taken as the fold record.
- No spelling sweep (§5).
- The evaluator is still linear. Where that mattered (`EvaluateChannel`'s arms, `CharCollide::Deform`'s
  loop, `SetupXSection`'s stack layout), the row was read by hand.

Artifacts (uncommitted, `~/tmp/w16tq/`):
- `rows49.json`: the population, with exec counts.
- `diffs/*.md`: the 49 listings.
- `run.log`: call / mem / imm summaries.
- `charges.txt`, `charges_all.txt`.
- `consts.txt`, `strmulti.txt`, `sympair.txt`.
- `calldiff6.txt` (TO's evaluator), `calldiff7.txt`, `calldiff8.txt`.
- `ctl_*.mdx`: the mutation controls.
- `prior_mentions.txt`.
