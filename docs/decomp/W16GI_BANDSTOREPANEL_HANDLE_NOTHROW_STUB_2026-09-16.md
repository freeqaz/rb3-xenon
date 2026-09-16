# W16-GI — `BandStorePanel::Handle`: the 3-insert/3-delete "scheduling residual"
# was the CALLEE's empty stub being proven nothrow — ported body, row at 100

Date 2026-09-16 · base main `85b84e32` · worktree `~/tmp/wt-w16gi` · branch `w16-gi`
Ruler: **graded `name_check`**, resolved from `report.json`'s own `provenance`
(`run_objdiff` self-labelled GRADED and agreed with `report.json` on every read).
Every number below is from a **full `./tools/ninja-locked`** (never a single
`.obj`), logs in `~/tmp/rb3_build_w16gi_{settle1,t1,t2,c1,final}.log`; the
whole-binary delta is from `tools/ab_measure.py --from-dirty`
(`~/tmp/w16gi_ab_t1.log`, both legs settled to zero work).

## Verdict

**CROSSED. `?Handle@BandStorePanel@@UAA?AVDataNode@@PAVDataArray@@_N@Z`
(1,928 B) reads fuzzy 100.0 / mpn 100.0 in `report.json`. +1 function /
+1,928 B whole-binary, no other row moved.**

The fix is not in `Handle` at all. It is a real body for
`BandStorePanel::OnMsg(const LocalUserLeftMsg &)`, whose same-TU empty stub
was the defect. This is the fourteenth instance of "a scheduling residual is
the symptom of a source-shape defect" and the second of the specific
**empty-stub → nothrow → caller codegen changes** mechanism
(memory `project_oracle_fidelity_has_four_modes_2026-08-17.md`).

## Measured state, before

    row ?Handle@BandStorePanel@@UAA?AVDataNode@@PAVDataArray@@_N@Z
    size 1928   fuzzy 98.75519   mpn 98.75519   0 diff_arg
    charges: 3 insert (idx 396,400,402) + 3 delete (idx 404,405,406)
      addi r5,r31,0x58 / subi r4,r26,0xec / addi r3,r31,0x68
    retail: the three are CONTIGUOUS immediately before bl OnMsg(LocalUserLeftMsg&)
    ours:   the three are interleaved into the stack LocalUserLeftMsg construction

No `diff_arg`, so there was **no callee question and no alias to install**.

## Diagnosis — the within-function control that gave it away

`Handle` has two `HANDLE_MESSAGE` arms with the same shape:

    HANDLE_MESSAGE(LocalUserLeftMsg)      // charged
    HANDLE_MESSAGE(MetadataLoadedMsg)     // byte-identical to retail

Same macro (`if (sym == msg::Type()) _HANDLE_CHECKED(OnMsg(msg(_msg)))`), same
`Message(DataArray*)` ctor (`AddRef` — the `sth 0xa(r27)`), same non-trivial
`~Message` (`Release`), same sret `DataNode` return. The only difference between
the two arms is the callee. `OnMsg(const MetadataLoadedMsg&)` had a real body;
`OnMsg(const LocalUserLeftMsg&)` was

    DataNode BandStorePanel::OnMsg(const LocalUserLeftMsg &) { return DataNode(1); }

MSVC proves a same-TU empty body **nothrow**. That changes the *caller's*
codegen: with a nothrow callee there is no EH region to protect the stack
`LocalUserLeftMsg` temporary across the call (its destructor is non-trivial, so
with a throwing callee the region must exist), and the post-RA scheduler is
then free to hoist the three arg-setup instructions into the construction
sequence. With a real body the EH region pins them contiguous — exactly what
the `MetadataLoadedMsg` arm shows.

"The oracle is the defect": the rb3-Wii dev decomp
(`../rb3/src/band3/meta_band/BandStorePanel.cpp:126-128`) *is* the empty stub.
Retail TU5 has a real 232 B body at `0x826067C0`, which is currently mis-pinned
into `Mat.cpp`'s unit (Mat's `.text` block ends exactly at `0x826067C0` and
resumes at `0x826068A8`). Read off retail bytes (`build/45410914/asm/Mat.s`,
keyed on `.fn fn_826067C0`):

    lwz 0x4(r?)+GetObj<LocalUser>(2)  -> msg.GetUser()
    cmp against StoreUser()
    guard-bit static Symbol from "critical_user_drop_out" (lbl_820B42A0)
      -> function-local static (/DRB3_HANDLE_LOCAL_STATIC dialect), NOT the
         Symbols2.h global that CriticalUserListener uses
    lwz ?TheUIEventMgr@@; li r5,0; bl ?TriggerEvent@UIEventMgr@@QAAXVSymbol@@PAVDataArray@@@Z
    li r11,1; stw 0,4(sret); stw r11,0(sret)   -> return DataNode(1)

`fn_825D70C8` (`CriticalUserListener.s`) references the same string — a
retail sibling with the same shape, corroborating the reading.

