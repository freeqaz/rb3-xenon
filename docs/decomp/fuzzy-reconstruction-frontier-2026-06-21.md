# The Fuzzy Reconstruction Frontier

**rb3-xenon decomp — findings, the body-divergence wall, and the fuzzy reframe**
*Written 2026-06-21. Baseline: `main` @ `535af96` (Wave-20 close).*
*All numbers in this document were live-verified against `build/45410914/report.json`, `unified_id_rb3wii.json`, and the `../objdiff` / `../jeff` git state on the date of writing. Where a survey figure differed from a re-derived figure, the re-derived figure is used and noted.*

---

## Executive Summary

rb3-xenon stands at **9793 / 65547 functions byte-exact (14.94%)** — `matched_code = 836,420 / 11,057,676` code bytes (**7.56%**), `fuzzy_match_percent = 10.07%`. The session arc is **6932 → 9793 (+2861)** across waves 3–20.

The project has hit a decisive **inflection**. Every *cheap* matching vein — span-pin, relocate, sliver-repin, hash_map struct-swap, map-augment, fresh-inventory, truncation-pool, inline-policy, permuter — is practically exhausted. The evidence is the wave-delta collapse: **W9 +723 → W12 +103 → W19 +16 → W20 +1**, a cost-per-match rise of roughly **three orders of magnitude** since the keystone era. Wave-20's own close declares "PRACTICAL EXHAUSTION OF THE CHEAP-MATCHING WAVE FORMAT" (`docs/plans/decomp-state-and-roadmap-2026-06-09.md:2025`, `:2058`).

The remaining oracle-backed surface is **RB3-specific game code** (`src/band3/`, `src/network/`, especially `meta_band`). Two facts make this surface qualitatively harder than everything matched so far:

1. **It is maximally ICF-scattered.** From the rb3-Wii BinDiff oracle (`unified_id_rb3wii.json`, 9301 pairs), **610 translation units / 9146 functions** span more than 2 MB of the 10.5 MB `.text` — i.e. their methods are sprinkled binary-wide by the linker's identical-COMDAT folding. `meta_band` alone is **173 TUs / 2813 functions**, **159** of which span >1 MB. The span-pin/relocate veins that won waves 3–15 **physically cannot touch these** — there is no contiguous cluster to pin.

2. **Its only oracle is a cross-compiler, dev-build twin.** DC3 (the same-compiler `MSVC-X360` platform twin, BinDiff similarity ~0.98) **does not contain `meta_band`/most `band3` TUs**, so it is a *false friend* for game code. The only identity+logic oracle is **rb3-Wii** — `MWCC-PowerPC`, a *development* build. Its similarity to retail-360 is structurally **low**: median **0.246**, only **7.6%** of pairs clear 0.5. A faithful `MWCC→MSVC` port therefore provably stalls short of byte-exact: Wave-16 ported **BandProfile.cpp** end-to-end (1013 lines, obj defined 115 symbols, 64 micro-ranges carved) and **0 of 64 functions reached 100%** — best fuzzy **47.8%**, the constructor **1.7%** (`docs/plans/decomp-state-and-roadmap-2026-06-09.md:1823-1827`).

The project owner's directive follows directly and is **structural, not aspirational**: expecting 100% byte-exact for RB3-specific code is unrealistic, and **the system must become fuzzy**. `report.json` already carries `fuzzy_match_percent` per function and in `measures` — the binary is at **10.07% fuzzy vs 7.56% strict**, ~**260,000 fuzzy-credit bytes** the headline currently discards — but nothing rewards or tracks that partial progress, so a reconstruction that takes a function from 0% → 85% registers as **zero**.

**Recommended path (cheap-first):**
1. **Operationalize a tiered fuzzy metric now** (~40 lines; `tools/fuzzy_progress.py` already exists — promote and wire it). Zero new computation.
2. **Run the MemTemp-RAII keystone (Wave-21)** as a clean go/no-go on whether *any* tractable byte-exact matching remains.
3. **Build the per-function reconstruction workbench** (rb3-Wii body + Ghidra ground-truth + `classify_nearmiss` divergence + agentic fix loop; ~80% reuse), and **pilot it on ONE small scattered TU** (SongSortNode/LockStepMgr, not BandProfile).
4. **Only then integrate the banked objdiff case-B fork**, whose +150–220 ceiling is theoretical until the body-divergence wall is cracked.

A low-risk side-vein runs concurrently: **thirdparty** (zlib/ogg/vorbis/tomcrypt/curl — 10124 fns / 1.31 MB, only 7.29% matched) is public, same-compiler source with no cross-compiler divergence and has never been targeted by a wave.

---

## 1. What EXISTS — the pipeline and tooling inventory

### 1.1 Two oracles, and the provenance law that governs them

There are two cross-binary BinDiff oracles, both built the same way (Ghidra import → BinExport → BinDiff → JSON, per `docs/plans/bindiff-integration.md`), differing only in which reference binary was diffed against the RB3-360 XEX:

| Oracle | Artifact | Entries | Compiler/Platform | Similarity | Use |
|---|---|---|---|---|---|
| **DC3** | `tools/bindiff_match.json` → `unified_id.json` | 11,057 | `MSVC-X360` (same twin) | ~0.98 (87% are 1.0) | **ENGINE body oracle** for `src/system/*` (Milo) **only** |
| **rb3-Wii** | `unified_id_rb3wii.json` | 9,301 | `MWCC-PowerPC` (cross) | median **0.246**, 7.6% ≥0.5 | **Identity + logic** oracle for RB3-specific game code — **the only one** |

