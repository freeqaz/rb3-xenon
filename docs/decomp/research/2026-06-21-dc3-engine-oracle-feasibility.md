# DC3 Engine Body-Oracle Feasibility — VERDICT (rb3-xenon)

Date: 2026-06-21
Scope: sub-problem (A) from `feedback_build_tooling_for_hard_frontier.md` — a
**DC3-vs-RB3-360 BinDiff body-oracle** for the SHARED Milo engine (`src/system/*`).
Context: cheap-matching waves are practically exhausted; the next lever is a
high-precision oracle that beats the cross-arch rb3-Wii oracle (median sim 0.16).

## VERDICT: GO

There is a large, named, same-platform, mostly-unmatched shared-engine vein. The
bet is sound: DC3 is the **same Xbox-360 / MSVC-X360 toolchain, same `/O1 /Oi /GR
/EHsc` flags, same Milo engine family** as RB3-360, so its bodies should byte-match
RB3 far more reliably than the cross-arch Wii game layer. DC3's `ham_xbox_r.map`
gives named functions + VAs at the **same preferred load address `0x82000000`** as
RB3 — so a BinDiff DC3↔RB3-360 oracle maps name+VA directly with no rebasing.

## 1. The vein (intersection: DC3-named ∩ in-RB3-binary ∩ not-yet-matched)

`vein_size_fns = 8599` — the **honest, directly-actionable** count: unmatched
functions sitting in the **490 already-PINNED shared-engine TUs** in rb3-xenon
(from `build/45410914/report.json`, mtime 2026-06-21 16:47). These already have a
dtk target `.obj`+`.s` emitted, so a DC3-sourced match **registers in objdiff
immediately** with no pin/wire step. This is the GO number.

Larger but lower-readiness tiers beyond the 8599 (need a pin/wire step first, so
NOT counted in `vein_size_fns`):
- **34 wired-but-unpinned engine TUs** (~500 fns): in `objects.json` but no `.text`
  range in `splits.txt`. One pin line each → instantly matchable. Topped by
  `CharClipGroup`(101), `ClipDistMap`(78), `Cam`(57), `MessageTimer`(56),
  `Lit_NG`(49), `CamAnim`(44). Cheapest expansion of the vein.
- **~430 fully-unwired engine TUs**: anonymized inside the 1063 `auto_generated`
  blobs (subset of 44,509 untagged unmatched fns). Need wire+pin; biggest reserve.

DC3-side supply (oracle coverage): DC3 has **665 named engine TUs / 30,499 named
`.text` fns** (96.5% map coverage). Filtering DC3-game-specific subdirs (`hamobj`
= Dance Central, `gesture` = Kinect) leaves **550 shared-engine TUs / 24,282 named
fns** — of which **465 are already pinned in rb3-xenon** (22,542 fns) with target
objs ready. So the oracle's named-fn supply (24k) comfortably covers the 8599
actionable unmatched fns.

### Spot-verification — top candidate TUs ARE in the RB3 retail binary

Method: `strings -a orig/45410914/default.xex` (XEX data section is uncompressed)
grepping RTTI type-descriptors `.?AV<Class>@@` and distinctive literals. RTTI
presence proves the class is instantiated in retail.

| DC3 engine TU | marker in RB3 XEX | hits | in binary? |
|---|---|---|---|
| `char/Character.cpp` | `bone_pelvis.mesh` | 1 | YES |
| `char/CharClipGroup.cpp` (top unpinned) | `.?AVCharClipGroup@@` | 1 | YES |
| `world/LightPreset.cpp` | `.?AVLightPreset@@` | 1 | YES (also 224/356 already matched) |
| `synth/Synth.cpp` | `.?AVSynth@@` | 1 | YES |
| `rndobj/Mesh.cpp` | `.?AVRndMesh@@` | 1 | YES |
| `rndobj/EventTrigger.cpp` | `.?AVEventTrigger@@` | 1 | YES |
| `char/CharClip.cpp` | `.?AVCharClip@@` | 1 | YES |
| `synth/Sequence.cpp` | `.?AVSequence@@` | 1 | YES |
| `rndobj/Text.cpp` | `.?AVRndText@@` | 1 | YES |
| `rndobj/PropAnim.cpp` | `.?AVRndPropAnim@@` | 1 | YES (RB3 name = `Rnd`-prefixed) |
| `rndobj/Cam.cpp` | `.?AVRndCam@@` | 1 | YES (RB3 name = `Rnd`-prefixed) |
| `obj/Dir.cpp` | `.?AVObjectDir@@` | 1 | YES (RB3 name `ObjectDir`) |

