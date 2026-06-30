# Topological Locator — design + verdict (2026-06-30)

Hard-frontier tooling: a BETTER scattered-TU IDENTIFIER for the class-B identification wall.
Judge-panel design (workflow hardfrontier-identifier-design). The panel PROTOTYPED the pilot;
numbers below are measured this session, not estimated.

## WINNER
Proposal 1 — Callee-Set Topological Locator (tools/topo_locate.py): pure confirmed-anchor call-bag intersection, no similarity diffusion.

## RANKING
- P1 — Callee-Set Topological Locator (call-bag intersection): the only proposal whose pilot I independently reproduced to the digit (28 game anchors with >=2 anchored callees, held-out precision@1 17/28=61%); zero ephemeral deps; smallest LOC; honest measured ceiling.
- P4 — ARFT (anchor-relative fingerprint triangulation): same anchor-resolved-callee core as P1 but adds a string/imm/size fusion layer and a proven 98.8% self-relocation harness; honestly flags Regime-LOCATE as the wall. Loses to P1 on the BSim/baseline_matches.json dependency it half-leans on and a slightly inflated edge-density claim, but its self-relocation harness and string fusion are the best grafts.
- P2 — SearchLocator (multi-signal fused search + learned weights): richest design and the most valuable *negative* deliverable (a calibrated P/R curve), but a 7-weight+2-threshold logistic on ~611 anchors overfits, and it hard-depends on /tmp/bsim_seed/baseline_matches.json which is ephemeral/uncommitted (51% VA coverage). Highest build cost (~600 LOC) for marginal EV over P1+P4.
- P3 — constloc (compiler-invariant constant fingerprint): the one genuinely orthogonal signal channel and it did the most rigorous self-diagnosis (proved fingerprints.json imms are relocation-poisoned, measured 8.0% binary-wide unique-content ceiling, ~30/53 SongSortNode rows const-poor). But it self-refutes: it concentrates its 472 unique signatures in the Quazal/network stub family, ~0 in class-B game code, and depends on a capstone re-extraction pipeline that doubles build cost. Best kept as a future opt-in fusion axis, not the keystone.

## SYNTHESIS


## BUILD PLAN
NEW TOOL: tools/topo_locate.py (~380 LOC, standalone, read-only, no Ghidra/BSim/objdiff in loop, runs in seconds). All inputs are COMMITTED on-disk assets (verified present this session).

INPUTS (all verified to exist):
- build/45410914/report.json (10.1MB) — matched@100 source. NOTE: match field is `match_percent_normalized` (NOT `match_percent`); the `address` field is a useless decimal relative offset — DO NOT use it. Recover VA from the function `name`: `fn_82XXXXXX` encodes the VA directly (6,509 anchors); mangled names resolve via target_symbol_map inversion (4,155 anchors). Total = 10,664 anchor VAs (verified).
- scripts/target_symbol_map.json (13,404 entries, VA->mangled-name; one `_denylist` list entry to skip). Invert to name->VA (uppercase hex, strip 0X/0x).
- fingerprints.json (61,618 fns; each: name,size,n_insns,n_callees,callees[],imms[],strings[]). callees[] are uppercase-hex VA strings (verified format '824FC6A0'); 92.1% resolve to known VAs.
- unified_id_rb3wii.json (9,301 rows: rb3_addr, wii_name 'Class::Method(args)', similarity, confidence). Yields 1,196 game-named confirmed anchors keyed by class::method (verified).
- ../rb3/build/SZBE69_B8/asm/<path>/<TU>.s — MWCC Wii asm; parse .fn boundaries + `bl <gcc2name>`.

