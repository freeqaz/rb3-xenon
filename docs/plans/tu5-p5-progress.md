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
| wave 6 | 15,207 | +2 (LightPreset body port + 1 unicorn fix); **3 veins closed** |
| BinStreamRev lever | 15,226 | +19 foundational (base-at-0 inheritance; streaming family); LEAD B ~+15-20 pending |
| wave 7 | 15,227 | +1 (Game::Poll gameplay RE); bodyport premise refuted, re-anchor vein found |
| re-anchor | 15,236 | +9 (EndingBonus/GemSmasher drifted-map refresh; size-match guardrail) |
| jeff Class-2 merge | 15,303 | +67 (leaf-fragment merge pass; `eb4863cc`, jeff `7f69b9e`) — fleet tooling lever |
| BinStreamRev LEAD B w1 | 15,319 | +16 (per-TU rev statics, 4-lane wave; `677fc117`) — 9 ICF/sibling bonus flips rode along |
| jeff Class-4 over-carve merge | 15,354 | +35 (post-blr/branch over-carve tail merge; `f03b9719`, jeff `b50881e`) — 2nd fleet tooling lever |
| SongSort identification | **15,364** | +10 (verified map entries, SongStatusMgr family; `528c51c7`) — vein drained, rest ICF-walled |

Ceiling reference: 15,804 (recover-all-but-48-sanctioned). Remaining gap
≈ 440, now overwhelmingly **deep work** (below). **All cheap/tooling/identification
veins are drained.** New reusable instrument: `scripts/harvest/tu5_reloc_masked_
correlate.py` (byte-identity pairing after masking COFF relocs — bypasses TU5
address drift; the right tool for whole-unit-0% drifted units). Note:
`gen_game_target_map.py` is DEAD for TU5 (its `unified_id_rb3wii.json` oracle is
TU0-addressed → all `out_of_span`) — use the correlator instead. **Cheap veins + both tooling
multipliers (BinStreamRev LEAD A, jeff leaf-split) are landed, and LEAD B wave 1
is in.** Remainder is BinStreamRev LEAD B wave 2 (evidence-selected by
`scripts/harvest/leadb_signature_scan.py` — incl. cross-TU rev owners like
SampleZone←MidiInstrument::Load), permuter-harness tooling, jeff Class-1/3
(terminatorless fragments, stray except_data), and genuine C_DIVERGED body ports
(Matchmaker rewrites, SongSort gen_game_target_map identification wave).

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
1. **jeff leaf-split fragment merges — Class 2 (+67, `eb4863cc`) + Class 4 (+35,
   `f03b9719`) BOTH LANDED; Class 1 & 3 NO-GO.** Class 2 = fall-through PDATA-less
   leaf merge (`merge_fallthrough_leaf_fragments`). Class 4 = post-`blr`/branch
   over-carve tail merge (`merge_branch_reached_overcarve_tails`, jeff `b50881e`),
   its exact complement — the ~85% root cause of the low-% (0.3-1%) named-fn mass
   (110 branch-proven groups, 193 tails; branch-target-proof P2′ is load-bearing).
   Census (`~/tmp/jeffc13/design.md`) settled the rest: **Class 3 NO-GO** (pop 0,
   already solved by b1bc97c write-gate) and **Class 1 NO-GO** (64% false
   positives — 522 guard-c noreturn-`bl` would CORRUPT if extended; genuine ~33
   low-yield, cleanest already swept by Class 4). Convergence note: committed
   symbols.txt must be the FIXED POINT (Class-4 climbs 15,342→15,354 over re-splits
   via symbols.txt feedback; byte-stable d12af934). Remaining jeff work: none
   high-value — the ~15% genuine-divergence remainder (OnSetMode-class) is a
   body-port lane, NOT jeff. Handoffs: `docs/plans/jeff-leaf-split-fix-status.md`,
   `~/tmp/jeffc13/design.md`, `~/tmp/lowpct-diag/verdict.md`.
2. **BinStreamRev base-at-0 inheritance** — LEAD A LANDED (`2b7b557a`, +19),
   LEAD B wave 1 LANDED (`677fc117`, +16). Per-TU `static unsigned short sXxxRev`
   (retail's `lbl_82CC*` halfword) set once at the outer `Load` entry from the
   popped rev, replacing `d.rev`/`d.altRev` reads in inner sub-Loads. **Width is
   per-TU** — `unsigned short`/`lhz`+`cmpwi` (most) vs signed `int`/`lwz`+`cmpwi`
   (Morph). Wave 2 selected by `scripts/harvest/leadb_signature_scan.py` (target
   `lhz lbl_82CC*` vs our `lwz 0xC(rN)` member). NOTE from wave 1: LEAD-A residual
   backlog was optimistic — several entries walled by non-rev causes (element
   ctor, Symbol default-construct, struct-size); and some sub-loaders inherit
   their rev **cross-TU** from a different outer Load (SampleZone←`MidiInstrument
   ::Load`) — wave 2 must set the static in the owner TU. Codegen fingerprint
   (reusable): extra `lwz <off>` where retail does `mr r3,&this` =
   composition-vs-inheritance mismatch on any Milo wrapper type.
3. **Re-anchor drifted whole-unit-0% units** (REPLACES the refuted "969
   unported" vein — wave 7 proved game .cpp is all wired+ported; the ~6,113
   absent symbols are 95% out-of-scope XDK/audio). Real vein: fully-ported
   units reading whole-unit-0% because their target_symbol_map addresses
   drifted base→TU5 (EndingBonus map@0x822C1xxx vs split@0x822D39xx). The
   wave-6 sweep gated these out ("unit has ≥1 matched fn"). Being tested; if
   it lands, relax `scripts/harvest/tu5_stale_anchor_sweep.py`'s gate. Also:
   Matchmaker/Session misanchors, Ham↔Band DC3-leak naming.
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
