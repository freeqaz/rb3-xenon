# Ghidra fork / XEXLoaderWV: upstream candidates, ranked (2026-10-02)

Answers `ghidra-upstream-candidates-brief-2026-10-02.md`. Read-only audit. No PRs
opened, nothing pushed, no lane worktree edited. All scratch work is in
`~/tmp/ghidra-upstream-audit/`: binutils `ppc-opc.c`, and a throwaway XEXLoaderWV
clone used for the cherry-pick and compile test. **The user decides what, if
anything, goes upstream.** This doc is the candidate list and the evidence for each.

Refs as audited: `nsa/master` = `a462673d2f`. GitHub's live upstream `master` was
re-queried today and is still `a462673d2f`, so "upstream" below means current HEAD.
zeroKilo/XEXLoaderWV `master` = `4bcf58d`, also confirmed live. Lane B's branch
moved while I was reading (`18df784ff9` landed at 07:57). This doc covers its tip,
`18df784ff9`.

## Ranked list

| # | what | commit(s) | target | verdict |
|---|---|---|---|---|
| 1 | Five SLEIGH encoding bugs (`msgclru` steals `addc`, `mtfsfi` fields, `mffsl`, `mffscdrni`, `urfid`) | `0f0647307c` (lane A) | NSA Ghidra | **Submit.** Bugs in all PowerPC languages or all ISA PowerPC languages; shipped in 12.1.3/12.1.4; still on upstream HEAD; no issue filed |
| 2 | Fix for our own open PR #8964 (its analyzer test has never passed) | `9434f1c110` | NSA PR #8964 (push to the PR branch) | **Submit.** The open PR is broken as filed |
| 3 | XEX loader: image base, truncated `.reloc`, thunk patch that never reached memory, exact `.pdata` bodies | `613f2fb` + `0fa5154` (lane D) | XEXLoaderWV | **Submit** as one PR, or split by fix (see below) |
| 4 | Missing kernel/XAM export names (xenia supplement) | `08ab440` (lane D) | XEXLoaderWV | **Submit** after adding xenia's BSD-3 attribution |
| 5 | PDB `S_PUB32` offsets resolved against the wrong section | `5d0188c` (lane D) | XEXLoaderWV | Real bug by code reading, but **never run**. Hold until exercised on a real PDB |
| 6 | `VTAssociationDB` O(n²) hash/equals | `df874cfe14` | NSA Ghidra | Real performance bug. **Fix the equals/hashCode contract hole first**; no measurement or test yet |
| 7 | Parallel and memory-bounded reference correlator | `0963a5e934` + `611a8d1dc5` | NSA Ghidra | Plausible. Needs benchmark numbers and an output-identity test |
| 8 | Xenon VMX128 SLEIGH audit | `eb63c445b4`, `7946c08d80`, `18df784ff9` (lane B), with `56ec2c4bcf` | 0dinD's `vmx128` branch / issue #2094, **not** a standalone NSA PR | Xenon-only. Upstream has no Xenon language to patch |
| 9 | Xenon MSVC cspec with 236 save/restore call-fixups and an analyzer | `b8c5d33b8f` (lane C) | Rides with #8 | Xenon-only. Needs a test, and a review of the 7.3k-line generated file |
| 10 | Gekko/Broadway language | `1566260f1e` | NSA Ghidra | Low. Arithmetic is 28 opaque pcodeops, and a community extension already exists |
| 11 | BSim top-K cap, parallel Phase-B | `1220f13915`, `eebbd8ba3a` (`bsim-xenon-patches`, not `master`) | — | **Do not submit.** Changes behaviour, and serves a pipeline we measured as NO-GO |