**The hard law (capture this everywhere):** DC3 is the *same-compiler/same-platform* twin and a strong **body** oracle, but **`meta_band` and most `band3`/`network` TUs do not exist in DC3** — there is no DC3 source for them, so DC3 is a **false friend** for game code. For RB3-specific TUs, **rb3-Wii is the only oracle**, and because it is *cross-compiler* it gives function **identity** (`rb3_addr → wii_name + bindiff_src .cpp`) and **logic**, but **not byte-faithful bodies**. The `sim ≥ 0.5` gate used throughout the honesty tooling is meaningful precisely because the median is 0.246.

Both fold into `tools/scope_data/uid_merge.json` via `tools/scope_data/gen_uid_merge.py` (DC3 `sim≥0.9 conf0.95` outranks rb3-Wii `sim≥0.7 conf0.7` — DC3 ranks higher on trust). **Gap:** `uid_merge.json` is regenerable from checkout, but `unified_id_rb3wii.json` is a *checked-in harvested artifact* — no committed Ghidra→BinDiff→JSON regen script for the rb3-Wii leg was found in-tree (the DC3 merge leg exists as `fingerprint_match.py merge_bindiff`).

### 1.2 The rb3-Wii → MSVC-X360 compile pipeline (`tools/fingerprint_pipeline.py`)

The spike-verified premise (`tools/fingerprint_pipeline.py:1-30`): rb3-Wii game *source* compiled under **our** MSVC-X360 toolchain fingerprints the retail XEX **far** better (~0.95–1.0 on revision-matched functions) than the cross-compiler rb3-Wii *binary* bindiff oracle (~0.25). This is what enabled the entire `meta_band` belt (~40+ ported TUs, waves 9–17).

**The orchestration is automated; the port itself is not.** `fingerprint_pipeline.py:11-13` states it plainly: *"Porting fixes themselves stay manual/agent (MWCC→MSVC isn't auto-translatable), but everything else is here."* Three automated subcommands:

- **`candidates`** — ranks unported game TUs by oracle coverage × source presence in `../rb3`. (Verified run: `--min-fns 5` returns GemPlayer/VocalPart/GemManager/BandUser/BandUserMgr — the remaining pool is now mostly the big-scatter player TUs the playbook forbids span-pinning.)
- **`scaffold T`** — raw-copies `../rb3/<rel>` into `src/<rel>` verbatim and prints the `objects.json` `NonMatching` entry + a *provisional* `splits.txt` `.text` span ("VERIFY before pinning"). It does **not** apply any mechanical fix, so a scaffolded TU starts non-compiling.
- **`manifest`** — the fuzzy-reframe tool: buckets every game/network function by `fuzzy_match_percent` and reports a "same-compiler hit rate." (Verified: 74 units / 4789 fns / **54% high-or-matched**; the bimodal yield is visible — SongUpgradeMgr 92.4% vs SessionDiscoveryTable 0%.)

Supporting automation:
- `tools/gen_game_target_map.py` — **CRITICAL pairing step**: generates `scripts/target_symbol_map.json` entries mapping the dtk-split anonymous `fn_<addr>` *target* symbols to our MSVC-mangled compiled symbols. **Without it a pinned game TU reads a false 0%.**
- `tools/game_splits.py` — target-only `.text` spans for *sourceless* TUs from the oracle (grows the denominator without source; `KNOWN_FP_RELS` guards Ghidra-confirmed misattributions).
- `tools/defines_common.py:8-38` — include order mirroring rb3-Wii's `-i` layout: **STLport first** (shadows native CRT), `src/xdk/LIBCMT` (Xbox 360 XDK C CRT), `src`, `src/system`, `src/band3`, `src/network`, oggvorbis, speex. STLport + XDK CRT vendored from dc3-decomp.
- `src/types.h:45-62` — dual-target type defs (`HX_NATIVE` int-width LP64 branch vs Xbox ILP32 long branch) so the same headers compile under both the matching and native builds.
- `configure.py:339-373` — the wired renamer/patcher chain: pre-compile `obj_target_symbol_renamer` (target `fn_<addr>` → MSVC mangled) + post-compile `anon_ns`/`dynamic_init`/`guard`/`bool_mangle`/`atexit_scope`. This is what makes game-TU fuzzy% in `report.json` **real** rather than false-0.

