# Sibling-title oracles — RB1/RB2/TBRB/GDRB/LRB/devkit/TU builds as identification sources

Status: DRAFT-RFC | Date: 2026-07-08 | Author: Claude Opus (paths-to-100 wave) | Theme: identification

## Summary

Other Harmonix X360 Milo titles (RB1/RB2/TBRB/GDRB/LRB) and alternate builds of
RB3 itself (360 devkit, PS3 SELF, retail Wii DOL) sit on disk at
`/home/free/code/milohax/milo-executable-library/`. The scout's inventory is
**broadly correct** — the RB3 "360 devkit 12 minutes build" exists. But
verification kills the headline play: the RB3 360 **vanilla** in that library is
**byte-identical to our target** (no new info), and the RB3 360 **devkit** is a
symbol-stripped, near-code-identical (`.text` differs by 368 bytes) sibling with
**no map** — so it offers almost nothing for identification. The only sibling
oracle with named symbols we don't already exploit is engine-shared and
already covered by DC3. **Verdict: mostly DEFER; one narrow PILOT (GDRB-360 as a
closer-in-time BinDiff bridge for engine funcs) worth ~a day.**

## Motivation

The two confirmed walls (per shared facts) are IDENTIFICATION (class-B
ICF-scattered methods can't be located) and BODY-DIVERGENCE (MWCC→MSVC codegen
stalls ported functions <100%). Sibling titles are attractive because a *named*
X360 MSVC Milo binary would be a Rosetta Stone for both: BinDiff RB3↔sibling
transfers names structurally, and a same-compiler sibling body doesn't have the
MWCC divergence problem. DC3 already plays this role for the engine
(`docs/decomp/` calls it the "closest twin"). The question this RFC answers:
**do the other titles add anything DC3 + rb3-Wii don't already give us?**

## Current state (verified)

Verified 2026-07-08 against the live library and `../jeff` dtk.

**Strict baseline** (`build/45410914/report.json`): 11,240 / 65,619 functions
(17.13%); 962,656 / 11,074,108 code bytes (8.69%). Matches the shared facts.

**Library inventory** — `/home/free/code/milohax/milo-executable-library/` is a
real git repo (`README.md` describes it as a bugfix-executable round-up). Titles
present: `rb1 rb2 rb3 rb4(empty) tbrb gdrb lrb dc1 dc2 dc3 gh1/2/3/80s amp2003
amp2016 freq fantasia kr1 rbacdc rbblitz rbms rbvr`. The scout's specific claim —
`rb1/rb2/gdrb/lrb/tbrb/dc3 XEXes incl. an "rb3 360 devkit 12 minutes build"` — is
**CONFIRMED** (dir `rb3/360 devkit 12 minutes build/`).

**RB3 artifacts (verified via `dtk xex info`):**

