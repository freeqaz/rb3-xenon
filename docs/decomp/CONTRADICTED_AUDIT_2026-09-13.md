# Adjudicating the 104 live CONTRADICTED alias memberships — lane W10-D, 2026-09-13

Branch `w10-contradicted`, worktree `~/tmp/wt-w10-d`, based on main `dee126a1`
(`git merge-base --is-ancestor dee126a1 HEAD` asserted before the first edit).

This lane executes lane W9-D's handoff **#1**
(`docs/decomp/ALIAS_HYGIENE_2026-09-13.md` §11): *104 CONTRADICTED memberships
are still LIVE ledger-wide, carrying no withdrawal at all, so nothing currently
opposes them.* W9-D's instruction was explicit and is the shape of this lane:
**do not bulk-withdraw** — in W9-D's own scope a sweep got **69 of 74**
withdrawals wrong, and a bulk action here would have repeated that at 35× scale.

**Headline: 89 FABRICATED / 7 REAL / 8 UNPROVABLE. Enacting only the 89 costs
−696 B where the blind sweep would have cost −5,384 B — adjudication protects
4,688 B, 87.1% of the exposure.**

| | memberships | Δmatched_code | Δmatched_functions | rows |
|---|---:|---:|---:|---:|
| all 104 withdrawn **blind** | 104 | **−5,384 B** | −22 | 22 |
| …**FABRICATED** ⇒ enacted here | 89 | **−696 B** | −7 | 7 |
| …**REAL** ⇒ kept, positive evidence | 7 | +0 (4,136 B protected) | — | 13 |
| …**UNPROVABLE** ⇒ kept, declared | 8 | +0 (552 B protected) | — | 2 |

The three classes sum to the blind total **exactly** (696 + 552 + 4,136 =
5,384). That is a real check, not bookkeeping: rows can be charged by more than
one alias pair, so the parts were not guaranteed to add up.

Baseline after this lane's **first full build** (mandatory — a reflinked
worktree's target objs are pre-renamer, so every mangled-name lookup reads
"absent" until it has been built once, and any negative taken before that is
vacuous):

```
./tools/ninja-locked                                       EXIT=0
python3 scripts/verify_objs_patched.py --verify-manifest
  [patch-state] OK: 1205 decomp, 3085 target objects match
  tree_sha256=8ec7b2e363e2b058                             rc=0
report.json  matched_functions 42,749 / matched_code 3,871,308 / 37.783768%
```

★ That baseline **reconciles with W9-D's close on both keys** through the merge
that landed between us. W9-D finished at 42,733 / 3,864,908; merge `88c944d4`'s
ledger line records **+16 fns / +6,400 B from one row**; 42,733 + 16 = **42,749**
and 3,864,908 + 6,400 = **3,871,308**. Two independently measured absolutes
agreeing through a third recorded delta.

## 1. The briefed figures, tested literally

| briefed | measured here | verdict |
|---|---|---|
| 104 live CONTRADICTED memberships | **104**, and **set-identical** to `~/tmp/w9d/live_contradicted.json` (0 only-mine, 0 only-theirs) | ✅ exact |
| 535 live `NEEDS_SOURCE` | **535** | ✅ exact |
| 107 live `NEEDS_MAP_ID` | **107** | ✅ exact |
| `icf_alias_finder --validate` reports 0 CONTRADICTED on the same tree | **0 contradicted, PASS** | ✅ exact |
| FOLDPROVE-2's "57 retail call sites reach `0x82bab758`" | **39 distinct calling bodies** | ⚠ *different unit* — see below |

Re-derived from the shipped `scripts/symbol_aliases.json` with
`tools/alias_membership_adjudicate.py`, anti-vacuity gate passed (**27,195**
mangled names among 69,414 target bodies). Ledger state on arrival matched
W9-D's closing figures exactly: 1,595 groups / 5,342 live / 9,993 withdrawn /
71 restored.

★ **This is the first briefed population in several waves to survive literal
re-testing** — W9-D found a briefed "8" was 11 and a briefed "563 withdrawals"
was the wrong unit entirely. It was still re-derived rather than inherited,
because that is the only way the agreement is worth anything.

