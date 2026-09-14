# W16-L — PlatformMgr bodies, Server's surplus virtuals, three wrong-callee bugs

Lane W16-L, 2026-09-14, branch `w16-l`, worktree `~/tmp/wt-w16-l`.
Follows W16-G (`VTABLE_SLOT_DIVERGENCES_2026-09-14.md`, class SHAPE) and W16-E
(`PLATFORMMGR_FOLLOWUPS_2026-09-14.md`). W16-G fixed the shapes and left the
bodies; this lane took the bodies, and the largest single win turned out not to
be a body at all.

**Whole-binary, pre-lane -> post-lane** (all from `build/45410914/report.json`,
ruler `functionRelocDiffs=name_check`, full builds):

| measure | pre | post | delta |
|---|---:|---:|---:|
| `matched_functions` | 43,174 | **43,183** | **+9** |
| `matched_code` | 3,929,984 | **3,934,228** | **+4,244 B** |
| `matched_code_percent` | 38.35644 | 38.39786 | +0.04142 pp |
| `fuzzy_match_percent` | 49.31390 | 49.32195 | +0.00805 pp |
| `total_code` | 10,245,956 | 10,245,956 | 0 |
| `total_functions` | 69,217 | 69,217 | 0 |

Every step below was pre-registered before its build and **every prediction was
exact**. `total_code` and `total_functions` never moved, so none of the gain is
denominator motion.

---

## 1. `StoreInfoPanel::GetRecommendationIndexPath` — TWO bugs, not one

Retail `fn_82638D58` does **not** call `Server::GetMasterProfileID()`. It
dispatches Server slot 7 (`lwz r11, 0x1c(...)`) with an argument in r4 produced
by `StorePanel::Instance()` -> slot 17 (`StoreUser`) -> slot 0 (`GetPadNum`).
That is `GetPlayerID(padnum)`.

The row was **anonymous in the map (fuzzy 0)**, so the source fix alone was
structurally invisible — the pairability-as-correctness-instrument trap. Our obj
defines the symbol as a real COMDAT (sec=386, class=2), so it was named.
Row 0.0 -> 99.83, whole binary Delta 0/0 (naming pays through pairing, not bytes).

Then a **second** bug in the same row: retail calls
`?SystemLocale@@YA?AVSymbol@@XZ` (0x82510040), not `SystemLanguage`, and passes
it as `const char *` (`??$MakeString@VSymbol@@PBDPBD@@`), not as a `Symbol`. An
ICF fold was ruled out — `?SystemLanguage@@` is at **no** address in the map and
in **no** alias group. `SystemLocale().Str()` closed both: row **100.0, +236 B**.

## 2. PlatformMgr sub-100 rows

Three of W16-G's four cheap rows crossed, and the biggest prize in the cluster
turned out to be a map defect rather than a body:

| row | size | before | after | delta |
|---|---:|---:|---:|---|
| `?ThreadStart@PlatformMgr@@` | 124 | 86.5 | 100 | +1 fn / +124 B |
| `?EnumerateFriends@PlatformMgr@@` | 356 | 97.75 | 100 | +1 fn / +356 B |
| `?ThreadDone@PlatformMgr@@` | 56 | 95.7 | mpn 100 | +1 fn / **+0 B** |
| `?Handle@PlatformMgr@@` | 3,112 | 99.9807 | **100** | +1 fn / **+3,112 B** |
| `?EnableXMP@PlatformMgr@@` | 4 | 0 (mis-named) | **100** | +1 fn / +4 B |

- **ThreadStart**: hoist `BOOL result = 0;` above the `for`.
- **ThreadDone**: `int rerun` -> `unsigned int rerun` (the `cmplwi`).
- **EnumerateFriends**: materialise `unsigned long res = XFriendsCreateEnumerator(...)`
  before `bool failed`, and test `res != ERROR_SUCCESS`.

**`ThreadDone` STOPPED as permuter-class.** After its one real charge closed, the
whole residual is a pure r10<->r11 transposition across 10 instructions with no
opcode, immediate, structural or liveness difference. Permuter is OFF by
directive; the row is at mpn 100 and will never reach fuzzy 100 without it.

