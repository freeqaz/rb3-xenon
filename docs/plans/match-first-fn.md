# Plan: drive `fn_8275A2C0` (`MasterAudio::SetupTrackChannel_`) to byte-match

**Goal:** Land the project's first matched function. Pipeline is already
end-to-end green for `MasterAudio.cpp`; this plan closes the gap from
"`MasterAudio.obj` compiles" to "objdiff certifies fn_8275A2C0 byte-equal."

## Target

`build/45410914/asm/MasterAudio.s:2627-2634` — 0x14 bytes at `0x8275A2C0`:

```
8275A2C0  89 05 00 08   lbz  r8, 0x8(r5)    # info.mDuckable -> b5
8275A2C4  C0 25 00 04   lfs  f1, 0x4(r5)    # info.unk4      -> f3
8275A2C8  88 E5 00 09   lbz  r7, 0x9(r5)    # info.mVocalCue -> b4
8275A2CC  88 A5 00 0A   lbz  r5, 0xa(r5)    # info.mVocal    -> b2 (clobbers &info, last)
8275A2D0  4B FF F7 A8   b    fn_82759A78    # tail-call ::SetupTrackChannel
```

Source (verified `src/system/beatmatch/MasterAudio.cpp:258-260`):

```cpp
void MasterAudio::SetupTrackChannel_(int i, ExtraTrackInfo &info) {
    SetupTrackChannel(i, info.mVocal, info.unk4, info.mVocalCue, info.mDuckable);
}
```

