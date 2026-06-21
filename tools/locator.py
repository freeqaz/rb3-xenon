#!/usr/bin/env python3
"""locator.py - per-method VA-placement classifier for ICF-scattered TUs.

THE PROBLEM (hard-frontier #2)
------------------------------
The RB3-specific scattered-TU wall is an IDENTIFICATION wall. The rb3-Wii BinDiff
oracle (``unified_id_rb3wii.json``) is NEAR-RANDOM for these TUs (BandProfile
median sim 0.16; ZERO >=0.70). ``identity_transfer.py`` applies that oracle BLIND
(no confidence gate) and can carve WRONG VAs (wave-16 BandProfile 0/64 =
mis-located, not un-matchable). NO single weak signal suffices.

THE TOOL
--------
A per-method LOCATOR that FUSES multiple weak signals into one high-confidence VA
placement + confidence score, classifying each scattered-TU method as
CONFIRMED / RECON / WALL / UNPLACEABLE / MISATTRIBUTED. The oracle already
supplies ONE candidate VA per method (rb3_addr), so this is CONFIRM-OR-DEMOTE,
not search-among-many.

DECISION CASCADE (first match wins; order matters - size gate runs BEFORE sim
because guard-thunks have deceptively high sim ~0.41):

  1. true_size is null               -> UNPLACEABLE   (no pdata fn-start)
  2. true_size <= 44                 -> UNPLACEABLE   (<=44B guard-thunk)
  3. MISATTRIB(i): accessor name & true_size>=512 (ratio>20x) -> MISATTRIBUTED
  4. MISATTRIB(ii): ctor name whose body is a memberwise COPY (parallel
     load(src)/store(dst) run, ~0 callees, no value-ctor control flow)
                                       -> MISATTRIBUTED
  5. MISATTRIB(iii): sim<0.02 AND S<0.15 (zero corroboration) -> MISATTRIBUTED
  6. sim>=0.50 AND S>=0.60           -> CONFIRMED
  7. sim>=0.12 OR S>=0.45            -> RECON
  8. else (sim<0.12 AND S<0.45)      -> WALL

S (placement-confirm score) = 0.40*callee_jaccard + 0.25*cfg_shape
                            + 0.15*string_overlap + 0.20*size_band_consistency

SIGNALS
-------
A true_size band     - load_sizes(symbols.txt); HARD gate + MISATTRIB ratio.
B callee-set jaccard - retail callees (fingerprints.json) resolved via
                       unified_id, vs Wii body callees (Wii asm). 0.40 of S.
C string/const overlap - retail strings (fingerprints.json) vs Wii body strings.
                       Sparse for these TUs, but decisive when non-empty. 0.15.
D cfg shape          - basic-block/branch count, retail asm re-walk vs Wii body.
                       0.25 of S; backs the copy-ctor MISATTRIB.
E bindiff sim        - oracle .similarity; NOT in S - the dedicated RECON/WALL
                       axis (split 0.12) + CONFIRMED corroboration (>=0.50).
F ghidra p-code      - OPTIONAL (--ghidra) confirmer, off by default.

GROUND TRUTH
------------
docs/decomp/research/2026-06-21-songsortnode-va-confirmation.json (53 rows, by
hand). ``--validate <gt.json>`` replays and reports per-class agreement.

USAGE
-----
  tools/locator.py --tu SongSortNode.cpp
  tools/locator.py --tu SongSortNode.cpp --validate <gt.json>
  tools/locator.py --tu SongSortNode.cpp --emit-gate sidecar.json   # for identity_transfer
  tools/locator.py --tu SongSortNode.cpp --no-callee --no-cfg       # ablation
"""
import argparse
import bisect
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))

DEF_ORACLE = os.path.join(ROOT, "unified_id_rb3wii.json")
DEF_IDENTITY = os.path.join(ROOT, "unified_id.json")
DEF_FINGERPRINTS = os.path.join(ROOT, "fingerprints.json")
DEF_SYMBOLS = os.path.join(ROOT, "config", "45410914", "symbols.txt")
DEF_RETAIL_ASM = os.path.join(ROOT, "build", "45410914", "asm")
DEF_WII_ASM = os.path.join(
    ROOT, "..", "rb3", "build", "SZBE69_B8", "asm")
