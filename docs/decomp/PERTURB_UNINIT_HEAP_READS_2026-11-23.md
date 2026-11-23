# Uninitialised native heap reads under `glibc.malloc.perturb` (2026-11-23)

Lane `perturb`, worktree `~/tmp/wt-perturb`, base main `a75e31b6a`. The method is
`FORCERELOAD_MERGER_INIT_2026-11-23.md`'s: run the same native binaries with default malloc and with
`GLIBC_TUNABLES=glibc.malloc.perturb=N`, diff behaviour, and trace each divergence to the member that is never
set. Scratch, scripts and logs are in `~/tmp/perturb/` (named below).

**Result: perturbing the heap produces no behavioural divergence on this tip.** This holds for the 18 headless
`native_health` targets and for the rb3-app walk, under both fills. The walk instrument was shown able to see
the worked example's byte (§3).

**Second instrument.** A perturb diff only sees reads that change output. memcheck with `--track-origins`
over 14 headless targets found **two heap reads of never-written members**, and both are retail-faithful.
Both are now zeroed natively (`caf143186`, `HX_NATIVE` only, X360 Δ0):
- `VoiceBeat::mVoiceEnergy` / `mFullBandEnergy` (§5.1).
- `VocalPart::unk38` (§5.2).

On silent-mic harness input neither read reaches an output, which is why perturb could not see them.

## 1. Fills, and a control that the fill reaches the binary

glibc's `perturb=N` fills each `malloc` return (not `calloc`) with `N ^ 0xFF` and each `free`d block with `N`.

| arm | tunable | malloc fill | free fill |
|---|---|---|---|
| p190 | `glibc.malloc.perturb=190` | 0x41 | 0xBE |
| p65 | `glibc.malloc.perturb=65` | 0xBE | 0x41 |

Natively, `operator new`, `PoolAlloc` and `MemAlloc` all reach `malloc` (MemMgr.cpp and PoolAlloc.cpp,
`HX_NATIVE` arms), so the fill reaches game objects. Every run goes through `coord/no610m.sh`, which is
`bwrap` (not setuid here, `-rwxr-xr-x root`). Control `~/tmp/perturb/ctl/m` mallocs 100 B and 200 KB and
prints a byte of each. With default malloc it prints `00 00`, under p190 `41 41` and under p65 `be be`, all
through `no610m.sh timeout`.

## 2. Headless targets (native_health's 18 runtime targets, its argv)

Script: `~/tmp/perturb/runh.sh <tag> [env]`. It uses the same inputs and arguments as `tools/native_health.sh`
lines 373-396, with `RB3_CONFIG_DUMP` per target. It skips strace and the file ledger. Arms: base1, base2
(default malloc), p190 and p65. `--selftest` was not run, per the brief.

- **rc:** 18/18 rc 0 in every arm. rb3-render gave `ALL GATES PASSED`, 414 PASS / 0 FAIL, in all four.
- **Logs:** `normlog.py` normalises tag paths, pointers and times, then diffs the multiset of lines. 17
  targets differ from base1 in **0 lines** in every arm (base2, p190, p65).
- **rb3-render:** base1 vs base2 already differ in 120 lines, because its `uj-venue`/`ul-venue` sample is
  random and many lines carry ms timings or poll counts. With those lines excluded, every remaining
  difference in p190 and p65 is the same timing class, so there is no content difference.
- **Byte outputs:** rb3-render's 11 PNGs, rb3-frame's PNG, and every `RB3_CONFIG_DUMP` and the ark
  `--config-dump` hash identically in all four arms (`cb33e69d49f6`, `3cd57ad02d39`, `ecf03302dffa`).

Divergences: **none**. The earlier sanitizer lanes' retail-faithful uninit reads show up here too (GameGem
words, `SongParser` TrackType, `mJumpSamplesInvalid`, `mIsShared`). None of them changes any output of these
targets.

## 3. rb3-app walk

Script: `~/tmp/perturb/runw.sh`, which is `a25/runb.sh` with `A=~/tmp/perturb` and `X=~/tmp/wt-perturb`. It
runs `a25/walk.txt` (the seeded memcard store) with SECS=540, TMO=580, the NVIDIA 3090 ICD under
`gpu1.lock` and `no610m.sh`.

### 3.1 The walk is not deterministic, even with default malloc

