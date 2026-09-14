# The PlatformMgr region 0x8251B000-0x8251E000: a destination built, two blocks re-homed

**Lane W15-F, 2026-09-14.** Branch `w15-f` off `main` `5b14bc86`.
Worktree `~/tmp/wt-w15-f`. Continues W15-A
(`docs/decomp/CIRCULAR_PIN_CENSUS_2026-09-14.md`) and W14-C
(`docs/decomp/SWAPPED_NAMES_2026-09-14.md`).

> **VERDICT SUMMARY**
>
> | | |
> |---|---|
> | W15-A's blocker | **REMOVED.** `src/system/os/PlatformMgr_Xbox.cpp` is wired, compiles, and now DEFINES all six names it said no compiled TU defined (COFF `SectionNumber > 0`). |
> | blocks re-homed | **2**, both A/B'd settled, per-unit attributed |
> | measured total | **+12 matched functions / +1,160 B / +0.011322 pp code% / +0.013458 pp fuzzy / +1 unit at 100%** (3 settled A/Bs: +8/+644, +3/+444, +1/+72) |
> | `total_code` | **10,245,956 on every leg of every run — unchanged.** No denominator movement to explain. |
> | ★ largest surprise | **Five of seven ported DC3 bodies were byte-exact for RB3 on the first try.** My Move-1 pre-registration said "I do NOT predict a gain"; measured **+644 B**. |
> | ⛔ blocks deliberately NOT moved | **7**, all COMDAT-hosting. For a COMDAT "which retail TU owned it" is not answerable; see §5. |
> | bonus | `InitXinputJoypadThreadData` ported to 100% (+72 B), the exact figure W15-A predicted was collectable |

---

## 1. Provenance

Full `./tools/ninja-locked` before any name-keyed analysis — a fresh worktree's
reflinked target objs are **pre-renamer**, so every mangled-name lookup would
read "absent" and every negative would be vacuous.

```
EXIT=0
[patch-state] OK: 1205 decomp, 3083 target objects match (tree_sha256=d04e571de9645a09)
scripts/verify_objs_patched.py --verify-manifest -> rc=0
```

Ruler **`name_check`**, read from `report.json`'s `provenance.diff_config`, not
assumed. Baseline at `5b14bc86`:
**42,860 matched / 3,898,632 B / 38.050446% / fuzzy 49.200085 /
total_code 10,245,956 / total_functions 69,219 / masked_equal 22,997.**

## 2. The region table

Every `.text` block overlapping `0x8251B000-0x8251E000`, its pin **as this lane
found it**, its named rows, and the verdict. `ACTION` marks what this lane did.