**DC3-newer caveat confirmed (1 of 12 spot-checks):** `char/ClipDistMap.cpp` —
NO hit for `.?AVClipDistMap@@` and no `clipdist` literal in the RB3 XEX. This is
exactly the "DC3-decomp is NEWER than RB3" risk from CLAUDE.md: a DC3 engine TU
that may not exist (or is differently named/factored) in RB3 retail. Such TUs must
be **dropped, not counted** — do NOT blind-pin `ClipDistMap`. Note also DC3 alias
naming (`PropAnim`→`RndPropAnim`, `Cam`→`RndCam`, `Dir`→`ObjectDir`); the BinDiff
oracle handles this automatically (it pairs by structure, not name).

## 2. Why GO (vs the Wii game-layer wall)

1. **Same compiler/platform/flags ⇒ byte-match.** DC3 and RB3-360 share MSVC-X360,
   `/O1 /Oi /GR /EHsc`, no LTCG. Unlike rb3-Wii (MWCC PowerPC, different ABI/codegen),
   DC3 bodies should compile to the same bytes — so port-then-match is reliable,
   not a reconstruction grind.
2. **Named oracle straight from `ham_xbox_r.map`** (96.5% coverage), same load
   address `0x82000000` ⇒ VA↔VA with no rebase.
3. **Spatial TU grouping preserved** (no LTCG) ⇒ BinDiff finds contiguous engine
   spans; oracle locates them in RB3 for clean pinning.
4. **8599 already-pinned, objdiff-registering** unmatched fns + ~500 one-pin-away
   + ~430 wire-away ⇒ substantial, multi-wave inventory.
5. DC3 source is **already 360-ported** (`/dc3-pair`, `src/system/` ⟵ dc3-decomp
   per CLAUDE.md) ⇒ porting cost is low (often just compile + member/inline drift).

Not a stronger verdict only because of (a) the DC3-newer drift tax — a minority of
engine fns need an rb3-Wii cross-check (added/dropped members, inline-policy: cf.
CharHair rev-13, RndAnimatable inline cascade), and (b) the actionable 8599 is the
*pinned* remainder; the full ~430-TU reserve needs wiring first.

## 3. ORACLE-BUILD RECIPE — `dc3_oracle.json`

Builds a DC3-VA ↔ RB3Xenon-VA map with BinDiff similarity + DC3 method name.
READ-ONLY w.r.t. both Ghidra projects. Same load address (`0x82000000`) ⇒ a high
BinDiff sim implies same VA. Tooling all present:
`../bindiff/build/bindiff`, BinExport ext `/opt/bindiff/extra/ghidra/BinExport/lib/BinExport.jar`
(installed in `~/.config/ghidra/ghidra_12.1_DEV/Extensions/BinExport`), VMX128
Ghidra at `/home/free/code/milohax/ghidra/build/ghidra`, projects
`dc3-decomp/ghidra_projects/DC3` and `rb3-xenon/ghidra_projects/RB3Xenon`.

