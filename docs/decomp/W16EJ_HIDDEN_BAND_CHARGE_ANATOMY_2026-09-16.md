# W16-EJ — the band the `.1f` display was hiding: what those 22 rows actually are

Lane W16-EJ, 2026-09-16, worktree `~/tmp/wt-w16-ej`, branch `w16-ej`, off `ec15a785`.
Ruler: shipped graded `name_check` (objdiff 4.2.9, `tool_commit a5f0ea903ec1`), read from
`build/45410914/report.json`, whose `provenance.diff_config` was checked rather than assumed.

## Result in one line

**+1 function / +572 B / +0.005581 pp**, from ONE row. The other 21 rows are adjudicated,
and 20 of them are **NOT reachable by source work** — the reason is below, and it is not the
reason the brief predicted.

## 1. The population is real; the brief's rationale for it is REFUTED

The band exists: at `ec15a785`, game-layer (`band3/` + `network/`) rows with
`99.95 <= fuzzy < 100` are **22 rows / 17,052 B** (whole binary: 147 rows / 118,340 B).
Every figure in the brief's table reproduced EXACTLY against `report.json`. Good brief.

The brief's *reasoning* did not survive:

> "ALL 22 have `match_percent_normalized < 100` — i.e. every one carries instruction-level
> penalties reachable by source work. Zero are in the arg-only (relocation-name) class."

**Both halves are false, and the population proves it.** Of 26 charged sites across the 22
rows, **24 are relocation-name charges and only 2 are instruction-level** (both immediates).
`mpn < 100` did not mean what the brief needed it to mean.

**Mechanism** (this is the durable part). objdiff-core `b14ba45` (2026-08-20), *"NameCheck:
let a vetted wrong-callee reach `match_percent_normalized`"*, added `vetted_reloc_name_diff`
to `diff/code.rs`. Under `name_check` ONLY, a relocation-name diff that passes three screens
(not a regalloc save helper, not a placeholder name, not a local-static-ordinal diff) is
**excluded from `arg_diff_score`** — so it no longer cancels out of
`mpn = diff_score - arg_diff_score` and lands squarely in `mpn`. CLAUDE.md already records
this (lane W16-AR, 2026-09-14) as the reason `matched_functions` stopped being
ruler-invariant; its consequence for *screening* had not been drawn.

⇒ **On the shipped ruler, `mpn < 100` certifies NOTHING about source-reachability.**
The tell was visible for free and nobody looked: **`mpn == fuzzy` to the digit on all 22
rows**, i.e. `arg_diff_score == 0`. That equality does not mean "no relocation charges" — it
means every relocation charge present was *vetted* and therefore promoted into `mpn`.

★ The only surviving valid direction: `mpn == 100` ⇒ all charges are arg-only ⇒ not
source-reachable. The converse is dead. **Enumerate the charges. There is no percentage
screen that substitutes for it.**

## 2. A charge-counting instrument you can use without running objdiff

`report.json` alone gives the exact charge weight, because objdiff's penalties are integers
(`diff/code.rs`): `PENALTY_IMM_DIFF=1`, `PENALTY_REG_DIFF=5`, `PENALTY_REPLACE=60`,
`PENALTY_INSERT_DELETE=100`, and `max_score = n_instructions * 100`. Therefore

```
diff_score = (100 - fuzzy_match_percent) * size_bytes / 4
```

Validated against the tool on two rows to 5 decimals: PrefabMgr `1 - 5/10300 = 99.95146` vs
report `99.95145`; UGCPurchasePanel `1 - 1/27600 = 99.996377` vs report `99.99638`.

All 22 rows came out at `diff_score` ∈ **{1, 5, 10, 15}** — i.e. 1 immediate, or 1/2/3
non-immediate arg diffs. **A score of 1 is one immediate; 5 is one register-or-relocation
arg.** No row carried a `Replace` (60) or an insert/delete (100).

