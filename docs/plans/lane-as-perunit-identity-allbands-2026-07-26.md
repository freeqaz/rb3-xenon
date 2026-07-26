# laneAS — the per-unit identity channel across ALL size bands (2026-07-26)

Landed on main as `204d59d2` (+305) and `cc19803c` (+16).
Whole-lane A/B in `~/tmp/wt-laneAS-land`, measured on **current main** `24f508fb`:

> **37,278 → 37,599 = +321 strict, 0 losses, 0 gains outside the fragment**
> (unit+name and name-only deltas identical, so no unit migration and no
> stale-obj phantom).

Predecessor: `docs/plans/lane-aq-identity-funnel-2026-07-26.md`, which proved the
channel exists but worked **only the >68 B slice**.

---

## 1. The pool re-sized from first principles — AQ's slice was 47 % of it

AQ's pool: *anonymous `fn_` targets, in-scope, not strict, **> 68 B*** = 6,542.
The 68 B floor was inherited from laneAL's pool definition, which took it from
laneAN's measurement that base-side `is_funclet_like` symbols are 99.91 % ≤ 68 B.
That is a fact about the **auto-pairing** path, not about which functions a map
entry can reach. Dropping it:

| | count |
|---|--:|
| anonymous `fn_` rows in `report.json` | 44,692 |
| ...in scope (outside vendor `0x82800000..0x82D00000`) | 32,328 |
| ...and not strict — **THE POOL** | **13,233** |
| ├ in `auto_*` carve units | 3,676 |
| └ **in already-pinned units** | **9,557** |
| &nbsp;&nbsp;├ **≤ 68 B** (AQ never looked here) | **4,827** |
| &nbsp;&nbsp;└ > 68 B | 4,730 |

The whole-binary accounting closes exactly: of 32,348 non-strict target
functions, 11,425 are anonymous in the hard-skipped vendor window, 3,676
anonymous in `auto_*` units, 9,557 anonymous in pinned units, 5,400 named in
`auto_*` units at 0 %, 256 named in pinned units at 0 %, and 2,034 named and
partially matched (body divergence). 11,425+3,676+9,557+5,400+256+2,034 = 32,348.

## 2. ★ A tooling defect that hid 651 pool members

`size_order_automap.resolve_unit()` resolves a bare-stem unit name by
`rglob`-ing for `<stem>.obj` and preferring the deepest hit that has a compiled
`src/` counterpart. For the 37 units whose report name is flat but whose base obj
is nested (`default/LightPreset` → `src/system/world/LightPreset.obj`), that rule
**rejects the correct flat target** and picks a same-named nested obj. Result:
651 pool members classified `NO_TARGET_ASM` — i.e. silently dropped.

`objdiff.json` carries the exact `target_path`/`base_path` the diff itself uses.
Resolving from it recovers 650 of the 651 and adds 29 EXACT candidates.
**Any tool built on `resolve_unit()` for a bare-stem unit is under-counting.**

## 3. ★★ A/B contamination source that is NOT in the existing trap list

`setup_worktree.sh` reflinks `build/45410914/` from main — including
`build/45410914/src/*.obj`. When main's working tree is **dirty** (it usually is;
other lanes hold uncommitted `src/` edits), those base objs are artifacts of
*someone else's uncommitted source*. A fresh worktree is clean HEAD, so **any
scan run before the first full build reads base bytes that do not correspond to
any committed state.**

Measured on this lane's own tree, identical code, scan before vs after the build:

| | pre-build (contaminated) | post-build (clean) |
|---|--:|--:|
| EXACT_UNIQUE ≤68 B | 268 | 263 |
| EXACT_UNIQUE >68 B | **73** | **32** |
| EXACT_AMBIG ≤68 B | 461 | 402 |
| SOURCE_MISSING >68 B | 249 | 392 |
| held-out EXACT_UNIQUE precision | 99.39 % | **99.62 %** |
| held-out EXACT_AMBIG precision | 35.48 % | **58.76 %** |

