# W16-JH — every W16-JG UNDISCHARGED alias membership decided on retail bytes (2026-10-01)

Lane W16-JH, branch `w16-jh`, worktree `~/tmp/wt-w16-jh`, started off main `bd4ae22ec`.
Main moved three times during the lane (W16-JD `8091a837c`, W16-JC `17aaa4298`, W16-JI
`545bdf90e`), and the branch was rebased each time; it sits on **`545bdf90e`**. Not merged.

## 1. The brief, and the result

W16-JG (`docs/decomp/W16JG_PLACEHOLDER_SLOT_AUDIT_2026-10-01.md`) closed the
placeholder-slot hole in `tools/icf_pair_adjudicate.py` and left **341** memberships
UNDISCHARGED: unproven but not refuted, carrying 189 rows / 66,628 B at fuzzy 100.
This lane decided every one, largest dependent bytes first. Each is either **PROVEN
under the fixed chase**, or **withdrawn with a record**, with the source or the map fixed
wherever the underlying call was wrong.

| fate of W16-JG's 341 (final tree) | n |
|---|---:|
| **PROVEN**: audit CLEAN, every slot discharged, no CYCLE-ASSUMED | **131** |
| withdrawn **REFUTED** on retail bytes | **39** |
| ↳ PLACEHOLDER_SLOT_CALLEE_LOCATED_ELSEWHERE | 15 |
| ↳ SURVIVOR_RENAMED_CHASE_REFUTED (§5) | 18 |
| ↳ PLACEHOLDER_SLOT_CALLEE_REFUTED_BY_RTTI (§4.3) | 5 |
| ↳ PLACEHOLDER_SLOT_MAPPED_VS_PLACEHOLDER (CD-9) | 1 |
| withdrawn **SUPERSEDED**: the map now names the address with the spelling | **5** |
| withdrawn **UNPROVEN, not refuted**: restorable, with evidence | **166** |
| **total** | **341** |

- **210 withdrawal records**, lane `W16-JH 2026-10-01`, groups kept, nothing pruned.
  Every record is one of the 341.
- Re-audit of the final ledger: **0 CONTRADICTED, 0 UNDISCHARGED** (fixed point).
- **Dependence of the withdrawn set** (repaired census, §2): **3 paired rows**.
  - 2 are at fuzzy 100 (192 B, the SongPattern pair, refuted in §4.3).
  - 1 is already below 100 (`vector<FlowMathOp>::_M_insert_overflow_aux`).

## 2. Method, and an instrument defect the A/B caught

Each step was measured by re-running `tools/alias_placeholder_slot_audit.py`, with a
prediction stated first. Dependent bytes come from a census of paired-row relocation
sites where retail names the group survivor and we name the folded spelling.

⚠ **The first census was blind to every data-relocation site.** It compared
relocations as `{offset: name}`, and the `@comp.id` relocation MSVC emits at the same
offset as a `lis/addi` data reference **overwrote the real name**, so function pointers
and vtable addresses never appeared. It reported 2 dependent rows. The first whole-branch
A/B then knocked MetaPerformer's ctor/dtor (and 6 EH funclets) off fuzzy 100. The repaired
census compares a per-offset multiset with `@comp.id` dropped, and found the missing
dependence: `??_DInstarank`, which turned out to be a **wrong map name** (§3.2). A
one-sided blind spot like this reads as "no dependence", which is the reassuring answer,
so only an end-to-end measurement could catch it.

Transition table, from the `bd4ae22ec` re-audit (W16-JG reported 341/0; drift on main had
already moved 2):

| step | UNDISCHARGED | CONTRADICTED | moves |
|---|---:|---:|---|
| start | 339 | 1 | |
| MakeString `operator<<` (source) | 266 | 1 | 73 → CLEAN |
| `ObjPtrList::Unlink` (source) | 236 | 16 | 15 → CLEAN, 15 → CONTRADICTED |
| tail-pad rule (tool) | 200 | 17 | 35 → CLEAN, 1 → CONTRADICTED, 11 LAX-ALSO-FAILS → CLEAN |
| XPhysical* / `_S_next_size` (map) | 199 | 17 | 1 → CLEAN |
| ObjRefOwner ≡ ObjRef (tool) | 196 | 17 | 3 → CLEAN |
| `ObjOwnerPtr` copy ctor (source) | 194 | 17 | 2 → CLEAN |
| `operator<<(BinStream&, ObjOwnerPtr)` (source) + survivor map fixes | 190 | 17 | 1 → CLEAN, 3 superseded |
| over-carve rule (tool) + Waypoint lever (source) | 188 | 17 | 2 → CLEAN |

