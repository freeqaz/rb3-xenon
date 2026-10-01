# W16-JG — every landed alias membership audited for the placeholder-slot hole (2026-10-01)

Lane W16-JG, branch `w16-jg`, worktree `~/tmp/wt-w16-jg` off main `99b26594d`. Not merged.

## 1. The hole, and what was asked

Lane W16-JE (`docs/decomp/W16JE_NAME_ONLY_RELOC_PAIRS_2026-10-01.md` §3) found that
`tools/icf_pair_adjudicate.py --chase` takes any relocation slot with an **unnamed
retail target** (`fn_X`, `lbl_X`, `vftable_X`) on trust. It does this on its general
path, and flat T1 (`icf_alias_build.relocs_agree`) does the same. In a ctor or dtor
that slot is the **vtable**, the one field that names the type. In a template
wrapper it is the **unnamed retail callee**. JE found 11 bad folds that way in its own
~300 pairs and left the tool unchanged.

This lane did three things:
1. audited every membership in `scripts/symbol_aliases.json` (5,885 live at the start);
2. withdrew the refuted ones with a record, pruning nothing;
3. closed the hole in the tool, with controls that are shown to fail.

## 2. Census of the hole (before any rule was written)

A hooked run of the real `chase` logged every slot that the general path tolerated,
and a no-break depth-0 sweep logged every placeholder slot.

| | value |
|---|---:|
| live memberships | 5,885 |
| slot-aligned at depth 0 (equal masked bytes + reloc shape) | 5,161 |
| not compiled by us / survivor outside every pinned span / bytes differ | 456 / 256 / 12 |
| tolerated placeholder slots (all depths) | ~3,000 |

The tolerated slots, by kind:

| slot kind | slots |
|---|---:|
| data global (`?gChunkAlloc` alone accounts for 1,352) | ~1,400 |
| unnamed retail callee (662 at depth 0, 133 nested) | 795 |
| vtable (`lbl_`/`vftable_` against our `??_7`) | 560 |
| `__real@` float/double constants | 151 |
| string literals | 78 |

That is about 10× JE's sample.

## 3. The rule each slot kind gets (now in the tool)

`discharge_slot()` in `tools/icf_pair_adjudicate.py` returns OK, CONTRADICTED or
UNDISCHARGED. **A PROVEN verdict now needs every slot discharged.**
**CONTRADICTED needs positive retail-side evidence;** a slot that merely fails to
prove is UNDISCHARGED.

| our side | retail check |
|---|---|
| `??_7X` vtable | retail COL (word at vt−4) → TypeDescriptor name must be `X`. If retail's vtable has **no COL**, our class's own vtables are located (name → TD → every COL → every vtable) and must not include the slot. |
| `??_R0` type descriptor | retail descriptor name string |
| `??_C@` literal | decoded mangling (length + first 32 bytes) vs retail bytes + NUL |
| `__real@` constant | the value in the name vs retail bytes |
| callee `C` vs retail `fn_X` | recursive `chase(fn_X, C)`; if it fails, CONTRADICTED only if **our `C` is PROVEN (clean chase) to be a different retail body `Y`** — `/OPT:ICF` folds identical COMDATs, so retail's copy of that code lives at `Y`, not `X` |
| one of our names at two retail addresses inside one proof | CONTRADICTED (one COMDAT, one address) |
| other data global | accepted with a record, plus one-name-one-address consistency inside the proof and across the whole ledger (measured: **0** names at two retail addresses) |

### Four over-reaches caught on retail bytes while building the rules

Each of these was a candidate refutation rule that fired on real data before it was
corrected. The figures are what each wrong rule produced.

