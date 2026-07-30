export const meta = {
  name: 'tooling-gameid-recon',
  description: 'Investigate the tooling levers (FRAME_ONLY funclet/naming cascade in jeff+objdiff) and game-ID approaches (Ghidra capabilities, BinDiff-vs-Rust-tool, spatial/contiguity pinning). Read-only analysis + prototypes + proposed patches; the orchestrator executes the shared-toolchain changes. NEVER modify ../jeff or ../objdiff source or commit anything.',
  phases: [
    { title: 'Tooling', detail: 'FRAME_ONLY quantify, objdiff bl-compare-by-address, jeff funclet emission' },
    { title: 'GameID',  detail: 'Ghidra caps, BinDiff-vs-Rust tool, spatial/contiguity pinning prototype' },
  ],
}

const REPO = '/home/free/code/milohax/rb3-xenon'

const SCHEMA = {
  type: 'object', additionalProperties: false,
  properties: {
    stream: { type: 'string' },
    verdict: { type: 'string' },                 // the bottom-line answer
    key_findings: { type: 'array', items: { type: 'string' } },
    proposed_action: { type: 'string' },         // concrete next step (patch location, tool design, etc.)
    est_yield: { type: 'string' },               // realistic matched-fn / coverage estimate, with caveats
    feasibility: { type: 'string' },             // easy | moderate | hard | infeasible + why
    artifacts: { type: 'array', items: { type: 'string' } },  // any files written to ~/tmp/recon/
    notes: { type: 'string' },
  },
  required: ['stream','verdict','key_findings','proposed_action','est_yield','feasibility','notes'],
}

phase('Tooling')

const CTX = `CONTEXT (verified by the orchestrator on main a178205, 6562 matched): retail RB3-360 is NOT an LTO/LTCG build (proven — TUs are contiguous in .text). The "FRAME_ONLY" near-miss bucket (~615 fns in true_progress.py, mostly ??_G scalar-deleting-destructor + ??_E vbase-dtor thunks at 99.9x% normalized) was hypothesized to be a single tooling bug worth +615 — but a probe of ??_GWaypoint (norm 99.45%, 80/80 bytes) showed the residual is a MIX, not one bug: [5] subi r31,r3,0x340 (target) vs 0xf0 (base) = a REAL this-adjust/vbase-offset LAYOUT delta; [8] bl fn_822C8B58 vs bl ??_DWaypoint and [12] bl fn_82797AA0 vs bl ??3Waypoint = NAME_RELOC (the target's anonymous bl targets ARE the class ~dtor / operator delete, just not named in scripts/target_symbol_map.json). Do NOT trust the +615; quantify the real sub-classes. Tools: tools/true_progress.py (writes /tmp/true_progress.json with per-fn bucket+counts), bin/objdiff-cli diff -p . '<sym>' -f json --include-instructions, build/45410914/report.json, build/45410914/asm/*.s. mkdir -p ~/tmp/recon and write any artifacts there. READ-ONLY: do not edit source, do not modify ../jeff or ../objdiff, do not build.`

const funcletQuantify = () => agent(`${CTX}

STREAM = 'funclet-quantify'. Rigorously classify the FRAME_ONLY bucket so we know the REAL recoverable yield per lever. Steps:
1. Run tools/true_progress.py --lo 90 --hi 100; load /tmp/true_progress.json; take all bucket=='FRAME_ONLY' rows (~615).
2. For a representative SAMPLE (>=40, spanning ??_G / ??_E / non-thunk), objdiff each (--include-instructions) and categorize EACH residual instruction:
   - NAME_RELOC-callee: target 'bl fn_<addr>' vs base 'bl <named>' where the target addr IS the correct callee (the named base fn corresponds to that retail address). CHECK that correspondence: does our base's named callee match retail's fn_<addr> (is fn_<addr> the ~dtor/operator-delete/base-dtor of this class)? If yes -> flippable purely by NAMING fn_<addr> in target_symbol_map (no code change) OR by an objdiff address-based bl comparison.
   - LAYOUT (subi this/r3/r31, N const delta; or member-offset delta) = real struct work, NOT tooling.
   - jeff-misnest (wrong/missing bytes, funclet bytes in wrong COMDAT, size mismatch target_size!=base_size).
   - other.
3. EXTRAPOLATE counts across the 615. Output: how many are PURE-NAME_RELOC (flippable by naming/objdiff-fix, the real tooling lever), how many LAYOUT, how many jeff-misnest, how many mixed/other. For the PURE-NAME_RELOC class, how many of the needed callees are ALREADY matched/identified (so namable now) vs not.
Return SCHEMA: verdict = the honest recoverable count per lever; proposed_action = which lever to build (target_symbol_map dtor/delete population sweep? objdiff bl-by-address? both?); est_yield with the breakdown. artifacts = ~/tmp/recon/funclet_classification.json.`,
  { label: 'funclet-quantify', phase: 'Tooling', schema: SCHEMA })