⚠ The one non-match is **not a refutation**: I counted distinct retail *bodies*
carrying a relocation to the survivor, FOLDPROVE-2 counted *call sites*. A body
with two calls counts once for me and twice for it. Different units, exactly the
error class W9-D's own "563 withdrawals" correction is about — so it is recorded
as incomparable, not as a correction.

## 2. The instrument gap that makes this lane necessary

`icf_alias_finder --validate` returns **PASS — 0 contradicted** on the very same
tree that carries these 104. That is not a contradiction between tools; it is the
`OK (grounded)` → `OK (MAP-CONSISTENT)` lesson with a current number. The
validator measures **map-consistency** — is the survivor map-resident, is every
spelling referenced — and it is *provably insensitive* to the claim under test:
a group emptied of every folded spelling still classifies OK.

⇒ **The standing green gate is not evidence about these 104, before or after
this lane.** It was green before the audit and it is green after it. Anyone
reading `VALIDATE: PASS` as "the alias ledger is proven" is reading a
coverage statistic as a proof.

## 3. Structure: 104 memberships, but only 15 groups

The population is far more concentrated than a membership count suggests, which
is what makes individual adjudication tractable at all:

- **15 distinct groups**, 15 distinct addresses, **0 with `address: null`** (so
  none is in the class the map renderer skips outright — the exposure is real).
- Two `$_Param_Construct` groups (`612`, `638`) hold **80 of the 104**, 40 each.
- The remaining 24 are spread over 13 groups, 9 of which hold exactly one.

## 4. The instrument

W7-D's rule — **a family folds iff its relocation targets are type-independent**
— applied **one level down**, on **COMDAT bytes**.

For a membership (survivor `S`, folded `F`), the adjudicator compares retail's
body at `S` against our compiled body at `F` (retail has no separate body for a
folded spelling — that is what folding means). `CONTRADICTED` means those differ
in a resolved, non-placeholder way. When the difference is a relocation **target
name** pair `(rn, on)`, the parent fold is possible only if `rn` and `on`
themselves folded. So the question becomes decidable one level down:

| callee-level finding | consequence |
|---|---|
| our two callee COMDATs differ in **size** | they cannot be one COMDAT ⇒ parent fold **foreclosed** |
| our two callee COMDATs are **byte+reloc identical** | `/OPT:ICF` **must** fold them ⇒ parent fold **supported** |
| same size, differing relocation count/offsets/targets | genuinely different code ⇒ **foreclosed** |
| we do not compile one of them | **unprovable** |

⛔ **This is deliberately NOT the fixpoint chase.** Name equality closed over the
very fold class under test is the circularity `FIXPOINT_ROOT_DIFFERS` is named
for, and it is the reason W9-D refused the 4 `L2_RECURSIVE` memberships and
W41-CONTRA's `0x8233c668`. COMDAT *bytes* are the ground truth `/OPT:ICF`
actually consumes, so grounding one level down on bytes is not a chase.

⛔ **Callee absence is never read as evidence of folding.** That model is
refuted on record: the map names 41.7% of functions, and "callee absent from map
⇒ fold-alias" carries an enrichment of only ~1.95×. Absent ⇒ UNPROVABLE.

### 4.1 Validating it — against an instrument that cannot share its bias

A new instrument that confirms the prior it was built to test is this project's
most repeated failure mode, so the byte instrument was cross-tabulated against a
**source-independent** one: `DISTINCT_ADDRESSES_CANNOT_FOLD` applied to the
callees. If `target_symbol_map.json` places both callees at distinct retail
addresses, they provably did not fold — and that rests on the map's own address
assignment, not on our source and not on the fold class under test.

| source-INDEPENDENT (map addresses) | source-DEPENDENT (callee COMDAT bytes) | n |
|---|---|---:|
| `CALLEES_DISTINCT_ADDR` | `CALLEE_SIZE_DIFFERS` | **7** |
| `CALLEE_UNMAPPED` | `CALLEE_SIZE_DIFFERS` | 69 |
| `CALLEE_UNMAPPED` | `CALLEE_BYTES_IDENT` | 7 |
| `CALLEE_UNMAPPED` | `CALLEE_ABSENT` | 6 |
| `CALLEE_UNMAPPED` | `CALLEE_BYTES_DIFFER` | 6 |
| `SIZE_DIFFERS` (parent) | `HARD_SIZE` | 5 |
| `WORD_DIFFERS` (parent) | `NONRELOC_WORD` | 4 |