**What is manual** (the bulk of the labor) is fully cataloged in `docs/plans/meta_band-port-breaking-changes.md` (3 sessions, ~35 TUs). Recurring per-TU fixes: include-path rewrites; strip MWCC `#pragma force_active/pool_data/dont_inline`; `Symbol::mStr → .Str()` / `DataArray::mSize → .Size()`; `operator new(unsigned long) → (size_t)` and `std::vector<T,unsigned short>` → drop 2nd arg; ref-vs-pointer global flips (`TheUI`/`TheContentMgr`); signature divergences (LocalizeSeparatedInt gained a `TheLocale` arg; `Achievements::Submit`; `Data::Print` dropped DC3's `int indent`); `revolution/OS.h` substitutions; deep include-cascade stubs (some TUs *excluded* for 15+ missing headers — BandStoreUIPanel, AccomplishmentPanel). **None of these ~6 deterministic patterns has a codemod** — each is applied by hand every time. That is the clearest un-built force-multiplier in an otherwise mature pipeline.

### 1.3 Identity transfer & micro-pinning (`tools/identity_transfer.py`)

Per-function micro-pin transfer for ICF-scattered TUs (`docs/decomp/identity-transfer.md`, ~410 LOC). Where span-pinning fails — and *mints fake matches* via ≤44 B ICF stub-folds (the Wave-14/15 +57 refutations) — this carves each method into the TU obj by its individual VA using N-range splits. It classifies **CASE-A** (method in an unowned `auto_*` blob → pins cleanly, the bulk), **SELF** (already in own pin → reveal_sweep territory), and **CASE-B** (inside a *foreign* unit's pin → skipped to `/tmp/idtransfer_caseb_<tu>.json`, eviction-gated). Map writes are **strict add-only** (never wholesale-regen = poison). First win: **RockCentral.cpp +17** (whole-binary, 0 regressions). **Limitation:** the transport is gated on *porting the MWCC source first* (so the obj defines the methods) — and Wave-16 proved ported bodies diverge (BandProfile 0/64 at 100%). So identity-transfer is a *transport* that needs a *byte-exact body* to carry, which is exactly the wall.

### 1.4 The objdiff case-B byte-equality fork (BANKED, `../objdiff` @ `b1c92be`)

objdiff is freeqaz's fork. Stock features already in the shipping CLI: `FunctionRelocDiffs::NameOnly` mode, `??__E`/`??__F` funclet pairing, MSVC-X360 FP-anchor frame-establisher slip normalization. The **banked** case-B fork (branch `caseb-global-byteeq @ b1c92be`, doc `docs/decomp/handoff/objdiff-caseb-fork-banked.md`) adds a cross-unit **global byte-equality second pass** in the report driver — it promotes an unmatched *named* target fn whose retail body was carved into a *foreign* unit's span (identity-transfer case-B) when a byte-identical base symbol exists in any unit, gated by masked-bytes-equal + reloc-target-name-equal + a **required** oracle (`--global-byte-eq-oracle unified_id_rb3wii.json`, `sim ≥ 0.5` AND attributes to the claiming unit; it *refuses to run* without the oracle).

**Critical state:** built **only to `/tmp/objdiff-fork-target`**; the shared `../objdiff/target/release/objdiff-cli` was **not** rebuilt, so **all current rb3-xenon measures use stock objdiff** (`fresh_report.sh` does not pass `--global-byte-eq`). Honest payoff **now = +0** (do-no-harm; it correctly rejected 4 un-oracled STL-fold demos). Ceiling **+150–220** is gated *upstream* on a case-B harvest wave (scattered TUs ported byte-exact first) — and because it counts by byte-equality only, **its true EV is near-zero until the body-divergence wall is cracked**. It is correctly banked.

### 1.5 The honesty layer

- **`tools/icf_alias_check.py`** (commit `23bb6ee`) — the required audit gate for sourceless pins and every case-B promotion. Catches the recurring fake-match shape: tiny (≤44 B) thunks/getters/guard-stubs/STL accessors ICF-fold byte-identically across unrelated TUs, so a pin reports them 100% without *owning* the code (+57 fake in Wave-14, +57 in Wave-15). Verdict `INFLATED` when stub-folds dominate (`STUB_DOMINANCE=0.60`) with no own-bodied anchor, or a ≥8-contiguous foreign run. Exit 1 = INFLATED; wave audits gate on it.
- **`tools/true_progress.py`** — refuted the "ICF is hiding ~625 matches" hypothesis (settled: no LTCG, ICF link-lever ≈0). Classifies the [90,100) near-miss pool by root cause, separating recoverable `NAME_RELOC` from real `STRUCT_OFF` from `FRAME_RECON` funclet noise.
- **`tools/classify_nearmiss.py`** — per-instruction divergence classifier (`NAME_RELOC`/`WRONG_PAIR`/`OFFSET`/`REG`/`OPCODE`/`OTHER`) from objdiff per-fn JSON. **This is the divergence-classifier core a reconstruction workbench would reuse.**

### 1.6 jeff / dtk (`../jeff` @ `39e482f`)

Local jeff fork for RB3-retail XEX SPLIT. Recent fixes: `grow_undersized_function_symbols` (the Wave-18 **+108 truncation fix** — grows any pdata-anchored Function symbol whose cached `symbols.txt` size is shorter than its `.pdata` length; restored e.g. `GemTrack::See` 0x28→0x64, 0→99.4%; 1207 sizes grow, 0 shrink, 0 overlap); plus a clamp for oversized symbols and a phantom-overlap prune; asm-write-failures downgraded to warnings (UTF-16/PpcRel tolerated). Truncation mode-2 (`except_data` mis-decode) is **not** addressed. **Side-effect to track:** 5 pre-existing pins authored around truncated sizes now need their `splits.txt` `.text` end extended (Rand/JoypadClient/HDCache/SongPreview/FlowSetProperty).

### 1.7 The reveal cascade and supporting analysis

- **`tools/reveal_sweep.py` + `safe_name_merge.py` + `refill_loop.sh`** — finds an unmapped target `fn_<addr>` whose *normalized* bytes equal a not-yet-matched real symbol in our compiled obj for the same pinned unit, and emits a map entry that *reveals* an already-byte-exact match objdiff couldn't pair (it pairs by name only). **Self-validating** (a wrong addr cannot produce a byte-exact normalized match). `refill_loop.sh` is the standing post-wave driver (precedent +255/+172); it **exits nonzero if any unit drops** (honesty gate). Note the **mandatory renamer refresh** (`rm target_symbol_renames.stamp && touch config.yml`) or new map entries read +0.
- **`scripts/analysis/diff_inspect.py`** (1969 LOC) — the analysis engine behind `/compare-asm` + `/stack-layout` and MCP `run_diff_inspect`. Modes: `--diagnose`/`--attributed`/`--clusters`/`--regswaps`/`--offsets`/`--replaces`/`--stack-layout`/`--compare-asm`/`--compare`. (Trust `report.json measures.matched_functions` after `fresh_report.sh`, *not* the `--diagnose` headline.)
- **`tools/pin_audit.py`** — the sliver/over-pin/displaced-pin detector with seven FP filters; read-only; the vein it mines is now largely exhausted.
- **`tools/scope_map.py`** — priority-tiered % toward 100% of the whole binary (oracle-backed vs no-oracle). Worldview: everything is matchable, no-oracle code is lower *priority*, not a ceiling.

### 1.8 The fuzzy signal already exists

`report.json` per-function carries **both** `match_percent_normalized` (the canonical reloc-normalized score) and `fuzzy_match_percent`; `measures` carries `matched_functions`/`matched_code` (strict) **and** `fuzzy_match_percent` (= **10.07%** vs strict 7.56%). `tools/fuzzy_progress.py` already exists and reports the leading-indicator bands. **What is missing is not data — it is a goal:** nothing treats a high-fuzzy RB3-specific function as a first-class deliverable. (`tools/fuzzy_content_match.py` / `global_fuzzy_index.py` provide MinHash-LSH similarity pairing; `global_fuzzy_pairs.json` is the live artifact, currently stale with dead `merged_*`/dup rows — regenerate via `global_fuzzy_index.py 64 0.85`.)

---

## 2. What we have DONE — progress, veins, keystones

### 2.1 Session arc: 6932 → 9793 (+2861)

All deltas are composed-verified EXACT with ~0 regressions (from the roadmap wave-close lines):

| Wave | Δ | Dominant method |
|---|---|---|
| W3 | **+853** | sliver/relocate/displaced-pin vein (~+600 of +853 from mis-pinned wired TUs: UIList +80, CharEyes +68, Player +35) |
| W4 | +81 (+refill) | bodyport pool + refill_loop |
| W5 | +109 | **hash_map vein discovered** (STLport hash_map masquerading as std::map) |
| W6 | +47 | SongMgr all-5 hash_map; Waypoint relocation +31 |
| W7 | +14 | Part dual-range, FSSS |
| W8 | +80 | adversarial re-verify caught 4 wrong backlog claims; MakeString by-value +23 |
| **W9** | **+723** | **the keystone wave** (MILO_MESSAGE_TIMERS Handle-gate +217 + 13 game-ports + refill) |
| W10–W14 | +118/+146/+94/+50/+23 | `meta_band` belt (one-TU-per-lane port-then-pin from rb3-Wii) |
| W15 | +58 (+17 idt) | AccomplishmentConditional std::list cascade; identity-transfer milestone (RockCentral +17) |
| W16 | **+0** | **productive negative — the body-divergence WALL found** (BandProfile) |
| W17 | +40 | struct-lever body-ports (OnlineID XUID, UIList mListDir) |
| W18 | **+122** | **truncation fix +108** (2nd-biggest lever) + CharHair revert |
| W19 | +16 | inline-policy vein (SetFrame) |
| W20 | +1 | cheap-matching format declared **PRACTICALLY EXHAUSTED** |

**The trend is the headline:** **W9 +723 → W12 +103 → W19 +16 → W20 +1** — cost-per-match up ~3 orders of magnitude.

### 2.2 The keystones (binary-wide force-multipliers)

1. **MILO_MESSAGE_TIMERS Handle-macro gate — +217** (commit `3b86e9a`, W9). Retail compiled every Milo Object's `::Handle` with MessageTimer profiling *off*; gated behind a new `MILO_MESSAGE_TIMERS` macro (undefined = retail) in `Object.h`/`ObjMacros.h`. Binary-wide across 10+ unit families. **Durable** — covers any future Handle near-miss.
2. **Truncation / symbols.txt grow fix — +108** (W18, jeff branch `@39e482f`). A stale committed `symbols.txt` made dtk emit 1875 truncated function symbols objdiff couldn't pair (false 0%); regenerating it via `grow_undersized_function_symbols` also *refilled* the frontier with de-truncated full-bodied near-misses + cascaded +33.
3. **MakeString.h by-value template params — +23** (commit `36b9817`, W8). Global header convention: DC3 const-ref vs rb3-Wii by-value template params; a broad inlining keystone.
4. **Struct-levers** (DC3-version-drift — our DC3-sourced tree carries later-revision members RB3-retail lacks): OnlineID 0x10 XUID-size, UIList `mListDir` +0xC member-shift (+14), CharHair full-revert (+14), `Data::Print` drop DC3 `int indent`, AccomplishmentConditional std::list-not-vector + base-tail pad (+58 cascade). These body-ports **refill** the reveal pool (unlike belt-pins, which cascade ~0).
5. **Inline-policy force-multiplier** (W19): `RndAnimatable::SetFrame` out-of-line (DC3) → inline (retail) cascade +4. Now dry.
6. **~40+ `meta_band`/game-TU ports from rb3-Wii** (+423 in W9 + the W9–14 belt): Sequence +111, SongSortMgr +78, BandSongMgr +63, SongUpgradeMgr +41, Campaign +58, ManageBandPanel +62.

### 2.3 Process maturity

Wave-loop SOP (`docs/decomp/handoff/wave-loop-SOP-2026-06-20.md`); two-stage honesty defense (adversarial own-vs-foreign audit via `icf_alias_check` → composed whole-binary A/B); worktree pool via `scripts/setup_worktree.sh` (btrfs CoW reflinks); orchestrator MCP + Ghidra MCP. The matching machinery is mature; the *frontier* changed under it.

---

## 3. What we have NOT done — the hard frontier

### 3.1 The scattered RB3-specific TUs (largest untapped oracle-backed inventory)

From `unified_id_rb3wii.json` (9301 pairs), **610 TUs / 9146 functions** span >2 MB — maximally ICF-scattered. `meta_band` = **173 TUs / 2813 fns**, 159 span >1 MB. Every top TU spans essentially the whole `.text`:

| TU | fns | span |
|---|---|---|
| GemPlayer.cpp | 169 | 9.30 MB |
| VocalPlayer.cpp | 167 | 9.23 MB |
| VocalTrack.cpp | 139 | 9.42 MB |
| RockCentral.cpp | 135 | 9.27 MB |
| AccomplishmentManager.cpp | 131 | 9.55 MB |
| AccomplishmentProgress.cpp | 118 | 9.45 MB |
| NetSession.cpp | 111 | 9.40 MB |
| ProfileMgr.cpp | 108 | 8.62 MB |
| BandProfile.cpp | 104 | (`0x822639F0..0x82BD66B0`) |
| SongSortNode.cpp | 53 | 9.31 MB |
| LockStepMgr.cpp | 26 | 8.98 MB |

Retail compiled with `/O1` (no LTCG) but the **linker** applied ICF folding, so each TU's methods are scattered binary-wide. The span-pin/relocate veins cannot touch them. The only mechanism is **per-method identity-transfer of a ported body** (§1.3) — which requires porting the MWCC source first, which hits §3.2.

### 3.2 The body-divergence WALL (the actual blocker)

Wave-16 ported **BandProfile.cpp** fully (1013 lines MWCC→MSVC, obj defined 115 symbols, carved 64 micro-ranges, oracle-named 23) — and **0 of 64 functions reached 100%**: best fuzzy **47.8%**, constructor **1.7%** (`roadmap:1823-1827`; branch `w16-bandprofile @ ec65595` kept). Why they diverge, in priority order:

1. **Cross-compiler (dominant).** The only oracle is rb3-Wii = `MWCC-PowerPC`, not `MSVC-X360`. Even with identical logic, instruction selection, register allocation, and scheduling differ everywhere. DC3 (the only same-compiler twin) **does not contain `meta_band`**, so there is no same-compiler source for this code at all.
2. **Wrong revision.** rb3-Wii is the *dev* build, not retail — extra MILO_ASSERT/debug paths and member-set drift (AccomplishmentConditional vector-vs-list, CharHair rev-13-vs-rev-11 prove retail differs from both DC3-newer *and* Wii-dev).
3. **Struct/layout.** A single wrong this-relative offset propagates through every method (the cascading struct-lever class — partly fixable, e.g. the +58 std::list cascade).
4. **Inline-policy.** DC3/retail inline-decision differences emit spurious `bl` (the SetFrame vein, now dry).

The implication is exactly the owner's: **byte-exact for cross-compiler RB3-specific bodies is not realistic.** The fuzzy surface is being *measured* (`fuzzy_match_percent` = 10.07% vs strict 7.56%) but not *treated as the primary success criterion*.

### 3.3 Unexploited tractable inventory

- **thirdparty** (zlib/ogg/vorbis/tomcrypt/curl) — **10124 fns / 1.31 MB, only 7.29% matched.** Public, same-compiler source; **no cross-compiler divergence**; **no wave has targeted it.** The cheapest remaining oracle-backed (in fact *source-exact*) inventory.
- **The one un-attempted force-multiplier** — the **MemTemp-RAII keystone** (Wave-21, `fn_82797500` no-arg RAII, ~36 `bl` callers; `roadmap:2047-2049`). The roadmap states its outcome is the literal go/no-go for whether *any* tractable byte-exact matching remains. It must be run before declaring strict exhaustion final.

### 3.4 The fuzzy reframe is not operationalized

Success is still gated on 100%-only. A 47.8% BandProfile body counts as 0. There is no fuzzy honesty gate distinct from `icf_alias_check`'s byte-equality one, no fuzzy leaderboard in `fresh_report.sh`, no behavioral-equivalence success criterion (the unicorn verifier is ported from DC3 but **unpopulated** here — `decomp.db` unicorn columns are all NULL).

### 3.5 No-oracle code (lower priority, partly out of scope as byte targets)

`vendor` = **5320 fns / 3.11 MB at 0%** (statically-linked Microsoft D3DX/D3D9/XGRAPHICS/XAudio/XACT/XMA + RAD BINK + Quazal/NetZ) — no source twin. `xdk` = 164 fns. `scope_map`'s worldview is that these are matchable in principle (reconstruct from headers/disasm) but **lower priority**, not a hard ceiling. The `auto_03_*_text` blobs hold the bulk of unmatched bytes and are where scattered-TU methods and no-oracle code intermix.

---

## 4. THE FUZZY REFRAME — why and how

### 4.1 The distribution that justifies it

Live-computed over all 65,547 functions from `report.json` (`match_percent_normalized`, falling back to fuzzy):

| Band | Count |
|---|---|
| **100% (strict)** | **9793** |
| [95, 100) | 1352 |
| [90, 95) | 207 |
| [80, 90) | 101 |
| [50, 80) | 160 |
| (0, 50) | 154 |
| 0% / unwired | 53780 |

Beyond the byte-exact set, **~1820 functions are already >50% fuzzy** (1559 of them ≥90%). Of the 53,780 reading 0%, the overwhelming majority are **"not yet attempted"** (no `.text` pin + compiled obj), **not** "attempted and failed."

**The wired (oracle-backed/attempted) set, n = 11,744** — the honest denominator:
- ≥100 = 9793 (83.4%), ≥95 = 11,145, ≥90 = 11,352.
- **Fuzzy-code completion = 95.2% vs strict 72.8%** — a **~260,000-byte fuzzy surplus** the headline discards.

**RB3-specific (band3 + network), n = 4688 fns:** 2642 wired (2225 @100, **313 near-miss [90,100)**, 54 mid, 50 low) + **2046 unwired**. **Fuzzy-code completion 91.9% vs strict ~29% byte-coverage.** Those 313 near-misses + 2046 unwired are *exactly* the reconstruction frontier that currently shows ~0 movement.

### 4.2 The proposed metric — tiered, anti-gaming

Promote the data `report.json` already carries; keep strict as the immutable north star.

**PRIMARY HEADLINE** (three co-equal lines):
```
STRICT      9793 / 65547 fns byte-exact (14.94%)      [immutable north star]
FUZZY-CODE  10.066%  (size-weighted per-fn fuzzy over the whole 11.06M-byte binary)
STAIRCASE   N≥100 = 9793 | N≥95 = 11145 | N≥90 = 11352   [completion staircase]
```

**PRIMARY FUZZY GOAL = fuzzy-code-% over the WIRED denominator** (currently **95.2% vs 72.8% strict**) — the honest "how close is the attempted set to perfect," immune to the ~75% no-oracle bulk dragging it down. Whole-binary fuzzy% stays as a secondary public figure.

**TWO SUB-GOALS** (so engine noise never masks the frontier):
- **RB3-specific (band3/network) wired completion: 91.9%** across 2642 fns; 313 near-miss + 2046 unwired to climb. *This single weekly delta is the truest measure of the hard-frontier campaign.*
- **Engine (src/system) completion** reported separately — **DC3 oracle, byte-exact still expected here; do not relax the bar where it is achievable.**

**FUZZY-CREDIT LEDGER:** `matched_code_strict ≈ 836,420 bytes`; fuzzy ≈ 1,109,000 bytes ⇒ **~260,000 "fuzzy-credit bytes"** (~2.4% of binary) that strict discards. A body-port's deliverable becomes `+X fuzzy-credit bytes` = Σ(`fuzzy_match_percent × size`) deltas. A scattered RB3 TU taken 9% → 85% is now a positive, reportable result.

**ACCEPTANCE BAR for RB3-specific TUs** where byte-exact is declared unrealistic: **≥90% normalized = "fuzzy-DONE,"** with a documented carve-out — a fn ≥90 whose residual diff is classified by `true_progress.py` as regalloc / FP-scheduling / funclet / build-env noise (not logic) is **"logic-complete"** and counts toward a separate `logic-matched` tally. Where the unicorn verifier is populated, **behaviorally EQUIVALENT at <100% byte** also counts as "done-fuzzy."

**ANTI-GAMING GUARANTEES:**
1. `matched_functions` (strict, whole-binary) stays the immutable north star; fuzzy is secondary/diagnostic.
2. Fuzzy is byte/size-weighted, so a 30%-wired fn contributes only `0.30 × size`, never a full point.
3. The primary fuzzy goal uses the **wired** denominator, so wiring no-oracle junk at low % cannot inflate it.
4. Honesty gates compose: `icf_alias_check.py` (>44 B real-body, byte-eq) PLUS a new fuzzy-gain gate that rejects credit from ICF-stub/attribution-orphan fragments.
5. Before adopting as official, run a **determinism check** that `match_percent_normalized` is stable across objdiff-fork rebuilds (a noisy denominator would make the fuzzy headline jitter).

**IMPLEMENTATION:** `tools/fuzzy_progress.py` already exists — promote it to emit exactly this block (whole-binary + wired + RB3-specific tiers, the bands, the credit ledger) and have the wave-loop SOP close print it alongside `matched_functions`. ~40 lines of additive python; all inputs already in `report.json`.

---

## 5. IDEAS — the path forward (EV-ranked, with effort)

> Effort is honest and uncertain. "Days/Weeks" are coordinator-time estimates, not guarantees. The two big unknowns — whether a body-port can be pushed past the 47.8% ceiling, and the case-B fork's *real* ceiling under the divergence wall — are explicitly called out as things to *measure*, not assume.

### Rank 1 — Operationalize a tiered fuzzy metric NOW · **Effort: trivial (days)** · EV: highest
Promote `fuzzy_match_percent` to a co-equal headline; commit/wire `tools/fuzzy_progress.py` to print the three headline lines, the staircase, the fuzzy-credit ledger, and the wired + RB3-specific sub-goals. Wire into the wave-loop SOP close. *Why:* directly answers "the system NEEDS to be fuzzy"; zero new computation; instantly makes a 0→85% body-port register as progress; it is the dense reward signal every downstream idea depends on. Keep strict as the immutable north star to resist gaming.

### Rank 2 — Run the MemTemp-RAII keystone (Wave-21) as a clean go/no-go · **Effort: low (days)** · EV: high
Execute `fn_82797500` no-arg RAII, ~36 `bl` callers (`roadmap:2047`), as a short standalone keystone wave with composed whole-binary A/B. *Why:* the only remaining un-tested tractable byte-exact lever; its outcome is the literal go/no-go for whether *any* cheap strict matching remains. Decisive information value regardless of outcome — if it walls, that confirms full practical exhaustion and justifies pivoting all effort to the fuzzy/reconstruction track with a clear conscience.

### Rank 3 — Build the reconstruction workbench · **Effort: medium (weeks)** · EV: high (gated on rank 1)
One tool `tools/workbench.py` that, per function, fuses a dossier: (1) rb3-Wii body via `unified_id_rb3wii.json` `bindiff_src`+`wii_name` (logical template), (2) Ghidra retail decompiled C via `tools/ghidra/ghidra-decompile.py` port 8002 (the **only** behavioral ground truth for RB3-specific code), (3) `classify_nearmiss` per-instruction divergence buckets, (4) `scripts/recon.py` struct field-access map, (5) `report.json` `fuzzy_match_percent`. Feed it to a bodyport-style agentic edit→rebuild→re-measure loop. ~80% reuse (`classify_nearmiss.py` + `recon.py` + the ghidra client + the oracle all exist). **First**, run `classify_nearmiss` across all 64 kept BandProfile functions to get the divergence root-cause breakdown — this decides whether the workbench is a *matching* tool or a *fuzzy-credit* tool. *Caveat:* Ghidra MCP (port 8002) is single-process and not currently running; a fan-out wave needs a serialization/lock layer around it.

### Rank 4 — Pilot ONE small scattered TU end-to-end under workbench + fuzzy · **Effort: medium (weeks)** · EV: medium-high
Take **SongSortNode (53 fns)** or **LockStepMgr (26 fns)** — *not* BandProfile's 104 — through the workbench with the case-B fork wired in a worktree. *Why:* replaces the theoretical +150–220 case-B ceiling with a *measured* one, and tells you whether the workbench delivers matches or only fuzzy-credit, **before** committing to attrition-prone large-TU ports.

### Rank 5 — MWCC→MSVC mechanical codemod + revision-match predictor · **Effort: low-medium (days-weeks)** · EV: medium
(a) Codemod the ~6 deterministic patterns from `meta_band-port-breaking-changes.md` (strip `#pragma force_active/pool_data/dont_inline`, `Symbol::mStr→.Str()`, `DataArray::mSize→.Size()`, `operator new(unsigned long)→size_t`, `std::vector<T,unsigned short>→std::vector<T>`, ref/ptr global flips) and wire it into `fingerprint_pipeline.py scaffold` so a scaffolded TU is closer to compiling. (b) Add a pre-port **revision-match predictor** that samples a few oracle-named fns and quick-fuzzy-diffs the rb3-Wii body vs the retail target to flag likely-divergent (BandProfile-class) TUs *before* paying the full port cost. *Why:* attacks the manual-translation tax and the bimodal-yield blind spot (currently only discoverable *after* compiling).

### Rank 6 — Harvest thirdparty as a low-risk mechanical side-vein · **Effort: low-medium (incremental weeks)** · EV: medium
Target thirdparty (10124 fns / 1.31 MB, 7.29% matched) with public upstream source under the same MSVC-X360 compiler — no cross-compiler divergence, no oracle gymnastics. *Why:* the cheapest remaining oracle-backed (source-exact) inventory; no wave has touched it. Pure strict matches that grow the honest count, running concurrently with the research track.

### Rank 7 — Repopulate the unicorn behavioral verifier + improve same-ISA identity · **Effort: medium (weeks)** · EV: medium
(a) Re-wire and run the ported unicorn runner so `decomp.db` unicorn columns are populated here — making **behavioral equivalence** (EQUIVALENT/DIVERGENT + logic/build_env/regalloc class) a first-class "done-fuzzy" criterion for MWCC-ported bodies. *Unknown:* whether the DC3 unicorn runner is runnable in-repo (verify first — no `scripts/unicorn/` found; `recon.py` imports it lazily). (b) Improve rb3-Wii↔RB3-360 identity recall with CFG-shape + call-graph-neighbor + constant/string co-occurrence features (both are PowerPC, so same-ISA features transfer — unlike the cross-arch BSim that closed negative), keeping the >44 B real-body `icf_alias_check` gate to dodge the coverage-stub mirage. *Why:* behavioral equivalence is the natural real success criterion when byte-exact is off the table; identity is the layer every other idea depends on.

### Rank 8 — DEFER (do not do now): land case-B as default, or build a DC3-360 BinDiff body oracle · EV: negative-if-now
Keep the case-B fork BANKED (+0 honest, /tmp only) until rank 4 proves a scattered body can be made byte-exact — its ceiling is unvalidated and likely ~0 under the divergence wall. A second DC3-360↔RB3-360 BinDiff body oracle is **engine-only** (DC3 lacks game TUs) and the engine is at-limit (the `dc3_residual_rank` vein is drained, ceiling ~57 fns) — marginal over the existing `dc3_content_match` + `global_fuzzy_pairs`. Both are real but downstream/limited; sequencing them early burns effort on a theoretical ceiling and a pre-solved engine surface.

### Recommended sequence
**Adopt the fuzzy metric now (rank 1, cheap) → run the MemTemp-RAII go/no-go (rank 2) → build the reconstruction workbench (rank 3) → pilot ONE small scattered TU under workbench + fuzzy + case-B (rank 4) → only then integrate the case-B fork as default.** Side-vein: thirdparty mechanical sweep (rank 6) runs concurrently throughout.

---

## 6. Risks, unknowns & verification debt

- **`scope_map.py` matched counts are stale** (a cached snapshot that lags the live `report.json`); its tier *ratios* are the structural picture, but the per-tier matched counts should be regenerated before being quoted as exact. Use `report.json measures.matched_functions = 9793` as authoritative.
- **The case-B fork's +150–220 ceiling is unvalidated against the divergence wall.** It counts by byte-equality only and the one ported scattered TU is 0/64 byte-exact, so its true EV may be ~0 until body-divergence is solved. Rank 4 measures this.
- **The unicorn behavioral runner is ported but UNPOPULATED here** and its runnability in-repo is unconfirmed — the biggest missing piece for a behavioral fuzzy workbench.
- **Ghidra MCP (port 8002) is not currently running** and is single-process (ClosedException under concurrent agents); a fan-out workbench needs a serialization layer.
- **The rb3-Wii oracle is not regenerable from checkout** (no committed Ghidra→BinDiff→JSON script for that leg); it is a frozen artifact that `identity_transfer` + `icf_alias_check` + the case-B fork all depend on.
- **`match_percent_normalized` determinism across objdiff-fork rebuilds is unchecked** — a noisy denominator would make the fuzzy headline jitter. Run a determinism check before adopting fuzzy% as official.
- **The 7171 named >44 B fns at fuzzy 0 are uncharacterized** (scattered-TU methods awaiting a port vs methods walled by revision/struct). A classifier pass (oracle-named + does-our-obj-define-it) would size the true port backlog vs the dead surface.
- **The BandProfile body-divergence has no measured root-cause breakdown** (MWCC-vs-MSVC instruction selection vs struct-layout vs inline-policy). Without it, it is unknown how much a fix-loop can close vs what only the fuzzy metric can credit. Rank 3's first step measures this.

---

## Appendix A — verified numbers & provenance

| Figure | Value | Source |
|---|---|---|
| matched_functions | 9793 / 65547 (14.94%) | `build/45410914/report.json` `measures` |
| matched_code | 836,420 / 11,057,676 (7.564%) | same |
| fuzzy_match_percent | 10.066% | same |
| matched_data | 16 / 4,118,360 | same |
| total_units | 1711 | same |
| Fuzzy bands (whole binary) | 100=9793, [95,100)=1352, [90,95)=207, [80,90)=101, [50,80)=160, (0,50)=154, 0=53780 | live-computed from `report.json` |
| Wired set (n) | 11,744 | `report.json` (fns with fuzzy field) |
| Wired fuzzy-code completion | 95.2% (vs 72.8% strict) | live-computed |
| Staircase | N≥100=9793, N≥95=11145, N≥90=11352 | live-computed |
| RB3-specific (band3+network) | 4688 fns; 2642 wired (2225@100, 313 near-miss, 54 mid, 50 low), 2046 unwired; 91.9% fuzzy completion | live-computed |
| rb3-Wii oracle | 9301 entries; sim min 0.0, median 0.246, max 1.0; 708 (7.6%) ≥0.5 | `unified_id_rb3wii.json` |
| Scattered TUs (>2 MB span) | 610 TUs / 9146 fns | live-computed from oracle |
| meta_band | 173 TUs / 2813 fns; 159 span >1 MB | same |
| BandProfile wall | 1013 lines ported, 115 obj symbols, 0/64 at 100% (best 47.8%, ctor 1.7%) | `roadmap:1823-1827` |
| Wave-20 close | +1; "PRACTICAL EXHAUSTION" | `roadmap:2025,2058` |
| objdiff case-B fork | `../objdiff` branch `caseb-global-byteeq @ b1c92be` | `git -C ../objdiff` |
| jeff truncation fix | `../jeff` `@ 39e482f` | `git -C ../jeff` |
| Cost-per-match trend | W9 +723 → W12 +103 → W19 +16 → W20 +1 | roadmap wave closes |

*Note on minor survey-vs-rederived discrepancies:* the survey reported "576 TUs >2 MB" and "wired fuzzy 95.5%"; the re-derived figures are **610 TUs** and **95.2%** (same direction, used here). `matched_code` strict-byte sum over 100% functions came to 848,620 in the raw recompute vs the `measures.matched_code` of 836,420 — the small gap is unit-level strict-matched-code bookkeeping vs a naive per-fn sum; the authoritative headline is `measures.matched_code = 836,420`.

## Appendix B — the MWCC→MSVC breaking-changes catalog

The deterministic-pattern codemod spec (rank 5a) is the recurring-fixes list in **`docs/plans/meta_band-port-breaking-changes.md`** (3 sessions, ~35 TUs): pragma strips (`force_active`/`pool_data`/`dont_inline`), `Symbol::mStr → .Str()`, `DataArray::mSize → .Size()`, `operator new(unsigned long) → (size_t)`, `std::vector<T,unsigned short>` 2nd-arg drop, ref/ptr global flips (`TheUI`/`TheContentMgr`), signature divergences (`TheLocale`, `Achievements::Submit`, `Data::Print indent`), `revolution/OS.h` substitutions, and the deep-include-cascade stub list (with the TUs *excluded* for 15+ missing headers).
