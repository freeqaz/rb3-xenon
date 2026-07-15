# Ghidra TU0→TU5 cross-port — leveraging the "banks" model for RB3-Xenon

> **STATUS (2026-07-15): the two-program "bank" is LANDED and live on :8002.**
> Both programs are co-resident in the `RB3Xenon` project and cross-queryable by
> `binary_name`:
> - **TU0** `default.xex-35adb6` — now **fully named** (~15.3k symbols applied from
>   the whole oracle universe, up from ~1,139; `run_apply_symbols.sh --full`).
> - **TU5** `default_tu5.xex-c5a170` — imported from the durable
>   `orig/45410914/default_tu5.xex` (sha `c5a17091`, the exact skeleton-map source),
>   loaded natively as **`PowerPC:BE:64:Xenon`**, `.text` @ `0x82270000` (VA-aligned
>   with `base_to_tu5_map`), full analysis complete, 57,733 `.pdata` fn seeds.
>
> **How it was made servable (reproducible):**
> 1. Built **XEXLoaderWV for Ghidra 12.2** (the fork had none) — source at
>    `../XEXLoaderWV`, patched for 12.2 (Jython `Log`→`Msg` shim; **preferred
>    LoadSpec = `PowerPC:BE:64:Xenon`**). Installed into the fork's `Ghidra/Extensions/`.
> 2. `tools/ghidra/pyghidra-service.sh` now serves **both** XEX paths; pyghidra-mcp
>    forces Xenon for XEX2 files and imports+analyzes+indexes TU5 on start.
> 3. `tools/ghidra/run_apply_symbols.sh --full` (+ `build_full_symbol_map.py`) names TU0.
>
> Validation: `MasterAudio::IsLoaded` base `0x82756d98` ≡ TU5 `0x8277b6e8` decompile
> to identical bodies. The name/type **port itself** (skeleton map / VT) is the
> next-phase work below — TU5 functions are currently `FUN_` (fresh analysis).
> Original investigation notes (still accurate for the pipeline) follow.

**Date:** 2026-07-07 · **Status:** investigation / recommended pipeline (READ-ONLY;
no programs, decomp.db, or configs were mutated). A concurrent workflow is
ingesting TU5 in worktree `.claude/worktrees/tu5-migrate` — this doc REUSES its
extracted TU5 PE and its skeleton map; it does not re-ingest or touch the port-8002
base program.

TL;DR — Two builds of one game (TU0 v0.0.0.1, the decomp target ↔ TU5 v0.0.5.1)
is the exact shape of the Wii `bank8_target ↔ band_r_wii` problem: port
names/types across a same-source rebuild. The **skeleton/fingerprint map already
built by the concurrent workflow is the primary name-port vehicle** (96.4% of
named functions, byte-exact-verified). **Ghidra Version Tracking (headless
`AutoVersionTrackingScript.java`, on the VT-perf fork) is the recommended
structural cross-check + residue-closer** for the ~478 changed-set functions, and
gives a call-graph-aware match the linear co-walk can't. This is complementary,
not either/or.

---

## 1. Is our retail decomp data actually IN the Ghidra base program? — PARTIALLY

**Yes, names are applied (USER_DEFINED), but only a ~1,139-function subset — NOT
the full 13,846 named universe.** This is the key prerequisite gap.

Verified against the live port-8002 base program (`/default.xex-35adb6`, TU0):

| Sample symbol | Ghidra state |
|---|---|
| `?IsActive@OvershellPartSelectProvider@@UBA_NH@Z` @ `0x8264b5f8` | **named, USER_DEFINED** |
| `?SetupTrackChannel@MasterAudio@@…` @ `0x82759a78` | **named, USER_DEFINED** |
| `?StaticClassName@UIComponent@@…` (+ many) | **named, USER_DEFINED** |
| 18 / 20 *random* `target_symbol_map.json` addresses | **unnamed** (`Function_82xxxxxx`) |

