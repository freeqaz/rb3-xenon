Dependencies
============

> **STATUS (2026-07-06):** PARTIALLY STALE — the Linux "wibo will be
> automatically downloaded" claim below is WRONG for this repo (verified
> against `configure.py`); see "This repo" section for the real toolchain.
> The Windows/macOS sections are unverified/unused dtk-template boilerplate —
> this project is developed and built on Linux only.

Windows
--------

On Windows, it's **highly recommended** to use native tooling. WSL or msys2 are **not** required.  
When running under WSL, [objdiff](https://github.com/encounter/objdiff) is unable to get filesystem notifications for automatic rebuilds.

- Install [Python](https://www.python.org/downloads/) and add it to `%PATH%`.
  - Also available from the [Windows Store](https://apps.microsoft.com/store/detail/python-311/9NRWMJP3717K).
- Download [ninja](https://github.com/ninja-build/ninja/releases) and add it to `%PATH%`.
  - Quick install via pip: `pip install ninja`

macOS
------

- Install [ninja](https://github.com/ninja-build/ninja/wiki/Pre-built-Ninja-packages):

  ```sh
  brew install ninja
  ```

- Install [wine-crossover](https://github.com/Gcenx/homebrew-wine):

  ```sh
  brew install --cask --no-quarantine gcenx/wine/wine-crossover
  ```

After OS upgrades, if macOS complains about `Wine Crossover.app` being unverified, you can unquarantine it using:

```sh
sudo xattr -rd com.apple.quarantine '/Applications/Wine Crossover.app'
```

Linux
------

- Install [ninja](https://github.com/ninja-build/ninja/wiki/Pre-built-Ninja-packages).
- For non-x86(_64) platforms: Install wine from your package manager.
  - For x86(_64), [wibo](https://github.com/decompals/wibo), a minimal 32-bit Windows binary wrapper, will be automatically downloaded and used.
    **Not true for this repo** — see below.

## This repo (Linux, verified 2026-07-06)

rb3-xenon does **not** use the stock dtk-template auto-download path for wibo.
`configure.py` resolves four sibling forks/tools by absolute path (falls back
to an upward sibling-directory walk, so worktrees under this repo work too),
but the missing-binary behavior differs per tool (verified directly in
`configure.py`, not uniform as an earlier draft of this note claimed):

- **wibo** — `freeqaz/wibo` fork, built manually: `cd ../wibo && cmake --preset
  release && cmake --build --preset release` → binary at
  `/home/free/code/milohax/wibo/build/release/wibo`. **Hard-fails**
  (`_gate_wibo_wrapper`/`sys.exit`) if missing or not the fork build — no
  stock-wibo fallback, because a stock wibo would silently corrupt dependency
  tracking.
- **objcache** (not part of upstream dtk-template at all) — a content-addressed
  MSVC object cache, `cargo build --release` in `../objcache`. Also
  **hard-fails** if missing, unless `RB3_OBJCACHE_OPTIONAL=1` is set (then the
  msvc rule just omits the cache prefix — uncached but correct).
- **dtk** — `rjkiv/jeff` fork (RB3-retail fixes), built via `cargo build
  --release` in `../jeff`. If the local checkout isn't found, `configure.py`
  only **warns** and falls back to downloading a release build of that same
  fork (`rjkiv/jeff`) — it does not hard-fail.
- **objdiff-cli** — `freeqaz/objdiff` fork, built the same way in `../objdiff`.
  Same **warn-and-fallback** behavior (downloads a `freeqaz/objdiff` release),
  not a hard fail.

None of these four is a ninja input/edge in this repo — for reliable local
iteration on jeff/objdiff sources, build them once by hand rather than relying
on the fallback download (which serves a possibly-stale tagged release, not
your local edits). Full detail, rebuild commands, and the byte-identity
staging discipline for wibo are in `CLAUDE.md` ("Two build tracks" → track 1,
and the "wibo staging discipline" / "objcache" subsections).
