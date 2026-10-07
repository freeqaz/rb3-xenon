# W16-RY: the ICF-alias validator's 1918/146 vs 1912/152 flip (2026-10-07)

**Question.** `tools/icf_alias_finder.py --validate` read `1918 map-consistent / 146
STALE_SPELLING` at `774f9fe86` (`~/tmp/rq_val.log`) and `1912 / 152` at `2b280289f`
(`~/tmp/rw_val.log`). Between those commits nothing touched the map or the aliases. The
compiled side read 871,227 vs 871,360 symbols over the same 1,268 objs.

**Answer.** This is not a source effect and it is not a validator read-order bug. It is
**build-environment leakage through objcache.** The fix makes the counts
**1925 / 139 at both commits and in every build state measured**.

## The six spellings

All six are DepthBuffer3D.obj's copies of the Env_NG light helpers. `gesture/DepthBuffer3D.cpp`
scatter-includes `rndobj/Rnd.cpp` and `rndobj/Env_NG.cpp`, so the helpers are compiled a second
time inside its TU.

| group address | folded spelling |
|---|---|
| 0x82b85c58 | `?CheckPointLight@?A0x07a67bbc@@YA_NAAVNgLight@@@Z` |
| 0x82b85cf8 | `?CheckProjLight@?A0x07a67bbc@@YA_NAAVNgLight@@@Z` |
| 0x82b85b18 | `?ClearLightRegisters@?A0x07a67bbc@@YAXH@Z` |
| 0x82b85a30 | `?ClearLightTransforms@?A0x07a67bbc@@YAXXZ` |
| 0x82b85aa0 | `?ClearPointCubeTex@?A0x07a67bbc@@YAXXZ` |
| 0x82b860d0 | `?SetProjLightRegisters@?A0x07a67bbc@@YA_NHHAAVNgLight@@@Z` |

The survivors use Env_NG's retail hash `?A0x8e417309`, which `obj_anon_ns_patcher` writes and
which is stable. DepthBuffer3D's hash went from `07a67bbc` to `0d205712`. Retail's
DepthBuffer3D has no anonymous-namespace hash, so the patcher SKIPs that object (`original ...
has no anonymous namespace hashes`) and the raw compiler value survives.

## Mechanism, measured

1. **The raw hash depends on the compile root.** One TU, one source tree at `2b280289f`, with
   `OBJCACHE=off`:
   - compiled from the main repo root: `?A0x07a67bbc`
   - compiled from `~/tmp/wt-w16ry`: `?A0x68bc3f2e`
   - repeat compiles give the same value each time, so it is deterministic.
   - changing `Rnd.cpp`'s mtime has no effect.
   A whole-binary control recompiled all 86 objs that carry a non-retail hash uncached from
   the worktree root. All 86 changed hash, and nothing else in any symbol table changed.
2. **objcache does not key on the root.** Whichever root first compiles a given content
   populates the entry, and every other root is served that root's hash. W16-RW changed
   `Rnd.cpp` (inside `#ifdef HX_NATIVE`) and `Flare.h`, which made 169 TUs recompile. Those
   TUs were first compiled in a lane worktree, so main was served `0d205712`. That value is
   neither main's `07a67bbc` nor this worktree's `68bc3f2e`; it presumably belongs to the
   W16-RW worktree, which no longer exists. That attribution is inferred, not measured.
3. **The old stale test was literal** (`f not in compiled`), so it answered "which worktree
   populated the cache". JoypadMsgs, AmbientOcclusion, rndobj/Utl, Rot and TexBlender also
   lost raw-hash spellings in the same landing. Their groups did not flip only because they
   already had another stale member.
4. **The 871,227 → 871,360 count is noise.** Every symbol that differs is a compiler-numbered
   local label (`$M`, `$T`, `__unwind$N`, `__catch$N`, EH tables), renumbered by the +2 lines
   in `Flare.h`, or one of 783 re-rooted anon-hash names. No external mangled name changed.

**Score impact: none at this commit.** The cold-from-worktree leg and the cache-served leg
give identical `report.json`: 0 of 68,884 rows differ, and `matched_code` is 6,041,188 on
both. The six helpers are internal to an unscored TU. ⚠ This means "objcache-served objs
differ only in the COFF timestamp and `/Fo`" is false for these 86 objs. It is harmless to
the metric today, but nothing guarantees that.

## Fix (`tools/icf_alias_finder.py`)

- **Build-host hashes are compared by template, not literally.** A build-host hash is any
  `?A0x` hash that no retail spelling uses (live target objs plus
  `target_symbol_map.json`; 76 hashes). Retail-attested hashes are still compared literally.
  For each template, a group's k distinct spellings need k distinct compiled objs that emit
  it. This works because MSVC hashes only the primary `.cpp`: all 14 hash occurrences in
  DepthBuffer3D.obj share one value. Any spellings beyond k are STALE. The test does not
  claim which ones, because that is the root-dependent part. Applied to the same frozen
  index snapshots, the old literal rule reproduced 1918/146 and 1912/152 exactly.
- **Orphans are excluded.** The compiled side is now the glob restricted to the outputs of
  `ninja -t targets all`. If the graph cannot be read, it falls back to the glob and prints a
  warning. Two orphans were found, `hamobj/MeterDisplay.obj` and
  `hamobj/MiniLeaderboardDisplay.obj`, left behind when `0b8ec763c` moved those sources to
  bandobj/. They were the only referencers of 8 alias spellings, but excluding them changes
  the counts by 0 today. This is hygiene against a history dependence, not the cause of the
  flip.
- **New output line:** `build-host anon hashes: 229 folded spellings ... a literal test would
  answer N differently on THIS tree`. N was 30 at `774f`, and 78 at `2b28` both cache-served
  and cold. That spread is the root-dependent residue this change removes from the counts.
- **`--selftest`** has 6 new frozen cases. Two sabotages each turned it red: disabling the
  multiplicity check (2 FAIL) and wildcarding retail hashes (2 FAIL). The literal-mode
  control flips as it should.

| tree state | old literal | **new** |
|---|---|---|
| 774f9fe86, cache-served (main-root hashes) | 1918 / 146 | **1925 / 139** |
| 2b280289f, reflinked from main at 00:16 | 1912 / 152 | **1925 / 139** |
| 2b280289f, cache-served rebuild | 1912 / 152 | **1925 / 139** |
| 2b280289f, 86 raw-hash objs compiled cold from the worktree | 1912 / 152 | **1925 / 139** |
| 774f9fe86 rebuilt over that mixed-root tree | — | **1925 / 139** |

The 7 groups beyond the old 1918 are `clear` 0x82b9a138, `erase` 0x82766828,
`_Copy_Construct` 0x8271c2d8, `FindPanel` 0x82537620, `MainHubAdvanceMsg` 0x8261fbb0,
`ParticlePoolSize` 0x82446440 and `NewNetMessage` 0x825ab0f0. Each was STALE only because
its spelling carries a hash that no current obj and no retail obj spells. That hash was
captured from some other root or state. Each template is still emitted by exactly one
compiled obj. All 7 runs gave `0 contradicted`; the verdict never moved.

**Not done:** `tools/icf_alias_build.py` and `pool_referencing_unit_stems` still glob and
compare literally. A root-independent compile would remove the cause, for example wibo
presenting a fixed path to cl so the anon hash stops encoding the worktree. That is a fleet
tool change and is left to the coordinator.
