# Lane W11-D — alias groups with no map-resident member: the class is real, but the vein it opens is a different one

**Branch** `w11-alias-unmapped` · worktree `~/tmp/wt-w11-d` · base `77cac933`.
Ruler: **`name_check` (graded)**, read from `report.json`'s `provenance.diff_config`.

Baseline after this lane's mandatory first full build (a reflinked worktree's
target objs are pre-renamer, so every mangled-name lookup reads "absent" until
the renamer's pre-compile step has run — FOLDPROVE-2). Sanity check asserted
**before** any name-keyed analysis: `collect(target)` indexes **69,414 symbols
over 3,083 objs** (main reads 69,415; a pre-renamer tree reads ~69,438 with
every mangled name missing).

```
matched_functions   42,766      masked_equal        22,986
matched_code     3,874,292 B    matched_code_percent 37.812890
total_code      10,245,956      fuzzy_match_percent  49.143230
verify_objs_patched --verify-manifest -> OK: 1205 decomp, 3083 target objects
icf_alias_finder --validate -> PASS, 1352 map-consistent, 241 tolerated, 0 contradicted, 1595 total
```

⚠ **A correction to this document, caught by its own closing gate.** The
baseline `matched_code` above first read **3,868,092 B** — a figure I *derived*
in prose rather than read from a leg. The final build's absolute
(3,876,796 B) minus this lane's two measured deltas (+2,504 B) gives
**3,874,292 B**, which is what leg A actually held. The measured deltas were
never affected; only the prose absolute was. *Deltas compose, absolutes do
not* — and the way that rule bites is exactly this: an absolute written down
from memory between two correct measurements.

Net for the lane: **+2,504 B / +16 functions**, across two changes, **each
predicted exactly before measurement**.

Closing state on the landed branch, after a full `./tools/ninja-locked`
(`EXIT=0`) following a `touch config/45410914/config.yml`:

```
matched_functions 42,782   masked_equal 22,986   matched_code 3,876,796 B
matched_code_percent 37.837330   fuzzy_match_percent 49.143272
total_code 10,245,956   total_functions 69,219

[patch-state] OK: 1205 decomp, 3083 target objects match 2026-09-13T06:36:41Z
              (tree_sha256=d3c2c6dfffa07e6b)
VALIDATE: PASS -- 1363 map-consistent, 238 tolerated (enumerated above),
              0 contradicted, 1603 total
```

⚠ **That absolute is a different tree from leg A** — it carries nothing but
this lane, but it must not be differenced against any other lane's baseline.
**The lane's contribution is the SUM OF ITS TWO MEASURED DELTAS.**

---

## 0. The headline

W10-B's H4 asked for an audit of "alias groups with no map-resident member",
sized by the one instance it hit by accident (**+116 B**). The class is real
and worth stating precisely. But the audit's yield did **not** come from
repairing groups. It came from turning the question around.

> ★★★★ **The group-side screen is the wrong unit of work. The CHARGE-side
> screen is the right one.** Enumerating groups with no map-resident member
> finds 130 rows, of which **2** are payable. Enumerating the *charged
> relocation-name pairs* that such a repair would close finds a **class** —
> 13 pairs / 21 sites — of which 12 were installable and worth **+2,504 B**.
> The group screen missed 11 of the 13 because it required our spelling to
> already **be** a group member; the most valuable ones were in no group at all.

| # | question | verdict |
|---|---|---|
| **H4** | the NOMAP class | **130 groups, fully decomposed (§1). Invisible to the validator's fatal check — a structural blind spot, §1.1.** |
| **H4′** | what the class is actually worth | **+2,504 B / +16 fns, installed and measured exactly (§3)** |
| **H1** | `0x827bb0e8` → 2-arg `PoolAlloc` | **NOT LANDED — and `PoolAlloc.h`'s stated reason for keeping the 5-arg form is STALE (§4)** |
| **H5** | `__destroy_aux<LevelData>` duplicate | **NOT a clear defect — both addresses are vendor-band, where `/Gy`-off makes duplicate instantiations legitimate (§5)** |
| **H6** | `RecursePatternInternal` 892 B | **REFUSED, and it is worth far more than 892 B — +3,084 B / +11 fns across 12 rows, blocked on a MAP question (§6)** |
| — | Char3D | **REFUSED (§6.1)** |

