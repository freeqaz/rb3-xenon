# Lane AI — joint owner of `target_symbol_map.json` + `splits.txt`, round 2

**Date:** 2026-07-26 · **Branch:** `laneAI-joint2` · **Baseline:** 30,244 strict
· **Result:** **30,278 strict (+34, 0 losses)** · **8 commits**

Single-owner round clearing the backlog four lanes deferred, plus a full
alternating map↔splits fixpoint loop.

---

## 1. ★ Verdict on the "free / near-free" claims — 3 of 4 were real, all needed correction

The calibration question was: how should we read a handoff that says "guaranteed
free flip"? Answer: **the *location* claims held up, the *name* claims did not.**
Every one of the three that paid needed its proposed mangled name corrected
first; the one that failed was a misdiagnosis of the mechanism.

| claim | verdict | what actually happened |
|---|---|---|
| `0x82813190` is `UIPanel::FinishLoad`, "guaranteed free flip" | **free, after a fix** | The handoff proposed `?FinishLoad@UIPanel@@`**`U`**`AAXXZ`. We emit **`M`** (protected), not `U` (public virtual). Mapped as proposed it would have read 0% and looked like a refutation. Corrected → **+1** |
| wrong-template-argument family, "2 are rename-only = free +2" | **free, exactly +2** | Both `deque<VocalTrack::RangeShift>` and `vector<deque<TubePlate*> >` flipped. The reconstructed mangled name in the handoff was subtly wrong (`@2@@stlpmtx_std@@` vs `@stlpmtx_std@@@stlpmtx_std@@`) — always take the name from the obj's COFF symbol table, never retype it |
| `??8Head@BandCharDesc` is a 4-byte target-carve truncation, "flips for free" | **REFUTED** | Not a carve bug. Retail's `operator==` **falls through into the adjacent 4-byte `blr` stub** at `0x82333E14`, which is a separate, correctly-mapped function (`??3@YAXPAX0@Z`, placement `operator delete`, whose entire body is that one `blr`). Extending the carve would swallow another function's only byte. This is a linker epilogue-share artifact ⇒ **at_limit**. A whole-file sweep found **0 end-truncations and 0 start-truncations across all 5,521 pinned `.text` ranges** — the class does not currently exist |
| `0X`-uppercase lint, "2-for-2 on defects" | **generalises, 10 of 18** | See §3 |

★ **How to read such claims in future:** treat the *address* as evidence and the
*mangled name* as a hypothesis. A handoff that decoded retail bytes at a VA is
usually right about what lives there and usually wrong about how to spell it.

★ **Correction:** the map has **11** `0X`-uppercase keys (18 not fully
lowercase), not the **264** quoted in `map_displace_round.py`'s docstring. That
figure is stale and was propagated into the lane brief.

---

## 2. ★★ The biggest finding: a default-off flag was hiding 15 free matches

`map_displace_round.py` classifies a byte-unique claimant whose VA is
**unoccupied** as

```
'free-va(homing_gen territory)': 15
```

and drops it, on the theory that `homing_gen.py` will pick it up. It does not —
`homing_gen` runs off a different pool. Those 15 sat in that counter across at
least two lanes. They are unmapped names whose reloc-masked bytes are **unique
in the whole 11.8 MB binary**: proof of location, no holder to displace,
nothing to risk.

```
--include-free    -> 15 candidates
span_predictor    -> 9 PAYS, 6 UNPINNED
measured          -> 15 of 15 flipped to strict-100, 0 losses
```

**But only jointly.** 9 were a map-only insert. The other 6 were the entire tail
of `band3/meta_band/InterstitialMgr`, whose VAs fell in two holes of that unit's
pinned ranges — map-only they read 0% forever.

| | flipped |
|---|---|
| map alone | 9 of 15 |
| splits alone | 0 of 15 |
| **joint** | **15 of 15** |

★ **`--include-free` should be the default**, or at minimum part of every
map-owner runbook. An unlogged-but-counted discard is how a seam hides — the
same failure mode as `map_repoint_round.py`'s silent discriminator-2 drop.

---

## 3. ★ New seam shape #3, and a new scanner for it

`?FocusComponent@UIPanel@@`**`U`**`AAPAVUIComponent@@XZ` @ `0x828027c0` needed
**both** halves at once, and is invisible to every existing tool *because* of it:

