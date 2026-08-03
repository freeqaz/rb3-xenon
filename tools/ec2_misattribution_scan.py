#!/usr/bin/env python3
"""EC-2: size the MISATTRIBUTION contamination of the COMPLETABLE bucket.

A blocker row is MISATTRIBUTED when the retail function at the pinned VA does
not belong to the class/signature the map names it.  The witness must be
ICF-IMMUNE: objdiff loads ~784 ICF equivalence entries, so a callee name only
proves which body you EQUAL, never whose you ARE.

The witness used here is the INCOMING-ARGUMENT REGISTER SET -- the registers a
body reads before ever defining them.  That is fixed by the ABI from the
*signature*, so:
  * a folded (ICF-alias) body has the SAME incoming-arg set by construction;
  * no amount of body divergence in our source can invent or remove an
    incoming FPR argument.
It is deliberately ASYMMETRIC: "target consumes an incoming FPR arg and base
does not" is decisive (we cannot read a float we were never passed), while the
converse is not (retail may simply never use a parameter it was given).

Anti-vacuity control: the same scan is run over a CONTROL population of rows
that are at 100% (mpn==100) in the same units.  If the flag fires on those at a
comparable rate it is measuring noise, not attribution -- print the rate and
say so.
"""
import argparse, json, pathlib, subprocess, sys, collections

CLI = "/home/free/code/milohax/objdiff/target/release/objdiff-cli"
FPR_ARGS = {f"f{i}" for i in range(1, 14)}
GPR_ARGS = {f"r{i}" for i in range(3, 11)}


def dest_regs(op, args):
    """Registers WRITTEN by this instruction (approximate, PPC)."""
    if not args:
        return set()
    o = op.lower()
    if o.startswith(("st", "b", "cmp", "mt", "tw", "trap", "dcb", "sync", "isync", "eieio")):
        d = set()
        if o.startswith("st") and o.endswith("u") and len(args) >= 3:
            v = args[-1].get("value")
            if isinstance(v, str): d.add(v)      # update form writes the base reg
        return d
    v = args[0].get("value")
    return {v} if isinstance(v, str) else set()


def incoming_args(side, instrs):
    """Regs read before ever being written == ABI incoming arguments."""
    written, incoming = set(), set()
    for i in instrs:
        sd = i.get(side)
        if not sd:
            continue
        op = sd.get("opcode") or ""
        args = sd.get("typed_args") or []
        d = dest_regs(op, args)
        for j, a in enumerate(args):
            if a.get("type") != "Register":
                continue
            v = a.get("value")
            if v in d:
                continue
            if v not in written:
                incoming.add(v)
        written |= d
    return incoming


def this_offsets(side, instrs):
    """Displacements used off the register chain rooted at r3 on entry (`this`).

    Tracks `this`-carrying registers through `mr`/`or rX,rY,rY` copies and drops
    a register the moment anything else defines it.  Returns the set of
    displacements of loads/stores whose BASE register still carries `this`.
    Offsets far beyond the class size are a layout-level witness that the body
    belongs to a different class -- ICF-immune, since a folded alias touches the
    same offsets."""
    carriers = {"r3"}
    offs = set()
    first = True
    for i in instrs:
        sd = i.get(side)
        if not sd:
            continue
        op = (sd.get("opcode") or "").lower()
        args = sd.get("typed_args") or []
        vals = [a.get("value") for a in args]
        typs = [a.get("type") for a in args]
        # memory ops: <op> reg, disp, base   (objdiff order)
        if (op.startswith(("lwz", "lhz", "lha", "lbz", "lfs", "lfd", "ld", "lwa"))
                or op.startswith(("stw", "sth", "stb", "stfs", "stfd", "std"))) and len(args) >= 3:
            if typs[1] in ("Signed", "Unsigned") and typs[2] == "Register" and vals[2] in carriers:
                offs.add(int(vals[1]))
        if op in ("addi", "addis") and len(args) >= 3 and typs[2] in ("Signed", "Unsigned") \
                and typs[1] == "Register" and vals[1] in carriers:
            offs.add(int(vals[2]))
        d = dest_regs(op, args)
        # propagate `this` through register copies
        if op in ("mr", "or") and len(args) >= 2 and vals[1] in carriers:
            carriers.add(vals[0])
        for r in d:
            if r in carriers and not (op in ("mr", "or") and len(args) >= 2 and vals[1] in carriers):
                carriers.discard(r)
        first = False
    return offs


def has_op(side, instrs, prefixes):
    for i in instrs:
        sd = i.get(side)
        if sd and (sd.get("opcode") or "").lower().startswith(prefixes):
            return True
    return False


def probe(root, unit, sym):
    p = subprocess.run([CLI, "diff", "-p", str(root), "-u", unit,
                        "--include-instructions", "--summary", "-f", "json", "-o", "-", sym],
                       capture_output=True, text=True, timeout=300)
    if p.returncode != 0 or not p.stdout.strip():
        return None
    return json.loads(p.stdout)