**Prediction failures, recorded:**
- **Tail-pad rule.** Predicted to move 34 memberships. It moved 35, plus 11
  LAX-ALSO-FAILS, plus 1 to CONTRADICTED. All 47 were inspected: one membership I had
  missed; 11 resting on a wrong map row the rule exposed (`_S_next_size`, §3.2); and one
  legitimate downstream refutation.
- **Census of over-carve shapes.** My first census read **0**, a vacuous screen: it tested
  the masked opcode byte, which masking zeroes. The known positive caught it.
- **Instarank regeneration.** Predicted 210 withdrawals, measured 209. Renaming
  `0x8257C368` took its group's other member out of the audit's reach, and it was then
  decided in §5.

## 3. What proved the 131

### 3.1 Source: retail's code instead of DC3's, each byte-verified against the retail callee
- **`FormatString::operator<<` (`utl/MakeString.cpp`), 73.** Retail `MakeString.cpp`
  (`.text 0x827C3D40–0x827C43D4`) keeps four `operator<<` bodies, one of them
  **`fn_827C40E8` (124 B)**: the String& body minus its `lwz r6,8(r4)`. On Xenon every
  scalar or pointer argument rides in one 64-bit GPR, so int, unsigned, long, unsigned
  long, long long, unsigned long long, `void*`, `const char*` and Symbol are one folded
  body. It calls CRT `_snprintf` with no type check, no arg-in-buffer check and no
  `bufExceeded` latch.
  - All nine of ours went from 152–220 B to 124 B and chase PROVEN.
  - Native keeps the checks behind `HX_NATIVE`.
  - This **proves on bytes** what W16-JG could only suspect (§3 item 1 of its doc).
- **`ObjPtrList<T>::Unlink` (`obj/ObjPtr_p.h`), 15 proven + 15 refuted.** Retail
  `fn_8271A138` (224 B) has a shared `mSize--` / return, and its tail arm steps from
  `mNodes->prev->prev`. The 14 offset-0 instantiations now equal it. The 34 virtual-base
  instantiations equal a **second** retail body, **`fn_8227D0E8`** (240 B, with the
  vbtable adjust). That located the 15 virtual-base `clear<T>` of group `0x8249d1f0`
  elsewhere.
- **`ObjOwnerPtr<T>` copy ctor (`obj/ObjPtr_p.h`), 2.** `mObject(nullptr)` plus
  reassignment emitted an extra `li/stw 0`. Before the fix **0 of 10** instantiations
  equalled a retail body; after it, **10 of 10** do, including named retail rows. This is
  most of the A/B's gain.
- **`operator<<(BinStream&, const ObjOwnerPtr<T>&)` (`obj/ObjPtr_p.h`), 1.** It was
  declared but **never defined** in the match build. Retail `0x8238B5B8` is the
  `ObjPtr<T>` body, and ours is now byte-identical. It sits in the `!HX_NATIVE` branch,
  because native defines its own.
- **`char/Waypoint.cpp` opts into the existing `ObjOwnerPtr` inline owner-ctor lever, 1.**
  Retail `ObjVector<ObjOwnerPtr<Waypoint>>::resize` (`0x823DCD38`) inlines the ctor
  (RTTI-verified vtable). Waypoint unit: 75 → **77 rows at fuzzy 100, 0 down**.

### 3.2 Map: wrong names corrected and missing ones named, with a caller census before each

