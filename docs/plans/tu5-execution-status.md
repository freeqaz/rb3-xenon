# RB3 Xenon → TU5 Migration — Execution Status

**Date:** 2026-07-07 · **Synthesizer:** Opus · **Status:** P0 + P1 map + TU5 same-instrument retarget COMPLETE (worktree-only, nothing committed)

---

## Executive Summary

The TU5 migration keystone is built and validated. TU5 (v0.0.5.1) was ingested
side-by-side with TU0, a **section-mapped VA reader** was proven (flat `0x3000+VA`
addressing is forbidden on TU5 and is the exact source of the spike's uniform
**−0x8000** address error), and a full **base→TU5 function map** now resolves
**94.3% of all text functions** and **96.4% of named functions** (clears the >95%
target). On top of that map, the same-instrument gameplay patch was fully
re-targeted to correct TU5 addresses and a **byte-verified patched TU5 XEX** was
produced. What is not yet done is any runtime boot/gameplay confirmation and the
downstream decomp re-anchor (splits/symbols/db regen + the 81 genuinely-changed
bodies). Everything lives in the `tu5-migrate` worktree; main's base binaries and
`decomp.db` are untouched.

---

## 1. STATE

**Worktree:** `/home/free/code/milohax/rb3-xenon/.claude/worktrees/tu5-migrate`
(branch `tu5-migrate`, off main `a1312de`).
**TU0 frozen:** tag `target/tu0-frozen` = `e589bf5bc7c80457ab87123c7e18c1adf65c6357`.
Main base artifacts untouched (`band.exe` mtime Jul 5, `default.xex` May 25,
`orig/config/decomp.db/Ghidra` read-only).

**TU5 ingest (side-by-side, TU0 kept live for the map pipeline):**
- `orig/45410914/default_tu5.xex` — SHA1 `c5a17091cb44c0119424390a1738d161995e430e`, 13,971,456 B, v0.0.5.1, entry `0x8283CD20`
- `orig/45410914/band_tu5.exe` — dtk-extracted PE, 14,363,648 B
- TU0 `default.xex` + regenerated `band.exe` preserved

**Section-mapped reader:** `tools/tu5_va.py` (+ `tools/va_disasm_tu5.py`). Proven:
entry `0x8283CD20` → `7D8802A6 mflr r12` via section map, garbage via flat map.
TU5 `.text`: VA `0x82270000`, rawptr `0x264E00` (rawptr ≠ VA−base, so flat drifts).

**base→TU5 map — BUILT & VALIDATED** (`tools/tu5_map_build.py`, ~27s, reproducible):

| Metric | Value |
|---|---|
| Text functions resolved (named + `fn_`) | **61,629 / 65,357 (94.3%)** |
| Named `.text` denominator | 13,295 |
| **Matched (HIGH+MED, body_identical)** | **12,817 (96.4%)** — clears >95% |
| — HIGH (skeleton-unique anchors) | 8,855 |
| — MED (co-walk verified) | 3,962 |
| **Changed-set (AMBIG + MISS)** | **478** |
| — AMBIG (present, position unpinned; mechanical) | 397 |
| — **MISS (body genuinely diverged — real work)** | **81** |

Changed-set of 478 sizes the remaining decomp re-anchor; 333 are `<0x80`
small/dup getters, so true logic-changed cost ≈ **81 MISS bodies**.

Independent ground truth: 7 same-instrument anchors match P0 recovery (5 exact,
IsActive correctly MISS at ~56% body divergence, GetDiffList a `type:label` leaf
that decodes byte-exact); 5-fn spot-check all mirror TU0 prologues.

**Spike-address correction (load-bearing):** the same-instrument spike's 7 TU5
addresses were **ALL WRONG (uniformly −0x8000)** — each decoded as a
mid-function/epilogue. Correct section-mapped, reloc-normalized replacements were
recovered and verified as real prologues. Downstream tooling must consume the
correct-TU5 column, never the spike column.

**TU5 same-instrument patch — RE-TARGETED & BYTE-VERIFIED:**
- Patched XEX: `orig/45410914/default_tu5_patched.xex` — sha1 `a9fa9a91863cbe727377420bd6debe2790ffeac1`, 13,971,456 B
- TU5-aware tooling (base tools untouched): `RB3Enhanced/scripts/objcave_pack_tu5.py`, `tools/xex_binpatch_tu5.py` (per-section delta `.text +0x6200`, `.data +0xD400`), `patches/45410914_same_instrument_full_tu5.patch.toml`
- All detour/GAME_FN/CRT addresses re-derived & byte-verified vs `band_tu5.exe`; struct offsets confirmed identical to base
- Cave `= 0x82C8A000` (file-backed `.data` zero run), **corrected** from INGEST's `0x82C55010` (which section-maps into BINK code)
- Byte-verification passed: 4 detour sites were real `mflr r12` entries → now `b <cave>`; cave == blob (700 words, 0 mismatch); whole-file diff exact (592 changed = writes, header intact)

