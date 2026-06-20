# W9-L7: "Family A HANDLE_CHECK comma-form" — REFUTED

Date: 2026-06-20. Baseline: main @ 8314 matched. Mode: adversarial discover (read-only main).

## The frontier claim

> 37 ObjMacros.h-style TUs (only GuitarController patched). ObjMacros.h:210-218 puts
> PathName eval in a separate HANDLE_CHECK(line_num) macro (comma-form). A global
> HX_NATIVE-gated HANDLE_CHECK comma-form fix does for Family A what the reconcile
> prereq did for Family B. Est +15.

## What "the comma-form" actually is (mechanism, corrected)

It is NOT a change to the `HANDLE_CHECK` macro in `ObjMacros.h`. There is no comma in
`HANDLE_CHECK` (rb3-Wii ObjMacros.h:210, rb3-xenon:191) — it is plain
`if (_warn) MILO_WARN("%s(%d): %s unhandled msg: %s", __FILE__, line_num, PathName(this), sym);`,
byte-identical between rb3-Wii and rb3-xenon.

The real lever is a **per-TU `MILO_WARN` override**:
```cpp
#ifndef HX_NATIVE
#undef MILO_WARN
#define MILO_WARN(...) ((void)(__VA_ARGS__))   // comma form: EVALUATES args
#endif
```
applied at the top of a `.cpp`. It exists today in exactly 3 TUs: `GuitarController.cpp`,
`Gem.cpp`, `rndobj/Utl.cpp` (grep `MILO_WARN(...) ((void)(__VA_ARGS__))`).

The global form (`src/system/os/Debug.h:149`) is the OPPOSITE:
`#define MILO_WARN(...) ((void)sizeof(MakeString(__VA_ARGS__)))` — `sizeof` strips ALL
arg evaluation (no `PathName(this)` vcall), compiling WARN to NO code. Debug.h:138-145
documents that this global no-op is exactly what delivers the historical **+23** ("engine
logging-heavy TUs like MidiParser jump"). The two forms are **mutually exclusive per TU**
and have OPPOSITE codegen. A "global comma-form" would therefore *undo the +23*, not add to it.

So the premise of a single global HX_NATIVE-gated comma-form fix is structurally
self-contradictory: it would flip every WARN call site in the binary from "no code" to
"args evaluated," regressing the +23 cluster.

## Ground-truth refutation (the decisive evidence)

Only **3 paired Handle functions exist in the ENTIRE binary**, and ZERO are at 100%
(report.json, `::Handle(class DataArray`):

| TU | Handle norm% | residual cause (objdiff) |
|----|------|------|
| GuitarController (engine, *already comma-form*) | 97.8% | BOOL_MASK[24] + OFFSET_SWAP (0x60,0x64)/(0x58,0x5c) — PERMUTER-class, NOT HANDLE_CHECK |
| DancerSequence (engine) | 20.0% | 199 *deleted* insns = massive body-port gap; no HANDLE_CHECK in source |
| UIManager (`default/UI`) | 0.3% | UIManager.cpp not even in the source tree (unwired/auto-blob) |

The "already-patched" reference TU (GuitarController) is itself **NOT matched** — comma-form
got the HANDLE_CHECK/PathName region matching, but the function sits at 97.8% on unrelated
bool-materialization + stack-slot codegen. So even the canonical success case is a near-miss
that the comma-form alone never closes.

### Why the other ~34 "Family A" TUs cannot register a gain

Every game Handle function (Player, GamePanel, GameMode, VocalTrack, MusicLibrary,
OvershellSlot, CalibrationPanel, VocalPlayer, AccomplishmentManager/Panel/Progress,
TrainerPanel, GemTrack, QuestFilterPanel, TourDescPanel, UIEventMgr, NetSync, …) is an
**anonymous `fn_<addr>`** — none appear with a demangled `::Handle` name in report.json,
none are in `scripts/target_symbol_map.json`, and the authoritative retail COFF
(`auto_03_82260000_text.obj`, 107,917 symbols) contains ZERO mangled C++ names — it is
fully `fn_<hex>` (65,170 of them; `?Handle@` count = 0). A function that is not paired
to a named target cannot move `measures.matched_functions`. The comma-form override would
change anonymous bytes that nothing is measuring.

To make any of these TUs' Handle register a match you would FIRST need: pin the Handle
VA inside the TU's `.text` span (it currently isn't — e.g. Player.cpp is pinned
0x826843F0–0x8268C4A8 and `Player.s` has no `Handle@Player` fn) AND add a
`target_symbol_map.json` entry. That is a pin/relocation work-item in its own right
(attribution-risk), and only AFTER pairing could you tell whether HANDLE_CHECK is even
the residual — and the one data point we have (GuitarController) says it usually is NOT.

## Verdict

**REFUTED.** The "+15 global comma-form" is not real:
1. The mechanism described (an ObjMacros.h HANDLE_CHECK edit) does not exist; the lever is
   a per-TU MILO_WARN override whose global form would regress the +23 WARN-no-op cluster.
2. Only 3 Handle functions are paired binary-wide; none reach 100%, and the residuals are
   permuter-class / body-port — not the HANDLE_CHECK region.
3. The other ~34 TUs' Handle functions are unpaired anonymous `fn_` (retail stripped all
   names); a body-text change there moves nothing measurable without a prior pin+map work-item.
4. The "already patched" reference (GuitarController) is itself a 97.8% near-miss, proving
   comma-form ≠ a clean match.

## Non-dead adjacent leads (seed later layers)

- **GuitarController::Handle 97.8% → 100% (permuter):** residual is BOOL_MASK[24] +
  OFFSET_SWAP (0x60,0x64)/(0x58,0x5c). Pure permuter/decl-reorder, comma-form already in
  place. A `/permute` + stack-slot-inversion attempt is the only concrete, paired,
  near-100 Handle in the binary. Independent, no pin risk.
- **Pin+map the game Handle functions (PREREQUISITE vein, not this item):** Player/GamePanel/
  GameMode/etc. Handle are anonymous fn_ inside (or adjacent to) already-pinned game TU spans.
  A pin-audit-style sweep (tools/pin_audit.py) to bring each Handle VA into its owner TU's
  span + add target_symbol_map entries is the actual unlock — and only then is "is HANDLE_CHECK
  the residual?" even a question. This is the `handle-checked-store-order` / pin vein the
  frontier item gestured at; it is attribution-risk pin work, separate from any macro edit.
- **Audit the 3 comma-form TUs for correctness, not count:** Gem.cpp / Utl.cpp use comma-form
  for the GetMeasureMap/other vcall-survival reason, not Handle. Confirm none is silently a
  semantic corruption (the comma-form keeps side effects, so it's the safe direction, but
  it's worth a one-time check that no `MILO_WARN`-only TU got the override and lost the +23).
