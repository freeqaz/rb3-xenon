# Lane BP-6 — map fragment justification (24 entries)

Fragment: `~/tmp/bp6_map_fragment.json` (flat `{addr: name}`, 24 entries).
Also committed for durability at
`docs/plans/lane-bp6-multi-content-fragment-2026-07-29.json` on branch `laneBP6`.

Source: `scripts/harvest/multi_content_disambiguate.py` re-run (the sanctioned
refill of lane G's channel, `docs/plans/laneG-multi-content-join-2026-07-24.md`)
over a full-tree `homing_scan_all.sh` sweep of **all 1,094 objs** in
`/home/free/tmp/wt-bp6` at the 40,870 tree state.

## Acceptance rule actually applied

1. **Map-free evidence only (`--no-sym`).** Accepted classes are `str` / `vfstr`
   / `f32` / `f64`: our obj's relocation at a masked offset points at a
   `??_C@` string COMDAT (or `__real@`), and the retail VA's decoded operand
   resolves to byte-identically the same string/constant. No `target_symbol_map`
   lookup participates, so the map cannot contaminate its own evidence.
2. **The tool's honesty clause** (unchanged from lane G): accept `c` iff `c` has
   ≥1 AGREE and 0 CONFLICT, **and** every rival `c'` is *positively excluded at
   an offset where `c` positively agreed*. `TIE`, `NO-WINNER`, `NOT-EXCLUDED`
   are all rejected rather than ranked.
3. **Phantom-class veto** — any proposal whose mangled class is one of the 13
   classes proven absent from band.exe
   (`docs/plans/lane-bp4-map-contradiction-adjudication-2026-07-29.md` §6:
   BandStoreUIPanel BaseMaterial ClipCollide DancerSequence FitnessFilterObj
   FlowCommand FlowIf FlowNode FlowOnStop MetaMaterial RndSpline SkeletonClip
   WiiFriendsScreen) is rejected by construction. **0 proposals tripped it.**
4. **No repoints.** A proposal whose name already exists in the map at a
   different VA is excluded — it is a permutation problem for the map owner, and
   a partial repair can strand matches.

### Why `sym` evidence was rejected outright (refutation of lane G's advice)

Lane G's negative-results table says "always `--trust-file`" for the `sym`
class. Measured held-out precision **at this tree state** says that is no longer
sufficient — turn `sym` off entirely:

| leg | RESOLVED-STRONG | RESOLVED-SYM | demonstrated errors |
|---|---|---|---|
| `--trust-file` (lane G's recipe) | 2697/2759 = **97.75%** | 779/1083 = **71.93%** | **7** (`MISS/TRUTH-AGREE`) |
| `--no-sym` (this lane) | 2327/2382 = **97.69%** | — | **0** (all 55 misses are `MISS/TRUTH-CONFLICT`) |

`MISS/TRUTH-AGREE` = the map's own label for that function *is* content-
corroborated and the resolver still picked a different VA — a demonstrated
resolver error, which lane G measured as exactly zero. Mechanism: enabling `sym`
adds extra evidence slots, which can put a CONFLICT on the true candidate and
change which rival gets excluded. So a 72%-precision class does not merely add
its own errors, it **corrupts the otherwise-clean strong class**. The trust set
grew 2,191 → 3,359 as intervening lanes repaired the map, so `sym` now fires
4.4× more often (247 → 1,083 decisions) and its errors dominate.

## Rejection count under that rule

Over the 31,943 `MULTI` + `UNIQUE-ICF` records:

| verdict | count | disposition |
|---|---|---|
| `ALREADY-HOMED` | 17,927 | our name is already on a hit — nothing to do |
| `NO-EVIDENCE` | 12,455 | references no string/constant at any masked slot — structural floor |
| `NO-WINNER` | 729 | every candidate has a content CONFLICT |
| `TIE` | 679 | ≥2 candidates satisfy the content — refused by rule |
| `DROP-CONTESTED` | 14 VAs | one retail VA claimed by ≥2 mangled names (genuine ICF fold) |
| `RESOLVED-STRONG` | 153 occurrences | accepted |

**REJECTED by the acceptance rule: 1,422** (729 `NO-WINNER` + 679 `TIE` + 14
contested VAs), plus **1 repoint** excluded by rule 4 and **9,289** `sym`-class
decisions declined wholesale by rule 1 (the 1,083 validated sym decisions scale
to the full pool). The 153 accepted occurrences collapse to **25 distinct
(name, VA) pairs** — the rest are the same COMDAT emitted into many TUs
(`StaticClassName@RndEnviron` alone recurs in 17 objs). 25 − 1 repoint = **24
applied**.

## Second, independent corroboration for the high-multiplicity entries

The mission asked for ≥2 independent corroborations. For the `StaticClassName`
family the second channel is source: `OBJ_CLASSNAME(X)` fixes the Milo type name,
which is *not* the C++ class name — the `Rnd`/`Dx` prefix is dropped. The
content join independently recovered exactly that string:

| entry | VA | retail VA references | source declares | file |
|---|---|---|---|---|
| `?StaticClassName@RndEnviron@@` | `0x82739080` | `"Environ"` | `OBJ_CLASSNAME(Environ)` | `src/system/rndobj/Env.h:38` |
| `?StaticClassName@RndFur@@` | `0x8240dd38` | `"Fur"` | `OBJ_CLASSNAME(Fur)` | `src/system/rndobj/Fur.h:14` |
| `?StaticClassName@RndParticleSys@@` | `0x8240ddc0` | `"ParticleSys"` | `OBJ_CLASSNAME(ParticleSys)` | `src/system/rndobj/Part.h:157` |
| `?StaticClassName@DxLight@@` | `0x8240dcb8` | `"Light"` | `OBJ_CLASSNAME(Light)` | `src/system/rnddx9/Lit.h:10` |

A prefix-stripped name is a strong, map-free coincidence filter: nothing about
the byte compare or the string join knows about `OBJ_CLASSNAME`.

## Known caveat carried, not hidden

`0x8264bce8 → ?StaticClassName@MiniLeaderboardDisplay@@` is a **dupname**:
`class MiniLeaderboardDisplay` is defined twice in our tree
(`src/system/bandobj/MiniLeaderboardDisplay.h:14` and
`src/system/hamobj/MiniLeaderboardDisplay.h:9`), so both TUs emit the identical
mangled COMDAT and the string evidence cannot discriminate between them. It is
accepted only because it is **reveal-only** — the VA already lies inside
`NextSongPanel.cpp`'s existing `.text` range, so no pin attribution depends on
it, and the failure mode is "no credit", not a mispair. It did **not** flip.

## Collision check vs MAIN's map at report time

Main's `scripts/target_symbol_map.json` last changed in `fb83d49d` (lane BP-5);
BP-7's map edits had not landed when this was checked. Result:

- VAs already present in main's map: **NONE**
- Names already present in main's map: **NONE**

All 24 are pure additions. Re-run the check before applying if BP-7 lands first;
the one name to watch is the excluded repoint below.

## Handed to the map owner, NOT applied

`?SetType@RndScreenMask@@UAAXVSymbol@@@Z` — main's map has it at `0x82481ad8`,
but that VA is not even a reloc-masked byte match for our compiled body, whereas
`0x826059e8` is *and* is content-corroborated (`"types"` at the confirmed slot).
Evidence favours repointing to `0x826059e8`. Excluded here by rule 4;
`homing_gen4.py` independently declined it (`drop_name_in_map: 1`).
