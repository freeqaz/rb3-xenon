# W16-OA: `--chasetest` controls no longer depend on unnamed addresses; held-back names landed; alias-group keys repaired (2026-10-02)

Branch `w16-oa`, off main `d792b486f`. Four results:

1. `tools/icf_pair_adjudicate.py --chasetest` evaluates its hardcoded and ledger-sourced controls in a
   recorded **naming fixture**. Map work can no longer retire a control. Every `--self-break` mode still
   turns its controls red (§2).
2. The names W16-NZ held back are landed: `0x8264ec88` / `0x8264ec08`. A/B **+2 fns / +2 honest /
   +168 B, 0 rows down**, as predicted (§3).
3. `scripts/symbol_aliases.json` had 7 duplicate survivors and 5 duplicate placed addresses. They are
   repaired on retail bytes, and the 4 failing alias tests pass **unchanged**. A/B **Δ0 on every
   measure**, with 8 rendered buckets changed (§4).
4. Why those failures sat unseen: 4 of the 5 alias test files ran **zero tests** when invoked as scripts
   (§4.1).

## 1. The defect

A slot decoy is lax-PROVEN only while some retail slot in its proof is still a placeholder, or while one
of our spellings is still unmapped. Correct map work therefore kept retiring decoys:

- W16-NU held back `0x822e4fd8`.
- W16-NZ's naming of `0x8259f7e0` retired W16-NX's decoy.
- W16-NZ held back `0x8264ec88` / `0x8264ec08`.

**A silent case existed as well.** The IN-FAMILY DECOY was the literal string `fn_827B0E78`. W16-NF
named that address in `0aa7f19f8`, and from then on the decoy "passed" as REFUTED on `MISSING(retail)`
without comparing a single byte. It was found on this lane's first run (`chasetest_base.log`: FLAT T1
UNDECIDABLE, "survivor absent from the dtk target objs").

## 2. The fix (`tools/icf_pair_adjudicate.py`, `scripts/chasetest_fixtures.json`)

**What a fixture records.** For one control, a fixture records the naming neighbourhood its verdicts
read:

- the name carried at record time by every retail address that the control's lax and discharge chases
  touched (keys looked up, plus every relocation target of every body read);
- every `in mapped` answer the chase consulted.

**How it is used.** At run time the control is evaluated in an in-memory view of the tree with that
naming restored. Bodies, relocation shapes, the retail image and our objs stay live. A source or split
change can still move a control; a map change cannot. Views are for controls only and are never used
for an admission.

There are 7 fixtures:

| fixture | names |
|---|---|
| IN-FAMILY DECOY | 3 |
| SLOT DECOY: W16-JE vtable pair | 4 |
| SLOT DECOY: VTABLE_OF_CLASS_ELSEWHERE | 3 |
| SLOT DECOY: VTABLE_RTTI_DIFFERS | 4 |
| SLOT DECOY: CALLEE_LOCATED_ELSEWHERE | 7 |
| SLOT DECOY: MAPPED_VS_PLACEHOLDER | 20 |
| RENAME (positive + decoy) | 4 + 2 |

The MAPPED_VS_PLACEHOLDER fixture includes `fn_8264EC08` and `fn_8264EC88`. It also includes the
`in mapped` answer for our `insert_after<int,SongMetadata*>` (False), which is what W16-NX's decoy died
on.

Other changes in the tool:

- **IN-FAMILY DECOY** is keyed by address (`IN_FAMILY_DECOY_ADDR`). It now refutes on
  `BYTES-DIFFER` between `_M_find<Symbol>` and `_M_find<int>`, the discriminator it was written to
  probe.
- **Vacuity guard.** A control whose survivor or our spelling is absent from its side now REFUSES.
  Absence is never a verdict.
- **`--record-fixtures [--rerecord]`.** Records a fixture for each fixture-able control that lacks one.
  Run it on a tree where `--chasetest` passes.
