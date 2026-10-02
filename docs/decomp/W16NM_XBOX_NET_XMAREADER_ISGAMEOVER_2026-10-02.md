# W16-NM — the Xbox net code W16-NK left in `auto_03_823ED458`, the XMAReader under ContextChecker's pin, and the IsGameOver carve (2026-10-02)

**Branch** `w16-nm`, rebased onto main `a9ea11644` (W16-NN). **Ruler** `name_check` (graded;
`report.json` `provenance.diff_config`). **Brief** (from `W16NK_GAME_ANON_ROWS_2026-10-02.md` §6/§8):
the rest of XboxServer, `InviteAcceptedMsg` and XSessionData in the unidentified unit at
`0x823ED458`; ContextChecker pins that held XSessionData and XMAReader code; `TrackPanel::IsGameOver`
(`0x82B90770`) carved by dtk into two 12-B rows. No oracle in either sibling repo: `../dc3-decomp` has no
XboxServer / XSessionSearcher / XboxSession / XSessionData / XMAReader (its map has none of those names),
and rb3-Wii's `Server_Wii.cpp` is an 11-line ctor. Every body below is written from retail asm.

## 1. Whole-binary A/B

`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16nm-ab --patch ab.patch` — the whole branch diff
against main `a9ea11644`, docs excluded. `ab_measure` refuses a patch that touches `symbols.txt`, so leg A's
base commit carries the branch's `symbols.txt` hunk (the three carve fixes of §4) — W16-MC's recipe. The
IsGameOver *name* is a map row and is in the patch, so its +1 is in the Δ; the carves themselves are in
both legs. Run dir `~/tmp/wt-w16nm-ab/.ab_measure_runs/20261002-135416-ab-414692/`.

```
leg A: matched=51162 masked=24557 honest=26605 code%=54.260452  (recompiles: 0, settled)
leg B: matched=51268 masked=24604 honest=26664 code%=54.343086  (recompiles: 337, split=1, settle iterations: 2)
split fixed point: leg A converged after 0 extra re-split(s), leg B after 0
Δmatched=+106  Δmasked_equal=+47  Δhonest=+59  Δcode%=+0.082634pp  Δcode_bytes=+8468
Δfuzzy=+0.120172pp   (legA 60.172770 -> legB 60.292942)
unit improvements: Server +37, SessionSearcher_Xbox +28 (new, at 100), NetSession_Xbox +21,
                   XMAReader +21, TrackPanel +1
unit REGRESSIONS: ContextChecker -2 (58->56)
units at 100% [mpn]: 473 -> 474 (SessionSearcher_Xbox, NEW_UNIT; 0 fell off)
units at 100% [all-rows-fuzzy]: 419 -> 419
[control none] +8,508 B -- NOT_APPLICABLE (source + map + splits in patch)
```

**Prediction, written before the run:** about +105 functions / about +8,400 B (in-tree 51,162 → 51,268,
less IsGameOver, which I wrongly placed wholly in leg A). **Measured +106 / +8,468 B.** The missing 1 is
IsGameOver: its name is in the patch.

**Row level, the two archived legs each resolved through its own map, keyed by address: 118 rows up,
0 down, 0 gone, 0 new.** ContextChecker's −2 is not a row going down: the two XMAReader EH funclets at
`0x82B6A688` / `0x82B6A914` read 100 under ContextChecker's old pin and read 100 under XMAReader's, so
they changed unit, not score.

**Δhonest (+59) is smaller than Δmatched (+106)** because 47 of the new matches are EH funclets (and
`$4` thunks) that pair by masked byte signature (`masked_equal`). That is what those rows are; the
named bodies are in the +59.

## 2. What the region is

Read off retail `.rdata`, scanned for every vtable that points into `0x823EC418`–`0x823EF5F8` (the COL
before each vtable gives the class):

| vtable | class | own `.text` |
|---|---|---|
| `0x820577BC` / `0x82057764` | `Server` | generic code at `0x823F5C98`; the shared dtor/getters at `0x823EC418` |
| `0x8205793C` / `0x820578E4` | `XboxServer` | `0x823EC638`–`0x823EDE08` |
| `0x82057EC4` / `0x82057E6C` | `XSessionSearcher` | `0x823EDE08`–`0x823EEAB8` |
| `0x82058074` | `SessionData` | `??_G` at `0x823EEAB8` (shared with XSessionData) |
| `0x820582A4` | `XSessionData` | `0x823EF2C8`–`0x823EF590` |
| `0x8205848C` / `0x82058434` | `XboxSession` | from `0x823EEB00`, running on to `~0x823F0A00` |

