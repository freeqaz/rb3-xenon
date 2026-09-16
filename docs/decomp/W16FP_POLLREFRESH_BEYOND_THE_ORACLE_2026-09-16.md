# W16-FP — `XboxContentMgr::PollRefresh` (824 B): crossing a row whose oracle sits at 95.17

Lane W16-FP, 2026-09-16. Branch `w16-fp` off main `3c6eacd1`, worktree
`~/tmp/wt-w16-fp`. One file touched: `src/system/os/ContentMgr_Xbox.cpp`.
Commits: `1f6540d5` (0 -> 93.16), `6b4dc12d` (93.16 -> 96.67), `8cf6693f`
(96.67 -> 100.0). No header touched; every one of the eleven builds
recompiled exactly one TU (`[5/14] MSVC build/45410914/src/system/os/ContentMgr_Xbox.obj`).

## TL;DR

- `?PollRefresh@XboxContentMgr@@UAAXXZ` (824 B): **fuzzy 0.000 -> 100.0, mpn 100.0**
  (worktree `build/45410914/report.json`, ruler `name_check`, full
  `./tools/ninja-locked` per step). The row crossed; `matched_code` credit is
  the whole 824 B, confirmed by the authoritative A/B in §6.
- DC3's own copy of this function is at fuzzy 95.17, and its residual is
  **the oracle's spelling of one call**: `(*it)->ContentDiscovered(Symbol(filename))`.
  With an explicit functional-cast temporary in the argument list, MSVC 10224
  reads the callee's vptr *before* the `Symbol` ctor `bl` and keeps it live across
  the call in a callee-saved register; retail reads it after. The implicit
  conversion `(*it)->ContentDiscovered(filename)` gives retail's shape exactly.
- Two retail-vs-DC3 structural differences that are **behavioural**, not cosmetic:
  the ignored-content list is a static 8-string table (not a `vector<String>`
  filled from SystemConfig), and the `kMounting` arm's flag polarity is
  **inverted in DC3** (retail stores `mState` only when the all-done flag is
  FALSE).

## 1. Starting state and why a faithful port pays zero

Brief: row at fuzzy 0.000, unit `default/ContentMgr_Xbox` (`NonMatching` in
`objects.json`). DC3's copy at 95.17. `matched_code` is all-or-nothing on
`fuzzy == 100`, so transcribing DC3 lands at ~95 and banks 0 B. The job was to
find *what* DC3's residual is and whether it transfers (W16-FJ/FK's lesson: a
twin's wall transfers only if the oracle's failed spellings span the mechanism).
They did not span it — DC3 never tried the implicit-conversion spelling.

## 2. Retail (`fn_82520C18`, `~/tmp/w16fp_retail.s`) vs DC3, structurally

