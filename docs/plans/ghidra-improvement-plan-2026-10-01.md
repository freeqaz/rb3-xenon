# Ghidra / extension improvement plan for rb3-xenon decomp — ranked (2026-10-01)

Read-only review of `../ghidra` (fork), `../XEXLoaderWV`, `../pyghidra-mcp`, and
rb3-xenon's Ghidra tooling against the campaign's current hard frontier. Companion
to `ghidra-fork-cleanup-2026-10-01.md` (lanes A sync / B SLEIGH / C decomp-quality /
D XEX). Everything below is **beyond** those four lanes; where an item touches a
lane's territory it says so. No code was edited and no patch was produced.

Every number here was measured today on main `358101f81` unless a date is given;
re-measure before building on one.

## 0. What was measured (facts the ranking rests on)

| fact | measured value | how |
|---|---|---|
| Fork-only commits on `master` ahead of `nsa/master` | 21 (VMX128 SLEIGH series, MSVC switch fix ×2 + index-injection fix, Gekko variant, VT/BSim perf, README) | `git log nsa/master..master` |
| Lane worktrees live | `~/tmp/ghidra-laneC` (callfixups), `~/tmp/ghidra-sleigh`, `~/tmp/ghidra-sync` (NSA merge at `f0ece63c48`, 1,003 commits), `~/tmp/xexloader-laneD` | `git worktree list` |
| Xenon language cspec | `ppc_64_32.cspec` — the **generic** PPC32-on-64 ABI: no VMX registers in `unaffected`/`killedbycall`, only `cr4` of CR2–CR4 non-volatile, one callfixup (`__get_pc_thunk_lr`, GCC-only), no `this`/MSVC prototype | read the file |
| Ghidra TU5 name bank | **17,294** names, last synced **2026-07-18**; `scripts/target_symbol_map.json` now holds **31,367** rows ⇒ the bank is **~14,000 names (45%) stale** | `len(rb3_symbol_map.full.json)` vs map row count |
| Last commit touching `tools/ghidra/` or the RTTI probes | 2026-08-16 (test hardening) | `git log --since=2026-07-01` |
| `bctr` sites in dtk `.s` (keyed on instruction text, never the synthetic address column) | **648** non-`bctrl`: `lwz` 556 (the `lwz r11,0(r3); lwz r11,N(r11); mtctr; bctr` **virtual tail-call thunk** shape), **`lbzx` 52**, `lhzx` 21, `lwzx` 17, `lbz` 1 | Python over `build/45410914/asm/*.s`, 10-instruction window |
| Form the fork's switch tooling keys on | `RecoverMSVCSwitchTables.java`: **`lhzx` only** ("No MSVC switch pattern detected (no lhzx found)"); `PowerPCMSVCSwitchTest` fixture is synthetic `lhzx`; pyghidra-mcp `detect_switch_statements` documents **`lwzx` only** | grep |
| Real RB3 switch shape (MusicLibrary.s) | `cmplwi; bgt; lis/addi jumptable; lbzx r0,r12,r30; lis/addi lbl; add; mtctr; bctr` — **byte-offset table**, matches memory `project_vtable_citation_confusable_is_eh_map` ("208 tables, 0 with pointer shape") | read the listing |
| XEXLoaderWV `.pdata` handling | our `56bf704`: label only (`Function_XXXXXXXX`); upstream `13962de` (lane D is merging): adds `CreateFunctionCmd` at the first word; **the second `.pdata` word (function length / prolog length / EH flag) is never decoded** on either side | read `XEXHeader.ProcessPData` + the upstream diff |
| XEXLoaderWV imports | `__imp__` labels + thunk labels via `ImportRenamer`; thunk code bytes are **patched in the image** (`0x38 0x60 … 0x38 0x80`) before Ghidra sees them; no external function signatures | read `ProcessImportLibraries` |
| pyghidra-mcp tool surface | `decompile_function(_by_name_or_addr)`, `find_function(s)`, `list_xrefs`, `gen_callgraph`, `read_bytes`, `search_code`, `bulk_create_functions`, `apply_demangled_signatures` (skipped in RB3 "no map"), `apply_this_types`, `create_structures`, `set_function_prototype`, `annotate_merged_calls` (map-parser only), `annotate_ppc_decompilation` (cntlzw idiom only), `detect_switch_statements` | grep `def` in `tools.py`/`mcp_tools.py` |
| rb3-xenon consumers of Ghidra | `tools/ghidra/*` (decompile/search/xrefs/struct_check/batch_export/BSim-VT drivers), `scripts/rtti_probe.py` + `batch_rtti_probe.py` (RTTI type-descriptor read via decompile), `tools/locator.py`, `tools/struct_db.py import-ghidra`, skills `/ghidra-decompile /ghidra-search /ghidra-struct`, orchestrator `worktree_pool.py` (2 mentions) | grep |
| Load-bearing campaign instruments that do **not** use Ghidra | `tools/retail_rtti.py`, `tools/anon_candidate_scorer.py`, `tools/anon_proposal_adjudicate.py`, `tools/icf_pair_adjudicate.py`, `scripts/dump_vtable.py`, `scripts/harvest/pdata_parent_owner.py`, `tools/vtable_claim_audit.py` — all read retail PE / COFF / dtk `.s` directly | docstrings |
| Measured dead ends on the Ghidra side | VT-BSim seed propagation **NO-GO** (2026-06-21); BinDiff DC3→RB3 **SATURATED**, engine-naming only (r1 286 names, r2 263; 98.6% precision anchored); `bindiff_match.json` pipeline **ran three times** and was funded twice more by lanes that could not see the gitignored artifact (W3 retraction, 08-17) | memory + docs |