### `?Handle@PlatformMgr@@` — 3,112 B behind three map defects

Priced from `report.json`'s charged-site list (not a mismatch count): exactly
**three** charges, all `diff_arg` relocation names.

1. **`0x8251c170` carried a WRONG map name.** It was
   `??1?$ObjPtr@VMoggClip@@@@UAA@XZ`. Its retail body is one word, `48550f48` =
   `b 0x82A6D0B8`, and `0x82A6D0B8` is `XMPRestoreBackgroundMusic`. A virtual
   destructor for `ObjPtr<MoggClip>` cannot tail-branch into an XDK audio API,
   and the address is inside PlatformMgr_Xbox's own `.text`. It is
   `PlatformMgr::EnableXMP() { XMPRestoreBackgroundMusic(); }`. Confirmed
   non-vacuously from our COFF: `?EnableXMP@PlatformMgr@@QAAXXZ` is a 4-byte
   COMDAT holding `48000000` with **one relocation** — the same `b`, unresolved.
   Renaming was safe on both hazards CLAUDE.md names: our base obj defines the
   new name, and the old name has **zero** references anywhere in the asm tree.
2. **The other two charges target `0x826c3888`**, the documented bare-`blr` ICF
   survivor, whose alias group already carries `??3@YAXPAX0@Z` and
   `?Copy@BandTrack@@` at tier FT-EMPTY. `?CheckMailbox@PlatformMgr@@QAAXXZ` and
   `?RunNetStartUtility@PlatformMgr@@QAAXXZ` are `{}` and compile to 4 bytes of
   `4e800020` with **zero** relocations — byte-identical to the survivor.

   The byte comparison is vacuous in the usual FT-EMPTY way, but this pair has
   evidence the group's existing members lack: **the intended spelling is
   recoverable from the call site.** Retail's `Handle` charges these two names in
   the run of DataArray action dispatches adjacent to `enable_xmp`, i.e.
   `HANDLE_ACTION(check_mailbox, ...)` / `HANDLE_ACTION(run_net_start_utility, ...)`
   in `PlatformMgr.cpp`. And retail's callee *being* a bare `blr` independently
   proves retail's own bodies are empty, so the alias forgives nothing real — it
   is not the lift-by-construction hazard.

### STOPPED: `?Poll@PlatformMgr@@` (1,844 B, 99.245) — uncollectable as it stands

12 charged sites. Six of them are one source construct: retail hoists
`mFriendsBuffer + 8` into an induction register and reads the `XONLINE_FRIEND`
fields at `-0x8` (xuid, `ld`), `0x0`/`mr r4` (szGamertag) and `0x10`
(dwFriendState), where we keep `xf` itself and pay an `addi r4, r29, 0x8` per
iteration. That is a compiler choice of strength-reduction base, i.e. the
regalloc/schedule class.

**But the row is uncollectable even if that lands**, and this is the RESIDUAL-1
rule, not a guess: two of the remaining charges are relocation-name folds —
`??2Friend@@SAPAXI@Z` vs `??2CriticalSection@@SAPAXI@Z` (class-scoped
`operator new`), and `push_back<Friend*>` vs `push_back<ChatReceiver*>`. Because
`matched_code` keys on `fuzzy == 100` and is all-or-nothing per row, the 1,844 B
requires **both folds proven** as well. Sized follow-up, not a tail.

Left untouched and priced: `??0PlatformMgr` 364 B @ 68.84, `?UpdateSigninState@`
340 B @ 75.99, `??0ProfileSwappedMsg@` 164 B @ 0 (body not written),
`fn_8251D378` `PlatformMgr::Init` 220 B @ 0 (two callees still unidentified).

## 3. Server's two surplus tail virtuals — **NOT PROVABLE**, and the reason is structural

We declare 21 primary-vtable slots, retail declares 19. The compiler
(`class_layout_report.py`) gives the authoritative table — **21 real slots**, the
23 it prints including 2 vbtable rows (the W16-G correction). `IsConnected` is
`[5]`/0x14 and `GetPlayerID` is `[7]`/0x1c, exactly the offsets retail
dispatches, so the head of the table is confirmed by the metric.