| block | size | pin (before) | named rows | evidence for true owner | verdict |
|---|---:|---|---|---|---|
| `0x8251AA60-0x8251B09C` | 1596 | UsbMidiKeyboard.cpp | 3 `UsbMidiKeyboard` methods | self-consistent | keep |
| `0x8251B09C-0x8251B3D8` | 828 | UsbMidiKeyboard.cpp | — | — | keep |
| `0x8251B3D8-0x8251B410` | 56 | System_Xbox.cpp | `SystemPreInit` | dc3 map: `os:System_Xbox.obj` | **keep — POSITIVE CONTROL** |
| `0x8251B410-0x8251B848` | 1080 | UsbMidiKeyboard.cpp | — | — | keep |
| `0x8251B848-0x8251B880` | 56 | System_Xbox.cpp | `Set/GetDiskErrorCallback` | dc3 map: `os:System_Xbox.obj` | **keep — POSITIVE CONTROL** |
| `0x8251B880-0x8251B900` | 128 | Timer.cpp | `?Type@ProfileSwappedMsg@@` | dc3: `profile_swapped` -> `meta:StorePanel.obj` | COMDAT — unprovable (§5) |
| `0x8251B900-0x8251BA00` | 256 | ContentMgr_Xbox.cpp | `?Type@StorageChangedMsg@@`, `?Type@ContentInstalledMsg@@` | dc3: both strings -> `os:PlatformMgr_Xbox.obj` | COMDAT — **suspect, left** (§5) |
| `0x8251BA00-0x8251BA80` | 128 | MetaPanel.cpp | `?Type@XMPStateChangedMsg@@` | dc3: `xmp_state_changed` -> `os:PlatformMgr_Xbox.obj` | COMDAT — **suspect, left** (§5) |
| `0x8251BA80-0x8251BB00` | 128 | OvershellPanel.cpp | `?Type@PartyMembersChangedMsg@@` | dc3: `party_members_changed` -> `os:PlatformMgr_Xbox.obj` | COMDAT — **suspect, left** (§5) |
| `0x8251BB00-0x8251BBF0` | 240 | MemcardMgr_Xbox.cpp | `?Type@UIChangedMsg@@`, `?SetName@Friend@@` | **our** MemcardMgr_Xbox.cpp emits `UIChangedMsg`; PlatformMgr_Xbox.cpp does not | **keep — proven by OUR-SIDE emission** |
| `0x8251BBF0-0x8251BFA8` | 952 | MemcardMgr_Xbox.cpp | `XPrivilegeCheck`, `IsEthernetCableConnected`, `SetPadContext`, `SetPadPresence` | dc3 map: all 4 -> `os:PlatformMgr_Xbox.obj`; internal linkage | **ACTION: MOVED (+444 B)** |
| `0x8251BFA8-0x8251CBE0` | 3128 | MoggClipMap.cpp | 6 `PlatformMgr` methods + `GetPadNumFromXuid` + a 4 B `ObjPtr<MoggClip>` thunk | dc3 map 6/6; internal linkage; dtk's own `.pdata` re-derivation | **ACTION: MOVED (+644 B)** |
| `0x8251CBE0-0x8251CD08` | 296 | PlatformMgr.cpp | `??0SigninChangedMsg@@` | dc3: `signin_changed` -> `os:PlatformMgr.obj` | **keep — POSITIVE CONTROL on W14-C's landed repair** |
| `0x8251CD08-0x8251CED0` | 456 | UI.cpp | `??0UITransitionCompleteMsg@@` | plausible | keep |
| `0x8251CED0-0x8251D000` | 304 | DingoSvr.cpp | `??0ServerStatusChangedMsg@@` | dc3: `server_status_changed` -> `net:DingoSvr_Xbox.obj` | ⚠ **FLAGGED** (§6) |
| `0x8251D000-0x8251D0E0` | 224 | HamNavList.cpp | `??0LeftHandListEngagementMsg@@` | dc3: -> `hamobj:HamNavList.obj` | **keep — POSITIVE CONTROL** |
| `0x8251D0E0-0x8251D4C8` | 1000 | MemcardMgr_Xbox.cpp | `??0MCResultMsg@@`, `??0PlatformMgrOpCompleteMsg@@`, `??0UIChangedMsg@@` | dc3: `platform_mgr_op_complete` -> `os:PlatformMgr_Xbox.obj` | COMDAT ctors — **suspect, left** (§5) |
| `0x8251D4C8-0x8251D8A4` | 988 | GameMicManager.cpp | `?SetType@MsgSource@@` thunk, `??_GGameMicManager@@` | self-consistent | keep |
| `0x8251D8A4-0x8251D9C0` | 284 | BandUser.cpp | `vector<u64>::_M_insert_overflow` | STL template COMDAT | keep |
| `0x8251D9C0-0x8251DA38` | 120 | BandUser.cpp | `vector<u64>::push_back` | STL template COMDAT | keep |
| `0x8251DA38-0x8251DAB0` | 120 | PassiveMessenger.cpp | `?Type@InviteSentMsg@@` | dc3: `invite_sent` not in map | keep |
| `0x8251DAB0-0x8251E000` | ~1360 | *(unpinned)* | — | `auto_*` | — |

★ **The census channel DISCRIMINATES, and that is what makes it usable.** DC3's
leaked `ham_xbox_r.map` assigns 3 of the region's rows to `os:System_Xbox.obj`
(matching their existing pins exactly), `signin_changed` to `os:PlatformMgr.obj`
(matching W14-C's *landed* repair), and `left_hand_list_engagement` to
`hamobj:HamNavList.obj` (matching its existing pin) — while assigning **10 rows
to `os:PlatformMgr_Xbox.obj` that were pinned elsewhere**. A channel that
answered "PlatformMgr_Xbox" for everything could not produce that split, and two
of its agreements are with repairs made independently by another lane.

## 3. The wiring (commit `aa5cd1ec`)

W15-A's blocker was exact: *"no TU we compile emits these names, so no pin move
can make them pair."* `src/system/os/PlatformMgr_Xbox.cpp` was in-tree (728
lines) but absent from `objects.json`, and `tools/project.py` drops the compile
edge **silently** (`warn_missing_source=False`) — the same mechanism behind
CLAUDE.md's 230 no-source units.