Dirty objs **manufacture spurious byte-twins**, which inflates the ambiguous
class and deflates measured precision. laneAQ's headline 35.48 % EXACT_AMBIG
figure is a contamination artifact.

> **Rule: run a full `./tools/ninja-locked` in a new worktree BEFORE any scan
> that reads `build/45410914/src/*.obj`.** This is separate from, and additional
> to, the `symbols.txt` and `target_symbol_renames.stamp` traps.

## 4. ★★ The "17–68 B scoreable window" is a property of the PAIRING PATH, not of the functions

laneAP measured, over all pinned anonymous `fn_`, a scoreable window of 17–68 B
(> 84 B: 2 matches in 10,511). Read as a ceiling, that would kill this lane.
Two independent measurements settle it.

**Cross-sectional** — strict rate by size band, pinned in-scope units, split by
whether the target carries a name:

| size band | ANON n | strict | rate | NAMED n | strict | rate |
|---|--:|--:|--:|--:|--:|--:|
| 0-16 | 910 | 3 | **0.3 %** | 2,906 | 2,495 | 85.9 % |
| 17-32 | 8,669 | 7,827 | 90.3 % | 599 | 486 | 81.1 % |
| 33-44 | 12,469 | 9,938 | 79.7 % | 319 | 274 | 85.9 % |
| 45-68 | 1,849 | 1,302 | 70.4 % | 1,449 | 1,344 | 92.8 % |
| 69-84 | 453 | 23 | **5.1 %** | 2,203 | 2,045 | 92.8 % |
| 85-128 | 1,250 | 2 | **0.2 %** | 4,862 | 4,466 | 91.9 % |
| 129-256 | 1,310 | 0 | **0.0 %** | 4,064 | 3,677 | 90.5 % |
| 257-512 | 1,108 | 0 | **0.0 %** | 2,023 | 1,670 | 82.6 % |
| 513+ | 634 | 0 | **0.0 %** | 889 | 567 | 63.8 % |

The window exists **only in the anonymous column**. ~12,400 named functions
above 84 B are strict, so the band is not structurally unmatchable.

**Natural experiment** — the same 356 functions, all at exactly 0.0 % while
anonymous, after being given a name:

| size band | n | → 100 | rate | anon base rate |
|---|--:|--:|--:|--:|
| 0-16 | 180 | 180 | **100.0 %** | 0.3 % |
| 17-32 | 66 | 64 | 97.0 % | 90.3 % |
| 33-44 | 27 | 27 | 100.0 % | 79.7 % |
| 45-68 | 21 | 21 | 100.0 % | 70.4 % |
| 69-84 | 5 | 3 | 60.0 % | 5.1 % |
| 85-128 | 9 | 5 | 55.6 % | 0.2 % |
| 129-256 | 19 | 1 | 5.3 % | 0.0 % |
| 257+ | 29 | 4 | 13.8 % | 0.0 % |

Mechanism (laneAN, `objdiff-core/src/diff/mod.rs:1423,1438`): `is_funclet_like`
gates **both** sides of `pair_funclets_by_bytes`, and base-side funclet-shaped
symbols are 99.91 % ≤ 68 B with only 65 above 84 B tree-wide. A target of size N
can only auto-score if its unit's base obj holds a funclet-shaped symbol of
*exactly* N — which predicts the observed cliff location precisely.

### ★ But naming is necessary, not sufficient — the honest refinement

Flip rate above 68 B depends entirely on **what kind of evidence** produced the
name:

| evidence channel | ≤68 B | > 68 B | all |
|---|--:|--:|--:|
| **EXACT** (per-unit exact reloc-masked byte equality) | 237/237 = **100 %** | 7/7 = **100 %** | **244/244 = 100 %** |
| VTABLE (multi-anchor slot alignment) | 51/52 = 98.1 % | **0/33 = 0.0 %** | 51/85 = 60 % |
| WD1 (one differing masked word) | 4/5 | 6/22 | 10/27 = 37 % |

