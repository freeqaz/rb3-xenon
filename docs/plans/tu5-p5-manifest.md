# TU5 re-base — P5 enumerated-drop manifest

**Date:** 2026-07-15  **Role:** P5-manifest (makes the TU5 flip legal under the roadmap NO-GO gate: *unexplained losses > ~159 = NO-GO*).

**Machine-readable manifest:** `docs/plans/tu5-p5-manifest.json` (repo copy; scratch original at ~/tmp/tu5_forensics/)
(losses[] one row per lost fn with class+evidence+sanctioned; relabels[] audit; sanctioned_not_dropped[]).

## Bottom line

Every one of the **1,034 net-deficit** functions (TU0 matched 15852 → TU5 gate 14818) is attributed to an explained bucket with per-function evidence. **Unexplained residue (UNRESOLVED) = 20, vastly under the 159 budget → the flip is a legal GO** (satisfied by explanation, not by hitting 15,804).

## Arithmetic reconciliation (closes exactly)

```
losses (not matched in TU5)      = 1407   = A 527 + B 480 + C 380 + UNRESOLVED 20
TU5-only gains                   = 373
net deficit                      = 1407 - 373 = 1034
CHECK: 15852 (TU0) - 1407 losses + 373 gains = 14818  ==  14818 (TU5)   ✓

Full population split of the 15852 TU0-matched fns:
  RELABEL (anon fn_ matched under shifted VA, NOT a loss)  = 8086
  named matched under same name (excluded, "appear matched") = 6359
  losses (A/B/C/UNRESOLVED)                                  = 1407
  8086 + 6359 + 1407 = 15852  == 15852  ✓
```

## Bucket table

| class | count | mechanism | recovery path |
|---|---:|---|---|
| **A_TOOLING** | 527 | body present in TU5 (byte/reloc-identical, or found at a shifted VA) — loss is pure `base_to_tu5_map`/pairing coverage gap. **Zero source change.** | complete the VA-map anchoring (fixes 1+2) |
| **B_STRUCT_OFFSET** | 480 | present near-miss: D-form load/store IMM shift (Harmonix re-based class members TU0→TU5) or tail insert/delete. | source header retype per shared class (fix 3) — the struct-rebasing campaign |
| **C_DIVERGED** | 380 | genuine rewrite / different call-target-overload / removal / inline. Includes 45 sanctioned drops that dropped. | body-port vs oracle (fixes 4/5) + accept sanctioned set |
| **UNRESOLVED** | 20 | name absent from the entire TU5 report **and** the whole source unit is unanchored in `base_to_tu5_map` — mechanism indeterminate. | better per-unit anchoring will resolve |
| *(audit) RELABEL* | 8086 | anon `fn_<VA>` matched in TU5 under a shifted VA name — **not a loss**, already counted in the 14818. | n/a |

**Unexplained residue: 20 ≪ 159 budget.** The 20 UNRESOLVED are all in 6 wholesale-unanchored small units (default/Defines, default/EndingBonus, default/Trig, default/band3/game/VocalGuidePitch, default/band3/game/GemTrainerPanel, default/GemSmasher); their base VAs never anchored to a TU5 VA and their mangled names have zero occurrences in the TU5 report, so their mechanism (removed vs present-as-anon) cannot be proven from current artifacts. Even if all 20 were genuine unexplained losses, 20 < 159.

## Classification method + evidence priority

Spine = `census.json` (15,852 TU0-matched rows, authoritative `gate` ∈ matched/degraded/absent). Losses = degraded (631) + absent (776) = 1,407. Per-fn class assigned by priority:
1. `honest_losses.json` **confirmed[]** (traced tu5_va + tu5_fuzzy proof): fz≥90 → B, fz<90 → C.
2. `classify_all_out.json` **scoring** (191 named report-level regressions, 92% struct-offset IMM) → B.
3. UNRESOLVED/ERR rows → **direct TU5-report name lookup** (mangled names are stable): present norm≥90 → B; present <90 → C; **name-absent + unit siblings mapped → C (removed/inlined)**; name-absent + whole unit unanchored → UNRESOLVED.
4. `census.refined` heuristic: RELOC_ONLY/SIZE_MISMATCH degraded → B (masked IMM); UNMAPPED-FOUND degraded → B, absent → A; RELOC_ONLY/STRICT absent-or-identical → A; UNMAPPED-GONE2/STRUCTURAL_DIFF → C.

Each `losses[]` row carries its `evidence` string and source.

