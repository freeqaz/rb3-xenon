# W9 L6 scout — songselect-panel-family-port-scout

**Date:** 2026-06-20 · **Mode:** adversarial discover/planner (read-only in main)
**Frontier item:** `songselect-panel-family-port-scout` (kind=scout, est +40)
**Verdict:** **REFUTED (as stated) → REAL_ACTIONABLE (re-targeted)**

## Frontier premise (as written)

> The 224-fn region `0x82558EE0-0x8255DE88` is its own unmined vein: ~dozen band3
> game panel TUs (RetryAudio/SelectDifficulty/SongSelect/Training/TourDesc/
> TourChallengeResults/QuestFilter + Credits/Movie/BandScreen). Per-panel:
> identify span, scaffold+pin+port from rb3-Wii.

## Ground-truth findings (FALSIFY the premise)

### 1. The region is a STATIC-INIT / Symbol-intern blob, not panel bodies

Parsed the authoritative retail symbol-by-VA from `auto_03_82260000_text.obj`
(little-endian PPC COFF, section base `0x82260000`, 107917 symbols, NRELOC_OVFL
flag → 411183 relocs). The 0x82558EE0-0x8255DE88 region = **223 functions in
0x4FA8 bytes** = **avg ~36 bytes/fn**:

| size bucket | count |
|---|---|
| <0x20 | 14 |
| 0x20-0x40 | 100 |
| 0x40-0x80 | 86 |
| 0x80-0x100 | 9 |
| >=0x100 | 14 |

This is the textbook **static-init / `??__E` dynamic-init / Symbol-intern /
trivial-thunk** profile — NOT substantial panel logic.

### 2. Each panel's StaticClassName is referenced exactly ONCE — by its static-init sliver

StaticClassName string VAs (from `auto_00_82000400_rdata.obj`, base 0x82000400):
`SongSelectPanel=0x8209af20`, `RetryAudioPanel=0x8209a958`,
`SelectDifficultyPanel=0x8209ac78`, `TrainingPanel=0x8209b120`,
`TourChallengeResultsPanel=0x8209aae0`, `TourDescPanel=0x8209aa60`,
`QuestFilterPanel=0x8209a9d8`.

Walking ALL 411183 relocs in `auto_03` .text: each string is referenced by
**exactly one** function, all in 0x82559258-0x8255A288:

```
RetryAudioPanel           -> fn_82559258
QuestFilterPanel          -> fn_82559370
TourDescPanel             -> fn_82559488
TourChallengeResultsPanel -> fn_825595A0   (== the Meta.cpp 0x58 sliver pin, matched 1/1)
SelectDifficultyPanel     -> fn_825598E8
SongSelectPanel           -> fn_82559E28
TrainingPanel             -> fn_8255A288
```

`fn_82559E28` (SongSelectPanel) disassembles to a pure **Symbol static-init**:
`bl fn_8279B788` (Symbol::Symbol(const char*)) storing into `.data` global
`lbl_82DCD0C8` — i.e. a file-scope `static Symbol`. Not a ctor/method.

### 3. The real panel BODIES live ELSEWHERE — two cases

**(a) Already wired + partially matched** (bodies at `0x82B4xxxx` / `0x82789xxx`,
NOT in the scouted region):

| panel | pin | matched | source |
|---|---|---|---|
| QuestFilterPanel | 0x82B4B850-0x82B4CED0 | 30/51 | band3/tour/QuestFilterPanel.cpp |
| TourDescPanel | 0x82B4DBA0-0x82B4F8D8 | 33/46 | band3/tour/TourDescPanel.cpp |
| CreditsPanel | 0x82789950-0x8278A908 | 10/34 | system/meta/CreditsPanel.cpp |
| MoviePanel | 0x8278A908-0x8278C7B8 | 32/75 | system/meta/MoviePanel.cpp |
| CalibrationPanel | 0x825EC708-0x825EF598 | 29/76 | band3/meta_band/CalibrationPanel.cpp |

The static-init for these is **linker-split** away from the body cluster (the
0x82558 slivers), which is why the premise mistook the sliver region for the TU.

**(b) RetryAudio/SelectDifficulty/SongSelect/Training — bodies not locatable as
contiguous clusters.** Their distinctive Wii strings either aren't referenced in
`.text` at all (`content_loading_panel` 0x8208DA0C, `lb_success` 0x820B7EE0,
`select_difficulty` 0x820B44C4 — zero `.text` refs → inlined/ICF-folded) or land
in unrelated clusters (`mini_leaderboard_rotation_off` 0x820C91D0 → 0x826135CC,
which is an UNPINNED `meta_music` flow blob, not a panel). These are small
classes whose handful of methods retail inlined into callers; their only
standalone code is the static-init sliver already in-region. **No mineable
panel-body vein exists for these four.**

### 4. The region is essentially unpinned (auto-blob)

Only `Meta.cpp` (0x825595a0-0x825595f8, the TourChallengeResults static-init
sliver, matched 1/1) overlaps. Neighbours: `ContextChecker.cpp` ends 0x82558eac
(just below), next real pin `StreamRecorder.cpp` @0x825732d0. A ~0x1AA00 gap
holds the region — but it is static-init/meta-flow tiny-fn filler, not panels.

## Conclusion

The scouted region is a **mirage** (cf. MEMORY game-code-instrumentation:
static-init/Symbol-intern blobs are not portable veins). The +40 estimate does
NOT exist there. **The genuine adjacent value is bodyport of the near-miss
methods in the ALREADY-WIRED panels.** Verified by objdiff:
`CreditsPanel::Exit` = 99.9% normalized, 2 diff_arg only (near-certain close).

## Actionable (re-targeted, self-contained per panel, vs main@8314)

- **CreditsPanel bodyport** (HIGHEST EV): 4 named real bodies at 99.8-99.9%
  (Exit 99.9% sz0x8c, Unload 99.9% sz0x70, ctor 99.8% sz0xcc, PausePanel 99.8%
  sz0xe4). Source `src/system/meta/CreditsPanel.cpp` already wired+pinned. Oracle
  rb3-Wii not strictly needed — these are last-0.1% regalloc/reloc nits. Est +3-4.
- **TourDescPanel bodyport**: 3 near (TourDescProvider::Update vstate 95.4% sz0x68,
  UpdateExtendedInfo 90.6% sz0x798, TourDescPanel::LoadIcons 86.1% sz0x168). Oracle
  band3/tour/TourDescPanel.cpp. Est +2-3.
- **CalibrationPanel bodyport**: UpdateAnimation 93.1% sz0x288 + residual. Est +1-2.

## Discovered frontier (seed later layers)

- `meta_music-flow-blob` @ ~0x8255B638 / 0x82613xxx — UNPINNED `meta_music`
  jump-table-driven state code (string `meta_music` 0x8209B6C4, jumptable
  0x8209B6E0 → 0x8255B9xx). Distinct from panels; needs owner-TU ID (MetaPerformer
  / MetaMusicState?). NEEDS_DEEPER.
- `static-init-sliver-cleanup` — the 0x82558 region's 223 tiny static-init/Symbol
  fns are mostly the `??__E` dynamic-init thunks of the already-wired panel/meta
  TUs. Possible micro-reveal wins if a wired TU's `??__E` happens byte-exact but
  unpinned. Low EV, audit-only.
