# Wave-8 Pin-Audit Re-check — Adversarial Refutation Audit (2026-06-19)

**Mode:** PLANNER / ADVERSARIAL VERIFIER (Opus), read-only on main.
**Baseline:** 8234 matched functions (`report.json measures.matched_functions`), HEAD `da8258f`.
**Run:** `venv/bin/python tools/pin_audit.py --json /tmp/w8_pin_audit.json`
→ 619 units, 12489 map entries, **14 candidates, 43 filtered, 110 deferred** (unchanged from w7).

**The Waypoint lesson applied:** every prior "refuted/blocked/net-1" verdict was treated as a
HYPOTHESIS TO FALSIFY using fresh ground truth: the retail symbol table
(`scripts/target_symbol_map.json` VA→mangled-name, 12492 entries), the per-fn match status
(`report.json`, VA parsed from `fn_<VA>` names), the compiled COFF objs
(`build/45410914/src/.../X.obj` — what each TU actually emits), the retail anonymous dump
(`auto_03_82260000_text.obj` — every fn boundary/size by VA), and Ghidra MCP decompiles.

Ground-truth helper used: `/tmp/va_oracle.py {named|report} LO HI` (retail names + report status by VA);
`/tmp/coff_va.py` (COFF symbol→section→size).

---

## HEADLINE VERDICTS

| Item | Prior verdict | W8 adversarial verdict | Why |
|------|--------------|------------------------|-----|
| **Character.cpp relocate** | RECON-FIRST (w7) | **LIVE — CONFIRMED HONEST, actionable** | Ghidra proves the 15-fn sub-gap is Character own-TU (fn_8235E300 calls Character::EnableBlinks directly) |
| **UIGuide / LabelNumberTicker** | "net −1, CrowdAudio loss" (w5/w7) | **REFUTATION_WRONG (reasoning), but pin is BODYPORT-GATED** | CrowdAudio is at 0x82301xxx, 5 MB from the 0x82803xxx carve — the "loss" was mislocated. LNT IS a real unpinned TU. BUT its bodies are near-miss (~85-90%, base-layout/permuter gated), so the pin alone is not a clean +N. Secondary: the **current UIGuide pin is itself dishonest** (tail swallows App::Run + the entire EH runtime, a ≥8 foreign run). |
| InlineHelp | REFUTED (CrowdAudio interleave) | **REFUTATION HOLDS** | Genuine 3-way interleave (CrowdAudio + HelpBarPanel dtor @0x82301578 + InlineHelp ActionElement). No clean carve boundary. |
| TypeProps | BLOCKED (FlowEventListener inside) | **REFUTATION HOLDS** | FlowEventListener pinned [0x827418D0) + MsgSinks::Export @0x82741AC0 inside; only 2 TypeProps named methods in the clean sub-range over 16 fns. Scatter, not misplaced TU. |
| Mic / FxSendDistortion | REFUTED (FxSend compiled into Mic TU) | **REFUTATION HOLDS** | Mic pin [0x82B2F568..0x82B32C20) genuinely contains FxSendDistortion360 / CSampleXAPOBase<DistortionEffect> Process/LockForProcess interleaved with MicXbox. Attribution follows compiled obj. |
| Band / Game head +4 | REFUTED (ODR/per-TU, 0 readers) | **REFUTATION HOLDS** | Header-edit framing dead. (Lever survives elsewhere: the only confirmed +4 reader is the *unpinned* Player ctor fn_82688E40 — a pin-extension, gated on a correct +4 header fix.) |
| **AsyncFileHolmes HEAD split** | (new, A4) | **NOT ACTIONABLE** | The foreign TUs (RndSoftParticleBuffer / StartTransitionMsg / CurrentScreenChangedMsg) have NO source/obj anywhere; MetaPerformer is unwired (Wii src only); DOFProc.obj is a near-empty stub. Nothing to pair → 0% pins. Port-campaign target, not a pin lever. |

---

## 1. Character.cpp relocation — LIVE, HONEST, ACTIONABLE (the survivor)

**Current pin:** worthless sliver `Character.cpp .text [0x822911F0..0x82291238)` (0x48 B, mf=0).
**Proposed:** relocate to the real cluster `[0x8235B1D0..0x8235F180)` (between the TypeProps sliver
end 0x8235B138 and the CharBlendBone.cpp pin 0x8235F180). Entire range is currently in the
`auto_03_8235B138_text` auto-blob reading 0%.

### Own-vs-foreign ground truth (the honesty gate)

Compiled `Character.obj` **defines** (COFF-verified):
- 7 own named methods: ForceBlink, EnableBlinks, SetFocusInterest, SetInterestFilterFlags,
  Teleport, AddedObject, SetInterestObjects (+ `operator<<(BinStream, Character::Lod)`).
