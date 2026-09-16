# W16-DB — the 213 `UNDER_PARTITIONED_ICF_CLOSURE` withdrawals, adjudicated per membership

**Date:** 2026-09-16 · **Lane:** W16-DB · **Branch:** `w16-db` · **Base:** `56e36a72`
**Tree:** built and patched before any name lookup — `[patch-state] OK: 1219 decomp,
3105 target objects match`, `tree_sha256=77d8bae6ac786cb6` (**unchanged by this lane** —
the alias ledger is a report-time input). Ruler `functionRelocDiffs=name_check`,
objdiff 4.2.9 (`a5f0ea903ec1`), read from `report.json`'s own `provenance`.

---

## 0. Verdict in one line

**139 of the 213 are restored on flat, non-circular retail-byte evidence; 74 stay
withdrawn, each for a reason stated on retail bytes.** Measured **+1,072 B / +1 fn**,
permuted decoy **+0 B**, both legs asserting they reached the ruler.

## 1. The structural finding the dispatch did not anticipate

`tools/gen_symbol_alias_map.py`'s `render_map` **skips address-less groups**
(`if not g.get("address"): continue`). The census of where each withdrawn spelling
now lives:

| spelling live elsewhere at | count |
|---|---:|
| a group with **NO address** (renders no map line) | **154** |
| a different address | 2 |
| the same address | 0 |
| nowhere (truly dead) | 57 |

⇒ The `repartitioned` half was not deleted, it was **moved into an address-less
partition class** — live in the ledger, **rendered into no map line, forgiving exactly
nothing**. The generator's own comment names this shape ("a partitioned-out class from
`alias_repair.py` is exactly this shape"). So these memberships were *invisible*, not
*refuted*, which is why re-adjudicating them can pay at all.

⚠ **objdiff's parser is a UNION, not a partition** (`objdiff-core/src/obj/map_file.rs`):
a name emitted at several addresses keeps **every asserted pair**, deliberately and
non-transitively; 36 symbols already ship that way. So a restore *adds* assertions and
cannot corrupt an existing bucket. ⛔ `w16cp_layer.py`'s `if len(a)==1` drop-ambiguous
helper is **not** objdiff's semantics — it is a local gate helper, and reading it as the
ruler would mis-price every multi-address spelling.

## 2. Adjudication — reproduced independently, then tested for circularity

`tools/alias_forgiveness_audit.Sides.verdict` over all 213, controls first:

```
ANTI-VACUITY: target bodies 69432 (mangled 27605) | our bodies 95494
CONTROL+ known-live membership -> L1_T1        (expect a PROVEN layer)  OK
CONTROL- nonsense pair         -> NEEDS_SOURCE (expect NEEDS_SOURCE)    OK
L1_T1 142 · NEEDS_SOURCE 29 · CONTRADICTED 26 · L2_RECURSIVE 8 · NEEDS_MAP_ID 7 · L5_INCONSISTENCY 1
```

This **reproduces W16-CP's layer assignment 213/213** in a different tree at a different
base. The population also reconciles exactly: CP's 456 − CU's 242 − CT's 1 = **213**.

**Circularity test, not a reading.** `adjudicate()` consults only `mapped`, never the
alias groups — but that is documentation. Re-running every PROVEN row with `Sides.eq`
**emptied**: all **142 L1_T1 stay L1_T1**. The proof does not close over the fold class
under test.

## 3. The self-alias trap — a stronger reason than "no-op"