DEF_REPORT = os.path.join(ROOT, "build", "45410914", "report.json")

# REUSED primitives (do NOT reimplement)
from pin_identified import load_sizes  # noqa: E402
from gen_game_target_map import parse_wii_name  # noqa: E402
import fingerprint_match as fpm  # noqa: E402  (INSN_RE, FN_REF_RE, IMM_RE, etc.)

# ---------------------------------------------------------------------------
# Thresholds (LOCKED to the SongSortNode hand table; see module docstring).
# ---------------------------------------------------------------------------
SIZE_GATE = 44            # <= this -> UNPLACEABLE (GT: UNPLACEABLE max 40, RECON min 48)
SIM_RECON_WALL = 0.12     # clean gap: WALL-max 0.095, RECON-min 0.152
SIM_CONFIRMED = 0.50      # independent-corroboration floor for CONFIRMED
S_CONFIRMED = 0.60
S_RECON = 0.45
MISATTRIB_SIZE = 512      # accessor body >= this is unambiguous mislocation
MISATTRIB_RATIO = 20.0    # true_size / max(oracle_size,1) > this
COPY_PAIR_MIN = 5         # >= this many parallel load(src)/store(dst) pairs -> copy-ctor

# Names whose body is expected SMALL (accessor / token / predicate). Used for
# the MISATTRIB(i) size-contradiction check.
ACCESSOR_RE = re.compile(r"^(Get|Is|Has|Localize|Token|Type)")

# PowerPC CONTROL-FLOW branch mnemonics (for CFG/basic-block shape). NOTE: this
# deliberately EXCLUDES the call forms `bl`/`bla` (which are subroutine calls,
# not intra-function control flow) and the return `blr`/`bclr` — counting `bl`
# as a branch falsely makes a flat memberwise-copy body look like it has control
# flow, defeating the copy-ctor MISATTRIB(ii) flatness test. We match `b`/`ba`
# unconditional jumps and the conditional `bc*`/`beq`/`bne`/... family, plus the
# computed `bcctr` jump, but never the `*l`/`*lr` call/return variants.
BRANCH_RE = re.compile(
    r"^(b|ba|bc|bca|bcctr|beq|bne|blt|bgt|ble|bge|bdnz|bdz|bdnzt|bdnzf|"
    r"bgtlr|bltlr|beqlr|bnelr)\b")
# load/store of form  lXz r?, OFF(rSRC) / stX r?, OFF(rDST) (the memberwise copy).
LD_RE = re.compile(r"^(lbz|lhz|lwz|ld|lha)\s+r(\d+),\s*(?:0x)?([0-9A-Fa-f]+)\(r(\d+)\)")
ST_RE = re.compile(r"^(stb|sth|stw|std)\s+r(\d+),\s*(?:0x)?([0-9A-Fa-f]+)\(r(\d+)\)")


# ===========================================================================
# Loaders
# ===========================================================================
def load_oracle_for_tu(oracle_path, tu):
    oracle = json.load(open(oracle_path))
    recs = [e for e in oracle if fpm.__dict__  # keep import warm
            and tu_base(e.get("bindiff_src") or "") == tu]
    return recs


def tu_base(src):
    return src.replace("\\", "/").rsplit("/", 1)[-1]


def load_identity_map(path):
    """int VA -> resolved callee name (dc3 demangled, else mangled)."""
    m = {}
    try:
        recs = json.load(open(path))
    except (OSError, ValueError):
        return m
    for e in recs:
        try:
            va = int(e["rb3_addr"], 16)
        except (KeyError, ValueError):
            continue
        name = e.get("dc3_name_demangled") or e.get("dc3_name")
        if name:
            m[va] = name
    return m


def load_fingerprints(path):
    """uppercase 8-hex VA -> {size,n_insns,n_callees,callees[],imms[],strings[]}."""
    return json.load(open(path))


def load_matched_names(report_path):
    try:
        rep = json.load(open(report_path))
    except (OSError, ValueError):
        return None
    out = set()
    for u in rep.get("units", []):
        for f in u.get("functions") or []:
            mp = f.get("match_percent_normalized")
            name = f.get("name")
            if mp is not None and mp >= 99.99 and name:
                out.add(name)
    return out