Two corrections to premises in the lane plan doc, both small:

- The lane doc says MSVC uses "16-bit relative offsets (`lhzx`)". On RB3 the **byte
  table (`lbzx`) is the majority form (52 vs 21)**. Lane C's secondary target
  ("unresolved `bctr` switch targets") should be measured per form; see P4.
- The lane doc counts "~966 commits" behind; lane A's merge commit records **1,003**.

## 1. Where Ghidra sits in the campaign today (why the ranking looks the way it does)

The campaign's frontier (per `hub_campaign.md`, the 09-30 breadth directive, and the
W17 queue) is, in byte-weighted order:

1. **Identification of anonymous `fn_` rows inside compiled units** (~1.2 MB at
   fuzzy 0; 97% anonymous). The working method is byte-level and Ghidra-free:
   `anon_candidate_scorer` (COFF scratch rename) → `anon_proposal_adjudicate`
   (strings/floats/RTTI/binding table), plus W16-IB's **name-by-RTTI-vtable-slot +
   caller alignment**, plus the template-family method (fix the shared template,
   name instantiations by their own type descriptor).
2. **Drifted-body repair** (functions we define whose bodies diverged; W16-HK/HO/HN
   were the biggest breadth levers of 09-30 at +28–35 KB each). These lanes **read
   retail bodies by the dozen** — this is where decompiler quality is paid for.
3. **Wrong-callee adjudication by reading the callee's body** ("what global / API
   does `0x82510040` touch" — the SystemLanguage/SystemLocale reversal on 09-30 was a
   lane that trusted a map name instead of reading the body).
4. ICF fold adjudication (relocation-target identity), vtable slot count/order,
   EH-funclet parentage and the EH-state class, mis-pin adjudication.

So Ghidra is a **reading instrument**, not a scoring or naming instrument, and the
things that pay are: (a) make every retail body read *named, typed and correctly
bounded*, (b) stop the decompiler from lying in the specific ways MSVC X360 code
trips it (save/restore helpers → lane C; byte-table switches; EH prefixes and
funclets; VMX128 → lane B), and (c) do **not** re-fund similarity-based naming —
every Ghidra-side similarity lever (BinDiff, BSim, VT seed-prop) has a measured
negative or saturation verdict, and the naming A/B rule (hold-out precision control)
applies to anything that proposes names.

## 2. Ranked plan

Ranking key: expected payoff for the current frontier × cheapness × independence
from the four running lanes. Tier 1 items are worth a lane each; Tier 2 after lane C
lands; Tier 3 are probes or housekeeping.

### Tier 1

#### P1. Re-sync the TU5 Ghidra bank and make the sync part of landing — highest payoff per hour

