# W1-C — PCH port: apply + fully verify in a worktree, deliver a ready-to-land patch

Model: **opus**. Wave 1. **Worktree only — you must not modify rb3-xenon main.** Your output
is a verified patch (a `git format-patch` file) + measured numbers + gate evidence, which
W2-A lands on main in wave 2.

## Hard rules

- NEVER `git stash` / `git checkout <file>` / `git restore` / `git reset --hard` in any main
  repo. All work in a worktree under `~/tmp`:
  `scripts/setup_worktree.sh ~/tmp/wt-pch pch-port` (run from /home/free/code/milohax/rb3-xenon).
- Build ONLY via the worktree's `./tools/ninja-locked`, tee to `~/tmp/rb3_build_pch_*.log`.
- MSVC objs are NEVER bit-stable across recompiles (COFF TimeDateStamp, 4 bytes at offset 4).
  ANY byte comparison needs a same-settings recompile control. PCH additionally perturbs
  .debug$S/.XBLD$W/.drectve/.bss — so compare SECTION bytes (.text/.rdata/.data), never whole
  files.
- When done: `git worktree remove --force ~/tmp/wt-pch` (from main) unless handing the
  worktree itself to the orchestrator — prefer handing the patch file.

## Context

Goal: port dc3-decomp's proven MSVC precompiled-header build to rb3-xenon. Everything below
was researched and cross-checked; the boundary files ALREADY EXIST in rb3-xenon and are
byte-identical to dc3's:
- /home/free/code/milohax/rb3-xenon/src/system/decomp_pch.h (`#pragma once` +
  `#include "obj/Object.h"` + `#include "os/Debug.h"`)
- /home/free/code/milohax/rb3-xenon/src/system/decomp_pch.cpp (`#include "decomp_pch.h"`)
Both git-tracked, currently unreferenced by the build. Do NOT add decomp_pch.cpp to
objects.json (it is a build input, not a matchable unit).

dc3 reference implementation (same compiler, same flags, same engine — read these):
- /home/free/code/milohax/dc3-decomp/tools/project.py:192-195 (config fields), :777-803
  (the two rules), :897-960 (PCH edge), :1177-1191 (per-object eligibility switch)
- /home/free/code/milohax/dc3-decomp/configure.py:283-289 (enablement + eligible dirs)
- /home/free/code/milohax/dc3-decomp/docs/sessions/PCH_BUILD_OPTIMIZATION.md — dc3's byte
  verification: ALL .text sections byte-identical with/without PCH (HamCamShot.cpp, Mesh.cpp);
  only .debug$S (PCH signature), .XBLD$W (C1/C2 pass id), .drectve (/alternatename order),
  .bss (2 metadata bytes) differ; objdiff match% unchanged (99.6% either way).

Matching-safety rationale: MSVC /Yu restores the exact serialized front-end state /Yc wrote;
/FI injects decomp_pch.h at TU position 0. Codegen can only drift if the PCH headers carry
unbalanced #pragma state or /FI forces Object.h into TUs that lacked it (extra template
instantiations). dc3 constrained the PCH to exactly Object.h + Debug.h (stable,
pragma-balanced) — rb3-xenon inherits the same two headers unchanged. The gates below are
still mandatory.

