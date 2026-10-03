# W16-OM: chase learns RTTI-named vacuous vtable slots and the dead trailing blr (2026-10-03)

Branch `w16-om`, rebased on main `dd334a0fa`. Tool: `tools/icf_pair_adjudicate.py`.
No alias membership and no map name was changed in this lane; the tool now *decides*
pairs it used to decline, and the verdict changes below are reported, not acted on.

## What the chase was refusing, and why it was not retail disagreeing

W16-OK (`80fc81d14`, and the `rechase_2026-10-03` fields in
`scripts/symbol_aliases.json`) found 7 of its 14 new memberships read
`CHASED T1 REFUTED` while no retail byte disagreed with ours. Reproduced 7/7 on
`80fc81d14` (tree built first; `--chasetest` PASSED, 19 controls). Bodies read
from the built tree:

| pair | retail | ours | the only difference |
|---|---|---|---|
| `~Message` <- 2 message dtors | 24 B, vacuous | 24 B | vptr HA/LO slots: retail `lbl_82000A18`, ours `??_7Message@@6B@` |
| `~NetMessage` <- 3 message dtors | 16 B, vacuous | 16 B | vptr HA/LO slots: retail `lbl_8203D19C`, ours `??_7NetMessage@@6B@` |
| `~BandHeadShaper` <- `~TrackerDesc` | 32 B | 36 B | ours ends `b MemOrPoolFreeSTL; blr`, retail ends at the `b` |
| `GetIdentifyingToken` <- `GetCurrentQuestFilter` | 16 B | 12 B, vacuous | retail = ours + one zero alignment word (not in this lane's scope) |

The vacuous branch refuses every placeholder slot (`VACUOUS-PLACEHOLDER-SLOT`),
and the general path treats ours = retail + 4 B as `BYTES-DIFFER`.

## Rule A: `vacuous_vtable_slot` (vacuous branch only)

A placeholder `lbl_X` / `vftable_X` against our **primary** `??_7C@@6B@` is
accounted for (not tolerated) iff:

1. retail RTTI (COL at X-4) names exactly `C`. The `ObjectDir` default-argument
   normalisation is the only equivalence allowed. No `CLASS_RENAMES`, related-class
   or template-arity tolerance applies;
2. the COL offset is 0, so X is a primary vtable (sub-object `6BBase@@@` spellings
   are not handled);
3. X is the **only** offset-0 retail vtable of `C`, found outward from the
   type-descriptor string;
4. one of our names is never paired with two retail addresses inside one proof.

Then the masked field resolves to exactly one address, which is the invariant
the vacuous branch maintains. Every other placeholder slot in a vacuous body still
refuses (callee `fn_`, data globals, sub-object vtables).

Controls: **VTVAC POSITIVE** is retail `~NetMessage` (0x826908C8) vs our
`~KickPlayerMsg`, expected PROVEN. **VTVAC DECOY** is the same retail body with only
the vptr slots re-pointed at `lbl_82000A18`, a real retail vtable whose RTTI is
`.?AVMessage@@`. It is expected REFUTED, and it is refused on
`VACUOUS-SLOT-REFUSED:VTABLE-RTTI-DIFFERS`. `--self-break-vtvac` accepts any readable
RTTI, which turns the decoy red and moves no other control.

## Rule B: `retail_dead_blr` (general path only)

ours = retail masked body + `blr` is admitted iff:

1. no relocation on either side lies in the extra word;
2. retail's last word is an unconditional, non-linking, relocated `b` (opcode 18,
   AA=LK=0, REL24 at that offset), so the blr is unreachable by fall-through;
3. the raw retail bytes at the survivor's map address equal **our** body, `blr`
   included, under our relocation masks. This anchors the address and shows that
   the next raw word is a `blr`;
4. **census:** nothing in retail references that word. That means 0 relative
   branches in `.text` land on it, 0 aligned big-endian words anywhere in the image
   equal it, 0 `lis rD,ha` + `addi rX,rD,lo` pairs in `.text`/`BINK` build it, and
   no `.pdata` function starts there.

