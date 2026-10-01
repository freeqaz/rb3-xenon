# Campaign state + roadmap — 2026-10-01

Fourth edition of the single current-state doc. Supersedes
`CAMPAIGN_STATE_2026-08-17.md` for *numbers and priorities*; that doc's
methodology sections and the partition provenance stay valid as records.

## Measured (full `./tools/ninja-locked`, main `a95d5c525`, 2026-10-01)

Ruler `name_check` (objdiff 4.2.9, tool commit `a5f0ea903ec1`), read from
`build/45410914/report.json`, ceiling from
`python3 tools/ceiling_recompute.py build/45410914/report.json objdiff.json . "main a95d5c525 (2026-10-01)"`.

```
total_code            10,247,072
PAIRABLE               6,484,424 = 63.281%   (1,053 units, 54,502 rows)
− scaffold shells        178,664             (103 units, 906 rows; cliff: thr≤5 = 0 units, ≤6 = 103)
= reachable ceiling    6,305,760 = 61.537%
matched_code           4,698,184 = 45.849% of total_code = 74.51% of ceiling
gap to ceiling         1,607,576
matched_functions         46,539 / 69,131   masked_equal 23,699   honest 22,840
fuzzy_match_percent       54.918
```

Dashboard tiers (pinned denominator, aon% = bytes at fuzzy==100):
game **75.96%** (18,252 / 20,777 fns, 1.73 / 2.28 MB) · engine **69.93%**
(28,055 / 33,446, 2.66 / 3.80 MB) · thirdparty 89.63%. 7-day trend:
+2,385 fns / +5.08 pp matched / +4.42 pp fuzzy.

Versus 2026-09-16 (`3890b450`): matched 40.392% → 45.849% (+5.46 pp), share
of ceiling 65.64% → 74.51% (+8.87 pp), ceiling 61.535% → 61.537% (+0.002 pp).
Same shape as the 08-17 → 09-16 asymmetry: the campaign closes real distance,
the denominator does not move.

## Roadmap (priority order — standing user directives)

1. **Breadth / raise the floor (09-30).** Rank by size×(100−fuzzy); the
   reachable gap is dominated by anonymous `fn_` rows inside already-compiled
   units (~6,500 rows / 1.19 MB at the 09-30 count — re-count before briefing)
   ⇒ identification first, each wave with a measured precision control. W17
   owns `src/system/{bandobj,rndobj,char,rnddx9}`; the W16-H* session owns the
   rest of system + game + network. The identification instrument is the
   peer's `tools/anon_candidate_scorer.py` — not usable until its two-leg
   control lands; do not fork it. W16-HZ (`a95d5c525`, +136 fns / +20,336 B)
   is the latest wave of exactly this kind.
2. **Cleanup-before-grind + name-as-you-go (09-30).** Codegen-neutral clarity
   pass (Δ0 on every touched row) before funding a sub-100 row that has prior
   attempts.
3. **Vtable/struct work is high-value**; permuter stays deferred until after
   the ceiling.
4. **Game layer over engine** — DC3 pre-solves the engine (same engine, same
   platform, renders RB3 assets unmodified).
5. **The native port is the real goal**; matching serves it. Run
   `tools/native_build_gate.sh` (expect `PASS 18/18, rc=0`) before landing
   shared `src/` changes.

Out of scope: raising the ceiling (Δgap exactly 0, measured 08-17), XDK
porting (pinning only), permuter sweeps.

## Open hard items (carried from `W17_NEXT_WAVE_TARGETING_2026-09-30.md` + memory)

- `BandStorePanel::OnMsg(MetadataLoadedMsg)` 908 B @ 99.27 — open, not
  at_limit; untried: Symbol materialisation, declaration placement keeping
  the frame at 0xf0.
- Wave-7 F4 integer operand-order calibration — 7 game rows / 8,648 B.
- `MemAlloc(int,int)` residue at 92.98 (`fa658762`): rotated temp-search loop
  + r24/r25 swap; three negative variants recorded in the merge.
- `_S_sort<TextInstance>` 99.67, `Sort@MultiMeshWidgetImp` 99.83.
- `ObjPtr<T>` / `ObjRefConcrete<T,ObjectDir>` destructor family — 181 pairs /
  65,608 B; `icf_pair_adjudicate --chase` refutes by vacuity (placeholder
  vtable relocs). Needs an instrument that resolves the vtable slot.
- 25 wrong-callee source leads: `W16HA_FRESH_ALIAS_PAIRS_2026-09-30.md` (15)
  + W16-HC's doc (10, incl. the `KeyGreaterEq<Quat>`/`KeyLessEq<Color>`
  comparator swap).
- `CharPollableSorter::Sort` 1,136 B, `FxSend::BuildChainVector` 240 B (W16-GR
  leftovers).

## Tooling track in flight (2026-10-01)

Ghidra is used by every decompile-guided workflow (pyghidra-mcp :8002, the
`ghidra-*` skills, decomp_synth's Ghidra-guided patterns), so its correctness
is a force multiplier. Lane context + ground rules:
`docs/plans/ghidra-fork-cleanup-2026-10-01.md`. Lanes:

| lane | what | why it matters here |
|---|---|---|
| A SYNC | merge NSA upstream (966 commits behind) into the Xbox 360 fork, preserving VMX128 / MSVC-switch / Gekko / VT commits | stale fork; upstream added PowerPC v3.0 instructions that may collide with VMX128 encodings |
| B SLEIGH | audit `vmx128.sinc`/`altivec.sinc` against the MSVC `link /dump /disasm` oracle | wrong VMX decode/p-code silently corrupts every vector-math decompile |
| C DECOMP-QUALITY | call-fixups for `__savegprlr`/`__restgprlr`/`__savefpr`/`__restfpr` helpers (no PowerPC cspec has them) | decompile noise on nearly every non-leaf MSVC function |
| D XEX | merge XEXLoaderWV upstream (11 commits behind), review loader against real TU5/TU0 bytes | `.pdata` function creation + imports feed the named bank |
| plan | Fable read-only review ⇒ `docs/plans/ghidra-improvement-plan-2026-10-01.md` | ranked further improvements |

Dispatch lesson (09-30/10-01, measured): Opus briefs must be ~5 sentences
pointing at a context doc; long inline briefs are rejected
(`[reasoning_extraction]`). Put the context in a doc, not the prompt.

## Process rules that keep biting (pointers, not a rewrite)

`CLAUDE.md` is authoritative. Short list: read `report.json` for the score,
never a displayed 100 · `ninja <one.obj>` and `objdiff-cli --build` skip the
6 obj patchers · `grep` in the Claude shell is binary-blind (`command grep
-a`) · never `stash`/`checkout -f`/`reset --hard` in the shared tree · land
lanes with `git merge --no-ff` + a real message · no AI trailers · push only
when asked · worktrees and logs under `~/tmp`, never `/tmp`.