```bash
set -euo pipefail
GHIDRA=/home/free/code/milohax/ghidra/build/ghidra
HEADLESS=$GHIDRA/support/analyzeHeadless     # VMX128 build w/ BinExport ext installed
BINDIFF=/home/free/code/milohax/bindiff/build/bindiff
OUT=/tmp/dc3_oracle && mkdir -p "$OUT"

# --- 1. Export DC3 BinExport (read-only: -import existing program, no re-analysis) ---
#     DC3 program is already analyzed in its project; -process by program name.
"$HEADLESS" /home/free/code/milohax/dc3-decomp/ghidra_projects DC3 \
  -process 'default.xex' -noanalysis -readOnly \
  -postScript ExportBinExport.java "$OUT/dc3.BinExport"
# If ExportBinExport.java isn't on the script path, use the BinExport "Exporter"
# headless form (Ghidra Exporter API) or copy the .java from BinExport-src.zip
# (lib/BinExport-src.zip) into a -scriptPath dir. Either yields dc3.BinExport.

# --- 2. Export RB3Xenon BinExport (READ-ONLY — never re-analyze the shared project) ---
"$HEADLESS" /home/free/code/milohax/rb3-xenon/ghidra_projects RB3Xenon \
  -process 'default.xex' -noanalysis -readOnly \
  -postScript ExportBinExport.java "$OUT/rb3xenon.BinExport"

# --- 3. BinDiff DC3 (primary) vs RB3Xenon (secondary) ---
cd "$OUT"
"$BINDIFF" --primary="$OUT/dc3.BinExport" --secondary="$OUT/rb3xenon.BinExport"
#   -> emits dc3_vs_rb3xenon.BinDiff (SQLite). Same compiler/platform => expect
#   HIGH sim (>>0.5) on shared engine fns, vs ~0.16 for the cross-arch Wii oracle.

# --- 4. Render to dc3_oracle.json (DC3-name + DC3-VA + RB3-VA + sim + confidence) ---
#     Read the BinDiff SQLite 'function' table (address1=DC3 VA, address2=RB3 VA,
#     similarity, confidence) and join DC3 names from ham_xbox_r.map by DC3 VA.
python3 /home/free/code/milohax/rb3-xenon/tools/build_dc3_oracle.py \
  --bindiff "$OUT/dc3_vs_rb3xenon.BinDiff" \
  --dc3-map /home/free/code/milohax/dc3-decomp/orig/373307D9/ham_xbox_r.map \
  --min-sim 0.50 \
  --out /home/free/code/milohax/rb3-xenon/dc3_oracle.json
#   build_dc3_oracle.py is a NEW small script (~80 LOC) to author:
#     - sqlite3 SELECT address1,address2,similarity,confidence FROM function
#     - parse ham_xbox_r.map section 0005: lines -> {dc3_va: (name, Lib:Object TU)}
#     - emit [{dc3_va, rb3_va, dc3_name, dc3_tu, similarity, confidence}], sim-desc
#   Model it on unified_id_rb3wii.json shape; reuse the map-parser from the
#   dc3-coverage lane (counts .text named syms by Lib:Object subdir:File.obj).
```

**VALIDATION GATE (do this before trusting the oracle):** pick a known-matched
engine method (e.g. a `Vec`/`Mtx`/`Hmx::Object` or a `LightPreset` method already
at 100% in report.json) and confirm its row has `similarity >> 0.5` AND
`rb3_va == dc3_va` (same load addr). If a broad sample of engine fns scores low,
the BinExport analysis or VMX128 SLEIGH disagrees — fix before scaling. Contrast:
the rb3-Wii oracle's 0.16 median is the cross-arch floor we must beat.

**Workflow once oracle exists:** oracle locates the contiguous DC3-shared engine
span in RB3 → add `.text` pin to `splits.txt` (for the 34 wired-unpinned and ~430
unwired TUs) → port DC3 source (already 360-ported; `/dc3-pair`) → objdiff
byte-match. For the 8599 already-pinned fns, the oracle just supplies the
name+body so the port targets the right method directly.

## 4. Ranked first targets

Ordering = (highest readiness: already pinned + partially matched ⇒ oracle proven
on that TU) then (cheapest expansion: one-pin-away). All RTTI-verified present in
RB3 XEX. ClipDistMap deliberately EXCLUDED (DC3-newer, absent from RB3).

