# W16-KF — alloc_fold_gate re-keyed on the global operator new; the 123-member allocator-thunk group adjudicated on retail bytes (2026-10-01)

Branch `w16-kf`, worktree `~/tmp/wt-w16-kf`, base `main f6479f395`. **Not merged to main.**

## 1. Brief and what was true of it

W16-KD renamed retail `0x827BD2F0` (`li r4,0; b MemAlloc`, fan-in 1,048) from `??2CriticalSection@@SAPAXI@Z`
to the global `??2@YAPAXI@Z` and re-keyed group `operator_new_alloc_thunk` (123 folded). Two follow-ups were
asked: make `tools/alloc_fold_gate.py` work against the new survivor without weakening its controls, and
adjudicate all 123 memberships on retail bytes, withdrawing the unproven ones with records.

Measured before changing anything:

| claim in the brief | measured |
|---|---|
| gate refuses with "our objs do not define the survivor spelling" | reproduced. Cause: `SURVIVOR` was a **hardcoded constant**, `??2CriticalSection@@SAPAXI@Z`, which KD made `#ifdef HX_NATIVE`. The refusal was about the constant, not about evidence. |
| "MemMgr.cpp defines the global operator new as an ordinary function, **not a COMDAT**" | **half right.** Under `/Gy` it **is** in a COMDAT section (`IMAGE_SCN_LNK_COMDAT` set). What differs is the selection: **1 = NODUPLICATES**. Every inline class allocator is **2 = ANY** (`??2Accomplishment` in BandSwatch.obj). `comdat_bytes.comdats()` already read its body: 8 B, `38800000 4bfffffc`, one reloc to `?MemAlloc@@YAPAXHH@Z`. |
| strict `icf_pair_adjudicate.py --chase` refutes 42 under both old and new survivor | **new survivor: reproduced, 81 PROVEN / 42 REFUTED.** The *old*-survivor leg is **vacuous on this tree**: the target obj no longer carries `??2CriticalSection`, so all 123 read REFUTED via `MISSING(retail)`. It agrees with nothing and is evidence of nothing. |

## 2. Gate changes (`tools/alloc_fold_gate.py`, `tools/comdat_bytes.py`)

- **Survivor read from the map** at `SURVIVOR_VA`. It refuses if the address is unnamed.
- **`comdat_bytes.py`** gains two additive keys, `section_comdat` and `comdat_select`, the latter read from
  the section-definition symbol's aux byte 14. No existing key changes meaning, and
  `tools/test_fold_gate_function_extent.py` still passes.
- **New refusals, each strictly narrower than before:**
  - Survivor in a **non-COMDAT** section ⇒ REFUSE ALL. `/OPT:ICF` folds only COMDATs, so nothing can have
    folded onto plain `.text`; identical members would fold among themselves, at an address of their own.
  - **NODUPLICATES** survivor with more than one definer ⇒ REFUSE ALL (LNK2005: our objs are not one program).
  - **Member** in a non-COMDAT section ⇒ not foldable.
  - **Destination name.** When the map names retail's branch target (`0x827bcd38` = `?MemAlloc@@YAPAXHH@Z`
    since MAPID-1), our survivor's relocation must name the same function.
- **`--audit-members`** re-adjudicates every installed membership. The install path never re-asked this,
  which is how 39 memberships rotted (§3). It uses three legs:
  - **BODY**: our COMDAT is one variant, is a COMDAT, and is byte- and reloc-identical to our survivor.
  - **SITES**: retail call sites from the dtk-split target objs, paired by caller name with ours (units from
    `objdiff.json` via `icf_site_census`). Only the **strict** tier decides: identical size and
    `(offset, type)` sequence. The bl-index tier is printed but never decides. Measured, it mis-slots
    `CharLipSyncDriver::Sync` by one call: retail has an extra `ObjPtr::Release` at +0x60, so the tier
    reads retail `??3BinStream` against our `??2PlayBack@CharLipSync`. Realigned, it is a witness
    (retail +0xa4 `??2@YAPAXI@Z` ↔ ours +0x8c `??2PlayBack`).
  - **NEWOBJECT**: retail `?NewObject@<Class>@@` when map-resident. THUNK means it calls `0x827BD2F0`;
    INLINED means it calls `StaticClassName@<Class>` and `MemAlloc` itself.
- **`--withdraw`** moves non-PROVEN memberships to `withdrawn` with a generated evidence record. Nothing is
  deleted. `NO_DEFINITION` records carry `"regate": true` (unproven, not refuted); `--install` honours the
  old never-re-admit ban for every other record.
