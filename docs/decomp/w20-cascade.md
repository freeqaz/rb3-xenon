# W20-CASCADE — the caller cascade, made mechanical

**2026-08-17.** Baseline verified in-worktree before any edit, reproducing the
brief **exactly** on the shipped `name_check` ruler: **44,488 fns /
3,750,264 B = 36.337430%**, honest 21,588, `total_code` 10,320,664,
`total_functions` 69,226.

**Shipped: a fixture-validated pricer and two quantified refusals. Δmetric 0 by
design** — no map edit was landed, because the tool priced both open candidates
at **+0 cascade** and said so before the edit was made. That is the tool
working, not the tool finding nothing.

---

## The deliverable: `tools/cascade_price.py`

Three lanes priced a map-name repair and all three undershot in the same
direction (W8 +24/+184, W9 ~0/+268, W17 +1,072/+1,652). "Remember to include the
call sites" has failed as a rule three times, so it is now enumerated.

**Known-answer test, passing in BOTH directions**, against
`docs/decomp/w17-cascade-fixture.json` (generated *from* commit `7e9c2d01`, not
transcribed — these are 200-character mangled names):

| row | want | got |
|---|---:|---:|
| `SupportChar@RndText` | ±212 | **±212** |
| `CharAdvance@RndFont` | ±76 | **±76** |
| `CharWidth@RndFont` | ±52 | **±52** |
| `_Rb_tree<String,DataNode>` ctor | ±240 | **±240** |
| **cascade total** | **±580** | **±580** |

The fixture auto-detects direction from the tree's own map state and **REFUSES**
on a tree in neither state rather than scoring something it cannot interpret.
The forward (production) direction was scored on a **genuinely reconstructed
pre-W17 tree**, not simulated: the map-only inverse was applied for real,
measured, and forward-priced from there.

---

## ★★★ The round trip refuted my own pre-registration, and that is the finding

Pre-registered before measuring: **graded −2,180 · `none` 0 · Δfns 0**.
Measured: **graded −2,976 · `none` −2,520 · Δfns −10**. All three misses have one
cause, and `tools/rbtree_attribute.py` splits the −2,976 into two channels that
share no mechanism (row-level net **−2,976 == headline −2,976, AGREE**):

| channel | rows | bytes | signature |
|---|---:|---:|---|
| **CASCADE** — call-site name charges | 4 | **−580** | row still present, `fuzzy` just below 100 (99.9167 / 99.9057 / 99.7368 / 99.6154) |
| **PAIRING** — row un-pairs entirely | 10 | **−2,396** | `fuzzy` **absent** from the report, not merely < 100 |

⇒ **The channel this lane was built for is 19.5% of the delta. The channel
nobody had named is 80.5%.** The ten rows vanished because the pinned unit's obj
cannot define the reverted spelling — so the measurement is an independent,
at-scale reproduction of **W9's −180 B failure mode**, which is the strongest
possible argument for gating on it mechanically.

Two corrections this forced into the tool:

* ⛔ **"a pure rename moves `none` by 0" is FALSE whenever the edit changes
  PAIRING.** A name added, removed, or re-homed is a pairing change, and `none`
  **sees pairing** even though it ignores relocation names. My −2,520 miss was
  exactly this, and `ab_measure`'s own control diagnosed it as `REAL_PAIRING`
  while I was still calling it a pure rename.
* ⛔ **The pricer got 5 rows' bytes RIGHT BY THE WRONG MECHANISM.** It scored
  them `FALLS` via `NEW_CHARGE`; they actually fell by un-pairing. Same sign,
  same magnitude, different cause — CLAUDE.md's "count right, cause wrong". The
  point estimate now covers **pure callers only**, which is precisely the
  quantity the fixture validates; edited rows are reported in a separate
  **PAIRING** section whose byte value is an explicit **bound, never a point
  estimate** (measured on this fixture: 2,520 B of candidates against 2,396 B
  actually recovered — loose by 124 B, exactly as labelled).

---

## ★★★ The caller-spelling SHAPE separates a wrong name from an ICF survivor

This is the part that generalises, and it is what killed both candidates.

An **ICF survivor's** name is arbitrary (W7's fixed-point trap), so every tree
that folded into that body reaches it and **our** side spells a *different*
per-tree name at each site. At most one spelling can ever match ⇒ **the cascade
is structurally ZERO** and no rename can collect it. A genuinely **wrong map
name** looks the opposite: the callers agree with each other and disagree only
with the map, so one repair clears every site.

So **concentration of the caller spellings is the map-independent test** — the
computed form of what W17 did by hand when the cascade broke the
`RndText::mMeshMap` tie.