Declared `NonMatching` in the `engine` category. Two compile errors, **both
DC3-only code refuted by retail bytes** — the oracle is newer than RB3 and loses
where they differ:

| error | retail evidence | action |
|---|---|---|
| `mOverlapped.hEvent = 0` | `XOVERLAPPED mOverlapped` is DC3's XSocial block; lane NCCC removed it from `PlatformMgr.h` because retail's member block runs `0x1c..0x47` (44 B), proven by `PlatformMgr::Handle` (`0x825152e0`) opening `subi r3,r25,0x4c`. It does not fit. | dropped the write, did **not** re-add the member |
| `SmartGlassMsg` undeclared | SmartGlass is a 2012 DC3 feature. `"smart_glass_msg"` occurs **0 times** in `orig/45410914/band.exe`, and **none** of the retail map's 27 `PlatformMgr` rows is a SmartGlass symbol. | declared the class **file-locally**, recorded the block as DC3-only dead code |

★ `PlatformMgr.h`'s own size-budget comment **predicted this exact collision**
and named this file as the only user of the removed members. Re-adding
`mOverlapped` would have cascaded through a PCH-eligible shared header to
reinstate a layout retail contradicts — the metric-shaped fix would have been
the wrong one.

Seven bodies were missing, all already **declared** in `PlatformMgr.h` with
signatures that mangle exactly to retail's names: `IsInParty`,
`IsInPartyWithOthers`, `SetPadProperty`, `GetName`, `GetPadNumFromXuid`,
`XPrivilegeCheck`, `ShowGamercard`.

⚠ **`ShowGamercard` is not a straight oracle copy.** DC3 splits it into
`ShowGamercardForPadNum(int, const OnlineID*)` plus a wrapper. Retail RB3's only
gamercard row is `ShowGamercard(LocalUser*, const OnlineID*)` at `0x8251c960`,
**248 B** — far too large for a thin pad-num wrapper. RB3 carries the full body
on the `LocalUser` overload; DC3's split is a later refactor. Ported accordingly.

**Verified the way W15-A verified the negative** — COFF symbol read,
`SectionNumber > 0`, which is definition and not mere reference:

```
sec447 ?IsInParty@PlatformMgr@@QAA_NXZ
sec449 ?IsInPartyWithOthers@PlatformMgr@@QAA_NXZ
sec174 ?SetScreenSaver@PlatformMgr@@QAAX_N@Z
sec159 ?SetPadProperty@PlatformMgr@@QBAXHHPBG@Z
sec195 ?ShowGamercard@PlatformMgr@@QAA?AW4ShowGamercardResult@@PAVLocalUser@@PBVOnlineID@@@Z
sec451 ?GetName@PlatformMgr@@QBAPBDH@Z
sec139/150/153  IsEthernetCableConnected / SetPadContext / SetPadPresence
```

The wiring alone is **metric-neutral by construction and confirmed so**: with no
splits pin there is no target obj to pair against, and the full build's measures
came back **bit-identical** to the pre-change build on the same tree.

⚠ **Port and pin are COUPLED, which was not obvious in advance.** Our object
first emitted the anonymous-namespace hash `?A0x2875847b` where retail's map
says `?A0x8a9ffbf2`. `obj_anon_ns_patcher` normalises that — but it resolves
pairing through `objdiff.json`, so it **could not act until a target obj
existed**, i.e. until the block was pinned. After the pin the hash was rewritten
to retail's spelling. Two consequences for anyone repeating this:
the build needs **two or three iterations** to reach a fixed point (the first
fails the split-guard because `.pdata` is derived output and dtk rewrites
`splits.txt`; a later one fails `verify_objs_patched --check` because the
patcher stamp is current while a *new target* appeared — `touch` the source to
force the recompile). Neither is a defect; both are the documented contracts
firing correctly.

## 4. The two re-homes

### 4.1 `036c18ac` — `0x8251BFA8-0x8251CBE0` MoggClipMap.cpp -> PlatformMgr_Xbox.cpp

Evidence: 7 of 8 named rows are `PlatformMgr`; dc3's map places 6 in
`os:PlatformMgr_Xbox.obj`; W15-A's internal-linkage proof (`ShowGamercard` calls
the anon-namespace static `XPrivilegeCheck`); and ★ **dtk re-derived `.pdata`
`0x82218858-0x82218918` from MoggClipMap to the new unit UNPROMPTED**, using
unwind-record ownership and knowing nothing of the argument — the splitter moved
in the same direction, exactly as it did for W15-A's §9.2.

