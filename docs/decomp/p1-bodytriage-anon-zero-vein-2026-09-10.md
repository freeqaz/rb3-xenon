# The last large vein triaged by ORACLE AVAILABILITY — **DO NOT FUND** (lane P1-BODYTRIAGE, 2026-09-10)

> **VERDICT: DO NOT FUND as a wave.** The oracle-backed slice the roadmap hoped
> for **does not exist**. Of the ~1.32 MB, the genuine "write a body from an
> oracle" surface is **≤ 3.4 kB (0.03% of `total_code`)**, the Quazal slice is
> **96.8% inside the measured `/Od` region** and therefore unmatchable by our
> `/O1` build at any source quality, and the remaining ~83% is *divergence in
> code we already hold* whose historical conversion is **0/10 TUs and 0/55
> methods**. This closes item 4 of
> `docs/plans/ROADMAP_GAP_TO_TARGET_2026-09-01.md` §7 — the last item on it.

Read-only lane. No `src/` edits, no builds, no A/B. Worktree `~/tmp/wt-p1body`
off `5aa1cb7a`.

## 0. Provenance — re-derived, not inherited

`build/45410914/report.json`, objdiff **4.2.8** (`032122696555`, xxh3
`14ac591a…`), ruler `functionRelocDiffs=name_check` read from
`provenance.diff_config`. `scripts/analysis/freshness.py::ensure_measurable`
**PASS** (manifest 4,293 objects, split current, tool identity OK, alias map not
newer than the report).

| measure | value |
|---|---:|
| `total_code` | **10,245,956 B** / 69,219 fns |
| `matched_code` | **3,774,924 B = 36.843%** |
| `matched_functions` | 42,305 · `masked_equal` 22,915 |

⚠ The roadmap's own headline (3,772,844 / 42,276) is already stale by the
map/alias repairs landed since — **+2,080 B / +29 fns**. Re-deriving was not a
formality.

## 1. The vein reproduces; its decomposition does not

The briefed definition — anonymous (placeholder-named) rows at `fuzzy == 0`
inside units that HAVE a base obj — measures **7,257 rows / 1,319,540 B =
12.88% of `total_code`** (briefed 1,322,732 B; reproduces to **0.24%**).

It is **not one class**, and the crosswalk to the established class-3 census is
exact:

```
class 3 (no body held anywhere, has base obj)   1,176,404 B   6,695 rows
  − rows already partially credited (0<fuzzy)      −58,644 B  −1,603 rows
  + rows where we DO hold the body elsewhere     +201,780 B  +2,165 rows
                                               = 1,319,540 B   7,257 rows  ✓
```

The 201,780 B where **we already hold a byte-identical body** is not a body
problem at all — it is the bijective-identification channel, already adjudicated
to ~0.2% of `total_code` (`tools/ident_body_channel.py`, IDENT-1). The
fundable question therefore lives in class 3: **1,176,404 B (report) /
1,176,040 B (asm extents) / 11.48%**, reproduced independently by this lane's
tool and **asserted equal** to `nobody_unit_census.py` (`--expect-rows 6695
--expect-bytes 1176404`).

## 2. Four briefed figures re-measured — three do not survive

| briefed | measured today | verdict |
|---|---|---|
| "**~71% is missing bodies**" | **17.7%** (208,588 B); divergent/other **82.3%** | ⛔ **inverted** |
| "~13% fold-ambiguous by construction" | **3.9%** same target obj; **10.0%** under the loosest definition (body duplicated in *any* target obj) | ⛔ over-stated |
| "mostly Quazal-flavored" | Quazal is **17.9%** of the vein; engine is **51.9%**, game **26.1%** | ⛔ true only of the *UNWRITTEN* subset (97.3%), not the vein |
| 1,322,732 B | 1,319,540 B | ✅ reproduces (0.24%) |

The 17.7% figure is not new: **GRIND-1 measured 17% on 2026-08-14** and the
record sat in `docs/decomp/bodywrite-surface-repriced-GRIND1-2026-08-14.md` the
whole time. This lane reproduces it within 0.5% on a different tree. *The
briefed decomposition was the one number nobody re-read the in-tree record
for.*

