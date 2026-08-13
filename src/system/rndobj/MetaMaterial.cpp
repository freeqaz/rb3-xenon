// RB3-360 retail has NO `MetaMaterial` class, and no metamaterial machinery of any
// kind: no metamaterial ObjectDir, no <prop>_edit_action property system, no code
// path that could create or load one. Settled on retail bytes by lane METAMAT-1 --
// see docs/decomp/metamaterial-does-not-exist-in-rb3-retail-2026-08-13.md.
//
// This FILE therefore has nothing left to compile. It survives only as a
// unit-boundary artifact, because config/45410914/{splits.txt,objects.json} still
// name it and draining a unit's last pin is lane SPLITS-3's call, not ours.
//
// ⚠ FOR SPLITS-3: the two spans still pinned here are now ORPHANED.
//   * .text 0x824382D4-0x8243833C (104 B) -- two 32-byte funclets left behind when
//     7782ed48 moved 0x82436488-0x82438138 to Mat.cpp.
//   * .text 0x82578A90-0x82578C1C (396 B) -- our compiled counterpart was
//     ?_M_insert_overflow_aux@?$vector@W4MatPropEditAction@@..., the std::vector
//     instantiation that existed ONLY for MetaMaterial::mMatPropEditActions. With
//     the enum gone we no longer instantiate that spelling, so this row can no
//     longer pair from here. It read 96.34 fuzzy / 96.75 mpn (never 100), so nothing
//     at 100% and no matched_code is lost by that -- but the pin now needs a
//     different home.
//
// Nothing below; do not add code here without re-checking which unit owns the span.
