# Campaign state — 2026-08-17 (third edition)

> **STATUS (2026-08-17): CURRENT — this edition replaces
> `CAMPAIGN_STATE_2026-08-14.md` wholesale** (the precedent CONSOLIDATE-1 set).
> Every byte absolute below is on the **shipped `name_check` ruler** unless
> tagged `@none`. Written by the coordinator from two Opus analysis lanes
> (GAPMAP-1: gap decomposition; DOCAUDIT-1: history + docs audit), both run
> 2026-08-17 with **all decomp lanes paused**.

## 1. Headline, measured at HEAD `6e13ee3f`

| key | value | provenance |
|---|---:|---|
| `matched_functions` | **44,444 / 69,227** | `report.json` regenerated at HEAD (the prior on-disk report was one merge stale — it predated laneR's +204 B/+1 fn and GROUNDED-2's +1,728 B; composition checks exactly: 3,721,772 + 204 + 1,728 = 3,723,704) |
| `matched_code` | **3,723,704 / 10,320,664 B = 36.080082%** | same |
| honest floor | **21,546** (= matched − masked_equal 22,898) | same |
| fuzzy (whole-binary) | 48.591938 | same |
| ruler | `name_check` (self-declared in `provenance.diff_config`) | same |

Per-category (dashboard `progress_categories`; the five categories cover
8,594,604 B — the remaining 1,726,060 B is exactly the unattributed `auto_*`
class, which carries no category by construction):

| category | total_code | matched_code | code% | wmean fuzzy |
|---|---:|---:|---:|---:|
| game | 2,114,248 | 1,356,216 | 64.15% | 82.47 |
| engine | 3,995,832 | 2,239,228 | 56.04% | 78.19 |
| thirdparty | 105,740 | 94,352 | 89.23% | 93.77 |
| network | 269,640 | 31,496 | 11.68% | 17.41 |
| sdk | 2,109,144 | 480 | 0.02% | 0.03 |

*(Category rows are from the 06:49 report snapshot; the +1,932 B HEAD delta
lands in game/engine rows and does not change any percentage by more than
0.09 pp.)*

## 2. The partition — where every byte of the 10.32 MB stands

Computed by lane GAPMAP-1 (2026-08-17) from `report.json` joined to
`objdiff.json` + per-unit COFF symbol counts. **Self-validated four ways**:
rows sum to `total_functions`, bytes to `total_code`, Σsize(fuzzy==100) ==
`matched_code`, count(mpn==100) == `matched_functions`. Computed at the 06:49
snapshot (1,932 B / 1 fn behind HEAD — immaterial to every share below).

| # | class | rows | bytes | % total_code |
|---|---|---:|---:|---:|
| 1 | **MATCHED** (`fuzzy==100`) | 38,649 | 3,721,772 | **36.06%** |
| 2a | UNPAIRABLE — no source (229/230 units `xdk/*`, out of scope) | 4,454 | 2,106,356 | 20.41% |
| 2b | UNPAIRABLE — `auto_*` unattributed (identification) | 10,101 | 1,726,060 | 16.72% |
| 2c | UNPAIRABLE — map-scaffold units (base obj ≤2 syms) | 914 | 180,196 | 1.75% |
| 3a | PAIRABLE, arg-only (`mpn==100`, `fuzzy<100`) — **drained** | 5,794 | 797,144 | 7.72% |
| 3b | PAIRABLE, `fuzzy∈[95,100)`, `mpn<100` | 1,634 | 253,352 | 2.46% |
| 3c | PAIRABLE, `fuzzy∈(0,95)`, `mpn<100` | 959 | 337,064 | 3.27% |
| 3d | PAIRABLE, `fuzzy==0` (96% of bytes are ANONYMOUS targets ⇒ identification, not unported code) | 6,722 | 1,198,720 | 11.62% |
| | **TOTAL** | **69,227** | **10,320,664** | **100.00%** |

The 2c threshold is a genuine cliff, not a fitted cut (≤1 sym: 0 units; ≤2:
105; ≤3: 109), and reproduces AUTOID-1's 08-13 figure to the byte.

## 3. Reachable ceiling — 61.12%, and we stand at 59.0% of it

```
PAIRABLE bytes (unit has a base obj)          6,488,248 = 62.867%  raw
  − map-scaffold shells                         180,196
= CORRECTED reachable ceiling                 6,308,052 = 61.121%
matched_code 3,723,704  =  59.03% of the corrected reachable surface
gap-to-corrected-ceiling                      2,584,348 B
```

**What moved since 08-13/08-14:** ceiling fell 63.10% → 62.87% raw, fully
attributed — PAIRABLE lost exactly the 24,276 B that `auto_*` gained (a pin
reattribution, not a regression); no-source and scaffold classes byte-identical
across all three measurements. Ceiling vs 08-14 is flat (−1,304 B). ⛔ Standing
rule unchanged: **the ceiling moves both ways — re-measure it, never inherit.**