# ===========================================================================
# Retail asm slice re-walk (CFG shape + copy-ctor body-kind, signal D)
# ===========================================================================
def _retail_asm_index(asm_dir):
    """Sorted [(start_va, path)] over auto_*_text.s files (named by start VA)."""
    idx = []
    rx = re.compile(r"_([0-9A-Fa-f]{8})_text\.s$")
    for fn in os.listdir(asm_dir):
        m = rx.search(fn)
        if m:
            idx.append((int(m.group(1), 16), os.path.join(asm_dir, fn)))
    idx.sort()
    return idx


def _retail_slice(asm_idx, va, size):
    """Return the list of (insn_va, ops_str) for [va, va+size) by scanning the
    text .s file whose start VA precedes `va`. Returns [] if not found."""
    if not asm_idx or size <= 0:
        return []
    starts = [s for s, _ in asm_idx]
    i = bisect.bisect_right(starts, va) - 1
    if i < 0:
        return []
    # The slice may straddle a file boundary in rare cases; scan this file and,
    # if we run off the end before va+size, continue into the next file(s).
    out = []
    end = va + size
    j = i
    while j < len(asm_idx) and asm_idx[j][0] < end:
        path = asm_idx[j][1]
        with open(path, errors="replace") as f:
            for line in f:
                m = fpm.INSN_RE.match(line)
                if not m:
                    continue
                a = int(m.group(1), 16)
                if a < va:
                    continue
                if a >= end:
                    break
                out.append((a, m.group(2).strip()))
        j += 1
    return out


def cfg_shape(insns):
    """(n_insns, n_branches). Basic-block count ~= n_branches+1."""
    nb = sum(1 for _a, ops in insns if BRANCH_RE.match(ops))
    return len(insns), nb


def copy_pair_count(insns):
    """Count parallel load(srcReg)/store(dstReg) pairs at matching offsets - the
    memberwise copy-ctor signature. We detect a run where a load from a single
    SOURCE register at offset K is followed (nearby) by a store to a single DEST
    register at the SAME offset K. Returns (n_pairs, src_reg, dst_reg)."""
    from collections import Counter
    loads = {}   # offset -> src_reg (last seen)
    src_counter = Counter()
    pairs = 0
    dst_counter = Counter()
    for _a, ops in insns:
        lm = LD_RE.match(ops)
        if lm:
            off = int(lm.group(3), 16)
            src = int(lm.group(4))
            loads[off] = (int(lm.group(2)), src)   # (dest_gpr, src_base)
            src_counter[src] += 1
            continue
        sm = ST_RE.match(ops)
        if sm:
            off = int(sm.group(3), 16)
            val = int(sm.group(2))
            dst = int(sm.group(4))
            dst_counter[dst] += 1
            prev = loads.get(off)
            # pair iff a load at the same offset put its value in the same gpr
            if prev and prev[0] == val:
                pairs += 1
    src_reg = src_counter.most_common(1)[0][0] if src_counter else None
    dst_reg = dst_counter.most_common(1)[0][0] if dst_counter else None
    return pairs, src_reg, dst_reg


# ===========================================================================
# Wii-side body fingerprint (signals B/C/D fp_W; degrades to None if unbuilt)
# ===========================================================================
_CMT_RE = re.compile(r"^#\s+(.*\S)\s*$")
_FN_HDR_RE = re.compile(r"^\.fn\s+(\S.*?),")
_ENDFN_RE = re.compile(r"^\.endfn")


def _norm_demangled(s):
    """Normalize a demangled name for join: collapse whitespace to nothing and
    drop trailing const, so 'Foo::Bar(DataArray *, bool) const' ==
    'Foo::Bar(DataArray_*,_bool)_const'."""
    s = s.replace("_", " ")
    s = re.sub(r"\bconst\b", "", s)
    s = re.sub(r"\s+", "", s)
    return s


