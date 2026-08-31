#!/usr/bin/env python3
"""Assert that `objdiff-cli diff` scores a function the same way `report generate` does.

Why this exists
---------------
`objdiff-cli report generate` and `objdiff-cli diff` carry DIFFERENT hardcoded
base configs, and neither is the schema default:

    report generate   objdiff-cli/src/cmd/report.rs:581
                      functionRelocDiffs=none, combineDataSections=true,
                      combineTextSections=true, ppc.calculatePoolRelocations=false

    diff              objdiff-cli/src/cmd/diff.rs:1070  (and the --batch path at
                      diff.rs:1807, identically)
                      functionRelocDiffs=data_value, and the other three at their
                      SCHEMA defaults: false / false / TRUE

Both then layer the project's `objdiff.json` "options" block on top.  This repo's
options block set only `functionRelocDiffs` (shipped 2026-08-12, `d04c83df`),
which fixed the ruler both paths agreed to argue about and left them disagreeing
on the other three.

`ppc.calculatePoolRelocations` is the one that bites.  It SYNTHESIZES
`R_PPC_NONE` "fake" relocations for pooled data loads
(`objdiff-core/src/arch/ppc/mod.rs:819 make_fake_pool_reloc`; the schema calls
them "fake relocations" in as many words), reconstructed per object from that
object's own symbol table.  A dtk-carved target obj -- a whole linked data
section, anonymous `lbl_*` labels -- and our MSVC per-TU COMDAT obj do not
reconstruct the same set.  `reloc_eq` (`objdiff-core/src/diff/code.rs:1330-1338`)
forgives a BASE-only synthesized relocation under `name_check` but its
`_ => return false` arm charges a TARGET-only one under every ruler except
`none`.  So the per-function path charges rows whose two sides are textually
identical.

This is upstream objdiff behaviour, not a fork bug: the three extra report-side
values arrive in `0c9e552` "Combine sections when generating report" (Luke
Street, 2025-05-07), which touched `report.rs` only.  `bin/objdiff-cli` is a
symlink shared with ../rb3 and ../dc3-decomp, so all three repos are exposed and
the fix is config-only in each.

The finding originated in dc3-decomp (Dance Central 3, MSVC/Xbox 360), where it
was measured at 155 functions / 120,728 bytes and fixed the same way.

What this checks
----------------
`--check` (fast, ~0.2 s): every key on which the two CLI base configs disagree
is pinned in `objdiff.json`'s options block AND agrees with the value
`report.json`'s own `provenance.diff_config` says the grading run used.
report.json is authoritative here by construction: it is not a description of
the config, it IS the config the score was taken under.

`--verify-scores`: the end-to-end assertion.  Batch-diffs symbols through
`objdiff-cli diff --batch` with NO `-c` flags -- i.e. exactly what a lane's
per-function tooling sees -- and compares each `canonical_match_percent`
against report.json's `match_percent_normalized`.  Rows the batch path scored
against ANOTHER unit's base object (its cross-unit COMDAT fallback, disclosed
as `base_unit` in the output) are reported separately: the report scores
per-unit only, so those two numbers answer different questions and their
disagreement is not this defect.  Unpaired rows (batch returns null, report
returns 0.0) are agreement, and are counted as neither.

`--selftest`: the negative control.  Re-runs `--verify-scores` over the same
symbols with `-c ppc.calculatePoolRelocations=true`, restoring the `diff`
path's own default, and REQUIRES that to produce disagreements.  A check that
cannot be made to fail is not a check; if the flipped run comes back clean this
exits 5 saying the probe went vacuous rather than reporting success.
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent

# The keys on which `report generate`'s base config and `diff`'s base config
# disagree.  Values here are `report generate`'s -- the grading semantics every
# recorded number in this project was taken under.  This table is a FALLBACK for
# the "no report.json yet" case; when a report exists its provenance wins.
DIVERGENT_KEYS: dict[str, str] = {
    "functionRelocDiffs": "name_check",  # set by this project; both paths honour it
    "combineDataSections": "true",
    "combineTextSections": "true",
    "ppc.calculatePoolRelocations": "false",
}

# The knob the negative control flips.  Restoring `diff`'s own default here must
# reintroduce disagreements, or the probe proved nothing.
SELFTEST_OVERRIDE = "ppc.calculatePoolRelocations=true"

# Units carrying a known witness -- a function whose score moves when the knob is
# flipped -- so `--selftest` costs seconds rather than a whole-binary sweep.
# These ROT as work lands.  When they do, the selftest says so and tells you to
# re-derive with `--all`; it never silently reports success from an empty probe.
WITNESS_UNITS: tuple[str, ...] = (
    "default/BandCharacter",
    "default/MemMgr",
    "default/system/bandobj/BandTrack",
    "default/MetaPerformer",
    "default/BandDirector",
    "default/BandWardrobe",
    "default/MusicLibrary",
    "default/PatchDir",
    "default/BoxMap",
    "default/EventTrigger",
    "default/system/rndobj/Rnd",
    "default/Character",
    "default/MemTrack",
    "default/keygen_xbox",
)


def _norm(value) -> str:
    if isinstance(value, bool):
        return "true" if value else "false"
    return str(value)


def find_report(repo: Path) -> Path | None:
    hits = sorted(repo.glob("build/*/report.json"))
    return hits[0] if hits else None


def grader_config(repo: Path) -> tuple[dict[str, str], str]:
    """The effective config the grading run used, and where that came from."""
    report = find_report(repo)
    if report is not None:
        with report.open() as fh:
            data = json.load(fh)
        entries = (data.get("provenance") or {}).get("diff_config") or []
        if entries:
            cfg = {}
            for entry in entries:
                key, _, value = entry.partition("=")
                cfg[key] = value
            return cfg, f"{report.relative_to(repo)} provenance.diff_config"
    return dict(DIVERGENT_KEYS), "verify_ruler_agreement.DIVERGENT_KEYS (FALLBACK: no report.json)"


def check_config(repo: Path, pins_only: bool = False, stamp_out: str | None = None) -> int:
    """Assert the four divergent keys are pinned, and (unless pins_only) that
    they agree with the config the last grading run actually used.

    `pins_only` exists for the ninja edge.  The report cross-check is the
    stronger assertion and is what a human should run -- but it is also FALSE
    for one legitimate build, the one that first regenerates report.json after
    a deliberate ruler change.  Gating the REPORT edge on it would deadlock:
    the report cannot be regenerated because the guard reads the report it is
    meant to replace.  So the build edge asserts the invariant that has no
    legitimate transient -- "all four are pinned, to `report generate`'s own
    base values" -- and the report cross-check stays a manual / CI step.
    """
    with (repo / "objdiff.json").open() as fh:
        options = json.load(fh).get("options") or {}
    pinned = {k: _norm(v) for k, v in options.items()}
    if pins_only:
        grader, source = dict(DIVERGENT_KEYS), (
            "verify_ruler_agreement.DIVERGENT_KEYS (--pins-only: report.json NOT cross-checked)"
        )
    else:
        grader, source = grader_config(repo)

    print(f"grader config source: {source}")
    problems = []
    for key, fallback in DIVERGENT_KEYS.items():
        want = grader.get(key, fallback)
        have = pinned.get(key)
        if have is None:
            problems.append(
                f"  {key}: NOT PINNED in objdiff.json options -- "
                f"`report generate` uses {want!r}, `objdiff-cli diff` will use its own "
                f"base default instead"
            )
        elif have != want:
            problems.append(f"  {key}: objdiff.json says {have!r}, the grading run used {want!r}")
        else:
            print(f"  OK  {key} = {have}")

    if problems:
        print(
            "\nFAIL: `objdiff-cli diff` and `objdiff-cli report generate` will not agree.\n"
            "Every per-function measurement -- run_objdiff, run_diff_inspect, "
            "`diff --batch` -- reads a different ruler than report.json, and the "
            "per-function side reads LOW.\n"
        )
        print("\n".join(problems))
        print(
            "\nFix in the `options` block of tools/project.py (NOT by passing -c at each "
            "call site), then re-run configure.py."
        )
        return 1
    print("\nOK: both objdiff-cli entry points resolve the same ruler.")
    if stamp_out:
        # write-if-changed, so ninja's restat can keep this from dirtying the
        # REPORT edge on every quiet build.
        payload = json.dumps({k: pinned[k] for k in DIVERGENT_KEYS}, sort_keys=True) + "\n"
        path = Path(stamp_out)
        if not path.is_file() or path.read_text() != payload:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(payload)
    return 0


def batch_scores(repo: Path, symbols: list[str], extra_config: list[str]) -> dict[str, dict]:
    cmd = [str(repo / "bin" / "objdiff-cli"), "diff", "--batch", "-p", str(repo), "-f", "json"]
    for item in extra_config:
        cmd += ["-c", item]
    proc = subprocess.run(
        cmd,
        input="\n".join(symbols) + "\n",
        capture_output=True,
        text=True,
        cwd=str(repo),
    )
    if proc.returncode != 0:
        sys.stderr.write(proc.stderr[-4000:])
        raise SystemExit(f"objdiff-cli diff --batch failed (exit {proc.returncode})")
    out: dict[str, dict] = {}
    for line in proc.stdout.splitlines():
        if not line.strip():
            continue
        row = json.loads(line)
        if "error" in row:
            continue
        out[row["symbol"]] = row
    return out


def load_report_rows(repo: Path, units: tuple[str, ...] | None) -> dict[str, tuple[str, float, int]]:
    report = find_report(repo)
    if report is None:
        raise SystemExit("no build/*/report.json -- run ninja first")
    with report.open() as fh:
        data = json.load(fh)
    seen: dict[str, int] = {}
    rows: dict[str, tuple[str, float, int]] = {}
    for unit in data["units"]:
        if units is not None and unit["name"] not in units:
            continue
        for fn in unit.get("functions", []):
            seen[fn["name"]] = seen.get(fn["name"], 0) + 1
            rows[fn["name"]] = (
                unit["name"],
                fn["match_percent_normalized"],
                int(fn.get("size") or 0),
            )
    # A name defined in more than one unit cannot be attributed from a batch run
    # (the batch resolves a bare name to ONE unit), so drop it rather than guess.
    return {k: v for k, v in rows.items() if seen[k] == 1}


def verify_scores(repo: Path, units: tuple[str, ...] | None, extra_config: list[str]) -> dict:
    rows = load_report_rows(repo, units)
    scores = batch_scores(repo, sorted(rows), extra_config)
    result = {
        "examined": 0,
        "agree": 0,
        "disagree": [],
        "cross_unit_fallback": 0,
        "unpaired": 0,
        "unresolved": 0,
        "universe": len(rows),
    }
    for name, (unit, want, size) in rows.items():
        row = scores.get(name)
        if row is None:
            result["unresolved"] += 1
            continue
        if row.get("unit") != unit:
            result["unresolved"] += 1
            continue
        got = row.get("canonical_match_percent")
        if got is None:
            result["unpaired"] += 1
            continue
        result["examined"] += 1
        if abs(got - want) < 1e-4:
            result["agree"] += 1
        elif row.get("base_unit"):
            result["cross_unit_fallback"] += 1
        else:
            result["disagree"].append((name, unit, want, got, size))
    return result


def report_scores(label: str, res: dict) -> None:
    dis = res["disagree"]
    print(
        f"{label}: universe {res['universe']} | examined {res['examined']} | "
        f"agree {res['agree']} | disagree {len(dis)} | "
        f"cross-unit base_unit fallback {res['cross_unit_fallback']} | "
        f"unpaired (no base symbol) {res['unpaired']} | unresolved {res['unresolved']}"
    )
    if dis:
        report_higher = [d for d in dis if d[2] > d[3]]
        diff_higher = [d for d in dis if d[3] > d[2]]
        total_bytes = sum(d[4] for d in dis)
        worst = max(abs(d[2] - d[3]) for d in dis)
        perfect = [d for d in dis if abs(d[2] - 100.0) < 1e-4]
        print(
            f"    direction: report higher on {len(report_higher)}, "
            f"diff higher on {len(diff_higher)} | bytes {total_bytes} | "
            f"max delta {worst:.5f} pp | report==100.0 on {len(perfect)} "
            f"({sum(d[4] for d in perfect)} B)"
        )
    for name, unit, want, got, size in sorted(dis, key=lambda r: r[2] - r[3], reverse=True)[:25]:
        print(f"    report {want:9.5f}  diff {got:9.5f}  {size:7d} B  {unit}  {name}")
    if len(dis) > 25:
        print(f"    ... and {len(dis) - 25} more")


def main() -> int:
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    ap.add_argument("--repo", default=str(REPO_ROOT), help="project dir (default: this checkout)")
    ap.add_argument("--check", action="store_true", help="config-pin assertion only (~0.2 s)")
    ap.add_argument(
        "--pins-only",
        action="store_true",
        help="with --check: assert the pins without cross-checking report.json "
        "(the build-edge form -- see check_config's docstring)",
    )
    ap.add_argument("--stamp-out", help="with --check: write-if-changed stamp for ninja")
    ap.add_argument("--quiet", action="store_true", help="suppress the OK lines")
    ap.add_argument("--verify-scores", action="store_true", help="end-to-end score comparison")
    ap.add_argument(
        "--selftest",
        action="store_true",
        help="negative control: flip the knob back and require failure",
    )
    ap.add_argument(
        "--all",
        action="store_true",
        help="with --verify-scores/--selftest: every unit, not just the witness units",
    )
    ap.add_argument("--json-out", help="write the disagreement set to this path")
    args = ap.parse_args()

    repo = Path(args.repo).resolve()
    if not (args.check or args.verify_scores or args.selftest):
        args.check = True

    rc = 0
    if args.check:
        if args.quiet:
            import io
            import contextlib

            buf = io.StringIO()
            with contextlib.redirect_stdout(buf):
                sub = check_config(repo, args.pins_only, args.stamp_out)
            if sub:
                sys.stdout.write(buf.getvalue())
            rc |= sub
        else:
            rc |= check_config(repo, args.pins_only, args.stamp_out)

    units = None if args.all else WITNESS_UNITS

    if args.verify_scores:
        res = verify_scores(repo, units, [])
        report_scores("as configured", res)
        if args.json_out:
            Path(args.json_out).write_text(json.dumps(res, indent=1))
        if res["examined"] == 0:
            print("FAIL: examined 0 functions -- an empty comparison agrees by construction.")
            return 4
        if res["disagree"]:
            print("FAIL: the per-function path disagrees with report.json on the rows above.")
            rc |= 1

    if args.selftest:
        base = verify_scores(repo, units, [])
        flipped = verify_scores(repo, units, [SELFTEST_OVERRIDE])
        report_scores("as configured        ", base)
        report_scores(f"with {SELFTEST_OVERRIDE}", flipped)
        if base["examined"] == 0:
            print("FAIL: examined 0 functions.")
            return 4
        if not flipped["disagree"]:
            print(
                "\nVACUOUS (exit 5): restoring `diff`'s own "
                f"{SELFTEST_OVERRIDE} produced NO disagreement over "
                f"{flipped['examined']} functions, so this probe cannot distinguish a "
                "working check from a broken one.\n"
                "The witness units have rotted. Re-run with --all to search the whole "
                "binary, and refresh WITNESS_UNITS from what it finds. Do NOT read this "
                "as a pass."
            )
            return 5
        if base["disagree"]:
            print("\nFAIL: the as-configured run disagrees with report.json.")
            rc |= 1
        else:
            print(
                f"\nOK: as-configured agrees on all {base['examined']} functions, and the "
                f"control flip produces {len(flipped['disagree'])} disagreement(s) -- "
                "the check can fail."
            )

    return rc


if __name__ == "__main__":
    raise SystemExit(main())
