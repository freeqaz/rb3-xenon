# HARMWALK: autoplayed vocal-harmony walks in rb3-app (2026-11-23)

Lane harmwalk, branch `harmwalk` (from main `7a05edbf4`), worktree `~/tmp/wt-harmwalk`.
Scratch, logs and binaries: `~/tmp/harmwalk/`. Scripts are committed in
`tools/rb3_app_walks/harmwalk/` (copy them into `~/tmp/harmwalk/`, which they `cd` into;
`runb.sh` is deploywalk's with `A=`/`X=` changed). Every run went through
`rb3-scout/coord/no610m.sh` under the shared `gpu1.lock`, with a fresh reflink copy of
`memcard/content_a`.

## Goal

Make rb3-app play a harmonies song with two and three vocal parts, then look for native bugs on
that path: multi-part phrase scoring, harmony phrase bonuses, unison/overdrive across parts, and
the 3-part stats.

## What changed

| commit | what |
|---|---|
| `3af20c4ae`, `b9249ec88` | `RB3_APP_DET_HARM=1` (`native/src/rb3_app_determinism.cpp`): the det line also carries the vocal player's harmony state; once per part load it prints each part's note list |
| `c85b08b67` + this one | walk scripts (`tools/rb3_app_walks/harmwalk/`) and this record |

No `src/` file changes. No fix landed: I found no native bug on the harmony scoring path (below).

- **`RB3_APP_DET_HARM=1`** adds `harm H pst T` (MetaPerformer::IsNowUsingVocalHarmony, the
  user's preferred score type; 4 = kScoreHarmony), per part `vpK ph I sc S/CAP rk R sp ID en E`
  (phrase index, mPhraseScore / mPhraseScoreMax, mPhraseRank, mSpotlightPhraseID, scoring flag),
  per singer `sK ap A as P` (autoplay part, last assigned part), and
  `hstat H dbl h/n tri h/n vpc p0/p1/p2` (Stats mHarmony, double / triple harmony hit / count,
  per-part percentages). All of it is in the hash. Outside the hash, whenever a VocalPlayer's
  parts (re)load: `rb3-app-det: vocal part K notes N phrases P with-notes W weights N sum S`.
  Unset, the line is unchanged (the brief's other walks do not set it).
- **`walk_harm.txt`** is deploywalk's `walk_od.txt` plus `+180 down` on `part_difficulty_screen`,
  which moves pad 0's part list from `overshell_vocal_solo` to `overshell_vocal_harmony`
  (`OvershellPartSelectProvider::Reload`, kControllerVocals). `runs_h.sh` is the run set,
  `disc_h.sh` the seed sweep, `harm.py` the summary.

## Findings

### 1. Harmony singers are the VocalPlayer's Singers, not band users (the extra-pad route does not apply)

The brief proposed extra gamepad pads as the extra singers. In the source (all matched rows) a
harmony song has **one** vocal band user and **one** VocalPlayer: `Band::NewPlayer` passes
`PlayerTrackConfigList::NumSingers()`, which returns 3, and each `Singer` is a mic
(`MicClientID(singerIndex, -1)`). `VocalPlayer::SetAutoplay` sets each singer's autoplay part to
its own index, and `Singer::Poll_` autoplays without a mic, so an autoplayed harmony vocalist
already sings all parts with three singers: singer 0 part 0, singer 1 part 1, singer 2 part 2
(silent on a 2-part song). The det line confirms it (`singers 3`, `s1 ap 1`, `s2 ap 2`).
Two vocal band users are also excluded by the overshell: `OvershellPartSelectProvider::IsActive`
greys out a part another slot holds, and `RepresentSamePart(kTrackVocals, kTrackVocals)` is true.

Probe `gp` (bin-h0, `RB3_APP_EXTRA_PADS=gamepad=p1_gpad.txt`): pad 1's A on the hub reached the
game (`poll 524 pad 1 type 2 controller analog button 6 action 1`), but its overshell slot never
left state 0, and u1 stayed `tt 10 ct 5` (pending, kControllerNone) for the whole run while pad 0
sang harmonies. Not pursued: why a second gamepad does not even reach kState_Join is in the
overshell's join script; harmonies do not need it.

### 2. Harmonies run, and all three parts score (no bug found)

Seed sweep (bin-h0, 3,000 frames, seeds 0-5): harmonies were on in every run (`harm 1 pst 4`);
seed 0's random song has 3 parts, seeds 1, 2, 3, 5 have 2. At 3,000 frames parts 1 and 2 read
`sc 0.0/0.0` throughout, which looked like a bug. **It is not**: the note-list dump shows the
harmony parts have notes (seed 1: part 1 has 200 notes in 20 of 38 phrases) and the first harmony
phrase is later than frame 3000. The full-song walks score every part:

| run | song | parts | max cap/score per part | singer-frames assigned | end of song |
|---|---|---|---|---|---|
| v0A (seed 0, solo) | 397/116/82 notes | 3 | 2491.9 / 5923.4 / 6073.3, each reached | s0 2167, s1 889, s2 935 | `hstat 300 dbl 10/10 tri 5/5 vpc 1/1/1 od 8/8 deploys 4`, score 36,008, 39 hit 0 miss, 5.0 stars |
| v1A (seed 1, solo) | 366/200 notes | 2 | 2128.8 / 2128.8 | s0 2347, s1 1231 | song 1 (frame 9834): `hstat 400 dbl 20/20 tri 0/0 vpc 1/1 od 9/9 deploys 4`, score 33,360 |
| b0A (seed 0 + guitar, drums, bass pads, AUTODEPLOY) | 366/200 notes | 2 | 2128.8 / 2128.8 | s0 2052, s1 1231 | singer `hstat 400 dbl 20/20 od 9/9 deploys 4` score 66,978; band 437,644, 5.0 stars, `bmult` max 6 |

- **Multi-part phrase scoring**: each part's phrase cap and score move independently and reach
  the cap; the phrase rank (`rk`) and singer-to-part assignment (`as`) follow retail's
  HandlePhraseEnd / FindBestPart, all matched rows.
- **Harmony phrase bonuses**: `dbl`/`tri` count every 2- and 3-part phrase (v0A 10 doubles of
  which 5 triples; 2-part songs 20/20), all hit under autoplay; `hstat` (mHarmony, AddHarmonyStat
  for parts ranked != 0) accumulates.
- **Overdrive across parts**: the spotlight phrase id comes from part 0 (`GetSpotlightPhraseID`),
  od 8/8 and 9/9 completed, and the singer deploys from Select in freestyle sections (4 deploys).
- **Unison**: the vocal player's `uni` stays `0/0`; its unison count is
  `TheSongDB->GetNumUnisonPhrases(vocal track)` (Player.cpp:1019), i.e. this chart has no vocal
  unison phrases. The gem players' unisons run as in bandwalk. I did not find a harmony song with
  a vocal unison phrase, so a vocal unison across parts is **not exercised**.
- **3-part stats**: `vpc 1/1/1` (Stats::SetVocalPartPercentage per part) and the
  double/triple counters above; the endgame screens ran with no FAIL in solo walks.

Static check (main's `build/45410914/report.json`): every VocalPlayer, CommonPhraseCapturer,
Band, Player row on the path is at 100%. The sub-100 rows I diffed with `bin/objdiff-cli` are
codegen-only: `VocalPart::CalcPhraseScoreMax` (fuzzy 76.96%: retail re-loads the phrase bounds
inside the loop, ours hoists them; same arithmetic, same Min/Max `fsel`s) and
`Singer::ResolveAmbiguity` (99.47%: commuted `lfsx`/`stfsx` index operands and one commuted
`fadds`). The HX_NATIVE arms on the path (VocalPlayer, VocalPart, Singer, CommonPhraseCapturer,
TambourineManager, GameMicManager) are either rb3-app-gated already
(`HX_NATIVE && !RB3_NATIVE_APP`) or render/debug-only.

### 3. Seed 4 crashes at boot in BandPatchMesh (bandobj render; not on the harmony path, not fixed)

`RB3_APP_SEED=4` dies at frame 116 (calibration) with
`FAIL: BandPatchMesh.cpp Line: 1143 Error: dest` then SIGSEGV (rc 139). Under ASan (run s4):
SEGV at 0xa0 in `RndTex::Width()` from `BandPatchMesh::WorkVerts::SetVertsAndFaces`
(BandPatchMesh.cpp:899) <- `Construct` (:646) <- `ProjectPatches` (:607) <- `PreRender` (:371)
<- `OutfitConfig::DrawPreClear` <- `Rnd::DrawPreClear`: the patch mesh's material has no diffuse
texture (`MILO_ASSERT(dest, 0x477)` is live natively; retail would dereference it too, so on the
360 the texture is there). The seed picks a character outfit with a patch whose material
texture is missing natively — a material/texture load question (rndobj/char territory, excluded).

### 4. A new UBSan site in the native keygen stub (engine repo; not fixed)

`milo-native-engine/src/platform/Keygen_Stub.cpp:63: signed integer overflow: 1405067833 *
1664526` in `random(int)` <- `KeyChain::getMasher` <- `VorbisReader::setupCypher`. These songs'
moggs take a keygen path the earlier walks did not. It is an LCG that relies on wrapping; the fix
(unsigned arithmetic) belongs in the shared engine tree, which this lane does not touch.

## Runs (det + autoplay + DET_OD + DET_HARM, 12,000-frame cap, `SECS=520 TMO=580`)

Binaries: bin-h0 `3160298b7e3e` (tip `3af20c4ae`, det fields), bin-h0b `c17a070ec645` (plus the
note-list dump), bin-asan-h0b `cdbab5af1cad` (`-DRB3X_SANITIZE=address,undefined`, same tree).
`runs_h.sh`: v0 (seed 0, solo, 3 parts), v1 (seed 1, solo, 2 parts), b0 (seed 0, harmony singer
plus deploywalk's guitar/drums/bass pads, `RB3_APP_AUTODEPLOY=1`).

| set | binary | extra env | v0 | v1 | b0 |
|---|---|---|---|---|---|
| A | bin-h0b | - | rc 0 | rc 0 | rc 0 |
| B | bin-h0b | - | **IDENTICAL over 12,000** | **IDENTICAL over 12,000** | **IDENTICAL over 11,195** (B ran beside P and was cut by the wall clock) |
| P | bin-h0b | `GLIBC_TUNABLES=glibc.malloc.perturb=190` | **IDENTICAL over 12,000** | **IDENTICAL over 12,000** | **IDENTICAL over 12,000** |
| S | bin-asan-h0b | deploywalk's ASAN/UBSAN_OPTIONS | **IDENTICAL over 12,000** | **IDENTICAL over 12,000** | **IDENTICAL over 12,000** |

(`tools/rb3_app_det_cmp.py` against A.) S: 0 ASan reports; 20 unique UBSan file:line sites
(`~/tmp/harmwalk/S.sites`), 19 of them in deploywalk's `allprior.sites`; the new one is finding 4.
The harmony path (three singers, three parts, AddHarmonyStat, the double/triple counters) adds no
UBSan site. FAIL lines: only the three known `NetSession.cpp:763` asserts per band run (bandwalk
finding 2); none in the solo walks. Discovery: `disc_h.sh` on bin-h0, seeds 0-5 at 3,000 frames
(seed 4 is finding 3), probe `gp` (finding 1).

## Excluded areas (listed, not touched)

- **bandobj/rndobj/char**: finding 3 (missing diffuse texture on a BandPatchMesh material, seed 4).
- **engine repo**: finding 4 (Keygen_Stub signed overflow).
- **overshell join script**: a second gamepad never joins (finding 1); not needed for harmonies.

## Not done

- No fix: none of the observed harmony behaviour differed from what the matched code says, and
  I cannot compare the numbers with a retail run.
- A harmony song with vocal unison phrases (vocal unison across parts) was not found or run.
- Song 2 of the solo walks was reached (v0 ends on `part_difficulty_screen` for song 2, v1 in
  song 2) but not analysed.
- `tools/native_health.sh --selftest` not run (brief). Only rb3-app (plain + ASan) was built.
- Not landed; nothing pushed.
