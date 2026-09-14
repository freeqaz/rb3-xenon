# W16-AE (fable escalation) — `0x823eb548` thunk homing, six span "carves", two tooling gaps, `0x827f42a8`

Lane: **W16-AE**, branch `w16-ae` off main `bb9b7e55`, worktree `~/tmp/wt-w16-ae`.
Escalation of lane W16-AB (opus, `docs/decomp/W16AB_SIZE_MISMATCH_44_STUB_BODIES_NOPDATA24_2026-09-14.md`),
which reported four things it "could not" fix. Brief: `~/tmp/brief_w16ae.md`.

**Headline: every one of W16-AB's four "cannot be fixed" items was fixable, and in each case the
stated reason was not the real cause.** Two were wrong-unit / wrong-name map defects hiding
behind a plausible label, one was a premise that was already false when written, and one was a
generator that never read the survivor's own bytes.

Lane-internal, `name_check`, objdiff 4.2.9, same worktree, full builds only:

| | matched_functions | matched_code | matched_code_percent |
|---|---:|---:|---:|
| main `bb9b7e55` (row baseline `~/tmp/rows_w16ab_main.json`) | 43,309 | 3,989,624 B | 38.938522 |
| `w16-ae` tip (build 10) | **43,344** | **3,993,196 B** | **38.973200** |
| Δ | **+35** | **+3,572 B** | +0.034678 pp |

Set-diff of the `fuzzy==100` row set vs main (`tools/rowset_snapshot.py diff`, run inside the
tree): **CROSSED IN 53 rows / 4,320 B, FELL OUT 14 rows / 748 B.** Every FELL OUT row is
accounted for as a byte-neutral reattribution — see §7; there is no real loss.

Commits on `w16-ae`, in order: `569d81b7` `093b927c` `3e27b6bf` (Item 1) · `49c14853` `186a79c6`
(Item 3b generator) · `49948220` (Item 3a) · `c9a2ddd6` (Item 3b carry path) · `2b49abd1` (Item 2 +
Item 4 + 11 withdrawals) · `1fdcb69e` (Item 2 fallen-row adjudication) · `3f9d864f` (coordinator
add-on + `fold_thunk_gate.py` fixes) · this doc.

---

## 1. Item 1 (LEAD) — `0x823eb548`: the vtordisp thunk is SessionSearcher's, not Anim.cpp's

**W16-AB claimed.** Retail bytes at `0x823eb548` are the 16 B thunk
`?Replace@MsgSource@@$4PPPPPPPM@BI@AAXPAVObjRef@@PAVObject@Hmx@@@Z` (proven), the map wrongly names
it `__insertion_sort<RndPollable**>` (80 B), and it cannot be repaired because (a) our COMDAT for the
thunk is emitted only by four `band3/meta_band/` objs, (b) the address "sits inside `Anim.cpp`'s
ENGINE `.text` span", (c) rename-without-re-home ⇒ permanent 0 %, re-home ⇒ circular pin. "The
correct repair is to find the engine TU that emits this thunk."

**What I measured — both premises (a) and (b) were false.**

- *(a) Owning vtable.* Scanning retail `.rdata` for big-endian pointers to `0x823eb548` finds the
  reference inside a vtable whose `??_R4` COL names `.?AVSessionSearcher@@`. `SessionSearcher` is
  `Hmx::Object` + a **virtual** `MsgSource` base at this-offset 24 — exactly the
  `vtordisp{-4,24}` the mangling encodes. The four meta_band objs emit the thunk because they
  instantiate other `MsgSource`-derived classes; **the retail TU that emits this particular one is
  `network/net/SessionSearcher.cpp`, a GAME TU we had never ported** — not an engine TU at all.
- *(b) "Inside Anim.cpp's span".* The zone `0x823EAD68–0x823EB958` was not Anim.cpp's. It was
  covered by **seven circular single-function `.text` pins** — FlowTrigger, Anim, PracticeSection,
  BandUI, Group ×2, VocalNoteList — each carved to fit a folded template name (`__median`,
  `__unguarded_partition`, `Type@InviteAcceptedMsg`, `AddTambourineGem`…). Retail link order
  there is `SessionSearcher.cpp → NetLog.cpp → NetworkEmulator.cpp` (RTTI/vtable references and the
  `.pdata` extents of the 22 functions in the zone).
