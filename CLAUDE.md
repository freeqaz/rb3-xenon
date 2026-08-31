# rb3-xenon — Claude Context

## ★ Think out loud (applies to every turn, every agent)

**Narrate your work in visible output as you go.** Say what you are about to do,
why you chose it over the alternative, what you expect to see, and then what you
actually saw — *before* moving on. Do not batch a silent run of ten tool calls
and emit only a conclusion.

**Why: the log is the deliverable.** This work is reviewed after the fact —
weeks later, by someone (or some agent) who was not there and cannot re-run the
session. The transcript is the only record of *why* a lever was chosen, what was
measured, and what was ruled out. Hidden reasoning is not recoverable; only the
visible trajectory and narration survive. So write for that reader: a correct
patch with no narration teaches nothing and cannot be audited, while a narrated
dead end stays useful forever — it stops the next lane from re-hunting a drained
vein.

What "out loud" means concretely:

- **Before a tool call / batch:** one line on intent — *"reading `splits.txt` to
  confirm the `.text` span before pinning"*, not silence.
- **Before a decision:** name the alternatives you rejected and the reason.
  *"Doing this in a worktree rather than main because the header change cascades
  to ~281 TUs."*
- **State your expectation first, then the result.** A prediction that fails is
  the most informative line in a transcript — call it out explicitly (*"expected
  +12, got 0 — so the cause is shared, not per-unit"*).
- **Surface surprises immediately**, especially ones that contradict a memory,
  a doc, or an earlier claim of yours. Say what changed your mind.
- **Say what you did NOT do** and why — deferred walls, skipped units, unverified
  assumptions. Silence reads as coverage.
- **Numbers with provenance:** quote the measured value and how it was measured
  (which leg, which build, cache cold/warm). "Improved" without a number is noise.

This applies to subagents too — a subagent's final report is the only thing that
survives it, so it must carry the reasoning, not just the verdict. Keep it prose
and proportional: a sentence or two per step, not an essay per tool call.

---

Decompilation of **Rock Band 3** for **Xbox 360** (PowerPC). Goal: matching
machine code from C++ source. Target binary: vanilla retail XEX, title ID
`45410914`, at `orig/45410914/default.xex` (not committed).

**Docs index: `docs/INDEX.md`** — audited master index of every doc under
`docs/` (2026-07-06): current references vs `[HIST]` frozen records, plus a
"known traps" box. Check it before trusting any doc's current-state claims;
stale/boilerplate docs carry a `> **STATUS (2026-07-06):**` banner.

**Optimization level: `/O1 /Oi /GR /EHsc` (+ implicit `/fp:fast`) — retail
size-optimized release, no LTCG. ✅ VERIFIED AGAINST RB3 RETAIL (lane CB-4,
2026-07-30), no longer inferred from dc3.** Every one of seven perturbations
scored *worse* and none scored better, so the "metric-fitted build config" hazard
never arose (that hazard only bites when you have to interpret a **gain**):

| evidence class | finding |
|---|---|
| `??_R4` Complete Object Locators in retail `.rdata` | **2,220** ⇒ **`/GR` ON** |
| `FuncInfo` magic `0x19930522`, `EHFlags == 1` (`FI_EHS_FLAG`) on **all 8,541** | **`/EHs*` synchronous** — not `/EHa`, not EH-off |
| Rich header: 401×`0x00AA` + 1,760×`0x00AB` plain C/C++ (a real `/GL` obj has machine `0x0000` and **no** `@comp.id`) | **no LTCG — now verified, not inferred** |
| `bl __security_check_cookie`: **12** sites in one 24 KB span (DC3: **599** over 6 MB) | **`/GS` OFF** for Harmonix code |
| whole-binary A/B (all legs on the correct 10224 compiler) | `/O2` **−13,813** matched / −29.00 pp · `/Ou` −14,255 / −29.17 pp · `/GS` −598 · `/Oi-` −465 · `/fp:precise` −47 · `/EHs` (dropping the `c`) −43 |

⚠ **The era-inference was right about the flags and wrong about the compiler
build** — RB3 retail used cl **10224**, dc3 used **11886** (see
`docs/decomp/xdk-11164-compiler.md`). Don't let the flags being confirmed
retro-justify the reasoning; it was the same reasoning either way.

★★★ **`/GR` can NEVER be settled by a match-% sweep: `/GR-` is `.text`-byte-identical.**
Only the `.rdata` COL evidence answers it — which is why the binary-evidence class
was mandatory rather than a nicety. ⚠ And RTTI **type-name strings are NOT a `/GR`
test**: `/GR-` drops `??_R4` 44→0 but `??_R0` only 73→40, because EH also emits
them. Two control failures shaped this result — the first `/GS` cookie detector
was **vacuous** (0 hits on a known-`/GS` object, because it assumed `lis`/`lwz`
adjacency and the compiler schedules two instructions between them) and was caught
before it could produce a false "retail has no `/GS`".

Also measured: `/Gy` and `/GF` are byte-identical to default (already on), `/Oy`
is a **no-op on PowerPC**, `/Ob2` is byte-identical to `/O1` (validating the claim
below), and `@comp.id` is unchanged across **all 22 flag legs** ⇒ the Rich header
encodes **no** flag information (a useful bound on that instrument).

⛔ **Still unsettled:** the `c` in `/EHsc` rests on the metric alone (−43); the
per-function EH instrument **failed to discriminate** (TIGHT 99 under both). And
the Rich header's 401 C vs 1,760 C++ objects implies a `/TC`//`/TP` split nobody
has examined.

★ **Per-TU flag heterogeneity is no longer unexplored — it is MEASURED** (lane
CF-4, 2026-08-01), with a control:

| finding | value |
|---|---|
| `/Od` region (Quazal NetZ) | **`0x82A6D168`–`0x82B54190`**, 5,782 fns / 933,792 B; 2,352 flagged `/Od` (61.8% of bytes) |
| separate `/Od` island **inside** HMX code | `keygen_xbox.s` @ `0x82724A90` |
| control: named HMX fns below `0x82A00000` flagged | **0 / 13,676** (MasterAudio 0/57, BandDirector 0/117, BandCharacter 0/165) |
| enrichment | ≈ **413×**, exactly one false positive |

⇒ **`/Od` is OBJECT-granular, not address-range** — the `keygen_xbox` island proves
it. Two corrections to this doc's own prior claims: the **`0x82A00000` boundary is
~450 KB too low**, and **"vendor ⇒ `/Od`" is FALSE** (zlib `inflate`/`deflate` are
textbook `/O1`).
⚠ The detector only became trustworthy once the **untreated-population control**
was run — the raw "unoptimized score" alone fires on any large `/O1` function under
register pressure, i.e. it confirms whatever you point it at.
⚠ Side-findings for a map lane, **flagged but unverified**: nine named engine units
each claim exactly one function inside the Quazal block, and `CameraManager.s`
claims 13 at `0x82B02748`–`0x82B031A0` — almost certainly bad `.text` pins reading
as false 0%.

★★★ **The old "4,364 addresses (3.58%) disagree ⇒ filter `.s` to files newer than
2026-07-15" note is REPLACED — it was wrong three ways, and the filter it
prescribed is NOT SAFE.** Measured 2026-08-04:

- **`4,364` was a FILE count mislabelled as an ADDRESS count.** The real
  disagreement is **1,747,064 of 1,965,755 doubly-covered addresses (88.87%)**,
  plus 32,305 addresses that exist *only* in stale files.
- **The mtime filter catches 39% of the problem.** 4,388 stale files predate the
  cutoff; **6,763 (61%) are NEWER and slip straight through** (3,857 from 07-26,
  1,947 from 08-01, more on 08-02/03). It is insufficient, not merely a
  workaround.
- **Staleness was 82.2% of the tree**: 111 named orphans + 11,040 stale `auto_*`
  = **11,151 stale `.s`** and **11,173 stale `.obj`**, because split rewrites the
  live set every run and never removes what it stopped emitting. Now fixed —
  `tools/prune_split_outputs.py` runs on every successful split, pruning against
  dtk's own `config.json` (and refusing on a 0-unit config).

⛔⛔ **AND NO MTIME FILTER COULD EVER HAVE FIXED IT, because the `.s` ADDRESS AND
FILE-OFFSET COLUMNS ARE SYNTHETIC for multi-block units** — dtk computes them as
`first_block_start + cumulative section offset`, so they name addresses the
function does not live at (`Timer.s` renders `fn_82511430`'s body as `82270294`;
retail has `00 00 00 00` there and the real body at `0x82511430`). **Key every
asm-wide scan on the `.fn fn_<addr>` symbol, NEVER the address column.**
⚠ "live-vs-live disagreement is 0" is **VACUOUS** — 0 addresses are covered by
≥2 live files. The honest statement is structural: the live split set is a clean
partition, one opinion per address.

**Crucially, no whole-program optimization** — TU spatial grouping in `.text` is
preserved (empirically: the MasterAudio.cpp cluster of 46 functions packs into
8 KB).

What `/O1` does: `/Ob2` aggressive within-TU inlining (so leaf math like SHA1's
K-constants disappear into callers), but *no cross-TU reordering or whole-program
inlining*. dc3's linker map tags ~32k functions `f i` — those are `/Ob2` inlines,
not LTCG magic.

★ **ICF is a LINKER option (`/OPT:ICF`), NOT a `/O1` effect** — this doc said
otherwise for months. **Retail RB3 does have it**, verified on `band.exe` (lane
CD-7, 2026-07-31): over 40,609 non-funclet `.pdata`-sized functions, bodies
identical **including call targets** show only **51 surplus copies — *below* three
random-offset nulls (115/158/170)** — while bodies identical **ignoring** call
targets survive **3,967 times in 1,061 groups**. That ~78× gap *is* the signature
of relocation-restricted folding; full folding would collapse both populations and
no folding would leave both intact. Cross-binary positive control: the same
scanner on **DC3 — proven ICF-ON by its leaked map** — produces the same
signature (28 reloc-identical vs 4,941 shape-identical surplus).

⇒ **MSVC folds only COMDATs identical *including relocations and associated
`.xdata`*.** So byte-*similar* bodies at distinct addresses are **expected and do
NOT refute ICF** — e.g. `_List_base<T>::clear` has 42 addresses, reloc-identical
surplus **0**, because its members differ in four `bl` targets (per-`T` node
deallocators). Folding is near-total in HMX code (**6** surplus / 32,580) and
**17× weaker in vendor/CRT code ≥ `0x82A00000`** (25 / 8,029) — a concrete
instance of the per-TU flag heterogeneity flagged as unexplored above, likely
`/Gy`-off monolithic non-COMDAT `.text`.

⛔ **SCOPE BOUND on every number above: the population is `.pdata`-sized
functions, which is NOT a function census — the whole sub-`.pdata` stub stratum
is excluded BY CONSTRUCTION** (lane AUDIT-NC, 2026-08-13, `9143ac89`). **0 of
31** candidate tiny alias stubs are `.pdata` BeginAddresses — *including* an
allocator survivor with fan-in **1,048** — because an **8-byte leaf stub touches
neither the stack nor LR, so it gets no unwind record.** ⇒ the HMX `6 / 32,580`
and vendor `25 / 8,029` figures **license no claim about tiny stubs in either
direction**; do not brief either one as an attack line on the stub stratum (one
lane was briefed off the vendor-band figure and it was inapplicable).
⚠ Corroboration, measured independently (lane MAP-FIX, 2026-08-13): **7,472 of
28,956 named `target_symbol_map.json` rows (25.8%) are not `.pdata`
BeginAddresses** at all.
⇒ **CD-7 is CORRECT WITHIN ITS POPULATION and its methodology stands** —
relocation-normalized body hashing against a random-offset null remains the only
instrument that settles ICF. What is corrected is the *reach* of its numbers,
never their validity.

⚠ **Instruments structurally INCAPABLE of settling ICF** (same trap as `/GR`):
match-%/objdiff — a folded callee and a wrong callee score identically — and raw
`memcmp` for duplicate bodies (**silently vacuous**:
PC-relative `bl` displacements differ at different addresses, so identical
functions are *not* identical bytes — this would "prove" ICF by finding nothing).
The instrument that works is relocation-normalized body hashing over
`.pdata`-authoritative extents, split reloc-identical vs shape-identical, against
a random-offset null.
⚠ **The CONCLUSION survives the 2026-08-12 ruler flip but its old REASON does
not** (lane RULER-SWEEP, 2026-08-13). This used to read "`report.rs` masks reloc
args"; the report path has shipped `functionRelocDiffs=name_check` since
`d04c83df`, so relocation **names** are compared now. objdiff still cannot settle
ICF, for a *different* reason: a folded callee resolves to the survivor's single
arbitrary name, so if our source spells the twin, `name_check` charges it exactly
as it charges a genuinely wrong callee. **It conflates "folded" with "wrong"
rather than masking both** — same verdict, new mechanism. Do not cite the masking
reason anywhere; it is stale.

⛔⛔ **AND THAT CONFLATION IS SHIPPED AS A CONFIDENT `AT_LIMIT` VERDICT — DO NOT
BELIEVE IT ON THE `diff_arg`-ONLY STRATUM** (lane MPNGAP-1, 2026-08-13,
`2f5a3cd3`). objdiff's `LINKER_MERGED` detector emits
`ICF: X (cross-function merge)` and the human-readable claim **"no source
mutation can close them"** whenever *target calls A, we call B, A≠B, and both
look like function names* — which is **bit-for-bit the definition of a wrong
callee**. On the 2,890 named rows of the mpn==100/fuzzy<100 stratum it labels
**2,648 `AT_LIMIT`**, and it was measured **wrong on the rows the lane then fixed
by editing source** (+6,304 B, predicted exactly). ⇒ **An `AT_LIMIT` label on a
row whose only penalties are relocation-name args carries no information** — it
is the detector restating its own input. Adjudicate on retail bytes instead
(**does the named callee's signature match the call site?** — MPNGAP-1 killed
`Handle@GemPlayer`, 5,612 B, by observing retail's callee returns `void` where
the site dereferences `r3` and takes no args where the site passes `r4`: our
source was right and the **map** was wrong). This is the same disease as the
`REGISTER_SWAP` label being a symptom rather than a diagnosis — **a tool's
confident "unfixable" is the claim most worth auditing, because it CLOSES veins
and nobody re-opens them.**

⛔ **The "71.5% of `name_check` sites are ICF fold-aliases ⇒ NOISE" model does NOT
survive** — and for a reason *independent* of ICF being real. "Callee absent from
map ⇒ fold-alias" never measured folding; it measured **identification coverage**.
The map names 27,515 of 66,003 functions (**41.7%**), and a null shows **36.8%** of
*all* call sites have a callee absent from the map vs 71.6% in the charged stratum
— an enrichment of only **~1.95×**, used as if it were a deterministic classifier.
Retail-byte adjudication refuted **641 pairs / 2,131 sites** the census called
noise, and only **~28%** of the stratum is explained by a proven fold. The honest
name for that stratum is *"our callee has no identified retail address"* — a
**triage backlog**, not noise (3,776 distinct pairs, top 100 = 46.2% of sites).
⚠ This does **not** by itself reopen the `name_check` default decision; that rests
on the separate finding that name_check *aggregate* code% is build-unstable.

The asymmetry between binaries is **not** optimization level — it's that dc3 had
a leaked PDB/.map giving its functions names+addresses, while RB3's are
anonymous `fn_8XXXXXXX`. dc3 is therefore a **Rosetta Stone** for retail Milo
(same flags, named functions): match RB3 by transferring dc3's labels via
shared string content (`tools/fingerprint_match.py`) or structural similarity
(Ghidra+BinDiff).

## Source provenance (important)

Two sibling repos feed this one. Pick the right source per directory:

- **`src/system/` (Milo engine) ⟵ `../dc3-decomp`.** Dance Central 3 is the
  *same Milo engine*, already 360-ported (uses `RTL_CRITICAL_SECTION`,
  `xdk/XBOXKRNL.h`, etc. — not Wii's `revolution/OS.h`). Its engine headers and
  `.cpp` files compile under the 360 toolchain, so they are the correct base.
  Compiled with the same `/O1 /Oi /GR /EHsc` retail flags as us; the leaked
  PDB/.map (`ham_xbox_r.map`) gives it named functions — that's *our* asymmetric
  advantage, not a cleaner build.
- **`src/band3/`, `src/network/` (RB3 game code) ⟵ `../rb3` (rb3-Wii **DEV**
  decomp).** Important: `../rb3` is the Wii *development* build's decomp, not
  retail. It retains `MILO_ASSERT` source-path strings and named functions that
  the retail Xbox build stripped — a **richer source oracle** for cross-binary
  identification. Wii-targeted (MWCC PowerPC), needs Wii→360 porting.
- **`src/xdk/`, `src/system/stlport/`, toolchain ⟵ `../dc3-decomp`.**

**Caveat (from the project owner):** dc3-decomp is *newer* than RB3. Its engine
code may have subtle behavioral differences or version incompatibilities. When a
file misbehaves, cross-check against rb3-Wii's equivalent and merge intent — do
not assume dc3's version is correct for RB3.

⚠ **`MILO_DEBUG` is force-defined tree-wide (`src/macros.h:3`) and it does NOT
gate `MILO_ASSERT`** — the whole `MILO_*` family is `#ifdef HX_NATIVE`, which the
match build never defines (⚠ corrected 2026-08-13, lane METAMAT-1: cflags carry
**exactly two `/D`s — `/DCURL_STATICLIB` and `/D_XBOX360` — NOT "no `/D` at all"**
as this doc claimed for months; the load-bearing point is unchanged, since
neither is `HX_NATIVE`), so
`MILO_ASSERT(cond,line)` is just `((void)(cond))`. The force-define's only effect
is to switch ON rb3-Wii **dev-build** code that retail compiled out, so every
inherited `#ifdef MILO_DEBUG` is a suspect. **Fix per-site with the house
pattern** `#if defined(MILO_DEBUG) && defined(HX_NATIVE)` (keeps native
behaviour; see `os/Timer.h`, `obj/ObjMacros.h`, `utl/Loader.h`) — ⚠ **never
blanket-remove**: the measured whole-binary control is **−21** (some guards are
genuinely in retail). Details + census:
`docs/decomp/patterns/milo-debug-force-define.md`.

## Decomp priority: the GAME, not the engine

**Spend matching/porting effort on RB3's game layer (`src/band3/`, `src/network/`),
NOT on the Milo engine (`src/system/`).** The engine is effectively pre-solved:
DC3 is the same engine on the same platform (Xbox 360), and we verified that DC3's
already-decompiled engine **loads and renders RB3-360 `.milo_xbox` assets** with
zero rb3-xenon code (same texture tiling / vertex compression / endianness; DC3's
milo loaders keep backward-compat parse branches for RB3's older revisions). A
3-way `rndobj` cross-check shows RB3-360 ≈ DC3 on every divergence point (NgRnd,
BaseMaterial, MetaMaterial, atlas particles, FontMap3d, Matrix4, `rnddx9`); only
**rb3-Wii** is the outlier (its `rndwii`/GX branch). So the renderer, materials,
textures, mesh/skeleton load are all supplied by DC3 — the part that's actually
RB3-specific, and where decomp value concentrates, is the game code.

Full evidence + the asset-render experiment + the "bigger play" (a native target
that injects DC3 rndobj + only RB3 game code) are in
`docs/plans/engine-reuse-and-asset-rendering.md`.

## Git & worktrees (concurrent agents) **important**

### ★ Landing a worktree branch: ALWAYS `git merge --no-ff`

**Effective 2026-08-04, this SUPERSEDES the previous cherry-pick / `git apply` /
`format-patch` / `--ff-only` landing strategies wherever they appear in older
docs.** Those older `docs/plans/*.md` are dated records — do not rewrite them,
but do not follow them either.

```bash
# in the main repo, after the lane's branch is rebased onto main and verified
git merge --no-ff lane-branch-name          # NOT --ff-only, NOT cherry-pick
```

**Why:** the lane's intermediate commits are the point. The failed attempt, the
revert with its reasoning, the "tried X, regressed N units" — that history is
what makes the log worth reading, and it feeds the training-data pipelines. A
cherry-pick or squash throws away everything except the final diff, and
`--ff-only` loses the branch boundary so you can no longer tell where a lane
began and ended. A merge commit preserves both the individual commits **and**
the shape of the work.

This also fixes a real bookkeeping problem noted in
`docs/plans/branch-audit-2026-07-29.md`: landing by patch leaves branches
**permanently "unmerged"** to git, so `git branch --merged` is useless and dead
branches accumulate undetectably.

- **Rebase onto `main` first**, then merge. Merge commits are for preserving
  lane history, not for recording that the lane was stale.
- **Write a real merge-commit message** — what the lane set out to do, what it
  found, and what it deliberately did *not* do. Do not accept the default
  `Merge branch 'x'`.
- The old rules still apply on top of this: stage only your own paths, no
  `Co-Authored-By`, committing is standing-authorized but **pushing is not**.
- Cherry-pick remains legitimate for exactly one case: **salvaging one commit
  out of a lane whose remainder is being abandoned.** Say so in the message.

**Assume other agents are working in the main repo right now.** The main
working tree is shared, so any command that mutates tracked files or the index
out from under them will *deeply break* concurrent work. Hard rules:

- **Never `git stash` in the main repo.** It silently yanks everyone's
  uncommitted changes. To compare a change against `HEAD` or another commit, do
  it in a worktree, not by stashing.
- **Never `git checkout`/`git restore`/`git reset --hard` *files* in the main
  repo** to discard or swap working-tree content. Another agent's in-flight edits
  to that file would be destroyed. (Switching branches is also off-limits in the
  shared tree — use a worktree.)
- **Do your isolated/experimental work in a git worktree.** A bare
  `git worktree add` is *unbuildable* here — the build inputs and toolchain are
  gitignored (`build/`, `orig/*`, `build.ninja`, `objdiff.json`). Use
  **`scripts/setup_worktree.sh [path] [branch]`** to get a buildable + diffable
  worktree in seconds via btrfs CoW reflinks: it reflinks `orig/` and
  `build/45410914/` (a *private* warm-cache build dir — never a symlink into
  main, so the worktree's build can't corrupt the shared one), symlinks the
  read-only toolchain, baks absolute tool paths into the worktree's
  `build.ninja` via `configure.py`, and primes ninja state. A fresh **warm**
  worktree also **seeds main's `.ninja_log`/`.ninja_deps`** so its first full
  `ninja` is a true 0-compile no-op instead of recompiling all ~745 objs. Seeding is gated: it only
  happens when main is clean (no `src/`/`config/` or `configure.py`/
  `tools/project.py` diffs), the worktree's msvc rule blocks are byte-identical
  to main's, and main's deps are uniformly repo-root-relative; if any gate
  fails it auto-skips with a printed reason and falls back to the old
  rm-and-rebuild path. (Portability requires the PCH `/Fp` path to stay
  repo-root-relative — see `tools/project.py`; an absolute `/Fp` would make the
  PCH command worktree-specific and re-trigger the full cascade.) Add
  `--cold-cache` for a guaranteed-clean A/B baseline — seeding is disabled on
  that path, so cold baselines stay honestly cold. Remove with
  `git worktree remove --force <path>`.
- ⛔⛔ **A KILLED `--agent-tools` GRIND RUN LEAVES THE WORKTREE WITHOUT A `.git`,
  AND GIT WILL EVENTUALLY PRUNE IT PERMANENTLY** (measured 2026-08-09, three
  worktrees hit, one lost). decomp-synth's agent-tools mode deliberately MOVES
  `<wt>/.git` to a sidecar `~/tmp/.<name>.gitmeta/gitfile` so the model cannot
  read git history, and restores it on clean exit. Kill the run — Ctrl-C,
  `pkill`, a harness crash — and the move is never undone. The worktree then
  looks like a plain directory: `git -C <wt> status` says *"not a git
  repository"*, and `git worktree list` marks it **`prunable`**. Once anything
  prunes it, `.git/worktrees/<name>` is gone and **`git worktree repair` cannot
  bring it back** — the tree becomes an orphan you can build in but never
  update.
  **Recovery, in this order:**
  ```bash
  cp ~/tmp/.<name>.gitmeta/gitfile <wt>/.git       # restore the pointer
  git -C <mainrepo> worktree repair <wt>           # re-link the admin entry
  git -C <mainrepo> worktree list | grep <name>    # verify: NOT "prunable"
  ```
  If `.git/worktrees/<name>` is already gone, the tree is unrecoverable as a
  worktree — treat it as a scratch build dir or recreate it.
  ⚠ Also: **never run two campaigns against one worktree** — they patch source
  in place and corrupt each other, and the second one's `.git` move races the
  first one's restore.
- **Put worktrees + all scratch under `~/tmp` (= `/home/free/tmp`), NEVER `/tmp`.**
  `/tmp` is a RAM-backed **tmpfs** (47 GB, shared across everything, fills fast —
  we hit "Disk quota exceeded" mid-build this way), *and* tmpfs has no btrfs
  reflink, so `setup_worktree.sh`'s CoW fast-path silently falls back to full
  ~660 MB copies there. `~/tmp` is on the **same btrfs as the repo** → CoW
  reflinks work (cheap, fast) and there's ~300 GB+ free. So
  `scripts/setup_worktree.sh ~/tmp/wt-foo foo`, build logs to
  `~/tmp/rb3_build_{task}.log`, etc. (The harness's own task/transcript files
  already live under `~/tmp` — follow suit for worktrees and logs.)
- ⛔⛔ **A FRESH WORKTREE'S REFLINKED TARGET OBJS ARE *PRE-RENAMER*, SO EVERY
  RETAIL MANGLED NAME READS "ABSENT" UNTIL YOU BUILD** (lane FOLDPROVE-2,
  2026-08-14). `obj_target_symbol_renamer` is a **pre-compile custom build step**
  that rewrites the dtk-split target obj's anonymous `fn_<addr>` symbols to MSVC
  mangled names; a reflinked tree carries the objs but **not the effect of that
  step** until its first build runs.
  ⚠ **The failure is silent and agrees with your prior.** FOLDPROVE-2's first
  cheap-kill run reported a **unanimous 100/100 refuted — exactly the answer it
  was primed to expect** — because every name it looked up was missing. It was
  caught **only** because the symbol count disagreed with main's (**69,438 vs
  69,415**).
  ⇒ **ANY analysis keyed on retail symbol NAMES must build the worktree first**,
  and should assert a symbol-count/known-name sanity check before trusting a
  negative. *A vacuity that confirms your hypothesis is the hardest kind to
  catch* — cf. the `grep`-binary and `all([])` traps elsewhere in this doc.
- The orchestrator MCP manages a pool of these worktrees
  (`scripts/orchestrator/worktree_pool.py`) for its agents; `setup_worktree.sh`
  is the same machinery you can drive by hand.

### ★ Clean up your worktree after you land — `tools/prune_worktrees.py`

Nothing used to remove lane worktrees, so they accumulated to **202 registered
trees / ~272 GB** (lane WT-PRUNE, 2026-08-13). **Remove your worktree once your
branch is landed**, or run the sweeper — dry-run by default, `--execute` to act,
`--protect <path>` for every live lane:

```bash
python3 tools/prune_worktrees.py                       # inventory, removes nothing
python3 tools/prune_worktrees.py --protect ~/tmp/wt-live --execute
```

It keeps any tree with **uncommitted** content, touched in the last 2 h, or
missing its `.git`; re-probes each candidate immediately before removing it; and
archives a manifest (+ `git diff` and untracked tar for anything it removes that
had a diff) under `~/tmp/worktree-prune-archive/`. It **never deletes a branch.**

Two counter-intuitive facts it is built on — both verified, not assumed:

- ⛔ **`git worktree remove` does NOT delete the branch.** The branch ref, the
  tip commit and its file contents all still resolve afterwards. So **unlanded
  commits are not a reason to keep a directory** — the only irrecoverable loss
  is *uncommitted* work: dirty tracked files, and untracked-non-ignored files.
  ⚠ Untracked files are frequently a lane's **entire** deliverable, and
  `remove --force` deletes them without a word — "no tracked modifications" is
  NOT "clean". 28 trees were kept for exactly this.
- ⛔ **`git branch --merged` is a useless signal here** and is deliberately not
  a criterion: lanes land by patch, so a fully-landed branch still reads as
  unmerged. Patch-id equivalence (`git cherry main <branch>`) is reported as
  information only.

⚠ **Two liveness traps, both measured — do not reinvent this check.** The mtime
of git's admin **directory** (`.git/worktrees/<name>/`) is bumped for *every*
worktree at once by ordinary git housekeeping, and the admin **`index`** file is
refreshed by anyone who merely *looks* at the tree. Reading either made all 209
trees report "modified 0.1 h ago" — a liveness gate that **cannot fail**, which
is worse than no gate. Only tree content, `HEAD` and `logs/HEAD` are honest.

## Two build tracks

**1. X360 decomp-matching build** — compile-to-match the retail XEX (MSVC X360).

```bash
./tools/ninja-locked 2>&1 | tee ~/tmp/rb3_build_{task}.log
python3 configure.py     # regenerate build.ninja (after editing objects.json/splits.txt)
```

**ALWAYS `tee` the build output to a log file** (`~/tmp/rb3_build_{task}.log` or
similar — use `~/tmp`, not the RAM-backed `/tmp`; see the worktree note above).
Makes debugging easier.

dtk is the local **jeff** fork at `../jeff`; **objdiff is also a local fork**
at `../objdiff` (freeqaz/objdiff, with custom pattern-detector work and
normalized-diff changes). `configure.py` resolves BOTH to their **prebuilt
release binaries** (`<fork>/target/release/{dtk,objdiff-cli}`, absolute paths)
— there are no cargo build edges in build.ninja anymore. That absolute-path
parity (main and worktrees bake the identical command strings) is what enables
warm-worktree command-hash reuse.

The compiler wrapper is the **freeqaz/wibo fork** release binary at
`/home/free/code/milohax/wibo/build/release/wibo` (configure.py resolves it by
default; the old `build/tools/wibo` download edge is gone — it used to fetch
stock upstream wibo and silently disable the FS cache). configure.py
**hard-fails** on a wrapper binary lacking the fork's `WIBO_FS_CACHE` /
`WIBO_REWRITE_SHOWINCLUDES` feature bytes — intentional (a stock binary would
silently corrupt `deps = msvc` dependency tracking), not a bug.
`tools/transform_dep.py` is no longer in the msvc rule (wibo rewrites
`/showIncludes` output in-process).

**wibo staging discipline (important).** The live `build/release/wibo` is invoked
by *every* MSVC compile in main AND all worktrees, and now includes the
residual-perf merge (readlink storm fix, scoped negative-exists cache, in-process
stats reporter, `current_path()` memo). Because a stock/regressed wibo silently
corrupts obj bytes fleet-wide, **never overwrite `build/release/wibo` directly**:
rebuild to `build/staging/wibo`, run the byte gate (compile ≥3 TUs with the staged
vs live binary → 0 differing bytes beyond the COFF timestamp), then atomically
swap (`mv release/wibo <backup>; mv staging/wibo release/wibo`). The binary is not
a ninja input, so the swap triggers no recompiles — which is exactly why a bad one
goes unnoticed until objs are wrong. Rollback = `mv <backup> release/wibo`.

**Live binary as of 2026-08-05: `wibo 1.2.0-c2rs.1`** (freeqaz/wibo tag
`1.2.0-c2rs.1`, built from wibo's `build/release64-clang/` — CI's
`release64-clang` preset, LTO on). Rollback binary is
`build/release/wibo.bak-20260805-pre-c2rs.1` (`1.2.0-27-geab90f0`). The byte gate
was run at **60 TUs, not the minimum 3**, lifted straight out of this tree's own
`build.ninja` with the objcache prefix stripped so both sides really compiled:
60/60 byte-identical with the COFF timestamp zeroed. Two traps if you re-run it:
both loaders must write to the **same** `/Fo` path (the obj embeds its own `/Fo`
spelling, so two scratch dirs differ for that reason alone), and a `/Fo(\S+)`
substitution will also match inside source paths like `rndobj/Font3d.cpp`.

⚠️ **objcache does not key on the wibo binary** (its key is compiler-DLL identity
+ cflags + source + dep hashes). A wibo swap therefore invalidates *nothing* and
the cache will keep serving objs produced by the previous loader. That is safe
only because the byte gate above says the two loaders agree — it is not something
the cache checks for you. If a future swap ever fails the byte gate, the cache
must be dropped as well as the binary rolled back.

**objcache — shared content-addressed MSVC object cache.** Sibling Rust repo at
`/home/free/code/milohax/objcache` (rebuilt manually like jeff/objdiff:
`cargo build --release`). Every `msvc`/`msvc_pch`/`msvc_pch_create` rule is
prefixed with `objcache exec --fo $out -- <wibo> cl.exe …` (resolved to the same
absolute path in main and every worktree via `configure.py`'s `_find_local_fork`,
so command strings stay byte-identical → warm-worktree hits). configure.py
**hard-fails** if the binary is missing (`RB3_OBJCACHE_OPTIONAL=1` opts out to a
plain uncached-but-correct compile). Cache lives at `~/.cache/rb3-objcache`
(config, manifests, objects, stats).
- **What it buys:** a cold `rm -rf build/45410914/src && ./tools/ninja-locked` is
  now all-cache-hits — measured **3.5 s / 778 hits / 5 misses** vs a full ~5-min
  recompile. Cold A/B baselines and fresh worktrees are near-free.
- **Kill switch (no re-log):** `objcache off` (or `OBJCACHE=off` env) → the prefix
  stays in the rule but the binary passes straight through to the real compiler.
  `objcache on` re-enables. `objcache stats [--reset]`, `objcache gc --max-size 10G`.
- **Correctness model:** key = compiler-DLL identity + full cflags + source +
  validated dep-closure hashes (+ PCH identity for PCH TUs); any anomaly (e.g. a
  missing dep file) → passthrough, never a stale serve. Served objs are
  byte-identical to a real compile except (a) the 4-byte COFF timestamp (zeroed on
  hits) and (b) for cross-root hits, the single embedded `/Fo` path string — both
  match-irrelevant
  ⛔ **"match-irrelevant" is TRUE FOR THE METRIC AND BADLY MISLEADING FOR BYTE
  COMPARISON — raw `.obj` byte comparison is a DEAD INSTRUMENT here.** A
  cache-served obj embeds the `/Fo` path of whichever worktree **populated** the
  entry, and **four characters of path difference shift every subsequent file
  offset, turning one string into 96,681 differing bytes.** Measured 2026-08-01: a
  warm worktree sampled **0 of 40** objs carrying its own root (four distinct
  foreign roots); repo main 56/60 own + 4 foreign. A same-source null produced
  **951 differing objs — more than the treatment it was meant to validate.**
  Separately, the **PCH consistency signature** (4 bytes at `0x010980`, refreshed
  on every PCH rebuild) makes **379 objs differ** — all inside PCH-eligible dirs,
  essentially every obj in them, zero outside — even with `OBJCACHE=off`.
  ⚠⚠ The obvious determinism control — revert and rebuild **with the cache on** —
  is **VACUOUS**: it re-serves the identical cached bytes, so the natural way to
  validate an obj-byte comparator *confirms* it.
  ⇒ An obj-byte comparison is meaningful **only** with `OBJCACHE=off` on BOTH legs
  **and** the PCH-dir residue subtracted. Otherwise use `matched_functions` /
  `fuzzy_match_percent` or a relocation-normalized body hash.
  ★ **That construction now EXISTS as a tool — use it, don't rebuild it:**
  `tools/gate_liveness.py` compiles a TU twice (flag on/off) with `OBJCACHE=off`
  and **the same `/Fo` on both legs**, so every hazard above is neutralised *by
  construction*, and reports **which owned symbols changed**. It answers "is this
  `/D` gate live in this TU?" **non-metrically** (lane DK-3, `0d6933b5`;
  `docs/decomp/patterns/gate-liveness-probe.md`). ⚠ Its own lesson: **raw `.text`
  identity is TOO BLUNT** — it read LIVE for four TUs whose only change sat in a
  *shared template COMDAT* the linker resolves arbitrarily. **The signal is
  changed words in TU-OWNED symbols.** (whole-binary `matched_functions` holds equal through all-hits
  rebuilds). objcache also normalizes recorded deps to **repo-root-relative** (no
  absolute src paths in `ninja -t deps` → enables warm-worktree `.ninja_deps` seeding).
- **Resolved gotcha (7956af7):** `<memory.h>` used to have no real match on the
  include path, so wibo's case-insensitive resolve fell through to the game's
  `src/Memory.h` and recorded a lowercase dep that doesn't exist on Linux —
  leaving 5 soundtouch TUs perpetually ninja-dirty. Fixed by adding the CRT
  compat header `src/xdk/LIBCMT/memory.h` (defers to `string.h`); LIBCMT
  precedes `src` in the include order so it wins with exact casing. Lesson: a
  header include that only resolves case-insensitively will churn forever —
  give it a real exact-case target on the include path.

> **Editing the jeff/objdiff/wibo/objcache sources? Rebuild the release binary
> manually** — ninja no longer builds or tracks the tools:
> `cargo build --release` in `../jeff`, `../objdiff`, or `../objcache`;
> **`cmake --preset release64 && cmake --build --preset release64`** in `../wibo`.
>
> ⛔⛔ **`cargo build --release` in `../jeff` OVERWRITES THE LIVE FLEET BINARY.**
> Cargo's output path *is* the deployed path (`jeff/target/release/dtk`), which
> `build.ninja` resolves in rb3-xenon **and** dc3-decomp. So the rebuild command
> written directly above is also an unannounced fleet deployment — it swaps the
> splitter out from under every concurrently-running lane, silently changing
> split output mid-A/B. This is the same hazard the **wibo** section documents
> staging discipline for; jeff had none, and a lane hit it on 2026-08-04.
> **Build somewhere else and swap deliberately:**
> ```bash
> CARGO_TARGET_DIR=~/tmp/jeff-build cargo build --release   # never touches live
> cp jeff/target/release/dtk ~/tmp/dtk-backup/dtk.<version>-<sha8>   # back up FIRST
> cp ~/tmp/jeff-build/release/dtk jeff/target/release/dtk           # then swap
> ```
> ★ **Verify a restore by BYTES, not by behaviour.** The same lane "restored"
> the live binary by rebuilding the same commit and checking split output was
> unchanged — but a rebuild is functionally identical and **byte-different**, so
> the deployed binary silently stopped matching the published release asset.
> `sha256sum` it against `~/tmp/dtk-backup/` or the GitHub asset.
> ⚠ And bump `Cargo.toml`'s version whenever split output changes: a version
> constant across materially different binaries is not a weak identifier, it is
> **no identifier**, which is what v1.9.3 existed to fix.
> None of these binaries is an implicit ninja input, so a tool rebuild
> does not retrigger compiles (wibo is byte-neutral to objs — verified fork
> vs stock objs = 0 differing bytes; objcache serves timestamp-only-different objs).
>
> ★ **`release64`, NOT `release` — this doc said `release` for five months and it
> is the wrong architecture.** `CMakePresets.json`'s `release` preset uses the
> `i686-linux-gcc.cmake` toolchain (32-bit), but the deployed
> `build/release/wibo` is `ELF 64-bit x86-64`; `release64` uses
> `x86_64-linux-gcc.cmake`. Building `release` produces an i686 binary that has
> been broken since February — and chasing that breakage cost two lanes real
> budget on 2026-07-30, one of which mis-attributed it to a clang-22 upgrade.
> ⚠ Note the confusing layout: the *output directory* is `build/release/`
> regardless of which preset you used, so the path does not tell you the arch —
> check with `file build/release/wibo`.

**2. Native engine build** (`native/`, x86_64 Linux + clang) — runs the engine
on the host. Currently boots headlessly and loads RB3 `songs.dta`.

```bash
cd native && cmake -S . -B build -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build build && ./build/rb3-dta <songs.dta>
```

Full native-port build recipe, hard-won lessons, and link-stub regeneration are
in Claude's memory: `project_native_port.md`. Highlights: needs dc3's dual-target
`types.h` (LP64 int-width vs Xbox ILP32 long); `Symbol::Init()` mandatory before
interning; rb3-xenon's RB3 engine is more complete than DC3's so several dc3
`_Native` shims are redundant; ~74 rendering/MIDI/synth/Win32 symbols off the DTA
path are satisfied by weak stubs in `native/src/dta_link_stubs.s`.

**Architectural goal — DONE as of X1 (2026-08-01).** The shared native Milo
runtime lives in `../milo-native-engine` and is consumed here via
`add_subdirectory` with a soft SHA pin (`MILO_ENGINE_PIN` in
`native/CMakeLists.txt`, currently `138e1606`). rb3-xenon, rb3 (Wii) and
dc3-decomp all consume it; xenon uses `MILO_ENGINE_GPU_BACKEND=dc3` because its
`rndobj/` is DC3-shaped. **Only the coordinator bumps the pin; lanes file engine
change requests as text.** As of X4d the native build loads and renders real
venue roots with their own shipped lighting, and drives characters from real
`CharClip`s — it is no longer a headless DTA reader. Ladder and per-milestone
docs: `docs/plans/x{1,2,3}-*.md`, `docs/plans/x4{a,b,c,d}-*.md`.

### ⚠ Never put render/gate flags in a shell variable — zsh will not word-split it

**Three consecutive native lanes (X4b, X4c, X5) hit this, and it does not
error — it runs something else and returns `rc=0`.** X4c produced two wrong
measurements from it; X5's camera sweep silently rendered the *default* cells
instead of the venue and passed.

```zsh
FLAGS="--frames 1 --clip crowd_reaching_01"
./build/rb3-render $ASSETS out $FLAGS       # ⛔ ONE argv entry: "--frames 1 --clip …"
```

zsh (unlike bash) does **not** word-split unquoted parameter expansions. The
whole string arrives as a single argument, the parser rejects or ignores it,
and the driver falls back to defaults — so you get a green run and a plausible
PNG of the wrong thing. Write the flags out in full at every call site, or use
an array (`flags=(--frames 1 …); cmd $flags`).

**The general shape:** a harness whose failure mode is "renders something else
and passes" is worse than one that crashes. When a render or gate result looks
right, confirm it rendered what you asked for — cell name in the log, not just
`rc=0`.

### ✅ `ab_measure` now hands the worktree back exactly as it found it — FIXED 2026-08-13

**This section used to say "`--revert` leaves the reverted patch in the
worktree" and framed it as a `--revert` quirk. It was ONE CELL of a much
broader defect, and the fix (lane TOOL-AB) covers all of them.** Measured by
executing the real tool over 12 mode × exit-path cells: **8 left the tree
different from how the run found it**, and three of those **silently deleted
the lane's own uncommitted work**:

| mode | success | success `--restore` | refusal before apply | refusal after apply | Ctrl-C |
|---|---|---|---|---|---|
| `--patch` | left **PATCHED** | restored | clean | left patched | left patched |
| `--pick` | left **PICKED** | restored | clean | left applied | left applied |
| `--revert` | left **REVERTED** (X11, EE2-A) | restored | clean | left reverted | left reverted |
| `--from-dirty` | preserved | ⛔ **WORK DELETED** (EE2-B) | ⛔ **WORK DELETED** (DTOR-A) | preserved | ⛔ **WORK DELETED** |

⚠ Note the correction to the folklore: plain **`--patch` left the tree PATCHED,
not unpatched**. The "green run, worktree held main's code with every fix
stripped" report is the **`--from-dirty --restore`** cell — `--restore` was
defined against the PATCH, and under `--from-dirty` the patch *is* your work.

**Now:** the tree is restored on **every** exit path (success, refusal, Ctrl-C,
unhandled exception), verified by re-reading the diff rather than by assuming
the git commands worked, and the pre-run state is snapshotted to the run dir
*before the first mutation* (the only recovery a SIGKILL can have). `--restore`
is a deprecated no-op; **`--keep-applied`** opts out and prints a banner naming
every file left modified. A restore *failure* is a loud banner, never a
refusal — cleanup must not void a verdict already measured (lane CK-2's rule).

Two adjacent defects fixed with it, both of the same disease — *the tool
misrepresenting what it measured, silently*:

- **Repeating `--patch`/`--pick`/`--revert` is now REFUSED.** argparse's
  mutually-exclusive group fires only across *different* options, so repeating
  the *same* one silently kept the **LAST** value: lane EE2-C passed three
  `--revert` flags and got a confident number for **one** of them. Combine
  them into a single patch and measure that.
- **A staged index is now REFUSED under `--from-dirty`.** `git diff` does not
  see staged hunks but `git checkout --` restores *from the index*, so a
  half-staged file measured only the unstaged hunks **against a leg A silently
  carrying the staged ones** — absent-vs-absent for half the change, rc=0,
  clean verdict.

The general lesson stands and generalises past this tool: when an artifact
matches a baseline you did not expect it to match, **`cmp` against every
candidate, not just the one you assume you're comparing to** — that is what
caught it. Same family as the "compare artifacts, not transcribed hashes" rule
below.

### ⚠ Run the native gate before landing shared-`src/` changes

**`tools/native_build_gate.sh` (expect `PASS 18/18, rc=0`).** This has now
caught `main` broken by a matching lane **four separate times** (X4a, X4d ×2,
MILOKEEP-1), each time costing the native lane a repair it did not own.

**Read the exit code (2026-08-17, task #90 — it now says four different things):**

| rc | meaning |
|---|---|
| 0 | full coverage, and all of it builds |
| 1 | **the native build is BROKEN** (or cmake configure failed) |
| 2 | the gate could not run at all (bad option, no such dir, no `native/`, its own ninja probe returned nothing) |
| 3 | **the gate ran and does NOT vouch for full coverage** — targets SKIPPED (INCOMPLETE) or a subset requested (PARTIAL). *Not broken — not tested.* |

rc=3 was previously rc=0, i.e. an incomplete run was indistinguishable from a
full pass to anything reading `rc`. `--strict` now means "promote incomplete to
a hard FAIL (rc=1)", and it finally covers `NATIVE_GATE_ONLY` subsets too — that
combination used to exit 0. `NATIVE_GATE_ALLOW_INCOMPLETE=1` forces an
INCOMPLETE run back to rc=0 **and says so on the verdict line**; it is for
environments that structurally cannot host the engine (CI, another machine, the
frozen tree copies in `decomp-bench/` and `decomp-synth/out/`), never a default,
and it does **not** apply to PARTIAL.

✅ **The gate was AUDITED 2026-08-14 (lane GATEGAP-1) and it *discriminates*: on
a fully seeded tree it FAILs a broken build and PASSes a healthy one.** Three
lanes reported `PASS 18/18, 0 SKIPs` around the window MILOKEEP-1 found `main`
broken, which looked like the gate failing at its one job. Reproduced in a
`~/tmp` worktree with the seeded flags, **same worktree, same cache, one
variable**:

| tree | verdict |
|---|---|
| `b81c03b8` (BODYPORT-4) | `PASS 18/18, 0 SKIPs, rc=0` |
| `0dfc1ec3` (BODYPORT-5) | `FAIL 16/18, rc=1`, `NOBINARY rb3-milo`/`rb3-render` |

⇒ the instrument **discriminates** (the PASS leg is the control — a gate that
FAILs on everything proves nothing). **BODYPORT-3/4 predate the breakage
entirely, so their PASS is worth exactly what it says.**

⚠ **That audit covered what the gate PRINTS, not what it RETURNS**, and only on
a *seeded* tree. On an unseeded one it printed
`PASS (INCOMPLETE: 15/18 verified, 3 SKIPPED)` and **exited 0** — indistinguishable
from a full pass to anything checking `rc`, which is why `0 SKIPs` was a rule a
human had to apply by hand, and why it is the rule and not the exit code that
caught every false green so far (X21, MATCH-A). **Fixed 2026-08-17 (task #90):
an unseeded worktree now gets 18/18 because `native/CMakeLists.txt` resolves its
siblings from the real repo, and an incomplete run returns rc=3.** Keep applying
the 0-SKIP rule anyway — it is the rule with the track record.

⛔⛔ **A COMMENT-ONLY COMMIT BROKE THE NATIVE LINK.** Configure-only bisect
(the prune decision is made at configure time, so no build is needed) pinned it
to `6c087cbd` — a `docs(src)` commit adding the prose
`// #ifdef HX_NATIVE and the match build never defines it.` to
`rndobj/TexRenderer.cpp`. `ScatterIncludes.cmake` matched `#if` **anywhere on a
line**, so that comment pushed an unmatched `HX_NATIVE` frame; the
`#include "math/mtx.cpp"` 212 lines below was reclassified from UNCONDITIONAL to
conditional-and-HX-guarded — **the one bucket the module ignores in SILENCE**
(not pruned, not warned) — and `mtx.cpp` was then compiled standalone *and*
emitted from TexRenderer's TU ⇒ 17 duplicate definitions. Fixed two ways
(line-anchored directives + `/* */` stripping, **and** an `#if`/`#endif`
balance check at EOF that fires for any future desync cause). ⇒ **"it's only a
comment, no need to re-gate" is NOT safe**, and the lane's gate run must be its
**last** action, not its second-to-last.

📋 **Paste the gate's own summary line into your write-up — do not paraphrase
the verdict.** The last line of every run, on every exit path, is

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

`verdict ∈ {PASS, INCOMPLETE, PARTIAL, FAIL, UNRUNNABLE}`. This is the only
machine-readable surface the gate has, and it exists because the prose verdict
is easy to relay wrongly: the full-pass line is `PASS  (rc=0, …` and the
incomplete one is `PASS (INCOMPLETE: …`, one space apart (lane X21).

⚠ **The verdict now self-labels an incomplete run**:
`PASS (INCOMPLETE: 15/18 verified, 3 SKIPPED) -- NOT full coverage`, listing the
untested targets and printing the seed command. The old wording put `PASS`
first and `3 skipped` in a parenthetical, which is trivially relayed upstream as
"PASS" (lane X21). ★ A link diagnostic contains **no `error:` token** (34
`multiple definition` lines, 0 matches), so linker errors are now counted and
shown separately, and a **link** edge's failing target is finally named — that
attribution matched only compile edges, so a pure link failure printed
`across target(s): ` blank.

Why it happens: the X360 match build **compiles** `src/`, but the native targets
**link** a superset of it. A change that matches perfectly can still leave an
undefined symbol, an uninstantiated template, or a missing operator that only a
linker sees — `ObjOwnerPtr<>`'s save operator and `RndEnvAnim::Save` were exactly
this. The matching build is structurally incapable of catching that class.

So: if your change touches `src/system/**`, `src/band3/**` or any shared header,
run the gate before you land.

✅ **You no longer need to seed a worktree (2026-08-17, task #90).**
`native/CMakeLists.txt` resolves `MILO_ENGINE_PATH` and `Dawn_DIR` from the
**real** repository — sibling of the repo found via `git rev-parse
--git-common-dir`, then sibling of the source tree, then `$HOME/code/milohax`,
each candidate confirmed by a witness file — so a cold worktree with **no
seeding of any kind** measured `PASS 18/18, 0 SKIPs, rc=0`. The configure log
names the rule that answered (`-- [sibling] milo-native-engine -> … (rule
1:real-repo-sibling …)`); check it if you land in INCOMPLETE. Ported from
`60837907`, same bug class as the DC3-map path. **The rest of this bullet is
the fallback**, for a machine where the deps really are elsewhere — and it is
still the record of the trap, so read it before hand-seeding anything.

- **If you do seed, PIN THE COMPILERS.** The gate's own
  `cmake` line omits `-DMILO_ENGINE_PATH=` and `-DDawn_DIR=`, and without them
  three targets silently **SKIP** while the gate still reports `PASS`. It
  *also* sets the compiler (`native_build_gate.sh`) — and **a seed configure
  that does not pin the compiler is worse than not seeding at all**: the cache
  then holds the resolved defaults `/usr/bin/cc` + `/usr/bin/c++`, the gate's
  `-DCMAKE_C_COMPILER=clang` differs, and CMake answers *"You have changed
  variables that require your cache to be deleted"* — silently wiping your two
  path flags and re-deriving them from the relative defaults.
  **Re-measured 2026-08-17 (task #90, CMake 4.4.1), cold worktree, both legs:**

  | seed | gate's reconfigure over it | `cache to be deleted` | paths after | targets in graph |
  |---|---|---|---|---|
  | no compiler flags | `-DCMAKE_C_COMPILER=clang` | **1** | wiped → `<wt>/native/../../…` | **15** |
  | `-DCMAKE_C_COMPILER=/usr/bin/clang` | `-DCMAKE_C_COMPILER=clang` | **0** | intact | **18** |

  ★ So the wipe is real but **the *spelling* of the pinned compiler is not
  load-bearing** — this corrects X21 (`x21-compose-path-2026-08-03.md:399-411`)
  and the earlier wording here, both of which blamed `/usr/bin/clang` vs bare
  `clang`. CMake 4.4.1 resolves the literal before comparing. Pinning *at all*
  is what matters. Likewise `-Dglfw3_DIR=` and `-DRB3X_BUILD_ENGINE=ON` are
  **not** required: measured, `find_package(glfw3)` writes `glfw3_DIR` and
  `option()` writes `RB3X_BUILD_ENGINE:BOOL=ON` into the cache by themselves,
  which is why every seeded worktree cache carries them. Use the gate's own
  spelling anyway — bare `clang` is guaranteed identical to what the gate
  passes, so it cannot be the thing that differs.
  X21 hit the wipe after X18 documented the recipe — its first baseline
  read 15/18 with 3 SKIPs, and **the 0-SKIP rule is what caught it**. Always
  require `0 SKIPs`, never just `PASS`.
  ★ **WHY it used to fire in every `~/tmp` worktree (measured 2026-08-09, lane
  MATCH-A; FIXED 2026-08-17, task #90):** the build resolved its siblings
  **relative to the project dir** — `native/../../milo-native-engine` and
  `native/../../dc3-decomp-deps/dawn/lib/cmake/Dawn`. From `~/tmp/wt-foo` those
  resolve under `~/tmp/`, which does not exist, so `rb3-milo`, `rb3-render` and
  `rb3-frame` SKIPped and the gate **still printed `PASS`** at rc=0. Since every
  lane works in `~/tmp` (house rule), *the default gate run in a worktree was
  structurally incapable of testing the three engine targets* — the three its
  own comment calls most likely to break. Now resolved from the real repo; the
  explicit seed below remains valid and still wins (an existing cache entry is
  never overwritten):
  ```bash
  cd <worktree>/native && rm -rf build && cmake -S . -B build -G Ninja \
    -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ \
    -DMILO_ENGINE_PATH="$HOME/code/milohax/milo-native-engine" \
    -DDawn_DIR="$HOME/code/milohax/dc3-decomp-deps/dawn/lib/cmake/Dawn"
  ```
  ⚠ Same bug class as `tools/pin_from_symnames.py`'s DC3-map path (fixed in
  `60837907`): **a sibling-relative path silently vanishes in a worktree, and
  the failure is shaped like a legitimate "not applicable" rather than an
  error.**

## Build wiring

⛔⛔ **NEVER `ninja <one>.obj`, and never `objdiff-cli --build` — they are the
same command, and both leave the tree measurably WRONG for everyone else.**
The six post-compile patchers hang off the `post-compile` phony, *downstream*
of the compile edges. Ninja builds only a target's **ancestors**, so naming a
single `.obj` stops one edge short of every patcher and the fresh compile
overwrites the previously-patched bytes. `objdiff-cli diff --build` (without
`--full-build`) **is** `ninja <base_obj_path>`.

Measured 2026-08-21 (rb3-xenon, worktree off `0f7f213b`) — one `touch`, one
targeted `.obj` build, nothing else:

| ruler | patched | unpatched |
|---|---|---|
| unit `default/BandUI` `matched_code_percent` | 93.299904 | **91.293884** (−2.006 pp) |
| unit `default/BandUI` `matched_code` | 18,604 | 18,204 (−400 B) |
| `?InitPanels@BandUI@@QAAXXZ` (400 B) | **100.0** | **99.7** |
| whole-build `matched_code_percent` | 36.738945 | 36.735040 |

⇒ a function that IS matched reads as a near-miss, and byte-exact is the
admission gate. The bias is **one-directional** (an unpatched object can only
lose points) and **invisible on the tool that causes it** — `run_objdiff`
reported the same verdict in both states, so `report.json` and
`measure_progress.sh` silently disagreed with it and neither looked wrong.
⚠ And because the patchers **preserve mtime** (they must — see any patcher's
`_write_preserving_mtime`), the degraded state shows up in **no timestamp**.
⚠ Do NOT dismiss this as build noise: a full build restored the object's exact
prior sha256 and `report.json`'s measures byte-identically, **four times**
across this lane's sabotage cycles. Two clean builds here do not differ at all.

**Instruments** (landed 2026-08-21; dc3 `2f35703d0` and decomp-synth
`bacceb083` are the siblings):

- `scripts/verify_objs_patched.py` — `--check` re-runs all six passes dry and
  fails the build if the tree is not a fixed point; `--emit` writes
  `build/45410914/patch_state.json`, a content manifest of every decomp **and
  target** object; `--verify-manifest` recomputes it with no toolchain. Wired
  as the last `post-compile` edge, so every completed build leaves a reference
  state. **Ask this before trusting a tree you did not build.**
- `scripts/orchestrator/patch_guard.py` — `ensure_patched_tree()`: builds
  `post-compile` through `objdiff.json`'s `custom_make` (`tools/ninja-locked`),
  then **asserts**, raising `UnpatchedTreeError` rather than returning a
  number. ~0.81 s on a consistent tree. Every tool that builds-and-scores now
  goes through it.

⚠ **Two honest limits on the green light, both measured, neither fixed:**
1. **Three of the six passes are idle** — `guard`, `bool_mangle` and
   `atexit_scope` patch **0 files repo-wide** in APPLY mode on a fully built
   tree. A green `--check` is earned by three passes, not six. (This is why
   `patch_guard` asserts the *manifest*, which is content-keyed and does not
   depend on any pass still being active.)
2. **Those same three pair target↔base by RELPATH**, reaching only **347 of
   the 1,048** pairs `objdiff.json` declares — **701 (66.9%) invisible**, and
   **3** of the 347 paired against a *different* target obj than objdiff.json
   names. `obj_anon_ns_patcher.py` already solved this with `--objdiff-config`.
   `--check` prints this coverage and `--emit` records it, so the denominator
   is visible rather than hidden behind an exit code. **Closing it is a
   separate lane: it would change matched bytes.**

- `tools/defines_common.py` — include paths. **STLport must come first**, then
  `src/xdk/LIBCMT` (C CRT), then `src`, `src/system`.
- `config/45410914/objects.json` — declares which `.cpp` files to compile and
  their match status. New files: add here as `NonMatching`.
- `config/45410914/splits.txt` — pins per-object section ranges so dtk emits
  per-unit target `.obj` + `.s` for objdiff to compare against. Pin **just
  `.text`** for a new cluster; on next `ninja` (after `touch config.yml`) dtk
  auto-derives and back-fills the matching `.pdata` range. Other sections
  (`.rdata`, `.data`) need manual pinning if the TU has them.
  ⛔⛔ **BARE vs NESTED headings (707 bare / 569 nested) have now broken FOUR
  consecutive lanes' scans — key on FULL PATH, never `basename()`, and replicate
  `tools/project.py`'s own `objects()` + basename-alias step rather than
  reconstructing paths yourself.** The failures were not subtle in size and were
  invisible without a control: MISPIN-1's scan silently failed nested lookups and
  reported **10,563** mis-pins (true: **149**); PINFIX-1's fix made the
  mirror-image error and **misfiled 4,940 rows**, then the same `basename()` slip
  corrupted its own result attribution; NOOBJ-1's first classifier read
  **703 of 1,045 healthy units as "no source"**. ⚠ `Movie.obj` genuinely
  **collides** between `rnddx9/` and `rndobj/`, and a basename match will also
  produce false "pinned but unwired" hits for files like `rnddx9/Cam.cpp` that
  are `#include`d rather than headings. ★ **Self-validate any such census by
  reproducing the known figures — 22,384 healthy / 4,515 no-obj / 93 wrong-unit /
  149 mis-pin, and `total_functions` / `total_code` as exact row and byte sums.**
  Three lanes did this and each caught a defect the raw run reported cleanly.
  ★`.pdata` lines are **derived output, not input**: every split run clears the
  entire `.pdata` split set and re-derives one range per `.text` block (jeff
  `split.rs:1035` `split_pdata`), then rewrites splits.txt. Measured: 54
  deleted `.pdata` lines regenerated byte-identical; a hand-made `.pdata`
  overlap silently healed. So move/edit only `.text` — `.pdata` follows
  automatically. If `.pdata` lines *don't* regenerate, the split run itself
  failed (check the log — e.g. symbols.txt-drift "ends within symbol"); the
  derivation never selectively skips.
  ★A move that drains a unit's **last `.text` block** must delete the unit's
  whole splits.txt entry in the same edit: an empty unit still emits a 42-byte
  `obj/<unit>.obj` — the split succeeds, then `report.json` hard-fails with
  `Failed to open <unit>.obj: Invalid COFF/PE section headers` (verified; with
  the entry removed the full build passes). ⇒ a **single-function unit can never
  be completed by a boundary move** — it VANISHES instead of reaching 100% (lane
  DG-2 `1cbcabc8`: 3 of 23 candidates, a case the candidate filter never tested).
  ★★★ **PIN NEUTRALITY IS SCOPED TO *REATTRIBUTION*, AND THIS DOC CARRIED
  NEITHER HALF OF IT** (recorded 2026-08-14; the scoping lives in
  `docs/decomp/pin-neutrality-scoped-2026-08-14.md`, which corrects
  `PIN_WAVES_AND_DENOMINATOR_2026-08-09.md`):
  - **ADDING a pin over previously-unpinned (`auto_*`) code is metric-neutral —
    Δ exactly 0 on the matching keys**, because `auto_*` units are **already in
    the denominator**; a pin only *reattributes* bytes anonymous→named. Such
    waves need **no A/B**.
  - ⛔ **RE-HOMING an already-pinned address is NOT neutral** — measured **+3
    functions / +428 B** (lane PINHOME-1, `8e6eb9be`). **objdiff pairs by NAME,
    so a target row whose base obj cannot define that name reads 0% however
    correct our code is**; re-homing changes *which base obj is consulted*, and
    therefore changes pairability. Reattribution never does.
  ⚠ **But the re-homable vein is 13× smaller than its raw figure**: of 267
  orphan-pin rows / 38,096 B, only **59 rows / 2,972 B** are re-homable — the
  rest is absent source or a wrong map name, which no pin move can help.
  ⚠ And ⛔ **`total_code` is NOT guaranteed neutral even for additions**: at
  scale a pin wave measured **−5,120 B / −1 function**, because pinning **evicted
  a PHANTOM `type:label` row whose extent DOUBLE-COUNTED 33 real functions.**
  That is a *correction*, not a regression.
- `tools/project.py` — patched so objects in `objects.json` get a compile edge
  even without a `splits.txt` address range (compile-only scaffolding).
- `tools/fingerprint_match.py` — function-identification tool (extract / report
  / autoid / identify subcommands). Indexes all 66,838 RB3 functions by
  referenced strings/callees/constants; cross-refs strings against
  `../rb3/src` + `../dc3-decomp/src` to propose source-file mappings. Generates
  `fingerprints.json` + `autoid.json` (gitignored, regenerable). See
  `project_function_identification.md` in Claude's memory.
- `src/` include style mirrors rb3-Wii: `#include "math/Vec.h"` resolves via
  `/I src/system`. **Beware include shadowing:** `/I src` precedes
  `/I src/system`, so a file at `src/os/Foo.h` will shadow `src/system/os/Foo.h`.
  (A stub `src/os/Debug.h` once shadowed the real engine `Debug.h` and broke
  every macro-using header — don't reintroduce stubs at `src/` root.)
- **PCH (precompiled header, dc3 port).** 9 engine dirs (`hamobj synth flow
  gesture meta obj os utl movie`, ~281 TUs under `src/system`) compile through a
  `/Yc//Yu` PCH at `build/45410914/pch/system.pch`, built from
  `src/system/decomp_pch.h` (= only `obj/Object.h` + `os/Debug.h`). Config lives in
  `configure.py` (`config.pch_header/pch_source/pch_eligible_dirs`); the
  `msvc_pch_create`//`msvc_pch` rules + PCH edge + per-object eligibility switch are
  in `tools/project.py`. `deps="msvc"` makes ninja auto-track the PCH's headers, so
  touching `os/Debug.h` rebuilds the PCH and cascades to all eligible objs (proven
  staleness chain). **Matching-safe, gated:** whole-binary `matched_functions` is
  equal PCH-on vs PCH-off (W1-C worktree A/B + re-verified on main). NOTE: unlike
  dc3, `/FI"decomp_pch.h"` here is NOT strict-`.text`-byte-identical (it perturbs
  *untracked* helper/template inlining) — that is why `char rndobj world ui` are
  **excluded** (they regressed tracked matches, net -10); do not add them without a
  fresh 3-gate A/B. **`decomp_pch.h` is codegen-load-bearing — keep it sacred**
  (Object.h + Debug.h only; native-only edits must be `#ifdef HX_NATIVE`).
  **Instant disable:** `config.pch_eligible_dirs = set()` in `configure.py` +
  `python3 configure.py` → every TU reverts to the plain `msvc` rule, byte-identical
  to pre-PCH (`decomp_pch.h/.cpp` stay, unreferenced).

## Known issues / expected noise

- dtk SPLIT prints `WARN` lines about UTF-16 strings, `PpcRel`/`PpcAddr16`
  relocations, and unaligned symbols. These are **tolerated** — jeff was patched
  to downgrade asm-write failures to warnings (see jeff `src/cmd/xex.rs`), so
  `config.json` is still emitted and the build proceeds. **~30 such lines
  survive; that is the expected steady state.**
- ★ **A fresh `ninja` prints ~114 lines, NOT ~62,000 — as of dtk `v1.9.3`
  (`a88009b`, 2026-08-04).** If you see tens of thousands of
  `Skipping tail block merge` / `Not a function @` / `Control flow … hit known
  function` / `Warning! Illegal inst`, **you are running an old dtk** — check
  `dtk --version`, don't go hunting a split bug. Those five classes were 99% of
  the old log and **every one of them described expected behaviour**, measured
  against the retail `.pdata`: `Illegal inst` is inter-function **alignment
  padding** (all 265 sites are `0x00000000`, all in gaps between function
  extents — *not* VMX128), and ~17,200 lines were one cause, the **8-byte EH
  prefix** (a `.text` pointer + an `.rdata` pointer before a function; 97.0%
  have `addr+8` a real function start). Full census + the traps:
  Claude memory `project_split_log_noise_audit_2026-08-04.md`.
  ⚠ **`symbols.txt` no longer drifts** (fixed point committed) — if it does
  again, that is a real change, not the old cosmetic churn.
  ⛔ **rb3 (Wii) must keep stock dtk `1.3.0`** — the jeff fork exposes only 6
  subcommands and has **no `dol`**, which rb3's split invokes.
- Denominator is the **whole binary**, so this is the honest dc3-comparable
  metric — there's no denominator gaming. Matches register only when a unit has
  both (a) pinned section ranges in `splits.txt` and (b) a compiled `.obj` that
  objdiff equates byte-for-byte with the dtk target `.obj`.
  ★★★ **BUT THE REACHABLE CEILING IS 63.10%, NOT 100% — price headroom against
  that** (lane NOOBJ-1, 2026-08-13, `13de1453`; `tools/noobj_census.py`).
  **3,808,140 B / 36.90% of `total_code` CANNOT PAIR at current pinning**, so a
  measured `matched_code_percent` of 34.60% is **54.83% of the reachable
  surface**, not 34.60% of it. The census self-validates — rows sum to
  `total_functions` (69,228) and bytes to `total_code` (10,320,664), zero rows
  dropped:

  | class | units | rows | bytes | % total_code |
  |---|---|---|---|---|
  | PAIRABLE (has base obj) | 1,045 | 54,691 | 6,512,524 | **63.10%** |
  | UNPAIRABLE — no source | 230 | 4,454 | 2,106,356 | 20.41% |
  | UNPAIRABLE — `auto_*` (unattributed) | 1,809 | 10,083 | 1,701,784 | 16.49% |

  ⛔ **The 230 no-source units are NOT missing scaffolding — they are already
  declared in `objects.json` with a `src_path` that does not exist**, and
  `tools/project.py` drops the compile edge **SILENTLY**
  (`warn_missing_source=False`). Triangulated exactly: 1,434 declared − 1,204
  compiled = **230**. **229 of 230 are `xdk/*`** (D3DX9 shader compiler 637 kB,
  xgraphics ucode compiler 1.21 MB, xaudio2 167 kB) ⇒ closing them means
  **writing Microsoft vendor source**, which is out of scope per the standing
  user directive — and they are **already 100% MAPPED**, so the mapping goal is
  satisfied. ⚠ **Do NOT stub them into compiling**: that buys "pairable" rows at
  0% with no content, i.e. `ForceEmit_*`-class metric fitting.
  ⇒ **The `auto_*` class (1.70 MB, 16.49%) is the only one of the two unpairable
  classes that is reachable, and it needs IDENTIFICATION, not source.** The
  in-reach slice of the no-source class is just `xdk/LIBCMT` (160 rows /
  18,200 B — `__savefpr`/`onexit`/CRT stubs).
  ⚠ **CORRECTED SAME DAY (lane AUTOID-1, `eb7fd2b1`): 63.10% is itself ~1.75pp
  OPTIMISTIC — the true reachable ceiling is ≈ 61.35%.** **105 units counted
  PAIRABLE have base objs defining only 1–2 symbols** (**914 rows / 180,196 B /
  1.75% of `total_code`**) — the same map-scaffold mechanism as the no-source
  class, but counted on the *favourable* side.
  ✅ **RE-MEASURED 2026-08-17 at `6e13ee3f` on the `name_check` ruler: raw
  **62.867%** (6,488,248 B pairable) / **CORRECTED 61.121%** (6,308,052 B, after
  subtracting the 180,196 B map-scaffold class above), and the current
  `matched_code` of 3,723,704 B / **36.080082%** is **59.03% of the corrected
  reachable surface** — not 36.08% of it.** `total_functions` is now **69,227**
  (the 69,228 above is NOOBJ-1's own 08-13 self-validation, left as written).
  ⇒ The movement off 63.10% is **fully attributed and is NOT a regression**:
  PAIRABLE lost exactly the 24,276 B that `auto_*` gained — a **pin
  reattribution** — while the no-source and map-scaffold classes are
  byte-identical across all three measurements. ★ **This is the standing rule
  demonstrating itself: the ceiling moves BOTH WAYS, so re-measure it and NEVER
  inherit a prior lane's figure.** Current partition + provenance:
  `docs/decomp/CAMPAIGN_STATE_2026-08-17.md`.
  ⛔⛔ **AND THE `auto_*` CLASS IS MOSTLY UNREACHABLE TOO — DO NOT FUND
  ATTRIBUTION THERE.** Only **8.9% (1,766 rows / 151,024 B = 1.46% of
  `total_code`)** is attributable-**and-portable**, at a measured 0.64% FP;
  **two-thirds is flanked by XDK source we lack or by 7-LINE QUAZAL MAP
  SCAFFOLDS** (`src/network/quazal/*.cpp` are `namespace Quazal {}`; 103 of 117
  `network/` sources are <20 lines, median **7**), so attributing them buys a
  pairable row **at 0% with no content**. Extreme upper bound **5.51%**.
  ⚠ Two instrument traps from that lane, both worth reusing: **"enclosed by the
  same heading on both sides ⇒ membership" FAILS at 66.24% precision (33.76%
  FP)** — it is also exactly the `spatial:*` provenance `tools/scope_map.py`
  puts in its tier denominators (now surfaced per-tier by `36b844c4`) — and
  **XDK is INTERLEAVED throughout `.text`, NOT confined above `0x82A00000`**: an
  address-band pass read **42.7% of rows as GAME and all 3,941 were Quazal
  middleware**.
  ⚠ **Do NOT hardcode the denominator — read `total_code` / `total_functions`
  from `report.json`.** This doc said **11,790,708 code bytes** for months; lane
  CJ-3 measured **10,688,812** (2026-08-02, confirmed independently on the landed
  tree). A lane predicting a Δcode% off the stale figure misses by ~10%.
  `total_code` also **moves** when splits pins change — `4b3c098d` shifted it by
  52,184 B — so it is not a constant to memorise at all.
  ★ **And it moved again the same day**: at `f48bcad7` the measured value is
  **10,688,688** (`total_functions` 69,357), 124 B below CJ-3's figure a few
  commits earlier. Two "current" readings taken hours apart already disagree —
  which is the point of the rule, not an exception to it. **Read the key.**
  ★★★ **AND SEVERAL `report.json` NUMBERS ARE JSON *STRINGS*, NOT NUMBERS** —
  confirmed on `matched_code` and `functions[].size` (2026-08-03, four lanes).
  Un-coerced, `+` silently **concatenates** and a `>` comparison compares
  lexicographically, so a size filter reads a clean, decisive-looking **`0 rows`**
  — which is exactly how one lane lost a census. **`int()`-coerce every numeric
  you pull out of `report.json`**, and treat a suspiciously empty result here as a
  type bug before believing it.
- ★★★ **THE SOURCE-ONLY UNIT CEILING IS NOT A CONSTANT AND NOT MONOTONIC.**
  Measured 253 (DS-4) → reproduced 253 (DT-1) → **293** (EB-3, after ~33 units
  crossed in as unpaired-anon went to zero) → **290** (EC-2, when three units
  proved never completable by any amount of source work). **It moves in BOTH
  directions**, so a prior lane's ceiling is neither a target nor a floor —
  re-measure it exactly like `total_code`. ⛔ Corollary: "N units of headroom
  remain" is only true at the commit it was measured on.
- ★★★ **RULER CHANGE 2026-08-02 — every "honest" figure written before that date
  is stale by ~21,500, and NOT because anything regressed.** The objdiff fork was
  flipped so `masked_equal_functions` discloses **all** funclet byte-signature
  pairings instead of pass-2b over-subscription only: **1,096 → 22,640**, so
  honest (`matched − masked_equal`) went **42,358 → 20,814** and the disclosure
  share went 2.52% → **52.10%**. `matched_functions`, `matched_code_percent` and
  every other score key are **UNCHANGED** — this was disclosure, never scoring.
  Baseline at `f48bcad7`: **43,454 matched / 22,640 masked_equal / 20,814 honest /
  38.810524 code% / fuzzy 45.912785**.
  ⚠ **Δhonest values do not compose across the flip.** A pre-flip Δhonest is
  still valid *as a delta on the old ruler* but is not comparable to a post-flip
  one; never chain them. `tools/ab_measure.py` has a same-ruler guard (`373d17c6`)
  that pins objdiff-cli's sha256 across both legs and REFUSES on a mid-run swap —
  which would otherwise fabricate Δhonest ≈ −21,500 from an untouched tree.
  Authoritative record + rollback: `docs/decomp/RULER_CHANGE_2026-08-02.md`.
- ★★★ **THE TWO HEADLINE NUMBERS ARE COMPUTED ON DIFFERENT RULERS** — verified
  exactly, on both legs of an A/B, with the rival hypothesis failing in both
  directions (lane DB-4, 2026-08-02):

  | measure | rule | verified |
  |---|---|---|
  | `matched_functions` | **count** of rows with `match_percent_normalized == 100` | 43,456 / 43,458 exact |
  | `matched_code`      | **Σ size** of rows with `fuzzy_match_percent == 100`    | 4,148,656 / 4,148,948 exact |

  `mpn` **excludes arg-only penalties**, so the two disagree on **219 rows /
  101,996 B (0.954 pp of `total_code`)** at `9c8e4f2c`: counted as matched
  *functions*, bytes withheld from matched *code*. ⇒ **A change can move bytes
  with Δfunctions = 0, or functions with Δbytes = 0** — that is not an anomaly to
  chase, it is the definition. It explains DA-1's SIVideo row exactly
  (`?Frame@SIVideo@@QAAPADH@Z`, 72 B, fuzzy 99.72222 → 100.0, mpn 100.0 on BOTH
  sides ⇒ **+72 B / +0 fns**), and it is the same mechanism as "naming pays +1
  honest / +0.000000pp code%" running in the opposite direction.
  ★★★ **And mpn's arg-blindness is not merely a pricing quirk — IT HIDES REAL
  BUGS.** A caller that indexes the wrong container type, or calls the wrong
  callee, scores a clean **100 before AND after** the fix — five lanes, five
  waves (TourWeightManager `d7a9775a`, LayerDir `81d23046`, SetPropertyValue
  `dbab6082`). ⇒ **Never read a 100% row as evidence that a member type or callee
  is right, and expect a correct fix here to be Δmatched 0 — land it anyway.**
  ⚠ **Do NOT read the 0.954 pp as a lever — it is SIZED AND EMPTY** (DC-4
  `dcd456f6`, exact 219/219): **184 register** (permuter, OFF by directive) ·
  **21 branch_dest** · **14 shift/mask** · **0 unknown**. ⛔ Two corrections to
  this doc's own earlier reading: the boundary/**naming** sub-class is not "real
  but unsized", it is **ZERO and structurally impossible** — objdiff's `reloc_eq`
  returns true *regardless of target name* under `functionRelocDiffs=none`, so
  naming costs zero on BOTH rulers
  ⛔⛔ **THAT "STRUCTURALLY IMPOSSIBLE" IS NOW FALSE — it was a property of the
  `none` ruler, which stopped shipping on 2026-08-12** (`d04c83df`; lane
  RULER-SWEEP, 2026-08-13). Under the shipped `name_check`, `reloc_eq` **does**
  compare the target name, so naming does **not** cost zero. Read the economics
  exactly, because they are asymmetric and counter-intuitive:
  **(a) repairing a WRONG existing map name PAYS** — MAPDEF-3 (`db9eb318`)
  measured **+108 B** from 9 such rows with the `none` control **unmoved at +0**.
  **(b) adding a NEW name to a previously-anonymous address has ZERO call-site
  upside and REAL downside** — `name_check` *forgives* a placeholder target name
  (`fn_`/`lbl_`/`jumptable_`/`data_`/`bss_`/`rdata_`; see
  `is_placeholder_symbol_name` in objdiff-core `diff/code.rs`), so an unnamed
  callee is already uncharged. Naming it converts a **forgiven** site into a
  **checked** one: right name = still 0, wrong name = a new charge. ⇒ **naming is
  now a bet, not a freebie** — though it still pays via the separate *pairing*
  channel (+1 honest), which is unchanged.
  **(c) adding an ALIAS is forgiveness and therefore always "pays"** — objdiff
  consults `SymbolEquivalences` and drops the charge — so an *unproven* alias
  lifts the score **by construction** and is an integrity hazard, not a win.
  ⚠ The `none` control **CANNOT catch a fabricated alias**: `none` ignores
  relocation names, so it reads +0 there by construction. **That flatness is the
  SIGNATURE of the hazard, not a clearance** — pair it with retail-byte evidence
  that the fold is real. ★ **But name_check-UP / `none`-FLAT is ALSO the
  WRONG-CALLEE-FIX signature** — correcting a genuinely wrong callee moves
  `name_check` alone for the same reason. **The two shapes are separable only by
  PATCH KIND**: map-only ⇒ alias-suspect, `source` in the patch ⇒ the most
  valuable class of real fix we have. `ab_measure`'s `control_none_shape()`
  encodes exactly this (`5247b811`) and fires ALIAS_SUSPECT only on map-only
  patches — do not re-derive the guard by hand. `scripts/icf_alias_groups.json` holds **1,407** ungated
  groups and advertised itself as free to merge until 2026-08-13; do not.
  ★★★★ **AND THE WHOLE MECHANISM IS WORTH 720,992 B / 6.985907 pp — MEASURED
  2026-08-14 (lane ALIASAUDIT-1, `df90b49f`), never sized before.** Emptying all
  **1,493** groups moves `matched_code` by **−720,992 B**, `matched_functions` by
  **exactly +0**, the `none` ruler **exactly flat**, and units at all-rows-`fuzzy`
  **124 → 51 (−73)** — the arg-blind shape exactly. ⇒ **~7 pp of `matched_code`
  is alias forgiveness.**
  ✅ **RE-MEASURED 2026-08-16 by the same ablation (lane ALIAS-2, `64088f62`):
  **1,528 groups / 15,196 folded memberships / 818,416 B = 7.929877 pp.** The
  mechanism GREW — **~22% of everything we count as matched rests on it.** The
  evidence-split table below is superseded along with it: **PROVEN 92.73% ·
  NEEDS_SOURCE 1.96% · CONTRADICTED 1.78% · NEEDS_MAP_ID 0.00%**, the last
  **drained to zero** by MAPID-1 (see the `0x827bcd38` correction below).
  ⛔ **And the table's "11.00% unattributable" row was a CENSUS BLIND SPOT, not
  an exposure** — ablation shows **0 of 1,894 rows depend on a non-proven
  membership**, i.e. the name-keyed census could not see what the forgiveness
  actually rested on and booked its own blindness as risk. ⇒ **size this
  mechanism by ABLATION, never by a name-keyed census.**
  ⛔⛔ **`OK (grounded)` MEANT MAP-CONSISTENCY, NOT PROOF OF FOLDING** — the
  survivor is map-resident and every spelling is referenced, nothing more. The
  gate is real (a *fabricated* alias between symbols our map places at distinct
  addresses ⇒ **exit 2 fatal**, demonstrated by mutation — a PASS is worthless
  until the gate is shown to fail), and the audit found **0 CONTRADICTED**. But
  "not contradicted" is not "proven". **Renamed `OK (MAP-CONSISTENT)`**, and the
  rename is *proved* mechanically: eight groups emptied of **every** folded
  spelling — declaring no fold whatsoever — **still land in that bucket and the
  count does not move.**
  ✅ **CORRECTED SAME DAY (lane GROUNDED-1, `f4e26fcc`) — THIS DOC FIRST SAID
  "1,287 groups carry essentially all 720,992 B on map-consistency". THAT
  OVERSTATED THE EXPOSURE: 82.51% of the forgiven bytes ARE proven on retail
  bytes.** The error was **conflating the validator's re-check LABEL with the
  INSTALLATION evidence**, which is recorded per group in `symbol_aliases.json`
  as tiers **T1/T2/T3** (T1 = retail-byte identity with relocation **target
  names** compared, plus anti-vacuity guards). **Most bytes were never resting
  on map-consistency — the LABEL was.**

  | class | rows | bytes | share |
  |---|---:|---:|---:|
  | **PROVEN on retail bytes** | 2,718 | **594,904 B** | **82.51%** |
  | unattributable by this method | 1,889 | 79,288 B | 11.00% |
  | UNPROVABLE — needs absent source | 218 | 44,140 B | 6.12% |
  | UNPROVABLE — needs one map identification | 5 | 740 B | 0.10% |
  | **CONTRADICTED → withdrawn** | 10 | **1,920 B** | 0.27% |

  ★ **Only 448 of 1,493 groups forgive any bytes at all**; top 10 = **55.6%**,
  top 50 = 85.0%, top 100 = **96.4%** — price any alias work against that
  concentration, not against the group count.
  ⚠ **Flat T1 alone UNDERSTATES provability by 27 pp** (55.5% → 82.51%): its
  vacuity guard is right as a *guard* and wrong as a *verdict* — when relocation
  target names **agree**, the destination is not masked at all, and for a thunk
  **the destination is the whole information content.**
  ★★★ **129,360 pair-bytes are IRREDUCIBLE, not a backlog** — relocation-free
  thunks where the fold is proven but **which name the call site meant was
  destroyed by ICF itself.** That is the honest floor. And **one address is
  worth 73,496 B**, proven only by our-side COMDAT identity because retail's
  branch destination `0x827bcd38` is **unnamed** (no `?MemAlloc@@YAPAXHH@Z` in
  the map at all).
  ✅ **NO LONGER TRUE — CORRECTED 2026-08-16 (lane MAPID-1, `436bfb22`):
  `0x827bcd38` IS NOW NAMED `?MemAlloc@@YAPAXHH@Z`.** The identification was
  made and measured **−1,656 B** — deliberately net-negative, accuracy over
  headline — and it **exposed 6 real wrong-callee divergences** the alias had
  been forgiving (`MemAlloc` / `_MemAlloc` / `_MemAllocTemp`; the temp allocator
  is a *different allocator*, so those are behavioural bugs, not naming noise).
  `NEEDS_MAP_ID` is drained to zero as a result. ⇒ **the payout of naming an
  anonymous address under `name_check` is BUG EXPOSURE, not bytes.**
  ⛔ **THE 10 WITHDRAWN WERE A REAL DEFECT THE ALIAS WAS HIDING** — eight
  memberships where **retail's instantiation is 8 bytes SMALLER than ours**
  (different-size COMDATs **cannot** fold; the alias was forgiving **our use of
  the wrong overload**), plus one where retail calls `KeyLessEq` and we call
  `KeyGreaterEq`. Predicted **−1,920 B**, measured **−1,920 B exactly.** Groups
  kept with `folded: []` + a `withdrawn` record; **nothing pruned.**
  ⚠⚠ **CORRECTED 2026-08-16/17 — THE SIZE PREMISE WAS AN ARTIFACT IN OUR OWN
  READER, AND 6 OF THE 8 SIZE-BASED WITHDRAWALS ARE RESTORED.** STLPORT-1
  (`ff832b50`) refuted the "+8 B STLport source bug" outright: it did not
  exist — `tools/coff_bodies_ext.py` was billing the **successor symbol's EH
  funclet prefix** into the COMDAT span, so "retail's instantiation is 8 bytes
  SMALLER" compared a `.pdata` *function extent* against a COMDAT *span
  including the funclet*. The two sides were never the same measurement.
  GROUNDED-2 (`6e13ee3f`) restored **6 folds at +1,728 B**, confirmed on three
  independent instruments. The `KeyLessEq`/`KeyGreaterEq` membership
  (`Keys<Quat>::Remove`) is **untouched by this and stays confirmed-withdrawn**
  — that one was a genuine defect. ⇒ **Count right, cause wrong: a reproducing
  count is NOT evidence for its mechanism**, and a *size* test cannot catch a
  one-sided reader artifact because it cancels on both sides
  (`project_one_sided_instrument_error_invisible_to_two_sided_control_2026-08-16.md`).
  ✅ **FIXED (lane ALLOCGATE-1, `07c62807`)** — this doc used to warn that
  `tools/alloc_fold_gate.py`'s docstring *still* refused `??2@YAPAXI@Z` over a
  `gNewOperatorAlign` divergence that no longer exists. That paragraph now
  carries a **⚠ DATED RECORD** banner and a **★ TODAY IT ADMITS** section (the
  admission was installed in `b288c232` and measured **+67,884 B / +339
  complete fns**). ★ The durable point the gate makes about itself: **nothing
  in it is hardcoded — every verdict is recomputed from the compiled COMDAT
  bytes, so a refusal recorded in prose can never be an operative refusal.
  Re-run it; do not read a docstring for a verdict.**
  ⚠ **Do NOT prune the classes that currently forgive 0** (`STALE_SPELLING` 143,
  `UNWITNESSED` 11): they **become live as porting advances**, and a prior prune
  (`a745039e`) cost **+94,616 B to reverse**. **A Δ0 today would license a change
  that degrades later.**
  ⛔ And the 219-row population above should be **re-derived on `name_check`**
  before anyone calls it empty again — "SIZED AND EMPTY" was measured on `none`.
  ✅ **DONE 2026-08-13 (lane MPNGAP-1, `2f5a3cd3`) — DC-4's verdict SURVIVES IN
  SUBSTANCE at 9× the nominal size.** On `name_check` the population is
  **6,384 rows / 930,204 B** (8.7% of `total_code`) vs DC-4's 219 / 101,996 B.
  The expansion is *by construction* the relocation-NAME class: report `fuzzy`
  **is** `match_percent`, and `arg_diff_score` (what `mpn` excludes) counts only
  **non-immediate** arg diffs, so this stratum is exactly "rows whose every
  penalty is reloc / register / branch-dest" — under `none` those were free.
  ★★★ **But it is DEPLETED, not enriched, against a control that could fail**
  (400 rows at `mpn < 100`, same units, size-matched): real-defect class
  **10.7% vs 12.1% control = 0.88×**; charged name-sites per row 1.43 vs 1.34
  = **1.07×**. ⇒ **Conditional on a charged relocation-name site existing, the
  fold-vs-defect mix is the same as in ordinary broken rows.** The stratum's
  special property is **composition, not density** — name-sites are 75.7% of
  charged instructions here vs 3.1% in control, and rows carry 1.89 charged
  instructions vs 43.4 — so what it buys is **realisability** (all-or-nothing
  `matched_code` means fixing the one name site suffices to cross). Budget:
  15.8% anonymous rows (**zero** byte upside — placeholder targets are already
  forgiven), 13.1% ceiling, 5.3% class A, **0.9% provably fixable**, of which
  **+6,304 B realised, predicted exactly**. ⇒ **~91% is irreducible fold/map
  noise. Do not re-fund this as a byte lever.**

  Continuing DC-4's second correction: the 14 shift/mask rows (twice sold as a
  "struct-size oracle") adjudicate on retail bytes to **0 real defects** (DD-1
  `78e19b99`, refuted before as BZ-3) — do not re-fund. 176 of the 219 sit at
  fuzzy ≥ 99. ⚠ And a `REGISTER_SWAP` label on a **sub-100** row is a SYMPTOM,
  not a diagnosis: 13-/24-instruction swaps and a full prologue delta all
  DISSOLVED once the real source defect was fixed (`5d8fc966`, `c14bba5c`,
  `d7a9775a`; 12 instances) — never defer a row as permuter-bound on that label.
  ⛔⛔ **EB-4's EQUIVALENCE BELOW IS STALE — IT PREDATES THE 2026-08-12
  `name_check` FLIP** (measured by lane RECOVER-95K, 2026-08-13). `report.json`
  now scores on **`name_check`**, while `run_objdiff`'s "normalized" is still the
  **`none`** ruler ⇒ **the two no longer agree, and "normalized" is STRUCTURALLY
  BLIND to the wrong-callee / relocation-NAME defect class.** Measured live:
  `run_objdiff` reported a row *"Complete (High confidence), 100.0% normalized,
  0 mismatches"* while `report.json` **on the same tree** had it **below
  `fuzzy == 100`**. ⇒ **Drive `objdiff-cli` at `name_check` explicitly, or read
  `report.json`.**
  ⛔⛔ **AND THE MISMATCH *COUNT* IS `none`-RULER TOO — IT UNDERCOUNTS, WHICH
  MANUFACTURES PHANTOM PRIZES.** (lane RESIDUAL-1, 2026-08-14, `348e3c7b`.)
  `?Handle@CustomizePanel@@` was briefed to **three** consecutive lanes as
  *"5,036 B behind exactly 3 mismatches"* — the best size-if-it-crosses in the
  tree. On the graded ruler it has **FIVE** charged sites: the 3 insert/delete
  **plus 2 `diff_arg` ICF fold-aliases** (`hash_map<int,SongUpgradeData*>` vs our
  `<int,UIComponent*>`; `NUISPEECH::CCFGLM::RemoveCPPT` vs our
  `ClosetMgr::TakePortrait`). Because `matched_code` keys on `fuzzy == 100`,
  **closing all three instructions buys `mpn` 100 (+1 function) and EXACTLY ZERO
  BYTES** — the 5,036 B additionally requires **both aliases proven on retail
  bytes**. ⇒ **A row's headline prize computed from `run_objdiff` can be
  uncollectable by source work in principle.** Price a candidate from
  `report.json`'s charged-site list, not from a mismatch count, **before** it is
  briefed as a target.
  ✅ **THE `none`-RULER HALF OF THAT IS FIXED (lane MCPRULER-1, 2026-08-14,
  `7286bfd1`)** — `mcp_server.py` had been passing `-c functionRelocDiffs=none`,
  and since `objdiff-cli` applies `-c` **LAST** it was **actively OVERRIDING** the
  shipped `name_check` (7,157 rows disagreed; 5,555 rows / 674,936 B read
  `fuzzy == 100` under `none` but below 100 graded). It now resolves the ruler
  from `report.json`'s `provenance` and **self-labels**; `none`/`data_value` are
  explicit opt-ins.
  ⛔⛔ **BUT THE READING ERROR SURVIVES THE FIX, AND HAS NOW BITTEN THREE LANES:
  "N/N instructions equal" is INSTRUCTION-level and does NOT include
  relocation-name charges, which are ARGUMENT-level (`diff_arg`).** They coexist
  with *all instructions equal* — one row reads **"205 instructions | all
  equal"** while scoring **98.4% graded**, and a lane pre-registered **+96 B**
  off a "24/24 equal" reading and measured **−92 B**. ⇒ **An equality count and a
  mismatch count are both the wrong instrument. Price from `report.json`'s
  charged-site list — always.**
  ★★★ **`run_objdiff`'s "normalized (raw)" pair vs the report keys — SETTLED
  (lane EB-4, 2026-08-03). This note used to say the pair "does not equal these
  report keys", which was MISLEADING: its own example proves one of them equals
  a report key exactly.** In `97.5/95.0` vs `mpn 100.0 / fuzzy 97.5`, the
  printed **normalized 97.5 IS report's `fuzzy_match_percent` 97.5** — measured
  **EXACT on 2,669 rows / 0 disagreements** across every stratum, once `diff` is
  given the report's config. **There is NO defect in either path.** The two real
  differences:
  1. **"normalized" names two orthogonal axes.** `objdiff-cli diff`'s
     `normalized_match_percent` = **relocation**-normalized `match_percent`
     (== report `fuzzy`); report's `match_percent_normalized` (**`mpn` — the
     ruler `matched_functions` counts on**) = **arg-penalty-excluded**, and
     `diff` **never emits it**. `mpn ≥ fuzzy` ALWAYS (`code.rs:285`), so
     **a sub-100 `run_objdiff` reading NEVER proves a row is unmatched** — it is
     a lower bound on BOTH report keys. 221 rows / 102,900 B sit at
     `mpn == 100` with `fuzzy < 100`.
  2. **`ppc.calculatePoolRelocations`** — report sets `false`, `objdiff-cli diff`
     defaults `true`. This was worth **118 of 1,639** named sub-100 rows (max
     **14.75 pp**) and made **11 of 20,667** rows the grader scores at `fuzzy==100`
     read 97.7–99.3, i.e. lanes grinding already-complete rows. **Fixed in
     `mcp_server.py`**; `run_objdiff`/`run_diff_inspect` now replicate the
     grader's config and agree exactly.
     ✅✅ **AND NOW FIXED FOR EVERY CALLER, IN THE PROJECT CONFIG (2026-08-31).**
     The `mcp_server.py` fix covered the MCP tools and nothing else — a bare
     `bin/objdiff-cli diff` (the command in half of `docs/plans/`), a `--batch`
     sweep, permuter scoring and any new script all still read `diff`'s own base
     config. **`objdiff.json`'s `options` block is the one place BOTH CLI entry
     points read unconditionally**, and it now pins **all four** divergent keys
     (`functionRelocDiffs=name_check`, `combineDataSections=true`,
     `combineTextSections=true`, `ppc.calculatePoolRelocations=false`) in
     `tools/project.py`. Whole-binary sweep on this repo at `26576070`, objdiff
     4.2.8, full build first: of **47,208 comparable rows** (22,009 unpaired =
     agreement, 123 `base_unit` = a different question), **102 functions /
     55,604 B disagreed**, report higher on 100 and `diff` higher on 2, up to
     **65.20 pp**; one 308 B row read exactly 100.0 in `report.json` and <100
     through `diff`. `ppc.calculatePoolRelocations` alone explains 102/102 — but
     the two `combine*` keys are **not inert**: applied without it they ADD two
     disagreements, so pin all four. Charged rows can be two *textually
     identical* instructions, because the "relocation" is a synthesized display
     annotation (`arch/ppc/mod.rs:819 make_fake_pool_reloc`) reconstructed from
     each object's own symbol table, and `reloc_eq`'s `_ => return false` arm
     charges a **target-only** one under every ruler except `none`. **No
     recorded number moved** (42,274 / 3,772,560 / 36.819992% / fuzzy 48.921097
     before and after; objdiff's own `Report cache: 3091 hits, 0 misses`
     independently certifies the resolved report config is unchanged), and the
     re-sweep leaves **0**. Upstream objdiff, not a fork bug (`0c9e552`,
     2025-05-07, `report.rs` only) — `bin/objdiff-cli` is a symlink shared with
     `../rb3` (151 fns / 224,892 B) and `../dc3-decomp` (155 fns / 120,728 B),
     and all three are now fixed the same way, **config-only, no tool rebuild**.
     Guard: `python3 scripts/verify_ruler_agreement.py --check` (~0.2 s) and
     `--selftest` (flips the knob back and *requires* failure; exits 5 "vacuous"
     rather than passing on an empty probe — verified: 3,320 witnesses agree,
     31 disagree under the flip, and a witness-free unit exits 5). Also wired
     into ninja as `CHECK RULER AGREEMENT`, gating `REPORT`; verified failing by
     deleting one pin. Full write-up:
     [docs/decomp/patterns/two-objdiff-entry-points-two-rulers.md](docs/decomp/patterns/two-objdiff-entry-points-two-rulers.md).
  ⛔⛔ **THE "`diff_inspect.py` + `stack_layout.py` RUN AT `DataValue`" CLAIM IS
  REFUTED — and the mechanism that broke it also broke `mcp_server.py` the other
  way** (lane MCPRULER-1, 2026-08-14). Both facts follow from one line:
  **`objdiff-cli diff` applies `objdiff.json`'s `options` block over its own base
  config** (`diff.rs:953`), and `-c` args are applied **last** (`diff.rs:959`).
  So since `d04c83df` shipped `options = {"functionRelocDiffs": "name_check"}`
  on 2026-08-12:
  - **passing NO `-c` silently stopped meaning `DataValue` and started meaning
    `name_check`** — measured on `?Handle@OvershellSlot@@`: no-`-c` =
    **99.995690**, explicit `data_value` = **98.044420**, explicit `name_check` =
    **99.995690**. The two scripts were *already* on the graded ruler, and the
    "deliberately left alone, `DataValue` shows a wrong `bl`" property had
    quietly evaporated. (It is no loss: `name_check` charges a wrong callee by
    NAME, so the defect stays visible without the address noise.)
  - conversely **`mcp_server.py`'s hardcoded `-c functionRelocDiffs=none`
    OVERRODE the shipped ruler**, because `-c` wins.
  ⇒ **Both are now RESOLVED AT RUNTIME from `report.json`'s
  `provenance.diff_config`** (a complete 22-key dump written by the grading run
  itself) via **`scripts/analysis/ruler.py`**, never hardcoded — a second
  hardcoded constant would rot on the same silent schedule. `ruler=graded`
  (default) / `none` / `data_value` are explicit opt-ins that change **only**
  `functionRelocDiffs`, and **every percentage is now labelled with its ruler**.
  Verified: `objdiff-cli diff` at graded == `report.json`'s
  `fuzzy_match_percent` on **2,617 / 2,617 rows, 0 disagreements** (787 further
  sampled rows are `fuzzy == 0` with no base symbol — the UNPAIRABLE
  `auto_*`/`xdk` classes — and fail identically on both legs, so they hide
  nothing); the same comparison run on the OLD hardcoded ruler **disagrees on
  1,332 rows**, i.e. the check can fail.
  ⚠ **What this cost, measured:** **5,555 rows / 674,936 B** read `fuzzy == 100`
  under `none` but below 100 on the graded ruler — rows the orchestrator called
  *"Complete — No action needed"* while the grader withheld every byte.
  ⚠ **The mismatch COUNT is ruler-dependent too, not just the percent**: one row
  (`?Handle@OvershellSlot@@`) shows **0 / 2 / 641** charged sites at
  `none` / `name_check` / `data_value`.
  Full mechanism, counts, conversion rule and nulls (⚠ EB-4's `-c` prescription
  is superseded above, its *measurements* stand):
  `docs/decomp/OBJDIFF_DIFF_VS_REPORT_SETTLED_2026-08-03.md`.
  **Still: believe `report.json` for the score.**
- ★ **`total_code` is EXACTLY Σ(listed function sizes)** — verified whole-binary
  (10,688,672 == 10,688,672) and per-unit (CheatProvider 7,512 == 7,512). So
  bytes with no listed function row — notably the 8-byte EH prefixes — are **not
  in the denominator at all**.
- **Case sensitivity:** dc3's tree has case-variant files (e.g. `vec.cpp` vs
  `Vec.cpp`) that collide on Windows but coexist on Linux. Use the name dc3's
  `objects.json` actually builds (lowercase `vec.cpp`, `mtx.cpp`).
- ★★★ **`grep` in an agent's Claude shell is BINARY-BLIND — it yields only FALSE
  NEGATIVES.** The shell snapshot (`~/.claude/shell-snapshots/snapshot-zsh-*.sh`)
  defines a `grep` **function** routing through **ugrep with `-I`** (ignore binary
  files). `grep -c '\.?AVUIComponent@@' orig/45410914/band.exe` prints **nothing**
  and exits 1; `command grep -ac …` prints `1`. Silent, and shaped exactly like a
  *decisive negative* — the verdict class that closes veins. It has already cost
  real yield (a "no `$4` form exists anywhere" claim blocked a repairable row;
  Python found 21).
  **THE RULE: always pass `-a` when grep may see binary bytes.** `-a` overrides
  the shim's `-I` *and* real grep's "binary file matches" suppression — one flag
  defeats both independent sources. Better still: **scan binaries in Python.**
  - Measured scope (don't over-apply): the shim dies at every process boundary —
    zsh does not export functions — so `#!/bin/bash` scripts, `sh -c`,
    `subprocess(..., shell=True)`, and even a fresh `zsh -c` all get **real**
    grep. **Only commands typed directly into the Claude Bash tool are affected**,
    which makes *documented command recipes in `.md`/skill files* the real hazard.
  - `strings binary | grep …` is **safe** (strings output is text), but
    `cat binary | grep …` is **suppressed** — the shim's `-I` applies to stdin too.
  - Real grep's own binary suppression hits **printing** modes (`-o`, plain);
    `-c` and `-q` are unaffected.
  - Guard: **`python3 tools/grep_binary_guard.py`** (also a CI step). It builds
    its own binary fixture so it never skips, reconstructs the shim in a subshell
    to test the *actual* risk, and fails loudly if a recommended method returns a
    false negative. Prove it can fail: `--self-break`.

## Whole-binary A/B measurement — use the tool, not the checklist

**`python3 tools/ab_measure.py --worktree <wt> --from-dirty`** (or `--patch
<diff>` / `--pick REF` / `--revert REF`) is the DEFAULT way to price any change
against the whole-binary metric. The `/ab-measure` skill wraps it. It executes
the entire A/B protocol itself and **REFUSES (exit 2, no numbers) when a
precondition fails** — replacing the prose checklist that was broken from
memory repeatedly (four lanes burned a leg A on 198 settle-recompiles in one
session; one false +3 was pure settling noise). `--selftest` sanity-checks the
refusal logic without building; the selftest itself is validated to FAIL under
a sabotaged (vacuous) log classifier.

What it enforces — the manual steps survive here only as the explanation of
*why* (do not hand-run them as the normal path anymore):

- **Settle-to-zero-work before leg A — ★ and, since lane DT-3, before leg B
  too, over BOTH the default target AND the report target.** A fresh
  worktree's first build reads ~+193 matched / +0.51pp of settling noise; the
  tool discards every pre-quiescent reading and refuses if it can't reach a
  zero-work build.
  ⚠ **Leg B used to get exactly ONE build with no retry** while the leg *reads*
  build the **report** target — and the graph intermittently produces a
  **second wave of dirtiness after a build finishes**. Measured twice on a
  PCH-cascading patch (`src/system/obj/Object.h` is a PCH input; 9 engine dirs
  / ~281 TUs compile through the PCH): leg B's default build did **956** objs
  and the very next report build did **1101** — the same 956 *again* plus 145
  — so the read's zero-work guard correctly REFUSED a legitimate measurement
  (lane DS-1). ⚠⚠ It is **NOT deterministic**: an immediate re-run of the same
  patch, same worktree, old tool, did **not** reproduce it, and four
  hand-driven probes did not either. That intermittency is the argument for
  the loop — a single *unverified* build cannot be trusted either way. Cost of
  the fix: **+2.9 s** of zero-work ninja invocations per run (measured).
  ⚠ **Unsettled is WRONG, not merely noisy — the SIGN flips.** Lane DF-2 read
  **+23 matched / 17 bodies at 100** off apply-revert cycles in its worktree; the
  settled A/B found **0 at 100 and most bodies WORSE** (82.25 → 34.48). Applies
  to *any* in-worktree `report.json` read, not just an A/B leg.
- **`report.json` + `report.cache` wiped before EVERY read** (stale cache
  inflates); measures parsed **by exact key** — a missing key (e.g. the old
  `.get('masked_equal', 0)` wrong-key bug) REFUSES instead of defaulting to 0.
- **symbols.txt auto-restored**; patches touching it are refused outright.
- **Map/splits patches force a re-split on BOTH legs** (rm renamer stamp +
  `touch config/45410914/config.yml`) — an un-resplit map edit is INERT
  (`[APPLIED] … 0 files patched`; lane CF-1 lost a whole leg to that
  absent-vs-absent A/B). Leg B must show SPLIT ran and the renamer patched
  >0 files, or the run refuses.
  ★★ **AND THE RE-SPLIT IS NOW ITERATED TO A `symbols.txt` FIXED POINT, ON
  BOTH LEGS — ONE FORCED SPLIT PER LEG SILENTLY UNDER-REPORTED BYTES** (lane
  ABSPLIT-1, 2026-08-14). `symbols.txt` is simultaneously a *discovered input*
  of the SPLIT edge and an *output* of it, so jeff's Class-4 over-carve merge
  (`merge_branch_reached_overcarve_tails`) converges **across** re-splits:
  split #1 performs the merge and writes the merged symbol back, but the obj
  it emitted was carved before that merge was recorded. Measured on the landed
  known-answer fixture `ab5ebed3` (worth **+1 fn / +120 B** at its own
  converged fixed point), splits-only patch, one worktree, warm cache:

  | | Δmatched | Δcode_bytes |
  |---|---|---|
  | one forced split per leg (old) | **+1** | **+0** ⛔ |
  | iterate to the fixed point (new) | **+1** | **+120** ✅ |

  ⛔ **The shortfall was invisible because `matched_functions` moved either
  way** — `mpn` excludes the arg-only penalty the un-converged carve leaves
  behind, so the two rulers disagreed and nothing flagged it; leg B read
  `fuzzy 99.833` un-converged vs `100.0` converged. A confident
  "A/B RESULT (MEASURED)" was printed for a reading short by the FULL amount.
  ⇒ Each leg now re-splits until the split's **output** `symbols.txt` equals
  its **input** (`--max-resplit`, default 4), and a leg that never converges
  is **REFUSED**, never priced (oscillation is diagnosed separately — raising
  the bound cannot help a two-cycle).
  ⚠ **The iteration count is leg-DEPENDENT data, which is why both legs must
  iterate**: on that fixture leg A was *already* at a fixed point (0 extra
  builds, and its leg-A numbers are byte-identical old-vs-new) while leg B
  needed 1 — so "one split each" was comparing two structurally different
  states. A map-only patch converges in **0 on both legs** (measured), so the
  fix is free on that path.
  ⚠ **Nothing here restores `symbols.txt`** — convergence works by feeding the
  drift BACK IN, the exact opposite of the wave-CJ in-loop restore that made
  settling impossible (lane CK-2). The selftest's shape guard was widened to
  cover the new code for precisely that reason.
- **Source patches must recompile ≥1 TU in leg B** or the run refuses as
  absent-vs-absent. The leg B recompile count is taken from the build log
  BEFORE any report step (`run_objdiff`-style flows compile invisibly, so a
  later count reads 0 and proves nothing). ★ With leg B now settling, the
  application assertions read the **FIRST ITERATION's** counts, never the
  aggregate — otherwise unrelated work in a later settle build could
  masquerade as "the patch compiled".
- ★ **UNIT COMPLETIONS are reported by SET-DIFF of AT_100 membership, not by
  "whose matched count rose"** (lane DT-3, fixing lane DS-4's finding).
  **3 of DS-4's 13 unit completions had Δmatched = 0** — the fix removed a
  **wrongly-attributed row from the DENOMINATOR** (12/13 → 12/12), a class the
  old per-unit list could not see at all (nor could it see a unit *falling
  off* 100% the same way). Each completion is labelled with the mechanism its
  row counts imply (`MATCHED_ROSE` / `DENOMINATOR_SHRANK` / `MIXED` /
  `NEW_UNIT`), on **both** rulers — units are counted on `mpn`, **bytes follow
  fuzzy**. ⚠ **Units and bytes are SEPARATE measures**: DS-4's 13 completions
  moved code% by **+0.0123pp**, so unit completion is *not* a code% play.
  AT_100 is derived two ways that share no arithmetic (row-wise `mpn == 100`
  vs `matched_functions == total_functions`) and **REFUSES** if they disagree.
- **Deltas only from legs measured in-run.** There is deliberately no
  `--baseline` flag: deltas compose, absolutes do not, and a baseline file is
  an absolute somebody else measured (the coordinator once briefed 41955 by
  summing deltas; the measured value was 41956).
- ⛔⛔ **CORRECTED 2026-08-13 (lane FORK-DIAG). This bullet used to say the ninja
  report edge is "hard-coded `functionRelocDiffs=none`" — FALSE since `d04c83df`
  (2026-08-12 00:47) "Ship functionRelocDiffs=name_check".** `objdiff.json` now
  carries `options = {"functionRelocDiffs": "name_check"}` and
  `objdiff_report_args` is empty ⇒ **name_check is the SHIPPED DEFAULT; `none` is
  now the OPT-IN one.** `--name-check` keeps its noise-floor warning (nc
  aggregate code% is build-unstable ~0.05pp).
  ★★★ **THE RULER ALONE MOVES BYTES ~817 kB / 7.9 pp WITH ZERO SOURCE CHANGE.**
  Measured on ONE binary (`9f6c6c32ae11`), one tree, cache wiped between legs:
  `none` = **4,366,752 B / 42.31%** vs `name_check` = **3,549,568 B / 34.39%** —
  while `matched_functions` (44,252) and `masked_equal` (22,886) are
  **bit-identical**, because `mpn` excludes arg-only penalties and
  `none`→`name_check` changes *only* relocation-name arg comparison.
  ⇒ **ANY byte absolute recorded before 2026-08-12 00:47 is incomparable to one
  after it unless the ruler is stated.** ⚠ A swing of exactly this shape was
  mis-attributed to an objdiff **rebuild** on 2026-08-13 — it was the **ruler**,
  not the binary. ★ **`report.json` self-declares `diff_config`, `tool_commit`
  and `tool_binary_hash` in a `provenance` block — READ IT, don't infer.**
  ⚠ **`report.json` is protobuf-JSON: DEFAULTS ARE OMITTED.** Absent
  `fuzzy_match_percent` = 0, absent `masked_equal` = false, absent unit
  `matched_code` = 0; **zero** rows carry an explicit `0.0`/`false`, and a naive
  `d['matched_code']` **raises `KeyError`**. With the JSON-strings trap, read
  every numeric as `int(x.get(k, 0))`.
  ⚠ **The `masked_equal` ROW-FLAG is a SUPERSET of the MEASURE** — 24,386 flagged
  rows vs `masked_equal_functions` = 22,886. Not a defect: the counter increments
  only inside `match_percent_normalized == 100.0`, since it exists to discount
  *credit*. **Do not count flagged rows and expect the measure.**
- ★ **The worktree is handed back exactly as it was found, on EVERY exit path**
  (`--keep-applied` opts out, loudly). **ONE change per run** — a repeated
  `--patch`/`--pick`/`--revert` is refused instead of silently measuring the
  last one, and a **staged index** is refused under `--from-dirty`. See
  "✅ `ab_measure` now hands the worktree back exactly as it found it" above
  for the measured 12-cell matrix these replaced.

Controls re-executed 2026-08-03 (lane DT-3), all on the live tree at
`matched 43,694 / code% 39.466717 / 221 units at 100% of 1,023`: neutral 1-TU
comment ⇒ **Δ0 with exactly 1 leg-B recompile**; PCH-cascading `Object.h`
comment ⇒ **Δ0 with 2,057 recompiles, MEASURED** where the old tool's shape
refused; bad edit (immediates, not relocs) ⇒ **−1 matched / −72 B**, with
`MidiChannel` correctly reported as **falling off 100% on both rulers**;
map-row deletion ⇒ full re-split, `renamer_patched=1044`, **Δmatched 0 /
Δfuzzy −0.000077pp**; future-mtime sabotage ⇒ **REFUSED at settle** after 4
bounded attempts; patch to a TU absent from the ninja dep graph ⇒ **REFUSED
absent-vs-absent**.
⚠ Two fixture lessons from that run, both worth reusing: a **float-constant**
edit measures **Δ0** because the constant is a *relocation argument* the
default ruler masks — a bad-edit control must move an **immediate**; and a
`src/` file being absent from `objects.json` does **NOT** make it absent from
the build (`rnddx9/Cam.cpp` is `#include`d by 3 TUs and recompiled them). Pick
the fixture with `ninja -t deps`, not with `objects.json`.

Controls executed 2026-08-01 (lane AB-TOOL): neutral comment edit ⇒ Δ0 with 1
real leg-B recompile; bad branch-condition edit ⇒ exactly −1 matched / −88 B;
map-row deletion ⇒ −1 via the full re-split path (renamer_patched=1674);
future-mtime sabotage ⇒ REFUSED at settle; patch to an uncompiled source ⇒
REFUSED absent-vs-absent.

## Matching phase (active)

The pipeline is proven end-to-end on `MasterAudio.cpp` (2026-05-26): pinning a
real `.text` range in `splits.txt` produces a dtk target `.obj` + per-object
`.s`. The remaining work is per-target: derive splits → port source so it
compiles → diff via objdiff. See `project_rb3_xenon_roadmap.md` Phase 5 and
`project_function_identification.md` in memory for state.

### Splits-bootstrap recipe (per new cluster)

1. Run `tools/fingerprint_match.py autoid` to get source-file proposals.
2. For each tight cluster (≥3% density, ≥3 corroborating strings, NOT
   `Symbols*.cpp` which is a systematic FP), compute `[min(fn), max(fn)+size)`
   = the `.text` span. Cross-check the strings against `../rb3/src` or
   `../dc3-decomp/src` to confirm cluster identity.
3. Add to `splits.txt`:
   ```
   FooBar.cpp:
       .text       start:0xAAAAAAAA end:0xBBBBBBBB
   ```
4. `touch config/45410914/config.yml && ninja`. dtk emits
   `build/45410914/asm/FooBar.s` + `build/45410914/obj/FooBar.obj` and
   auto-derives the matching `.pdata` range (back-filled into `splits.txt`).
   This is not a one-time backfill: `.pdata` is re-derived from the `.text`
   splits on **every** split run — never hand-edit or hand-carry `.pdata`
   lines (see the splits.txt bullet in Build wiring for the two ★ traps).

### Obj patchers (WIRED)

`scripts/` holds dc3's MSVC object patchers. They rewrite the COFF **symbol
table** (not machine code) of `.obj` files to neutralize build-environment-specific
naming that MSVC bakes in, so objdiff compares real code rather than naming noise.
They are **wired and active** in `configure.py` (`config.custom_build_steps`,
mirroring dc3's block of the same name — line ranges deliberately omitted, both
cited ranges had drifted by hundreds of lines):

- **pre-compile** — `obj_target_symbol_renamer` rewrites the dtk-split *target* obj's
  anonymous `fn_<addr>` symbols to MSVC mangled names from
  `scripts/target_symbol_map.json`, so objdiff can pair target↔base by name. (Game
  entries are generated by `tools/gen_game_target_map.py` from the rb3-Wii oracle;
  without a map entry a pinned game TU reads a false 0%.)
- **post-compile** (on our compiled obj) — `anon_ns` (anonymous-namespace hashes,
  which MSVC derives from machine name + source path), `dynamic_init` (`??__E`
  STATIC→EXTERNAL), `guard` (`$S`→`??_B` static-init guards), `bool_mangle` (bool
  back-ref mangling), `atexit_scope` (`??__F` scope counters).

`regswap` + `transplant` exist in `scripts/` but are **not** in the wired list (enable
per-function when needed). The "guard-thunk wall" that drags game-unit fuzzy down
(retail emits `??__E`/`??__F`/guard thunks our objs don't pair) is what these address.

### Identification tooling

- `tools/fingerprint_match.py` — see Build wiring above. Generates the
  identification table used to derive splits.
- Cross-binary identification (planned): Ghidra + BinDiff transfer dc3's
  named functions (from leaked `ham_xbox_r.map`) onto RB3's anonymous
  `fn_8XXXXXXX` by structural similarity. BinDiff installed at
  `/usr/bin/bindiff`; BinExport plugin ships at `/opt/bindiff/extra/ghidra/`;
  XEXLoaderWV source cloned at `/home/free/code/milohax/XEXLoaderWV/` (needs
  rebuild for Ghidra 12.1 — installed prebuilt is 12.0.1).

## Orchestrator MCP, Ghidra MCP, skills

Ported from DC3 (2026-05-27). Both projects share the MSVC X360 toolchain so
most tooling transfers verbatim.

**Orchestrator MCP** (`.mcp.json` → `scripts/orchestrator/`):
- Server name: `decomp`. Backed by `decomp.db` (SQLite, 66k functions seeded
  from `build/45410914/report.json` via `scripts/ingest_report.py`).
- 11 tools: `report_result`, `query_functions`, `get_attempts`, `lookup_rb3wii`
  (greps `~/code/milohax/rb3/src` — RB3 Wii dev decomp, named functions),
  `lookup_dc3` (greps `~/code/milohax/dc3-decomp/src` — same compiler twin),
  `run_objdiff`, `run_analyze_function`, `run_diff_inspect`,
  `lookup_struct_offset`, `lookup_merged_symbol`, `mark_patch_result`.
- Worktree pool (`scripts/orchestrator/worktree_pool.py`) tracks per-agent
  worktrees in `decomp.db.worktrees`. Set up via `scripts/setup_worktree.sh`.
- Python env: symlinked `venv` → `../dc3-decomp/venv` (shared deps: `mcp`,
  `pyghidra-mcp`, etc.). Regenerate DB anytime with
  `venv/bin/python scripts/ingest_report.py build/45410914/report.json`.

**Ghidra MCP** (`tools/ghidra/pyghidra-service.sh`):
- Port **8002** (DC3 owns 8000, rb3-Wii owns 8001).
- Project at `ghidra_projects/RB3Xenon/RB3Xenon` (build via
  `tools/ghidra/import-xex.sh` — single-pass full analysis, no leaked .map).
- Uses VMX128 SLEIGH fork at `/home/free/code/milohax/ghidra/build/ghidra/`
  (same Ghidra build DC3 uses).
- Python client: `tools/ghidra/mcp_client.py` — default URL
  `http://127.0.0.1:8002/mcp`, session cache at
  `/tmp/claude/ghidra_mcp_session_rb3xenon.txt`.
- Sub-tools: `pcode_inspect.py`, `code_search.py`, `struct_check.py`,
  `ghidra-decompile.py`, `ghidra-search.py`, `ghidra-xrefs.py`,
  `ghidra-callgraph.py`, `batch_export.py`.

**Skills** (`.claude/skills/`, 24 total): batch-check, compare-asm, data-diff,
ghidra-{decompile,search,struct}, permute, progress, recon, refactor-staff,
resolve-vcall, stack-layout, struct-info, vtable, dc3-pair (primary engine
oracle — DC3 is the closest twin), rb3wii-pair (game-code oracle — richer
named-function source). All ported with port 8002 + title-ID 45410914
substitutions applied.

**Analysis engine** (`scripts/analysis/diff_inspect.py`, 1969 LOC): modes
`diagnose`, `clusters`, `regswaps`, `offsets`, `replaces`, `compare`,
`save_baseline`, `mismatches`, `stack-layout`, `asm_listing`. Backs the
`/compare-asm` + `/stack-layout` skills and the MCP `run_diff_inspect` tool.

**Struct + vtable** (`tools/struct_db.py`, `scripts/dump_vtable.py`):
`// 0xHEX` annotated headers → `struct_db.sqlite`; COFF `??_7*@@6B` vtables
decoded with `??_R4` RTTI Complete Object Locator parsing.

★ **Class layout: ask the compiler, not the comments.**
`scripts/harvest/class_layout_report.py <Class>` wraps
`cl.exe /d1reportSingleClassLayout<Class>` (an undocumented MSVC flag that works
through wibo) and prints real offsets, explicit padding rows, vtable slots with
the supplying class, and `this` adjustors. It is **authoritative**; the `// 0xHEX`
header comments — and `struct_db.sqlite`/`lookup_struct_offset`, which are
*derived from those comments* — are measurably wrong in places (`CharEyes.h`: 20
wrong offsets; `SaveLoadManager.h`: uniformly +4 stale). `lookup_struct_offset`
now consults the compiler by default (`verify=true`, `project_dir=<worktree>`) and
labels comment-derived answers **UNVERIFIED**. `--check-header` audits a header's
comments against the compiler; `--offset 0x118` answers "which member is here".

**MSVC pattern docs** (`docs/decomp/patterns/`, `docs/decomp/MSVC_X360_REGALLOC.md`,
`docs/decomp/TECHNICAL_NOTES.md`, `docs/decomp/PRAGMA_*.md`,
`docs/decomp/XBOX360_FLOATING_POINT_CODEGEN.md`): ported verbatim from DC3.
Same compiler, same flags → applies directly. (Verified 2026-08-04: our `base`
cflags and dc3-decomp's are byte-identical — `/nologo /wd4355 /wd4164 /c /GR /O1
/Oi /EHsc`, `config/45410914/config.json` vs `config/373307D9/config.json`. The
*targets* still differ: ours is retail with ICF, DC3's is a dev/debug build. So
the codegen **mechanism** transfers verbatim; a per-function **number** does not.)

★ **`MSVC_X360_REGALLOC.md`'s "declaration order controls assignment" is
corrected.** Declaration order controls **stack slots**; it is measured *inert*
for register-only swaps (12+ byte-identical hand variants across 4 functions, two
zero-gain beam sweeps). Registers follow **liveness** and **scheduling** — read
[`docs/decomp/patterns/fixable-liveness.md`](docs/decomp/patterns/fixable-liveness.md)
before opening a `REGISTER_SWAP` residual, and its Triage Split before choosing
*which* function to open at all.

### objdiff pattern-doc links resolve against THIS repo

objdiff-cli emits pattern-doc URLs relative to the **consuming** repo, detected by
marker filename in `docs/decomp/patterns/`. Because we carry DC3's filenames
(`PERMUTER_ROI_ANALYSIS.md`, `at-limit-systemic.md`), **we resolve as
`DocProject::Dc3`** — which is correct, we are the MSVC repo. `../rb3` carries
`permuter-roi.md` / `at-limit-mwcc.md` and resolves as `Rb3`. Override with
`OBJDIFF_DOC_PROJECT={dc3,rb3,unknown}` if you ever need to.

Two operational consequences:

- **Anchor stability is a contract.** objdiff renders only the **first** URL per
  pattern, so renaming a heading those links point at silently degrades tool
  output — no error, just a link that no longer lands. Verify any doc rename with
  `python3 ../objdiff/scripts/check_doc_links.py --dc3 . --allow-missing`
  (currently **30/30**; it was 27/30 before 2026-08-04, failing on
  `fixable-liveness.md` ×2 and `PERMUTER_ROI_ANALYSIS.md#instruction-scheduling`).
- **One binary, three repos.** `bin/objdiff-cli` here, in `../rb3` and in
  `../dc3-decomp` are all symlinks to the *same*
  `../objdiff/target/release/objdiff-cli`. A single
  `cargo build --release -p objdiff-cli` propagates to all three — and,
  conversely, **nothing propagates until someone rebuilds**. A doc/link fix
  committed in `../objdiff` is inert in every repo until that build runs.

## Phase tracking

Memory files at `~/.claude/projects/-home-free-code-milohax-rb3-xenon/memory/`:
- `project_rb3_xenon_roadmap.md` — overall phase tracking + current state.
- `project_function_identification.md` — the fingerprint-match approach.
- `project_native_port.md` — native host engine build (separate from matching).
- `project_jeff_fork.md` — local jeff dtk fork (RB3-retail fixes).
- `feedback_verify_assumptions.md` — verify load-bearing claims via Opus
  subagent before committing to them (killed the `.xidata`-lever plan
  pre-emptively this way).
- `feedback_plans_with_refs.md` — cross-session plans must embed
  doc/file/URL references for cold pickup.
- `feedback_autonomy.md` — execute autonomously on rb3-xenon.

Live plan files (per-stream, written by planning agents) live at
`~/.claude/plans/rb3-xenon-*.md`.