**Evidence.** The bank carries 17,294 names; the map carries 31,367. Every lane that
decompiles a retail body today sees ~45% of the identified callees/vtables as
`FUN_82xxxxxx`/`fn_`, which is exactly the information the drifted-body and
wrong-callee lanes go to Ghidra for. The sync tooling exists and is idempotent
(`tools/ghidra/run_apply_symbols.sh --full`, `build_full_symbol_map.py`) and the
bank memo says to re-run it "after future identification landings" — nobody has
since 07-18 because nothing enforces it.

**Do.** (1) Run the full apply once (stops :8002, ~minutes). (2) Add a
**reconciliation mode** to `apply_symbols.py`: the map has had names *withdrawn and
moved* since July (W16-HB reverts, thunk re-homes, `~VarStack` withdrawal), and the
current apply only ever adds/renames forward — a stale name on a body the map no
longer claims is a wrong name the lane will read as fact. Diff bank-vs-map and
un-name/rename accordingly, print the delta. (3) Load `scripts/symbol_aliases.json`
(1,528 fold groups) as secondary labels on survivors so a folded callee shows all its
spellings. (4) Wire an incremental apply into the landing chain next to the alias
validator (it is cheap: only rows that changed), or at minimum into
`tools/prune_worktrees.py`-style housekeeping with a staleness banner in
`ghidra-status.py` ("bank N names / map M rows / last sync date").

**Control.** Before/after: pick 20 retail bodies from the 09-30 lane docs; count
unnamed call targets in the decompile. Expect a large drop; a non-drop means the
apply is not reaching the TU5 program (the duplicate-program trap in the bank memo).

**Deliberately not:** naming anything the map does not already name.

#### P2. Decode the X360 `.pdata` second word: exact function extents, EH-funclet classification, funclet→parent links

**Evidence.** Neither loader version reads the second `.pdata` word. The campaign
already proves that word is the authoritative extent oracle: jeff sizes functions
from it, ~24% of all retail functions are EH funclets (16,821), funclets encode their
parent's frame in their first instruction, and `scripts/harvest/pdata_parent_owner.py`
derives funclet→parent attribution from `.pdata` as the one *hard* (non-similarity)
signal. Ghidra currently discovers function bounds by flow, so EH prefixes (8 bytes:
`__CxxFrameHandler` + `__ehfuncinfo$`) and funclets land inside or between the wrong
functions and decompiles of EH-bearing bodies carry garbage at their edges.