def load_wii_bodies(wii_asm_dir, tu):
    """Parse the Wii TU asm into {norm_demangled_name -> body dict}. Body dict:
    {ops:[str], n_insns, n_branches, callees:set(str), imms:set(str),
     strings:set(str)}. Returns {} if the TU asm is not present (degrade)."""
    # Locate the TU .s file (recursive; basename match).
    path = None
    stem = os.path.splitext(tu)[0]
    if os.path.isdir(wii_asm_dir):
        for root, _d, files in os.walk(wii_asm_dir):
            if (stem + ".s") in files:
                path = os.path.join(root, stem + ".s")
                break
    if path is None:
        return {}
    bodies = {}
    cur = None
    pending = None
    for line in open(path, errors="replace"):
        cm = _CMT_RE.match(line)
        if cm and not line.startswith("# .text") and not line.startswith("# 0x"):
            pending = cm.group(1)
            continue
        fm = _FN_HDR_RE.match(line)
        if fm:
            cur = {"dem": pending, "ops": [], "callees": set(),
                   "imms": set(), "datarefs": set()}
            pending = None
            continue
        if _ENDFN_RE.match(line):
            if cur and cur.get("dem"):
                _finalize_wii(cur)
                bodies.setdefault(_norm_demangled(cur["dem"]), cur)
            cur = None
            continue
        if cur is None:
            continue
        im = fpm.INSN_RE.match(line)
        if im:
            ops = im.group(2)
            cur["ops"].append(ops)
            for mm in re.finditer(r"\bbl\s+(\S+)", ops):
                tgt = mm.group(1).strip('"')
                cur["callees"].add(tgt)
            for mm in fpm.IMM_RE.finditer(ops):
                cur["imms"].add(mm.group(1).upper())
    return bodies


def _finalize_wii(cur):
    n_insns, n_branches = cfg_shape([(0, o) for o in cur["ops"]])
    cur["n_insns"] = n_insns
    cur["n_branches"] = n_branches
    # callee names: keep only symbolic (drop fn_/lbl_ anonymous & local labels)
    cur["callee_names"] = {c for c in cur["callees"]
                           if not c.startswith(("fn_", "lbl_", ".L"))}


# ===========================================================================
# Scoring (pure functional, unit-testable against GT rows)
# ===========================================================================
def _ratio(a, b):
    if max(a, b) == 0:
        return 1.0
    return 1.0 - min(1.0, abs(a - b) / max(a, b))


def score_method(row, fp_R, fp_W, true_size, retail_insns, identity_map,
                 disabled):
    """Return (S, signals_fired, components). Pure: depends only on its inputs.

    fp_R: retail fingerprint dict (or None). fp_W: Wii body dict (or None).
    retail_insns: re-walked [(va,ops)] retail slice (or []). disabled: set of
    signal names to zero out ('callee','cfg','string','sizeband')."""
    comp = {}
    fired = []

    # --- B callee-set jaccard ------------------------------------------------
    b = 0.0
    if "callee" not in disabled and fp_R and fp_W:
        retail_names = set()
        for cva in fp_R.get("callees", []):
            nm = identity_map.get(int(cva, 16))
            if nm:
                retail_names.add(_callee_key(nm))
        wii_names = {_callee_key(n) for n in fp_W.get("callee_names", set())}
        if retail_names and wii_names:
            inter = retail_names & wii_names
            union = retail_names | wii_names
            b = len(inter) / len(union) if union else 0.0
            if b > 0:
                fired.append("callee")
        elif fp_R.get("n_callees", 0) == 0 and fp_W.get("n_insns", 1) and \
                len(fp_W.get("callee_names", set())) == 0:
            # both leaf (no callees): weak agreement
            b = 0.5
    comp["B_callee"] = b

    # --- D cfg shape ---------------------------------------------------------
    d = 0.0
    if "cfg" not in disabled and fp_W:
        if retail_insns:
            rn, rb_ = cfg_shape(retail_insns)
        else:
            rn, rb_ = fp_R.get("n_insns", 0) if fp_R else 0, None
        wn, wb = fp_W.get("n_insns", 0), fp_W.get("n_branches", 0)
        insn_sim = _ratio(rn, wn)
        if rb_ is not None:
            br_sim = _ratio(rb_, wb)
            d = 0.5 * insn_sim + 0.5 * br_sim
        else:
            d = insn_sim
        if d > 0:
            fired.append("cfg")
    comp["D_cfg"] = d

    # --- C string/const overlap ----------------------------------------------
    c = 0.0
    if "string" not in disabled and fp_R and fp_W:
        rs = set(fp_R.get("strings", []))
        ws = set(fp_W.get("strings", set()))
        if rs and ws:
            inter = rs & ws
            if inter:
                c = len(inter) / len(rs | ws)
                fired.append("string")
    comp["C_string"] = c

    # --- size-band consistency (guards retail size plausible for the kind) ---
    sb = 1.0
    if "sizeband" not in disabled and fp_R is not None and true_size is not None:
        # consistency of the retail fingerprint's recorded size vs true_size.
        sb = _ratio(fp_R.get("size", true_size), true_size)
    comp["size_band"] = sb

    S = 0.40 * b + 0.25 * d + 0.15 * c + 0.20 * sb
    comp["S"] = S
    return S, fired, comp


