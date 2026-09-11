# BUILD OWED — `rc=0` now means "built from the source in this tree"

Lane BUILD-OWED, 2026-09-11. Code: `496f32a0`.
Files: `scripts/verify_objs_patched.py`, `scripts/test_patch_state.py`,
`scripts/orchestrator/patch_guard.py`.

## The defect

`--verify-manifest` got a v3 provenance upgrade the same morning (lane
GATE-DISC, merge `9468ed0e`): the manifest records git HEAD and the split
inputs, and provenance reasons are consulted **only inside the content-differs
path**, so a reason can *explain* an existing difference and can never
*manufacture* one. That property was reviewed and approved deliberately, and it
is correct.

⛔ It is also the blind spot. **A merge without a build moves the source and
rewrites no object**, so the object scan finds nothing to explain and returns
`rc=0` before provenance is ever read.

Hit in production the same day: a source lane was merged (+9 functions / +80 B)
with no build, `--verify-manifest` returned 0, and a peer session recorded a
ledger snapshot off that "clean" tree — **42,505** matched functions where the
built tree reads **42,514**. Nothing in the tree disagreed with the snapshot.

★ The tool answered the question it was asked. Two statements share the word
"clean", and every caller reads the second one off `rc=0`:

| | |
|---|---|
| (a) these objects are a **patched fixed point** | what was measured |
| (b) these objects were **built from the source in the tree right now** | what was assumed |

## The signal — and why it is not git HEAD

**The verdict is a content digest over an enumerated build-input set**, recorded
in the manifest at schema **v4** (`provenance.build_inputs`, superseding
`split_inputs`) and recompared on every `--verify-manifest`. 3,477 inputs /
24.3 MB / **0.058 s measured**, against the ~0.56 s the object scan already
costs.

⛔ **NOT git HEAD**, and this is the whole design, not a detail. Committing is
standing-authorized and constant here — docs, roadmap, ledger,
memory-adjacent commits. `patch_guard.ensure_patched_tree` turns **every**
non-zero into a hard `UnpatchedTreeError`, so a HEAD-keyed verdict would not
merely annoy somebody: it would **stop measurement fleet-wide until someone
disabled it**, which is how gates die in this repo. HEAD survives inside
`provenance` and still appears as a *reason* on the already-different path,
where it can only explain.

The input set is enumerated, never globbed, and each exclusion is load-bearing:

* `src/**` filtered to compilable extensions. `src/` carries 214 files of
  vendored `.am`/`.doc`/`.txt`/`.jpg`/`.vcproj` debris out of 3,674; hashing it
  would let a stray README owe a rebuild.
* ⛔ **NOT `config/<v>/**` as a glob.** That directory holds `scope_map.json`
  and `scope_map.json.bak`, **rewritten by the build itself** (measured: mtime
  02:55 on a build that started 02:54), plus dtk's generated `config.json`.
  Hashing a build's own outputs as its inputs makes every build owe another
  build — a gate that cannot go green. The five real pins are named instead.
* The wiring (`configure.py`, `tools/project.py`, `tools/defines_common.py`)
  and the object rewriters (six patchers + the target renamer + `obj_pairing`).
* ⚠ **NOT `verify_objs_patched.py` itself.** It rewrites no object. Including
  it would make every edit to it declare every tree in the fleet to owe a
  build — a pure nuisance signal carrying no statement about object state.

It errs **wide**, not narrow: `objects.json`'s declared sources are not the
build-input closure (CLAUDE.md's own trap — `rnddx9/Cam.cpp` is absent from
`objects.json` and is a build input, because three TUs `#include` it). The cost
of erring wide is one no-op build; the cost of erring narrow is the silent
stale measurement this exists to stop.

## ⛔ Why not ninja — measured on this graph, not inherited

Ninja's dirtiness computation *is* the question "would a build produce
different objects", so it was the obvious instrument. All legs below were run
in a worktree off `8b9a4d4b` **immediately after a settled full build that
reads `--verify-manifest` rc=0 green**:

| probe | settled tree | one stale `.cpp` |
|---|---|---|
| `ninja -n post-compile` | **3 edges pending** | 11 |
| `ninja -n all_source` | **2 edges pending** | 4 |

`CHECK SPLIT CURRENT` and `PATCH target fn_<addr> -> MSVC mangled names` are
**unconditionally dirty** — three consecutive *real* `./tools/ninja-locked
post-compile` runs left them pending. ⇒ **as a staleness count the PASS line is
unreachable**: vacuity #1 of `project_build_probe_vacuities_2026-08-01`,
reproduced verbatim on the X360 graph. It discriminates only against a
hardcoded floor ("more than 2 edges"), and that floor moves the moment
`configure.py` gains a step.

Two further measurements killed it outright:

* ★ **A `splits.txt` CONTENT change produces 0 ninja edges.** That is why
  CLAUDE.md's recipe is `touch config.yml && ninja`. Ninja is *structurally
  blind* to an input class that decides what the target objects contain — the
  recorded digest is not.
* **`ninja -n` OVER-reports downstream of an always-run edge**, because a dry
  run cannot do `restat` pruning. It listed the VERIFY/emit edge as pending on
  a settled tree while three consecutive real builds left the manifest's
  `generated_utc` untouched. Reading the dry run would have concluded the
  manifest was being refreshed on every build. It is not.