## 4. The gap, replayed on the 08-14 five-class scheme — shares stable to ~1 pp

| class (over the 2,766,476 B pairable gap) | bytes | % gap |
|---|---:|---:|
| anon `fuzzy==0` — **IDENTIFICATION**, unpairable at any source quality | 1,328,620 | **48.0%** |
| arg-only / reloc-name — **DRAINED** (~91% irreducible fold/map noise) | 797,144 | **28.8%** |
| named partial — **DIVERGENCE** (the real write surface) | 528,996 | **19.1%** |
| anon partial | 61,420 | 2.2% |
| named `fuzzy==0` (no body/stub) | 50,296 | 1.8% |

Divergence by category: **engine 387,776 B · game 126,520 B · network
9,828 B · thirdparty 4,872 B.** Within it, **93.6% of bytes are ARG-GATED**
(closing the instruction mismatches buys `mpn=100` / +1 fn and **zero bytes**
until register/branch/reloc-name charges also resolve); the clean
pure-source-collects-everything surface is **33,788 B (0.33 pp)** — and
5,036 B of that is the drained `CustomizePanel::Handle`. Structural bound
unchanged: pure regalloc rows cannot appear in `mpn<100`, so the permuter
floor inside divergence is 8.1% (permuter OFF by user directive).

⚠ Audit note from the replay: the 08-14 record's own five rows sum to
2,761,960 B against its stated 2,762,688 B gap — a 728 B internal leak in
that memory. Shares unaffected; today's replay has zero residue.

## 5. Alias-mechanism exposure — hold this number next to the headline

`scripts/symbol_aliases.json` at HEAD: **1,528 groups / 15,196 folded
memberships** (= MAPID-1's 15,190 + GROUNDED-2's 6 restorations, exact); 29
groups carry withdrawals, 6 restorations, nothing pruned. Magnitude by
ablation (ALIAS-2, `64088f62`): **818,416 B / 7.93 pp** — **~22% of everything
we count as matched rests on this mechanism.** Evidence split: PROVEN 92.73% ·
NEEDS_SOURCE 1.96% · CONTRADICTED 1.78% · NEEDS_MAP_ID **0.00% (drained to
zero by MAPID-1)**. **129,360 B is irreducible by construction** —
relocation-free thunks where ICF destroyed which name the site meant. The 11%
"unattributable" class from GROUNDED-1 was adjudicated a **census blind spot**:
0 of 1,894 rows depend on a non-proven membership.

## 6. What the last fortnight did (08-01 → 08-16, ~130 lanes)

Full narrative in DOCAUDIT-1's audit; one line per arc:

1. **Pin/splits hygiene** — ~1.7 MB vendor pinned; `total_code` de-inflated
   (one 204 B fn had been billed 210,136 B). Drained.
2. **Source-lever grind** — rev-statics +13, container types, `/fp:fast`
   paren barrier; `MILO_WARN` refuted thin; cheap-model lane closed by user.
   Drained.
3. **Ruler flip to `name_check` (08-12)** — the pivot: −817 kB / −7.9 pp from
   the ruler alone, `matched_functions` bit-identical; two tools found on the
   wrong ruler in opposite directions, both now resolve it at runtime. See
   `RULER_CHANGE_name_check_2026-08-12.md`.
4. **Map-defect campaign** — the period's best byte source: DC3-signature-on-
   RB3-address class (+5,392, +5,976, +5,236 B rows…), transposed pairs,
   splits re-homing. Concentrated in game; engine thin; network zero.
5. **Alias-ledger adjudication** — sized (818 kB), tiered, 80 fabricated
   memberships withdrawn (−10,916 B predicted exactly), `NEEDS_MAP_ID`
   drained (−1,656 B, exposing 6 real wrong-callee bugs), the "+8 B STLport
   source bug" refuted as our own COFF reader, 6 folds restored (+1,728 B).
   Deliberately net-negative: accuracy over headline.
6. **Body-port waves** — INSDEL-1..5/SRCARG/FAMILY/STORE-2: real fixes
   (comparator inversion +1,804 B, `EnumerateOffers` port +692 B…); the class
   is **bounded at ~9.6 kB actionable**, not the 440 kB it opened as.
7. **Measurement epistemics** — `ab_measure` tree-restore matrix fixed (8/12
   exit paths corrupted the tree), split fixed-point iteration added, native
   gate audited sound, ~35 instrument failures catalogued. Arguably the
   period's real product.
8. **DB integrity** — `IDENTITY_UNESTABLISHED` verdict wired into all 13 work
   selectors so a row we can't vouch is *the* function is never offered again.

Net 08-14→08-16: **+40 functions / −1,856 B** — the sign is the directive
working, not a regression.

## 7. ROADMAP — the honest paths from 59.0%-of-reachable

**Framing first: "fully matching" cannot mean 100% of the XEX.** 20.4% is
Microsoft vendor source we will never write (out of scope by standing
directive) and ~1.75% is map scaffolding. The meaningful end-states, in the
order the standing directives rank them:

**W0 — Correctness debt (small, behavioural, fund first).** The six
allocator-spelling divergences MAPID-1 exposed (`MemAlloc` / `_MemAlloc` /
`_MemAllocTemp` — the temp allocator is a *different allocator*, so these are
real bugs, near-zero bytes). Plus the `RndBone`↔`FilePath` swapped-`sizeof`
map mis-assignment and 21 sibling per-instantiation `sizeof` divergences.
~1–2 lanes.

**W1 — Game-layer divergence (the priority layer, per directive).**
126,520 B named divergence + 50,296 B named-no-body across band3/network.
Rank by size-if-it-crosses at fuzzy≥95 but **price from `report.json`'s
charged-site list, never a mismatch count** — 19 of the top 20 are arg-gated.
Expect many fixes to land as Δbytes 0 / +1 fn (mpn-only) — land them anyway;
`mpn==100` rows can still hide wrong callees. Sustainable yield: single-digit
kB per lane.

**W2 — Engine divergence (387,776 B, secondary per game-first directive).**
Same discipline. DC3-verbatim units cannot be adjudicated by source diff
(DC3 is newer); adjudicate on retail bytes.

**W3 — Identification at scale (the only route to the big class: 1.33 MB anon
`fuzzy==0` + what's reachable of 1.73 MB `auto_*`).** Byte-cheap per row
(naming buys rows at 0%) but it is what converts UNPAIRABLE→PAIRABLE and
raises the *ceiling*. The funded route is infrastructure, not grinding:
Ghidra+BinDiff transfer of dc3's named functions (`ham_xbox_r.map` Rosetta) —
standing plan, never executed (XEXLoaderWV needs a Ghidra 12.1 rebuild).
⚠ Naming under `name_check` is a bet: right name = 0, wrong name = new
charge; its payout is **bug exposure, not bytes** (MemAlloc precedent).
⚠ Only ~8.9% of `auto_*` is attributable-and-portable; do not fund blind
attribution.

**W4 — Alias ledger residue.** TEMPLATE-1 (~98 kB, largest open queue; needs
a demangled→mangled join); 76 withheld contradictions (`verdict()` fallback ≠
refutation); the 129,360 B floor is accepted, not work.

**W5 — Gated / declined (do not fund without new evidence).** Permuter
(user OFF; floor 8.1% of divergence); jeff P1 relaxation for
`DataNode::operator==` (fleet-shared binary, declined 3×); `/TC-/TP` split
and `/EHsc`-`c` residue (metric-only evidence); sdk/xdk source (out of
scope); stubbing no-source units (metric-fitting).

**The strategic statement, unchanged from INSTR-1 and re-confirmed today:
there is no big lever left.** Every large-looking lever was sized and
deflated. From here the campaign is (a) a long, honest grind on W1/W2 in
single-digit-kB increments, (b) the W3 identification infrastructure play,
which is the only thing that moves the *ceiling*, and (c) the native port
(`hub_native.md`: **the user's stated real goal**), for which the matching
metric is instrumental, not terminal.

## 8. Top-20 crossing candidates (fuzzy≥95, mpn<100), with the trap labelled

See GAPMAP-1's table in the transcript / `docs/INDEX.md` pointer. Headline
rows: `CharacterCreatorPanel::Handle` 5,164 B (~209 est arg sites — likely a
register cascade downstream of one defect), `VocalPlayer::Handle` 4,936 B
(~224), `Spotlight::SyncProperty` 4,728 B (~103), `BandDirector::OnFileLoaded`
3,816 B (**~2 sites — RESIDUAL-1 shape, independent charges**),
`SaveLoadManager::SetState` 4,096 B. ⚠ `CustomizePanel::Handle` (5,036 B)
reads CLEAN and is **DRAINED** (RESIDUAL-2: 9 shapes inert + 3 worse) — do
not re-brief. Small est-site counts (2–3) = independent reloc charges that
survive a body fix; large counts = probably dissolve with the real defect.
Both readings are bets, not verdicts.

## 9. Provenance

- Gap partition, ceiling, top-20: lane GAPMAP-1, 2026-08-17, scripts in
  `~/tmp/gapmap/`, from `report.json` (06:49 snapshot) + `objdiff.json` +
  COFF symbol counts; four exact-sum validations passed.
- History + docs audit: lane DOCAUDIT-1, 2026-08-17, from merge-commit
  bodies 08-01→08-16 + `CAMPAIGN_STATE_2026-08-14.md` + docs sweep.
- HEAD baseline: report regenerated at `6e13ee3f` by the coordinator,
  2026-08-17 (`rm report.json report.cache && ninja <report>`; 3,088 units,
  0 cache hits).
- Alias figures: `docs/decomp/ALIAS_UNPROVEN_REMAINDER_ADJUDICATED_2026-08-16.md`,
  `ALIAS_NEEDS_MAP_ID_DRAINED_2026-08-16.md`, GROUNDED-2 merge `6e13ee3f`.
