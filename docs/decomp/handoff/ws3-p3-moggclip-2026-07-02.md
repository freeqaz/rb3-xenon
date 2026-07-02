# ws3-optionc p3 — MoggClip.cpp port+pin+carve (2026-07-02)

Worker: Opus. Worktree `/home/free/tmp/wt-exec-ws3-optionc-p3`, branch
`exec/ws3-optionc-0702-p3` (main @00c5b19). Frozen baseline
`/home/free/tmp/exec-ws3-optionc-p3-baseline-report.json`.

## Result: +20 MoggClip functions at strict 100% (16 real-bodied >44B + 4 small-real)

`system/synth/MoggClip.cpp` wired NonMatching, pinned as a **multi-range carve**
around the three landed BinkClip ICF micro-pins. Zero BinkClip regression.

## BinkClip conflict resolution: OPTION (a) CARVE (as recommended)

The three "BinkClip micro-pins" are ICF-merged twins physically living inside
MoggClip's `.text` block. Confirmed via `scripts/target_symbol_map.json`:
- `0x826EF948` = `?SetLoop@BinkClip@@QAAX_N@Z`
- `0x826EFA28` = `?KillStream@BinkClip@@QAAXXZ`
- `0x826EFB20` = `?Stop@BinkClip@@QAAXXZ`

These are ICF-canonical for BinkClip (the map named them BinkClip when BinkClip
landed first). MoggClip's own KillStream/Stop compile byte-identical and fold to
these same addresses, but the addresses are BinkClip-owned. Re-attributing would
just move the match from BinkClip→MoggClip (net zero + risk), so I **carved
around** them. MoggClip loses those ~3 twins to BinkClip ownership; everything
else is captured.

**No BinkClip regression** (verified — identical to baseline):
`?SetLoop@BinkClip` 88.3% (baseline 88.31%), `?KillStream@BinkClip` 99.9%
(baseline 99.87%), `?Stop@BinkClip` 99.9% (baseline 99.94%). These were already
fuzzy near-misses pre-carve; the carve preserved them exactly.

## Final pin (config/45410914/splits.txt)

```
system/synth/MoggClip.cpp:
	.pdata      start:0x8222FC98 end:0x8222FCB0     (dtk back-filled, carves BinkClip .pdata)
	.pdata      start:0x8222FCC0 end:0x8222FCD0
	.pdata      start:0x8222FCD8 end:0x8222FE40
	.text       start:0x826EF6E8 end:0x826EF948     (Save..fn before BinkClip::SetLoop)
	.text       start:0x826EF9BC end:0x826EFA28     (carve out BinkClip::SetLoop)
	.text       start:0x826EFA84 end:0x826EFB20     (carve out BinkClip::KillStream)
	.text       start:0x826EFB60 end:0x826F1088     (carve out BinkClip::Stop; ends at synth/Utl.cpp)
```

Span start `0x826EF6E8` = MoggClip::Save (first MoggClip fn; the ??_GUITrigger
ICF thunk at 0x826EF698 just below is a foreign fold, excluded). End `0x826F1088`
= start of synth/Utl.cpp except_data (CalcRateForTempoSync @0x826F1090 is Utl, not
MoggClip). overlap_check clean (1082 .text / 991 .pdata ranges, 0 overlaps).

## Flavor: RETAIL is DC3-flavor logic on a DRIFTED layout (neither oracle verbatim)

Target has DC3-only methods (`IsReadyToPlay`, `UnloadWhenFinishedPlaying`,
`AddFader`, virtual `Pause`, dual volume) — rb3-Wii is the stripped "Ogg not
supported" build. But the retail **layout** matches neither oracle; it mirrors
the in-repo `BinkClip` sibling (`Hmx::Object, SynthPollable`), shifted +0xC vs
BinkClip because SynthPollable occupies 0x28-0x34. Reconstructed from Ghidra
(ctor 0x826F0790, dtor, Pause, SynthPoll, EnsureLoaded, GetSoundDisplayName):

```
mMoggFile(FilePath) 0x34, mControllerVolume 0x40, mLoop 0x44, mVolume 0x48,
mStream 0x4c, unk50(float) 0x50, mData 0x54, mDataSize 0x58, mLoader 0x5c,
mFaders(vec) 0x60, mPanInfos(vec) 0x6c, mFader 0x78, mUnloadWhenFinished 0x7d,
mPlaying 0x7e, mLoopStartSample 0x80, mLoopEndSample 0x84, mNumChannels 0x88,
mFxSend 0x8c
```

