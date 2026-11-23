# BANDWALK: a four-player autoplayed band walk in rb3-app (2026-11-23)

Lane bandwalk, branch `bandwalk` (from `gemwalk` `37e98abc9`), worktree `~/tmp/wt-bandwalk`.
Scratch, logs and binaries: `~/tmp/bandwalk/` (`runb.sh` is a copy of `rb3-scout/a25/runb.sh`
with `A=~/tmp/bandwalk`, `X=~/tmp/wt-bandwalk`; every run goes through `coord/no610m.sh` under
the shared `gpu1.lock`, with a fresh reflink copy of `memcard/content_a`; `runs.sh` is the run
set, `disc.sh` the short discovery walk, `cmp.py` the det-line comparison, `p1_guitar.txt`
`p2_drums.txt` `p3_bass.txt` the extra pads' scripts, `walk1.txt` a verbatim copy of
`a25/walk.txt`).

## Goal

The gemwalk walk (`GEMWALK_GUITAR_SCORING_WALK_2026-11-23.md`) plays one instrument on one pad,
so only one-player scoring had run. This lane adds opt-in extra native pads, joins them to the
band through the game's own overshell, hashes every player and the band-level state, and runs
the band walk for identity under plain, malloc-perturb and ASan+UBSan builds.

## What changed

| commit | what |
|---|---|
| `b3a8625d3` | `RB3_APP_EXTRA_PADS`; band-level and overshell state in the det line (only when extra pads are on) |
| `b3a1b0593` | **fix, half**: rb3-app runs VocalPlayer::HandlePhraseEnd's overdrive arbiter (retail 0x826EA014..0x826EA06C); inert alone |
| `a5a58b4bb` | **fix, other half**: rb3-app takes VocalPart::HandlePhraseEnd's retail spotlight-phrase arm (0x826F3370..0x826F3398) |

**Landing note (2026-11-23, rebase onto main `24024b599`).** Lane vocalod landed the same two-half fix
first (`VocalPart` 0x826F3370, `VocalPlayer` 0x826EA06C), by moving both functions' whole retail arm
to rb3-app (`HX_NATIVE && !RB3_NATIVE_APP`). On the rebase `a5a58b4bb` resolved to empty and was dropped,
and `b3a1b0593`'s `#ifdef RB3_NATIVE_APP` call to `HandleVocalPhrase` landed inside the
`!RB3_NATIVE_APP` arm, where it is dead code; the next commit removes it. The branch now changes no
`src/band3` file; rb3-app runs retail's arbiter through vocalod's arm. The 0/9 -> 9/9 measurement below
was taken with this branch's own pair.

- **`RB3_APP_EXTRA_PADS=KIND=SCRIPT[,KIND=SCRIPT...]`** (`native/src/rb3_app_joypad.cpp`) names
  pads 1, 2, 3 in order. KIND is `gamepad`, `guitar` or `drums`, the XInput subtypes
  `RB3_APP_JOYPAD_KIND` already gives pad 0 (bass is a guitar pad that picks bass in the
  overshell). SCRIPT is that pad's own script in a subset of the engine's MILO_INPUT_SCRIPT
  grammar (`wait_screen NAME`, `+N button`, `N button`, `#`, the engine's button names; a press
  lasts one poll, a wait gives up after 1800 polls, as in `JoypadNativeSource.cpp`'s
  executor). The pad's state goes through the same retail path as pad 0
  (`GetXinputSinceLastFrame` -> `ReadSingleXinputJoypad` -> `JoypadPollCommon`), so a press
  reaches the game as a ButtonDownMsg from local user N (JoypadResetXboxPC binds user N to pad
  N). The engine's script stays pad 0's: the shared engine is not touched. Unset, nothing new
  runs; a bad KIND or an unreadable script exits 2 at init.
- **Det line** (`native/src/rb3_app_determinism.cpp`): with at least one extra pad, every frame
  also hashes and prints the band's `mMultiplier/mMaxMultiplier` (`bmult`),
  `mBonusLevel/mMaxBonusLevel` (`bonus`), `mCrowdRatings` (`crowds`), the
  CommonPhraseCapturer (`unison dis/fin` track masks, `in` = mInUnisonPhrase, `last` phrase id,
  `phrases`, and the raw phrase-state array), each player's unison / overdrive phrases
  completed/count and deploy count (`uni`, `od`, `deploys`), each overshell slot's state id
  (`os`), and each local band user's track type, controller type, difficulty, overshell state
  and whether it has a Player (`uN tt ct diff os pl`). Without extra pads the line is
  byte-for-byte gemwalk's (control V below).

