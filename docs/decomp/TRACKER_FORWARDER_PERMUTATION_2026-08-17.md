# `Game` → `TrackerManager` → `Tracker` was a two-level name permutation (lane W34-TRACKER, 2026-08-17)

**Verdict: ACTIONED. +68 B / +0.000660 pp, 4 rows crossed to `fuzzy == 100`,
ZERO fell off, Δ`matched_functions` = 0.** Ruler = shipped **`name_check`**
(read from `report.json` `provenance`: `functionRelocDiffs=name_check`,
`ppc.calculatePoolRelocations=false`).

Baseline reproduced in the lane worktree at `1fde5496` + build:
`matched_functions 44,508 / matched_code 3,767,864 / total_code 10,320,664 /
code% 36.507960`.

Handed off by W31-SONGDB §6 (`SONGDB_FORWARDER_PERMUTATION_2026-08-17.md`),
which evidenced but deliberately did not action it. **Re-derived from retail
bytes, not inherited — and the re-derivation corrected the handoff twice.**

---

## 1. Two corrections to the handoff, both found by re-deriving

- The `Game` forwarder block is **11 forwarders, 8 consistent / 3 name
  mismatches** — not "7/10 with 3 anomalies". All eleven load `this->0x150`
  (the `TrackerManager*`).
- **`Game::OnPlayerRemoved → TrackerManager::HandleRemovePlayer` is NOT a
  defect.** A raw name-equality screen flags it, but `Game.cpp:1138` says
  exactly that delegation. ⇒ **the SOURCE is the arbiter, not name equality.**
  Three further "differing" rows at the `TrackerManager` level are legitimate
  for the same reason (§3).

## 2. The family invariant, and the control that could have failed

Every forwarder is `Outer::X(args) { inner->Y(args); }` where the pair `X→Y` is
fixed by our source. Extracted mechanically from `TrackerManager.cpp`:

| `Tracker::Y` | `TrackerManager::X` (source) |
|---|---|
| `HandleAddPlayer` | `HandleAddPlayer` |
| `HandleRemovePlayer` | `HandleRemovePlayer` |
| `ReconcileStats` | **`OnStatsSynced`** |
| `HandlePlayerSaved` | **`OnPlayerSaved`** |
| `RemoteSetPlayerProgress` | **`OnRemoteTrackerPlayerProgress`** |
| `RemoteTrackerPlayerDisplay` | **`OnRemoteTrackerPlayerDisplay`** |
| `RemoteEndStreak` | **`OnRemoteTrackerEndStreak`** |

★ **The bolded rows are the control.** The invariant is *demonstrated, not
assumed*: it predicts four already-correct addresses, and **three of those four
have DIFFERENT names on the two sides**. A screen based on name equality would
have called them defects; the invariant calls them correct, and they are. A
control that only ever confirmed same-name rows would have proved nothing.

## 3. The derived assignment (retail bytes ⇒ source invariant)

`TrackerManager` level — every row's true name is read off its `Tracker`-level
tail-call target:

| addr | map name BEFORE | retail tail-call | TRUE name |
|---|---|---|---|
| `0x82693140` | `HandleAddPlayer` | `Tracker::HandleAddPlayer` | ✅ CONTROL |
| `0x82693158` | `HandleGameOver(float)` | `Tracker::HandleRemovePlayer` | **`HandleRemovePlayer(Player*)`** |
| `0x82693498` | `OnStatsSynced` | `Tracker::ReconcileStats` | ✅ CONTROL (names differ, legit) |
| `0x82693530` | `HandleRemovePlayer` | `Tracker::HandlePlayerSaved` | **`OnPlayerSaved(Player*)`** |
| `0x826935a8` | `OnRemoteTrackerPlayerProgress` | `Tracker::RemoteSetPlayerProgress` | ✅ CONTROL (names differ) |
| `0x82693620` | `OnRemoteTrackerPlayerDisplay` | `Tracker::RemoteTrackerPlayerDisplay` | ✅ CONTROL (names differ) |
| `0x826936f8` | `OnPlayerQuarantined` | `Tracker::RemoteEndStreak` | **`OnRemoteTrackerEndStreak(Player*,int,int)`** |

`Game` level — the slots shift by exactly one, because one ICF fold consumes a slot:

| addr | map name BEFORE | forwards to | TRUE name |
|---|---|---|---|
| `0x82677450` | `OnPlayerRemoved` | `0x82693530` = `TM::OnPlayerSaved` | **`OnPlayerSaved`** |
| `0x82677458` | `OnPlayerQuarantined` | `0x82693158` = `TM::HandleRemovePlayer` | **`OnPlayerRemoved`** (ICF survivor) |

