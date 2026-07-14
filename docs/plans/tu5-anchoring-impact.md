# rb3-xenon base(TU0) → TU5 — ANCHORING IMPACT (Lane B, 2026-07-07)

Read-only investigation of rb3-xenon. Companion to `tu5-migration-scope.md`
(the end-to-end plan) — this doc drills the **anchoring architecture**: exactly
how VAs flow through every artifact, which break on a version change, and the
recommended migration architecture with steps. Nothing in the decomp was mutated.

Premise (from the prior probe, unchanged): the decomp target
`orig/45410914/default.xex` is **TU0 / v0.0.0.1**, not TU5; `/srv/.../rb3/default.xex`
is clean retail **TU5 / v0.0.5.1**. See `_tu5probe/FINDINGS.md`.

---

## 0. The load-bearing distinction: WORK PRODUCT vs TARGET DESCRIPTION

The single most important finding for the architecture decision — and a
**correction to the prior plan** (which called `decomp.db` "keyed by base addr"):

The project splits cleanly into two layers with opposite portability:

- **WORK PRODUCT — NAME/SOURCE-keyed → PORTABLE across versions.** The actual
  human output: the matched C++ in `src/`, the per-file status in
  `config/45410914/objects.json` (100% file-path-keyed, **zero VA references**
  confirmed), and the named-function verdicts/attempts in `decomp.db`
  (`functions` table PK is `symbol TEXT UNIQUE` — a *name*, not an address
  column). None of these carry an absolute VA. They survive a re-base intact as
  long as the TU5 function still exists under the same name.
- **TARGET DESCRIPTION — absolute-VA-keyed → BREAKS on version change.**
  The machine-generated map that ties names to the *specific bytes of TU0*:
  `splits.txt`, `symbols.txt`, `scope_map.json`, the Ghidra base program, and
  the whole evidence-JSON family. All must be re-anchored to TU5's byte layout.

The migration is therefore **not** "redo the decomp on TU5" — it is "regenerate
the target-description layer for TU5 and re-attach the portable work product to
it by name." That is a far smaller, mostly-automatable job, and it is what makes
full re-base (below) the right call.

Caveat on `decomp.db`: 57,550 of its 69,741 `functions` rows are named
`fn_<ADDR>` (the VA is embedded *in the name*), and only 12,191 have real
symbol names. So the un-named majority is VA-keyed-via-name and effectively
re-seeds from TU5's dtk output anyway; the **12,191 real-named rows are the
portable asset**. `source_patch` column is empty (0 rows) — the matched code
lives in `src/`, not the DB, so the DB is metadata (verdicts, attempt history,
pattern flags), portable by name, non-critical if partially lost.

---

## 1. Artifact-by-artifact re-anchor impact table

Counts measured this session. "Keying" = what a row is addressed by.

