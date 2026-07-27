# The EH-funclet cascade, and the guard-bit timeline that closes it

*Derived and measured 2026-07-26 (lane `guardbit` + 6 sub-lanes). Numbers here are
whole-binary A/B results, not estimates.*

MSVC X360 `/EHsc` emits one ~40-byte **EH funclet** per cleanup state. A funclet's
first instruction is literally `subi rX, r12, <PARENT FRAME>` — it encodes its
parent's frame size in its own machine code, and its body names the frame slot it
cleans up.

**A funclet flips to a strict 100% match as soon as its parent's (a) frame size
(`stwu r1,-N(r1)`) and (b) saved-register range (`bl __savegprlr_N`) are exact —
even if the parent's body still does not match.**

Two consequences that govern how you work this pool:

1. **Never chase funclets directly. Fix the parent.** They flip as a block.
2. **Parent match% is the wrong scoreboard; the frame is the scoreboard.** Lane
   `gbB` closed `TessellateMesh`'s frame 0x290 → 0x280 and all three of its
   funclets flipped while the parent's own percentage went *down* (73.3 → 70.3).
   That is a win, not a regression.

Census tool: `scripts/harvest/funclet_cascade_rank.py` (EH-derived parent→funclet
association read out of the retail PE's `_s_FuncInfo` unwind maps, cross-checked
by a prologue screen — no spatial guessing).

## Where the pool actually is (TU5, 2026-07-26)

**1,004 parents own 2,720 unmatched funclets.** The split is the important part:

| pool | parents | funclets | blocker |
|---|---:|---:|---|
| **UNNAMED parents** | 843 | **2,363 (87%)** | identification — no `target_symbol_map` entry |
| named, frame ≠ target | 46 | 101 | **source — this is the workable lever** |
| named, frame = target | 92 | 220 | local-slot / temp packing (often at-limit) |
| parent outside its funclets' pinned unit | 33 | 81 | `splits.txt` attribution |

Top frames by funclet mass: `0x80`→395, `0x70`→355, `0xb0`→271 (`fn_822FC508`/
Waypoint alone = 120), `0xc0`→194, `0x90`→171, `0x160`→101 (`fn_822ECC48`/
VocalTrackDir = 82).

**So ~13% of this pool is source-blocked.** Do not plan a decomp campaign around
the other 87% — it needs the map lane. Unmapped funclets *do* still pair (objdiff
pairs them positionally within a unit; 11,518 were already matched with essentially
zero map coverage), so naming the *parent* is what unlocks them.

## The guard-bit timeline

**The guard-bit timeline is a complete transcript of the source's static-declaration
structure.** Each function-local `static` is lazily initialised behind one bit of a
per-function guard word:

- **bit ORDER = declaration order in the source**
- **the GAPS between guard-check runs = the grouping and placement of those declarations**

Read it straight out of the retail listing: for each `ori`/`oris rN, rN, <imm>`
inside the function, the bit index is the declaration index, and the string
constant loaded a few instructions later names it. `build/45410914/asm/<Unit>.s`
plus the `PE` reader in `scripts/harvest/funclet_cascade_rank.py` is all you need.

Worked example — `OvershellSlot::UpdateView` (`fn_825DB930`, guard word
`0x82DFFC9C`, bits 0-28, 29 statics). Reconstructing it named every divergence,
with none left unexplained, and closed frame 0x380 → 0x350 exactly: **70 funclets
flipped, 75.9% → 100%.**

Three failure shapes to look for, all of which appeared in that one function:

- **A MISSING key** — retail declares a static we do not (16 of them here, all
  referenced as `Symbols*.h` file-scope globals in our source).
- **An EXTRA key** — we declare one retail does not (`update_restart_allowed`, an
  rb3-Wii DEV addition retail predates).
- **A DEAD key** — retail declares and constructs a static it never reads. Tell:
  its label is referenced *only* by its own guard-init block, and unlike its live
  neighbours it gets no callee-saved register. (`keys` and `guitar` here. Keep them
  as declared-but-unused statics; they are load-bearing for the bit numbering.)

Ordering note: big recorders declare **DataPoint first, then keys**; small getters
do the reverse.

## Root-cause families behind a frame gap

Ranked by measured hit rate across seven lanes:

1. **Extra code retail predates.** Base-only `insert` clusters in objdiff *are* our
   surplus. `HANDLE_MESSAGE(UserLoginMsg)` in `OvershellSlot::Handle` was 30
   base-only instructions whose two stack temps were the entire +0x10 gap. Gate
   under `#ifdef HX_NATIVE` when the native port needs the behaviour.
2. **Local-static form.** We reference extern `*_msg` / `Symbols*.h` globals, or
   the non-`_STATIC` propsync macros, where retail declares function-local statics.
   `ObjMacros.h` already ships `SYNC_PROP*_STATIC` / `HANDLE_STATIC` for the macro
   cases. (Two independent lanes converged on the same declaration list for
   `ChordbookPanel`/`NewAwardPanel`/`BandStarDisplay` — the guard word is that
   deterministic a census.)
3. **DC3-newer drift.** DC3 is newer than RB3; where it grew a member or a
   parameter, retail has the older form. `UIFontImporter` carried six DC3-only
   members (vbase offset 0xf4 vs retail 0xe8); `MemFreeBlockStats` grew a 5th
   out-param; `StorePurchaser`'s DC3 `NeedsEnum` vtable slot has **zero** RB3 call
   sites and was shifting `Poll` 0x14 → 0x18.