1. **Our build's distinctness is not retail's.** The first callee rules refuted when
   our survivor's callee and our folded spelling's callee were different code in OUR
   build ("anchored" / "pigeonhole"). That fired on 41 MakeString memberships.
   Retail `MakeString<const char*,const char*>` (0x8229d148) calls `fn_827C40E8` at
   the slots where we call `operator<<(int)` and `operator<<(const char*)`, and JE
   recorded retail `MakeString<int,int,int,int>` calling the same address four times.
   So retail's `FormatString::operator<<` overloads are **one folded body**: on PPC
   the int and the pointer reach `_snprintf` in the same GPR. Our FormatString
   diverges (our `operator<<(int)` is 208 B against retail's 124), which makes our
   overloads differ. Both rules now return **UNDISCHARGED**; only
   CALLEE-LOCATED-ELSEWHERE refutes.
   ⚠ This also weakens JE's own `MakeString<int,int,int,int>` vs
   `MakeString<const char*,u64,…>` refutation (JE §3/§7), which read "one retail
   `operator<<` called four times" as proof of a different instantiation.
2. **Template arity.** Retail's `ObjPtr`/`ObjOwnerPtr` carry a second template
   argument (`ObjectDir`, the default), and ours do not. So
   `.?AV?$ObjPtr@VEventTrigger@@VObjectDir@@@@` and our `?$ObjPtr@VEventTrigger@@@@`
   are the same class. This accounted for 18 of 20 nested RTTI "contradictions".
   Fix: a trailing `VObjectDir@@` argument is normalized away. Any other
   arity-only difference is UNDISCHARGED.
3. **DC3-only intermediate bases.** Our `~ObjOwnerPtr` stores `ObjRefOwner`'s
   vtable, while retail stores `ObjRef`'s. `ObjRefOwner` is a DC3 class that RB3 does
   not have. An RTTI mismatch therefore refutes only when our class **exists in
   retail** and is **unrelated** to retail's class in retail's own Class Hierarchy
   Descriptor (COL+16). The COL-less branch refutes only when none of our class's
   retail bases is itself COL-less.
4. **Addresses in names.** `?lbl_82F14008@@3HA` is a global a porter named after a
   (likely TU0) address, so the address in the name is not a claim about retail.
   It is UNDISCHARGED, not refuted.

Known-answer control for the vtable locator: it reproduces the three vtables that
W16-HZ found by hand, Mic 0x820fe2dc, NetworkSocket 0x8208d484 and
TrackWatcherParent 0x8210f2fc, each with matching RTTI.

## 4. Result of the audit (final rules, main `99b26594d` tree)

| verdict | memberships |
|---|---:|
| CLEAN (every placeholder slot discharged) | 4,335 |
| CLEAN-CYCLE (only through a coinductive cycle; pre-existing class) | 65 |
| **CONTRADICTED → withdrawn** | **152** |
| UNDISCHARGED (rested on the hole; now unproven, **not** withdrawn) | 341 |
| LAX-ALSO-FAILS (admitted on other tiers; the hole was not what held them) | 268 |
| NOT-ALIGNED / NO-RETAIL / NO-OURS | 12 / 256 / 456 |

The 152 withdrawals by class (recorded as `PLACEHOLDER_SLOT_*`; lane
`W16-JG 2026-10-01`; groups kept):

| class | n | example |
|---|---:|---|
| VTABLE_OF_CLASS_ELSEWHERE | 89 | all in the `??_GCMemoryManagedUnknown` group (0x82bf6f58), detailed below |
| CALLEE_LOCATED_ELSEWHERE | 40 | `??_DUIEventMgr` ← `??_DCharBlendBone`: our `~CharBlendBone` is PROVEN to be retail `fn_823C4630`, not `fn_8257B428`. Our `operator<<(Color)` is PROVEN to be retail `operator<<(Vector4)`. |
| VTABLE_RTTI_DIFFERS | 13 | `??1StartLockMsg` ← `??1AccomplishmentEarnedMsg`; `??0UIListCustom` ← `??0UIListSubList`; `ObjDirPtr<ObjectDir>` ← `ObjDirPtr<UILabelDir>`; 3 are nested inside a callee chase (`ObjPtr<RndMesh>` vs ours `ObjPtr<EventTrigger>`; `ObjPtr<Object>` vs `ObjPtr<Task>`) |
| MAPPED_VS_PLACEHOLDER (CD-9) | 10 | |

The 0x82bf6f58 group, retail-disassembled: the body does `lis r11,0x821b`,
`addi r11,r11,-0x2ebc` (= `0x821ad144`), `stw r11,0(r31)`, then
`if (flags & 1) bl operator delete`. The word before `0x821ad144` is a code pointer,
so that vtable has **no COL** (a `/GR-` class). Each folded `??_GX` stores `??_7X`
instead, and `X`'s own COL-bearing retail vtable is elsewhere. The `StackString<N>`
members store `TextStream`'s (0x82000e8c / 0x821cecec). This is the
RTTI_PROVES_OWN_ADDRESS method that W16-HZ, W17-ANON2 and W16-IF applied by hand to
six members of this group, now applied mechanically. 14 members remain
UNDISCHARGED: their classes are not in retail RTTI at all (DC3-only gesture
filters, etc.).

0x82b8c8c8 (`??_E?$StandardEffect…` survivor, 12 folded constructors): retail RTTI
says the body is **`UGCNet_Server`'s constructor**. 9 of the 12 are withdrawn:
- 6 by CD-9 (their base-ctor callees are map-resident elsewhere);
- 2 by RTTI (`ClientProtocol`, `DxShaderInclude`);
- 1 located elsewhere (`AnimPtr`).

The other 3 (`DisplacementNode`, `Ham1FilterVersion`, `Ham2FilterVersion`, all
DC3-only classes) stay folded as UNDISCHARGED. Nothing in retail can be compared
against them, and that is not a refutation.

**122 of the 152 supersede a restoration**, 62 by W16-CU (2026-09-15) and 60 by
W16-DB (2026-09-16). Both restored on "flat L1_T1 (byte identity with relocation
TARGET NAMES compared)". But `relocs_agree` tolerates placeholder targets, so that
proof never compared the slot decided here. Each record carries
`supersedes_restoration` and `why_the_restoration_does_not_stand`. ALIAS-REPAIR's
original withdrawal (an our-build congruence predicate) was right for these
memberships, though for a reason W16-CU correctly called unsound.