1. **`world/LightPreset.cpp`** — pinned, 224/356, **132 unmatched**; densest
   pinned engine partial that's RTTI-confirmed and oracle-ready.
2. **`rndobj/MeshAnim.cpp`** — pinned-partial 82/271, **189 unmatched**; rndobj =
   pre-solved renderer, DC3 bodies expected to byte-match.
3. **`os/AsyncFileHolmes.cpp`** — pinned-partial 7/212, **205 unmatched**; largest
   single pinned-partial engine remainder.
4. **`synth/StreamNull.cpp`** — pinned-partial 18/193, **175 unmatched**.
5. **`char/CharClipGroup.cpp`** — wired-but-UNPINNED, ~101 DC3 fns; one `splits.txt`
   `.text` pin then DC3 oracle — cheapest *new* inventory; RTTI-confirmed.

(Behind these: `bandobj/BandWardrobe` 160, `bandobj/BandCamShot` 159,
`world/PhysicsVolume` 158, `rndobj/Rnd` 153 unmatched — all pinned-partial. And the
remaining 33 wired-unpinned TUs `Cam`/`MessageTimer`/`CamAnim`/`Lit_NG`/... for
cheap one-pin expansion. `MessageTimer` ties to the proven MILO_MESSAGE_TIMERS
keystone area.)

## Risks

- **DC3-newer drift tax** (proven, bounded): a minority of engine fns differ by
  added/dropped members or inline-policy (CharHair rev-13, RndAnimatable inline,
  Save/Load REVISION constants). Mitigate with an rb3-Wii cross-check on
  near-misses; gate any shared-header change on a whole-binary composed A/B.
- **DC3-only TUs**: ClipDistMap-class TUs that don't exist in RB3. Mitigate: the
  oracle's own low-sim / no-pair rows auto-flag these — never blind-pin a TU on
  source-presence alone; require an RB3-XEX RTTI/string hit + a BinDiff pair.
- **ICF-alias inflation**: ≤44B engine stubs (`??_E` dtor thunks, getters) fold
  identically across TUs and can fake matches. Mitigate: existing
  `tools/icf_alias_check.py` SOP gate; require BinDiff named-hit attribution + fn
  SIZE, not just byte-equality.
- **BinExport/SLEIGH version skew**: BinExport ext is in Ghidra 12.1_DEV but the
  dist build is 12.2_DEV; both projects must export from the SAME VMX128 Ghidra so
  basic-block boundaries align. Validate sim on a known-matched fn before scaling.
- **8599 is the pinned remainder, not the ceiling**: the ~430 fully-unwired engine
  TUs (in `auto_generated` blobs) are a bigger reserve but need wire+pin first; the
  oracle's value there is *locating* their spans.

## Key files
- DC3 oracle map: `/home/free/code/milohax/dc3-decomp/orig/373307D9/ham_xbox_r.map` (load 0x82000000)
- DC3 objects: `/home/free/code/milohax/dc3-decomp/config/373307D9/objects.json`
- DC3 engine src: `/home/free/code/milohax/dc3-decomp/src/system/`
- RB3 report: `/home/free/code/milohax/rb3-xenon/build/45410914/report.json` (9834/65544)
- RB3 wiring: `/home/free/code/milohax/rb3-xenon/config/45410914/{objects.json,splits.txt}`
- RB3 XEX (verify presence): `/home/free/code/milohax/rb3-xenon/orig/45410914/default.xex`
- bindiff: `/home/free/code/milohax/bindiff/build/bindiff`; BinExport ext:
  `/opt/bindiff/extra/ghidra/BinExport/lib/BinExport.jar`
- Ghidra (VMX128): `/home/free/code/milohax/ghidra/build/ghidra`; projects
  `dc3-decomp/ghidra_projects/DC3`, `rb3-xenon/ghidra_projects/RB3Xenon`