Body-identity evidence flips at 100 % at **any** size (32/32 combined with worker
A's independent 25/25 at > 68 B, against a 0.05 % base rate). Position-only
evidence (a vtable slot) says nothing about the body, so above 68 B — where
bodies actually diverge — it yields **scored body-port targets instead of
matches**. Below 68 B the two are indistinguishable because small functions are
byte-trivial once named.

> **Answer for the next lanes: the window is a gate artifact, and > 84 B is
> reachable — but only through body-identity evidence. It is supply-limited, not
> gate-limited.**

## 5. Held-out precision — and worker B's refutation of my own baseline

Leave-one-out over real map entries (hide a known VA↔name pair, restore the
held-out symbol to supply, re-resolve, score the pick), n = 19,043, clean tree:

| tier | n | precision |
|---|--:|--:|
| EXACT_UNIQUE all | 12,877 | **99.62 %** |
| EXACT_UNIQUE ≤68 B | 2,863 | 99.34 % |
| EXACT_UNIQUE ≤32 B | 1,761 | **99.55 %** |
| EXACT_UNIQUE >68 B | 10,014 | 99.70 % |
| EXACT_AMBIG all | 3,303 | 58.76 % |

**★ The 58.76 % is circular and worker B refuted it.** 837 of the 3,304 held-out
rows have a "truth" the map itself tags `_bijection_arbitrary` / `_icf_arbitrary`
— i.e. the truth was *itself* assigned by the alphabetical tie-break being
measured. Split: **99.04 %** on the arbitrary-truth rows (pure circularity),
**44.95 %** on the 2,467 clean-truth rows. **The honest EXACT_AMBIG baseline is
44.95 %.** This also explains the otherwise-inexplicable "small functions are
easier" inversion — the arbitrary rows skew small.

**Refuted (mine):** that masked-body information content should gate the tier.
EXACT_UNIQUE precision is flat across four orders of evidence density — distinct
non-zero masked words 0-2 → 99.53 %, 3-4 → 99.66 %, 5-6 → 99.27 %, 7-9 → 99.00 %,
10-14 → 99.18 %, 15-24 → 99.72 %, 25+ → 99.68 %; by masked zero-fraction,
0.00-0.10 → 99.39 % up to 0.60+ → 100.00 %. **Uniqueness is self-calibrating**: a
low-information body is only dangerous when it has twins, and if it has twins it
lands in EXACT_AMBIG, not EXACT_UNIQUE. (laneAP's contrary finding was about
*cross-unit, pin-moving* byte twins — a different and much weaker claim.)

## 6. Channels, measured

| channel | shipped | flip | held-out precision | verdict |
|---|--:|--:|--:|---|
| per-unit EXACT_UNIQUE, all bands (lead + worker A) | 244 + 27 | 100 % / 37 % (WD1) | 99.62 % | **the lane** |
| vtable multi-anchor + `--icf-anchors` (worker C) | 85 | 60 % | 93-98 % (ICF-priced) | yield is 100 % ≤68 B |
| reloc-content discriminator (worker B) | 13 | 100 % | **95.62 %** (33-68 B) vs 44.95 % | new channel, real |
| de-size-gated order bijection (worker D) | 60 → 17 marginal | 95.1 % | 99.6 % | confirmed |
| `auto_*` carve arm (worker D) | **0** | — | 82.5 % locality | **structurally unshippable** |

### 6.1 Cross-validation: four methodologically unrelated channels, zero disagreements

| channel | overlap with the landed set | agree | disagree |
|---|--:|--:|--:|
| worker A (per-unit exact bytes) | 264 | 264 | **0** |
| worker C (vtable slot position) | 96 | 96 | **0** |
| worker D (monotone order between name anchors) | 41 | 41 | **0** |
| worker B (relocation content) | 0 | — | — |

Worker D found and disclosed its own single mispair (`0x826f07b8`, forced
`?Enabled@Metronome@@QBA_NXZ`; per-unit exact says `?Enable@Metronome@@QAAX_N@Z`,
reads 70.0 — the per-unit answer is right), dropped it, and re-verified.
Worker D also reports that order-forcing **disambiguates worker B's tier**: 18 of
its entries are per-unit EXACT_AMBIG, all 18 agree and all 18 reached 100.

### 6.2 The `auto_*` arm is closed, with a mechanism

**0 of 681 `auto_*` units carry a `base_path` in `objdiff.json`, and all 19,899
functions in all 3,176 auto units read exactly 0.0 %.** No map entry placed there
can ever produce a strict match. AQ's punt was correct, now with a reason.

Worker D also **refuted its own** first held-out result there: leave-one-out
reported GLOBAL_EXACT_UNIQUE at 10,600/10,600 = 100 %, which is *vacuous by
construction* (the harness restores the truth to supply, so a singleton candidate
set **is** the truth). Under abstention — truth withheld — the tier still fires
977 times in 3,135 and is **95.5 % wrong**; the locality tier fires 670 times and
is **99.7 % wrong**.

> **Any leave-one-out harness that restores the held-out truth to the candidate
> supply must also measure the ABSTENTION case, or its "100 % precision" is an
> artifact of the protocol.** This applies to the harness in §5 as well: its
> numbers are `P(pick correct | truth is in supply)`, which is the right
> conditional for pinned units (where the body demonstrably is compiled) and the
> wrong one for the carve arm (71 % NOMATCH).

## 7. Fixpoint: one shot, not a flywheel

Re-running the funnel on the post-landing tree: EXACT_UNIQUE **270 → 26**, and
**EXACT_AMBIG did not refill at all** (400 → 360, shrinking only by entries
promoted to unique). Consuming a unique name cannot promote an ambiguous group.
Independently confirmed by worker A on a different base.

Worker C's vtable channel reached fixpoint in **2** rounds (round 2: placement
506 → 531 but 0 gated-live; everything new routes to the pinning owner). Worker D
confirms AQ's "order bijection is fixpoint in one round" — the refill from 432
new anchors was **1 candidate**.