Random-address spot-check: only ~10% of the 13,846 named addresses are actually
named in Ghidra — consistent with the applied set being `rb3_symbol_map.json`
(**1,139 entries**), a strict subset (1,129/1,139 ⊂ target_symbol_map).

### How names get in (the applier chain)

- `tools/ghidra/build_symbol_map.py` — resolves matched/pinned decomp functions to
  absolute XEX VAs (from `build/45410914/asm/*.s` `# .text:0x… | 0xABS | size`
  comments + COMDAT size/reloc disambiguation) and emits
  `tools/ghidra/rb3_symbol_map.json` = `{ "0x82…": {symbol, demangled, percent,
  unit, method} }`. **Gated by `--min-percent` (default 80)** → that is why only
  ~1,139 land: it only names *matched-decomp* functions, not the whole named
  universe.
- `tools/ghidra/apply_symbols.py` — standalone PyGhidra (JPype) headless; opens the
  project exclusively, renames `fn_<addr>`/`FUN_<addr>` → mangled symbol,
  `SourceType.USER_DEFINED`, idempotent, re-runnable after re-import.
- `tools/ghidra/run_apply_symbols.sh` — orchestrates: (re)gen map → **stop the
  :8002 service** (exclusive lock) → `analyzeHeadless … -postScript
  apply_symbols.py -noanalysis` → restart service.
- `tools/ghidra/ImportMapFile.java` — REFERENCE ONLY (DC3's leaked-`.map`
  workflow; RB3 has no leaked map). Not the RB3-Xenon path.

### Prerequisite fix (pipeline step a)

Before using Ghidra as the base "bank", **fully name the base program from the
13,846-entry `scripts/target_symbol_map.json`**, not just the 1,139 high-%-match
subset. Two ways:

1. Run `build_symbol_map.py --min-percent 0` (widens to every resolvable target
   symbol) then `run_apply_symbols.sh --no-regen`; **or**
2. Add a thin adapter that feeds `scripts/target_symbol_map.json`
   (`{VA: mangled}`) straight into `apply_symbols.py` (it currently expects the
   `{VA:{symbol,…}}` shape — a 5-line format shim). `target_symbol_map.json` is
   the authoritative 1:1 VA→mangled table (13,846), so this names the base program
   to full decomp coverage in one headless pass.

**Note on necessity:** for the *pure* name-port, Ghidra does NOT strictly need the
base named first — the skeleton map (§4) carries `(base_va, symbol)` from
`target_symbol_map.json` directly and applies to TU5. Naming the base program
fully matters for the **Ghidra-VT structural route** (VT propagates *markup* from a
named source program) and to make the base program a genuine `bank8`-analog
reference for future work. Do step (a) if you run the VT route; skip it if you only
run the skeleton map.

---

## 2. The banks / two-program setup for TU0 + TU5

Mirror the Wii server that holds `bank8_target` + `band_r_wii` in one instance.
Here: **port-8002 already holds TU0** (`/default.xex-35adb6`); **add TU5 as a
second program in the same RB3Xenon Ghidra project.**

### Reuse the already-extracted TU5 PE (do NOT re-extract)

The concurrent worktree has already dtk-extracted TU5 to a section-mapped PE:

- `/.claude/worktrees/tu5-migrate/orig/45410914/band_tu5.exe`
  (14,363,648 B, MZ/PE, 12 sections, image_base `0x82000000`, `.text` VA
  `0x82270000`). SHA1 of source XEX `c5a17091…` (clean retail TU5 v0.0.5.1).

TU5's XEX is *basic*-format → the loaded image is **section-mapped**; the flat
`0x3000+VA` offset that works on the pre-flattened TU0 base xex **drifts** on TU5.
Import the **dtk-extracted PE** (`band_tu5.exe`), which Ghidra section-maps
correctly — do not import the raw TU5 XEX with flat assumptions.

### Import cost & command (headless)

~14 MB PE, full auto-analysis of ~65k functions runs in single-digit minutes
headless (comparable to the existing TU0 import). Use the fork's `analyzeHeadless`
against the **same project** so both programs are co-resident:

