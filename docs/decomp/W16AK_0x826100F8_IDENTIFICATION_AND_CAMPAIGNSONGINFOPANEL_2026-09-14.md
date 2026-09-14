# W16-AK — `default/OvershellPanel`: charged-site census, source work, anonymous-row identification

**Lane:** W16-AK · **Branch:** `w16-ak` · **Base:** `0ba54b71` (= main at dispatch) · **Date:** 2026-09-14
**Worktree:** `/home/free/tmp/wt-w16-ak` · **Ruler:** `functionRelocDiffs=name_check` (graded), read from
`report.json`'s `provenance.diff_config` on every measurement.

> **Filename note.** The brief named this deliverable
> `W16AK_0x826100F8_IDENTIFICATION_AND_CAMPAIGNSONGINFOPANEL_2026-09-14.md`. Neither `0x826100F8` nor
> CampaignSongInfoPanel is in this lane's scope (CampaignSongInfoPanel is W16-AJ's, and was a concurrency
> bar for me). The filename is kept verbatim because the coordinator may be keying on the path; the
> **content** is the OvershellPanel lane it actually describes.

## Headline

| measure | before (`0ba54b71`) | after | delta |
|---|---:|---:|---:|
| whole-binary `matched_functions` | 43,356 | **43,359** | **+3** |
| whole-binary `matched_code` | 3,997,100 | **3,997,784** | **+684 B** |
| whole-binary `matched_code_percent` | 39.011490 | 39.018166 | +0.006676 pp |
| unit `default/OvershellPanel` | 250/272, 18,564 B | **253/272, 19,248 B** | +3 fns / +684 B |

Priced by set-diff of the `fuzzy==100` row set (`tools/rowset_snapshot.py`), not by mismatch count:
**CROSSED IN 3 rows / 684 B · FELL OUT 0 rows / 0 B.** No regression anywhere in the binary.

```
+  332 B  default/OvershellPanel::?ResolveAutoSignInStates@OvershellPanel@@QAAXXZ
+  252 B  default/OvershellPanel::?ResolveChooseProfileStates@OvershellPanel@@QAAXXZ
+  100 B  default/OvershellPanel::?Poll@OvershellPanel@@UAAXXZ
```

## The finding that organises the whole lane

Every defect found here is the same one: **our OvershellPanel carries rb3-Wii DEV-build logic that RB3-360
retail does not have.** It shows up five separate times, and in each case the retail bytes are decisive
while the oracle is actively misleading. The brief's rule — retail bytes outrank the oracle — was load-bearing,
not ceremonial.

The Wii-only material removed: the `unk4cc == 2` guarded `ShowNetError`/`EndOverrideFlow` block (twice), the
whole `TheRnd->mProcCmds & kProcessPost` block in `Poll` (including `ThePlatformMgr.mHomeMenuWii`, a
Wii-named member, and a `static bWasFinding`), and the `kState_AutoSignInNintendo` (0x8B = 139) paths (twice).

## Item 1 — charged-site census of every named sub-100 row

