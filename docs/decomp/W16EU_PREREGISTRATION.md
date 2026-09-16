# W16-EU pre-registration (written BEFORE any A/B leg was run)

Baseline in worktree `~/tmp/wt-w16-eu` @ `fa93fb76`, full build green (BUILD_RC=0),
ruler `functionRelocDiffs=name_check` read from `report.json` provenance.
Baseline measures: matched_functions=43969, matched_code=4130116, code%=40.305344,
total_code=10247068.

## Task 1 — re-home 0x823E1B40 (SpeechMgr -> NetSession)

Edit: splits.txt only. Delete SpeechMgr's `.text 0x823E1B40-0x823E1C20`;
merge NetSession's `0x823E16A8-0x823E1B40` + `0x823E1C20-0x823E1D20`
into `0x823E16A8-0x823E1D20`.

Evidence: `??0AddUserResultMsg@@QAA@_N@Z` is DEFINED in
`build/45410914/src/network/net/NetSession.obj` (COFF sec=492) and is only an
UNDEF extern elsewhere. Cross-object diff of SpeechMgr *target* obj vs
NetSession *base* obj at the graded ruler reports **fuzzy=100.0, 136/136 B,
0 charged sites** -- i.e. the post-re-home pairing is already measured.

PREDICTION: **+136 B / +1 function.** Range **+56 .. +136 B**.

Per-row:
- `??0AddUserResultMsg@@QAA@_N@Z` 136 B: 0.0 -> 100.0   => +136 B, +1 fn
- `fn_823E1BC8` 40 B (100.0 today, SpeechMgr): expect re-pair in NetSession => 0
- `fn_823E1BF0` 40 B (100.0 today, SpeechMgr): expect re-pair in NetSession => 0

Both funclets are provably the ctor's own EH funclets (funclet A cleans
`r31+0x58` = the ctor's bool local; funclet B loads `r31+0x94` = the ctor's
`this`), so they belong in NetSession on accuracy grounds regardless of metric.
NetSession.obj contains 11 / 15 byte-compatible funclet candidates vs
SpeechMgr.obj's 2 / 4, so byte-signature re-pairing should succeed.

Named failure modes:
1. funclets fail to re-pair in NetSession => -80 B (lands at +56).
2. adding 3 symbols to NetSession's larger pairing pool displaces an existing
   byte-signature pairing (the `b341d7ab` -40 B EH-funclet re-pair effect).
3. re-split / symbols.txt convergence moves unrelated rows.

## Task 2 — rename 0x823F2C98 -> ??0MakeQuazalSessionJob@@QAA@PAPAVQuazalSession@@_N@Z

Edit: `scripts/target_symbol_map.json` only (one value).

Evidence, three independent strands:
1. Fan-in = 3, censused off retail `.text` (opcode 18, both LK values):
   0x823E4A00, 0x823E4DB8, 0x823E6100 -- all inside NetSession's span.
   Mislabel signature (folds in W16-EQ showed 15/44/10/453).
2. Body at 0x823F2C98 is a C++ ctor, NOT an STL allocate-and-copy: base-class
   ctor call (`bl fn_827CBAD8`), pointer stored at this+0x8, **byte** stored at
   this+0xc (a bool), vtable pointer written to this+0x0, returns this.
   Matches `MakeQuazalSessionJob(QuazalSession**, bool)` : public Job with a
   virtual dtor. An STL `_M_allocate_and_copy` has no vtable and no base ctor.
3. Caller bijection 3-for-3: our source constructs it in
   `NetSession::RegisterOnline` (:142), `NetSession::Join` (:365),
   `NetSession::OnMsg(JoinResponseMsg)` (:520).

Blast radius closed: 3 `bl`, 0 tail-calls, the only raw pointer to the address
is its own `.pdata` unwind record. The old name is carried by exactly one
address; the new name exists nowhere in the map (no duplicate created).

PREDICTION: **+136 B / +1 function.**