(4) is the discriminator because MSVC does not fold one COMDAT into the tail of
another. If that `blr` were a separate, referenced function, retail's COMDAT at X
would genuinely lack our trailing blr, and the body there could not be ours.

Each census channel is checked for vacuity against a fixed retail witness
(`CENSUS_WITNESS`), and `--chasetest` refuses to run if any of them reads 0. The
measured counts are 7 branches into 0x822AFD68, 28 data words for 0x826783B0, and
1 `lis/addi` for 0x822703A8. The dead word 0x822AFD88 reads 0 / 0 / 0, and it is
not a `.pdata` start.

Controls: **DEADBLR POSITIVE** is the `~BandHeadShaper` same-name pair, expected
PROVEN. **DEADBLR DECOY** is the identical pair with one branch from outside
injected into the census for that word, expected REFUTED (`BYTES-DIFFER`).
`--self-break-deadblr` drops the census, which turns the decoy red and moves no
other control.

Measured reach before writing the rule: over every same-name pair and every alias
membership, the shape "ours = retail + blr" occurs exactly twice (the
`~BandHeadShaper` self-pair and `~TrackerDesc`). In both, retail ends in a `b`,
the next word is `blr`, the census is empty, and the bodies are non-vacuous.

## Instrument results (rebased tree `dd334a0fa`, built, `--verify-manifest` OK)

| run | result |
|---|---|
| `--chasetest` | PASSED, 23 controls (19 before + 4 new) |
| `--selftest` | PASSED |
| `--chasetest --simulate-naming` | PASSED |
| `--self-break` | OK (VACUOUS DECOY red) |
| `--self-break-slots` | OK (6/6 SLOT DECOYs red) |
| `--self-break-tailpad` | OK |
| `--self-break-rename` | OK |
| `--self-break-overcarve` | OK |
| `--self-break-size eh / tol / onesided / firstdef` | OK / OK / OK / OK |
| `--self-break-vtvac` (new) | OK (VTVAC DECOY red, nothing else moved) |
| `--self-break-deadblr` (new) | OK (DEADBLR DECOY red, nothing else moved) |

The same 14 runs also passed on `80fc81d14` before the rebase.

Native gate, run last on the rebased branch (only `tools/` and `docs/` changed):

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

## Re-chase of `scripts/symbol_aliases.json`

