# W16-AL — installing the adjudicated alias/map proposals from W16-AJ, W16-AH and W16-AK

Lane W16-AL, branch `w16-al`, 2026-09-14. Owns `scripts/symbol_aliases.json` and the
`icf_alias_*` tools for the lane's life, plus the map rows named below.

**Why the proof is the deliverable and not the bytes.** Under the shipped `name_check`
ruler an alias is *pure forgiveness*: objdiff consults `SymbolEquivalences` and drops the
charge. So an **unproven alias lifts the score BY CONSTRUCTION**, and the `none` control
cannot see it (`none` ignores relocation names, so it reads +0 there by definition — that
flatness is the *signature* of the hazard, not a clearance). Every membership below is
therefore adjudicated on **retail bytes**, and three proposals that arrived marked PROVEN
were refused.

Baseline for cumulative pricing: main `424107a6` (W16-AN booked) =
**43,428 fns / 4,007,884 B / 39.116558 %**, `total_code` 10,246,004, ruler `name_check`.
Rows snapshot `~/tmp/rows_w16an_main.json`.

---

## Summary

| item | proposals | installed | refused | held |
|---|---:|---:|---:|---:|
| W16-AJ §6 | 7 | 6 | 1 | 0 |
| W16-AH held rows | 14 memberships / 2 addresses | 0 | 0 | 14 |
| W16-AK | 7 | 6 | 1 (converted to a map repair) | 0 |
| inherited `--validate` failure | 5 groups | — | 5 withdrawn | 0 |

Cumulative lane measure, set-diff of the `fuzzy==100` row set against the baseline:
**+96 rows / +15,100 B crossed in, −2 rows / −280 B fell out, net +14,820 B / +14 fns.**

---

## Item 1 — W16-AJ §6, proposals 1–7

### AJ1 — Group A, survivor `??1?$hashtable@U?$pair@$$CBHPAVUIComponent@@…` @ `0x826100f8`
Verdict `L1_T1` on 25 of 25 memberships. Map row for `0x826100f8` installed in the **same
commit** as the group, per the brief (naming it alone had measured −2,908 B).

- **Predicted:** the 25 dtor spellings cross; the map row costs bytes by exposing
  previously-forgiven callers.
- **Measured:** **net −176 B**, and *that is the point* — naming an anonymous address pays
  in **bug exposure, not bytes** (`name_check` forgives a placeholder target, so naming
  converts a forgiven site into a checked one). Accuracy beats headline %.
- Commit **`05acf5a5`**.

### AJ2 — Group B, survivor `??1?$hash_map@HPAVUIComponent@@…` @ `0x826101b8`
The six thunks Appendix A lists were excluded, as instructed.

