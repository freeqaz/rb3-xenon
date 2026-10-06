# W16-PQ — behaviour-only read of the native-compiled hard-core gap rows (2026-10-06)

Lane W16-PQ took lever 2 of `CAMPAIGN_STATE_2026-10-06.md` §5: read every long-worked in-scope gap row whose
source file the native build compiles today against retail's bytes, **for behaviour only** — what it computes,
calls, stores and returns — and fix every behavioural difference, ignoring register, scheduling and layout
residue. Branch `w16-pq`, based on main `dc238f637` (W16-PM merged). No `src/network`, `src/xdk`, map, splits,
alias or `symbols.txt` edit.

**Result: 64 rows read, 8 behaviour defects fixed in 6 rows, Δ0 bytes, 0 rows down.**

| verdict | rows | bytes |
|---|---:|---:|
| EQUAL (behaviour identical; codegen residue only) | 57 | 38,336 |
| FIXED | 6 | 10,916 |
| DIFFERENT, NOT FIXED (runtime-dead branch, see `NewFile`) | 1 | 424 |
| **total** | **64** | **49,676** |

Every one of the 64 rows has exactly one verdict; sizes sum to the live pool (§1).

## 1. The pool

Taken from 10-06's per-row table (`~/tmp/w16pn-gap/dispo_rows.json`): rows in disposition `09 source divergence:
opened and left (recorded)` or `08 I1/I3/I4: worked by W16-PC and left` whose `src` is in the native build's
file list (`native_src.json`). **Reproduces the doc's figure exactly: 65 rows / 49,784 B** (30 + 35).

Re-read at `dc238f637` after a full build (`BUILD_RC=0`, `[patch-state] OK`), keyed on (unit, symbol):
- `SymToTrackType` (108 B) is already at 100 (W16-PG landed after 10-06's measurement) — dropped.
- `ObjectDir::PreLoad` moved 99.62 → 99.70 (W16-PK) and stays in.
- **64 live rows / 49,676 B.**

## 2. Method

The 63 rows other than `SongDB::PostLoad` (read by hand first, to calibrate the instrument) were split into nine
file-coherent groups. Each group was read in its own `scripts/setup_worktree.sh` worktree at `dc238f637` after a
full build. Instrument: objdiff's full two-column listing on the graded ruler (`name_check`), read for calls and
their arguments, member load/store offsets, immediates, branch conditions and return values. Retail float
constants were read directly from `orig/45410914/band.exe` at the target's relocation addresses wherever a
constant was involved. Oracles (dc3, rb3-Wii) were consulted only when the listing was ambiguous; **retail bytes
outrank them**, and three of the eight defects are ones the oracle shares.

Every fix was rebuilt with a full `./tools/ninja-locked` in the group's worktree and checked row-by-row against
that worktree's pre-edit `report.json`; then the combined patches were measured with `tools/ab_measure.py` in the
lane worktree (§4).

## 3. Verdicts

### 3.1 Fixed (8 defects, 6 rows)

| row | size | defect (retail evidence) | fuzzy |
|---|---:|---|---|
| `Locale::Init` | 1,304 | `StringTable` size is Σ`strlen(s)+1` per unique string (`add r11,r11,r17; addi r17,r11,1`); ours omitted the NUL, one byte short per string. DC3 and rb3-Wii agree with retail. | 95.95 → 96.47 |
| `ObjectDir::Save` | 2,108 | Ours stored 0 to `mCurViewportID` (+0x8C) on every save; retail's only access is the load for `bs << mCurViewportID`. | 95.97 → 99.58 |
| `ObjectDir::ResetViewports` | 532 | Retail stores −1.0 to +0xD8 (`vp[3].m.y.z`); ours +1.0, a reflection (det −1) instead of a rotation. Now `Set(1,0,0, 0,0,-1, 0,1,0)`, as DC3. | 98.31 → 98.35 |
| `SongParser::HandleRGGemStop` | 2,176 | The area-strum window is tested against `tick` (`r23`, written once by `mr r23,r4`; re-verified in the lane) at both bounds, not `on_tick`. Retail picks `strum_type` by the chord's **stop** tick. **The rb3-Wii oracle has `on_tick`.** | 97.357 → 97.375 |
| `VocalPlayer::Poll` (1/3) | 3,388 | `frameMinPitch` starts at **9.0e9f**: retail `.rdata 0x820EEF18` = `50 06 1C 46`. Ours was 9985.578125f = `0x461C0650` — **the same word byte-swapped**; the rb3-Wii oracle carries the same error. | unchanged |
| `VocalPlayer::Poll` (2/3) | — | Retail's Poll, `Restart` and `HookupTrack` all call an out-of-line `0x826E3E90` = `!TheNetSession->IsLocal() && PressingToTalk()` (callees named in the map: `?IsLocal@NetSession@@QBA_NXZ`, `?PressingToTalk@VocalPlayer@@QAA_NXZ`; body disassembled in the lane). `f3ec9592d` had collapsed all three to `PressingToTalk()`. Restored as `VocalPlayer::CanChat`, placed after `PressingToTalk` as in retail. | — |
| `VocalPlayer::Poll` (3/3) | — | Callee `GameMicManager::GetMicCount`: retail `0x8235BA70` is `lwz r3,0x34(r3); blr`. Ours returned 4 in `frame_rate` mode (DC3's fake-mic feature); that branch is now `#ifdef HX_NATIVE`, as `GetMic`'s already is, so native behaviour is unchanged. | — |
| `VoiceBeat::Analyze` | 1,408 | Syllable trigger is `4*min(0.15, mFloorSigma)` (`fcmpu floor,0.15; ble -> &mFloorSigma`); ours and the rb3-Wii oracle had `max`. The `blt`/`ble` residue differs only at equality, where both operands are the same value. | unchanged |

