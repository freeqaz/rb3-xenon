# Wave-loop SOP + live state (2026-06-20, pre-compaction handoff)

## EXACT STATE
- main @ `c00664c`, **9404 / 65546 matched**, build green, tree clean (only
  untracked auto_*.obj/global_fuzzy_pairs.json/function_analysis/dc3_content_match.json).
- Worktrees: just main + pre-existing `rb3-sizedvec`. All wave 3–12 branches pruned.
- **Session 6932 → 9404 (+2472)** across waves 3–12, every landed lever composed-verified EXACT.

## ⚠ IN-FLIGHT: WAVE 13 is RUNNING
- Workflow `wf_6e9aaa4c-105`, task id `wxj0w8jxu`.
- Result lands at `/home/free/tmp/claude-1000/-home-free-code-milohax-rb3-xenon/e8dfac0c-3411-4ec7-a6ed-8da30a81c451/tasks/wxj0w8jxu.output` (JSON doc; access `doc["result"]`).
- Script: `…/workflows/scripts/wave12-metaband-belt-2-wf_98c92d16-cd2.js` (reused, retargeted to wave-13 @ BASELINE 9404).
- 7 lanes: gapA-bisect-port, gapB-bisect-port, SaveLoadManager, SongSortNode,
  SavedSetlist-retry (FixedSizeSaveable.h template overload — soft-rule, gate on composed A/B),
  AccomplishmentConditional-evict. On completion: HARVEST via the SOP below.

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
4. **Reduce** (Opus fan-in): landing guide (EV order + adjacent-pin overlap risk),
   same-file unions, regenerated next_frontier, vein_status.

## COORDINATOR HARVEST/LAND SOP (do this on every wave result)
1. Parse result compactly (don't dump big JSON into context): logs + `cleared[]`
   (tu, net_delta, branch, files_changed, _audit.honest) + reduce.next_frontier.
2. **De-dup**: trust the reducer's "winning variant per TU"; NEVER land a sibling
   that regenerated target_symbol_map.json wholesale (poison — re-pairs whole binary).
3. Land each cleared winner sequentially (rebase onto growing main → ff-merge):
   - Helper: `/tmp/land2.sh <branch-suffix>` rebases + auto-resolves JSON unions +
     splits union; defers on cascade. Uses `/tmp/resolve_json_union.py <wt> <relpath>`
     (stage-2/3 dict merge for target_symbol_map.json / objects.json) and
     `/tmp/resolve_splits_union.py <wt>` (stage-2 + theirs-added splits block).
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