---

## Both open leads priced, both REFUSED — with numbers

**`0x822dea78` `clear@map<Symbol,CharLipSync*>` → `clear@set<Symbol>`.**
W17 proved the name wrong (its `_M_erase` deallocates `0x14`; the declared map
needs `0x18`) and refused to ship it on judgement. Now quantified: **62 retail
call sites, 20 charged, SEVEN distinct our-side spellings** (`_Rb_tree<H,..>`
×7, `<H,pair<const H,M>>` ×6, `<PAVTrackWidget,..>` ×3, `<G,..>`, `<VSymbol,..>`
…) ⇒ **DISPERSED ⇒ cascade structurally +0**, against 80 B of `BLOCKED`
exposure. **W17's refusal was correct.** The remedy for a dispersed set is a
*proven* alias, never a rename — renaming to capture those sites is picking the
higher-scoring arbitrary name, i.e. metric fitting.

**`0x8233bea0` `map<int,Symbol>::_M_create_node` → `map<int,bool>`.**
W17 proved this assignment on node **size** and node **shape** and handed it on
as *"a contained, fully specified next lane"* needing a three-way carve.
**Priced: cascade +0** — its only caller is `fn_8233C2C8`, itself unpaired at
`fuzzy` 0 — against **84 B of `BLOCKED` exposure** (`Song.obj` cannot define the
replacement). ⇒ **The carve buys the pairing term alone and none of the cascade
that made W17 profitable.** Recorded so the next lane does not pay for the carve
to discover the cascade was empty.

---

## ⛔ The tool caught a bug in itself, in the most expensive place available

`base_obj_for_unit` resolved the splits heading `Song.cpp` to
**`SongSortBySong.obj`** via `endswith(stem + ".obj")` — CLAUDE.md's basename
hazard in a fresh costume, and it would have produced a confidently **wrong
`BLOCKED`/`OK` verdict on the one decision that can send a row to 0%
permanently**. Now keyed on `report.json`'s **own unit name** (an identity, not
a string guess), with a path-**component** fallback that returns `None` on
ambiguity rather than picking a candidate.

---

## `tools/arity_screen.py` — W18's "100% UNDEMANGLABLE" was an artifact, and total

`llvm-undname` echoes each input then writes its result, but the record
separator is a **blank line** and the error goes to **stderr** — so on stdout an
accepted name is a 2-line record and a **rejected name is a ONE-LINE record**.
The old parser dropped blank lines and paired the survivors two-at-a-time, so it
**desynced permanently at the first rejection**.

Measured over the 30,059 distinct names in `target_symbol_map.json`:

| | new parser | old parser |
|---|---:|---:|
| demangles OK | **27,931** | 1,063 |
| reads as `None` | 2,128 | 28,996 |
| entries whose "demangling" is **another input name** | 0 | **1,063** |

⇒ **every one of the old parser's 1,063 non-`None` answers was corrupt** — the
failure was total, not partial, and worse than W18 reported.

Fixed by parsing blank-line-delimited records, matched to inputs
**positionally** with the echo cross-checked; a desync now raises and falls back
to one-name-per-invocation with a warning, so it can never corrupt the mapping
silently. That guard immediately earned itself — the map contains a
whitespace-padded prose row, which `llvm-undname` echoes stripped.

**Regression fixture: `arity_screen.py demangle-selftest`.** It carries three
rejected names and **deliberately none of them last** (the bug is a desync; a
fixture whose only rejection sits at the end cannot expose it), and it runs the
**old** parser over the same input and **requires it to fail** — printing
`VACUOUS` and exiting 2 if it ever passes. A regression test both parsers
satisfy proves nothing.

---

## Deliberately NOT done

* **No map edit landed, and no alias added.** Both candidates priced to +0
  cascade; shipping either would have been a bet against the tool built to
  price it. Adding an alias lifts the score **by construction** and the `none`
  control cannot catch a fabrication.
* **No `src/**` touched**, so no native gate run was required.
* **The DATA reference graph is not covered.** Callers are found by scanning
  retail `.text` for `bl`; a vtable slot or function-pointer table is not a `bl`
  and is not enumerated. A clean run is a clearance for call sites, never for
  the whole reference graph.
* **The PAIRING channel is bounded, not predicted.** Whether a newly-pairable
  row reaches `fuzzy == 100` depends on the BODY, which cannot be diffed until
  the edit lands and the tree re-splits. Closing that would need a speculative
  re-split, which was not attempted.
* **W17's 35 `NO_NODE_FN` components remain untested**, as they were under W17's
  own instrument. Nothing here reaches them.
