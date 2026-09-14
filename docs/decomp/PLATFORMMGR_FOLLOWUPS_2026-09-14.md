# PlatformMgr follow-ups: two crossings, one re-home, and a name that is provably wrong

**Lane W16-E, 2026-09-14.** Branch `w16-e` off `main` `a427ff73`.
Worktree `~/tmp/wt-w16-e`. Executes the four handoffs of W15-F
(`docs/decomp/PLATFORMMGR_REGION_2026-09-14.md` §3, §5, §6, §8).

> **VERDICT SUMMARY**
>
> | | |
> |---|---|
> | measured total | **+3 matched functions / +684 B / +0.006674 pp code% / +0.004632 pp fuzzy** (3 settled A/Bs) |
> | `GetName` | 88.929 -> **100.000** (+168 B). Retail uses the **2-arg** `Localize`. |
> | `ShowGamercard` | 35.805 -> **100.000** (+164 B). Retail has **no NUI branch** and validates **before** `GetPadNum`. |
> | item 3 (`DingoSvr` block) | **NOT MOVED — and the premise is refuted.** Both our DingoSvr objs emit the ctor; more importantly the row is **not a `ServerStatusChangedMsg` ctor at all.** |
> | item 4 (COMDAT table) | 11 COMDAT-hosting blocks tested mechanically. **Every one stays.** |
> | ★ unlooked-for gain | The mechanical sweep found the region's only genuine mis-pin, and it is **not** a Msg block: two `vector<u64>` COMDATs, **+352 B**. |
> | `total_code` | **10,245,956 on every leg of every run — unchanged.** None of the gain is bookkeeping. |
> | ⛔ judged uncollectable | `??0ServerStatusChangedMsg@@` (132 B) — retail-byte reason in §5. |

Baseline at `a427ff73`, ruler **`name_check`** read from `report.json`'s
`provenance.diff_config` (not assumed), after a full `./tools/ninja-locked`:
**43,108 matched / 3,920,752 B / 38.266340% / fuzzy 49.232150 /
total_code 10,245,956 / total_functions 69,219 / masked_equal 22,999**,
`verify_objs_patched --verify-manifest` OK (1,206 decomp + 3,084 target objects).

---

## 1. `GetName` — the 2-arg `Localize` (+168 B)

W15-F ported the body from DC3, which calls the 3-arg
`Localize(Symbol, bool*, Locale&)`. `src/system/utl/Locale.h` already carried a
VERIFIED note that RB3 call sites use the 2-arg form. Retail settles it outright —
`fn_8251CA58`, at `.L_8251CAD8`:

```
li   r4, 0x0        <- bool* success = 0
lwz  r3, 0x0(r30)   <- the static Symbol
bl   fn_827C9750    <- Localize
```

**Only r3 and r4 are set; there is no r5**, so no `&TheLocale`. Corroborated
independently from `target_symbol_map.json`:
`0x827c9750 = ?Localize@@YAPBDVSymbol@@PA_N@Z`.

All **six** charged sites were this one construct (3 insert / 1 replace / 1 delete,
plus a `diff_arg` whose target callee is the 2-arg symbol). The target-side callee
is a **named** symbol, so that `diff_arg` was a genuine wrong-callee charge, not an
ICF fold-alias.

**PRE-REGISTERED:** crosses +1 fn / +168 B / 1 unit · or pairs-but-arg-only
+1 fn / +0 B · or 0-0.
**MEASURED** (`ab_measure --from-dirty`, both legs settled, full build, `name_check`):

```
leg A: matched=43108 code%=38.266340   leg B: matched=43109 code%=38.267975
Δmatched=+1  Δcode_bytes=+168  Δcode%=+0.001635pp  Δfuzzy=+0.000184pp
unit net (ALL units) = +1 ;  default/PlatformMgr_Xbox 14 -> 15
```

Outcome 1, exact on every pre-registered figure. Commit `05cc287c`.

---

## 2. `ShowGamercard` — no NUI branch, and validity precedes `GetPadNum` (+164 B)

fuzzy 35.805 -> **100.000** on the first attempt. Two structural divergences, both
DC3-newer code that retail refutes — the same class as the SmartGlass block and
`mOverlapped` that W15-F refuted in this very file.

