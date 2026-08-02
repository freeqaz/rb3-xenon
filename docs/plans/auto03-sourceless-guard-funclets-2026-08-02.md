# The 75 source-less `auto_03_*` guard funclets — sized and identified (lane DA-4, 2026-08-02)

Measured at HEAD `f48bcad7` in `/home/free/tmp/laneDA4/wt`. Instrument:
`tools/guard_funclet_census.py . --deficit` (ruler-independent — see the RULER note
in that tool; its default-mode `masked_equal` estimator was broken by the
2026-08-02 disclosure flip and is now guarded).

## What they are, mechanically

Lane CY-2 split the storage-class residue as **235 in units with a compiled base
obj / 75 in fifteen source-less `auto_03_*` units**. Reproduced independently here:
**exactly 15 units, exactly 75 funclets.** Today's residue is **303 unmatched =
75 source-less + 228 source-backed** (tree-wide deficit **1,002**, down from CY-2's
1,042 — lane CZ-2's landings drained ~40).

Every one of the 15 has **`base_path: null`** in `objdiff.json` and **ours = 0** in
`--deficit`. That is the whole mechanism: objdiff never *attempts* pairing, exactly
as `plans/attribution-frontier-census-2026-07-29.md` found. Only a `splits.txt`
claim plus a real source file changes it.

★ **Pin geometry is NOT the blocker anywhere.** All 15 are bracketed by already-pinned
neighbours with gaps of **0–12 bytes**. Identification and porting are the only cost.

## Sizing against the other half

CZ-2 showed the 228 source-backed sites are really **~16** Δmatched-payable (117 of
171 guard rows sit under anonymous or unresolved owners). So by residue count the
source-less half is **75 vs ~16** — the larger half, as suspected. **But by
*tractable* count it is ~13 vs ~16, i.e. roughly equal**, because 50.7% of the 75 has
no oracle at all (below). Quote the tractable number, not the residue number.

⚠ Funclet count is a **locator, not a forecast** — porting a unit also pays whatever
of its *other* functions match. Each unit below currently contributes **0** matched,
so its function count is the ceiling, not its funclet count.

## The 15 units

| start VA | funclets | fns | code B | bracketed by (prev → next) | identity | tier |
|---|--:|--:|--:|---|---|---|
| `82560B08` | 19 | 95 | 9,892 | UIStats → MetaPerformer | **RBN audition** | 3 |
| `825632BC` | 19 | 35 | 4,392 | MetaPerformer → CharSync | **RBN audition** | 3 |
| `82B7EFBC` | **12** | 45 | 4,480 | TourDescPanel → synth/SynthSample | **`TourChallengeResultsPanel.cpp`** | **1** |
| `82604520` | 7 | 14 | 628 | AppInlineHelp → CharServoBone | no strings | 5 |
| `823ECD58` | 5 | 92 | 9,804 | Server → ContextChecker | `net`/`searcher`/`search_limit` | 5 |
| `82650B74` | 3 | 28 | 3,404 | Instarank → Matchmaker | `h2h`/`ranked`/`default_ranked_match` | 5 |
| `8266B3E8` | 2 | 28 | 2,404 | AccomplishmentConditional → Leaderboard | `assets` | 5 |
| `823028E8` | 1 | 11 | 4,020 | **VocalTrackDir** → TrackPanelDir | `vocal_harmony_prototype` | **2** |
| `823F2CD4` | 1 | 30 | 2,328 | StorePurchaser → PrefabMgr | `settings_changed` | 5 |
| `823F64AC` | 1 | 31 | 3,040 | ClipCollide → HiResScreen | no strings | 5 |
| `8251DAB0` | 1 | 16 | 2,436 | PassiveMessenger → ContentMgr | no strings | 5 |
| `826319F4` | 1 | 1 | 32 | RetryAudioPanel → SaveLoadStatusPanel | a lone 32 B funclet | 5 |
| `8273CBFC` | 1 | 11 | 2,980 | DirUnloader → rnddx9/CubeTex | `cpu` | 5 |
| `82AC7948` | 1 | **702** | 142,524 | Scheduler → ssluse.c | Quazal `/Od` | 4 |
| `82B68D40` | 1 | 7 | 2,288 | **sslgen.c** → FxSend | OpenSSL continuation | 4 |

Totals: **1,146 functions / 194,652 code bytes = 1.82% of `total_code`.**

Identification method: resolve every `lbl_XXXXXXXX` in the live target
`build/45410914/asm/auto_03_*_text.s` against `orig/45410914/band.exe` .rdata
(`tools/xex_string_at.py`'s section mapper), then cross-reference the oracles.
Liveness established by **`objdiff.json` membership**, never mtime (mtime is a
refuted freshness proxy — see `docs/INDEX.md` Known traps).

## Tiers

**Tier 1 — actionable now. `82B7EFBC` = `TourChallengeResultsPanel.cpp`, 12 funclets.**
The single best target in the pool. All eleven of its resolved literals
(`update_setlist_label`, `get_songcount`, `get_challengestars`, `get_songstars`,
`update_songname`, `get_song_total_stars`, `get_challenge_name`, `get_gig_max_stars`,
`get_gig_total_stars`, `get_tour_total_stars`, `get_pregig_total_stars`) are
**exclusive to `~/code/milohax/rb3/src/band3/tour/TourChallengeResultsPanel.cpp`**
within the Wii tour directory — none appears in the adjacent `TourDescPanel.cpp`, which
refutes the competing "this is just more TourDescPanel" hypothesis despite
TourDescPanel's last pinned block ending at exactly `0x82B7EFBC`. Our tree already has
`src/band3/tour/TourChallengeResultsPanel.h` but **no `.cpp`**. Oracle is 120 lines.
Span `0x82B7EFBC–0x82B801A8` is unpinned and hard-bracketed at **both** ends (gap 0/0).
⚠ It sits above `0x82A00000`, but *above* the measured Quazal `/Od` block
(`…–0x82B54190`), so the vendor heuristic does not apply — its neighbours are
TourDescPanel and game code.

**Tier 2 — probably a splits extension, not a port. `823028E8`, 1 funclet.** Bracketed
by `VocalTrackDir.cpp` → `TrackPanelDir.cpp`, and its only literal
(`vocal_harmony_prototype`) lives in `VocalTrackDir.cpp`, which is **already pinned**.
This looks like a scattered COMDAT of a live unit; the fix shape is a `.text` claim,
not a new TU. (Owner note: `splits.txt` is not lane DA-4's to edit.)

**Tier 3 — identified but NO ORACLE: RBN audition, 38 funclets = 50.7% of the 75.**
`82560B08` + `825632BC` are **contiguous** (`…B08–83250`, `…632BC–64408`, 108 B apart)
and jointly intern `audition_mgr`, `audition_main_screen`, `rbn/audition/fail`,
`enter_audition` / `exit_audition` / `can_enter_audition`, `start_validation` /
`is_validating` / `has_validation_failed` / `on_validation_success` /
`on_validation_failed`, `mogg_encryption`, `license_bits`, `get_download_progress`,
`get_loaded_song_status`, `get_network_status`, `sign_out`, and
`An Xbox Without A Signed In Gamer Profile`. This is Rock Band Network audition mode.
**Measured: `audition_mgr`, `can_enter_audition`, `start_validation` and
`mogg_encryption` return ZERO hits in rb3-Wii, in DC3, and in our own tree.** That
matches the known "~18 RBN-authoring classes" 360-exclusive set in
`plans/rb3-360-vs-wii-coverage-2026-07-29.md`. ⇒ **The biggest half of the 75 is the
least tractable**: a pin+port here is from-scratch reconstruction from assembly, not a
port. Do not budget it as a port.

**Tier 4 — vendor, hard-skip (2 funclets).** `82AC7948` is 702 functions / 142 KB
inside the Quazal `/Od` region for **one** funclet — `/Od` code cannot match our `/O1`
flags, and Quazal is out of scope by standing directive. `82B68D40` begins exactly
where `sslgen.c` ends: an OpenSSL continuation.

**Tier 5 — weak or no content evidence (10 funclets across 9 units).** Ranked by
funclets: `82604520` (7, zero resolvable strings), `823ECD58` (5, network search),
`82650B74` (3, ranked matchmaking — literals present only in `Symbols2`, no owning
`.cpp` in the oracle), `8266B3E8` (2), then six singletons. Nothing here is refuted;
it is simply un-evidenced, and `82604520` at 7 funclets is the one worth a second look.

## What was NOT done

No pin, no port, no `splits.txt` or map edit — DA-4 owns `docs/` and this question, not
the pins. Tier 5 was not pushed past literal evidence (callee-set / call-graph
triangulation would be the next instrument). The Tier-1 port was scoped but not
attempted.
