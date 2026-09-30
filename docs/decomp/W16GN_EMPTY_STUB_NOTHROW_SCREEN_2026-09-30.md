# W16-GN — a screen for W16-GI's nothrow-stub mechanism, and what it found

Date 2026-09-30 · base main `8af79551` · worktree `~/tmp/wt-w16-gn` · branch `w16-gn`
Pre-GI validation tree: `~/tmp/wt-w16gn-pre`, branch `w16-gn-pre` at `85b84e32`.

Ruler: graded `name_check`, read from `report.json`'s own `provenance`. Every
number is from a full `./tools/ninja-locked` + `report.json`; whole-binary
deltas are from `tools/ab_measure.py --from-dirty` (both legs settled).
The two pre-registrations (P1, P2) at the bottom were committed BEFORE the
edits they predict (`50919cf8`, `4cdee26d`).

## Verdict

**The mechanism is real and nearly drained. Across the WHOLE binary the screen
finds exactly two named-caller true positives: W16-GI's own row, and one more —
`?Handle@RndPropAnim@@` (3,136 B), now at 100.** In the game layer proper, GI
was the entire population. A second, cheaper fix came from the screen's
EH-deficit pass (`BandProfile::GetBandName`, +216 B), which is a different
defect class (local-static dialect), not a stub.

    fix                                   Δmatched  Δcode_bytes   commit
    RndPropAnim::ForeachKeyframe (port)        +1       +3,136    8b5b84d7
    BandProfile::GetBandName (local static)    +1         +216    9b522563
    lane total                                 +2       +3,352

    final tree: 44,156 matched / 4,181,340 B / 40.805233% / fuzzy 50.498394 /
    masked_equal 23,323   (base 8af79551: 44,154 / 4,177,988 B / 40.772522%)

## 1. The screen — `tools/nothrow_stub_screen.py`

Caller-keyed. For every sub-100 row (`fuzzy_match_percent < 100`) it runs
`objdiff-cli diff --batch --include-instructions` per unit (JSONL, keyed on
`match_type`) and, at every aligned pair where both sides are `bl`:

- **base callee**: defined in the SAME compiled object (our COFF, via
  `coff_bodies_ext`, which carries the STLPORT-1 EH-prefix fix), a **LEAF**
  (no REL24 relocation to anything but the save/restore helpers, no `bctrl`),
  and ≤ 64 B;
- **retail callee**: resolved from `fn_<addr>` or the map's inverse; size from
  retail `.pdata` (fallback `symbols.txt` — sub-`.pdata` leaves have no unwind
  record), **NON-LEAF** (≥1 `bl`/external tail-`b`/`bctrl` decoded word by word
  in its extent) and ≥ 2× the base size.

Layer is decided by `objdiff.json` `metadata.source_path`
(`src/band3/`, `src/network/`), **not by unit name** — the first version keyed
on `default/band3/` and silently dropped every bare-heading game unit
(`default/RockCentral`, `default/VocalPlayer`, …), halving the population
(2,007 → 3,996 rows). Same trap CLAUDE.md documents; it bit this tool too.

`--eh` adds a second pass: rows where OUR `FuncInfo.maxState` < retail's, via
`tools/eh_state_screen.py`.

### Threshold, from data (not a guess)

`--survey` on the pre-GI tree, every paired same-obj `bl` in sub-100 game rows
(909 sites):

    base / retail          sites
    non-leaf / non-leaf     763
    non-leaf / leaf          87   (reverse direction: W12-A's surplus-EH class)
    leaf / leaf              53   (45/53 exact size agreement = the two size
                                   readers agree; a built-in control)
    leaf / NON-LEAF           6   <- the mechanism

Leaf base sizes among the 6 flips: 4 / 16 / 20 B; legitimate leaf/leaf pairs
run 4–200 B. So **leafness is the discriminator; size is only a guard**:
`--base-max 64` (16 insns), `--min-ratio 2`. Controls C2/C3 below confirm the
leafness choice mechanically.

### Validation on known positives (fires before, silent after)

    positive                     tree                    bl pass       --eh pass
    Handle@BandStorePanel (GI)   85b84e32 (pre-GI)       EXPECT-HIT    EXPECT-EH-HIT   PASS
                                 8af79551                EXPECT-SILENT EXPECT-EH-SILENT PASS
    Handle@RndPropAnim           4fdcb017 (pre-fix)      fires (named) not in list (!)
                                 17f4749d (post-fix)     EXPECT-SILENT PASS

Between `85b84e32` and `8af79551` the ONLY change in the whole EH-deficit set
is GI's row (retail maxState 25; ours 23 → 25). `--selftest` runs predicate
known answers, retail decoder known answers (`0x826067C0` = 0xC8 B non-leaf;
`0x826C3888` = 4 B `blr` leaf), our-side leaf parsing, and the nested-unit
maxState read; `--self-break` inverts leafness and the selftest FAILS as
required.

### Hit counts, rates, precision

    scope (8af79551)     sub-100 rows  paired  bl sites  hit sites  NAMED hits  true pos.
    game (source_path)        3,996    1,267     2,305        10          0         0
    all layers               27,818    6,505    11,335        47          2         1 (PropAnim)
    all, after both fixes    27,816    6,503    11,313        46          1         0

- Named-caller precision (all layers): **1 / 2**; the other is the
  `vector<Vector2>` ctor → `__uninitialized_fill_n<Vector2>` (48 B) vs retail's
  `<String>` (96 B): a template-fold mispair, shape (b).
- Anonymous `fn_` callers: **45 / 45 false positives**, every one an EH unwind
  funclet where OUR funclet calls a trivial dtor/placement-delete and retail's
  paired funclet calls an UNRELATED class's dtor (`~String`,
  `~ObjRefConcrete<…>`, `~Object`, …): cross-function mispairs by byte
  signature, shape (d). The tool reports them separately.