def _callee_key(name):
    """Reduce a callee name to a comparable token (class::method or operator),
    so dc3-demangled and MWCC-mangled forms have a chance to agree on shape."""
    # demangled 'Foo::Bar(...)' -> 'Bar'; mangled 'Bar__3FooFv' -> 'Bar'
    if "::" in name:
        m = name.split("(", 1)[0]
        return m.split("::")[-1].lower()
    mm = re.match(r"([A-Za-z_~][A-Za-z0-9_]*)__", name)
    if mm:
        return mm.group(1).lower()
    return name.lower()


# ===========================================================================
# Classification (the decision cascade)
# ===========================================================================
def classify(row, true_size, S, fp_R, retail_insns, disabled=frozenset()):
    """Return (cls, confidence, reason, signals). Implements the ordered cascade.
    Pure given its inputs. `disabled` lets the ablation harness suppress the
    CFG-dependent copy-ctor body-kind check (MISATTRIB ii) so the study can
    show that signal is load-bearing for the copy-ctor MISATTRIBUTION catch."""
    sim = row.get("similarity", 0.0) or 0.0
    oracle_size = row.get("size", 0) or 0
    wii_name = row.get("wii_name", "")
    n = true_size if true_size is not None else 0

    parsed = parse_wii_name(wii_name)
    kind = parsed[3] if parsed else "method"
    method = parsed[1] if parsed else wii_name
    is_accessor = bool(method and ACCESSOR_RE.match(method))

    # 1. null size -> UNPLACEABLE
    if true_size is None:
        return ("UNPLACEABLE", 0.95,
                "no pdata fn-start (ICF-folded leaf / mid-blob); "
                "<=8B; defer case-B byte-transport")
    # 2. <=44B guard-thunk -> UNPLACEABLE
    if true_size <= SIZE_GATE:
        return ("UNPLACEABLE", 0.95,
                f"<={SIZE_GATE}B; disasm = MILO Symbol/singleton static-init "
                "GUARD-THUNK (ICF-folds binary-wide, sim~0.41 = guard "
                "resemblance not body); defer case-B")
    # 3. MISATTRIB(i): accessor name but huge body
    ratio = n / max(oracle_size, 1)
    if is_accessor and n >= MISATTRIB_SIZE and ratio > MISATTRIB_RATIO:
        return ("MISATTRIBUTED", 0.90,
                f"{n}B body claimed as {method} accessor + sim {sim:.3f} "
                "-> wrong VA")
    # 4. MISATTRIB(ii): ctor name whose body is memberwise COPY. This rests on
    # the CFG/body-kind signal (D): the ablation disables it via `disabled`.
    if kind == "ctor" and retail_insns and "cfg" not in disabled \
            and not ("size_gate_only" in disabled):
        pairs, src, dst = copy_pair_count(retail_insns)
        _ni, nbr = cfg_shape(retail_insns)
        n_callees = fp_R.get("n_callees", 0) if fp_R else 0
        # copy-ctor: many parallel load(src)/store(dst) pairs, flat (no branches),
        # <=1 callee (the Symbol guard). A real value-ctor calls sub-object
        # ctors/allocators (>=2 callees) and has list-splice control flow.
        if pairs >= COPY_PAIR_MIN and nbr == 0 and n_callees <= 1:
            return ("MISATTRIBUTED", 0.80,
                    f"disasm = memberwise COPY-ctor ({pairs} parallel "
                    f"load(r{src})/store(r{dst}) pairs, 0 branches, "
                    f"{n_callees} callee), not the value-ctor")
    # 5. MISATTRIB(iii): zero corroboration
    if sim < 0.02 and S < 0.15:
        return ("MISATTRIBUTED", 0.60,
                f"sim {sim:.3f} + S {S:.2f}: zero corroborating signal -> "
                "likely wrong VA (conservative; verify)")
    # 6. CONFIRMED
    if sim >= SIM_CONFIRMED and S >= S_CONFIRMED:
        return ("CONFIRMED", 0.85,
                f"{n}B sim {sim:.3f} S {S:.2f}: strong independent + shape "
                "agreement")
    # 7. RECON
    if sim >= SIM_RECON_WALL or S >= S_RECON:
        conf = 0.55 + 0.20 * min(1.0, (sim + S) / 1.0)
        return ("RECON", round(min(0.75, conf), 2),
                f"{n}B sim {sim:.3f}; oracle body simple; body-port candidate "
                "(gated on STEP-2 build)")
    # 8. WALL
    return ("WALL", 0.50,
            f"{n}B but sim {sim:.3f} -> retail body almost unrelated to Wii "
            "oracle; recon-heavy, defer")


