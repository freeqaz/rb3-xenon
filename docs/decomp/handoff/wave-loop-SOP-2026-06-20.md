# Wave-loop SOP + live state (2026-06-20, pre-compaction handoff)

## EXACT STATE
- main @ `b43162c` (+ a roadmap/SOP docs commit on top), **9454 / 65552 matched**,
  build green, tree clean (only untracked auto_*.obj/global_fuzzy_pairs.json/
  function_analysis/dc3_content_match.json).
- Worktrees: main + pre-existing `rb3-sizedvec` + `wt-w13-refill` (refill sweep in
  flight as of wave-13 close — harvest/remove it when it returns). Wave 3–13 lane
  branches pruned.
- **Session 6932 → 9454 (+2522)** across waves 3–13, every landed lever composed-verified EXACT.

## WAVE 13 — LANDED (2026-06-20): 9404 → 9454 (+50)
- gapA/CharData.cpp +14, SavedSetlist-retry +33, gapB/AccomplishmentSetlist relocate +3;
  SongSortNode honest-negative (ICF-scattered); SaveLoadManager foundational-flagged (deferred).
- Harvest tooling now lives in `scripts/harvest/` (was /tmp). See WAVE-13 CLOSE in the roadmap doc.
- **WAVE-14 FRONTIER = the gapB conditional RELOCATE belt** (~+60 mechanical relocates:
  AccomplishmentPlayerConditional +20, fresh-pin batch +25, smaller Acc* relocates) +
  OvershellSlot-head extension +10 + LockStepMgr +20. Keystone chain (own short wave,
  sequence): ProfileMgr → BandProfile → SaveLoadManager. Full list = roadmap WAVE-13 CLOSE.