**Do not schedule a second round. Re-run only when objs move.**

## 8. Refuted this lane (including our own claims)

* **Mine:** ">68 B entries are worth more per entry than ≤68 B." Both bands flip
  at 100 % for exact-byte evidence. They differ in remaining *supply* (263 vs 32),
  not in value.
* **Mine:** "≤32 B is weaker identity evidence." 99.55 % held-out — the *weakest*
  sub-band is 33-68 B at 99.00 %.
* **Mine:** "an entropy / reloc-density gate should be added." Flat precision; the
  gate is pure recall loss (§5).
* **Mine (via worker D):** "removing the size projection corrupts the alignment
  for large functions too." Removing it while holding a > 68 B pool makes forcing
  *strictly worse* (470 → 183 forced). The real fix is **symmetry** — adding
  base-side `__unwind$` as pure order ballast (never emitted) restores forcing to
  430 and nearly doubles shippable candidates.
* **laneAQ's 35.48 % EXACT_AMBIG** — a contamination artifact (§3).
* **laneAQ's 58.76 % → my restatement of it** — circular (§5); real figure 44.95 %.
* **laneAQ's "318 vtables placed"** — worker C gets 305 on a clean tree; not
  reproducible.
* **laneAQ's "every held-out vtable error is an ICF fold"** — false above 68 B
  (11 of 13 are real misassignments, e.g. `?ScreenDumpUnique@Rnd@@` for
  `?ScreenDump@Rnd@@`). It holds at ≤32 B (19/28) and 33-68 B (8/11).
