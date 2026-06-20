# W9 L9 — handle-attribution-realclass-id-sweep (ADVERSARIAL DISCOVER/PLANNER)

**Date:** 2026-06-20  **Baseline:** main @812e1df (8314 matched, FIXED for all W9 agents)
**Mode:** read-only in main; ground truth from dtk-split target asm
(`build/45410914/asm/*.s`), `auto_*` COFF VAs, `scripts/target_symbol_map.json`,
`splits.txt`/`objects.json`, `run_objdiff` on the post-prereq worktree
(`wt-w9-handle-pair-clean-char-tier-7-post-prereq` @80a2857), and the DC3/rb3-Wii oracles.
**Verdict: REAL_ACTIONABLE** — but the frontier's framing is corrected on three load-bearing
points (the keystone is NOT a clean map-add; the LARGE Handles are NOT mis-pins; the
"~30 mis-pins" collapses to ~4 genuinely-actionable items + 1 hard structural case).

---

## TL;DR — what the frontier got RIGHT / WRONG

- **RIGHT:** the census of 107 wired `::Handle` bodies (`/tmp/wired_handles2.json`)
  DOES contain rows where the owning-`.s` name != the body's real class. The
  super-forward `bl` chain (the 2nd non-helper `bl` after `fn_82725EE8`=`DataNode::Sym`
  head) IS the discriminator: `HANDLE_SUPERCLASS(X)` compiles to `bl X::Handle`, so the
  forward target's class is the body's real base — if that base is incompatible with the
  owning-`.s` class hierarchy, the row is mis-attributed.
- **WRONG #1 — the LARGE "real Handles pinned elsewhere" are NOT mis-pins.**
  `Part fn_82437110` (893i), `AsyncFileHolmes fn_82525CE0` (891i), `DirLoader` 665i =
  `fn_827371D8` — checked all three:
  - **Part** forwards `RndDrawable→PropAnim→RndTransformable→RndPollable→Hmx::Object`
    (its real base chain) and is INSIDE `Part.cpp`'s pin `[0x82433730,0x82439D8C)`,
    defined in `Part.s`. **Correctly pinned.** Not a mis-pin.
  - **AsyncFileHolmes** `fn_82525CE0` is INSIDE `AsyncFileHolmes.cpp`'s pin
    `[0x825221D0,0x82527920)`. **Correctly pinned.** Not a mis-pin.
  - **DirLoader 665i** `fn_827371D8` is the **keystone** — see WRONG #2.
- **WRONG #2 — the "DirLoader 665i Handle" IS `Hmx::Object::Handle`, displaced into
  DirLoader's pin.** `fn_827371D8` is a **leaf Handle**: it interns the "get"/"set"
  message Symbols (`lbl_82102010`, `lbl_82101FEC+0x18`), dispatches via own handlers, and
  terminates via the **PathName unhandled tail** (`bl fn_82732F68; li r11,6; stw`). That is
  the signature of the root `Hmx::Object::Handle` (`BEGIN_HANDLERS(Hmx::Object)` in
  `src/system/obj/Object.cpp:210`, `HANDLE(get,OnGet)/HANDLE(set,OnSet)/…`). It is at
  `0x827371D8`, which falls inside `DirLoader.cpp`'s pin `[0x8272FF10,0x82737FE8)`, so the
  census mislabeled it "DirLoader". **This single fact also rewrites the L8 super-VA table
  ("fn_827371D8 = Hmx::Object::Handle") to a correct claim — but the label "DirLoader"
  in the census is the mis-pin.** This is a STRUCTURAL displaced-cluster, NOT a free
  map-add (see §"Keystone" below — it is the one HARD case).
