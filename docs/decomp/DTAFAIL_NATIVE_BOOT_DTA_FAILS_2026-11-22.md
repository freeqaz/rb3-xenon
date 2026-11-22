# Lane dtafail: the two non-fatal DTA failures in every native rb3-app boot (2026-11-22)

Branch `dtafail`. Every native `rb3-app` boot (`RB3_APP_RUN=1 RB3_APP_FRAMES=900`,
`MILO_INPUT_SCRIPT=~/tmp/skipped_boot/start.txt`, TU5 mounted) logged:

    FAIL: Couldn't find 'preview_db' in array (file config/sound.dta, line 186)   x1
    FAIL: Couldn't find key (file ui/ui.dta, line 150)                            x2
      (each followed by 2x "Data kDataUnhandled is not Array (... line 151)" and 2x "elem: null array")

Both are native differences. Retail would hit neither on the same data.

## How the data was read

The Xbox dtbs (`../rb3/orig-assets/extracted-xbox-full/{ui,config}/gen/*.dtb`) are
new-style encrypted (seed = first LE dword, byte ^= low byte of the Park-Miller step
`(s%0x1F31D)*0x41A7 - (s/0x1F31D)*0xB14`), then little-endian `DataArray::Load` layout
(u8 1, then per array u16 size, u16 line, u16 deprecated; per node i32 type + value).
The text `orig-assets/extracted/*.dta` files are reconstructions whose line numbers do
not match the dtb, so do not use them for a `(file, line)` lookup. A throwaway decoder
was used (not committed); the dtb line numbers matched both failure lines exactly.

## 1. `config/sound.dta` line 186: native read a DC3 key retail never reads

Line 186 is `(song_select (loop_forever TRUE) (fade_time 0.5) (attenuation 12))`. No
`preview_db`. `SongPreview::Init` under `HX_NATIVE` added
`cfg->FindData("preview_db", mPreviewDb, true)` (DC3 scaffold, `c5c1650fb`).

Retail `?Init@SongPreview@@QAAXXZ` (0x827A5508) builds `SystemConfig("sound",
"song_select")` (strings at 0x82000C70 / 0x820A5074) and calls FindData exactly three
times: `loop_forever` (0x8211237C) at 0x827A55CC, `fade_time` (0x820F3960) at
0x827A55F8, `attenuation` (0x82112370) at 0x827A5620. `Start` (0x827A5790) sets the
music fader with f1 = 0.0 (0x82000D78) at 0x827A5878. So retail never looks the key up.

Fix: drop the native read and `mPreviewDb`; native `Start` sets 0 dB like retail.
X360 preprocessed output unchanged (edits are inside `#ifdef HX_NATIVE` / comments).

## 2. `ui/ui.dta` line 150: native `DataNode::Equal` compared string pointers

Lines 146-152 are the `FOCUS_MSG` handler:

    {unless {|| {== $nav_type ""} {== $nav_type none}}
       {do ($ct_arr {find $syscfg joypad instrument_mapping $nav_type}) ...}}

A temporary trace in `DataFindExists` showed the missing key is the symbol `''`
(type 5) both times: the focus changes come from `PanelDir::SetFocusComponent(c,
gNullStr)` when splash_screen and first_time_calibration enter. So the `unless` guard
`(== $nav_type "")` read false.

Cause: native `operator==` routes through `DataNode::Equal` (DC3-era; retail has no
Equal). Its string branches passed `second.UncheckedStr()` for a `kDataString` node, but
a string node's `mValue` is the `DataArray` holding the characters
(`DataNode(const char*)`: `mValue.array = new DataArray(c, len)`), so streq compared
pointer bytes. Symbol-vs-string, object-vs-string and string-vs-string all were wrong.

Retail `??8DataNode@@QBA_NABV0@@Z` (0x8274ABA0): same type 0x12 -> two `lwz 0(rN)` per
side then the byte loop (0x8274ABB8..0x8274ABC4); mixed with either side 0x12 ->
`LiteralStr` per side, which dereferences once more for the non-symbol side
(0x8274AC9C..0x8274ACB8). So retail evaluates `(== '' "")` true and skips the find.
dc3-decomp's current `Equal` already has the dereference; ours was scaffolded before it.

Fix: dereference (`mValue.var->mValue.symbol`) on the string side in all three
branches. `Equal` is compiled in the X360 build too (callers in flow/, Object.cpp,
UI.cpp), so this one was A/B'd.

Not done: no audit of other `UncheckedStr()` uses on possibly-string nodes
(`DataArray.cpp:371/394` pointer compares are deliberate symbol-pointer paths).

## Results

Native (worktree build `native/build/rb3-app`, run through `no610m.sh`, same command as
`~/tmp/symempty_app/run.sh` with fresh `RB3_CONTENT_DIR`s under `~/tmp/dtafail/`):

| run | `Couldn't find` lines | result line |
|---|---|---|
| before (`26cadc5ed`) | 1x preview_db + 2x ui.dta:150 (+4 `kDataUnhandled is not Array`, +4 `elem: null array`) | `APP RAN 900 FRAMES screen=first_time_calibration`, rc=0, PASS=53 |
| after (`fa5187f86`) | 0 (and 0 of the follow-on lines) | `APP RAN 900 FRAMES screen=first_time_calibration`, rc=0, PASS=53 |

Prediction was that both lines vanish and nothing else changes. Measured: a log diff
with numbers and pointers masked differs only in the removed lines plus bone/skin
transform values and timing lines. Those also vary between two pre-fix runs
(`denimjacket_clean_*` loads: 2 in one pre-fix run, 4 in the other), so they are run noise.

X360: `tools/ab_measure.py --revert ff991fd9e` (the Equal commit; SongPreview's commit
does not change X360 preprocessed output and was not measured): Δmatched +0,
Δhonest +0, Δcode +0 B, Δfuzzy +0.000000pp, units at 100% 635 -> 635 (name_check,
objdiff sha256 ec3badc0). Expected: retail has no `Equal`, so its row cannot pair.

`tools/native_health.sh --selftest` was started and then killed on the coordinator's
instruction: under `no610m.sh` it can only end rc 3 (no GPU), and the coordinator's
landing gate runs all 19 targets on a GPU. This lane has no gate result of its own.
Not merged to main.