⚠ Corollary that kills a tempting shortcut: "bytes per charge" ranks identically to "fuzzy
descending", because `diff_score` is already size-normalised. The ranking that matters is by
charge **KIND**, which only enumeration gives you.

## 3. What the 26 charges are

| class | charges | rows | disposition |
|---|---:|---:|---|
| immediate (struct size) | 2 | 2 | deferred, evidence below |
| reloc-name, our source genuinely WRONG | 1 | 1 | **FIXED, crossed** |
| reloc-name, ICF fold (chase-proven) | 17 | — | alias-gated, NOT landed (§5) |
| reloc-name, fold REFUTED on retail bytes | 6 | — | candidate real defects (§6) |

## 4. The one crossing: VocalGuidePitch (+572 B), commit `403649a3`

Our object emitted `bl NoteAt__13VocalNoteListCFf` — a **Metrowerks (Wii) mangled name** —
against retail's `bl ?NoteAt@VocalNoteList@@QBAPBVVocalNote@@M@Z`. The `extern "C"` shim at
`VocalGuidePitch.cpp:15` was legacy cruft: `VocalNoteList::NoteAt` is declared
(`beatmatch/VocalNote.h:119`), defined (`beatmatch/VocalNoteList.cpp:569`) and already called
as a plain member at `game/VocalPlayer.cpp:526`. Same ABI, so only the symbol moves.

Priced with `tools/ab_measure.py --revert 403649a3`, both legs settled, tree restored:

```
leg A (with fix) matched=43957  code%=40.248314   leg B (reverted) matched=43956  code%=40.242733
Δmatched=-1  Δcode_bytes=-572  Δcode%=-0.005581pp   (i.e. the fix is worth +1 / +572 B)
unit regressions on revert: 1 (default/band3/game/VocalGuidePitch 14->13)
```

`native/src/m10_support.cpp:38` still DEFINES the MWCC name and `game/VocalPart.cpp` still
calls it, so nothing was orphaned. **VocalPart.cpp deliberately untouched** (outside the band).

⚠ My own pre-A/B hand-read was UNSETTLED — this worktree's first build ran all 397 edges,
exactly the documented ~+193/+0.51 pp settling hazard. The hand-read happened to agree with
the tool (+572 landed precisely on the row size, `matched_code` being all-or-nothing per
row), but it was not entitled to. Use the tool.

## 5. The force multiplier that ISN'T: 17 chase-proven folds, deliberately NOT landed

18 rows charge on STL container-type relocation names, and **five separate rows charge against
the same target symbol** `?push_back@?$vector@PAVChatReceiver@@...` (InterstitialMgr,
Gem::AddRep, PrefabMgr, TourDesc, NetSession::OnMsg). One symbol serving as five different
pointer-vectors' target is the signature of an ICF fold survivor, so the obvious play was to
extend the existing proven group at `0x82b5f808` in `scripts/symbol_aliases.json`.

Adjudicated with `tools/icf_pair_adjudicate.py` (the project's own T1 magnifying glass;
`--selftest` and `--chasetest` were BOTH run first and BOTH discriminate — positive control
PROVEN, negative/decoy REFUTED):

* **Flat T1: 21 REFUTED / 2 UNDECIDABLE, 0 proven.** Dominant reason: *"masked bodies match
  but relocation TARGETS disagree — template-twin, not a fold."* MSVC folds only COMDATs
  identical **including relocations**; `vector<PrefabChar*>::push_back` and
  `vector<ChatReceiver*>::push_back` have identical machine bytes but call different per-`T`
  helpers. Same mechanism CLAUDE.md records for `_List_base<T>::clear` (42 addresses,
  reloc-identical surplus 0).
