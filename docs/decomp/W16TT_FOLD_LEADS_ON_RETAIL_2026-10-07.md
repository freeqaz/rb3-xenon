# W16-TT: this round's fold-group leads, adjudicated on retail bytes

**Date:** 2026-10-07 · **Lane:** W16-TT · **Branch:** `w16-tt` off main `19aac6dd0` · **Worktree:** `~/tmp/wt-w16tt`

The leads are the fold questions three lane docs left for a name lane: W16-TQ §4.6, W16-TI §4.13/§4.18/§7 and W16-TO §4.7.
All are settled on retail bytes (`band.exe`, clean TU5) with `tools/icf_pair_adjudicate.py --chase`, and every alias edit is
priced with `tools/ab_measure.py`. One lead (group 771) had a recorded proof that was wrong, so §2–§3 explain why that proof
passed and measure whether the same gap admits other memberships.

## 0. Verdicts

| lead | verdict | edit | measured (`ab_measure`, name_check) |
|---|---|---|---|
| group 771: `~vector<vector<short>>` folded into `0x82775950` | **WRONG.** Our body is `0x82BB3BB8` (group 697) | moved 771 → 697 | **+0 fns / +44 B** (A1) |
| group 1879: `__destroy_range_aux<rev_it<vector<short>*>>` into `0x82773E70` | **WRONG.** Our body is `0x82BB3510` | moved 1879 → new group @ `0x82BB3510` | (in A1) |
| same gap, other groups | **10 more wrong memberships** (W16-CU restorations, groups 723 ×5, 875, 1201 ×3, 1223) | withdrawn | **exactly 0** (A2) |
| `Fader::DoFade`: `list<FaderTask*>::insert` and group 1457 | **REAL FOLD.** Chase PROVEN | re-admitted, with override record | **+1 fn / +444 B** (B) |
| `~FaderGroup`: `ObjPtrList::erase` home | **NOT a template defect.** Local to this dtor; no spelling found | none (3 probes reverted) | — |
| `PatchDir::LoadStickerData` callee and group `0x824b9a58` | **REAL FOLD.** Chase PROVEN | admitted | **+0 / +0** (C), row 98.880 → 98.907 |

Total on the branch: **+1 function / +488 B**, plus 12 wrong memberships removed and 4 proven ones added.

## 1. Groups 771 and 1879: the `VorbisReader` destructor chain

W16-TQ read the outer bodies. I read the whole chain from `band.exe` (capstone, `~/tmp/w16tt/rdis.py`):

| level | `0x82775950` chain (771 / 1879) | `0x82BB3BB8` chain (697 / new) |
|---|---|---|
| `~vector<vector<T>>` | `bl 0x82773E70`; outer element `li 0xc`/`divw`/`mulli 0xc` | `bl 0x82BB3510`; same outer code |
| `__destroy_range_aux` | `bl 0x8269C800` | `bl 0x8241B2D0` |
| element `??_G` | `li r10,0xc` / `divw` / `mulli r3,r11,0xc`: **12-byte element** (`RangedData<uint>`) | `srawi r11,r11,1` / `slwi r3,r11,1`: **2-byte element** |

Our `vector<short>` has a 2-byte element. Adjudicator output (`~/tmp/w16tt/adj1_chase.txt`, `adj2_chase.txt`):

- 771 survivor vs ours: flat REFUTED, **CHASED REFUTED** (`BYTES-DIFFER ??_GCommonPhraseCapturer` vs `??_G<vector<short>>`).
- 697 survivor (`0x82BB3BB8`) vs ours: **CHASED PROVEN** (slots `__destroy_range_aux<ushort>`, `??_G<vector<ushort>>`, leaf `??3`).
- 1879 survivor vs ours: **CHASED REFUTED**, same depth-1 `BYTES-DIFFER`.
- `0x82BB3510` vs our `__destroy_range_aux<short>`: **CHASED PROVEN**. (So is `0x8241B2D0` vs our `??_G<vector<short>>`.)
- `locate_retail` places our two bodies at exactly `0x82BB3BB8` and `0x82BB3510`, and nowhere else.

**Edit (A1, `2aaaf3366`).** Withdrew our spelling from 771 and from 1879, with class `PROVEN_RECORD_REFUTED_ON_RETAIL_BYTES`.
Admitted it to 697, and added group 2159 at `0x82BB3510` (survivor = the map's name there, which passes the build's
survivor-drift check).