- **WRONG #3 — the "trap" smalls are ICF/interleave aliases, not relocatable bodies.**
  `CharEyeDartRuleset fn_823ABFD8`, `UIGuide fn_828020D0`, `Flow fn_8229D0E0`,
  `CharIKFoot`'s super `fn_82383CF8`, `PropKeys fn_8264B118` are bodies of a DIFFERENT
  class merged (ICF) or interleaved into the named `.s`. The real fix for the
  source-divergent two (FlowIf, real-UIGuide `fn_82803500`) is a 1-line
  `HANDLE_SUPERCLASS` correction; the rest are "do NOT pair blind by owning-.s name".

## HARD PREREQUISITE (gating dependency — unchanged from L5/L6/L7/L8)

Every Handle-pairing item is **blocked** until the Family-B macro reconcile lands
(`w9-land-reconcile-handle-prereq-9fb9016` @a7175af = ONE commit on main@812e1df,
"+196 @100%"). On main, `Object.h` still emits the timer head + sizeof-stripped END tail,
so NO `::Handle` body is byte-exact and any map entry reads 0%. **A self-contained item
must carry the a7175af diff** (or branch off it). I verified ground truth on the
post-prereq worktree `wt-w9-handle-pair-clean-char-tier-7-post-prereq` @80a2857
(a7175af + the 5 clean char pairings; report = 8319).

## Ground-truth method (reproducible)