| Artifact | Keying | Size / count | Portable? | Re-anchor action |
|---|---|---|---|---|
| `src/**/*.cpp` (matched source) | source file + symbol | — | **PORTABLE** | None. The decomp itself. Re-attaches by name. |
| `config/45410914/objects.json` | file path → status | 774 files, 2456 units | **PORTABLE** | None (verified: 0 VA refs). |
| `config/45410914/config.json` | compiler-flag names | small | **PORTABLE** | None. |
| `config/45410914/config.yml` | file paths | 1 `object:` line | **PORTABLE** | Point `object:` at the TU5 xex. 1-line edit. |
| `decomp.db` real-named fns | `symbol` (name) | 12,191 | **PORTABLE** | Keep verdicts/attempts by name; drop rows whose TU5 body changed → re-open. |
| `decomp.db` `fn_<ADDR>` fns | VA-in-name | 57,550 | must-rebuild | Re-seed from TU5 `report.json` via `scripts/ingest_report.py`. |
| `decomp.db` `decompilations` | `symbol` PK + `address` | 238 | mostly portable | Ghidra-C cache; `address` col re-keys, code re-fetch cheap. |
| `config/45410914/splits.txt` | `.text/.pdata start:VA end:VA` | 3870 lines / 774 units | **MUST REBUILD** | Mechanical VA-remap via base→TU5 map (§3); dtk re-derives. |
| `config/45410914/symbols.txt` | `sym = section:0xVA` | 251,332 lines (103k `.text`) | **MUST REBUILD** | dtk **regenerates** from TU5 analysis; re-apply named syms from map. Do NOT hand-remap. |
| `config/45410914/scope_map.json` | bare VA (`82260000`) | 8.2 MB | **MUST REBUILD** | Regenerate from TU5 dtk scope analysis (VA-dense). |
| `scripts/target_symbol_map.json` | `0xVA → mangled` | 13,846 | must-rebuild | Remap keys base→TU5 (value/name invariant; only key moves). |
| `report.json` | build output | — | regen | Auto-regenerates on the TU5 build. |
| `fingerprints.json` | VA-keyed | 12 MB / 61,618 | regen | `fingerprint_match.py extract` on TU5 asm (cheap). |
| `autoid.json` | `fn_<ADDR>` | 511 | regen | Re-extract string-anchors on TU5. |
| `ghidriff_identities.json` (+`_loose`) | `rb3_addr` | 978 | must-rebuild | Re-key via base→TU5 map; Wii-side identity invariant. |
| `global_fuzzy_pairs.json` | `rb3_addr` | 2000 | must-rebuild | Re-key via map, or re-run fuzzy vs TU5. |
| `game_content_match.json` | `rb3_addr` + `masked_sha` | 394 | **re-run (cheap)** | Byte-hash match vs TU5 — authoritative, feeds the map (§3). |
| `dc3_content_match.json` | `rb3_addr` + `masked_sha` | 5029 | **re-run (cheap)** | Same — the `masked_sha` machinery IS the map builder. |
| `unified_id_rb3wii.json` | `rb3_addr` | 9301 | must-rebuild | Re-key via map. |
| `unified_id*.json` (callgraph/rtti/vtable/…) | VA | ~7 files | must-rebuild | Re-key or regenerate against TU5. |
| Ghidra base program `RB3Xenon` | analysis on TU0 image | 1 program | **re-import** | Import TU5 xex as a 2nd program; keep TU0 for BinDiff transfer (§3). |
| `orig/45410914/default.xex` | the target itself | 15.4 MB | **swap** | Replace with TU5 (13.97 MB) on a worktree; human-gated on main. |

**Summary:** PORTABLE = the whole work product (src, objects.json, config.json,
config.yml modulo 1 line, 12,191 named DB rows). MUST-REBUILD/REGEN = every
byte-layout description (splits, symbols, scope_map, target_symbol_map) and every
evidence JSON, plus the Ghidra program. The rebuild is either **mechanical remap**
(splits, target_symbol_map, ghidriff/fuzzy/unified re-key) or **cheap regeneration**
(symbols via dtk, fingerprints/autoid via extract, content_match via byte-hash).

---

## 2. Build + objdiff re-point analysis

**How the target bytes enter the build (measured from `build.ninja` + `objdiff.json`):**

```
rule split:  dtk xex split  config/45410914/config.yml  build/45410914
```

`config.yml` names the xex + `splits.txt` + `symbols.txt`. dtk carves the xex's
`.text`/`.pdata` at each split range and emits a **target `.obj` per unit** into
`build/45410914/obj/…`. Separately, MSVC compiles `src/**` → **base `.obj`** in
`build/45410914/src/…`. `objdiff.json` pairs them per unit:

```
target_path: build/45410914/obj/MasterAudio.obj          ← carved from the xex
base_path:   build/45410914/src/system/beatmatch/MasterAudio.obj  ← compiled source
```

objdiff diffs target-vs-base **at the byte level over the split ranges**. So the
xex's bytes are the ground truth, addressed purely through `splits.txt` VAs +
`symbols.txt`.

**What re-pointing to TU5 requires:**

1. **New xex** (`config.yml object:`) → dtk now carves TU5 bytes.
2. **Re-anchored `splits.txt`** → the *only* thing that tells dtk *where* each
   unit's bytes live. TU5 moved every `.text` VA (base `.text`@`0x82260000`,
   TU5@`0x82270000`; whole-section divergence), so **all 3870 range lines move**.
3. **Regenerated `symbols.txt`** → dtk needs the TU5 symbol table to name carve
   points and resolve relocations.
4. **No change to base objs** → source compilation is untouched; objdiff simply
   re-diffs the same compiled code against the newly-carved TU5 target objs.

