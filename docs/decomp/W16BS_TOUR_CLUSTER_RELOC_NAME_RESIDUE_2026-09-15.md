# W16-BS — Tour-cluster relocation-name residue, adjudicated on retail bytes

**Lane:** W16-BS · **Date:** 2026-09-15 · **Branch:** `w16-bs`
**Ruler:** graded (`functionRelocDiffs=name_check`), read from `report.json`
`provenance.diff_config` — every figure below is on that ruler.

**Lane-internal measures** (same worktree, full builds, no cross-ruler mixing):

| measure | before | after | Δ |
|---|---:|---:|---:|
| `matched_functions` | 43,550 | 43,575 | **+25** |
| `matched_code` | 4,041,980 | 4,050,984 | **+9,004 B** |
| `matched_code_percent` | 39.445232 | 39.533104 | **+0.087872 pp** |
| `total_functions` | 69,240 | 69,240 | 0 |
| `total_code` | 10,247,068 | 10,247,068 | 0 |

`total_*` unchanged ⇒ no denominator movement; every byte is real crossing.

---

## 0. What this lane was asked to do, and what it actually found

The brief named five relocation-name sites across three rows (8,332 B) and asked
for each to be classified `WRONG_MAP_NAME` / `PROVEN_FOLD` / `SOURCE_DIVERGENCE`
/ `IRREDUCIBLE` on retail bytes. All five were adjudicated (§1). The residue that
survived that pass then turned out to be the more valuable half of the lane: a
**container-type source divergence in `Tour`** that the rb3-Wii oracle actively
contradicts and retail bytes settle three separate ways (§2), plus a
**720 B → 72 B correction to `Hmx::Object::SyncProperty`** (§3).

**Two of this lane's own hypotheses were refuted mid-stream and are recorded as
such** (§2.2, §4) — including one I had already written up as a finding.

---

## 1. The five briefed sites

Method for every row: decode the retail body on `orig/45410914/band.exe` bytes
(Python, PE sections, imagebase `0x82000000`), read the callee's *signature off
its call site* (what is in r3/r4, what the result is used as), and compare with
our compiled COMDAT extracted from the COFF symbol table.

| # | address | briefed name | verdict | real identity |
|---|---|---|---|---|
| 1 | `0x8235bbf8` | `??0Tour@@` | **WRONG_MAP_NAME** | `?InitializeTour@Tour@@` |
| 2 | `0x8235cd38` | `?GetConclusionText@Tour@@` | **WRONG_MAP_NAME** | `?GetTourGigGuideMap@Tour@@` |
| 3 | `0x8235c2e0` | Tour/Object `SyncProperty` | **PROVEN_FOLD + SOURCE_DIVERGENCE** | both (§3) |
| 4 | `0x823609f8` | `?GetCurrentQuestSuccessMessage@…` | **WRONG_MAP_NAME** | `?GetCurrentQuestDisplayName@…` |
| 5 | `0x82360fb0` | `?SetDancer@AppLabel@@` | **WRONG_MAP_NAME** | `?UpdateTourPlayerContributionLabel@TourPerformerImpl@@` |

Deciding bytes, briefly:

- **1.** `0x8235bbf8` takes `this` only (`mr r29,r3`; r4–r7 never read), stores no
  vptr, and reads members that already exist — not a constructor. The real
  `??0Tour@@` is at **`0x8235FD10`** (300 B): stores vtable `0x8203df04`, calls
  `??0Object@Hmx@@`, two container ctors, `??0TourWeightManager@@`,
  `SetName("tour")`, `SystemConfig`, `Init`, two `AddSink`.
- **2.** `0x8235cd38` calls `?GetGigGuideMap@TourDesc@@` and returns `Symbol("")`
  (`0x82000c55`) on the null path. The real `GetConclusionText` is at
  **`0x8235CDB8`** (120 B).
- **4.** Four 88-byte bodies (`0x823609f8/A50/AA8/BA0`) identical **but for the
  final `bl`** ⇒ they cannot fold (MSVC folds only COMDATs identical *including
  relocations*), so the four are four distinct functions and the map had them
  shifted by one. `GetCurrentQuestSuccessMessage` belongs at **`0x82360BA0`**.
  Its old **100.0 was a FALSE 100** — the differing callee was a placeholder
  name, which `name_check` forgives.
- **5.** `0x82360fb0` calls `??$SetTokenFmt@PAD@UILabel@@` on arg 2 and passes r5
  through to a helper reaching `GetPlayerContributionString@TrackerManager`.
  `AppLabel::SetDancer` **does not exist in this tree at all** — a DC3-map
  artifact with no home here.

