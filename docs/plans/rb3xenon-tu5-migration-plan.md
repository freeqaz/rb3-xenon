# rb3-xenon: base(TU0) → TU5 Migration Plan

**Status:** APPROVED PLAN (synthesis of Lane A acquisition, Lane B anchoring impact, Lane C mapping spike)
**Date:** 2026-07-07
**Owner docs:** `tu5-acquisition.md` (Lane A), `tu5-anchoring-impact.md` (Lane B), `base-to-tu5-map-spike.md` (Lane C)

---

## Executive summary

- **GO** on `/srv/torrents/games/arbys/rb3/default.xex` — proven CLEAN RETAIL RB3 **Title Update 5** (v0.0.5.1), sha1 `c5a17091cb44c0119424390a1738d161995e430e`, 13,971,456 B. Zero mod/DX/Enhanced markers, retail import set, TU5 build date (Sep 02 2011). Ingestion-ready; no fallback needed.
- **ARCHITECTURE: full re-base to TU5.** The work product (matched `src/`, `objects.json`, `config.*`, 12,191 named `decomp.db` rows) is **name/source-keyed → portable** and survives untouched. Only the machine-generated, absolute-VA-keyed **target-description layer** (`splits.txt`, `symbols.txt`, `scope_map.json`, `target_symbol_map.json`, all evidence JSON, the Ghidra base program) must be regenerated. Dual-target and overlay/delta both rejected (double CI with no consumer for the base half; no clean delta exists — TU5 changed bodies+sizes).
- **Mapping feasibility: >95% exact/structural** once the full pipeline runs. Read-only skeleton spike alone located+structurally-matched **17/25 (68%)** and present-with-identical-body **22/25 (88%)** on an adversarial sample; **every function ≥128 B matched (14/14)**. Manual work concentrates on the ~8–12% genuinely-changed set plus tiny (<128 B) common-skeleton leaves needing caller-context disambiguation.
- **IMMEDIATE WIN (unblocks shipping the same-instrument patch):** 6 of 7 patch functions re-located on TU5 with HIGH confidence + identical bodies; 1 (`IsActive`) is entry-hookable but its body changed — detour must be re-derived on TU5. Free TU5 cave at **0x82C55010** (29,684 B file-backed zeros). See §4.

**Recommended first step:** run the content-hash pass to produce `base_to_tu5_map.json` — it is the single mechanism that gates the splits remap, the `target_symbol_map` rewrite, and every evidence-JSON re-key.

---

## 1. GO/NO-GO + architecture decision

### 1.1 Canonical TU5 XEX — GO

**Adopt:** `/srv/torrents/games/arbys/rb3/default.xex`
- sha1 `c5a17091cb44c0119424390a1738d161995e430e`, 13,971,456 B, v0.0.5.1, enc=0, retail/uncompressed-basefile/unencrypted.
- Cleanliness: 0 injected sections (12, identical names+order to base), identical import libs (`xam.xex`,`xboxkrnl.exe`), identical 18 optional-header keys, identical 15 static libs+versions. String scan for RB3DX/RB3Enhanced/RB3E/Xenia/`.dll`/`AllowSameInstrument`/`rb3.ini` → 0 hits each. `deluxe/forge/custom` hits are legit retail asset paths present in the base too.
- Same `image_base 0x82000000`, `pe_off 0x3000` as base → VA math transfers. `dtk xex extract` yields valid MZ/PE, 12 sections, entry decodes sane PPC (`mflr r12`).
- **The disc zip is NOT TU5** — it carries the encrypted vanilla base (v0.0.0.1, sha1 `d57b7df5…` == `default_vanilla.xex`). Do not use it as the TU5 source.

**CRITICAL mapping correction (measured, Lane A):** the `/srv` TU5 xex is a *normal basic-compressed* retail XEX. Flat `0x3000+VA` addressing **DRIFTS and returns WRONG bytes on TU5** — proven: TU5 entry `0x8283cd20` = `7d8802a6 mflr` via dtk section-map but garbage `4bfe4588` via flat. **All TU5 VAs must be read via dtk PE section-mapping, never flat.** `_tu5probe/FINDINGS.md`'s flat-mapped TU5 words are unreliable and must not be propagated. The base xex is pre-flattened (flat==section), which is why the existing flat patcher works there. base=TU0 conclusion still holds and is cleaner under correct mapping.