- *(c)* was therefore moot: the address is re-homed to the TU that genuinely owns it, on geometry
  and RTTI evidence that has nothing to do with the name.

**What I changed.**

- `569d81b7` — ported `src/network/net/SessionSearcher.cpp` (from rb3-Wii dev, shaped by `.pdata`
  extents + retail bytes) and `src/network/net/NetLog.cpp` (the `LogFile` definition that emits
  `~LogFile` at `0x823eb8e0`); removed the 7 circular pins, deleted the drained `FlowLabel.cpp`
  entry, added the two new units. Two porting findings worth keeping:
  `SessionSearcher::OnMsg(const InviteAcceptedMsg&)` returns **bool** (retail `Handle` has
  `clrlwi r11,r3,24` after the `bctrl`; `Handle` 672 B crossed on this change alone), and
  `LogFile` must **not** declare `virtual ~LogFile() {}` — retail's 76 B dtor has no vptr store
  at entry, the elision only an implicitly-declared dtor gets; the Wii dev `delete mFile` body is
  not in retail 360.
- `093b927c` — 22 map rows named in the zone; `0x823eb548 → ?Replace@MsgSource@@$4PPPPPPPM@BI@…`;
  `__insertion_sort<RndPollable**>` **withdrawn** at that address (class `NOT_A_FOLD` — the old
  T1 evidence was vacuous, 0 census sites; our `__insertion_sort` is 80 B / 3 relocs and cannot
  fold onto a 16 B thunk). `0x823eb72c` (`__unwind$129452`, a PracticeSection funclet label from
  the circular pin) deleted — pairs anonymously now.
- `3e27b6bf` — five fold memberships proven by `tools/icf_pair_adjudicate.py --chase` (4 CHASED T1,
  1 our-side COMDAT identity), including `AddTambourineGem@VocalNoteList` → `UpdateSearchList`
  at `0x823eb5d8` (44 B both, slot fold `push_back<ChatReceiver*>` ↔ `push_back<int>` already
  proven at `0x82b5f808`).

**Predicted vs measured.** Builds 3+4: predicted "the 22 zone rows become pairable, the four
mis-named rows financed by wrong names fall out"; measured **+16 fns / +1,520 B** (28 rows gained
in the two new units; 14 rows / 764 B fell out: `Group::__median` 172, `__unguarded_partition` 128,
`BandUI::Type@InviteAcceptedMsg` 88, `VocalNoteList::AddTambourineGem` 44, 9 anonymous funclet
rows that moved unit, `SongData::OnTambourineGem` 20 B financed by the wrong callee name). Build 5
(alias edit only): predicted ~+8 / +668, measured **+7 fns / +708 B**.

**Verdict on W16-AB's "cannot be fixed": wrong.** Real cause: a game TU that was never ported,
whose address zone had been carved into seven engine units by name-fitting pins. "Find the engine
TU" was the wrong search — it is not an engine thunk.

---

## 2. Item 2 — six "need carving" addresses: the surplus was never a second function

**W16-AB claimed.** `0x822c83d0` (span 120 B vs fn 108), `0x82389608` (120 vs 108), `0x8238c798`
(104 vs 96), `0x823ea1c8` (112 vs 104), `0x822c5600` (472 B span, fn at +0x80), `0x8231a578`
(272 vs 224) have "a unique folded spelling but a non-island span" and need carving.

**What I measured.** For each, `.pdata` (big-endian) + retail bytes: every block is
`[function + trailing pad + the NEXT function's 8 B EH prefix]` — the same shape as every other
`.text` block in `splits.txt`. There was nothing to carve. The real defect was that each block sat
in the **wrong unit** (Archive.cpp, PracticeSection.cpp, SongLayout.cpp, BandCrowdMeter.cpp,
BustAMoveData.cpp, UI.cpp) while flanked on both sides by the blocks of the TU that owns it, and the
map row named the **other** member of an alias group — the retail size refutes that name outright:

| addr | map name (ours, size) | retail size = the real body |
|---|---|---|
| `0x822c83d0` | `vector<String>::_M_fill_insert` 112 | 108 = `vector<DeltaArray@BandFaceDeform>` |
| `0x82389608` | `vector<PracticeStep>::_M_fill_insert` 112 | 108 = `vector<CharInterestState@CharEyes>` |
| `0x8238c798` | `__destroy_range_aux<MoveReplacer>` 100 | 96 = `<Weight@PlayBack@CharLipSync>` |
| `0x823ea1c8` | `vector<Hmx::Color>::_M_fill_insert` 108 | 104 = `vector<char>` (SyncStore) |
| `0x822c5600` | `>>(BAMPhrase,BinStreamRev)` 108 | 88 = `>>(BinStream&, ObjVector<Constraint@BandIKEffector>)` |
| `0x8231a578` | `??1Automator` (1 vtable) 132 | 224 = `??1MeterDisplay` (3 vtable stores at +0x18/+0x20/+0x28) |

A tooling defect surfaced on the way: `icf_pair_adjudicate.py` chased every `[S,S]` self-pair
PROVEN at depth 0 without reading a byte (`49c14853`, §4 below) — which is how these survived
"adjudication".

**What I changed (`2b49abd1`).** `.text` lines moved by content into the flanking TU (dtk
re-derived the six `.pdata` lines), each map row renamed to the only spelling whose COMDAT matches,
old survivor withdrawn (`SURVIVOR_NAME_WRONG_SIZE_AND_BODY_DIFFER`) and the folded spelling made
survivor with `folded: []` (a role swap asserting **no** fold). Bonus: `0x822c5580` (84 B, first
function of the BandIKEffector block) named `?resize@ObjVector<Constraint>` — retail's `>>` calls
it where our source does.

**Predicted vs measured (build 5 → 7).** Predicted the six rows at their retail sizes (728 B);
measured **CROSSED IN 10 rows / 912 B** (the six, `resize<Constraint>` 84, the Item-4 thunk 12, and
two anonymous rows `fn_8231A658` 48 + `fn_822C55D4` 40 the re-home made pairable — unpredicted),
**FELL OUT 2 rows / 440 B** (`?RemoveUser@Band@@` 436, `??1ObjList<BitmapOverride>` 4 — call sites
the withdrawn memberships had been forgiving). +8 fns / +472 B.

**Fallen-row adjudication (`1fdcb69e`) — both were map defects, not folds:**
- `0x8268b1f0` (44 B, BandUser.cpp span, called from `Band::RemoveUser` +0x58 where our source
  calls `DeletePlayer`): retail words `7c6b1b78 80630088 39400000 2b030000 914b008c 4d9a0020
  81630000 816b017c 7d6903a6 4e800420` + dead `4e800020` are **byte-identical** to our
  `?DeletePlayer@BandUser@@QAAXXZ`. The map named it `?FinalRelease@CSpPhoneConverter@NUISPEECH@@`
  — a vendor speech-SDK name inside a game TU. Renamed; FinalRelease withdrawn.
  **Retraction:** my own `SIZE_AND_BODY_DIFFER` withdrawal of DeletePlayer in `2b49abd1` (40 vs
  44 B) was the `.pdata`-extent-vs-COMDAT-span artifact — dtk's extent stops at the `bctr` and
  excludes the unreachable trailing `blr`. Record removed.
- `0x822b6530` (4 B): our `??1ObjList<EventCall@EventAnim>` is exactly `(4, [b clear<EventCall>])`
  = retail; BandCamShot.cpp:243 instantiates it. `??1ObjList<BitmapOverride>` was a masked-equal
  twin mis-assignment. Renamed.
- `0x822b6538` (144 B `resize<list<…>>` twin) — **NOT renamed.** A first rename to
  `resize<Target@BandCamShot>` failed the build at `map_name_injectivity_checked.stamp` (that name
  already lives at `0x824cf890`, PanelDir). Retail's callees at +0x58/+0x7c are
  `?erase@list<MatOverride@WorldDir>` and anonymous `fn_824CE130`; retail's own
  `resize<MatOverride>` is at `0x823106d8`. This is a **fourth** instantiation whose insert callee
  is unnamed. Lead only.

Measured build 7 → 9: **CROSSED IN 4 rows / 536 B** (`RemoveUser` 436 + `??1ObjList<EventCall>` 4
predicted; `fn_824CA184` + `fn_824CA1B4` 48+48 unpredicted — retail callers of `0x822b6530` whose
callee name now agrees, the cascade channel), FELL OUT 0.