```bash
GHIDRA=/home/free/code/milohax/ghidra/build/ghidra/support/analyzeHeadless
PROJ=/home/free/code/milohax/rb3-xenon/ghidra_projects   # RB3Xenon.gpr lives here
TU5=/home/free/code/milohax/rb3-xenon/.claude/worktrees/tu5-migrate/orig/45410914/band_tu5.exe

# Import TU5 as a SECOND program named e.g. band_tu5.exe into the RB3Xenon project.
# (Stop the :8002 service first for exclusive lock; restart after — same dance as
#  run_apply_symbols.sh. Do NOT touch the TU0 program.)
"$GHIDRA" "$PROJ" RB3Xenon -import "$TU5" \
    -analysisTimeoutPerFile 3600 -log /tmp/ghidra-tu5-import.log
```

**Language/loader caveat (load-bearing):** stock Ghidra has NO XEX loader. TU0 was
imported via the **`XEXLoaderWV` extension** under language
**`PowerPC:BE:64:Xenon`** (VMX128; `ppc.ldefs:335` in the fork). **Both programs
must land in the same project under the SAME language** before VT/ghidriff can diff
them. Since the worktree already dtk-extracted a plain PE (`band_tu5.exe`), Ghidra's
PE loader will take it — but you MUST force the same `PowerPC:BE:64:Xenon` language
(add `-processor PowerPC:BE:64:Xenon` or pick it in the loader), NOT let it
auto-pick a generic PPC. Simplest/safest: point `import-xex.sh` at the TU5 *XEX*
(via XEXLoaderWV, identical path to how TU0 was done) with a distinct program name,
rather than importing the bare PE — that guarantees language parity.

After import, both programs are selectable per MCP call via `binary_name` exactly
like the Wii `bank8_target`/`band_r_wii` split.

---

## 3. Which cross-program matcher for TU0↔TU5 name porting

TU0→TU5 is a **same-compiler, same-source rebuild**: ~94–96% of functions are
byte-identical modulo relocation/address drift. That profile decides the matcher.

### Recommendation

- **Primary bulk matcher: the skeleton/fingerprint map (§4) — already built,
  96.4%.** It is cheaper and already done; Ghidra should not re-derive the bulk.
- **Structural cross-check + residue-closer: Ghidra Version Tracking, headless,
  on the VT-perf fork.** Use it to (a) independently corroborate the skeleton
  HIGH/MED matches, and (b) recover changed-set functions the linear co-walk
  can't place, via **call-graph reference propagation**.
- **BinDiff** is the fallback structural matcher and is **already a proven headless
  path on this exact binary class** — see §3c.

### 3a. Ghidra Version Tracking (recommended structural route)

`AutoVersionTrackingScript.java`
(`ghidra/Ghidra/Features/VersionTracking/ghidra_scripts/`) runs the whole
correlator cascade **headless**: destination = the program in the tool (TU5),
source = a chosen program (TU0 base). It runs, in order
(`AutoVersionTrackingTask.java`):

1. **Symbol Name** (`SymbolNameProgramCorrelatorFactory`) — needs names on *both*
   sides; TU5 is fresh/unnamed, so this is a no-op here (skip).
2. **Exact Data Match** (`ExactDataMatchProgramCorrelatorFactory`).
3. **Exact Function Bytes** (`ExactMatchBytesProgramCorrelatorFactory`) — requires
   byte-identical *including operands* → **low yield** (relocation/address drift
   changes operand bytes).
4. **Exact Function Instructions** (`ExactMatchInstructionsProgramCorrelatorFactory`)
   — **THE money correlator.** `ExactInstructionsFunctionHasher` calls
   `applyMask()` with `instruction.getPrototype().getInstructionMask()`, which
   **keeps opcode-defining bits and zeroes operand bits** (addresses, immediates,
   register fields). So it is robust to relocation/address drift *by design* — the
   direct analog of the skeleton map's `&0xFC000003` bl/bc + `&0xFFFF0000` D-form
   masking, and actually *more* permissive (it also ignores register/immediate
   churn). False positives are gated by min-length (default 10 instructions) +
   one-to-one uniqueness. This is the primary structural matcher for
   same-source-different-relocation, **not Bytes**.
