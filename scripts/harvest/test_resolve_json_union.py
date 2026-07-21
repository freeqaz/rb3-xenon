#!/usr/bin/env python3
"""Regression tests for the 3-way JSON-union resolver (scripts/harvest/
resolve_json_union.py). Reproduces the batch-5 resurrection bug
(docs/plans/fpcarve-batch5.md friction #1) and asserts it is fixed.

Two layers:
  1. Unit tests over merge3(base, ours, theirs) — the pure merge core.
  2. An end-to-end test that stages real conflict entries (git index stages
     1/2/3) and runs the script, exercising the stage-reading path too.

Run:  python3 scripts/harvest/test_resolve_json_union.py
Exit 0 = all pass. No pytest dependency.
"""
import os, sys, json, subprocess, tempfile, collections

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import resolve_json_union as R  # noqa: E402

MISSING = R.MISSING
PASS, FAIL = 0, 0


def check(name, cond, detail=""):
    global PASS, FAIL
    if cond:
        PASS += 1
        print(f"  ok   {name}")
    else:
        FAIL += 1
        print(f"  FAIL {name}  {detail}")


# --------------------------------------------------------------------------
# Layer 1: merge3 unit tests
# --------------------------------------------------------------------------
def od(*pairs):
    return collections.OrderedDict(pairs)


def test_deletion_not_resurrected():
    # THE batch-5 case (a)/(b): base has the stale funclet key; MAIN deleted it;
    # the worker branch predates the deletion and still carries it unchanged.
    K = "0x823e8cc8"
    base = od((K, "__unwind$176976"))
    ours = od()                      # main deleted K
    theirs = od((K, "__unwind$176976"))  # worker still has base value
    merged, conflicts = R.merge3(base, ours, theirs)
    check("deletion: key dropped (not resurrected)", K not in merged,
          f"merged={dict(merged)}")
    check("deletion: no false conflict", conflicts == [], str(conflicts))


def test_repoint_main_wins():
    # batch-5 case (c): MAIN re-pointed K V1->V2; worker still holds V1.
    # 2-way union "kept theirs" (V1, stale). 3-way: main wins (V2).
    K = "0x826e3010"
    base = od((K, "DetermineHeaderSymbolForSong"))
    ours = od((K, "GetIDFromInstrument"))          # main's fix
    theirs = od((K, "DetermineHeaderSymbolForSong"))  # worker's stale value
    merged, conflicts = R.merge3(base, ours, theirs)
    check("repoint: main value wins", merged.get(K) == "GetIDFromInstrument",
          f"got {merged.get(K)!r}")
    check("repoint: no false conflict", conflicts == [], str(conflicts))
    check("repoint: stale worker value absent",
          "DetermineHeaderSymbolForSong" not in merged.values())


def test_worker_addition_kept():
    base = od()
    ours = od()
    theirs = od(("0x82000000", "NewWorkerFn"))     # genuine worker addition
    merged, conflicts = R.merge3(base, ours, theirs)
    check("worker addition kept", merged.get("0x82000000") == "NewWorkerFn")
    check("worker addition: no conflict", conflicts == [])


def test_main_addition_kept():
    base = od()
    ours = od(("0x82111111", "MainOnlyFn"))        # main-only addition
    theirs = od()
    merged, conflicts = R.merge3(base, ours, theirs)
    check("main addition kept", merged.get("0x82111111") == "MainOnlyFn")
    check("main addition: no conflict", conflicts == [])


def test_worker_deletion_honored():
    K = "0x82222222"
    base = od((K, "Old"))
    ours = od((K, "Old"))            # main unchanged
    theirs = od()                    # worker deleted
    merged, conflicts = R.merge3(base, ours, theirs)
    check("worker deletion honored", K not in merged)
    check("worker deletion: no conflict", conflicts == [])


def test_both_changed_conflicts():
    K = "0x82333333"
    base = od((K, "V1"))
    ours = od((K, "V2"))             # main -> V2
    theirs = od((K, "V3"))           # worker -> V3, differently
    merged, conflicts = R.merge3(base, ours, theirs)
    check("both-changed-differently -> conflict", len(conflicts) == 1,
          str(conflicts))
    if conflicts:
        p, bv, ov, tv = conflicts[0]
        check("conflict tuple carries base/main/worker",
              (bv, ov, tv) == ("V1", "V2", "V3"), str(conflicts[0]))


def test_both_added_same_key_diff_value_conflicts():
    K = "0x82444444"
    base = od()                      # absent in base
    ours = od((K, "MainAdd"))
    theirs = od((K, "WorkerAdd"))
    _, conflicts = R.merge3(base, ours, theirs)
    check("both-added-different -> conflict", len(conflicts) == 1, str(conflicts))


def test_same_change_both_sides_no_conflict():
    K = "0x82555555"
    base = od((K, "V1"))
    ours = od((K, "V2"))
    theirs = od((K, "V2"))           # both re-pointed identically
    merged, conflicts = R.merge3(base, ours, theirs)
    check("identical change: no conflict", conflicts == [])
    check("identical change: value taken", merged.get(K) == "V2")