### 3.2 Behaviour equal (57 rows)

Per-row evidence is in the group files (`~/tmp/w16pq/g*_verdicts.md`, not committed); the one-line reason for each:

| group | rows | what the residue is |
|---|---|---|
| os (`Joypad`, `UsbMidi*`, `File`, `Archive`, `Debug`, `ChunkStream`) | `JoypadPollCommon` 2,468 · `UsbMidiGuitar::Poll` 1,244 · `UsbMidiKeyboard::Poll` 1,164 · `ChunkStream::Eof` 932 · `FileMakePath` 792 · `Archive::Merge` 516 · `Archive::GetFileInfo` 432 · `Debug::Fail` 264 | frame size, constants held vs reloaded, commutative operand order, same stores reordered, loop rotation, 4-byte-element ICF fold names |
| utl (`MemMgr`, `MemHeap`, `MemTracker`, `Song`, `NetCacheMgr`, `MultiTempoTempoMap`, STLport map) | `Song::SyncState` 1,192 · `NetCacheMgr::AddLoaderRef` 696 · `MemHeap::Alloc` 652 · `MemAlloc` 644 · `ThreadMemStack` 348 · `AddHeap` 344 · `MemTracker::MemTracker` 312 · `map<Symbol,DataNode>::operator[]` 220 · `MemHeap::Init` 216 · `MultiTempoTempoMap::GetLoopTick` 164 | loop direction over the same vector, dead stack stores, a no-op re-store under a lock, a temporary DataNode copied instead of built in place (int node, no refcount), proven folds |
| obj | `ObjectDir::PreLoad` 3,092 | 773 vs 773 instructions, all 46 mismatches an r17↔r18 rename (re-confirms W16-PK) |
| beatmatch | `SongParser::StartVocalNote` 1,104 · `ParseText` 696 · `SongData::UnflipGems` 656 · `SongData::Load` 612 · `TrimOverlappingGems` 536 · `GameGemList::AddGameGem` 272 | load order, shared-tail block placement, a hoisted safe load, `extsw` on a pointer, comparator folds (`operator<` ≡ `CompareTimes`) |
| math | `MakeBSPTree` 1,580 · `Intersect(Triangle,Box)` 848 · `Multiply(Matrix3…)` 680 · `BSPFace::Update` 612 · `MakeRotQuat` 248 · `MakeEulerScale` 224 · `Intersect(V,V,Box,…)` 188 | all nine SAT axes and all nine matrix dot products rebuilt on both sides; constants read from band.exe; `/fp:fast` association order of 3-term sums |
| bandobj A | `CharKeyHandMidi::Poll` 2,236 · `BandHeadShaper::Init` 1,144 · `SetMeshAnim` 852 · `DeltaArray::AppendDeltas` 584 · `BandCharacter::StartLoad` 292 · `CharKeyHandMidi::FindPreferredFinger` 228 · `BandCharacter::AddOverlays` 112 | register renames, branchless vs branched clamp, dead spill, retail FP registers hand-decoded to the same association as our source |
| bandobj B | `OutfitConfig::SetSkinTextures` 1,192 · `BandRetargetVignette::EnterDir` 772 · `BandCrowdMeter::Poll` 652 · `BandCharDesc::CopyCharDesc` 636 · `BandRetargetVignette::Poll` 280 · `BandList::UpdateConcealState` 228 · `BandIKEffector::ComputeElbowPullAndQuat` 184 | indexed vs pointer walk over the same 5 elements, `d + m*(-0.1f)` vs `d - m*0.1f` (constants read: exact negation, one fused op), block placement, sum-of-squares association |
| game | `MultiplayerAnalyzer::AddGems` 400 · `VocalPart::CalcPhraseScoreMax` 192 · `VocalPart::IsEmptyPhrase` 116 · `SongDB::PostLoad` 88 | reloads, counter vs pointer trip count, a no-op `clrrwi r10,r10,0`; PostLoad: same five calls in order, retail has a frame pointer and 16 B more frame |
| meta_band | `SongSortMgr::DoesOfferMatchFilter` 1,416 · `DoesSongMatchFilter` 568 · `BandSongMgr::SyncSharedSongs` 668 · `RemoveOldestCachedContent` 624 · `ReadCachedMetadataFromStream` 400 · `BandSongMetadata::HasPart` 340 · `AppLabel::SetLeaderboardRankAndName` 184 | `find != end` booleanised twice vs once, a 16-byte copy in another word order, add operand order, retail format string `"%s) %s"` read from band.exe = ours |