const objdiffBl = () => agent(`${CTX}

STREAM = 'objdiff-bl-compare'. Determine whether objdiff (../objdiff, Rust, freeqaz fork) scores a 'bl' to a DIFFERENTLY-NAMED but SAME-RESOLVED-ADDRESS target as a mismatch — and whether a normalization fix would flip the FRAME_ONLY NAME_RELOC cascade in one shot. Steps:
1. Read ../objdiff Rust source: find where instruction args / branch+call targets are compared and where normalized_match_percent is computed (objdiff-cli/src and the diff core crate). Identify how a 'bl <symbol>' relocation target is compared between target and base — by symbol NAME, by resolved ADDRESS, or by paired-symbol identity. Look at the existing reloc/normalization handling (the fork already did a funclet over-subscription fix 48a5255 — find it).
2. For the ??_GWaypoint case: is target's fn_822C8B58 PAIRED with base's ??_DWaypoint by objdiff (same logical function), and if so why does it still score diff_arg? Is it because target_symbol_map doesn't name fn_822C8B58 (so objdiff sees two different symbols), or a genuine normalization gap?
3. CONCLUDE: is the right fix (a) populate target_symbol_map (data, no code change) so the names match, or (b) a Rust change to objdiff to compare call/branch targets by resolved-address/paired-symbol rather than raw name? If (b), pinpoint the exact file:function and sketch the patch + its blast radius (could it create false matches?). Do NOT edit ../objdiff — propose only.
Return SCHEMA: verdict, proposed_action (exact file:fn + patch sketch OR 'data-only via target_symbol_map'), feasibility, est_yield.`,
  { label: 'objdiff-bl-compare', phase: 'Tooling', schema: SCHEMA })

const jeffFunclet = () => agent(`${CTX}

STREAM = 'jeff-funclet'. Investigate the project_jeff_asm_misnest claim: that ../jeff (dtk fork, Rust) emits mis-nested .fn/.endfn in xex-split asm so funclet bytes land in the wrong COMDAT / target obj, blocking matches. Steps:
1. Read ../jeff/src/cmd/xex.rs (and related split/asm-emit code): how does it determine function boundaries + funclet (.fn/.endfn) nesting when splitting the XEX into per-TU target objs? Is there a known mis-nesting where a funclet of fn A gets attributed to neighbor fn B or split into the wrong COMDAT?
2. EVIDENCE on the binary: for several FRAME_ONLY fns, compare target_size vs base_size (build/45410914/report.json) — a size mismatch or a target obj missing the funclet bytes indicates a jeff split bug, NOT just naming. Quantify how many FRAME_ONLY have size mismatches (= jeff-misnest candidates) vs size-equal (= naming/layout).
3. CONCLUDE: is jeff actually mis-emitting, or is the target obj correct and the issue is downstream (objdiff/naming/layout)? If jeff IS buggy, pinpoint the file:fn in ../jeff and sketch the fix + estimate affected fns. Do NOT edit ../jeff — propose only.
Return SCHEMA: verdict (jeff buggy? y/n + scope), proposed_action, feasibility, est_yield.`,
  { label: 'jeff-funclet', phase: 'Tooling', schema: SCHEMA })

phase('GameID')

const GCTX = `CONTEXT: band3 game code can't be pinned as TU clusters because (a) no leaked map (RB3 fns are anonymous fn_<addr>; dc3 HAS ham_xbox_r.map = name+addr+obj, which is why dc3 pins everything), (b) the only game oracle is rb3-Wii (cross-compiler MWCC-Wii vs MSVC-X360, dev-not-retail, divergent source) so BinDiff similarity is low/scattered, (c) retail STUBBED trivial game accessors into identical 32-byte coverage breadcrumbs which poison fingerprinting + alias oracle names onto scattered stubs. BUT no-LTO guarantees each game TU's fns ARE physically contiguous in .text (we pinned 25 game TUs like Gem.cpp/Player.cpp as clean blocks; 10 others e.g. GemManager failed — scattered/unfindable). Goal: find a reliable way to LOCATE game-TU boundaries to grow pinnable+matchable game coverage. Available: Ghidra source+build at /home/free/code/milohax/ghidra (VMX128 SLEIGH fork) + Ghidra MCP on port 8002 (tools/ghidra/*); BinDiff at /usr/bin/bindiff + /opt/bindiff, BinExport plugin /opt/bindiff/extra/ghidra/; ../objdiff + ../jeff are Rust (reusable crates); build/45410914/report.json, config/45410914/splits.txt (627 existing pins = anchors). READ-ONLY analysis + prototypes to ~/tmp/recon/. Do not modify shared tooling.`

