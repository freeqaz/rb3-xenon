# Pinning is metric-neutral for REATTRIBUTION, not for RE-HOMING (lane PINHOME-1, 2026-08-14)

**The standing claim "pinning is METRIC-NEUTRAL — Δ exactly 0, pins only
REATTRIBUTE" is CORRECT within its original scope and FALSE outside it.**
Measured here: moving three already-pinned addresses from a unit that *cannot*
define their symbols to one that can is **+3 matched / +428 B**, with the
denominator provably unmoved.

Cite this file before assuming a pin change cannot move the metric.

## The two cases are different, and the difference is mechanical

| case | what changes | measured |
|---|---|---|
| **Reattribution** — pin previously-*unpinned* code | a row moves between units; it was pairable before and after, or unpairable before and after | **Δ exactly 0** (2026-08-06) |
| **Re-homing** — move a pinned row from a unit whose obj cannot define its symbol to one that can | the row gains a base counterpart **for the first time** | **+3 fns / +428 B** (this lane) |

objdiff pairs target↔base **by name**. A target row whose base obj does not
define that name has nothing to pair against and reads **0% regardless of how
correct the name is**. Re-homing changes *which base obj is consulted*, so it
changes pairability — which reattribution never does.

## The measurement

`ab_measure --from-dirty`, both legs freshly re-split (`renamer_patched=1821`),
objdiff-cli sha pinned across legs. Confirmed by an independent `--revert` run
that returned the **exact negation** (−3 / −428, same unit), so this is not a
settling artifact.

```
Δmatched      +3    (44358 -> 44361, entirely default/DataNode 69->72)
Δcode_bytes  +428   (Δcode% +0.004146pp)
Δtotal_code    0    (10,320,664 both legs; total_functions 69,228 both legs)
units at 100%  +0    (none reached, none fell off, on either ruler)
```

★ **Δtotal_code is exactly 0** — the same functions exist, they are merely
attributed elsewhere. So this is a pure `matched_code` gain and *not* an
instance of the "code% moved because the denominator moved" class.

⚠ **Pre-registered +512 B, got +428.** The 84 B gap is one row:
`_M_create_node` reaches **mpn 100** but **fuzzy 99.7619**, so its bytes are
withheld while its function-count credit is granted. Its sole residual is a
**relocation-NAME** charge — the `none`-ruler control reads exactly **+512**,
i.e. all three cross there. This is the documented mpn-vs-fuzzy split
(`matched_functions` counts `mpn`; `matched_code` follows `fuzzy`), not an
anomaly.

## ⛔ Why this was a STALE pin and not metric-fitting

"Re-home it to a unit that can define it" is **circular** if that is the whole
argument — it is fitting a pin to the metric, the MASKED-CLASS FALSE PAIRING
hazard. The non-circular fact:

`0x824f8968` sits in a **scattered template-COMDAT zone** where every adjacent
block belongs to a different unit, and the siblings of the *same instantiation*
obey a house convention **3/3**:

| addr | symbol | pinned to | that obj defines it |
|---|---|---|---|
| `0x824f8270` | `??_G pair<const Symbol,DataNode>` | RockCentral.cpp | ✅ |
| `0x824f9050` | `_M_erase<Symbol,DataNode>` | FlowManager.cpp | ✅ |
| `0x824f91e0` | `clear<Symbol,DataNode>` | AccomplishmentProgress.cpp | ✅ |

⇒ pins in this zone were **derived from the map name**. BandSongMetadata emits
24 distinct `_Rb_tree<Symbol,String>` COMDATs and **zero** `DataNode` ones —
and `<Symbol,String>` is exactly what these three were *called* until lane
CONTAINER-1 renamed them earlier the same day. **The name was corrected and the
pin was not.** Re-homing completes CONTAINER-1's fix rather than inventing a
new assignment.

⚠ **Which TU is retail's true contributor remains UNPROVEN and unprovable by
this instrument.** Eight TUs emit byte-identical copies of all three COMDATs
(CharBonesMeshes, CriticalUserListener, DataNode, DataPointMgr, FlowManager,
GemManager, RockCentral, ViewSetting) and are metrically indistinguishable.
`DataNode.cpp` was chosen because it is the value type's own TU and already
hosts the *other* `insert_unique` overload of this instantiation
(`0x8274b638`, fuzzy 100). **Recorded as a choice, not a proof.**

## The census: the vein is real but SMALL — 2,972 B, not 38 kB

`tools/orphan_pin_census.py` generalises the defect: target rows with a real
mangled name whose paired base obj does not define it. Over 1,045 units with
both objs, 22,973 named rows:

| class | rows | bytes |
|---|---|---|
| named + paired | 22,685 | — |
| **ORPHAN PINS** | **267** | **38,096 B** (0.37% of `total_code`) |
| ⤷ **re-homable** (another built obj defines the exact name) | **59** | **2,972 B** |
| ⤷ not re-homable (no TU we compile emits it) | 208 | 35,124 B |

⛔ **Only the re-homable slice is a pinning problem at all.** The other 208 rows
(35,124 B — 92% of the orphan bytes) have *no* TU in our tree emitting that
name; that is absent source or a wrong map name, and **moving the pin cannot
help them.** Reporting 38 kB as the lever would be ~13× optimistic.

⚠ And the 2,972 B is fragmented across **44 units**, median row **76 B**,
largest **164 B**. The 76 B rows are overwhelmingly `??_G` scalar-deleting
destructors and `ObjPtr`/`ObjPtrList` thunks whose "provider" is just another
TU instantiating the same template — so *which* unit hosts them is arbitrary in
exactly the way flagged above. **This is a candidate queue needing per-row
adjudication, not a batch-apply.**

## ★ The self-validation fired, and it mattered

Every flagged row must read `fuzzy 0.0`; one did not
(`?Dispatch@SyncLocalMachineMsg@?A0x6c4eb79b@@`, **100.0**). Cause: the
`anon_ns` post-compile patcher rewrites anonymous-namespace hashes, so for
`?A0x*` symbols **name-absence is not evidence of unpairability**. 21 such rows
(2,828 B) excluded; validation then passes 0/267. Without the check the census
would have shipped a **7.9%-contaminated** population — and a contaminated
"structurally unpairable" list is the kind of confident negative that closes
veins.

## Not done

* The 8-way ambiguity over the true contributing TU is **not** resolved.
* `_M_create_node`'s remaining 84 B (one relocation-name charge) is **not**
  chased — it is the `diff_arg` stratum that MPNGAP-1 measured ~91% irreducible.
* The 59 re-homable rows are **not** applied.
* `src/` did not move, so `tools/native_build_gate.sh` was correctly not run.