- **Songs:** every run picked a different song (table below), and so did six 1,200-frame Dawn Null runs
  (`VK_ICD_FILENAMES=/nonexistent`). Every one of these pairs differed in the song: nb1/nb2 (default),
  n190a/b and n65a/b.
- **Ordered logs:** `firstdiv.py` filters timing lines and finds the first ordered divergence. For every
  pair, base vs base included, it is on `splash_screen` at filtered line ~4,422 of ~7,000, in an async
  texture/mesh load interleaving.
- **Consequence:** the RNG draw count depends on wall-clock timing past the splash. A forcereload-style RNG
  shift would sit under that noise, so ordered or count diffs of the walk cannot discriminate.

### 3.2 Instrument used, and a positive control showing it can fire

- **Presence screen (`presence.py`):** report normalised, number-stripped lines present in every run of an
  arm and in no base run, and the reverse. Content lines that depend on the song or characters are
  filtered out. The screen fires on single runs: base1 vs base2 gives 30 lines, nb1 vs n65a 17.
- **Transition vignette:** which `tv3_<x>_screen` the song load goes through.
- **Positive control:** `rb3-app.ctl` is the tip with the two `HX_NATIVE` `mForceReload` inits removed from
  `FileMerger.h`, rebuilt for rb3-app only. The header was then restored and the clean binary put back
  (`cmp` equal to the saved copy).

| binary | malloc | runs | vignette |
|---|---|---|---|
| tip | default | base1-3, nb1, nb2 | `tv3_a` 5/5 |
| tip | p190 | p190, p190b, n190a, n190b | `tv3_a` 4/4 |
| tip | p65 | p65, p65b, p65c, n65a, n65b | `tv3_a` 5/5 |
| ctl (init removed) | default | cb | `tv3_a` 1/1 |
| ctl (init removed) | p190 | c190a, c190b | **`tv3_c` 2/2** |

The presence screen, ctl+p190 vs the four tip Dawn Null runs, gives 8 lines in every ctl run and in no tip run
(the `tv3_c` screen lines and a `smoke_machine.milo` prop), and 11 the other way. Tip p190 or p65 vs the four
tip baselines gives **0 lines both ways**. So the instrument sees the worked example's unset byte, and it sees
nothing on the tip.

### 3.3 Walk outcomes

| run | malloc | song | game_screen | song end | outcome |
|---|---|---|---|---|---|
| base1 | default | smokeonthewater | 18.7 s | 387.2 s | cap at 540 s on coop_endgame (see note a) |
| base2 | default | chinagrove | 15.7 s | 227.8 s | **SIGSEGV** leaving coop_endgame; 4 `RING CORRUPTION`, 2 `abstract ObjRef` |
| base3 | default | heymanniceshot | 19.8 s | 332.2 s | crossed into song 2 (song_select at the cap) |
| p190 | p190 | lastdance | 18.1 s | 247.9 s | **SIGSEGV** leaving coop_endgame |
| p190b | p190 | getfree | 21.2 s | 164.7 s | **SIGSEGV** leaving coop_endgame |
| p65 | p65 | roundabout | 18.6 s | — | cap at 540 s inside the song (see note b) |
| p65b | p65 | nooneknows | 20.8 s | 297.4 s | crossed into song 2 (part_difficulty at the cap) |
| p65c | p65 | needyoutonight | 19.4 s | 222.7 s | crossed into song 2 (song_select at the cap) |

base3 and p65b/c each print one `FAIL: store_preview_mgr already exists` on the song-2 path. It appears in
both arms.

- **(a)** base1's song outlasted the 24 chained 1,800-frame `wait_screen coop_endgame_screen` lines: the
  game started at frame 1,489 and the endgame came at frame 46,098. So the five A presses fired during the
  song (polls 44,482-44,962), and nothing pressed on coop_endgame. This is a walk-script artifact.
- **(b)** Roundabout is about 8.5 minutes long, and game_screen ran 521 s to the cap. This is not a hang.
  The headless rb3-song/rb3-midi/mogg gates are identical under p65, and the song-end timing of every other
  run matches its song's length.

### 3.4 The endgame-exit SIGSEGV (base 1/2, p190 2/2, p65 0/2 of the runs that reached the exit)

Both of this lane's retained cores (`coredumpctl`, PIDs 4059635 and 52910, the second being p190b) have the
same stack: `Hmx::Object::IsRingPrevAlive` ← `AddRef` ← `ObjOwnerPtr<CharClip>::operator=` ← `vector::_M_erase`
← **`CharClipGroup::Replace`** ← `ObjOwnerPtr<CharClip>::Replace` ← `Hmx::Object::ReplaceRefs` ← `~Object` ←
`~CharClip` ← `FileMerger::Merger::Clear` ← `AppendLoader` ← `StartLoadInternal`.

