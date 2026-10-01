# W16-JB — bandobj + char units: anonymous rows named, Save bodies written, ctor/dtor shape (2026-10-01)

**Branch** `w16-jb`, rebased onto main `99b26594d` (after W16-JE and the comment clean-up).
**Ruler** `name_check` (graded, read from `report.json` `provenance.diff_config`).
**Scope** every unit whose source is under `src/system/bandobj/` or `src/system/char/`
(110 units). Rows were ranked by `size × (100 − fuzzy)` from the worktree's own built
`report.json`.

## 1. Whole-branch A/B

`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16-jb-ab --patch <git diff main..w16-jb, docs excluded>`,
**one run**:
- **Worktree:** fresh, at main `99b26594d`.
- **Patch:** 53 files, kinds map + source; `symbols.txt` untouched.
- **objdiff-cli:** pinned across both legs (sha256 `c1b7d95240a35cd6`).
- **Legs:** both read at a split fixed point. Leg B had 347 recompiles.
- **Run dir:** archived to `~/tmp/w16jb/ab_run/20261001-091435-w16-jb-branch-3977434/`.

```
leg A: matched=47759 masked=23822 honest=23937 code%=48.044130
leg B: matched=47923 masked=23924 honest=23999 code%=48.263584
Δmatched=+164  Δmasked_equal=+102  Δhonest=+62  Δcode%=+0.219454pp  Δcode_bytes=+22488
Δfuzzy=+0.140453pp   (legA 56.697243 -> legB 56.837696)
units at 100% [mpn]: 299 -> 302 (CharBoneOffset, CharPosConstraint, CharSleeve; 0 fell off)
units at 100% [all-rows-fuzzy]: 256 -> 256
unit net = +164 over 40 units; 0 units regressed
[control none] +28,444 B -- NOT_APPLICABLE (source in patch)
```

**Prediction, written before the run:** about +163 (±5) functions and +22.3 KB. It came from
the in-tree increments on the pre-rebase base (`169512b3e`): 47,391 → 47,554 functions and
4,830,232 → 4,852,540 B. **Measured: +164 / +22,488 B.** Leg B equals the branch's own in-tree
build exactly (47,923 matched).

**Row-level diff of the two archived leg reports: 174 rows up, 2 down, 0 off 100.**
- 35 GONE/NEW pairs are the 35 newly named anonymous rows. Every GONE row was an `fn_` row,
  and 20 of the 35 new names read 100.
- Of the 49 rows that reached 100, 20 are those new names.
- Δhonest (+62) is below Δmatched (+164) because the constructor fixes also bring their EH
  funclets to 100, and funclets pair by byte signature (`masked_equal` +102).

## 2. Population and how it was split

At lane start (main `169512b3e`, settled worktree build) the scope held **2,052 sub-100 rows,
weight 115,635**. I wrote a classifier (`~/tmp/w16jb/classify.py`) that runs
`objdiff-cli diff --batch --include-instructions` (graded ruler, via `objdiff.json`) over every
named sub-100 row and buckets it by what is charged:

| class | rows | bytes | weight | handling |
|---|---:|---:|---:|---|
| anonymous `fn_` at fuzzy 0 | 529 | 81,788 | 81,788 | identify on retail bytes |
| INSN (any insert/delete/replace/diff_op) | 418 | 178,512 | 32,219 | source work (this lane) |
| RELOCNAME (only relocation-name args) | 165 | 42,036 | 223 | **left to W16-JE** (coordinator scope note) |
| REGONLY (only register args) | 33 | 24,056 | 43 | skipped: register-allocation residue |
| ARGMIX | 28 | 11,404 | 46 | not worked |
| NOCHARGE (name pinned in a foreign unit) | 16 | 904 | 904 | not worked (re-home candidates) |

The plan was six forks on disjoint unit sets. The harness refused every fork launch
(concurrent-subagent limit already in use by other lanes), so the lane ran serially in one
worktree. Rule for every edit: full `./tools/ninja-locked` build (forced re-split after any map
edit, iterated to a `symbols.txt` fixed point), whole-report row diff against the previous
build, keep the edit only if no row went down. The three accepted exceptions are in §5.

I fixed no row that was already in the RELOCNAME class. One row moved INTO that class:
`PatchDir::LoadStickerTex` (§4.1), whose only remaining charge is a folded `push_back` name.

## 3. Anonymous rows identified

### 3.1 Callee/string overlap (W16-HS method, scoped to bandobj + char)

`~/tmp/w16hs/sem.py` re-scoped (`~/tmp/w16jb/sem.py`): retail fingerprint strings and map-named
callees against each candidate's relocation names in the unit's base object. Proposal rule
unchanged (top overlap ≥ 2, margin ≥ 1): **35 proposals / 11,004 B**.