The walk does **not** identify which two of
`{GetSecureConnectionClient, GetAccountManagementClient, GetMasterProfileID,
CreateProfile, DeleteProfile, GetCustomAuthData}` retail lacks. Three independent
reasons, each of which alone is sufficient:

1. **Retail's `Server` vtable slots [9..18] are all the SAME ICF-folded body**
   (`li r3,0; blr`, the survivor at `0x823591e8`, independently reproduced). The
   table therefore carries **zero per-slot information beyond its length**.
2. **No call site in the binary dispatches any tail slot.** A whole-asm census of
   every Server dispatch — both `TheServer` (`extern Server &`, the pointer load
   from `lbl_82C6EB50`) and `TheNet.GetServer()` (`lbl_82CBFAB8 + 0x34`) — finds
   **40 sites whose maximum vtable offset is 0x38 = slot 14**
   (`GetCompetitionClient`, in `RockCentral::OnMsg(ServerStatusChangedMsg)`).
   **Not one site** dispatches 0x3c, 0x40, 0x44, 0x48, 0x4c or 0x50. So nothing
   in the image can name any of retail's four tail slots [15..18].
3. `XboxServer` overrides exactly **one** tail slot, at `fn_823EC788` =
   `lwz r3, 0x7c(r3); b fn_82A89FF8`, where `fn_82A89FF8` is `/Od` Quazal code
   returning `x ? *(x+0x1c) : 0`. That body is consistent with several of the six
   names; it fixes an **index**, not an identity.

One genuine narrowing, recorded but **not acted on**: of our twelve tail virtuals
[9..20], eleven have body `return 0` and exactly one — `GetCustomAuthData` —
returns `&emptyDataHolder`. Retail has exactly ten slots and they are all the
`return 0` fold. So `GetCustomAuthData` cannot occupy any retail slot [9..18]
*unless our body is wrong*, and our body is oracle-derived, not retail-derived.
That narrows to one-of-eleven for the second removal; it does not close it.

**Nothing was removed.** A header change here cascades and silently shifts every
slot index at or above the removal point, and the payoff is **zero by
construction**: `??_7Server@@6B@` is not scored at all — `default/Server` already
reads `matched_data_percent: 100.0` / `complete_data_percent: 100.0` with no
`.rdata` pinned. A guess would buy nothing and could cost correctness.

**Reusable escalation note:** the only lever left is identifying `fn_82A89FF8`
(and its sibling `fn_82A89F38`) inside the Quazal `/Od` band well enough to name
`XboxServer`'s single tail override. Everything keyed on the *vtable* or on
*call sites* is drained, and that is a property of the image, not of effort.

### Side-finding that came out of item 3: two MORE instances of item 1's bug

Chasing item 3's call sites surfaced the same wrong callee twice more, in
`TokenRedemptionPanel`. Retail `fn_8263FAA0` (`GetOffersForToken`) and
`fn_8263F948` (`GetPreviousOffersForUser`) both run the identical shape:

```
lwz  r3, lbl_82C6EB50@l(rN)                       ; TheServer (reference => ptr load)
lwz  r11,0x0(r3); lwz r11,0x14(r11); bctrl        ; slot 5  IsConnected()
clrlwi. r11,r3,24 ; beq <skip>                    ; id stays 0 when offline
lwz  r10,0x4(user); lwz r10,0xc(r10); add r11,r10,user
addi r3,r11,0x4 ; lwz r11,0x4(r11); lwz r11,0x0(r11); bctrl   ; slot 0 GetPadNum()
lwz  r11,0x1c(rS) ; mr r4,r3 ; r3 = TheServer ; bctrl          ; slot 7 GetPlayerID
```

Our source called `TheServer.GetMasterProfileID()` at both.
`ShowPurchaseUIForOffer` (`fn_8263FB98`) already carried the right shape from an
earlier lane. Both target rows were **anonymous**, so — as in item 1 — the source
fix alone would have been invisible; our obj defines both as class=2 COMDATs, so
they were named. **+2 fns / +396 B (212 + 184), predicted exactly.**