## T1 — port the body (the fix)

    DataNode BandStorePanel::OnMsg(const LocalUserLeftMsg &msg) {
        LocalUser *user = msg.GetUser();
        if (user == StoreUser()) {
            static Symbol critical_user_drop_out("critical_user_drop_out");
            TheUIEventMgr->TriggerEvent(critical_user_drop_out, 0);
        }
        return DataNode(1);
    }

Pre-registered P1 (`~/tmp/w16gi_prereg.md`, written before the build): the 6
charges vanish, fuzzy → 100.0, whole-binary +1 fn / +1,928 B, exactly 1 leg-B
recompile, no other row in the unit moves. Falsifier F1: charges survive with a
real body ⇒ mechanism wrong.

**Measured — P1 held on every key:**

    row     fuzzy 98.75519 -> 100.0   mpn -> 100.0   482/482 equal   (Complete, High)
    unit    BandStorePanel 120/127 -> 121/127

    A/B (ab_measure --from-dirty, both legs settled, 1 leg-B MSVC recompile):
      leg A  matched=44139  masked=23323  honest=20816  code%=40.692772
      leg B  matched=44140  masked=23323  honest=20817  code%=40.711586
      Δmatched=+1  Δcode_bytes=+1928  Δhonest=+1  Δmasked_equal=+0
      Δcode%=+0.018814pp  Δfuzzy=+0.000234pp
      unit net over ALL units = +1 == whole-binary Δmatched  (nothing else moved)
      units at 100%: 195 -> 195 (mpn), 173 -> 173 (all-rows-fuzzy)
      none ruler: +1928 B too (expected for a source patch; NOT the alias shape)

Commit `13c6e89c`.

## T2 — isolation: is it the BODY or the DEFINITION ORDER?

Pre-registered P2: keep the EMPTY stub but move its definition BELOW `Handle`.
If MSVC's nothrow proof were textual-order-dependent, T2 would also read 100;
if whole-TU, T2 stays at baseline.

**Measured: byte-identical to baseline.**

    fuzzy 98.75519  mpn 98.75519   same 3 insert / 3 delete at the same indices
    whole-binary 44,139 / 4,169,816 B / 40.692772%  == leg A exactly

⇒ Definition order is **not** the lever. The proof is whole-TU; only body
content decides whether the caller's EH region exists. Commit `10ff064f`,
reverted in `8d1f74b6`.

## C1 — the control that makes the 100 trustworthy

"Reads 100" is worthless unless the instrument can move. On the T1 tree,
change an **immediate** (CLAUDE.md: a relocation-arg edit is masked and proves
nothing) — `Request(mPrevChunkPath.c_str(), true)` → `false`. Expected exactly
one charged site, `li r5,0x1` → `li r5,0x0`.

    fuzzy 100.0 -> 99.997925   1 diff_arg at idx 90 (`li` off:-1), 481 equal
    whole-binary 44,140 / 4,171,744 B -> 44,139 / 4,169,816 B
      (-1 fn / -1,928 B: the row's all-or-nothing credit withdrawn by ONE immediate)

The witness discriminates. Commit `f5b48b8e`, reverted in `2b3d246e`. The
final tree rebuilt from the T1 state reproduces leg B to the last digit
(44,140 / 4,171,744 / 40.711586 / fuzzy 50.49751 / masked 23,323) — four
builds, zero nondeterminism.

## What this establishes

1. **A 3-insert/3-delete arg-setup permutation around a `bl` to a same-TU
   function is a callee question, not a caller question.** Check whether the
   callee is an empty stub before touching the caller's source shape at all.
   The sibling `MetadataLoadedMsg` arm was the free within-function control.
2. **It is whole-TU, not order-dependent** (T2). Moving stubs around does
   nothing; only giving them their real body (or moving them out of the TU)
   changes the caller.
3. The rb3-Wii dev oracle carried the stub, so "our source matches the oracle"
   was true and wrong at the same time — retail bytes outrank the oracle.

## Deliberately NOT done, and why

- **No `target_symbol_map.json` entry for `0x826067C0`.** The address is
  pinned into `Mat.cpp`'s unit; naming it `?OnMsg@BandStorePanel@@…LocalUserLeftMsg`
  there would create a row that Mat's base obj can never define ⇒ permanent 0%.
  The ported body is compiled and correct but **deliberately unscored** until a
  map lane re-homes the block.
- **No splits re-home of `0x826067C0`–`0x826068A8`.** Re-homing an
  already-pinned address is NOT metric-neutral (PINHOME-1) and needs its own
  A/B; it is a map-lane deliverable, not this lane's.
- **No alias.** The row had 0 `diff_arg`; there was no callee question.
- **No permuter, no declaration-order lever, no caller-side rewrite.** The
  mechanism explains all six charges; nothing was left to shape.
- **No `total_code` / ceiling re-measurement.** `total_code` is unchanged
  (10,247,068 on both legs); nothing here moves a denominator.

## Whole-binary result (landed)

    matched_functions       44140   (Δ +1)
    matched_code          4171744   (Δ +1,928)
    matched_code_percent 40.711586  (Δ +0.018814)
    fuzzy_match_percent   50.49751  (Δ +0.000234)
    masked_equal_functions  23323   (Δ 0)
    honest                  20817   (Δ +1)