**Quantify / automation:** 774 source units, 2456 objdiff units, 3870 split
ranges. splits are remapped **mechanically** by the existing
`tools/relocate_game_splits.py` / `relocate_engine_splits.py` (they already do
cross-binary VA-pinning with *non-bisecting function-boundary snap* + *fail-closed
overlap guard* — exactly the invariants a re-base needs) once the base→TU5
address map (§3) exists. symbols.txt is **regenerated wholesale** by dtk. So the
per-unit human cost is ~zero; the whole re-point is driven by one artifact — the
base→TU5 map — plus a dtk re-run. The verification gate is objdiff itself: units
that re-point cleanly read 100% again; units whose TU5 body genuinely changed
drop to NonMatching and re-enter the normal loop (expected small — §3.4).

Current baseline to preserve/compare against (`report.json`): **11,240
matched_functions (17.13%), 8.69% code.** A correct re-base lands near this minus
the genuinely-changed set; a large drop signals a **mispair**, not a regression.

---

## 3. The base→TU5 map — the one mechanism that drives everything

TU0→TU5 is **the same source recompiled** (bug-fix title update, identical MSVC
`/O1 /Oi /EHsc` flags per `config.json`) → the vast majority of functions are
byte-identical modulo relocation. This is the *easiest* cross-binary identity
problem the project has (it already maps Wii↔Xenon and DC3↔Xenon across
compilers/arches). Build `base_to_tu5_map.json` (`base_VA → tu5_VA + method +
confidence`), ascending cost:

1. **Relocation-normalized byte-hash exact match (cheapest, authoritative).**
   The `*_content_match.json` files already carry `masked_sha` (reloc-masked
   instruction hash). Compute the same over every TU5 function; equal hash =
   identity. This is literally how `dc3_content_match.json` (5029) and
   `game_content_match.json` (394) were built — reuse that machinery. Expect the
   **large majority** of the 13,846 named base fns to map 1:1 (TU5 touched only a
   few TUs).
2. **ghidriff / BSim / BinDiff for the residue.** Point the existing pipelines
   base-program↔TU5-program (both PPC/MSVC → BinDiff scores far above the current
   cross-arch runs). Covers fns whose bytes shifted (moved callee reloc) but
   structure held.
3. **fingerprint_match string/const anchors** for anything still ambiguous.
4. **Genuinely-changed TU5 fns = `{base named} − {matched via 1-3}`.** Set-difference
   yields the precise TU5-edit worklist (expected tens–low-hundreds, concentrated
   in a few bug-fix/anti-piracy/network TUs). These keep their *names*, lose their
   *match*, and re-enter the loop.

The map then drives: the splits remap (via `relocate_*_splits.py`), the
`target_symbol_map.json` key rewrite, and the re-key of every VA-keyed evidence
JSON. Build `tools/remap_to_tu5.py` as a thin fork of the relocate tools (reuse
their boundary-snap + overlap guard verbatim). **Expected >95% exact/structural.**

---

## 4. Architecture recommendation: (A) FULL RE-BASE TO TU5

Weighed against the alternatives:

- **(A) Full re-base to TU5 — CHOSEN.** Replace the target; regenerate the
  target-description layer once; re-attach the portable work product by name.
  - *Preserves existing work:* the entire expensive human asset (matched `src/`,
    objects.json, 12,191 named verdicts) is name-keyed and survives untouched.
    Only machine-generated tables are rebuilt.
  - *Tooling reuse:* the content-hash matcher, relocate/snap tools, ghidriff/BSim/
    BinDiff, fingerprint_match, and `ingest_report.py` all apply directly — the
    map is the same class of problem they already solve, only easier.
  - *End goal alignment:* every downstream consumer — RB3Enhanced, the
    same-instrument patch, actual players — is TU5. The base binary has **no
    independent consumer**; keeping it as a live target is pure overhead.
- **(B) Dual-target (base + TU5 side by side).** Doubles build/CI (2×2456 units),
  splits agent attention and the DB namespace, and the base half serves nobody.
  **Reject.**
- **(C) Base primary + base→TU5 overlay/delta layer.** No clean offset delta
  exists — TU5 changed function *bodies and sizes*, not just positions, so an
  address-delta can't be applied at emit time without per-function identity
  anyway. Brittle, and it makes the shipping target (TU5) the *derived* one.
  **Reject as the primary model** — but its by-product, the base→TU5 identity
  map (§3), IS the migration mechanism for (A).

### Migration steps (architecture-level)

1. **Freeze TU0.** Git tag `target/tu0-frozen` on the current config for
   provenance. **Do not touch `orig/45410914/default.xex` on main** — the running
   base-patch test uses it. All work happens on a worktree/branch.
