NEXT-WAVE TARGETING BRIEF (W17-TARGET) -- priced on main 8af79551 (2026-09-30)
===========================================================================
Re-derived from scratch on a freshly built report.json. Do NOT inherit these
numbers into a later wave; re-read each row's own pre-state (standing rule).

Tree: worktree ~/tmp/wt-w17target at 8af79551 (== main HEAD), full
./tools/ninja-locked rc=0, `verify_objs_patched.py --check` = fixed point of
all 6 post-compile passes, 1049/1049 objects paired.
Ruler, read from report.json provenance: functionRelocDiffs=name_check,
ppc.calculatePoolRelocations=false, objdiff 4.2.9 (5a51cd51), icf_aliases.map
6,108 entries.

HEADLINE AT 8af79551
  matched_functions 44,154   matched_code 4,177,988 B   40.772522 %
  fuzzy 50.497726            masked_equal 23,323         honest 20,831
  total_code 10,247,068      total_functions 69,240
  Reconciliation vs roadmap 7v.12 (44,139 / 4,169,816): +15 fns / +8,172 B
  = GI 1,928 + GJ 1,668 + GK 4,576 EXACTLY. Nothing else moved.

VEIN: game rows (source under src/band3/ or src/network/, resolved from
objdiff.json metadata.source_path -- NOT from the unit name), >=700 B,
fuzzy in [90,100).
  raw            66 rows / 106,964 B
  vs prior brief 69 rows / 112,144 B:  -3 rows / -5,180 B
                 = GF 1,584 + GI 1,928 + GJ 1,668 = 5,180.  Reconciles to the byte.
  minus ledger    8 rows /  25,464 B (7 briefed items + GH's landed residual;
                                     the 8th briefed item, OnFileLoaded@
                                     BandDirector, is src/system/bandobj and
                                     was never in this vein -- see E4)
                + 1 row  /   5,036 B CustomizePanel::Handle (priced refusal the
                                     ledger was MISSING -- see LEDGER ERRORS)
  LIVE           57 rows /  76,464 B

Instrument validity, measured not assumed:
  * objdiff-cli diff (no --build, no -c) fuzzy/canonical == report.json
    fuzzy/mpn on 66/66 vein rows, 99/99 (game 300-700), 31/31 (bandobj),
    1,185/1,185 (binary-wide near-100 census). 0 disagreements, 0 errors.
  * Every sub-100 row fires >=1 non-`equal` instruction (census not vacuous).
  * The new COMMUTE/REGALLOC split reproduces every known answer:
    ParseDataResultsIntoSetlists = REGALLOC 46 (documented pure regalloc);
    TourProgress = COMMUTE 2; VocalPart::HandlePhraseEnd / Asset = COMMUTE 1.

>>> READ THIS FIRST: THE ">=700 B GAME SOURCE VEIN" IS NEARLY DEPLETED. <<<

Shape of all 66 rows in the vein (charge-class census, all three instruments):
    diffuse / mixed                         28 rows  53,308 B
    pure relocation-NAME (alias/map work)   20 rows  20,524 B
    pure REGALLOC (permuter, OFF)            5 rows  14,208 B
    small mixed (<=2 ins/del, <=6 arg)       6 rows  10,276 B   (1 is CustomizePanel)
    pure COMMUTE (operand order)             7 rows   8,648 B
The source-crossable remainder is ~4-5 rows. So I WIDENED, deliberately, to
(c) BLOCK-LEVEL plays: a binary-wide census of every row >=100 B with fuzzy
in [99,100) (1,185 rows / 440,160 B), grouped by the ONE lever that would
close each row. That is where the funding is. Two of the five funded targets
are blocks; three are single game rows.

--- FUND FIRST ------------------------------------------------------------