KEY FUNCTIONS:
1. build_anchor_va_set() ~45 LOC: scan report.json matched@100; regex fn_(8[0-9A-Fa-f]{7}) -> VA; else invert target_symbol_map (name->VA) for mangled names. Returns set of 10,664 VAs. (Verified working this session — reproduce exactly.)
2. build_game_anchor_map() ~25 LOC: for each unified_id_rb3wii row with rb3_addr in anchor_va_set and '::' in wii_name, key A[class::method].add(VA). Returns 1,196-entry map. KEEP ICF alias class (one key -> multiple VAs).
3. build_retail_graph() ~30 LOC: from fingerprints.json build callers_of[calleeVA]={callerVAs} (reverse edges, 27,567 callee nodes verified) and va2cm[VA]=class::method (invert game_anchor_map). NOTE measured ceiling: only 3,089/10,664 anchors appear as a callee at all — this is the binding recall constraint, document it.
4. parse_wii_method_callees(tu) ~40 LOC: reuse fingerprint_match.py load_wii_bodies machinery; .fn regex r'^\.fn (.+?), (?:global|local)'; scan `bl ([A-Za-z_][\w$]*)`; drop STL/compiler stubs (_savegpr/_restgpr/_save|restfpr/__dt/__dl/__nw/_MemOrPool/stlpmtx/memcpy/memset). Return {wii_fn:[callee_names], insn_count, strings}.
5. demangle_gcc2_key(mangled) ~20 LOC: r'(.+?)__(\d+)(.+)' -> Method,len,rest; cls=rest[:len]; return cls+'::'+meth. (Verified: yields 34 keys on SongSortNode, 2 anchored.) Handle Q<n> nesting.
6. locate(tu) ~90 LOC: per wii method, resolve callees -> anchored groups (N=distinct class::method). REQUIRE N>=2. Vote: for each group, union ICF callers, +1 per caller VA. cand={VA:votes>=max(2,N-1)}. RANK by (votes desc, GRAFT-1: string_overlap(M.strings, fp[VA].strings) desc, |fp[VA].n_insns - M.insn_count| asc). Emit top-1 + vote_margin + has_string + size_band. SELF-CONSISTENCY GUARD: reject if VA is a confirmed anchor for a DIFFERENT class::method (collision), or fingerprints size<=44 (stub), or VA already pinned to a different confirmed method.
7. --validate (self-relocation harness, GRAFTED from P4) ~70 LOC: for each game anchor with N>=2, HIDE its VA, run locate, record top1==trueVA and trueVA-in-candset; bucket by (N, has_string, vote_margin) and emit per-bucket empirical precision@1 = the confidence calibration table.
8. --emit-gate ~20 LOC: write {VA:{class:'TOPO_LOCATED', confidence}} sidecar — EXACT format locator.py --emit-gate emits (verified: locator.py:592 writes {r['va']:{'class':...,'confidence':...}}).