* **map half** — the name says `U` (public virtual); we emit `Q` (public).
* **splits half** — the VA was pinned to `UI.cpp`; only `UIPanel.obj` defines it.

Rename alone reads 0% (wrong unit). Move alone reads 0% (wrong name). Together:
**0% → 99.8%**.

★ **Why no tool saw it:** `span_predictor.py` and `joint_unblock.py` both start
from `{name → units that define it}`. A row whose name is emitted by **no** obj
is dropped *before* classification, so it never reaches the
PAYS / WRONG-UNIT / UNPINNED buckets at all.

### New tool: `scripts/harvest/access_specifier_scan.py`

Flags map rows whose name no obj defines but which differ from an emitted name
by exactly one character — the access-and-storage code after the scope's closing
`@@` (`A/B` private, `E/F` private virtual, `I/J` protected, `M/N` protected
virtual, `Q/R` public, `U/V` public virtual). That letter changes **no codegen
in the body**: access is a pure declaration attribute, and `virtual` changes the
*caller*. Static (`SA…`) is deliberately excluded — static-vs-instance *does*
change codegen (no `this` in r3), which is `argreg_mispair_scan.py`'s job.

Whole-map scan: 4 candidates, 0 ambiguous, all applied, all now pair against
real bodies:

| VA | change | result |
|---|---|---|
| `0x827582a8` `InitObject@Hmx::Object` | public virtual → public | 0% → **88.85%** |
| `0x827f2890` `SetInt@UILabel` | public virtual → public | 0% → **92.59%** |
| `0x82b81ff0` `??_GJsonObject` | protected virtual → public virtual | 0% → **78.95%** |
| `0x82270298` `??1ObjRef` | public virtual → public | 0% → **25.00%** |

3 of the 4 additionally needed a splits ADD (`UILabel.cpp`, `JsonUtils.cpp`,
`MatAnim.cpp`) — the joint move again.

★ **`InitObject` is the map-side resolution of a measured source-side failure.**
Lane AE added `virtual` to `Hmx::Object::InitObject` in *source* to make it
pair, got the identical **88.85%**, and paid **−598** because the vtable slid
fleet-wide. The map rename buys the same pairing at zero source risk. This is
the mechanism behind the standing rule *"DC3 has `virtual`, we don't" is a
map-lineage artifact by default* — the correct channel is the map, always.

---

## 4. ★ "Unmapped beats wrongly-mapped" went 4 for 4

Deleting a wrong name off an EH funclet lets objdiff's **positional** pairing of
anonymous funclets take over, and it matches. A wrong name is not merely
useless — it **actively blocks an already-correct anonymous match**.

* `0X8240EA08` deleted → `fn_8240EA08` gained (+1)
* `0X82334A40` deleted → `fn_82334A40` gained (+1)
* `0x82c309a8` (16 B `oggpack_read` ghost) deleted → **`oggpack_look`** gained (+1)
* `0x822f64d8` (`ConfigPanels@VocalTrackDir` mapped at two VAs in one unit, and
  pairing against the *wrong* 632 B target at 1.77% while the real 748 B target
  sat at 0%) deleted → **1.77% → 81.96%** on the real one

### The `0X`-uppercase lint: 10 of 18 are defects (56%)

All 10 are DELETEs; none had a recoverable correct name. 3 EH funclets
(`0X8240EA08`, `0X82334A40`, `0X826D34D0` — all the `subi r31,r12,0xNN` before
`mflr` prologue), 2 adjustor/vtordisp thunks (`0X8240F158`, `0X8245C2E8`), 2
mid-body offsets that are not function starts (`0X82636E58/68`, both inside
`fn_82636D80`), 2 pointing at `except_data_*` EH records (`0X82670D10`,
`0X827BD2F8`), 1 at 4 bytes of zero padding (`0X82670F84`). The other 8 are
genuine functions, 3 already at strict-100 — left alone. **Lint drained: 1
uppercase key remains and it is correct** (`0X82266BF8`,
`__stl_throw_length_error`).

---

## 5. The alternating loop — per-iteration yield, and it converges in 2