---

## 1. The NOMAP class, decomposed

130 of 1,595 groups have **no member** — survivor or folded — appearing
anywhere in `target_symbol_map.json`. They are not one thing:

| sub-class | n | verdict |
|---|---:|---|
| `vftable_*` placeholder survivors | 34 | **structurally exempt** — a FUNCTION map can never name a data symbol |
| `address: null` | 51 | **deliberate** — ALIAS-REPAIR's `UNDER_PARTITIONED_ICF_CLOSURE`; "no retail address is known for this class, so it renders into no map bucket" |
| real address, nothing anchoring it | 45 | the actual audit surface |

Of the 45, only **25** sit at an address the map names at all, and **21 of
those 25 declare `folded: []`** — empty husks left by ALIAS-CONSOLIDATION
2026-08-19's neutral withdrawal. **An alias group of one member forgives
nothing**, so the raw "25 groups violate the file's own invariant" figure is
dominated by inert rows. Only **4** both forgive something and sit at a named
address, and only **2** of those were payable.

⇒ **Do not price this class by group count.** Price it by charged sites.

### 1.1 ⚠ The class is invisible to the validator's only FATAL check

`classify_group` orders its tests so that

```
if len(named) > 1:      -> CONTRADICTED     (the only fatal class)
if not named:           -> UNWITNESSED      <-- short-circuits here
...
if survivor not in tmap: -> CONTRADICTED
```

Every one of the 130 is caught at `not named` (96) or at the placeholder test
(34), so the `survivor not in tmap` fatal branch is **never reached for them**.
Cross-tabulated exactly: **34 `PLACEHOLDER_SURVIVOR` + 96 `UNWITNESSED` = 130**.

⇒ **`0 contradicted` says nothing whatever about this class.** That is why C1's
defect survived until W10-B tripped over it, and it generalises the standing
warning that `--validate`'s PASS measures map-consistency, not folding.

---

## 2. What the audit actually found: a same-`T` fold class

Chasing the two payable groups from the charge side exposed the real unit of
work. Both were the same shape: retail's map names `_Param_Construct<T>` at an
address where our code emits `_Copy_Construct<T>` — **the same `T`**.

`src/system/stlport/stl/_construct.h` settles it at source level:

```cpp
template <class _Tp>            inline void _Copy_Construct (_Tp* __p, const _Tp& __val) { _STLP_PLACEMENT_NEW (__p) _Tp (__val); }
template <class _T1, class _T2> inline void _Param_Construct(_T1* __p, const _T2& __val) { _STLP_PLACEMENT_NEW (__p) _T1 (__val); }
```

