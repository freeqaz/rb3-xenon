# DEPLOYWALK: autoplayed walks that deploy overdrive in rb3-app (2026-11-23)

Lane deploywalk, branch `deploywalk` (from `keyswalk` `1a7ab81f3`), worktree `~/tmp/wt-deploywalk`.
Scratch, logs and binaries: `~/tmp/deploywalk/`. The walk scripts and helpers are committed in
`tools/rb3_app_walks/deploywalk/` (copy them into `~/tmp/deploywalk/`, which they `cd` into;
`runb.sh` is `rb3-scout/a38/runb.sh` with `A=` and `X=` changed). Every run goes through
`coord/no610m.sh` under the shared `gpu1.lock`, with a fresh reflink copy of `memcard/content_a`.

## Goal

Every walk so far filled energy and never deployed it (`energy 1.0 deploy 0`). This lane makes the
autoplayed walks deploy, on vocals, guitar, drums and the 4-pad band walk, and looks for native
bugs on the deploy path: the deploy, the doubled multiplier, energy drain, the band bonus level /
multiplier, crowd, and band saves.

## What changed

| commit | what |
|---|---|
| `63dba82e1` | walk options `RB3_APP_AUTODEPLOY`, `RB3_APP_AUTOPLAY_SKIP`, `RB3_APP_DET_OD` (`native/src/rb3_app_determinism.cpp`); **fix**: `VocalPlayer::OnMsg(ButtonDownMsg)` takes the retail body in rb3-app |
| `3eff92d12` | rb3-app's Player sends retail's `send_update_energy` (SetEnergy and Poll's 2 s broadcast); walk scripts |

- **`RB3_APP_AUTODEPLOY=1`** (needs `RB3_APP_AUTOPLAY`): at the start of every frame, each local
  autoplayed **GemPlayer** that `CanDeployOverdrive()` is sent `deploy_if_possible`, Player's own
  handler (`DeployBandEnergyIfPossible(false)`), the message retail `Game::OnMsg(ButtonDownMsg)`
  sends every player for its R2 action. Each one logs `rb3-app-det: autodeploy frame N pI track T
  energy E -> deploying D`. Vocal players are not touched: they deploy from input (below).
- **`RB3_APP_AUTOPLAY_SKIP=<pad digits>`**: users on those pads are not autoplayed (so they miss,
  fail and become saveable).
- **`RB3_APP_DET_OD=1`**: the det line also carries, per player, `uni/od/deploys` (the band walk's
  fields) and `mult M (i1,i2,i3) ind I faildep F saved S` (`Player::GetMultiplier(true,...)`, the
  individual multiplier, `Stats::mFailedDeploy`, `Stats::mPlayersSaved`), and the band's
  `bmult/bonus/crowds/unison` fields even without extra pads. Unset, the line is unchanged
  (control C below).
- Walk scripts: `walk_od.txt` is `a38/walk1.txt` plus `wait_screen game_screen` and a Select
  press every 30 polls from +1500 to +8400 (song 1 only); `p1_od/p2_od/p3_od.txt` are the a38 band
  pads plus Select every 300 polls; `p3_save.txt` also presses DOWN before the difficulty A
  (medium: easy cannot fail, `Player::CheckCrowdFailure`).

## Findings

### 1. Autoplay cannot deploy from input on a gem instrument (retail; harness stand-in added)

Prediction: Select on an autoplayed guitar deploys (ButtonGuitarController::OnMsg ->
ForceMercurySwitch -> GemPlayer::MercurySwitch -> DeployBandEnergyIfPossible). **Wrong.** Walk
g0 (bin-k3, keyswalk): 23 Select presses reached the game (`button 8 action 5` in the input
log), 0 deploys. Cause, retail: `BeatMatcher::ForceMercurySwitch` (0x82791490) opens with
`lbz r11,0xb1(r3)` (mAutoplay) / `bne` past the whole body, as our source's `if (!mAutoplay)`
says; `MercurySwitch` (0x827913D8) has the same test, and the drum input handlers
(BeatMatcher.cpp:152/173/184) too, so autoplay plays no fills either. So retail autoplay never
deploys on guitar, bass, keys or drums. Not a port bug; `RB3_APP_AUTODEPLOY` stands in for the
input, and everything after the input (the deploy itself onward) is retail code.

### 2. A gamepad singer could never deploy natively (fixed: VocalPlayer::OnMsg, 0x826E64A0)

`VocalPlayer::OnMsg(ButtonDownMsg)` was `#ifdef HX_NATIVE return 0;` for every native target
("no UI headless"). rb3-app has TheUI, the overshell and a real VocalTrack, so the premise is
false there, and the stub dropped the tambourine button, the volume buttons and the
Select-in-freestyle deploy. Retail bytes (`tools/va_disasm.py 826e64a0 560`) are the `#else`
body: user test (0x825150C8), FocusPanel vs TheGamePanel / `__RTDynamicCast` PracticePanel,
overshell FindSlotForUser (0x825B3380) IsLeavingOptions / IsHidden, GetButton, the
`mUnkTU5_movieSync` R1/R2 skip (`lbz 0x2f`, buttons 3 and 1), tambourine at +0x3D0
(0x826F9878 / 0x826FB650), volume (0x826E5FB0 and the inlined step), then `cmpwi r31,8`
(kPad_Select) -> vtable 0x158 InFreestyleSection -> 0x190 DeployBandEnergyIfPossible(0). Fix:
the stub is `HX_NATIVE && !RB3_NATIVE_APP` (the pattern of this file's other rb3-app arms); the
match build compiles the same `#else` arm as before, so its preprocessed output is unchanged
(not priced with `ab_measure`: the two `#if` changes are identities when `HX_NATIVE` is undefined).

Measured on the same `walk_od.txt` vocal walk (12,000 frames, det + autoplay): bin-d0 (keyswalk
tip) **0** frames with `deploy 1` (vc0, see Runs); bin-d2 (this branch) deploys three times in
song 1 (v2: `deploys 3`, mult 4 -> 8 `(4,1,2)` while deploying, `faildep 0`: a Select outside a
freestyle section does nothing, as retail). Song 1 score 30,000 -> 34,188, 39/39 phrases both.

### 3. The deploy path itself is retail-faithful on every instrument (no further bug found)

Every function on the chain is a 100%-matched row in `build/45410914/report.json` (fuzzy and
mpn 100: DeployBandEnergyIfPossible, LocalDeployBandEnergy, PerformDeployBandEnergy, Deploy,
UpdateEnergy, SubtractEnergy, StopDeployingBandEnergy, CanDeployOverdrive, GetMultiplier,
GetIndividualMultiplier, PollMultiplier, Band::DeployBandEnergy / UpdateBonusLevel / Poll /
AnyoneSaveable / SaveAll, Player::Save / Saveable, Stats::DeployOverdrive /
StopDeployingOverdrive / AddToPlayersSaved, GemPlayer::MercurySwitch / FillComplete). Natively
the walks show the chain doing what that code says:

- **Deploy and doubled multiplier** (d2, drums, frame 2137): energy 0.5020 -> deploy 1,
  `mult 4 (4,1,1)` -> `8 (4,1,2)`, `bmult 2/2 bonus 1/4`. Points per gem 100 -> 200 while
  deploying (25 x 8 vs 25 x 4).
- **Drain**: 0.5020 -> 0 in ~203 frames (6.8 s, 16 beats: `mDeployBeats`), then
  StopDeployingBandEnergy: deploy 0, mult back to 4, `bonus 0/4`, `bmult 1/2` (max kept).
- **Band bonus level** (4-pad walk b2/FA): bonus levels 1, 2 and 3 all reached (1,110 / 390 /
  1,635 frames), `bmult` max 6; never 4/4 (the singer's freestyle deploys do not line up with
  the gem players'). A player's multiplier is streak x band multiplier: bass reaches
  `mult 36` (6 x 6), the singer 24 (4 x 6).
- **Song 1 scores** (det + autoplay, same walks with and without deploys): guitar solo
  13,295 -> 16,795 (3 deploys, 140/140 gems both); drums solo 37,275 -> 61,475 (6 deploys; hits
  387 -> 498, because while energy < 0.5 drum fills are off and the fill gems are scored instead
  of ignored); vocals 30,000 -> 34,188 (3 deploys); band 170,122 (bandwalk record) -> **434,380**
  (singer 63,714, guitar 79,691, drums 180,775, bass 110,200; deploys 4 / 9 / 10 / 10).
- **Crowd**: crowd stays 1.0 on the autoplayed players throughout.
- **Band saves** (s2 / FX, `p3_save.txt`, `RB3_APP_AUTOPLAY_SKIP=3`, bass on medium): bass
  fails at frame 2385 (`en 1`); its crowd rating then runs negative (-1.3 by 2853:
  PollEnabledState's `-(mCrowdLossPerMs x (players-1)) x elapsed` display value, retail);
  at 2859 the guitarist's autodeploy **saves** it (`saved 1`, `deploying 0`: the save costs
  `mSaveEnergy` instead of deploying), the drummer's deploy in the same frame is an ordinary
  deploy (`bonus 1/4`); bass `en 2` (being saved) -> `en 0` at 2965, crowd reset to 0.86. A
  second save at ~3900; after the third failure the band crowd falls to 0.2 and the game goes
  to `lose_screen` at frame 5301 (the walk has no presses for it). The failed bass's Select
  presses set `faildep 1` (energy 0, a failed deploy, as retail).
- Not a bug, noted: a solo player's overdrive phrase that the chart marks unison awards energy
  twice (0 -> 0.502 with `od` +1): `LocalHitLastGem` runs OneTrackCompletedPhrase, then, since
  `GetCommonPhraseTracks` is masked by `TheGame->GetScoringTracks()` (only the soloist's track),
  AllTracksCompletedPhrase too. Both are matched code; I did not check retail on a console.

### 4. Player's energy broadcast was skipped in rb3-app (changed: 0x826A50F0, Poll)

`Player::SetEnergy` and `Player::Poll` send `send_update_energy` (retail SetEnergy
0x826A50F0..0x826A5138 builds the message and calls the handler through the vtable) and both
were `#ifndef HX_NATIVE` ("its handler reads the BandUser's track, which no native headless
player has"). rb3-app's players have both, so they now send it there
(`!HX_NATIVE || RB3_NATIVE_APP`). Measured: the five d2 discovery walks are identical to d1
(without it) over all 12,000 frames (`tools/rb3_app_det_cmp.py`), with no FAIL: offline the
script handler changes no hashed state. Kept for fidelity (the handler is script, compiled in
the ark; I could not read it).

## Runs

Binaries: bin-k3 = keyswalk's `bin-k3` (built before keyswalk's rebase onto main, so it lacks
apparms' vocal display/sync arms: its band walk splits from bin-d0's at frame 1826 on the singer's
crowd; it was only used for the first discovery set); bin-d0 `b8bb2764675f` (keyswalk tip `1a7ab81f3`);
bin-d1 `64fddac1e558` (`63dba82e1`); bin-d2 (`3eff92d12`) and bin-asan-d2 `870207961e66`
(`-DRB3X_SANITIZE=address,undefined`). Discovery walks are 12,000 frames, `disc.sh` (g guitar,
d drums, v vocals, b 4-pad band, s band save), all `RB3_APP_DETERMINISTIC=1 RB3_APP_AUTOPLAY=1`.

| run | binary | env | result |
|---|---|---|---|
| g0 d0 v0 b0 | bin-k3 | 300-poll Select walks | 0 deploys anywhere (finding 1; v0 by finding 2) |
| g1 d1 v1 b1 s1 | bin-d1 | `DET_OD AUTODEPLOY` (+ save variant) | deploys on all five; numbers above |
| g2 d2 v2 b2 s2 | bin-d2 | same | **IDENTICAL** to the d1 runs over all 12,000 frames each; 0 FAIL in g/d/v, the 3 known NetSession 763 in b/s |
| vc0 | bin-d0 | vocal `walk_od.txt` (same script as v2) | **0** frames with `deploy 1` (v2: 962), rc 0 |
| Cd0 vs Cd2 | bin-d0 vs bin-d2 | bandwalk's own band walk (`walk1.txt` + `p1_guitar/p2_drums/p3_bass`), none of this lane's env (`ctl.sh`) | **IDENTICAL over 12,000** frames: without the new env the branch changes nothing in the det record |
| Ck3 vs Cd2 | bin-k3 vs bin-d2 | same | first difference frame 1826 (bin-k3 is pre-rebase, see above; not this lane's change) |
| FA | bin-d2 `be888682292a` | `runs.sh F`: 4-pad deploy band walk | 53,676 frames (rc 4, the 540 s cap: eight runs shared the GPU lock); = b2 over its 12,000 |
| FB | bin-d2 | same | vs FA: **IDENTICAL over 52,822** |
| FP | bin-d2 | same, `GLIBC_TUNABLES=glibc.malloc.perturb=190` | vs FA: **IDENTICAL over 51,676** |
| FS | bin-asan-d2 | same, ASan+UBSan (gemwalk's options) | vs FA: **IDENTICAL over 18,010** (all of song 1 and its endgame); 0 ASan reports; 22 UBSan file:line sites, **all 22 already in the bandwalk/gemwalk/keyswalk site lists** (`comm -23` empty) |
| FX | bin-d2 | save walk (`p3_save.txt`, `AUTOPLAY_SKIP=3`) | 59,054 frames; = s2 over its 12,000; ends on `lose_screen` |

- FAIL lines: only the three known `NetSession.cpp:763` guest-join asserts per band run; 0 in the
  solo runs; 0 UNREACHED stubs anywhere. The 29 autodeploy lines of FA are the gem players'
  deploys in song 1 (9 + 10 + 10); the singer's 4 deploys are its own Select presses.

## Excluded areas (BandCharacter.cpp, src/system/ui, rndobj, char: not edited)

- Nothing found there on the deploy path; no UBSan site in those areas is new (FS).
- Correction to bandwalk finding 5 for this base: the first-gig vignette is no longer skipped
  (`src/system/ui/UI.cpp:421` now shows it). FA goes `coop_endgame` -> `(none)` (36 frames) ->
  `campaign_rewardvignette_firstgig` (779) -> `accomplishments_newaward` -> `song_select` ->
  `part_difficulty`, and waits there for the rest of the run: the extra pads' song-2 presses
  (written for the old flow) have already fired, so nobody picks a part. A walk-script issue,
  not chased (`rb3-scout/a38/p*_2.txt` look like song-2 variants).
- `net/NetSession.cpp:763` assert per guest join (bandwalk finding 2), 3 per band run.

## Not done

- `tools/native_health.sh --selftest` not run (brief: the coordinator runs it). Only rb3-app
  (plain + ASan) built. `ab_measure` not run (see finding 2).
- Retail behaviour of autoplay deploys was established from bytes, not by running retail.
- Vocals deploy only through Select in a freestyle section; the mic-loudness freestyle path
  (`Singer::AddToFreestyleDeployment`) needs mic energy above the scream threshold, which
  autoplay does not produce; not exercised.
- Song 2 has no Select presses; the band walk plays one song (its song-2 joins stall, above).
- Not landed; nothing pushed.
