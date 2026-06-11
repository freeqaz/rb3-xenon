# Band head +4 / Game head +4 — RESEARCH-COMPLETE: REFUTED (zero measurable value; per-TU divergence, not a header fix)

**Research dossier, 2026-06-11.** Worktree `bandgame-head4` (branch off main
`c73823b`, 6965 matched). Two PARKED bonus levers from the Player +4 dossier
(`docs/decomp/research/2026-06-11-player-plus4-layout.md` §6.3). Verdict:
**both levers are REAL +4 deltas but have ZERO measurable value in the current
pinning, and are NOT clean single-member header fixes** — they are a per-TU
layout divergence. Do not apply a header edit. Outcome = clean refutation.

## TL;DR

- **Direction confirmed (ours = retail + 4) for both.** Our compiled Player ctor
  (`??0Player@@QAA@PAVBandUser@@PAVBand@@HPAVBeatMaster@@@Z`, in
  `build/45410914/src/band3/game/Player.obj`) bakes:
  - `lwz r11, 0x94(r27)` — **Band+0x94** (`mCommonPhraseCapturer`); retail Player
    ctor (`fn_82688E40`, Ghidra) reads `*(undefined4 *)((int)param_3 + 0x90)` =
    **Band+0x90**. Δ **+4**.
  - `lwz r11, ?TheGame@@3PAVGame@@A(r9); lbz r11, 0x41(r11)` — **Game+0x41**
    (`mProperties.mEnableOverdrive`, mProperties[0x11]); retail reads
    `*(undefined1 *)(DAT_82dd09e8 + 0x3d)` = **Game+0x3d** ⇒ retail mProperties @
    Game+0x2c, ours @ Game+0x30. Δ **+4**.
- **Zero measurable value.** An exhaustive scan of all **165** measured near-miss
  functions (60–99.99%) across every compiled unit found **zero** genuine
  Band/Game +4 readers (the one apparent hit, `MD5::transform` in Quazal, is an
  unrelated MD5-buffer 0x90/0x94 — a false positive). A targeted +4-dominant-delta
  scan of 86 game/reader-TU near-misses also found **zero**. The +4 is baked only
  into the **unpinned Player ctor** (`0x82688E40`, OUTSIDE the pinned Player.cpp
  range `0x826843F0–0x82686C50`, splits.txt:2390) and into **anonymous unpaired
  `fn_` readers** (e.g. GemTrack `fn_82B62658`) that carry no target counterpart in
  the measured set.
- **NOT a header fix.** `src/band3/game/Band.h` and `src/band3/game/Game.h` are
  **byte-identical** to the rb3-Wii oracle (`../rb3/src/band3/game/{Band,Game}.h`),
  and there is **no DC3-added member** (DC3 = Dance Central has no Band/Game class;
  `../dc3-decomp/src` has only `lazer/game/Game.h`, an unrelated class). The +4 is a
  **per-TU divergence**: `GemTrack.cpp` compiles `Game` with `mProperties @ 0x2c`
  (= retail; its target readers MATCH at the retail offset), while `Player.cpp`
  compiles the SAME `Game.h` with `mProperties @ 0x30` (+4). Same `Game.h`, same
  already-landed `SongPos.h` fix (0x14). So the excess 4 bytes come from an
  include-set difference between the two TUs, not from a nameable header member —
  there is no clean single-member edit to make.

**Recommendation:** Do NOT pursue either lever until (a) the Player ctor /
`Band.cpp` / `Game.cpp` are actually pinned (so the readers become measured), AND
(b) the per-TU divergence is root-caused (which TU sees Game/Object +4 and why).
Both are prerequisites; neither holds today. Net delta of any header edit now ≈ 0
(no measured reader), with downside risk if a currently-matched function bakes the
+4-inclusive offset against a like-baked target.

## 1. Evidence — the +4 is real (both)

### 1.1 Retail (Ghidra MCP, port 8002, binary `/default.xex-35adb6`)

Retail `Player::Player` = `fn_82688E40` (decompile via
`venv/bin/python tools/ghidra/ghidra-decompile.py 0x82688e40`). Two head-reader lines:

```c
puVar2[0x99] = *(undefined4 *)((int)param_3 + 0x90);   // Band+0x90  = mCommonPhraseCapturer
...
*(undefined1 *)(puVar2 + 0xb9) = *(undefined1 *)(DAT_82dd09e8 + 0x3d);  // Game+0x3d = mProperties.mEnableOverdrive
```
`param_3` = the `Band*` ctor arg; `DAT_82dd09e8` = `TheGame`. So **retail: Band+0x90,
Game+0x3d** ⇒ retail mProperties @ Game+0x2c.

### 1.2 Ours (self-diff our compiled obj, no rebuild)

```
bin/objdiff-cli diff -1 build/45410914/src/band3/game/Player.obj \
                     -2 build/45410914/src/band3/game/Player.obj \
  '??0Player@@QAA@PAVBandUser@@PAVBand@@HPAVBeatMaster@@@Z' \
  --full-listing -f json -o /tmp/claude/bg/playerctor.json
```
Memory-op dump shows (word idx → instruction):
- `66  lwz r11, 0x94, r27`  → **Band+0x94** (the `mCommonPhraseCapturer` read fed to `puVar2[0x99]`)
- `106 lwz r11, ?TheGame@@3PAVGame@@A, r9` / `107 lbz r11, 0x41, r11` → **Game+0x41** (mProperties[0x11])

⇒ ours mProperties @ Game+0x30, mCommonPhraseCapturer @ Band+0x94. **Both +4 vs retail.**

### 1.3 The per-TU divergence (the decisive nuance)