### 1.2 Architecture — full RE-BASE (chosen)

The project splits into two layers with **opposite portability** (Lane B central finding):

| Layer | Keying | Portable? | Examples |
|---|---|---|---|
| **Work product** | name / source | **YES — survives untouched** | matched `src/` C++, `objects.json` (774 files, verified **0 VA refs**), `config.json`, `config.yml`, 12,191 real-named `decomp.db functions` rows (PK = `symbol TEXT UNIQUE`, **no addr column** — corrects the prior "keyed by base addr" claim) |
| **Target description** | absolute VA | **NO — must rebuild** | `splits.txt` (3870 lines), `symbols.txt` (251k lines), `scope_map.json` (8.2 MB), `target_symbol_map.json` (13,846), all evidence JSON (`ghidriff_identities` 978, `global_fuzzy_pairs` 2000, `dc3_content_match` 394/5029, `unified_id*` 9301+, `fingerprints` 61,618, `autoid` 511), Ghidra base program |

The migration is therefore **"regenerate the machine layer, re-attach the human layer by name"** — NOT "redo the decomp on TU5." Nothing expensive is lost.

**Rejected alternatives:**
- **(B) Dual-target (keep base + add TU5):** doubles CI cost; the base half has no downstream consumer (all consumers — RB3E, the same-instrument patch, players — are on TU5). No benefit.
- **(C) Overlay/delta (patch base description with a TU5 diff):** TU5 changed function bodies AND sizes; there is no clean structural delta. Would make the *shipping* target a derived artifact of a build nobody runs. Fragile.

**Why re-base is cheap here:** the base→TU5 problem is the *easiest* cross-binary problem the project owns — same source recompiled by the same toolchain at the same `/O1` flags. It reuses the content-hash `masked_sha` matcher verbatim; expected >95% exact/structural. Baseline to preserve through the migration: **11,240 matched_functions (17.13%)**.

---

## 2. Phased migration plan

Each phase notes: what · mechanical/manual · effort · tooling · output artifact.

### Phase 0 — Ingest TU5 XEX into a worktree
- **What:** `cp` TU5 → `orig/45410914/default.xex` in an isolated worktree; `dtk xex extract` → `band.exe` (MZ/PE, 12 sections). Stand up the Ghidra TU5 program from the extracted PE.
- **Mechanical.** Effort: ~2 h.
- **Tooling:** `dtk xex extract`, `tools/setup-worktree.sh`, Ghidra import.
- **Output:** `orig/45410914/{default.xex,band.exe}`, TU5 Ghidra program. **Gate:** entry disassembles as sane PPC; section table matches Lane A (12 sections, base 0x82000000, pe_off 0x3000).

### Phase 1 — Build `base_to_tu5_map.json` (CRITICAL PATH)
- **What:** map every base function → its TU5 counterpart. Staged, best-signal-first: (1) content-hash / `masked_sha` exact match (relocation-normalized body hash) → the bulk; (2) ghidriff / BSim structural for regalloc-drifted survivors; (3) fingerprint (61,618-entry corpus) + skeleton-uniqueness (`_tu5probe/skel_match.py` method) for residue; (4) **set-difference → the small TU5-changed worklist** (functions with no confident mapping).
- **Mostly mechanical / automated**, with manual adjudication only on the residue and the changed-set.
- Effort: **~2–3 days** (dominated by compute + adjudicating ambiguous tiny leaves). This is the gate for Phases 2–4.
- **Tooling:** existing content-hash matcher, ghidriff/BSim (`sim×conf≥15` axis, human-judged 0.900 band from the ghidriff-divergence work), fingerprint transfer, `skel_match.py`.
- **Output:** `base_to_tu5_map.json` (base VA → TU5 VA + confidence + method) and `tu5_changed_worklist.json` (the ~8–12% needing manual attention).

### Phase 2 — Remap `splits.txt` to TU5
- **What:** rewrite each split's absolute VA range to its TU5 range via the Phase-1 map; boundary-snap with the existing relocation tools.
- **Mechanical** (map-driven), with manual snap-review at changed-set boundaries.
- Effort: ~1 day.
- **Tooling:** `relocate_*_splits.py` boundary-snap scripts, `base_to_tu5_map.json`.
- **Output:** TU5 `splits.txt` (3870 lines re-anchored). **Gate:** `dtk xex split config.yml build/45410914` carves clean per-unit target `.obj`s with no overlap/gap errors.