- **`--no-fixtures`.** Diagnostic: evaluates every control on the live tree only.
- **`--simulate-naming`.** Diagnostic. In memory, it names every placeholder still present in any
  fixture neighbourhood, updating keys, relocations, `mapped` and `_addr_name`.
- The `locate_retail` body index is now cached per tgt object, because several views coexist.

`slot_controls` tries a class's fixture first and falls back to the live candidates. A fixture whose
view is no longer lax-PROVEN prints a warning and falls back. That case means a body changed, which is
exactly when a decoy should be re-chosen.

**Scope bound.** `locate_retail` caps its candidates at 64 in name order. A candidate set larger than
the cap could still be reordered by naming an address outside the neighbourhood. No current fixture is
near the cap.

### Measurements

| tree | `--chasetest` | 10 self-break legs | `--no-fixtures` |
|---|---|---|---|
| pre-naming (`06ade06ec`) | rc=0 | all rc=0 (slots: all 6 SLOT DECOYs red, nothing else moved) | rc=0 |
| names landed (`c590dd979`) | rc=0, MVP view restores exactly 2 names, decoy trace **identical** to pre-naming | all rc=0 | **rc=1 REFUSING** for `PLACEHOLDER_SLOT_MAPPED_VS_PLACEHOLDER` |
| + `--simulate-naming` (10 more addresses, 221 bodies rewritten) | rc=0, **20/20 chase sections identical** to the unsimulated run | slots, rename: rc=0 | rc=1 REFUSING for MAPPED_VS_PLACEHOLDER **and** VTABLE_OF_CLASS_ELSEWHERE |
| final tip (`7e3123c5c`) | see §5 | see §5 | see §5 |

The ten self-break legs are `--self-break`, `-slots`, `-tailpad`, `-rename`, `-overcarve`, and
`-size eh|tol|onesided|firstdef`.

**A failed prediction, recorded.** I expected the first simulation to pass. Instead the MVP decoy went
lax-REFUTED. The cause was a bug in the *simulation*: it overwrote the address→name answer for
addresses the map already named, so the view could not find them and restored only part of its
neighbourhood. Two fixes followed:

- The simulation now touches only addresses that are still placeholders in the tree.
- The view now prints any recorded address that the tree spells neither by its map name nor by a
  placeholder, instead of skipping it silently.

## 3. The held-back names

- `0x8264ec88` = `slist<pair<const Symbol,vector<Symbol>>>::insert_after`. Chase PROVEN, 88 B on both
  sides.
- `0x8264ec08` = the same type's `_M_create_node`. Chase PROVEN, 80 B on both sides. It is flat-T1
  REFUTED only on the allocator fold chain (`MemOrPoolAlloc`/`MemOrPoolAllocSTL`, the `??2` thunk,
  `gChunkAlloc`).

Both addresses are in LicenseMgr's pinned span, and LicenseMgr.obj defines both spellings. The only
retail callers are `0x8264ece0` and `0x8264ed88`, the vector<Symbol> table's own functions, whose
compiled bodies call these exact spellings. So no forgiven site becomes a charged one.

**Predicted +2 fns / +168 B. Measured:**

```
ab_measure --pick c590dd979 (leg A = c6ea8273f), ruler name_check
Δmatched=+2  Δmasked_equal=+0  Δhonest=+2  Δcode%=+0.001640pp  Δcode_bytes=+168  Δfuzzy=+0.001633pp
control none: +168 B (REAL_PAIRING)    units at 100% [mpn]: 490 -> 491 (LicenseMgr)
```

A per-row diff of the archived leg reports shows **0 rows down**. The 2 rows up are the two names; the
2 rows present only in leg A are their former placeholders.

## 4. The duplicate alias-group keys

### 4.1 Why they were unseen

`tools/test_alias_group_key.py`, `test_icf_alias_join_guard.py`, `test_icf_alias_no_ourbuild_gate.py`
and `test_icf_alias_withdrawal_guard.py` are pytest-only and had no `__main__`. Running
`python3 tools/test_*.py`, the gate lanes used, executed zero tests and exited 0.