- Hit rate: 0.72% of paired sub-100 rows raw; **1 / 6,505 = 0.015%** named and
  real. This is not a null (it fires on both known positives and on nothing
  else named-and-real) — it is a small population.

### The EH-deficit pass (complement)

After fixing a real defect in `tools/eh_state_screen.py` (below): 47 sub-100
rows all layers (7 game), plus **67 rows at fuzzy 100 that also carry a
deficit** — so a deficit alone is not a code defect (EH tables are not scored).
Adjudicated, all 7 game rows:

    row                                          size  fuzzy   r/o  cause
    VocalPlayer::Poll                            3388  93.84   7/4  local-object/regalloc divergence; deferred
    Game::Game                                   1504  80.80  14/10 58-insn TU5 block missing (Symbol x3,
                                                                    JoypadSubscribe, CriticalSection, ...)
    RockCentral::OnMsg(ServerStatusChangedMsg)   1024  94.02   9/6  13-insn block missing (see §4) + wrong
                                                                    callee SystemLanguage vs retail SystemLocale
    VocalTrack::VocalTrack                        868  84.70  24/21 ObjPtr ctors out-of-line vs inlined; deque folds
    TrackPanel::TrackPanel                        460  99.96   7/6  only charge = push_back template fold; deficit inert
    BandProfile::GetBandName                      216  67.20   2/1  local-static Symbol -> FIXED (+216 B)
    ProfileMgr::FakeProfileFill                   136  27.88   2/1  MAP mispair (retail body = StoreArtLoaderPanel dtor)

Nothrow-stub precision of the EH pass on the current tree: **0/7** (GI was 1/8
pre-GI). As a general defect pointer: 5/7 real source defects, 1 map, 1 inert.
Over all 47 rows, no deleted retail `bl` corresponds to an inlined same-TU stub
(checked by listing every retail-only call); what recurs instead is the
**function-local-static dialect** (`Symbol`/`Message` ctor + `atexit` behind a
guard bit): `BandTrack::DisablePlayer` (1,240 B), `BandTrack::Reset` (1,116 B),
`GetBandName` (fixed), `CharClipSet::SetBpm` (partly).

## 2. Two sub-mechanisms, one predicate (new)

`Handle@RndPropAnim` had retail maxState **33 == ours 33 on BOTH sides of the
fix**. So its stub changed no EH region at all. What it changed: MSVC saw the
16 B leaf could not write the caller's local-static guard word, so it **elided
the guard reload after the call** (the one `delete` at idx 480) and the whole
function's regalloc diverged (a pervasive r10↔r11 swap over ~150 charged
instructions). GI was the other half: nothrow → EH region elided → scheduler
freed. Both come from "the same-TU callee is fully analyzable".

- The `bl` pass sees both halves (when the stub is out of line).
- The `--eh` pass sees only the nothrow half — but sees it even when the stub
  was INLINED. **Neither alone is enough**; GI is caught by both, PropAnim only
  by the `bl` pass.

## 3. Fix P1 — `RndPropAnim::ForeachKeyframe` (+1 fn / +3,136 B)