Withdrawals counted by spelling: 152 records over **135 distinct spellings**.
**67** of them remain folded in some other group (which this audit did not
contradict); **68** are now forgiven nowhere.

Re-audit of the updated ledger: **0 CONTRADICTED** (fixed point).

## 5. Measurement

`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16-jg-ab --patch <git diff 99b26594d w16-jg>`.
This is a fresh worktree at main. Patch kind is `map`, so both legs were force
re-split, both reached a `symbols.txt` fixed point after 0 extra splits, and leg B's
renamer patched 1,853 files. Run dir:
`~/tmp/wt-w16-jg-ab/.ab_measure_runs/20261001-091315-branch-3965151/`.

| | leg A (main) | leg B (w16-jg) | Δ |
|---|---:|---:|---:|
| matched_functions | 47,759 | 47,759 | **+0** |
| masked_equal | 23,822 | 23,822 | +0 |
| honest | 23,937 | 23,937 | +0 |
| matched_code_percent | 48.044130 | 48.044130 | **+0.000000 pp (+0 B)** |
| fuzzy | 56.697243 | 56.697243 | +0.000000 |
| units at 100 (mpn / all-rows-fuzzy) | 299 / 256 | 299 / 256 | 0 reached, 0 fell off |

`none` control: +0 B.

**Prediction failed:** I predicted −5 to −40 functions. The Δ0 is real, not
absent-vs-absent. Leg B's `icf_aliases.map` had dropped every withdrawn spelling
except the ones still folded in another group, and its ledger was byte-equal to the
branch's.

The explanation was measured directly. Over every function row present on both
sides, there are **0** sites where retail names a withdrawn membership's survivor
and we name the withdrawn spelling at the same offset. The same census over the
5,733 live memberships finds **9,637 sites in 5,244 rows**, so the census can find
sites. The refuted memberships were forgiving nothing. Most are `??_G` reached only
through vtable slots, or template wrappers whose callers do not pair. A separately
built branch worktree reads 47,759 / 4,923,120, identical to main.

⚠ **`ab_measure` defect observed, not fixed here:** the run ended rc=0 with a
"COULD NOT RESTORE THE WORKTREE" banner. `action: restored`, but both patched files
were still modified, and the patch's one new file
(`tools/alias_placeholder_slot_audit.py`) had never been created ("modified:"
listed only the two existing files). The pre-run state was clean (a 0-byte saved
diff) and was recovered with `git checkout --` in that scratch worktree. A new-file
patch is the likely trigger.

