# W16-EL — `AccomplishmentProgress::mGigTypeCompletedMap`: the key is `int`. REFUTED.

**Verdict: outcome (B) of the brief.** Our `std::hash_map<int, int>` declaration is **CORRECT**.
W16-EJ's "container-type defect, top candidate" is **refuted on retail bytes**. The single
`diff_arg` charge on `??0AccomplishmentProgress@@QAA@PAVBandProfile@@@Z` (588 B, fuzzy
99.96599) is an **ICF fold-survivor naming artifact**, not a type error. No source was changed
and none should be.

Retail names the container type explicitly in its own symbol table. This is not an inference.

---

## 1. The charge, enumerated by kind (never screened on a percentage)

`run_objdiff` at the graded `name_check` ruler, on a fully built tree:

| | |
|---|---|
| row | `??0AccomplishmentProgress@@QAA@PAVBandProfile@@@Z`, 588 B |
| instructions | 147 total: **146 equal, 1 `diff_arg`** (index 135), 0 insert/delete |
| diff score | 5 / 14700 ⇒ fuzzy **99.96599**, mpn **99.96599** |
| pattern | `TEMPLATE_INSTANTIATION_MISMATCH` — target `??0?$hash_map@VSymbol@@H…::hash_map()` vs base `??0?$hash_map@HH…::hash_map()` |

So the row is one relocation-NAME charge and nothing else. It crosses iff that name agrees.
⚠ `mpn == fuzzy` here — consistent with objdiff-core `b14ba45` promoting a vetted
relocation-name diff into `mpn`. **`mpn < 100` certified no instruction-level defect**, exactly
as the brief warned.

## 2. Retail-byte evidence that the key is `int` — three independent families, each with a control

The member is at `+0x64c`. Its three siblings (`mToursPlayedMap` `+0x5f8`, `mTourMostStarsMap`
`+0x614`, `mToursGotAllStarsMap` `+0x630`) are `hash_map<Symbol,int>` and serve as the
untreated control in every test below: if a probe cannot separate `+0x64c` from those three, it
is not measuring key type.

### (a) ★ DECISIVE — retail's own symbol names spell `hash_map<int,int>`

`?SaveFixed@AccomplishmentProgress@@UBAXAAVFixedSizeSaveableStream@@@Z` (0x825909b8) calls, in
member order:

| member | retail callee | map name |
|---|---|---|
| `+0x5f8`, `+0x614`, `+0x630` | `fn_82590100` ×3 | `??$SaveStd@H@FixedSizeSaveable@@…ABV?$hash_map@VSymbol@@H…` |
| **`+0x64c`** | **`fn_82590198`** | **`??$SaveStd@HH@FixedSizeSaveable@@…ABV?$hash_map@HH U?$hash@H@…`** |

`?LoadFixed@…` (0x82591a50) mirrors it exactly: `fn_82591360` (`LoadStd<H>` over
`hash_map<Symbol,int>`) ×3, then **`fn_82591420` = `??$LoadStd@HH@…AAV?$hash_map@HH…`**.

⇒ Retail contains a **distinct** `SaveStd`/`LoadStd` instantiation whose parameter type is
literally `const hash_map<int,int>&`, at a **different address** from the Symbol-keyed one, and
`AccomplishmentProgress` calls it **for this member**. The container is `hash_map<int,int>`.

### (b) `operator[]` — `SetQuestCompletedCount(TourGameType, int)` @ 0x82591f78

```
stw   r4, 0x50(r1)        ; the TourGameType arg, spilled
addi  r4, r1, 0x50        ; &key
addi  r3, r3, 0x64c       ; &mGigTypeCompletedMap
bl    fn_825D3228         ; ??A?$hash_map@H PAVSongUpgradeData@@… ::operator[](const int&)
stw   r30, 0x0(r3)        ; *slot = count
```
Key type `H` = `int`. **Control:** `SetToursPlayed` / `SetMostStars` / `SetToursGotAllStars`
(0x82591e40 / 0x82591ed8 / 0x82591f28) perform the identical shape against `+0x5f8` / `+0x614` /
`+0x630` and call a **different** function, `fn_827B0E78`.

### (c) `find` — `GetQuestCompletedCount(TourGameType)` @ 0x82590fc8

Calls `fn_82576088` = `??$_M_find@H@?$hashtable@U?$pair@$$CBH PAVSongMetadata@@…` — `_M_find<int>`
on a hashtable whose key is `H`. **Control:** the three sibling getters (0x82590ed8 / 0x82590f28 /
0x82590f78) call `fn_82557770` = `??$_M_find@VSymbol@@@?$hashtable@U?$pair@$$CBVSymbol@@H@…`.

`_M_find` is the one function in the chain whose body genuinely depends on the key type — it
invokes `hash<K>` and `equal_to<K>`. Retail holds **two distinct bodies** and uses the **int**
one for `+0x64c`.

## 3. Why the charged callee cannot discriminate key type — measured, not argued

