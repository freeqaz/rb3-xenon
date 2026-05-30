# Object patchers

These MSVC `.obj` patchers are ported from `../dc3-decomp/scripts/`. They rewrite
the COFF **symbol table** of object files — *not the machine code* — to match the
name-mangling / linkage / initializer patterns of the original Xbox 360 binary.
The point: MSVC bakes in symbols whose names are deterministic but
*environment-dependent* (anonymous-namespace hashes keyed on machine name + source
path, static-init guard names, atexit scope counters, mangling back-refs). Our
recompile picks different values than Harmonix's build did, **even for byte-identical
code**, so objdiff would report spurious diffs (or fail to pair symbols). These
patchers normalize our output to the original's naming so the diff reflects real
code differences only. (Patches are reapplied every build — they're idempotent build
steps, not committed edits to the objs.)

**Status: WIRED and active** in `configure.py` (`config.custom_build_rules` +
`config.custom_build_steps`, ~lines 284-368; mirrors dc3 `configure.py:294-357`).

## Wired

Pre-compile (operates on the dtk-split **target** obj):
- **`obj_target_symbol_renamer.py`** — rewrites the target's anonymous `fn_<addr>`
  symbols to MSVC mangled names from `target_symbol_map.json`, so objdiff can pair
  target↔base by name at all. Map entries come from `tools/fingerprint_match.py
  gen_target_map` (engine, from `unified_id.json`) and `tools/gen_game_target_map.py`
  (game, from the rb3-Wii oracle `unified_id_rb3wii.json`). A pinned TU with no map
  entry reads a false 0%.

Post-compile (operates on **our compiled** obj):
- **`obj_anon_ns_patcher.py`** — anonymous-namespace hashes (`?A0x........@@`).
- **`obj_dynamic_init_patcher.py`** — `??__E` dynamic initializers, STATIC→EXTERNAL.
- **`obj_guard_patcher.py`** — `$S` static-init guard vars → `??_B` naming/numbering.
- **`obj_bool_mangle_patcher.py`** — bool-parameter back-reference mangling.
- **`obj_atexit_scope_patcher.py`** — `??__F` atexit scope counters (fuzzy match).

## Present but NOT wired (enable per-function as needed)

- **`obj_regswap_patcher.py`** — neutralizes register-allocation swaps.
- **`obj_transplant_patcher.py`** — moves/transplants symbols between objs.

Each patcher takes `--batch [--apply] [--verbose]`; without `--apply` it's a dry run.
Caveat (still true): dc3 is a newer, non-LTO build, so verify a patcher's byte/symbol
assumptions against the RB3 retail target before relying on it for a given unit.