**On every membership where a source-independent answer exists, the two
instruments agree: 7/7, zero conflicts**, and **zero** of the `CALLEE_BYTES_IDENT`
class is map-contradicted. The map is decisive on only 7 because coverage is
41.7% and these per-`T` constructors are mostly unidentified — which is precisely
why the byte instrument is needed, and why validating it on the 7 where both
speak is what licenses extending it to the 69 where only one does.

⚠ **Honest bound on that validation.** It is 7 memberships, and it validates only
the **foreclosure** direction (both instruments saying "no fold"). It does *not*
validate `CALLEE_BYTES_IDENT ⇒ REAL`, because the map is silent on all 7 of
those. That direction rests instead on `L4_OURSIDE`, an already-licensed layer
with a measured 0.05% decoy false-positive rate, plus the independent witnesses
in §6.

### 4.2 The within-group control, and why it is weak

The same test was run on the members of the same 15 groups that the adjudicator
did *not* call CONTRADICTED. `CALLEE_SIZE_DIFFERS` fired **0 times**. But the
control population is only 18 members and 16 of them are `PARENT_ABSENT`
(untestable), so this is reported as **weak corroboration, not a discriminating
control**. The cross-tabulation in §4.1 is the control that carries weight.

## 5. Two alarms, checked rather than assumed — both cleared

Neither of these fired, and both are recorded because a check that clears is
still evidence, and because the next lane should not re-run them.

**(a) The STLPORT-1 EH-prefix reader artifact.** 22 of the 76 callee-size gaps
are **exactly ±8** — the artifact's signature, where `coff_bodies_ext.py` bills a
successor's 8-byte EH prefix into the preceding COMDAT. Checked directly on the
bytes: the bodies begin `7d8802a6` (`mflr r12`) and end `4e800020` (`blr`),
neither is the other ±8 (`big[8:] == small` and `big[:-8] == small` both False),
and both carry 9 relocations. These are genuinely different-sized bodies
(`ObjPtr<BandTrack>::ObjPtr(const&)` 136 B vs
`ObjPtrVec<FlowLabel,ObjectDir>::Node::Node(const&)` 144 B). **Not the artifact.**
Note also the artifact's sign is wrong for this population: it inflates *our*
side, and the reader here carries both the 2026-08-16 fix and its 2026-08-17
correction.

**(b) Group 1586's "size 68 vs 60".** +8 by arithmetic, so it entered the same
alarm. It is not a size story at all: **3 relocations vs 1**, and retail's body
calls a `vector<pair<int,float>>` copy constructor through `__savegprlr_29`
where ours calls `Frame@RhythmDetector`'s. Decisively different.

## 6. The 7 REAL — the CONTRADICTED record is wrong

Both groups' callees are **byte+reloc identical in our build**, so the linker
must fold them; and both carry an independent witness the adjudicator is
structurally blind to, because it inspects **callees within a body and never
callers**.

**Group 1577** (`0x82bab758`, FOLDPROVE-2, `vector<4-byte T>::operator=`, 6
memberships). Verified here rather than inherited: **39 distinct retail bodies**
relocate to that one address, and their own names carry **six different element
types** — `vector<TrackChannels>`, `vector<int>`, `vector<float>`,
`vector<UIScreen*>`, `vector<RemoteBandUser*>`,
`vector<pair<vector<int>,…>>`. One retail address serving six instantiations is
proof by internal inconsistency lifted to the caller level, and it needs no fold
model. Callers include `??4SongFilter@SongSortMgr@@`, `?Copy@BandCrowdMeter@@`,
`?Copy@CamShot@@`, `??4Stats@@`.