- **Predicted:** close `fn_825F5244`, +40 B (the brief's estimate).
- **Measured:** **+81 rows / +3,568 B** — the brief's estimate was low by ~89×, because the
  thunk family is much wider than the single named row it was priced on.
- **Control:** the 7 excluded thunks were adjudicated independently and **7/7 refuted**, so
  the exclusion was correct rather than merely obeyed.
- Commit **`c7100128`**.

### AJ3 — `??_GTourDescPanel@@UAAPAXI@Z` @ `0x825f4598` — ⛔ **REFUSED**
Proposed as a fold of `??_G`/`??_E` of `CampaignGoalsLeaderboardChoicePanel` and
`??_ETourDescPanel`; expected +8 B (`fn_825F4288`).

**Refused on retail bytes.** Retail's body at `0x825f4598` is **84 B**, and the two extra
instructions over the proposed twin are not padding or scheduling — they are

```
addi   r3, r31, 0x5c
bl     ??1Object@Hmx@@UAA@XZ
```

i.e. the destruction of a **second sub-object at member offset `0x5c`**. A deleting
destructor that destroys two sub-objects cannot be the same COMDAT as one that destroys
one, so `/OPT:ICF` cannot have folded them (ICF folds only COMDATs identical *including
relocations*). This is a real structural difference, not a naming one. **Nothing committed**
for AJ3 — no group, no map row, no `withdrawn` record (there was nothing installed to
withdraw).

### AJ4 — repair the T1 group @ `0x8261feb8` — ✅ installed
AJ proved the body at `0x8261feb8` is **MainHubPanel's** `Unload` (member `0x44`), while
CampaignSongInfoPanel's `Unload` (member `0x3c`) lives at `0x825f58c8`. I re-verified the
member-offset claim on retail bytes before editing, as the brief required.

Map row `0x8261feb8` → `?Unload@MainHubPanel@@UAAXXZ`; group survivor → the same; folded →
`[]` with a `withdrawn` record naming the inversion. Commit **`9bd7996d`**.

### AJ5 — new fold @ `0x825f58c8` — ✅ installed
Survivor `?Unload@CampaignGoalsLeaderboardPanel@@UAAXXZ`, folded
`?Unload@CampaignSongInfoPanel@@UAAXXZ`. Commit **`8527629e`**.

**AJ4→AJ5 are order-dependent** (AJ5 references the name AJ4 moves) and were landed in
consecutive commits, priced together and separately, per the brief.

### AJ6 — new fold @ `0x825f5920` — ✅ installed
Survivor `?Load@SetlistToStorePanel@@UAAXXZ`, folded `?Load@CampaignSongInfoPanel@@UAAXXZ`;
both are `b ?Load@UIPanel@@MAAXXZ`. Commit **`6f6f85b8`**.

### AJ7 — `0x82603a78`, the `??_E…$4` thunk — ✅ installed, Δ0 as predicted
The brief said to read the folded spelling off `AuditionSessionPanel`'s vtable with
`scripts/dump_vtable.py`. **That instrument is structurally incapable of answering here** —
`AuditionSessionPanel` is absent from *both* source oracles (`command grep -ral` over
`../rb3/src` and `../dc3-decomp/src`: rc=1, 0 matches), so there is no class for the script
to decode a vtable of.

Proved instead with **two independent retail-byte instruments**, both returning exactly 2
users of `0x82603a78`:
1. RTTI Complete-Object-Locator enumeration (`tools/retail_rtti.py owning_vtables`), and
2. a raw whole-image word scan for the address.

A 1,197-member "body twin" trap was disarmed first by showing four sampled twins sit at
four **distinct addresses** — distinct addresses are what *non*-folding looks like, since
folding leaves exactly one.

Installed as a **forward (0-forgiveness) membership** with the honest verdicts
`NEEDS_SOURCE` / `UNDECIDABLE` recorded in `evidence`, and the derived-vs-measured boundary
stated explicitly in the group. **Predicted Δ0, measured Δ0.** Commit **`fe5223d1`**.

---

## Item 2 — W16-AH's 14 HELD rows — **all 14 remain HELD**, on much stronger evidence

The 14 rows are **not 14 addresses**: they are **14 memberships across 2 survivor
addresses** — group 557 @ `0x82706100` (7 memberships) and group 680 @ `0x823c9178` (7).
Every survivor is an `ObjPtrVec<T,ObjectDir>::Node` `_Copy_Construct` / `_Param_Construct`;
every folded member is the same function template at a **different element type**
(`Object`, `RndTex`, `Spotlight` vs the survivors' `RndGroup` / `RndTransformable`).

### What I proved that AH did not

**(a) `ObjPtrVec` does not exist in retail at all — a controlled negative.**
AH reported 0 RTTI hits. Measured over the **whole 14,363,648 B image**, not just the RTTI
tables:

| string | occurrences in retail |
|---|---:|
| `?$ObjPtrVec@` | **0** |
| `?$ObjPtrList@` | **45** |

Both classes derive from `ObjRefOwner` and are polymorphic in our source, so both would
emit TypeDescriptors under `/GR`. The instrument **demonstrably finds the sibling** (45
hits), so the zero is a real negative rather than a vacuity — the discipline that an
instrument must be shown able to produce the other answer before its answer means anything.
`../rb3` (the Wii oracle) spells `ObjPtrList` and never `ObjPtrVec`. ⇒ **`ObjPtrVec` is a
DC3-newer container; retail RB3 has only `ObjPtrList`.** Our engine being a verbatim DC3
copy, and DC3 being newer, is the standing explanation and it holds here.

**(b) The fold the memberships assert is refuted on retail bytes.**
Relocation-normalized body hashing (zeroing every `b`/`bl` displacement) over all 57,733
`.pdata` function extents, restricted to the 560 of size 60:

| survivor | reloc-normalized twins in retail | distinct addresses |
|---|---:|---:|
| `0x82706100` | **112** | 112 |
| `0x823c9178` | **112** | 112 |

`/OPT:ICF` folds only COMDATs identical *including relocations*, and a fold leaves **one**
address. 112 shape-twins at 112 distinct addresses, each `bl`-ing its **own adjacent per-`T`
`Node::Node(const Node&)`** (`0x82706100`→`0x82706068`, 0x98 earlier; `0x823c9178`→
`0x8229dc70`), is exactly the documented **`_List_base<T>::clear` population**: 42
addresses, reloc-identical surplus 0. The element-type argument *is* the relocation that
prevents folding.

⚠ The handful of "byte-identical at two addresses" pairs are the documented `memcmp` trap
running in reverse: equal PC-relative displacements at **different** addresses mean the two
bodies call **different** functions. Byte identity across addresses is evidence *against*
folding here, not for it.

**(c) AH's own witness data already said so.** `witness_verdict` is
`NO_WITNESS_FOLDED_SIDE` on all 14 — AH found no witness that any folded spelling ever
reaches the survivor address. My census supplies the positive refutation AH lacked.

### Why the map row is nevertheless NOT repaired here

The type-correct name is an `ObjPtrList` spelling. Installing it would be **actively
harmful**, for a reason independent of its correctness:

- Our build emits **0** `ObjPtrList` `_Copy_Construct`-family rows and only 3 `ObjPtrVec`
  ones, because our source spells `ObjPtrVec`. objdiff pairs target↔base **by name**, so a
  target row renamed to a spelling our base obj cannot define reads **0% forever**,
  however correct the name is. (Proving a name wrong is not the same as renaming being
  safe.)
- Both survivor rows are **currently `fuzzy 100.0 / mpn 100.0`, 60 B each** (units
  `default/Sequence` and `default/CharIKMidi`). A map-only rename therefore costs a
  measured **−120 B** and buys no truth, because —
- — the rows are at 100 via **placeholder forgiveness**, not via the alias: both
  discriminating callees, `0x82706068` and `0x8229dc70`, are **absent from
  `target_symbol_map.json`**, so `name_check` forgives them
  (`is_placeholder_symbol_name`). **The 14 memberships forgive 0 bytes today.**

### Why the memberships are NOT withdrawn either

They are refuted *as a fold*, but a `withdrawn` record is a statement **about a survivor**,
and this survivor's own identity is known-wrong. Withdrawing memberships from a group whose
survivor name must itself change would record the withdrawal against the wrong thing and
would have to be reversed by the source fix — the failure mode that cost **+94,616 B to
reverse** the last time memberships were removed on a Δ0 reading. AH's classification
("a map identification question, not an alias withdrawal") is correct and I am not
overriding it. **Held, not pruned, not clobbered.**

### Exactly what evidence is missing

1. **A `src/` change** switching these containers `ObjPtrVec` → `ObjPtrList` in
   `src/system/obj/Object.h` and its users, so that the type-correct map name is a name our
   base objects actually define. **Barred in this lane** (no `src/` edits) — filed below.
2. **Identification of `0x82706068` and `0x8229dc70`** (`Node::Node(const Node&)`). Naming
   them converts the discriminator from *forgiven* to *checked*, which is what would let
   the map row be adjudicated at all instead of resting on placeholder forgiveness. Both are
   `a:CLOSABLE-BY-NAMING` in AH's own discriminator channel — that is the lever, and it is
   a naming lever, not an alias one.
3. Note in passing: AH's MEDIUM-tier witness dest `0x822a43e8` is already named
   `??0MatSwap@OutfitConfig@@QAA@ABV01@@Z` — an unrelated class — so that witness chain
   does not corroborate the survivor either.

---

## Item 3 — W16-AK's 7 proposals

### AK1–AK6 — ✅ installed
Verdicts as proposed after re-adjudication on the rebased tree (AK's source commit is
`97c4ccdc` post-rebase, was `0cd974cb`). P1/P2 `OnMsg` folded into existing group 65
(`0x825b7f70`); P3–P5 `push_back` into existing group 10 (`0x82b5f808`); P6 `reserve` as a
new group.

- **Predicted:** 8,412 B.
- **Measured:** **+11 rows / +10,580 B.**
- Commit **`8ec857c0`**.

### AK7 — ⛔ **REFUSED as an alias**, converted into a map repair worth **+848 B**
Proposed: `??__FsLoadedFile@@YAXXZ` aliased to `?GetBandUser@BandUserMgr@@SA…`, verdict
`L3_EXACT` — i.e. the proposer's own adjudicator passed it.

**The L3_EXACT pass was false, and diagnosing *why* is the useful part.** L3_EXACT compares
bodies under `name_check`, which **forgives placeholder relocation targets** — and for this
function the two placeholder relocations *carry its entire identity*. With them forgiven,
an atexit destructor and a `dynamic_cast` read as equal.

Resolved on retail bytes: the body at `0x82682668` is
`dynamic_cast<BandUser*>(User*)` — the two RTTI TypeDescriptor operands resolve to
`.?AVBandUser@@` and `.?AVUser@@`. Two independent refutations of the atexit reading:

1. An atexit destructor (`??__F…`) has **zero callers** by construction; this address has
   **six `bl` sites**.
2. The RTTI operands name the cast types explicitly.

⇒ the alias is a fabrication; the correct action is a **map row**, not forgiveness.
Installed `0x82682668` → `?GetBandUser@BandUserMgr@@SAPAVBandUser@@PAVUser@@@Z`.

- **Predicted:** the alias would have scoped 176 B; the map repair should pay more.
- **Measured:** **+3 rows / +848 B.**
- Commit **`19f6f262`**.

⚠ **Caller-attribution trap, recorded so nobody repeats it.** Attributing the six `bl` sites
by "nearest map key ≤ address" misattributed two of them to
`?ResolveSignInWaitStates@OvershellPanel@@` — a function at fuzzy 100.0 that does not call
this address. Caught by arithmetic: `0x825b52c0 + 452 = 0x825b5484`, which is **below** the
sites at `0x825b54f4` / `0x825b55ac`. Fixed by attributing through real `.pdata` extents
(`RetailRtti.extents` + `bisect`); the two sites belong to anonymous 180 B functions at
`0x825b54b0` and `0x825b5568`.

---

## Inherited failure cleared — the 5 CONTRADICTED groups

`tools/icf_alias_finder.py --validate` returned **rc=1** on main at base `8d4fb23a` with 5
CONTRADICTED groups (four STLport `AccomplishmentCmp`/`GoalCmp` pairs, plus the
`SetTypeDef@UIPanel` / `Highlight@RndDir` vcall-thunk pair), reported by W16-AN. They were
not in my worklist; I adjudicated them rather than reverting the two prior restorations
(RELOC-RECONCILE 08-20, W9-D 09-13) that had touched them.

I accepted W9-D's rebuttal of ALIAS-REPAIR while showing **W9-D's own positive claim does
not reproduce**, and excluded circular closure by testing `Sides.equiv` on the comparators
themselves (**False** — so the comparators are not aliased to each other and the argument
is not self-supporting).

All five **withdrawn with records, nothing pruned**. **Measured Δ0**, which is the proof
that they forgave nothing and were pure latent risk. `--validate` rc=1 → **rc=0**.
Commit **`a4ed94a2`**.

---

## Recurring finding across three independent items

**Shape identity is not identity.** AJ7's 1,197 body twins, AK7's 97.57% shape match, the
STLport comparator pairs, and now AH's 112 reloc-normalized twins — in every case the
bodies agree and the *relocations* carry the identity. That is the same reason
`/OPT:ICF` did not fold any of them, and the same reason a match-% instrument cannot settle
any of these questions.

---

## Gates

Run in order, in the worktree, after the final full build (`~/tmp/rb3_build_w16al_12.log`,
rc=0):

```
BUILD rc=0
scripts/verify_ruler_agreement.py --check      -> see below
scripts/verify_objs_patched.py --verify-manifest -> see below
tools/icf_alias_finder.py --validate           -> see below
tools/native_build_gate.sh                     -> NATIVE_GATE_RESULT line, verbatim, below
```

```
BUILD rc=0                                       (~/tmp/rb3_build_w16al_12.log, full ./tools/ninja-locked after `touch config/45410914/config.yml`)

$ python3 scripts/verify_ruler_agreement.py --check        rc=0
  OK  ppc.calculatePoolRelocations = false
OK: both objdiff-cli entry points resolve the same ruler.

$ python3 scripts/verify_objs_patched.py --verify-manifest  rc=0
[denylist] OK: 6 denylisted address(es), 3 with a live map string, none named in 3115 target objects (495648 symbols scanned)
[patch-state] OK: 1215 decomp, 3115 target objects match 2026-09-14T21:22:31Z (tree_sha256=0748ab1f9adfce5c)

$ python3 tools/icf_alias_finder.py --validate              rc=0
  TOLERATED SURVIVOR_MISLABELED     27
  TOLERATED UNWITNESSED            100
  CONTRADICTION_EXEMPT       1
  CONTRADICTED (FATAL)       0
VALIDATE: PASS -- 1396 map-consistent, 247 tolerated (enumerated above), 0 contradicted, 1644 total

$ tools/native_build_gate.sh                                rc=0
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

`--validate` entered this lane at **rc=1** (5 CONTRADICTED, inherited from main and reported
by W16-AN) and leaves it at **rc=0**.

---

## NOT done, and why

- **AJ3** — refused, not installed. Retail's 84 B body destroys a second sub-object at
  member `0x5c`; a two-sub-object deleting dtor cannot be the same COMDAT as a
  one-sub-object one.
- **AH's 14 memberships** — held. The alias half is refuted (112 non-folded twins) but the
  map half needs a `src/` change this lane is barred from, and withdrawing against a
  known-wrong survivor would have to be reversed by that fix.
- **`0x823d3918` / `?Handle@CharLipSync@@`** (handed over by W16-AM) — **filed, not
  attempted.** AM identified it correctly (16 retail `.rdata` slot-6 vtable pointers + 1
  `.pdata` BeginAddress; unit ownership singles out `CharLipSync`), but the +164 B repair
  needs **both** an alias membership *and* a source fix — our `Handle` is **412 B** with
  `parse`/`parse_array` cases against retail's generic **164 B** forwarder, a DC3-newer
  over-implementation. Installing the membership alone would forgive nothing (the row is
  nulled, so the target is a placeholder `name_check` already forgives) while pre-committing
  to an identification that depends on a source fix that has not happened. **Needs a source
  lane.**
- **AM's 20 structural + 148 reloc-only contradictions** — characterized and handed back,
  not acted on. All 20 "structural" entries have `folded_members: [null]`, i.e. they are
  **survivor-SELF** rows: map identification questions, *not* alias withdrawals. Four
  (`0x823f0b50`, `0x8248f1c0`, `0x82787718`, `0x827d5bb0`) are W16-AM's own reserved
  addresses and were not touched.
- **`src/` edits** — none, per the concurrency bar. Two container-type source divergences
  are filed for a source lane: `TourProgress` and `NameGenerator` (their dtors are the two
  rows that **fell out**, −192 B and −88 B — deliberate bug exposure from AJ1's Group A map
  row, landed rather than reverted).
- **`config/45410914/splits.txt`** — not edited, per the bar. Filed: `.text 0x82682668` (28 B
  island) is homed to `CharLipSync.cpp` but the body is `BandUserMgr::GetBandUser`; re-homing
  it to `band3/game/BandUserMgr.cpp` is worth **+28 B** and belongs to a splits lane.
- **`ResolvePartWaitStates`** — filed, not pursued: idx 161 `beq` branch charge, 1,356 B.
- **W16-AM's reserved rows** (`0x823d3918`, `0x823f0b50`, `0x8248f1c0`, `0x82787718`,
  `0x827d5bb0`, `0x822b6538`, `fn_824CE130`, `_bijection_arbitrary`) — not touched.
- **OvershellPanel map rows** — untouched until AK's hand-over message arrived, per the bar.
- **No membership pruned anywhere in this lane.** Five withdrawals, all with records.
