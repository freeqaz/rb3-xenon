# Lane H — cycle-aware repair of the mispaired `target_symbol_map` entries (2026-07-25)

**Result: 52 mispaired map entries repaired atomically at a perfectly flat
26,545 strict; the repaired map then re-opened lane G's drained content-join
vein for +24. Final 26,569 strict, 0 unexplained losses.** Branch `laneH-mapfix`,
from `de23426b`.

Follow-up to `docs/plans/laneG-multi-content-join-2026-07-24.md`, which
discovered that **423 already-mapped names sit on a retail VA whose content
contradicts them** and left the repair for a later lane because it is a
permutation, not a lookup.

## 1. The audit reproduces exactly

`multi_content_disambiguate.py --trust-audit` re-run on a fresh worktree:

```
10,660 names checked -> 2,192 corroborated, 423 CONTRADICTED, 0 no content
```

Diffed against the committed
`docs/plans/laneG-map-trust-contradicted-2026-07-24.json`: **identical name set,
identical VAs, zero disagreements.**

Six pairs were then verified independently, with a throwaway PPC decoder that
does not share code with the resolver (walks `addis`/`addi`/`ori` pairs in
`band.exe` and prints the C string at the formed address):

| symbol | mapped VA references | content-determined VA references |
|---|---|---|
| `?StaticClassName@Sfx@@` | `0x826fc3e8` → `"FxSendCompress"` | `0x826b1c88` → `"Sfx"` |
| `?StaticClassName@Object@Hmx@@` | `0x8227ae48` → `"TrackPanelDir"` | `0x82271a90` → `"Object"` |
| `?StaticClassName@CharHair@@` | `0x8236a3a8` → `"CharIKFoot"` | `0x8236a628` → `"CharHair"` |
| `?StaticClassName@CamShot@@` | `0x82742b90` → `"TexMovie"` | `0x824bd6c0` → `"CamShot"` |
| `?StaticClassName@UIScreen@@` | `0x826fc568` → `"FxSendMeterEffect"` | `0x827f1f20` → `"UIScreen"` |
| `?StaticClassName@FileMerger@@` | `0x8236aaa8` → `"CharDriverMidi"` | `0x8236afa8` → `"FileMerger"` |

`X::StaticClassName` is `static Symbol s("X")`; it *must* reference the literal
`"X"`. Unambiguous in all six.

## 2. New tool — `scripts/harvest/map_rotation_repair.py`

Three subcommands.

* **`analyze`** — for every function we compile that the homing scan gave
  byte-identical retail hits for, run the **map-free** content resolver
  (`--no-sym` equivalent) over *all* hits, never just the currently-mapped one.
  `sym`-class evidence is deliberately excluded: it inherits the very mispairs
  being repaired.
  Result: 19,202 names with hits → 2,478 content-resolved → **2,166 agree with
  the map, 312 do not** (115 of those mapped at exactly one VA).
  This is a strict superset of lane G's published 95: all 94 determinable ones
  produced **the identical correct VA**, and the 95th
  (`?NodeCmp@@YAHPBX0@Z`) is a self-entry (`cur == correct`).

* **`plan`** — turns `desired` into a conflict-free rewrite (`set` VA→name,
  `remove` vacated VA), decomposed into closed cycles and chains. Two rules
  earn their keep:
  * a destination held by a name with **no proven home of its own** blocks the
    move (that holder would have to be silently dropped), and dropping a blocked
    mover can block another that was relying on it to vacate — so blocking is
    iterated **to a fixpoint**. Getting this wrong (first cut did) silently
    scheduled 10 evictions the plan claimed it wasn't making;
  * `vacated` is computed *after* the fixpoint, so a mover that got dropped
    does not have its current VA removed.

* **`apply`** — line-oriented textual rewrite. Never `json.dump`: the map is a
  21.6k-line 1-space-indent file whose formatting is a project invariant.
  Untouched lines stay byte-identical. All key comparisons are
  case-insensitive (there are legacy uppercase `"0X…"` keys, plus 4 keys that
  differ only in case — both pre-existing). Asserts the post-state introduces no
  new name duplication and that the file still parses.

## 3. Wave 1 — the atomic repair

52 movers: **one closed 6-cycle** — the `Keyboard*Msg::Type` family
(`ExpressionPedal → ConnectedAccessories → LowHandPlacement → HighHandPlacement
→ KeyReleased → Mod →` back) — plus **36 chains**. Written in one pass:
52 `set`, 33 `remove`.

```
TOTAL 26,545   NEW 13   LOST 13      <- exactly flat
```

The 13 LOST and 13 NEW are the repair itself, e.g.
`?StaticClassName@DeJitterPanel@@` LOST from `default/Meta` and NEW in
`default/MoviePanel`: its true home falls in a different unit's pinned span, so
the pairing moved units.