## 3. The partition, by class (asm extents, precedence: fold-ambiguity > origin)

| class | units | rows | asm B | % vein | % `total_code` |
|---|---:|---:|---:|---:|---:|
| **C_SYSTEM** — Milo engine (DC3 oracle) | 437 | 3,237 | **610,764** | 51.9% | 5.96% |
| **B_BAND3** — HMX game (rb3-Wii oracle) | 168 | 1,606 | **306,404** | 26.1% | 2.99% |
| **A_QUAZAL** — network middleware | 110 | 885 | **210,580** | 17.9% | 2.06% |
| **E_FOLD_AMBIG** — body dup'd in own target obj | — | 956 | 46,840 | 4.0% | 0.46% |
| OTHER / **D_XDK** | 4 | 11 | 1,452 | 0.1% | 0.01% |
| **TOTAL** | **719** | **6,695** | **1,176,040** | 100% | 11.48% |

Shape (reproduced from `nobody_unit_census.py`): **719 units, median 788 B,
largest 24,188 B, ZERO units ≥ 32 kB.** Half the class needs ~91 separate TUs
opened, 75% needs ~206. There is no fat target anywhere in it.

## 4. Oracle availability — the axis this lane was chartered to add

`tools/p1_body_oracle_triage.py`. **Oracle PRESENCE is not oracle VALUE**: our
`src/system/` is a verbatim DC3 copy and DC3 is *newer*, so a file existing at
the same path proves nothing. A unit is `ORACLE_SURPLUS` only when an oracle is
materially larger than what we already hold.

| class | oracle state | units | asm B |
|---|---|---:|---:|
| **B_BAND3** | **WE_ALREADY_HOLD** | **166** | **299,312** |
| | NONE | 1 | 7,040 |
| | ORACLE_SURPLUS | **1** | **52** |
| **C_SYSTEM** | WE_ALREADY_HOLD | 358 | 495,276 |
| | ORACLE_SURPLUS | 71 | 113,996 |
| | oracle-only (our src is a scaffold) | 5 | 756 |
| **A_QUAZAL** | **SCAFFOLD_NO_ORACLE** | **101** | **170,100** |
| | WE_ALREADY_HOLD | 8 | 40,192 |

★ **(b), the slice the roadmap hoped to fund, is empty.** Of band3's 306 kB,
**97.7% is in units where our source already equals or exceeds the rb3-Wii
oracle** — one unit / **52 bytes** has any oracle surplus at all. Even relaxing
to "the Wii file has *more lines* than ours" finds only 15 units, every one of
them with a **negative** function-count deficit (MetaPerformer −666, Game −819,
ProfileMgr −559): we compile *more* code than retail's TU contains, so the extra
Wii lines are dev-build-only functions retail never shipped. This is exactly the
suspicion GRIND-1 flagged and could not close; it is now closed.

★ **(c)'s `ORACLE_SURPLUS` 113,996 B is a LINE-COUNT MIRAGE.** Intersect it with
the direct measure — retail code symbols in the pin minus symbols our base obj
defines — and the surface collapses to **6 units / 988 B**. Every other
`ORACLE_SURPLUS` unit has deficit ≤ 0, i.e. is not short of code. GRIND-1 warned
the line-count proxy conflates comments and dev-build code; this quantifies it
at **115×**.

## 5. Quazal is not merely low-value — it is STRUCTURALLY UNMATCHABLE

CLAUDE.md (lane CF-4) measured a `/Od` region at
**`0x82A6D168`–`0x82B54190`** = Quazal NetZ. Testing this vein's rows for
membership:

| class | in `/Od` band | of | share |
|---|---:|---:|---:|
| **A_QUAZAL** | **210,940 B** | 218,008 B | **96.8%** |
| C_SYSTEM | 1,088 B | 638,468 B | 0.2% |
| B_BAND3 | 0 B | 318,476 B | **0.0%** |

