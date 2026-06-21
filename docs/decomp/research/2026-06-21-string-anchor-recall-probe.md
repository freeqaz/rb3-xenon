# String/symbol-literal anchoring as an orthogonal recall lever (2026-06-21 probe)

**Status: feasibility probe, verdict-agnostic.** Run while the BSim seed-propagation
experiment (`2026-06-21-bsim-seedprop-densification.*`) was building. Characterizes a
SECOND, orthogonal identification signal for the scattered RB3-specific game layer.

## Why
The scattered-game-layer wall is RECALL of identification (locating each method's VA),
not body reconstruction. BSim/BinDiff are *structural* (p-code / CFG) signals — the
`gameid` experiment showed their high-precision intersection is sparse (146 fns
binary-wide, mostly singletons). **String/symbol-literal content is an orthogonal
signal**: game code is Symbol-driven and string-heavy, and a retail function that
references a distinctive literal can be tied to the named rb3-Wii source method that
references the same literal — independent of codegen similarity. This locates
string-heavy / structurally-generic methods that BSim misses (and vice versa), so
fusing the two raises recall above either alone.

## Method
- Source: `fingerprints.json` (66,838 retail fns, each `{name,size,n_insns,n_callees,
  callees,imms,strings}`; `strings` = referenced rdata literals). Built a
  `string → {referencing VAs}` index; bucketed by rarity. Usable string = `len >= 5`
  (drops `Object`-class junk).
- Cross-match: grep a sample of UNIQUE-string anchors against `../rb3/src` (the rb3-Wii
  DEV decomp, named functions) to test whether each maps to ONE source method.

## Findings (the ceiling)
Of all 66,838 retail functions:
- **3,921** reference any usable (`len>=5`) string at all — string literals are SPARSE;
  most functions reference none.
- **1,067** have a UNIQUE-string anchor (a literal referenced by exactly 1 function).
- **1,602** have a RARE-string anchor (literal referenced by <=3 functions).

⇒ String-anchoring is inherently **high-precision, low-recall** — a binary-wide ceiling
of ~1,067 cleanly-anchorable functions (minus those already matched in pinned engine
TUs). It is a COMPLEMENT to BSim, not a recall panacea.

## Cross-match precision (sample → `../rb3/src`)
| retail unique string | rb3-Wii source hit | quality |
|---|---|---|
| `coop_%s_%s%s` | `BandDirector.cpp` (one method) | ✅ clean content/format-string anchor |
| `spazz` | `BandWardrobe.cpp`, `BandCharacter.cpp` | ✅ near-unique (2 methods) |
| `is_loading_stickers`, `clear_sticker`, `cam_postproc` | `Symbols2.cpp` / `Symbols3.cpp` | ⚠ hits the Symbol DECLARATION TU (the known `fingerprint_match.py` FP), not the using method |
| `remove_midi_parsers`, `BandCrowdMeterDir`, `PitchArrowDir` | (no hit) | ⚠ absent from rb3-Wii source — RB3-360-specific feature or an RTTI/TypeDef class-name string |

## Verdict
A worthwhile **fusion input** for `tools/locator.py`, orthogonal to BSim/BinDiff, but
NOT standalone. Two precision rules required when building it:
1. **Symbol-literal anchors must resolve to the USAGE site** — grep `../rb3/src`
   EXCLUDING `Symbols*.cpp` (the declaration TU is a systematic FP). The usage method is
   the real anchor.
2. **Some retail strings have no rb3-Wii counterpart** (RB3-360-only features, RTTI
   class-name strings) — these are non-anchorable; expect them to drop out.

## Build sketch (only if pursued)
For each retail fn with a unique/rare CONTENT string: grep `../rb3/src` (excl.
`Symbols*.cpp`) for the literal → candidate source method → emit `(retail VA, source
method, anchor_string, rarity)`. Fuse with the BinDiff/BSim VA signal; calibrate
precision against the 25 known game pins (`config/45410914/splits.txt`, `src/band3`/
`src/network` stems). Feed confirmed anchors into `locator.py` as an additional
CONFIRMED source alongside BSim∩BinDiff.

## Prioritization
- If the BSim seed-propagation verdict is **GO**: build this as a precision-booster /
  recall-extender fused into the locator.
- If **NO-GO**: this becomes the primary fallback recall lever (orthogonal to the
  structural signals that failed), though its ~1,067 ceiling caps the prize.