The sole MoggClipMap justification is a **4-byte** `ObjPtr<MoggClip>` destructor
at fuzzy 95, and a 4-byte body is relocation-masked byte-identical to every
other 4-byte thunk in the binary, so that name carries no information.

**PRE-REGISTERED:** *"Delta matched_code BETWEEN -72 and 0 ... I do NOT predict
a gain."* Reasoning: the block's only 72 matched bytes were two `masked_equal`
rows pairing by funclet byte signature, and a first port rarely reaches 100.

**MEASURED** (settled A/B, both legs at a split fixed point):

```
leg A: matched=42860 masked=22997 honest=19863 code%=38.050446  (settled)
leg B: matched=42868 masked=23000 honest=19868 code%=38.056732
Δmatched=+8  Δmasked_equal=+3  Δhonest=+5  Δcode%=+0.006286pp  Δcode_bytes=+644
Δfuzzy=+0.008300pp    total_code 10245956 on BOTH legs (unchanged)
units at 100%: 165 -> 166  (MoggClipMap reached 100, DENOMINATOR_SHRANK)
```

| unit | leg A | leg B |
|---|---|---|
| `default/PlatformMgr_Xbox` | — (did not exist) | **10** matched |
| `default/MoggClipMap` | 10 matched / 45 rows | **8** matched / **8** rows |

**Exactly 2 units changed in the whole binary.**

★★ **MY PREDICTION WAS WRONG, IN THE FAVOURABLE DIRECTION, AND THE REASON IS THE
FINDING.** Five of the seven ported DC3 bodies are **byte-exact for RB3 on the
first try** — `IsInParty` 136 B, `IsInPartyWithOthers` 72 B, `SetScreenSaver`
12 B, `GetPadNumFromXuid` 208 B, `SetPadProperty` 104 B. I had assumed a first
port would not match. For this engine it did. That is CLAUDE.md's "DC3 is the
same Milo engine" thesis paying out **literally**, not as an approximation, and
it should raise the prior for any future *engine* body-port from DC3. Both
`masked_equal` rows also survived the move, and two more appeared.

⚠ **MoggClipMap reached 100% by `DENOMINATOR_SHRANK`, not by matching anything.**
Wrongly-attributed rows left its denominator (45 rows -> 8). `ab_measure`'s
set-diff labels the mechanism; a "whose matched count rose" list is blind to it.

⚠ **Secondary error, recorded so it is not repeated:** I sized candidate rows by
**address-gap subtraction** and predicted `SetScreenSaver` at "200 B"; its true
extent is **12 B**. The gaps include EH prefixes and funclets sitting *between*
functions. Size from report extents, never by subtracting adjacent symbol
addresses.

### 4.2 `d34f1f1b` — `0x8251BBF0-0x8251BFA8` MemcardMgr_Xbox.cpp -> PlatformMgr_Xbox.cpp

The block W15-A flagged (`0x8251BB00-0x8251BFA8`) is **MIXED**, so only the
proven part moved — W15-A's own conservative §9.2 precedent.

★ **The split point was chosen on OUR-SIDE COMDAT EMISSION, not on the map**,
and that is the reusable rule of this lane (§5). The first 240 B hold
`?Type@UIChangedMsg@@` (88 B, pairing **by name** today) and `?SetName@Friend@@`
(inline in `Friend.h`). Checked at source level: our `PlatformMgr_Xbox.cpp`
never mentions `UIChangedMsg` (our `MemcardMgr_Xbox.cpp` uses it twice) and
holds `Friend` only as a **pointer**, so it instantiates neither COMDAT. Moving
the whole block would have cost 88 B **with certainty**, not as a risk.

**PRE-REGISTERED:** Δmatched **+3** (range 0..+4); Δcode_bytes **+300** (range
0..+700); Δtotal_code 0; exactly 2 units change; *"MemcardMgr_Xbox must lose 0
matched bytes — if it loses 88 I mis-read the COMDAT emission and will say so."*

**MEASURED:**

```
Δmatched=+3  Δmasked_equal=-1  Δhonest=+4  Δcode%=+0.004334pp  Δcode_bytes=+444
Δfuzzy=+0.004335pp    total_code 10245956 on BOTH legs (unchanged)
units at 100%: 166 -> 166   exactly 2 units changed
default/PlatformMgr_Xbox 10 -> 14      default/MemcardMgr_Xbox 47 -> 46
MemcardMgr_Xbox matched_code = 3692 on BOTH legs  <- lost ZERO bytes
```