4. **Container identity from CALL SHAPE.** `std::hash_map<Symbol,int>` vs
   `std::map`: retail called an out-of-line ctor/dtor and walked a null-terminated
   chain (node+0 next, +4 key, +8 value); `std::map` inlines its ctor and iterates
   via `_M_increment` against `end()`.
5. **Retail micro-idioms**, when the diff shows them: `size() != 0` (subf+divw) not
   `!empty()`; `(int)(b != 0)` normalisation; and explicit `>= A && <= B` /
   `< A || >= B` comparisons rather than the `(unsigned)(x - A) <= N` range trick
   (two instances in `SongParser::ParseText` alone).

## Don't chase — measured dead ends

- **`CharEyes`'s ~15 near-miss 40-byte funclets are objdiff MISPAIRINGS, not
  defects.** They differ by a single `lwz r3, 0x54(r31)` vs `0xbc(r31)` because a
  target funclet got paired against a base funclet belonging to a *different*
  parent. Working the ctor will not move them.
- **Forcing the inlined `ObjPtr(owner)` ctor globally measures NET −121.** Retail
  expands `mFoo(this)` two ways — an out-of-line `bl` (the majority) or inlined to
  exactly three stores (`mOwner@4`, `mObject@8=0`, vtable@0, no `AddRef`). Our
  two-arg ctor's `if (mObject) AddRef(this)` pushes it past `/Ob2` under `/O1`.
  It is therefore gated per-TU behind `RB3_OBJPTR_INLINE_OWNER_CTOR`; opt in only
  where measured neutral-or-better, never globally.
- **The temp-slot permutation wall** — see
  `docs/decomp/patterns/at-limit-systemic.md` §8. Check `set(target slots) ==
  set(base slots)` *before* investing in a "named parent, frame already correct"
  target; if the sets match, it is at-limit.

## Map defects surfaced while working this pool (for the map lane)

- `0x82577ab8` is mapped to `BandSongMgr::Handle` but is actually
  **`BandSongMgr::SyncSharedSongs()`** (167 instructions, local statics
  `real_guitar`/`real_bass`, matches rb3-Wii `BandSongMgr.cpp:914-958`).
  `SyncSharedSongs` has no map entry at all. This mispair is why
  `BandSongMgr::Handle` reads 2.5%.
- `FakeProfileFill@ProfileMgr` → retail `fn_827B7CA0` is a **destructor** that
  writes two vfptrs through a virtual-base table at `this-0x48` then calls
  `UIPanel::~UIPanel`. `ProfileMgr` is not `UIPanel`-derived and the VA sits in
  StorePanel territory (`0x827b6020`).

## Two hazards found while diagnosing this campaign's own regressions

Both surfaced as EH-funclet churn (40-byte funclets dropping 100 → 99.9), and
both are general — neither is specific to `MILO_WARN`.

### 1. When a macro takes over a behaviour, every prior hand-rolled emulation of
### that behaviour silently becomes a defect

`PrefabMgr.cpp` carried

```cpp
String warnCC(str); // retail: MILO_WARN copies the String vararg
MILO_WARN("Bad charcreator prefab name: (%s)\n", warnCC);
```

An earlier lane had correctly observed that retail's stripped WARN residue copies
its `String` argument, and hand-emulated that copy with an explicit local. That
was right at the time. Once `MILO_WARN` itself began copying (via `MiloStripEval`)
the site became a **double** copy — explicit temp *plus* by-value parameter — which
inflated the parent's frame and un-paired its five funclets.

**When you move a behaviour into a macro, grep for prior emulations of it.** Here
the tell was a comment naming the very behaviour the macro now provides. PrefabMgr
was the only such site; removing the three wrappers took the unit 58 → 63.

### 2. Comma form vs function call is an ARGUMENT-ORDER decision, not only a
### copying decision

```
MSVC evaluates FUNCTION ARGUMENTS  RIGHT-TO-LEFT.
A comma expression evaluates       LEFT-TO-RIGHT.
```

So any "stripped debug output" form that is a *function call* (e.g.
`MiloStripEval(...)`) silently reverses the order in which its argument
expressions run, relative to the comma form `((void)(a, b, c))`. Where those
expressions have side effects the target emits in source order, the body stops
matching — with no diff in the arguments themselves, only in call sequencing.

Control case, decisive: the `?SetType@*@@UAAXVSymbol@@@Z` family's stripped
residue calls `PathName(this)` **before** the `ClassName()` vcall, i.e.
left-to-right. `OBJ_SET_TYPE_ENGINE` (`Object.h`) spells it with `MILO_NOTIFY`
(comma form) and `RndGroup::SetType` held at 100% throughout; `OBJ_SET_TYPE`
(`ObjMacros.h`) spelled the same residue with `MILO_WARN`, and
`GamePanel::SetType` fell 100% → 96.2% the moment `MILO_WARN` became a call.
Same code, two macros, one broke.

**The two properties are independent, and a site needs whichever its arguments
actually depend on:**

| property | reproduced by | needed when |
|---|---|---|
| **copying** of class-typed args | `MiloStripEval` only | args are destructible class types (String temps → EH states → funclets) |
| **left-to-right ordering** | comma form only | args have side effects the target emits in source order |

A site needing *both* is expressible in neither form and would have to hoist its
temporaries into explicit locals in source order.

This also corrects the original reading of the `MILO_NOTIFY → MiloStripEval` A/B
(−20): that population is dominated by ordering-sensitive sites, **not** by sites
that "do not copy". We have no evidence either way on whether retail's NOTIFY
residue copies, and should not imply we do.
