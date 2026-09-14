# `BeatClock.cpp` / `AudioDucker.cpp` — retail does NOT contain these classes (2026-09-14, lane W16-A)

**Verdict: leave both unwired.** Retail RB3-360 contains neither `BeatClock` nor
`AudioDucker`/`AudioDuckerGroup`/`AudioDuckerTrigger`. They arrived in our tree with
the DC3 engine import and are **DC3-era classes that RB3 (2010) predates**. Wiring
them into `objects.json` would create pairable rows with no target rows behind them
— `ForceEmit_*`-class metric fitting, which CLAUDE.md forbids.

Lane W16-A Task B. Subjects picked by the coordinator as the two plausible retail TUs
among the non-XDK files that are neither declared in `objects.json` nor `#include`d by
any compiled TU: `src/system/world/BeatClock.cpp` (6,425 B),
`src/system/synth/AudioDucker.cpp` (2,867 B).

## ⚠ The near-miss that shaped this probe

The first version of this probe counted **plain class-name strings in the XEX** and
would have concluded "absent" from that alone. That reading is **unsound**, and the
control that proves it is `MeasureMap`: a class that **is** in retail (8 map rows) and
scores **0 plain-string hits and 0 RTTI hits**, because it is **not polymorphic**.

`/GR` is ON (2,220 `??_R4` Complete Object Locators in retail `.rdata`), so a
polymorphic class emits a `.?AV<Class>@@` RTTI type-name string — and a
non-polymorphic one emits nothing. ⇒ **the string/RTTI channel is structurally blind
to exactly half the subjects here**, and a negative from it means nothing on its own.

So each subject is adjudicated on the channel that is *valid for that subject*, and
`AudioDuckerTrigger` (which **is** polymorphic — `: public Hmx::Object`) is used as the
polymorphic proxy for the AudioDucker TU.

## Measured (reproduce with the script recorded at the end)

`orig/45410914/default.xex` 13,971,456 B · `band.exe` 14,363,648 B ·
`scripts/target_symbol_map.json` 29,039 symbols · `report.json` 69,219 rows.

### Positive controls — the probe can return YES

| class | polymorphic | plain | RTTI | map | report |
|---|---:|---:|---:|---:|---:|
| `RndPollable` | yes | 1 | **1** | 9 | 9 |
| `UIComponent` | yes | 9 | **1** | 95 | 95 |
| `BandDirector` | yes | 3 | **1** | 76 | 76 |
| `MasterAudio` | yes | 1 | **1** | 57 | 57 |
| `RndDrawable` | yes | 4 | **1** | 48 | 48 |
| `Synth` | yes | 12 | **1** | 34 | 34 |
| `BandCharacter` | yes | 3 | **1** | 101 | 101 |
| `DataArray` | **no** | 3 | 0 | **33** | 33 |

8/8 fire. Note `DataArray` already showing the blind spot: present, non-polymorphic,
RTTI 0 — caught by the map channel.

### Negative controls — the probe can return NO, and its blind spot is characterised

| class | polymorphic | plain | RTTI | map | report | what it proves |
|---|---:|---:|---:|---:|---:|---|
| `MeasureMap` | **no** | **0** | **0** | **8** | 8 | **present in retail, invisible to string+RTTI** ⇒ channel 1 is blind for non-poly |
| `ZzzNotAClassZzz` | — | 0 | 0 | 0 | 0 | pure-absence null |

### Subjects — every valid channel reads zero

| class | polymorphic | plain | RTTI | map | report | valid channel | reads |
|---|---:|---:|---:|---:|---:|---|---|
| `BeatClock` | **yes** | 0 | **0** | **0** | **0** | RTTI **and** map | absent |
| `AudioDuckerTrigger` | **yes** | 0 | **0** | **0** | **0** | RTTI **and** map | absent |
| `AudioDucker` | no | 0 | 0 | **0** | **0** | map | absent |
| `AudioDuckerGroup` | no | 0 | 0 | **0** | **0** | map | absent |

Both TUs contain a polymorphic class, so for both the **RTTI channel is valid and
returns 0** — this is not resting on the blind channel. The map channel, which is the
one that rescued `MeasureMap`, independently returns 0 for all four.

### Corroborating literals

Every string these classes would have to emit (`OBJ_CLASSNAME` names, DTA keys) is
absent from the XEX: `beat_clock` 0, `BeatClock` 0, `audio_ducker` 0, `AudioDucker` 0,
`ducker` 0, `TheBeatClock` 0, `beatclock` 0.

## Third channel — DC3 and rb3-Wii triangulate it

- **DC3 HAS both, and declares them `Matching`** in `config/373307D9/objects.json`
  (`system/synth/AudioDucker.cpp` line 646, `system/world/BeatClock.cpp` line 863),
  with **80 rows** across DC3's report for these classes (e.g. `??1AudioDucker@@QAA@XZ`,
  64 B, in `default/system/synth/Sound`).
- **rb3-Wii has NEITHER file** — `../rb3/src/system/world/BeatClock.cpp` and
  `.../synth/AudioDucker.cpp` do not exist.

⇒ present in the newer engine, absent from both RB3 builds. This is exactly the
caveat CLAUDE.md records from the project owner — **dc3-decomp is *newer* than RB3** —
and it is the mechanism by which our `src/system/` can hold classes retail never had.

## What was NOT done, and the untestable hypothesis

- **Not wired.** No `objects.json` edit, no `splits.txt` pin, no source change. The
  brief's "wire as `NonMatching`, confirm neutrality with a build" path was not taken
  because its precondition (retail contains the class) is refuted.
- **Not searched for under another name.** The hypothesis I could not test is that
  retail contains this *functionality* under a different class name that our map does
  not identify, in which case it would be hiding in the `auto_*` unattributed stratum
  rather than being absent. Discriminating that needs a behavioural/structural match
  (BinDiff against DC3's named `BeatClock` bodies), not a name probe. Sized at
  ~9.3 kB of DC3 source; not attempted here.

## Reproduce

```
python3 - <<'EOF'   # from the repo root; counts against orig/45410914/default.xex,
                    # scripts/target_symbol_map.json and build/45410914/report.json
EOF
```
The exact script and its raw output are at `~/tmp/w16a/taskb_probe.txt` (lane scratch).
The shape that matters is: **for every subject, name the channel that is valid for it,
and prove that channel can return YES (positive control) and NO (absence null) before
believing its verdict.**