⚠ **Mtime is out for the same class of reason**: `scripts/setup_worktree.sh`
stamps every tracked file in a worktree to **2020-01-01**, so "input newer than
object" is not merely noisy but inverted, in exactly the trees every lane works
in.

## The verdict

**`6` — BUILD OWED.** Objects match the manifest exactly (nothing corrupt,
nothing patched outside the graph) and a build **input** moved.

`4` and `6` are both "a build is owed" and are deliberately separate, because
they license different conclusions: **4 means something already rewrote
objects** (a targeted `ninja <one>.obj`, a re-split); **6 means nothing has, and
the tree is merely BEHIND**. Collapsing them would hide a bypassed-patcher
build inside an ordinary pending rebuild.

Precedence, each with a test: object drift outranks build-owed (→ 4); a held
build lock outranks build-owed (→ 5, because *a running build is the owed
build*, and telling a caller to start a second one against a tree in motion is
the advice most likely to produce the corrupt-looking state GATE-DISC spent a
lane learning not to accuse).

A **v3 or older manifest** reports the question as **UNESTABLISHED** — a loud
note on stderr, `rc` unaffected — rather than guessing. Going red would fail
every tree in the fleet until it rebuilt; going silently green *is* the blind
spot, restored and wearing a green light. One build re-baselines it.

## `patch_guard` refuses on 6 — argued, not assumed

Refusing is right because the number would be reported under the current
source's name while describing the previous source's objects. That is not a
slightly stale measurement, it is a **mislabelled** one, and strictly harder to
detect afterwards than a low number.

What it costs, checked before choosing it:

* `build=True` (the default, and every orchestrator path) builds
  `post-compile` first, which compiles the changed TUs and re-emits the
  manifest — **so the state that produces rc=6 is repaired before the assertion
  runs, and the refusal is unreachable on that path.**
* `build=False` is the read-only look, and there rc=6 is exactly the case worth
  refusing.

⚠ **The residual, stated because silence would read as coverage:** a tree whose
changed inputs feed *no compile edge* (a `splits.txt` edit; a source file no TU
compiles or includes) does no ninja work, so the manifest is not re-emitted and
the verdict persists. The remedy text names the bounded re-baseline
(`--check --emit`, which re-verifies the patch fixed point before recording the
new input state) **and its precondition** — only after a full build reports *no
work to do*, i.e. only when the build system's own dependency knowledge says
these objects are current. Running it otherwise records stale objects as the
reference state and restores the exact blind spot this closes.

⛔ `patch_guard`'s fixed raw-compiler-output accusation is now **per-code**.
Printing that for rc=6 — where the objects are *perfect* — would repeat the
mechanism-it-cannot-observe defect GATE-DISC had just removed from the
verifier, the one that cost two investigations and produced two wrong
diagnoses.

## Evidence

Tests 17 → 26, both directions; `python3 scripts/test_patch_state.py` → **OK,
26 tests**.

★ **All 8 new tests FAIL against `HEAD`'s verifier** — 6 failures + 2 errors,
demonstrated by swapping `git show HEAD:scripts/verify_objs_patched.py` into
place and restoring it with a **sha256 byte check**, not by assuming the swap
worked.

The negative controls are the harder half and are real, not synthetic: a
git-backed fixture commits a docs file, **asserts HEAD actually moved** (if git
were broken both HEADs would be `None`, the term would be inert, and the test
would pass while proving nothing), and requires `rc=0`; the same fixture then
takes a source edit and requires `rc=6`.

Real tree, worktree off `8b9a4d4b`:

| leg | result |
|---|---|
| v3 manifest on disk | `rc=0` + explicit UNESTABLISHED note |
| rebuild | manifest v4 over **3,477 build inputs**, `tree_sha256` **unchanged** (`62c138ce…`) ⇒ the change perturbs no object |
| **negative control** — real commit of non-input code, HEAD moved | **`rc=0`**, silent |
| **positive** — one source file changed, not built | **`rc=6`**, names `src/band3/meta_band/BandUI.cpp`, "4291 objects match EXACTLY" |
| **self-heal** — full build | recompiled `BandUI.obj`, re-emitted manifest, **`rc=0`** |
| source restored byte-exact + rebuild | `tree_sha256` back to `62c138ce…` |
| **corruption control** — one byte flipped in an object, no build in flight | **`rc=1`**, "OUTSIDE the full build graph", object named, **no** BUILD OWED |

## What this lane did NOT do

* **Did not commit anything under `src/`.** The real-tree positive leg is an
  *uncommitted* source edit, restored byte-exact (`sha256sum -c` OK). The digest
  never consults git, so an uncommitted edit is the identical stimulus; the
  commit/merge semantics are covered by the git-backed fixture test instead.
* **Did not touch `configure.py` / `tools/project.py`.** A cleaner fix exists
  there — make the VERIFY/emit edge unconditional so *every* build re-baselines
  the manifest, which would eliminate the residual above entirely. It belongs
  to whoever owns those files.
* **Did not register `scripts/test_patch_state.py` in `scripts/test_tools.py`.**
  It is **not** currently registered, so these 26 tests have no CI caller and
  run only when somebody runs them. `test_obj_pairing.py` is registered;
  this file is not. That is a real gap and it is outside this lane's file list.
* **Did not re-measure the 42,505 / 42,514 incident.** It is quoted as the
  reporting lane recorded it, and is labelled as such in the tool's own output.