- **`--install` semantics changed deliberately.** It used to rebuild the group from scratch, which silently
  dropped later lanes' members and evidence text (W16-M's notes). It now unions into the existing group, and
  removal goes only through `--withdraw`, which leaves a record.
- **The JSON writer uses `ensure_ascii=False`.** The first `--withdraw` re-encoded 22 other groups' evidence
  (`…` → `…`). The file round-trips byte-identically under the new setting; that run was discarded
  (`a88890dc9`).

### 2.1 The controls, and proof each can fail

`--selftest` runs 12 controls. `--self-break CHECK` removes one check and exits 0 **only if** every control
tagged with that check goes red **and no other control moves**. Run at the branch tip after the full rebuild:

| check removed | control that went RED | anything else move? |
|---|---|---|
| `retail` | retail survivor word0 altered ⇒ accepted | no |
| `dest` | our survivor relocating to `_MemAllocTemp` ⇒ accepted | no |
| `comdat` | survivor in non-COMDAT section ⇒ accepted; `??2Accomplishment` moved to non-COMDAT ⇒ IDENTICAL | no |
| `nodup` | NODUPLICATES survivor defined in 2 objs ⇒ accepted | no |
| `relocs` | **in-family decoy** `??2AsyncFileWin` (same 8 bytes, `b _MemAllocTemp`) ⇒ IDENTICAL | no |
| `sites` | `??2Accomplishment` + injected strict retail contradiction ⇒ PROVEN | no |
| `newobject` | `??2Accomplishment` + injected INLINED NewObject ⇒ PROVEN | no |

All seven printed `self-break <check> OK`. Positive controls cover three cases: the live survivor
corroborates; a strict-witnessed member reads PROVEN; and the live NewObject classifier must return
**both** THUNK and INLINED on real retail bytes.

⚠ **One control went vacuous mid-lane, and I fixed it rather than relaxing it** (`127d159ce`). After `--withdraw` removed every member whose NewObject
inlines, "classifier says INLINED somewhere" (population: *folded* members only) went RED at the tip. The
cleanup this classifier drove had emptied its own control population. That population is now folded plus
withdrawn spellings, giving INLINED 45 (this lane's 41 plus ALIAS-2's four) and THUNK 1. All seven
self-breaks were re-run after the fix.

## 3. Adjudication of the 123 memberships

`python3 tools/alloc_fold_gate.py --audit-members` (pre-withdrawal):

| verdict | n | retail-byte evidence |
|---|---:|---|
| PROVEN [SITE] | **44** | body identical; **143** strict-aligned retail call sites name `0x827BD2F0` where ours names the member, plus 19 at bl tier; **0** strict contradictions |
| PROVEN [BODY-ONLY] | **35** | body identical to retail's survivor body; no aligned call site to witness (6 have bl-tier-only witnesses: Friend, HttpGet, Interpolator, NetCacheLoader, two `??_U`) |
| CONTRADICTED_BODY → `BODY_CANNOT_FOLD` | **39** | `OBJ_(NEW_)MEM_OVERLOAD` classes. Our operator new is the **60-B** `StaticClassName().Str()` + `MemAlloc(s,0)` body, and different-size COMDATs cannot fold. **39 / 39 retail NewObjects INLINE** `bl StaticClassName@<Class>` + `bl MemAlloc`, and **0 of 651** retail 60-B functions have that body shape. Retail has no out-of-line copy to fold. |
| CONTRADICTED_BODY → `FOLDS_ELSEWHERE` | **1** | `??2AsyncFileWin`: same 8 bytes, but `b _MemAllocTemp`. Retail's only `li r4,0; b _MemAllocTemp` body is **`0x82C30B00` (`OggMalloc`)**, a different address. The header already said retail inlines it. |
| CONTRADICTED_NEWOBJECT → `RETAIL_NEWOBJECT_INLINES` | **2** | `??2EventAnim`, `??2RndEnvAnim`: our COMDAT **is** byte-identical to the survivor (chase PROVEN), but retail NewObject (`0x824AAB10`, `0x82410290`) inlines StaticClassName + MemAlloc. **This is a source defect the alias was forgiving** (§4). |
| NO_DEFINITION (`regate: true`) | **2** | `??2AsyncFileHolmes`, `??2BmpCache@HiResScreen`: no compiled obj defines them. Unproven, not refuted. |

