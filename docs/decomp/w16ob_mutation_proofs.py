"""W16-OB mutation proofs: each repaired test must go RED under a targeted defect
and GREEN again once restored.  Record: docs/decomp/W16OB_TEST_LANE_REPAIR_2026-10-02.md.

⛔ MUTATES THE CHECKOUT IN PLACE (each defect is written into the real file, the
test is run, and the file is restored with `git checkout --`).  Run it ONLY in
your own worktree, never in the shared main checkout, and only with no
uncommitted edits to the mutated files.  That is also why it is NOT registered in
scripts/test_tools.py's SCRIPT_ARM, whose entries promise never to write to the
checkout.  Set WT to your worktree.  Exit 0 = every defect caught by the named
test and every restore green."""
import subprocess, sys, json
import os
WT = os.environ.get("WT", os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))
PY = sys.executable

def sh(cmd):
    return subprocess.run(cmd, cwd=WT, capture_output=True, text=True, shell=isinstance(cmd, str))

def run_test(cmd):
    r = sh(cmd)
    out = r.stdout + r.stderr
    return r.returncode, out

PS = [PY, "-m", "pytest", "-q", "-p", "no:cacheprovider"]
M = [
 # (label, file, old, new, test cmd, must-appear-in-output)
 ("PS-1 denylist dropped from run_check", "scripts/verify_objs_patched.py",
  "    rc = check_denylist_applied(repo, quiet=quiet)  # ★ W16-AE\n", "    rc = 0\n",
  PS + ["scripts/test_patch_state.py"], "test_the_denylist_check_is_still_in_the_chain"),
 ("PS-2 denylist dropped from verify_manifest", "scripts/verify_objs_patched.py",
  "        rc = check_denylist_applied(repo, quiet=quiet)\n        if rc:\n            return rc\n        if not quiet:",
  "        rc = 0\n        if rc:\n            return rc\n        if not quiet:",
  PS + ["scripts/test_patch_state.py"], "test_the_denylist_check_is_still_in_the_chain"),
 ("PS-3 verifier runs only 5 of 6 passes", "scripts/verify_objs_patched.py",
  '    "obj_eh_boundary_patcher.py",\n]', ']',
  PS + ["scripts/test_patch_state.py"], "test_check_runs_every_patcher"),
 ("RV-1 extra unadjudicated overlap in the live map", "scripts/target_symbol_map.json",
  '"_icf_arbitrary": [', '"_icf_arbitrary": [\n    "@BIJ@",',
  PS + ["tools/test_comdat_retail_verify.py"], "overlap_only_where_adjudicated"),
 ("RV-2 adjudicated overlap removed from _icf_arbitrary", "scripts/target_symbol_map.json",
  "@ICF826101B8@", "",
  PS + ["tools/test_comdat_retail_verify.py"], "overlap_only_where_adjudicated"),
 ("RV-3 name_grain_index first-key-wins", "tools/comdat_retail_verify.py",
  "tags[addr] = label if prev is None else '+'.join(sorted({prev, label}))",
  "tags[addr] = label if prev is None else prev",
  PS + ["tools/test_comdat_retail_verify.py"], "overlap_only_where_adjudicated"),
 ("MS-1 CF5 laundering guard removed", "tools/comdat_fold_gate.py",
  "    placed = sorted(retail.byname.get(F, set()))\n    if placed:",
  "    placed = sorted(retail.byname.get(F, set()))\n    if False:",
  [PY, "tools/test_comdat_fold_gate_map_silent.py"], "[FAIL] map-RESIDENT spelling submitted as map-silent is REFUSED (laundering)"),
 ("MS-2 fixture map row moved (fixture goes stale)", "scripts/target_symbol_map.json",
  '"0x826c83e8": "', '"0x826c83e8": "X',
  [PY, "tools/test_comdat_fold_gate_map_silent.py"], "[FAIL] fixture precondition"),
 ("FT-1 mask_word masks unrelocated words again (the pre-W16-EK vacuity)", "tools/fold_thunk_gate.py",
  "    if not relocated:\n        return w\n    op = w >> 26", "    op = w >> 26",
  PS + ["tools/test_fold_thunk_gate_mask.py"], "test_an_unrelocated_field_is_compared_whole"),
]

results = []
for label, path, old, new, cmd, needle in M:
    full = f"{WT}/{path}"
    txt = open(full).read()
    if "@BIJ@" in new:
        raw = json.loads(txt)
        extra = next(a for a in raw["_bijection_arbitrary"] if a.lower() != "0x826101b8")
        new = new.replace("@BIJ@", extra)
    if old == "@ICF826101B8@":
        # remove the "0x826101b8" element from the _icf_arbitrary list only
        i = txt.index('"_icf_arbitrary": [')
        j = txt.index("]", i)
        seg = txt[i:j]
        import re
        seg2 = re.sub(r'\s*"0x826101b8",?', "", seg, count=1)
        assert seg2 != seg
        mutated = txt[:i] + seg2.rstrip().rstrip(",") + "\n  " + txt[j:] if seg2.rstrip().endswith(",") else txt[:i] + seg2 + txt[j:]
    else:
        assert txt.count(old) == 1, (label, txt.count(old))
        mutated = txt.replace(old, new)
    open(full, "w").write(mutated)
    try:
        if path.endswith(".json"):
            json.loads(mutated)  # mutation must still be valid JSON
        rc, out = run_test(cmd)
    finally:
        sh(["git", "checkout", "--", path])
    assert open(full).read() == txt, "restore failed: " + path
    rc2, _ = run_test(cmd)
    red = rc != 0 and needle in out
    results.append((label, rc, red, rc2))
    print(f"{'RED ' if red else 'MISS'} rc={rc} restored-rc={rc2}  {label}", flush=True)
print("dirty after run:", sh(["git", "status", "--porcelain"]).stdout.strip() or "clean")
sys.exit(0 if all(r[2] and r[3] == 0 for r in results) else 1)