F1  ALIAS BLOCK: extend group 10, push_back<ChatReceiver*> @ 0x82b5f808
    66 rows / 23,376 B whose ONLY charges are relocation names resolving to
    this one survivor (44 game, 17 engine, 5 bandobj). 0 masked_equal => all
    honest. Largest: ByteGrinder::Init 2,084, VocalPlayer ctor 1,364,
    PrefabMgr ctor 1,188, FingerShape ctor 1,040, GemSmasher ctor 908,
    SongSortMgr::BuildInternalSetlists 780.
    ** 51 distinct our-spellings, ALL CLEAN: 0 map-resident (none contradicted
       -- W16-GK's decisive test), 0 in withdrawn[] of any group, 0 members of
       any other group. 51/51 are 4-byte pointer element types (49 object
       pointers + one function pointer + const VocalNote*), the same class as
       every prior admission.
    ** The recipe is established, not speculative: W16-M, W16-Z, W16-AE and
       W16-AL each admitted spellings to THIS group on CHASED T1 PROVEN
       (tools/icf_pair_adjudicate.py --survivor 0x82b5f808 --ours <sp> --chase;
       the one differing slot is _M_insert_overflow<T*>, whose chain folds).
    ** Lever: chase each spelling; install the PROVEN ones; forced re-split A/B.
    ** HARD FALSIFIER (pre-registered):
       (a) run --chasetest FIRST; if its in-family decoy is not REFUTED, admit
           NOTHING and stop -- the instrument is not discriminating.
       (b) a spelling is admitted only on CHASED T1 PROVEN with 0 CYCLE-ASSUMED
           and 0 SLOT-REFUTED; anything else stays out, individually.
       (c) compute the predicted Delta BEFORE installing (sum of rows whose
           spellings are ALL admitted); if the measured Delta misses by >1 row,
           revert and report.
       (d) ab_measure WILL print ALIAS_SUSPECT (name_check up, `none` flat, on
           a map-only patch) -- that is the expected signature and is cleared
           ONLY by the per-spelling chase record, never by the Delta.
    Expected: most of the 23,376 B; admit rate for pointer spellings to this
    group has been 100% so far, but assume ~0.7 until measured.
    (9 binary-wide rows carry >8 name charges and my census truncated their
     name lists -- the lane must re-derive membership for those, not trust
     this grouping.)

F2  ?Handle@MusicLibrary@@UAA?AVDataNode@@PAVDataArray@@_N@Z
    default/MusicLibrary  6,160 B  fuzzy 99.97403  mpn 100.0  ME false
    charges: 0 ins / 0 del / 0 rep / 8 diff_arg, all REGISTER args, at exactly
    two sites.  opset identical.
    ** THE CONTROL IS IN THE FUNCTION. Handle has THREE __RTDynamicCast sites.
       The first (idx 119) matches retail's argument setup exactly; the two
       charged ones (idx 1146, 1183) differ only in the ORDER the RTTI
       descriptors are materialised (retail builds Src=Object into r11 first,
       we build the Target first; li r4/li r7 swap with it).
       The matching site is spelled   _msg->Obj<LocalBandUser>(2)
       -- the cast happens INSIDE the inlined DataNode::Obj<T> template.
       Both charged sites are spelled
         dynamic_cast<StoreSongSortNode *>(_msg->Obj<Hmx::Object>(2))->mOffer...
         dynamic_cast<StoreOffer *>(_msg->Obj<Hmx::Object>(2))->...
       (MusicLibrary.cpp Handle arms, ~l.72/78 of the handler block.)
    ** Named suspected defect: the explicit outer dynamic_cast; retail
       almost certainly spelled _msg->Obj<StoreSongSortNode>(2) and
       _msg->Obj<StoreOffer>(2). Both keep the RTTI target realbug-fixes
       2026-07-29 corrected (it fixed a cast to Hmx::Object), so accuracy is
       preserved either way.
    ** Price: mpn is already 100, so crossing is +0 functions / +6,160 B.
    ** HARD FALSIFIER: rewrite both arms as Obj<T>. If, with the TU verifiably
       recompiled, the lis/addi/li order at both sites is byte-unchanged, STOP:
       the row goes to the permuter bank. At most three spellings (Obj<T>,
       a named `T *x = _msg->Obj<T>(2);` local, and the original) -- no
       grinding beyond that.
    ** Why it was missed: W16-FW's screen said "mpn == 100 => every penalty is
       relocation-name => do not brief as source". The premise is FALSE -- mpn
       excludes ALL argument-level penalties, register args included -- and
       these are register args. See LEDGER ERRORS.

F3  RETAIL-BYTE-PROVEN NAME REPAIRS (one lane, three items, same instruments)
    All three are PROVEN on retail bytes in this brief, not inferred.
    (a) ?Handle@StoreMenuPanel@@  836 B  fuzzy 99.97607, ONE charge:
        retail calls ?Type@MetadataLoadedMsg@@ where we call
        ?Type@MultipleItemsEnumCompleteMsg@@.
        Retail bytes: 0x826050a8 constructs the string "metadata_loaded"
        (read out of band.exe at 0x820BF1E8) -- the map name is right, the two
        Type() bodies build different strings so ICF cannot fold them.
        rb3-Wii's BEGIN_HANDLERS also says HANDLE_MESSAGE(MetadataLoadedMsg),
        and our OnMsg body reads Array(2)/Int(3)/Str(4)/Int(5) + "submenus",
        i.e. it IS the metadata handler under the wrong class name.
        => A REAL BEHAVIOURAL BUG (the arm dispatches on the wrong message),
        introduced by ac075cef (2026-07-10), a month before name_check made it
        visible.
        Lever: HANDLE_MESSAGE + OnMsg signature -> MetadataLoadedMsg, AND
        rename map row 0x8263cf28, which inherited our wrong spelling
        (?OnMsg@StoreMenuPanel@@...ABVMultipleItemsEnumCompleteMsg@@@Z), or the
        320 B OnMsg row (100% today) un-pairs.
        Predicted: +1 fn / +836 B, OnMsg holding at 100.
    (b) Six ??0Keyboard*Msg@@QAA@HH@Z ctors, src/system/os/UsbMidiKeyboardMsgs
        (engine dir, but RB3-only Pro Keys code; DC3 has none): 6 x 164 B =
        984 B, each ONE name charge, and the pairs form a CLOSED 6-CYCLE.
        Retail bytes: all six ?Type@Keyboard*Msg@@ map names are CORRECT (each
        builds its own "keyboard_*" string), and the ctor the map calls
        ??0KeyboardModMsg (0x8252fe60) calls KeyReleased's Type(). => the six
        CTOR map names are rotated one slot (address order: KeyReleased < Mod
        < ExpressionPedal < ConnectedAccessories < LowHand < HighHand).
        Our source is right on all six. Lever: rotate six map names.
        Predicted: +6 fns / +984 B, plus possibly caller rows (retail callers
        of these ctors are charged against the rotated names today).
    (c) 0x82553f28, mapped ?PrefabIsCustomizable@PrefabMgr@@SA_NXZ.
        Retail body: `lis r11,0x82E0 / lwz r3,-0x2588(r11) / blr` -- return a
        global POINTER. A bool PrefabIsCustomizable() cannot compile to that;
        it is ?GetPrefabMgr@PrefabMgr@@SAPAV1@XZ (our spelling, map-resident
        nowhere). 6 GAME rows / 1,676 B are charged ONLY on this name:
        RemoteBandUser::SyncLoad 740, BandProfile::GetLastCharUsed 288,
        ManageBandPanel::UpdateCharacterFromStandInList 196, OutfitProvider
        ctor 168, CharacterCreatorPanel::HandleGenderChanged 160,
        FaceTypeProvider::Update 124. (CharSync::UpdateCharCache and
        CharProvider::Reload carry it too, with other survivors.)
        Lever: rename one map row. Predicted +6 fns / +1,676 B.
    ** Economics: this is MAPDEF-3's class -- REPAIRING A WRONG EXISTING NAME
       PAYS -- not "naming an anonymous address" (a bet). ab_measure will read
       name_check up / `none` flat on the map-only parts; the retail-byte
       evidence above is what clears it.
    ** HARD FALSIFIERS: (a) if the OnMsg row falls below 100 or
       map_name_injectivity fails, revert (a) whole. (b) if ANY of the six
       ctors fails to reach 100 after the rotation, the rotation is wrong --
       revert all six, do not keep a partial. (c) if any row currently at 100
       falls (someone did call PrefabIsCustomizable at that address), the net
       is negative -- revert.
    Expected: ~3,496 B at high confidence, +13 fns, one real bug fixed.

F4  INTEGER ARITH_COMMUTE -- a calibration lane for W16-C's lever
    7 game rows / 8,648 B, EVERY charge an operand-order swap on a
    commutative integer op, all mpn 100 (crossing = +0 fns / +bytes), 0 ME:
      TourProgress::Handle              2,596  2x add  (accumulator, NOT base+IV)
      MetaPerformer::SelectRandomVenue  1,924  2x add  (GA's "2 diff_arg" residual)
      VocalPart::HandlePhraseEnd        1,120  1x mullw
      SongUpgradeData ctor                788  2x add  (base+IV)
      VocalPart::SetDifficultyVariables   768  2x add  (2 of 8 identical sites)
      Asset ctor                          736  1x add  (W16-C's EXACT shape:
                                                  loaded mNodes + IV, inlined
                                                  DataArray::Node)
      ChordbookPanel::SetFret             716  and + or
    Binary-wide the pure-COMMUTE class is 72 rows / 38,956 B (integer 42 rows
    / 20,428 B; FP 18,528 B carries a weaker record -- leave FP out).
    ** WHY THIS IS FUNDABLE DESPITE "PROVEN INERT": crossing_worklist.py
       (09-11) rests on ONE experiment -- swapping the source operands is
       byte-identical. That is TRUE and W16-EZ/W16-CZ/W16-GJ each re-measured
       it. W16-C (09-14, a427ff73, still 100.0 today) then crossed a 12,220 B
       row of this class by a DIFFERENT lever: a codegen-free temporary
       elsewhere in the function (operand order follows internal node
       identity, not source text; retail itself emits both orders from one
       construct). NOBODY HAS APPLIED THAT LEVER TO ANY OTHER ROW. W16-EZ's
       "do not re-open" for HandlePhraseEnd/SetDifficultyVariables is scoped
       to FLIPS; its own evidence (both sides emit both orders from one
       construct) is exactly the condition W16-C characterised.
    ** Lever: W16-C's /FAs harness (NEXTSONGPANEL_COMMUTE_AUDIT_2026-09-14.md
       "Harness"), screening eliminated-temporary spellings, ~2 s per compile.
       ORDER: Asset ctor first (1 site, exact shape), then SongUpgradeData,
       SelectRandomVenue, TourProgress; SetDifficultyVariables (needs 2-of-8
       selectivity), HandlePhraseEnd and SetFret last.
    ** HARD FALSIFIER: <=20 screened spellings per row, and require the rest
       of the instruction stream identical before any full build. If Asset
       ctor AND SongUpgradeData both fail within budget, STOP THE LANE and
       record "W16-C's lever does not transfer to base+IV adds outside
       NextSongPanel" -- that negative is worth as much as a crossing, because
       it settles a 20 kB class either way.
    ** NEVER edit obj/Data.h (tree-wide, ~281 PCH TUs; NEAR_CROSSINGS 4.1).
    Expected: ~0.35 per row => ~3 kB; strategic value is the class verdict.

F5  STACK-SLOT / LOCAL-PLACEMENT PAIR (GH mechanism)
    (a) ?DrawTrackElements@GemTrack@@QAAXHH@Z  default/GemTrack  1,432 B
        fuzzy 97.82961 / mpn 98.02514; 4 ins / 3 del / 15 diff_arg
        (imm 6, REGALLOC 5, imm+reg 2, br 2). Handed forward; now diagnosed:
        retail keeps GetPlayer()'s result in r29 and homes it at 0x78(r31)
        (reloaded after the loop, idx 217); we home it at 0xc8(r31) and use
        r29 for an early hoisted load of the local static `res` (idx 59),
        which retail reads at the DrawFill call (idx 68). The six +4
        immediates (0xd0/0xd4, 0xcc/0xd0, 0xc8/0xcc) are locals shifting
        around our extra slot; +80 is 0xc8-0x78, the player slot itself.
        The GetFillInfo/DrawFill arg-order swaps sit inside that and look like
        symptoms. Named defect: the `player` local's declaration position /
        lifetime + where `res` is read. No prior lane record in the source.
    (b) ?OnMsg@BandStorePanel@@QAA?AVDataNode@@ABVMetadataLoadedMsg@@@Z
        908 B  fuzzy 99.26872 == mpn; 1 ins / 1 rep / 6 diff_arg, ALL imm.
        Three frame slots PERMUTED (retail Symbol temps 0x50, Sym() return
        0x54, int 0x58; ours 0x58/0x50/0x54) + one materialisation (retail
        reloads the Symbol from its slot; we go via the ctor's returned this).
        W16-CK tried a named local: closed 2 charges, grew the frame
        0xf0->0x100, -96 B. BANNED. W16-GH (later) found the real mechanism:
        MSVC resets its temp-slot pool PER STATEMENT and gives distinct slots
        to temps within one full-expression -- statement grouping is the
        lever, and CK never had it.
    ** HARD FALSIFIER: frame size must stay at retail's on every variant;
       <=5 statement/declaration-placement variants per row; if the slot map
       does not move toward retail's, STOP (it is then scheduling).
    Expected: ~0.35 each => ~0.8 kB.

--- DO NOT FUND -----------------------------------------------------------

X1  ?Handle@CustomizePanel@@  5,036 B, ONE charged `clrlwi` -- PRICED REFUSAL,
    not in the ledger this brief was handed. Eight attempts (DQ-1 ...
    W37-CLRLWI); W16-CG swept 28 codegen knobs + the other compiler build and
    closed the codegen channel (edd6b216). Read the in-source record at
    CustomizePanel.cpp before anyone re-briefs it.
X2  ?MaybePublish@UIStats@@  2,604 B -- W16-EI closed it: alias-gated on two
    fold memberships AND 63 further charges (PERMUTED temp slots + a
    6-register rotation). Five prior attempts, one regressed.
X3  ?RecordAccomplishmentData@RockCentral@@  4,676 B, mpn 100 -- a consistent
    r29<->r30 swap across 8 sites (my classifier's lone "COMMUTE" there is an
    `addi` with an immediate, i.e. regalloc). Permuter class; bank it.
X4  ?SetState@SaveLoadManager@@  4,096 B -- REGALLOC 80 + 11/11 moves, opset
    identical => scheduling. W16-FW/W16-CF records agree. Bank.
X5  The 0x823d14c0 list<Object*>::insert family (51 rows / 17,304 B): 22 of
    its 28 spellings sit in withdrawn[] as FABRICATED_CLOSURE_NOT_PARTITION
    (ALIAS-CONSOLIDATION 2026-08-19), 5 belong to other groups, 1 is clean.
    Contested; GK re-admitted two by per-spelling chase. Not a block play.
X6  default/Accomplishment residue: 20 rows / 1,768 B, 13 of them anonymous
    fn_ rows at fuzzy 0 (identification, not source). The named near-misses
    are GetRequiredScoreType 200 B and ??_GRndAnimatable 136 B. No shared
    defect; too small for a lane.
X7  The briefed ledger, all verified where the record says:
      Hit@GemPlayer 2,724 @99.89721 (6 REGALLOC + 2 name) -- permuter;
      ParseDataResultsIntoSetlists 1,968 @99.52235 (REGALLOC 46) -- permuter;
      ResolveSlotStates 1,416 @98.84180 (2 ins/2 del/2 br) -- drained;
      OnFileLoaded@BandDirector 3,816 @99.77988 -- deferred (NOTE: it lives in
        src/system/bandobj, outside the literal game vein);
      UpdateScrolling@VocalTrack 8,948 @94.52079 -- deferred;
      DisplayChord@ChordbookPanel 3,436 @97.18510 -- refused/control;
      Poll@VocalPlayer 3,388 / RebuildHUD@VocalTrack 2,188 -- diffuse;
      OnMsg@OvershellSlot(ButtonDownMsg) 1,396 @99.42693 -- GH residual,
        1 ins/1 del, scheduling, as GH recorded.
X8  Other pure-REGALLOC game rows (StoreOfferProvider::Handle 1,556,
    UpdatePitchArrow 928, DrawTails 888, TriggerSongCompletion 1,052,
    RockCentral::UpdateSetlist 1,268): permuter bank.

--- WORTH A LOOK (priced, not funded) -------------------------------------

W1  GA's 13 local-static rows: UNTOUCHED since GA (every fuzzy reproduces GA's
    table to 3 dp). 5,632 B total at fuzzy 23-75: ApplyFontStyle 1,164,
    MicInputArrow::Handle 712, EnterVenue 604, OnSelectRow 436, GetController
    404, GetHighestDifficultyForPart 364, PropSync<MeshAO> 360, GetModeInst
    332, PartPlaysInSet 316, GetSetlistMaxVocalParts 292, Rank 236,
    RankTierToken 224, AccomplishmentCategory::Configure 188. GA's recipe
    (guard-bit census -> function-local statics in retail's CLAIM order, not
    execution order) took its own row 58 -> 99.96 but not to 100. A good
    block for a lane whose falsifier is "stop a row once its guard-bit
    popcount matches and residue is not a named construct". MicInputArrow and
    OutfitConfig mix statics with temporaries (per GA) -- read per site.
W2  0x82272a60 vector<Object*>::_M_fill_insert: 22 rows / 8,376 B (11 game),
    11 clean spellings, no group yet. Second alias block after F1, same recipe.
W3  NAME-PERMUTATION CYCLES (a cheap detector: edges our-spelling -> retail
    name; any cycle >= 2 is a candidate swap). Beyond F3(b)'s 6-cycle:
      Campaign::GetCurrentPoints...ForUser <-> GetTotalPoints...ForUser  736 B (GAME)
      OnFileExecRoot <-> OnSynchProc                                     2,080 B
      Synapse::SetVoiceAmount <-> SetVoiceProximityFocus                   648 B
      StringTable::Size <-> UsedSize                                       512 B
      ObjPtrList<...> dtor 3-cycle                                       4,608 B
    Each is a swapped map pair OR a genuine source swap; retail bytes decide,
    and either answer is fixable. Template 2-cycles (PropSync<RndDrawable/
    RndPollable>, sort helpers<FileCacheEntry/MoveDetector>) are more likely
    fold twins -- adjudicate separately.
W4  ?CheckConditionsForSong@AccomplishmentSongConditional@@ 1,188 B, 2 name
    charges. TU ORDER says the MAP is wrong, not us: retail's checkers sit in
    definition order Stars 0x82669898 < Score < Accuracy < Streak <
    0x82669a98 < 0x82669b18, and in our source the 5th/6th are HoposPercent
    and SoloPercent -- the map calls them Awesomes/DoubleAwesomes. Their
    getters (0x825d1d98/0x825d1cb8) are mapped GetBestAwesomes /
    GetBestDoubleAwesomes while GetBestHOPOPercent / GetBestSoloPercent are
    absent from the map entirely. Probably a shifted naming chain across two
    TUs. Needs member-offset reading of the getter bodies before any rename.
W5  ?Poll@PracticePanel@@ 1,656 B (handed forward): four distinct
    source-shaped defects -- Symbol temp slot 0x60 vs 0x58 (GH), a consistent
    r24<->r25 swap (likely symptom), a bool narrowed ONCE after `or` in retail
    vs per-operand in ours (bool-typed accumulator), and a 1.0f RELOADED from
    memory in retail at the GetMusicSpeed() compare where we CSE it into f30
    (we use 1.0f somewhere retail doesn't). All source class; too many for one
    falsifier, hence not funded.
W6  ??1Game@@UAA@XZ 792 B: STRUCTURAL -- retail homes this+0xec and resets a
    vtable at 0xec and 0x100; we compute this+0xec+0x14. Our member at 0xec
    has its vtable'd sub-object at +0x14, retail's at +0. A member-layout /
    hierarchy question (user directive: vtable+struct work is high value).
    Run class_layout_report.py on Game before anything else.
W7  ?BuildList@StoreOfferProvider@@ 2,536 B (ours 40 B SMALLER): retail tests
    the string's first byte AND a second condition at two sites (4 of 16
    deletes -- real source), and spills `this` to 0x194(r31) at ~6 sites
    (register pressure, the hard part), plus 6 group-10 names that F1 would
    close. Revisit after F1 lands.
W8  ?Poll_@PerfectOverdriveTracker@@ 1,248 B: 1 extra `mr` of ours (a GJ-style
    materialisation) + 2 commutes + 1 regalloc. ?OnMsg@RockCentral@@
    (ServerStatusChangedMsg) 1,024 B: retail calls SystemLocale where we call
    SystemLanguage (ours map-resident nowhere) + 13 retail-only instructions.
W9  bandobj small-mixed rows (RB3-specific, src/system/bandobj): StreakMeter::
    SyncObjects 1,328 (1/1 + 4 names), PitchArrow ctor 1,132 (1/1 + a +-8
    imm pair), BandStarDisplay::SetNumStars 780, BandRetargetVignette::
    EnterDir 772. The bandobj >=700 band is 31 rows / 43,656 B, same shape
    mix as the game vein.

--- LEDGER ERRORS FOUND (flag to the coordinator) -------------------------

E1  CustomizePanel::Handle (5,036 B) was absent from the ledger handed to this
    lane although it is the most-attempted priced refusal in the tree.
E2  W16-FW's screen premise "mpn == 100 => every penalty is relocation-name"
    is FALSE. mpn excludes ALL argument-level penalties; register args
    included. It sent MusicLibrary::Handle (register args at two
    specifically-spelled sites, F2) and the COMMUTE rows to "do not brief as
    source". Fourth instance of the 7v.10 diff_arg misread.
E3  crossing_worklist.py's "ARITH_COMMUTE -- PROVEN INERT, DO NOT FUND" and the
    roadmap's 09-16 coordinator check (which inherits it and gates on W16-CG)
    contradict W16-C's landed +12,220 B crossing of that exact class. The
    experiment behind "inert" (operand swap) is correct; its REACH is wrong.
    W16-CG (the gate named) settled CustomizePanel's clrlwi, not operand order,
    so the gate is satisfied without resolving anything. The docstring should
    be corrected to scope "inert" to operand swaps.
E4  OnFileLoaded@BandDirector is src/system/bandobj, not game. Harmless, but a
    vein filter keyed on src/band3 cannot see it.
E5  Map row 0x8263cf28 carries OUR wrong message type (StoreMenuPanel OnMsg),
    i.e. the map was generated from source and inherited a source bug; map row
    0x82553f28 is misnamed PrefabIsCustomizable (body = GetPrefabMgr); six
    Keyboard*Msg ctor map rows are rotated one slot. (All three: F3.)
E6  GA's SelectRandomVenue residual "2 diff_arg" is two commutative adds --
    it belongs to F4's class, not to local statics.

--- INSTRUMENT NOTE -------------------------------------------------------
* objdiff-cli diff -p <wt> -u <unit> -f json --include-instructions <sym>,
  no --build (the six patchers are part of the ruler), no -c (objdiff.json's
  options block pins all four divergent keys). Validated per row against
  report.json fuzzy AND mpn -- do not skip that check; it is what makes a
  census trustworthy.
* Per-instruction class: `match_type` ; per-argument: diff_breakdown.
  arguments[].arg_type in {register, immediate, symbol, branch_dest}. A
  `symbol` arg whose TARGET is a placeholder (fn_/lbl_/...) is forgiven by
  name_check and must not be priced.
* NEW split used here: a register-only diff_arg whose register-operand
  MULTISET is identical on both sides with the same opcode = COMMUTE, else
  REGALLOC. Caveats: `cmpw` swaps land in COMMUTE but are DIRECTIONAL
  (crossing_worklist's CMP_REVERSAL -- FUND class); an `addi` with an
  immediate can masquerade (RecordAccomplishmentData). Read the opcode.
* Block pricing: for each pure-name row, map every charged retail name to its
  address; rows needing EXACTLY ONE survivor form a one-lever block. Then
  check every our-spelling for map residency, withdrawn[] (search the whole
  record, not just `spelling`), and membership in any other group.
* Traps hit this lane: Python's hash() is salted per process (use hashlib for
  cache keys); zsh does not word-split `$flags` (a --map-file probe silently
  became one argv element); a stale output file from the previous leg looked
  like a result. Retail bodies for addresses with no `.fn` in any split .s
  were read straight from orig/45410914/band.exe via the PE section table and
  disassembled with `llvm-mc --disassemble -triple=powerpc64`.

--- WHAT I DID NOT DO -----------------------------------------------------
* No source, map, splits or alias edits; no A/B. Every "predicted" figure is
  size-if-it-crosses, not a measurement.
* Did not run icf_pair_adjudicate --chase on any F1 spelling; the "clean"
  status is residency/withdrawn/membership only.
* Did not verify W3's cycles on retail bytes except the Keyboard one, nor
  W4's getter bodies.
* 9 binary-wide rows have >8 name charges and their name lists were truncated;
  they may need more than one survivor.
* The binary-wide census covered fuzzy in [99,100) and >=100 B only; the
  engine vein was priced by shape, not row by row.