---

## 2. WHAT SHIPS NOW vs WHAT REMAINS

### Ships now
- A **bootable candidate**: the byte-verified patched TU5 XEX, pairable with the `/srv` TU5 gen ARKs, ready for a Xenia boot smoke test.
- The **reusable migration map** (`base_to_tu5_map.full.json`, 61,629 entries) that every downstream re-anchor phase (P2–P5) consumes to move base VAs → TU5 VAs.
- **TU5-aware tooling** (reader, packer, section-mapped patcher) — base tooling untouched, so TU0 work is unaffected.

### Remains unverified (runtime)
- **Xenia boot-spike** to confirm the `.data` cave executes with `writable_code_segments` (strict-NX/console targets would need an executable-section cave — none is file-backed on TU5).
- **No live global occupies the cave** (`0x82C8A000`) at runtime.
- **TU5 basefile hashes** for the toml; **runtime gameplay** confirmation of the same-instrument feature; **allocator symmetry** (MemFree path).

### Remaining decomp re-anchor phases (effort estimates)
| Phase | Work | Estimate |
|---|---|---|
| **P2 — splits remap** | Re-anchor `splits.txt` base VAs → TU5 via `base_to_tu5_map.full.json`; place the 397 AMBIG mechanically | ~0.5–1 day (scripted; AMBIG placement is the bulk) |
| **P3 — symbols regen** | Regenerate `symbols.txt` at TU5 VAs; carry names across the 12,817 matched | ~0.5 day (scripted off the map) |
| **P4 — build re-verify** | Point decomp.db/report at TU5, rebuild, confirm match% holds on unchanged bodies | ~1 day (config + first clean build) |
| **P5 — changed-set decomp** | Re-derive the **81 MISS bodies** (+ spot-audit AMBIG); IsActive-class detour re-derivation | ~3–6 days (the real matching work; 81 bodies at varied size/complexity) |

P2–P4 are largely mechanical and script-driven off the already-built map; P5 is
the genuine per-function effort. `decomp.db` stays read-only until P4 config is
staged and reviewed.

---

## 3. NEXT STEP

**Boot `orig/45410914/default_tu5_patched.xex` in Xenia paired with the `/srv` TU5
gen ARKs, with `writable_code_segments` enabled, and watch for the `.data`-cave
same-instrument hook to execute.** This is the single highest-value action: it
converts the byte-verified static artifact into a runtime-confirmed shippable
patch, closes the three open runtime unknowns (cave executes / no live global
collision / gameplay), and gates whether an executable-section cave is needed
before any effort is spent on the P2–P5 decomp re-anchor.

---

## Deliverable paths (worktree)
- `_tu5probe/tu5_migrate/base_to_tu5_map.json` — named-function records
- `_tu5probe/tu5_migrate/base_to_tu5_map.full.json` — `{base_va: tu5_va}` × 61,629 (P2–P5 input)
- `_tu5probe/tu5_migrate/tu5_changed_worklist.json` — 478 changed
- `_tu5probe/tu5_migrate/{ingest,p0,map,apply_tu5,verify_tu5}.json` + `base_to_tu5_map.seed7.json` — checkpoints
- `tools/{tu5_va.py, va_disasm_tu5.py, tu5_skel_recover.py, tu5_map_build.py, xex_binpatch_tu5.py}`
- `orig/45410914/{default_tu5.xex, band_tu5.exe, default_tu5_patched.xex}`
- `RB3Enhanced/scripts/objcave_pack_tu5.py`, `RB3Enhanced/build_patch/checkpoints/tu5retarget.json`
- Docs: `docs/plans/{base-to-tu5-map.md, same-instrument-tu5-retarget.md, tu5-execution-status.md}`

## Flagged (not fixed)
RB3Enhanced `ports_xbox360.h` mixes TU5 general ports with **BASE** same-instrument
addresses → the base packer would resolve MemFree/GetBandUserFromSlot to TU5 VAs
that are mid-function on the base binary (latent, uncaught by base selftest). The
TU5 artifact here uses correct TU5 VAs; the base header needs a separate fix.