**(a) Order.** Retail `fn_8251C960` opens `lbz r11,0x8(r5)` / `cmplwi` / `bne` /
`li r3,-0x1` — the `onlineID` validity test — and only *then* performs the virtual
slot-0 `GetPadNum` (`lwz r11,0(r4)`; `lwz r11,0(r11)`; `mtctr`; `bctrl`). Our body
called `GetPadNum` first. A virtual call cannot be sunk across a branch by the
compiler, so this ordering is **source-visible and load-bearing**, not scheduling
noise.

**(b) No NUI branch.** Retail reaches `XShowGamerCardUI` by ONE unconditional
`bl fn_8283D720`. There is no load of `?sXShowCallback@PlatformMgr@@2P6A_NAAK@ZA`
and no `XShowNuiGamerCardUI` anywhere in the body. NUI is Kinect — a 2012 DC3-era
feature; RB3 is 2010. Deleting it also retired the dead `unsigned long trackingID`,
whose address-taken-ness was the entire reason our frame was `stwu r1,-0x80`
against retail's `-0x70` (objdiff `[off:-16]`).

Retail's returns decode onto the existing enum exactly, independently confirming
the signature: `-1` Failed, `-2` PrivilegeFailed, `-3` NotSignedIn, and a
branchless `subfic r11,r3,0 / subfe r3,r11,r11` tail = `(ret==0) ? 0 : -1`.
Callees verified: `0x82524528 = ?GetXUID@OnlineID@@QBA_KXZ`; `fn_8251BBF0` is the
anon-ns `XPrivilegeCheck` with `r3=0xf9` (PROFILE_VIEWING), `r4=0xf8`
(PROFILE_VIEWING_FRIENDS_ONLY). `fn_8251BE30` (`IsSignedIntoLive`) is unnamed in
the map, so objdiff **forgives** that call's placeholder target name.

**PRE-REGISTERED:** crosses +1 fn / +164 B / 1 unit · or improves-but-residual
+0/+0 with the **named** risk being regalloc on r29/r30/r31 · or 0-0.
**MEASURED:**

```
leg A: matched=43109 code%=38.267975   leg B: matched=43110 code%=38.269577
Δmatched=+1  Δcode_bytes=+164  Δcode%=+0.001602pp  Δfuzzy=+0.001011pp
unit net (ALL units) = +1 ;  default/PlatformMgr_Xbox 15 -> 16
```

★ The regalloc risk **did not materialise**: restoring retail's order restored
retail's liveness and r29/r30/r31 fell out identically, unprompted. Commit `11c5b96c`.

⚠ **Correction to W15-F and to this file's own comment.** The extent is **164 B**,
not the "248 B" recorded. 248 is `0x8251ca58-0x8251c960`, an **address-gap
subtraction** across the 72 B funclet `fn_8251CA08` plus an 8 B EH prefix — the
exact error W15-F warned against in its own §4.1. W15-F's **conclusion** (RB3
carries the full body on the `LocalUser` overload, DC3's `ForPadNum` split is a
later refactor) is **correct and is now proven by the body matching**; only its
stated reason was unsound. *Count right, cause wrong.*

---

## 3. The COMDAT emitter table (items 3 and 4)

Method, now shipped as **`tools/comdat_emitter_census.py`**: for every `.text`
block in `0x8251A000-0x8251E100`, take the symbols it hosts from
`target_symbol_map.json` and ask which of our **1,206 compiled objects** *defines*
each (COFF `SectionNumber > 0`; a mere reference is section 0 and does not count).

★ **The census DISCRIMINATES, which is what makes it usable.** Every ordinary
single-owner block resolves to exactly one object that IS its pinned unit —
FileCache (39 rows), UsbMidiGuitar, System_Xbox, UsbMidiKeyboard, GameMicManager.
A census that answered "one emitter" or "many" everywhere would confirm whatever
it was pointed at. Those controls are the reason the COMDAT verdicts below are
worth anything.