### Phase 3 — Regenerate `symbols.txt` + rewrite VA-keyed metadata
- **What:** dtk-regenerate `symbols.txt` from the TU5 xex; rewrite `target_symbol_map.json` (13,846), `scope_map.json` (8.2 MB), and re-key every evidence JSON (`ghidriff_identities`, `global_fuzzy_pairs`, `dc3_content_match`, `unified_id*`, `fingerprints`, `autoid`) through `base_to_tu5_map.json`. `decomp.db` name-keyed rows need NO change; if any addr-bearing sidecar tables exist, re-key by the map.
- **Mechanical** (dtk + map-driven re-key script), manual only where a base VA had no TU5 mapping.
- Effort: ~1–1.5 days.
- **Tooling:** `dtk`, a re-key pass over each JSON keyed on `base_to_tu5_map.json`.
- **Output:** TU5 `symbols.txt`, re-keyed `target_symbol_map.json`/`scope_map.json`/evidence JSONs.

### Phase 4 — Re-point build + objdiff, re-verify match%
- **What:** update `config.yml`'s one `object:` line to the TU5 xex/build dir; `configure.py` + `tools/ninja-locked`; run objdiff over all 2456 units. Base MSVC-compiled objs are **untouched** — only the target side moves.
- **Mechanical.** Effort: ~0.5 day + iteration.
- **Tooling:** `configure.py`, `ninja-locked`, `objdiff-cli`.
- **Output:** TU5 `report.json`. **Gate:** matched_functions ≥ 11,240 (17.13% floor). A drop localizes to the changed-set from Phase 1 and is worked as normal decomp.

### Phase 5 — Re-baseline the changed-set + re-verify consumers
- **What:** work the `tu5_changed_worklist.json` (genuinely rewritten TU5 bodies) as ordinary decomp targets; re-derive any patch detours whose bodies changed (see §4, `IsActive`). Re-verify RB3E-adjacent consumers point at TU5 addresses.
- **Manual** (real decomp on the small changed-set).
- Effort: scales with the changed-set size (~8–12% of touched fns, but only a fraction are patch-relevant); the same-instrument patch itself is §4 and can ship ahead of this phase.
- **Output:** restored/advanced match% on TU5; shippable TU5 same-instrument patch.

---

## 3. Base→TU5 mapping feasibility verdict

**PROVEN** by the read-only Lane C skeleton spike (`_tu5probe/skel_match.py`): relocation-normalized opcode skeletons (mask `bl` 24-bit target, `bc` displacement, D-form 16-bit imm/disp; keep opcodes+registers) locate a base function as a unique contiguous run in TU5 `.text`. Unique hit = same body, shifted addresses. This is a strict *lower bound* (misses regalloc drift, which content-hash/ghidriff recover downstream).

Adversarial 25-fn sample (6 patch + 8 sizable + 5 tiny getters + 1 fragment + 1 early-CRT):
- **14 HIGH-unique (56%)**, 3 near-unique MED (12%), 5 tiny-common needing caller-context (20%), 3 changed/miss (12%).
- Located + structurally-identical by skeleton alone: **17/25 (68%)**.
- Present with identical body incl. resolvable tiny: **22/25 (88%)**.
- Genuine TU5 change: **~8–12%** — only `IsActive` is a true rewrite; the other two "misses" are a BB-fragment sampling artifact and an early-CRT reorder.
- **Every sizable non-fragment function (≥128 B) matched — 14/14, including all located patch fns.**

**Extrapolation:** >95% exact/structural once the full pipeline (content-hash → ghidriff/BSim → fingerprint) closes tiny-function ambiguity and isolates the changed-set — consistent with Lane B's target.

**Where manual work concentrates:**
1. Tiny (<128 B) common-skeleton leaves (getters/thunks) — disambiguated by caller-context, not body.
2. The ~8–12% genuinely-changed TU5 bodies (Phase 5 decomp).
3. Boundary-snap review at changed-set edges in `splits.txt` (Phase 2).

---

## 4. THE IMMEDIATE WIN — same-instrument patch on TU5

