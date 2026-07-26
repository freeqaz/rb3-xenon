# Lane AB — symbol-map repair round 2

**2026-07-26.** Worktree `~/tmp/wt-laneAB-map2`, branch `laneAB-map2`, base
`b4c09028` (**29,666** strict). Single-owner round for
`scripts/target_symbol_map.json`.

**Result: 29,666 → 29,784 (+118 gained, 0 lost, 0 regressions), and the
argument-register-proven mispair pool went 36 → 0.**

An independent Opus verifier, working from its own cold baseline in its own
worktree, reproduced the headline exactly (29,666 → 29,743 at `07111129`,
GAINED 77, **LOST set empty**) and agreed with `report.json`'s own
`matched_functions` scalar at every leg.

Strict is `report.json match_percent_normalized == 100.0` only, counted
unit-agnostically as the set of `(unit, function)` pairs, re-measured after
every wave with `rm -f build/45410914/report.cache` first.

---

## 1. The finding that made the round: a gap between two tools

Both existing repairers reported **FIXPOINT** at the base state.
`map_repoint_round.py` planned 0 moves; `map_rotation_repair.py plan` planned 0.
The round nearly ended there.

It shouldn't have. `map_repoint_round.py` opens with

```python
for name, hits in hits_of.items():
    cur = name2va.get(name)
    if not cur or (cur & hits):
        continue
```

so it can only ever **move a name already in the map**. A symbol our build
emits that is byte-identical to exactly one place in the whole 11.8 MB binary,
but which is *not in the map at all*, is skipped on the first clause. And if
that one place is already occupied, `homing_gen`'s insert path refuses it too —
`tu5_map_apply_fragment.py` asserts on address collisions, by design.

**Those functions fall between the two tools and stay dark indefinitely.**
`homing_scan.py` already labels the population: `cls == 'ALL-MAPPED'`, 37,293
records. Almost all are simply correct (holder == name). The residue is the
lever:

> holder H sits at VA, claimant C is byte-identical at VA, **H is not**.

H cannot be the function at VA — if it were, objdiff would already read 100.
C provably is. The swap is a guaranteed flip, not a gamble.

Productionised as **`scripts/harvest/map_displace_round.py`**.

### 1.1 The case that exposed it

`default/DateTime` mapped `?ToString@DateTime@@QBAXAAVString@@@Z` at **two** VAs:
`0x82522b58` (79.0%) and `0x82523178` (0.0%). Three independent signals agreed:

| signal | says |
|---|---|
| homing byte identity | ToString's bytes are at `0x82523178` **only** |
| `argreg_mispair_scan.py` | the body at `0x82522b58` reads r5; `ToString(String&)` is (this, str) — no r5 |
| reading the target | `0x82523178` is `ToDateString(str); str += MakeString(" %02d:%02d:%02d", ...)`, exactly our source |

And `??$MakeString@HE@@YAPBDPBDHE@Z` — *not in the map at all* — is
byte-identical at `0x82522b58` and nowhere else. Same story one function later:
`?GetTimeZoneBias@@YAXAAJ@Z` at `0x82522bb8` is really `??$MakeString@E@@`.

Neither existing tool could express that repair.

---

## 2. Waves, each A/B'd whole-binary

| wave | what | Δ | lost |
|---|---|--:|--:|
| 1 | displace wrong holders, PAYS destinations | **+51** | 0 |
| 2 | `--include-free`: unique claimants on unoccupied VAs, PAYS | **+26** | 0 |
| 3 | 155 evidence-backed assertions on non-paying spans | +0 | 0 |
| 4 | evict 32 argreg-proven mispairs | **+1** | 0 |
| 5 | evict the last 2 `ObjRefConcrete<T>::SetObj` family entries | **+1** | 0 |
| 6 | spatially break ICF ties, iterated to fixpoint | **+13** | 0 |
| 7 | resolve 17 duplicate names by byte identity | +0 | 0 |
| 8 | name 25 unoccupied ICF-folded VAs — **isolated, reviewable** | **+25** | 0 |
| 9 | splits: merge the TexProc hole back into Spotlight | **+1** | 0 |
| | **total** | **+118** | **0** |

### 2.1 ★ The two guards were both *measured*, not assumed

The first attempt at wave 1 emitted 65 displacements and scored **+51 (65 gained
/ 14 lost)**. Every one of the 14 losses was the same shape: our build emits
**one function under two spellings**, so the "unique" claimant was an alias of
the sitting holder and the displacement just swapped which spelling wins.