Per-row:
- `?RegisterOnline@NetSession@@QAAXXZ` 136 B, 99.85294, exactly ONE charge and
  it is this one: 99.85294 -> 100.0  => +136 B, +1 fn
- `??$_M_allocate_and_copy@PB_K@...` 60 B in StorePurchaser, fuzzy 31.0:
  un-pairs to 0.0 => **0 B** (it was below 100; `matched_code` is
  all-or-nothing). Aggregate *fuzzy* will dip slightly -- expected, not a
  regression.
- `fn_823E4808` (pinned to MeshAnim, 0.0) and `fn_823E4B68` (NetSession, 0.0):
  the other two callers, both already 0.0 => 0 B either way.

Named failure modes:
1. a second charge on RegisterOnline I failed to enumerate;
2. a reference class my census cannot see;
3. aggregate fuzzy drops from forfeiting the StorePurchaser row's partial credit.

## Brief corrections registered BEFORE measuring

- Brief says `0x826F1080` is pinned `.text 0x826F1080-0x826F1188`. FALSE: the
  block is `0x826F0F98-0x826F10A8` (272 B) in ViewSetting.
- Brief/EQ say `?Disconnect@NetSession@@QAAXXZ` (136 B) carries this charge.
  FALSE: **no such row exists anywhere in report.json.** The 136 B/99.85294 row
  that does exist is `??0ProcessedJoinRequestMsg@@QAA@_N@Z`, whose single charge
  is a DIFFERENT defect (`?Type@SessionReadyMsg@@` vs
  `?Type@ProcessedJoinRequestMsg@@`).
- Therefore Task 2 is worth **136 B, not the briefed 272 B**.
- Brief says re-homing `0x826F1080` collects ~36 B of the 172 B. FALSE:
  **no compiled object defines `?GetFinger@FretHand@@QBAXIAAH00@Z`** -- it is an
  UNDEF extern in ChordbookPanel.obj / PracticePanel.obj / RGTrainerPanel.obj and
  there is no `FretHand.cpp` in the tree. No pin move can make it pair.
  Task 1 is therefore worth **136 B, not 172 B**.
- EQ named the other two callers of 0x823F2C98 `??_ENetSession` and
  `?UpdateSyncStore@NetSession@@`. Not supported: they resolve to unmapped
  `fn_823E4808` (544 B) and `fn_823E4B68` (756 B). EQ's conclusion holds; its
  caller names do not.

## Combined prediction

**+272 B / +2 functions** total, measured as two separate A/B runs so each stays
attributable.

---

# Task 3 pre-registration (written BEFORE any Task-3 leg was run)

Discovered while investigating the brief's phantom `?Disconnect@NetSession@@`
row. A FIFTH mislabel of EQ's shape, plus a second instance of EQ's int/bool
source bug. Baseline = `b8eb29e7` (Tasks 1+2 committed):
matched_functions=43971, matched_code=4130388, code%=40.308000.

## The defect

`0x823E1808` is mapped `??0ProcessedJoinRequestMsg@@QAA@_N@Z`. It is really
`??0SessionReadyMsg@@QAA@_N@Z`, and the true ProcessedJoinRequestMsg ctor is the
UNMAPPED `fn_823E1938`.

Evidence (all off retail bytes, none from the map):
- 0x823E1808 calls the accessor at 0x823E0928, which interns the .rdata literal
  **"session_ready"**. 0x823E1938 calls 0x823E0A28, which interns
  **"processed_join_request"**. Both accessors' own map names are therefore
  CORRECT -- so the defect is in the ctor names, not the Type() names.
- Both ctors carry `0x548B063E` = `clrlwi r11, r4, 24` at +0x18: EQ's bool
  marker. Retail takes a **bool** in both.