Patcher/objdiff interaction: NONE. The 5 post-compile patchers + pre-compile renamer touch
only COFF symbol tables / machine code under build/45410914/src/** ; the PCH's throwaway
decomp_pch.obj lands in build/45410914/pch/ (outside their os.walk root, see
scripts/obj_anon_ns_patcher.py:29 SRC_DIR) and is not an objects.json unit, so objdiff/report
never see it.

IMPORTANT drift note: line numbers below were verified 2026-07-02 but main moves constantly —
anchor on the quoted code, not the numbers. Also: wave 1's W1-A task is concurrently changing
the msvc rule on MAIN (dropping the `bash -c … | transform_dep.py` pipe). Your worktree was
created BEFORE that lands, so your worktree's `msvc_cmd` is the piped `bash -c` form. The
`.replace()` anchor below works in BOTH forms — that is by design; your delivered patch must
apply cleanly to post-W1-A main (verify at the end with `git fetch`-less `git -C <main> diff`
awareness: rebase/re-verify the patch against main's then-current tools/project.py if W1-A has
already been committed when you finish).

## The 5 edits (make them in the worktree)

### Edit 1 — config fields, tools/project.py (ProjectConfig.__init__)

Insert after `self.extra_clang_flags: List[str] = []` (~line 191; `Set` is already imported
at line 30):

```python
        # Precompiled header (PCH) support (ported from dc3-decomp)
        self.pch_header: Optional[str] = None       # e.g. "decomp_pch.h"
        self.pch_source: Optional[Path] = None      # e.g. Path("src/system/decomp_pch.cpp")
        self.pch_eligible_dirs: Optional[Set[str]] = None  # parent-dir basenames eligible for PCH
```

### Edit 2 — the two ninja rules, tools/project.py

Immediately after the `msvc` rule block (the `n.rule(name="msvc", ...)` + `n.newline()`,
~line 765-772). Build both commands by `.replace()` on the FINAL `msvc_cmd` string. The anchor
is xenon's `"$cflags /showIncludes /Fo$out $in"` — present both in the current piped form and
in W1-A's unpiped form. Do NOT copy dc3's anchor (`$cflags /Fo$abs_out $in_win`) — it does not
exist here and the replace would silently no-op (rules become plain compiles; you'd notice
because no .pch is ever produced).

```python
    # MSVC PCH create rule (ported from dc3 tools/project.py:777-789)
    msvc_pch_create_cmd = msvc_cmd.replace(
        "$cflags /showIncludes /Fo$out $in",
        '/Yc"decomp_pch.h" /Fp$pch_out $cflags /showIncludes /Fo$out $in',
    )
    n.comment("MSVC PCH create")
    n.rule(name="msvc_pch_create", command=msvc_pch_create_cmd,
           description="PCH $pch_out", deps="msvc")
    n.newline()

    # MSVC build-with-PCH rule (ported from dc3 tools/project.py:792-803)
    msvc_pch_cmd = msvc_cmd.replace(
        "$cflags /showIncludes /Fo$out $in",
        '/Yu"decomp_pch.h" /FI"decomp_pch.h" /Fp$pch_file $cflags /showIncludes /Fo$out $in',
    )
    n.comment("MSVC build with PCH")
    n.rule(name="msvc_pch", command=msvc_pch_cmd, description="MSVC $out", deps="msvc")
    n.newline()
```

Add an assertion right after: `assert "/Yc" in msvc_pch_create_cmd and "/Yu" in msvc_pch_cmd,
"PCH replace anchor missing from msvc_cmd"` — turns the silent-no-op failure mode into a loud
configure failure (this protects W3-A's later rule change too).

Both rules carry `deps="msvc"` → ninja auto-tracks Object.h/Debug.h + transitive includes on
the create edge and auto-rebuilds the PCH; dc3 listed PCH staleness as an unmitigated risk —
xenon gets it for free.

### Edit 3 — the PCH build edge, tools/project.py

Insert right after `write_custom_step("pre-compile")` (~line 863), before the `# Source files`
comment. In scope: `build_path` (~487), `config.libs`, `make_flags_str`, `mwcc_implicit` (~694).

```python
    ###
    # PCH build edge
    ###
    pch_path: Optional[Path] = None
    if config.pch_source and config.pch_header:
        pch_dir = build_path / "pch"
        pch_path = pch_dir / "system.pch"
        pch_obj = pch_dir / "decomp_pch.obj"
        # The PCH MUST compile with the SAME flags eligible TUs use. All eligible
        # libs share the resolved 'base' cflags (objects.json: every lib uses
        # cflags key 'base'; zero per-object overrides), so take the 'engine'
        # lib's cflags + /TP -- byte-identical to what c_build produces for an
        # eligible .cpp. Do NOT absolutize /I (dc3 does; we must not): consuming
        # compiles use relative /I from the repo-root cwd and MSVC's PCH
        # consistency check requires create/use include sets to match exactly.
        pch_cflags_str = ""
        if config.libs:
            pch_lib = next((l for l in config.libs if l["lib"] == "engine"), config.libs[0])
            pch_cflags_str = make_flags_str([*pch_lib["cflags"], "/TP"])
        n.comment("Precompiled header")
        n.build(
            outputs=[pch_obj],
            rule="msvc_pch_create",
            inputs=config.pch_source,
            implicit=[*mwcc_implicit],
            implicit_outputs=[pch_path],
            variables={"cflags": pch_cflags_str, "pch_out": str(pch_path.resolve())},
            order_only="pre-compile",
        )
        n.newline()
```

Placement facts: build/45410914/pch/ is OUTSIDE build/45410914/src (patchers never visit);
order_only after pre-compile keeps the renamer-stamp ordering; pch_obj must NOT be added to
source_inputs/link/report. NOTE: scripts/setup_worktree.sh already pre-creates
`$WT_BUILD/pch/system.pch` on the cold path (a workaround for a WIBO_FS_CACHE
new-file-creation bug that wibo commit 6a7c37e fixed) — harmless either way.

### Edit 4 — per-object eligibility switch, tools/project.py c_build

Between the extab/shift_jis rule-selection chain (ends ~line 1013) and the
`n.comment(...)`/`n.build(...)` at ~1014-1022. `build_rule`/`variables`/`build_implcit`/
`all_cflags` are set by then (~988-995):

```python
            # Use PCH for eligible files (plain msvc rule, C++ mode, eligible dir)
            pch_implicit: List[Optional[Path]] = []
            if (
                pch_path is not None
                and build_rule == "msvc"
                and file_is_cpp(src_path)
                and "/TC" not in all_cflags
                and config.pch_eligible_dirs
                and src_path.parent.name in config.pch_eligible_dirs
            ):
                build_rule = "msvc_pch"
                variables["pch_file"] = str(pch_path.resolve())
                pch_implicit = [pch_path]
```

and change the existing `n.build(... implicit=build_implcit ...)` (~line 1020) to
`implicit=[*build_implcit, *pch_implicit]`. `pch_path` is a closure variable from Edit 3
(same generate_build_ninja scope). pch_path as a REAL implicit input means touching
decomp_pch.h rebuilds all eligible objs — correct.

### Edit 5 — enable in configure.py (Phase 1 = dc3's byte-verified dir set)

In configure.py, after `config.progress_all = False` (~line 324, just above the
custom_build_steps block; `Path` imported at line 19):

```python
# Precompiled header: engine dirs dc3 byte-verified (identical Object.h/Debug.h).
config.pch_header = "decomp_pch.h"
config.pch_source = Path("src/system/decomp_pch.cpp")
config.pch_eligible_dirs = {
    "rndobj", "hamobj", "char", "synth", "ui", "flow", "gesture",
    "world", "meta", "obj", "os", "utl", "movie",
}
```

= ~471 eligible cpp TUs under src/system. Deliberately EXCLUDED (do not add): math (0/18
Object.h inclusion), synth_xbox (2/28), bandobj, beatmatch, all band3/network game dirs —
low Object.h coverage = no benefit + higher /FI-instantiation risk; game dirs are "Phase 2",
a separate future campaign with its own gates.

## Verification protocol (ALL in the worktree; every gate mandatory)

Configure first: `python3 configure.py` (in ~/tmp/wt-pch — it re-bakes the worktree's absolute
tool paths from build.ninja's `configure_args`; if configure fails on wrapper validation after
W1-A lands, pass the same `--wrapper /home/free/code/milohax/wibo/build/release/wibo` the
worktree was created with). Confirm build.ninja now contains `/Yc` and `/Yu` rules and a
`build build/45410914/pch/decomp_pch.obj: msvc_pch_create` edge.

### Gate 0 — it works + measure (honest numbers)

- Full build: `./tools/ninja-locked 2>&1 | tee ~/tmp/rb3_build_pch_full.log`. Confirm
  build/45410914/pch/system.pch exists and eligible TUs compiled with the msvc_pch rule.
- Measure per-TU: pick one eligible TU (e.g. a src/system/rndobj unit). Time
  `./tools/ninja-locked <obj>` after touching its .cpp, with PCH on. Then temporarily remove
  its dir from pch_eligible_dirs, reconfigure, retime the same touch+rebuild. ≥5 timed runs
  each, report medians + the one-time PCH-create time (visible in the build log). Restore the
  dir + reconfigure afterwards.

### Gate 1 — section-byte gate (≥5 objs across dirs, timestamp-controlled)

For one unit each from ≥5 dirs (e.g. rndobj, char, utl, os, meta):
(a) build WITHOUT PCH twice (drop the dir from eligibility, reconfigure) → establish the
    same-settings noise floor (only the COFF timestamp at offset 4 should differ);
(b) build WITH PCH;
(c) extract and byte-compare the .text (and .rdata/.data if present) sections PCH vs no-PCH.
    Extraction: `llvm-objcopy --dump-section=.text=/tmp/x.bin <obj>` or python pefile/coff
    parsing — whatever you use, validate it on the (a) control pair first.
PASS = sections byte-identical. ANY .text delta = ABORT the task, report the offending unit.

### Gate 2 — objdiff gate

For the same ≥5 units: `mcp` run_objdiff (project_dir=~/tmp/wt-pch) per tracked function,
PCH on vs off → match% IDENTICAL for every function.

### Gate 3 — whole-binary report gate (EQUALITY)

Full build + report with PCH off (baseline in this worktree) vs on:
`python3 -c "import json;print(json.load(open('build/45410914/report.json'))['measures']['matched_functions'])"`
PASS CONDITION IS EQUALITY. A decrease = codegen drift (pragma leak / /FI instantiation) →
bisect by dropping dirs from the eligible set, identify the offender, EXCLUDE it, re-run all
gates, and note the exclusion in your report.

## Deliverables

1. `git format-patch` (or a committed branch `pch-port` in the worktree) containing exactly:
   tools/project.py (Edits 1-4), configure.py (Edit 5). Nothing else.
2. Verification evidence: gate outputs, measured per-TU savings, PCH-create time, the exact
   eligible-dir set that passed (if you had to drop any).
3. Confirmation the patch applies cleanly onto CURRENT main (`git -C ~/tmp/wt-pch fetch` is
   not needed — worktrees share the repo; check `git log -1 origin/main`-equivalent via the
   main checkout's HEAD and `git apply --check`). If W1-A landed and the msvc_cmd hunk moved,
   rebase the patch and re-run Gate 0 quickly (the anchor survives; only context lines shift).

## Rollback (for the record — W2-A inherits this)

Instant disable: `config.pch_eligible_dirs = set()` (or comment the three config.pch_* lines)
+ reconfigure — Edits 1-4 short-circuit on `pch_path is None`, every TU reverts to the plain
msvc rule, byte-identical to today. Full revert: revert the landing commit. decomp_pch.h/.cpp
stay (pre-existing, harmless).