| Artifact | md5 / detail | Symbols? | Value |
|---|---|---|---|
| `rb3/360 Vanilla/default-binary_retail.xex` | `e55d9c4e…` — **byte-identical to our `orig/45410914/default.xex`** | stripped | **ZERO new** (it IS our target) |
| `rb3/360 devkit 12 minutes build/default-binary_proto.xex` | `ef77164f…`; entry `0x82819D90`; filetime **14:40:16** (12 min before vanilla's 14:52:23); retail/uncompressed/unencrypted | **stripped, no map** | near-nil (see below) |
| `rb3/360 xexp/tu1..tu5` | title-update patch deltas | stripped | patch-delta only |
| `rb3/PS3 Vanilla 1.01–1.05`, `1.05 No Checksum Fast Start` | `EBOOT.BIN` magic `SCE\0` = **encrypted SELF** | needs scetool+keys | blocked on decrypt |
| `rb3/Wii Vanilla/main.dol` (retail Wii) | 9.3 MB, mostly stripped (194 `.cpp` strings) | ~none | MWCC, low value |
| `rb3/Wii Proto (Bank 5) (Debug)/band_r_wii.{elf,map}` | 40 MB ELF + **12.8 MB named `.map`** | **FULL named map** | **already the `../rb3` oracle** |

**Critical devkit finding.** The devkit `.text` VirtualSize is `0x009B4764` vs
vanilla `0x009B48D4` — a **368-byte (0.004%) difference**. A `comm` of unique
`*.cpp` strings shows **0 files present in devkit but not vanilla**. So the
devkit is *not* a debug build with extra asserts/functions — it is the **same
retail source, same `/O1` flags, built ~12 min earlier**, relinked (hence the
different entry point and the 11 MB raw byte-diff, which is pure address/layout
shift, not new code). It has no `.pdb`, no `.map`, and anonymous functions —
exactly the identification poverty of our own target.

**The Wii Proto Bank 5 map is already exploited.** `../rb3` (the rb3-Wii DEV
decomp, our game-code oracle) is built from `orig/SZBE69_B8/files/band_r_wii.map`
(verified in `../rb3/CLAUDE.md`), which is *the same file* as
`rb3/Wii Proto (Bank 5) (Debug)/band_r_wii.map` (`SZBE69_B8` = Bank 5). Its
symbols (`GetGroupName__13BandCharacterCFv`, `BandOffline.o`,
`NetSession_RV.o`, …) are the named source we already port from. This oracle is
**not new**.

**Non-RB3 X360 siblings (verified via `dtk xex info`):**

| Title | 360 XEX | PE / filetime | Engine era |
|---|---|---|---|
| GDRB | `gdrb/Xbox 1.0 No Checksum Strum Limit Fix/default.xex` | `proj9.exe`, **2010-03-31** | ~5 mo before RB3 |
| TBRB | `tbrb/Xbox 1.0 No Checksum No Strum Limit/default.xex` | `proj9.exe`, **2009-06-15** | ~14 mo before RB3 |
| LRB | `lrb/1.0 360 Strum Limit Fix no checksum/` | (not opened) | Lego RB, RB3-era |
| RB1/RB2 | 360 TU builds under `rb1/`, `rb2/` | stripped retail | pre-Milo-360-maturity |

All are **retail, stripped, no `.map`**. GDRB is the closest-in-time X360 MSVC
Milo binary to RB3 that isn't RB3 (2010-03 vs 2010-08). GDRB/TBRB also ship **Wii
Debug ELFs** (`gdrb/Wii Debug/green_s_wii.elf` 11.7 MB with DWARF, but **no
`.map`**).

**Infra exists:** BinDiff at `/usr/bin/bindiff` (→ `/opt/bindiff/`), BinExport
Ghidra plugin at `/opt/bindiff/extra/ghidra/`, XEXLoaderWV cloned at
`/home/free/code/milohax/XEXLoaderWV/`, Ghidra project at
`ghidra_projects/RB3Xenon`, import driver `tools/ghidra/import-xex.sh` (takes the
XEX path — trivially retargetable), symbol-apply path `tools/ghidra/apply_symbols.py`.
Ports: DC3=8000, rb3-Wii=8001, RB3=8002. dtk (`../jeff`) decodes all these XEXes
(uncompressed/unencrypted) directly.

## Proposal

Because verification demoted most of this vein, the proposal is a **ranked,
mostly-negative triage** with exactly one low-cost pilot.

### Rank 1 (PILOT, ~1 day): GDRB-360 as an engine-func BinDiff bridge

GDRB-360 is the only artifact that is (a) same compiler+platform (MSVC X360),
(b) *different* enough from RB3 to add signal, (c) closer in time than DC3
(2010-03 vs DC3's 2012-09), and (d) already on disk, decodable, decryptable-free.
It has **no symbols of its own**, so it is useless as a *name* source directly.
Its only possible value: a **three-way structural anchor**. If a Milo engine
function is named in DC3 and structurally survives DC3→GDRB→RB3, the GDRB
intermediate can raise BinDiff confidence for RB3 functions where the direct
DC3→RB3 match is ambiguous (ICF-scattered / low-confidence). Pipeline:

1. Import GDRB-360 into Ghidra via XEXLoaderWV (reuse `import-xex.sh` with the
   GDRB path; new project `GDRB360`). Full auto-analysis, no map.
2. BinExport all three (DC3 already imported at port 8000; RB3 at 8002; GDRB new).
3. BinDiff DC3↔GDRB (DC3 is named) → transfer DC3 names onto GDRB by structure.
4. BinDiff GDRB↔RB3 → propagate the now-named GDRB functions onto RB3.
5. Compare the RB3 name set from this chain against the **direct DC3↔RB3** BinDiff
   (which sibling `05-data-xref-anchoring.md` and `07-icf-constraint-solver.md`
   also feed). Only *new* RB3 identifications (not already found by DC3↔RB3
   direct) count as yield. Feed those as candidate `.text` spans to
   `04-pinning-at-scale.md` / `splits.txt`.

Kill fast: if step 5 yields **< ~30 net-new high-confidence engine idents** over
DC3-direct, stop — the transitive hop through an unnamed intermediate loses more
confidence than it gains, and the ICF wall (`topo_locate` precision 0.13, BSim
0.24 — see `docs/decomp/research/2026-06-30-topo-locator-design.md`) is the same
wall in GDRB.

### Rank 2 (DEFER): RB3 PS3 SELF decrypt for a symbol/RTTI cross-check

PS3 RB3 uses SNC/GCC PPU (different compiler → bodies won't match MSVC), so it is
**not a codegen oracle**. Its *only* possible value is (a) RTTI/vtable **class
layout** (endianness-flipped but structurally identical — feeds
`05-data-xref-anchoring.md`) and (b) whether a PS3 debug/`.self` retained
`.cpp`/symbol strings RB3-360 stripped. Blocked on: `EBOOT.BIN` is an encrypted
SCE SELF (`SCE\0` magic verified); needs `scetool` + PS3 keys we do not have on
disk (`ls /home/free/code/milohax` shows no scetool/keys). **Do not pursue until
someone supplies a decrypted PS3 ELF** — acquisition is the blocker, not analysis.

### Rank 3 (DO-NOT, documented dead): RB3 360 devkit differential

Tempting ("a second build of our exact binary!") but verification kills it: the
devkit is the same source, same flags, `.text` differs by 368 bytes, **stripped,
no map**. It gives us a *second anonymous copy* of the problem, not a symbol
source. The only conceivable use — diffing devkit↔vanilla to find the ~368 bytes
that changed in 12 minutes — is a curiosity, not an identification lever (it
tells us which 1-2 functions a late fix touched, in a binary we already match by
other means). **Record as dead so no future agent re-litigates it.**

### Rank 4 (DO-NOT): RB1/RB2/TBRB/LRB and the retail-Wii DOL

- RB1/RB2-360: pre-RB3 Milo, large engine drift, stripped, no map. DC3 is a
  *better* engine twin (same flags, *named*). Adds noise, not signal.
- TBRB-360 (2009-06): older `proj9.exe` engine, stripped. Same problem as GDRB
  but further from RB3 in time — strictly dominated by the GDRB pilot.
- LRB-360: RB3-era Lego RB; stripped, no map; game code is Lego-specific, engine
  is DC3-covered. Skip.
- **RB3 retail Wii DOL** (`rb3/Wii Vanilla/main.dol`): the brief hypothesized this
  "bridges dev-Wii to retail codegen." Verified: it is **MWCC PowerPC** (Wii),
  not MSVC X360 — so it does *not* bridge to *our* X360 codegen at all. A
  dev-Wii→retail-Wii diff would only help the *rb3-Wii* decomp, a different repo.
  And it is largely stripped (194 `.cpp` strings). No value here.
- GDRB/TBRB **Wii Debug ELFs** have DWARF types but no `.map`; they could enrich
  *engine struct layouts* for `05-data-xref-anchoring.md`, but DC3's leaked PDB
  already provides richer, same-platform layouts. Marginal; not worth a lane.

## Alternatives considered

- **Full N-way BinDiff constellation** (all Milo titles → RB3): rejected. Cost
  scales with titles, signal doesn't — every added binary is stripped, and the
  transfer confidence is gated by the *named* source (only DC3), which is already
  used directly. This is the ICF constraint-solver's job with better math
  (`07-icf-constraint-solver.md`), not a "throw more binaries at it" job.
- **Devkit as a "ground-truth pin validator"**: since devkit ≈ vanilla, one could
  in principle use devkit to sanity-check pins. But vanilla *is* the target, so
  there's nothing devkit validates that the target doesn't validate directly.
- **Acquire a leaked RB3 PDB/map** (the DC3-style Rosetta Stone): this would
  *solve* identification outright, but it is an **acquisition play, not an
  engineering one** — nothing on disk suggests one exists (`docs/INDEX.md`
  "Known traps" explicitly states there is NO RB3 map). Out of scope; flagged in
  Open questions.

## Effort & expected value

Anchored to comparable past results in this repo:

- **GDRB pilot (Rank 1):** ~1 engineer-day (mostly Ghidra import + 2 BinDiff
  runs; infra all exists). Honest EV: **0–40 net-new engine idents**, most likely
  **< 20**. Rationale: DC3↔RB3 direct BinDiff is the strong channel; a transitive
  hop through an *unnamed* GDRB can only recover cases where DC3↔RB3 was
  *ambiguous but GDRB↔RB3 is sharp* — a thin slice. Compare: the class-A TU-pure
  span harvest yielded +403 in one session and is now EXHAUSTED (wave-8 +0); the
  LLM grind loop lands +22 (3342b30). A sibling-BinDiff lane landing even +20
  strict would be a *middling* wave, and that's the optimistic tail.
- **PS3 layout cross-check (Rank 2):** blocked on acquisition; if a decrypted ELF
  appears, ~0.5 day to diff RTTI/vtables; EV feeds `05` (a few struct-layout
  fixes → force-multiplier improvements, cf. Waypoint `d3c6e4f` +7).
- **Everything else:** EV ≈ 0. Do not staff.

Net honest read: **this whole vein is worth at most one 1-day GDRB pilot**, and
the base case is that it, too, joins the killed-levers list.

## Risks & failure modes

- **Transitive-confidence collapse.** Names propagated DC3→GDRB→RB3 through two
  fuzzy BinDiff hops can be *wrong*; a mis-transferred name that gets pinned reads
  a false 0% (or worse, a false match if the obj symbol renamer pairs it). Gate:
  only accept idents corroborated by an *independent* signal (data-xref anchor
  from `05`, or a direct DC3↔RB3 near-hit), and always confirm via a real objdiff
  pin before crediting. Never auto-land BinDiff-only idents.
- **Wasted Ghidra time.** GDRB import + analysis is ~hours of single-process
  Ghidra (projects are single-process — the `gameid-crossval` skill already warns
  RB3Xenon may be locked mid-import). Serialize; don't block other Ghidra work.
- **Scope creep into acquisition.** The seductive framing ("get an RB3 PDB / PS3
  keys") pulls effort into things we can't control. Keep this an engineering RFC:
  only analyze bytes already on disk.

## Kill criteria

- **GDRB pilot dies** if the DC3→GDRB→RB3 chain yields **< 30 net-new
  high-confidence engine idents** over DC3↔RB3-direct (measured as new candidate
  spans that a real objdiff pin then confirms ≥ 1 additional 100% function).
- **The entire vein is confirmed dead** if, after the GDRB pilot, no sibling-360
  binary has produced a *named-symbol source* better than DC3 (already true on
  paper) AND no PS3 ELF has been acquired. At that point, record in MEMORY.md
  alongside the topo-locate / BSim kills and stop.
- **Devkit / RB1 / RB2 / TBRB / LRB / retail-Wii-DOL are killed NOW** by this
  RFC's verification (stripped, no map, dominated by DC3/rb3-Wii). Do not reopen
  without a *new* artifact (a symbol table, map, or PDB) appearing on disk.

## Open questions

1. Does anyone in the MiloHax community hold an **RB3 X360 PDB or linker map**?
   That single artifact would obsolete this entire RFC and most of the
   identification-wall work. (Acquisition, not engineering — flag to the owner.)
2. Are the **RB3 PS3 debug builds** (the library has PS3 vanilla 1.01–1.05 but no
   PS3 *debug*) obtainable? A PS3 debug ELF with `.cpp`/symbol strings, even
   MWCC/SNC, could enrich the rb3-Wii-style path oracle for *network* code
   (Quazal is cross-platform C++).
3. Is the GDRB↔RB3 engine-version delta small enough that BinDiff even *aligns*
   the two? If GDRB's Milo predates key RB3 engine refactors, the structural
   match rate may be too low for the transitive hop to work at all — measurable
   in step 3 of the pilot before investing in step 4.

## References

- **Library (verified on disk):**
  `/home/free/code/milohax/milo-executable-library/` — `README.md`;
  `rb3/360 Vanilla/default-binary_retail.xex` (md5 `e55d9c4e…` = our target);
  `rb3/360 devkit 12 minutes build/default-binary_proto.xex` (md5 `ef77164f…`);
  `rb3/PS3 Vanilla 1.05/EBOOT.BIN` (encrypted SELF);
  `rb3/Wii Proto (Bank 5) (Debug)/band_r_wii.map` (= the `../rb3` oracle);
  `gdrb/Xbox 1.0 No Checksum Strum Limit Fix/default.xex`;
  `tbrb/Xbox 1.0 No Checksum No Strum Limit/default.xex`.
- **Our target:** `/home/free/code/milohax/rb3-xenon/orig/45410914/default.xex`.
- **rb3-Wii oracle provenance:** `../rb3/CLAUDE.md`,
  `../rb3/orig/SZBE69_B8/files/band_r_wii.map`.
- **Tooling:** `../jeff/target/release/dtk` (`xex info`);
  `tools/ghidra/import-xex.sh`, `tools/ghidra/apply_symbols.py`;
  `/usr/bin/bindiff`, `/opt/bindiff/extra/ghidra/BinExport`;
  `/home/free/code/milohax/XEXLoaderWV/`; Ghidra project
  `ghidra_projects/RB3Xenon`.
- **Walls / prior kills:**
  `docs/decomp/research/2026-06-30-topo-locator-design.md` (topo_locate 0.13),
  `docs/decomp/research/2026-06-24-pivot-bodyport-classb-results.md` (MWCC→MSVC
  body divergence), `docs/INDEX.md` "Known traps" (no RB3 map).
- **Sibling RFCs:** `05-data-xref-anchoring.md` (PS3 RTTI / Wii-debug DWARF layout
  feed), `07-icf-constraint-solver.md` (the principled multi-binary assignment
  this RFC's ad-hoc BinDiff defers to), `04-pinning-at-scale.md` (consumes any
  confirmed spans), `06-oracle-refresh-loops.md` (re-diff cadence),
  `08-ml-embedding-triage.md` (alternative triage amplifier),
  `10-middleware-and-denominator.md` (Quazal/Bink cross-title middleware overlap).
- **Verification method:** strict stats from
  `build/45410914/report.json` (`matched_functions` 11240/65619); XEX metadata
  via `dtk xex info`; byte-identity via `md5sum` and `cmp`; `.text` size delta
  and unique-`.cpp` `comm` diff run 2026-07-08 (all above).