Exactly 3 rows are self-aliases (spelling == the survivor of the very group carrying the
withdrawal), and they are **exactly** the 3 `dropped_singleton` rows scoring L1_T1
(`ObjDirItr<SpotlightDrawer>`, `<BandList>`, `<RndGroup>`). Their L1_T1 is the
adjudicator comparing retail's body at address X against **our** body for the *same
name* — a match check, not fold evidence; the alias it would install ("X may stand in
for X") asserts nothing. **Excluded.** 139 installed.

## 4. Pre-registration, scored

Registered **before** any measurement:

| quantity | predicted | measured | |
|---|---|---|---|
| Δ`matched_code` | +0…+1,500 B, point **+600** | **+1,072 B** | in band, **point estimate 79% low** |
| Δ`matched_functions` | +0…+4, point **+1** | **+1** | exact |
| rows falling out | **0** | **0** | exact |
| permuted decoy | **+0 B** | **+0 B** | exact |
| map lines each leg | **8,524** | 8,524 | exact |
| the 64 vendor-band rows pay | **0** | **0** | confirmed |

**The miss is the informative part.** I discounted the point estimate because 64 of the
139 sit at `0x82bf6f58`, above `0x82A00000`, where CLAUDE.md records folding as 17×
weaker. That sub-prediction was **right** — those 64 paid nothing — but the discount was
then wrong overall, because payment concentrated in two *small* groups I had priced at
nearly zero. **Being right about the 64 and wrong about the total is one prediction, not
two:** the vendor-band reasoning was never load-bearing for the total.

## 5. The controls

**Permuted decoy.** Both legs install the same 139 spellings into the same 22 groups
from the same population; only the pairing differs. A random-symbol decoy would pay 0
vacuously; a derangement cannot.

| leg | map lines | Δcode | Δfns | crossed in | fell out |
|---|---|---:|---:|---:|---:|
| TRUE pairing | 8,524 (+139) | **+1,072 B** | +1 | 2 | 0 |
| PERMUTED decoy | 8,524 (+139) | **+0 B** | +0 | 0 | 0 |

**Anti-vacuity, per CU's caught defect:** each leg asserts *inside the measured path*
that the rendered map is exactly 8,524 lines, so a leg that never reached the ruler
REFUSES rather than reporting a clean false zero. Both legs asserted; ledger and map
restored afterwards with sha verification.

⚠ The `none` ruler is **not** used as a clearance: an alias lifts `name_check` by
construction and leaves `none` flat by construction, so its flatness is the signature of
the hazard, not evidence. `ALIAS_SUSPECT` is likewise structurally silent here — 137 of
the 139 forgive 0 B and cannot move either ruler.

## 6. Structural attribution — payment lands where the fold predicts

Both crossing rows carry **exactly one** charged relocation-name pair, and in both the
counterparty is **the survivor of the group the spelling was restored into**:

| row | Δ | our spelling restored | counterparty at the site |
|---|---:|---|---|
| `?Handle@RndGroup@@` (`default/Group`) | **+1,012 B** | `clear@ObjPtrList<Object,ObjectDir>` → gi 37 @`0x8249d1f0` | = that group's **survivor** `clear@ObjPtrList<EventTrigger,ObjectDir>` |
| `fn_8233DFE0` (`default/BandList`) | **+60 B** | `_Destroy_Range<HighlightObject*>` → gi 154 @`0x8233c8c0` | = that group's **survivor** `_Destroy_Range<map<int,float>*>` |

⚠ **I got this wrong once and caught it.** My first attribution reported payer 2's
counterparty as *not* the survivor — because I hand-reconstructed the mangled name from
a print truncated at 110 characters, inventing `@1@0@Z` where the artifact says `@0@0@Z`.
Re-read from the JSON, it matches at all 217 characters. This is CLAUDE.md's
"compare artifacts, not transcribed values" rule biting on a 217-character symbol;
**never retype a mangled name.** Note also that `fn_8233DFE0` is an **anonymous** row —
set-diff attribution prices it exactly as well as a named one.

## 7. What stays withdrawn, and why — the 74

**26 CONTRADICTED — now adjudicated on retail bytes, not merely counted** (CP §7.4's
standing caveat is discharged): 3 refuted by body **size** (a 112 B vs 56 B COMDAT pair
cannot fold), 23 by relocation targets naming genuinely different callees. Representative,
and none of them subtle: `GetSystemDateAndTime` vs `GetDateAndTime` call **`GetLocalTime`
vs `GetSystemTime`**; `NewObject@HamSong` constructs `BandSong`; `__insertion_sort` under
`GoalAlpaCmp` vs `AccomplishmentCategoryCmp`; and a family of `$4PPPPPPPM@` vtable
adjustor thunks forwarding to different virtuals. **Correctly withdrawn.**

**7 NEEDS_MAP_ID — chased one level, and the class does NOT drain the way MAPID-1's did.**
Each is blocked by one slot whose retail callee is an unnamed `fn_XXXXXXXX` (5 distinct
addresses). Adjudicating retail's body at that address against our callee:

| blocked row | result of the chase |
|---|---|
| `_M_clear_after_move` @`0x82703c58` | **CONTRADICTED** — `~String` vs `~PropertyFilter` |
| `Replace` @`0x823afe88` ×2 | **CONTRADICTED** by size (144 vs 120 / 124); the *only* body of ours matching `fn_823AE888` is `CharWeightable::Replace` — i.e. retail's slot is the survivor's own class and our thunks forward elsewhere |
| `$?6VPracticeStep` @`0x82346de0` | **CONTRADICTED** by size (92 vs 96), 0 of our bodies match |
| `$__destroy_range` @`0x82387960` ×2 | **CONTRADICTED**, word differs @`0x3c`, 0 of our bodies match |
| `clear` @`0x823d9920` | still map-blocked — and **6 of our symbols share retail's body** at `fn_823D95C8`, so naming it is a 6-way coin flip |

⇒ **6 of 7 are refuted, not unfunded.** Naming these addresses would expose divergences,
not restore folds. ⚠ Scope: a one-level chase with the same T1 comparator removes the
route the `NEEDS_MAP_ID` label names; it is not a proof that no fold exists by any other
argument. **No map edit was made** — the honest deliverable here is that the vein the
brief hoped for is not there.

**29 NEEDS_SOURCE** — one side absent from the compiled objs (`BandCharacter::Init`,
`Memcard::Memcard`, …). Unprovable without source; correctly withdrawn.

**8 L2_RECURSIVE** — all one group, `_M_erase@vector<ObjPtr<T>>` → survivor
`_M_erase@vector<ObjOwnerPtr<Waypoint>>`. L2 is closed under the fold class under test
and does not outrank a recorded withdrawal; W9-D, W16-CP and W16-CU all left it
withdrawn and I concur. ★ **Corroborated from inside this lane's own data:** the
CONTRADICTED set independently refutes two `PropSync` rows precisely on the
`ObjPtr`/`ObjOwnerPtr` distinction (`SetObjConcrete` vs `SetOwnerObj`), so that
distinction is demonstrably load-bearing in this tree rather than cosmetic.

**1 L5_INCONSISTENCY** (`resize@list<OldMatOption>` → `resize@list<OldMMInst>`) —
**deliberately left withdrawn, and this is a judgement call worth re-opening.** It is a
PROVEN layer and survives the flat re-run, but `alias_forgiveness_audit`'s published
decoy study bounds the false-positive rate of **L3 (0.07%) and L4 (0.05%) only** — L5's
is uncontrolled in this tree. One row resting solely on an uncontrolled layer does not
clear the bar the 139 cleared. A lane that controls L5 against decoys should take it.

**3 self-aliases** — §3.

## 8. Landed

139 memberships restored across 22 groups, GROUNDED-2 / W9-D convention: the record
**moves** out of `withdrawn[]` into `restored[]` with its original text verbatim under
`superseded_records`, the spelling joins `folded[]`. Nothing deleted, nothing merely
annotated. Deep-compare asserted **only the 22 intended groups changed**, and within them
only `folded`/`withdrawn`/`restored`.

```
folded 5336 -> 5475 (+139)   withdrawn 10243 -> 10104 (-139)   restored 316 -> 455 (+139)
map 8385 -> 8524 lines
matched_code 4119188 -> 4120260 (+1072 B)   matched_functions 43934 -> 43935 (+1)
code% 40.198700 -> 40.209160                fuzzy 49.993137 -> 49.993150
UNDER_PARTITIONED_ICF_CLOSURE 213 -> 74     FABRICATED_CLOSURE_NOT_PARTITION 9393 (UNTOUCHED)
CROSSED IN 2 (1072 B)   FELL OUT 0          PREDICTION HELD IN BAND, POINT ESTIMATE LOW
```

Installed **because they are true, not because they pay**: 137 of the 139 forgive nothing
at all and are installed anyway.

## 9. Data-hygiene findings in the artifact

1. **2 `withdrawn[]` entries are bare STRINGS, not dicts** — group 648
   `OnPlayerQuarantined` @`0x826936f8` holding
   `?OnRemoteTrackerEndStreak@TrackerManager@@QAAXPAVPlayer@@HH@Z`, and group 1394
   `GetVocalNoteList` @`0x82770730` holding `?GetGemList@SongData@@QAAPAVGameGemList@@H@Z`.
   A naive `w.get(...)` census dies on them. **Left in place and reported** — they carry a
   spelling but no class, so silently dropping them would erase a withdrawal.
2. **5 withdrawal records carry no `class` key at all.**
3. The class census has **45 distinct classes**, not the 12 a top-N table suggests.

## 10. What this lane did NOT do

1. **No map edit, no splits edit, no source edit.** `tree_sha256` unchanged, `[patch-state] OK`
   before and after. Only `scripts/symbol_aliases.json` and this document.
2. **The 9,393 `FABRICATED_CLOSURE_NOT_PARTITION` records were not opened** — verified
   unchanged at 9,393. That vein is closed
   (`docs/decomp/ALIAS_FABRICATED_CLOSURE_CLOSED_2026-09-16.md`).
3. **`default/ChordbookPanel` (W16-CZ) and the `TheDebug`/`MakeString` source stratum
   (W16-DA) untouched.** 12 of the restores are `MakeString` *aliases*; that is the ledger
   surface, a different file from DA's source stratum.
4. **Per-membership install-one-at-a-time attribution was not run** (CU's 242-leg method).
   It was unnecessary: only 2 rows crossed and each carries exactly one charged pair, so
   attribution is exact from the charge itself at a fraction of the cost.