* **The brief's "AQ size-gated the vtable aligner"** — no size gate existed;
  `--min-size` already defaulted to 0. The real ceiling was a *second* seeding
  defect: `align_all` tolerated ICF-explainable disagreements in the full-run
  check but never on the **anchor** slots, so one ICF-folded anchor killed an
  otherwise-clean alignment. `--icf-anchors` fixes it: placed 305 → 506,
  gated-live 1 → 97, `no_consistent_align` 767 → 72.
* **Worker B's own R1/R2 relocation filters** — well-motivated, worthless:
  378 → 194 decisions for 82.54 % → 82.47 % precision. Pure recall loss.
* **`ELIM_ONLY`** (survivor by elimination, no positive evidence) — 32.86 %
  precision against a 72.86 % baseline. Actively harmful; never ship it.

## 9. Reject controls (what makes the gates credible)

| gate | shipped | reject control, identical protocol |
|---|--:|---|
| worker A WD1 | 34.4 % flip | 105 rejects → **+9** (8.6 %), band `0-50 ×?` |
| worker D order | 60 → **+58** (95.1 %) | 265 rejects → **+5**, band `100×5 · 90-99×42 · 50-90×86 · **0-50×121**` |

## 10. Disclosed ambiguity

* **EXACT_AMBIG was NOT shipped** (423 live candidates, 44.95 % honest precision)
  except for worker B's 13 reloc-decisive entries at 95.62 %.
* Worker C's vtable fragment carries disclosed pollution: **~2 % of the >68 B arm
  and ~6.8 % of the ≤32 B arm are real misassignments**, and a further **~17 % of
  the ≤32 B arm carries an ICF-twin name** that scores 100 but is
  `_bijection_arbitrary`.
* The per-unit EXACT_UNIQUE tier is uniqueness-gated *within its unit*; an entry
  whose base symbol is an ICF twin of a same-bytes symbol in **another** unit is
  not detectable from within-unit evidence and was not attempted.

## 11. Product that never shows in the strict count

**50 VAs converted from unpaired (0.00 %, structurally unpairable) to SCORED
body-port targets**, 15 of them at ≥ 99 %:
`GemPlayer::Penalize` 99.987, `BandProfile::SaveLoadComplete` 99.983,
`ChunkAllocator::Print` 99.983, `AddUserRequestMsg::AddUserRequestMsg` 99.980,
`PreloadPanel::~PreloadPanel` 99.979, `MasterAudio::FillChannelList` 99.966,
`StreamReceiver::Stop` 99.952, `UIList::GetDistanceToPlane` 99.948,
`SongParser::SongParser` 99.853, `Tour::ChooseRandomSongsForQuestFilter` 99.381,
`GemPlayer::CheckHeldNotes` 99.333, `RockCentral::ExecuteConfig` 99.286.

A ready-made cluster lever from worker C: **seven `?SetType@X@@UAAXVSymbol@@@Z`
(252 B each) all read exactly 64.95238 %** — one shared cause.

## 12. Residue, named

**9,088 in pinned units**, none reachable by any identity channel:
`WD4+` 5,821 · `NEARSIZE` 1,925 · `SOURCE_MISSING` 407 · `EXACT_AMBIG` 360 ·
`WD3` 236 · `WD2` 228 · `WD1` 111. Plus **3,674** in `auto_*` units that cannot
score at all (§6.2) and 11,425 anonymous in the hard-skipped vendor window.

Top units by residue: `RockCentral` 222, `VocalTrackDir` 159, `Waypoint` 156,
`UIStats` 153, `Mesh` 146, `BandCharacter` 135, `OvershellSlot` 118, `rndobj/Rnd`
118, `DataFunc` 112, `TrackWatcherImpl` 109, `game/VocalPlayer` 100.

**This is a source lane's problem, not an identity lane's.** Caveat on the
classification: at ≤68 B a body has ~10 masked words, so `WD4+` there means
"≥4 of 10 differ" while at 512 B it means "≥4 of 128" — the class is not
comparable across bands.

## 13. Handoffs

### 13.1 Over-covering `.text` pins (worker C, 42 found; `splits.txt` NOT touched)