**Correction to lane G's neutrality claim.** Lane G argued the repair cannot
change the score because "our function byte-matches whichever twin it's pointed
at". That holds only when the *true* twin is also pinned **and inside the span
of the unit that compiles the symbol**. For 13 of the 52 it is not — the true
home is either in no pinned span at all (`Character::StaticClassName` →
`0x8236d348`) or attributed to a different TU's span
(`AnimTask::StaticClassName` → `0x82400620`, inside `Tex.cpp`'s range). Those 13
"matches" were **false**, and repairing them costs them. They were exactly
offset by 13 functions reaching their true home, so the wave measured flat — but
the flatness is arithmetic, not structural. A span-membership predictor
(`splits.txt` ranges × `report.json`) called this ahead of the build and is the
right pre-check for any future repair wave.

Trust audit after wave 1:

```
10,674 names checked -> 2,244 corroborated, 385 CONTRADICTED   (was 423)
```

The vein is **drained without eviction**: re-running `analyze`+`plan` after each
wave yields 0 further moves.

## 4. Did the callee-evidence unlock materialise? Yes — measured

Held-out `--validate` A/B, *same code, same scan results, only the map differs*
(pre-repair map kept at `~/tmp/laneH/map_pre_wave.json`, fed via `--tmap`):

| evidence class | pre-repair | post-repair |
|---|---|---|
| `RESOLVED-STRONG` (map-free `str`/`f32`) | 1,139 / 1,915 = **59.48%** | 1,287 / 1,927 = **66.79%** |
| `RESOLVED-SYM` (trust-gated callee) | 469 / 481 = **97.51%** | 470 / 502 = **93.63%** |
| combined | 1,608 / 2,396 = **67.11%** | 1,757 / 2,429 = **72.33%** |
| combined, ungated `sym` | 4,189 / 5,885 = **71.18%** | 4,319 / 5,898 = **73.23%** |

Read these carefully — the interesting movement is in the *denominator's
honesty*, not the tool:

* every `RESOLVED-STRONG` "miss" is `MISS/TRUTH-CONFLICT`, i.e. the ground truth
  itself is content-contradicted. Those fell **776 → 640**: the benchmark got
  136 entries less wrong. That is the repair showing up as measured precision.
* every `RESOLVED-SYM` "miss" is `MISS/NO-EVIDENCE` — the map's VA references
  nothing checkable, so the miss is *unadjudicable*, not a demonstrated tool
  error. The apparent 97.5% → 93.6% dip is 21 extra decisions of that
  unadjudicable kind, not 20 new errors. Volume is the real change.

## 5. Stretch — the repaired map re-opened lane G's drained vein

Lane G's wave C was dry, so the MULTI content-join was declared drained at the
current obj set. With the repair applied it is not: freed VAs and a cleaner
trusted callee set produce fresh resolutions.

| iter | proposals | map entries | splits ranges | strict | Δ |
|---|---|---|---|---|---|
| wave 1 (repair) | — | 52 set / 33 removed | — | 26,545 | **0** |
| stretch 1 | 125 | 34 | 11 | 26,566 | **+21** |
| stretch 2 | 28 | 3 | 2 | 26,569 | **+3** |
| stretch 3 | 15 | 0 | 0 | — | dry |

**Final 26,569 strict, +24 over the 26,545 baseline, 0 unexplained losses** (the
only LOST-vs-baseline entries are wave 1's 13, already accounted for above).

Note this is *not* the 18,081 no-evidence pool cracking open — that pool is
essentially unchanged (17,873) and remains structurally unresolvable by
content. The +24 came from the repair freeing correct homes and un-poisoning
callee evidence, exactly the mechanism lane G predicted, at a modest size.
**Re-run the repair + content join after every body-port / source-wiring wave**;
they feed each other.

## 6. Residual 385 contradicted entries — classification

| class | count | matched today | what it is |
|---|---|---|---|
| `EVICTION-ONLY` | 56 | 14 | content determines the correct home, but that VA is held by a name with **no proven home of its own**. Repair requires evicting 34 such holders |
| `NO-DETERMINED-HOME` | 329 | 278 | the current pairing is contradicted, but **no candidate in the hit set is content-confirmed** as the true home. Usually the true home is not in the hit set at all — our source diverges, or the function references nothing decodable |

Per-entry detail: `docs/plans/laneH-residual-contradicted-2026-07-25.json`.

### The deferred eviction wave (NOT applied)

`docs/plans/laneH-eviction-plan-2026-07-25.json` is a complete, ready-to-apply
plan: **63 moves, 34 removals, 34 evicted holders**. Apply with

```bash
python3 scripts/harvest/map_rotation_repair.py apply \
  --plan docs/plans/laneH-eviction-plan-2026-07-25.json \
  --map scripts/target_symbol_map.json
touch config/45410914/config.yml && rm -f build/45410914/report.cache && ./tools/ninja-locked
```

It is **deferred deliberately**: the span predictor puts it at **GAIN 5 / LOSS
27 = net −22**. Every one of those 27 losses is a *false* pairing (e.g.
`?StaticClassName@HamMove@@` — a Dance Central class RB3 does not have — sitting
on `0x82271a90`, which references `"Object"`), so the wave trades 22 headline
matches for 56 honest map entries. That is a real integrity gain at a real score
cost and is a coordinator call, not a worker call. If the 34 evicted holders'
true homes are ever pinned, the cost drops toward zero.

## 7. Traps hit / worth knowing

* **Fixpoint, not one pass.** Blocking cascades. A single-pass blocked check
  reported "0 evicted" while quietly scheduling 10 evictions.
* **`vacated` after the fixpoint.** Computing it before means dropped movers
  still get their current VA removed — silent match loss.
* **Span membership is the real predictor of a repair's score impact**, not
  byte identity. `splits.txt` `.text` ranges × the compiling unit from
  `report.json`. Byte identity is guaranteed by the hit set; *pairing* is not.
* **`analyze` must be map-free.** Using `sym` evidence to repair the map is
  circular — a bad entry makes the true candidate CONFLICT so a decoy wins.
* The map's `_denylist` key holds a **list**, and `_comment`-style keys exist —
  filter to `isinstance(v, str)` and `k.startswith('0x')` before any name/VA
  bookkeeping.
* 117 names are legitimately mapped at more than one VA; `plan` refuses to move
  them (`len(name2va[name]) != 1`) rather than guess.