**Group 1582** (`0x82441658`, W39-PUSHBACK, `vector<Vector2>::push_back`, 1
membership). 38 distinct retail calling bodies. Its own recorded evidence decodes
a retail call site (`Player::LocalSetEnabledState` @ VA `0x826a8250` →
`bl 0x82441658`). ★ And the body is *self*-witnessing: under a survivor spelled
`vector<Vector2>`, retail's body calls
`?_M_insert_overflow_aux@?$vector@V?$Key@M@@…` — it names a **second**
instantiation (`Key<float>`), which is the same internal-inconsistency proof
inside a single body.

Reversing a CONTRADICTED record is a live option W9-D exercised 65 times. Here
no reversal is needed: these 7 carry **no withdrawal record**, so leaving them
live already records the correct outcome. Nothing was edited for them.

★ These 7 forgive **4,136 B / 13 rows**, measured directly — including
`?LocalSetEnabledState@Player@@` (1,432 B) and `??0SongInfoCopy@@` (856 B). A
blind sweep would have destroyed all of it.

## 7. The 89 FABRICATED — enacted

See the per-group table in §9. The dominant shape, 76 of the 89, is the textbook
W7-D foreclosure: **`_Copy_Construct<T>` / `_Param_Construct<T>` calls `T`'s own
copy constructor**, a per-`T` relocation target by definition. Group 612's retail
body calls `??0CacheDirEntry@@QAA@ABV0@@Z` at +0x24 for all 40; each folded
spelling's body calls its own `T`'s constructor. For one body to serve both, the
two constructors would have to be at one address — and their COMDATs differ in
size (gaps of +60, +44, +156, −52, −80, −360 B; 54 of 76 are ≥16 B).

⚠ The original T1 evidence on these groups is **masked-T1**: it asserts identity
"modulo relocated fields", i.e. it masked the very relocation that differs. The
tool's own docstring warns that T1's vacuity guard "is right as a guard and wrong
as a verdict". The contradiction is measured on **unmasked target names**, so it
outranks the evidence that installed the group.

Four smaller shapes, each individually inspected:

- **Group 593** (`??0NgMat`, 3 of 5). Every membership differs at exactly one
  slot, +0x14: the **base-class** constructor. Retail names `??0RndMat@@IAA@XZ`;
  the folded spellings name `??0BaseDisplacementNode`, `??0FilterVersion` (×2).
  Five unrelated classes' constructors cannot share one body.
- **Group 450** (2). Same size, same structure, but retail's
  `__uninitialized_copy<pair<float,float>>` copies with `lfs/stfs` and our
  `<Vector2>` / `<XUSER_ACHIEVEMENT>` copy with `lwz/stw` — four consecutive
  **non-relocated** words differ. Different instructions cannot be one COMDAT.
- **Group 786** (1). Two adjustor thunks, `$4PPPPPPPM@EM@` vs `$4PPPPPPPM@EE@`,
  reaching **different virtual functions** (`?DataDir@UIPanel@@UAA…` vs
  `?Load@UIPanel@@UAA…`). A thunk's entire information content is its adjustment
  and destination.
- **Group 1353** (1). Identical code, one immediate apart:
  `li r11,1; stb r11,0x10(r3); blr` vs `li r11,1; stb r11,0xc(r3); blr`.
- **Groups 1584 / 1585 / 1586** (4). Parent bodies 96 vs 64, 96 vs 48, 68 vs 60.
- **Group 612's 5 same-size callees.** `??0CacheDirEntry` (84 B, 2 rel:
  `??0String` @0x1c + `memcpy` @0x2c) against callees whose relocations sit at
  *different offsets* (`memcpy` @0x34), or number 1 instead of 2, or name
  `??0ObjPtr<RndTransformable>` / `??0vector<int>`. Different code, not different
  names.

## 8. The 8 UNPROVABLE — kept, and priced

W8-D's distinction between REFUTED and UNPROVABLE is the honest one, and it is
applied here rather than resolved by preference.

**Six are genuinely unknowable and forgive exactly 0 B — measured, not assumed**
(`MINUS_ABSENT6: DELTA +0 B / +0 fns / 0 rows, RECONCILES`). We do not compile
one of the two callees (`??0Protocol@Quazal@@QAA@I@Z`,
`??0ID3DXInclude@@QAA@XZ`, `??0BandPatchMesh@@QAA@ABV0@@Z`), so no retail-byte
evidence can license *or* refute them. Keeping them is therefore **free**, and
CLAUDE.md's standing prohibition applies directly: do not prune classes that
currently forgive 0, because they become live as porting advances — a prior such
prune cost **+94,616 B** to reverse.