The `hash_map` default ctor never touches the key: it forwards to the hashtable ctor. Retail's
bodies show this directly.

* `fn_8255D480` (`hash_map<Symbol,int>::hash_map`, map name) — **19 words / 76 B**.
* `fn_82599090` (`??0?$hash_map@H U?$pair@H_N@…` — an **int-keyed** hash_map ctor) — **19 words /
  76 B, identical to `fn_8255D480` in 18 of 19 words**, differing only at the `+0x30 bl`.

⇒ A `hash_map` default-ctor body is **key-type-independent**. "Retail calls the Symbol-keyed
ctor" therefore carries **zero** information about the key, and the whole charge is decided one
level down, by which hashtable ctor the `+0x30 bl` names.

Our own compiled COMDATs in `build/45410914/src/band3/meta_band/AccomplishmentProgress.obj`
reproduce the same structure:

| pair | size | differing words | relocations |
|---|---:|---:|---|
| `??0?$hash_map@HH…` vs `??0?$hash_map@VSymbol@@H…` | 76 B | **0** | 1 each, both at `+0x30`, type 6, differing only in the referenced NAME |
| `??0?$hashtable@U?$pair@$$CBHH@…` vs `??0?$hashtable@U?$pair@$$CBVSymbol@@H@…` | 120 B | **0** | 5 each, identical offsets/types; only the `_M_initialize_buckets` NAME differs |

And against retail, measured in this lane (not inherited from W16-BU):

> **our `??0?$hash_map@HH…` (76 B) vs retail `0x8255d480` (76 B): differing words at `[0x30]`
> only — and `+0x30` is precisely our one relocation site.**

That is the T1 fold shape: identical linked body modulo a single relocated branch.

## 4. So what IS the charge? An observed ICF fold, blocked by an alias-placement conflict

Retail's ctor (`fn_82592210`) constructs **all four** maps with the *same* `bl fn_8255D480`
(`+0x30`, `+0x5f8`, `+0x614`, `+0x630`, `+0x64c` — five sites). Since §2 proves `+0x64c` is
`hash_map<int,int>`, retail's `0x8255d480` **demonstrably serves an int-keyed instantiation**.
That is the same argument form W16-BU used to install the existing group at this very address
("retail itself collapsed the two, observed rather than inferred").