## Findings

### 1. Joining: the overshell's choose_profile, then part and difficulty

Discovery walk d1 (bin-w1, 4000 frames). Pressing A on pads 1-3 on `main_hub_screen` 20/25/30
polls after it settles (pad 0's first hub press is at +60) takes each slot from kState_Join (1)
to **kState_ChooseProfile (31)**, not kState_JoinedDefault: the guest has no profile. Slots are
not pads: pad 0 is overshell slot 2 (`overshell.dta` `joining_priority 0 3 2 1`). On
`part_difficulty_screen` the first A on a guest leaves choose_profile (first row,
`overshell_continue_without_profile`) straight into kState_ChoosePart (10); A picks the
part, kState_ChooseDiff (12); A the difficulty, kState_ReadyToPlay (14). The game starts once
all four slots are ready.

Prediction for pad 3: DOWN before the first A selects bass. **Wrong**: in choose_profile DOWN
moves onto pad 0's profile in `profiles.lst`, A asks kState_ChooseProfileConfirm (32), and the
next A (the swap) aborted the run, rc 134, on `UNREACHED stub [ring online-netsession]
Synth360::ActivateLocalChat(int, bool)`. Moving the DOWN after the profile A (it then moves
ChoosePart's list from guitar to bass) fixed it (d2): u3 `tt 2` (kTrackBass). The swap path
was not pursued (online voice chat, a 360-only ring).

### 2. Each join logs a native assert in NetSession::AddLocalUser (net game-start; not fixed)

`FAIL: NetSession.cpp Line: 763 Error: ThePlatformMgr.IsUserSignedIn(newUser)`, once per
guest. The source comment there already records the retail bytes: retail evaluates
`IsUserSignedIn` and discards the result (MILO_ASSERT is `((void)(cond))` in the match build),
so retail adds the unsigned guest; natively the assert is live and logs, and the run goes on
(the user is added). Excluded area (net game-start), listed below.

### 3. A four-player band scores (vocals + guitar + drums + bass)

Run A (bin-w1). Song 1 starts at frame 1010 (game_screen 1115) and ends at 9783; at frame
10415 (after FinalizeStats):

| player | part | score | hit / miss | stars | od phrases | unison phrases |
|---|---|---|---|---|---|---|
| p0 (pad 0, gamepad) | vocals (3) | 28,400 | 37 / 0 | 5.0 | **0/9** | 0/0 |
| p1 (pad 1, guitar) | guitar (1) | 34,022 | 334 / 0 | 5.0 | 16/16 | 7/7 |
| p2 (pad 2, drums) | drums (0) | 61,375 | 628 / 0 | 5.0 | 17/17 | 7/7 |
| p3 (pad 3, guitar) | bass (2) | 46,325 | 333 / 0 | 5.0 | 16/16 | 7/7 |

Band performer score 170,122 (= the four), 4.1249 stars, crowd 1.0, all four crowd ratings 1.0.
The three gem players' energy reaches 1.0; `deploys 0` and `bmult 1/1` throughout (autoplay
never deploys, as gemwalk found; so overdrive sharing through the band multiplier never runs —
`bonus 0/4`). Unison phrases ran: `in 1` on 178 frames, `fin` track masks 2, 8 and 0xa seen,
and each gem player completed all 7. `band acc` (mAccumulatedScore) stays 0, as in gemwalk's
single-player walk.

### 4. The singer completed 0 of 9 overdrive phrases (fixed: b3a1b0593 + a5a58b4bb)

Scorewalk's open item 3 (vocal energy stays 0) is now quantified: the singer hit every phrase
and completed **0/9** overdrive phrases. Cause: `VocalPlayer::HandlePhraseEnd`'s HX_NATIVE arm
skips the whole retail phrase-end tail on the premise that `mTrack` is a sentinel with no
`mTrackConfig`; since scorewalk (`4fde7ca18`) rb3-app hooks the real VocalTrack, so the premise
no longer holds there, and the skipped tail includes the overdrive arbiter. Retail bytes
(`tools/va_disasm.py 826e97f8`): 0x826EA014 tests bSpotlightPhraseHit (r26), then
ScoringEnabled (`lbz 0x3cc`, `bl 0x826A24F8`), `mTrack` (`lwz 0x30c`) non-null, `TrackNum`
(`bl 0x822740C8` on mTrack+0x28), then `bl 0x826ED5D0` = CommonPhraseCapturer::HandleVocalPhrase
(mCommonPhraseCapturer at 0x264, this, track, phrase id r19, hit r26). Fix: under
`RB3_NATIVE_APP` that one call runs, with retail's condition; the render and net leaves
(VocalTrack::OnPhraseComplete, LocalScorePhrase's VocalTrackDir feedback, send_score_phrase)
stay skipped. Other native programs keep the sentinel arm. The match build compiles the
`#else` arm, unchanged: `tools/ab_measure.py --revert` of each commit (shipped `name_check`
ruler, `none` control) reads **Δmatched +0, Δhonest +0, Δcode_bytes +0, Δfuzzy +0.000000 pp,
units at 100% 635 -> 635** for both (`~/tmp/bandwalk/ab_vocalpart.log`,
`ab_vocalplayer.log`, rc 0). A first `--pick HEAD` attempt was refused (the commit was already
applied), as it should be.

Prediction: with b3a1b0593 the singer completes its overdrive phrases. **Wrong**: the w2 runs
(bin-w2) are identical to A over all 90,000 frames (A2, B2, P2, S2), and V2 to V: the call
never ran, because `bSpotlightPhraseHit` needs `iSpotlightPhraseID != -1`, and
`VocalPart::HandlePhraseEnd`'s own HX_NATIVE arm forces `mSpotlightPhraseID = -1` ("no
BandUser, no TheGameConfig track table" -- true of the headless vocal harnesses, not of
rb3-app). Retail 0x826F3370..0x826F3398 (`tools/va_disasm.py 826f3068`): `bl 0x826A2600`
(GetUserGuid), `lwz 0x2418` TheGameConfig, `bl 0x82688780` (GameConfig::GetTrackNum),
`bl 0x826853E0` (SongDB::GetCommonPhraseID), `stw r3,0x90(r31)` (mSpotlightPhraseID), i.e. the
`#else` arm. a5a58b4bb gates that block `HX_NATIVE && !RB3_NATIVE_APP` (the HookupTrack
pattern). Discovery walk d3 (bin-w3, 11,000 frames) against A: first difference at frame
2636 (the first sung overdrive phrase), and at frame 10415 the singer has `energy 1.0000`,
`od 9/9`; every other field (all four scores, hits, streaks, stars, the gem players' od/uni,
band score 170,122, 4.1249 stars) is unchanged, as it should be with no deploy. The solo
vocal walk moves the same way (V3 vs V: first difference at frame 2116, energy 0.502 then
1.0), so scorewalk's open item 3 ("vocal energy stays 0") is closed by these two commits.

### 5. The band walk stops after song 1: native UIManager skips the first-gig vignette (ui; not fixed)

After song 1's `coop_endgame_screen` -> `meta_loading_continue_screen`, the band walk logs
`Skipping screen 'campaign_rewardvignette_firstgig_screen' on native (campaign not supported)`
twice, then the current screen is `(none)` with no transition for the rest of the run (frame
10418 on); the frames are cheap, so A/B/P reach the 90000-frame cap in ~127 s. The
single-player walks go `meta_loading_continue_screen` -> `accomplishments_newaward_screen`
-> `song_select_screen` instead: the four-player session earns the multiplayer first-gig
reward. The skip is `UIManager::GotoScreenImpl`'s HX_NATIVE block (`src/system/ui/UI.cpp:416`,
a DC3-inherited `strstr(name, "campaign")` early return), which returns without going
anywhere, so the flow has no screen. `ui` is excluded from this lane; listed below. So each
band walk plays one band song; the extra pads' song-2 presses time out and fire on the
`(none)` screen (deterministically, inside the hash).