- anonymous-namespace hashes (`?A0x0884b0cf` vs `?A0xb0de99ba`) — MSVC derives
  these from machine name + source path, and the `anon_ns` obj patcher
  normalises them, so two spellings denote one function;
- a `Ham*`/`Band*` class rename (`?Store@Target@HamCamShot@@` vs
  `…@BandCamShot@@`).

**Guard 1 — a holder already reading strict-100 is the right name for that VA,
whatever the byte scan thinks.** But the *name-only* form of that guard
over-refuses: a name mapped at two VAs can be reading 100 at the other one.
Restricting it to "…**and this VA is the holder's sole home**" recovers exactly
2 clean gains (measured: the guarded set went 49 → 51 and then reproduced the
empirically-clean 51 exactly).

**Guard 2 — ICF.** If the holder is *also* byte-identical at the VA, two source
functions compiled to the same machine code and the linker merged them; a
VA→name map cannot express both and byte identity cannot rank them. Refuse,
assert neither. (This is the previously-measured −23/+0 rule, kept.)

### 2.2 ★ ICF ties CAN be broken — spatially

Standing doctrine was to assert neither twin, which left **324** destinations
dark. But **retail is not LTCG-built, so `.text` preserves per-TU grouping**.
Feeding every tied claimant through `span_predictor.py` and keeping the tie only
when **exactly one** of them is defined by the unit whose pinned span owns the
VA is a *positive spatial fact*, not a coin flip — the same discriminator that
measured +21/−0 as `map_repoint_round.py`'s discriminator 2.

57 ties broken on the first pass → **+13**, 0 lost, fixpoint on the next.

### 2.2b Duplicate names are resolvable the same way

A name mapped at two VAs is right at one and wrong at the other. When the
homing scan places that name's bytes at a strict subset of its mapped VAs, the
rest are provably not it. 20 of 92 duplicate names resolved this way (18 VAs
evicted, 92 -> 75); the remainder either have no byte evidence at all or are
ICF-folded at every mapped VA and so genuinely inexpressible. Score unchanged --
this is hygiene, and it is exactly the shape that turned up the `DateTime`
`?ToString@` duplicate that started the round.

### 2.3 Unmapped really does beat wrongly-mapped

The two eviction waves are the ones that *should* have scored 0 — every evicted
entry was sub-100, so it contributed nothing. They scored **+1 each**. Removing
wrong entries has now gained matches on three separate occasions across lanes.

---

## 3. What the argument-register test proved

`scripts/harvest/argreg_mispair_scan.py` (no build, ~3 s, 0 false positives in
12,183 strict-100 controls): **if the target body reads an argument register the
mapped signature does not declare, the pairing is wrong.** It flagged 36 forward
mispairs at base.

| disposition | n |
|---|--:|
| **repaired** — the displacement waves found the true owner | **4** |
| **evicted** — proven wrong, no destination any method can assert | **32** |
| hand-proven family members the scanner punts (by-value aggregates / over-carve) | +2 evicted |

**Forward pool after the round: 0.**

### 3.1 The `ObjRefConcrete<T>::SetObj` family (10 entries, all closed)

All ten targets read **r5**. `SetObj(Object*)` is `(this=r3, root=r4)` and has no
r5; the bodies are `Replace(from, to)`. Decisively, **our build emits zero
`?Replace@?$ObjRefConcrete@…` symbols**, so the real bodies at those VAs are
unclaimable by any name we compile — the repair is a *source* change (give
`ObjRefConcrete<T>` its own `Replace`), not a map change. Handed off.

That family had been published as `at_limit` — "a return-value
register-retention artifact" — and a sibling `??$__uninitialized_copy@` mispair
had nearly funded a fleet-wide `_STLP_DONT_USE_EXCEPTIONS` change across ~281
STLport TUs. **Leaving proven-wrong entries mapped is what keeps costing other
lanes real work**, which is the argument for evicting even at zero measured
gain.

### 3.2 ★ FRAGMENT is a *carving* defect, not a map defect

22 listings read `r0`/`r11`/`r12`/`f9`–`f13` undefined at entry, or have
instruction VAs that disagree with their own `.fn` label:

```
.fn fn_8228E5A0, global
/* 8228E484 00283284  3B EC FF 90 */  subi r31, r12, 0x70   <-- EH-funclet prologue
```

That is the known jeff/dtk mis-nest (`project_jeff_asm_misnest`), and
`subi r31, r12, N` is a funclet prologue, not a function entry. **Never condemn
a map entry off such a listing** — it also explains an independent report that
"the `??1Synth` body is a constructor", which was the same artifact. These
belong to the jeff/dtk lane.

---

## 4. Negative results worth not re-deriving

