# TU5 P5 — post-flip matching progress

**Status:** ACTIVE. Single current-state tracker for the post-TU5-flip matching
campaign. Companion to `tu5-landing-runbook.md` (the flip itself) and
`tu5-p5-manifest.md` (the enumerated-drop attribution). Updated per wave.

## Scoreboard

| point | matched | note |
|---|---|---|
| TU5 flip landed | 14,847 | `d9c44305`, 2026-07-15 |
| struct-rebase waves 1–4 | 15,100 | +253, keystone header re-bases (Player/BandDirector/Game/…) |
| fixwaves (concurrent) | 15,160 | +60, ≥99% near-miss triage lanes |
| wave 5 + 5b | 15,205 | +45, online/SongSort/TrackConfig **map re-anchors** + 1 permuter flip |
| wave 6 | **15,207** | +2 (LightPreset body port + 1 unicorn fix); **3 veins closed** |

Ceiling reference: 15,804 (recover-all-but-48-sanctioned). Remaining gap
≈ 597, now overwhelmingly **deep work** (below).

## Vein status (what's drained vs live)

**DRAINED / CLOSED (do not re-run — evidence in memory `project_tu5_p5_wave6`):**
- **Struct-rebase keystones** — the cascading member-insert wins are captured
  (Player/BandDirector/Game/BandUser/GameMode/User/Loader/MemMgr + wave-6
  residual). Yields decayed 111→112→26→4→~0.
- **Stale-anchor map sweep** — Ghidra ground truth agrees with the map on all
  6,178 zero-scoring named entries. Reusable tool
  `scripts/harvest/tu5_stale_anchor_sweep.py` emits 0 proposals at this baseline.
- **B_STRUCT_OFFSET residual** — no clean cascading struct cause left; remainder
  is tooling/permuter (see live veins).

**LIVE (ranked by leverage):**
1. **jeff leaf-split fix (Class 2 `.pdata` over-splits)** — dtk emits genuine
   single bodies as 2+ anon `fn_` fragments at retail `.pdata` boundaries; the
   SongSortBy{Rank,Song,Artist,Review,Recent} clusters + SongStatusMgr/Track/
   TrackWidget are blocked on this (named fns already 100%, trailing fragments
   never pair). Design: `docs/plans/jeff-pdata-boundary-round3.md` Class 2.
   Fleet-wide multiplier, source-side-unfixable. **← active tooling session.**
2. **BinStreamRev base-at-0 inheritance** — TU5 models `BinStreamRev : public
   BinStream` (base off 0); ours composes `stream@0x8` + delegates → extra `lwz`
   per `d >> field` across the world/rndobj streaming family. Prototype
   measuring; verdict `~/tmp/p5w6/binstreamrev-verdict.md`.
3. **969 unported-source names** — names reading 0%/0-fuzzy whose symbol is
   absent from our compiled objs (the original pre-flip body-port/TU-wiring
   vein, now enumerated). Body-port waves.
4. **Real TU5 gameplay RE** — Game::Poll-class (divergence sketches in
   `~/tmp/p5w6/cbodies-notes.md`: cached demo bool@Game+0x30, movie-sync block
   calling fn_826C91C8), Matchmaker genuine rewrites.
5. **Permuter sweeps** — CharHair/GamePanel regalloc + 11/14 of the wave-5
   permuter queue still walled.
6. **Unicorn behavioral vein** — object_memory/call_arg classes actionable on
   sub-100 fns (calibrated wave 6; modest yield, best decomp-synth training
   signal). Structural leads: BandCamShot layout, XboxSessionJob base size.

## Documented walls (source-side unfixable — need tooling/permuter)

CharClip rbtree 0x18/0x1c ODR coupling; MusicLibrary::PlaySetlist GameMode
vbase vtordisp; CharClipSet fn_823D0AFC anon inner-class +4; BandDirector::
SyncProperty local-static (−38); Character::Lod / HamNavProvider::NavItem
shrink mirages; LightPreset EnvironmentEntry-vs-SpotlightEntry target-map
mispair (needs re-anchor not header).

## Method (stable across waves)

Workflow crack→review, both Opus in isolated worktrees; coordinator harvests the
**review** worktree's clean diff (never the JSON — round-trips corrupt),
path-limited commit, whole-binary A/B on main vs a snapshot, 0-regression gate.
Map edits need `touch config/45410914/config.yml` before each A/B leg (renamer
re-split trap). Reviewers re-baseline against live main (concurrent-session
drift). Training rows to `~/tmp/grind_runs/` → B2 corpus.