Two verified independently by the lead from `splits.txt` geometry:

| interloper span | contains | evidence | belongs to |
|---|---|---|---|
| `DirLoader.cpp [0x8275A64C..0x8275AB18)` | `?Copy@Object@Hmx@@` `0x8275A898`+236, `?FindPathName@Object@Hmx@@` `0x8275A9D0`+324 | 8 and 6 vtable anchors | `Object.cpp` — confirmed sandwiched between `Object.cpp` blocks `[0x8275A5C0..0x8275A64C)` and `[0x8275AB18..0x8275AB8C)` |
| `SongStatusMgr.cpp [0x825D4604..0x825D46B4)` | `?IsActive@SortViewSetting@@` `0x825D4608`+172 (the whole span) | 10 anchors | `band3/meta_band/ViewSetting.cpp` — confirmed between its blocks ending `0x825D4604` and resuming `0x825D46B8` |

Also near-certain (not independently re-verified):
`FlowValueCase.cpp [0x823B9744..0x823B9D38)` holding `?PollDeps@CharWeightSetter@@`;
`AccomplishmentSongListConditional.cpp [0x825E8AE8..0x825E8C30)` holding two
`AccomplishmentSongFilterConditional` methods. Full 42-row table with
vtable/slot/anchor evidence:
`/home/free/tmp/laneAS/c_vtm_icfa/multianchor_frag_unpinned.json`; a further 69
unpinned VAs are a separate handoff.

### 13.2 Overlapping data carves (worker B)

**2,036 `lbl_` VAs disagree in content** between `auto_06_82C34400_data` and
`auto_06_82C64400_data`. At `lbl_82C6C064` the `82C34400` carve reads
`"...MatchMakingService"` while `82C64400` reads the correct RTTI descriptor
`.?AVBandDirector@@`. A real splits defect.

### 13.3 Next levers, priced but unworked

1. ★ **The 1,207 `_bijection_arbitrary` map entries.** Worker B's discriminator
   can re-decide them and its content sub-channel is **100 % held-out** (33/33).
   This is *identity correction* rather than new coverage — the largest untouched
   application of the channel.
2. **230 `TIE_WITH_EVIDENCE` rows** (216 of them ≤32 B) — argmax-by-agreement with
   a margin requirement is the obvious next move; untested.
3. **Worker C's 659 refused tied alignments** — a secondary tie-break on fewest
   ICF-soft disagreements; the held-out harness now prices it in one process.
4. 35 gate-passing reloc candidates at ≤32 B (83.9 %) and 1 at >68 B (85.4 %),
   deliberately refused at laneAQ's tier-C 93.3 % tolerance.

## 14. Reproducing

```bash
scripts/setup_worktree.sh ~/tmp/wt-X laneX
cd ~/tmp/wt-X && git checkout -- config/45410914/symbols.txt \
  && touch config/45410914/config.yml \
  && rm -f build/45410914/{report.cache,target_symbol_renames.stamp} \
  && ./tools/ninja-locked            # MANDATORY before any scan -- see §3
python3 /home/free/tmp/laneAS/perunit_funnel.py --worktree ~/tmp/wt-X \
        --min-size 0 --max-size 68 --out funnel.json
python3 /home/free/tmp/laneAS/heldout_exact.py --worktree ~/tmp/wt-X
python3 /home/free/tmp/laneAS/compose.py --worktree ~/tmp/wt-X --frag A=... --out c.json
python3 /home/free/tmp/laneAS/strictdiff.py snap <report.json> snap.json
python3 /home/free/tmp/laneAS/strictdiff.py diff before.json after.json --frag c.json
```

Worker tooling committed on its branches: `laneAS-B`
(`scripts/harvest/reloc_disc/`), `laneAS-C` (`vtable_multianchor.py
--icf-anchors`), `laneAS-D` (`order_anchored_bijection.py --min-size /
--demand-min-size / --supply-unwind`, `autocarve_global_identity.py`).