Argument-to-register mapping (PPC X360 ABI, member fn → `this=r3`, `i=r4`,
`b2=r5`, `f3=f1`, `b4=r7` (r6 skipped because `f3` consumes a slot? actually
the ABI here pairs float in f1 *and* still consumes r6 in some MSVC variants —
the target asm shows r6 untouched, so the compiler model is consistent with
the source). The store-order in the target (`r8, f1, r7, r5`) is the
classic MSVC scheduling pattern: `&info` is in r5, so the
overwrite-`r5`-last constraint forces this exact ordering. If we get this
ordering wrong, we have a real codegen mismatch, not just symbol noise.

`ExtraTrackInfo` layout (verified `MasterAudio.h:116-124`):
```
0x0  bool  mPlayable
0x4  float unk4         (compiler pads bool->float)
0x8  bool  mDuckable
0x9  bool  mVocalCue
0xa  bool  mVocal
```
Loads at `0x4`, `0x8`, `0x9`, `0xa` in the target match this layout exactly,
so no struct-layout work is required.

## Background already verified (do NOT re-check)

- `build/45410914/asm/MasterAudio.s` exists; `fn_8275A2C0` and `fn_82759A78`
  (the 5-arg callee) and `fn_8275A2D8`/`fn_82759BC0` (the Background pair)
  are all present.
- `MasterAudio.cpp` compiles in main: `build/45410914/src/system/beatmatch/MasterAudio.obj`
  is 241,927 bytes (Ninja-built 2026-05-26 21:08).
- The dtk target obj exists: `build/45410914/obj/MasterAudio.obj` (32,609 bytes).
- Compiled symbol names (probed by reading the COFF symtab):
  - `?SetupTrackChannel_@MasterAudio@@QAAXHAAVExtraTrackInfo@1@@Z`
  - `?SetupTrackChannel@MasterAudio@@QAAXH_NM00@Z` (the callee, **with `_N` back-refs `00`**)
  - `?SetupBackgroundChannel_@MasterAudio@@QAAXHAAVExtraTrackInfo@1@@Z`
  - `?SetupBackgroundChannel@MasterAudio@@QAAXH_NM00@Z`
- `tools/bindiff_match.json` (11,057 entries) has **no entry** for
  `0x8275A2C0`, `0x8275A2D8`, `0x82759A78`, or `0x82759BC0`. Cluster sat
  below BinDiff's confidence threshold. We're matching this purely by
  source + structural shape, not by cross-binary identification.
- Bool-mangle patcher (`scripts/obj_bool_mangle_patcher.py`) is wired
  in `configure.py:286-294` and runs `--batch --apply` every build.
  It rewrites `_N00` → `_N_N_N` style. The callee mangling above
  (`H_NM00`) is exactly the pattern it targets.

## Load-bearing assumptions to verify FIRST (per [[verify-load-bearing-assumptions]])

These are the things that, if wrong, sink the plan. Verify each before
spending time on the iteration loop:

1. **The `_` suffix on the source name is correct.** The dtk asm names the
   function `fn_8275A2C0` (anonymous); we are asserting it is
   `MasterAudio::SetupTrackChannel_`. Evidence: the body matches the
   source exactly (5-arg tail-call to `SetupTrackChannel` reading
   `ExtraTrackInfo` fields). The Background pair at `fn_8275A2D8`
   tail-calling `fn_82759BC0` confirms the structural pattern. **Cost
   of being wrong is low** (we'd discover via diff), but cross-check
   `MasterAudio.cpp:209-210` which takes `&MasterAudio::SetupTrackChannel_`
   as a member fn pointer — those exist because the public `SetupTrackChannel`
   isn't a member-fn-ptr candidate.

2. **Compiled obj actually contains a 0x14-byte body matching the target
   bytes.** This is what we'll check in Step 1 below. If the compiled body
   is *not* the same bytes, the plan changes from "fix symbol mangling" to
   "fix codegen" — fundamentally different.

3. **Bool-mangle patcher really ran on our obj.** Stamp at
   `build/45410914/bool_mangle_patched.stamp`. If absent or older than
   the obj, the patcher silently no-op'd (or the obj was rebuilt
   afterward).

## Step 1 — Confirm compiled body bytes

Extract the body of `?SetupTrackChannel_@MasterAudio@@QAAXHAAVExtraTrackInfo@1@@Z`
from our compiled obj and compare to the target's 0x14 bytes.

```bash
# Method A: objdiff one-shot diff, function-scoped
/home/free/.local/bin/objdiff-cli diff \
    -1 build/45410914/obj/MasterAudio.obj \
    -2 build/45410914/src/system/beatmatch/MasterAudio.obj \
    --include-instructions --summary --verdict \
    '?SetupTrackChannel_@MasterAudio@@QAAXHAAVExtraTrackInfo@1@@Z'
```

(`-1` is target / `-2` is base per `objdiff-cli diff --help`.)

If the symbol name in `-1` (target) doesn't match (target obj uses
dtk-anonymous `fn_8275A2C0`), drop the symbol argument and rely on
objdiff's auto-pairing; or pass `--map-file` if a linker map exists. The
expected outcome is a per-instruction diff. **Expected**: instructions
identical, but symbol/relocation labels differ (target references
`fn_82759A78`, our obj references the mangled `?SetupTrackChannel@...`).

**Decision branch:**

- If instructions are identical → go to Step 2 (symbol/reloc work).
- If instructions differ → go to Step 3 (codegen iteration).

## Step 2 — Symbol & relocation reconciliation

The compiled obj's `b fn_82759A78` tail-call is encoded as a relocation
against `?SetupTrackChannel@MasterAudio@@QAAXH_NM00@Z`. The target obj
references `fn_82759A78` (an internal label). objdiff needs to equate
these. Two paths:

1. **Map-file path.** Generate or supply an objdiff `--map-file` that
   maps `?SetupTrackChannel@MasterAudio@@QAAXH_NM00@Z` ↔ `fn_82759A78`.
   No MSVC link map exists for the retail XEX, so this means hand-writing
   a small map for the matched pair.
2. **Symbol-rename path.** Use the patcher infrastructure to rename our
   compiled `SetupTrackChannel` callee symbol to a name the target obj
   uses for the same address. This is the dc3 pattern; see
   `scripts/obj_bool_mangle_patcher.py` for the reference rewriting code.

Check first whether the bool-mangle stamp exists and is fresh:
```bash
ls -la build/45410914/bool_mangle_patched.stamp
ls -la build/45410914/src/system/beatmatch/MasterAudio.obj
```
If stamp is older than the obj, force re-patch:
```bash
python3 scripts/obj_bool_mangle_patcher.py \
    build/45410914/src/system/beatmatch/MasterAudio.obj \
    --apply --verbose
# (or just `ninja` after touching configure.py to invalidate the stamp)
```