## 6. Gates (branch worktree, fully built)

- `tools/map_name_injectivity.py`: **OK** (32,159 applied rows, injective).
- `tools/icf_alias_finder.py --validate`: **PASS**, 1,618 map-consistent /
  254 tolerated / 0 contradicted / 1,873. The classification is **identical on main
  `99b26594d`** (re-run in the A/B worktree with main's ledger). JE's log read
  1,620 / STALE 101, and that difference is pre-existing drift on main, not this
  patch.
- `scripts/verify_objs_patched.py --verify-manifest`: **OK** (1,242 decomp + 3,120
  target objects).
- `tools/native_build_gate.sh`:
  `NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0`.
  This was run after the last ledger and tool change; nothing under `src/` changed
  on the branch.
- Tool controls, all rc=0: `--selftest`, `--chasetest`, `--self-break`, and the new
  `--self-break-slots`. Under `--chasetest` all 5 slot decoys read **REFUTED**:
  - JE's documented `~ObjPtr<SeqInst>` vs `~ObjPtr<Sequence>`;
  - one per withdrawal class, read back from the ledger, each required to be
    **PROVEN by the old lax rule**.

  The 2 slot positives (a vtable discharged by RTTI, a callee discharged by chase)
  read PROVEN. Under `--self-break-slots` (old rule restored) **all 5 decoys go red
  and no other control moves.** A ledger class with no lax-PROVEN decoy is now a
  refusal: one control briefly vanished silently after a record-class rename, and
  that is what forced the change.

## 7. What is still open (reported, not acted on)

- **341 UNDISCHARGED memberships in 69 groups**, carrying **293 paired-row sites in
  219 rows**, of which **189 rows / 66,628 B are at fuzzy 100.** Kinds:
  - CALLEE-UNPROVEN-OURS-DISTINCT 204: our build says the survivor's and the folded
    spelling's callees differ, but the survivor's callee is not proven against retail
    either. The MakeString family (0x8229d148 / 0x8229d1a8 / 0x823de3c0) is here.
  - CALLEE-UNPROVEN 110.
  - Vtable cases 26: our class is not in retail RTTI, the retail vtable is COL-less,
    or the classes are related.

  Largest groups: `_Rb_tree<Symbol>::clear` 0x823d9920 (51),
  `ObjPtrList<EventTrigger>::clear` 0x8249d1f0 (29), `??_GAutomator` 0x823f5270
  (28), `CharLipSync::Handle` 0x823d3918 (20). Each needs a retail-side argument in
  one direction or the other; "not proven" was not treated as "refuted".
- **268 LAX-ALSO-FAILS**: memberships the old chase also fails, admitted on T2/T3,
  thunk or allocator gates. Out of this hole's reach.
- **The generator's flat T1 (`icf_alias_build.relocs_agree`) still tolerates
  placeholders.** Only `icf_pair_adjudicate` was changed. The withdrawal ledger
  (`alias_withdrawals.load_ledger`) keeps the generator from re-proposing the 152,
  but new T1 admissions should be routed through `discharge_slot`.
- `tools/incomplete_group_adjudicate.py` / `incomplete_group_control.py` call
  `chase()` with defaults and are therefore now strict too. Their own controls were
  not re-run by this lane.

## 8. Files

- `tools/icf_pair_adjudicate.py`: `discharge_slot`, `retail_rtti_name`,
  `retail_vtables_of`, `retail_bases`, `locate_retail`, `ours_distinct`,
  `decode_strlit`, `slot_controls`; flags `--self-break-slots` and `--lax-slots`.
  `SLOT_POLICY` defaults to `"discharge"`.
- `tools/alias_placeholder_slot_audit.py` (new): the audit; `--apply` writes
  withdrawal records and re-emits the ledger byte-for-byte outside the change
  (verified structurally: exactly 152 `folded` removals + 152 appended records,
  0 other differences).
- `scripts/symbol_aliases.json`: the 152 withdrawals.
- Working outputs (not committed): `~/tmp/w16jg/` (`census.py`, `audit*.json`,
  `ab_whole.log`, `ctl*.log`, `gate_*.log`, `native_gate.log`).