| DC3 `PollRefresh` | retail RB3 | evidence on bytes |
|---|---|---|
| `std::vector<String>` of ignored names built from `SystemConfig("ignored_content")`, walked with `String::operator==` | **static table of 8 C strings**, walked with an `/Oi`-inlined `strcmp` (`DIM` loop; `cmplwi 0x20` bound = 8 x 4) | the inlined-strcmp loop body and the 0x20 bound; table = `"rbsongcache" "rb2songcache" "band" "band3" "netcache" "Song Export" "globaloptions" "rbdxcache"` (`.rdata`, irrelevant to `.text` bytes; `"rbdxcache"` is the DX-lineage entry) |
| `dwContentType == 0x7000` branch calling `ContentTitleDiscovered` | absent | no such compare/vcall in the listing |
| `XGetOverlappedExtendedError` + `MILO_NOTIFY` else-arm | absent | no such `bl` |
| `unk7fc = 0` at entry | absent | no store at entry |
| pending (`0x3E4`) result falls through to `ContentMountBegun` / base `PollRefresh` | **returns straight to the epilogue** after `mState = kDiscoveryMounting` | branch target is the epilogue |
| `numItems` zero-initialised | **uninitialised** | no `stw` to `0x50(r31)` before `XGetOverlappedResult` (E1, +1.29) |
| `kMounting` arm: `allDone &= (state != kNeedsMounting)`, then `if (allDone) mState = ...` | `kNeedsMounting` case is a **select** (`subfic/subfe` 0/-1 mask AND'd into the flag) and the store happens **only when the flag is FALSE** (`bne` past the store) | E4; this is a real behavioural divergence in DC3's source, not a spelling |
| j-loop indexes `xdatas[j]` at both uses | **the filename pointer is the only induction variable**, created in the preheader *after* the `numItems == 0` guard, record rematerialised for the ctor as `filename - 0x108` (`subi r4,r26,0x108`) | E2b/E2c (+1.66 / +0.99) |

Sizes used: `sizeof(XboxContent) == 0x174`, `XCONTENT_CROSS_TITLE_DATA` stride
`0x138`, `szFileName` at `+0x108` (all read off the listing's immediates).

### Alias coverage (nothing added by this lane)

Five `bl`s on this row differ by relocation name from retail and are forgiven
by existing `symbol_aliases.json` groups: `??2CriticalSection`<->`??2Content`,
`??3BinStream`<->`??3@YAXPAX@Z`, `insert<Hmx::Object*>`<->`insert<Content*>`,
`fn_8283F340`<->`XEnumerateCrossTitle`, `fn_8251F1E0`<->`?PollRefresh@ContentMgr@@UAAXXZ`.
`fn_827C0728` is the named `??0Symbol@@QAA@PBD@Z` (no charge). I added no alias
and no map row, so the `none`-flat / name_check-up hazard does not arise here:
every point moved by source.

## 3. The ladder — eleven builds, one full locked build each

Score = worktree `report.json` `fuzzy_match_percent` / `match_percent_normalized`
on `?PollRefresh@XboxContentMgr@@UAAXXZ`, ruler `name_check`.

| build | spelling | fuzzy | mpn | verdict |
|---|---|---:|---:|---|
| v1 | rewrite to retail shape (table, no 0x7000 arm, pending returns) — `1f6540d5` | 93.155 | 94.709 | +93 from 0 |
| v2 | E1 `numItems` uninitialised | 94.442 | | |
| v3 | E4 `kMounting` polarity + select | **94.024** | | region byte-equal, fuzzy **fell** 0.42: diff alignment re-paired idx 96-98 as one op. Kept on bytes. Fuzzy is not monotone in structural closeness. |
| v4 | E2a unnamed record, `xdatas[j]` at both uses | 94.024 | | INERT (exact) |
| v5 | E2b filename pointer as the only j-IV, record = `filename - 0x108` | 95.680 | | |
| v6 | E2c filename computed in-body from the folded base (`szFileName + j*sizeof`) — `6b4dc12d` | 96.675 | 98.034 | prologue `__savegprlr_18` -> `_19` (target `_20`); one residual cause left |
| v7 | C `Callback *cb = *it; cb->ContentDiscovered(Symbol(filename))` | 96.675 | 98.034 | INERT (exact) — evaluation order of `this` is not the lever |
| v8 | B' `Symbol sym(filename); (*it)->ContentDiscovered(sym)` | 99.330 | 99.476 | **POSITIVE CONTROL**: vptr load moves after the `bl`; Symbol read from a frame slot |
| v9 | A' `const Symbol &sym = Symbol(filename)` | 96.767 | 98.199 | REFUTED my prediction (100): reference materialises as a loop-invariant frame address in r27 |
| v10 | D `cb->ContentDiscovered(filename)` (implicit conversion) | **100.0** | **100.0** | crossed |
| v11 | D' `(*it)->ContentDiscovered(filename)` — `8cf6693f` | **100.0** | **100.0** | `cb` local inert; committed text |

Logs: `~/tmp/rb3_build_w16fp_v1.log` … `v11.log`, all rc=0, one TU each.

## 4. The finding: where the vptr load lands relative to an argument ctor

Retail, callback loop body (r26 = filename, r10/r11 scratch):

```
mr   r3, <temp at 0x5c(sp)>   ; Symbol temp
mr   r4, r26                  ; filename
bl   ??0Symbol@@QAA@PBD@Z
lwz  r11, 0(<*it>)            ; vptr loaded AFTER the ctor
mr   r10, r3                  ; Symbol read through the ctor's returned this
lwz  r4, 0(r10)
lwz  r11, 8(r11)              ; Callback::ContentDiscovered slot
bctrl
```

DC3's spelling `(*it)->ContentDiscovered(Symbol(filename))` compiles to the vptr
`lwz` **before** the `bl`, held in a callee-saved register across it. That
costs one more saved GPR (`__savegprlr_19` vs retail `_20`), frame +0x10, and
renames every register in the j-loop — which is the whole 3.3-point residual
and, by construction, DC3's own 95.17 wall.

The four spellings separate two things that were confounded in DC3's text:

| spelling | vptr load vs `bl` | Symbol read via | result |
|---|---|---|---|
| explicit temp `f(Symbol(p))` | before | returned `this` | 96.67 |
| named local `Symbol s(p); f(s)` | after | frame slot (address) | 99.33 |
| `const Symbol &s = Symbol(p); f(s)` | after | loop-invariant frame address (r27) | 96.77 |
| implicit `f(p)` (parameter copy-init) | **after** | **returned `this`** | **100.0** |

Ruled out: ctor-body visibility. `Symbol(const char *)` is declaration-only in
`src/system/utl/Symbol.h:17` here **and** in DC3, so both compile the same
out-of-line `bl`; the discriminator is the front end's copy-initialisation
path, not inlining. Also ruled out (v7): sequencing the `*it` load into a
named local — inert on both the explicit-temp form (v7) and the implicit form
(v10 vs v11 identical).

**Transferable rule**: when retail shows `bl <ctor>` immediately followed by the
callee-object's vptr load, and our build hoists that load above the `bl` at
the cost of an extra `__savegprlr_N`, look for an explicit temporary in the
argument list and let the parameter conversion do it instead. This is the
*same* class as the "oracle's spelling is the defect" instances catalogued on
09-16 (`project_oracle_fidelity_has_four_modes_2026-08-17.md`): the oracle's
text is a hypothesis, and here it was the residual.

## 5. Unit-level effect in the worktree

`default/ContentMgr_Xbox` (worktree report): matched functions 70 -> 72 of 75,
`matched_code` 6112 -> 6976 (+864 B) between `6b4dc12d` and v10/v11. The +864
is the 824 B row plus one 40 B row; the unit holds 14 rows of 40 B, all at
100 and all `masked_equal` (funclet byte-signature pairings), so the extra is
PollRefresh's EH cleanup funclet re-pairing. I did not assert this from the
snapshot — §6's set-diff is the attribution.

## 6. Authoritative A/B (`tools/ab_measure.py`)

Construction: `ab_measure` has no base-ref option and applies `--pick`/`--patch`
onto the worktree HEAD, which on `w16-fp` already carries the change. So the
worktree was detached (worktree only — main untouched) at `3c6eacd1`, and
`--patch ~/tmp/w16fp_combined.patch` (= `git diff 3c6eacd1..8cf6693f`, one
file, +53/-45) was measured: leg A = main's tree, leg B = A + patch.

Pre-registered before the run:

- Predicted `Δmatched_code = +864 B`, `Δmatched_functions = +2`,
  `Δmasked_equal = +1` (Δhonest +1), no unit completion, every other unit Δ0,
  leg B recompiles exactly 1 TU.
- Expressible alternative: `+824 B / +1 fn / +0 masked_equal` if the funclet
  already paired at main.
- Falsifiers: F1 Δcode < +824 => row did not cross under settled legs, banks
  zero. F2 any other unit moves => hidden cascade / instrument fault. F3 leg-B
  recompiles != 1 => patch scope wrong. F4 Δcode > +864 => unattributed gain,
  must be named before claiming.

Measured: **exactly the primary prediction** (run dir
`.ab_measure_runs/20260916-122027-w16fp-pollrefresh-2285952`, tool blob
`9c5dec33` = HEAD, objdiff-cli sha `c1b7d952` stable across legs, both legs
settled, leg B first iteration `msvc=1 split=0 patch=6`):

| | leg A (`3c6eacd1`) | leg B (A + patch) | Δ |
|---|---:|---:|---:|
| `matched_functions` | 44,030 | 44,032 | **+2** |
| `masked_equal` | 23,245 | 23,246 | +1 |
| honest | 20,785 | 20,786 | +1 |
| `matched_code_percent` | 40.476086 | 40.484516 | **+0.008430 pp** |
| `matched_code` bytes | | | **+864 B** |
| fuzzy (whole binary) | 50.345950 | 50.353905 | +0.007955 pp |
| units at 100 (mpn / all-rows-fuzzy) | 191 / 171 | 191 / 171 | 0 / 0 |
| `none` ruler control | 44.613113 | 44.621544 | +864 B (source patch: movement expected, not adjudicable as alias) |

Unit set-diff (archived `legA_report.json.gz` vs `legB_report.json.gz`,
unit `default/ContentMgr_Xbox`, 75 rows both legs, no rows appeared or
vanished): exactly two rows changed —

- `?PollRefresh@XboxContentMgr@@UAAXXZ` 824 B: fuzzy **0.0 -> 100.0**, mpn
  1.04 -> 100.0 (so on main the body existed and scored ~nothing).
- `fn_82520F50` 40 B: fuzzy 99.8 -> 100.0, `masked_equal` in **both** legs.
  `0x82520C18 + 0x338 (824) = 0x82520F50`: it is PollRefresh's own EH cleanup
  funclet, already paired by byte signature, carrying one relocation-name arg
  that the new body resolves. That is the +40 and the +1 `masked_equal`.

Unit 70 -> 72 of 75; unit net = whole-binary Δmatched, so nothing outside the
unit moved (F2 did not fire). F1, F3, F4 did not fire either.

## 7. What was NOT done, and why

- `?Poll@XboxContent@@UAAXXZ` (424 B, fuzzy 89.65) and `??0XboxContentMgr@@QAA@XZ`
  (184 B, 23.52) — same unit, untouched. The brief's prize was the 824 B row and
  the "beyond the oracle" question; the two secondary rows were left for a
  follow-on lane rather than started with no budget to finish.
- `??$Obj@VCharPollable@@@DataNode@@…` (60 B, 31.8) — a scatter-included
  `char/CharPollGroup.cpp` template row; not this lane's subject.
- Six 44 B rows `fn_825214B0 … fn_82521638` read fuzzy 99.545 / mpn 100.0 —
  one relocation-name arg each. Not investigated; likely fold/alias aliases,
  and adjudicating them is map work, not source work.
- No DC3 backport of the implicit-conversion spelling (DC3's copy would move
  from 95.17 by this finding; that is a different repo and out of lane scope —
  recorded here for whoever owns it).
- No alias, no map row, no header edit, no `objects.json` status flip
  (`ContentMgr_Xbox` stays `NonMatching`: 72/75 rows, not 75/75).
- Permuter not used (off by directive; every point here was a named source
  construct, which is the standard the SOURCE_INSDEL series set).

## 8. Native gate (last action)

Run **after** this doc's commit so that it is genuinely the last action on
the lane; the verbatim `NATIVE_GATE_RESULT` line is in the lane's final
report. The gated tree is `w16-fp` at this commit plus nothing (docs-only
commit on top of `8cf6693f`; the only `src/` change is `ContentMgr_Xbox.cpp`,
which the native build does not compile — it is an `_Xbox` TU — so the gate is
run for the house rule, not because a mechanism is expected).