5. **Exact Function Mnemonics** (`ExactMatchMnemonicsProgramCorrelatorFactory`) —
   hashes only mnemonic string + operand count → coarser, more collisions; a
   *follow-on* after Instructions, not the primary.
6. **Duplicate Function Instructions** (`DuplicateFunctionMatchProgramCorrelatorFactory`)
   — resolves non-unique (identical-skeleton getter cluster) matches.
7. **Reference correlators** (Data/Function/Combined; `REF_CORRELATOR_MIN_SCORE`
   default 0.95, `MIN_CONF` 10.0) — propagate from *already-accepted* matches
   through the call graph to place neighbours. **This is a strictly richer version
   of the skeleton map's contiguity co-walk** (it uses the actual call graph, not
   just `.text` emission order), so it should close some AMBIG/MISS the co-walk
   breaks on.

`AutoVersionTrackingScript` **auto-accepts unique matches, applies markup, and
calls `destinationProgram.save(...)`** — it names TU5 in place. Its default options
(`createDefaultOptions()`) enable all exact correlators + ref correlators with
`REF_CORRELATOR_MIN_SCORE=0.95`, `MIN_CONF=10.0`, `MIN_VOTES=2`, `MAX_CONFLICTS=0`.
Set non-CLI options (thresholds, which correlators, **and — critically — apply
function signatures / data types, the type-port lever**) via a copy of
`SetAutoVersionTrackingOptionsScript.java` run as a first `-postScript`. Export the
destination symbols → TU5 symbol map (§4d); the ready-made exporter pattern is
`tools/ghidra/VTSeedPropDriver.java`'s `exportMatches()`, which walks
`session.getMatchSets()` → JSON `{dst_va, src_va, name, sim, conf, status}`.

**What the VT perf fork enables that stock Ghidra doesn't** (fork at
`/home/free/code/milohax/ghidra`, already the build the xenon `analyzeHeadless`
runs — so no jar swap needed on xenon):

- `df874cfe14` — `VTAssociationDB` record-key identity in `equals/hashCode`: kills
  the O(n²) duplicate-apply blow-up. At 65k functions / 13k names this is the
  difference between "runs" and "hangs".
- `0963a5e934` — parallel reference-correlator scoring (stateless compare, serial
  commit): the reference-propagation stage (step 7) is the slowest; this
  parallelizes it.
- `611a8d1dc5` — chunked score+commit to bound peak memory on the reference
  scorer. Needed at this program size.

(The Wii `rb3/build/SZBE69_B8/ghidra/VersionTracking-opt1212.jar` is the *same
fixes recompiled against a stock 12.1.2 install* for the Wii box. On xenon the
fork **is** the install, so the fixes are native — nothing to deploy.)

Also present and directly useful:
- `FindChangedFunctionsScript.java` — the **bank_divergence analog**: given two
  program versions, reports the functions that changed. Use it as an independent
  TU0↔TU5 changed-set gauge (cross-checks the skeleton `MISS`/`AMBIG` set, §4e).
- `CreateAppliedExactMatchingSessionScript.java` — one-shot exact-match session.
- Local drivers already in `tools/ghidra/`: `VTSeedPropDriver.java` +
  `bsim_seedprop_measure.py` (VT/BSim seed-and-propagate experiments) and
  `BSimQueryToJson.java`.

### 3b. BSim

BSim matches on feature vectors over *decompiled p-code* (structural, tolerant of
instruction-level differences), needs a backing DB (H2 embedded / PostgreSQL /
Elasticsearch) built via GenerateSignatures→CreateBSimDatabase→CommitSignatures.
It shines on *different-compiler / different-optimization* ports. For a
same-compiler same-source rebuild where exact-mnemonic already nails 90%+, BSim is
**overkill for the bulk** — its value is narrowly on the **MISS set** (genuinely
rewritten bodies) where even mnemonic masking fails but the decompiled structure is
similar. Keep it in reserve for the ~81 MISS, not the primary.

