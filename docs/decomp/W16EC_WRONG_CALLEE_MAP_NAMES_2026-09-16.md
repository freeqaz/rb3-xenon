# W16-EC — candidate wrong-callee pairs adjudicated by correcting MAP NAMES

**Date:** 2026-09-16 · **Branch:** `w16-ec` off `f729af14` · **Worktree:** `~/tmp/wt-w16-ec`
**Ruler:** every percentage and byte figure below is the **graded `name_check`** ruler, read
from `build/45410914/report.json` (provenance `functionRelocDiffs=name_check`,
`ppc.calculatePoolRelocations=false`) after a full `./tools/ninja-locked` build, or from
`tools/ab_measure.py`'s archived leg reports. No figure here comes from `run_objdiff`'s
headline percent, and no figure is inherited from a prior lane without re-measurement.

This is an **accuracy and bug-exposure** lane, not a byte chase. Lane MPNGAP-1's verdict —
*"~91% of the `diff_arg`-only stratum is irreducible fold/map noise; do not re-fund this as
a byte lever"* — **stands and is not challenged here.** Four names were corrected; the
refutations in §3 are the larger part of the deliverable.

## 0. Net result

| measure | leg A (`f729af14`) | leg B (W16-EC) | delta |
|---|---:|---:|---:|
| `matched_functions` | 0 | **0** | **+0** |
| `matched_code` | 0 B | **0 B** | **+0 B** |
| `matched_code_percent` | 0.000000 | **0.000000** | +0.000000 pp |
| `fuzzy_match_percent` | 0.000000 | **0.000000** | +0.000000 pp |
| `masked_equal_functions` | 0 | 0 | +0 |
| honest (`matched - masked_equal`) | 0 | 0 | +0 |

Denominator read from the same reports, never hardcoded: `total_code` = 0 B,
`total_functions` = 0 (identical on both legs).

**Units:** `default/BandUser` 145 -> 147 · `default/NetGameMsgs` 118 -> 119 ·
`default/StarDisplay` 96 -> 97 · `default/UIFontImporter` 148 -> 149.
**Zero unit regressions, zero rows lost anywhere in the binary.**

**Rows that moved** (all five, complete):

| unit | row | size | fuzzy |
|---|---|---:|---|
| `default/StarDisplay` | `?GetStarCountForSymbol@StarDisplay@@SAHVSymbol@@@Z` | 100 B | 99.8000 -> **100** |
| `default/UIFontImporter` | `?OnAttachToImportFont@UIFontImporter@@IAA?AVDataNode@@PAVDataArray@@@Z` | 80 B | 99.7500 -> **100** |
| `default/BandUser` | `?SetDifficulty@BandUser@@QAAXVSymbol@@@Z` | 60 B | 99.6667 -> **100** |
| `default/BandUser` | `?SetTrackType@BandUser@@QAAXVSymbol@@@Z` | 60 B | (row absent) -> **100** |
| `default/BandUser` | `?SetControllerType@BandUser@@QAAXVSymbol@@@Z` | 60 B | 99.3333 -> (row gone; it was never collecting) |
| `default/NetGameMsgs` | `?Dispatch@SetUserTrackTypeMsg@@UAAXXZ` | 132 B | 99.8485 -> **100** |

**Provenance:** `tools/ab_measure.py --worktree ~/tmp/wt-w16-ec --from-dirty`, run dir
`.ab_measure_runs/20260916-020528-from-dirty-3728269`. Both legs settled to a zero-work build; **both at a `symbols.txt` split fixed
point** (so the ABSPLIT-1 under-report does not apply); leg B forced a re-split with
`renamer_patched=1830`, so the map edit was **not inert**; `objdiff-cli` sha256
`c1b7d95240a35cd6` stable across both legs. The `none` control is reported
NOT_APPLICABLE by the tool and is correct to be: the patch contains source, so a flat
`none` is also the wrong-callee-fix signature and adjudicates nothing about aliases.

## 1. The four adjudications

### FIX A — `BandUser`'s two `Symbol` wrappers were transposed (map, 2 rows)

