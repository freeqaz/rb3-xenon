# Object patchers (staged, NOT wired)

These MSVC `.obj` post-compile patchers are copied verbatim from
`../dc3-decomp/scripts/`. They rewrite compiler output to match the
name-mangling / initializer patterns of the original Xbox 360 binary
(anonymous-namespace hashes, `??__E` dynamic initializers, `$S` guard
variables, bool-parameter mangling, `??__F` atexit scope counters, register
swaps, and symbol transplants).

**Status: dormant.** rb3-xenon is still in the scaffold/compile-only phase —
every object is `NonMatching`, so there's nothing to match yet. These are
parked here for the matching phase.

To activate later, mirror dc3-decomp's wiring in `configure.py`
(`config.custom_build_rules` + `config.custom_build_steps`, ~lines 292-356)
and adjust paths for rb3-xenon's layout. Verify each patcher's assumptions
still hold against the RB3 retail binary before enabling — dc3 is a newer,
non-LTO build, so the exact byte patterns may differ.