**44 withdrawn, 79 remain folded.** Every withdrawn spelling had **zero** aligned call sites in any paired
caller, so before measuring I predicted **Δ0 B** for the withdrawals.

★ **The chase and the gate missed the same two rows for the same reason.** Body identity proves our COMDAT
*would* fold if it existed in retail; it cannot show that retail emits an out-of-line allocator for that
class at all. The NewObject leg is what separates EventAnim/RndEnvAnim from the 79.

## 4. Source fix exposed by the audit

`src/system/world/EventAnim.h`, `src/system/rndobj/EnvAnim.h`: `NEW_OVERLOAD` → `OBJ_NEW_OVERLOAD` (the
retail "shape (b)" already used by BandButton/BandCamShot), with each class's delete macro unchanged.
Predicted: `?NewObject@EventAnim` and `?NewObject@RndEnvAnim` go 86.93 → 100 (112 B each). Measured on a
full build: **100.0 / mpn 100.0, both.**

## 5. Whole-branch A/B

`tools/ab_measure.py --worktree ~/tmp/wt-w16-kf --patch <git diff f6479f395..83c6bc05f>`. The worktree was
detached at main for the run; kinds were `map` and `source`, with forced re-splits on both legs. Both legs
sat at the split fixed point; leg B recompiled 14 objs and the renamer patched 1,856.

```
leg A: matched=49551 masked=24323 honest=25228 code%=50.907200
leg B: matched=49553 masked=24323 honest=25230 code%=50.909390
Δmatched=+2  Δmasked_equal=+0  Δhonest=+2  Δcode%=+0.002190pp  Δcode_bytes=+224
units at 100% [mpn]: 376 -> 377 (default/system/world/World, MATCHED_ROSE)
[control none] Δmatched_code=+224 B (default ruler +224 B)
```

Leg A equals W16-KD's recorded final (49,551). The prediction was +2 / +224 B for the source fix plus Δ0 for
the withdrawals, and it **landed exactly**. The decomposition is by construction, not inference: the `none`
ruler cannot see alias membership, and it moved the same +224 B as `name_check`. The 44 withdrawals are
therefore worth **exactly 0 B** on the graded ruler. They forgave nothing; they were wrong.

The patch measured was `f6479f395..83c6bc05f`. The one later code commit (the selftest population fix) is
tools-only and not a build input.

## 6. Gates (branch tip, after a full build)

```
[map-injectivity] OK: 32899 applied rows, 32898 distinct names, injective (+1 enumerated internal-linkage exception(s))
VALIDATE: PASS -- 1644 map-consistent, 271 tolerated (enumerated above), 0 contradicted, 1916 total
[patch-state] OK: tree is a fixed point of 6 post-compile passes
alloc_fold_gate --selftest: PASSED -- 12 controls; --self-break x7: all OK
```

VALIDATE moved 1643/272 → 1644/271 (one group from tolerated to map-consistent), with 0 contradicted on
both sides. The native gate line is in §8.

## 7. Not done

- **Not merged to main**, per the brief.
- `??2PropertyEventProvider` and `??_UVert@RndMesh` gate-ADMIT but are not installed. W16-M left them
  unadjudicated and they are outside this lane's 123.
- The 35 BODY-ONLY memberships stay folded. Their body equals retail's survivor body including the
  relocation name, which is the house's chase standard. The gate reports the weaker grade so a later lane
  can look for a call-site witness.
- The 39 `OBJ_MEM_OVERLOAD` classes' out-of-line 60-B COMDATs are emitted by us and referenced by no
  paired caller. I did not try to suppress them, because they cost no row.
- No edits to `symbols.txt`, the map, or PCH inputs. No permuter.

## 8. Native gate

Run at code tip `127d159ce`, after every code commit; only this docs-only commit follows:

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

## 9. Commits

| commit | what |
|---|---|
| `b40f06e19` | gate: survivor from the map, COMDAT-ness checks, `--audit-members` / `--withdraw` / `--selftest` / `--self-break`; `comdat_bytes` keys |
| `a88890dc9` | gate writes `symbol_aliases.json` with `ensure_ascii=False` |
| `91d7c78e4` | aliases: withdraw 44 of 123 with records |
| `83c6bc05f` | EventAnim / RndEnvAnim: `OBJ_NEW_OVERLOAD` (+2 fns / +224 B) |
| `127d159ce` | selftest: live NewObject control population = folded + withdrawn |