| VA | was | is |
|---|---|---|
| `0x8268bab0` | `?SetControllerType@BandUser@@QAAXVSymbol@@@Z` | **`?SetDifficulty@BandUser@@QAAXVSymbol@@@Z`** |
| `0x8268bb68` | `?SetDifficulty@BandUser@@QAAXVSymbol@@@Z` | **`?SetTrackType@BandUser@@QAAXVSymbol@@@Z`** |

**Evidence — the inner enum overload, read off retail bytes.** Each wrapper converts a
`Symbol` and tail-calls the enum-typed overload, and *that callee* identifies it:

```
fn_8268BAB0:  bl fn_8268F770 (?SymToDifficulty@@YA?AW4Difficulty@@VSymbol@@@Z)
              bl fn_8268BA18 (?SetDifficulty@BandUser@@QAAXW4Difficulty@@@Z)   => SetDifficulty(Symbol)
fn_8268BB68:  bl fn_8277B530 (unnamed Symbol->enum converter)
              bl fn_8268BB00 (?SetTrackType@BandUser@@QAAXW4TrackType@@@Z)     => SetTrackType(Symbol)
```

Both enum overloads sit at **distinct VAs, both at fuzzy 100**, so they are not folded with
each other; and two wrappers whose relocations differ **cannot** be ICF-folded at all (MSVC
folds only COMDATs identical *including relocations*). This discriminator is immune to the
fold hazard that defeats byte-identity, which is why it — not byte-identity — is the
load-bearing evidence.

**A prior T1 finding was correct and mis-interpreted.** Lane W16-CU recorded byte-identity
between our compiled `?SetTrackType@BandUser@@QAAXVSymbol@@@Z` and retail `0x8268bb68` and
read it as *a fold of two spellings*. It is not: it proves that VA **is** `SetTrackType(Symbol)`.
The alias group at `0x8268bb68` was therefore **withdrawn** (`folded: []` + a `withdrawn`
record; nothing pruned, per the never-prune rule) and the map repaired instead. Both VAs were
removed from `_bijection_arbitrary` because their identity is now established on retail
relocations.

Safety: `build/45410914/src/band3/game/BandUser.obj` defines
`?SetTrackType@BandUser@@QAAXVSymbol@@@Z` (COFF symbol count 1), so the renamed row can pair —
clearing the *"proving a name wrong does not make renaming safe"* trap.

### FIX B — `StarDisplay::GetStarCountForSymbol` calls the free function (source)

Retail calls the **free** `GetStarsToken` (`fn_8231C458`, defined in the same TU, our body at
fuzzy 100 against it), not the static member `StarDisplay::GetSymbolForStarCount`. Proven by
the charged `bl` at index 9 naming `?GetStarsToken@@YA?AVSymbol@@H@Z`, and corroborated by the
same finding already recorded in `SongSortByStars.cpp`. Our tree carries **both** spellings, so
this is a genuine wrong-callee in our source, not a naming artifact. Row 100 B, fuzzy 99.8000,
1 of 25 instructions charged.

### FIX C — `UIFontImporter::OnAttachToImportFont` calls `ImportSettingsFromFont` directly (source)

The rb3-Wii dev oracle routes through an `AttachImporterToFont` wrapper; retail does not. The
charged `bl` at index 10 names `?ImportSettingsFromFont@UIFontImporter@@QAAXPAVRndFont@@@Z`,
and our body of *that* name is byte-exact against retail `0x82818840` (1036 B, fuzzy 100).
**Retail bytes outrank the oracle.** `AttachImporterToFont` is left defined (now uncalled) so
the native link is unaffected. Row 80 B, fuzzy 99.7500.

### FIX D — `NetGameMsgs`' two `Dispatch` names were transposed (map, 2 rows)

| VA | was | is |
|---|---|---|
| `0x82691068` | `?Dispatch@SetUserDifficultyMsg@@UAAXXZ` | **`?Dispatch@SetUserTrackTypeMsg@@UAAXXZ`** |
| `0x826910f0` | `?Dispatch@SetUserTrackTypeMsg@@UAAXXZ` | **`?Dispatch@SetUserDifficultyMsg@@UAAXXZ`** |