* **`--chase` (recursive T1, relaxing reloc-target NAME equality to a recursively verified
  fold of that slot's callee pair): 17 PROVEN / 6 REFUTED.**

Sized: landing all 17 would cross **13 further rows / 10,136 B** (14 rows / 10,708 B counting
VocalGuidePitch, already banked).

**I did not land them, and this is a deliberate integrity call, not an omission:**

1. `--chase` is **not one of the declared T1/T2/T3 evidence tiers** that
   `scripts/symbol_aliases.json` documents and that the batch generator
   `tools/icf_alias_build.py` implements. Landing chase-derived memberships silently
   introduces a fourth, weaker tier into a file whose entire value is that its tiers are
   stated and auditable.
2. The generated `build/45410914/icf_aliases.map` says *"DO NOT EDIT BY HAND. Source of
   truth: scripts/symbol_aliases.json"*, and the JSON's own pipeline cannot re-derive a chase
   membership — `icf_alias_build.py` does not pass `--chase`. That makes them
   **unreproducible**, the exact defect `tools/icf_site_census.py`'s docstring exists to
   prevent ("a generator you cannot re-run is a number you cannot re-derive").
3. An alias is *forgiveness*: it lifts the score **by construction**, and CLAUDE.md is
   explicit that the `none`-ruler control **cannot** catch a fabricated one. ~7.9 pp of
   `matched_code` already rests on this mechanism (ALIAS-2, 2026-08-16). Expanding it by
   ~10 kB is a campaign decision.
4. Flat T1's refusal is *informative*, not noise: it says the disagreement is in the
   **callees**. The honest fix order is to prove and land the callee folds first, after which
   flat T1 passes these parents on its own evidence.

**Recommendation for the coordinator (sized, not asserted):** decide whether `--chase`
becomes a declared tier in `icf_alias_build.py`. If yes, the 17 pairs are in
`~/tmp/w16ej/chase.txt` and are worth 13 rows / 10,136 B in this band alone. Do not hand-edit
them in.

## 6. The 6 fold-REFUTED pairs are the real prize for the next lane

These are NOT folds on retail bytes, so our callee genuinely differs from retail's — which is
either a real defect (the most valuable class we have, per MPNGAP-1) or a map-naming defect.

| bytes | unit | retail calls | we call | adjudication |
|---:|---|---|---|---|
| 588 | meta_band/AccomplishmentProgress | `hash_map<Symbol,int>::hash_map` | `hash_map<int,int>::hash_map` | **container-type defect, top candidate** |
| 460 | network/net/NetSession | `ProcessedJoinRequestMsg(bool)` | `SessionReadyMsg(int)` | bodies differ — wrong message class |
| 656 | band3/game/TrainerGemTab | `_M_erase<vector<RndLine::Point>>` (96 B) | `_M_erase<vector<TrainerGemTab::ExtraTail>>` (88 B) | bodies differ ⇒ **element size wrong** |
| 1392 | band3/tour/TourChallengeResultsPanel | `__copy_ptrs<const int*,int*>` | `Tour::GetQuest` | bodies differ — structural |
| 836 | meta_band/StoreMenuPanel | `MetadataLoadedMsg::Type` | `MultipleItemsEnumCompleteMsg::Type` | **VACUOUS** (body < 4 words) — undecidable, do not treat the REFUTED as evidence |
| 1220 | band3/game/RGTrainerPanel | `RndGroup::Draw` | `FretHand::GetFinger` | "our spelling is in no compiled obj" — inspect first |

★ Top candidate detail, ready to pick up: the AccomplishmentProgress charge is
`addi r3, r30, 0x64c` immediately before the ctor call, and **0x64c is `mGigTypeCompletedMap`**
(`AccomplishmentProgress.h:284`), declared `std::hash_map<int, int>` where retail constructs
`hash_map<Symbol, int>`. NOT a one-liner: the member is exposed via
`GetGigTypeCompletedMap()` and the header carries a deliberate comment about these maps, so
the key-type change is cross-cutting. It is one charge on a 588 B row — it crosses if closed.

## 7. The 2 immediate rows, both struct sizes, both deferred with evidence

**`?Poll@UGCPurchasePanel@@UAAXXZ` — 1104 B, `diff_score` 1, the single best
bytes-per-charge row in the band.** Target `li r3, 0x28`, ours `li r3, 0x50`, immediately
before `operator new` and an inlined ctor at `bl fn_827B2800`; the argument setup
(`this, padnum, offerID, 0, 0, Symbol, flags`) matches our `XboxPurchaser` ctor exactly.
⇒ **retail `sizeof(XboxPurchaser) == 0x28` (40 B); ours is 0x50 (80 B).**
Independent control: `build/45410914/asm/band3/meta_band/StandIn.s:630` emits `li r3, 0x28`
before its own `bl fn_827B2800`. Two call sites agree.
Compiler layout (`scripts/harvest/class_layout_report.py XboxPurchaser`, authoritative) shows
`Hmx::Object` alone contributes 40 B (0xc..0x34) — so **retail's 40-byte object cannot
contain a full `Hmx::Object` base at all.** That is an engine base-class question in
`src/system/meta/StorePurchaser.h`, blast radius ≥5 units (StorePanel, StandIn,
TokenRedemptionPanel, MusicLibraryStore, SetlistToStorePanel). **Deferred: too wide to land
and price inside this lane.** Whoever takes it: it is probably worth far more than 1104 B,
since every one of those call sites currently allocates the wrong size.

**`?clear@?$_List_base@VSetlistArtRecord@MusicLibraryNetSetlists@@...` — 88 B,
`diff_score` 1.** Target `li r3, 0x4c` vs ours `li r3, 0x10` into
`MemOrPoolFreeSTL(size, ptr)` ⇒ retail frees 76-byte list nodes, ours 16-byte ⇒ retail's
`SetlistArtRecord` is 68 B; ours is 8 B (`Symbol unk0; RndTex *unk4;`).
**The rb3-Wii oracle AGREES WITH US and disagrees with retail** (same two members; its
`mSetlists` sits at 0x58 vs our 0x6c) — so there is no oracle support for a 68-byte layout,
and padding to size would be inventing 60 bytes of unknown members. **Dropped:** 88 B is not
worth a speculative layout, and a wrong guess would perturb every other function that touches
`mSetlists`. Flagged for whoever identifies the record properly.

## 8. What I did NOT do

* Did not land the 17 chase-proven aliases (§5) — reasons given; this is the single biggest
  uncollected item in the band.
* Did not attempt `XboxPurchaser` (§7) or `SetlistArtRecord` (§7).
* Did not attempt any of the 6 fold-refuted candidate defects (§6), including
  AccomplishmentProgress.
* Did not touch `src/band3/game/VocalPart.cpp`, which carries the *same* MWCC `extern "C"`
  shim at line 630 and would likely benefit from the same repair — it is outside this band
  and unmeasured here.
* Did not re-run `tools/icf_site_census.py`. Note for whoever does: the artifacts the alias
  pipeline defaults to (`~/tmp/cd9_allsites.json`, `~/tmp/cd9_evidence.json`) are dated
  **2026-07-31**, six weeks stale, which is the most likely reason these spellings were never
  proposed (rather than refuted) when `symbol_aliases.json` was last generated.

## 9. Method notes worth keeping

* `objdiff-cli diff` WITHOUT `--build` on a fully built tree reproduces `report.json`'s
  `fuzzy_match_percent` exactly (both now read `objdiff.json`'s `options` block). Never
  `ninja <one>.obj`, never `--build`.
* Drive objdiff from Python with an argv list, not a shell string: these mangled names are
  full of `$`, `?`, `@` and zsh modifier/expansion rules have burned lanes here.
* My liveness check `pgrep -f "ab_measure.py"` reported RUNNING *after* the tool had printed
  its result and restored the tree — the self-matching-pattern trap, since the pattern is in
  the checking shell's own argv. `pgrep -af "[a]b_measure.py"` is correct.