Retail target asm for an *anonymous* GemTrack reader (`fn_82B62658`, in
`build/45410914/asm/band3/bandtrack/GemTrack.s`):
```
lwz r11, 0x9e8(r27)   ; r11 = TheGame
lbz r10, 0x3e(r11)    ; mProperties[0x12] = mEnableCoda  => mProperties @ 0x2c (retail)
```
i.e. GemTrack's target reads at the **retail (0x2c-based)** offsets, and GemTrack
(36 matched fns, only one 99.90 funclet near-miss) compiles these correctly. Yet
the Player.cpp TU bakes the **+4 (0x30-based)** layout. Both compile the identical
`Game.h` and the identical already-fixed `SongPos.h` (verified: SongPos.h carries
the landed 0x14/no-mPhrase form; both Player.obj and GemTrack.obj are newer than
SongPos.h, so neither is stale). Therefore the +4 is an **include-set / ODR-style
per-TU divergence**, the same family as the RB3_RBTREE_0x1C per-TU map split — NOT
a property of the shared header.

Computed onset for Game: head = `BeatMasterSink`(vptr,4) + `Hmx::Object` +
`DiscErrorMgrWii::Callback`(vptr,4), then `mProperties`. retail mProperties@0x2c ⇒
sizeof(Hmx::Object)=0x24 as seen by GemTrack; ours @0x30 ⇒ Player.cpp sees the head
(Object or a base) as 0x28 — a +4 on a base/Object that is otherwise binary-wide
stable. (Header `// 0xNN` comments are stale: Game.h says `mProperties // 0x24`,
but even retail-correct is 0x2c — the comment is −0x8 low. Band.h says
`mCommonPhraseCapturer // 0x78`, retail-correct is 0x90 — −0x18 low.)

## 2. Evidence — zero measurable value (both)

| scan | population | Band/Game +4 readers found |
|---|---|---|
| dominant-+4-delta over game/reader-TU near-misses | 86 fns (85–99.99%) | **0** |
| explicit `lbz 0x3d→0x41` / `lwz 0x90→0x94` over ALL measured near-misses | 165 fns (60–99.99%, all units) | **0** (1 FP = `MD5::transform`@Quazal) |
| pinned Player.cpp fns baking Band+0x94 / Game+0x41 | all Player.obj fns | **only the unpinned ctor** |

The Player-chain near-misses that the SongPos dossier predicted are already resolved
on this base (SongPos lever landed). The remaining Player/VocalPlayer/Singer
near-misses are funclet/body-port class (`fn_8268663C` 99.90 EH-funclet;
`?ResolveAmbiguity@Singer@@QAAXXZ` 66.76 `divw`-idiom body-port;
`?Rollback@VocalPlayer@@UAAXMM@Z` 90.00 epilogue) — none are Band/Game readers.

GemTrack's `TheGame->mProperties` readers (lines 386/514/611/639) and the other
cited reader sites (GamePanel 243/262/332/338, VocalPlayer 1898) compile to
**anonymous `fn_` functions with no map entry** (unpaired ⇒ `fuzzy_match_percent =
None` ⇒ not measured). They cannot flip until they are named/pinned.

## 3. Why no edit was applied

Task criterion #3: ambiguous/refuted evidence ⇒ document and STOP, do not force an
edit. All three apply here:
1. **No clean single-member fix exists** — headers match the rb3-Wii oracle exactly;
   no DC3-added member; the divergence is per-TU (include-set), not header-resident.
2. **Zero measured impact** — no measured function reads Band/Game at the +4 offset;
   the only confirmed +4 reader (Player ctor) is unpinned.
3. **Downside risk** — a per-TU header edit that "fixes" the +4 could regress any
   currently-matched function whose target ALSO bakes the +4 (the GemTrack-vs-Player
   asymmetry proves the two layouts coexist in the binary).

## 4. What would make these levers live (handoff)

- Pin the **Player ctor** (`fn_82688E40`, size 704) — extend/add a Player.cpp split
  range or a target-symbol-map entry so the ctor becomes measured. THEN the Band+0x94
  / Game+0x41 +4 becomes a real near-miss and the direction here lets it be closed.
- Pin / name the GemTrack/GamePanel **`TheGame->mProperties` anonymous readers** so
  they pair, then re-scan for the +4 — but note GemTrack's target already uses the
  0x2c (retail) layout, so the fix there is to make Player.cpp's TU AGREE with
  GemTrack's TU, i.e. root-cause the per-TU Object/Game head divergence.
- Root-cause the per-TU divergence: diff the effective include set of `Player.cpp`
  vs `GemTrack.cpp` for whatever makes `Hmx::Object`/a Game base 4 bytes larger in
  the Player.cpp TU (candidates ruled out: SongPos already 0x14, std::vector is 0xc
  3-pointer, `_STLP_USE_SIZED_VECTOR` not defined, VectorSizeDefs.h is macro-only).
  This is an ODR/per-TU question, not a header-member question.

## Appendix: repro

- Our Player ctor offsets: `/tmp/claude/bg/playerctor.json` (regen via §1.2 self-diff
  + `diff_inspect`/manual lwz/lbz dump).
- Retail Player ctor: `ghidra-decompile.py 0x82688e40` (binary `/default.xex-35adb6`).
- Target GemTrack reader: `build/45410914/asm/band3/bandtrack/GemTrack.s` line ~1138
  (`fn_82B62658`: `lwz r11,0x9e8(r27); lbz r10,0x3e(r11)`).
- Headers: `src/band3/game/Band.h`, `src/band3/game/Game.h` (== rb3-Wii oracle).
- Pinned Player range: `config/45410914/splits.txt:2390` (0x826843F0–0x82686C50);
  the ctor at 0x82688E40 is OUTSIDE it.
- Baseline: `measures.matched_functions == 6965` (this worktree, fresh).
