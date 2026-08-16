"""Resolve the diff ruler the GRADER is actually using, at runtime.

★ Why this module exists (lane MCPRULER-1, 2026-08-14)
──────────────────────────────────────────────────────
`matched_code` moves by ~675 kB / 6.54 pp on this tree with ZERO source change,
purely by flipping `functionRelocDiffs` between `none` and `name_check`
(measured whole-binary: 4,397,412 B / 42.61% vs 3,722,476 B / 36.07%, with
`matched_functions` = 44,394 and `masked_equal` = 22,897 BIT-IDENTICAL on both
legs — the flip touches relocation-name comparison only, and `mpn` excludes
arg-only penalties).

So **a percentage without its ruler is not a measurement.** Every consumer here
must (a) score on the same ruler as `report.json`, and (b) say which ruler it
used.

The defect this replaces
────────────────────────
`scripts/orchestrator/mcp_server.py` hardcoded `-c functionRelocDiffs=none` in
four places. That was CORRECT when lane EB-4 wrote it (2026-08-03): back then
`objdiff-cli report generate`'s base config really was `None`, and `objdiff.json`
carried no `options` block to override it. On **2026-08-12 (`d04c83df`)** the
project shipped `options = {"functionRelocDiffs": "name_check"}` and the
hardcoded constant silently became a LIE about the grader.

Consequences, measured on this tree at `1f078361`:
  * **5,555 rows / 674,936 B** read `fuzzy == 100` under `none` but below 100 on
    the graded ruler. Those are rows the orchestrator reported as
    *"100.0% normalized, all equal, Complete — No action needed"* while the
    grader withheld every one of their bytes.
  * 7,157 rows disagree between the two rulers in total.

⇒ **Never hardcode the ruler again.** A second hardcoded constant would rot in
exactly the same way, on exactly the same silent schedule. Read it from the
artifact the grading run itself wrote.

Source of truth, in priority order
──────────────────────────────────
1. **`build/<version>/report.json` → `provenance.diff_config`.** This is a
   COMPLETE dump of every config key the grading run used, written by that run.
   It is authoritative by construction: it is not a description of the config,
   it IS the config. All 22 keys it emits are accepted verbatim as `-c` args
   (verified end-to-end).
2. **`objdiff.json` → `options`, layered on `report generate`'s base.** Used
   when no report has been generated yet. This reproduces the grader's layering
   (`report.rs:512` base → project `options` → unit `options` → `-c`) but cannot
   see per-unit `options` blocks, so it is labelled DERIVED.
3. **`report generate`'s base alone**, labelled a loud FALLBACK.

Why the base four are still needed here
───────────────────────────────────────
`objdiff-cli diff` and `objdiff-cli report generate` have DIFFERENT hardcoded
base configs, and neither is the schema default:

    report generate (report.rs:512)  functionRelocDiffs=None, combineData=true,
                                     combineText=true,  pool=false
    diff            (diff.rs:949)    functionRelocDiffs=DataValue, everything
                                     else = schema default
                                     (combineData=false, combineText=false,
                                      pool=true)

Both then layer the project's `options` block on top. So `objdiff.json`'s
`options` fixes `functionRelocDiffs` for both — but leaves `diff` disagreeing
with the grader on the OTHER three. Lane EB-4 measured `ppc.calculatePoolRelocations`
alone as worth up to 14.75 pp on 118 of 1,639 named sub-100 rows. That is why
source (1) — which carries all of them — is strongly preferred over "just drop
the `-c` flags".

⚠ `map_file` needs no handling here: `diff.rs:964` already loads `objdiff.json`'s
`map_file` on its own, so the ICF alias equivalences (7,174 entries) are shared
with the grader automatically.
"""

from __future__ import annotations

import json
import os
from dataclasses import dataclass, field
from pathlib import Path

# ── `objdiff-cli report generate`'s base config (report.rs:512) ───────────────
# These four differ from the schema defaults and are the de-facto scoring
# semantics of every project that runs `report generate`. Used only as the
# FALLBACK layer when no report.json provenance is available.
REPORT_GENERATE_BASE: dict[str, str] = {
    "functionRelocDiffs": "none",
    "combineDataSections": "true",
    "combineTextSections": "true",
    "ppc.calculatePoolRelocations": "false",
}

RELOC_KEY = "functionRelocDiffs"

