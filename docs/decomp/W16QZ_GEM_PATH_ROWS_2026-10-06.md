# W16-QZ: GemManager / GemTrackDir / SongParser sub-100 rows (2026-10-06)

Branch `w16-qz`, off main `d76ab0ca4`. Brief: bring the gem-path rows below
100 to 100 by fixing source against retail bytes, largest first:
`GemManager::SetupGems` (3,404 B, 97.56), `GemTrackDir::Handle` (95.74),
`SongParser::HandleRGGemStop` (97.38), then any other sub-100 rows in those
three units. The code is linked into the native build, so behaviour fixes count.

## Result

Measured with `tools/ab_measure.py --patch` on the whole lane diff (source only)
in this worktree, detached at `d76ab0ca4`. Ruler `name_check`, both legs settled.

```
leg A: matched=54688 honest=29479 code%=58.663370
leg B: matched=54691 honest=29482 code%=58.707397
Δmatched=+3  Δhonest=+3  Δcode_bytes=+4512  Δcode%=+0.044027pp
units: GemManager +1, GemTrackDir +1, SongParser +1; units at 100 583 -> 583, 0 fell off
```

Predicted before the run: +3 fns / +4,512 B (2,236 + 2,176 + 100). Measured
exactly that.

Native gate after the last commit:
`NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0`

| row | size | before | after | fix |
|---|---:|---:|---:|---|
| `GemTrackDir::Handle` | 2,236 | 95.74 | **100** | `SetFOV` tests `Cam()` directly |
| `SongParser::HandleRGGemStop` | 2,176 | 97.38 | **100** | root-note fallback (behaviour) + loop/geminfo shape |
| `GemManager::IsEndOfFill` | 100 | 92.80 | **100** | return the `&&` expression |
| `GemManager::SetupGems` | 3,404 | 97.56 | 98.54 | `unk130` is `unsigned int` |

## Behaviour fix: HandleRGGemStop root note

Retail only zeroes `geminfo.root_note` when the root is unset **and** the chord
shows its name (after the "No root note set" warning). In retail, the
`!show_chord_names` branch jumps straight to the arm that stores `mRGRootNote`,
so an unset root (`-1`) on a chord without a name is stored as `0xFF`. Ours
zeroed it in both cases. The fix is

```cpp
if (mRGRootNote < 0 && geminfo.show_chord_names) { MILO_WARN(...); geminfo.root_note = 0; }
else geminfo.root_note = (unsigned char)mRGRootNote;
```

That change also cleared a tick/info register swap the earlier edits had
introduced, which is what took the row from 99.67 to 100.

The other HandleRGGemStop edits change shape, not behaviour:
- Loops 1 and 2 (string-state checks) and the fret-fill loop index
  `info.mRGGemsInfo[i]` directly. Retail biases each loop's induction pointer to
  the last field the loop reads (+0x38 = `&[0].unk18` for loops 1–2, +0x34 for
  the fret loop), and the third loop gets its own `+0x20` base. With the
  `gems` alias and the `src++` pointer, ours shared one `+0x20` base across all
  of them. 97.38 → 98.46 → 99.07.
- The geminfo duration block follows the sibling `HandleGemStop` order (ms,
  tick, duration_ms, duration_ticks), with `ignore_duration` as a plain boolean
  expression. Retail sets `false` only on the fall-through path. → 99.67.
- Chord-text distance is `abs(on_tick - mRGChordTextTick) < 10`. Retail's xor
  operand order is the intrinsic's (`dist ^ sign`), not the hand-written
  `sign ^ dist`.

## GemTrackDir::Handle: SetFOV spelling

Retail has no out-of-line `SetFOV`. It is inlined into `Handle` and tests the
camera the way `SetCamPos` (100%) does: a signed `cmpwi` on the raw `ObjPtr`
word, then a second `Cam()` load through the adjusted `this`. Our
`RndCam *cam = Cam(); if (cam)` gave an unsigned compare and a single load. That
difference drove the msg/symbol register swap (r27/r28) and the early
`subi r30, r25, 0x7bc` in every handler arm. With
`if (Cam()) Cam()->SetFrustum(Cam()->NearPlane(), Cam()->FarPlane(), …)` the
whole 2,236 B function matches.

## SetupGems: what moved and what is left

`unk130` (offset 0x14c) is `unsigned int`. Retail rematerializes `li -1` for each
`unk130 = -1` store instead of reusing the shared `int -1` register, and compares
it against an unsigned chord ID. That fixed the `this`/`gem` swap (r26/r25):
diff_arg rows 148 → 53, fuzzy 97.56 → 98.54. `Min(phraseEnd, adjustedEnd)` puts
the trainer-clamp compare operands in retail's order. No other GemManager row
moved.