const ghidraCaps = () => agent(`${GCTX}

STREAM = 'ghidra-caps'. Research what the Ghidra install (source at /home/free/code/milohax/ghidra + MCP port 8002) can do for GAME-TU identification that we're not yet using. Investigate: (1) Ghidra Function ID / FidDb (FLIRT-like library-function signatures) — could we build a FidDb from dc3's NAMED engine + rb3-Wii's named game and apply it to RB3 to ID functions? (2) callgraph propagation — Ghidra's call references let us say 'fn called only by identified GemManager fns is probably GemManager' (exploits contiguity). (3) decompiler-based structural matching (p-code / BSim — Ghidra's built-in similarity DB). (4) what the existing tools/ghidra/* sub-tools already expose vs what we'd add. Concretely recommend 1-2 Ghidra-driven techniques most likely to locate game-TU boundaries, with how to drive them (MCP vs headless script). Return SCHEMA: verdict (what Ghidra unlocks for game-ID), proposed_action (the technique + how to run it), feasibility, est_yield (how many game fns/TUs it might newly locate).`,
  { label: 'ghidra-caps', phase: 'GameID', schema: SCHEMA })

const bindiffVsRust = () => agent(`${GCTX}

STREAM = 'bindiff-vs-rust'. Recommend the best vehicle for GAME-TU boundary detection + identification. Compare: (A) cloning/modifying BinDiff — locate its source (is google/bindiff open source buildable? what's at /opt/bindiff?), assess whether we can modify its matching to be robust to the coverage-stub aliasing + cross-compiler divergence. (B) a from-scratch Rust tool reusing ../objdiff (instruction normalization, reloc handling) + ../jeff (XEX parse, symbol/section data) crates — design a matcher that: normalizes RB3 vs oracle functions (mask relocs/regs), filters out the identical coverage stubs (detect the 32-byte bit-set breadcrumb shape), uses content+callgraph+spatial-contiguity to assign TU membership and BRACKET contiguous TU spans. Which is the better path (effort vs robustness)? Sketch the Rust tool's architecture + which objdiff/jeff modules to reuse if (B). Return SCHEMA: verdict (A or B + why), proposed_action (concrete build plan), feasibility, est_yield.`,
  { label: 'bindiff-vs-rust', phase: 'GameID', schema: SCHEMA })

const spatialProto = () => agent(`${GCTX}

STREAM = 'spatial-pinning'. Design + PROTOTYPE (python, read-only, to ~/tmp/recon/) an algorithm that exploits no-LTO contiguity to locate game-TU boundaries WITHOUT a perfect per-fn map. Idea: the 627 existing pins partition .text into pinned blocks separated by UNPINNED GAPS; each gap is a run of contiguous fn_<addr>; a gap belongs (mostly) to one or a few TUs. For each large unpinned gap in game-address space: (1) gather string-content + callgraph edges for the fns in the gap (the unified_id_rb3wii.json oracle is NOT available: **DEAD DATA WARNING**: unified_id_rb3wii.json, dc3_oracle.json, unified_id*.json, global_fuzzy_pairs.json and tools/scope_data/uid_merge.json are TU0-era and INFORMATIONLESS (2-6% of their addresses are real .text function starts; an arbitrary address list scores ~2-3% by chance; an exhaustive search over every 4-byte shift in +/-0x20000 cannot lift them above single digits). Do NOT derive spans, pins, names or verdicts from them. The tools that read them now HARD-FAIL by design (tools/dead_index_guard.py) -- that is not a bug to work around, and you must NOT set RB3_ALLOW_DEAD_INDEX. Live sources: scripts/target_symbol_map.json (99.79%) and autoid.json (100%, regenerate with: python3 tools/fingerprint_match.py autoid). Verify anything by running the audit tool (tools/dead_index_guard.py --audit).); (2) detect+exclude coverage-stub fns (32-byte bit-set shape from the asm); (3) score the dominant TU(s) and propose a clean sub-span that is plausibly one TU, bracketed at fn boundaries. Prototype on 2-3 real game gaps; report: does a confident, contiguous, single-TU sub-span emerge that we could pin (even sourceless, to grow the denominator / enable later matching)? Quantify how many game TUs/fns this could newly pin. Be honest if the stub-scatter defeats it. Return SCHEMA: verdict (does spatial bracketing work for game?), proposed_action, feasibility, est_yield, artifacts (the prototype + its output).`,
  { label: 'spatial-pinning', phase: 'GameID', schema: SCHEMA })

const results = (await parallel([funcletQuantify, objdiffBl, jeffFunclet, ghidraCaps, bindiffVsRust, spatialProto])).filter(Boolean)
return { results }