Seven map edits landed in `9719b76b` (five renames + two previously-anonymous
addresses given the names that were displaced). Measured **+5,688 B / +2 fns**;
**0 rows fell out**; `total_*` unchanged. CROSSED IN: `?Handle@Tour@@` (3,492 B)
and `?Handle@TourPerformerImpl@@` (2,196 B).

⚠ **Pre-registered +5,812 B / +3 fns, measured +5,688 B / +2 fns.** The gap was
one row — `GetTourGigGuideMap` stalling at 99.8387 on a *further* relocation-name
charge at `0x8235cc80`. Chasing that gap is §2, and it was worth more than the
whole briefed set.

### 1.1 In-tree records that disagreed, and which one was right

`docs/decomp/sympair-adjudicated-W18.tsv` records `0x8235cd38` as
`A_PROVEN_FOLD STRONG 25`; `docs/decomp/SYMBOL_HEADS_2026-09-10.md` records the
same address as "UNDECIDED — unresolved reloc". Both were re-derived from bytes.
**W18 is refuted**: the address is a single unfolded function
(`GetTourGigGuideMap`), and the "fold" was the wrong map name being read as one.

---

## 2. The residue: `Tour`'s maps are `hash_map`, not `std::map`

### 2.1 The evidence (three independent reads, all agreeing)

**(a) A call-site signature read — the strongest single piece.** Retail
`?GetTourGigGuideMap@Tour@@` at `0x8235cd38`:

```
8235cd4c  mr    r31,r4        ; r31 = this (Tour)   [sret in r3, this in r4]
8235cd50  lwz   r4,52(r4)     ; r4 = this->0x34 = m_pTourProgress
8235cd58  cmplwi r4,0
8235cd5c  beq   -> return Symbol("")
8235cd60  addi  r3,r1,80
8235cd64  bl    0x82594038    ; Symbol tmp = m_pTourProgress->...()
8235cd68  mr    r3,r31        ; r3 = Tour this      <-- !!
8235cd6c  lwz   r4,80(r1)     ; r4 = that Symbol
8235cd70  bl    0x8235cc80    ; (Tour*, Symbol) -> ?
8235cd74  mr.   r4,r3         ; result, tested for NULL
8235cd80  bl    ?GetGigGuideMap@TourDesc@@   ; result used AS TourDesc* this
```

So `0x8235cc80` is called with a `Tour*` and a `Symbol`, returns a pointer, and
that pointer is used as a `TourDesc*`. **That call site is
`Tour::GetTourDesc(Symbol) -> TourDesc*`.** Its body is the **hashtable**
`_M_find` form reading `this+0x80`:

```
8235cc80  7d8802a6 9181fff8 9421ffa0 9081007c 38830080 38a1007c 38610050
          bl <??$_M_find@VSymbol@@@?$hashtable@U?$pair@$$CBVSymbol@@H@...>
          81630000 2b0b0000 419a000c 806b0008 48000008 38600000 ...
```

**(b) The constructor's container census.** Retail `??0Tour@@` at `0x8235fd10`
default-constructs `??0?$hash_map@VSymbol@@HU?$hash@VSymbol@@…@Z` **twice** and
constructs **no `_Rb_tree` at all**. `Tour` has exactly two map members
(`m_mapTourProperties`, `m_mapTourDesc`) ⇒ **both are `hash_map`.**

**(c) Our own COMDATs, after the correction.** `Tour::GetTourDesc` compiles to
72 B / 18 words, **byte-identical to retail `0x8235cc80` and to our
`AccomplishmentManager::GetAward`**, one relocation (type 6) at +28; the single
differing word is that relocated `bl`.

### 2.2 ⛔ The oracle says `std::map`, and the oracle is wrong here

`../rb3` (rb3-Wii) declares `std::map<Symbol, TourDesc *> m_mapTourDesc` and
`std::map<Symbol, TourProperty *> m_mapTourProperties`. Our source inherited
that. **rb3-Wii is the Wii *development* build; it is not this image**, and the
standing rule applies: *retail bytes outrank the oracle in every measured mode.*

**This refuted my own in-flight hypothesis twice**, and both refutations are the
useful part of the record:

1. I first concluded `hash_map` from a caller-population split at `0x8235cc80`,
   then **refuted it** when `?HasTourDesc@Tour@@` (`0x8258bab8`) decoded as an
   `_Rb_tree::_M_find`, and the oracle agreed with `std::map`. I recorded the
   refactor as dead.
2. The call-site read in (a) then **un-refuted it**, and located the real error:
   it is `0x8258bab8`'s *name* that is wrong (§2.3), not the container.

The generalisable lesson: **a caller-population split is not evidence of a fold**
— it is consistent with a fold *and* with one of the two names being wrong. Only
a call-site **signature** read separates them.

### 2.3 `?HasTourDesc@Tour@@` at `0x8258bab8` is a spurious map name