**This row was found by the A/B, not by the queue** — see §5. Correcting FIX A withdrew a
subsidy: `0x8268bb68` had been misnamed `SetDifficulty`, which accidentally *agreed* with our
source at this call site, holding the row at 100 for the wrong reason. Repairing the wrapper
exposed a second wrong name underneath, and the row fell 100 -> 99.84849 (-132 B).

**Evidence is threefold and entirely on retail data.**

1. **Callee.** `fn_82691068` ends `bl fn_8268BB68` = `SetTrackType(Symbol)`; `fn_826910F0` ends
   `bl fn_8268BAB0` = `SetDifficulty(Symbol)`. A message carrying `mTrackType` dispatches to the
   track-type setter.
2. **vtable co-location** (retail `.rdata`, found by scanning `band.exe` for the big-endian
   pointers — the map has no `??_7` rows for these classes):

   | vtable blob | `Dispatch` | `Name` (+0x10) | `StaticByteCode` (+0x0C) |
   |---|---|---|---|
   | `0xDC040` | `0x82691068` | `0x82677EF0` = **TrackType** | `0x82675D20` = **TrackType** |
   | `0xDC0F0` | `0x826910F0` | `0x82677FF0` = **Difficulty** | `0x82675E68` = **Difficulty** |

   Two independently-mapped members of the same class accompany each `Dispatch` pointer.
3. **Why the names were arbitrary in the first place.** `Load`, `Save`, `Name` and the
   destructors of these twin classes *did* fold across the two classes (they have alias groups
   in `scripts/symbol_aliases.json`). `Dispatch` is the one member whose relocation differs, so
   it could not fold, both copies survived, and the two names were assigned by bijection —
   **both VAs were listed in `_bijection_arbitrary`**, i.e. the map itself declared which name
   belonged on which VA to be unestablished. Both were removed on correction.

Safety: `build/45410914/src/band3/game/NetGameMsgs.obj` defines **both** mangled names (1
each), and our source spells the matching callee in each body
(`SetUserTrackTypeMsg::Dispatch` -> `SetTrackType`, `SetUserDifficultyMsg::Dispatch` ->
`SetDifficulty`), so both rows can pair. No alias covers either `Dispatch` spelling.

## 2. The instrument

For each charged pair: **does the named callee's signature match the call site?** — extended
with two discriminators that proved decisive and are reusable:

- **The inner enum overload.** Where a family of one-line `Symbol` wrappers differs only in a
  `bl`, the *callee's* mangled type settles which wrapper it is. Byte-identity cannot, because
  the bodies are identical modulo that relocation.
- **vtable co-location on raw retail data.** Where a virtual's name is arbitrary, scan the image
  for its VA as a big-endian word; the enclosing vtable blob carries sibling slots whose names
  are independently established. This needs no map row for the vtable itself.

And one structural rule used throughout: **two functions that call different callees cannot be
ICF-folded**, so if both candidate names exist at distinct VAs, a residual charge naming them is
a *mis-named VA*, not a fold.

## 3. Refuted / examined and deliberately NOT changed

- **`PropSync<RndPollable*>` (`0x82405750`) and its pair — sized, evidenced, and priced at ZERO.**
  Both halves show the same three `diff_arg` charges (indices 64/71/77), consistent with a
  transposition. But the existing alias covers `?_M_fill_insert_aux@?$vector@PAVRndDrawable@@...`
  folding the `Object*` instantiation — that is `_M_fill_insert_**aux**`, **not** the charged
  plain `_M_fill_insert`. So a name swap would take each row from 3 charges to 1 and buy
  **0 bytes**. Recorded as a pure-accuracy lead; not attempted.
- **The shuffled `PropKeys::Copy` family** — same shape, same zero pricing. Left for a lane that
  wants accuracy rather than bytes.
- **`map_misassignment` sub-class: confirmed DRAINED and NOT re-run** (27 rows, zero live), per
  the coordinator's standing instruction.
- **No alias was installed anywhere.** Every fix here is a falsifiable *name correction*. An
  unproven alias lifts `name_check` by construction, and the `none` control cannot catch a
  fabricated one — that flatness is the signature of the hazard, not a clearance.