**Two forgive the entire remaining 552 B, and both carry positive partial byte
evidence** — so the exposure is not "unknown", it is "under-determined in the
direction of REAL":

- **Group 1583** (`0x8268b1f0`, 436 B via `?RemoveUser@Band@@`). The group's
  recorded evidence claims retail is *byte-for-byte identical* to our
  `?DeletePlayer@BandUser@@` with zero relocations on both sides. Today it
  measures 40 B vs 44 B — but `big[:-4] == small` is **True**: our body is
  retail's body **plus one trailing, unreachable `blr`** after a `bctr` tail
  call. **All 40 overlapping bytes are identical**, corroborating the recorded
  evidence; the 4-byte tail is an extent/codegen question this lane cannot
  settle. Retail has no `DeletePlayer` body and we do not compile `FinalRelease`,
  so there is no second view.
- **Group 1587** (`0x82772b08`, 116 B via `SongData`). The two callees'
  COMDATs are **byte-EQUAL** (120 B, 5 relocations each); only their *own* per-`T`
  sub-callees differ (`get_allocator<…>`, `_Vector_base<…>`). That is a
  second-level fixpoint — plausibly a real fold, but grounding it needs one more
  level than this lane's instrument reaches.

★ **Which branch pays, stated plainly.** Keeping these two is the
score-favouring branch: it preserves 552 B. I took it, and I am flagging it
rather than burying it — the licence is the positive partial byte evidence above,
not the 552 B. Withdrawing them would enact a `CONTRADICTED` verdict that §5 and
§8 show is not established, which is the same error in the opposite direction
from the one W9-D measured 69 times. The paying branch I *declined* is the larger
one: I did not reverse or weaken any record to harvest the 4,136 B in §6, and I
did not touch the 535 `NEEDS_SOURCE` / 107 `NEEDS_MAP_ID` memberships.

## 9. Per-group adjudication