`0x8258bab8` sits **entirely inside a BandProfile cluster**, far outside
Tour.cpp's `0x8235b…`–`0x82360…` span:

```
8258B998  ?DeleteChar@BandProfile@@
8258BA50  ?DeleteSavedSetlist@BandProfile@@
8258BAB8  ?HasTourDesc@Tour@@            <-- claimed
8258BF60  ?HasSeenHint@BandProfile@@QBA_NVSymbol@@@Z
8258C118  ?GetAvailableStandins@BandProfile@@
```

Its immediate neighbour `?HasSeenHint@BandProfile@@` has the **identical**
`bool Has…(Symbol)` signature, and `0x8258bab8` is an `_Rb_tree` find at
`this+0x80` returning bool. A class cannot have its `GetTourDesc` read a
hashtable at `+0x80` while its `HasTourDesc` reads an `_Rb_tree` at `+0x80`, so
exactly one of the two names is wrong — and the call-site read in (a) is far
stronger than a name in a foreign cluster.

⚠ **Consequence, and it is the honest kind:** `?HasTourDesc@Tour@@` drops
**80.29** after the correction, because our `std::map` version had been matching
that mis-named `_Rb_tree` target row. **That high score was financed by a wrong
map name.** Re-homing/renaming `0x8258bab8` is deliberately **not** attempted
here — see §5.

### 2.4 The change, and why it is safe

- **Layout-neutral.** `cl /d1reportSingleClassLayoutTour` reports STLport
  `std::map` at **28 bytes** — identical to `hash_map`. `m_mapTourDesc` stays at
  `0x80`, `sizeof(Tour)` stays 164. Our compiled `GetTourDesc` already emitted
  `addi r31,r3,0x80` *before* the change, matching retail.
- **The insert idiom decompiles to `operator[]`.** Both sites used
  `lower_bound` + compare + hinted `insert` — the canonical MSVC expansion of
  `map::operator[]`. With `hash_map` they collapse back to `m[name] = p`, which
  is what retail emits.
- **Stale annotations corrected.** Every `// 0xHEX` comment in `Tour.h` after
  `0x38` was wrong (`0x50/0x5c/0x7c/0x94/0x98/0x99` vs real
  `0x54/0x60/0x80/0x9c/0xa0/0xa1`). The *code* was already right — a clean
  instance of the standing "ask the compiler, not the comments" rule.

Files: `src/band3/tour/Tour.{h,cpp}`, `src/band3/tour/TourDescPanel.cpp`,
`src/band3/tour/TourProgress.cpp`, `src/band3/tour/TourPropertyCollection.cpp`,
`src/band3/meta_band/AccomplishmentProgress.cpp`.

### 2.5 The alias, and why it is T1 and not a fabrication

Installed one group in `scripts/symbol_aliases.json`:

```
address  0x8235cc80
survivor ?GetAward@AccomplishmentManager@@QBAPAVAward@@VSymbol@@@Z
folded   ?GetTourDesc@Tour@@QBAPAVTourDesc@@VSymbol@@@Z
```

**Anti-vacuity evidence is retail's own bytes.** Retail's callee at `+28` inside
`0x8235cc80` is a **third** instantiation spelling —
`hashtable<pair<const Symbol,int>>::_M_find`, value type `int` — inside a body
that loads node+8 and returns it as a **pointer**. That is only possible if
`/OPT:ICF` folded the `_M_find` instantiations first (`TourDesc*`, `Award*`,
`int` all emit identical code; `find` never touches the value type), which made
the two callers relocation-identical and so folded them in turn.

This is the fold signature the house doctrine describes, observed directly
rather than inferred. It is **not** the "`none` unmoved ⇒ safe" reasoning, which
cannot validate an alias; and the patch is not map-only (it carries source), so
it is not in the `ALIAS_SUSPECT` shape either.

**Measured: `?GetTourGigGuideMap@Tour@@` 99.83871 → 100.0 (+124 B / +1 fn),
exactly as pre-registered**, plus a cascade of **+24 fns / +3,232 B** total as
the same forgiveness reaches the folded address's other call sites.

---

## 3. `Hmx::Object::SyncProperty` — 720 B of source that RB3 does not have

Retail `0x8235c2e0` is **72 bytes** — exactly what `BEGIN_PROPSYNCS` /
`END_PROPSYNCS` emit with **no** `SYNC_PROP` entries:

```cpp
if (_i == _prop->Size()) return true;
else { Symbol sym = _prop->Sym(_i); return false; }
```

Ours was **720 bytes**, carrying three DC3-era entries (`name`, `type`, `sinks`).
rb3-Wii's `BEGIN_PROPSYNCS(Hmx::Object)` is likewise **empty**, and the `"sinks"`
string has exactly **two** retail xrefs, both in `MsgSource` — nothing in
`Object`. The three entries are moved under `#ifdef HX_NATIVE`, which keeps the
native build (which relies on them) working while the match build gets the empty
terminal. After the change our `Object::SyncProperty` is **byte-identical to
retail `0x8235c2e0`**, all 18 words.