def classify(d):
    ins = d.get("instructions", [])
    t_in, b_in = incoming_args("target", ins), incoming_args("base", ins)
    t_fpr, b_fpr = t_in & FPR_ARGS, b_in & FPR_ARGS
    t_gpr, b_gpr = t_in & GPR_ARGS, b_in & GPR_ARGS
    su = d.get("instruction_summary", {})
    rec = dict(
        tsz=d.get("target_size"), bsz=d.get("base_size"),
        fz=round(d.get("fuzzy_match_percent", 0), 2),
        mpn=round(d.get("normalized_match_percent", 0), 2),
        eqpct=round(su.get("equal_percent", 0), 2),
        t_fpr_args=sorted(t_fpr), b_fpr_args=sorted(b_fpr),
        t_gpr_args=sorted(t_gpr), b_gpr_args=sorted(b_gpr),
        t_fstore=has_op("target", ins, ("stfs", "stfd")),
        b_fstore=has_op("base", ins, ("stfs", "stfd")),
        t_float=has_op("target", ins, ("lfs", "lfd", "fmr", "fadd", "fmul", "fsub", "fdiv", "frsp", "fctiw")),
        b_float=has_op("base", ins, ("lfs", "lfd", "fmr", "fadd", "fmul", "fsub", "fdiv", "frsp", "fctiw")),
    )
    t_off, b_off = this_offsets("target", ins), this_offsets("base", ins)
    rec["t_this_max"] = max(t_off) if t_off else 0
    rec["b_this_max"] = max(b_off) if b_off else 0

    # ASYMMETRIC verdict.  Every flag requires a REAL body on BOTH sides:
    # bsz==0 means our obj defines no such symbol at all, which is a MISSING
    # IMPLEMENTATION, not a misattribution, and would fire every flag vacuously.
    flags = []
    both = (d.get("target_size") or 0) > 0 and (d.get("base_size") or 0) > 0
    if not both:
        rec["flags"] = ["MISSING_BASE_BODY"] if (d.get("base_size") or 0) == 0 else ["MISSING_TARGET_BODY"]
        return rec
    if t_fpr and not b_fpr:
        flags.append("FPR_ARG_TARGET_ONLY")
    if len(t_gpr) > len(b_gpr) + 1:
        flags.append("GPR_ARITY_TARGET_HIGHER")
    if rec["t_float"] and not rec["b_float"]:
        flags.append("FLOAT_TARGET_ONLY")
    if rec["t_this_max"] > 0x400 and rec["t_this_max"] > 4 * max(rec["b_this_max"], 1):
        flags.append("THIS_OFFSET_FAR_BEYOND_BASE")
    rec["flags"] = flags
    return rec


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=".")
    ap.add_argument("--census", required=True)
    ap.add_argument("--buckets", default="COMPLETABLE")
    ap.add_argument("--control-sample", type=int, default=0,
                    help="UNTREATED CONTROL: N sub-100 rows (real body both sides) drawn "
                         "from units OUTSIDE --buckets.  A control of mpn==100 rows is "
                         "VACUOUS -- their instruction streams are identical, so no "
                         "asymmetric flag can ever fire on them.")
    ap.add_argument("--seed", type=int, default=20260803)
    ap.add_argument("--out", required=True)
    a = ap.parse_args()

    root = pathlib.Path(a.root).resolve()
    cen = json.loads(pathlib.Path(a.census).read_text())
    rep = json.loads((root / "build/45410914/report.json").read_text())
    runits = {u["name"]: u for u in rep["units"]}
    want = set(a.buckets.split(","))
    units = [u for u in cen["units"] if u["bucket"] in want]

    charged, control = [], []
    for c in sorted(units, key=lambda x: x["unit"]):
        un = c["unit"]
        ru = runits.get(un)
        if not ru:
            continue
        fns = ru.get("functions") or []
        blk = [f for f in fns if f["match_percent_normalized"] < 100.0]
        ok = [f for f in fns if f["match_percent_normalized"] == 100.0]
        for f in blk:
            d = probe(root, un, f["name"])
            if d is None:
                charged.append(dict(unit=un, sym=f["name"], err="probe_failed")); continue
            r = classify(d); r.update(unit=un, sym=f["name"], stratum="CHARGED")
            charged.append(r)
            print(f"  charged {un[:38]:38s} {f['name'][:44]}", file=sys.stderr)
    if a.control_sample:
        import random
        rng = random.Random(a.seed)
        pool = []
        treated = {u["unit"] for u in units}
        for c in cen["units"]:
            if c["unit"] in treated:
                continue
            ru = runits.get(c["unit"])
            if not ru:
                continue
            for f in (ru.get("functions") or []):
                if f["match_percent_normalized"] < 100.0 and not f["name"].startswith("fn_"):
                    pool.append((c["unit"], f["name"]))
        rng.shuffle(pool)
        taken = 0
        for un, sym in pool:
            if taken >= a.control_sample:
                break
            d = probe(root, un, sym)
            if d is None:
                continue
            r = classify(d); r.update(unit=un, sym=sym, stratum="CONTROL")
            control.append(r); taken += 1
            print(f"  control {un[:38]:38s} {sym[:44]}", file=sys.stderr)

    allrows = charged + control
    pathlib.Path(a.out).write_text(json.dumps(allrows, indent=1))

    def rate(rows, flag):
        ok = [r for r in rows if "err" not in r]
        n = sum(1 for r in ok if flag in r.get("flags", []))
        return n, len(ok), (100.0 * n / len(ok) if ok else 0.0)

    print()
    for flag in ("FPR_ARG_TARGET_ONLY", "GPR_ARITY_TARGET_HIGHER", "FLOAT_TARGET_ONLY",
                 "THIS_OFFSET_FAR_BEYOND_BASE", "MISSING_BASE_BODY"):
        cn, ct, cp = rate(charged, flag)
        if control:
            nn, nt, np_ = rate(control, flag)
            enr = (cp / np_) if np_ else float("inf")
            print(f"{flag:26s} charged {cn:3d}/{ct:3d} = {cp:5.2f}%   "
                  f"control {nn:3d}/{nt:3d} = {np_:5.2f}%   enrichment {enr:.2f}x")
        else:
            print(f"{flag:26s} charged {cn:3d}/{ct:3d} = {cp:5.2f}%")
    print(f"\nwrote {a.out}  ({len(charged)} charged, {len(control)} control)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