⚠ Four rows carry a `/fp:fast` **association-order** residue (`ComputeElbowPullAndQuat`,
`CharKeyHandMidi::Poll`'s inlined `Length`, `Multiply`, and `VoiceBeat::Analyze`'s filter sums). The terms and
coefficients are identical and in each case our source spells retail's order; the difference is the compiler's
reassociation and can only move the last bit. Recorded as codegen, not fixed.

### 3.3 Different, not fixed (1 row)

`NewFile` (424 B): ours has `if (gNullFiles) return new NullFile();`, which retail lacks; the rest matches call for
call. `gNullFiles` is never written anywhere in `src/` or `native/`, so the branch never runs. The in-source
comment records that removing it measured −4 fns / −88 B (it is our only `NullFile` vtable emission site). Left
alone: behaviour is equal on every run, and removing it is a codegen trade, not a correctness fix.

## 4. Measurement

Two `tools/ab_measure.py --from-dirty` runs in `~/tmp/wt-w16pq`, both settled to zero-work on both legs; ruler
`name_check`, objdiff `sha256:c1b7d95240a35cd6` stable across legs. Per-row deltas are from the archived
`legA/legB_report.json.gz`, keyed on (unit, symbol), 68,909 rows with identical row sets on both legs.

**Prediction for run 1, written before it ran:** Δmatched 0, Δcode_bytes 0 (no row reaches 100); exactly four
rows up — Locale::Init 95.95→96.47, ObjectDir::Save 95.97→99.58, ResetViewports 98.31→98.35, HandleRGGemStop
97.357→97.375 — and 0 down.

| run | patch | recompiles | Δmatched | Δcode_bytes | Δfuzzy | rows up | rows down |
|---|---|---:|---:|---:|---:|---:|---:|
| `20261006-093151` | Locale + Dir + SongParser (`c2313064d`) | 7 | +0 | +0 | +0.000477 pp | **4 (exactly the predicted four, to the printed digit)** | **0** |
| `20261006-094107` | VocalPlayer + GameMicManager + VoiceBeat (`7e3f3bb18`, `src/` part) | 17 | +0 | +0 | +0.000000 pp | 0 | **0** |

Run 2's leg A is run 1's committed tree (`fuzzy` 63.696045 on both), so the two compose against `dc238f637`:
**Δ0 functions, Δ0 bytes, +0.000477 pp fuzzy, 4 rows up, 0 rows down.** The lane's prior (10-06 §5: "expect
almost no bytes") held exactly.

Run 2 is Δ0 on every row because each of its fixes sits where `name_check` does not look: the wrong constant was
behind a placeholder label (§5.1), `CanChat`'s retail address is anonymous, `GetMicCount` is an 8-byte getter whose
row was already matched, and the `min`/`max` swap leaves the instruction sequence the same shape.

## 5. Findings

### 5.1 A wrong float literal behind a placeholder label scores zero

`frameMinPitch`'s initial value was wrong by six orders of magnitude and `VocalPlayer::Poll`'s score did not see
it. Retail loads the constant through `lbl_820EEF18`; `name_check` forgives placeholder relocation targets
(`lbl_`/`fn_`/…), and the relocation **name** is the only thing objdiff compares for a constant-pool load — the
pointed-to **value** is never read. So **every float/double literal whose retail pool entry is unnamed is
unchecked by the metric**, right or wrong. This defect was found only because the fork read the retail word out
of `band.exe`.