2. **Acquire + validate TU5** (belt-and-suspenders; §1 of scope doc). Stage as
   `orig/45410914/default.xex` on the worktree; decompress its PE for
   `va_disasm`/fingerprint use.
3. **Build `base_to_tu5_map.json`** (§3): content-hash → ghidriff/BinDiff →
   fingerprint → set-difference worklist. This is the critical path (~2–3 days,
   mostly automated).
4. **Regenerate the target-description layer** on the worktree:
   `remap_to_tu5.py` rewrites `splits.txt` + `target_symbol_map.json`; dtk
   regenerates `symbols.txt` + `scope_map.json`; re-key/re-run the evidence JSONs;
   re-import TU5 into Ghidra (keep TU0 program for BinDiff transfer).
5. **Re-attach work product by name.** `config.yml object:` → TU5; keep
   objects.json as-is; carry the 12,191 named DB verdicts forward; set the
   §3.4 changed set to NonMatching.
6. **Build + verify** (§2 gate): `configure.py && ninja-locked`; zero "Split ends
   within symbol" (boundary-snap guarantees); matched_functions ≈ 11,240 minus
   the changed set; `run_objdiff` spot-check on MasterAudio/RockCentral/Object →
   100% on TU5 objs.
7. **Land on `tu5-migrate` branch;** human-gated swap of the main `orig/` xex only
   after green. Re-derive the same-instrument patch on the TU5 program (separate
   effort — `tu5-migration-scope.md` §5; the map gives most of its ~15 addresses
   as table lookups, the cave moved so re-scan the TU5 section gaps).

### Effort

Anchoring re-key ~1 day tooling + runs; base→TU5 map ~2–3 days (critical path);
verification ~1 day. The map is the gate for everything else. Consistent with the
scope doc's ~1-week total (incl. acquisition + patch re-target).

---

## 5. Top anchoring risks

1. **Silent mispair in the base→TU5 map** → fake matches / dtk bisect errors.
   Mitigate: boundary-snap + fail-closed overlap guard (already in the relocate
   tools) + the matched-count gate (a *drop* is a mispair, not a regression).
2. **Under-counting TU5 code edits** → surprise NonMatching units. Mitigate: the
   explicit `{named} − {matched}` set-difference makes the changed set an
   enumerated worklist, not a surprise.
3. **Losing DB attempt-history** on the fn_<ADDR> majority. Low impact
   (`source_patch` is empty; real code is in `src/`), but preserve the 12,191
   named rows explicitly by name-join, not addr-join.
4. **Disturbing the running base patch test.** Worktree-only, TU0 frozen,
   main-`orig/` swap human-gated last.

---

## Appendix — measured facts (this session, read-only)

- `decomp.db functions`: 69,741 total; **PK = `symbol` (name), no addr column**;
  57,550 `fn_<ADDR>`-named, 12,191 real names; 4,661 COMPLETE; `source_patch`
  empty. `decompilations`: 238 rows (`symbol` PK + `address`).
- `objects.json`: 774 files across `main/engine/…`; file→status; **0 VA refs**.
- `config.yml`: `object: orig/45410914/default.xex` + splits + symbols (3 lines);
  header comment confirms the template was deliberately switched off
  `default_plus_TU5.xex` to vanilla.
- `splits.txt`: 3870 lines, 774 unit headers, `.text/.pdata start:VA end:VA`.
- `symbols.txt`: 251,332 lines, `sym = section:0xVA`.
- `scope_map.json`: 8.2 MB, bare-VA keyed.
- `objdiff.json`: 2456 units, `custom_make: tools/ninja-locked`, target=dtk-carved
  obj vs base=MSVC-compiled obj.
- `build.ninja`: `dtk xex split config.yml build/45410914` produces target objs.
- `report.json`: 11,240 matched_functions (17.13%), 8.69% matched code.
- Evidence JSONs keyed by `rb3_addr`/VA: ghidriff_identities 978 (+_loose),
  global_fuzzy_pairs 2000, game_content_match 394 (+masked_sha),
  dc3_content_match 5029 (+masked_sha), unified_id_rb3wii 9301, unified_id*
  family, autoid 511, fingerprints 61,618, target_symbol_map 13,846.
- Existing remap tooling: `tools/relocate_game_splits.py`,
  `tools/relocate_engine_splits.py` (non-bisecting boundary-snap + fail-closed
  overlap guard).