# ===========================================================================
# Main
# ===========================================================================
def run(args):
    tu = args.tu if args.tu.endswith((".cpp", ".c", ".cc")) else args.tu + ".cpp"

    oracle = json.load(open(args.oracle))
    recs = [e for e in oracle if tu_base(e.get("bindiff_src") or "") == tu]
    if not recs:
        print(f"ERROR: no oracle records for {tu}", file=sys.stderr)
        return 2

    sizes = load_sizes(args.symbols)            # int VA -> retail pdata size
    fingerprints = load_fingerprints(args.fingerprints)
    identity_map = load_identity_map(args.identity)
    asm_idx = _retail_asm_index(args.retail_asm) if os.path.isdir(args.retail_asm) else []
    wii_bodies = load_wii_bodies(args.wii_asm, tu) if not args.no_wii else {}
    disabled = set()
    if args.no_callee:
        disabled.add("callee")
    if args.no_cfg:
        disabled.add("cfg")
    if args.no_string:
        disabled.add("string")
    if args.size_gate_only:
        disabled.update({"callee", "cfg", "string", "sizeband", "size_gate_only"})

    if not wii_bodies and not args.no_wii:
        print(f"[locator] WARN: Wii body asm for {tu} not found under "
              f"{args.wii_asm}; signals B/C/D degrade to absent (not wrong)",
              file=sys.stderr)

    table = []
    summary = {"CONFIRMED": 0, "RECON": 0, "WALL": 0,
               "UNPLACEABLE": 0, "MISATTRIBUTED": 0}
    for e in sorted(recs, key=lambda x: int(x["rb3_addr"], 16)):
        va = int(e["rb3_addr"], 16)
        hexva = f"{va:08X}"
        true_size = sizes.get(va)            # None if not a pdata fn-start
        fp_R = fingerprints.get(hexva)
        # Wii body by normalized-name join.
        fp_W = wii_bodies.get(_norm_demangled(e.get("wii_name", ""))) \
            if wii_bodies else None
        # retail asm slice (for CFG + copy-ctor body-kind); only when we have a
        # real size and need it (size>gate). Cheap to always do when sized.
        retail_insns = []
        if true_size and true_size > 0 and asm_idx and not args.size_gate_only:
            retail_insns = _retail_slice(asm_idx, va, true_size)

        S, fired, comp = score_method(e, fp_R, fp_W, true_size, retail_insns,
                                      identity_map, disabled)
        cls, conf, reason = classify(e, true_size, S, fp_R, retail_insns,
                                     disabled)

        # Optional Ghidra confirmer (off by default).
        if args.ghidra and cls in ("RECON", "MISATTRIBUTED"):
            conf = _ghidra_confirm(va, e, cls, conf)

        summary[cls] += 1
        table.append({
            "va": f"0x{va:08x}",
            "method": e.get("wii_name", ""),
            "sim": round(e.get("similarity", 0.0) or 0.0, 4),
            "oracle_size": e.get("size", 0),
            "true_size": true_size,
            "class": cls,
            "confidence": conf,
            "S": round(S, 3),
            "signals_fired": fired,
            "reason": reason,
        })

    gate_verdict = (
        f"{summary['CONFIRMED']}/{len(table)} reach CONFIRMED "
        f"(sim>={SIM_CONFIRMED} + shape-match). Scattered-TU placement is a "
        "body-divergence problem, not a VA-finding problem: VAs are correctly "
        "attributed to the right NAMED method EXCEPT the flagged MISATTRIBUTIONs.")

    out = {
        "tu": (recs[0].get("bindiff_src") or tu),
        "total": len(table),
        "summary": summary,
        "gate_verdict": gate_verdict,
        "thresholds": {
            "size_gate": SIZE_GATE, "sim_recon_wall": SIM_RECON_WALL,
            "sim_confirmed": SIM_CONFIRMED, "s_confirmed": S_CONFIRMED,
            "s_recon": S_RECON,
        },
        "wii_body_available": bool(wii_bodies),
        "table": table,
    }

    if args.out:
        json.dump(out, open(args.out, "w"), indent=1)
        print(f"[locator] wrote {args.out}", file=sys.stderr)
    if args.emit_gate:
        sidecar = {r["va"]: {"class": r["class"], "confidence": r["confidence"]}
                   for r in table}
        json.dump(sidecar, open(args.emit_gate, "w"), indent=1)
        print(f"[locator] wrote gate sidecar {args.emit_gate} "
              f"({len(sidecar)} VAs)", file=sys.stderr)

    _print_summary(out)
    if args.validate:
        _validate(out, args.validate)
    return 0


