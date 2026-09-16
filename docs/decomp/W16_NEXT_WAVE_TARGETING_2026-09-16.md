NEXT-WAVE TARGETING BRIEF -- priced on main 5848c081 (2026-09-16)
================================================================
Re-priced from scratch on today's report.json. Do NOT inherit these numbers
into a later wave; re-read the row's own pre-state (standing rule).

VEIN: game-layer rows (src/band3/ + src/network/), >=700 B, fuzzy in [90,100).
  raw            69 rows / 112,144 B
  minus ledger    9 rows /  40,088 B   (drained + deferred + GC's refused control)
  LIVE           60 rows /  72,056 B   <- reconciles TO THE BYTE with the
                                          pre-GC/GB triage, i.e. wave 4 banked
                                          NOTHING out of this vein.

>>> READ THIS FIRST: THE RANKING REVERSED TWICE WHILE I BUILT IT. <<<

Three instruments, each one refuting the ranking the previous one produced:

  1. CHARGE COUNT (+ size) said ?Hit@GemPlayer@@ was best: 2,724 B, the biggest
     prize, 0 insert / 0 delete.
  2. CHARGE CLASS refuted that -- 6 of its 8 charges are REGISTER args, so it
     is permuter class and banks 0 B however many names get proven. It then
     said ?Handle@BandStorePanel@@ was best: 1,928 B, 0 diff_arg, no alias
     dependency at all.
  3. OPERAND DELTA refuted THAT -- BandStorePanel's instruction MULTISET is
     identical on both sides, so it is an instruction-SCHEDULING residual
     (permuter territory), and it promoted a row I had already written off.

  ==> A charge COUNT, a charge CLASS, and a charge's actual OPERAND DELTA are
      three different instruments. Each is cheap. Run all three BEFORE briefing
      a lane -- stopping at any one of them would have pointed a lane at a row
      the permuter owns.

--- FUND FIRST ------------------------------------------------------------
T1  ?OnMsg@OvershellSlot@@QAA?AVDataNode@@ABVButtonDownMsg@@@Z
    default/OvershellSlot   1,396 B   fuzzy 99.30373 == mpn 99.30373
    charges: 1 insert / 1 delete / 43 diff_arg / 0 replace

    ** I had this in DO-NOT-FUND as "arg-dominated". That was wrong, and the
       operand delta is what showed it. ALL 43 diff_arg charges differ ONLY in
       a hex immediate -- same opcode, same registers, on both sides. ZERO
       regalloc content. Measured, not inferred: 43 of 43.

    ** It is ONE defect: our stack frame is 16 bytes larger than retail's.
           subi r31, r1, 0xf0    vs ours  0x100
           stwu r1,  -0xf0(r1)   vs ours -0x100(r1)
       and the locals shift with it. Immediate deltas (ours - retail):
           +8 x38   +16 x3   +4 x1   -16 x1
       so the dominant shape is a uniform +8 on 38 local-slot references, on
       top of a +16 frame.

    ** THE LEVER EXISTS AND IS NAMED. MSVC_X360_REGALLOC.md's claim that
       "declaration order controls assignment" was corrected to: declaration
       order controls STACK SLOTS, and is inert only for REGISTER-only swaps.
       This row is stack slots. So declaration order is live here in exactly
       the way it is dead on a REGISTER_SWAP row.
    ** Instrument: run_diff_inspect mode=stack-layout (and the /stack-layout
       skill), which exists for precisely this shape.
    ** No alias is involved (0 name charges), so all 1,396 B are collectable by
       source work alone -- no fold has to be proven for the row to cross.

T2  ?Handle@BandStorePanel@@UAA?AVDataNode@@PAVDataArray@@_N@Z
    default/band3/meta_band/BandStorePanel   1,928 B   fuzzy 98.75519 == mpn
    charges: 3 insert / 3 delete / 0 diff_arg / 0 replace

    ** Bigger prize than T1 and also alias-free, but a WORSE bet, because the
       instruction multiset is IDENTICAL on both sides -- same opcodes, same
       registers, same immediates, different schedule:

         RETAIL (0x2adc..0x2b00)              OURS (0x8a6c..0x8a90)
           lis  r10, lbl_8205507C@h             lis  r10, ??_7LocalUserLeftMsg@@6B@h
           stw  r27, 0x5c(r31)                  addi r5, r31, 0x58     <-- hoisted
           addi r10, r10, ...@l                 stw  r27, 0x5c(r31)
           addi r11, r11, 0x1                   addi r10, r10, ...@l
           stw  r10, 0x58(r31)                  addi r11, r11, 0x1
           sth  r11, 0xa(r27)                   subi r4, r26, 0xec     <-- hoisted
           addi r5, r31, 0x58  <-+ contiguous   stw  r10, 0x58(r31)
           subi r4, r26, 0xec    | arg setup    addi r3, r31, 0x68     <-- hoisted
           addi r3, r31, 0x68  <-+              sth  r11, 0xa(r27)
           bl   OnMsg(LocalUserLeftMsg)         bl   OnMsg(LocalUserLeftMsg)

       Retail groups the three-argument setup contiguously immediately before
       the call; we interleave it into the message construction.
    ** That is INSTRUCTION SCHEDULING (PERMUTER_ROI_ANALYSIS.md#instruction-
       scheduling) and the permuter is OFF. Worth ONE lane only because a
       scheduling residual is a SYMPTOM, not a diagnosis -- 12 recorded
       instances dissolved once the real source defect was found (5d8fc966,
       c14bba5c, d7a9775a; docs/decomp/patterns/fixable-liveness.md).
       Context: a stack-constructed LocalUserLeftMsg at r31+0x58 (vtable ->
       0x58, DataArray r27 -> 0x5c, refcount r27->0xa incremented), then an
       sret call OnMsg(ret = r31+0x68, this = r26-0xec, msg = r31+0x58). The
       r26-0xec `this` adjustment is a base-subobject shape worth a look.
    ** HARD FALSIFIER, pre-registered so the lane cannot grind: if the multiset
       stays identical under every source shape tried, report AT_LIMIT
       promptly. An identical multiset in a different order with the permuter
       off is a stopping condition, not a puzzle.

T3  ?OnInitializeContent@CalibrationPanel@@QAA?AVDataNode@@PAVDataArray@@@Z
    default/CalibrationPanel   1,668 B   fuzzy 98.94485 / mpn 98.99281
    charges: 1 insert / 2 delete / 2 replace / 3 diff_arg (2 REG, 1 mixed)
    Smallest total charge count on the board (8). The one "NAME" charge is
    really mixed -- `lis r10, lbl_820BFEF8@h` vs our
    `??_C@_0BC@IFDIBBJ@bone_prog_bar?4tnm?$AA@@h`, where the TARGET name is a
    lbl_ placeholder (forgiven under name_check) so the live charge is the
    r10/r11 register difference, not the string. Treat as regalloc-flavoured.

--- DO NOT FUND -----------------------------------------------------------
X1  ?Hit@GemPlayer@@   2,724 B   0 ins / 0 del / 8 diff_arg   fuzzy 99.89721
    PERMUTER CLASS. Looks like the ideal map lane -- zero instruction
    differences, biggest prize -- and is not: 6 of 8 charges are register args
    (fadds f0,f31,f0 ; fadds f13,f31,f13 ; mr r5,r27 x2 ; mr r3,r26 x2), only
    2 are relocation names (push_back<vector<Vector2>>, ?QueueEnumJob@
    PlatformMgr@@). diff_arg charges fuzzy, so collecting the 2,724 B needs ALL
    EIGHT closed; proving both names still leaves six regalloc charges and the
    row still banks 0 B.
    (?QueueEnumJob@PlatformMgr@@ inside GemPlayer::Hit is almost certainly an
     ICF fold-alias and worth FILING on its own -- but filing it buys 0 B here,
     so it is not a reason to fund the row.)

X2  ?ParseDataResultsIntoSetlists@MusicLibraryNetSetlists@@   1,968 B
    0 ins / 0 del / 0 replace / 47 diff_arg (46 REG, 1 name).
    The purest regalloc row priced: a single consistent r28<->r29 assignment
    difference cascading through 46 sites, with an otherwise perfect body.
    Permuter class, permuter OFF. Bank it for the permuter, do not grind it.

X3  ?Poll@VocalPlayer@@ (3,388 B, 16/24/123) and ?RebuildHUD@VocalTrack@@
    (2,188 B, 24/18/89) -- too diffuse to price as single-defect rows.

--- WORTH A LOOK, NOT YET PRICED -----------------------------------------
    ?DrawTrackElements@GemTrack@@ (1,432 B, 4/3/15): its charges include
    `stw r29, 0x78(r31)` vs `stw r3, 0xc8(r31)` -- BOTH register and a 0x50
    offset difference, so there may be a stack-layout component like T1's under
    the regalloc. Run the operand-delta split before funding.
    ?Poll@PracticePanel@@ (1,656 B, 2/3/15): mixed; includes
    `addi r3, r31, 0x60` vs `0x58` (offset) and a TheUI placeholder/register
    pair. Same treatment.

--- INSTRUMENT NOTE -------------------------------------------------------
objdiff-cli diff with no -c reads objdiff.json's options block, which has
pinned all four divergent keys since 2026-08-31, so these ARE graded-ruler
figures. --build was NOT passed (it would skip the six obj patchers).
?GetRandomSongs@SongSortMgr@@ produced no JSON -- the same template-argument
failure seen on four symbols in the previous triage, not chased.