| group | retail addr | family | n | REAL | FABR | UNPROV | adjudicating evidence |
|---|---|---|---:|---:|---:|---:|---|
| 612 | `0x822d70e0` | `$_Param_Construct` | 40 | 0 | 38 | 2 | per-`T` copy ctor differs in COMDAT **size** (3 also map-distinct) |
| 638 | `0x82304870` | `$_Param_Construct` | 40 | 0 | 38 | 2 | per-`T` copy ctor differs in COMDAT **size** (3 also map-distinct) |
| 1577 | `0x82bab758` | `FOLDPROVE2_vector_4byte_operator_ass` | 6 | 6 | 0 | 0 | callee COMDATs **byte+reloc identical**; 39-body heterogeneous retail fan-in naming 6 element types |
| 593 | `0x824aa280` | `0NgMat` | 5 | 0 | 3 | 2 | per-`T` **base-class** ctor differs (`??0RndMat` vs `??0Protocol`/`??0FilterVersion`/…); 1 map-distinct |
| 450 | `0x82576320` | `$__uninitialized_copy` | 2 | 0 | 2 | 0 | `lfs/stfs` vs `lwz/stw` — **non-relocated** words differ @0x10–0x1c |
| 1586 | `0x826989e0` | `consensus_partition` | 2 | 0 | 2 | 0 | 68 B/**3 rel** vs 60 B/**1 rel**; retail calls a vector copy ctor via `__savegprlr_29` |
| 393 | `0x82713ad8` | `$_Copy_Construct` | 1 | 0 | 1 | 0 | per-`T` copy ctor size |
| 759 | `0x822a46c8` | `$_Param_Construct` | 1 | 0 | 1 | 0 | per-`T` copy ctor size (map-distinct) |
| 786 | `0x827b6c78` | `DataDir` | 1 | 0 | 1 | 0 | adjustor thunks `$4…EM@` vs `$4…EE@` to **different** virtual functions |
| 1353 | `0x827cfc90` | `PostDownload` | 1 | 0 | 1 | 0 | identical code, `stb r11,0x10(r3)` vs `stb r11,0xc(r3)` |
| 1582 | `0x82441658` | `push_back` | 1 | 1 | 0 | 0 | callee COMDATs **byte+reloc identical**; retail body itself names a 2nd instantiation (`Key<float>` under a `Vector2` survivor) |
| 1583 | `0x8268b1f0` | `DeletePlayer` | 1 | 0 | 0 | 1 | ours = retail **+ one trailing unreachable `blr`**; the overlapping 40 B are byte-identical |
| 1584 | `0x827ed198` | `consensus_partition` | 1 | 0 | 1 | 0 | 96 vs 64 B |
| 1585 | `0x823d3b38` | `consensus_partition` | 1 | 0 | 1 | 0 | 96 vs 48 B |
| 1587 | `0x82772b08` | `consensus_partition` | 1 | 0 | 0 | 1 | callee COMDATs **byte-EQUAL**; only their own per-`T` sub-callees differ |


## 10. Predicted vs measured

| change | commit | predicted | measured | agree |
|---|---|---|---|---|
| **A** — withdraw the 89 FABRICATED | `e6a1cb47` | −696 B / −7 fns / 7 rows | **−696 B / −7 fns / 7 rows**; units HamCamTransform −2, Stats −2, Achievements −1, HamMove −1, RhythmDetector −1 (sum −7 = whole-binary Δ) | ✅ exact, both keys and attribution |

Measured with `python3 tools/ab_measure.py --worktree ~/tmp/wt-w10-d
--from-dirty`, one change in the run. Both legs settled with **0 recompiles**,
both legs at a **split fixed point** (0 extra re-splits each), `Δmasked_equal
+0`, units at 100% unmoved on both rulers (162 mpn / 134 all-rows-fuzzy).
Final state **42,742 / 3,870,612 / 37.776974%** — matching the post-gate
`report.json` exactly.

The prediction came from the licensed report-only ablation
(`tools/alias_forgiveness_audit.py`'s `leg()` construction, re-keyed on **group
index + spelling** so a shared address cannot over-remove; it refuses if ninja
compiles anything and restores the ledger on every exit path). Every reading in
this document **RECONCILES** — the fallen rows' sizes sum exactly to the measured
`matched_code` delta, which is the prober's own refusal condition.

⚠ **The `none`-ruler control read FLAT** (`[control none] Δmatched_code=+0 B`).
That is **expected and proves nothing**: `none` ignores relocation names, so it
reads flat over any alias change by construction, and that flatness is the
fabrication hazard's *signature*, not a clearance. `ALIAS_SUSPECT` on an
alias-only patch is likewise expected. The licence for change A is retail bytes.

## 11. Gates

Run after the alias edit — `touch config/45410914/config.yml`, full build, then:

```
./tools/ninja-locked                                            EXIT=0
python3 tools/icf_alias_finder.py --validate
  VALIDATE: PASS -- 1357 map-consistent, 236 tolerated, 0 contradicted, 1595 total   rc=0
python3 scripts/verify_objs_patched.py --verify-manifest
  [patch-state] OK: 1205 decomp, 3085 target objects match
  tree_sha256=8ec7b2e363e2b058                                  rc=0
```

`tree_sha256` is **unchanged across the whole lane** — expected and worth
stating: the alias ledger is a *report-time* input, so it moves `report.json`
without moving a single object byte.

⚠ And re-read §2 before quoting that `VALIDATE: PASS`. It was green **before**
this lane too, with all 104 live. It is a map-consistency statement, and it is
not evidence about folding in either direction.

Ledger accounting — records **MOVED**, never deleted (the GROUNDED-2 convention,
verified against the file by W9-D; the durability guard reads `withdrawn`, so
annotation alone would not hold):

```
groups              1,595 -> 1,595   (unchanged)
live memberships    5,342 -> 5,253   (-89)
withdrawn records   9,993 -> 10,082  (+89, all CREATED; none deleted)
restored records       71 ->     71   (unchanged)
```

Each new record carries `spelling`, `lane`, `class:
CONTRADICTED_ON_RETAIL_BYTES`, `disposition: withdrawn`, the verdict, the
per-membership `why`, the raw `adjudicator_detail`, the `callee_layer`, and a
note naming the instrument and its cross-validation — so a future generator
cannot silently re-propose any of them, and a future lane can re-admit one via
the override mechanism on positive retail-byte fold evidence.

## 12. What this lane did NOT do

* **Did not bulk-withdraw**, which was the explicit instruction and the whole
  point. Every one of the 104 was classified individually; 15 of them were
  inspected at the byte level by hand.
* **Did not touch H1** — the star→clique union at the survivor key that
  manufactures `FABRICATED_CLOSURE_NOT_PARTITION` (revert `760cb450`). Out of
  scope, and still the root cause.
* **Did not touch any map row**, any source file, or `symbols.txt`. The only
  file changed is `scripts/symbol_aliases.json`.
* **Did not regenerate `scripts/symbol_aliases.json`.** It is a fixed point, not
  a derived artifact; its own `_comment` says a regeneration is a regression.
  Every change here is a targeted edit to the shipped file.
* **Did not delete a single record**, including the 6 zero-forgiveness
  UNPROVABLE memberships.
* **Did not adjudicate the 535 `NEEDS_SOURCE` or 107 `NEEDS_MAP_ID`
  memberships.** They reproduce exactly and remain the unadjudicable remainder.
* **Did not chase the second-level fixpoint** at group 1587, or settle group
  1583's 4-byte tail. Both are declared UNPROVABLE with their price attached
  (§8) rather than resolved by preference.
* **Did not run the permuter** (OFF by standing directive) and did not touch
  the native gate — no `src/` file changed, so it is not implicated.

## 13. Handoffs

1. **`0x8268b1f0` (group 1583) is a 4-byte extent question worth 436 B.** Our
   `?DeletePlayer@BandUser@@QAAXXZ` COMDAT is 44 B ending `mtctr; bctr; blr`;
   retail's folded survivor is 40 B ending `mtctr; bctr`. The first 40 bytes are
   identical. Someone who can settle whether that trailing unreachable `blr` is
   real codegen or a COMDAT-extent artifact converts an UNPROVABLE into a verdict
   either way. Note the recorded W39-PUSHBACK evidence asserts exact byte
   identity, so if the `blr` is real, **our source drifted after the evidence was
   taken** — which would be a source bug the alias is currently masking, not a
   fabrication.
2. **⚠ `0x827cfc90` (group 1353) looks like a struct-layout defect wearing an
   alias.** Retail `?PostDownload@NetLoader@@` is `li r11,1; stb r11,0x10(r3);
   blr`; our `?MakeDirty@Profile@@` is the same with **`0xc`**. If these two
   really are one retail body, our `Profile` dirty-flag member is **4 bytes too
   low**. The alias has been forgiving that. Withdrawn here as an alias, but the
   *layout* question is untouched and is exactly the "a metric that hides real
   bugs is worse than a lower metric" shape. Cheap to check with
   `scripts/harvest/class_layout_report.py Profile`.
3. **Group 450 is a second possible source signal.** Retail copies
   `pair<float,float>` with `lfs/stfs`; we copy `Vector2` with `lwz/stw`. Same
   size, same loop structure, different register class. Worth one look at how
   `Vector2`'s members are declared.
4. **Group 1587 needs one more level of grounding** (552 B shared with #1). Its
   two callees are byte-EQUAL; only `get_allocator<T>` and `_Vector_base<T>`
   differ. A three-level version of §4's instrument would settle it.
5. **The instrument in §4 generalises and is cheap.** It is ~120 lines over
   `Sides`, needs no build, and answered 96 of 104 memberships. The natural next
   use is the **535 `NEEDS_SOURCE`** backlog: as source lands, each one becomes
   decidable by exactly this test, and #1 shows the same test also finds source
   drift.
6. **H1 remains the root cause** and is further evidenced: groups 612 and 638 are
   two survivors carrying the *same* 40 refuted spellings each, which is the
   under-partitioned-closure signature rather than 80 independent defects.