# Ruler selectors accepted by the MCP tools.
RULER_GRADED = "graded"
RULER_NONE = "none"
RULER_DATA_VALUE = "data_value"
VALID_RULERS = (RULER_GRADED, RULER_NONE, RULER_DATA_VALUE)

_RULER_OVERRIDE = {
    RULER_NONE: "none",
    RULER_DATA_VALUE: "data_value",
}

# Memoize per (path, mtime) — report.json is ~15 MB and these tools are called
# in tight loops. Parsing it is ~0.11 s, which is cheap but not free.
_CACHE: dict[tuple[str, float], list[str]] = {}


@dataclass
class Ruler:
    """The effective diff configuration, plus where it came from."""

    reloc_mode: str                 # e.g. "name_check" / "none" / "data_value"
    config: dict[str, str]          # full key -> value map
    source: str                     # human-readable provenance
    selector: str = RULER_GRADED    # which ruler the caller asked for
    warning: str | None = None      # loud text when derived/fallback
    graded_reloc_mode: str | None = None  # the grader's mode, when overridden
    authoritative: bool = True      # False => read from no grading run

    @property
    def args(self) -> list[str]:
        """Flat `-c key=value` argv for objdiff-cli."""
        out: list[str] = []
        for k, v in self.config.items():
            out += ["-c", f"{k}={v}"]
        return out

    def label(self) -> str:
        """One-line ruler disclosure. ALWAYS render this next to a percentage."""
        if self.selector == RULER_GRADED:
            if self.authoritative:
                head = (
                    f"ruler: `functionRelocDiffs={self.reloc_mode}` "
                    "(GRADED — same as report.json)"
                )
            else:
                head = (
                    f"ruler: `functionRelocDiffs={self.reloc_mode}` "
                    "(**NOT read from a grading run** — see warning)"
                )
        else:
            head = (
                f"ruler: `functionRelocDiffs={self.reloc_mode}` "
                f"(**NOT the graded ruler** — grader uses `{self.graded_reloc_mode}`)"
            )
        return f"{head} · source: {self.source}"

    def banner(self) -> str:
        """Multi-line disclosure, including any warning."""
        lines = [self.label()]
        if self.selector == RULER_NONE:
            lines.append(
                "⚠ `none` ignores relocation NAMES: a wrong callee and a folded "
                "callee both read as equal. Percentages here are an UPPER BOUND "
                "on the graded score, never a completion proof."
            )
        elif self.selector == RULER_DATA_VALUE:
            lines.append(
                "⚠ `data_value` charges relocation ADDRESSES too, so it reads "
                "LOWER than the graded score. It is a defect-hunting ruler (a "
                "wrong `bl` callee is visible), never the graded score."
            )
        if self.warning:
            lines.append(f"⚠ {self.warning}")
        return "\n".join(lines)


def _find_report_json(project_dir: Path) -> Path | None:
    """Newest build/<version>/report.json under project_dir, if any."""
    build = project_dir / "build"
    if not build.is_dir():
        return None
    best: tuple[float, Path] | None = None
    try:
        for version_dir in build.iterdir():
            candidate = version_dir / "report.json"
            if candidate.is_file():
                mtime = candidate.stat().st_mtime
                if best is None or mtime > best[0]:
                    best = (mtime, candidate)
    except OSError:
        return None
    return best[1] if best else None


def _provenance_config(report_path: Path) -> list[str] | None:
    """`provenance.diff_config` from a report.json, memoized on mtime."""
    try:
        key = (str(report_path), report_path.stat().st_mtime)
    except OSError:
        return None
    if key in _CACHE:
        return _CACHE[key]
    try:
        with open(report_path) as fh:
            data = json.load(fh)
    except (OSError, ValueError):
        return None
    cfg = (data.get("provenance") or {}).get("diff_config")
    if not isinstance(cfg, list) or not cfg:
        return None
    cfg = [str(x) for x in cfg]
    _CACHE[key] = cfg
    return cfg


def _parse_kv(pairs: list[str]) -> dict[str, str]:
    out: dict[str, str] = {}
    for item in pairs:
        if "=" in item:
            k, v = item.split("=", 1)
            out[k.strip()] = v.strip()
    return out