## THE WAVE LOOP (proven, repeatable — this is the engine)
Single-pass INDEPENDENT-fanout Opus wave (NOT a deep loop). Per wave:
1. **Discover** (Opus, read-only, ONE distinct TU per lane = structural dedup):
   each lane verifies its TU is real (Waypoint-skeptic), produces a SELF-CONTAINED
   port-then-pin plan (port+wire+pin+map+reveal in one worktree), bounds the pin
   vs BOTH splits neighbours, emits discovered_frontier leads. flag_foundational
   for any shared-header/binary-wide lever (don't bundle — schedule keystone-first).
2. **Execute** (Opus worktrees, chunk 3): each agent does its whole self-contained
   item vs main@BASELINE, runs a splits overlap self-check before declaring landable.
3. **Audit** (Opus): own-vs-foreign honesty audit of every pin (Waypoint method:
   COFF auto_03 by VA + DC3 ham_xbox_r.map contiguous owner; longest foreign run)
   + splits-clean. Key audits by **TU name** (not branch — the wave-11 audit-key bug).
   - **ICF-alias gate (automated): `tools/icf_alias_check.py --worktree <wt> --baseline-report <pre-build report.json>`** — exit 1 = ICF-ALIAS INFLATION (the newly-100% set is ≤44B stub-folds with no real-bodied anchors, the wave-14/15 +57 fake-match shape). Use the **`--worktree` NEWLY-matched diff** mode (the strongest gate — catches a pin that ADDS only stub-folds even if the TU has anchors elsewhere); `--tu`/`--range` are lenient, for quick checks. byte-match ≠ ownership under ICF folding. Body-port lanes (splits unchanged) are exempt — stub-dominated is a span-pin concept only (the wave-15 false-drop).
4. **Reduce** (Opus fan-in): landing guide (EV order + adjacent-pin overlap risk),
   same-file unions, regenerated next_frontier, vein_status.

## COORDINATOR HARVEST/LAND SOP (do this on every wave result)
1. Parse result compactly (don't dump big JSON into context): logs + `cleared[]`
   (tu, net_delta, branch, files_changed, _audit.honest) + reduce.next_frontier.
2. **De-dup**: trust the reducer's "winning variant per TU"; NEVER land a sibling
   that regenerated target_symbol_map.json wholesale (poison — re-pairs whole binary).
3. Land each cleared winner sequentially (rebase onto growing main → ff-merge):
   - Helper: `scripts/harvest/land.sh <worktree|branch>` rebases + auto-resolves
     JSON unions + splits union; defers on cascade. Uses
     `scripts/harvest/resolve_json_union.py <wt> <relpath>` (stage-2/3 dict merge
     for target_symbol_map.json / objects.json) and
     `scripts/harvest/resolve_splits_union.py <wt>` (stage-2 + theirs-added splits
     block). (These were `/tmp/*` through wave 12; now committed in-repo.)
   - If a branch is "up to date" (based on current main), just `git merge --ff-only`.
   - Branch names: `w<N>-<tu>` (note: some keys already carry a `w<N>-` prefix →
     double prefix like `w10-w10-…`; check `git branch | grep wN-`).
4. **ALWAYS run the splits overlap self-check BEFORE building** (wave-9 build-break
   was two independent adjacent pins colliding):
   ```python
   import re; t=open('config/45410914/splits.txt').read()
   for k in ['pdata','text']:
     rs=sorted([(int(a,16),int(b,16)) for a,b in re.findall(rf'\.{k}\s+start:(0x[0-9A-Fa-f]+)\s+end:(0x[0-9A-Fa-f]+)',t)])
     print(k, sum(1 for i in range(1,len(rs)) if rs[i][0]<rs[i-1][1]),'overlaps')
   ```
   If overlap: two ports disagree on a shared boundary — fix the over-claiming one
   (shrink to the real neighbour boundary), drop its .pdata for dtk re-derive.
5. **Composed verify** (the only truth): `rm -f build/45410914/target_symbol_renames.stamp
   && touch config/45410914/config.yml && NINJA_JOBS=12 tools/fresh_report.sh`;
   re-run once (splits-only divergence WARN is a known FP). Read measures.matched_functions.
6. Refill (standing compounding step): launch a Sonnet agent running
   `tools/refill_loop.sh --map global_fuzzy_pairs.json` in its own worktree; land it.
7. Clean up: remove wt-w<N>-* worktrees; prune merged w<N>- branches (keep any deferred).
8. Docs-as-you-go: commit dossiers (docs/decomp/research/2026-06-20-w<N>-*.md),
   append a WAVE-<N> CLOSE to docs/plans/decomp-state-and-roadmap-2026-06-09.md,
   sync the MEMORY.md ⭐ line. Path-limited commits, Co-Authored-By trailer, never push.

## LESSONS (all proven this session — encoded in playbooks/bodyport-wave.md)
- **Keystone-first, THEN independent fan-out.** Deep 10-layer loops against a FIXED
  baseline DOUBLE-COUNT foundational levers (wave-9: nominal +3066 → real +723; the
  Handle keystone re-derived ~12×). Land foundational levers as their own short wave.
- **One TU per discover lane** = structural dedup (avoids same-TU variant pileups).
- **Soft-rule** (user-confirmed): shared-header / math/Color.h / math/Utl.h /
  codebase-wide edits ARE allowed when principled + composed-verified net-positive
  (the Handle MILO_MESSAGE_TIMERS keystone +217 and MakeString by-value +23 were
  exactly this). Gate the JUDGMENT on the whole-binary A/B, never on the file touched.
- **Honesty-audit every pin own-vs-foreign** before landing (Waypoint reversal proved
  our own "refuted/dishonest" verdicts can be wrong — re-check, ~70% hold).
- **Big-scattered-TU negative**: never span-pin a TU whose fns scatter >0x4000 / across
  MB (MainHubPanel: 44/45 fns are ICF aliases = reveal-territory under foreign pins).
- Honesty gate: net>0 AND zero unexplained per-unit regressions AND no ≥8-contiguous
  FOREIGN fn_@0% run; headline net == sum of intended unit gains.

## CURRENT VEIN: meta_band belt port-then-pin (0x825B–0x826D), MATURING
RB3-specific game TUs the rb3-Wii oracle HAS but DC3 lacks, sitting unpinned in
.text gaps between existing pins. Pattern: locate gap → string/COFF-fingerprint
owner → port MWCC→MSVC → bound-pin vs both neighbours → ADD map entries → reveal →
composed A/B. Cost-per-match RISING (panels increasingly scatter; oracle thinning).

## WAVE-14+ FRONTIER (after wave 13 lands)
- The two big gaps' remaining bisected sub-TUs (wave-13's gapA/gapB scout lanes emit them).
- Dependency chains: ProfileMgr (after SaveLoadManager), NavListSortNode (after SongSortNode),
  SongSort.cpp.
- Sliver-evict/relocate: AccomplishmentPlayerConditional/SongConditional (GAP B).
- **Re-run pin_audit.py after each wave** (each landed port creates new sliver candidates).
- When the belt thins: pivot to (a) a fresh binary-wide pin_audit round, (b) the
  hash_map vein's still-unowned cluster-alpha [0x825B86A0,0x825C10D8), (c) FixedSizeSaveable.h
  template-overload batch (if multiple TUs need it → keystone-first), (d) the jeff
  asm-misnest truncation fix (coordinator/tooling, ../jeff src/cmd/xex.rs).
