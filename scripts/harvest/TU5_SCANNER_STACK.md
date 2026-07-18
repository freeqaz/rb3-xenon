# TU5 identification scanner stack — re-run runbook

Three-stage byte-identity / reloc-target correlator that flips anonymous target
`fn_<addr>` symbols to their MSVC-mangled names by matching them against the
compiled base objs, then splices the winners into
`scripts/target_symbol_map.json`. Every added map entry is a guaranteed
strict-100 flip. The stack is re-runnable at any build state via the scripts in
this directory.

## Scripts

- `tu5_gen_pairs.py` — enumerate paired units (target obj + base obj + >=1
  `fn_` < 100%) into `pairs.json` for the current build state.
- `tu5_correlate_stage1.py` — stage 1: CLEAN 1<->1 reloc-masked byte-identity
  sweep. Emits `map_fragment.json` (yield = `cur_pct<100` only).
- `tu5_icf_disambiguate.py` — stage 2: reloc-TARGET-identity discriminator over
  the base-side ICF-ambiguous (MULTI) pool. Filters `__unwind$` pollution.
- `tu5_target_twin_disambiguate.py` — stage 3: target-twin disambiguation, same
  discriminator family.
- `tu5_map_apply_fragment.py` — textual applier: splices a fragment after the
  opening brace of the map. NEVER json.dump-rewrites the map (hard convention).
- `tu5_reloc_masked_correlate.py` / `tu5_reloc_seq.py` — the shared primitives
  (`func_bodies`, reloc sequence resolution) the stages import.

## Re-run recipe (as executed by lane-C, 2026-07-18)

### 0. Setup + baseline
```
scripts/setup_worktree.sh ~/tmp/wt-X branchX      # buildable + diffable worktree
cd ~/tmp/wt-X && ./tools/ninja-locked 2>&1 | tee ~/tmp/rb3_build_X.log
# save the strict-matched set from build/45410914/report.json (fn names at 100%)
# so you can A/B each round: gained == picks applied, LOST must be empty.
```

### 1. Enumerate pairs
```
scripts/harvest/tu5_gen_pairs.py --project-dir ~/tmp/wt-X --out ~/tmp/wt-X/pairs.json
```

### 2. Run the three stages SEQUENTIALLY
Each stage: run -> apply its `map_fragment.json` -> `touch config/45410914/config.yml`
-> rebuild -> A/B check (gained == picks, LOST empty) BEFORE the next stage.

```
# stage 1
scripts/harvest/tu5_correlate_stage1.py --project-dir ~/tmp/wt-X \
    --pairs ~/tmp/wt-X/pairs.json --out-dir ~/tmp/wt-X/tu5_stage1
scripts/harvest/tu5_map_apply_fragment.py ~/tmp/wt-X/tu5_stage1/map_fragment.json \
    ~/tmp/wt-X/scripts/target_symbol_map.json
touch ~/tmp/wt-X/config/45410914/config.yml && (cd ~/tmp/wt-X && ./tools/ninja-locked)
# A/B: gained == fragment size, LOST empty

# stage 2  (env ICFDIS_PROJECT points the resolver at the worktree)
ICFDIS_PROJECT=~/tmp/wt-X scripts/harvest/tu5_icf_disambiguate.py \
    ~/tmp/wt-X/pairs.json ~/tmp/wt-X/tu5_icf
scripts/harvest/tu5_map_apply_fragment.py ~/tmp/wt-X/tu5_icf/map_fragment.json \
    ~/tmp/wt-X/scripts/target_symbol_map.json
touch ~/tmp/wt-X/config/45410914/config.yml && (cd ~/tmp/wt-X && ./tools/ninja-locked)
# A/B check

# stage 3
ICFDIS_PROJECT=~/tmp/wt-X scripts/harvest/tu5_target_twin_disambiguate.py \
    ~/tmp/wt-X/pairs.json ~/tmp/wt-X/tu5_twin
scripts/harvest/tu5_map_apply_fragment.py ~/tmp/wt-X/tu5_twin/map_fragment.json \
    ~/tmp/wt-X/scripts/target_symbol_map.json
touch ~/tmp/wt-X/config/45410914/config.yml && (cd ~/tmp/wt-X && ./tools/ninja-locked)
# A/B check
```

### 3. Iterate rounds to fixed point
Re-run `tu5_gen_pairs.py` (build state changed) and repeat step 2. Each newly
named function can reveal further clean pairings, so loop until all three stages
emit 0 fragment entries in a round (fixed point). Lane-C hit the fixed point in
round 2.

## Known traps
- `__unwind$` / `__ehhandler$` COMDATs carry CODE+fn flags in MSVC X360 objs and
  pollute candidate sets. Stages 2/3 filter them to real functions first; leave
  that filter in place.
- The 922-class `skip_no_S` coincidental matches are REJECTED on purpose — the
  precision gate is what keeps LOST empty. Do not relax it to chase yield.
- The map is TEXTUAL-INSERT-ONLY. Never json.dump-rewrite
  `scripts/target_symbol_map.json` (1-space indent, ~17k lines is load-bearing).
- You MUST `touch config/45410914/config.yml` after editing the map or the
  target-symbol renamer will not re-run and the flip won't register.
- Report unit names are `default/<rel>`: ~105 are pathed (`system/rndobj/Rnd`),
  the rest bare basenames; plus duplicate `auto_*` split-stub names (first wins).
- 13 duplicate basenames (Dir, Utl, Rnd, Movie, CubeTex, FxSend*, ...) are
  disambiguated in `tu5_gen_pairs.py` by masked-content overlap between the
  target obj and each candidate base obj.

## Yields
- Original landings (first-time sweep): stage 1 **+1,493** (`366709b9`),
  stage 2 **+118** (`01c1414f`), stage 3 **+234** (`1000b661`).
- Lane-C re-run at the 17,339 baseline: **+21 / +2 / +3 = +26**, 0 lost,
  fixed point reached in round 2.