- Caller bijection 2-for-2 each:
  0x823E1808 <- OnCreateSessionJobComplete, OnRegisterSessionJobComplete; our
  source constructs SessionReadyMsg in exactly those two (:165, :209).
  0x823E1938 <- DenyRequest (anon ns), OnMsg(JoinRequestMsg); our source
  constructs ProcessedJoinRequestMsg in exactly those two (:377, :479).
- Fan-in 2 and 2 = mislabel signature (EQ's folds were 15/44/10/453).

Our `SessionReadyMsg(int i)` (SessionMgr.h:39) is the wrong signature -- and
:209 already passes a `bool b` into it. Same bug EQ fixed for AddUserResultMsg.

## Edits

- `src/band3/meta_band/SessionMgr.h:39` int -> bool
- `src/network/net/NetSession.cpp:165` `msg(0)` -> `msg(false)` (REQUIRED: with a
  bool overload, `0` is ambiguous against DECLARE_MESSAGE's `classname(DataArray*)`
  -- this is exactly the C2668 that refused EQ's first A/B)
- map `0x823e1808` -> `??0SessionReadyMsg@@QAA@_N@Z`
- map `0x823e1938` -> `??0ProcessedJoinRequestMsg@@QAA@_N@Z` (NEW entry)

⚠ The map fix ALONE is insufficient and would measure ~0: the charge is
target `??0ProcessedJoinRequestMsg@@QAA@_N@Z` vs base `??0SessionReadyMsg@@QAA@H@Z`,
so renaming to `@_N@Z` still mismatches our `@H@Z` until the source is fixed.
Both halves are required. This is a `map`+`source` patch, so ab_measure's
ALIAS_SUSPECT classifier will NOT fire (it fires only on map-only patches).

## PREDICTION: +732 B / +3 functions. Range +596 .. +1088.

Per-row, from enumerated charges (not from mismatch counts):
- `?OnCreateSessionJobComplete@NetSession@@` 460 B, 99.95652, **exactly 1 charge**
  and it is this ctor name => 100.0  => **+460 B**
- `??0SessionReadyMsg@@QAA@_N@Z` @0x823E1808 136 B, 99.85294, 1 charge (the
  Type() call) => pairs with our bool ctor, which IS emitted in NetSession.obj
  (today as `@H@Z`, sec=563) => 100.0 => **+136 B**
- `??0ProcessedJoinRequestMsg@@QAA@_N@Z` @0x823E1938 136 B, today an unpaired
  `fn_` at 0.0 => named and pairs with our (already-bool) ctor => **+136 B**
- `?OnRegisterSessionJobComplete@NetSession@@` 356 B, 99.77528, **3 charges**:
  the ctor name PLUS two register-allocation diffs (`mr r24,r4` vs `mr r29,r4`;
  `clrlwi. r11,r24,24` vs `clrlwi. r24,r29,24`). matched_code is all-or-nothing,
  so this row pays **+0** even though a real defect in it is fixed. If the
  register diffs dissolve with the signature change (the documented
  REGISTER_SWAP-is-a-symptom effect) it pays +356 -- that is the top of the range.
- `?DenyRequest@?A0x055cc49d@@` 184 B at **100.0**: naming 0x823E1938 converts its
  forgiven placeholder target into a checked one. Our DenyRequest calls
  `??0ProcessedJoinRequestMsg@@QAA@_N@Z`, i.e. the name I am assigning, so it
  should STAY at 100 => **0**. This is the main downside risk: -184 B if wrong.
- `?OnMsg@NetSession@@QAA_NABVJoinRequestMsg@@@Z` 1236 B, 99.95145, 3 genuine
  folds (EQ) => cannot cross => **0** either way.

Named failure modes:
1. C2668 ambiguity if any `SessionReadyMsg` construction still passes a literal
   `0` (only :165 and :209 exist tree-wide; :209 already passes a bool).
2. the int->bool change perturbs caller codegen (EQ's failure mode 1).
3. the 0x823E1808 row pairs but does not reach 100.
4. naming 0x823E1938 creates a charge at a site my census missed => -184 B.