**Measured +0 fns / +44 B.** I predicted +1 fn / +316 B, and that prediction failed: the 44 B is `fn_82BB441C`
(VorbisReader, 99.55 → 100). `~VorbisReader` lost its name charge as predicted, but it also carries a one-store scheduling
move (`stb r28,0x11a(r3)` two slots apart) that I had not priced. It went only 97.405 → 97.468. Behaviour is unchanged, as
W16-TQ said: `short` and `unsigned short` cannot be told apart in a destructor.

Not done: our `??_G<vector<short>>` is in no group. It is proven at `0x8241B2D0`, but it was never claimed anywhere, so I
added nothing.

## 2. Why group 771's PROVEN record passed

The record is W16-CU 2026-09-15 (`restored`): *"FOLD -- PROVEN on retail bytes, flat L1_T1 (byte identity with relocation
TARGET NAMES compared)"*, via `tools/alias_forgiveness_audit.Sides.verdict`.

On that date `scripts/target_symbol_map.json` (`4e35127ed`) named `0x82773E70`
**`??$__destroy_range_aux@V?$reverse_iterator@PAV?$vector@FV?$StlNodeAlloc@F...`, which is our spelling**, on the 12-byte
body. `0x82BB3510`, the real home, was unnamed. W16-SG renamed `0x82773E70` to the `RangedData<uint>` instantiation today
(`c83fa7001`).

Both T1 comparators accept a same-name slot without reading the callee:

- `tools/icf_alias_build.py` `relocs_agree`: `if rn == on: continue`.
- `tools/icf_pair_adjudicate.py` `chase`: `if survivor == our_name and depth > 0: return True` ("name equality IS the evidence").
  `_slots_agree` also does `if rn == on: continue` before it would recurse.

The retail name is the map's, so a same-name slot is only as good as the map's identification of the callee. Here the map
had put our spelling on a twin body, and the only slot that discriminates the fold compared equal to itself. The real
difference sits two levels down (§1), and nothing ever read it.

W16-SG's rename did not trigger a re-chase of 771, because 771's own survivor label did not change. W16-OS's survivor
re-chase (10-03) covers relabelled survivors, not relabelled callees. That is why 771 kept a record that its own tool
refutes today. 1879 is the mirror image: W16-OS re-chased it, read UNDECIDABLE, and carried it.

## 3. Does the gap admit other groups?

**Instrument: `tools/alias_samename_slot_audit.py`** (`2e9ab97bd`, read-only, not wired into the build). For every installed
membership it runs:

- **base**: the shipped adjudicator (flat T1, then chase).
- **strict**: the chase with every same-name slot whose callee is present and non-vacuous on both sides byte-checked
  recursively.

A same-name slot counts against a membership only when its bytes differ **and** `locate_retail` proves our callee at
another retail address. A byte difference alone is not evidence: see the second failed version below.

The instrument failed twice before it passed. Both failures were caught by its own controls:

1. The first version wrapped only `chase`. Its selftest (rebuild the 09-15 map state in memory, require shipped flat T1 =
   PROVEN and strict = REFUTED) read **strict = PROVEN, SELFTEST FAIL**. Cause: `_slots_agree` skips same-name slots
   itself. The fix was to wrap `_slots_agree` too.