This is also why item 3's `GetMasterProfileID` question matters: after this lane,
**our tree contains zero calls to any of the six tail virtuals**, and retail
contains zero dispatches of any tail slot. Every site we previously believed was
`GetMasterProfileID` was in fact `GetPlayerID(padnum)`.

### And the RedemptionState enum is wrong in the oracle

The rb3-Wii oracle's `RedemptionState` is `0,2,3,5,6,7,8`. Retail's is
**contiguous**. Witnessed stores/compares at `this+0x40`:

| retail fn | our name | value |
|---|---|---|
| `fn_8263FAA0` | `GetOffersForToken` | stw **1** |
| `fn_82640288` | `OnMsg(RockCentralOpCompleteMsg)` | stw **2**, stw **4**, cmpwi **3** |
| `fn_8263F948` | `GetPreviousOffersForUser` | stw **3** |
| `fn_8263FEB0` | `EnumerateOffers` | cmpwi **4** |
| `fn_8263FB98` | `ShowPurchaseUIForOffer` | stw **5** |

The Wii source is also internally inconsistent here: its `ShowPurchaseUIForOffer`
sets `kReportingPurchase`(8) while its `Poll` only ever services
`kPurchasing`(7) — it writes a state `Poll` never handles. Retail collapses that:
`ShowPurchaseUIForOffer` writes the value `Poll`'s purchase case reads.
`kReportingPurchase` is the one **unwitnessed** enumerator — no site emits it —
so 6 is labelled as the contiguous continuation, not claimed as a measurement.

## 4. UILabelDir `$4` thunk block — the brief's gap description was stale

`0x82812068` (`PostLoad`) and `0x82812080` (the dtor) are **already pinned** on
main. The only hole left was `fn_82812028`.

**Answering the brief's question on retail bytes: `fn_82812028` IS one 16-byte
thunk, not two mis-carved 8-byte ones.** `0x82812018`..`0x82812074` is six
virtual-base adjustor thunks on a 16-byte stride, each 12 B plus a 4-byte zero
pad. `fn_82812028` alone is four instructions because it carries a second fixed
this-adjustment:

```
lwz  r11, -4(r3)      ; vbptr displacement
subf r3, r11, r3
addi r3, r3, -692     ; <-- the extra word, -0x2B4 to the ObjectDir subobject
b    ?DataDir@ObjectDir@@UAAPAV1@XZ
```

And `0x82812078` is **not code**: it is the 8-byte EH prefix (a `.text` pointer
plus an `.rdata` pointer) of `??1UILabelDir@@` at `0x82812080`.

The thunk mangles on `ObjectDir`, not the derived class, and MSVC encodes both
displacements in A–P hex: `PPPPPPPM` = `0xFFFFFFFC` = -4, and the second field is
the fixed adjust (cf. `TrackPanelDirBase`'s `CBE` = 0x214 = 532). -692 = 0x2B4 =>
`CLE`, so the retail symbol is `?DataDir@ObjectDir@@$4PPPPPPPM@CLE@AAPAV1@XZ`.
That name was **derived from the displacement first** and only then confirmed
against our COFF symbol table, where `UILabelDir.obj` defines it as a real COMDAT
(class=2). Pinned + named: **+1 fn / +16 B, predicted exactly**, with `total_code`
and `total_functions` flat (pure reattribution plus one new pairing).

W16-G's "+10 fns from this pin" did **not** reproduce — because most of that pin
had already landed; only one function was left unattributed.

---

## Not done, and why

- **Two surplus `Server` virtuals not removed** — see item 3; unprovable from the
  image, and zero metric payoff either way.
- **`?Poll@PlatformMgr@@` (1,844 B) stopped** — the collectable prize requires two
  unproven relocation-name folds *in addition to* a regalloc-class source change.
- **`??0PlatformMgr`, `?UpdateSigninState@`, `??0ProfileSwappedMsg@`,
  `fn_8251D378`** — not opened; priced above.
- **Permuter not used** (OFF by directive). `?ThreadDone@PlatformMgr@@` is the one
  row explicitly parked in that class.