Each file now runs itself under pytest and is registered in `scripts/test_tools.py`. Control: against
main's ledger, `test_alias_group_key.py` exits **1** (3 failed) and `test_icf_alias_withdrawal_guard.py`
exits **1** (1 failed).

The duplicates were introduced by 9 commits, starting at `cee04e1ee` (W17-PRAC, 09-30); the latest is `e760fa852` (W16-NH, 10-02).

### 4.2 Why the tests are right

- objdiff buckets the rendered alias map by address, so two placed groups at one address merge into one
  equivalence class.
- `tools/alias_withdrawals.py` denies on (survivor | address, spelling), so a duplicated key let one
  group's record deny another group's live member. That was happening at `0x82714c98` and at every
  merge address below.

### 4.3 The repair (`tools/w16oa_dedup_alias_groups.py`)

The tool re-adjudicates each load-bearing pair with `chase()` at apply time (53 verdicts) and refuses
if any verdict differs from the one its decision rests on. The rule:

- **PROVEN** at an address: the member is live there.
- **CONTRADICTED** (positive evidence): withdrawn.
- **UNPROVEN** because our build compiles no body for it: carried unchanged. Such a member can forgive
  no call site of ours, which is ALIAS-CONSOLIDATION's neutral choice.

| case | decision |
|---|---|
| `0x82557060`, `0x822a6ce8`, `0x82483d90`, `0x82483d40` | address-less partition twin identical to its (since restored) placed group: removed, kept as `absorbed` history |
| `0x82714c98` | empty partition shell left by W16-NR; its one record denied this group's own PROVEN member: removed |
| `~ObjPtr<RndTex>` at `0x8229d930` and `0x8243dd20` | W17-OPTR moved the name to the 100 B body; `0x8243dd20` is an unnamed `b` wrapper (map row `null`). Folded into the real group, record carried |
| BandCamShot `list::insert` at `0x824ce130` and `0x822b55e0` | `0x824ce130` relabelled to its map name (`MatOverride`). That label was the survivor at `0x8230ed80`, which was relabelled to *its* map name (`PracticeSectionMapping`), chase PROVEN, 100 B. ALIAS-CONSOLIDATION's withdrawal of that spelling moved to `restored` |
| `0x82594990` | CVEIN-1's `map<int,float>` group re-addressed to its survivor's map address `0x82271e70`. Its W4b records stay keyed on their own survivor; they had denied the owner's survivor and a PROVEN member at `0x82594990` |
| `0x82706208`, `0x822cb4d8`, `0x822d70e0`, `0x82810ce0` | a lane opened a second group beside a stale-survivor group and overrode withdrawals in prose. Merged under the map-name survivor. All 4 newer members re-PROVEN; their 8 old withdrawals moved to `restored` |

What happened to the old survivors and members in the merged groups:

- Stale labels `_Param_Construct<ObjPtr<SeqInst>>` and `_Copy_Construct<ObjPtr<RndPartLauncher>>` are
  **CONTRADICTED** (`VTABLE-RTTI-DIFFERS`: retail stores another class's vtable). Both are withdrawn.
- The `DebugGraph` and `ChallengeRow` labels, and 27 members, have no compiled body. They are carried
  unchanged.
- `_Param_Construct<ObjPtr<RndPartLauncher>>` is CONTRADICTED at `0x822cb4d8` and **flat-T1 PROVEN at
  `0x822d7150`**, the map address of its `_Copy_Construct` sibling. It now lives in a new group there.

**Denylist accounting** (asserted; the tool refuses any unexplained loss). Keys went from 21,814 to
22,057. Of the 187 lost:

| lost keys | reason |
|---|---|
| 16 | they denied members PROVEN live at that address |
| 1 | `(0x8243dd20, ~Key<TexPtr>)`: no map-resident survivor exists there to propose |
| 86 | keyed on the unmapped `DebugGraph` label |
| 80 | still covered by an address key at the stale label's true map address |
| 4 | their spelling is live at the stale label's true address (`0x82706100`), so keeping them would deny live members |
| **0** | unexplained |

Groups went from 2,112 to 2,103.

**A/B:**

```
ab_measure --pick 7e3123c5c (leg A = fc98f2647), kinds ['map'], forced re-split both legs
Δmatched=+0  Δmasked_equal=+0  Δhonest=+0  Δcode%=+0.000000pp  Δcode_bytes=+0  Δfuzzy=+0.000000pp
control none: +0 B (FLAT)
```

This is not an inert leg: `build/45410914/icf_aliases.map` is a report input, and rendering the two
ledgers differs in **8 buckets**:

- 6 stale labels leave address buckets they did not belong to: `SeqInst` at `0x82706208`,
  `RndPartLauncher` at `0x822cb4d8`, `Target` at `0x824ce130`, `map<int,float>` at `0x82594990`,
  `~ObjPtr<RndTex>` at `0x8243dd20`, `MatOverride` at `0x8230ed80`.
- The RndPartLauncher pair moves to `0x822d7150`.
- Two map names enter their own buckets.

So none of those stale labels was forgiving a call site. **0 rows down.**

## 5. Gates (final tip `7e3123c5c`, after a full `./tools/ninja-locked`)

**`icf_pair_adjudicate.py`:**

- `--chasetest`: rc=0, "selftest PASSED". 19 controls, 8 of them in a fixture view.
- Self-break legs, all rc=0:
  - `--self-break`: the VACUOUS DECOY goes red.
  - `--self-break-slots`: all 6 SLOT DECOYs go red; no other control moves.
  - `--self-break-tailpad`, `--self-break-rename`, `--self-break-overcarve`: each targeted decoy goes
    red; nothing else moves.
  - `--self-break-size eh|tol|onesided|firstdef`: exactly 2 / 3 / 1 / 1 size controls go red.
- `--no-fixtures`: rc=1, REFUSING for `PLACEHOLDER_SLOT_MAPPED_VS_PLACEHOLDER`. This is the fixture
  shown to be load-bearing.
- `--simulate-naming`: rc=0.
- `--simulate-naming --no-fixtures`: rc=1, REFUSING for two classes.

**Ledger and map checks:**

- `icf_alias_finder.py --validate`: **PASS**, 1793 map-consistent / 309 tolerated / **0 contradicted** /
  2103 groups.
- `map_name_injectivity.py`: OK, 33,871 applied rows, injective (+1 enumerated exception).
- `alias_placeholder_slot_audit.py` (dry), over the 15 touched addresses: 12 CLEAN and 29 NO-OURS (the
  carried inert members). No contradiction.
- The five alias test files: **23/23 under pytest**, and each exits 0 when run as a script (now a real
  run).

- `tools/native_build_gate.sh`, run last on the tip `551cfe20a` (only this doc line follows it):
  `NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0`

No `src/` file changed. No commit carries a co-author line.

## 6. Found, deliberately not fixed

- **228 of 2,062 placed groups (11%) carry a survivor that is not the map name at their address.**
  Breakdown: 84 address named otherwise, survivor unmapped, folded > 0; 69 the survivor is mapped
  elsewhere, folded = 0; 36 address unnamed, folded > 0; plus smaller classes. The 12 duplicates were
  the colliding tip of this. Example: `0x82706100`'s survivor is `_Copy_Construct<Node<RndGroup>>`, but
  the map names it `_Param_Construct<ObjPtr<SeqInst>>`. The tests do not pin survivor == map name, and
  relabelling cascades (see `0x8230ed80` above), so this is a separate lane.
- **27 inert carried members** (`DebugGraph` ×4 namespaces, `map<Symbol,String>`) are folded at 3 of
  the merge addresses. `DebugGraph` is folded at 16 addresses. Our build compiles no body for any of
  them; this is ALIAS-CONSOLIDATION's closure residue.
- The withdrawal-override channel `scripts/alias_withdrawal_overrides.json` holds 1 entry, while 46
  groups carry a spelling that is both live and withdrawn. Lanes override by prose or by `restored`,
  not through the channel.