| address | was | now | evidence |
|---|---|---|---|
| `0x8283C960` | — | `XPhysicalSize` | `b` to XEX import record `0x00C500C5` = xboxkrnl 0xC5 `MmQueryAllocationSize` (checked against Xenia's export table) |
| `0x8283C968` | — | `XPhysicalFree` | `mr r4,r3; li r3,0; b` import 0xBD `MmFreePhysicalMemory` |
| `0x82276828` | `list<FileCache*>::erase` | `_Stl_prime<bool>::_S_next_size` | loads the prime table at +0/+8, its first 92 B equal ours, all 6 paired callers call ours |
| `0x8240F0C8` | `LocalTalkerIsHeadsetPresent` | `Rnd::UpdateHeap` | every other LocalTalker* sits at `0x82C1….`; its one paired caller calls ours |
| `0x823AAE58` | `PropSync<Flow::DynamicPropertyEntry>` | `PropSync<CharHair::Strand>` | calls `vector<Strand>::_M_erase` and `~Strand` ×2 |
| `0x823A8F98` | — | `CharHair::Strand::Strand` | inlines `ObjPtr<RndTransformable>` (RTTI) at +4 = `mRoot` |
| `0x8257C368` | `??_DUIEventMgr` | `??_DInstarank` | **every** retail reference (MetaPerformer's eh-vector element dtor next to `??0Instarank`, plus 6 funclets) is where we reference `??_DInstarank` |
| `0x8257B428` | — | `Instarank::~Instarank` | called by the above, and by MetaPerformer's dtor where ours calls `~Instarank` |

- Each "now" name was unmapped before, and the net map delta against main is exactly these
  8 rows.
- **Main corroborated this lane's identifications twice, independently:**
  - W16-JD made the same `0x824D02C8 → ??_DWorldDir` / `0x824CE910 → ~WorldDir`
    correction (dropped from this branch as a duplicate);
  - W16-JD named `0x8274A9D0` `??0DataNode@@QAA@ABV0@@Z`, which is the tail-pad
    positive below.

### 3.3 Tool: three one-sided reader artifacts, each with a positive, a decoy and a self-break
All in `tools/icf_pair_adjudicate.py`, general (non-vacuous) path only.

1. **`retail_tail_pad`.** A `.pdata`-less leaf's dtk extent runs to the next symbol, so
   alignment `0x00000000` after `blr` was billed into retail `0x8274A9D0` (DataNode's
   copy ctor, 44 B + 4). The rule requires raw zeros with no relocation in the tail.
   - Decoy: the same body with its pad word replaced by a `nop` ⇒ REFUTED.
   - `--self-break-tailpad` turns the decoy red.
   - A relocated-tail decoy was **dropped**, because `_slots_agree` already implies that
     clause and it cannot be made to fail.
2. **`CLASS_RENAMES = {ObjRefOwner@@: ObjRef@@}`.** W17-OPTR established that our X360
   `ObjRefOwner` is retail's `ObjRef`; the four vtable slots match. The witness is
   re-derived on every use:
   - our spelling has no retail RTTI of its own;
   - the map already names the retail vtable's slot-0 dtor as our class's.

   Its reach was sized by a what-if run first: exactly 3.
   - Decoy: the same body re-pointed at an unrelated retail vtable ⇒ REFUTED.
   - `--self-break-rename` turns it red.
3. **`retail_overcarve`.** dtk split the leaf `0x82B9F540–0x82B9F5C8` (our
   `_Deque_iterator_base<T*>::_M_advance`, whose raw 136 B equal ours) at an internal
   branch target. The rule requires all of: a REL24 to the *immediately next*
   placeholder; raw contiguous bytes equal ours; a **whole-`.text` branch census** with
   every entry into the extent coming from inside; and no `.pdata` start inside.
   - Decoy: the same pair with one outside branch injected into the cached census ⇒
     REFUTED.
   - `--self-break-overcarve` turns it red.
   - ★ **W16-JC then fixed this carve at its root** (`symbols.txt` size 0x88), which is
     independent confirmation of the diagnosis. The positive is now rebuilt from the
     real retail record with only the old split simulated. The rule stays, because 54
     retail bodies have the shape.

Control positives are **keyed by address**. Main's namings stranded hard-coded ones
twice, and each time every leg correctly REFUSED instead of passing vacuously.
- Final controls: `--selftest`, `--chasetest`, `--self-break`, `--self-break-slots`,
  `-tailpad`, `-rename` and `-overcarve` are all rc=0.
- Each new positive's trace shows its rule firing: `RETAIL-TAIL-PAD`,
  `VTABLE-CLASS-RENAMED`, `RETAIL-OVERCARVE`.

## 4. Withdrawn

### 4.1 Refuted on bytes (21, plus 18 in §5)
- 15 `ObjPtrList<vbase T>::clear` in group `0x8249d1f0`: our `Unlink<T>` is PROVEN to be
  `fn_8227D0E8`, not `fn_8271A138`.
- `ObjDirItr<UILabel>` ctor: CD-9.
- 5 in `0x822E5040` (§4.3).

### 4.2 Superseded (5)
`UpdateHeap`, `PropSync<Strand>`, `??_DWorldDir`, `??_DInstarank`, and
`??_EAppScoreDisplay@@$4…BGI@` (re-mapped by W16-JC). In each, the folded spelling was the
address's real name and the survivor spelling was wrong.

### 4.3 ⚠ `0x822E5040` is not `_Copy_Construct<SongPattern>`
Its callee calls `fn_822B1728` twice, and `fn_822B1728`'s vtable reads RTTI
`.?AV?$ObjPtr@VEventTrigger@@VObjectDir@@@@`. The group's own CLEAN members are
`_Copy_Construct` / `_Param_Construct<pair<ObjPtr<EventTrigger>, ObjPtr<EventTrigger>>>`.
The withdrawn members (SongPattern, `HamMove::LocalizedName`, `MoveReplacer`,
`map<int,float>`, `set<MoveParent*>`) copy no ObjPtr. **This is the only withdrawal with
bytes at fuzzy 100: 2 rows / 192 B** (`__uninitialized_copy<SongPattern>` and
`__uninitialized_fill_n<SongPattern>`, almost certainly misnamed rows of the same pair
family). The family's map spellings were **not** renamed (§8).

### 4.4 Unproven, not refuted (166)
Every record carries `slot_evidence`: retail vs our callee sizes, the first differing
instruction, and any retail body byte-equal to our callee. Main groups:
- **`_Rb_tree<T>::clear` (`0x823d9920`)**: retail frees 0x1c-byte nodes (`li r3,0x1c`),
  ours free 0x14/0x18/0x20/0x24/0x34. The record names the retail `_M_erase` each of ours
  *does* equal; that proof needs a self-recursive cycle, which the brief does not admit.
- **`??_GAutomator` (28)**: different classes' destructors.
- **`??_GCMemoryManagedUnknown` (14)**: COL-less vtables of classes with no retail RTTI.
- **`ObjDirItr<T>`**: per-T `dynamic_cast`.
- **3 no longer compiled by us.**

## 5. Memberships main's re-identifications moved out of the audit's reach

W16-JD and W16-JC re-mapped `0x82706100`, `0x82706208`, `0x8276CFD0`, `0x823294D8`,
`0x825722D0` and others, and this lane re-mapped `0x8257C368`. Members of those groups
then read NO-RETAIL / LAX-ALSO-FAILS / NO-OURS, and the placeholder audit no longer keys on
them. Each was decided against the **address's current retail name** with the strict chase:
- **18 refuted**: the named retail callee (`ObjPtr<SeqInst>` / `ObjPtr<Sequence>` copy,
  `_Copy_Construct<ObjPtr<Object>>`, `~LayerDir`, `~Instarank`) has different bytes from
  ours;
- **1 superseded** (`??_EAppScoreDisplay…BGI@`, which W16-JC mapped onto its own
  address);
- **3 unproven**: our spelling or its callee is no longer compiled.

Every rebase regenerated the ledger **with the tool, from main's ledger byte-for-byte**,
and never trusted a text merge: main reordered or changed 408 groups. Structural check
each time: exactly the withdrawals plus 0 other differences. One clean text merge left a
**duplicate map key** (`0x824ce910`) that a JSON loader silently collapses; the map
re-apply now asserts against duplicate keys.

## 6. Measurement

Command: `python3 tools/ab_measure.py --worktree ~/tmp/wt-w16-jh-ab --patch <git diff 545bdf90e w16-jh -- . ':(exclude)docs'>`.
- Fresh scratch worktree at main `545bdf90e`, with the patch built against that
  **explicit** base. An earlier launch built it against `main` just as main moved, so the
  patch spanned 119 files (it would also have reverted W16-JC); that launch was killed
  before it touched the tree.
- `docs/` is excluded: it holds the only new files and no build step reads it.
- Patch kinds `map, source`. Both legs were force re-split, both read at a `symbols.txt`
  fixed point after 0 extra splits, and leg B recompiled 994 objects.
- Run dir: `~/tmp/wt-w16-jh-ab/.ab_measure_runs/20261001-114546-branch_ab-1022980/`.

| | leg A (main) | leg B (w16-jh) | Δ |
|---|---:|---:|---:|
| matched_functions | 48,716 | 48,726 | **+10** |
| masked_equal | 24,175 | 24,175 | +0 |
| honest | 24,541 | 24,551 | **+10** |
| matched_code_percent | 49.338818 | 49.358803 | **+0.019985 pp (+2,048 B)** |
| fuzzy | 57.876728 | 57.876590 | −0.000138 |
| units at 100 (mpn / all-rows-fuzzy) | 335 / 284 | 336 / 285 | +1 (SongSortByRank), **0 fell off** |

**Prediction:** +3 to +12 functions, −200 to +2,000 B. **Measured: +10 / +2,048 B**, just
above the byte range, because of the five rows below that I had not foreseen.

Row-level account (legA vs legB reports): **+2,296 B newly at fuzzy 100, −248 B newly off.**
- **+** The `ObjOwnerPtr<T>` copy-ctor fix: 6 named rows, 91.1 → 100
  (RndTransformable, CharLookAt, EventTrigger, RndAnimatable, RndDrawable, Waypoint).
- **+** Waypoint `PropSync<ObjOwnerPtr<Waypoint>>` and `fn_823DCDB4`.
- **+** `Rnd::UpdateHeap`, now paired by its own name.
- **+ 5 hashtable `resize` / `_M_initialize_buckets` rows, not predicted.** Their
  `_S_next_size` slot used to read the wrong map name `list<FileCache*>::erase` and was
  charged at every caller (a wrong name is financed by its callers).
- **− SongLayout** `__uninitialized_copy` / `__uninitialized_fill_n<SongPattern>`
  (2 × 96 B): the refuted fold of §4.3.
- **− `??_DUIEventMgr` (56 B) was a FALSE 100.** It was paired by name with retail
  `0x8257C368`, which is `??_DInstarank`, with its callee slot forgiven as a placeholder.
  The correctly named row is pinned in the UIEventMgr unit, so it reads 0%: a re-home
  candidate (§8).
- Paired newly at <100: `Strand::Strand` 54.7%, `~Instarank` 65.8%. These are real
  divergences in our ports, now visible instead of hidden behind anonymous rows.

A first whole-branch A/B, on the `8091a837c` base before the Instarank correction,
measured +9 / +396 B and showed MetaPerformer −2. That reading is what exposed the census
defect of §2.

## 7. Gates (branch worktree on `545bdf90e`, fully built)

- `tools/map_name_injectivity.py`: **OK**, 32,648 applied rows, injective.
- `tools/icf_alias_finder.py --validate`: **PASS**, 1,609 map-consistent / 268 tolerated /
  **0 contradicted** / 1,878.
- `scripts/verify_objs_patched.py --verify-manifest`: **OK** (1,249 decomp + 3,123 target).
- `tools/native_build_gate.sh`: run on the final rebased tree, after the last `src/` change:
  `NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0`.
- Tool controls: all seven rc=0. Placeholder audit: 0 CONTRADICTED / 0 UNDISCHARGED.

## 8. Open, reported and not acted on

- **Misnamed map families.**
  - `0x822E5040` and its `vector<SongPattern>` relatives carry SongPattern names on a
    `pair<ObjPtr<EventTrigger>,ObjPtr<EventTrigger>>` body.
  - `0x823aad78`, `0x823aade0` and `0x823ab598` carry `Flow::DynamicPropertyEntry` names
    inside CharHair's range.

  Each needs its own caller census.
- **Two corrected addresses are pinned into the wrong unit, so their correctly named rows
  read 0%.** A re-home would pair them:
  - `0x823AAE58` `PropSync<Strand>` is pinned in `Flow.cpp` (it was 99.7 under the wrong
    name);
  - `0x8257C368` `??_DInstarank` is pinned in `UIEventMgr`.
- **Our ports still diverge** from bodies now named and paired:
  - `Strand::Strand` 54.7%: retail inlines `ObjPtr` and CharHair cannot take the TU-wide
    lever (W16-JB);
  - `~Instarank` stores vtables that retail does not;
  - `~WorldDir` lacks a member delete.
- **`lbl_82F14008`**, a DC3 address used as a global's name in `rndobj/Rnd.cpp`, is never
  defined in the match build. Retail's heap index is at `0x82C6FB90`, initial value −1.
- **The generator's flat T1 still tolerates placeholder slots** (JG §7), and the three
  new rules live only in the chase.
- **166 UNPROVEN withdrawals are restorable**; each record says what differs.

## 9. Files

- `src/system/utl/MakeString.cpp`, `src/system/obj/ObjPtr_p.h`, `src/system/char/Waypoint.cpp`
- `scripts/target_symbol_map.json` (8 rows), `scripts/symbol_aliases.json` (210 withdrawals)
- `tools/icf_pair_adjudicate.py`: `retail_tail_pad`, `CLASS_RENAMES` +
  `_rename_witness`, `retail_overcarve` + branch census, address-keyed controls,
  `--self-break-{tailpad,rename,overcarve}`
- `tools/alias_placeholder_slot_audit.py`: `--withdraw-undischarged`, `--extra`,
  `--lane`, `slot_evidence`
- `docs/decomp/W16JH_alias_decisions.json`: the 31 decided withdrawals beyond the
  automatic ones
- Working outputs (not committed): `~/tmp/w16jh/`
  - `depcensus2.py`, the repaired census;
  - `audit*.json`, `ab_whole*.log`, `ctl*.log`, `gate_*.log`, `native_gate*.log`.

## 10. Landing note (for whoever merges)

Measured on main `545bdf90e`. Main has since moved to `e762a9298` (W16-JA), which again
changes `scripts/symbol_aliases.json` and `scripts/target_symbol_map.json` and neither
source file of this branch. Do **not** trust a text merge of either ledger; this lane hit
a silent duplicate map key that way. The rebase recipe used here, three times:
1. Rebase. For a map conflict, take HEAD's file and re-apply that commit's key-level delta
   (`~/tmp/w16jh/resolve_map.py`; it asserts no duplicate keys). For a ledger conflict,
   take HEAD's file.
2. `git show <main>:scripts/symbol_aliases.json > scripts/symbol_aliases.json`, force a
   re-split (`rm build/45410914/target_symbol_renames.stamp; touch config/45410914/config.yml`),
   then run `./tools/ninja-locked`.
3. Run `python3 tools/alias_placeholder_slot_audit.py --out A.json --apply --withdraw-undischarged --extra docs/decomp/W16JH_alias_decisions.json --lane "W16-JH 2026-10-01"`.
   Then structurally diff against main's ledger: exactly N folded removals + N records,
   0 other differences, and every withdrawal one of W16-JG's 341.
4. Re-audit, expecting 0 CONTRADICTED / 0 UNDISCHARGED. Re-account the 341: anything
   main's re-mappings moved out of reach must be decided against the address's current
   name (§5). Then re-run all seven tool controls.

## 11. Re-landed on main `e762a9298` (W16-JA), using the §10 recipe

- **Rebase.** One map conflict, resolved by re-applying the key-level delta. The net map
  delta against main is now **7 rows, 0 duplicate keys**: W16-JA independently corrected
  `0x8240F0C8` to `?UpdateHeap@Rnd@@IAAXXZ` (`9fbf41cc4`). That is the third time main
  has independently made one of this lane's identifications.
- **Ledger.** Restored to `e762a9298`'s byte-for-byte, then a forced re-split rebuild
  (3,123 target objs, 1,861 renamed), then `--apply --withdraw-undischarged --extra …`.
  - Exactly **210** removals + 210 records, 0 other differences, and every one is one of
    the 341.
  - The regenerated ledger is **byte-identical** to the rebase's own result.
- **Audit:** 0 CONTRADICTED / 0 UNDISCHARGED; the 341 split as 131 proven + 210 withdrawn.
- **Dependent rows:** unchanged, 3 (repaired census).
- **Gates:** injectivity OK (32,780 rows); validator PASS (1,609 / 268 / **0
  contradicted** / 1,878); manifest OK. All seven tool controls rc=0, and each new
  positive's trace shows its rule firing.
- **Whole-branch `ab_measure` against `e762a9298`:** **Δmatched +9 / Δhonest +9 /
  +1,908 B (+0.018619 pp)**, 0 units off 100, SongSortByRank reaches 100.
  - Predicted: +9 / +1,908, i.e. §6's +10 / +2,048 minus the UpdateHeap row
    (1 fn / 140 B) that main now supplies. Measured exactly that.
  - Run dir: `~/tmp/wt-w16-jh-ab/.ab_measure_runs/20261001-120106-branch_ab5-1190798/`.
- **Native gate:** run last, after this doc commit; result in the commit that follows.
