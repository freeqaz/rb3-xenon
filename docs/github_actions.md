# GitHub Actions

> **STATUS (2026-07-06):** SUPERSEDED for this repo — verified against
> `.github/workflows/build.yml` and `gh run list`. This doc's "rename
> `.github.example` to `.github`" bootstrap instructions are moot: a real
> `.github/workflows/build.yml` already exists (no `.github.example` present).
> It ran exactly once (2026-02-22, commit `40572c3`), **failed**, and hasn't
> been touched since — it predates the sibling-fork toolchain (`../jeff`,
> `../objdiff`, the `freeqaz/wibo` fork, `objcache`) that the real build now
> depends on (see `CLAUDE.md`), and its `configure.py --binutils /binutils
> --compilers /compilers` invocation doesn't reflect current `configure.py`
> usage. Treat CI as dead/unmaintained, not a source of truth for how to build.

The dtk-template original content (private build-container setup, `decomp.dev`
registration) is left below for historical reference — it describes the
*intended* CI mechanism, not rb3-xenon's current state:

- [Build Repository](#build-repository)
- [Workflow](#workflow)
- [decomp.dev](#decompdev)

## Build Repository

This repository will be used to build and store the CI build container.

> [!CAUTION]
> This repository should be **private** to avoid exposing the game's assets.

1. [Create a **private** repository from `encounter/dtk-template-build`](https://github.com/new?template_name=dtk-template-build&template_owner=encounter). A common name is your project's repository name with `-build` appended. For example, `tww-build`.

2. Once the repository is created, add your game's assets to the `orig/GAMEID` directory. (Replace `GAMEID` with your game's ID, matching the `orig` layout in your main repository.)  
    **Only include game files necessary for the build**, such as `sys/main.dol` and any `.rel` or `.sel` files.

3. Once the build container action completes, visit the package settings:  
    ![GitHub repository packages](images/github_build_repo_packages.png)  
    ![GitHub package settings](images/github_package_settings.png)

4. Under "Manage Actions access", add your project's main repository with the "Read" role:  
    ![GitHub package Actions access](images/github_package_settings_access.png)

## Workflow

`.github/workflows/build.yml` already exists in this repo (targeting the
private container `ghcr.io/rjkiv/rb3-xenon-build:main`), so the "rename
`.github.example`" step below doesn't apply — it's shown only for context on
what the workflow does:

1. ~~Rename `.github.example` to `.github`.~~ Already exists.

2. In `build.yml`, update the `container:` to point to the new [build image](#build-repository).

3. In `build.yml`, replace `GAMEID` with your game's ID. (Or list of IDs, for multi-version support.)

4. Commit and push the changes to your repository.

If everything is set up correctly, the workflow will build all versions on every push or pull request.
As of 2026-07-06 the single recorded run failed and no one has revisited it —
verify with `gh run list --workflow=build.yml` before relying on it.

## decomp.dev

Once the build workflow is running on the main branch, you can add your game to <https://decomp.dev>.

Visit <https://decomp.dev/manage/new>, select your GitHub repository and fill out the required fields.

If you have questions or issues, try asking in the [GC/Wii Decompilation Discord](https://discord.gg/hKx3FJJgrV) #decomp.dev channel.