- **No `splits.txt` re-home** was attempted. Re-homing an already-pinned address is measured
  non-neutral; none of these fixes needed it.

## 4. Inherited figures that did not survive re-measurement

- **Liveness of the residual stratum.** The brief carried *203 still live* of the 535
  `unclassified: candidate genuine wrong callee` pairs in
  `docs/plans/wrong-callee-triage-2026-08-12.json`. My recount gives **140 live / 376 dead /
  19 unknown**. Recorded rather than silently accepted; the gap is most likely a different
  liveness key (target row vs victim rows). **Re-derive before funding a lane off either number.**
- **`docs/decomp/nogroup-wrong-callee-queue-NOGROUP1.tsv`** re-checked today: 313 rows ->
  **91 LIVE (11,048 B) / 102 DEAD (26,104 B) / 120 ABSENT (3,456 B)**. Both artifacts are dated
  2026-08-12 and every figure in them should be treated as stale until re-checked.
- **`obj_target_symbol_renamer`'s docstring says "10 sub-100" `_bijection_arbitrary` rows.**
  Measured this lane: **57 sub-100 rows / 3,728 B**. The docstring is stale.

## 5. Pre-registrations, and the one that MISSED

Stated before any measurement, and reported here whether or not they held.

**Wave 1 (FIX A+B+C): predicted +300 B / +4 fns. MEASURED +168 B / +3 fns — a MISS.**
Every per-fix prediction was exactly right at row level (A +120/+2, B +100/+1, C +80/+1 =
+300/+4 gained). The entire shortfall was **one unpriced collateral**: `NetGameMsgs`
`?Dispatch@SetUserDifficultyMsg@@UAAXXZ` fell 100 -> 99.84849, **-132 B / -1 fn**.
My stated downside risk named the *wrong mechanism* — I expected the alias-1353 withdrawal to
un-forgive callers; the real cause was a caller un-pairing because a **second** map name was
wrong. This is the doctrine's *"un-pairing is ~80% of a map edit's delta"* and *"a wrong name is
financed by its callers"* observed directly.

**The miss is the lane's most productive event.** The regression is what located FIX D; the
queue never surfaced it.

**Wave 2 (FIX A+B+C+D): predicted +432 B / +5 fns**, units `BandUser` +2, `StarDisplay` +1,
`UIFontImporter` +1, `NetGameMsgs` 118 -> 119. Note the `Dispatch` pair nets **+132 B, not
+264** — one of the two rows was already collecting bytes under the wrong name, and a subsidy
must not be booked twice. **MEASURED: +432 B / +5 fns -- EXACT.** Units moved exactly as predicted, with zero regressions and zero rows lost. The `Dispatch` pair contributed exactly **+132 B**: `SetUserTrackTypeMsg::Dispatch` crossed 99.84849 -> 100 while `SetUserDifficultyMsg::Dispatch` stayed at 100 under its corrected VA. The subsidy was not double-booked.

## 6. Deliberately NOT done

- **No permuter** (standing user directive).
- **No re-run of the drained `map_misassignment` class.**
- **No alias installed, no alias group pruned.**
- **No `splits.txt` edits** of any kind.
- **No attempt on `PropSync` / `PropKeys::Copy`** — correctly priced at 0 bytes (§3).
- **The 120 TSV rows whose victim row is absent were not pursued**; an absent victim row cannot
  be adjudicated by this instrument.
- **I did not overturn MPNGAP-1**, and nothing here should be read as reopening that stratum as
  a byte lever.

## 7. Leads for the next lane

- The **`_bijection_arbitrary` sub-100 census (57 rows / 3,728 B)** is the live vein this lane
  worked. Both corrections here came out of it, and both were *transpositions between two
  same-shaped siblings* — that is the signature worth targeting, not size.
- **`0x8277B530` is unnamed** and is `BandUser`'s `Symbol -> TrackType` converter (called only by
  `SetTrackType(Symbol)`). Naming it is a bet that pays in bug exposure, not bytes.
- `PropSync` and `PropKeys::Copy` (§3): real, sized, and worth **0 bytes** — accuracy only.