So `auto_03_823ED458` was not one unknown TU but the tail of XboxServer plus XSessionSearcher plus the
head of XboxSession. ContextChecker's `0x823EF498`–`0x823EF5F8` block was XSessionData's `New`/`Save`/
`Load` and a DataNode func; its `0x82B6A384`–`0x82B6AA90` block was XMAReader (vtable `0x82197138`,
COL → `.?AVXMAReader@@`).

## 3. Pins

| unit | `.text` | was |
|---|---|---|
| `Server.cpp` (+2 blocks) | `0x823ECD58`–`0x823ED1E8`, `0x823ED458`–`0x823EDE08` | `auto_03_823ECD58`, `auto_03_823ED458` |
| `network/net/SessionSearcher_Xbox.cpp` (new) | `0x823EDE08`–`0x823EEAB8` | `auto_03_823ED458` |
| `network/net/NetSession_Xbox.cpp` (new) | `0x823EEAB8`–`0x823EF5F8` | `auto_03_823ED458` + ContextChecker |
| `system/synth_xbox/XMAReader.cpp` (new) | `0x82B6A384`–`0x82B6AA10`, `0x82B6AA90`–`0x82B6B4F0` | ContextChecker + `auto_03_82B6AA90` |

ContextChecker keeps `0x82B6AA10`–`0x82B6AA90`: the `push_back<WeightedEntry>` row there was already at
100 and is the fold survivor of XMAReader's own `push_back<XMA_PLAYBACK_INIT>` (§6), so moving it would
only have traded a working pairing for a respelling. `.pdata` was re-derived by the split.

**Deliberately not re-pinned:** the XboxSession code from `0x823EF5F8` to `~0x823F0A00` sits under
`CheatProvider.cpp` (`0x823EF5F8`–`0x823F0310`) and `CharIKSliderMidi.cpp` (`0x823F0310`–`0x823F0A00`).
Those pins pair only EH funclets by byte signature today (several rows at 99.x–100), and re-homing an
already-pinned address is not neutral (CLAUDE.md, pin neutrality). A follow-up lane that writes the rest
of XboxSession should move them with it. ContextChecker's `0x823F4268`–`0x823F43A0` block is Quazal code
(vtable `0x8205942C`, callees in `0x82A9xxxx`), also left alone.

## 4. Carve fixes (`symbols.txt`)

| function | before | after | evidence |
|---|---|---|---|
| `TrackPanel::IsGameOver` `0x82B90770` | 12-B head, `subi` in no function, 12-B tail `0x82B90780` | 28 B | nothing in `.text` branches to `0x82B90780` and nothing in `.rdata`/`.data` points at it (scanned); 4 vtables (TrackPanel, GemTrack, Track, VocalTrack) point at `0x82B90770` |
| XMAReader `TableSum` `0x82B6A518` | 0x40, ended at a label one `blr` early | 0x44 | the `blr` is at `0x82B6A558` |
| `XMAReader::Seek` `0x82B6A738` | 0x40 + 0x28 + 0x20 (three rows) | 0x84 | one `lower_bound` with its tail; no branch from outside |

IsGameOver measured alone, in-tree after a forced re-split: the row reads 100, matched 51,161 → 51,162.
The carve hunk is in the A/B's leg-A base (§1), so its value is not in the Δ.

## 5. Source written

**XboxServer** (`net/Server_Xbox.h`, bodies in `Server.cpp`): Init, Login, Poll, LoginNextPlayer,
FindPadToLogin, the logout helper (`LogoutImpl`, retail `0x823EDA20`: Logout passes true, Terminate false),
Terminate, Logout, GetPlayerIDs, the client getters. Members named from use: `0x6c` credentials,
`0x70`/`0x74`/`0x78` login / per-pad login / logout call contexts, `0x7c` the LSP back end (sizeof `0xB0`),
`0x80`–`0x88` the match-making / persistent-store / competition clients. The Quazal callees are anonymous
in retail, so the Quazal-side declarations are made from their call sites with the retail address beside
each (`Server_Xbox.h`). `ServerStatusChangedMsg(bool)` and `UserLoginMsg(int)` ctors added.
`Server::GetCompetitionClient` now returns `Quazal::ServiceClient *` (was `int`).
`MatchMakingClient` padded to its retail sizeof `0x8C` (`li r3, 0x8c` in Poll).