**Verdict on "need carving, not a line move": wrong.** Real cause: wrong unit + the alias group's
other member's name, with a self-pair adjudicator that could not refuse.

---

## 3. Item 3a — `_denylist` "never polices the applied map"

**W16-AB claimed.** `0x82553fc8`'s argreg refutation "was already in `_denylist` yet the binding
stayed live", so `_denylist` governs only `gen_target_map` auto-emission.

**What I measured — the premise is wrong on both halves.** `0x82553fc8` is **not** in `_denylist`
(it was lifted; its row carries `??1CharCreatorPrefab@PrefabMgr@@QAA@XZ` deliberately), and
`obj_target_symbol_renamer.py` **has honoured `_denylist` since `f3fe9ab1`** (`REFUSAL_KEYS`,
strict-parsed, denylisted rows skipped). The real gap: nothing verified the refusal *reached the
built target objs* — a pre-`f3fe9ab1` "declared and ignored" tree passed every gate there was.

**What I changed (`49948220`).** `check_denylist_applied()` in `scripts/verify_objs_patched.py`:
for every `_denylist` address whose map row still carries a string name (three do on the final tree, on purpose —
the refuted hypothesis stays on record), scan the COFF symbol table of every dtk-split target obj;
name present with no `fn_`/`lbl_` placeholder anywhere ⇒ **exit 7** (new code, documented in the
exit table); malformed denylist or a tree with no target objs/symbols ⇒ exit 7 too (must not pass
by having nothing to look at). Wired into `run_check` (every full build) and `--verify-manifest`;
standalone `--check-denylist [--map <sandbox>]`. Cost: 3,116 objs / ~495.7 k symbols in ~0.4 s.
`scripts/test_denylist_applied.py` — four arms on the built tree: TREE rc 0; MUTATION (sandbox map
with `0x82553fc8` appended to `_denylist`; its name is defined in MemTracker/VocalTrack/PrefabMgr
objs, so the arm is not vacuous) **rc 7 naming the address**; NULL rc 0; MALFORMED rc 7.
Registered in `scripts/test_tools.py`.

Side finding: `0x82c16aa0` is denylisted with a null value and **no** `_denylist_comment` entry —
no rationale on record.

**Verdict: the gap W16-AB described did not exist; a different, real gap did, and is closed.**

---

## 4. Item 3b — why `icf_alias_build.py` minted T1 claims the adjudicator refutes

**W16-AB claimed.** Two T1 groups (`0x82b9b1f8` `RndText::Line` erase 64 B with a call vs the
reloc-free 92 B survivor; `0x82336af8` retail 40 B / 5 relocs vs our `swap<…>` 28 B / 0 relocs)
were emitted by the builder and refuted by `icf_pair_adjudicate.py`.

**Stale-brief finding first:** both memberships were **already withdrawn by W16-AB itself** —
`scripts/symbol_aliases.json` carries `lane: "W16-AB 2026-09-14"`,
`class: CONTRADICTED_DIFFERENT_BODY_CANNOT_FOLD` records on both groups — so "re-run it on those
two groups and require it now refuses them" had no live membership to refuse. The generator fix
is demonstrated on synthetic reproductions of both cases plus the live carry path instead.

**Root cause (measured, `186a79c6`).** The generation loop compared `retail.get(t)` against
`ours.get(b)` — the **folded** spelling's COMDAT — and trusted the map for the survivor NAME `t`.
`ours.get(t)` was never consulted. So a wrong map name whose body is the fold twin of a spelling we
do compile minted a "T1" group with the wrong survivor and the right folded member. The vacuity
guard, size and reloc checks all ran — **on the wrong side**. Fix: `survivor_self_check(rt,
ours.get(t))` refuses a pair when our survivor COMDAT contradicts the retail body at `addr(t)` by
size, reloc count, reloc shape or reloc targets; masked body-word differences alone are counted,
not refused, in the default `shape` mode (that is what an imperfect port of the right function
looks like); `strict` refuses those too; `off` restores the old behaviour so the gate can be shown
to matter. A landed group failing the check is **carried unchanged and flagged**, never dropped.