## Runs (90000-frame cap, SECS=540 TMO=580, all `RB3_APP_DETERMINISTIC=1 RB3_APP_AUTOPLAY=1`)

Pad 0 is the default pad on `walk1.txt`; A/B/P/S add
`RB3_APP_EXTRA_PADS=guitar=p1_guitar.txt,drums=p2_drums.txt,guitar=p3_bass.txt`. bin-w1
`1ae5fab9a055` (tip `b3a8625d3`), bin-asan-w1 `aeff783074cd`
(`-DRB3X_SANITIZE=address,undefined`); bin-w2 `a20e9b25f31f` (tip `b3a1b0593`), bin-asan-w2
`2cf73f5e4b26`. Predictions: A = B = P; S = A; V = gemwalk V.

| run | binary | extra env | frames | vs A |
|---|---|---|---|---|
| A | bin-w1 | 4 pads | 90,000 | - |
| B | bin-w1 | 4 pads | 90,000 | **IDENTICAL over 90,000** |
| P | bin-w1 | 4 pads, `GLIBC_TUNABLES=glibc.malloc.perturb=190` | 90,000 | **IDENTICAL over 90,000** |
| S | bin-asan-w1 | 4 pads, gemwalk's ASAN/UBSAN_OPTIONS | 90,000 | **IDENTICAL over 90,000** |
| V | bin-w1 | default pad only | 64,514 | vs gemwalk V: **IDENTICAL over 45,818** (all of gemwalk V) |
| G | bin-w1 | `RB3_APP_JOYPAD_GATE=1`, default pad | - | `RESULT: JOYPAD GATE PASSED`, rc 0 |
| A2, B2, P2, S2 | bin-w2 / bin-asan-w2 | as A, B, P, S | 90,000 each | **IDENTICAL over 90,000** (b3a1b0593 inert alone, finding 4) |
| V2 | bin-w2 | default pad only | 55914 | vs V: IDENTICAL over the common prefix |
| G2 | bin-w2 | gate | - | PASSED, rc 0 |
| d3 | bin-w3 | 4 pads, 11,000 frames | 11,000 | first difference frame 2636 (the singer's first overdrive phrase) |
| A3 | bin-w3 `4b402a248b93` (tip `a5a58b4bb`) | 4 pads | 90,000 | w3 leg: - (= d3 over its 11,000) |
| B3 | bin-w3 | 4 pads | 90,000 | vs A3: **IDENTICAL over 90,000** |
| P3 | bin-w3 | 4 pads, perturb 190 | 90,000 | vs A3: **IDENTICAL over 90,000** |
| S3 | bin-asan-w3 `99126a98499c` | 4 pads, ASan+UBSan | 90,000 | vs A3: **IDENTICAL over 90,000**; 0 ASan, 28 UBSan sites, none new |
| V3 | bin-w3 | default pad only | 43,138 | vs V: first difference frame 2116 (the solo singer's first overdrive phrase); energy 1.0 from then on, 0 FAIL |
| G3 | bin-w3 | gate | - | PASSED, rc 0 |

- Last common A/B/P/S line: `frame 90000 state dc69e72b chain 9525df72641dd2d2`; A3/B3/P3:
  `frame 90000 state dc69e72b chain c25f5f5144c5e7cb` (same final state, different chain: the
  singer's energy is in the per-frame hash from frame 2636).
- S: 0 ASan reports; 26 unique UBSan file:line sites (`~/tmp/bandwalk/S.sites`), **all 26 in
  gemwalk's S or scorewalk's S2** (`prior.sites`; `comm -23` empty). The four-player path
  (overshell joins, NetSession::AddLocalUser, three GemPlayers and a VocalPlayer in one Game,
  the unison CommonPhraseCapturer) adds **no** UBSan site, so there was no new report to triage.
- S3 (both vocal fixes): 0 ASan reports, 28 unique UBSan sites (`S3.sites`), again all in
  `prior.sites`: the singer's overdrive path (HandleVocalPhrase, CommonPhraseCapturer,
  Player::CompleteCommonPhrase) adds none.
- FAIL lines: the three NetSession 763 asserts of finding 2 per band run; none in V or G.
  UNREACHED stubs: 0 in the run set (d1's ActivateLocalChat came from the profile swap the
  final scripts avoid).

## Excluded areas (not fixed, as briefed: Rnd/Draw/Text/Spotlight, ui, char, net sync/game-start)

- **ui**: `UIManager::GotoScreenImpl` (`src/system/ui/UI.cpp:416`) drops
  `campaign_rewardvignette_firstgig_screen`, leaving no screen after a multiplayer song
  (finding 5). This is what stops the band walk at one song; the next lane on it should
  check whether retail shows the vignette and what the native skip should do instead.
- **net game-start**: `NetSession::AddLocalUser`'s live `IsUserSignedIn` assert for a guest
  (finding 2); retail discards the check.
- **net / online**: `Synth360::ActivateLocalChat` is an aborting stub reached by a
  choose_profile swap (finding 1).
- Same UBSan sites as gemwalk/scorewalk in char/net/bandobj render; not reopened.

## Not done

- `tools/native_health.sh --selftest` not run (brief). Only rb3-app (plain + ASan) was built.
- Song 2 of the band walk (blocked by finding 5). Two- and three-player bands were not run
  separately (the four-player band covers 2-4 at the join and scoring layer; the scripts are
  independent, so dropping an item from `RB3_APP_EXTRA_PADS` gives a smaller band).
- Overdrive deployment and band-multiplier sharing are hashed but never exercised: autoplay
  never deploys. No script button deploys overdrive yet.
- Not landed; nothing pushed.