**XSessionSearcher** (`SessionSearcher_Xbox.cpp`, sizeof `0xCC`): ctor, dtor, Poll (QoS probe of the
`XSessionSearchEx` results; invite-host probe), StartSearching, StopSearching, ClearSearchResults,
`OnMsg(InviteAcceptedMsg)`, `SessionSearcher::New`. The search property/context arrays and the size are
external globals: retail reaches each through its own relocation, where MSVC would co-address internal
statics off one base (CLAUDE.md memory, global co-addressing) — the static version read 81 %, the
external one 100.

**XSessionData** (`NetSession_Xbox.cpp`): CopyInto, Save, Load, Equals (session info only — the nonce is not
compared), `SessionData::New`.

**XboxSession** (`NetSession_Xbox.cpp`, the bodies inside this pin): SetInvitesAllowed / OnSetPublic
(`XSESSION_CREATE_INVITES_DISABLED` / `_JOIN_VIA_PRESENCE_DISABLED` through the flag helper
`0x823EF1E0`), PrepareRegisterHostSessionJob, Add/Remove Local/Remote ToSession, StartSession,
PrepareRegisterArbitrationJob. Members `0x74` session handle, `0x78` create flags.

**XMAReader** (`XMAReader.cpp`; layout from the ctor and dtor, sizeof `0x74` kept): ctor, dtor, `??_G`,
Seek, Done, `TableSum`, `FinishSeek` (`0x82B6A3E8`), `Init` (`0x82B6AA98`: header parse, two physical
`XMABuffer(phys)` buffers, one block per table entry, `XMAPlaybackCreate`), `DeleteAll<vector<XMAReaderBlock*>>`.

**XDK declarations added** (headers only): `XNQOS`/`XNetQosLookup`/`XNetQosRelease`,
`XSESSION_SEARCHRESULT(_HEADER)`, `XUSER_CONTEXT`, `XSessionSearchEx`, `XINVITE_INFO` (4-packed: retail
holds one at `XSessionSearcher+0x44` with its own members at `+0x98`), `XInviteGetAcceptedInfo`, two
`XSESSION_CREATE_*` flags, and `xdk/xaudio2/xmaplayback.h` (`XMA_PLAYBACK_INIT`, Create/Destroy/FlushData,
declared from their call sites).

## 6. Fold memberships (all `--chase`d)