**Second defect (`c9a2ddd6`).** v1 compared reloc target names literally and refused 572
generation pairs / 279 carried groups — mostly callees differing by spelling inside one alias
class, which `report.json` proves the grader forgives (`??_GPreloadPanel` fuzzy 100.0 with such a
callee) while charging a target in no class (`??_GAppLabel` 99.74 over `MemFree`). Fix:
`load_equivalences()`/`canon_relocs()` rewrite both sides through `symbol_aliases.json` before
`relocs_agree()`. Measured: **336 / 173**. Also: the refusal string named the first *textual*
difference, not the pair `relocs_agree()` rejected (`lbl_82000D78 vs __real@00000000` was blamed
at three sites where that pair is tolerated); `_first_disagreeing_target()` now replicates the
per-pair rule.

**Third defect (`49c14853`, `icf_pair_adjudicate.py`).** `chase()` returned True whenever
`survivor == our_name` at any depth, so every depth-0 `[S,S]` self-pair read PROVEN without
comparing a byte. Measured on the six Item-2 survivors: all six `[S,S]` chased PROVEN before,
all six **REFUTED (BYTES-DIFFER)** after, while the six `[S, twin]` pairs stay PROVEN.
`--chasetest` gains two live self-pair controls (one must REFUTE, one must PROVE — "refuse every
self-pair" fails the test too). 4/4, `--selftest` PASS.

**Tests.** `tools/test_icf_alias_survivor_gate.py` — 21 cases including both brief cases
reproduced synthetically, the equivalence-accept and equivalence-control (must refuse and must name
`?MemFree`), placeholder tolerance, round-trip. PASS 21/21, registered in `scripts/test_tools.py`.

**Full-set run.** The carry path now re-verifies every landed membership against retail bytes and
printed **38 contradictions** on this tree (report-only). Adjudicated with
`icf_pair_adjudicate.py --chase`: **12 REFUTED** → 11 withdrawn in `2b49abd1` with records citing
SLOT-REFUTED / BYTES-DIFFER (groups `0x823ff400`, `0x822b4bd0`, `0x827fb758`, `0x825c2430`,
`0x827bd990`, `0x824aa280` ×2, `0x82304870` ×2, `0x82826b10`, `0x8268b1f0`) and the 12th
(`DeletePlayer` at `0x8268b1f0`) withdrawn then **retracted** in `1fdcb69e` as the
`.pdata`-extent artifact. The remaining 26 were left in place — chase-PROVEN through a slot fold
the flat check cannot see, or word-only differences (imperfect port of the right body). My
pre-compaction tally split those as 8 flat-PROVEN disagreements / 14 CHASED-PROVEN over-flags /
the rest word-only; **that split was not re-derived after compaction** — re-run
`icf_alias_build.py --merge scripts/symbol_aliases.json --survivor-self-check shape` to reproduce.
Nothing was pruned.

**Verdict: the generator defect was real and is fixed; W16-AB's two example groups were already
withdrawn by W16-AB, so that part of the brief was stale.**

---

## 5. Item 4 — `0x827f42a8` (`??_EUIButton@@$4PPPPPPPM@A@`, "STILL_UNDECIDED")

**W16-AB claimed.** 2-word thunk; the chain terminates in a symbol we do not emit; size evidence
is a carve artifact.

**What I measured.** The thunk's branch lands at `0x827f5348`, which the map names
`??_GUILabel@@UAAPAXI@Z`. `??_GUIButton@@UAAPAXI@Z` is **T1-proven against the retail bytes at
`0x827f5348`** by `icf_pair_adjudicate.py --chase` — the deleting dtors of UIButton and UILabel are
body- and reloc-identical once the `??1` chain folds. `??_EUIButton`/`??_EUILabel` are COFF weak
externals defaulting to the `??_G` spelling. So the terminal IS a symbol we emit; it was just
spelled by the other class of the fold.

**What I changed (`2b49abd1`).** New alias group at `0x827f5348`, survivor `??_GUILabel`, folded
`??_GUIButton` + `??_EUIButton` + `??_EUILabel`. Measured: the 12 B thunk row
`UIButton::??_EUIButton@@$4PPPPPPPM@A@AAPAXI@Z` crossed to `fuzzy==100` (predicted).

**Verdict: decided — our `UIButton` does emit the thunk, and it is correct.**

---

## 6. Coordinator add-on — group 1537 (`0x826c3888`), `InputMgr::{Set,Clear}InvalidMessageSink`

Requested mid-lane: add the two memberships **only if `tools/fold_thunk_gate.py` admits them**.

**Verified before gating.** Retail `?Enter@OvershellPanel@@` (`0x825b7840`) +0xb0 and `?Exit@`
(`0x825b26f8`) +0x20 both `bl 0x826c3888`; our `OvershellPanel.obj` names the two InputMgr
functions at exactly those offsets; our COMDATs are (4 B, 0 relocs) = `blr`; bodies are `{}` in
`src/band3/meta_band/InputMgr.cpp:141-142` and in the rb3-Wii oracle; neither name has a map row
or alias mention; **exactly 1 reloc site each across all our objs** (both in OvershellPanel.obj),
so the coordinator's "other OvershellPanel 99.8x rows likely share it" is not the case.

**Gate outcome: ADMIT, FT-EMPTY ×2** (same tier as the group's five prior members). Installed
(`3f9d864f`), call-site witness appended to the group's evidence. Round-trip identical, 3 insertions
/ 1 deletion vs HEAD. `icf_alias_finder.py --validate`: PASS, 1391 map-consistent, 0 contradicted,
1638 groups.

**Two gate defects found and fixed on the way (`3f9d864f`):**
1. `int(r["base_addr"], 16)` crashed on a name with **no map row** — the very case the docstring's
   FT1 tier describes. Routed to FT1 with a stated `retail_F` note.
2. `--install` **clobbered** any group whose evidence carried the tool's OWNED prefix: on a partial
   worklist it replaced `folded` with only that run's admissions. Measured on the first install:
   the group's five prior members (`??3@YAXPAX0@Z`, `CheckMailbox@PlatformMgr`, `Copy@BandTrack`,
   `RunNetStartUtility@PlatformMgr`, `ToggleMuteStatus@SessionUsersProvider`) and the hand-appended
   W15-D/W16-J evidence were **dropped**, and the whole file re-serialised at indent=2
   (75,719-line diff). Reverted (`git checkout --` inside the worktree only). `install()` now
   unions membership, appends evidence, leaves an existing FOLD-THUNK TIER comment alone, and writes
   the house round-trip. `tools/test_fold_thunk_gate_install.py` **fails against the pre-fix
   module** (`FTG_MODULE=<old copy>`) and passes on the fixed one; registered in `scripts/test_tools.py`.

**Predicted vs measured (build 9 → 10).** Predicted +336 B / +0 fns (I assumed Enter/Exit were
already `mpn==100`); measured **CROSSED IN 2 rows / 336 B** (`?Enter@OvershellPanel@@` 204,
`?Exit@OvershellPanel@@` 132 — exactly the predicted rows), FELL OUT 0, **matched_functions +2**
(the fns prediction was wrong: the reloc-name charge had been counting against `mpn` on these rows).

---

## 7. Set-diff vs main, every FELL OUT row accounted for

CROSSED IN 53 rows / 4,320 B: 41 rows in the two new units `network/net/SessionSearcher` (incl.
`Handle` 672, ctor 320, dtor 236, `StopSearching` 200, `__median<NetSearchResult*>` 172,
`__unguarded_partition` 128, the four `MsgSource` vtordisp thunks 16 each incl. `Replace` at
`0x823eb548`) and `network/net/NetLog` (`~LogFile` 76); the six Item-2 rows at their retail sizes
(`??1MeterDisplay` 224, `_M_fill_insert` ×3 108/108/104, `__destroy_range_aux` 96, `>>` 88) +
`resize<Constraint>` 84 + `fn_8231A658` 48 + `fn_822C55D4` 40; `??_EUIButton@@$4…` 12;
`??1ObjList<EventCall>` 4 + `fn_824CA184`/`fn_824CA1B4` 48+48; `PlatformMgr_Xbox::fn_8251E2B4` 40;
`OvershellPanel::Enter` 204 + `Exit` 132.

FELL OUT 14 rows / 748 B — **all byte-neutral reattributions of the same bytes**:

| fell out (unit::row, B) | where the same bytes crossed in |
|---|---|
| `Group::__median<RndDrawable>` 172, `Group::__unguarded_partition<RndDrawable>` 128 | `SessionSearcher::__median<NetSearchResult*>` 172 / `__unguarded_partition` 128 (masked-equal twins; the RndDrawable spellings are now folded members) |
| `BandUI::Type@InviteAcceptedMsg` 88 | `SessionSearcher::Type@InviteAcceptedMsg` 88 |
| `VocalNoteList::AddTambourineGem` 44 | `SessionSearcher::UpdateSearchList` 44 (`0x823eb5d8`; AddTambourineGem kept as a proven folded spelling) |
| `Group::fn_823EB1C8/1E8/208` 32 ×3, `Group::fn_823EB250/278` 40 ×2, `Group::fn_823EB504` 32, `BandUI::fn_823EADC0` 32, `FlowTrigger::fn_823EB8B8` 32, `FlowTrigger::fn_823EB92C` 40 | the identical anonymous rows under `SessionSearcher` / `NetLog` (circular pins removed) |
| `BandCamShot::??1ObjList<BitmapOverride>` 4 | `BandCamShot::??1ObjList<EventCall>` 4 (rename) |

Net **+3,572 B / +35 fns**, no row lost for a substantive reason.

---

## 8. Gates (run last, in the worktree, after the final edit)

- Full build 11: `./tools/ninja-locked` → rc 0 (§10).
- `python3 scripts/verify_ruler_agreement.py --check` → rc 0, "both objdiff-cli entry points
  resolve the same ruler".
- `python3 scripts/verify_objs_patched.py --verify-manifest` → rc 0 (lines in §10).
- `tools/native_build_gate.sh` → NATIVE_GATE_RESULT line pasted verbatim in §10.
- `scripts/test_tools.py`: the three new tests (`test_denylist_applied.py`,
  `test_icf_alias_survivor_gate.py`, `test_fold_thunk_gate_install.py`) all `[ ok]`. **Pre-existing
  red arms** in that run: 21 failures in `scripts/test_patch_state.py` and 1 in
  `scripts/unicorn_runner/tests/test_prober.py::test_format_input_sensitive`. Those files are
  byte-identical to main (`git diff bb9b7e55 --` empty) and the lane touched nothing they import;
  **not verified against a main run** (running tests in the shared main tree is off-limits for this
  lane) — flagged, not explained.

---

## 9. NOT done

- **Item 5 (optional) — the five arbitrary-bijection classes** (`0x823d3918` ×20, `0x823f0b50`,
  `0x8248f1c0`, `0x82787718`, `0x827d5bb0`): not started; no budget after the four mandatory items
  plus the coordinator add-on. W16-AB's "ICF destroyed which name the call site meant" is
  **unverified either way** — the brief's 2-candidate `.xdata`/reloc comparison remains the right
  first test.
- **`0x822b6538`** — a fourth `resize<list<…>>` instantiation with an anonymous insert callee
  (`fn_824CE130`); left at the original (wrong-twin) `BitmapOverride` spelling with the lead
  recorded in `1fdcb69e`. Naming it requires identifying `fn_824CE130` first.
- **The 26 non-refuted carry-path contradictions** were left in place on chase/word-only grounds;
  the exact 8/14/rest split is from pre-compaction notes and should be re-derived by re-running
  the carry path before anyone cites it.
- **Pre-existing red test arms** (§8) not diagnosed.
- **`0x82c16aa0`** denylist entry has no rationale on record — reported, not resolved.

## 10. Final gate lines (filled after the last build)

Build 11 (`~/tmp/rb3_build_w16ae_11.log`), zero-work, rc=0; report.json 43,344 / 3,993,196 B /
38.9732 % (identical to build 10).

```
RULER rc=0
OK: both objdiff-cli entry points resolve the same ruler.
MANIFEST rc=0
[denylist] OK: 6 denylisted address(es), 3 with a live map string, none named in 3116 target objects (495685 symbols scanned)
[patch-state] OK: 1214 decomp, 3116 target objects match 2026-09-14T19:48:29Z (tree_sha256=eb7e26068d14c573)
NATIVE rc=0
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```
