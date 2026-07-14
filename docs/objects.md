# `objects.json`

> **STATUS (2026-07-06):** HISTORICAL dtk-template doc — the "left blank for
> now" line is WRONG for this repo: `config/45410914/objects.json` is heavily
> populated (~950 lines, hundreds of `.cpp` entries across `main`/`engine`/
> other groups) and has been for a long time. See "This repo" below.

This file contains the object configurations for your project. It is left blank for now - add objects you are decompiling here as needed.

## Format

```json
    "main": {
        "progress_category": "your-category",
        "mw_version": "X360/16.00.11886.00",
        "cflags": "base",
        "objects": {
            "path/to/file1.cpp": "MISSING",
            "path/to/file2.cpp": "MISSING"
        }
    }
```

- `"main"` The type of objects being configured here. The example above is for `main`, but you can add others as you see fit for your project (`engine`, `xdk`, etc).
- `"progress_category"` The category from `config.json` that this object type will count towards.
- `"mw_version"` The X360 compiler version.
- `"cflags"` The compiler flags to use for this object type.
- `"objects"` The different objects that make up this object type.

For additional reference on how this json should be formatted, feel free to use [the ongoing DC3 decomp's json](https://github.com/rjkiv/dc3-decomp/blob/main/config/373307D9/config.json) as an extra resource.

## This repo

The real file is `config/45410914/objects.json` (title-ID-keyed path, not
`objects.json` at the config root). Verified structure as of 2026-07-06 — it
has (at least) two top-level groups, both using `"mw_version":
"X360/16.00.11886.00"`:

- `"main"` — `progress_category: "game"`, `cflags: "base"` — e.g. `Main.cpp`,
  `Memory_Xbox.cpp`, `keygen_xbox.cpp` (the latter opts into
  `"extra_cflags": ["/Od"]` via the object-level form, not just the bare
  `"NonMatching"` string form).
- `"engine"` — `progress_category: "engine"`, `cflags: "base"` — hundreds of
  `src/system/...` entries (math, bandobj, etc.), matching
  `config.json`'s `progress_categories` (`game`/`engine`/`sdk`/`network`; see
  `docs/config.md`).

Per `CLAUDE.md`: **new files being added for matching go in here as
`"NonMatching"`** (plain string or the `{"status": ..., "extra_cflags": [...]}`
object form shown above), and only take effect for objdiff comparisons once
they also have a pinned `.text` range in `config/45410914/splits.txt` — see
the "Splits-bootstrap recipe" in `CLAUDE.md` and `docs/splits.md` (unchanged,
verified accurate) for that file's format.