- **184** `CharPollableSorter::Dep**` sort-template COMDATs (`__median/__push_heap/__adjust_heap/
  __make_heap/__insertion_sort/__final_insertion_sort/sort_heap/__unguarded_partition/...`) — all
  Character's OWN dependent templates.
- `?CalcDistTo@RndCam@@` (1) + `?CalcScreenHeight@RndCam@@` (1) — **emitted by Character.obj**
  (RndCam math inlined-out-of-line into Character.cpp's TU, ICF-shared; they read the "RndCam"
  name but the BODY is Character's). Verified: 0x8235B3C8 decompiles to the dot-product distance
  body Character.cpp calls.
- `?ChangedByRecurse@CharPollableSorter@@` (1), `VectorRemove<RndDrawable>` (1) — own.

Genuinely FOREIGN ICF address-aliases in range (NOT in Character.obj, read 0%): `ClassName@CharWeightable`
(0x8235C278), `Pose@RndMorph` ctor (0x8235C560), `DebugFailer::operator()` (0x8235C970), and the
`__destroy_range<ChallengeRow/IKTarget@CharIKHand/CamShotFrame/StepMoves>` + `~vector<ChallengeRow>`
template instantiations (0x8235DAD0, 0x8235ED50, 0x8235EF28, 0x8235F0D0, 0x8235EA18). **All are isolated
singletons**, each bracketed by own Character fns — exactly the Waypoint ICF-alias pattern. No ≥8
contiguous foreign run.

### The 15-fn sub-gap [0x8235DC48..0x8235E928] — RESOLVED by Ghidra (the w7 open question)

w7 flagged this as the actionability gate. **Ghidra MCP decompile (port 8002) RESOLVES it as Character own-TU:**
- `fn_8235DC48` (0x53C): float/Vector3/Matrix-heavy; calls `Function_8235D778` + `Function_8235D7F0`
  (both inside the Character cluster) and `Function_82398AE8(<Symbol-literal>, this)` — a Character
  bone/interest update helper. Own.
- `fn_8235E300` (0x4AC): **calls `Function_8235B208` = `?EnableBlinks@Character@@` directly** (a named
  Character method!), plus 8235CF48/8235CEE0/8235CD48/8235E188 (all in-cluster), with `param_2 + -0x26c`
  MI-subobject this-adjust. Unambiguously a Character method. Own.
- `fn_8235E188` (0x170): vtable-slot dispatch + Symbol/DataArray helper; in-cluster callees. Own.
- 0x8235E7AC..0x8235E928 (10 small 0x20-0x74 fns): funclets/thunks bracketed by own content. Own.

→ **The 15-fn gap is Character's own bodies. Honesty gate passes. Pin is actionable.**

### Boundaries
- Start 0x8235B1D0 = `?ForceBlink@Character@@` (Character's first own method). The lone fn at 0x8235B138
  (0x94 B) between the TypeProps sliver and Character is excluded (left in auto-blob; ambiguous owner).
- End 0x8235F180 = `CharBlendBone.cpp` pin start. The last in-range fn `~vector<ChallengeRow>`
  @0x8235F0D0 ends before it. Clean.

**Expected delta: +9** (low-confidence band +5..+14). The sort templates + small accessors + RndCam
math are deterministic and likely land at 100%; the larger float methods may be near-miss. dtk
re-derives `.pdata` from the relocated `.text`. **attribution_risk = TRUE** (own-vs-foreign audit
required post-build: confirm no ≥8 foreign @0% run appears).

---

## 2. UIGuide / LabelNumberTicker — REFUTATION_WRONG (reasoning) + a dishonest current pin

### What the prior refutation claimed (w5 origin, repeated w6/w7)
> "UIGuide / LabelNumberTicker: Net −1 — UIGuide trim orphans 2 CRT thunks; carving loses CrowdAudio
> tail (fn_823017A8/fn_823019C0)."

### Why it is WRONG
The carve range is `[0x82802F30..0x828036E8]` (the LabelNumberTicker cluster). **CrowdAudio's pin is
`[0x822FEDF8..0x82301F64)` — at 0x82301xxx, ~5 MB BELOW the carve.** `fn_823019C0` lives inside
CrowdAudio's pin, nowhere near the 0x82803xxx carve. The "CrowdAudio loss" objection was mislocated
(conflated with the InlineHelp item, which genuinely abuts CrowdAudio). Carving LabelNumberTicker
does not touch CrowdAudio at all.

### The real LabelNumberTicker TU (COFF + size-match proof)
Retail places LabelNumberTicker methods at `[0x82802F30..0x828036E8]` *inside the UIGuide pin*,
reading 0% because LNT isn't pinned there. Compiled `LabelNumberTicker.obj` (built from
`src/system/ui/LabelNumberTicker.cpp`, NonMatching) defines exactly these, and the sizes match retail:

| method | retail VA | retail size | compiled size |
|---|---|---|---|
| Save | 0x82802F30 | 0xD8 | 0xD8 ✓ |
| UpdateDisplay | 0x82803230 | 0x58 | 0x60 |
| Poll | 0x82803288 | 0x180 | 0x17C ✓ |
| SetDesiredValue | 0x82803408 | 0xA0 | 0xA0 ✓ |
| CountUp | 0x828034A8 | 0x58 | 0x50 |
| SyncProperty | (in InlineHelp.obj range) | — | 0x590 |

UIGuide.obj defines ONLY UIGuide methods (8) + Timer/DataArray STL — it does NOT define LNT. So LNT
is a separate, real, currently-unpinned TU. **The refutation's logic was wrong.**

### BUT the pin alone is BODYPORT-GATED (the honest tempering)
Byte-diffing retail vs compiled `LNT::Save` (excluding reloc placeholders): **7 non-reloc word diffs /
54 words (~87%)**. `LNT::Poll` shows uniform `+8` member-offset shifts. RB3 and DC3 `LabelNumberTicker.h`
headers MATCH (members `mLabel@0x10c`, `mDesiredValue@0x118`, …), so the divergence is NOT in LNT's own
header — it is a **base-class (UIComponent/RndHighlightable) layout shift** (the known UICOMP wall) or
permuter-class codegen. Therefore the LNT method bodies will read <100% even when pinned; the pin
surfaces them for objdiff but does not by itself add matched_functions beyond the funclets.

Pin accounting (relocate LNT to [0x82802F30..0x828036E8], shrink UIGuide to end at 0x82802F30):
- UIGuide head [0x82801070..0x82802F30]: keeps its 11 matched (unchanged).
- LNT carve: 2 funclets (0x82803674/0x82803694, both 100%) re-attribute UIGuide→LNT (net 0); the 6
  real LNT methods stay <100% until body-ported. Net from real methods ≈ 0.
- UIGuide tail [0x828036E8..0x82804770]: shrinking it LOSES the 2 tail funclets (0x82803B20/0x82803B68)
  currently matched — these sit in the **foreign EH-runtime block** UIGuide should never have owned.

→ **Naive pin-only is net ≈ −2** (the w5 "net −1" was directionally right by accident, wrong on cause).
The VALUE here is: (a) correct the record (refutation reasoning was wrong), (b) LNT is a real
bodyport-gated target (port LNT to 100% once the UIComponent base layout is fixed → then pin = clean),
(c) the **current UIGuide pin is itself dishonest** and should be trimmed of its foreign EH tail.

### Secondary finding — the UIGuide pin fails the honesty gate TODAY
The current UIGuide pin `[0x82801070..0x82804770]` swallows, in its tail `[0x828036E8..0x82804770]`,
24 fns including 16 named **foreign** CRT/EH-runtime symbols: `?Run@App@@`, `_cfltcvt_init`,
`__savegprlr_14..28`, `_SaveUnwindContext`, `_CxxThrowException`, `_purecall`, `_CreateFrameInfo`,
`__FrameUnwindToEmptyState`, `?__SehTransFilter@@`, … — a clear ≥8-contiguous-foreign run. The pin
"works" only because it harvests 2 funclet matches from that block. This is the inverse of a clean pin
and an honesty liability; a future trim to UIGuide's real TU end (≈0x82802F30) is the correct shape.

**Disposition:** record correction. Pin-only NOT landable (net-negative). Real action = body-port LNT
(bodyport-batch target, base-layout-gated) → then pin. attribution_risk = TRUE.

---

## 3. InlineHelp — REFUTATION HOLDS (genuine 3-way interleave)

CrowdAudio pin `[0x822FEDF8..0x82301F64)`. InlineHelp pin `[0x823024E8..0x82303530)`. pin_audit proposes
InlineHelp range `[0x823018E0..0x82304880)`. The InlineHelp `ActionElement` methods (SetToken @0x823019E0,
SetString @0x82301A40, ctors @0x823018E0/0x82301D48, operator= @0x82301DD0) ARE InlineHelp-own (verified:
`InlineHelp.obj` defines `ActionElement@InlineHelp` — SetToken/SetString/ctors/HasSecondaryStr/...) and
sit INSIDE CrowdAudio's pin. **But** between CrowdAudio's last method (Exit @0x82301318) and the
ActionElement block sits `??_GHelpBarPanel@@` (HelpBarPanel scalar-deleting dtor) @0x82301578 — a THIRD
class. CrowdAudio's pin currently has 45 matched fns; the boundary at 0x823018E0 is NOT clean (HelpBarPanel
dtor + CrowdAudio funclets interleave the ActionElement methods). This is the C5 scatter/attribution-orphan
pattern, not a misplaced contiguous TU. No clean carve exists. **Refutation correct.** (The prior
"fn_823019C0 loss" detail was imprecise — that funclet is likely InlineHelp's own ActionElement unwind
handler — but the interleave blocks a clean split regardless, so the verdict stands.)