Residue-closer nuance: the **reference correlators inside the AutoVT cascade**
(`CombinedFunctionAndDataReferenceProgramCorrelator`) are seeded automatically by
the exact matches and propagate identity through call/data-ref edges when a body
changed — **no database needed**, and this is exactly the stage the fork
parallelizes/hardens. Reach for BSim only if refs leave gaps.

### 3b-bis. ghidriff (best as non-mutating cross-check)

`ghidriff` (`python -m ghidriff`, entrypoint `ghidriff.__main__:main`) has a
`VersionTrackingDiff` engine (default) that runs its OWN correlator cascade
(`ExactBytes → ExactInstructions → StructuralGraphExact → ExactMnemonics → BSim →
bulk hashers`, plus a local `rb3-improvements` branch with `--seed-matches`,
`--vt-ref-correlators`). It emits a clean JSON `function_matches` list
(`{p1_addr, p2_addr, match_types, p1_name, p2_name, scores}`, p1=TU0/old,
p2=TU5/new) into `json/<old>-<new>.matches.json`, plus markdown + optional
side-by-side HTML. **It does NOT auto-apply names** — you feed its map into a
separate applier (`apply_symbols.py`/`ImportMapFile.java`). So: use VT for
in-place auto-apply-and-save; use ghidriff when you want a reviewable diff and a
JSON map without mutating TU5:
```bash
python -m ghidriff --engine VersionTrackingDiff -o out TU0.exe TU5.exe   # --no-bsim ok (exacts carry it)
```

### 3c. BinDiff — proven on this exact binary class

BinDiff (BinExport + `bindiff --primary --secondary --output_format=bin` → SQLite)
is **already used on rb3-xenon**: `docs/plans/bindiff-vs-rb3wii.md` BinDiffed
RB3-360 against RB3-Wii (21,151/36,343 matched) and against DC3, producing
`unified_id_rb3wii.json` / `bindiff_match.json` consumed by the fingerprint
pipeline. The same recipe runs TU0↔TU5 trivially (same architecture, near-identical
image) and will score *very* high. BinDiff's MD-index/basic-block/call-graph
matching overlaps VT's strengths; choose VT for the tighter Ghidra-native
apply-markup-to-destination + symbol-export loop, or BinDiff if you prefer its
SQLite match table feeding a Python applier (the pattern already wired here).

### Verdict

| Job | Tool | Why |
|---|---|---|
| Bulk name port (byte-identical ~94–96%) | **Skeleton map (§4)** | Already built, byte-verified, free |
| Independent structural cross-check | **Ghidra VT (mnemonic + ref correlators, headless)** | Call-graph-aware; fork makes it feasible at scale |
| Residue: 397 AMBIG (dup getters) | **VT Duplicate-Function + Reference correlators** | Places by call-graph position, not just `.text` order |
| Residue: 81 MISS (rewritten bodies) | **BSim** (or manual on TU5 dtk disasm) | p-code structure survives body rewrite |
| Fallback / cross-oracle | **BinDiff** | Proven headless path already wired on xenon |

---

## 4. Recommended end-to-end pipeline

### (a) Ensure the base program is fully named
```bash
cd /home/free/code/milohax/rb3-xenon
# Widen the applied set from ~1,139 to the full 13,846 named universe:
python3 tools/ghidra/build_symbol_map.py --min-percent 0 --out tools/ghidra/rb3_symbol_map.json
tools/ghidra/run_apply_symbols.sh --no-regen        # stops :8002, applies, restarts
# (OR: adapt apply_symbols.py to read scripts/target_symbol_map.json directly.)
```
Skip this step if you are ONLY running the skeleton-map route (§4c-i), which
carries names from `target_symbol_map.json` without needing the base program named.