What is left (67 mismatch rows) and what was tried:

- **Stack slots.** Eight spill/home slots in 0x68–0x90 are permuted (trackNum,
  tonality, repeatedChordStart/End, phraseStart, otherSlot,
  lastArpeggioEndTick, the TheSongDB hi-half spill). Swapping the
  `lastArpeggioEndTick`/`nextFretForTrill` declarations was byte-inert. Most of
  these are spill slots, not declared homes.
- **Roll branch.** Retail stores `gem.mSlots` into slot 0x64 (rollSlots' home)
  twice, before the `beq` and after the `andc.`. An in-place
  `rollSlots &= ~gem.mSlots` loop was inert. Unexplained.
- **Trainer clamp.** Retail selects then stores unconditionally; ours stores
  conditionally. `Min(a,b)`, `Min(b,a)` and an explicit ternary all give the
  conditional store.
- **`gems[i + 1]`.** Retail has a dead `addi r11, r10, 0x44` before
  `lwz r11, 0x48(r10)`. `.GetTick()` instead of `.mTick` was inert.
- Scheduling only: the `mEnd`/`mBegin` store order, and the TheSongDB reload
  register in the arpeggio arm.

## Other rows, recorded

| row | size | fuzzy | finding |
|---|---:|---:|---|
| `GemTrackDir::SetPitch` | 380 | 99.89 | One `fmuls` operand order (`offset * inv` vs `inv * offset`). Seven spellings tried (swap, temp, `*=`, `-x/y`, `x/-y`, `-(x/y)`, `x*-(1/y)`): unchanged or worse. Matches the in-tree "commutative order is liveness-driven" record. |
| `SemitoneToWhiteKey` + `fn_822E3630` | 96 + 48 | 50 / 0 | Split mis-carve: retail's six switch arms (`addi r3,r3,N; blr`) are carved off as `fn_822E3630`. Needs a symbols.txt fix, which `ab_measure` refuses. Not touched. |
| `Find<RndCam>` (GemTrackDir) | 164 | unpaired | Retail's COMDAT sits in GemTrackDir's span (between `Find<TrackWidget>` and `_Rebalance_for_erase`), but nothing in retail GemTrackDir calls it. The callers are TrackPanelDir and Splash. Wii has `DECOMP_FORCEACTIVE(GemTrackDir, "game.cam")` between `SetInstrument` and `SetPlayerLocal`, and "game.cam" is absent from retail. So a non-virtual function doing `Find<RndCam>("game.cam")` existed and was stripped by `/OPT:REF`, while its instantiation survived through TrackPanelDir. Pairing it would need an invented function; not done. |
| `fn_822EE490`, `fn_822EAB90` | 8, 4 | 0 | A folded virtual accessor (`lwz r3,0x1dc(r3); blr`, 7 vtable refs) and a branch thunk. Identification, not source. |
| `SongParser::StartVocalNote` | 1,104 | 98.51 | Load scheduling for `mPrevVocalNote` end tick/ms. `EndMs()`/`EndTick()`, the mixed forms and both declaration orders were inert or worse (97.03). |
| `SongParser::ParseText` | 696 | 98.68 | The four warning arms are byte-identical (MILO_WARN reduces to the `PrintTick` evaluation). Retail keeps the merged `~String` tail on the 3rd laid-out arm (fall-through "bad mix"); ours keeps it on the 4th ("improperly formatted"). Inverting the digit-check if/else and folding the submix lookup into one condition were both inert. |
| `GemManager::CheckRemoveChordBracket` | 372 | 98.60 | Retail skips re-normalizing `GetHit`'s result before `&=`. An `int` mask in `GemStatus::GetHit` drops `AllCodaGemsHit` 100 → 87.40 and `OnGetPercentHitGemsPractice` 100 → 96.50. Reverted. A swapped `&` at the call site was inert. |
| `GemManager::DrawTrackMasks` | 1,600 | 98.78 | poolShape/widget register swap (r27/r28) plus index/base register numbering. `unsigned` `mNextArpeggioPhrase` was byte-identical. |
| `UpdateSlotPositions`, `PollVisibleGems` | 232, 156 | 99.83, 99.74 | `add` operand order for `&mGems[i]`. `(mGems.begin() + i)` was inert. |
| `SetupRealGuitarAreaStrumSections` | 344 | 99.19 | `0` / `0x44` constant register swap (r24/r25). Not attempted further. |
| `GemManager::GemManager` | 896 | 99.95 | Init-list store scheduling (0xcc before 0xc4). Member order is fixed by the class. |
| `fn_82B9BC84`, `fn_82B9F358` | 44 each | 99.55 | Anonymous catch funclets; not investigated. |

## Not done

No map, splits, symbols.txt or alias edits. No merge or push: that is left to
the coordinator.