⛔ **But I did not install the alias, and the reason is concrete, not squeamishness.**
Our `??0?$hash_map@HH…`'s single relocation names `??0?$hashtable@U?$pair@$$CBHH@…`, and
`scripts/symbol_aliases.json` **already places that spelling at `0x825a07e0`** (group
`"0?$hashtable"`, 11 members), whereas retail's `0x8255d480` branches to **`0x8255c968`**.
Installing my group would transitively assert `0x8255c968 ≡ 0x825a07e0` — a claim nobody has
tested, and exactly what `tools/comdat_fold_gate.py` refuses ("F admitted against more than one
survivor ADDRESS → REFUSE every one of them"). This is, almost certainly, the mechanism behind
W16-EJ's "fold-REFUTED" verdict, and EJ's own §5 states the right order: **prove the callee
folds first, then the parent passes on its own evidence.**

⚠ And "identical ⇒ folded" is **empirically false in this neighbourhood**, so the gap cannot be
closed by inference. Retail holds **three** 120-B hashtable-ctor bodies — `0x8255c968`,
`0x825983d8`, `0x825a07e0` — identical in all 29 non-`bl` words, *unfolded*, at three addresses.
`0x8255c968` and `0x825983d8` are byte-identical **including** their `bl` target (`fn_8256ad18`)
and still did not fold. That is within CD-7's measured ICF residue (51 surplus copies), and it
means a fold must be **observed**, never deduced from byte identity.

**Fan-in, which sharpens the follow-up:** `fn_8255C968` has exactly **one** caller
(`fn_8255D480`); `fn_825983D8` exactly one (`fn_82599090`). But `fn_825A07E0`'s three callers are
`_Copy_Construct<set<MoveParent*>>`, `fn_825A38E8` and `_M_insert_overflow_aux<vector<set<…>>>` —
**not hash_map ctors at all**. So the existing placement of our int-keyed hashtable ctor at
`0x825a07e0` looks weakly grounded, and the evidence here implies its real home is `0x8255c968`.
Re-homing it would touch an 11-member group shared with SongMetadata / SongStatus / UIComponent /
ColorPalette / String / Symbol units — **too wide to land and price inside this lane.**

## 5. Pre-registration vs. what I measured

**Pre-registered, before reading any score:** if the key were really `Symbol`, retail's
initialisation and accessors for `+0x64c` would look like the three Symbol siblings'. Named
failure mode: if `+0x64c`'s accessors called the *same* callees as the siblings, the Symbol
hypothesis would survive and I would have had to take it seriously.

**Measured:** they call **different** callees in all three families (`operator[]`, `_M_find`,
`SaveStd`/`LoadStd`), and in the `SaveStd`/`LoadStd` family retail's symbol name spells
`hash_map<int,int>` outright. The Symbol hypothesis fails its own test.

**Pre-registered economics of the "fix":** flipping the declaration would gain at most **+588 B**
(the one charge) and put the rows that currently pair *because* the type is `int` at risk.
**Measured, from `report.json` on this tree — all currently perfect:**

| row | size | fuzzy |
|---|---:|---:|
| `?SaveFixed@AccomplishmentProgress@@UBAX…` | 1232 | **100.00000** |
| `?LoadFixed@AccomplishmentProgress@@UAAX…` | 932 | **100.00000** |
| `?Clear@AccomplishmentProgress@@QAAXXZ` | 440 | **100.00000** |
| `?SetQuestCompletedCount@…@W4TourGameType@@H@Z` | 80 | **100.00000** |
| `?GetQuestCompletedCount@…@W4TourGameType@@@Z` | 80 | **100.00000** |

⇒ **2,764 B of already-matching rows would be put at risk to chase 588 B**, and `SaveFixed` /
`LoadFixed` (2,164 B of it) pair by name against retail's `SaveStd<int,int>` / `LoadStd<int,int>`
— they would break *by construction* under a Symbol key.

**No A/B was run, deliberately, and this is a result rather than an omission:** the proposed
change is **not expressible**. `AccomplishmentProgress.cpp:549` is
`for (int i = 0; i < 0x32; i++) mGigTypeCompletedMap[i + 0x3E8] = 0;` and
`SetQuestCompletedCount` keys on a `TourGameType` enum. A `Symbol` cannot be produced by
`i + 0x3E8`, so a "flip the type" patch does not compile without inventing key semantics — and
inventing them to move one relocation name is precisely the metric-fitting the brief forbade.
Pricing a fabricated patch would have measured the fabrication, not the hypothesis.

## 6. Corroboration from the oracle and from our own tree

* **rb3-Wii oracle** (`../rb3/src/band3/meta_band/AccomplishmentProgress.h:236`) declares
  `std::map<int, int> mGigTypeCompletedMap;` — int-keyed, and not even a hash_map. It agrees
  with us on the key and disagrees only on the container, which retail settles as `hash_map`.
* `scripts/symbol_aliases.json` **already** carries group `"A?$hash_map@H"` at `0x825d3228`
  whose folded list contains `??A?$hash_map@HH…::operator[]`. That group is why
  `SetQuestCompletedCount` scores 100 today — i.e. **the tree has already accepted, and is
  already scoring on, the proposition that this member is int-keyed.**

## 7. What I deliberately did NOT do

* **Did not change the type**, or any source, header, or map file. The declaration at
  `AccomplishmentProgress.h:284` is correct as written; the three sibling maps being
  `hash_map<Symbol,int>` and this one being `hash_map<int,int>` is retail's own design.
* **Did not install an alias group** for `??0?$hash_map@HH…` at `0x8255d480`, despite the
  observational evidence, because of the `0x825a07e0` placement conflict in §4. An alias raises
  the score **by construction** and the `none` control cannot catch a fabricated one; I will not
  buy 588 B with an untested transitive claim about two retail addresses.
* **Did not re-home** `??0?$hashtable@U?$pair@$$CBHH@…` from `0x825a07e0` to `0x8255c968`. It is
  the right next question (§4) but it is an 11-member shared group and a multi-unit A/B.
* **Did not run `tools/comdat_fold_gate.py` on this pair.** Our spelling is absent from
  `target_symbol_map.json` (0 entries for `??0?$hash_map@HH`), so a worklist row would have
  required me to invent a `base_addr`. I ran `--selftest --sample 200` instead to confirm the
  instrument is operative before quoting anything from it: **SELFTEST PASS, 0 defects**, with
  its documented map-unresolvable-branch vacuum disclosed at 87/210 readable bodies.
* **Did not touch `src/band3/net_band/RockCentral.cpp`.** The two `GetGigTypeCompletedMap()`
  sites at 1193-1194 are only in scope for the change I am declining to make.

## 8. For whoever takes the 588 B

It is collectable, but only after the callee is settled. The decision procedure:

1. Establish the true retail home of `??0?$hashtable@U?$pair@$$CBHH@…` — `0x8255c968` (implied by
   this lane) or `0x825a07e0` (the current alias placement). The fan-in in §4 is the starting
   evidence: `0x825a07e0`'s callers are not hash_map ctors.
2. If it is `0x8255c968`, correct the `0x825a07e0` group **first**, A/B the correction alone
   (expect movement in both directions across the 11 members), then add
   `??0?$hash_map@HH…::hash_map()` to the `0x8255d480` group, whose own evidence is already
   established here: retail constructs a provably `hash_map<int,int>` member with it, and our
   compiled body is identical to it modulo one relocated `bl`.
3. Do **not** shortcut step 1 by adding the parent group alone. That is the untested transitive
   claim, and it would forgive whichever of the two placements is wrong.
