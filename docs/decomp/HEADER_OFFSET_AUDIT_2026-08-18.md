# Tree-wide header offset audit — 2026-08-18

> ⛔ **STATUS (2026-09-01): SUPERSEDED — DO NOT TRIAGE THIS ARTIFACT.**
> `header_offset_audit_2026-08-18.json` was produced by a version of
> `audit_header()` carrying a **fourth** defect of the same family as the three
> in §2: a base class's member was allowed to shadow the derived class's own
> member of the same name, so the row was reported against the BASE offset —
> and `--fix-header` writes rows back. Found on dc3-decomp (`21db38da9`) while
> porting this tooling there, fixed here in `f68f5276`. Measured A/B over the
> identical cached layouts: **23 of this artifact's rows are that artifact**,
> **6 of them on comments that are CORRECT** (e.g.
> `SyncGameStartPanel::mState // 0x3c` would have been rewritten to the
> inherited `0x20`). Current sweep and triage:
> [HEADER_OFFSET_AUDIT_2026-09-01.md](HEADER_OFFSET_AUDIT_2026-09-01.md),
> raw findings `header_offset_audit_2026-09-01.json`.
>
> *Historical status (2026-08-18): CURRENT.* First systematic sweep of `// 0xHEX`
> header comments against the compiler. **1,170/1,170 TUs, 0 failed.** Raw
> findings: `header_offset_audit_2026-08-18.json`. Tool:
> `tools/header_offset_audit.py`.
> ⛔ **The headline number is a DISAGREEMENT count, not a defect count** — read
> §3 before acting on it.

## 1. Result

| | |
|---|---:|
| TUs audited | **1,170 / 1,170** (0 failed) |
| classes examined | 2,121 |
| **classes disagreeing** | **740 (34.9%)** |
| disagreeing rows | **5,307** |
| distinct headers | 612 |

**61% of disagreeing classes carry a UNIFORM shift** — `+4` ×119, `−4` ×97,
`+12` ×73, `−8` ×45, `+8` ×19, `−12` ×14 … — which is the signature of a
miscounted `{vfptr}` or base-class size, **not** scattered typos. 289 are MIXED.

Top headers by disagreeing rows: `beatmatch/SongParser.h` (103),
`hamobj/HamDirector.h` (80), `rndobj/Part.h` (65), `bandobj/GemTrackDir.h` (62),
`band3/game/Stats.h` (61), `hamobj/RhythmBattlePlayer.h` (57),
`band3/game/Game.h` (56).

## 2. ⛔ THE INSTRUMENT WAS MORE DANGEROUS THAN THE DEFECT — three fixes first

This sweep was only trustworthy after **three** defects in the audit path were
found *by running it*, each with controls (`daf354f1`, `e1137e69`):

1. **`audit_header()` compared WHOLE-FILE comments against ONE class.** On
   `src/system/utl/Str.h` (declares `FixedString`, `String`, `StackString`),
   line 68's `char *mStr; // 0x8` is **correct for `String`** and retail-verified
   in the surrounding block — but auditing `FixedString` (`mStr` at 0x0) flagged
   it wrong by −8.
   ⛔⛔ **And `--fix-header` writes those rows back**, so the bug would have
   **REWRITTEN A CORRECT, RETAIL-VERIFIED COMMENT TO A WRONG VALUE** — silent
   corruption of the ground truth this tool exists to protect, which also feeds
   `struct_db.sqlite` and the MCP `lookup_struct_offset`. Fixed by brace-scoping
   to the class's own body; if the body cannot be located we audit **nothing**
   (under-report rather than corrupt).
2. **`#ifdef HX_NATIVE` comments document a DIFFERENT build's layout.**
   `obj/Object.h:1886` has `ObjRef mRefs; // 0x4 (native)` inside `#ifdef
   HX_NATIVE`; the match build never defines it, so the compiler reports the
   `#else` layout (`mRefs` at 0x20 — stated correctly in prose three lines
   above). `audit_header` is now preprocessor-aware.
3. **The sweep lost everything to a kill.** The first run buffered findings and
   printed progress every 25 TUs; killed before the first checkpoint, it produced
   an **empty log despite real work**. Now appends per TU with `fsync` and
   **resumes** from the ledger. ⚠ The coordinator had briefed a set of subagents
   to checkpoint incrementally *earlier the same day* and then wrote a sweep that
   buffers — *the rule is not "tell others to checkpoint", it is "checkpoint".*