- **`identical_pct_cluster_scan.py --mispair-check` ∩ argreg forward = 0**, and
  it is structural, not a bug: every one of the 36 forward entries scores
  penalty ≥ 60 (`STRUCTURAL`), never `ARG-ONLY`. The forward test catches gross
  mispairs with large structural diffs; ARG-ONLY clustering by construction only
  catches small immediate/register-only diffs. **The two detectors are
  near-disjoint by design — do not expect overlap.** Measured at the current
  state: 34–55 ARG-ONLY clusters / **276** functions (the earlier "27 / 269" no
  longer reproduces; other lanes landed in between).
- **The trust audit's fixed string reader**: 296 contradicted (broken reader)
  → **79** (fixed reader, already in-repo at `multi_content_disambiguate.py`,
  `ca7b9803`). 217 of the originals were false positives; **0** entries are
  newly condemned by the fix. That 79-entry population is **disjoint** from the
  byte-identity population — the trust audit only inspects names that are
  already byte-identity-correct where they sit.
- **The prior round's "269 byte-contradicted (131/71/43)" does not reconcile** —
  131+71+43 = 245, not 269. Its ICF figure of 43 is exactly the *single-hit*
  discriminator's count; the multi-hit/spatial path finds 27 more.

---

## 5. After-state, and what is structurally unreachable

| | before | after |
|---|--:|--:|
| strict matches | 29,666 | **29,784** |
| argreg forward (PROVEN mispairs) | 36 | **0** |
| byte-identity contradicted entries | 245 | **191** |
| trust-audit contradicted (fixed reader) | 79 | 79 |
| duplicate names | 99 | **75** |
| duplicate VAs | 0 | **0** |
| entries | 21,712 | 21,737 |

The residue is a floor, not a backlog:

| | n | why unreachable |
|---|--:|---|
| tied ICF folds with no spatial separator | 275 | two source functions, one VA, and the pinned span does not separate them. 25 of these were unoccupied AND paying, and wave 8 names one twin each — see the commit, which is isolated and reversible, and `laneAB-icf-tie-alternates-2026-07-26.json` for every alternate |
| destinations whose holder already reads 100 | 317 | correct as they stand |
| `T3_NO_RECORD` holders off any paying span | 30 | claimant evidence only; the one failure mode (an ICF twin) is exactly what a missing holder record makes uncheckable |
| FRAGMENT listings | 22 | jeff/dtk over-carve — a carving defect |
| contradicted with no paying span / several paying | 191 | repointing would be more correct but cannot pay until the owning span is pinned |

Both `map_repoint_round.py` and `map_displace_round.py` report FIXPOINT at the
after-state.

### 5.1 ★ Handoff: 475 sub-1KB splits holes — the rename is necessary, not sufficient

The verifier found the lane's own repoint of `0x824dbeb8` to `??_GSpotlight`
still reading **0.0%** after the map was right: `splits.txt` carved
`0x824DBEB8..0x824DBF08` out of the *middle* of `Spotlight.cpp`'s span and gave
it to `TexProc.cpp`, so objdiff could never pair it — our `??_GSpotlight` lives
in `Spotlight.obj`. Merging the hole back = **+1**.

Scanning `splits.txt` for the same shape: **475 sub-1KB gaps sit inside exactly
one other unit's `.text` range.** They are NOT all defects — retail genuinely
scatters COMDATs across TUs, which is the whole `comdat_scatter` vein. The
decisive test per hole is cheap and needs no build:

> does the unit the hole is assigned to actually **define** the symbols in it?
> If not, and the enclosing unit does, merge the hole back.

Left for the splits owner; this lane touched only the one case the verifier had
already measured.

---

## 6. Reusable procedure

```bash
scripts/harvest/homing_scan_all.sh $PWD ~/tmp/homing 16 20      # ~2 min
python3 scripts/harvest/argreg_mispair_scan.py --json ~/tmp/argreg.json
# capture the CURRENT strict-100 set as the guard input, then to fixpoint:
python3 scripts/harvest/map_displace_round.py --worktree $PWD \
    --results ~/tmp/homing/merged.json --out ~/tmp/plan.json \
    --pays-only --include-free --break-ties --strict-guard ~/tmp/cur_rep.json
python3 scripts/harvest/map_rotation_repair.py apply --plan ~/tmp/plan.json \
    --map scripts/target_symbol_map.json
touch config/45410914/config.yml && rm -f build/45410914/report.cache
./tools/ninja-locked
```

Re-run after **every** body-port wave: new compiled bodies create new unique
byte-identity claimants, and evictions free destinations for the next pass.