⇒ The Quazal slice is compiled `/Od` in retail while our build is `/O1`.
**No source is byte-faithful across that boundary**, so class (a) cannot pay at
any effort — a harder kill than the standing "network is the analogue of XDK"
directive. ★ The controls are what make this readable: band3 at **0.0%** and
engine at **0.2%** show the test discriminates rather than firing on everything.
Corroboration from inside the tree: our own `DuplicatedObject.cpp` carries a
lane's note that *"the retail `/Od` TU stores just the code-address word … we
cannot reproduce that exact frameless single-word store"* — a previous lane hit
this wall and recorded it in source.

(⚠ Denominator note: this probe classes rows by unit origin without the
fold-ambiguity precedence, so its Quazal denominator is 218,008 B rather than
the 210,580 B in §3. I did not re-derive the `/Od` band boundaries themselves —
inherited from CF-4; I tested membership only.)

## 6. The genuine write surface, adjudicated

`UNWRITTEN` (deficit > 0) is **121 units / 208,588 B**, and **97.3% of it is
Quazal** (202,900 B) — i.e. the class that §5 just killed. Non-network residue:
**18 units / 5,688 B**. Adjudicating its head:

- `band3/tour/TourCondition` (1,424 B, deficit +17) — **NOT unwritten.**
  Already adjudicated on retail bytes by GRIND-1: our 146-line source defines
  every method; the detached block is genuinely TourCondition's.
- `band3/meta_band/StandIn` (848 B, deficit +7) — **NOT unwritten.** Ours is 59
  lines against the Wii oracle's 58, with an **identical method list**
  (`SetNone`/`SetName`/`SetGuid`/`IsNone`/`IsPrefabCharacter`/`IsCustomCharacter`/
  `SaveSize`/`SaveFixed`/`LoadFixed`). The +7 is the documented COMDAT/funclet
  bias in `nobody_why.py`'s supply side.
- Three entries are **vendored third-party** living under `src/system` (curl
  `ssluse.c` 216 B, tomcrypt `aes.c` 68 B, LIBCMT `osfinfo.cpp` 52 B).

⇒ Both band3 `UNWRITTEN` units are refuted, so the honest write surface is
**≤ 3,416 B ≈ 0.03% of `total_code`**, and the `ORACLE_SURPLUS ∧ deficit>0`
intersection independently says **988 B**. Two methods, same order of magnitude,
both ~0.

## 7. Ranked "size-if-it-crosses" — and why the ranking is not a worklist

`matched_code` is all-or-nothing per row, so a unit pays only if **every** one
of its rows reaches `fuzzy == 100`. Priced on **asm extents**, never
`report.json` sizes (the known hazard: one row billed 8,852 B for a 12-byte
`return true`; here `MidiParser` is *under*-billed 6,544→7,496 and
`MusicLibraryStore` 6,016→7,424).

| unit | asm B | deficit | unit matched | oracle | class |
|---|---:|---:|---:|---|---|
| `DuplicatedObject` | 24,188 | +49 | 1% | WE_ALREADY_HOLD | **A** `/Od` |
| `RockCentral` | 20,368 | −353 | 56% | WE_ALREADY_HOLD | B |
| `VocalTrackDir` | 15,544 | −1313 | 56% | WE_ALREADY_HOLD | C |
| `quazal/…/PRUDPEndPoint` | 12,992 | +64 | 0% | SCAFFOLD_NO_ORACLE | **A** `/Od` |
| `BandSongMetadata` | 12,252 | −618 | 43% | WE_ALREADY_HOLD | B |
| `quazal/ObjDupProtocol` | 12,060 | +53 | 0% | SCAFFOLD_NO_ORACLE | **A** `/Od` |
| `rndobj/Text` | 10,412 | −467 | 42% | ORACLE_SURPLUS | C |
| `GemManager` | 10,288 | −1288 | 39% | WE_ALREADY_HOLD | B |
| `hamobj/MoveMgr` | 10,272 | −1848 | 13% | WE_ALREADY_HOLD | C |
| `world/LightPreset` | 10,092 | −5743 | 62% | WE_ALREADY_HOLD | C |

