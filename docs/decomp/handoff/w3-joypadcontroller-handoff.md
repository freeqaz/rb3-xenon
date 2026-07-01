# Port handoff — JoypadController.cpp (rb3-xenon)

Branch: `w3-joypadcontroller` (from main `52ceb11`). Worktree:
`/home/free/code/milohax/rb3-xenon/.claude/worktrees/wt-w3-joypadcontroller`.
TU: `src/system/beatmatch/JoypadController.cpp` (379-line MWCC oracle → MSVC
PPC-Xenon port), wired NonMatching in `config/45410914/objects.json` (module
`engine`).

## Result
- 2 worklist targets are **true-100 byte-equal** (pinned): `ButtonToSlot`, `Disable`.
- 2 worklist targets are **honest NonMatching** (source kept, identity confirmed):
  `MapSlot` 74.2%, `ReconcileFretState` 68.9%.
- ICF verdict: **HONEST**. Neither pin (0x82776D90, 0x82776E30) appears in
  `build/45410914/icf_aliases.map` (only PoolAlloc/MemOrPoolAlloc groups are
  there) — no COMDAT fold, no stub-fold. `raw_match_percent == 100.0` for both
  pins (every instruction + relocation exact, not just fuzzy).

## PINNED (true-100, `normalized_match_percent == raw == 100.0`, byte-equal)
| VA | size | fn | .pdata |
|---|---|---|---|
| 0x82776D90 | 100 (0x64) | `?ButtonToSlot@JoypadController@@UBAHW4JoypadButton@@@Z` | 0x822371F0 |
| 0x82776E30 | 140 (0x8C) | `?Disable@JoypadController@@UAAX_N@Z` | 0x82237200 |

Mangled names extracted from the built `JoypadController.obj` COFF symtab
(machine 0x01f2, PPC BE) — not hand-guessed. `.pdata` BeginAddresses read from
`orig/45410914/band.exe` `.pdata` RUNTIME_FUNCTION table (big-endian; validated
against known SongData::Poll@0x82235728 / MakeBackupTracks@0x822350B8).

## MAPPED SUPPORT (NonMatching, identity build-gate-confirmed by 100% callers)
| VA | size | norm% | fn |
|---|---|---|---|
| 0x82776CF0 | 160 (0xA0) | 74.2 | `?MapSlot@JoypadController@@QBAHH@Z` |
| 0x827767E0 | 228 (0xE4→base 260) | 68.9 | `?ReconcileFretState@JoypadController@@QAAXXZ` |

These two are **mapped + split even though sub-100** — required so the two
genuine pins resolve their intra-TU call relocations:
`ButtonToSlot` calls `MapSlot`, `Disable` calls `ReconcileFretState`. Without the
callee symbol renamed in the target obj, objdiff (and the whole-binary report)
sees the `bl` target as name-mismatched and the caller drops to ~99.8%. Their
identities are **not guesses** — `ButtonToSlot==100` is impossible unless
0x82776CF0 is `MapSlot` (the bl target must match by name), and `Disable==100`
is impossible unless 0x827767E0 is `ReconcileFretState`. So the 100% callers act
as the build gate on the callee identities (defeats sibling-aliasing poison).
Measured drops with the callees removed from split+map: ButtonToSlot 100→99.8,
Disable 100→99.86.

## DROPPED
None. All 4 worklist addresses are handled (2 pinned, 2 mapped-support). The
full TU source (constructor + 15 methods + BEGIN_HANDLERS) is retained as
NonMatching — real progress + fuzzy% even for the two support fns.

## Port adaptation notes (MWCC → MSVC)
- Headers already wired by the landed sibling `GuitarController.cpp`:
  `BeatMatchController.h`, `BeatMatchControllerSink.h`, `os/Joypad.h`,
  `os/JoypadMsgs.h`, `os/User.h`, `os/System.h` (3-Symbol `SystemConfig`
  overload present), `utl/Symbols.h` (externs `joypad`, `controllers`,
  `pad_shift_button`, `cymbal_shift_button`, `secondary_button` in Symbols2/3/4.h),
  `obj/ObjMacros.h` (`BEGIN_HANDLERS/HANDLE_MESSAGE/HANDLE_CHECK/END_HANDLERS`).
- New header added: `src/system/beatmatch/JoypadController.h` (copied from oracle
  verbatim; all base types already exist in the xenon tree).
- One additive change to `src/system/os/Joypad.h`: inline
  `JoypadData::IsButtonNewlyPressed(int)` (used by `NoSlotButtonsThisFrame`;
  matches oracle exactly, `mNewPressed & 1<<i`). Purely additive.
- Numeric `(HitType)0xE/0xF/0x11/...` casts kept exactly as the oracle (matches
  MWCC/MSVC codegen best).

## Verify method (per-function, isolated unit — whole-binary report NOT run)
```
python3 configure.py && touch config/45410914/config.yml
tools/ninja-locked build/45410914/config.json           # dtk split → target obj
python3 scripts/obj_target_symbol_renamer.py --batch --apply
tools/ninja-locked build/45410914/src/system/beatmatch/JoypadController.obj
build/tools/release/objdiff-cli diff -u "default/JoypadController" '<mangled>' \
  --format json -o /tmp/x.json     # read normalized_match_percent / raw_match_percent
```

## Leads (next agent)
- `MapSlot` (74.2%, both 160B) — same size, likely a branch-shape / regalloc /
  bool-materialization delta (the lefty/drum-mapping switch). Run `/permute` or
  objdiff `run_diff_inspect mismatches` before hand-editing. Highest ROI residual.
- `ReconcileFretState` (68.9%, base 260 vs target 228 — base is 32B larger) —
  real codegen divergence (loop / fret-mask shape), structural. Diagnose first.
  NOTE the aliasing trap: a near-identical `ReconcileFretState` also lives on
  `GuitarController` @0x82777698 (~0xEB0 away) — this pin is the *JoypadController*
  one (`?ReconcileFretState@JoypadController@@QAAXXZ`), distinct mangled name.