## Sanctioned expected-drops disposition (all 48 accounted, 0 in UNRESOLVED)

| disposition | count | note |
|---|---:|---|
| dropped, class **C_DIVERGED** | 40 | genuine game-code change (BandDirector, MusicLibrary, GameMode, Overshell*) |
| refined to **B_STRUCT_OFFSET** | 3 | per-fn scoring proves present near-miss (AccomplishmentManager x2, ContextChecker) — stronger evidence than the coarse VA-class drop heuristic |
| refined to **A_TOOLING** | 2 | ≤44B stub relabel (XboxContentMgr::Terminate, UserMgr::GetLocalUserFromPadNum) |
| **did NOT drop** (matched in TU5) | 3 | ToCode@DateTime, DrawString@Rnd, DataSymbol@Overshell — census gate=matched (better than expected) |

> **Sanity-gate note (loud):** the sanity gate asked for "all 48 in C". Per the task's own evidence-priority rule (honest_losses/scoring **override** the coarse `expected_drops` VA-class heuristic), **5 sanctioned drops refine out of C** (2→A tiny-stub, 3→B near-miss) and **3 did not drop at all**. All 48 remain `sanctioned:true`-tagged and enumerated; **none leaked into UNRESOLVED**. The gate's intent — no sanctioned drop silently unexplained — is fully met.

## Top-20 units per bucket

### A_TOOLING (map/pairing gap — recover via anchoring, zero source change)
| unit | fns |
|---|---:|
| `default/BandDirector` | 71 |
| `default/band3/meta_band/BandSongMgr` | 39 |
| `default/PlatformMgr` | 31 |
| `default/SongSortByArtist` | 25 |
| `default/SongSortByRecent` | 17 |
| `default/MusicLibrary` | 14 |
| `default/Crowd` | 14 |
| `default/SongSortMgr` | 13 |
| `default/GameMode` | 12 |
| `default/LightPreset` | 12 |
| `default/band3/meta_band/Campaign` | 12 |
| `default/SongSortByRank` | 12 |
| `default/SongSortByStars` | 11 |
| `default/SongSortByReview` | 11 |
| `default/band3/meta_band/ViewSetting` | 8 |
| `default/SongSortByDiff` | 8 |
| `default/SongSortBySong` | 8 |
| `default/band3/game/Player` | 7 |
| `default/system/synth_xbox/Synth` | 6 |
| `default/Sequence` | 6 |

### B_STRUCT_OFFSET (struct re-basing campaign — the biggest source lever)
| unit | fns |
|---|---:|
| `default/band3/game/VocalPlayer` | 43 |
| `default/BandDirector` | 39 |
| `default/band3/game/Game` | 31 |
| `default/GemPlayer` | 16 |
| `default/SongSortMgr` | 12 |
| `default/LightPreset` | 11 |
| `default/BandMachine` | 10 |
| `default/Mesh` | 9 |
| `default/band3/game/Player` | 9 |
| `default/SongSortByRank` | 7 |
| `default/SongSortBySong` | 7 |
| `default/VocalTrack` | 6 |
| `default/CharClip` | 6 |
| `default/system/synth_xbox/Synth` | 6 |
| `default/DataArray` | 6 |
| `default/SongSortByArtist` | 6 |
| `default/SongSortByReview` | 6 |
| `default/system/rndobj/Utl` | 5 |
| `default/PropKeys` | 5 |
| `default/DataFile` | 5 |

### C_DIVERGED (body-port / accept)
| unit | fns |
|---|---:|
| `default/SongSortNode` | 20 |
| `default/Matchmaker` | 17 |
| `default/SongStatusMgr` | 13 |
| `default/SessionUsersProviders` | 10 |
| `default/NetworkEmulator` | 7 |
| `default/TrackWidget` | 6 |
| `default/EventTrigger` | 5 |
| `default/LightPreset` | 5 |
| `default/PropKeys` | 5 |
| `default/Archive` | 5 |
| `default/band3/bandtrack/Track` | 5 |
| `default/band3/meta_band/PrefabMgr` | 5 |
| `default/GameMicManager` | 5 |
| `default/band3/game/Game` | 5 |
| `default/BaseGuitarTrackWatcherImpl` | 5 |
| `default/SongSortByStars` | 5 |
| `default/FocusTracker` | 4 |
| `default/OvershellSlot` | 4 |
| `default/Crowd` | 4 |
| `default/Joypad` | 4 |