**Two independent derivations agree.** MSVC emits in source order;
`Game.cpp:1133–1157` is `ForceTrackerStars, OnPlayerAddEnergy, OnPlayerSaved,
OnPlayerRemoved, OnPlayerQuarantined, OnRemoteTrackerFocus, …` — **12 functions
for 11 slots**, and the single missing slot is exactly the
`OnPlayerRemoved`/`OnPlayerQuarantined` fold. Source order and tail-call
targets independently produce the same assignment.

### The keystone: `OnPlayerSaved` was parked on a `MemFree` thunk
`?OnPlayerSaved@TrackerManager@@` was already in the map — at **`0x82276908`**,
which disassembles to `lwz r3,0(r3); cmplwi; beqlr; b ?MemFree@@YAXPAX@Z; blr`
and is owned by **`CharBonesMeshes.cpp`**. Our `CharBonesMeshes.obj` contains
**zero** `TrackerManager` symbols, so that row was pinned at a **permanent 0%**
and could never pair. That squatted name is *why* `0x82693530` had to borrow
`HandleRemovePlayer`, which cascaded the whole permutation. Entry removed.

### `TrackerManager::HandleGameOver` does not exist in retail
A whole-`.text` scan for tail-calls into `Tracker::HandleGameOver`
(`0x826d13b8`) returns **zero**. Its only caller is `TrackerManager::Poll`
(`TrackerManager.cpp:99`), so `/O1 /Ob2` inlined it and never emitted it
out-of-line. **The name has no retail address at all** — the standing "does the
thing needing a name EXIST?" check, run in the negative direction and answering
NO.

## 4. Caller semantics — the third independent instrument

A retail-wide xref attributed to the enclosing `.pdata` function, cross-checked
against our source spelling at each site:

| caller | our spelling | site | calls |
|---|---|---|---|
| `Player::LocalSetEnabledState` | `OnPlayerSaved` | `Player.cpp:478` | `0x82677450` |
| `Band::RemoveUser` | `OnPlayerRemoved` | `Band.cpp:161` | `0x82677458` |
| `Player::SetQuarantined` | `OnPlayerQuarantined` | `Player.cpp:1108` | `0x82677458` |
| `Player::Handle` | `OnRemoteTrackerEndStreak` | `Player.cpp:1258` | `0x82677490` |

★★ **Two DISTINCT source spellings call the single address `0x82677458`.** That
is what a fold *is*, established without looking at a single byte of the bodies.