`Tour::SyncProperty` was hand-written as `if (_prop->Size() == _i)` — reversed —
emitting `cmpw cr6,r11,r6` where retail has `cmpw cr6,r6,r11`. That single word
is what stopped it folding. Routed through the macro, our `Object::SyncProperty`
and `Tour::SyncProperty` COMDATs are now **identical including relocation
names**, both byte-identical to retail `0x8235c2e0` — a complete T1 fold, which
is what ICF folded in retail. **+72 B.**

⚠ **The `Object::SyncProperty` half measured ≈0 on the metric, and that is
expected, not a disappointment:** `0x8235c2e0` is **unnamed** in
`scripts/target_symbol_map.json`, so target call sites to it render as the
placeholder `fn_8235C2E0`, which `name_check` already forgives. There is no
paired row to move. It is a **correctness** fix — precisely the class the
standing directive protects ("a metric that hides real bugs is worse than a
lower metric"), and it is the prerequisite for §5's naming option.

---

## 4. Refuted, and left refuted

- **`0x8235cc80` is *not* provable by "52 callers split into two populations".**
  That was this lane's first argument and it is not sound (§2.2).
- **My claim that our `Tour::GetTourDesc` (`std::map`, `_Rb_tree::_M_find`,
  76 B) was correct** — refuted by the call-site read; it is now 72 B.
- **The `// 0x7c` annotation on `m_mapTourDesc`** — refuted by the compiler; the
  real offset is `0x80`, which our code already used.
- **`docs/decomp/sympair-adjudicated-W18.tsv`'s `A_PROVEN_FOLD` at
  `0x8235cd38`** — refuted on bytes (§1.1).

---

## 5. What is left, named exactly

Per the brief's §6 — each residual named by site, by the two names in tension,
and by why it is undecided *here*.

1. **`0x8258bab8` — `?HasTourDesc@Tour@@` vs a `BandProfile` `bool Has…(Symbol)`.**
   Established: the body is an `_Rb_tree` find at `+0x80` returning bool, inside
   a BandProfile cluster, and it cannot be `Tour::HasTourDesc` given §2. Not
   established: **which** BandProfile member it is. Renaming is deliberately not
   attempted — the standing rule is that proving a name wrong does **not** make
   renaming safe, because the base obj may not define the replacement and the
   row would then read 0% permanently. **Evidence that would settle it:** decode
   `BandProfile`'s `bool Has*(Symbol)` members and match the `+0x80` member
   offset against `cl /d1reportSingleClassLayoutBandProfile`.
   Current cost: `?HasTourDesc@Tour@@` sits at 80.29 (68 B).

2. **Naming `0x8235c2e0`.** Now that our `Object::SyncProperty` is byte-identical
   to it, the address could be named
   `?SyncProperty@Object@Hmx@@UAA_NAAVDataNode@@PAVDataArray@@HW4PropOp@@@Z`,
   converting a forgiven placeholder into a checked, pairable 72 B row.
   **Not done here, because the economics are a genuine bet**: retail's `Tour`
   vtable slot `0x8203df20` also points at `0x8235c2e0`, so naming it converts
   the ~18 `parent::SyncProperty` call sites from *forgiven* to *checked* (they
   would match), but the vtable site would then need the `Tour::SyncProperty`
   alias — which §3 has now made T1-provable. Sizing this needs an A/B, not an
   assertion.

3. **Rows the renames exposed, now honestly scored and all source work:**
   `?InitializeTour@Tour@@` 83.31 (360 B ours vs 284 retail),
   `??0Tour@@` 0.0 (660 B ours vs 300 retail),
   `?GetCurrentQuestDisplayName@…` (drop the extra `if (!pQuest) return quest;`
   — retail has no such check; 112 B vs 88),
   `?UpdateTourPlayerContributionLabel@…` (132 B vs 112; retail uses
   `SetTokenFmt<char*>`, we use `<const char*>`),
   `?GetConclusionText@Tour@@` 95.33 (124 B vs 120).

---

## 6. Provenance

Retail decode: `orig/45410914/band.exe`, PE sections, imagebase `0x82000000`,
big-endian PPC, sizes from `.pdata`. COMDAT extraction: COFF symbol table +
section relocations. Layouts: `scripts/harvest/class_layout_report.py`
(`cl /d1reportSingleClassLayout`), never the `// 0xHEX` comments. All measures
from `build/45410914/report.json` after a full `./tools/ninja-locked` — never a
single-`.obj` build, which skips the six obj patchers and is part of the ruler.
