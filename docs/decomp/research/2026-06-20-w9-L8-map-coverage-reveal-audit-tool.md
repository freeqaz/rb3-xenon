# W9 L8 — map-coverage reveal-audit-tool: TOOL EXISTS, lever = run the existing pipeline

**Date:** 2026-06-20
**Mode:** ADVERSARIAL DISCOVER/PLANNER (Opus, layer 8), READ-ONLY in main repo.
**Baseline:** main @812e1df, 8314 / 65544 matched.
**Frontier:** `map-coverage-reveal-audit-tool` (kind=tooling, est +15).
**Verdict:** **REAL_ACTIONABLE** — but the "build a generic reveal-audit tool"
premise is **REFUTED**. The tool already exists (`tools/reveal_sweep.py`), already
implements the EXACT dossier recipe (hand-parse BE-PPC COFF, size-bucket, normalized
word-compare masking reloc slots, unique-1:1 + precision-floor gate, emit
target_symbol_map fragment), and ALREADY auto-harvests post-port objs (it reads the
*currently-built* obj via objdiff.json, which in a port-then-pin worktree IS the
post-port obj). The real, unrun lever is **running the existing two-step pipeline as a
binary-wide second-pass harvest** — proven to yield **20 gated-safe candidates on
current main RIGHT NOW** (49 raw → 20 after the ICF gate), each a likely +1 with
ZERO source/pin/port change (pure map-add).

---

## Ground truth: the tool exists and does precisely what the frontier asks

`tools/reveal_sweep.py` (12.7 KB, last touched 2026-06-06). It:
- hand-parses the retail target COFF (`build/45410914/obj/<unit>.obj`, big-endian PPC,
  `fn_`/`lbl_` unmapped) — GNU/LLVM can't, same `struct.unpack` pattern the frontier
  cites from `dump_vtable.py` (shared via `fuzzy_content_match.read_coff_functions`);
- hand-parses OUR compiled base COFF (`build/45410914/src/.../<unit>.obj`), with
  `--include-static` to also reach COFF class-3 statics (oggvorbis/zlib C helpers);
- size-buckets, then normalized word-equality compare **masking reloc slots**
  (`fuzzy_content_match.word_eq_frac`, BE instruction words) — so raw-exact ⇒ true
  normalized count ≥ raw-exact, exactly the frontier's note;
- precision floors `MIN_SIZE 0x18`, `MIN_REAL_WORDS 5` + **UNIQUE-1:1 gate** to reject
  ICF/adjustor-thunk collisions (`$4...` thunks are byte-identical once branch reloc is
  masked → correctly rejected);
- emits a `target_symbol_map.json` fragment for `tools/safe_name_merge.py --gate`
  (the ICF/collision/non-real second gate; `name_collision_tsm`, `non_real_symbol`,
  `addr_exists`);
- self-validating: a wrong addr cannot read byte-exact at 100%, so any entry the
  rebuilt report.json confirms at 100% is correct.

Both downstream tools exist and are wired: `tools/safe_name_merge.py` (`--gate`/`--out`/
`--merge`), `scripts/setup_worktree.sh`, `tools/fresh_report.sh` for A/B. **No build
needed. No tool to write.** This finding mirrors and confirms the L7 dossier
(`2026-06-20-w9-L7-reveal-audit-tool-port-then-pin-branches.md`).

## Proof the tool ALREADY handles the "second pass" / post-port case

The frontier (and L7) worry the tool must be re-pointed at post-port objs. It already
is — `load_units()` reads `objdiff.json` + the live `build/.../obj` & `build/.../src`
paths, so in a port-then-pin worktree those ARE the post-port objs. Demonstrated:

- **SongStatusMgr base-plus-reveal worktree** (`bd9705b`, report 8358): running
  `reveal_sweep --units SongStatusMgr --include-static` IN that worktree (post +34 port,
  post +10 reveal, post 15000 star-cap source fix) finds **exactly 1 residual**:
  `0x825B8670 ?GetPossibleStars@SongStatusMgr` (48 B). Verified `bd9705b` fixed the
  star-cap in the *body* (making it byte-exact) but never added its map entry → genuine
  **+1 residual reveal**. The tool found it automatically. This is the mechanism working.
- **SongSortMgr port-then-pin worktree** (`dc30ed0`/`c73cd58`, report 8392): post-port
  reveal finds **0 unique** but **35 ambiguous** — the unit's residue is same-size
  byte-exact template/ICF dup methods the 1:1 gate correctly refuses (identity
  undetermined by bytes alone). NOT a free reveal; needs disambiguation (see frontier
  below). The "27 fn_ residue" in bd9705b's commit msg was an over-count: most are
  genuine bodyports, not reveals — the tool correctly separates them.