PLUG-IN (precise, with the integration gap flagged):
- topo_locate runs FIRST (SEARCH). It writes the {VA:{class,confidence}} gate sidecar that locator.py already produces and that downstream reads. locator.py then runs as the per-VA GRADER on topo_locate's top-1 (its S-score cascade confirms-or-demotes). They COMPOSE: topo_locate = missing SEARCH stage, locator.py = existing GRADE stage. No edits to locator.py.
- INTEGRATION GAP (must build, ~25 LOC in topo_locate, NOT in P1's plan): identity_transfer.py's --pin-only flag (identity_transfer.py:355) takes a JSON VA-list and RESTRICTS which oracle methods get carved, but it still carves at the oracle's rb3_addr — it CANNOT substitute a corrected VA. So topo_locate must additionally emit a --pin-only JSON list of the SUBSET of methods whose topo-located VA == the oracle's rb3_addr (i.e. topology CONFIRMS the oracle) — those are safe to carve via the existing path. For methods where topo says a DIFFERENT VA than the oracle, emit them to a separate `relocated.json` worklist and STOP (carving a corrected VA needs a new identity_transfer code path — out of scope for v1; flag for owner). This keeps v1 strictly additive and honest.

Total NEW code: ~380 LOC topo_locate.py + 0 LOC locator.py + a documented (deferred) identity_transfer relocate path.

## PILOT PLAN
GROUND-TRUTH PILOT (held-out confirmed-anchor precision@1, byte-match GT — strictly stronger than the BinDiff oracle, per HARD CONSTRAINT 3):

STAGE 1 — SELF-RELOCATION (reproduce + extend, already prototyped this session):
- Test set = the 28 game anchor methods with N>=2 anchored callees (their TRUE retail VA is known because they are matched@100). HIDE each VA, run locate, measure top1==trueVA.
- ACHIEVED THIS SESSION (independent reproduction): 28/28 trueVA-in-candidate-set, 17/28=61% top1==trueVA. This matches P1's claimed 60% precision@1 on its 5-recall subset and validates the whole approach end-to-end before any new code is written.
- Build deliverable: re-run with the GRAFT-1 string+size tiebreak and report whether precision@1 lifts above 17/28 (the 11 tie cases are where the tiebreak can pay).
- Calibration: emit the per-bucket confidence table from --validate.

STAGE 2 — CLASS-B FLOOR TEST (the kill probe, already run this session):
- SongSortNode (canonical class-B, 0 strings): MEASURED 99 Wii fns, 34 distinct callee keys, only 2 anchored callees (BandSongMetadata::HasAlbumArt, IsMasterRecording) -> N<2 for essentially every method -> topo_locate correctly emits ZERO candidates. This is the expected honest floor and CONFIRMS the consolidated verdict's class-B-unrecoverable finding.
- Second class-B TU: BandProfile.cpp — run the same callee-anchor count; expectation N<2 throughout (kill-confirming). If BandProfile also yields ~0 anchored-callee methods, the class-B wall is doubly confirmed.

STAGE 3 — UNCONFIRMED HARVEST (the actual prize):
- Run locate over all band3+network Wii TUs whose methods are NOT already matched@100. P1 measured ~134 unconfirmed game methods with N>=2 (161 total minus 27 self-confirmed). Emit top-1 VAs passing the self-consistency guard.
- A located VA COUNTS as a real harvest candidate only if (a) not the trivially-confirmed self, AND (b) it survives the identity_transfer carve + ports to >=95% fuzzy via fuzzy_progress.py OR the BinDiff oracle (docs/decomp/gameid/crossval_agree.json) independently agrees. Run an agree/disagree pass vs crossval_agree.json on the located VAs.

ACCEPTANCE BAR: self-relocation precision@1 >= 0.55 on the N>=2 pool (MET: 17/28=0.61), zero self-consistency-guard collisions, and SongSortNode/BandProfile correctly emit ~0 candidates (the floor is honest, not a false-positive generator).

## HONEST EV
HONEST EV: +6 to +9 strict matches, realistic ceiling ~+15 located VAs before body-divergence attrition. This is a SLICE of the consolidated verdict's +30-80 total ceiling, harvested via a genuinely new signal direction (confirmed-anchor call-graph topology) — it OPERATIONALIZES a sub-vein the verdict flagged as 'needs a fundamentally better identifier,' it does NOT contradict the verdict.

GROUNDED ARITHMETIC (from numbers I measured this session, not the proposal's): ~134 unconfirmed game methods have N>=2 anchored callees. At the measured held-out precision@1 of 61% (17/28), ~134 x 0.18 measured-recall-given-N>=2 x 0.61 precision ~= the same ~15 correctly+uniquely located VAs P1 estimated. After the documented body-divergence attrition (~40-60% of correctly-located game methods port to 100%; the rest hit permuter/struct walls — the consolidated verdict's repeated finding), net +6 to +9 STRICT.

THE DOMINANT, MEASURED LIMIT is RECALL, not precision. Only 3,089/10,664 anchors appear as a callee at all, and only 28 game anchor methods binary-wide clear N>=2 in the tight oracle-keyed set (P1's 161 is the looser full-Wii-TU-parse count). The class-B panel bulk (SongSortNode: 2 anchored callees across 99 fns; BandProfile expected similar) is STRUCTURALLY INVISIBLE to topology because its callees are themselves unmatched scattered methods — the signal cannot bootstrap. So the prize is hard-capped at low double digits. The honest value beyond the +6-9 is a SAFE, ORACLE-INDEPENDENT CONFIRMER that grows the ~146-fn high-precision core (zero FPs at K=ALL) rather than degrading it — and a hard-numbers characterization that the class-B bulk remains un-locatable by topology, closing that sub-question definitively.

vs the verdict: this is NOT a doubling and does not reopen the 6-8k prize. It is a marginal, additive, low-EV-but-honest harvest plus a clean negative on class-B — exactly the kind of bounded win the verdict's FORWARD OPTIONS (1) contemplated.

## KILL CRITERIA
KILL CRITERION (declare the topology wall confirmed and STOP):

PRIMARY KILL — self-relocation precision@1 < 0.55 on the N>=2 held-out pool. (Currently 17/28 = 0.61, just above. If the GRAFT-1 tiebreak FAILS to hold it at >=0.55 across a re-run, or the pool shrinks below ~20 usable held-out methods making the estimate statistically meaningless, STOP — pure topology is not precise enough to carve VAs blind.)

SECONDARY KILL — harvest yield: if Stage-3 produces fewer than ~8 located VAs that pass BOTH the self-consistency guard AND independent confirmation (BinDiff crossval_agree.json agreement OR >=95% fuzzy port), then net strict after body-divergence attrition is < +5 and the tool is not worth maintaining as a harvest engine — bank it as a one-shot confirmer and STOP iterating.

CLASS-B KILL (already effectively MET, confirms the wall by design): if SongSortNode AND BandProfile each yield < 3 methods with N>=2 anchored callees (MEASURED: SongSortNode = ~0; only 2 anchored callees total across 99 fns), then class-B is DECLARED un-recoverable by topology and we do NOT attempt the panel bulk with this tool. This is a CONFIRMING negative, not a failure of the tool — it precisely characterizes the boundary the consolidated verdict asserted.

OVERALL STOP CONDITION: if PRIMARY kill fires (precision < 0.55) the entire approach is dead — do not graft P2/P3/P4 signals on top to rescue it, because the recall ceiling (28-161 methods binary-wide) means even perfect precision caps the prize in the low double digits, and the verdict already proved no current oracle cracks the string-poor class-B bulk. In that case the deliverable is the hard-numbers negative: 'confirmed-anchor call-graph topology re-finds only N of M held-out game anchors at precision P; class-B yields ~0; the scattered-TU identification wall stands.'