def graded_ruler(project_dir: str | os.PathLike) -> Ruler:
    """The ruler `report.json` is scored on, resolved from artifacts on disk."""
    project_dir = Path(project_dir)

    # ── (1) report.json provenance — authoritative ────────────────────────────
    report_path = _find_report_json(project_dir)
    if report_path is not None:
        cfg_list = _provenance_config(report_path)
        if cfg_list:
            cfg = _parse_kv(cfg_list)
            mode = cfg.get(RELOC_KEY, "?")
            try:
                rel = report_path.relative_to(project_dir)
            except ValueError:
                rel = report_path
            return Ruler(
                reloc_mode=mode,
                config=cfg,
                source=f"{rel} `provenance.diff_config` ({len(cfg)} keys)",
                graded_reloc_mode=mode,
            )

    # ── (2) objdiff.json options layered on report generate's base ────────────
    objdiff_json = project_dir / "objdiff.json"
    if objdiff_json.is_file():
        try:
            with open(objdiff_json) as fh:
                proj = json.load(fh)
        except (OSError, ValueError):
            proj = {}
        options = proj.get("options")
        if isinstance(options, dict) and options:
            cfg = dict(REPORT_GENERATE_BASE)
            cfg.update({str(k): _as_cfg_value(v) for k, v in options.items()})
            mode = cfg.get(RELOC_KEY, "?")
            return Ruler(
                reloc_mode=mode,
                config=cfg,
                source="objdiff.json `options` + report-generate base (DERIVED)",
                graded_reloc_mode=mode,
                authoritative=False,
                warning=(
                    "No report.json found, so the ruler was DERIVED from "
                    "objdiff.json rather than read from a grading run. Per-unit "
                    "`options` blocks are invisible this way. Run `ninja "
                    "build/<version>/report.json` for an authoritative read."
                ),
            )

    # ── (3) loud fallback ─────────────────────────────────────────────────────
    cfg = dict(REPORT_GENERATE_BASE)
    mode = cfg[RELOC_KEY]
    return Ruler(
        reloc_mode=mode,
        config=cfg,
        source="report-generate base only (FALLBACK)",
        graded_reloc_mode=mode,
        authoritative=False,
        warning=(
            "Could not find report.json OR an objdiff.json `options` block under "
            f"{project_dir}. Fell back to `objdiff-cli report generate`'s base "
            "config. THE RULER IS UNVERIFIED — do not quote this percentage as "
            "the graded score."
        ),
    )


def _as_cfg_value(v) -> str:
    if isinstance(v, bool):
        return "true" if v else "false"
    return str(v)


def resolve_ruler(project_dir: str | os.PathLike, selector: str = RULER_GRADED) -> Ruler:
    """Graded ruler, optionally overridden to an explicit opt-in ruler.

    `none` and `data_value` keep EVERY other graded key identical and change
    only `functionRelocDiffs`, so a graded-vs-`none` delta isolates the
    relocation-name class cleanly — which is what `ab_measure`'s
    `control_none_shape()` depends on, and what separates a relocation-name
    issue from an instruction issue.
    """
    selector = (selector or RULER_GRADED).strip().lower()
    if selector not in VALID_RULERS:
        raise ValueError(
            f"Unknown ruler {selector!r}. Valid: {', '.join(VALID_RULERS)}"
        )
    base = graded_ruler(project_dir)
    if selector == RULER_GRADED:
        return base
    cfg = dict(base.config)
    cfg[RELOC_KEY] = _RULER_OVERRIDE[selector]
    return Ruler(
        reloc_mode=cfg[RELOC_KEY],
        config=cfg,
        source=f"{base.source} + explicit `{RELOC_KEY}={cfg[RELOC_KEY]}` override",
        selector=selector,
        warning=base.warning,
        graded_reloc_mode=base.graded_reloc_mode,
        authoritative=base.authoritative,
    )


__all__ = [
    "Ruler",
    "graded_ruler",
    "resolve_ruler",
    "REPORT_GENERATE_BASE",
    "RULER_GRADED",
    "RULER_NONE",
    "RULER_DATA_VALUE",
    "VALID_RULERS",
]