### (b) Import TU5 as the 2nd program (reuse the extracted PE)
See §2 command. Program name e.g. `band_tu5.exe`, into project `RB3Xenon`. Stop/
restart the :8002 service around the exclusive-lock import. **Do not disturb the
TU0 program.**

### (c) Run the matcher base→TU5

**c-i (primary, already done):** the skeleton map. Reproduce/refresh with the
worktree builder:
```bash
cd /home/free/code/milohax/rb3-xenon/.claude/worktrees/tu5-migrate
python3 tools/tu5_map_build.py          # ~27s → _tu5probe/tu5_migrate/base_to_tu5_map*.json
```
Outputs: `base_to_tu5_map.json` (per-named-function records:
`{base_va, symbol, size, tu5_va, confidence∈{HIGH,MED,AMBIG,MISS}, method,
body_identical}`, meta reports 12,817/13,295 = 96.4%), `base_to_tu5_map.full.json`
(`{base_va: tu5_va}` for all 61,629 resolved `.text` funcs), and
`tu5_changed_worklist.json` (478 = 397 AMBIG + 81 MISS).

**c-ii (structural cross-check + residue):** Ghidra VT headless:
```bash
GHIDRA=/home/free/code/milohax/ghidra/build/ghidra/support/analyzeHeadless
PROJ=/home/free/code/milohax/rb3-xenon/ghidra_projects
VTSCRIPTS=/home/free/code/milohax/ghidra/Ghidra/Features/VersionTracking/ghidra_scripts
# TU5 is the destination (-process); TU0 base is the source (last arg).
"$GHIDRA" "$PROJ" RB3Xenon -process band_tu5.exe -noanalysis \
    -scriptPath "$VTSCRIPTS" \
    -preScript SetAutoVersionTrackingOptionsScript.java \
    -postScript AutoVersionTrackingScript.java "/VTSessions" "TU0_to_TU5" \
    "/default.xex-35adb6"
```
(Copy `SetAutoVersionTrackingOptionsScript.java` and enable markup-apply +
mnemonic/duplicate/reference correlators + your score/conf thresholds before
running. Requires base fully named — step (a).)

### (d) Auto-apply names to TU5 + export a TU5 symbol map

- **From the skeleton map (recommended, deterministic):** apply
  `target_symbol_map.json` names at the mapped TU5 VAs. For Ghidra: adapt
  `apply_symbols.py` to take `{tu5_va: symbol}` = join `base_to_tu5_map.json`
  records (`symbol` at `tu5_va` for HIGH+MED) and run it against `band_tu5.exe`.
  For the config re-anchor (splits/symbols): rewrite each `.text` VA in
  `config/45410914/symbols.txt` and `splits.txt` through `base_to_tu5_map.full.json`
  (name-keyed `decomp.db` rows are portable and need no VA rewrite; only
  `fn_XXXXXXXX` VA suffixes rebase).
- **From VT:** with markup-apply enabled, VT writes names into `band_tu5.exe`
  directly; then export the destination program's symbols to JSON (a small
  post-run PyGhidra/MCP dump of `FunctionManager` symbols → `{tu5_va: symbol}`),
  which feeds the same re-anchor.
- The two agree on the ~96% bulk (that agreement IS the cross-check); disagreements
  flag either a skeleton mis-place or a VT false-accept → triage list.

### (e) Gauge per-function TU0↔TU5 divergence (the bank_divergence.py analog)

**This already exists** in the skeleton map, no new tool needed:
- **HIGH** (8,855) + **MED** (3,962) = `body_identical: true` → the "byte-identical"
  / TRUST bucket (equivalent to Wii TRUST).
- **AMBIG** (397) — skeleton present in TU5 but position unpinned (dup getters) →
  CAUTION; mechanically placeable.
- **MISS** (81) — skeleton absent = genuinely rewritten body → the true
  re-derivation work (equivalent to Wii MISLEADING / rewritten-era functions).