### UNRESOLVED (the entire unexplained residue)
| unit | fns |
|---|---:|
| `default/Defines` | 8 |
| `default/EndingBonus` | 5 |
| `default/Trig` | 3 |
| `default/band3/game/VocalGuidePitch` | 2 |
| `default/band3/game/GemTrainerPanel` | 1 |
| `default/GemSmasher` | 1 |


## B-bucket struct-offset pair table (seeds the struct-rebasing campaign)

Concrete D-form IMM offset shifts extracted from objdiff (`classify2_out.json` examples). `tu0` = current source offset, `tu5` = retail TU5 offset. One header member correction cascades to every reader.

| unit | function | fz% | offset shift(s) tu0→tu5 |
|---|---|---:|---|
| GameMode | `?InMode@GameMode@@QAA_NVSymbol@@@Z` | 99.9706 | `0x18`→`0x1c`, `0x18`→`0x1c` |
| LightHue | `?Sync@LightHue@@AAAXXZ` | 99.9945 | `0x48`→`0x4c` |
| Instance | `?Save@WorldInstance@@UAAXAAVBinStream@@@Z` | 99.9692 | `0x8`→`0xc`, `0x8`→`0xc` |
| PitchCorrectedVoice | `?SetMinIntegrationTime@ExposureRecipe@TrueCo` | 99.5 | `0x14`→`0x20` |
| ContentMgr_Xbox | `?NotifyFailed@XboxContentMgr@@MAAXPAVContent` | 99.9756 | `0x71`→`0x75` |
| Dir | `??1ObjectDir@@UAA@XZ` | 99.9921 | `0x4c`→`0x50` |
| User | `?SyncSave@User@@UBAXAAVBinStream@@I@Z` | 99.9667 | `0x70`→`0x74` |
| StreamPlayer | `?PlayFile@StreamPlayer@@QAAXPBDMM_N@Z` | 99.9643 | `0x0`→`0x1` |
| Loader | `?GetLoader@LoadMgr@@QBAPAVLoader@@ABVFilePat` | 99.9643 | `0x8`→`0xc` |
| SongInfoCopy | `??0SongInfoCopy@@QAA@PBVSongInfo@@@Z` | 99.9907 | `0x4c`→`0x50` |
| Singer | `?GetFrameMatchType@Singer@@QAAHXZ` | 99.9091 | `0x390`→`0x394` |
| SongDB | `?GetCommonPhraseID@SongDB@@QBAHHH@Z` | 99.9783 | `0x2f`→`0x31` |
| ContextChecker | `?CheckContextModeProperty@?A0x1e5d0754@@YA_N` | 95.9231 | `0x0`→`0x4` |
| Performer | `?LoseGame@Performer@@QAA_NXZ` | 99.973 | `0x41`→`0x43` |
| OutfitConfig | `?Load@?$ObjRefConcrete@VColorPalette@@VObjec` | 91.7069 | `0xec`→`0xdc`, `0xec`→`0xdc` |
| AppLabel | `?SetUserName@AppLabel@@QAAXPBVUser@@@Z` | 99.9583 | `0x70`→`0x74` |
| GemManager | `?IsSpotlightGem@GemManager@@QAA_NHAA_N@Z` | 99.9825 | `0x2f`→`0x31` |

Class-level pairs called out in the forensics prose (apply at the header, not per-fn): `GameMode` `0x18→0x1c`, `User` `0x70→0x74`, `Singer` `0x390→0x394`, `GemPlayer` `0x3ac→0x3b0`. The remaining ~460 B rows lack an extracted pair here (only the 27-fn stratified `classify2` sample was instruction-diffed); extract per-unit via objdiff `mismatches`/`asm_listing` as the campaign works each header. B concentrates in `VocalPlayer / BandDirector / Game / GemPlayer / SongSortMgr / LightPreset` — start there.

## Pointers

- Machine-readable: `docs/plans/tu5-p5-manifest.json` (repo copy; scratch original at ~/tmp/tu5_forensics/) — `meta`, `losses[]` (name/unit/base_va/tu5_va/class/evidence/sanctioned/size/tu5_fuzzy/offset_pairs), `relabels[]` (8,086 audit rows), `sanctioned_not_dropped[]` (3).
- Synthesis: `~/tmp/tu5_forensics/F1_DECISION.md`; lanes: `CENSUS.md`, `SCORING.md`, `CHURN.md`, `honest_losses.json`.
- Sanctioned floor: `~/tmp/tu5_floor/expected_drops.json`.
