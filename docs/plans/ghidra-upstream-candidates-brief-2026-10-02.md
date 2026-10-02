# Brief: which Ghidra-fork / XEXLoaderWV fixes should be upstreamed? (2026-10-02)

Read-only audit. Goal: a ranked list of fixes we made that NSA Ghidra
(https://github.com/NationalSecurityAgency/ghidra) or zeroKilo/XEXLoaderWV
upstream would plausibly accept, with for each: the commit, the bug in one
sentence, the evidence that it is a real bug (not an Xbox-360 preference), and
whether a test exists. Do not open PRs, do not push, do not edit code. Write
the result to `docs/plans/ghidra-upstream-candidates-2026-10-02.md` in
rb3-xenon.

## Where the work is (all in ~/tmp worktrees; `master` = our fork base)

| lane | worktree | branch | what |
|---|---|---|---|
| A | `~/tmp/ghidra-sync` | `sync-nsa-2026-10-01` | NSA merge + `0f0647307c` fixing five SLEIGH encoding bugs in upstream's own GP-6914 PowerPC v3.0 additions (`msgclru` opcode 10→110 clobbered `addc`; `mffscdrni`, `mffsl`, `urfid`, `mtfsfi`) |
| B | `~/tmp/ghidra-sleigh` | `xenon-sleigh-audit` | VMX128 audit; commits `eb63c445b4`, `7946c08d80` (+ possibly more by the time you read this — agent still finishing) |
| C | `~/tmp/ghidra-laneC` | `xenon-msvc-callfixups` | `b8c5d33b8f`: Xenon-MSVC cspec with 236 save/restore call-fixups + analyzer |
| D | `~/tmp/xexloader-laneD` | `xex-laneD-upstream-merge` | XEXLoaderWV: commits `613f2fb`, `5d0188c`, `08ab440`, `0fa5154` — image base 0, `.reloc` zero-fill, import-thunk patch never reaching memory, `.pdata` bodies, PDB section mapping, export names |
| older | fork `master` | — | MSVC PowerPC switch-table fix `8b7cf690e4` — already upstream PR #8964 (open since 2026-02-12); `9434f1c110` "inject assumed switch index" came after it |

Also the Gekko/Broadway language variant `1566260f1e` and the VT/BSim perf
commits on `master` — assess those too, but the four lanes are the priority.

## How to judge "upstreamable"

- A fix to an encoding/semantics bug that is wrong for *every* PowerPC
  (e.g. lane A's five) is a straightforward upstream bug report + patch.
- Something that is only correct for Xenon/MSVC (e.g. a cspec default
  switch, or dropping ISA 2.07/3.0 ops from the Xenon language) may still be
  upstreamable as a Xenon-variant-only change, but say so and note what
  upstream would want (a language version bump, tests, no default changes to
  other variants).
- XEXLoaderWV is a small third-party repo; its maintainer takes PRs readily
  (see its history). Lane D's loader fixes are likely all candidates.
- For each candidate check: does upstream's current `master` already have an
  equivalent fix? (fetch is done; `nsa/master` is at `a462673d2f`.) Does a
  GitHub issue already exist? (`gh issue list -R NationalSecurityAgency/ghidra
  --search "<term>"` works here.) Is there a test, and does it fail without
  the fix?

## Rules

Read-only on all repos. Scratch under `~/tmp`. `grep` typed in this shell
skips binary files — use `command grep -a` if you need to scan one. Note the
standing user rule: this project does not open upstream PRs itself by
default; your output is the candidate list and the evidence, the user decides.
