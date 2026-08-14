# `config.json`

> **STATUS (2026-07-06):** HISTORICAL dtk-template doc, generic format explanation
> still accurate. See "This repo" section below for rb3-xenon's actual values —
> `config/45410914/config.json`.

This file contains the progress categories and the compiler flags for your project.

## Format

```json
    "progress_categories": {
        "sdk": "XDK Code"
    },
    "asflags": [],
    "ldflags": [],
    "cflags": {
        "base": {
            "flags": [
                "/nologo",
                "/c"
            ]
        }
    }
```

- `"progress_categories"` The different progress categories for your project. Useful for tracking progress for game specific code, engine code, XDK code, etc. These will show up when your project finishes building and reports the progress percentages.
- `"asflags"` Leftover from dtk-template, goes unused.
- `"ldflags"` Leftover from dtk-template, goes unused.
- `"cflags"` Your project's compiler flags. Set the main compiler flags for your project as a whole in `base`, and then other sections can build off of it. So for example, you can have something like below, where you have a `base` compiler flag setup, and sub-configurations `engine` or `xdk` that build off the `base` flag set.

```json
    "cflags": {
        "base": {
            "flags": [
                "/nologo",
                "/c",
                "/GR",
                "/O1"
            ]
        },
        "engine": {
            "base": "base",
            "flags": [
                "/O2"
            ]
        },
        "xdk": {
            "base": "base",
            "flags": [
                "/Zi"
            ]
        }
    }
```

## This repo

rb3-xenon's actual file is `config/45410914/config.json` (not `config.json` —
paths are keyed by title ID, `45410914`). Verified contents as of 2026-08-14:

```json
"progress_categories": { "game": "Game Code", "engine": "Milo Engine Code",
                          "thirdparty": "Third-Party Libraries",
                          "sdk": "XDK Code", "network": "Quazal Network Code" },
"asflags": [], "ldflags": [],
"cflags": { "base": { "flags": ["/nologo", "/wd4355", "/wd4164", "/c",
                                 "/GR", "/O1", "/Oi", "/EHsc"] },
            "curl": { "base": "base", "flags": ["/TC", "/GS", "/D_XBOX360", "/DCURL_STATICLIB"] } }
```

Confirmed `"asflags"`/`"ldflags"` really do go unused here as the generic doc
above says: this project's build (`tools/project.py`) never emits a linker or
assembler edge for the X360/XEX target (there's no `mwld`-style link step —
we diff compiled `.obj`s directly against dtk-split target objects), so both
stay `[]`. The `/O1 /Oi /GR /EHsc` base flags and their rationale (retail
size-optimized release, no LTCG) are documented in `CLAUDE.md` under
"Optimization level". See `config/45410914/objects.json` (via `docs/objects.md`)
for how individual `.cpp` files pick a `cflags` set (mostly `"base"`; a few
opt into `"curl"` or add `extra_cflags` like `/Od`).

⚠ **A `progress_category` is NOT set here per object, and it is NOT the library
group's tag either — it is DERIVED FROM THE SOURCE PATH** by
`tools/source_category.py`, called from `configure.py` for all 1,434 declared
objects (lane CATTAG-1; `thirdparty` added by VENDTIER-1). The group tags still
in `objects.json` are a fallback used only when the classifier returns `None`,
which is currently true for zero objects. **To change a file's tier, move the
file** — editing a tag does nothing. Adding an id here without a matching rule
in `source_category.py` therefore produces a category no object can ever land
in; adding a rule without an id here makes `tools/project.py` hard-fail with
"Progress category '<id>' missing from config.progress_categories".

⚠ The tiers are **not** a partition of the library groups. `thirdparty` cuts
ACROSS them: the vendored libs compile with the engine's flags and are listed in
the `engine` group, so that group is legitimately tier-heterogeneous (732
`engine` + 28 `thirdparty`, plus 2 in `curl`). A library group is a **cflags**
grouping; do not read it as a tier.