Re-targeted addresses (read via dtk section-mapping, per §1.1 correction). All 6 located-HIGH share `7d8802a6 mflr` on both builds; the only differing prologue words are shifted save-thunk `bl` targets — the reloc signature confirming identity.

| Function | base (TU0) VA | **TU5 VA** | Confidence |
|---|---|---|---|
| ResolvePartWaitStates | 0x8259D948 | **0x825AE488** | HIGH, identical body |
| ProcessConfig | 0x8274ACF8 | **0x82767A08** | HIGH, identical body |
| RecalcGemList | 0x8276FBB0 | **0x8278C740** | HIGH, identical body |
| GameGemDB::Duplicate | 0x8276E590 | **0x8278B2C8** | HIGH, identical body |
| GameGemDB::CopyFrom | 0x82769450 | **0x82786168** | HIGH, identical body |
| GameGemDB::GetDiffList | 0x8276E010 | **0x8278B1C8** | MED-HIGH, cluster-disambiguated leaf |
| IsActive | 0x8264B5F8 | **0x826604C0** | Entry hookable (`7d8802a6 mflr` on TU5), but **BODY CHANGED (~56% sim) — detour logic must be RE-DERIVED on the TU5 program** |

**Free TU5 cave:** base cave `0x82C25000` is live code in TU5 — do not reuse. New TU5 cave = **0x82C55010**, 29,684 B of file-backed zeros in the executable BINK-section tail. Write directly with `xex_binpatch.py`; leave slack — start detour bytes ~**0x82C55800**. Alternate = BINK→BINKBSS gap `0x82C5D010` (12,272 B, NOT file-backed — avoid unless a file-backed region is exhausted).

**Patcher gotcha (must fix before writing bytes):** `tools/xex_binpatch.py` assumes a FLAT image. On the basic-compressed TU5 xex this is wrong (§1.1). Either (a) flatten the TU5 image first, then reuse its flat-offset math, or (b) rework the patcher to section-map. Re-derive cave + detour VAs from the dtk-extracted TU5 PE, not from flat `0x3000+VA`.

**Confidence + what still needs byte-verification:**
- 6/7 HIGH/MED-HIGH located with matching relocation-normalized bodies. **Before shipping**, byte-verify each detour site's exact prologue via dtk section-map extraction (not flat) and confirm the hook-point instruction is what the detour expects.
- `IsActive` (the 7th): re-derive its detour logic against the TU5 body — do not port the base detour blindly.
- Confirm the chosen cave region (`0x82C55010`+slack) is file-backed zeros in *this* TU5 xex before writing.

This deliverable is independent of Phases 1–5 and can ship first.

---

## 5. Risks + recommended first step

**Risks**
- **Flat-mapping contamination** (HIGH likelihood, HIGH impact): any downstream reuse of flat `0x3000+VA` TU5 addresses (incl. `_tu5probe/FINDINGS.md`'s flat words) yields wrong bytes. *Mitigation:* section-map via dtk everywhere; quarantine the flat FINDINGS words.
- **Patcher flat-image assumption:** `xex_binpatch.py` will silently patch the wrong offsets on TU5. *Mitigation:* flatten-first or section-map rework (§4), gated on a byte-verify of one known site.
- **Tiny-function ambiguity** (MED): common skeletons cause mis-maps in `base_to_tu5_map.json`. *Mitigation:* require caller-context/fingerprint agreement before accepting a <128 B mapping; the build itself is the precision gate (a wrong map → objdiff drop localizes it).
- **Changed-set underestimate:** if TU5 changed more bodies than the ~8–12% sample implies, Phase 5 grows. *Mitigation:* Phase 1 set-difference gives the exact count before committing effort.
- **Duplicate-port / concurrent-lane collision** (seen historically): the `_tu5probe` scratch already holds another lane's `remap_result.json`/`skel_match.py`. *Mitigation:* isolated worktree, checkpoint-commit-first, re-grep wiring on current main before any merge.

**Single recommended first step:** **Run Phase 1's content-hash pass to produce `base_to_tu5_map.json` + `tu5_changed_worklist.json`.** It is the critical path and the gate for the splits remap, the `target_symbol_map` rewrite, and every evidence re-key — and its set-difference output sizes the entire remaining effort before any manual work is committed. Ingest the TU5 xex into a worktree (Phase 0) as the immediate prerequisite.