# ── selftest ─────────────────────────────────────────────────────────────────
# `python3 scripts/analysis/ruler.py --selftest [project_dir]`
#
# The REGRESSION GUARD is the point: this defect was not a logic error, it was a
# constant that stopped being true while every test kept passing. So the guard
# greps the consumers for a hardcoded `functionRelocDiffs=` and FAILS on one.
#
# ⚠ Two things this guard got wrong, and why they are wired the way they are now
# (lane task93, 2026-08-16):
#
#   1. `_CONSUMERS` are files in THIS tool's own repo, but they used to be
#      resolved against `project_dir` — the argument of the documented
#      `--selftest [project_dir]` form. Point the selftest at any project that is
#      not this checkout (the normal case: a scored game tree) and all three
#      consumer greps resolved to paths that do not exist.
#   2. A consumer that did not resolve printed `[SKIP]` and did NOT set ok=False.
#
# Together those made the regression guard self-disabling: `--selftest <project>`
# printed three [SKIP]s, then a bare `PASS`, then exited 0 — with the guard
# having read nothing. That is the SAME failure shape the guard exists to catch
# (a check that stops being true while every test keeps passing), one level up.
# So: resolve consumers against the tool's own repo, and treat a consumer that
# cannot be read as a FAILURE. There is no benign reason for one to be missing —
# if a consumer is genuinely renamed or retired, edit `_CONSUMERS`.

# scripts/analysis/ruler.py → scripts/analysis → scripts → <tool repo root>
_TOOL_REPO = Path(__file__).resolve().parent.parent.parent

_CONSUMERS = (
    "scripts/orchestrator/mcp_server.py",
    "scripts/analysis/diff_inspect.py",
    "scripts/analysis/stack_layout.py",
)


def _selftest(project_dir: Path) -> tuple[bool, list[str]]:
    import re

    out: list[str] = []
    ok = True

    def check(label: str, cond: bool, detail: str = "") -> None:
        nonlocal ok
        out.append(f"  [{'PASS' if cond else 'FAIL'}] {label}{(' — ' + detail) if detail else ''}")
        if not cond:
            ok = False

    graded = graded_ruler(project_dir)
    out.append(f"resolved: {graded.label()}")

    report = _find_report_json(project_dir)
    if report is not None:
        with open(report) as fh:
            prov = (json.load(fh).get("provenance") or {})
        declared = _parse_kv([str(x) for x in prov.get("diff_config", [])])
        check("graded config == report.json provenance.diff_config",
              graded.config == declared,
              f"{len(graded.config)} keys vs {len(declared)}")
        check("graded ruler is authoritative", graded.authoritative)
    else:
        out.append("  [SKIP] no report.json under project_dir — cannot check provenance parity")

    # Selector overrides must change EXACTLY one key, or a graded-vs-none delta
    # no longer isolates the relocation-name class.
    for sel in (RULER_NONE, RULER_DATA_VALUE):
        r = resolve_ruler(project_dir, sel)
        differing = {k for k in set(r.config) | set(graded.config)
                     if r.config.get(k) != graded.config.get(k)}
        check(f"ruler={sel} changes exactly one key",
              differing == {RELOC_KEY}, f"changed: {sorted(differing)}")
        check(f"ruler={sel} is labelled NOT graded",
              "NOT the graded ruler" in r.label())

    try:
        resolve_ruler(project_dir, "bogus")
        check("unknown ruler is refused", False, "no exception raised")
    except ValueError:
        check("unknown ruler is refused", True)

    # ★ Regression guard against the original defect.
    #
    # Consumers live in THIS tool's repo, not in project_dir — see the note above
    # `_CONSUMERS`. An unreadable consumer FAILS; it never skips, because a guard
    # that silently reads nothing is indistinguishable from a guard that passed.
    pattern = re.compile(r"""["']-c["']\s*,\s*["']functionRelocDiffs=""")
    out.append(f"consumer scan root: {_TOOL_REPO} (this tool's repo, NOT project_dir)")
    for rel in _CONSUMERS:
        path = _TOOL_REPO / rel
        if not path.is_file():
            check(f"consumer is readable: {rel}", False,
                  "not found under the tool repo — the regression guard cannot "
                  "run; fix the path or edit _CONSUMERS")
            continue
        try:
            text = path.read_text()
        except OSError as exc:
            check(f"consumer is readable: {rel}", False, f"unreadable: {exc}")
            continue
        hits = [i + 1 for i, line in enumerate(text.splitlines())
                if pattern.search(line)]
        check(f"no hardcoded ruler in {rel}", not hits, f"lines {hits}")

    return ok, out


if __name__ == "__main__":
    import sys

    argv = [a for a in sys.argv[1:] if a != "--selftest"]
    target = Path(argv[0]) if argv else Path.cwd()
    print(f"# ruler.py selftest — project_dir={target}")
    passed, lines = _selftest(target)
    for line in lines:
        print(line)
    print("PASS" if passed else "FAIL")
    sys.exit(0 if passed else 1)