This is the A28/A29 open class: a ref freed or moved under `ReplaceRefs`' snapshot walk (a29 HANDOFF, "The
endgame ring crash"). Here `CharClipGroup::Replace` erases from its clip vector while the dying clip's ring
snapshot is walked, and the move-assign re-links refs whose neighbours are dead.

- **Fill dependence:** it reproduces with default malloc (base2), so it is not a perturb divergence.
- **Free fill:** the free fill can change how a stale pointer reads. 2/2 under p190 (free fill 0xBE) against
  0/2 under p65 (free fill 0x41) is suggestive but is 4 runs.
- **Verdict:** char area, out of this lane's fix scope. Listed, not fixed.

## 4. Divergences and verdicts

| # | where | observation | verdict |
|---|---|---|---|
| 1 | 17 headless targets | 0 normalised lines differ, both fills | no uninit-dependent behaviour |
| 2 | rb3-render | only timing and random-venue lines differ; PNGs identical | no uninit-dependent behaviour |
| 3 | rb3-app walk p65 | song never ended | **not a divergence**: Roundabout outlasts the 540 s cap |
| 4 | rb3-app walk base1 | never left coop_endgame | **not a divergence**: walk-script presses fired during a long song |
| 5 | rb3-app endgame exit | SIGSEGV in `CharClipGroup::Replace` under `~CharClip` ReplaceRefs | **char, listed**: A28/A29 ring class, also with default malloc |
| 6 | rb3-app song pick, vignette, presence screen | no line consistent across a perturb arm and absent from base | none on the tip (control: the forcereload byte flips the vignette) |
| 7 | memcheck: `VoiceBeat::mVoiceEnergy` +0x128, `mFullBandEnergy` +0x130 (heap, `new TalkyMatcher` in `Singer::Singer`) | read by `Analyze`; ratio drives `unk1` → `CouldScoreAgainstPart`, `GetBestHit` (rb3-harmony) | **retail-faithful; fixed natively** (§5.1) |
| 8 | memcheck: `VocalPart::unk38` +0x3C (heap, `new VocalPart` in `VocalPlayer::PostLoad`) | `Min(unk38, mPhraseScoreMax)` in `ScoreSinger` (rb3-harmony, rb3-vocal2) | **retail-faithful; fixed natively** (§5.2) |
| 9 | memcheck: `SongParser::OnEndOfTrack` / `SongData::OnEndOfTrack` / `NativeGemSink::OnEndOfTrack` | stack origin (the parser lives on `main`'s stack); 9 targets | the known TrackType read (asanwalk N6, retail-faithful); not heap, not changed |
| 10 | memcheck: a `VocalPlayer::FindBestPart` stack local → `SendVocalState`, `PackFloats`, `Clamp`, and rb3-harmony's printf | stack origin, rb3-harmony only | **not traced**: a stack read, outside this lane's heap brief and invisible to perturb. Most likely `localPitch` via an inlined `CouldScoreAgainstPart` → `GetSloppyPitch`. Open. |

## 5. memcheck pass and the two fixes

Script: `~/tmp/perturb/runv.sh` runs `valgrind --tool=memcheck --track-origins=yes` over the 14 cheap headless
targets with native_health's argv. rb3-ark, rb3-dta, rb3-frame and rb3-render were not run.
- **Setup:** Arch's `ld.so` is stripped, so the first attempt died at startup (missing `memcmp` redirect). It
  needs `DEBUGINFOD_URLS=https://debuginfod.archlinux.org`, the URL `/etc/debuginfod/archlinux.urls`
  already configures.
- **Control:** `ctl/m` prints a malloc'd byte, and memcheck reports it.
- **Results:** all 14 rc 0. Error summaries: rb3-harmony 20,427 (81 contexts), rb3-vocal2 4,260, score3 41,
  gem/hit/score 34, crowd/score4/vocal 22, and midi/milo/save/score2/song 0. Contexts by origin are in §4
  rows 7-10. Per-context grouping by origin: `~/tmp/perturb/vgsum.py` (first frame + origin), refined inline for §4.

### 5.1 `VoiceBeat::mVoiceEnergy` (+0x128) and `mFullBandEnergy` (+0x130)

- **Retail never stores them.** `VoiceBeat()` `0x82718578` stores the two vectors (0x140-0x154) and
  `mEnabled` (+8), then calls `Reset` `0x827182B8`. `Reset` stores 0x10-0x108, 0x110, 0x118, 0x120, 0x138,
  0x158, 0x160, +0, +1, +4 and 0x168, and not 0x128 or 0x130. `TalkyMatcher()` `0x82718768` calls
  `VoiceBeat()` and `EventTracker()` and then `memset(mBuffer, 0, 0x4000)`. The only accesses in `Analyze`
  `0x82717D38` are the smoothing pairs: `lfd` at +0x2B4/+0x2B8 and `stfd` at +0x2DC/+0x2E4.
- **Why it matters.** `ratio = mVoiceEnergy / mFullBandEnergy` sets `unk1`, which
  `VocalPart::CouldScoreAgainstPart` reads for unpitched notes. With a silent mic both values decay by the
  same 0.98 per sample, so the starting ratio persists until both underflow. On the 360 that ratio comes
  from heap history (MemHeap does not fill at debug level 0; see the worked example §2).
- **Natively:** default glibc gives 0/0 (NaN, so `unk1` is false). Under p190 the ratio is 1 (`unk1`
  true). The harness output does not change, because `unk0` and the `unk4` energy test stay false on
  silence.
- **Fix:** both are zeroed in the ctor, `HX_NATIVE` only. Zero is the value of a never-used block.

### 5.2 `VocalPart::unk38` (+0x3C)

- **Retail never stores it.** `VocalPart()` `0x826F34D0` stores +0x38 (`mPhraseScoreMax`) and +0x40 and
  skips +0x3C.
- **The read.** `ScoreSinger` `0x826F3658` loads +0x38 and +0x3C (`lfs` at +0x14/+0x1C) into the `fsel` Min
  that gives `VocalScoreCache::unk8`. That happens before the first phrase boundary, where
  `HandlePhraseEnd` stores 0 (VocalPart.cpp:753 after this change).
- **Under perturb.** 0xBEBEBEBE is −0.37, which would make the cap negative. 0x41414141 is 12.1, which Min
  takes down to 0.
- **Fix:** zeroed in the ctor body, `HX_NATIVE` only.

### 5.3 Verification

| check | before | after |
|---|---|---|
| memcheck rb3-harmony | 20,427 errors / 81 contexts, **5 heap-origin** | 5,265 / 76, **0 heap-origin** |
| memcheck rb3-vocal2 | 4,260 / 7, **1 heap-origin** | 22 / 6, **0 heap-origin** |
| headless 17 targets vs base1, default malloc and p65 | — | 0 normalised lines differ |
| rb3-render, p65 | — | rc 0, 414 PASS / 0 FAIL, PNG set `cb33e69d49f6` (same) |

The `h_fix65` rb3-render run first hit `runh.sh`'s own 180 s cap (rc 124) while the X360 A/B was building.
Rerun alone it took 187 s and gave rc 0. This is load, not the change.

**X360 Δ:** `tools/ab_measure.py --worktree ~/tmp/wt-perturb --from-dirty`, ruler `functionRelocDiffs=name_check`,
objdiff `sha256:ec3badc0f33323e9`. Leg B recompiled 2 MSVC objs.
- Δmatched **+0**, Δmasked_equal +0, Δhonest +0, Δcode% +0.000000 pp, Δcode_bytes +0, Δfuzzy +0.000000 pp.
- Units at 100%: 635 → 635 (mpn), 548 → 548 (fuzzy). The `none` control was +0.
- Leg A: matched 55,268, honest 29,908, code 59.665310%.
- Run dir: `~/tmp/wt-perturb/.ab_measure_runs/20261123-033316-perturb-vocal-inits-382670/`.

`VoiceBeat.cpp`'s header comment says the file is "not in objects.json". That is stale: it is listed as
NonMatching, and leg B recompiled it. Not changed here.

## 6. Not done

- No perturb divergence traced to an unset member, so the only inits are §5's memcheck finds.
- `native_health.sh --selftest` was not run, per the brief.
- The walk was not made deterministic: the RNG draw count follows wall-clock frame timing. A frame-locked
  clock for rb3-app would turn the walk into a diffable instrument and is the next lever for this method.
- The endgame crash's fill correlation (§3.4) was not tested past 4 runs, and the crash was not fixed (char).