| block | size | pinned unit | hosted symbol | emitted by (ours) | verdict |
|---|---:|---|---|---|---|
| `0x8251B880` | 128 | Timer.cpp | `?Type@ProfileSwappedMsg@@` | **5**: Campaign, ClosetMgr, SaveLoadManager, StorePanel, **Timer** | pinned emits ⇒ **keep** |
| `0x8251B900` | 256 | ContentMgr_Xbox.cpp | `?Type@StorageChangedMsg@@` · `?Type@ContentInstalledMsg@@` | **2**: MemcardMgr_Xbox, **ContentMgr_Xbox** · **1**: **ContentMgr_Xbox** | pinned emits BOTH ⇒ **keep** |
| `0x8251BA00` | 128 | MetaPanel.cpp | `?Type@XMPStateChangedMsg@@` | **1**: **MetaPanel** | **keep** — refutes dc3's map |
| `0x8251BA80` | 128 | OvershellPanel.cpp | `?Type@PartyMembersChangedMsg@@` | **1**: **OvershellPanel** | **keep** — refutes dc3's map |
| `0x8251BB00` | 240 | MemcardMgr_Xbox.cpp | `?Type@UIChangedMsg@@` · `?SetName@Friend@@` | **4** incl. **MemcardMgr_Xbox** · **NONE** | **keep** — confirms W15-F's §4.2 split point |
| `0x8251CBE0` | 296 | PlatformMgr.cpp | `??0SigninChangedMsg@@` | **1**: **PlatformMgr** | **keep** — POSITIVE CONTROL (W14-C's landed repair) |
| `0x8251CD08` | 456 | UI.cpp | `??0UITransitionCompleteMsg@@` | **1**: **UI** | **keep** |
| `0x8251CED0` | 304 | DingoSvr.cpp | `??0ServerStatusChangedMsg@@` | **2**: **DingoSvr**, DingoSvr_Xbox | **keep** — see §5, the premise is refuted |
| `0x8251D000` | 224 | HamNavList.cpp | `??0LeftHandListEngagementMsg@@` | **1**: **HamNavList** | **keep** — POSITIVE CONTROL |
| `0x8251D0E0` | 1000 | MemcardMgr_Xbox.cpp | `??0MCResultMsg@@` · `??0PlatformMgrOpCompleteMsg@@` · `??0UIChangedMsg@@` | **1**: **MemcardMgr_Xbox** · **NONE** · **NONE** | **keep** |
| `0x8251DA38` | 120 | PassiveMessenger.cpp | `?Type@InviteSentMsg@@` | **1**: **PassiveMessenger** | **keep** |

**EVERY COMDAT block stays.** In every case the pinned unit is among the emitters.
W15-F's conservative §5 call was right, and is now **proven rather than merely
cautious**.

★ **§5's thesis is demonstrated concretely, not just asserted.** dc3's map assigns
`xmp_state_changed` and `party_members_changed` to `os:PlatformMgr_Xbox.obj`. Our
`PlatformMgr_Xbox.obj` emits **neither**; `MetaPanel.obj` and `OvershellPanel.obj`
each emit exactly one, and each **is** its pinned unit. The map was recording DC3's
arbitrary ICF survivor under DC3's link order, exactly as §5 predicted.

### Item 3 specifically

W15-F asked: adjudicate by checking which of our two DingoSvr objects emits the
ctor. **Both do** (`system/net/DingoSvr.obj` and `system/net/DingoSvr_Xbox.obj`),
so by the stated rule the block stays. The pinned unit already emits it, the row
already pairs (fuzzy 99.848), and a move between two objects that both define the
symbol changes no pairability. But the real answer is worse than "both" — see §5.

### Three rows that no pin move can ever help

`?SetName@Friend@@` (68 B), `??0PlatformMgrOpCompleteMsg@@` (136 B) and
`??0UIChangedMsg@@` (136 B) are at fuzzy 0 and are emitted by **no compiled object
at all**. They are unpairable **by absence**: they need a TU that instantiates
them, not a pin. Note `?Type@UIChangedMsg@@` *is* emitted (4 objects) while the
**ctor** `??0UIChangedMsg@@` is emitted by none — nothing we compile ever
constructs one.

---

## 4. The region's only genuine mis-pin was not a Msg block (+352 B)

Running item 4's test **mechanically over every block**, rather than only the seven
flagged ones, surfaced the one real move:

```
0x8251D8A4-0x8251D9C0 (284 B) hosts ?_M_insert_overflow@vector<u64>  240 B  fuzzy 0.000
0x8251D9C0-0x8251DA38 (120 B) hosts ?push_back@vector<u64>           112 B  fuzzy 0.000
```

Both were pinned to `BandUser.cpp`. COFF says why they could never pair:

| object | `vector<u64>` members it instantiates |
|---|---|
| `band3/game/BandUser.obj` | `begin`, `end`, `rbegin`, `rend`, ctor, dtor — **never `push_back`**, so `_M_insert_overflow` is never instantiated |
| `band3/meta_band/TokenRedemptionPanel.obj` | `size`, `_M_set`, `_M_clear`, **`_M_insert_overflow` (sec717)**, **`push_back` (sec780)**, … |

objdiff pairs by NAME, so the pinned unit was **structurally incapable** of
supplying these bodies. Exactly one of our objects can.

⚠ **This does not violate §5.** For a template COMDAT "which retail TU owned it" is
as unanswerable as for a Msg COMDAT. The claim made here is the checkable one —
*which of our objects emits the body*. Note also that `0x8251B880-0x8251DAB0` hosts
Msg COMDATs from **ten unrelated TUs**: the region is a **linker COMDAT pool**, so
spatial adjacency to BandUser's other pins argues nothing either way.

**PRE-REGISTERED:** both cross +2 fns / +352 B / 2 units · or pair-but-not-match
+0/+0 · or 0-0; Δ`total_code` = 0.
**MEASURED** (both legs at a `symbols.txt` **split fixed point**):

```
leg A: matched=43110 code%=38.269577   leg B: matched=43111 code%=38.273014
Δmatched=+1  Δmasked_equal=-1  Δhonest=+2  Δcode_bytes=+352  Δcode%=+0.003437pp
+2  default/band3/meta_band/TokenRedemptionPanel (53->55) — both rows fuzzy 100.000
-1  default/BandUser (144->143), matched_code 12,188 on BOTH legs — it lost ZERO bytes
total_code 10,245,956 / total_functions 69,219 on BOTH legs
```

**Bytes hit the pre-registration exactly (+352). The function count did not**
(+1 vs +2): a zero-byte funclet row left BandUser's denominator with the block —
reattribution, the same mechanism W15-F saw on `fn_8251BDE0`, not a regression.
Recorded because predicting +2 was wrong in a way that is predictable next time.

⚠ **Negative result worth keeping.** The first A/B was **REFUSED** (`ab_measure`
rc=2) by the split-guard: a `.text` move requires `.pdata` to move with it, and
`.pdata` is **derived output**. dtk re-derived `0x82218A28-0x82218A40` to the new
unit **itself, unprompted** — the splitter moved in the same direction as the
argument, exactly as it did for W15-F §4.1. Recovery is one build; the converged
file is what is committed. Commit `e1ad49bb`.

---

## 5. ⛔ `??0ServerStatusChangedMsg@@` (132 B) — the map name is PROVABLY WRONG, and it is uncollectable today

The row sits at fuzzy **99.848** with exactly **one** charged site:

```
target:  bl ?Type@XMPStateChangedMsg@@SA?AVSymbol@@XZ
base:    bl ?Type@ServerStatusChangedMsg@@SA?AVSymbol@@XZ
```

This is the shape CLAUDE.md warns is meaningless to accept at face value — a
`diff_arg`-only row where a tool would happily label `AT_LIMIT`. Adjudicated on
retail bytes instead, and it is **not** an ICF fold: `?Type@ServerStatusChangedMsg@@`
holds its **own distinct address** `0x823ec318`, and a fold leaves one survivor.

Two independent retail-byte witnesses identify the function at `0x8251CED0`:

1. **String chain** (W14-C's `ctor -> Type() -> string -> class`). It calls
   `fn_8251BA00`, whose body interns the `.rdata` string at `0x82088880`. Read
   from `orig/45410914/band.exe` through the PE section table: **`"xmp_state_changed"`**.
2. **RTTI.** It stores vtable `0x82088BFC` at `0(r30)`. That vtable's `??_R4`
   Complete Object Locator at `vt-4` is `0x821DDA30`, whose type descriptor
   `0x82C71968` reads **`.?AVXMPStateChangedMsg@@`**.

⇒ **`0x8251CED0` is `??0XMPStateChangedMsg@@`, not `??0ServerStatusChangedMsg@@`.**
Our `ServerStatusChangedMsg(ServerStatusResult)` source has been scored against a
*different class's* constructor, and reads 99.848% because the two ctors are
structurally identical apart from the `Type()` callee. **The single charge is the
truth showing through.**

**Why the 132 B cannot be collected today**, which is the part that matters:

- The row can **never** cross by source work. Our `ServerStatusChangedMsg` ctor
  will always call `ServerStatusChangedMsg::Type`; the target is a different class.
- Retail's ctor takes one int-like argument and stores `mType = 0` — the codegen
  shape our `ServerStatusChangedMsg(enum)` produces — so the correct name is
  `??0XMPStateChangedMsg@@QAA@H@Z`, matching `MetaPanel.h:17`
  `XMPStateChangedMsg(int i) : Message(Type(), i) {}`.
- ⛔ **No compiled object of ours emits that symbol.** A census of all 1,206 objects
  finds only `??0XMPStateChangedMsg@@QAA@**PAVDataArray@@**@Z` (in `MetaPanel.obj`).
  The int ctor is inline in a header and **no TU we compile ever constructs one**.
- ⇒ Renaming the map row today would convert a 99.848% near-miss into a
  **permanently 0%** unpairable row for **zero** byte gain — precisely CLAUDE.md's
  *"proving a name wrong ≠ renaming is SAFE (the base obj may not define it)"*.

**Collecting it requires all three together**, which is a lane, not an edit:
(a) rename `0x8251ced0` -> `??0XMPStateChangedMsg@@QAA@H@Z`; (b) get some compiled
TU to emit that ctor; (c) re-home the block from `DingoSvr.cpp` to that TU, since
a row's unit comes from the splits pin, not from its name. Pre-register the sign:
per CLAUDE.md, **un-pairing is ~80.5% of a map edit's delta.**

★ This also **refutes the framing of W15-F's §6.1**, which called this the most
likely genuine mis-pin of the seven and proposed adjudicating by which DingoSvr
object emits the ctor. That test runs clean (both do) and is the wrong question:
the block is not mis-pinned, the **name** is wrong.

---

## 6. What this lane deliberately did NOT do

- **Did not move any COMDAT block** (§3). All eleven tested keep their pin, each
  because the pinned unit is among our emitters — a proven verdict, not a cautious one.
- **Did not rename `0x8251ced0`**, though the name is proven wrong (§5). Renaming
  alone is strictly negative; the fix is a coupled map+source+splits change.
- **Did not touch the three unpairable-by-absence rows** (§3) — they need a TU that
  instantiates them; no pin move can help, so none was attempted.
- **Did not delete the DC3-only SmartGlass block** or re-add `mOverlapped`, holding
  W15-F's line: both are contradicted by retail bytes.
- **Did not port `RunXinputJoypadLoop`** (W15-F §6.3) — still a subsystem port.
- **Did not re-derive the reachable ceiling** or any whole-binary census; every
  figure here is a delta measured in-run, never an inherited absolute.
- **Did not delete the worktree.**

## 7. Roll-up

| change | pre-registered | measured | units moved |
|---|---|---|---|
| `05cc287c` `GetName` 2-arg `Localize` | +1 fn / +168 B / 1 unit | **+1 / +168 B** | 1 |
| `11c5b96c` `ShowGamercard` order + no NUI | +1 fn / +164 B / 1 unit | **+1 / +164 B** | 1 |
| `e1ad49bb` `vector<u64>` blocks -> TokenRedemptionPanel | +2 fns / +352 B / 2 units | **+1 / +352 B** | 2 |

Whole binary **43,108 -> 43,111 matched / 3,920,752 -> 3,921,436 B /
38.266340% -> 38.273014% / fuzzy 49.232150 -> 49.236782**, with
**`total_code` 10,245,956 and `total_functions` 69,219 on every one of the six
legs** — no denominator movement anywhere, so none of the gain is bookkeeping.