⇒ Proposed instrument (not built here): for every constant-pool relocation in a paired row, read retail's pool
value at the relocation target and our `__real@XXXXXXXX` value, and list mismatches. It is mechanical, needs no
judgement per row, and its population is every paired function, not only the gap.

### 5.2 Retail outranked the oracle three times

`HandleRGGemStop` (`on_tick` in rb3-Wii), `VoiceBeat::Analyze` (`max` in rb3-Wii) and `frameMinPitch` (the
byte-swapped literal in rb3-Wii) are all oracle-shared defects. Retail bytes settled each; no oracle text was
taken as evidence for a fix.

### 5.3 A removed retail gate came back

`f3ec9592d` (wave 4, NCCC-0803-b2bb) replaced retail's `CanChat` wrapper with a direct `PressingToTalk()` call at
three sites. Native then segfaulted on the absent `BandUser` until W16-PD guarded `PressingToTalk` under
`HX_NATIVE` and deleted the native `NetSession::IsLocal`, which had become unreferenced. Restoring the retail call
graph references it again (§6).

### 5.4 Codegen found, not landed

- `MakeBSPTree`: replacing the explicit `clear()` pairs with retail's destructor-only clearing lifts the row
  95.45 → 98.12, but shrinks the frame by 0x10, and its three 40 B EH funclets (`fn_824F2D4C`, `fn_824F2D74`,
  `fn_824F2D9C`) fall 100 → 99.8–99.9: whole report **−3 fns / −120 B**. Reverted (g5 fork).
- `NewFile`'s dead `gNullFiles` branch (§3.3): −4 fns / −88 B on record.
- `BandCharDesc::CopyCharDesc`: retail keeps values in volatile registers across same-TU `operator==` calls, a
  codegen lead for a register lane.

## 6. Native

All fixed files are native-compiled. `CanChat` makes `VocalPlayer.cpp` reference `NetSession::IsLocal` again,
whose real body (`src/network/net/NetSession.cpp`) cannot link natively (its online path needs `IsHost` and the
Quazal session; neither is in any native binary — checked with `nm`).

**Prediction:** the native gate fails on `NetSession::IsLocal` in rb3-vocal2 and rb3-harmony. **Measured:**
`NATIVE_GATE_RESULT verdict=FAIL expected=18 verified=16 … failed=2`, `undefined reference to
'NetSession::IsLocal() const'` from `VocalPlayer::CanChat`, targets rb3-harmony rb3-vocal2.

Fix: `native/src/m10_leaf_stubs.cpp` defines `NetSession::IsLocal() const { return true; }`. That is the value the
real body returns for the only session these drivers hold: `NativeMakeNetSession()`'s calloc'd one, `mState ==
kIdle` (0, not a joining state) and `mOnlineEnabled == false`, which exits at the real body's `!mOnlineEnabled`
test. So `CanChat()` is false natively, exactly as `PressingToTalk()` already was there (no `BandUser`).

- Native gate after: `PASS 18/18, 0 skipped, rc=0` (final line in §8).
- `tools/native_health.sh`: PASS, 18/18 targets ran, 57/57 gates, 0 crashes.
- rb3-vocal2 and rb3-harmony stdout is **byte-identical** (131/131 and 185/185 lines, paths stripped) to three
  health runs from earlier today on pre-change trees. ⚠ That comparison has not been shown to discriminate: the
  printed summaries may simply not exercise the changed paths (`frameMinPitch`'s sentinel reaches `Singer::Poll`
  only on frames with no scorable note near; chat is false on both sides).

Native-observable effect of each fix, as far as the code reaches today:
- `Locale::Init`: string table sized for the terminators.
- `ObjectDir::Save`: saving a dir no longer resets the current viewport; viewport 3 is a rotation.
- `HandleRGGemStop`: a Pro Guitar/Bass chord whose area-strum window starts or ends between its on and stop ticks
  now gets retail's strum type.
- `VoiceBeat::Analyze`: syllable detection threshold uses the smaller of 0.15 and the noise floor.
- `frameMinPitch`, `CanChat`, `GetMicCount`: no change on the current drivers (see above).

## 7. Not done

- No codegen chasing on any row (by brief) — §5.4 lists what was found and not landed.
- The constant-pool value instrument of §5.1 is proposed, not built.
- Lever 3 (the same audit on in-scope files native does not link) was not started.
- The fork worktrees `~/tmp/wt-w16pq-g{1..9}_*` and their per-group verdict files under `~/tmp/w16pq/` are not
  committed; this doc carries every verdict.

## 8. Final gate lines

Recorded after the last commit (see the commit that adds them).
