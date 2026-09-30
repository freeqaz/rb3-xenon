# Post-compile patch stamps now depend on the split (lane W16-HW, 2026-09-30)

## Defect

Four of the six post-compile patchers (`anon_ns`, `guard`, `bool_mangle`,
`atexit_scope`) decide what to write by reading the **renamed target objects**
and **`objdiff.json`'s target<->base pairing**, not only our object. Their
stamps (`configure.py`, `post-compile` block) listed only `all_source` + the
script + the previous stamp. A change on the **target** side only (a
`splits.txt` re-home or a `target_symbol_map.json` rename) re-split and
re-renamed the targets but did not re-run those passes, because no decomp
object changed. The tree was left holding a pending patch. The `VERIFY` edge
(`patch_state.json`, which does depend on the split) then failed on **every**
build, and the verifier's own hint ("run a full build") could not clear it.

## Fix

The four target-reading stamps now also take
`build/45410914/target_symbol_renames.stamp` and `objdiff.json` as implicit
inputs, and the three that `import obj_pairing` also take
`scripts/obj_pairing.py`. The renamer stamp is the right signal: its edge
depends on `split_current_checked.stamp` (which moves on every split *run*),
`config.json` and the map, and it is touched only when that edge runs.
`dynamic_init` and `eh_boundary` read only the object in front of them and are
unchanged; the stamp chain re-runs them behind the others anyway.
`build.ninja` generated with and without the fix differs in exactly those four
edges (diffed, same configure args on both sides).

## Control

Worktree `~/tmp/wt-w16-hw`. Commits `20a1b2120` and `cad191b18` differ only in
`splits.txt`, `target_symbol_map.json` and `symbol_aliases.json` (W17-HCT
re-homed the HamCamTransform block), so switching between them re-splits and
compiles nothing. Logs are `~/tmp/rb3_build_w16hw_*.log`.

| leg | switch | build | PATCH edges run | `--check` |
|---|---|---|---|---|
| before fix | `cad191b18` -> `20a1b2120` | rc=1 | **none** | **FAIL** `anon_ns: 1 pending patch` |
| before fix | (same tree, rebuilt) | rc=1 | none | FAIL again (no way out) |
| after fix | -> `cad191b18` | rc=0 | all six (anon_ns 0 files) | OK |
| after fix | -> `20a1b2120` | rc=0 | all six, **anon_ns 1 file / 17 replacements** | **OK** |
| after fix | -> `cad191b18` | rc=0 | all six (anon_ns 0 files) | OK |
| after fix | -> `20a1b2120` again | rc=0 | all six, anon_ns 1 file / 17 | OK |

The pending patch was `system/hamobj/HamCamTransform.obj`: 17 occurrences,
`bf8abe13 -> fe071329`.

**No-op build.** The settled pre-fix no-op runs 5 edges, all driven by
`always`: CHECK SPLIT CURRENT, CHECK ICF-ALIAS MAP, CHECK MAP NAME-INJECTIVITY,
CHECK TARGET OBJS RENAMED, PROGRESS. After the fix, a no-op runs the **same 5
edges**: no PATCH, no VERIFY, no compile. Measured four times after the fix.

⚠ **One follow-up build (`noop.log`) recompiled 1,224 objects plus the PCH.**
It did not reproduce in three later switch/follow-up pairs run under
`-d explain`, which showed only the `always` edges. It cannot come from this
change: no compile edge gained or lost an input (see the `build.ninja` diff
above). The first pre-fix build of the session (`settle1.log`) also recompiled
1,240. It matches the intermittent post-build "second wave" CLAUDE.md records
under `ab_measure` (lane DS-1). The cause was not isolated.

## Residual, deliberately NOT fixed: anon_ns cannot un-apply a patch

The stamps now re-run, but `anon_ns` works **in place** and its `majority`
fallback keeps whatever hash the object already has. So a patch whose evidence
disappears is not reverted. Measured at `cad191b18`, with the same inputs on
both trees:

- after visiting `20a1b2120`: `HamCamTransform.obj` carries `fe071329` ×17 (sha1 `6bd0a1a3…`)
- after a fresh compile (`touch` the `.cpp`, full build): `bf8abe13` ×17 (sha1 `2fb99661…`)

**Both trees pass `--check`**, because each is a fixed point. So the object's
content depends on build history, not only on its inputs, and
`verify_objs_patched` is structurally unable to see that. The only instrument
that can is comparing against a fresh compile. At `cad191b18`, neither hash
appears in HamCamTransform's paired target (`fe071329` occurs only in
`obj/Crowd.obj`; `bf8abe13` occurs nowhere), so both are fallback spellings. A
score effect is therefore unlikely, but it was **not measured**. A real fix
needs the target-reading passes to see the raw compiler output whenever the
target side changes, for example by recompiling (cheap through objcache) when
the renamer stamp moves. That is a graph-design change outside this lane.
Until then, an A/B that crosses a split change should start from a fresh
compile (`rm -rf build/45410914/src` then a full build; objcache serves the
pre-patch compiler output, so this is cheap), not from a tree that visited
another commit.