Method: one loaded tree, and for every group, each `folded` spelling and each
`withdrawn` record was chased by the base tool (`dd334a0fa`'s copy) and by the new
one. Each pair got a fresh memo and context, the same call `--chase` makes.
That is 17,354 pairs. The run was repeated on `80fc81d14` and on `dd334a0fa`, with
identical counts both times.

| population | before -> after | count |
|---|---|---|
| folded | PROVEN -> PROVEN | 5,279 |
| folded | REFUTED -> REFUTED | 903 |
| **folded** | **REFUTED -> PROVEN** | **129** |
| folded | PROVEN -> REFUTED | **0** |
| withdrawn | REFUTED -> REFUTED | 10,935 |
| withdrawn | PROVEN -> PROVEN | 108 |
| withdrawn | any change | **0** |

**Prediction failure:** I predicted 6 flips (W16-OK's five message dtors plus
`~TrackerDesc`). The measured count is **129**. 128 come from rule A across 14
groups, because the vacuous-vtable class covers every derived implicit dtor already
folded into `~Message` (96) and `~NetMessage` (18), plus 12 ctor/dtor pairs that
only install a vptr. Exactly 1 comes from rule B (`~TrackerDesc`). The rules only
add admissions on paths that used to return False, so 0 PROVEN -> REFUTED is
expected by construction. 0 changes in the withdrawn records means neither rule
re-proves anything a previous lane withdrew.

Of W16-OK's seven, six now chase PROVEN. `GetCurrentQuestFilter` stays REFUTED
(`VACUOUS`). Its mechanism is a vacuous body plus an alignment word, and
`retail_tail_pad` is deliberately general-path-only. That is outside this lane's
two rules.

Rule A never fired-and-refused on a landed membership (0 `VACUOUS-SLOT-REFUSED`
traces among the 903 still-REFUTED), so it surfaces no contradiction. What still
blocks those 903 (terminal trace kind): `MISSING(ours)` 488, `MISSING(retail)` 321,
`SLOT-REFUTED` 66, `VACUOUS` 16, `BYTES-DIFFER` 8,
`SLOT-UNDISCHARGED:CALLEE-UNPROVEN-OURS-DISTINCT` 4. I checked the 16 `VACUOUS` and
8 `BYTES-DIFFER` cases, and none of them is either mechanism. The two
`VACUOUS-PLACEHOLDER-SLOT` survivors (`String(const char*)`, `AlternateSongDir`)
refuse on a callee `fn_` / unmapped data slot, and the 8 `BYTES-DIFFER` are real
instruction differences. These were not examined further.

## Not done, deliberately

- No alias membership, record or map name was changed. The 129 verdict changes
  are evidence a later lane may cite. They are not admissions.
- The vacuous branch does not learn tail padding (`GetCurrentQuestFilter`) or
  sub-object vtables (`??_7C@@6BBase@@@`).
- `size_gate` was not extended with the dead-blr direction. It still reports such
  a pair as "ours LARGER".
- `lis/ori` address materialisation is not in the census (only `lis/addi`, which
  is the form MSVC uses for a function address).

## The 129 memberships whose chase verdict changed (REFUTED -> PROVEN)

**`0x823426f8` `??1Message@@UAA@XZ`** -- 96 membership(s), rule A

- `??1AddLocalUserResultMsg@@UAA@XZ`
- `??1AddUserResultMsg@@UAA@XZ`
- `??1ButtonDownMsg@@UAA@XZ`
- `??1ButtonUpMsg@@UAA@XZ`
- `??1ConnectionStatusChangedMsg@@UAA@XZ`
- `??1ContentInstalledMsg@@UAA@XZ`
- `??1ContentReadFailureMsg@@UAA@XZ`
- `??1CurrentScreenChangedMsg@@UAA@XZ`
- `??1DeviceChosenMsg@@UAA@XZ`
- `??1DiskErrorMsg@@UAA@XZ`
- `??1EventDialogDismissMsg@@UAA@XZ`
- `??1EventDialogStartMsg@@UAA@XZ`
- `??1FriendsListChangedMsg@@UAA@XZ`
- `??1GameEndedMsg@@UAA@XZ`
- `??1GameMicsChangedMsg@@UAA@XZ`
- `??1InputStatusChangedMsg@@UAA@XZ`
- `??1InviteAcceptedMsg@@UAA@XZ`
- `??1InviteSentMsg@@UAA@XZ`
- `??1JoinResultMsg@@UAA@XZ`
- `??1JoypadBreedDataReadMsg@@UAA@XZ`
- `??1JoypadBreedDataWriteMsg@@UAA@XZ`
- `??1JoypadConnectionMsg@@UAA@XZ`
- `??1KeyboardConnectedAccessoriesMsg@@UAA@XZ`
- `??1KeyboardExpressionPedalMsg@@UAA@XZ`
- `??1KeyboardHighHandPlacementMsg@@UAA@XZ`
- `??1KeyboardKeyMsg@@UAA@XZ`
- `??1KeyboardKeyPressedMsg@@UAA@XZ`
- `??1KeyboardKeyReleasedMsg@@UAA@XZ`
- `??1KeyboardLowHandPlacementMsg@@UAA@XZ`
- `??1KeyboardModMsg@@UAA@XZ`
- `??1KeyboardStompBoxMsg@@UAA@XZ`
- `??1KeyboardSustainMsg@@UAA@XZ`
- `??1KeysAccelerometerMsg@@UAA@XZ`
- `??1LocalUserLeftMsg@@UAA@XZ`
- `??1LockStepCompleteMsg@@UAA@XZ`
- `??1LockStepStartMsg@@UAA@XZ`
- `??1MCResultMsg@@UAA@XZ`
- `??1MatchmakerChangedMsg@@UAA@XZ`
- `??1MetadataLoadedMsg@@UAA@XZ`
- `??1ModeChangedMsg@@UAA@XZ`
- `??1MultipleItemsEnumCompleteMsg@@UAA@XZ`
- `??1NetComponentScrollMsg@@UAA@XZ`
- `??1NetComponentSelectMsg@@UAA@XZ`
- `??1NetStartUtilityFinishedMsg@@UAA@XZ`
- `??1NewOvershellLocalUserMsg@@UAA@XZ`
- `??1NewRemoteMachineMsg@@UAA@XZ`
- `??1NewRemoteUserMsg@@UAA@XZ`
- `??1NoDeviceChosenMsg@@UAA@XZ`
- `??1OvershellActiveStatusChangedMsg@@UAA@XZ`
- `??1OvershellAllowingInputChangedMsg@@UAA@XZ`
- `??1OvershellOverrideEndedMsg@@UAA@XZ`
- `??1PartyMembersChangedMsg@@UAA@XZ`
- `??1PlatformMgrOpCompleteMsg@@UAA@XZ`
- `??1PrimaryProfileChangedMsg@@UAA@XZ`
- `??1ProcessedButtonDownMsg@@UAA@XZ`
- `??1ProcessedJoinRequestMsg@@UAA@XZ`
- `??1ProfileChangedMsg@@UAA@XZ`
- `??1ProfilePreDeleteMsg@@UAA@XZ`
- `??1ProfileSwappedMsg@@UAA@XZ`
- `??1RGAccelerometerMsg@@UAA@XZ`
- `??1RGConnectedAccessoriesMsg@@UAA@XZ`
- `??1RGFretButtonDownMsg@@UAA@XZ`
- `??1RGFretButtonUpMsg@@UAA@XZ`
- `??1RGMutingMsg@@UAA@XZ`
- `??1RGPitchBendMsg@@UAA@XZ`
- `??1RGProgramChangeMsg@@UAA@XZ`
- `??1RGStompBoxMsg@@UAA@XZ`
- `??1RGSwingMsg@@UAA@XZ`
- `??1ReleasingLockStepMsg@@UAA@XZ`
- `??1RemoteLeaderLeftMsg@@UAA@XZ`
- `??1RemoteMachineLeftMsg@@UAA@XZ`
- `??1RemoteMachineUpdatedMsg@@UAA@XZ`
- `??1RemoteUserLeftMsg@@UAA@XZ`
- `??1RemoteUserUpdatedMsg@@UAA@XZ`
- `??1SaveLoadMgrStatusUpdateMsg@@UAA@XZ`
- `??1ServerStatusChangedMsg@@UAA@XZ`
- `??1SessionBusyMsg@@UAA@XZ`
- `??1SessionDisconnectedMsg@@UAA@XZ`
- `??1SessionMgrUpdatedMsg@@UAA@XZ`
- `??1SessionReadyMsg@@UAA@XZ`
- `??1SigninChangedMsg@@UAA@XZ`
- `??1StorageChangedMsg@@UAA@XZ`
- `??1StringStrummedMsg@@UAA@XZ`
- `??1SyncStartGameMsg@@UAA@XZ`
- `??1UIChangedMsg@@UAA@XZ`
- `??1UIComponentFocusChangeMsg@@UAA@XZ`
- `??1UIComponentScrollMsg@@UAA@XZ`
- `??1UIComponentSelectDoneMsg@@UAA@XZ`
- `??1UIComponentSelectMsg@@UAA@XZ`
- `??1UIScreenChangeMsg@@UAA@XZ`
- `??1UITransitionCompleteMsg@@UAA@XZ`
- `??1UITriggerCompleteMsg@@UAA@XZ`
- `??1UserLoginMsg@@UAA@XZ`
- `??1VirtualKeyboardResultMsg@@UAA@XZ`
- `??1VoiceChatDisabledMsg@@UAA@XZ`
- `??1XMPStateChangedMsg@@UAA@XZ`

**`0x826908c8` `??1NetMessage@@UAA@XZ`** -- 18 membership(s), rule A

- `??1AccomplishmentMsg@@UAA@XZ`
- `??1AddUserResponseMsg@@UAA@XZ`
- `??1EndGameMsg@@UAA@XZ`
- `??1EnterFlowMsg@@UAA@XZ`
- `??1JoinResponseMsg@@UAA@XZ`
- `??1KickPlayerMsg@?A0x3d644e05@@UAA@XZ`
- `??1PlayerGameplayMsg@@UAA@XZ`
- `??1SessionMsg@@UAA@XZ`
- `??1SetPartyShuffleModeMsg@@UAA@XZ`
- `??1StartGameOnTimeMsg@@UAA@XZ`
- `??1SyncAllMsg@@UAA@XZ`
- `??1SyncLocalMachineMsg@?A0x5fd33732@@UAA@XZ`
- `??1SyncUserMsg@@UAA@XZ`
- `??1TourHideShowFiltersMsg@@UAA@XZ`
- `??1TourMostStarsMsg@@UAA@XZ`
- `??1TourPlayedMsg@@UAA@XZ`
- `??1TriggerBackSoundMsg@@UAA@XZ`
- `??1UserLeftMsg@@UAA@XZ`

**`0x826d27c8` `??0TrackerDisplay@@QAA@ABV0@@Z`** -- 3 membership(s), rule A

- `??1TrackerBandDisplay@@UAA@XZ`
- `??1TrackerBroadcastDisplay@@UAA@XZ`
- `??1TrackerPlayerDisplay@@UAA@XZ`

**`0x822afd68` `??1BandHeadShaper@@QAA@XZ`** -- 1 membership(s), rule B

- `??1TrackerDesc@@QAA@XZ`

**`0x8242ec18` `??0PostProcessor@@QAA@XZ`** -- 1 membership(s), rule A

- `??1PostProcessor@@UAA@XZ`

**`0x8257b408` `??$__destroy_aux@U?$pair@PAVBandProfile@@VPerformerStatsInfo@@@stlpmtx_std@@@stlpmtx_std@@YAXPAU?$pair@PAVBandProfile@@VPerformerStatsInfo@@@0@ABU__false_type@0@@Z`** -- 1 membership(s), rule A

- `??1?$pair@PAVBandProfile@@VPerformerStatsInfo@@@stlpmtx_std@@QAA@XZ`

**`0x82595f70` `??1?$pair@$$CBVSymbol@@VSetlistRecord@@@stlpmtx_std@@QAA@XZ`** -- 1 membership(s), rule A

- `??1?$pair@VSymbol@@VSetlistRecord@@@stlpmtx_std@@QAA@XZ`

**`0x825bf2a8` `??1LeafSortNode@@UAA@XZ`** -- 1 membership(s), rule A

- `??1SongSortNode@@UAA@XZ`

**`0x825efbc8` `??0Callback@Leaderboard@@QAA@XZ`** -- 1 membership(s), rule A

- `??1Callback@Leaderboard@@UAA@XZ`

**`0x82659160` `??1DestructiveTransitionEvent@@UAA@XZ`** -- 1 membership(s), rule A

- `??1UIEvent@@UAA@XZ`

**`0x82681180` `??0MicManagerInterface@@QAA@XZ`** -- 1 membership(s), rule A

- `??1MicManagerInterface@@UAA@XZ`

**`0x826e34d0` `??0HitSink@@QAA@XZ`** -- 1 membership(s), rule A

- `??1HitSink@@UAA@XZ`

**`0x82735798` `??0RndShaderBuffer@@QAA@XZ`** -- 1 membership(s), rule A

- `??1RndShaderBuffer@@UAA@XZ`

**`0x82793fb8` `??0SlotChannelMapping@@QAA@XZ`** -- 1 membership(s), rule A

- `??1SlotChannelMapping@@UAA@XZ`

**`0x8279b270` `??1BeatMatchController@@UAA@XZ`** -- 1 membership(s), rule A

- `??1JoypadController@@UAA@XZ`
