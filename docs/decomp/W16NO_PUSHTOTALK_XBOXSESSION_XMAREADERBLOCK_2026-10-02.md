# W16-NO — the push-to-talk misname, the rest of XboxSession, XMAReaderBlock and XMAReader::Poll (2026-10-02)

**Branch** `w16-no`, on main `cb7613a70` (W16-NM; main has not moved). **Ruler** `name_check` (graded;
`report.json` `provenance.diff_config`). **Brief** (W16-NM §7/§8): identify `0x82B5BBA8`, which the map called
the virtual `Synth360::RequirePushToTalk(bool,int)`; move the XboxSession code still under the
CheatProvider / CharIKSliderMidi pins into `NetSession_Xbox.cpp`, written from retail asm; identify the block
class in `auto_03_82BBABEC` and write `XMAReader::Poll`; close the listed near-misses where retail bytes
show the defect. The XMA part ran as a sub-lane (`w16-no-xma`, merged into `w16-no` with `--no-ff`).

## 1. Whole-binary A/B

`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16no-ab --patch ab2.patch`. The patch is the whole branch
diff against main `cb7613a70`, with docs and `symbols.txt` excluded. `ab_measure` refuses a patch that
touches `symbols.txt`, so leg A's base commit carries the branch's one `symbols.txt` hunk (the FreeSpace
carve, §4); that is W16-MC's and W16-NM's recipe. Run dir
`~/tmp/wt-w16no-ab/.ab_measure_runs/20261002-145544-ab2-804818/`.

```
leg A: matched=51268 masked=24604 honest=26664 code%=54.343086  (recompiles: 0, settled)
leg B: matched=51328 masked=24620 honest=26708 code%=54.419518  (recompiles: 1027, split=1, settle iterations: 2)
split fixed point: leg A converged after 0 extra re-split(s), leg B after 0
Δmatched=+60  Δmasked_equal=+16  Δhonest=+44  Δcode%=+0.076432pp  Δcode_bytes=+7832
Δfuzzy=+0.081856pp   (legA 60.292942 -> legB 60.374798)
unit improvements: NetSession_Xbox +52 (21->73), Mic +7, XMAReaderBlock +7 (new), Synth +2,
                   PlatformMgr_Xbox +1, SessionMessages +1, XMAReader +1
unit REGRESSIONS: CheatProvider -8 (27->19), CharIKSliderMidi -3 (50->47)
units at 100% [mpn]: 474 -> 476 (NetSession_Xbox MATCHED_ROSE, XMAReaderBlock NEW_UNIT; 0 fell off)
units at 100% [all-rows-fuzzy]: 419 -> 421
[control none] +7,712 B -- NOT_APPLICABLE (source + map + splits in patch)
```

**Row level.** Both archived legs were resolved, each through its own map, and keyed by address:
**64 rows up, 0 down, 0 gone, 0 new**, and the rows reaching 100 sum to exactly +7,832 B. CheatProvider's −8
and CharIKSliderMidi's −3 are not rows going down. They are XboxSession EH funclets that read 100 under those
pins by byte signature and read 100 under NetSession_Xbox now: they changed unit, not score.