**Do.** In the loader (post lane D's merge, as a separate analyzer so it can be
toggled): parse each `RUNTIME_FUNCTION` as big-endian `{BeginAddress, packed}`,
decode prolog length / function length / flag bits (verify the bit layout against
jeff's decoder — do not derive it from memory), create the function with an explicit
body `[begin, begin+len)`, mark the 8-byte EH prefix as data, tag funclets
(`.pdata`-sized, handler-flag set, parent found by `pdata_parent_owner`'s rule) with a
function tag and a comment naming the parent. Report count of functions whose
Ghidra body differs from the `.pdata` extent before/after.

**Payoff.** Every decompile is correctly bounded; funclets become readable as
funclets; the EH-state class (P8) becomes approachable; mis-pin adjudication in
Ghidra becomes trustworthy at block level. Lane D compares **counts**; this is
extents and classification, so coordinate but it is not the same work.

**Control.** `.pdata` entry count == functions created with explicit bodies; spot
check 10 funclets' first instruction `subi rX, r12, FRAME` against the parent's
`stwu r1,-FRAME(r1)`.

#### P3. A real `PowerPC:BE:64:Xenon` MSVC compiler spec (beyond lane C's callfixups)

**Evidence.** The language ships only the generic `ppc_64_32.cspec`. Lane C is adding
`__savegprlr`/`__restgprlr`/`__savefpr`/`__restfpr`/`__savevmx` callfixups to it. What
is still missing, and what the decompiler's parameter/return inference depends on:

- VMX non-volatile set (`v14–v31`, `v64–v127` — the existence of
  `__savevmx_14..31` / `__savevmx64_*` helpers is the retail witness) absent from
  `<unaffected>`; `v0–v13` volatile absent from `<killedbycall>`.
- `cr2`, `cr3` non-volatile (only `cr4` listed); `r2`, `r13` reserved.
- No MSVC prototype: `this` in `r3` (`__thiscall`), hidden struct-return pointer,
  `bool` return in the low byte of `r3`, float return in `f1`, **float arguments
  shadowing GPR slots** (believed: `Foo(int, float, int)` lands `r3, f1, r5` — verify
  on one compiled TU with `link /dump /disasm` before encoding; Ghidra models this
  the way `x86win` models its shared int/float slots).
- Stack argument home area / `extrapop` for the MSVC frame.

**Payoff.** Lanes currently establish signatures by hand ("the body returns in `f1`,
so it cannot be `bool`"; "incoming `r5` is never read before write"). With a correct
cspec the decompiler prints those facts in the prototype. This is also what makes P5
(apply mangled-name signatures) land cleanly instead of fighting the ABI.

**Control (free, and the reason this ranks above the others in Tier 2).** Mangled
names *encode the signature*. Build a 200-function corpus from
`target_symbol_map.json` rows at fuzzy 100, derive the expected parameter count and
float/int classes from the mangling, and score Ghidra's inferred prototype against
it generic-cspec vs new-cspec. That is a measured precision number, not a vibe; lane
C's 20–30-function baseline harness can be reused and widened.

#### P4. Switch recovery measured per form on real RB3 sites; add the `lbzx` byte-table form; classify the 556 vtable tail-call thunks

**Evidence.** The 648 `bctr` census above. The fork's recovery script and test only
know `lhzx`; `detect_switch_statements` only knows `lwzx`; the analyzer path
(`PowerPCAddressAnalyzer.recoverSwitches`, symbolic propagation) may or may not
follow a `lbzx` + `add` + `mtctr` chain — nobody has measured it on RB3. The rows that
matter most for breadth (`Handle`, `OnMsg`, `SyncProperty`, `Load` with revision
switches) are exactly the switch-heavy ones, and an unrecovered switch decompiles as
a `goto`/if ladder that hides the case structure a porting lane needs.

**Do.** (1) Harvest the 90 indexed-load `bctr` sites (52/21/17) from the `.s` files
as a fixture list with expected table base, code base and case count (the `.s` already
names `jumptable_*` and the `lbl_` code base). (2) Measure how many Ghidra resolves
(computed flow present). (3) Add the `lbzx` (and `lbz`) form to the analyzer and the
script, add a real-pattern unit test harvested from RB3 bytes instead of the
synthetic one, re-measure. (4) Separately, recognise the `lwz rN,0(r3); lwz rN,K(rN);
mtctr; bctr` shape as a **virtual tail-call thunk of slot K/4** and label it
(`vcall_slot_N_thunk`) with a call-return flow override — these 556 sites are the
fan-in objects the naming-by-vtable-slot method (W16-IB) reasons about, and today
they decompile as opaque indirect jumps.

**Overlap.** Lane C lists `bctr` switches as a secondary "if time remains". Hand it
the fixture list from step (1) if it gets there first; otherwise this is its own lane.

### Tier 2 (after lane C lands, or in parallel with P3)

#### P5. Apply mangled-name prototypes and compiler-verified class layouts to the Ghidra program

**Evidence.** `apply_demangled_signatures` exists in pyghidra-mcp and is skipped in
RB3 because the seed pipeline assumed a leaked map; we now have 31k mangled names.
`scripts/harvest/class_layout_report.py` gives **compiler-authoritative** layouts
(`/d1reportSingleClassLayout`), which the campaign treats as ground truth over
header comments; Ghidra's DTM today is seeded from `struct_db.sqlite`, i.e. from the
comments the campaign has shown to be sibling-decomp artifacts (91.3% copied from
DC3/Wii).

**Do.** After P3: set prototypes from mangling for every named function
(`set_function_prototype` / `apply_demangled_signatures`), then replace the DTM seed
with compiler layouts for every class the report can emit, and `apply_this_types`.
Decompiles then print `this->mOwner` instead of `*(int *)(param_1 + 0x14)` — the
single biggest readability gain for the drifted-body lanes after naming.

**Control.** `struct_check.py` becomes a *Ghidra-vs-compiler* comparison instead of
Ghidra-vs-comments; it should report zero disagreements on seeded classes by
construction, and its disagreements on *inferred* classes are then real leads.

**Deliberately not:** using Ghidra's inferred layouts as layout truth anywhere in
the match pipeline — the compiler is the oracle, Ghidra consumes it.

#### P6. ICF fold awareness inside Ghidra

**Evidence.** `annotate_merged_calls` only understands the DC3 map-parser shape; RB3's
fold knowledge lives in `scripts/symbol_aliases.json` (1,528 groups, 818 KB of
forgiveness) and `scripts/icf_alias_groups.json`. A lane reading a folded survivor in
Ghidra sees one arbitrary name and cannot tell it is a fold — the same conflation the
CLAUDE.md `AT_LIMIT` warning describes, reproduced in the reading instrument.

**Do.** Feed alias groups to the bank apply (part of P1 step 3) and to
`annotate_merged_calls`; add a `fold_group:<survivor>` tag; in decompile output
render `survivor /* fold: A, B, C */`. Small, mostly glue.

#### P7. X360-MSVC RTTI/vtable analyzer (port of `tools/retail_rtti.py` into Ghidra)

**Evidence.** Ghidra's RTTI analyzer is x86-MSVC; RB3's COL layout is non-standard
(three zero dwords, `pTypeDescriptor` at +0x0C — `docs/decomp/rtti-vtable-transitivity.md`).
`retail_rtti.py` already resolves 1,321 vtables and class hierarchies outside Ghidra,
and the campaign's rule is "do not rebuild the retail RTTI resolver" (three lanes
did, two got the address arithmetic wrong).

**Do.** Port, do not re-derive: a Ghidra analyzer that *imports* `retail_rtti.py`'s
output (vtable VA → class, slot → function, hierarchy) and creates vtable data,
`??_7X@@6B@` labels, class datatypes with vptr, and `[thunk]` adjustor labels.
Medium payoff: it makes W16-IB's slot-naming method readable in Ghidra and lets
`list_xrefs` on a vtable slot answer "who overrides this" — but the Python tool
already answers the same questions for the lanes that need them, so this is
convenience, not capability.

#### P8. EH model: `FuncInfo` / IP-to-state map parsing

**Evidence.** The one structure that *is* confusable with a vtable on this target is
the EH IP-to-state map (`{pc, state}[]`, interior pointers); W12-A found the
surplus-EH-state class (54 rows) is explained exactly by `FuncInfo.maxState`. Nothing
in Ghidra today parses `__ehfuncinfo$` or marks the 8-byte EH prefix.

**Do.** After P2: parse the prefix, `FuncInfo`, unwind map and IP-to-state map; mark
them as data with structures; comment each function with its `maxState` and unwind
entry count. Lets the EH-frame class be read in one place instead of hand-decoding
`.rdata`. Diagnostic value, bounded (54 rows), so Tier 2.

#### P9. VMX128 decompiler readability (follow-on to lane B)

Lane B is auditing semantics. The remaining item from `docs/vmx128/PHASE4_TODO.md`
is readability: named pcodeops per intrinsic (`vmaddfp128` → `XMVectorMultiplyAdd`
style) and `XMVECTOR` typing so vector-heavy `rndobj`/`char` math decompiles as
intrinsic calls rather than 128-bit arithmetic soup. Payoff is lower for RB3 than DC3
(no gesture/Kinect code), so it waits on B's measured agreement numbers.

#### P10. Keep pyghidra-mcp's `detect_switch_statements` and `pcode_inspect.py` in step with P4

Both document only one load form. Also port DC3's `tools/ghidra/pcode_export.py`
(real HIGH/RAW P-code via in-process pyghidra) — rb3-xenon's `pcode_inspect.py` never
touches P-code, only decompiled C plus hand-decoded bytes (DC3 renamed it
`switch_cast_inspect.py` for that reason). Small.

### Tier 3 (probes, housekeeping, do-not-fund)

- **P11. Cheap probe, not a lane: re-run the anchored BinDiff pass with the 31k-row
  map.** Round 2 (07-24) used 13,598 anchors at 98.6% precision and was engine-only;
  anchors have more than doubled. The W3 retraction stands (decoy null p95 = 1.000,
  no similarity threshold exists *unanchored*; the fundable residue was 234 rows then).
  So: one ~4-minute re-run, hold-out precision control, report the residue size. If
  it is still ~20 KB, close it for good in `tools/BINDIFF_MATCH_POINTER.md`.
- **P12. Import our own COFF objects into Ghidra for side-by-side decompile.** Our
  objs are machine `0x01F2`, which Ghidra's COFF loader rejects (the same magic
  `build_symbol_map.py` had to hand-parse). A small loader patch would let a
  drifted-body lane decompile *our* function next to retail's. The unicorn harness
  already gives a behavioural diff, so this is a reading convenience; low.
- **P13. Post-lane-A operations.** Rebuild the deployed `build/ghidra` from the merged
  tree (staged, byte-gated like wibo — never in place while :8002/:8003 run), rebuild
  XEXLoaderWV and BinExport against it, upgrade the existing `RB3Xenon` project in
  place (do **not** re-import — the bank and the TU0 reference live there), then run
  P1. Record SLEIGH version bumps: a changed `ppc_64_xenon.sla` re-disassembles the
  program on open.
- **P14. Housekeeping in the fork.** `ApplyMapSymbols.java` assumes a linker map RB3
  does not have — either retarget it at `target_symbol_map.json` or note it DC3-only;
  replace the synthetic `PowerPCMSVCSwitchTest` fixture with harvested RB3 bytes (P4);
  add the fork's README section describing which branch is deployed
  (`bsim-xenon-patches` is checked out in the main tree; the deployed install lags it
  by two BSim commits per the lane doc).
- **Do not fund:** VT-BSim seed propagation (measured NO-GO 06-21, degrades
  precision); any Ghidra-similarity naming without a hold-out precision control;
  Ghidra-inferred struct layouts as layout truth; Ghidra-guided permuter work (the
  permuter is OFF by directive).

## 3. Summary table

| rank | item | expected payoff | cost | depends on |
|---|---|---|---|---|
| P1 | Re-sync + reconcile + auto-sync the TU5 name bank (14k names stale) | every retail body read today is 45% unnamed; fixes that | hours | nothing |
| P2 | `.pdata` second word → exact extents, funclet tags, parent links | correct function bounds on ~24% funclets + all EH functions | 1 lane | lane D merge (sequencing only) |
| P3 | Real Xenon-MSVC cspec: VMX/CR non-volatiles, `this`, float/GPR shadowing, struct return; mangling-derived precision control | correct prototypes in every decompile; categorical witnesses for free | 1 lane | lane C's harness |
| P4 | Switch recovery per form (`lbzx` 52 / `lhzx` 21 / `lwzx` 17) + vtable tail-call thunk labelling (556) | readable `Handle`/`OnMsg`/`Load` switch bodies; fan-in objects named | 1 lane | coordinate with lane C secondary |
| P5 | Mangled-name prototypes + compiler layouts into DTM | `this->member` in decompiles | 1 lane | P3 |
| P6 | Fold-group labels from `symbol_aliases.json` | stops reading a fold survivor as a wrong callee | small | P1 |
| P7 | RTTI/vtable analyzer importing `retail_rtti.py` output | convenience for slot-naming method | medium | P2 |
| P8 | `FuncInfo` / IP-to-state parsing | EH-state class readable | medium | P2 |
| P9 | VMX128 intrinsic readability | lower for RB3 | medium | lane B |
| P10 | Sync switch detectors; port `pcode_export.py` | small | small | P4 |
| P11 | Anchored BinDiff re-run (probe) | probably ~0; closes a vein honestly | minutes | nothing |
| P12 | COFF `0x01F2` loader for our objs | reading convenience | small | nothing |
| P13 | Post-sync deploy/upgrade ops | enables everything above on 12.2 | hours | lane A |
| P14 | Fork housekeeping | hygiene | small | nothing |

## 4. What this review deliberately did not do

- Did not run Ghidra, touch any Ghidra project, or query :8002 — all Ghidra-side
  facts come from source, docs and git. The switch-form census is from dtk's `.s`
  output, not from Ghidra's analysis, so "how many of the 90 Ghidra resolves today"
  is **unmeasured** and is P4's first step.
- Did not verify the `.pdata` second-word bit layout or the float/GPR shadowing rule
  on retail bytes; both are flagged as verify-before-encoding inside P2 and P3.
- Did not read lane A/B/C/D's in-flight worktrees beyond `git log`/`status` (all three
  Ghidra worktrees were at their base commits at review time; the sync worktree holds
  the merge commit).
- Did not propose any naming or map change; every item here is a reading-instrument
  improvement, consistent with the 09-30 breadth directive that naming goes through
  the scorer/adjudicator with a measured precision control.