One deviation, and why: W16-HS dropped any candidate defined in more than two base objects as
a header inline. In this tree that filter is wrong for bandobj/char, because unit `.cpp` files
`#include` other `.cpp` files (scatter includes), so a non-inline free function such as
`CharInit` is defined in three objects. I kept all 35 and let identity evidence decide.

`tools/anon_proposal_adjudicate.py` was run normally and with `--independent`; W16-HS's
pre-registered `decide.py` gave **identical verdicts in both modes**: 19 MECH / 4 DRIFT /
6 WITNESS accepted, 6 refused (drift without anchor, e.g. `BuildContourCap`,
`CharacterTest::Load`, `FlowNode::Handle`).

Independent retail-byte check on the template-shaped accepts: each of the nine
`NewObject@Char*` rows calls exactly the proposed class's `StaticClassName` and constructor
(e.g. `0x8236BB88` → `0x8236A528 StaticClassName@CharBlendBone`, `0x823C4230 ??0CharBlendBone`).

**29 rows named; in-tree +9 fns / +1,108 B; 10 rows to 100.**

### 3.2 Caller binding (W16-HR method)

Over the 482 remaining anonymous rows: 346 have no caller witness, 59 one name defined in a
foreign unit, 19 one name in the row's own unit, 21 one name with no base definition. The
own-unit class went through `--independent` adjudication and W16-HR's `decide_call.py`
unchanged: 3 accepted, 13 refused, 3 held.