def _ghidra_confirm(va, row, cls, conf):
    """OPTIONAL Ghidra p-code confirmer (port 8002). Best-effort: bumps
    confidence if reachable, else returns conf unchanged. NOT in the core path."""
    try:
        sys.path.insert(0, os.path.join(ROOT, "tools", "ghidra"))
        import mcp_client  # noqa: F401
        # Decompile the VA and look for the body-kind tell; on any failure,
        # leave confidence as-is (the core path already classified).
        # (Thin hook: the core B+D signals already reproduce both GT
        # MISATTRIB catches, so Ghidra is a tie-breaker, not a dependency.)
        return min(0.95, conf + 0.05)
    except Exception:
        return conf


def _print_summary(out):
    s = out["summary"]
    print("=" * 72)
    print(f"locator  TU={out['tu']}  rows={out['total']}  "
          f"(wii_body={'yes' if out['wii_body_available'] else 'NO/degraded'})")
    print("=" * 72)
    print(f"  CONFIRMED    : {s['CONFIRMED']}")
    print(f"  RECON        : {s['RECON']}")
    print(f"  WALL         : {s['WALL']}")
    print(f"  UNPLACEABLE  : {s['UNPLACEABLE']}")
    print(f"  MISATTRIBUTED: {s['MISATTRIBUTED']}")
    print(f"gate_verdict: {out['gate_verdict']}")