So the per-unit second-pass yield is SMALL (SongStatusMgr +1, SongSortMgr +0). The
claimed "+10/unit" was the FIRST reveal pass, already executed by bd9705b. Frontier
est +15 is the right magnitude only when aggregated **binary-wide**, not per-unit.

## The real lever: binary-wide second-pass harvest on CURRENT main

Running the existing pipeline on main @812e1df's current built objs (no worktree, no
port — purely "which already-byte-exact own-unit methods lack a map entry"):

```
reveal_sweep.py --include-static            → 49 raw candidates  (1561 ambiguous rejected)
safe_name_merge.py --gate                   → 20 gated-safe       (29 rejected name_collision_tsm)
```

The **20 gated-safe** (each a likely +1, pure map-add, no source/pin/port):

| addr | symbol | unit |
|---|---|---|
| 0x82282488 | BandDirector::SetCharSpot | BandDirector.cpp |
| 0x82453D98 | RndConsole::List | Console.cpp |
| 0x82322DA0 | BandCharDesc::MakeInstrumentPath | BandCharDesc.cpp |
| 0x827A63B0 | HxGuid::ToString | HxGuid.cpp |
| 0x8235B350 | Character::RemovingObject | Character.cpp |
| 0x8235B4B8 | Character::UpdateSphere | Character.cpp |
| 0x8235B700 | Character::BoneServo | Character.cpp |
| 0x8235B840 | Character::ComputeScreenSize | Character.cpp |
| 0x8235BE18 | ObjectDir::New<RndGroup> | Character.cpp |
| 0x8235C5D8 | Character::Lod::operator= | Character.cpp |
| 0x8235CF48 | Character::OnPlayClip | Character.cpp |
| 0x82479EE8 | RndAmbientOcclusion::DumpObjList | AmbientOcclusion.cpp |
| 0x823C82F0 | Waypoint::OnWaypointFind | Waypoint.cpp |
| 0x823C8390 | Waypoint::OnWaypointNearest | Waypoint.cpp |
| 0x823C8790 | operator<< ObjOwnerPtr<Waypoint> vector | Waypoint.cpp |
| ... | (+5 more in /tmp/reveal_main_safe.json: Console, MeshAnim, etc.) | |

These are residual reveals left behind by prior lands (notably the WAVE-8 Character +45
and Waypoint +7/+31 relocations: the relocation made the cluster byte-exact, but only
the cheap accessors got map entries; these tail methods are still byte-exact-but-unnamed).
This IS the frontier's "auto-harvest residual reveals across ALL port-then-pin lands" —
delivered by the EXISTING tool, no new code.

⚠ The 29 rejected were `name_collision_tsm` (the symbol name already exists in the TSM /
matched elsewhere) and the raw 1561 "ambiguous" are ICF/thunk/template collisions — the
gate is doing its job. The 20 must STILL be build-validated (keep only 100%-landing).

## Where there IS a genuine (narrow) tool gap

The 35 SongSortMgr ambiguous + ~1561 binary-wide ambiguous edges are byte-exact pairs
blocked solely by the unique-1:1 gate (N same-size byte-exact methods in one unit). A
**disambiguation enhancement** — break ties by VA-vs-pin-span ownership or by relative
ordering (the i-th byte-exact target fn in pin order ↔ the i-th own method of that size)
— could recover a fraction. This is a REAL but speculative enhancement (risk: mis-pair
two genuinely-distinct same-size methods → wrong name → silent 0%, caught by build but
wastes a cycle). Emit as a discovered_frontier, NOT a primary actionable (low confidence,
the 1:1 gate exists precisely because bytes don't establish identity here).

## Self-containment

The 20-candidate binary-wide harvest is **already self-contained vs main@8314**: it
touches only `scripts/target_symbol_map.json` (map adds) — no source, no splits, no
objects.json. One worktree: merge the gated fragment, rebuild, keep only 100%-landing
entries, whole-binary A/B. attribution_risk=false (pure reveal, self-validating; a wrong
addr can't read 100%). The per-branch second-pass reveals (SongStatusMgr +1, etc.) are
NOT self-contained — they ride on their unmerged base port (L7 already planned those).

## Verdict

**REAL_ACTIONABLE.** Tool exists (build-WI REFUTED). Primary lever = run the existing
`reveal_sweep → safe_name_merge --gate` binary-wide harvest = 20 gated map-adds on main
NOW, ~+15-20 expected after build-validation, zero source risk. Secondary = the unrun
per-branch second passes (small, ride on L7's base ports). Frontier for the ambiguous-edge
disambiguation enhancement.