Measured with `run_diff_inspect(mode="mismatches")` on the graded ruler, on a fully built worktree.
Classes per the brief: **(a)** alias-bound to `0x826c3888` (W16-AE's), **(b)** other relocation-name
charges, **(c)** instruction-level source work.

| row | bytes | charges at dispatch | class |
|---|---:|---|---|
| `?Handle@…` | 5,768 | 2 × `diff_arg`, both `OnMsg` overloads | **(b) — but see below, it was NOT a fold** |
| `?ResolveSlotStates@…` | 1,416 | 40, real insert/delete | **(c)** |
| `?ResolvePartWaitStates@…` | 1,356 | 2 × `push_back` fold **+ 1 branch-dest** | (b) + **(c)** |
| `?RefreshJoinableUsers@…` | 852 | 2 × `push_back` fold | (b) |
| `?FinishLoad@…` | 788 | 1 × `reserve` + 1 × `push_back` fold | (b) |
| `?ResolveReadyToPlayStates@…` | 568 | 1 × `push_back` fold | (b) |
| `?FindSlotForRemoteUser@…` | 312 | 1 × `push_back` fold | (b) |
| `?Enter@…` | 204 | 1 → `0x826c3888` | (a) W16-AE |
| `?OnMsg@…RemoteUserLeftMsg@@` | 176 | 1 → `??__FsLoadedFile` | (b) |
| `?Exit@…` | 132 | 1 → `0x826c3888` | (a) W16-AE |
| `?QueueUserToJoin@…` | 124 | 1 × `push_back` fold | (b) |
| `?Poll@…` | 100 | 58, our body 81 instrs vs retail 25 | **(c)** |

`Enter`/`Exit` were verified against the coordinator's finding rather than assumed: both carry exactly one
charge, and it is the `bl` to the empty-function survivor. Confirmed, left alone, W16-AE's.

### ★ The 5,768 B `Handle` charges were NOT a fold — they were a source defect an alias would have hidden

**Predicted:** `Handle`'s two charges are a three-way `OnMsg` fold like the nine already in alias group 65,
so the row is alias-bound and worth 5,768 B to W16-AE.

**Measured:** the adjudicator returned **`CONTRADICTED — size 76 vs 132 | different body SIZE, cannot be one
COMDAT`** for both. Independently confirmed from the COFF: `ConnectionStatusChangedMsg` is retail 76 / ours 76;
every folding sibling (`SigninChangedMsg`, `SessionBusyMsg`, `MatchmakerChangedMsg`, …) is ours-76 with retail
absent (folded away); **only our `ServerStatusChangedMsg` and `NetStartUtilityFinishedMsg` were 132 B.**

The nine siblings are all `{ UpdateAll(); return 1; }`. Our two outliers carried the oracle's `unk4cc == 2`
block, rendered by an earlier port as `InOverrideFlow(kOverrideFlow_RegisterOnline)` and flagged
**UNVERIFIED in its own source comment**. Retail's `Handle` branches to the folded 76-byte survivor at *both*
dispatch sites, which is direct retail-byte evidence that retail's handlers are the bare body.

Installing those two memberships as briefed would have been a **fabricated alias forgiving a live defect** —
precisely the disease behind the 10 recorded withdrawals. After the fix (commit `0cd974cb`) both bodies are
76 B and the verdict flips **`CONTRADICTED` → `L1_T1`**.

**This is why the two rows were invisible:** retail folded both spellings away, so neither is pairable, and
scoring is structurally blind to them. They can never appear as a sub-100 row. The only instrument that could
see the defect was the fold **size** check. (`PAIRABILITY IS A CORRECTNESS INSTRUMENT`, demonstrated.)

Whole-binary delta of that commit: **exactly 0**, as predicted, and it was landed anyway.

## Item 2 — source work

### `?Poll@OvershellPanel@@UAAXXZ` — 0.00 → **100.0** (+100 B)

Retail `fn_825B3230` is 25 instructions: `mr r31,r3; bl UIPanel::Poll; addi r31,r31,0x74; <mSlots loop calling
OvershellSlot::Poll>`. Our body was 81 instructions. An earlier port had dropped the oracle's
`TheWiiFriendMgr` half of the `ProcCmds` block but kept the `inSession`/`TheNetSession`/`Matchmaker` logic,
the `ThePlatformMgr.mHomeMenuWii->mForcedHomeMenu` store and the `static bWasFinding` edge — **and kept the
slot loop inside the guard.** Retail has none of it and the loop is unguarded.

### `?ResolveSlotStates@OvershellPanel@@QAAXXZ` — 93.22 → **98.84**, charges 40 → 6 (0 bytes, see below)

Four independent defects, each read off retail `fn_825B6AD0`:

1. **A missing fifth call.** Retail opens with five calls; we made four. The first three are already mapped
   `ResolvePartWaitStates`, `ResolveReadyToPlayStates`, `ResolveSignInWaitStates` — exactly our first three, in
   order — so position fixes the fourth as `ResolveAutoSignInStates` and leaves a fifth we did not have.
2. An extra `if (!GetState()->InRegisterOnlineFlow())` guard around `SetOverrideFlowReturnState`; retail calls
   it unconditionally in that arm.
3. `static Message msg(hide_connect_controller_mesh, 1)` loaded the interned global Symbol; retail constructs
   it from the string literal (`bl ??0Symbol@@QAA@PBD@Z` into a stack temp).
4. The `kick_user` check was `mSessionMgr->HasUser(pUser)`; **retail calls BandUser's own vftable slot 0,
   `IsInSession(SessionMgr*)`, as `pUser-><slot0>(mSessionMgr)`.** This is the TU5 substitution already
   identified and documented in `src/band3/game/BandUser.h` by lane NCCC-0731-5f08/f76 at
   `InputMgr::IsActiveAndConnected` — **third recorded instance.** It also deletes 8 instructions of
   `BandUser*`→`User*` virtual-base adjust and null check that `HasUser`'s `User*` parameter forced.

★ **The `r18`/`r19` register-allocation difference at idx 16/19/20/26 DISSOLVED when defect 1 was fixed.** It
was a symptom of the missing call's register pressure, never an independent regalloc wall — the documented
trap, reproduced.

**Worth 0 bytes at 98.84%.** `matched_code` is all-or-nothing per row, so this row pays nothing until it
reaches exactly 100. It is landed for correctness (four real behavioural divergences), not for the metric.

## Item 3 — the ten anonymous 0% rows

**Two identified and mapped, both proven on retail bytes by raw byte equality. Eight deliberately left
unnamed.**

### `0x825b2ea0` = `?ResolveAutoSignInStates@OvershellPanel@@QAAXXZ` (+332 B)

Position (4th of the five head calls, first three already mapped to our first three) plus, after removing the
Wii-only `0x8B` arm, **`RAW BYTES EQUAL: True` at 332 == 332 B** against retail `fn_825B2EA0`. Scores
`fuzzy 100.0` once mapped, which additionally means every relocation target name agrees.

### `0x825b2ff0` = `?ResolveChooseProfileStates@OvershellPanel@@QAAXXZ` (+252 B)

The fifth head call — a function with **no oracle counterpart at all** (the Wii build has four `Resolve*`
helpers; retail has five). Reconstructed from the disassembly alone:

```cpp
for each slot: if (GetUser() && GetUser()->IsLocal()) {
    user  = GetUser()->GetLocalBandUser();
    ossID = GetState()->GetStateID();
    if (ossID == kState_ChooseProfile /*0x1f*/ && user->IsSignedIn()) slot->LeaveOptions();
}
```

`LeaveOptions` is `fn_825D8660` from the map. The guard is LocalUser vftable slot `0x10`, fixed as
`IsSignedIn()` because slots `0x8` and `0x14` on that same vfptr are `HasOnlinePrivilege()` and
`IsSignedInOnline()` (used by the byte-proven `ResolveAutoSignInStates`), and
`HasOnlinePrivilege/IsGuest/IsSignedIn/IsSignedInOnline` are declared consecutively in `LocalUser` — exactly
three slots apart. Our compiled body is **`RAW BYTES EQUAL: True` at 252 == 252 B**.

⚠ **The mangled SPELLING is ours, not a recovered Harmonix name** — no oracle name existed to transfer. The
row asserts "retail `0x825b2ff0` holds the function our source calls `ResolveChooseProfileStates`", which is
byte-proven; the name itself is descriptive. Do not read it as ground truth for the original identifier.

Both namings are also *safe*, not merely proven: the sole call site of each is inside `ResolveSlotStates`,
where retail's target was a forgiven placeholder (`fn_*`) and both sides now spell it identically, so no
previously-forgiven site became a charge.

### The other eight — NOT named, on purpose

`fn_825B4AA8` (716) · `fn_825B6188` (492) · `fn_825B50F8` (340) · `fn_825B72E8` (252) · `fn_825B7EA8` (200) ·
`fn_825B7410` (196) · `fn_825B5568` (180) · `fn_825B54B0` (180) — **2,556 B total.**

Every one of our 42 unpaired `OvershellPanel` COMDAT bodies was compared against all eight: **zero exact byte
matches**, and only three same-size candidates (`CanGuitarPlayKeys` at 252, `OnMsg` at 180 ×2) with nothing
corroborating them. Under `name_check` an unnamed callee is already forgiven, so naming is a **bet** whose
only payout is pairing; a wrong name manufactures charges. None of these eight is proven, so none was named.

## Alias proposals for the coordinator

`docs/decomp/W16AK_alias_proposals_2026-09-14.json` — **7 proposals, every one carrying an adjudicator
verdict from `tools/alias_forgiveness_audit.Sides.verdict()`** (the layered L1..L5 adjudicator), run on a
fully built tree with the anti-vacuity gate asserted each time (27,442 mangled names / 69,411 target bodies).

| survivor | folded spelling | verdict | unlocks |
|---|---|---|---|
| `OnMsg(ConnectionStatusChangedMsg)` grp 65 | `OnMsg(ServerStatusChangedMsg)` | **L1_T1** | `Handle` |
| `OnMsg(ConnectionStatusChangedMsg)` grp 65 | `OnMsg(NetStartUtilityFinishedMsg)` | **L1_T1** | `Handle` 5,768 B |
| `push_back<ChatReceiver*>` grp 10 | `push_back<OvershellSlot*>` | L2_RECURSIVE | 852+788+568+312 B |
| `push_back<ChatReceiver*>` grp 10 | `push_back<BandUser*>` | L2_RECURSIVE | *blocked, see below* |
| `push_back<ChatReceiver*>` grp 10 | `push_back<LocalBandUser*>` | L2_RECURSIVE | 124 B |
| `reserve<Dep*>` (**no group yet**) | `reserve<OvershellSlot*>` | L2_RECURSIVE | with the above, `FinishLoad` |
| `??__FsLoadedFile` (**no group yet**) | `BandUserMgr::GetBandUser` | **L3_EXACT** | 176 B |

**Total unlocked if all install: 8,588 B**, none of it collectable by this lane.

⚠ **`ResolvePartWaitStates` (1,356 B) is NOT in that total.** Installing `push_back<BandUser*>` clears two of
its three charges; the third is a genuine **branch-destination** charge (idx 161, `beq` target `0x4424` vs
base `0x1b5c8`) in a function with **zero** insert/delete. The row needs that resolved too.

⚠ The two `L1_T1` OnMsg proposals are valid **only against a tree containing commit `0cd974cb`**. Before it
they adjudicate `CONTRADICTED`.

I did not touch `scripts/symbol_aliases.json` or any `icf_alias_*` file (W16-AE's), and nothing under
CampaignSongInfoPanel (W16-AJ's).

## NOT done, and why

1. **`?ResolveSlotStates@…` left at 98.84% — 1,416 B forgone.** The 6 residual charges are one `ShowState`
   **tail-merge placement** difference: retail keeps the shared `mr r3,r30; bl ShowState` in the then-branch
   (idx 113-114) and jumps *back* to it from the `0x49` path; our compile keeps it in the else-branch (idx
   129-130) and jumps *forward*. Identical instructions, identical count, mirrored merge direction, plus the
   two resulting branch destinations. This is codegen block layout, not semantics — permuter-class, and the
   permuter is off by standing directive. **Anyone reopening this should attack the merge direction, not hunt
   for a semantic difference: there isn't one.**
2. **Eight anonymous rows (2,556 B) left unnamed** — no byte proof, and naming unproven addresses is a bet
   that manufactures charges when wrong. See above for the exact negative result (0 exact matches / 42 bodies).
3. **`ResolvePartWaitStates`' idx-161 branch charge not diagnosed.** The row is alias-bound for 2 of its 3
   charges, so it pays 0 bytes to this lane no matter what I do to the third; I priced it and stopped.
4. **No alias file edited** — concurrency bar, W16-AE owns it. Proposals filed instead.
5. **`?Handle@…` still 99.99%** — its only two charges are the alias memberships above. No source work
   remains on it; the source defect behind it is fixed.
6. **`fn_825B2FF0`'s real Harmonix name not recovered** — the body is byte-proven, the identifier is not
   available from any oracle.

## Gates

Run in the worktree, in the prescribed order, after the last source edit.

| gate | result |
|---|---|
| full `./tools/ninja-locked` | **rc=0** |
| `scripts/verify_ruler_agreement.py --check` | **rc=0** — "OK: both objdiff-cli entry points resolve the same ruler." |
| `scripts/verify_objs_patched.py --verify-manifest` | **rc=0** — `OK: 1213 decomp, 3115 target objects match` (`tree_sha256=97200affc7f542ae`) |
| `tools/native_build_gate.sh` | **rc=0**, verbatim below |

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

`skipped=0`, so this is full coverage, not the INCOMPLETE variant.

## Commits on `w16-ak`

| sha | what |
|---|---|
| `0cd974cb` | two `OnMsg` handlers carried Wii-dev-only bodies retail does not have (Δ0, flips 2 alias memberships `CONTRADICTED`→`L1_T1`) |
| `76bf1d6a` | `Poll`: retail has none of the Wii `ProcCmds` block — 0% → 100% (+1 fn / +100 B) |
| `3260d287` | drop the Wii-only `kState_AutoSignInNintendo` (0x8B) paths (Δ0; makes `ResolveAutoSignInStates` byte-identical to retail) |
| `25317366` | map `0x825b2ea0` = `ResolveAutoSignInStates`, proven byte-identical (+1 fn / +332 B) |
| `8464433f` | `ResolveSlotStates` 93.22 → 98.84, four retail-byte divergences (Δ0 — all-or-nothing row) |
| *(this)* | map `0x825b2ff0` = `ResolveChooseProfileStates` (+1 fn / +252 B), proposals + this document |

Three of the six commits measured **Δ0 on purpose**: two fix defects in rows retail folded away (structurally
unpairable, so scoring cannot see them) and one moves a row that pays nothing below 100. They are landed on
merit, per the standing "accuracy beats headline %" directive.
