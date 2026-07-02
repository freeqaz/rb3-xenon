# Port handoff — ADSR.cpp (port-frontier 2026-07-02)

Branch: `bp-pf-ADSR`  ·  Worktree: `~/tmp/wt-pf-ADSR`  ·  Owner: Opus porter (sysnet worklist)

## Assignment
TU `src/system/synth/ADSR.cpp`, marked `pin`/easy, 2 INSIDE-split targets:
- `0x8270c188` ADSR::SyncPacked (BSim sim*conf 19.2, bsim15-20 confirm-on-consume)
- `0x8270c1d0` ADSR::Load       (BSim sim*conf 17.7, bsim15-20 confirm-on-consume)
- optional OUTSIDE: `0x8270c008` Ps2ADSR::NearestSustainRate (bsim 30.5)

## What this actually was: a PORT, not a pin
The plan said "pin" (map-entry only), but the wired rb3-xenon source was the
**DC3-newer variant that had dropped SyncPacked + the PS2 bit-packing**. The two
target functions therefore had NO source method producing them — the TU report
showed `fn_8270C188`/`fn_8270C1D0` UNMATCHED (fuzzy None), and `ADSRImpl::Load`
used a `LOAD_REVS`+`MILO_FAIL(PathName(adsr)...)` shape that does not match retail.

Ported the missing pieces from the rb3-Wii oracle
(`/home/free/code/milohax/rb3/src/system/synth/ADSR.{cpp,h}`), adapted into
rb3-xenon's `ADSRImpl` structure (rb3-Wii had a flat `ADSR` struct; retail = the
`ADSRImpl` nested in `ADSR : Hmx::Object`). Disasm-verified against the retail
body at each address (`tools/va_disasm.py`), member layout confirmed off the
`ADSRImpl` ctor at 0x8270bd80 (mPacked@0x20 = 0x8F1F/0x3C7, mSynced@0x24).

### Source changes (`src/system/synth/ADSR.{cpp,h}`)
- `ADSRImpl::SyncPacked()` added: `if(!mSynced){ mPacked.Set(*this); mSynced=true; }`
  (matches 0x8270c188: lbz 0x24 guard, call Ps2ADSR::Set@0x8270c060, stb 1 → 0x24).
- `ADSRImpl::Load(BinStream&, ADSR*)` reworked to the retail shape:
  read `int version`; `if (version>1) MILO_WARN("Can't load new ADSR")` (a no-op in
  retail → no PathName vcall, matches the clean `bgt→exit` in the disasm) `else`
  read 5 floats + 3 modes, `mSynced=false`, `SyncPacked()`. (Retail sets mSynced=0
  between the sustain-mode store and the release-mode read — preserved.)
- `Ps2ADSR` gained int setters/getters, `Set(const ADSRImpl&)`, the PS2 rate/level
  tables (gDecayRate/gSustainLevel/gReleaseRate{Lin,Exp}/gLin{Inc,Dec}/gExp{Inc,Dec}),
  `FindNearestInTable`, and Nearest{Attack,Sustain,Release}Rate. `friend class Ps2ADSR`
  added to ADSRImpl so Set can read its members.
- `Ps2ADSR()` ctor fixed: `mReg1(0x8F1F), mReg2(0x3C7)` (was 0/0) — matches the
  retail ADSRImpl ctor's mPacked init.

## Verification (objdiff-cli direct, per-fn, on the current built objs)
| target | mangled | norm % | size | diff_score |
|---|---|---|---|---|
| 0x8270c1d0 Load | `?Load@ADSRImpl@@QAAXAAVBinStream@@PAVADSR@@@Z` | **100.0 strict** | 248/248 | 0/6200 |
| 0x8270c188 SyncPacked | `?SyncPacked@ADSRImpl@@QAAXXZ` | 99.72 (=report-100) | 72/72 | 5/1800 |
| (regression check) Save | `?Save@ADSRImpl@@QBAXAAVBinStream@@@Z` | **100.0** (unchanged) | 268/268 | 0/6700 |