Then re-probe symbols (the script in this plan's research did this; see
"Compiled symbol names" above). If the callee is still
`?SetupTrackChannel@MasterAudio@@QAAXH_NM00@Z` and that's what blocks
objdiff from pairing, the bool-mangle patcher *isn't actually finding a
matching original symbol to expand to*. The patcher uses
`find_bool_backref_mismatches`, which only fires when an `orig_symbols`
entry exists — but the target obj's symbols for `fn_82759A78` etc. are
anonymous (dtk-generated `fn_…` labels), so there's nothing to expand
toward. **This is the likely root cause: bool-mangle is a no-op on this
TU because the target has no named symbols to anchor to.**

If that diagnosis holds, prefer the map-file path (Step 2.1) — handcraft
a one-line objdiff symbol-equivalence config rather than fighting the
patcher.

## Step 3 — Codegen iteration loop (only if Step 1 showed instruction diffs)

Likely divergences, in priority order:

1. **Source-arg-order mistake.** Our source passes
   `(i, mVocal, unk4, mVocalCue, mDuckable)` — which is what the target
   does. Verified, no change needed. **But** the docstring in this plan's
   intro had the param order slightly off; trust the source, not the prose.
2. **`ExtraTrackInfo` layout drift.** Field offsets in our header
   (0/4/8/9/0xa) match the target loads. No padding `__declspec(align)`
   needed.
3. **`lbz` vs `lhz`/`lwz`.** /O1 picks the smallest typed load for bool.
   If the compiler chose otherwise, check that `bool` is 1-byte
   (`sizeof(bool) == 1`) in our toolchain.
4. **Tail-call vs call.** Target uses `b` (tail) not `bl`. The source is
   a clean tail return (no work after the call, no stack frame). /O1
   should pick `b`. If it picked `bl + blr`, force tail by ensuring no
   locals / no destructors live across the call (none in this fn).
5. **Calling-convention oddity.** Member fn → `this` in r3 implicitly.
   Our compiled obj must use the same `__thiscall`-equivalent (MSVC X360
   member-fn calling convention). Check object's `Q` (public) vs target
   inferred access; declared `public` in `MasterAudio.h:175,201,202`.

For any per-instruction mismatch surfaced by objdiff, the workflow is:
look at the diff, hypothesize a source-side fix, edit, `ninja`, re-diff.

## Step 4 — Definition of done

Both must be true:

1. `python3 configure.py progress` (or whatever the build emits — check
   `progress.json` in `build/45410914/`) shows `matched_functions ≥ 1`.
2. The objdiff command in Step 1 prints `match: 100.00%` (or its
   verdict-mode equivalent: `verdict: matching`).

If only (2) is true but (1) isn't, the splits.txt range or
`config.yml` section pinning doesn't cover this address. Verify
`config/45410914/splits.txt` has a `.text` range for `MasterAudio.cpp`
that **contains** `0x8275A2C0`. The cluster derivation already pinned
the cluster, so this is unlikely but worth a sanity check.

## Step 5 — Fallback ladder

1. **If `fn_8275A2C0` matches but progress doesn't tick:** check
   `splits.txt` range, then `objects.json` status (must be `NonMatching`
   not `MISSING` for it to be picked up; flip to `Matching` only when
   confirmed).
2. **If `fn_8275A2C0` resists matching:** try `fn_8275A2D8`
   (`SetupBackgroundChannel_`). Same shape, same patcher needs, same
   fallback ladder. If both fail identically, the fix is in the patcher
   / map-file path, not in source.
3. **If both wrappers fail but the 5-arg callees (`fn_82759A78`,
   `fn_82759BC0`) coincidentally match:** rare but possible; in that
   case the wrappers' issue is purely symbol-reloc, not codegen, and
   the map-file path is forced.
4. **If bool-mangle patcher is the blocker:** rather than enhance the
   patcher to handle anonymous targets, supply objdiff a one-off
   symbol-equivalence config (`-c symbol_equivalence=...` if supported,
   else `--map-file`).

## Step 6 — Risks