`tu5_changed_worklist.json` is the changed-set worklist. Cross-check it with
Ghidra's `FindChangedFunctionsScript.java` (§3a) for an independent second opinion
on which of the 478 truly changed. Independent ground truth already validated: the
7 same-instrument patch anchors and a 5-function prologue spot-check all resolve
(e.g. `RecalcGemList` base→`0x82794740`, `MasterAudio::IsLoaded`
`0x82756D98`→`0x8277B6E8`); `OvershellPartSelectProvider::IsActive` correctly falls
in MISS (body diverged ~56%).

---

## 5. Reusable-from-Wii-banks assets

| Wii asset | Transfers to xenon? | Adaptation |
|---|---|---|
| **VT perf fork** (`/home/free/code/milohax/ghidra`, commits `df874cfe14`/`0963a5e934`/`611a8d1dc5`) | **Directly — it IS the xenon Ghidra build** | None. The fixes are native; no jar swap. The Wii `VersionTracking-opt1212.jar` (recompiled vs stock 12.1.2) is Wii-box-only and NOT needed here. |
| `AutoVersionTrackingScript.java` + `SetAutoVersionTrackingOptionsScript.java` + `FindChangedFunctionsScript.java` | **Directly** (fork ships them) | Copy the options script, enable markup-apply + mnemonic/dup/reference correlators. |
| `scripts/analysis/bank_divergence.py` (Wii TRUST/CAUTION/MISLEADING) | **Superseded** | The skeleton map's HIGH/MED/AMBIG/MISS `body_identical` classification is the xenon-native equivalent; `FindChangedFunctionsScript` is the Ghidra-native second opinion. |
| `tools/ghidra/port_dwarf_types.py` (Bank5→Bank8 DWARF *type* port by mangled name) | **Concept only, NOT directly** | Wii ports **DWARF types** from a donor program; xenon TU0/TU5 have no DWARF. Two Ghidra-native type-port paths instead: (1) **VT signature apply** — enable "apply function signatures/data types" in `SetAutoVersionTrackingOptionsScript.java` so AutoVT ports TU0's prototypes/types onto TU5's accepted matches in the same pass (the closest VT analog to `port_dwarf_types.py`); (2) **from decomp** — signatures come from `decomp.db`/headers keyed by mangled name (portable across the rebuild), applied to TU5 via `MicrosoftDemangler` / `apply_demangled_signatures` (already in `mcp_client.py`). |
| The two-programs-one-server pattern (`bank8_target`+`band_r_wii`) | **Directly** | Import `band_tu5.exe` as the 2nd program in RB3Xenon; select per MCP call via `binary_name`. |
| DOL→ELF transcode import path | **Replaced** | Xenon uses XEX→dtk-extract→PE (`band_tu5.exe`) + the fork's XEX/PE loader, not the Wii DOL transcode. |

## Key file/path index

- Base program (do not disturb): port 8002 `/default.xex-35adb6` — `orig/45410914/default.xex` (TU0)
- TU5 PE (reuse): `.claude/worktrees/tu5-migrate/orig/45410914/band_tu5.exe`
- Naming: `tools/ghidra/{build_symbol_map.py,apply_symbols.py,run_apply_symbols.sh}`; map `tools/ghidra/rb3_symbol_map.json` (1,139); full universe `scripts/target_symbol_map.json` (13,846)
- Skeleton map + worklist: `.claude/worktrees/tu5-migrate/_tu5probe/tu5_migrate/{base_to_tu5_map.json,base_to_tu5_map.full.json,tu5_changed_worklist.json}`; builder `tools/tu5_map_build.py`; VA reader `tools/tu5_va.py`
- VT fork: `/home/free/code/milohax/ghidra` (scripts under `Ghidra/Features/VersionTracking/ghidra_scripts/`); `analyzeHeadless` at `.../build/ghidra/support/analyzeHeadless`
- MCP client (library, not CLI): `tools/ghidra/mcp_client.py` (`search_symbols`, `decompile_function`, `apply_demangled_signatures`, `bulk_create_functions`, …)
- BinDiff prior art: `docs/plans/{bindiff-vs-rb3wii.md,bindiff-integration.md}`