def test_nested_objects_json_shape():
    # objects.json nesting: main deletes a TU under a module; worker adds a
    # different TU under the same module.
    base = od(("modA", od(("TU_old", od(("status", "NonMatching"))))))
    ours = od(("modA", od()))                        # main removed TU_old
    theirs = od(("modA", od(
        ("TU_old", od(("status", "NonMatching"))),   # worker still has it
        ("TU_new", od(("status", "NonMatching"))))))  # worker added TU_new
    merged, conflicts = R.merge3(base, ours, theirs)
    check("nested: main deletion honored",
          "TU_old" not in merged.get("modA", {}), dict(merged.get("modA", {})))
    check("nested: worker addition merged",
          "TU_new" in merged.get("modA", {}), dict(merged.get("modA", {})))
    check("nested: no false conflict", conflicts == [])


# --------------------------------------------------------------------------
# Layer 2: end-to-end against a real conflicted git index (stages 1/2/3)
# --------------------------------------------------------------------------
def _git(repo, *args, **kw):
    return subprocess.run(["git", "-C", repo, *args],
                          capture_output=True, text=True, **kw)


def _blob(repo, text):
    r = subprocess.run(["git", "-C", repo, "hash-object", "-w", "--stdin"],
                       input=text, capture_output=True, text=True)
    assert r.returncode == 0, r.stderr
    return r.stdout.strip()


def test_end_to_end_script():
    """Stage base/ours/theirs directly into the index as conflict stages
    1/2/3, then run resolve_json_union.py and assert the resolved file drops
    the main-deleted key (no resurrection) and keeps the worker addition."""
    repo = tempfile.mkdtemp(prefix="jsonres_e2e_")
    rel = "scripts/target_symbol_map.json"
    _git(repo, "init", "-q")
    _git(repo, "config", "user.email", "t@t")
    _git(repo, "config", "user.name", "t")
    os.makedirs(os.path.join(repo, "scripts"), exist_ok=True)

    KDEL = "0x823e8cc8"   # main deletes this stale funclet
    KREP = "0x826e3010"   # main re-points this
    KWRK = "0x82abcdef"   # worker adds this

    base = od((KDEL, "__unwind$176976"),
              (KREP, "DetermineHeaderSymbolForSong"))
    ours = od((KREP, "GetIDFromInstrument"))          # main: del KDEL, repoint KREP
    theirs = od((KDEL, "__unwind$176976"),            # worker: stale, +KWRK
                (KREP, "DetermineHeaderSymbolForSong"),
                (KWRK, "WorkerFn"))

    b1 = _blob(repo, json.dumps(base, indent=1) + "\n")
    b2 = _blob(repo, json.dumps(ours, indent=1) + "\n")
    b3 = _blob(repo, json.dumps(theirs, indent=1) + "\n")
    idx = "".join(f"100644 {sha} {stg}\t{rel}\n"
                  for sha, stg in ((b1, 1), (b2, 2), (b3, 3)))
    r = subprocess.run(["git", "-C", repo, "update-index", "--index-info"],
                       input=idx, capture_output=True, text=True)
    assert r.returncode == 0, r.stderr

    run = subprocess.run(
        [sys.executable, os.path.join(HERE, "resolve_json_union.py"), repo, rel],
        capture_output=True, text=True)
    check("e2e: script exits 0 (clean resolve)", run.returncode == 0,
          run.stdout + run.stderr)
    with open(os.path.join(repo, rel)) as f:
        result = json.load(f)
    check("e2e: main-deleted key NOT resurrected", KDEL not in result,
          f"result={result}")
    check("e2e: re-point -> main value wins",
          result.get(KREP) == "GetIDFromInstrument", f"result={result}")
    check("e2e: worker addition preserved",
          result.get(KWRK) == "WorkerFn", f"result={result}")

    # And the true-conflict case must refuse (nonzero exit, file untouched).
    base2 = od((KREP, "V1"))
    o2 = od((KREP, "V2"))
    t2 = od((KREP, "V3"))
    c1 = _blob(repo, json.dumps(base2, indent=1) + "\n")
    c2 = _blob(repo, json.dumps(o2, indent=1) + "\n")
    c3 = _blob(repo, json.dumps(t2, indent=1) + "\n")
    subprocess.run(["git", "-C", repo, "update-index", "--index-info"],
                   input="".join(f"100644 {s} {g}\t{rel}\n"
                                 for s, g in ((c1, 1), (c2, 2), (c3, 3))),
                   capture_output=True, text=True)
    run2 = subprocess.run(
        [sys.executable, os.path.join(HERE, "resolve_json_union.py"), repo, rel],
        capture_output=True, text=True)
    check("e2e: real conflict refuses (nonzero exit)", run2.returncode != 0,
          f"rc={run2.returncode}")
    check("e2e: conflict message lists the key",
          KREP in run2.stdout, run2.stdout)


if __name__ == "__main__":
    for fn in sorted(k for k in dict(globals()) if k.startswith("test_")):
        print(f"[{fn}]")
        globals()[fn]()
    print(f"\n{PASS} passed, {FAIL} failed")
    sys.exit(1 if FAIL else 0)