Full table: `docs/decomp/p1-bodytriage-by-unit-2026-09-10.tsv` (719 rows).

**Every non-Quazal row in the head has a large NEGATIVE deficit** — these units
are not short of code, they are units where we compile far more than retail's TU
holds and our bodies diverge. `LightPreset` compiles 5,743 more code symbols
than retail's pin contains. Opening them is near-miss matching *plus* a per-row
identification, not body porting.

## 8. Honest expected yield, and its basis

The remaining ~917 kB (vein minus Quazal, fold-ambiguous and XDK) is divergence
in code we already hold. The measured base rates for converting exactly this:

- **0 of 10 fresh TUs landed** end-to-end through the identity-transfer
  pipeline (B2 5 + harvest 5), *with the oracle-misattribution screen working* —
  `docs/decomp/identity-transfer/B2-FINDINGS-oracle-wall.md`. Good-oracle method
  hit rate **0/55**, 28 methods body-diverged.
- **Real-bodied band3 functions sit at 0–11% fuzzy with zero near-misses** —
  "the ported rb3-Wii *dev* source is structurally different from retail bytes"
  (`docs/plans/remaining-matching-work-handoff.md`).
- Mis-pin contamination re-measured today: **8 units / 43,092 B MIS-PIN SUSPECT
  (3.66%)** with **44.24% UNDECIDABLE** — so an unknown further share of the
  vein is not ours to write at all.

At the historical conversion rate the expected yield of funding this vein is
**indistinguishable from zero**. To reach even **+0.5 pp of `total_code`** you
would need ~51 kB to cross — ~65 units at the class median, each requiring
per-row identification *and* a byte-exact body, against a measured 0/10 TU
record. I can construct no scenario in which that is the best use of the next
lane.

## 9. Recommendation

**DO NOT FUND** items (a)–(e) as a porting wave:

- **(a) Quazal / network** — 210,580 B. **96.8% `/Od`**; unmatchable at `/O1`.
  Out on structure, not just on the directive.
- **(b) band3 with an rb3-Wii oracle** — 306,404 B, of which the oracle-backed
  slice is **1 unit / 52 B**. *The fundable slice does not exist.*
- **(c) engine with a DC3 oracle** — 610,764 B; `ORACLE_SURPLUS` collapses
  115× to **988 B** against the direct deficit measure. We already hold this code.
- **(d) XDK** — 88 B. Out of scope by directive; irrelevant by size.
- **(e) fold-ambiguous** — 46,840 B (3.9%). Cannot pay at any effort.

**What is worth doing instead**, all cheap and already-identified: the ~3.4 kB
genuine write surface is *not* worth a lane on its own, but the 6 units in §4's
intersection are legitimate opportunistic targets. The vein's real value is
**accuracy, not bytes** — `DuplicatedObject` at 1% matched with a +49 deficit,
and the 8 MIS-PIN SUSPECT units (43,092 B), are places where the *pin* or the
*source* is wrong in ways that matter to the native port.

## 10. What this lane did NOT verify

- **No builds, no A/B, no `src/` edits.** Every number is read from the settled
  tree's `report.json` + COFF objects. Nothing here is a measured delta.
- **Did not re-derive the `/Od` band boundaries** (inherited from CF-4); I tested
  row membership and ran its controls only.
- **Did not adjudicate the 44.24% UNDECIDABLE pin state.** The 3.66% mis-pin
  figure is a lower bound; more of the vein may be foreign TUs. That direction
  only *shrinks* the vein further.
- **Did not adjudicate individual `C_SYSTEM` `ORACLE_SURPLUS` units on retail
  bytes** — only via the function-count deficit, whose bias runs both ways
  (`nobody_why.py` §5).
- **Did not run objdiff per row**, so I have not shown any specific row *could*
  cross with source work; I have shown the population's oracle backing is absent
  and its historical conversion is ~0.
- **Did not run `tools/native_build_gate.sh`** — correctly, as no `src/` file
  moved.
- LOC is a crude oracle proxy and is reported only beside the function-count
  deficit, which is the discriminator I actually relied on.