`python3 tools/icf_pair_adjudicate.py --pairs <the 5> --chase --size`: **5 CHASED T1 PROVEN, 0 REFUTED**;
size gate (W16-NN's like-for-like) **ACCEPT** on all five.

| survivor address | our spelling | flat T1 | note |
|---|---|---|---|
| `0x824B06C8` `vector<unsigned>::erase` | `vector<XMAReaderBlock*>::erase` | PROVEN | 84 == 84 |
| `0x826C3888` (the `blr` hub) | `SessionSearcher::Poll` | UNDECIDABLE (vacuous) | VACUOUS-BUT-IDENTICAL; SessionSearcher's own vtable `0x82057374` slot 0 is this address |
| `0x823E3BE0` **new group** `NetSession::RemoveLocalFromSession` | `NetSession::RemoveRemoteFromSession` | REFUTED (template twin) | the one differing slot chases |
| `0x82272A60` `vector<Hmx::Object*>::_M_fill_insert` | `vector<XMAReaderBlock*>::_M_fill_insert` | REFUTED (twin) | one **CYCLE-ASSUMED** sub-slot: `_M_fill_insert_aux` calls itself (STLport's self-aliasing branch), checked SLOT-FOLD-OK one level down |
| `0x82B6AA10` `vector<WeightedEntry>::push_back` | `vector<XMA_PLAYBACK_INIT>::push_back` | REFUTED (twin) | the survivor sits inside XMAReader's TU |

## 7. Wrong names corrected and findings

- **`0x823EE630` was mapped `??_EHollaBackMinigame@@UAAPAXI@Z`** — a DC3 `hamobj` class on an RB3
  address. It is `??_GXSessionSearcher` (it calls the XSessionSearcher dtor at `0x823EDF78`); 100.
- **`0x82B5BBA8` is not the virtual `Synth360::RequirePushToTalk(bool, int)` the map calls it.** No
  vtable references it (scanned), and both XboxSession call sites pass `(pad, bool)`: `r4 = pad`,
  `r5 = 1/0`. MicManagerXbox's callee `0x82B5E470` also indexes per-pad with `r4`. The row reads 100
  only because the body is a pass-through, where argument order is invisible. AddLocalToSession and
  RemoveLocalFromSession stay at 96.1 / 95.7 for exactly this. **I did not swap the arguments to match**;
  the fix is to identify the real `(int, bool)` method (and where retail's virtual RequirePushToTalk is).
- `0x823EC840` (4 B, `b qResult::Equals`) is reached only from Quazal code (`0x82A9E7E8`, `0x82AC9D08`);
  it sits in this TU's range but nothing in this TU calls it. Left anonymous.

## 8. Rows left, by reason

| row | size | now | why |
|---|---:|---:|---|
| `XboxServer::Poll` | 1,088 | 97.07 | retail keeps the call-context state in a stack slot (stored twice, never reloaded) and shifts every `new` temp by 4; a const-ref binding did not reproduce it |
| `XboxServer::Init` | 404 | 99.93 | retail reuses one dead temp slot (`0x50`) where ours opens a new one: frame `0x80` vs `0x90` |
| `XSessionSearcher::Poll` | 912 | 99.71 | `add rD, base, off` operand order on the result-array address (three sites) and one register pair in the QoS loop |
| `XMAReader::Init` | 916 | 99.56 | one dead store of the stream count to a temp slot before `XMAPlaybackCreate` |
| AddLocal/RemoveLocal ToSession | 212 / 192 | 96.1 / 95.7 | §7, push-to-talk argument order |
| `XMAReader::Poll` `0x82B6AEB0` | 1,556 | 0 | drives the XMA HAL and five methods of the unidentified block class (dtor `0x82BBB2E8`, in the unpinned `auto_03_82BBABEC`); needs that class first |
| XboxSession `0x823EEB58` (job factory), `0x823EECF8`, `0x823EEB00`, `0x823EF400`, `0x823EF450`, `0x823EF590` | 76–376 | 0 | the factory's NetGameData slot calls disagree with `BandNetGameData.h`'s slot names (its own comment marks one slot as a guess); the others need Quazal / XDK callees whose roles I could not pin |

## 9. Gates

Run on the rebased tip after a full build (a forced re-split to a fixed point; `symbols.txt` unchanged
by the split beyond the three carve hunks):

- `python3 tools/map_name_injectivity.py`: **OK**, 33,777 applied rows, injective (+1 enumerated exception).
- `python3 tools/icf_alias_finder.py --validate`: **PASS**, 1,798 map-consistent, 290 tolerated,
  **0 contradicted**, 2,089 total.
- `python3 tools/icf_pair_adjudicate.py --chase --pairs` over all 5 W16-NM memberships: **5 PROVEN**
  (1 with the self-recursive CYCLE-ASSUMED sub-slot noted in §6); `--chasetest`: "selftest PASSED -- the
  instrument can both pass and fail".
- `python3 scripts/verify_objs_patched.py --verify-manifest`: OK (1,261 decomp, 3,090 target objects).
- `tools/native_build_gate.sh`: NATIVE_PLACEHOLDER

## 10. Traps met

- **zsh does not word-split** a quoted `"lo hi"` pair passed through a loop variable (CLAUDE.md); the
  helper got one argv and failed loudly, which is the good outcome.
- **`#include "Memory.h"` resolves to `src/xdk/LIBCMT/memory.h`** (LIBCMT precedes `src`, the match is
  case-insensitive); `PhysicalFree` needs `"../../Memory.h"`, as `rnddx9/Rnd_Xbox.cpp` does.
- **Function-local / file statics vs externals**: one relocation per object in retail means external
  globals; internal statics get co-addressed off one base and cost ~19 points on StartSearching.
- **The `.s` address column is synthetic for multi-block units** — every row listing here keys on the
  `fn_<addr>` name or the map, never the column (CLAUDE.md).