★ A fourth would-be "artifact" proved **genuine**, and is the instrument's best
validation: `rndobj/BoxMap.h:94` `mQueued_Point // 0x5644` against a real 0x644 —
a one-character typo — while the **neighbouring** `mQueued_Spot // 0xe18` was
correctly **not** flagged. The most implausible delta in the set discriminated
within its own three-line block.

## 3. ⛔ DISAGREEMENT ≠ WRONG COMMENT. Two possibilities per row.

The tool compares our comments against **our own compiler**. It therefore finds
*disagreement*, and **says nothing about which side is wrong**:

- **class A** — stale comment, layout right ⇒ comment-only fix, metric-neutral.
- **class B** — comment right (often copied from retail RE), **our layout wrong**
  ⇒ a **real bug**, needing an A/B and the native gate.

A uniform `−8` on `ThreeDSoundManager` is *either* a stale comment *or* our
layout being 8 bytes wrong. **Never let a B be filed as an A.**

### Mechanical discriminator, and its honest coverage

★ **A class whose members are read by a function scoring `fuzzy == 100` has a
provably correct layout** — any offset error changes the instruction encoding and
would break the match. Applied via the owning unit's report rows:

| bucket | classes | rows |
|---|---:|---:|
| **A — layout PROVEN** (owning unit has ≥1 `fuzzy==100` fn) | 175 | 1,082 |
| **NEEDS WITNESS** (owning unit has no 100% fn) | 3 | 47 |
| ⚠ **UN-TRIAGED** (header→unit not resolved) | **562** | **4,178** |

⛔ **The un-triaged bucket is 76% of the finding and is NOT a clean bill.** The
header→unit mapping is a *substring heuristic* over `report.json` unit names and
resolved only 178 of 740 classes. **Do not read "175 proven" as "the rest are
fine."** Closing that gap is the next step and needs a real header→unit map (the
same `basename()` hazard that broke four pinning lanes applies — key on FULL
PATH, and replicate `tools/project.py`'s own `objects()`).

> ✅ **RESOLVED 2026-08-19 — see `HEADER_OFFSET_TRIAGE_2026-08-19.md`.** The
> un-triaged bucket is **0**: A_PROVEN **551 / 4,017**, NO_WITNESS **32 / 237**,
> and a third bucket this table did not anticipate, UNADJUDICABLE **157 / 1,053**
> (no identified member function in the RB3 binary at all).
> ⚠ **The prescription above was itself unnecessary**: no header→unit map was
> needed. MSVC mangled names carry their own class qualifier, so an **exact
> name-keyed join** resolves all 740 classes with *no path reconstruction*, which
> is why the `basename()` hazard cannot apply.
> ⛔⛔ **AND §3's PREMISE IS SUBSTANTIALLY WRONG.** Class B assumed the comment
> "often copied from retail RE". Measured: **91.3% of disagreeing comments are
> byte-identical to dc3-decomp's or rb3-Wii's** — inherited verbatim from a
> sibling decomp of a **different build** (rb3-Wii is a different CPU, compiler
> and ABI). They are **provenance artifacts, not retail measurements**, so the
> compiler is the right side by default and class-B risk is far smaller than
> "740 classes disagree" implies.

**Class-A candidates worth taking first** (proven layout ⇒ the comment is wrong,
fix is metric-neutral by construction): `band3/game/Stats.h` (61),
`band3/game/Game.h` (56), `band3/meta_band/PatchPanel.h` (52),
`MusicLibrary.h` (46), `AccomplishmentProgress.h` (45), `bandtrack/Lyric.h` (27),
`rnddx9/Rnd.h` (24).

**Class-B candidates** (no 100% witness in the owning unit — our layout could be
the wrong side): `rndobj/PostProc.h` (42), `rndobj/PostProcMgr.h` (4),
`beatmatch/DrumMap.h` (1).

## 4. Not done

No comment was rewritten and no layout was changed by this lane. `--fix-header`
exists and is now safe to use *per class*, but running it across 5,307 rows
before the A/B triage is complete would bake the un-triaged bucket's ambiguity
into the tree — and on a class-B row it would "fix" a **correct** comment to
match a **wrong** layout, which is the same failure the §2.1 bug would have
caused, just with extra steps.