- The 3 accepts were template instantiations (`__uninitialized_copy<ObjOwnerPtr<Waypoint>>`,
  `ObjVector<ObjOwnerPtr<Waypoint>>::resize`, `__uninitialized_copy<MeshFace>`). Naming them
  lowered three callers that spell a same-shape twin (forgiven placeholders before), so they
  are ICF template folds. **Reverted**; they need proven aliases (W16-JE's territory).
- **Seven refusals were refusals of OUR body, not of the identity**: the caller-witnessed name
  was a `Save` whose body in our source is a 4-byte `SAVE_OBJ`/`MILO_ASSERT(0)` stub. Retail
  has a real serializer at each address. §4.2 writes them.
- `0x822AF178`, offered as `BandSwatch::Save`, is a scalar deleting destructor (dtor call then
  `MemFree`), so the caller binding was wrong there and it is not named.

### 3.3 Vtable slots

A join of retail RTTI vtables to ours (`tools/vtable_order_sweep.py` helpers,
`~/tmp/w16jb/vtjoin.py`) over in-scope anonymous rows found only small rows: vtordisp thunks
(`$4…`), forwarders, and two classes our source does not define (`Label3d` slots pinned in
CharBoneDir, `PatchRenderer` in BandSwatch). Eight thunks scored 100, but all eight thunk names
are already mapped at other addresses (injectivity refusal), and settling which address is
right is a deep adjudication worth 84 B. **Not named; recorded.**

The `.rdata` references to most remaining anonymous rows are `.4byte fn_X+0x40` entries in
unnamed `lbl_` objects (EH IP-to-state tables), not vtables. That is why the remaining 346 rows
have neither a caller nor a vtable witness.

## 4. Source fixes

### 4.1 Wrong callee exposed by naming

`PatchDir::LoadStickerTex` called an invented `_outline_MakeLoader` helper. Retail calls
`PatchSticker::MakeLoader` (`0x822738D8`, named in §3.1) out of line, and tests
`sticker->mLoader` with a signed compare. `MakeLoader` is now `__declspec(noinline)` and the
helper is gone. Result: 97.83 → 99.83; the residue is a folded `push_back` name.

### 4.2 `Save` methods that are real serializers in retail

Each one was a `SAVE_OBJ` / `MILO_ASSERT(0)` stub (the debug-build shape). Retail writes the
current revision, then the loader's current-revision fields in load order:

| address | name | B | shape |
|---|---|---:|---|
| `0x822FA7E0` | `VocalTrackDir::Save` | 1,000 | rev 7, RndDir, PostLoad's rev ≥ 7 fields, then the track save |
| `0x8234ED60` | `BandTrack::SaveTrack(BinStream&, bool, bool)` | 244 | rev 3, mSimulatedNet, mInstrument, meters/feedback/triggers unless proxy |
| `0x822E5520` | `GemTrackDir::Save` | 608 | rev 12, PreLoad's fields (arrays as loops), SaveTrack, TrackDir |
| `0x822D75B8` | `StreakMeter::Save` | 264 | rev 3, multipliers, proxy-gated triggers, RndDir |
| `0x8231EB18` | `ReviewDisplay::Save` | 112 | rev 0, mScore, UIComponent |
| `0x822B94C0` | `CrowdMeterIcon::Save` | 88 | rev 0, RndDir |

All six reached 100 on the first build. `SaveTrack` has no name in any reference decomp (the debug build
stubs saves); the name is descriptive and mirrors `LoadTrack(BinStream&, bool, bool, bool)`.
Both track dirs pass `(bs, IsProxy(), true/false)`; the third argument is unused in the body.

### 4.3 `ObjPtr` constructor inlining (the largest lever)

Retail inlines the `ObjPtr` two-arg ctor at member-init sites (vtable, owner and null pointer
stored in place) in most bandobj/char constructors, where ours called it out of line. The tree
already has per-TU levers for this (`obj/ObjPtr_p.h`, `obj/Object.h`). The plain-inline one,
`RB3_OBJPTR_INLINE_TWOARG_CTOR` (ctor defined in-class, so MSVC decides per site), was right far
more often than `__forceinline`:

- **First batch:** 27 TUs whose sub-100 ctor had no lever. Result: **+65 fns / +8,188 B,
  0 rows down, 23 rows to 100.**
- **Swap:** six TUs moved off a stronger or owner-only lever (BandLeadMeter, BandScoreboard,
  TrackPanelDir, CharClip, CharEyes, Character). Their ctors reached 100. Result:
  **+33 fns / +3,828 B**; the three recorded exceptions in §5 come from this step.
- **`RB3_TU_OBJPTR_FORCEINLINE_CTOR`:** kept for CrowdMeterIcon and OverdriveMeter (each ctor
  to 100).
- **StreakMeter and EndingBonus:** forceinline broke `SyncObjects`' and `MiniIconData`'s
  out-of-line calls, while the plain-inline lever took both ctors to 100 with nothing down
  (+1,368 B and +740 B).
- **Blind batch:** the remaining 40 lever-free unit sources. Only BandCrowdMeter and BandButton
  gained (+124 B); the other 37 were inert and were reverted.
- **CharHair is left out.** With the ctor inlined, `CharHair::Point`'s ctor becomes a leaf that
  preserves `r6`, and MSVC's intra-TU callee-clobber analysis (W16-ER) then keeps `this` in `r6`
  across the call in `ObjVector<Point>::resize`/`PropSync`. Retail saves it in `r30`, so retail
  compiled that caller without the leaf's clobber set. That is probably a COMDAT copy from
  another TU, and it cannot be reproduced inside CharHair.cpp. Moving the definition is
  inert, as W16-ER also found.

### 4.4 Implicit vs user-declared destructors

Retail emits vtable/vtordisp re-stores only for user-declared destructors:

- **Made implicit:** `~BandLeadMeter`, `~CrowdMeterIcon`, `~EndingBonus`, `~LayerDir`,
  `~BandTrack`, `BandCharDesc::~Patch`, `BandCharDesc::Head`'s dtor (its inlined copy
  re-stored Head's vtable in `~BandCharDesc`), and `~TrackPanelDir`. The retail `~TrackPanelDir`
  (`0x82309DE0`, named this lane) also does not free `mGemTrackRsrcMgr`; the old comment said it
  "plain-deletes it", which predated the dtor's identification. Its only release is
  `SyncObjects`' `RELEASE`.
- **Made user-declared:** `~CharCuff` (retail re-stores the vtables).
- **Not changed:** `~BandCharDesc` itself. Making it implicit took it 91.8 → 57.6.

### 4.5 BandTrack: function-local statics and `MyTrackPanelDir()`

Retail BandTrack builds its Symbols and Messages as guarded function-local statics at the point
of use (atexit-registered), not the `utl/Symbols.h` / `utl/Messages.h` globals. Converted:

| function | before → after |
|---|---|
| `SetPlayerFeedbackShowing` | 30.4 → 100 |
| `Retract` | 52.6 → 100 |
| `ResetPlayerFeedback` | 29.7 → 100 |
| `SoloStart` | 52.6 → 100 |
| `SoloEnd` | 67.7 → 100 |
| `SoloHit` | 39.2 → 100 |
| `GameWon` | 85.1 → 100 |
| `Reset` | 78.2 → 98.3 |

Three spellings appear, and each was read off the guard-bit order before writing:
- a `Message` built from a temporary `Symbol` (`static Message reset("reset")`);
- a static `Symbol` followed by a static `Message`;
- a static `Symbol` alone.

`EnablePlayer`, `SpotlightPhraseSuccess`, `SetupCrowdMeter` and `CodaSuccess` call the
out-of-line `MyTrackPanelDir()` in retail, not an inline `dynamic_cast`:

| function | before → after |
|---|---|
| `CodaSuccess` | 65.7 → 100 |
| `SpotlightPhraseSuccess` | 0 → 94.1 |
| `EnablePlayer` | 30.9 → 93.7 |
| `SetupCrowdMeter` | 81.1 → 98.7 |

`DisablePlayer` keeps the inline cast. Converting it shrinks its 0xE0 frame and unpairs eight
of its byte-signature `~DataNode` funclets.

### 4.6 Behaviour fixes

- **`CharHair::Point` defaults.** Retail stores `outerRadius = 0` and `sideLength = -1` (off).
  Ours set `outerRadius = -1` and left `sideLength` uninitialised.
- **`~TrackPanelDir`** no longer deletes `mGemTrackRsrcMgr` (§4.4).
- **Saves (§4.2).** Six `Save` methods that asserted now serialize, as retail does.

## 5. Rows that went down (none from 100)

In the A/B, two rows end lower than main:

- **`Character::PostLoad` 99.72 → 99.69.** This came from the lever swap (§4.3), which took
  `??0Character` to 100 (+716 B). Accepted.
- **GemPlayer funclet `fn_826BC9A8` 70.91 → 70.45.** This came from the §3.1 naming commit. It
  is a 40 B anonymous funclet that re-paired after a callee was named. Accepted.

The in-tree step that swapped the BandLeadMeter lever also lowered two of its funclets,
61.4 → 61.3, but they are net up against main.

## 6. Left, by blocker

- **346 anonymous rows with no caller or vtable witness (~50 KB).** These need per-row reading
  of retail bytes. The biggest pools are OutfitConfig (44 rows), CharBoneDir (40), BandSwatch
  (29), StreakMeter (27) and Character (24).
- **Classes our source lacks:** `Label3d` (slots in CharBoneDir's pin) and `PatchRenderer`
  (BandSwatch's pin). Rows here can only be named once those classes are ported.
- **Block layout / tail merge:** `PropSync(BandCharDesc::Patch)` (656 B at 0) and
  `PropSync(OutfitPiece)` (480 B at 0). Retail cross-jumps the shared `PropSync<int>` call of
  arms 2–4; ours keeps one call per arm. No source lever found.
- **Bool return:** `BandTrack::Reset` residue is `GetNoBackFromBrink()` returning `bool` in
  retail (it is declared `int` in `TrackInterface` and band3's `Track`).
- **`CharHair` ctor lever:** blocked by the callee-clobber effect (§4.3).
- **Vtordisp thunk names:** eight thunk names that conflict with existing map rows (§3.3).
- **Large INSN rows not attempted:**
  - `BandDirector::OnMidiShot5Cleanup` (1,172 B at 27.8)
  - `ChordShapeGenerator::BuildChordMesh`
  - `BandCharDesc::ComputeDeformWeights`
  - `BandFaceDeform::DeltaArray::AppendDeltas`
  - `BandIKEffector::DoFancyElbow`
  - `CharLipSync::PlayBack::Poll`
  - `CharKeyHandMidi::Poll`

## 7. Gates

Run on the rebased tip before the docs commit:

- `python3 tools/map_name_injectivity.py`: **OK**, 32,194 applied rows, 32,193 distinct names,
  injective (+1 enumerated internal-linkage exception).
- `python3 tools/icf_alias_finder.py --validate`: **PASS**, 1,618 map-consistent,
  254 tolerated, **0 contradicted**, 1,873 total.
- `tools/native_build_gate.sh`:
  `NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0`.
  Only this docs-only commit came after it.
- **No alias groups were added**, so there was no `icf_pair_adjudicate.py --chase` to run. The
  three template names whose callers spelled a fold twin were reverted rather than aliased
  (§3.2).
- **`symbols.txt` is unchanged** on the branch. One Class-4 merge happened in-tree during the
  reverted caller-binding step (§3.2), and it was reverted with that step.

## 8. Rebase

The branch was rebased from `169512b3e` onto main `99b26594d` (W16-JE + the comment clean-up).
- **Header conflicts:** comment-only file-header conflicts took main's wording. My
  lever `#define` lines were kept above them.
- **`TrackPanelDir.h`:** main's wording said `~TrackPanelDir` "plain-deletes" the raw pointer,
  which is false once this branch's dtor change lands, so the resolution keeps the corrected
  fact and main's closing sentence.
- **BandLeadMeter fixup:** one automatic resolution doubled the BandLeadMeter lever, and the
  fixup commit `778a47c47` repairs it. Afterwards the lever defines equal the pre-rebase tip in
  every touched file (checked mechanically).