Save/PreLoad use the rb3-Wii `MILO_WARN("Can't load new")` rev scheme
(SAVE_REVS(2,0)), NOT DC3's LOAD_REVS/ASSERT_REVS. `mFxSend` added at end (0x8c,
uninit) only to satisfy `Sfx.cpp`'s `MoggClip::SetSend` reference; retail ctor
doesn't init it. SetFile does NOT call LoadNumChannels (retail).

## Per-function match % (strict 100% unless noted; reproduced via MCP run_objdiff)

| Function | Size | % | Notes |
|---|---|---|---|
| ??0MoggClip (ctor) | 0xF4 | 100 | fixed: unk50 is float (stfs), no mFxSend init |
| ??1MoggClip (dtor) | 0xF8 | 100 | |
| Save | 0xEC | 100 | SAVE_REVS(2,0) |
| Copy | 0x8C | 100 | |
| PreLoad | 0xB4 | 100 | rb3-Wii MILO_WARN rev pattern |
| Pause | 0x3C | 100 | |
| SetPan(int,float) | 0xBC | 100 | |
| AddFader | 0x9C | 100 | |
| RemoveFader | 0x94 | 100 | added (was missing) |
| SetFile | 0x4C | 100 | no LoadNumChannels |
| EnsureLoaded | 0xC0 | 100 | |
| UnloadData | 0x44 | 100 | |
| UpdateFaders | 0x64 | 100 | |
| UpdatePanInfo | 0x70 | 100 | |
| IsReadyToPlay | 0x48 | 100 | |
| GetSoundDisplayName | 0x4C | 100 | fixed: `!mPlaying` not `!IsPlaying()` (inline) |
| SetControllerVolume | 0x24 | 100 | 36B small-real |
| SetVolume | 0x24 | 100 | 36B small-real |
| FadeOut | 0x14 | 100 | 20B small-real (mFader->DoFade(-96,f)) |
| UnloadWhenFinishedPlaying | 0x8 | 100 | 8B small-real |
| SynthPoll | 0x178 | **97.6** | NEAR-MISS: reg-swap r30↔r31 + 1 extra local. Permuter. (inlined Stop already fixed KillStream/UnloadData tail) |
| LoadFile | 0xE0 | **95.8** | NEAR-MISS: extra stack local at 0x54, compiler keeps `0` in r27 (regalloc/scheduling). Permuter. |
| SetupPanInfo | 0x7C | **92.9** | NEAR-MISS: ch1 `-f2/2.0` (double) → target wants single `lfs -0.5; fmadds`. All single forms CSE (78.7%); double breaks CSE but adds frsp. Needs the exact non-CSE'd two-fmadds form. |
| Play | 0xF8 | **23.6** | NAME REMOVED — retail Play differs substantially from DC3-port; needs reconstruction. Addr 0x826F0040. |
| Handle | 0x354 | **40** | NAME REMOVED — exact BEGIN_HANDLERS list differs. Addr 0x826F0190. |
| SyncProperty | 0x1CC | **23** | NAME REMOVED — exact BEGIN_PROPSYNCS list differs. Addr 0x826F0E30. |
| LoadNumChannels | 0xFC | **21** | NAME REMOVED — retail channel-poll logic differs. Addr 0x826F09A0. |

Names for the 4 far near-misses (Play/Handle/SyncProperty/LoadNumChannels) were
REMOVED from the map per gate rule 5 (a name that doesn't produce a match is
removed). SynthPoll/LoadFile names also removed though correctly attributed —
addresses documented above for a permuter follow-up.

## Gates

- icf_alias_check: **VERDICT HONEST, exit 0** — 16 real-bodied (>44B) anchors,
  22 interspersed stub-folds all own (0 foreign), longest foreign run 6 < 8.
- overlap_check: clean.
- BinkClip regression: NONE (baseline == worktree for all 3 pins).
- Whole-binary composed A/B: **left to the reviewer** (worker rules: no
  whole-binary builds). Expect ~+16-20 net new strict from this unit.

## Files changed
- src/system/synth/MoggClip.h, src/system/synth/MoggClip.cpp (retail rewrite)
- config/45410914/objects.json (+MoggClip NonMatching)
- config/45410914/splits.txt (+carve pin; dtk back-filled .pdata)
- scripts/target_symbol_map.json (+14 verified 100% names, ADD-ONLY, all gated)