Our `ForeachKeyframe` was `{ return DataNode(0); }` from the 2026-05-26
scaffold — here the oracles were NOT the cause (both DC3 and rb3-Wii have real
bodies). Retail `fn_82429C38` is 2,256 B (`.pdata`) in no `.text` split and
calls **no `RemoveKey`**, so the control flow is rb3-Wii's (DC3's newer
`sRemoveFrame` branch is not RB3), spelled in our DC3-shaped accessor API.

    pre-registered P1a/P1b/P1c: ALL HELD, including the ~50% one (P1b)
    Handle@RndPropAnim  fuzzy 98.8648 -> 100.0  mpn -> 100.0  784/784 equal
    A/B: leg A 44154 / 40.772522 -> leg B 44155 / 40.803127
         Δmatched +1  Δcode_bytes +3136  Δhonest +1  Δmasked 0
         leg-B recompiles 2 (PropAnim + MetaMusic's scatter-include copy)
         unit net +1 == whole-binary; none control +3136 (source-patch shape)

Controls, each a full build (commits `31315c17`/`fd39c465`, `9941264c`/`604dbde4`,
`53a21572`/`5fb86a1a`):

    C2 minimal CALLING body (da->Obj(2); return 0;)  Handle 100.0, whole-binary == fix
       => body CONTENT is irrelevant; only the callee being opaque matters
    C3 LEAF that writes a global (sReplaceKey = true) Handle 98.463 / mpn 99.872,
       idx-480 delete back, whole-binary == leg A
       => a memory write is not enough; an opaque call is. Validates the
          leaf/non-leaf predicate. (Cannot separate "may throw" from "may write
          the guard": both are properties of an opaque call.)
    C1 sabotage Float(4)->Float(5) in Handle's set_key arm
       exactly 1 charge (addi 0x20 -> 0x28), fuzzy 99.998726, -1 fn / -3136 B
       => the witness discriminates

A process slip worth recording: the first C3 attempt used `git revert -q`
(invalid flag); the `&&` chain stopped, no edit was made, and the build
re-measured the C2 tree — a "C3 = 100" reading that was really C2. Caught by
checking `git log` before interpreting; discarded; redone.

⚠ **For the alias-owning lane (NOT touched here):** `scripts/symbol_aliases.json`
lists `?ForeachKeyframe@RndPropAnim@@QAA?AVDataNode@@PBVDataArray@@@Z` as a
folded member of a `return DataNode(0)`-shaped group (with `DataNop`,
`DataNotify`, …). That membership only existed because of our stub; retail's
body is 2,256 B and cannot fold. It is now CONTRADICTED by our own obj. Metric
effect measured here: 0 (the only call site targets a forgiven placeholder).

## 4. Fix P2 — `BandProfile::GetBandName` (+1 fn / +216 B)

EH-deficit hit, not a stub: retail constructs a guard-protected function-local
`static Symbol band_default_name("band_default_name")` at entry; ours read the
`Symbols2.h` global, verbatim from rb3-Wii.

    pre-registered P2a/P2b/P2c: ALL HELD
    row fuzzy 67.2037 -> 100.0, 54/54 equal
    A/B: leg A 44155 / 40.803127 -> leg B 44156 / 40.805233
         Δmatched +1  Δcode_bytes +216  Δhonest +1  1 leg-B recompile  unit net +1
    C1b (d53c5fa7, reverted 162583d6) strlen(...) == 0 -> == 1: row 96.944, the compare
        charged, whole-binary -1 / -216 B. Reverted.

## 5. Tool defect fixed: `eh_state_screen.py` globbed `asm/*.s` flat

Every NESTED split heading was excluded from the retail side, including GI's
row. Recursive glob: retail EH functions 6,948 → 8,816, joined 4,250 → 5,578,
deficits 98 → 114 (on 8af79551). Commit `3cc5bbce`. W12-A's deficit list was
therefore blind to most game units.

## Refusals / candidates NOT fixed, with evidence

- `vector<Vector2>` ctor (88 B): named bl-screen hit, template-fold mispair — refused.
- 45 anonymous funclet hits: cross-function mispairs — refused.
- `TrackPanel` ctor: EH deficit is code-inert; its one charge is an alias
  question (another lane's file).
- `ProfileMgr::FakeProfileFill`: map mispair; map work out of scope.
- `RockCentral::OnMsg(ServerStatusChangedMsg)`: retail has, after the third
  `RegisterExtraProtocol`, `addi r29,this,0x98; bl fn_8284D968 (12 B);
  bl fn_8284D880 (16 B, &this->u64@0xC0); _snprintf(staticbuf@lbl_82CC8F34, 0x14,
  "%llu", this->u64@0xC0)` — TU5-era, no oracle in either repo — and calls
  `SystemLocale()` where we call `SystemLanguage()`. Needs identification of two
  Quazal leaves and a member; not attempted.
- `VocalPlayer::Poll`, `Game` ctor, `VocalTrack` ctor, `BandTrack::DisablePlayer`,
  `BandTrack::Reset`: real divergences of other classes; not attempted.

## Deliberately NOT done

- No `target_symbol_map.json` entry for `fn_82429C38` (in no `.text` split);
  no splits/map re-homes; `scripts/symbol_aliases.json` untouched.
- No permuter; no caller-side reshaping of either `Handle`.
- Did not look for the mechanism in rows whose base symbol is UNPAIRED (21,313
  of 27,818 sub-100 rows have no base body; the screen cannot see them).
- Did not screen non-`bl` transfers to stubs (e.g. a tail `b`) or stubs in a
  DIFFERENT object (cross-TU calls cannot be proven nothrow without LTCG, so the
  mechanism cannot arise there).
- The pre-GI worktree `~/tmp/wt-w16gn-pre` (branch `w16-gn-pre`, no commits)
  is kept so the validation is reproducible; remove when the lane lands.

## Pre-registration P1 — port `RndPropAnim::ForeachKeyframe`

Written before editing `src/system/rndobj/PropAnim.cpp`.

State: `?ForeachKeyframe@RndPropAnim@@QAA?AVDataNode@@PBVDataArray@@@Z` is
`{ return DataNode(0); }` (16 B, a leaf) in our build. Retail's paired callee at
the call site in `?Handle@RndPropAnim@@UAA?AVDataNode@@PAVDataArray@@_N@Z` is
`fn_82429C38`, 0x8D0 = 2,256 B by `.pdata`, NON-leaf, in no `.text` split
(auto unit). Caller row: 3,136 B, fuzzy 98.8648; charges = a pervasive r10/r11
swap in the local-static guard checks + ONE delete at idx 480: retail reloads
the guard word right after `bl ForeachKeyframe` (idx 468), ours does not.

Port: rb3-Wii's control flow (retail has NO `RemoveKey` call, so DC3's newer
`sRemoveFrame` branch is excluded), spelled in our DC3-shaped accessor API.

Predictions:
- P1a (confident): the idx-480 delete disappears (a calling callee forces the
  guard reload).
- P1b (uncertain, ~50%): the r10/r11 swap also resolves and the row crosses:
  +1 fn / +3,136 B whole-binary. If it does not, fuzzy rises a little, 0 B.
- P1c: leg B recompiles exactly 2 TUs (PropAnim.obj, and MetaMusic.obj via its
  scatter-include of PropAnim.cpp); no other row in PropAnim or MetaMusic moves
  (only `Handle` calls `ForeachKeyframe`; no map entry is added, so the ported
  body is compiled but unscored).
- Falsifier F1: the idx-480 delete survives with a real body ⇒ the "our stub
  is proved memory-inert, so the reload is elided" reading is wrong.
- Not predicted to move: `scripts/symbol_aliases.json` lists
  `?ForeachKeyframe@RndPropAnim@@…` as a folded member of a `return DataNode(0)`
  group. That membership exists BECAUSE of our stub (retail's body is 2,256 B
  and cannot fold with a 16 B body). It becomes contradicted by this port; it is
  another lane's file and is NOT touched here. Expected metric effect: 0 (the
  only call site's target is a forgiven placeholder `fn_82429C38`).

## Pre-registration P2 — `BandProfile::GetBandName` local-static Symbol

Written before editing `src/band3/meta_band/BandProfile.cpp`. NOT the nothrow-stub
mechanism: an EH-deficit hit (retail maxState 2, ours 1) whose retail-only
calls are one guard-protected `??0Symbol@@QAA@PBD@Z("band_default_name")` at
function entry. Ours reads the global `?band_default_name@@3VSymbol@@A`
(`Symbols2.h`), verbatim from rb3-Wii.

Fix: `static Symbol band_default_name("band_default_name");` as the first
statement of `GetBandName` (the RB3_HANDLE_LOCAL_STATIC dialect GI also saw).

- P2a (confident): the 13 guard/Symbol-ctor deletes (idx 5-20) vanish, and
  our EH maxState for the row becomes 2.
- P2b (~60%): the register shift (`__savegprlr_28` vs `_29`) follows from the
  extra live value and the row crosses: +1 fn / +216 B whole-binary.
- P2c: exactly 1 leg-B recompile (BandProfile.cpp; no scatter-includes of it);
  no other row moves.
- Falsifier F2: deletes survive ⇒ the static is spelled/placed wrong.