- **`unk4` field name is a guess.** dc3-decomp may have named it
  differently. ABI doesn't care (offset 4, size 4, float), but if dc3
  ported this header later and renamed `unk4` to `mCueVolumeMultiplier`
  or similar, the source merge from `../dc3-decomp` may shift the
  *next* field's offset. Verified offsets are stable per `MasterAudio.h`
  in this tree; treat as low-risk for *this* fn but bookmark for the
  next.
- **Dynamic-init patcher already ran on 8 symbols in `MasterAudio.obj`
  this session** (per task brief). Confirm it touched none of the
  `SetupTrackChannel*` symbols — they have no `??__E` dynamic
  initializer prefix, so should be safe, but a stray substring match
  in `obj_dynamic_init_patcher.py` could collide. Verify by reading
  the patcher's match regex before assuming.
- **objdiff CLI version drift.** Local fork at `../objdiff` — confirm
  `/home/free/.local/bin/objdiff-cli` is from the fork (`-V`) and not
  the v3.2.1 download. Some flags above (`--verdict`, `--analyze`)
  may be fork-only.

## References

- Source: `src/system/beatmatch/MasterAudio.cpp:258-260`
- Header (layout + sig): `src/system/beatmatch/MasterAudio.h:116-124,175,202`
- Target asm: `build/45410914/asm/MasterAudio.s:2627-2634` (and 2637-2644 for
  Background sibling, 2025-2113 for `fn_82759A78` callee body)
- Patcher: `scripts/obj_bool_mangle_patcher.py` (full file; pipeline wiring
  at `configure.py:244-305`)
- Bindiff DB: `tools/bindiff_match.json` (no entry for this fn — confirmed
  via grep for `8275A2`, `82759A78`, `82759BC0`)
- objdiff CLI: `/home/free/.local/bin/objdiff-cli` (release build of fork at
  `../objdiff/target/release/objdiff-cli`) — `diff --help` for full flag set
- Verify-assumptions memory:
  `~/.claude/projects/-home-free-code-milohax-rb3-xenon/memory/feedback_verify_assumptions.md`

## Numbered execution steps (for Sonnet impl agent)

1. **Verify stamp freshness.** `ls -la build/45410914/bool_mangle_patched.stamp build/45410914/src/system/beatmatch/MasterAudio.obj` — stamp newer? If not, `ninja` (it's cheap; rebuilds nothing if obj is current, just re-runs patcher).
2. **Run objdiff for the function-scoped diff** using the Step 1 command. Capture output. If the symbol arg fails, drop it and grep the full output for `fn_8275A2C0`.
3. **Branch on result:**
   - All 5 instructions match → Step 4 (symbol/reloc work in Step 2 of this plan).
   - One or more mismatch → Step 5 (Step 3 of this plan: codegen iter).
4. **Symbol/reloc path:** check what objdiff reports as the diff. Most likely the `b fn_82759A78` shows as `b ?SetupTrackChannel@MasterAudio@@QAAXH_NM00@Z` (unrelocated) — feed objdiff a symbol-equivalence hint. Try `--map-file` with a 2-line file:
   ```
   0x82759A78 ?SetupTrackChannel@MasterAudio@@QAAXH_NM00@Z
   0x82759BC0 ?SetupBackgroundChannel@MasterAudio@@QAAXH_NM00@Z
   ```
   (exact format per objdiff fork docs; check `../objdiff/README.md`).
5. **Codegen path:** iterate per Step 3 of this plan.
6. **Confirm done:** re-run progress build (`ninja`), grep `progress.json` for `matched_functions`. Expect ≥ 1. Also visually re-run the objdiff diff and confirm verdict.
7. **If done:** flip `MasterAudio.cpp`'s entry in `config/45410914/objects.json` from `NonMatching` to `Equivalent` for *just `fn_8275A2C0`* (not the whole TU). If objdiff doesn't support per-function status, leave the TU at `NonMatching` — match still registers in the byte/function count.
8. **If stuck after one iteration cycle:** drop to `fn_8275A2D8` per fallback ladder. If both fail identically, escalate to user with the objdiff diff output — likely needs a patcher extension or map-file format clarification.