2. The second version then **over-rejected the negative control** (W16-JE's `SetObjConcrete<UILabel>`, flat-T1 PROVEN).
   Our `Hmx::Object::Release` differs from retail's in bytes because our port of it is imperfect, not because the map is
   wrong. The fix was to require a located home elsewhere.
3. Third version: **SELFTEST PASS** (09-15 state: flat PROVEN, strict REFUTED; negative control PROVEN).

**Result** (`~/tmp/w16tt/samename_audit.json`, HEAD = main `19aac6dd0` + the tool):

| base | strict | memberships |
|---|---|---:|
| PROVEN | PROVEN | 4,521 |
| REFUTED | REFUTED | **86** |
| UNDECIDABLE | UNDECIDABLE | 1,756 |
| | **total** | **6,363** (1,051 same-name slots byte-checked) |

**The strict pass adds no failure.** No membership that today's map and chase still prove rests on a same-name slot that
retail contradicts. I had predicted "a few more"; that prediction failed too. The live exposure is the 86 memberships that
the **shipped** chase already refutes:

| kind | memberships | groups |
|---|---:|---|
| W16-CU "restored, PROVEN flat L1_T1" | **11** | 723, 771, 875, 1201, 1223 |
| carried UNDECIDABLE by W16-OS / W16-OU | 15 | 93, 416, 443, 705, 949, 971, 1056, 1111, 1113, 1405, 1426, 1561, 1688, 1809, 1879 |
| generator T1, no per-membership record | 60 | 95, 111, 322, 546, 564, 581, 601, 623, 638, 846, 957 |

### 3.1 W16-CU's other 10: the same gap, second mechanism

W16-CU restored 242 memberships. Of those still folded today, 28 read PROVEN, 9 UNDECIDABLE and **11 REFUTED**. The 10
besides 771 refute at the depth-1 callee. On 2026-09-15 that callee was **unnamed**:

| group | retail callee | map @ W16-CU | map now (named 09-30) | row now | our callee |
|---|---|---|---|---|---|
| 723 (×5) | `0x827D7220` | none | `AllocInfo::Print` | 112 B, 100 | `CtrlPoint::Load`, `BinStream` savers |
| 1201 (×3) | `0x827427E8` | none | `Splash::UpdateThread` | 344 B, 100 | `D3DQuery_Issue`, `D3D*Buffer` unlock |
| 875, 1223 | `0x8249B7F0` | none | `~ObjOwnerPtr<RndDrawable>` | 116 B, 100 | `~ObjOwnerPtr<Hmx::Object>` (100 at its own address; different vtable store) |

`relocs_agree` tolerates a retail `fn_` placeholder whenever **our** callee is not map-resident (the CD-9 rule refutes only
when ours *is* mapped). Each of these flat-T1 proofs therefore skipped the one slot that names the callee. W16-JG closed this
hole on the **chase** path on 10-01 (placeholder-slot discharge). Flat `relocs_agree` still tolerates placeholders, and
`Sides.verdict` (W16-CU's instrument) takes flat PROVEN as final.

**Is that still live?** The strict pass always runs the JG-discharged chase, even on flat-PROVEN pairs. It agreed on all
4,521, so no installed membership depends on the flat placeholder tolerance today. The tolerance remains in the tool, so a
new flat-only proof could still pass through it.

Every one of the 10 reads CHASED REFUTED, and `locate_retail` places our body at no retail address. Each retail callee is a
100%-matched row under its present name, so retail calls a different function from ours. **Edit (A2, `c546a4fb6`):**
withdrew all 10. **Measured exactly 0 on every measure, fuzzy included (predicted 0).** They were forgiving nothing.

### 3.2 Not adjudicated here (lead for the coordinator)

The other **74** shipped-REFUTED memberships: 14 carried-UNDECIDABLE (1879 is now settled) and the 60 generator-T1 ones. They
still sit in `folded[]` while today's own chase refutes them. Bulk withdrawal would need positive evidence for each, as in §3.1.
The generator-T1 ones (mostly `_Copy_Construct`/`_Param_Construct`, `ObjPtrVec` nodes, DC3 spellings) are the likely
casualties of the same two mechanisms. Re-run `python3 tools/alias_samename_slot_audit.py --out <f>` and filter `base ==
REFUTED` for the current list.

**Durable fix, not made here:** make flat T1 apply W16-JG's placeholder discharge, or make `Sides.verdict` chase-confirm a flat
PROVEN. Separately, re-chase every membership whose slot callee's **map name** changed, not only memberships whose survivor
changed.

## 4. `Fader::DoFade`: `list<FaderTask*>::insert` and group 1457

Retail `DoFade` calls `0x823D14C0`, the 1457 survivor. Our spelling is not map-resident. It was withdrawn from **10** star
groups by ALIAS-CONSOLIDATION 2026-08-19 (`FABRICATED_CLOSURE_NOT_PARTITION`, a closure sweep with no per-pair evidence).

`--chase`: **CHASED PROVEN**, 100 B both sides. The slot chain is `_M_create_node<Content*>/<FaderTask*>` → `MemOrPoolAlloc` /
`MemOrPoolAllocSTL` → `PoolAlloc` 5-arg/2-arg → `??2ChunkAllocator`/`??2` (`VACUOUS-BUT-IDENTICAL`), with the `gChunkAlloc`
data slot accepted. The strict audit agrees.

**Edit (B, `543c0c96a`):** re-admitted to 1457 only, with an `admitted` record and an entry in
`scripts/alias_withdrawal_overrides.json` naming the class, so a regeneration keeps it. `alias_withdrawals.load_overrides`
accepts it, and `tools/test_icf_alias_withdrawal_guard.py` passes (7/7). The nine other star withdrawals stand.
**Measured +1 fn / +444 B (DoFade 99.955 → 100), predicted exactly.**

## 5. `~FaderGroup`: the `ObjPtrList::erase` home (W16-TI §4.18)

Retail (`0x8270CEE8`) stores the node to `0x54(r31)` **once**, after `lwz r30,0(r27)`. Ours stores it **twice**: once before
that load and once after. The extra store comes from `pop_front()`, i.e. `erase(mNodes)`: one store for the `Node*`→`iterator`
conversion temporary and one for `erase(iterator)`'s by-value copy. `iterator` has user-declared constructors, so under this
ABI a by-value `iterator` lives in a stack temporary.

**Is the shared template the defect?** No. `~/tmp/w16tt/unlink_home_scan.py` counted node-register stores to an `r31` slot in
the 6 words before every `ObjPtrList::Unlink` call, on both sides:

| retail homes | our homes | paired functions |
|---|---|---:|
| 0 | 0 | 88 (87 single-site + 1 two-site) |
| **1** | **2** | **1** (`~FaderGroup`) |

So 88 of 89 paired `pop_front`/`erase` callers home nothing on either side. Examples: `OnRestoreCategories`, `LoadObjs` and
`LoadDir` are 100 with no frame slot at all. Only `~FaderGroup` homes the node. It is an EH-framed destructor that also takes
`&this` for `erase_unique`. Editing the template would put risk on 88 callers to fix one.

Local spellings tried in `Faders.cpp` (via `run_objdiff`), all reverted:

| spelling | our homes |
|---|---:|
| `mFaders.pop_front()` (current) | 2 |
| `mFaders.erase(mFaders.begin())` | 2 |
| `it = begin(); frontObj = *it; erase(it)` | 0 |
| `frontObj = front(); it = begin(); erase(it)` | 2 |

Retail's single home was not reproduced, and the row stays at 97.50. Separately, retail calls the shared `Unlink` body
`fn_8271A138` (one body for every `T`, unnamed) where we call `ObjPtrList<Fader>::Unlink`. A placeholder target is forgiven
by name_check, so it is not charged.

## 6. `PatchDir::LoadStickerData` and group `0x824b9a58` (W16-TO §4.7)

Our `hash_map<Symbol, vector<PatchSticker*>, PatchDir::SymbolHash>::operator[]` against the survivor
`hash_map<Symbol, vector<LightPreset*>, hash<Symbol>>::operator[]`: flat REFUTED, **CHASED PROVEN**, 240 B both sides. All 13
slot folds are OK. The spelling is not map-resident and has never been withdrawn. The strict audit agrees.

**Edit (C, `2ce950595`):** admitted. **Measured +0 / +0 (predicted)**. The only row change is `LoadStickerData` 98.880 →
98.907, which keeps its one-instruction scheduling move.

## 7. Measurements

All runs: `python3 tools/ab_measure.py --worktree ~/tmp/wt-w16tt --patch <diff> --label <l>`. Kind `map`, both legs at the
split fixed point, objdiff `sha256:c1b7d952`. Each patch was measured on top of the previous commit. Run dirs are under
`~/tmp/wt-w16tt/.ab_measure_runs/`.

| patch | run dir | leg A → leg B matched | Δcode bytes | `none` control |
|---|---|---|---:|---|
| A1 | `20261007-124512-w16tt-A1-2360409` | 54,939 → 54,939 | +44 | flat (ALIAS_SUSPECT shape; adjudicated §1) |
| A2 | `20261007-124827-w16tt-A2-2379722` | 54,939 → 54,939 | 0 | flat |
| B | `20261007-125045-w16tt-B-2397021` | 54,939 → 54,940 | +444 | (+444 name_check) |
| C | `20261007-125321-w16tt-C-2434935` | 54,940 → 54,940 | 0 | flat |

The first A1 run was refused at preflight (untracked tool file). I committed the tool and re-ran.

**Post-edit control:** re-running the audit on HEAD gives 6,355 memberships, 4,525 PROVEN, 74 REFUTED. Exactly the 12
withdrawn disappear and the 4 added read PROVEN on both passes; nothing else moves.

## 8. Not done

- The 74 remaining shipped-REFUTED memberships (§3.2) and the durable comparator fix (§3).
- No `src/` change was kept, so the native gate was not run.
- Not merged and not pushed.
