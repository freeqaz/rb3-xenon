# Port handoff — BeatMatchController.cpp (rb3-xenon, MSVC PPC-Xenon)

Branch: `wt-beatmatchcontroller` (based on checkpoint `594d0d4`)
TU: `src/system/beatmatch/BeatMatchController.cpp` (NonMatching, RB3 game/engine boundary)

## Result: +1 strict byte-100, +2 confirmed-identity fuzzy pins, compiles clean

A prior agent salvaged the port (5 member functions: `ButtonToSlot(btn,arr)`,
`ButtonToSlot(btn)`, `RegisterHit`, `RegisterRGStrum`, `IsOurPadNum`) and
checkpoint-committed source + splits + `objects.json` with **0 map pins** and
some mis-bracketed splits. This FINALIZE pass verified every target against the
binary, corrected the splits, pinned the confirmed identities, and dropped the
ICF-stub thunks.

## Ground-truth reconciliation (the task's target labels were imprecise)

Disassembling `orig/45410914/band.exe` shows the real cluster (BeatMatchController's
members are contiguous around 0x8276Axxx–0x8276Bxxx; the task's `0x82675148`
"RegisterHit" and `0x8276ade8` "RegisterRGStrum" labels were wrong — `0x82675148`
is a different class (sink@0x1c), `0x8276ade8` is `RegisterKey`). Member offsets
in the header (Wii-era comments) are +0xC vs the binary because retail X360
`Hmx::Object` is 0x28 bytes; the header member *order* is correct and compiles to
the right offsets (verified: mUser@0x28, mLefty@0x30, mHitSink@0x38, mSlots@0x3c).

| fn | VA | size | .pdata | notes |
|---|---|---|---|---|
| `ButtonToSlot(JoypadButton,const DataArray*) const` (private) | 0x8276B300 | 132 (0x84) | 0x82236810 | |
| `ButtonToSlot(JoypadButton) const` (public virtual) | 0x8276B388 | 112 (0x70) | 0x82236818 | |
| `IsOurPadNum(int) const` | 0x8276AE60 | 136 (0x88) | 0x822367C8 | |
| `RegisterHit(HitType) const` (thunk) | 0x8276ADC0 | 36 (0x24) | none | ICF stub-fold |
| `RegisterRGStrum(int) const` (thunk) | 0x8276AE38 | 36 (0x24) | none | ICF stub-fold |

## Pinned — 3 (all disasm-confirmed identities, added to target_symbol_map.json)

| addr | fn | size | norm% | status |
|---|---|---|---|---|
| `0x8276B388` | `?ButtonToSlot@BeatMatchController@@UBAHW4JoypadButton@@@Z` | 112B | **100.0000** | TRUE byte-100 (strict) |
| `0x8276B300` | `?ButtonToSlot@BeatMatchController@@QBAHW4JoypadButton@@PBVDataArray@@@Z` | 132B | 99.697 | byte-exact modulo one unnamed callee reloc |
| `0x8276AE60` | `?IsOurPadNum@BeatMatchController@@QBA_NH@Z` | 136B | 96.765 | real codegen near-miss (permuter-class) |

- **ButtonToSlot(btn) public @ 0x8276B388** — 100% byte-equal, target_size==base_size==112.
  Not in `icf_aliases.map`, unique real body (>44B). ICF verdict HONEST.
- **ButtonToSlot(btn,arr) private @ 0x8276B300** — 99.70%; the ONLY mismatch is
  `bl fn_82726078` (target, unnamed) vs `bl ?Int@DataNode@@QBAHPBVDataArray@@@Z`
  (base) at two call sites. Same physical target (0x82726078 = `DataNode::Int`,
  proven: `DataArray::Int(int i){ return Node(i).Int(this); }` inlines the call).
  The encoded instruction bytes are identical — this is a symbol-naming artifact,
  not a code diff. Would reach 100% if 0x82726078 were named globally; deliberately
  NOT pinning `DataNode::Int` here (binary-wide symbol change, needs whole-binary
  verification out of scope for this TU).
- **IsOurPadNum @ 0x8276AE60** — 96.76%; genuine codegen near-miss. The ternary
  result flows through r11 + an extra `clrlwi r3,r11,24` bool-normalization (base
  140B) vs the target computing directly into r3 (136B). Register-allocation /
  bool-materialization — permuter-class, source-immune (a `&&` rewrite regressed
  it to 76%). Kept faithful ternary source.

## NOT pinned — 2 thunks (ICF stub-folds, dropped from splits)

`RegisterHit` (0x8276ADC0, vtable+4/Hit) and `RegisterRGStrum` (0x8276AE38,
vtable+16/RGStrum) are 36-byte `if(mHitSink) mHitSink->slot(x)` tail-thunks
(≤44B). They are ICF-folded — `0x8276ADC0` is already pinned to
`XAUDIO2::CX2SourceVoice::OnVoiceProcessingPassEnd` and the sibling
`RegisterRGFret`@0x8276AE10 to `XAUDIO2::...::OnPacketStart`. Pinning them would be
ICF-alias inflation and would collide with the existing XAUDIO2 pins. Their split
ranges were removed; bodies remain in the .cpp (required to compile + they emit
the pinned functions' context).

## Files changed (staged)
- `config/45410914/splits.txt` — removed the two thunk `.text` ranges; kept 3 pdata + 3 real `.text` ranges.
- `scripts/target_symbol_map.json` — ADD-ONLY, +3 pins (address order, no other entry touched).
- `src/system/beatmatch/BeatMatchController.cpp` + `config/45410914/objects.json` — from checkpoint, unchanged this pass.

## Gates
- **compiles**: yes (X360 MSVC, warnings only — the expected va_list/frsqrte intrinsic warnings).
- **splits overlap self-check**: 0 `.text` / 0 `.pdata` overlaps across the whole file.
- **ICF**: HONEST — the only 100% pin (0x8276B388, 112B) is a unique real body, absent from `icf_aliases.map`.
- All source (5 functions) rides in as NonMatching per the partial-counts directive.

## Leads (future)
- Naming `0x82726078 = ?Int@DataNode@@QBAHPBVDataArray@@@Z` (globally) closes
  ButtonToSlot(btn,arr) to 100% and likely lifts other units that call DataNode::Int
  — but it's a binary-wide symbol add; verify whole-binary before landing.
- `IsOurPadNum` 96.76% is a permuter-class ternary-result regalloc; try the source
  permuter before any hand re-attempt.
- The `NewController` factory + controller subclasses (JoypadController,
  RealGuitarController, …) are out of scope; a fuller TU port could pin more of the
  0x8276Axxx cluster.
