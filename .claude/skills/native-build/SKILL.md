---
name: native-build
description: Build the native port (x86_64 Linux, Clang). Targets include rb3-native, milo-viewer, render-test, milo-tests. Use when verifying native compilation or running native tests.
argument-hint: "[target] (default: all)"
allowed-tools: Bash(cmake *), Bash(ninja *), Read, Grep, Glob
---

# Native Build Skill

Build the native x86_64 Linux port of rb3-xenon.

## Arguments

`$ARGUMENTS`

## Build Directory

The native build uses CMake + Ninja with Clang:
- **Source**: `/home/free/code/milohax/rb3-xenon/native/`
- **Build dir**: `/home/free/code/milohax/rb3-xenon/native/build/`
- **Generator**: Ninja

## Steps

1. **Parse arguments.** The argument is an optional target name. If empty, build all targets.

   Available targets:
   - `rb3-native` — main engine executable (loads RB3 songs.dta)
   - `milo-viewer` — .milo file viewer
   - `render-test` — rendering tests
   - `milo-tests` — unit tests
   - (no target / `all`) — builds everything

2. **If build dir doesn't exist**, configure first:
   ```bash
   cd /home/free/code/milohax/rb3-xenon/native && cmake -S . -B build -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
   ```

3. **Build the target:**
   ```bash
   cmake --build /home/free/code/milohax/rb3-xenon/native/build --target TARGET -- -j$(nproc)
   ```
   Or for all targets:
   ```bash
   cmake --build /home/free/code/milohax/rb3-xenon/native/build -- -j$(nproc)
   ```

4. **If build fails**, show the first error with context. Common issues:
   - Missing `#ifdef HX_NATIVE` guards for PPC-specific code
   - STL API differences between MSVC PPC and modern Clang
   - Missing stub implementations (see `native/src/dta_link_stubs.s` for ~74 weak stubs)
   - `Symbol::Init()` must be called before interning symbols
   - Sandbox blocking Vulkan/GPU access — use `dangerouslyDisableSandbox: true`

5. **Run tests** (if target is `milo-tests`):
   ```bash
   cd /home/free/code/milohax/rb3-xenon/native/build && ctest --output-on-failure
   ```

## Tips

- The native build uses Clang with MSVC compatibility flags (`-fms-extensions`, `-fms-compatibility`)
- `HX_NATIVE` is defined for native builds — use `#ifdef HX_NATIVE` for platform-specific code
- rb3-xenon's RB3 engine is more complete than DC3's, so some DC3 `_Native` shims are redundant
- dc3-decomp's dual-target `types.h` handles LP64 int-width vs Xbox ILP32 long
- See `project_native_port.md` in Claude's memory for full build recipe and hard-won lessons
- GPU rendering requires `dangerouslyDisableSandbox: true` due to Vulkan ICD access
