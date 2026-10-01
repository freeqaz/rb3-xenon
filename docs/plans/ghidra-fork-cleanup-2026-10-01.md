# Ghidra fork + XEXLoaderWV cleanup — shared context (2026-10-01)

Goal: review our Xbox 360 Ghidra fork and the XEXLoaderWV loader extension,
fix bugs, clean up, sync with NSA upstream, and find Ghidra improvements that
help the hard rb3-xenon decomp tasks. This doc is shared context for several
parallel agents — read your assigned section plus "Ground rules."

## Ground rules (every lane)

- Repos: `/home/free/code/milohax/ghidra` (our fork; remotes `origin`=freeqaz,
  `nsa`=NationalSecurityAgency/ghidra, `0dinD`=original VMX128 author) and
  `/home/free/code/milohax/XEXLoaderWV` (loader extension; remote `origin`
  = upstream zeroKilo/XEXLoaderWV — we have no fork remote).
- Work in a `git worktree add ~/tmp/<name> -b <branch> <base>`. Never edit the
  main checkout's working tree, never `git stash`/`checkout -f`/`reset --hard`
  there. Never touch `build/ghidra`, `build/ghidra-dist`, or any running
  Ghidra/pyghidra-mcp process (one serves another live project on port 8003
  from `build/ghidra-dist`) — copy with `cp -r --reflink=auto` to `~/tmp` if
  you need to run/modify an install. Never open
  `rb3-xenon/ghidra_projects/*` in place — reflink-clone to `~/tmp` and delete
  `*.lock*` files in the clone first.
- Scratch, logs, worktrees all under `~/tmp` (NOT `/tmp` — tmpfs, small quota).
- `grep` typed directly in the Claude Bash tool is a `ugrep -I` shell function
  that silently misses matches in binary files — use `command grep -a` or
  Python when scanning binaries/objs.
- Commit freely on your branch with real messages. **No `Co-Authored-By` or
  any AI-attribution trailer, ever.** Do not push anything, anywhere.
- Land upstream merges with a real `git merge` (not rebase/squash) and a real
  merge message describing conflicts/resolutions.
- JDKs: `/usr/lib/jvm/java-21-openjdk` is JRE-only (no javac).
  `java-25-openjdk` and `java-26-openjdk` have javac — use one of those.
  System `gradle` is 9.7.1.
- When done: write a short report (what you measured, before/after numbers,
  what you deliberately didn't do) and say what branch + worktree path your
  work is on. Don't delete your worktree.

## Facts already measured (verify before relying on them further)

- `ghidra` fork `master` last merged `nsa/master` at `430465776d`
  (2026-06-08); `nsa/master` is now ~966 commits ahead. Fork-only commits on
  `master`: VMX128 SLEIGH series (2026-01-31), MSVC PowerPC switch-table fix
  (`f817a3716b`, `8b7cf690e4` — upstream PR #8964, still OPEN), a Gekko/Broadway
  PowerPC variant (`1566260f1e`), VT/BSim perf patches, `9434f1c110`
  ("inject assumed switch index"). Branch `bsim-xenon-patches` = master + 2
  more BSim commits.
- Deployed install `build/ghidra` is older than `master` tip (missing the
  last 2 BSim commits). Deployed `XEXLoaderWV` extension zip lives at
  `build/ghidra/Ghidra/Extensions/XEXLoaderWV`.
- Xenon language: `PowerPC:BE:64:Xenon` in
  `Ghidra/Processors/PowerPC/data/languages/ppc.ldefs`, slaspec
  `ppc_64_xenon.slaspec`, includes `vmx128.sinc` (2,734 lines, "full pcode
  semantics for all 77 VMX128 opcodes", commit `56ec2c4bcf`). Uses cspec
  `ppc_64_32.cspec` — the **generic** PPC cspec, shared with every other PPC
  variant; it has no MSVC/Xenon-specific call-fixups.
- Oracle for correct disassembly: MSVC's own disassembler works through wibo:
  `/home/free/code/milohax/wibo/build/release/wibo
  /home/free/code/milohax/rb3-xenon/build/compilers/X360/16.00.10224.00/link.exe
  /dump /disasm <file.obj>` on any `.obj` under
  `build/45410914/src/**/*.obj` (rb3-xenon) or dc3-decomp's build dir —
  confirmed working, prints real MSVC mnemonics including VMX128 ops.
- MSVC X360 prologues/epilogues almost always call shared helpers
  `__savegprlr_14..31` / `__restgprlr_14..31` / `__savefpr_14..31` /
  `__restfpr_14..31` (and VMX `__savevmx_*`/`__restvmx_*`) via `bl`/tail
  branch. Ghidra has no call-fixup for these on PowerPC, so the decompiler
  treats them as opaque calls clobbering/using registers — likely a real
  source of decompile noise on almost every non-leaf RB3 function.
- rb3-xenon's Ghidra setup: `tools/ghidra/pyghidra-service.sh` (port 8002),
  named-symbol bank described in memory file
  `project_ghidra_tu5_bank_2026-07-15.md`, target program
  `default_tu5.xex-c5a170` loaded from `orig/45410914/default_tu5.xex`.
  `scripts/target_symbol_map.json` has ~17k real mangled names you can use
  as a labeled-function corpus for decompile-quality testing.
- XEXLoaderWV: our one commit `56bf704` (Ghidra 12.2 build fix + prefer Xenon
  LoadSpec) is 11 commits behind `origin/master`
  (PDB/TPI import fixes, `.pdata` function-creation + LF_ARRAY sizing fix,
  log-handling change, build-prop fixes).

## Lane A — SYNC (upstream merge)

Merge NSA's 966 commits into our fork without losing any fork-only commit or
regressing VMX128/Xenon. Watch for conflicts in the PowerPC `.sinc`/`.ldefs`
files (upstream added PowerPC v3.0 instructions in `720b1a5f91`, which may
overlap VMX128 encodings) and in `PowerPCAddressAnalyzer.java`/VT files.
Build the merged result (don't touch the live install — stage elsewhere) and
show the PowerPC SLEIGH compiles clean and `PowerPCMSVCSwitchTest` still
passes. Report what's left for the actual install swap (don't do the swap).

## Lane B — SLEIGH (VMX128/Xenon correctness)

Audit `vmx128.sinc` + `altivec.sinc` disassembly and p-code semantics against
the MSVC-disassembler oracle above, on real VMX128-bearing `.obj` files from
rb3-xenon/dc3-decomp. Look for wrong mnemonics, wrong register-field
decoding, wrong immediates, and p-code semantics that disagree with the ISA
for ops that actually occur in RB3 (vperm128, vmaddfp128, vpkd3d128,
vrlimi128, lvlx/lvrx family, etc). Fix what you can prove wrong; show
before/after agreement numbers against the oracle.

## Lane C — DECOMP-QUALITY

Add MSVC X360 call-fixups for the save/restore helpers listed above (new
compiler spec or callfixup injection, keeping the existing default working).
Baseline decompile output on ~20-30 named RB3 functions (use
`scripts/target_symbol_map.json` for names) before and after, and report the
noise reduction. If time remains, look at unresolved `bctr` switch targets
and EH-funclet handling as secondary targets — only pursue what you can show
measurably helps.

## Lane D — XEX (loader review + upstream merge)

Merge XEXLoaderWV's 11 upstream commits (real merge, not squash) and review
the loader for bugs against actual XEX bytes (compare `.pdata`-derived
function count, import thunk naming, memory permissions) on
`orig/45410914/default_tu5.xex` and the TU0 archive. Fix what you find, build
the extension against a copied (not live) Ghidra install, and report
old-vs-new load differences.