Δmatched hit the pre-registered **+3 exactly**; bytes landed inside the stated
range. All four moved rows reached fuzzy 100 immediately (`XPrivilegeCheck`
228 B, `IsEthernetCableConnected` 40 B, `SetPadContext` 76 B, `SetPadPresence`
100 B = 444 B, the whole delta). The COMDAT-emission reading is **confirmed**
rather than merely un-refuted, because the stated falsifier (-88 B) did not fire.

MemcardMgr_Xbox's -1 matched function is `fn_8251BDE0` (mpn 100 / fuzzy 99.4,
contributing **0 bytes**) leaving its denominator with the block — reattribution,
not regression.

## 5. ⛔ Why SEVEN blocks were deliberately NOT moved — the COMDAT rule

Seven region blocks host `?Type@...Msg@@` functions or `??0...Msg@@`
constructors. `DECLARE_MESSAGE` defines `static Symbol Type()` **inside the class
body** and the custom ctors are declared inline in headers, so **every one of
them is a COMDAT**: emitted by *each* TU that uses it, and reduced by the linker
to one arbitrary survivor.

⇒ **For COMDAT content, "which retail TU owned it" is not a question with an
answer.** It is the same hazard as `_icf_arbitrary`, which W15-A's §3 shows
nearly produced a false finding. dc3's map assigning `storage_changed`,
`xmp_state_changed`, `party_members_changed` and `platform_mgr_op_complete` to
`os:PlatformMgr_Xbox.obj` tells us **DC3's** survivor, under **DC3's** link
order — and the same map assigns `profile_swapped` to `meta:StorePanel.obj` and
`ui_changed` to `gesture:CameraTilt.obj`, which is exactly the arbitrariness
showing through.

★ **The operative question instead is: which of OUR objects EMITS it?** That is
what decides pairability, it is checkable from our own source, and it is the
criterion that set §4.2's split point. Applied here it says **leave all seven
alone** — each currently pairs (or fails to) against a unit that does emit the
COMDAT, and moving them buys an unprovable accuracy claim at a certain byte cost.

⚠ This is **not** the same axis as W14-C's repair. W14-C fixed *which name sits
at which address* (a 6-cycle), which is answerable from retail bytes via
`ctor -> Type() -> string literal -> class`. Unit attribution of a COMDAT is not.

## 6. Findings handed off, not acted on

1. ⚠ **`0x8251CED0-0x8251D000` is pinned `DingoSvr.cpp`, but dc3's map puts
   `server_status_changed` in `net:DingoSvr_Xbox.obj`** — and
   `system/net/DingoSvr_Xbox.cpp` **is** in our `objects.json`. This is a
   COMDAT ctor, so §5 applies and I did not move it; but unlike the others its
   candidate destination is a *sibling of the same TU family*, which makes it
   the most likely of the seven to be a genuine mis-pin. Adjudicate by checking
   which of our two DingoSvr objects emits the ctor.
2. **`ShowGamercard` sits at fuzzy 35.8 and `GetName` at 88.9** — both now
   **pairable** where they were unpairable, which is the point of this lane;
   their residual is source work, not attribution. `GetName` has a concrete,
   named lever: it calls the **3-arg** `Localize(Symbol, bool*, Locale&)` per the
   DC3 oracle, but `src/system/utl/Locale.h` records a *verified* finding that
   RB3 call sites use the **2-arg** form. That is a one-line experiment.
3. **`RunXinputJoypadLoop` (`0x8252A0B8`) is NOT a body-port** — W15-A's handoff
   named it alongside `InitXinputJoypadThreadData`, but our `Joypad_Xbox.cpp` is
   missing the entire XInput2 raw-HID layer it depends on:
   `JoypadGetCachedXInputCaps`, `XInput2Sample`, `XInput2GetDeviceId`,
   `ParseRawData` and four `XINPUTID_*` constants. That is a subsystem port and
   deserves its own lane.
4. **The SmartGlass block in `PlatformMgr_Xbox.cpp` (~100 lines) is DC3-only
   dead code** that can never match. Left in place deliberately: extra base
   symbols cannot mis-pair (objdiff pairs by name), so deleting it is risk
   without benefit. Someone doing a fidelity pass on that file should remove it.

## 7. Bonus: `InitXinputJoypadThreadData` (W15-A's §9.1 handoff, collected)