★★ **A 4-for-4 predictive control.** The model ("charged iff our spelling ≠ the
map name at the called address") predicts each caller's state before measuring:

| caller | model | measured `fuzzy` |
|---|---|---|
| `Band::RemoveUser` | charged | 99.90826 ✓ |
| `Player::SetQuarantined` | **clean** | 100.0 ✓ |
| `Player::LocalSetEnabledState` | charged | 99.97207 ✓ |
| `Player::Handle` | **clean** | 100.0 ✓ |

Two of the four are *clean* predictions, so the control could have failed.

## 5. ⛔ The pre-existing alias at `0x826936f8` was a VACUOUS T1, and it is withdrawn

`symbol_aliases.json` carried a T1 group: survivor
`?OnPlayerQuarantined@TrackerManager@@`, folded
`?OnRemoteTrackerEndStreak@TrackerManager@@`. **It is not a fold.** Their branch
destinations are `Tracker::HandleRemovePlayer` vs `Tracker::RemoteEndStreak` —
different functions.

**Why the gate passed anyway, and it is a general defect, not a one-off:** every
`TrackerManager` guarded forwarder compiles to the *identical* 20-byte body

```
8063001c  lwz    r3, 0x1c(r3)     ; mTracker
2b030000  cmplwi cr6, r3, 0
4d9a0020  beqlr  cr6
4bfffff4  b      <Tracker::X>     ; <- RELOCATED, and MASKED by T1
4e800020  blr
```

T1 is *"byte-identical **modulo relocated fields**, ≥4 words, ≥50% unmasked"*.
Masking the one word that distinguishes them leaves 4/5 = 80% unmasked pure
boilerplate. ⇒ **masked-T1 will "prove" a fold between ANY two forwarders of
this shape.** This is the mirror image of GROUNDED-1's finding that flat T1
*understates* provability: for a thunk **the destination is the entire
information content**, so masking it is not conservative — it is vacuous.

Demonstrated with a control that fires (`~/tmp/w34/foldproof.py`, which compares
destinations instead of masking them):

| pair | body bytes | reloc target names | verdict |
|---|---|---|---|
| `TM::HandleRemovePlayer` vs `TM::OnPlayerQuarantined` | identical | **identical** (`Tracker::HandleRemovePlayer`) | **FOLD PROVEN** |
| `TM::HandleRemovePlayer` vs `TM::HandleGameOver` | **identical** | differ | **NOT A FOLD** ✅ control fires |

Withdrawn with `folded: []` + `withdrawn` + `withdrawn_reason`; **nothing
pruned**, per house rule.

### Two folds installed, both proven with the destination COMPARED
- `0x82693158` — survivor `TM::HandleRemovePlayer`, folded `TM::OnPlayerQuarantined`.
  Mechanism: `OnPlayerQuarantined(p) { HandleRemovePlayer(p); }` inlines under
  `/O1 /Ob2` into exactly `HandleRemovePlayer`'s body.
- `0x82677458` — survivor `Game::OnPlayerRemoved`, folded `Game::OnPlayerQuarantined`,
  by **`/OPT:ICF` fixpoint closure** over the group above.

⚠ The Game-level pair is **not** provable in isolation: all three `Game`
forwarders share the body `806301504bfffffc` and differ only in relocation
name, so `foldproof` correctly returns *NOT A FOLD* on them at our compile
level. The fold exists only **after** the `TrackerManager` fold is closed —
which is precisely why the fixpoint (`scripts/icf_alias_fixpoint.py`) exists.

## 6. ★ Measured as two separate legs, so the channels are ATTRIBUTED, not assumed

| leg | patch | Δ`matched` | Δ`matched_code` |
|---|---|---|---|
| **Run 1** | map renames only, **no alias edits** | 0 | **−92 B** |
| **Run 2** | map renames + 1 withdrawal + 2 proven folds | 0 | **+68 B** |
| ⇒ alias channel (Run 2 − Run 1) | | 0 | **+160 B** |

Run 2 row level: **4 rows crossed to `fuzzy == 100`, 0 fell off.**
`SetQuarantined` returned to 100.0 exactly as designed.

### Answering `ALIAS_SUSPECT` (the guard fired, as pre-registered)
`ab_measure` flagged *"default ruler UP (+68 B) while `none` is FLAT on a
map-only patch — the FABRICATED-ALIAS shape"*. The two-leg design answers it:

- The **map channel alone is net-NEGATIVE (−92 B)**. A fabricated alias
  manifests as a *gain with no offsetting loss*; here the alias's +160 B
  **exactly cancels a loss the correct rename inflicted** on `SetQuarantined`,
  and buys nothing else.
- The alias's single beneficiary is adjudicated on retail bytes: the one
  charged site in `SetQuarantined` is literally
  `target ?OnPlayerRemoved@Game@@` vs `ours ?OnPlayerQuarantined@Game@@` — the
  proven fold pair, nothing else.
- Net across both channels the patch is **+68 B**, i.e. **less** than the
  forgiveness it withdraws plus installs would suggest. It also **removes** a
  vacuous forgiveness.

## 7. ★★ Prediction vs measured — and the reusable instrument the miss produced

| quantity | predicted | measured |
|---|---|---|
| Δ`matched_functions` | 0 | **0** ✅ exact |
| direct rows | +68 B | **+68 B** ✅ exact |
| `SetQuarantined` regression (Run 1) | −160 B | **−160 B** ✅ exact |
| cascade (`RemoveUser` + `LocalSetEnabledState`) | +1,868 B | **+0** ❌ |

**The cascade prediction was wrong, and the reason is worth more than the
bytes.** Both rows moved *exactly halfway*: `RemoveUser` deficit 0.0917 →
0.0459, `LocalSetEnabledState` 0.0279 → 0.0140. Each had **two** charged
relocation-name sites; I fixed one.

⇒ **A charged relocation-name site costs exactly 0.05 instruction-equivalents**,
so the number of charged sites is computable from `report.json` alone:

```
charged_sites = (100 - fuzzy_match_percent) * size / 20
```

Validated to **exact integers on 8/8 rows** (2.00, 1.00, 2.00, 1.00, 0.00, 1.00,
1.00, 1.00). I had the data to get this right *before* measuring — I computed a
per-site weight of 0.05 for the forwarder rows and 0.1 for the caller rows and
waved the discrepancy away instead of concluding **0.1 = 2 × 0.05.**

⚠ This matters because the usual instruments cannot see these charges at all:
`run_objdiff` reported **both** cascade rows as *"N instructions | all equal /
Complete (High) — No action needed"* while `report.json` had them below 100.
That is CLAUDE.md's instruction-vs-argument trap, reproduced live. The formula
above is an *arithmetic* charge-counter needing no objdiff run.

★ **The cascade/pairing ratio is confirmed NOT to be a constant, in the third
direction.** W31 measured cascade 98.9% off forwarders with **15–22 call sites
each**; these carry **1–2**, and the cascade delivered **0**. Priced the fan-in
first, as instructed — the fan-in correctly predicted a small cascade; what it
could not predict was that the two candidate rows were each blocked by a
*second, unrelated* charge.

## 8. Priced handoff — the remaining +1,868 B, and why this lane did not take it

Both cascade rows are one charge away from crossing, and both charges are
**outside this family** (identified with `~/tmp/w34/relnames2.py`, an
offset-keyed relocation-name diff that applies `name_check`'s placeholder
forgiveness):

| row | prize | remaining charge | tractability |
|---|---|---|---|
| `Player::LocalSetEnabledState` | **+1,432 B** | `push_back@vector<Vector2>` vs `push_back@vector<Extent>` | **PROVABLE** — both STLport instantiations we compile, but in different TUs, so it needs a cross-obj fold proof |
| `Band::RemoveUser` | **+436 B** | `?FinalRelease@CSpPhoneConverter@NUISPEECH@@` vs `?DeletePlayer@BandUser@@` | **UNPROVABLE our-side** — NUISPEECH is vendor with no source (the "needs absent source" class) |

Not actioned here: installing either alias without a destination-compared proof
is exactly the fabrication hazard this lane just documented at
`0x826936f8`. `LocalSetEnabledState`'s other three fold pairs
(`FileLoader::GetSize` ≡ `Band::MainPerformer`, the two `MakeString`
instantiations, `CrowdRating::GetRawValue` ≡ `Performer::PollMs`) are **already
aliased** and correctly uncharged — note the first is the same fold W31 flagged
as the "implausible" `Game::GetMainPerformer → FileLoader::GetSize` row.

## 9. What this lane deliberately did NOT do

- **Did not touch `src/**`** ⇒ the native gate is **not applicable** (patch is
  map + alias JSON only).
- **Did not name `TrackerManager::ForceStars`.** `Game::ForceTrackerStars`
  tail-calls `0x82a478d0`, unnamed and in the vendor band; `ForceStars` is
  `{ unk0 = stars; }` = `stw r4,0(r3); blr`, which folds with a large generic
  class. Naming an anonymous address has zero byte upside by standing
  economics, and its payout (bug exposure) is not worth the fabrication risk on
  a fold that size.
- **Did not chase the two residual cascade charges** — priced in §8.

## 10. Safety checks, run BEFORE firing

- **DEFINED vs UNDEF**: our `TrackerManager.obj` **defines** all five spellings
  and `Game.obj` **defines** all four, so every move pairs — **no `splits.txt`
  pin move was needed** (unlike W31, where `SongData::GetGemList` was UNDEF and
  forced one).
- **Map injectivity**: 29,004 named entries / 29,002 distinct; the **2
  collisions are pre-existing and unrelated** (`?NodeCmp@@YAHPBX0@Z`,
  `__destroy_aux<LevelData>`) and the applier **asserts the collision set is
  unchanged**, refusing on any new one.
- **Byte geometry**: each renamed row's `report.json` size (20 B / 20 B / 20 B /
  8 B) equals exactly 5 / 5 / 5 / 2 instructions of the observed bodies, so none
  is a dtk mis-carve phantom.
- **Worktree hygiene**: the map was read from **the worktree's `HEAD`**, never
  from main's working tree (main was dirty — W31's near-miss).
- Pre/post conditions are asserted in the appliers themselves
  (`~/tmp/w34/apply_map.py`, `apply_alias.py`), so a re-run cannot double-apply
  — it refused when tried.

## Reusable instruments (`~/tmp/w34/`)
`fwdgen.py` (generalized `lwz r3,N(r3); b <t>` forwarder scan over a VA range),
`tmtable.py` (map-name vs tail-call target for every named address in a unit's
pinned spans), `relnames.py` / `relnames2.py` (offset-keyed relocation-name
diff applying `name_check` placeholder forgiveness — the only way to see these
charges), `foldproof.py` (**fold proof with the destination COMPARED, not
masked**, plus a control that fires), `apply_map.py` / `apply_alias.py`
(idempotent appliers with preconditions + injectivity gate).