For `_T1 == _T2 == _Tp` the bodies are **literally identical**, and so is the
relocation (that `T`'s copy constructor). That is `/OPT:ICF`'s fold condition —
identical *including relocations* — stated exactly. The mangling corroborates
the type identity independently: `??$_Param_Construct@VCacheDirEntry@@V1@@…`,
where `V1@` is a back-reference making `_T2` the same class.

**The precise same-`T` test is mangling-grounded**: for both spellings the
signature after `@stlpmtx_std@@` is byte-identical iff `T` matches
(`YAXPAVCacheDirEntry@@ABV1@@Z`), and differs otherwise. Applied to the charged
population: **13 same-`T` pairs / 21 sites charged, in BOTH directions**
(retail sometimes keeps `_Param_`, sometimes `_Copy_`).

### 2.1 The control is the load-bearing part

Adjudicated with `tools/icf_pair_adjudicate.py`: **13/13 FLAT T1 PROVEN** —
60 B both sides, 1 relocation, `reloc_tally {}`.

> ★★★★ A unanimous 13/13 positive is the exact shape FOLDPROVE-2 warns about,
> and this family is *built* to produce false positives: its masked body is
> shared **112 ways on the retail side and 270 ways on ours**. Without a
> control, "PROVEN" here would be the instrument agreeing with itself.

So 13 **cross-`T` decoys** were constructed from the same family (each retail
name paired with the next pair's our-name; verified 0 accidentally same-`T`)
and run through the same instrument:

| leg | result |
|---|---|
| 13 real same-`T` pairs | **13 PROVEN, 0 REFUTED** |
| 13 cross-`T` decoys | **13 REFUTED, 0 PROVEN** — *"masked bodies match but relocation TARGETS disagree — template-twin, not a fold"* |

⇒ the verdict rests on the **per-`T` copy-constructor relocation target**, not
on the family's shared shape. `--selftest` also passed (positive PROVEN,
negative REFUTED) before any of this was believed.

---

## 3. Per-change: predicted vs measured

Both predictions were computed from **`report.json`'s charge lists**, never
from a mismatch count (RESIDUAL-1), and pre-registered before the run.

| # | change | predicted | measured | |
|---|---|---|---|---|
| A1 | install the 6 pairs needing **no removal** (4 clean creations, 1 folded-addition, 1 survivor promotion) | **+712 B / +5 fns** | **+712 B / +5 fns**, Δcode% +0.006950pp | ✓ exact |
| A2 | **relocate** 6 proven spellings out of groups that place them elsewhere | **+1,792 B / +11 fns** | **+1,792 B / +11 fns**, Δcode% +0.017490pp | ✓ exact |

Attribution on both runs: `unit net (ALL units) == whole-binary Δmatched`
(+5 and +11), and **0 units fell off 100%** on either ruler in either run.

The pricing screen reproduced to the digit on every row, which is what made the
predictions exact: a 96 B row at fuzzy 99.79166 is `0.20834 / (5/24) = 1.0`
charge; a 328 B row at 99.939026 is exactly 1; the 328 B row at 99.81707 is
exactly 3. **The three rows predicted NOT to cross did not cross** —
`default/CharEyes`, `default/DrivenPropertyEntry` and `default/Sfx` each carry
residual `__uninitialized_copy` charges of a *different* kind (retail `PBV…`
const-pointer vs our `PAV…`), which is a separate, unrelated defect and a
handoff.

### 3.1 Why A2 needed removals at all, and what that revealed

`gen_symbol_alias_map.py` emits **one line per symbol per group**, and
`parse_msvc_map` groups whatever shares an address. A name present in two
groups at two addresses would therefore render at both and could **silently
merge two equivalence classes**. So a proven relocation is not "add here", it
is "move".

> ★★★★ **Every wrong-address group involved is itself UNANCHORED** — its
> survivor is absent from `target_symbol_map.json`. Misplaced spellings
> accumulate exactly where nothing anchors them. That, more than the byte
> count, is the substantive answer to H4: the NOMAP class is not primarily a
> set of groups that fail to forgive, it is **the sink that wrong placements
> drain into.**

Each removal was adjudicated **individually** (W9-D got 69 of 74 blind
withdrawals wrong; W10-D's one-at-a-time pass protected 87.1% of the exposure).
In 6 of 7 the moved spelling was the old group's *survivor*, and in **all** of
them the membership forgave **0 charged sites** — verified against the charge
census before the move, not assumed. Where a survivor was removed a
map-resident member was promoted in its place. **No spelling was pruned
anywhere**, in keeping with the rule that pruning zero-forgiving spellings once
cost +94,616 B to reverse.

---

## 4. H1 — not landed, and its stated justification is stale

`0x827bb0e8` → `?PoolAlloc@@YAPAXHH@Z` was handed over as accuracy-only, Δ0,
"land it anyway". Two findings change its shape, and neither is a reason to
skip it — they are reasons it is **not the one-line map edit it was briefed as**:

1. ⛔ **A map rename alone would UNPAIR the row.** `PoolAlloc.cpp:46` defines
   the 5-arg form **unconditionally**, and the row pairs at 100 today under
   that name. Renaming the map without touching source removes the name the
   base obj defines — CLAUDE.md's "proving a name wrong ≠ renaming is SAFE".
2. ⛔ **`PoolAlloc.h`'s own justification for keeping the 5-arg overload is
   STALE.** It reads *"hand-written debug call sites in synth360, e.g.
   StreamReceiver360/Voice, keep the 5-arg form … so we leave the 5-arg
   overload."* Measured on the current tree: **every** 5-arg `PoolAlloc` call
   site in `src/` is inside `#ifdef HX_NATIVE` (`ObjPtr_p.h:846`,
   `MemMgr.cpp:235`, `PoolAlloc.h:94`), and `StreamReceiver360.cpp:116` is now
   a *comment recording that spelling's removal*. In the match build the 5-arg
   definition has **zero callers**.

⇒ H1 is a coupled **map + source + alias-survivor** edit (the founding
`PoolAlloc` alias group's survivor is the 5-arg name and would stop being
map-resident), and it touches `src/`, so it needs the native gate. Deliberately
not attempted at the end of a lane: the map/alias coupling defects on `main`
required **three** consecutive repairs, and a rushed fourth is how a fifth gets
made. **Handoff, with the analysis done.**

---

## 5. H5 — the duplicate is probably not a defect

`??$__destroy_aux@ULevelData@@…` is mapped at `0x82b5b1d0` **and**
`0x82b63ec8` and is not on `_internal_linkage_allow`. A whole-map injectivity
sweep confirms exactly **2** duplicated names (the other, `?NodeCmp@@YAHPBX0@Z`,
is allowlisted and legitimate).

⚠ But **both** `LevelData` addresses are **≥ `0x82A00000`** — the vendor band,
where CLAUDE.md records folding is **17× weaker**, most likely `/Gy`-off
monolithic non-COMDAT `.text`. With `/Gy` off, two copies of one template
instantiation **legitimately survive at two addresses**, and the map naming
both is then *correct*, not a defect. `_internal_linkage_allow` is the wrong
home for it either way — that key is for file-static symbols, and a template
instantiation is not one.

⇒ **No action taken.** The allowlist may need a distinct "vendor-band duplicate
survivor" category, which is a decision about the map's schema, not a repair.

---

## 6. H6 — REFUSED, and it is the largest thing this lane found

`?RecursePatternInternal@@YAXPBDP6AX00@Z_N2@Z` (892 B, `default/File`,
fuzzy 99.97758) was briefed as "behind one relocation name". Both halves of
that are worth correcting.

**The charge count.** A raw slot-wise scan shows **15** name disagreements on
that row. Fourteen have a **placeholder** on the retail side (`lbl_82087E4C`,
`fn_82516550`, …), and `name_check` **forgives placeholder targets**. Exactly
**one** charge has a real name on both sides — which the pricing screen
confirms independently (`5/223 = 0.02242 pp`, and `100 − 99.97758 = 0.02242`).
This is the standing "CHARGE COUNTING CLOSES OPEN ROWS" trap reproducing
cleanly: *filter placeholders before counting.*

**The prize is not 892 B.** That single pair —
retail `??1?$vector@UNode@?$ObjPtrVec@VObject@Hmx@@VObjectDir@@…@@QAA@XZ`
vs ours `??1?$vector@VString@@…@@QAA@XZ` — is charged on **12 rows**:

```
+168 AssetMgr/StripFinish      +80 BandSongMgr/ClearAndShrink<String>
+396 LessonMgr/GetDifficulty  +108 MusicLibraryNetSetlists/~NetSavedSetlist
+368 VoiceoverPanel/SetVoiceoverSymbol   +120 CharLipSync/~CharLipSync
+192 PreloadPanel/~PreloadPanel          +228 MidiReader/~MidiReader
+204 Archive/~Archive                    +328 SongInfoCopy/~SongInfoCopy
+892 File/RecursePatternInternal
 (PrefabMgr 1,188 B has 7 charges of which 4 are this pair -> does NOT cross)
                                    total  +3,084 B / +11 fns
```

**The fold is chase-proven.** Flat T1 REFUTES ("relocation TARGETS disagree"),
but that refutation is a *naming* artifact: the differing targets are
`__destroy_range_aux<reverse_iterator<ObjDirPtr<ObjectDir>*>>` vs
`<reverse_iterator<String*>>`, and `--chase` recursively verifies **those**
fold → `CHASED T1: PROVEN`. The control ran too: `--chasetest`'s in-family
decoy is still `CHASED T1: REFUTED`, so `--chase` is not a rubber stamp.

Three further pieces of retail evidence all point the same way:

* our `~vector<String>` is **136 B with relocation offsets 0x48 / 0x6c** —
  matching retail@`0x822d8cc0` exactly;
* our `~vector<ObjPtrVec::Node>` is **132 B**, so it is **not** the body at
  `0x822d8cc0` despite wearing that name there;
* `0x822d8cc0` has **114 retail `bl` callers** — the profile of `~vector<String>`,
  not of a `vector<ObjPtrVec<Object,ObjectDir>::Node>` destructor.

### ⛔ So why refuse?

Because `??1?$vector@VString@@…` is **itself map-resident, at `0x82b74600`**.
Installing the alias would put **two target-obj-named members in one group** —
`classify_group`'s single FATAL class — and the validator would be *right*: the
map does place them at two addresses.

The only way through is a `contradiction_exempt`, and `symbol_aliases.json`
requires such an exemption to be licensed by *"a retail bl-caller census (an
address with 0 call sites in the whole image cannot be the callee our
relocation denotes)"* or a prior measured adjudication. **The census was run
and it REFUSES the license:**

```
0x822d8cc0  114 bl callers      0x82b74600    3 bl callers
0x824e1f68    4 bl callers      0x82b9b590    4 bl callers
```

`0x82b74600` is a **real, called function** — not the a745039e uncallable-junk
class. Its body has **three** relocations to our two, the first destroying a
`vector<vector<float>>`, and its report row sits at **fuzzy 46.2** — so it is
plainly not `~vector<String>`. But "not that" is not "is this", and the correct
repair is therefore a **map** edit (identify `0x82b74600`, re-home
`~vector<String>`), not an alias exemption. That is a map lane's work and it
must be measured on its own.

> ★★★ **A refusal is not the absence of a result here.** The instrument that
> stopped this is the same one W10-B's injectivity check is: it fired on a
> repair I had already assembled the evidence for, and the licensing standard —
> not my confidence — is what settled it.

### 6.1 Char3D — REFUSED for the same reason, and a map row proven wrong

The 13th same-`T` pair, `_Param_Construct<Char3D@CharData@WorldCrowd>` ↔
`_Copy_Construct<Char3D@…>` (worth +192 B / +2 fns in `default/Crowd`), is
blocked identically: our spelling is map-resident at `0x82b9b590` while its
proven home is `0x824e1f68`.

Retail bytes settle **which** row is wrong, though not what it should say:

* `0x824e1f68` relocates to `??0Char3D@CharData@WorldCrowd@@QAA@ABU012@@Z` ✓
* `0x82b9b590` relocates to **`??0Entry@LocalePanel@@QAA@ABU01@@Z`** — it
  constructs a *LocalePanel::Entry*, not a Char3D.

So retail has **one** Char3D construct helper and `0x82b9b590` is a
**misidentified map row**; the fold is not refuted, the map is wrong. Our
`_Copy_Construct<Entry@LocalePanel>` is 60 B with its relocation at offset 36
to that same ctor — a byte-and-relocation match, and our base obj defines the
name, so a rename would pass the "base obj must define it" test.

**Still refused**, because identifying `0x82b9b590` means reading the callee's
name out of *the very map under audit*, and there is a competing home: retail's
LocalePanel::Entry helper is also referenced as `fn_824ADBD0` from the Entry
vector code. Two candidate homes, resolved only by adjudicating a second map
row. Not settled ⇒ not installed.

### 6.2 ★ The cross-cutting finding: vendor-band map rows for Milo templates

Four blockers in this lane are the same shape, and all four sit **above
`0x82A00000`**:

| address | map says | reality |
|---|---|---|
| `0x82b9b590` | `_Param_Construct<Char3D@CharData@WorldCrowd>` | constructs `LocalePanel::Entry` |
| `0x82b74600` | `~vector<String>` | 3 relocs vs our 2; destroys `vector<vector<float>>`; row at fuzzy 46.2 |
| `0x82b5b1d0` + `0x82b63ec8` | `__destroy_aux<LevelData>` ×2 | duplicate (possibly legitimate, §5) |
| `0x82ba3298`, `0x82b9b590` | unanchored alias-group addresses | survivors absent from the map |

Only **4 of 118** STLport `_Construct` helper map rows sit in the vendor band —
a 3.4% outlier population, and this lane hit defects in a large share of what
it touched there. ⇒ **A vendor-band map row naming a Milo STLport template
instantiation is a suspect row.** That is a cheap, testable screen for a map
lane and it is the highest-leverage handoff here.

---

## 7. What this lane did NOT do

* ⛔ **Did not touch `src/`** — both changes are `scripts/symbol_aliases.json`
  only, so `tools/native_build_gate.sh` is **not applicable** (0 files under
  `src/` in the branch diff).
* ⛔ **Did not install H6** (+3,084 B / +11 fns) — §6. Blocked on a map
  question and refused the exemption its own licensing standard denies.
* ⛔ **Did not install the Char3D fold** (+192 B / +2 fns) — §6.1.
* ⛔ **Did not rename or delete `0x82b9b590` or `0x82b74600`.** Both are proven
  *wrong*; neither is proven *what*.
* ⛔ **Did not land H1** — §4; it is a coupled map + source + alias edit needing
  the native gate, not the Δ0 one-liner it was briefed as.
* ⛔ **Did not touch H5** — §5; probably not a defect.
* ⛔ **Did not prune anything.** No spelling was removed from the file; the 6
  relocations moved spellings between groups. `STALE_SPELLING` (85) and
  `UNWITNESSED` (96) are untouched, including `_Copy_Construct<DebugGraph@
  ?A0xfa5cc2c6>`, which adjudicates **UNDECIDABLE — "our spelling is in no
  compiled obj"** and therefore forgives 0 today.
* ⛔ **Did not withdraw the individually-REFUTED `_Param_Construct<InlinedDir@
  ObjectDir>` from anywhere** — it was *relocated* to `0x82750120`, where it is
  PROVEN, rather than deleted.
* ⛔ **Did not re-litigate the 21 empty (`folded: []`) NOMAP groups.** They
  forgive 0 and re-seeding them would be re-asserting the very fold claims
  ALIAS-CONSOLIDATION withdrew as fabricated.
* ⛔ **Did not run the permuter** (standing directive: OFF).

---

## 8. Handoffs

| # | handoff | evidence in hand |
|---|---|---|
| **H6′** | **`~vector<String>` is at `0x822d8cc0`, not `0x82b74600` — worth +3,084 B / +11 fns across 12 rows.** Needs a MAP repair (identify `0x82b74600`; it looks like a `vector<vector<float>>`-family dtor), after which the alias installs cleanly. | §6: chase-proven, 114-vs-3 caller census, 136 B / 0x48+0x6c reloc-offset match, target row at fuzzy 46.2 |
| **H4′** | **Identify `0x82b9b590`** (constructs `LocalePanel::Entry`; competing home `fn_824ADBD0`), then install the Char3D fold. | §6.1 — +192 B / +2 fns |
| **VB-1** | ★ **Screen every vendor-band (`≥0x82A00000`) map row naming a Milo STLport template.** 4/118 `_Construct` rows are there and this lane found defects in most of what it touched. | §6.2 |
| **H1′** | `0x827bb0e8` → 2-arg `PoolAlloc` is a **coupled map + source + alias-survivor** edit, not a map one-liner; `PoolAlloc.h`'s reason for keeping the 5-arg form is stale (all 5-arg sites are `HX_NATIVE`-only). Needs the native gate. | §4 |
| **UC-1** | The `__uninitialized_copy` **`PBV…` vs `PAV…`** (const vs non-const pointer) charge class is what keeps `default/CharEyes`, `default/Sfx` and `default/DrivenPropertyEntry` off 100 after this lane. A source-side const-correctness question, not an alias one. | §3 |
| **H5′** | The `__destroy_aux<LevelData>` duplicate is probably legitimate (`/Gy`-off vendor band). If so the map schema needs a duplicate-survivor category; `_internal_linkage_allow` is the wrong key. | §5 |

---

## 9. Reusable lessons

* ★★★★★ **Screen on the CHARGE, not on the artifact you were asked to audit.**
  "Groups with no map-resident member" found 130 rows and 2 payable ones;
  "charged pairs a repair would close" found a 13-member class worth 12× more.
  The group screen structurally could not see the best candidates, because it
  required our spelling to already be a member — and the valuable ones were in
  no group at all. **When an audit of X yields little, ask what X was supposed
  to buy and enumerate *that* directly.**
* ★★★★★ **A unanimous positive demands a decoy built from the same family.**
  13/13 PROVEN is worthless on a family whose masked body is shared 112/270
  ways; 13/13 cross-`T` REFUTED is what makes it evidence. The generic
  `--selftest` does **not** substitute — it proves the tool can fail
  *somewhere*, not that it can fail *here*.
* ★★★★ **A validator's ordering can hide a whole class from its own fatal
  check.** `not named` short-circuits before `survivor not in tmap`, so 130
  groups can never be CONTRADICTED however wrong they are. Read the classifier,
  not the summary line.
* ★★★★ **Unanchored groups are a SINK, not just a gap.** Every wrong-address
  placement this lane relocated came out of a group whose survivor is absent
  from the map. A group with no anchor cannot be checked, so errors migrate
  into it and stay.
* ★★★★ **Filter placeholder targets before counting charges.** H6 reads as 15
  charges and is 1; `name_check` forgives `fn_`/`lbl_`/… targets. The pp
  arithmetic (`5/N` per relocation-name charge) is an independent cross-check
  and agreed to the digit on every row in this lane — use both.
* ★★★★ **An exemption's licensing standard is a real gate — let it refuse you.**
  H6 had a chase proof, a size match, a reloc-offset match and a 114-vs-3
  caller ratio, and still failed the one test the file demands. The census was
  run *hoping* to license the install and returned the opposite; that is the
  only reason this lane did not ship an unproven alias worth +3,084 B.
* ★★★ **A flat T1 REFUTED on a template family may be a NAMING artifact.**
  `--chase` exists for exactly that, and it flipped H6 from REFUTED to PROVEN
  via a callee pair that itself folds. But run `--chasetest` alongside it: the
  in-family decoy must still refute.
* ★★★ **Two spellings of one source-identical helper is a whole class, not a
  one-off.** `_Param_Construct<T>` / `_Copy_Construct<T>` differ only in a
  template parameter list; for `T1==T2` STLport gives them the same body and
  the same relocation. Look for other such pairs (`_Destroy` / `_Destroy_Range`,
  `__uninitialized_copy` / `__uninitialized_copy_n`) before treating any of
  them as individual defects.
* ⚠ **`none` FLAT + `ALIAS_SUSPECT` fired on both landed changes.** Expected on
  any map-only patch and it licenses nothing in either direction: `none`
  ignores relocation names, so a fabricated alias produces the identical shape.
  The license is always the retail-byte adjudication.

---

## 10. Post-lane composition check

`main` advanced **`77cac933` → `a27b176a`** while this lane ran, and one of the
new commits (`d40af4fe`, W11-A's `list<T*>::insert` fold) edits
**`scripts/symbol_aliases.json`** — this lane's only functional file — while
four others edit `scripts/target_symbol_map.json`, which every screen here keys
on. W10-C's rule applies: *a clean textual rebase is not evidence a result
survived composition.* So it was checked semantically, not textually:

| check | result |
|---|---|
| my 13 same-`T` pairs already aliased on `main` | **0** — no semantic overlap with W11-A |
| the 13 map addresses this analysis keys on, changed on `main` | **0** — no drift |
| group count on `main` | 1,595 (= this lane's base); this branch is 1,603 |

⇒ the two landed changes are **independent of everything that landed
underneath them**, and their measured deltas should compose. They have **not**
been re-measured on `a27b176a`; the lane's contribution remains the sum of its
two in-run deltas.

⚠ `git diff --stat main..HEAD` on this branch lists **30 files**, including
deletions of other lanes' documents. That is the artifact W10-B §9 records —
`main` having advanced, not a real footprint. Against the merge-base the true
footprint is **2 files**:

```
docs/decomp/ALIAS_UNMAPPED_2026-09-13.md | 472 +++++
scripts/symbol_aliases.json              | 143 ++++--
```

**Always take the footprint against `git merge-base main HEAD`.**