## 4. TypeProps — REFUTATION HOLDS (scatter + pinned foreign inside)

Proposed range `[0x82740E40..0x82741D88)`. Contains FlowEventListener (already pinned
`[0x827418D0..0x82741950)`, contributing matches) AND `?Export@Sink@MsgSinks@@` @0x82741AC0. Only 2
TypeProps named methods (Size @0x82740E40, ClearAll @0x827417A8) appear in the clean pre-FlowEventListener
sub-range `[0x82740E40..0x827418D0)`, which has 16 fns / 0 matched / 14 anonymous of unknown ownership.
TypeProps.obj defines ~16 methods but retail exposes only 4 across the whole scattered span (0x82740E40..
0x82741A08, FlowEventListener + MsgSinks woven through). A dual-range escape (skip FlowEventListener)
would pin 2 named + 14 unknown anon — honesty risk, EV ≈ +1..2 at best. **Refutation holds; dual-range
not worth it.**

## 5. Mic / FxSendDistortion — REFUTATION HOLDS

Not a pin_audit survivor (prior-wave roadmap refutation). Verified fresh: Mic pin `[0x82B2F568..0x82B32C20)`
genuinely interleaves MicXbox methods (GetType @0x82B2F8E8, SetFxSend @0x82B31F08) with
`?SyncEffectParams@FxSendDistortion360@@` @0x82B32BC8 and `CSampleXAPOBase<DistortionEffect>`
Process/LockForProcess @0x82B32918/0x82B32958. Retail compiled the Distortion APO into Mic's .text
neighborhood; attribution follows the compiled obj. **Refutation correct.**

