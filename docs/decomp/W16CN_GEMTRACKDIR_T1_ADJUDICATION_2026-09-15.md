# W16-CN — GemTrackDir fold adjudication: discharging W16-BZ §5 item 1

**Date:** 2026-09-15 · **Tree:** `6e9caabe` (read-only; no build, no source edit)
**Instrument:** `tools/comdat_fold_gate.py`'s own `compare()`, called directly.

W16-BZ closed leaving one item explicitly for a coordinator: the 11 relocation-NAME
charges in `default/GemTrackDir`, worth **3,276 B**, which BZ asked to be settled by a
**T1** verdict — *retail bytes at the survivor address byte-identical to our compiled
body, modulo relocated fields, with relocation target names compared*. BZ recorded that
a failure would be "equally decisive and means our container types are wrong".

## Verdict

**The container types are NOT wrong.** Every pair the instrument could test passed T1.
**728 B is collectable; 2,548 B is blocked by two identified, non-type causes.**

| row | B | charges | our callee | T1 |
|---|---:|---:|---|---|
| `??1GemTrackDir@@UAA@XZ` | 728 | ×4 | `~vector<ObjPtr<RndPropAnim>>` | **PASS** |
| | | ×1 | `~vector<ObjPtr<EventTrigger>>` | **PASS** |
| | | ×2 | `~vector<pair<ObjPtr<EventTrigger>,ObjPtr<EventTrigger>>>` | **PASS** |
| `??0GemTrackDir@@QAA@XZ` | 2,548 | ×2 | `make_pair<ObjPtr<EventTrigger>,…>` | **FAIL — unresolvable** |
| | | ×2 | `push_back<vector<pair<ObjPtr<EventTrigger>,…>>>` | **FAIL — chained fold** |

`matched_code` is all-or-nothing per row, so the dtor's 3 pairs (7 charges) all passing
makes its **728 B collectable** — exactly BZ's prediction that "a T1 pass on the
`vector<…>::~vector` group alone collects 728 B". The ctor has **zero** passing pairs.

Each PASS reads: *identical: 32/34 words compared as FULL 32-bit values, 2 relocated
branch destinations resolved through the map and name-equal* (two of the three resolve
one destination via an already-installed alias group rather than a literal name match).

## Why the ctor is blocked — both causes are instrument/map limits, not defects

1. **`make_pair` — retail's branch at `0x4` targets `0x82829258`, which the map does not
   name.** This is the documented irreducible/NEEDS_MAP_ID class: the fold cannot be
   proven because the destination has no identity to compare. It is resolvable **only**
   by a map identification, the same shape MAPID-1 used at `0x827bcd38`. It is not
   evidence against the fold.
2. **`push_back` — retail calls `_Copy_Construct<SongPattern>`, we call
   `_Copy_Construct<pair<ObjPtr<EventTrigger>,…>>`.** A **chained fold**: this T1 can only
   pass once *that* pair is itself an established alias. Same template family as the
   `_Copy_Construct<EyeDesc>` admission already queued from the W16-CN gate repair.

⇒ Even if `_Copy_Construct` were installed, the ctor stays blocked on `make_pair`'s
unnamed destination. **The ctor's 2,548 B is not collectable by alias work alone.**

## ⚠ A false "ABSENT" this lane produced, and how it was caught

The first pass reported **2 of 5 of our spellings had no COMDAT in any compiled obj** —
a clean, decisive-looking finding. It was **wrong**. The mangled names had been
hand-reconstructed from JSON output that the printer had truncated at ~92 characters, so
the lookup key was a fabrication and the miss was guaranteed. A rescan keyed on exact
names pulled programmatically from the JSON found **5 of 5 present**, and a positive
control (6 defs of a known-present symbol) proved that scan non-vacuous.

This is the repo's standing hazard in a new costume: **a key you typed is not a key the
data contains**, and the resulting emptiness is shaped exactly like a result. Never
retype a mangled name — extract it.

A second, unrelated non-result: `comdat_fold_gate` **crashes** on `base_addr: null`
(`int() can't convert non-string with explicit base`, line 697). That is not a bug to
patch around. Its second gate adjudicates *retail's own definition of our spelling at
`addr(F)`*, which only exists for the 2026-08-12 worklist's pairs; our five spellings
carry no map address, so there is no contradiction to adjudicate and gate 1 (T1 body
identity) is the whole applicable test. Fabricating an address to satisfy the parser
would manufacture the evidence the gate exists to check.

## Action

**Install the 3 dtor memberships** (T1-proven, coordinator decision per BZ). Expected
yield **+728 B on one row**; `matched_functions` unchanged, since `mpn` is already 100 on
an arg-only row.

**Do not** fund the ctor as an alias target. Its only live lever is identifying
`0x82829258`, which per MAPID-1 pays in **bug exposure, not bytes**.