| cycle | map half | splits half | strict |
|---|---|---|---|
| 0 (backlog) | 11 renames + 4 deletes | — | **+11** |
| 1 | 1 displacement (`HasCampaignKey`→`GetCampaignKey`) | 4 MOVE + 3 ADD | **+5** |
| 1b | 7 lint deletes | — | **+1** |
| 1c | 15 free-VA inserts | 6 ADD | **+15** |
| 2 | 4 access-specifier renames | 3 ADD | +0 (4 rows now pair, fuzzy) |
| 2b | 2 adjudicated deletes | — | +0 |
| 2c | 1 sizeof(T) rename + 5 deletes | — | **+2** |
| **3 (fixpoint check)** | **0 candidates** | **0 MOVE, 0 ADD** | **—** |

Cycle 3 is empty on **every** channel simultaneously: `map_displace_round`
(0 free-VA, 0 actionable), `access_specifier_scan` (0 candidates),
`joint_unblock` (0 MOVE / 0 ADD / 0 map assertions), `splits_move scan`
(3 WRONG-UNIT, all 4-byte with `n_carved_in_span == 0` ⇒ refusal criterion 3).
**Two productive cycles, as documented. No sign of "regeneration".**

### Delta decomposition (34 gains, 0 losses)

* **real** 33 — named functions that now pair correctly against their true retail body
* **funclet-only** 2 of those 33 (`fn_8240EA08`, `fn_82334A40`) — anonymous
  positional re-pairings unblocked by deleting a wrong name; genuine, but they
  are anonymous funclets, not newly-understood source
* **fake-removed** 0 — no move in this lane vacated a fake positional match;
  `splits_move`'s fake-match exposure was 0 on every applied move
* **fuzzy-only** (no strict credit, real pairing where none existed):
  `FocusComponent@UIPanel` 99.8%, `SetInt@UILabel` 92.6%, `ConfigPanels@VocalTrackDir`
  82.0%, `InitObject@Hmx::Object` 88.9%, `??_GJsonObject` 78.9%,
  `operator>>(BinStream&, BandCamShot::Target&)` 92.0%, `??1ObjRef` 25.0%

---

## 6. Tool counts before / after

| | before | after |
|---|---|---|
| `argreg_mispair_scan` FORWARD | 1 | 1 |
| `argreg_mispair_scan` INVERSE_WEAK | 16 | 16 |
| `map_displace_round` actionable | 1 + 15 free | 0 |
| `access_specifier_scan` candidates | 4 | 0 |
| `joint_unblock` MOVE / ADD | 5 / 3 | 0 / 0 |
| `splits_move scan` WRONG-UNIT | 3 | 3 (all refused, `n_carved == 0`) |
| map keys | 21,767 | 21,763 |
| duplicate VAs (case-insensitive) | 0 | 0 |
| `0X` uppercase keys | 11 | 1 |
| splits audit findings | 0 | 0 |

★ **argreg is a true 0.** Its single FORWARD hit is `?Terminate@RndMat@@SAXXZ`,
the documented **score-neutral oscillator** (argreg evicts it, byte identity
re-inserts it, both legs measure 0). Repairs did **not** create new argreg
candidates this round — the "re-run it, repairs create candidates" hypothesis
measured **0 new**.

---

## 7. Structurally unreachable residue

* **258 `refuse-contested-claim`** — several unmapped names byte-identical at one
  VA, i.e. linker ICF folds. Byte identity cannot rank them; `--break-ties`
  resolved 61 spatially and the rest are genuinely ambiguous. A VA→name map
  cannot express a fold. **Do not coin-flip these** (measured −23/+0).
* **357 `refuse-holder-already-100`** — guarded, correctly.
* **21 of 23 duplicate-name groups** — both VAs have the **same retail size**
  (per-TU duplicate instantiations / true folds), and report.json already shows
  exactly one member at 100% and the rest at 0%: objdiff has already found the
  only pairing name-collision physics permits. A size-based winner/loser
  resolver emitted **2 safe deletes out of 48 VAs**. ★**Do not re-fund the dedup
  handoff — its yield is 0 and the only available action is deleting the
  winner.**
* **20 `refuse-definer-has-no-span`** — mostly vendor `.c` (DataFlex, arraylist,
  json_object, printbuf, info, mapping0): Quazal / json-c / vorbis, hard-skipped.
* **`??8Head@BandCharDesc`** — linker epilogue fall-through, at_limit (§1).
* **`?GetSustain@Stats`** (100%, float body vs int header at `+0xa4`) — a
  `struct_db`/header staleness lead for the **layout** owner, not a map defect.
* **jeff-lane "2 stale + 5 size-disagreement"** — re-derived against current
  state: the `0 < pct < 5` named population that produced them fell **88 → 10**,
  and all 10 now agree exactly with the `symbols.txt` carve size. **Zero stale,
  zero size-mismatch remain** — already drained by intervening lanes.

