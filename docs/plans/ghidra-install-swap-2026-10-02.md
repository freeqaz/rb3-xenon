# Ghidra install swap plan — 12.2 (0963a5e9) → 12.3 integration (2026-10-02)

What is being swapped in: the `xenon-integration-2026-10` branch of
`/home/free/code/milohax/ghidra` = fork `master` + NSA upstream (1,003
commits, lane A) + lane A's five SLEIGH encoding fixes + lane B's VMX128 audit
(3 commits) + lane C's Xenon-MSVC cspec/analyzer, plus the lane D
XEXLoaderWV extension rebuilt for 12.3.

## Preconditions (each must be true, with the evidence file)

| check | status | evidence |
|---|---|---|
| PowerPC module + all 20 SLEIGH languages compile on the integration tree | ✅ rc=0 | `~/tmp/integ-ppc-build.log` |
| full `buildGhidra` on the integration tree | ✅ BUILD SUCCESSFUL 7m16s, 0 errors → `~/tmp/ghidra-integ/build/dist/ghidra_12.3_DEV_20261002.zip` (564 MB) | `~/tmp/integ-buildGhidra.log` |
| `PowerPCMSVCSwitchTest` passes on the integration tree (read the XML, not BUILD SUCCESSFUL — the test task has `ignoreFailures=true`; it runs under `:PowerPC:integrationTest`, not `:test`) | ✅ 5/5, 0 failures, 0 errors | `~/tmp/integ-switchtest.log` + `Ghidra/Processors/PowerPC/build/test-results/` |
| XEXLoaderWV builds against 12.3 | ✅ rc=0, 4 warnings | `~/tmp/xexloader-laneD/XEXLoaderWV/dist/ghidra_12.3_DEV_20261002_XEXLoaderWV.zip` |
| pyghidra-mcp venv (pyghidra 3.0.2) starts the 12.3 install | ✅ `started 3.0.2 against 12.3` | `~/tmp/pyg-302-vs-123.log` |
| pyghidra 3.3.0 (shipped with 12.3) still exports the private symbols pyghidra-mcp imports | ✅ | `_PyGhidraStdOut`, `_run_mac_app` present |
| JAVA_HOME resolution follows the install | ✅ | rb3-xenon `077aaa5df` |
| RB3 TU5 disassembly A/B old vs new on the final integration build | ✅ 2,585,487 words: **1,886 text diffs, all 1,886 vector-class (0 non-vector)**, 1,395 p-code-only (VMX128 semantics + 10 FPSCR); vs lane A's merged-only build the text delta is identical (1,886) ⇒ the whole delta is lane B's declared fix | `~/tmp/ghidra-sync-verify/cmp_integ.txt`, `cmp_integ_vs_stage.txt`; ⚠ import with `-loader-baseAddr 0x82270000` — at base 0 every PC-relative branch reads as a diff (450,248, a harness artifact) |

## Consumers of the live install (all must be restarted / re-pointed)

- `build/ghidra` symlink → `ghidra-dist/ghidra_12.2_DEV`. Used by rb3-xenon
  `tools/ghidra/pyghidra-service.sh` (port 8002), dc3-decomp's equivalent
  (port 8000), rb3's (8001), and xex-patcher's (8003, runs from
  `ghidra-dist/ghidra_12.2_DEV` directly). After the 2026-10-02 reboot none
  of 8000–8003 is running, so there is no live-service constraint right now.
- Projects: `rb3-xenon/ghidra_projects/RB3Xenon` (1.3 GB), dc3-decomp's,
  rb3's, xex-patcher's. **Opening a project in 12.3 upgrades its database
  one-way.** Back each up (reflink copy) before the first open.
- Extensions installed in the 12.2 tree: `XEXLoaderWV`, `ghidra-xbe`;
  user-level `~/.config/ghidra/ghidra_12.2_DEV/Extensions/BinExport`. 12.3
  uses `~/.config/ghidra/ghidra_12.3_DEV/` — BinExport and ghidra-xbe need
  reinstalling/rebuilding there (BinExport 10.3.3 zip exists in the 12.2
  user dir; ghidra-xbe source location TBD).

## Steps

```bash
# 0. nothing listening on 8000-8003 (verify, don't assume)
ss -ltnp | command grep -E ':800[0-3]'

# 1. back up every project that will be opened in 12.3 (reflink = seconds)
for p in rb3-xenon/ghidra_projects/RB3Xenon dc3-decomp/ghidra_projects/* ; do
  cp -r --reflink=auto ~/code/milohax/$p ~/tmp/ghidra-project-backup-20261002/$p
done

# 2. unpack the integration build next to the old one, never over it
unzip -q ~/tmp/ghidra-integ/build/dist/ghidra_12.3_DEV_*.zip -d ~/code/milohax/ghidra/build/ghidra-dist/
#    -> build/ghidra-dist/ghidra_12.3_DEV

# 3. extensions into the NEW tree
unzip -q ~/tmp/xexloader-laneD/XEXLoaderWV/dist/ghidra_12.3_DEV_20261002_XEXLoaderWV.zip \
      -d ~/code/milohax/ghidra/build/ghidra-dist/ghidra_12.3_DEV/Ghidra/Extensions/
#    ghidra-xbe: rebuild for 12.3 (same gradle recipe) or leave out until needed
#    BinExport: install into ~/.config/ghidra/ghidra_12.3_DEV/Extensions/ (one copy only)

# 4. atomic symlink flip (rollback = the reverse ln)
cd ~/code/milohax/ghidra/build && ln -sfn ghidra-dist/ghidra_12.3_DEV ghidra.new && mv -T ghidra.new ghidra
readlink ghidra   # expect ghidra-dist/ghidra_12.3_DEV

# 5. first open of the RB3 project on 12.3: headless, then
#    run the "PowerPC MSVC Save/Restore Helpers" analyzer once (lane C) and
#    re-analyse so register references pick up lane B's moved VMX128 storage.
#    Then re-sync names (name bank is ~45% stale anyway — plan P1):
tools/ghidra/run_apply_symbols.sh --full

# 6. restart services; confirm each logs "Opening existing program" with no duplicate program
tools/ghidra/pyghidra-service.sh restart && tools/ghidra/pyghidra-service.sh status
```

Rollback: `cd ~/code/milohax/ghidra/build && ln -sfn ghidra-dist/ghidra_12.2_DEV ghidra.new && mv -T ghidra.new ghidra`,
restore the project backups if any were opened in 12.3, restart services.
The 12.2 tree is never modified by these steps.

## Known gaps carried into the swap (deliberate)

- No `ppc.ldefs` language version bump for lane B's register-storage move.
  Mitigated by re-analysing the RB3 project after the swap (step 5).
- xex-patcher's 8003 service points at `ghidra-dist/ghidra_12.2_DEV`
  directly, not the symlink; it stays on 12.2 until its owner re-points it.
- `application.revision.ghidra-sync` key naming (lane A built from a dir
  named `ghidra-sync`); the integration build ran from `ghidra-integ` so the
  key is `application.revision.ghidra-integ`. Nothing we run reads it.