Not candidates:
- `8291a64383` (`vadduws`) is already upstream as `c1f97cde08` (verified: byte-identical
  patch, 0dinD's PR #8821, merged via GP-6268). Our copy can drop on the next sync.
- `0d935a2639` (README).
- `f817a3716b` is the pre-PR version of #8964 plus fork-only scripts
  (`ApplyMapSymbols`, `RecoverMSVCSwitchTables`).
- XEXLoaderWV `56bf704` prefers the Xenon language, which stock Ghidra does not have.

⚠ **Trailer hazard.** `9434f1c110` and `1566260f1e` carry
`Co-Authored-By: Claude …` trailers. The standing user rule forbids that on any new
commit or PR. Anything submitted from these must be re-committed with a clean message.
Existing fork history stays as is (09-14 directive).

---

## 1. `0f0647307c`: five encoding bugs in upstream GP-6914 (lane A)

**Bugs.** GP-6914 (`720b1a5f91`, 2026-06-30, first released in Ghidra 12.1.3 and 12.1.4)
added PowerPC v3.0 instructions with five wrong encodings:
- `msgclru` has XO=10 instead of 110, so it shadows `addc rD,rA,rB` whenever rD=rA=0.
- `mffscdrni` has bits 16-20 = 20 instead of 21, so it steals `mffscdrn`. Its reserved
  field is also too wide.
- `mffsl` puts its 24 in the wrong 5-bit field.
- XO 306 is spelled `rfid` instead of `urfid`.
- `mtfsfi`'s U field is at bits 11-14 instead of 12-15, and W is read from bit 11
  instead of bit 16.

**Evidence that these are real bugs, from two implementations other than Ghidra:**
- **LLVM 22 `llvm-mc -mcpu=pwr10`**, decoded field by field:
  - `mffsl 1` = `fc38048e` (b16-20=24, b11-15=0).
  - `mffscdrni 1,7` = `fc353c8e` (b16-20=21, and b13 is set, so the old `BITS_13_15=0`
    rejects DRM ≥ 4).
  - `mtfsfi 6,13` = `ff00d10c` (b12-15=13); `mtfsfi 6,13,1` = `ff01d10c` (W at bit 16).
  - `addc 0,0,11` = `7c005814` (XO 10, b16-25=0, which is exactly the word `msgclru`
    with XO=10 matches).
- **binutils `opcodes/ppc-opc.c`** (sourceware HEAD, fetched today):
  - `msgclru XRTRA(31,110,0,0)`
  - `urfid XL(19,306)` versus `rfid XL(19,18)`
  - the `mtfsfi`, `mffscdrni` and `mffsl` masks agree with the fix.
- **Ghidra's own decode, from lane A's sweep** (`~/tmp/ghidra-sync-verify/cmp_synth*.txt`):
  - Before the fix, `7c000014 'addc r0,r0,r0' -> 'msgclru r0'` on 32 words.
  - `fc00110c` printed `mtfsfi 0x0,0x2`; after the fix it prints `0x1`, the binutils value.
  - ⚠ That sweep ran on the fork's **Xenon** language. Upstream reviewers will want it
    re-run on an upstream language (`PowerPC:BE:64:A2ALT-32addr`).

**Reach.**
- The `msgclru` collision is in `ppc_isa.sinc`, which is included by the six
  `ppc_64_isa_*` languages (A2 / A2ALT, BE and LE, and the VLE variants).
- **Stock XEXLoaderWV imports every XEX as `PowerPC:BE:64:A2ALT-32addr`**, so every
  XEXLoaderWV user on Ghidra 12.1.3+ is exposed.
- `mtfsfi` is in `ppc_instructions.sinc`, which **every** PowerPC language includes. The
  pre-GP-6914 form (`IMM=(11,15)`) was also wrong, and it has been wrong since the
  2019 public release (`79d8f164f8`). Its field `IMM4` is used only by `mtfsfi`, so
  changing the token is safe.
- **Honest scope:** RB3 retail `.text` has **0** `addc r0,r0,rB` words (22 `addc`
  total) and **0** `mtfsfi` words. Lane A measured 0 instruction-text differences on
  RB3 before and after. The fix does nothing for *our* binary; its value is to other
  PowerPC users.

**Already fixed upstream?** No. All five lines are verbatim on live upstream `master`
(fetched via the GitHub API). No commit has touched PowerPC languages since GP-6914.
**Issue?** None. Searches for msgclru, mtfsfi, mffsl, urfid and GP-6914 all return 0.

**Test?** None in-tree. Upstream PowerPC has no disassembly-vector harness, only
pcodetest emulator tests under `src/test.processors`. The PR should carry the vector
table (LLVM/binutils word → expected text) in its description.

**Readiness.** A 10-line diff across three `.sinc` files. It merges onto upstream because
the commit sits directly on the `nsa/master` merge. Precedent for acceptance is strong: upstream has
merged 0dinD's one-line `vadduws` PR (reviewed by GhidorahRex), the LQ/STQ fixes, `se_blrl`, and the
`bd*f*` condition fix, all external PowerPC SLEIGH PRs from the last year. A reviewer
may prefer an issue citing GP-6914 plus the PR. Either way it is cheap.

## 2. `9434f1c110`: our PR #8964 ships a test that has never passed

**Bug.** `recoverSwitches()` feeds assumed index values through the symbolic
propagator. For the MSVC pattern, the register's first read is the compare itself,
which happens before `hitTheGuard` is set. So the lazy `unknownValue()` returns null and
**no switch targets are ever recovered**. The fix sets the guard register to the assumed
value at the compare.

**Evidence.**
- PR #8964's head is `8b7cf690e4` **alone**, and `9434f1c110` is not on the PR branch
  (verified with `gh pr view`).
- Lane A's negative control (`~/tmp/ghidra-sync-switchtest-negctl.log`): without the fix,
  `5 tests completed, 1 failed`, with `testAnalyzerRecoversSwitchTargets` failing at
  `PowerPCMSVCSwitchTest.java:345`. With the fix, 5/5 pass, both on the merged tree and
  on the fork tip.
- ⇒ **A test exists and fails without the fix.** This is the strongest test evidence of
  any candidate.

**Upstream state.** PR #8964 is OPEN in `Status: Triage`, assigned `emteere`, with 0
reviews and 0 comments since 2026-02-12. Issue #8963 is open. The PR head still merges
cleanly into `nsa/master`, and `9434f1c110` applies cleanly on the PR head (both checked
with `git merge-tree`, read-only).

**Action shape.** Re-commit `9434f1c110` **without its Claude trailer** onto the
`powerpc-msvc-switch-fix` branch, then push to the PR.

⚠ Do not overstate the PR's coverage. The PR and its test know only the **`lhzx`**
(halfword) table form. On RB3, `lbzx` byte tables are the majority: 52 `lbzx` vs 21
`lhzx` sites, per `ghidra-improvement-plan-2026-10-01.md` §0. A reviewer running real
MSVC X360 code will hit the byte form. That is a follow-up commit (plan item P4), not a
blocker for #2.

## 3. `613f2fb` + `0fa5154`: XEX loader fixes (lane D)

The four fixes I could check by reading are **confirmed on pristine upstream `4bcf58d`**:

| fix | upstream code | confirmed |
|---|---|---|
| image base never set | no `setImageBase` anywhere in the tree (`git grep`) | yes |
| section past the image end is zero-filled entirely | `if (sec.VirtualAddress + size <= peImage.length)` copy, otherwise the buffer stays zero | yes (lane measured: 510,640 real nonzero `.reloc` bytes dropped) |
| import-thunk `li r3/li r4` patch never reaches memory | `ProcessImportLibraries` writes `peImage[pos]=0x38…`, but the loader calls it **after** `ProcessPEImage` has copied bytes into memory blocks (`XEXLoaderWVLoader.java:85-86`) | yes (lane measured: 0/287 thunks decodable before, 287/287 after) |
| `.pdata` body extent | upstream `13962de` uses the flow-follow body | lane: 961/57,733 bodies disagreed with `.pdata` |

The two commits must go **together**. `0fa5154` is the measured correction of
`613f2fb`'s `.pdata` change: restricting load-time disassembly lost about 70k
instructions of frameless leaves that have no `.pdata` entry. The combined result is a
strict instruction superset of upstream (2,542,163 vs 2,541,008).

Also in `613f2fb` and unverified by me: MZ/PE validation before touching the program, a
combined error message, and a null guard in `ResolveImportRecords`.

**Applies upstream?** Yes. In a throwaway clone, all four lane-D commits cherry-pick
cleanly onto `4bcf58d` with no dependency on our `56bf704`, and the result **compiles**
(`gradle compileJava`, rc=0) against a Ghidra 12.1.2 install.
⚠ That install is our fork's 12.1.2 build, which has the Xenon files. The loader uses no
language API, so the compile is still the relevant check.

**Issues.** Related to closed issue #26 ("Function_(address) not defined", fixed by
PR #33). No open issue covers the thunk, image-base or `.reloc` bugs.

**Test?** None. Upstream has only `src/test/java/README.test.txt`. Byte-level evidence
lives in the commit message (every byte equals the dtk decode except the 1,148 thunk
bytes).

**Suggestion.** `613f2fb` bundles six changes. The maintainer merges PRs readily (#29,
#33 and #37 were all multi-change), so one PR is acceptable, but splitting
thunk / image-base / section-truncation / pdata would make review trivial.

## 4. `08ab440`: export-name supplement (lane D)

**Bug.** ImportRenamer's xboxkrnl/xam/xbdm tables are a strict subset of xenia's (63,
342 and 2 ordinals missing; 0 ours-only), so some imports render as
`<module>_ord_<n>`. Measured on DC3: 357/360 → 360/360 IAT names. RB3 is unchanged.

**Precedent.** Issue #18 ("ord_ instead of function name") was fixed by the maintainer
adding names, so this is the same kind of change.

**Before submitting:** the file cites xenia's `*_table.inc` but carries **no xenia
copyright/BSD-3 notice**, and XEXLoaderWV has no LICENSE file of its own. Add the
attribution. The maintainer may also prefer the names folded into `ImportRenamer`'s
tables over a separate class. It applies cleanly; no test.

## 5. `5d0188c`: PDB `S_PUB32` section (lane D)

**Bug, confirmed by reading upstream `ProcessAdditionalPDB`.** The code adds the
`.text` VA to every public symbol regardless of its `seg` field, so symbols in a second
code section (e.g. `BINK`) land at the wrong address.

**But** the commit itself says *"Compiles; NOT exercised — no Xbox 360 PDB is available
on this box."* Do not submit an unexercised loader change. Hold it until someone with a
PDB/XDB can run it; open issue #27 ("Error importing file when using PDB/XDB") may have
a reporter willing to test.

## 6. `df874cfe14`: `VTAssociationDB` equals/hashCode

**Bug.**
- `hashCode()` sums two `Address` hashes. Each decode goes through the synchronized
  `AddressMapDB`.
- The offset-sum collides across byte-identical thunk groups, so a `HashSet` in
  `AutoVersionTrackingTask.getAllRelatedAssociations` degrades to O(n²) locked decodes
  ("hangs" vs "runs" at 65k functions, `ghidra-tu0-tu5-crossport.md:233`).
- Upstream `hashCode` is unchanged on HEAD, and the commit merges cleanly.

⚠ **Contract hole a reviewer will catch.** `equals` falls back to address comparison
for a `VTAssociationDB` from a *different* session (a different
`associationDBM`). Two such objects with equal addresses are therefore `equals`, but
they now hash by **different record keys**, which violates `Object.hashCode`.
`AssociationStub`, the only other implementation, uses identity equality, so the stub
case is unaffected. Fix before submitting: either hash on the cached (src, dst) offsets
(lock-free once cached), or make cross-session instances unequal.

**Missing:** a before/after number (only qualitative evidence exists) and a test.

## 7. `0963a5e934` + `611a8d1dc5`: parallel, memory-bounded reference correlator

The change parallelizes the O(dest × src) cosine scoring in
`VTAbstractReferenceProgramCorrelator`, with a serial, sorted, deterministic commit.
`611a8d1dc5` then fixes the OOM the first commit introduced under the default 2 GB heap.
Submit the two as one change.

- The race-freedom argument (pre-finalize every `LSHCosineVectorAccum` on a single
  thread) is spelled out in the commit message.
- **Output order changes:** previously HashMap iteration order, now address order. The
  claim "output identical" is true only up to that order.
- Upstream changed this file only trivially since our base (3 lines, GP-7104), and the
  pair merges cleanly.

**Missing:** wall-clock and peak-memory numbers on a public program pair, and a test
asserting the same match set as the serial path.

## 8. Lane B: VMX128 audit (`eb63c445b4`, `7946c08d80`, `18df784ff9`)

These are real, well-measured fixes, but **to code upstream does not have**.
`vmx128.sinc` and `ppc_64_xenon.slaspec` exist only in the fork. They descend from
PJ Oberoi (2022) and Odin Dahlström / `0dinD` (Dec 2025), whose `0dinD/ghidra@vmx128`
is exactly our base. There is **no upstream PR** for Xenon; the venue is open issue
#2094 ("[Request] Support for PowerPC VMX128/Xenon", 0dinD active there 2025-12).

| commit | measured (oracle: MSVC `link /dump /disasm` 16.00.10224) |
|---|---|
| `eb63c445b4`: guard ISA 2.07/3.0 vector ops, `maddhd*` and `brinc` behind `NO_ALTIVEC_207_300` | RB3 vector words 67.81% → 90.80% weighted agreement |
| `7946c08d80`: share the AltiVec register file (vr0-31 were separate storage overlapping `ACC`); field fixes; `(rA\|0)` addressing; real p-code | 90.80% → 100.00% (3,653/3,653); 362/362 new known-answer tests |
| `18df784ff9`: ignore vA on unary ops; vupk halfword/byte select | synthetic op-4/5/6 corpus 93.23% → 97.43%; RB3 unchanged at 100% |

- `eb63c445b4` touches the shared `altivec.sinc` and `ppc_isa.sinc`, but only adds
  `@ifndef` guards that just the Xenon slaspec defines, so other variants are untouched.
  The same mechanism as the fork's `NO_STVEPX`, which is also fork-only.
- **Tests are out of tree** (`~/tmp/sleigh-audit/emu_tests.py`, `gen_tests.py`,
  `compare.py`). An upstream contribution would need them as a JUnit test in the style
  of `PpcPcodeUseropLibraryTest`.
- **Recommended route:** offer the series (plus our `56ec2c4bcf` p-code semantics) to
  0dinD's branch, or post the measurements on #2094. A full Xenon-language PR to NSA is
  a much larger ask: a new language, `ldefs` and manifest entries, and tests. That is the
  user's call.

## 9. Lane C: `b8c5d33b8f`, Xenon MSVC cspec and call-fixups

Upstream PowerPC cspecs model **no** register save/restore millicode at all; the only
call-fixup is GCC's `get_pc_thunk_lr`. So the *idea* generalizes: GCC's ELF
`_savegpr0_N` / `_restgpr0_N` helpers are the same shape. **This commit does not**,
though: the helper names and the r12-based FPR/VMX entry points are MSVC X360, it binds
to the Xenon language's `default` compiler, and it is only meaningful alongside #8.

Notes for if it ever goes:
- The cspec is a **7,297-line generated file**. Ship the generator
  (`gen_ppc_64_xenon_msvc_cspec.py`, included) and expect reviewer pushback on size.
- The fixups **omit the 16-byte alignment mask of `stvx`/`lvx`**, a semantic shortcut a
  reviewer will ask about.
- **No test** for `PowerPCMsvcSaveRestoreAnalyzer`.
- The decompile A/B in the commit message is strong (386 → 0 functions losing `this` to
  a helper "return value", over 400 functions).

## 10. `1566260f1e`: Gekko/Broadway

- 62 constructors.
- Paired-single arithmetic is implemented as **28 opaque pcodeops**, not real semantics.
  There is also no GQR or dequantization modelling, so decompiler output on PS math is
  `ps_add_op(...)`.
- It touches the shared `ppc_common.sinc` (adds PSQ token fields; harmless to other
  variants).
- A well-known third-party Gekko/Broadway extension exists, and upstream has no request
  open for it (search returned 0).
- Carries a Claude trailer, and has no tests.
- Upstream would most likely want full p-code before taking a new language. Low priority.

## 11. BSim `1220f13915` / `eebbd8ba3a` (branch `bsim-xenon-patches`)

**Not recommended.**
- `1220f13915` changes results: it raises `SAFETY_MAX_LOOKUP_CANDIDATES` from 500 to
  10,000 and adds a top-K cap, so it is not a pure performance change.
- Both serve the VT-BSim seed propagation the campaign measured **NO-GO** (2026-06-21).
- Neither is on fork `master`.

---

## What I did not do

- No `git fetch`, push, PR, issue or comment on any repo.
- No edit in any lane worktree. Lane B committed `18df784ff9` during the audit, not me.
- Did not rebuild Ghidra or re-run lane A's sweep or the switch tests. Both test results
  above are **lane A's logs**, read, not reproduced.
- Did not exercise `5d0188c` (no PDB available).
- Did not measure the VT changes' speedup.
- Did not check whether `56bf704`'s Xenon-preferred `LoadSpec` degrades gracefully on a
  stock Ghidra that lacks the language.