## 6. Band / Game head +4 — REFUTATION HOLDS (the header-edit framing)

Per `2026-06-11-bandgame-head4.md`: direction confirmed (ours = retail+4) but ZERO measured readers
(exhaustive 165-near-miss scan found none), and it's a per-TU ODR/include-set divergence, not a single
header member. The one confirmed +4 reader is the **unpinned Player ctor fn_82688E40** (size 704, outside
the Player.cpp pin). **Header-edit refutation holds.** (A latent pin-extension lever — pin fn_82688E40 —
survives but is gated on a correct +4 fix and out of scope for pin_audit-recheck.)

## 7. AsyncFileHolmes HEAD [0x82522248..0x82527920] — NOT ACTIONABLE (A4)

AsyncFileHolmes.cpp pin `[0x825221D0..0x82527920)` (0x5750 B, 205 fns, only 5 matched). Head holds
scattered foreign TUs: AsyncFileHolmes dtor, RndSoftParticleBuffer (StaticClassName @0x825229E0),
StartTransitionMsg (@0x82523CB8), CurrentScreenChangedMsg (@0x82524008), MetaPerformer (OnMsg @0x825245A0),
DOFProc (StaticClassName @0x82527428). Source/obj availability:
- RndSoftParticleBuffer / StartTransitionMsg / CurrentScreenChangedMsg: **no obj, no source in any oracle**
  (DC3 or Wii). A pin over them reads 0% (nothing to pair). Cannot split.
- MetaPerformer: Wii source only, **unwired** (no obj). Port-campaign target, not a pin.
- DOFProc: DOFProc.obj is a near-empty STUB (defines only StaticClassName; pin is a 0x10 B sliver
  @0x82453468). Nothing substantive here.

No clean foreign-TU split exists. This is the obj_orphan / ATTRIBUTION_ORPHAN wall. **Not actionable as
a pin lever** — it is a body-port/wire campaign (MetaPerformer, DOFProc deepen).

---

## Tooling note (reusable)
`/tmp/va_oracle.py` — VA→retail-name (`scripts/target_symbol_map.json`) + VA→report-match-status
(`report.json`, VA from `fn_<VA>`). `/tmp/coff_va.py` — COFF symbol→section→size for any `.obj`. These
are the decisive own-vs-foreign instruments; consider promoting to `tools/` for future pin audits.

## Bottom line
- **1 live survivor (Character.cpp relocate, +9, recon-confirmed honest)** — emit as work_item.
- **1 REFUTATION_WRONG (UIGuide/LNT reasoning)** — record correction; pin is bodyport-gated, not a
  clean pin; also flags a dishonest current UIGuide pin. Emit as bodyport-gated work_item (not
  actionable-now).
- **5 refutations re-verified and HOLD** (InlineHelp, TypeProps, Mic/FxSend, Band/Game head, AsyncFileHolmes).
- pin_audit's pin-relocation/carve vein is **near-dry for clean +N pins** — Character is the last clean
  one. The remaining candidates are interleave/scatter/orphan walls or bodyport-gated.