W15-A moved `0x82529890` to `Joypad_Xbox.cpp` and predicted the 72 B was
collectable once a body existed. It is, and retail's own bytes supplied the
missing constraint rather than the oracle:

```
0x82529890  lis  r11, 0x82cd
0x82529898  addi r11, r11, -0x4728      => r11 = tInputStates
0x825298a4  addi r7,  r9, -0x46e8       => tBreed   = tInputStates + 0x40
0x825298ac  stb  r10, 0x98(r11)  (x4)   => tNeedCaps = tInputStates + 0x98
```

★ A **baked-in displacement** like `0x98(r11)` is only emitted between
**internal-linkage** statics; MSVC makes anonymous-namespace variables
**external**, and each external gets its own relocation. Our whole block was in
`namespace { }`, so the storage class — not just the order — had to change. The
resulting ascending `.bss` run (`tInputStates +0x00, tBreed +0x40, sThreadData
+0x70, tButtonStatesCurr +0x78, tButtonStatesPrev +0x88, tNeedCaps +0x98,
tCritSection +0x9c`) is byte-for-byte the run DC3's `Joypad_Xbox.cpp`
documents — **verified here against retail bytes, not inherited.** `tNeedCaps`
did not exist in our file at all. DC3's `__declspec(align(8))` on it is
deliberately **not** copied: that is a workaround for a 4-byte hole left by
`tRawOutput`/`tRawPending`, which this file does not have, and the measured
layout is correct without it.

**PRE-REGISTERED:** Δmatched **+1**; Δcode_bytes **+72**; **exactly one** unit
changes. **MEASURED** — all three exact:

```
Δmatched=+1  Δcode%=+0.000702pp  Δcode_bytes=+72  Δfuzzy=+0.000823pp
unit net (ALL units) = +1, and default/Joypad_Xbox 14 -> 15 is the only mover
?InitXinputJoypadThreadData@?A0x439b694a@@YAXXZ   72 B   fuzzy 100.000
```

⚠ Worth stating because it is counter-intuitive: **reordering those declarations
broke nothing.** The other 14 matching rows in the unit are unchanged, precisely
*because* anonymous-namespace variables are external and carry their own
relocations — so reordering is inert for every function that does not use a
displacement. The reorder only matters for the one function that does.

## 8. What this lane deliberately did NOT do

- **Did not move any of the seven COMDAT-hosting blocks** (§5), including the
  four whose dc3 map rows point at `PlatformMgr_Xbox.obj`. The map's answer for
  a COMDAT is DC3's arbitrary survivor, and acting on it would install exactly
  the plausible-but-unprovable attribution this thread of lanes exists to audit.
- **Did not unpin anything.** CF-9's conservative unpin precedent perturbs
  `total_code` (it measured +52,184 B once); every change here is a move, and
  `total_code` is 10,245,956 on **all six legs measured**.
- **Did not delete the DC3-only SmartGlass block**, or re-add `mOverlapped` to
  `PlatformMgr.h` — the second would have been the metric-shaped fix and is
  contradicted by retail bytes.
- **Did not port `RunXinputJoypadLoop`** (§6.3) — it needs a subsystem, not a body.
- **Did not chase `ShowGamercard` (35.8) or `GetName` (88.9)** to 100. They are
  now pairable; the rest is a source lane with a named lever.
- **Did not re-run W15-A's callee-set-plurality census.** It is measured noise
  (1.38x/1.51x) and W15-A closed it.
- **Did not delete the worktree.**

## 9. Roll-up

Three settled A/Bs, same ruler (`name_check`), each with its own
pre-registration and per-unit attribution:

| change | pre-registered | measured | units moved |
|---|---|---|---|
| `036c18ac` MoggClipMap -> PlatformMgr_Xbox | −72..0 B, "no gain" | **+8 fns / +644 B** | 2 |
| `d34f1f1b` MemcardMgr_Xbox -> PlatformMgr_Xbox | +3 fns / +300 B (0..+700) | **+3 fns / +444 B** | 2 |
| `bf03d939` `InitXinputJoypadThreadData` | +1 fn / +72 B / 1 unit | **+1 fn / +72 B / 1 unit** | 1 |

Whole binary **42,860 -> 42,872 matched / 3,898,632 -> 3,899,792 B /
38.050446% -> 38.061768% / fuzzy 49.200085 -> 49.213543 / 165 -> 166 units at
100%**, with **`total_code` 10,245,956 and `total_functions` 69,219 on every one
of the six legs** — no denominator movement anywhere, so none of the gain is
bookkeeping.