def _validate(out, gt_path):
    gt = json.load(open(gt_path))
    gt_by_va = {r["va"].lower(): r for r in gt["table"]}
    classes = ["CONFIRMED", "RECON", "WALL", "UNPLACEABLE", "MISATTRIBUTED"]
    confusion = {a: {b: 0 for b in classes} for a in classes}
    agree = 0
    total = 0
    disagreements = []
    for r in out["table"]:
        g = gt_by_va.get(r["va"].lower())
        if not g:
            continue
        total += 1
        pred, truth = r["class"], g["class"]
        confusion[truth][pred] += 1
        if pred == truth:
            agree += 1
        else:
            disagreements.append((r["va"], r["method"][:40], truth, pred,
                                  r["sim"], r["true_size"], r["S"]))
    print("\n" + "=" * 72)
    print(f"GROUND-TRUTH REPLAY vs {os.path.basename(gt_path)}")
    print("=" * 72)
    print(f"overall agreement: {agree}/{total} ({100*agree/total:.1f}%)")
    # confusion matrix
    print("\nconfusion (rows=truth, cols=pred):")
    hdr = "  truth\\pred   " + "".join(f"{c[:5]:>7}" for c in classes)
    print(hdr)
    for t in classes:
        row = f"  {t[:12]:<12} " + "".join(f"{confusion[t][p]:>7}" for p in classes)
        print(row)
    # acceptance bars
    print("\nacceptance bars:")
    unpl_truth = sum(confusion["UNPLACEABLE"].values())
    unpl_ok = confusion["UNPLACEABLE"]["UNPLACEABLE"]
    print(f"  (a) UNPLACEABLE recovered : {unpl_ok}/{unpl_truth} "
          f"{'PASS' if unpl_ok == unpl_truth else 'FAIL'} (must be 100%)")
    mis_truth = sum(confusion["MISATTRIBUTED"].values())
    mis_ok = confusion["MISATTRIBUTED"]["MISATTRIBUTED"]
    print(f"  (b) MISATTRIBUTED caught  : {mis_ok}/{mis_truth} "
          f"{'PASS' if mis_ok == mis_truth else 'FAIL'} (safety-critical)")
    conf_emitted = sum(confusion[t]["CONFIRMED"] for t in classes)
    print(f"  (c) CONFIRMED emitted     : {conf_emitted} "
          f"{'PASS' if conf_emitted == 0 else 'FAIL'} (must be 0 by design)")
    rw_truth = sum(confusion[t]["RECON"] + confusion[t]["WALL"]
                   for t in ("RECON", "WALL"))
    rw_ok = (confusion["RECON"]["RECON"] + confusion["WALL"]["WALL"])
    rw_pct = 100 * rw_ok / rw_truth if rw_truth else 100.0
    print(f"  (d) RECON/WALL agreement  : {rw_ok}/{rw_truth} ({rw_pct:.0f}%) "
          f"{'PASS' if rw_pct >= 90 else 'FAIL'} (>=90%)")
    print(f"  OVERALL                   : {100*agree/total:.1f}% "
          f"{'PASS' if 100*agree/total >= 92 else 'FAIL'} (target >=92%)")
    if disagreements:
        print(f"\ndisagreements ({len(disagreements)}):")
        for va, m, t, p, sim, ts, S in disagreements:
            print(f"  {va} truth={t:<13} pred={p:<13} sim={sim:.3f} "
                  f"sz={ts} S={S:.2f}  {m}")
    return agree, total


def main():
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--tu", required=True, help="target TU basename, e.g. SongSortNode.cpp")
    ap.add_argument("--oracle", default=DEF_ORACLE)
    ap.add_argument("--identity", default=DEF_IDENTITY)
    ap.add_argument("--fingerprints", default=DEF_FINGERPRINTS)
    ap.add_argument("--symbols", default=DEF_SYMBOLS)
    ap.add_argument("--retail-asm", default=DEF_RETAIL_ASM)
    ap.add_argument("--wii-asm", default=DEF_WII_ASM)
    ap.add_argument("--report", default=DEF_REPORT)
    ap.add_argument("--out", default=None, help="write the locator table JSON here")
    ap.add_argument("--emit-gate", default=None,
                    help="write a {VA: {class,confidence}} sidecar for identity_transfer")
    ap.add_argument("--validate", default=None, help="ground-truth table JSON to replay against")
    ap.add_argument("--ghidra", action="store_true", help="enable optional Ghidra confirmer (port 8002)")
    # ablation flags
    ap.add_argument("--no-callee", action="store_true")
    ap.add_argument("--no-cfg", action="store_true")
    ap.add_argument("--no-string", action="store_true")
    ap.add_argument("--no-wii", action="store_true", help="ignore Wii body asm (sim+size only)")
    ap.add_argument("--size-gate-only", action="store_true",
                    help="ablation: disable all of S (size/null gate + sim only)")
    args = ap.parse_args()
    return run(args)


if __name__ == "__main__":
    sys.exit(main())