SyncPacked's sole 0.28% residue is a **reloc-naming artifact**, proven by dumping
the proto diff: target side is `bl fn_8270C060`, base side is
`bl ?Set@Ps2ADSR@@QAAXABVADSRImpl@@@Z` — same instruction/offset, only the callee
symbol name differs because the target obj's `fn_8270C060` had not yet been renamed.
diff_inspect --diagnose reports 0 offset shifts / 0 arg diffs. This is the
documented "cli 99.4-99.7 + size-exact = report-normalized 100" class. Closed by
mapping the callee (below).

## Pinned (scripts/target_symbol_map.json)
- `0x8270C188 -> ?SyncPacked@ADSRImpl@@QAAXXZ`
- `0x8270C1D0 -> ?Load@ADSRImpl@@QAAXAAVBinStream@@PAVADSR@@@Z`
- `0x8270C060 -> ?Set@Ps2ADSR@@QAAXABVADSRImpl@@@Z`  (callee; pairs SyncPacked's call reloc → SyncPacked to clean 100)

No `splits.txt` change needed — both SyncPacked and Load are already INSIDE the
existing `ADSR.cpp:` .text ranges (`0x8270C188–0x8270C1D0`, `0x8270C1D0–0x8270C2C8`).
Ps2ADSR::Set@0x8270C060 is OUTSIDE the split (it's an external reloc reference in
the ADSR target obj), so the map entry pairs the call-site name without needing a
Set split.

## Identity confirmation (NOT sibling-aliased)
The bsim15-20 confirm-on-consume check passes decisively: SyncPacked and Load
**mutually reference each other** (Load's tail is `bl 0x8270c188`=SyncPacked;
SyncPacked calls Ps2ADSR::Set) and each matches the retail disasm shape uniquely
(mSynced-guard vs version+5-float+3-mode reader). No same-TU sibling has these
shapes. The build is the precision gate: both paired at their addresses and
scored ~100 → correct identities.

## ICF honesty
No `icf_alias_check` in rb3-xenon; checked `scripts/symbol_aliases.json` (no ADSR
entries) and confirmed manually: both are distinct substantial bodies (72B / 248B),
not trivial getters/stubs — no fold collision. Honest matches.

## Skipped
- `0x8270c008` Ps2ADSR::NearestSustainRate (optional micro-pin): NOT pinned this
  pass. It's OUTSIDE the split and would need a micro-pin + pdata check. The full
  Ps2ADSR (Set + Nearest*Rate + tables) IS now ported/compiled, so a follow-up can
  micro-pin 0x8270c060 (Set, 296B, clean boundary at 0x8270c060–0x8270c188) and
  0x8270c008 (NearestSustainRate) cheaply if desired. Left as low-ROI residue.

## Build note / caveat for the lander
The final whole-binary `ninja-locked` (triggered by the rename-stamp removal +
config.yml touch to re-apply the Set rename) STALLED under heavy concurrent
machine load (~300 hung wibo/cl.exe from sibling agents, 0 objs written for 15+ min)
— it did NOT complete in-session. This does NOT affect correctness: the per-fn
objdiff results above were measured on the correctly-built base+target objs, and
the map entries are what the landing `fresh_report` re-applies. The lander should
run a clean `fresh_report.sh` (or per-fn objdiff-cli) once load subsides to confirm
SyncPacked closes to report-100 and no whole-binary regression.

## Regressions
0 (Save@ADSRImpl still 100.0; only added a new method + reworked one Load body).

## Commits on bp-pf-ADSR
- `e30e405` port(synth): ADSR.cpp — add ADSRImpl::SyncPacked + rework Load to retail shape
- `1892e92` pin(synth): ADSR SyncPacked+Load+Ps2ADSR::Set map entries
- (this doc)