- Census: `/tmp/wired_handles2.json` (107 rows: unit, fn VA, instr, full `bl` chain).
- Super-forward VA → class map (resolved from the bodies + `target_symbol_map`):
  `fn_827371D8`=**Hmx::Object::Handle** (NOT a generic helper — it's a real leaf Handle),
  `fn_827D9928`=UIComponent, `fn_823E7E40`=RndTransformable, `fn_823F47C0`=RndDrawable,
  `fn_8240E828`=RndPollable, `fn_825BE6A8`=CharWeightable, `fn_823EF728`=RndPropAnim,
  `fn_823F2538`=RndAnimatable(Anim), `fn_8272B6C0`=ObjectDir, `fn_824B1960`=CamShot,
  `fn_82431B18`=RndTexRenderer, `fn_8229BA18`=Flow(real). Helpers (NOT supers):
  `fn_82725EE8`=DataNode::Sym head, `fn_82732F68`=PathName tail, `fn_8279B788`=Symbol(char*),
  `fn_82725930/82260570`=DataNode helpers, `fn_82804DA8`=__RTDynamicCast.
- Pin ownership: parse `.text start:/end:` in `splits.txt`; bisect each fn VA.
- Byte-exactness: `run_objdiff` on the post-prereq worktree.

## CENSUS CLASSIFICATION (107 rows → genuine attribution issues)

The vast majority of census rows are **correctly pinned** (owning-`.s` == real class):
all the Char* (CharFaceServo/CharSleeve/CharPosConstraint/CharIK*/BandIKEffector/Waypoint),
the big game/engine Handles (Part/AsyncFileHolmes/PlatformMgr/UIList/UIScreen/Character/
Dir/Anim/Draw/UIComponent/CameraShot/BandCamShot/…), and the L8 clean-7 tier. The genuine
attribution issues are:

### CAT-A — source-super divergence (1-line fix + per-fn verify), POST-PREREQ
| census row | fn | tgt-fwd | src has | fix | oracle |
|---|---|---|---|---|---|
| FlowIf | fn_823B4E30 | **Hmx::Object** (fn_827371D8) | `HANDLE_SUPERCLASS(FlowNode)` (FlowIf.cpp:13) | → `HANDLE_SUPERCLASS(Hmx::Object)` | DC3 agrees w/ FlowNode → retail divergence; DC3-only oracle |
| UIGuide (real) | fn_82803500 (93i) | **UIComponent** (fn_827D9928) | `HANDLE_SUPERCLASS(Hmx::Object)` (UIGuide.cpp:41) | → `HANDLE_SUPERCLASS(UIComponent)` | rb3-Wii UIGuide |

Both are inside their TU's pin (FlowIf `[0x823B4C20,0x823B523C)`, UIGuide `[0x82801070,
0x82804770)`). FlowIf is a pure-forward (52i) → near-clean flip once super is right.
UIGuide-real has OWN handlers (93i) → PER-FN verify (objdiff each). **Must verify the
super's own Handle (FlowNode::Handle / UIComponent::Handle) does NOT regress** — the fix
changes which super `bl` is emitted; if FlowNode::Handle is itself a pure Object forward
that ICF-folds onto Hmx::Object::Handle, retail picked Object's copy → our `bl
Hmx::Object::Handle` matches; verify.

### CAT-B — ICF/interleave aliases (DO NOT pair blind; real class is a different VA)
| census row (owning .s) | fn | real class (per body) | note |
|---|---|---|---|
| UIGuide (small) | fn_828020D0 (48i) | a UIComponent-derived (≠ UIGuide) | real UIGuide::Handle is fn_82803500; this 48i body is a different UI class merged into UIGuide.s |
| Flow (small) | fn_8229D0E0 (48i) | TexRenderer-range fwd (≠ Flow) | real Flow::Handle is fn_8229BA18 (80i, own actions); both in Flow.s pin |
| CharEyeDartRuleset | fn_823ABFD8 (67i) | RndTransformable-derived (≠ CharEyeDartRuleset=Hmx::Object only) | 2nd CharEyeDartRuleset range [0x823ABAE8,0x823ACF28) also holds `??_ERndEnviron` — fn is RndEnviron-family, ICF/interleave |
| CharIKFoot's super | fn_82383CF8 (89i) | **CharIKHand::Handle** (fwds CharWeightable) | in FileMerger.s; CharIKFoot→CharIKHand→CharWeightable chain; the census also lists it as a "FileMerger" Handle — it is CharIKHand's, displaced |
| PropKeys (2nd range) | fn_8264B118 (50i) | ambiguous (Keys<>/StarsDisplay cluster) | 2nd PropKeys range [0x82649C38,0x8264B5F8) is a template/sub-class cluster; class-ID needed |

CAT-B rows are NOT relocatable map-adds: their real-class named Handle is a DIFFERENT VA
(often already the right body), and the census VA is an ICF address-alias or an
interleaved sibling. Pairing the census VA to the owning-`.s` class name would be a FALSE
attribution. **These are refuted as actionable pins** — the value is recognising them as
traps so a downstream pairing wave does not mis-pair them (the L8 CharEyeDartRuleset trap
generalises).

### CAT-C — STRUCTURAL displaced-cluster (the keystone; HARD, NEEDS_DEEPER)
`Object.cpp`'s entire dispatch cluster is displaced into `DirLoader.cpp`'s pin:

| Object.cpp method | VA | currently attributed to |
|---|---|---|
| Hmx::Object::Handle | **0x827371D8** (UNMAPPED) | DirLoader pin |
| InitObject | 0x82733668 | DirLoader pin |
| DataDir | 0x827358A8 | DirLoader pin |
| SaveType | 0x82735B40 | DirLoader pin |
| Save | 0x82735F98 | DirLoader pin |
| HandleType | 0x82735FE0 | DirLoader pin (mapped, reads **Stub/32-insert** — objdiff builds DirLoader.obj!) |
| HandleProperty | 0x827363D8 | DirLoader pin |
| PropertyClear | 0x827370A0 | DirLoader pin |
| ctor/dtor/RegisterFactory/Load/Replace | 0x82737FE8.. | Object.cpp's OWN sliver pin [0x82737FE8,0x82738160) |

`Object.cpp` is pinned to a tail sliver `[0x82737FE8,0x82738160)` (ctor+dtor only;
report shows `default/Object` = 2 functions). Its real dispatch cluster `[0x82733668,
0x827385D0]` STRADDLES the DirLoader/Object boundary at 0x82737FE8 — the lower 8 methods
(incl. Handle) are inside DirLoader's pin. **PROOF:** `run_objdiff` for
`?HandleType@Object@Hmx@@…` on the prereq worktree reports `Stub (High) — 32 insert` and
the build log says `Building incremental: …/DirLoader.obj` — dtk attributes the
0x82735FE0 target VA to **DirLoader.obj**, while our compiled HandleType lives in
**Object.obj** (the sliver). They never pair.

**Why this is HARD (not a clean shared-boundary move):** the boundary zone
0x82732F00–0x82733860 is a genuine **COMDAT interleave** — file-scope free functions
(`PathName` 0x82732F68, `SafeName` 0x82733060, `IsASubclass` 0x82733160,
`SubDirStringUsed` 0x827331D0, `_S_merge` 0x82733580, **InitObject@Object** 0x82733668,
`SubDirHashUsed` 0x82733860) ALTERNATE between Object.cpp/DirLoader.cpp ownership. A single
pin boundary cannot separate them; per-function `.text$xx` sub-pinning would be required,
which is risky and likely net-negative on the honesty gate (foreign-fn runs). **Emitted as
discovered_frontier (NEEDS_DEEPER), not a clean actionable item.** The potential prize is
real (Object::Handle + the 7 displaced methods + their funclets ≈ +8..+15 if cleanly
un-interleaved on the prereq base), but it needs a per-function pin-surgery tool + an
honesty audit, not a one-shot pin edit.

## Actionable (self-contained, ONE worktree each, carry the a7175af prereq diff)

Both CAT-A items branch off the prereq tip (or carry a7175af) so they land independently
vs main@8314. Each: 1-line `HANDLE_SUPERCLASS` source edit + the matching
`target_symbol_map.json` pairing + `run_objdiff` verify on the changed fn AND the super's
own Handle + whole-binary A/B. attribution_risk=TRUE.

1. **FlowIf super → Hmx::Object** (est +1..+2): `src/system/flow/FlowIf.cpp:13`
   `HANDLE_SUPERCLASS(FlowNode)` → `HANDLE_SUPERCLASS(Hmx::Object)`; pair
   `0x823B4E30 -> ?Handle@FlowIf@@…`. Verify FlowNode::Handle's own pairing unchanged
   (it should: if FlowNode::Handle is a pure Object-forward it ICF-folds, so retail's
   FlowIf `bl`s Object directly). DC3-only oracle.
2. **UIGuide-real super → UIComponent** (est +1): `src/system/ui/UIGuide.cpp:41`
   `HANDLE_SUPERCLASS(Hmx::Object)` → `HANDLE_SUPERCLASS(UIComponent)`; pair
   `0x82803500 -> ?Handle@UIGuide@@…`. PER-FN (has own handlers) — objdiff-verify the
   full 93-instr body, not just the super `bl`. rb3-Wii oracle agrees on the class.

(Both are tiny; their value is also de-trapping the census so a later pairing wave does
not mis-pair `fn_828020D0`/`fn_8229D0E0` as UIGuide/Flow.)

## Honesty / attribution cautions

- **attribution_risk=TRUE on every pairing/relocation.** Verify byte-exact post-pair via
  `run_objdiff` (the wrong VA→name pair reads 0%, self-validating; but a RIGHT-VA wrong-CLASS
  pair could read 100% on an ICF-alias body → confirm the body's super `bl` matches the
  CLASS you're naming it, not just any 100%).
- The CAT-B rows are the WAYPOINT lesson in miniature: a census row whose owning-`.s`
  differs from the body's super-class is EITHER a relocatable mis-pin (CAT-C, rare) OR an
  ICF/interleave alias (CAT-B, common). Default to CAT-B and demand a forward-`bl`-class
  match before pairing.
- The keystone (CAT-C) MUST NOT be "fixed" by a naive DirLoader-pin shrink — the
  interleave means a shrink harvests DirLoader funclets into Object and vice-versa.
