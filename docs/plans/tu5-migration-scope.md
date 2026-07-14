# rb3-xenon base(TU0)→TU5 migration — PLANNER scope (2026-07-07)

Read-only investigation. Probe data + raw findings: `_tu5probe/FINDINGS.md`,
`_tu5probe/xex_headers.json`, `_tu5probe/xex_probe.py`, `_tu5probe/word_at.py`.

## TL;DR verdict
- **rb3-xenon's decomp target `orig/45410914/default.xex` is TU0 / v0.0.0.1**, NOT TU5.
  Proven by RB3Enhanced `ports_xbox360.h` (its documented "360 TU5" `bl` sites resolve on
  the /srv TU5 xex, not on rb3-xenon's base). `config/45410914/config.yml` even records the
  original template used `default_plus_TU5.xex` and was deliberately switched to vanilla.
  **All the repo docs that say "TU5 confirmed" are wrong** (they conflated title-id + base
  + entry `0x82816080`, which is the *TU0* entry; TU5 entry is `0x8283cd20`).
- **/srv/torrents/games/arbys/rb3/default.xex = clean retail TU5 v0.0.5.1** (12 sections,
  no injected sections, no mod strings). Usable as the migration target once dtk-split.
- **The same-instrument patch's ~11 addresses + 4 detours are BASE-derived and wrong on TU5**
  (all sites are `7d8802a6 mflr` on TU0 but arbitrary bytes on TU5). Must be re-derived.
- **Recommended architecture: full re-base to TU5** (single canonical target), because the
  intended runtime consumers (RB3Enhanced, the patch, players) are all TU5, and the base
  binary has no independent value. Keep TU0 as a frozen git tag/branch, not a live dual-target.

---

## Phase 1 — TU5 acquisition & validation → produce `orig/45410914/default.xex` (TU5) + dtk split

### 1a. Confirm the candidate is CLEAN retail TU5 (mostly DONE in probe)
Already established: version 0.0.5.1, 12 sections identical in name/order to base, zero
`RB3DX/RB3Enhanced/rb3.ini/RB3E/Xenia/AllowSameInstrument` strings, "deluxe" hits are legit
song paths. Remaining confirmations for the executing lane:
```bash
# import table sanity (should be pure XAM/XBOXKRNL/XBDM, no injected imports)
python3 tools/va_disasm.py 0x82e60000 64        # .idata region on TU5 (repoint PE var first)
# compare against a second independent TU5 source (defence-in-depth):
#   - the clean disc zip: /srv/torrents/games/arbys/Rock Band 3 (RF) (45410914).zip
#     → unzip, the disc default.xex is the ENCRYPTED base; apply official TU5 xexp to it
#   - RB3E patch.toml module hash 464451C1022FFF32 ("EA disc default.xex + TU5 applied")
#     is the canonical TU5 fingerprint; 02B607A811A4C291 is the RB3DX-modded one to REJECT.
```
Acceptance: `/srv/.../rb3/default.xex` == the "EA disc + TU5" module (not RB3DX). It already
looks clean; this step is a belt-and-suspenders cross-check, not a blocker.

### 1b. Fallback if the candidate is modded
Produce a clean TU5 from first principles (no XDK, no redistribution):
1. `default_vanilla.xex` (/srv, enc=1 v0.0.0.1) is the encrypted retail base. Decrypt with
   the retail/devkit key that jeff/xextool already supports (base is un-DRM'd retail —
   rb3-xenon's own base was decrypted this way to `band.exe`).
2. Apply the **official Microsoft TU5 update** (`TU5.....xexp`, obtainable via the same
   channels as the disc; a diff/patch blob, no XDK) with `xexp-apply`/xextool → clean TU5.
3. Or extract from the disc zip + official TU5. Either path yields a byte-image identical to
   1a's candidate if that candidate is genuine.

### 1c. Toolchain form the decomp needs
The matching build consumes `orig/45410914/default.xex` directly (dtk splits the XEX; see
`config.yml: object:`). To migrate:
```bash
# stage TU5 as the new target (do NOT clobber base in-place; do it on a branch/worktree)
cp /srv/torrents/games/arbys/rb3/default.xex  orig/45410914/default.xex   # TU5
# jeff/dtk needs the decompressed PE too, for va_disasm & fingerprint (band.exe analogue):
tools/… (jeff xex decompress) → orig/45410914/band.exe                    # TU5 flat PE
```
dtk SPLIT will emit fresh per-unit target `.obj`/`.s` from the TU5 `.text` once splits.txt is
re-anchored (Phase 2/3). The UTF-16/PpcRel WARN noise is tolerated (jeff downgrades to warn).

---

## Phase 2 — Anchoring impact & architecture decision

Every artifact keyed to **absolute base VAs** must be re-anchored. Inventory (measured):

| Artifact | Keyed by | Count | Re-anchor cost |
|---|---|---|---|
| `config/45410914/splits.txt` | `.text/.pdata start:0xVA end:0xVA` per TU | 3870 lines | **Mechanical remap** via base→TU5 addr map (Phase 3), then dtk re-derives `.pdata`. |
| `config/45410914/symbols.txt` | `sym = .text:0xVA` (dtk symbol table) | 251k lines / 103k `.text` | **Regenerate from TU5 dtk analysis** (dtk emits it); do NOT hand-remap. Named symbols re-applied from the transferred map. |
| `decomp.db` (SQLite) | `functions.addr` (base VA) | 69,741 fns | Re-seed from TU5 `report.json` (`scripts/ingest_report.py`). Named-fn evidence re-keyed via the base→TU5 map (Phase 3). |
| `scripts/target_symbol_map.json` | `0xVA → mangled` | 13,846 | **Remap keys** base→TU5 (the map value/name is version-invariant; only the address key moves). |
| `objects.json` | file→match status (no VA) | 7 objects wired | **Version-invariant** — no change. |
| `report.json` | regenerated by build | — | Auto-regenerates on TU5 build. |
| Ghidra project `RB3Xenon` | analysis on base image | 1 program | **Re-import TU5 XEX** (`tools/ghidra/import-xex.sh`), fresh single-pass analysis. Keep base program as a 2nd program for BinDiff transfer (§3). |
| `ghidriff_identities.json` | `rb3_addr` (base VA) | 978 | Re-key via base→TU5 map; the Wii-side identity is invariant. |
| `global_fuzzy_pairs.json` | base VA | 2000 | Re-key via map (or re-run fuzzy against TU5). |
| `game_content_match.json` / `dc3_content_match.json` | `rb3_addr`→name | 394 / 5029 | Re-run content-match against TU5 (byte-hash based — cheap, authoritative) OR re-key via map. |
| `unified_id_rb3wii.json` | base VA | 9,301 | Re-key via map. |
| `fingerprints.json` / `autoid.json` | base VA | 61,618 / 511 | Regenerate from TU5 asm (cheap, `fingerprint_match.py extract`). |

### Architecture options
- **(A) Full re-base to TU5** — replace the target; re-anchor all artifacts once. *Chosen.*
- (B) Dual-target (base + TU5 side-by-side, e.g. `config/45410914_TU5/`) — doubles build/CI,
  splits agent attention, and the base half has no consumer. Reject.
- (C) base→TU5 remap layer (keep base authoritative, apply address delta at emit time) —
  brittle: TU5 changed function *bodies* and *sizes*, not just offsets, so a pure delta map
  can't exist; you still need per-function identity. Reject as the primary model (but its
  by-product — the base→TU5 identity map — IS the migration mechanism, Phase 3).

**Recommendation: (A) full re-base.** Rationale: single canonical target that matches every
downstream consumer; the identity tooling that already maps Wii↔Xenon and DC3↔Xenon makes
base→TU5 (same source, sibling rebuild) the *easiest* cross-binary problem the project has
faced. Freeze TU0 as git tag `target/tu0-frozen` for provenance + to keep the running
base-patch test undisturbed (that test uses base `default.xex`; do NOT touch it — work on a
worktree/branch, swap the XEX only there).

---

## Phase 3 — base→TU5 function map (the migration mechanism)

TU0→TU5 is **the same source recompiled** (bug-fix title update, same MSVC `/O1` flags), so
the vast majority of functions are byte-identical modulo relocation → near-total match. Method,
using EXISTING tooling, in ascending cost:

1. **Byte-hash exact match (cheapest, highest confidence).** Reuse the `*_content_match.py`
   machinery (currently DC3/game→rb3): compute relocation-normalized instruction hashes for
   every TU0 named fn and every TU5 fn; equal hash ⇒ identity. This is exactly how
   `dc3_content_match.json` (5029) and `game_content_match.json` (394) were built. Expect the
   **large majority** of the 13,846 named base fns to map 1:1 this way (TU5 only touched a
   subset of TUs).
2. **ghidriff / BSim / BinDiff structural match for the residue.** The project already runs
   ghidriff (`ghidriff_identities.json`, 978), BSim, and BinDiff cross-binary. Point them
   base-program↔TU5-program (both PPC, same compiler → BinDiff scores far higher than the
   existing Wii↔Xenon cross-arch runs). Covers functions whose bytes changed (relocation of a
   moved callee, a recompiled neighbor) but structure didn't.
3. **fingerprint_match (string/const anchors) for anything still ambiguous.** `fingerprint_match.py
   extract` on TU5 asm, then match referenced-string sets base↔TU5 (version-invariant literals).
4. **Genuinely-changed functions (TU5 code edits).** The real diff between TU0 and TU5 is a
   handful of bug-fix/anti-piracy/network TUs. Those base names are still valid *names* but the
   TU5 *body* differs → they become `NonMatching` again and re-enter the normal matching loop.
   Enumerate them precisely by set-differencing: `{base named fns} − {step-1/2/3 matched}` = the
   TU5-changed worklist (expected small, tens–low-hundreds, concentrated in a few TUs).

Deliverable: `base_to_tu5_map.json` (`base_VA → tu5_VA + confidence + method`), which drives
the mechanical re-key of every table in Phase 2. Build a `tools/remap_to_tu5.py` (fork of the
existing `relocate_*_splits.py`, which already do VA-remapping of splits + target_symbol_map
with function-boundary snapping and fail-closed overlap guards — reuse verbatim).

Expected match rate: **>95% exact-or-structural** (same source rebuild). This is a *higher*
hit rate than any cross-binary work the project has done to date.

---

## Phase 4 — verification (does the build re-point cleanly against TU5)

1. `cp` TU5 xex into a **worktree** (`scripts/setup_worktree.sh ~/tmp/wt-tu5 tu5-migrate`),
   apply remapped splits.txt/symbols.txt/target_symbol_map.json, `touch config.yml`,
   `python3 configure.py`, `./tools/ninja-locked 2>&1 | tee ~/tmp/rb3_build_tu5.log`.
2. dtk SPLIT must carve TU5 `.text` with **zero "Split ends within symbol"** errors — the
   function-boundary-snap in the remap tool guarantees this (same invariant the existing
   relocate tools enforce). Any bisect error ⇒ that fn's TU5 boundary moved; fix its map entry.
3. Run `report.json` / `tools/true_progress.py`. **Gate: whole-binary matched_functions on TU5
   ≈ the TU0 number (11,240)** minus the genuinely-changed set. A large drop ⇒ a bad remap
   (mispaired address) not a real regression — bisect via `fn_resolver.py validate`.
4. Spot-check 10 known-matched TUs (MasterAudio, RockCentral, Object) with `run_objdiff` in the
   worktree: they must read 100% against the TU5 obj, confirming objdiff re-points cleanly.
5. Only after green on the worktree: land the re-based config on `tu5-migrate` branch, retag
   TU0, and (separately, human-gated) swap the main `orig/` XEX.

---

## Phase 5 — re-target the same-instrument patch for TU5

The patch derivation method is documented (`docs/plans/same-instrument-derived-addresses.md`
§ method, `rb3enhanced-same-instrument-patch.md` §8) and is Ghidra/va_disasm/fingerprint on the
BASE image. Re-run the identical method on the **TU5 program**:

1. **Re-import TU5 into Ghidra** (Phase 4) → MCP port 8002 now serves TU5.
2. The 6 VERIFIED targets already have exact byte signatures + Wii-oracle shapes in the derived
   doc — re-find each by **structural fingerprint on TU5** (member offsets 0x50/0xb0, callee
   sets, element-size divisors 0x44, tail-branch proofs). The base→TU5 map (Phase 3) will
   *directly* give most of them: e.g. base `TrackWatcherImpl::RecalcGemList 0x8276FBB0` →
   look up its TU5 VA in `base_to_tu5_map.json`. The 4 NOT-FOUND ones re-run the same time-boxed
   structural hunt on TU5.
3. Re-derive the 4 **detour sites** + the ~11 fixed-VA call targets
   (`same-instrument-compile-recipe.md` table: GameGemDBDuplicate 0x8276E590, CopyFrom
   0x82769450, GetDiffList 0x8276E010, SetOvershellSlotState 0x8266DB58, MemFree 0x827BC430,
   …). Each must be re-confirmed on TU5 with `va_disasm` (prologue = `7d8802a6` for detours;
   correct body for call targets). **None of the BASE addresses transfer** (verified: TU5 bytes
   at those VAs are unrelated).
4. **Pick a fresh TU5 code cave.** The base cave `0x82C25000..0x82C25AF0` is in the BINK→BINKBSS
   gap; TU5 has BINK at `0x82c4d000`, BINKBSS at `0x82c60000` → the gap MOVED. Re-scan the TU5
   section map for a zero-filled committed inter-section gap; re-pack with `xex_binpatch.py` /
   the packer against the TU5 `band.exe`.
5. Re-run the packer (`same-instrument-packer-status.md` recipe) with TU5 addresses; the
   `__savegprlr/__restgprlr` helper VAs (`0x82803F00/0x82803F50` on base) also move → pull from
   TU5 `symbols.txt`. Re-verify with `xex_binpatch.py verify` against the TU5 XEX and the RB3E
   patch.toml module hash `464451C1022FFF32`.

The whole patch is ~15 addresses + one cave; with the base→TU5 map most are a table lookup and
the rest are a bounded structural re-find. **Est. ½–1 day** once the map exists.

---

## Phase 6 — effort & risk

| Phase | Nature | Size |
|---|---|---|
| 1 acquisition/validation | mostly mechanical (probe already done) | ~½ day (fallback +1 day if modded) |
| 2 anchoring re-key | mechanical (scripted table rewrites) | ~1 day tooling + runs |
| 3 base→TU5 map | mostly automated (content-hash + ghidriff/BinDiff) + a manual residue | ~2–3 days |
| 4 verification | build + gate + spot-check | ~1 day |
| 5 patch re-target | bounded manual (≈15 addrs + cave) | ~½–1 day |
| **Total** | | **~1 week** |

### Biggest risks
1. **Silent mispairing in the base→TU5 map** → fake matches / build-time bisect errors.
   Mitigation: function-boundary snap + fail-closed overlap guard (already in `relocate_*`
   tools); gate on whole-binary matched_functions vs TU0 baseline; `fn_resolver validate`.
2. **TU5 candidate is subtly RB3DX-modded** (unlikely — probe is clean). Mitigation: module-hash
   cross-check (464451C1 vs 02B607A8) before committing the XEX; fallback = build clean TU5 from
   encrypted base + official TU5.
3. **Under-estimating the genuinely-changed set** (TU5 network/anti-cheat edits). Mitigation:
   Phase-3 step-4 set-difference makes it explicit and small; those TUs just re-enter matching.
4. **Disturbing the running BASE patch test.** Hard rule: all migration work on a
   worktree/branch; TU0 `orig/default.xex` untouched until a human-gated final swap; freeze TU0
   as `target/tu0-frozen`.
5. **Ghidra re-analysis time** on a 14 MB XEX (single-pass, no map). Mitigation: run headless
   overnight; keep both programs loaded for BinDiff transfer.

## Concrete first commands for the executing lane
```bash
# 0. verify candidate cleanliness (module hash)
#    (compute XEX security-header image hash; compare to 464451C1022FFF32)
# 1. stage TU5 in a worktree (never in main)
scripts/setup_worktree.sh ~/tmp/wt-tu5 tu5-migrate
cp /srv/torrents/games/arbys/rb3/default.xex ~/tmp/wt-tu5/orig/45410914/default.xex
# 2. build the base→TU5 map (content-hash first)
python3 tools/dc3_content_match.py --self base --target ~/tmp/wt-tu5/orig/.../default.xex ...   # adapt: base-named→TU5
# 3. ghidriff/BinDiff residue: import TU5 to Ghidra, run cross-program match
tools/ghidra/import-xex.sh ~/tmp/wt-tu5/orig/45410914/default.xex   # new TU5 program
# 4. remap tables (fork of relocate_game_splits.py)
python3 tools/remap_to_tu5.py --map base_to_tu5_map.json --apply --splits ~/tmp/wt-tu5/.../splits.txt --tsm ...
# 5. build + gate
cd ~/tmp/wt-tu5 && touch config/45410914/config.yml && python3 configure.py && ./tools/ninja-locked | tee ~/tmp/rb3_build_tu5.log
```