**The first run found one real row down, and it was fixed before this run.** The first A/B
(`…/20261002-145136-ab-787190`, same base) measured **+59 / +7,684 B with 1 row down**:
`MakeSessionJob::OnCompletion` (SessionJobs_Xbox, 148 B) went 100 → 99.86. It calls retail `0x823EF400`
through a placeholder declaration (`XboxSessionHelper_823EF400`). Naming that address `QosListenSetData`
turned a forgiven placeholder site into a checked one, and our TU still called the placeholder. The bytes
say the helper is one external function with two callers, not a file static. So `QosListenSetData` and the
Quazal key-register `0x82A8EEF8` (also a placeholder there) are now declared once in `net/XSessionData.h` and
called by name from both TUs.
**Prediction before the re-run:** +60 / +7,832 B (first run + OnCompletion's 148 B), 0 rows down.
**Measured:** exactly that.

**Δhonest (+44) is below Δmatched (+60)** because 16 of the new matches are EH funclets that pair by masked
byte signature (`masked_equal`).

## 2. `0x82B5BBA8` is `Synth360::ActivateLocalChat(int pad, bool on)`

| retail | what it does | evidence |
|---|---|---|
| `0x82B5BBA8` | `if (!mMics.empty()) MicManagerXbox::GetInstance()->ActivateLocalChat(pad, on)` | referenced by **no** vtable (`.rdata`/`.data` scan of `band.exe`: only its `.pdata` row); both callers are XboxSession (`AddLocalToSession` passes `(pad, 1)`, `RemoveLocalFromSession` `(pad, 0)`) and load `lbl_82E11ED0`, which is `TheXboxSynth` (the global BinkMovieSys_Xbox, FxSend and Mic load), not `TheSynth` |
| `0x82B5BC50` | `if (!mMics.empty()) MicManagerXbox::GetInstance()->RequirePushToTalk(b, i)` | **this** is the virtual `Synth360::RequirePushToTalk`: the Synth360 vtable slot at `.rdata 0x821949A0` points here, and its callee `0x82B5E0C8` is the mapped `MicManagerXbox::RequirePushToTalk` |
| `0x82B5E470` | `MicManagerXbox::ActivateLocalChat(int, bool)`: `unkc[pad]->ActivateProcessing(on)`, then under the crit-sec `receiver->unk8 = on` and, if the pad owned the shared (headset-less) mic and is going quiet, `unk18 = -1` | `unk8` is the flag `OnDataReady` tests before feeding a talker's chat to the shared mic |
| `0x82B5BC00` | `Synth360::GetChatData(int pad, void *buf, int max)` (returns -1 with no mics) | called only by XboxSession's voice pump (`Poll`, `0x823F06F0`) with a 100-byte stack buffer |
| `0x82B5F318`, `0x82B5EE50`, `0x82B5E2C8` | `MicManagerXbox::GetChatData` → `ChatReceiver::GetChatData` → `ChatReceiver::ReadLocalChat` | the read side of the same chain: XHV2 `IsLocalTalking` / `GetLocalChatData` (vtable `+0x38` / `+0x4c`) into the receiver's MemStream, handed out once 100 ms have passed (`lbl_82194CD0` = 100.0f) or a full buffer waits |
| `0x82B5E370` | `MicManagerXbox::AddRemoteChatData(const XUID &, const void *, int)` | appends to the matching 0x3F8-byte `ChatBuffer` (cap 1000), called by `XboxSession::OnMsg(VoiceDataMsg)` |
| `0x82B5F748` | `MicManagerXbox::RemoveRemoteMic` (declared, never written) | stop processing modes + unregister by 64-bit XUID, then `vector<ChatBuffer>::erase` (`0x82B5EF50`) |

Names other than `RequirePushToTalk` are ours: retail keeps none for these, and DC3 has no `(int, bool)` method
(its map has only the virtual `RequirePushToTalk`). The XHV2 header was wrong in three places the bytes show:
`StopRemoteProcessingModes` takes the XUID (`ld r4`), and `IsLocalTalking` / `GetLocalChatData` return
`BOOL` / `HRESULT` (signed `cmpwi`). `AddLocalToSession` 96.1 → **100** and `RemoveLocalFromSession`
95.7 → **100**; every renamed/new row in Synth and Mic reads 100.

## 3. The rest of XboxSession

**Re-pin.** `network/net/NetSession_Xbox.cpp` `.text` now runs `0x823EEAB8`–`0x823F0A00` (one block). The
CheatProvider block `0x823EF5F8`–`0x823F0310` and the CharIKSliderMidi block `0x823F0310`–`0x823F0A00` are
gone from those units; they paired only EH funclets by byte signature. `.pdata` was re-derived by the split.
This is a re-homing, so it is not pin-neutral, and it is in the A/B.

**What the region is** (read off the vtables W16-NM listed and the code itself): ctor `0x823EFD28`
(sizeof `0xCC`, `NetSession::New` at `0x823F0238`), dtor `0x823EFFA8`, `??_G` `0x823F0310` and the two `$4`
thunks on the Hmx::Object vtable `0x82058434` (`0x823EFF80` → `??_G`, `0x823EFF90` → `Handle` `0x823EFB68`),
then DeleteSession, FinishJoin, UpdateSettings, WriteStats, EndSession, Poll and `OnMsg(VoiceDataMsg)` from the
primary vtable, `OnMsg(SigninChangedMsg)` (`0x823EFAA8`, via `HANDLE_MESSAGE`), and the helpers below.

| retail | written as | note |
|---|---|---|
| `0x823EEB00` | `XboxSession::HasChatPrivilege(User *)` | `PlatformMgr::CanCommunicateWith` (`0x8251C830`, also new: `XPRIVILEGE_COMMUNICATIONS` / `_FRIENDS_ONLY`) on the user's OnlineID |
| `0x823EEB58` | `NewMakeSessionJob(bool host)` | flags `HOST?` \| `0x300` \| (`0x2e`, or `0x3e` when ranked) |
| `0x823EECF8` | `PrepareConnectSessionJob` | |
| `0x823EF400` / `0x823EF450` | `QosListenSetData` / `QosListenRelease` | `XNetQosListen(id, data, size, 0, data ? ENABLE\|SET_DATA : ENABLE)` / `(…, RELEASE)` |
| `0x823EF590` | `SetReliable` (DataFunc `set_reliable`) | writes the static bool at `0x82C6EC0B` (initial value 1) that the voice sender reads |
| `0x823F0108` / `0x823F02B0` | `IsMuted(User *)` / `CanTalkTo(User *)` | `XUserMuteListQuery` (`0x82A6AB38`) over the local users |
| `0x823F0360` | `SendToTalkers(const NetMessage &)` | one `NetMessenger::DeliverMsg` per remote machine, skipping machines with a muted or chat-barred user |
| `0x823F1D30` | `VoiceDataMsg(const void *, int, const User *)` | in SessionMessages' range; was 0 |

The Quazal callees (`0x82A8F3D0`, `0x82A8EEF8`, `0x82A8EC08`/`EC88`/`ECB8`, `0x82A8F7C0`/`F788`) are anonymous
in retail and declared in `NetSession_Xbox.cpp` from their call sites, with the address beside each.

**NetGameData's vtable was wrong.** Retail BandNetGameData (vtable `0x820D27FC`): slot 1 `return 4`, slot 2
GetEndGameStats (EndSession passes it a `vector<UserStat>&`), slot 3 `return 0x1000000E` (an XUser property
id: MakeSessionJob's `publicPropertyId`, and the property UpdateSettings sets to `mPublic`), slot 4 `return
0x45410914` (the title id, `PublicID`). Our header had a placeholder `GetNumPrivateSlotsAllowed` at slot 2.
It is now `PublicPropertyID` at slot 3. `UserStat` was an empty class; it is `{User*, team, score, view id}`
(stride 0x10), read off WriteStats and `WriteTrueSkillJob`'s property ids.

**Source shapes the bytes forced** (each measured):
- Retail keeps both QoS helpers out of line, so they are `__declspec(noinline)`, as at 80 other sites in
  the tree. Inlined, DeleteSession read 75.8 and FinishJoin 81.6; out of line both read 100.
- NewMakeSessionJob: one `__RTDynamicCast` after the branch is MSVC cross-jumping two casts. It needs
  `handle = …; data = dynamic_cast<…>(…)` in that order in each arm (84.7 → 100). A single cast after the `if`
  or two ternaries do not reproduce it.
- UpdateSettings: retail calls `mLocalHost->GetPadNum()` inside the custom-property loop and drops the
  result, passing the outer `pad`. That is the stripped `MILO_ASSERT` signature (99.0 → 100).
- `OnMsg(VoiceDataMsg)`: retail destroys the guid String separately on each return. That needs the
  failure path as an early `return false` before the success body (90.0 → 100).

**Result: `NetSession_Xbox` 73 / 73 rows at 100.**

## 4. XMAReaderBlock and XMAReader::Poll (sub-lane `w16-no-xma`)

- **The block class.** It is a byte FIFO over one owned, 0x20-aligned buffer:
  - members: size `+0`, read `+4`, write `+8`, full `+0xC`, buffer `+0x10`, owns `+0x14`; sizeof `0x18`;
  - no vtable, so retail has no RTTI name for it; kept as `XMAReaderBlock`;
  - XMAReader is its only user: the ctor is called only from `Init` with 0x10000, the dtor only through
    DeleteAll, and the other five methods only from `Poll`.
- **What it is not.** `fn_82BBABF8` at the head of `auto_03_82BBABEC` is called from UILabel; it is a
  different unit and is left alone.
- **Pin.** `system/synth_xbox/XMAReaderBlock.cpp` `.text 0x82BBB2E8`–`0x82BBB510`. All seven methods
  read 100.
- **Carve fix.** `FreeSpace` (`0x82BBB330`) was split by dtk into `0x2C` + `0x1C`. The tail at `0x82BBB35C`
  is reached only by FreeSpace's own `bge`, and leaf functions have no `.pdata`, so it is one `0x48`
  function (`symbols.txt`). The hunk is in the A/B's leg-A base (§1).
- **`XMAReader::Poll`** (`0x82B6AEB0`, 1,556 B) was 0 and now reads **99.85**, written from retail asm.

## 5. Fold memberships (all `--chase`d)

`python3 tools/icf_pair_adjudicate.py --pairs <the 3> --chase --size`: **3 CHASED T1 PROVEN, 0 REFUTED**, size gate
ACCEPT on all three. `--chasetest`: "selftest PASSED -- the instrument can both pass and fail".

| survivor | our spelling | flat T1 | retail witness |
|---|---|---|---|
| `0x823E30F0` `??1NewUserMsg` (**new group**) | `??1VoiceDataMsg` | PROVEN, 76 == 76 | `XboxSession::Poll` destroys its stack VoiceDataMsg with `bl 0x823E30F0` |
| `0x826A0CE8` `_Vector_base<StreakList>` dtor | `vector<UserStat>` dtor | PROVEN, 36 == 36 | EndSession's EH funclet `0x823F09A4` |
| `0x82803280` `_Vector_base<UIScreen*>` ctor | `_Vector_base<void*>` ctor | REFUTED (template twin) → chased PROVEN, 0 CYCLE-ASSUMED | `XMAReader::Poll` constructs its per-channel `vector<void*>` with `bl 0x82803280` |

## 6. Near-misses: what changed and what did not

| row | size | before | now | reason |
|---|---:|---:|---:|---|
| `XboxSession::AddLocal/RemoveLocalFromSession` | 212 / 192 | 96.1 / 95.7 | **100** | §2 |
| `XMAReader::Poll` | 1,556 | 0 | 99.85 | the seek-skip count is widened by zero-extension in retail (`clrrwi`); ~12 source forms give `extsw` (sub-lane) |
| `XMAReader::Init` | 916 | 99.56 | 99.56 | dead store of `unk24.size()` to a reused temp slot (`0x60`). It is not the `version < 2` `numStreams` (that lives at `0x54`), and `inits.size()` measured worse (97.9). Left |
| `XboxServer::Poll` | 1,088 | 97.07 | 97.07 | retail spills each call context's state to a stack temp (twice in the first block, once in the second) and never reloads it, which shifts every `new` temp by 4. `QuazalSession::Poll` compares `GetState()` directly and reads 100, so `GetState` itself does not force the temp. I found no source construct that the bytes name. Left |
| `XboxServer::Init` | 404 | 99.93 | 99.93 | slot order only: retail does keep a named `filter` String plus a by-value copy (making it a temporary fixed the frame size and broke the copy, 94.9). Left |
| `XSessionSearcher::Poll` | 912 | 99.71 | 99.71 | `add` operand order on the result address (×3) and one r10/r11 pair. `&p[i]` and `p + i` compile identically. Permuter-class. Left |

## 7. Gates

Run on the branch tip after a full build at a split fixed point:

- `python3 tools/map_name_injectivity.py`: **OK**, 33,820 applied rows, injective (+1 enumerated exception).
- `python3 tools/icf_alias_finder.py --validate`: **PASS**, 1,799 map-consistent, 290 tolerated,
  **0 contradicted**, 2,090 total.
- `python3 tools/icf_pair_adjudicate.py --chase --size --pairs` over all 3 W16-NO memberships: **3 PROVEN**;
  `--chasetest`: "selftest PASSED -- the instrument can both pass and fail".
- `python3 scripts/verify_objs_patched.py --verify-manifest`: OK (1,262 decomp, 3,093 target objects).
- `tools/native_build_gate.sh` (run last, on the final code): @@NATIVE@@

## 8. Traps met

- **`ninja` after a `splits.txt` edit fails once by design.** dtk rewrites the `.pdata` lines (derived output),
  and the split-current check stops the build so the rewrite is committed. The second build is a fixed point.
- **`static` + small + called once = inlined**, even at `/O1`. Retail did not inline the QoS helpers, so the
  tree's `__declspec(noinline)` is the lever.
- **zsh**: `tools/icf_pair_adjudicate.py --pairs` takes a *file*, not inline JSON (an inline argument fails
  with `FileNotFoundError`, loudly).