---

## 8. New tooling

* **`scripts/harvest/map_edit_textual.py`** — the missing RENAME/DELETE map
  primitive (`tu5_map_apply_fragment.py` can only ADD; `map_rotation_repair.py`
  only whole cycles). Textual single-line rewrites, never `json.dump`. Guards:
  stale-plan detection (`[expected_old, new]` form refuses if another wave
  already moved the entry), name-collision refusal, duplicate-VA invariant,
  case-insensitive key resolution, and a collateral-change assertion over every
  untouched key.
* **`scripts/harvest/access_specifier_scan.py`** — §3.

## 9. Refusals worth recording

* **Refused** renaming `0x823cbdc0`→`??_GCharIKSliderMidi` and
  `0x82682210`→`??_GGameMicManager` (2 of the 7 handed-off `??_E`→`??_G`
  flips): the correct `??_G` name is **already mapped at another VA in the same
  unit** and already reads 100%. The rename would have created a duplicate
  mangled name and split an existing match. Took the DELETE instead. **5 of 7,
  not 7 of 7.**
* **Refused** renaming `0x826975e8` (`?GetCodaPoints@Stats`) → `?GetHarmony@Stats`
  on an offset argument: that VA is **already at strict-100**. An offset argument
  never overrides the holder-already-100 guard.
* **Held back** the `Text.cpp → MeshAnim.cpp` MOVE that `joint_unblock` proposed
  for `0x8245C2E8`: the span is 16 bytes and the body is a vtordisp thunk
  tail-calling `?Print@RndTransformable@@UAAXXZ`, while our `operator>>` is 72 B.
  The pin could never pay. Deleted the map entry instead. ★A joint-tool proposal
  is not evidence about *what the code is* — always cross-check a byte decode.

---

## 10. ★ Independent cold-cache verification (separate agent, own worktrees)

Two fresh `--cold-cache` worktrees at `750ee3d8` vs `laneAI-joint2@f76b1a14`
(the first four commits, +32 of the final +34). Reproduced **exactly**:

| metric | base | head | delta |
|---|---|---|---|
| unit-agnostic (unique names) | 30,214 | 30,246 | **+32** |
| by-(unit, name) | 30,244 | 30,276 | **+32** |
| `matched_code` | 2,934,668 | 2,940,240 | +5,572 B |
| `total_functions` (denominator) | 69,418 | 69,418 | **0** — no denominator movement |

* **LOST = 0 in both counting modes.** (The 30-name gap between the two modes is
  names strict in more than one unit; identical on both legs.)
* ★ **Decisive phantom proof: all 923 compiled `.obj` files are byte-identical
  between the two legs.** No source changed, so the entire +32 is target-side
  (dtk carve + objdiff name pairing) and a stale-obj phantom is *structurally
  impossible*. All 32 gains attribute to a changed map entry at that VA, a
  removed bogus key, or a changed `.text` pin.
* **Negative control** (proves the check is not vacuous): breaking one applied
  map entry (`?FinishLoad@UIPanel@@MAAXXZ` → `…FinishLoadXX…`) dropped the count
  by **exactly 1**, losing only that name; restoring returned it exactly.
* **Stability:** two consecutive full rebuilds per leg agree on the *name set*,
  not merely the count.

### Two incidental fleet findings from the verification

1. ★ **`config/45410914/symbols.txt`'s permanent "uncommitted modification" is
   not a lane edit — it is a deterministic dtk SPLIT auto-regeneration
   artifact.** Two independent cold worktrees, both reset to committed HEAD,
   each regenerated it to a file byte-identical to each other *and* to main's
   uncommitted copy. It can be discounted as a confound in any A/B.
2. ★ **The wired `dynamic_init` obj patcher is not stable across rebuilds.** On
   a *first* cold build, 177 objs differed between legs by exactly one byte —
   the COFF storage class of `??__E*` dynamic-initializer symbols (2 EXTERNAL vs
   3 STATIC), i.e. whether the patcher's effect survived. After a second
   rebuild all 923 objs became byte-identical and the strict count was
   unchanged. Measurement-neutral for strict counts, but **an A/B that compares
   a 1-build leg against a multi-build leg on any metric counting `??__E`
   symbols would be wrong.** Always give both legs the same number of builds.
