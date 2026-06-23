#!/usr/bin/env python3
"""Generate the band3 porting worklist from the Wii->Xenon ghidriff identities.

CONSUMES (no ghidriff run): the 978 ACCEPT identities ingested into rb3-xenon
(`ghidriff_identities.json`, gitignored/regenerable) and the PRODUCTION pairing
set (`scripts/target_symbol_map.json`). Emits the NET-NEW band3 subset (RB3 game
code identities DC3 cannot supply) as a porting worklist + identity oracle.

NET-NEW = ghidriff identity whose normalized Xenon `rb3_addr` is NOT already a
key in `target_symbol_map.json` (the file objdiff actually pairs against). For
band3 this is 232 functions across 93 TUs (RB3 gameplay TUs the active port has
not yet reached).

This is a TARGETING/PORTING worklist + per-fn identity oracle, NOT a
target_symbol_map injection. The source for not-yet-ported TUs isn't compiled, so
there is no MSVC symbol to pair yet; injecting the CW/MWCC name as a key would
mis-pair objdiff at our ~0.90 precision (actively harmful). Both outputs are
additive + reversible.

Outputs (relative to rb3-xenon repo root):
  band3_port_worklist.json          gitignored data feed (one row per identity)
  docs/plans/band3-port-worklist.md tracked, TU-ranked human checklist

Run from anywhere:  python3 tools/gen_band3_port_worklist.py
Paths are absolute, so cwd does not matter.
"""
import json
import os
import re
import sys
from collections import Counter, defaultdict

# --- repo roots (absolute; cwd-independent) -----------------------------------
RB3X = "/home/free/code/milohax/rb3-xenon"
RB3 = "/home/free/code/milohax/rb3"
IDENT_PATH = os.path.join(RB3X, "ghidriff_identities.json")
TSM_PATH = os.path.join(RB3X, "scripts", "target_symbol_map.json")
CW_MAP = os.path.join(RB3, "orig", "SZBE69_B8", "files", "band_r_wii.map")
OUT_JSON = os.path.join(RB3X, "band3_port_worklist.json")
OUT_MD = os.path.join(RB3X, "docs", "plans", "band3-port-worklist.md")

# CW demangler: prefer the local rb3-xenon copy, else the rb3 round2 source.
sys.path.insert(0, os.path.join(RB3X, "tools"))
sys.path.insert(0, os.path.join(RB3, "docs", "decomp", "xenon-hardening", "round2", "forensics"))
from demangle_cw import demangle  # noqa: E402

HIGH_MATCH = (
    "ExactInstructionsFunctionHasher",
    "SwitchSigHasher",
    "Implied Match",
    "SymbolsHash",
)


def norm(a):
    """Address join key: lower; strip 0x; lstrip zeros; zfill 8."""
    a = str(a).lower()
    if a.startswith("0x"):
        a = a[2:]
    return a.lstrip("0").zfill(8)


def confidence_label(e):
    mts = e.get("match_types", [])
    if any(m in mts for m in HIGH_MATCH):
        return "high"
    sc = e.get("bsim_simconf")
    if sc is None:
        return "unknown"
    if sc >= 30:
        return "bsim>=30"
    if sc >= 20:
        return "bsim20-30"
    return "bsim15-20"


# Rank weight: HIGH and bsim>=30 are the "safest first" tier.
LABEL_ORDER = {"high": 0, "bsim>=30": 1, "bsim20-30": 2, "bsim15-20": 3, "unknown": 4}


def parse_cw_map(symbols):
    """symbol -> (bank8_addr_set, src_relpath) for the requested symbols."""
    sym2addr = defaultdict(set)
    sym2src = {}
    want = set(symbols)
    with open(CW_MAP) as f:
        for line in f:
            parts = line.split()
            if len(parts) < 7:
                continue
            # cols: secoff size addr fileoff align symbol ... path.o
            addr = parts[2]
            sym = parts[5]
            if sym not in want:
                continue
            if len(addr) == 8 and re.fullmatch(r"[0-9a-fA-F]{8}", addr):
                sym2addr[sym].add(addr.lower())
            for p in parts[6:]:
                if p.endswith(".o") and "src" in p:
                    sym2src[sym] = p
                    break
    return sym2addr, sym2src


def src_relpath(tu, raw_o_path):
    """Map a CW .o path -> rb3 src/band3/<sub>/<TU>.cpp."""
    m = re.search(r"src[\\/](.+?)[\\/]wii_release[\\/]([^\\/]+\.o)", raw_o_path or "")
    if not m:
        return f"src/band3/?/{tu[:-2]}.cpp"
    sub = m.group(1).replace("\\", "/")
    cpp = m.group(2)[:-2] + ".cpp"
    return f"src/band3/{sub}/{cpp}"


def main():
    ident = json.load(open(IDENT_PATH))
    tsm_n = {norm(k) for k in json.load(open(TSM_PATH))}

    band3 = [
        e
        for e in ident
        if e.get("category") == "band3" and norm(e["rb3_addr"]) not in tsm_n
    ]

    syms = {e["wii_symbol"] for e in band3}
    sym2addr, sym2src = parse_cw_map(syms)

    # ---- self-verifying VERIFY pass -----------------------------------------
    errs = []
    for e in band3:
        s = e["wii_symbol"]
        want = norm(e["wii_addr_bank8"])
        if s not in sym2addr:
            errs.append(f"MISSING in CW map: {s}")
        elif want not in {norm(a) for a in sym2addr[s]}:
            errs.append(f"ADDR MISMATCH: {s} want {want} got {sorted(sym2addr[s])}")
        if norm(e["rb3_addr"]) in tsm_n:
            errs.append(f"ALREADY IN MAP: {e['rb3_addr']}")
    if errs:
        print("VERIFY FAILED:", file=sys.stderr)
        for x in errs[:50]:
            print("  " + x, file=sys.stderr)
        sys.exit(1)

    # ---- build worklist rows ------------------------------------------------
    rows = []
    for e in band3:
        tu = e["tu"]
        row = {
            "rb3_addr": e["rb3_addr"],
            "wii_addr_bank8": e["wii_addr_bank8"],
            "wii_symbol": e["wii_symbol"],
            "wii_demangled": demangle(e["wii_symbol"]),
            "tu": tu,
            "src_path": src_relpath(tu, sym2src.get(e["wii_symbol"], "")),
            "match_type": (e.get("match_types") or ["?"])[0],
            "match_types": e.get("match_types", []),
            "confidence_label": confidence_label(e),
            "simconf": e.get("bsim_simconf"),
            "dc3_cannot_provide": True,
        }
        rows.append(row)

    rows.sort(key=lambda r: (r["tu"], r["rb3_addr"]))

    # ---- per-TU aggregation + ranking ---------------------------------------
    by_tu = defaultdict(list)
    for r in rows:
        by_tu[r["tu"]].append(r)

    def tu_stats(tu_rows):
        c = Counter(r["confidence_label"] for r in tu_rows)
        return {
            "n": len(tu_rows),
            "high": c.get("high", 0),
            "bsim>=30": c.get("bsim>=30", 0),
            "bsim20-30": c.get("bsim20-30", 0),
            "bsim15-20": c.get("bsim15-20", 0),
            "src_path": tu_rows[0]["src_path"],
        }

    tu_summary = {tu: tu_stats(rs) for tu, rs in by_tu.items()}
    # rank: (#high + #bsim>=30) desc, then total desc, then TU name
    ranked_tus = sorted(
        tu_summary,
        key=lambda tu: (
            -(tu_summary[tu]["high"] + tu_summary[tu]["bsim>=30"]),
            -tu_summary[tu]["n"],
            tu,
        ),
    )

    # ---- emit JSON feed ------------------------------------------------------
    totals = Counter(r["confidence_label"] for r in rows)
    feed = {
        "_meta": {
            "description": "Net-new band3 Wii->Xenon identities (RB3 game code DC3 cannot provide). "
            "Porting worklist + per-fn identity oracle. NOT a target_symbol_map injection.",
            "generated_by": "tools/gen_band3_port_worklist.py",
            "source_identities": "ghidriff_identities.json (ACCEPT tier, ghidriff-run3)",
            "net_new_filter": "rb3_addr not a key in scripts/target_symbol_map.json (normalized join)",
            "precision_band3_human_judged": 0.900,
            "dominant_failure_mode": "same-TU sibling aliasing (~10%): near-identical bodies "
            "differing in a type-tag immediate / STL node-size literal; verify small same-TU bodies",
            "count": len(rows),
            "tu_count": len(by_tu),
            "confidence_totals": dict(totals),
            "confidence_legend": {
                "high": "ExactInstructions/SwitchSig/Implied/SymbolsHash, or BSim simconf>=30",
                "bsim>=30": "BSim simconf >= 30",
                "bsim20-30": "BSim simconf 20-30",
                "bsim15-20": "BSim simconf 15-20",
            },
        },
        "tu_summary": tu_summary,
        "ranked_tus": ranked_tus,
        "worklist": rows,
    }
    with open(OUT_JSON, "w") as f:
        json.dump(feed, f, indent=2)
    print(f"wrote {OUT_JSON}  ({len(rows)} rows, {len(by_tu)} TUs)")

    # ---- emit markdown checklist --------------------------------------------
    write_markdown(OUT_MD, rows, by_tu, tu_summary, ranked_tus, totals)
    print(f"wrote {OUT_MD}")


def write_markdown(path, rows, by_tu, tu_summary, ranked_tus, totals):
    high_total = totals.get("high", 0)
    s30 = totals.get("bsim>=30", 0)
    s2030 = totals.get("bsim20-30", 0)
    s1520 = totals.get("bsim15-20", 0)
    L = []
    A = L.append
    A("# band3 porting worklist — net-new Wii→Xenon identities (DC3-cannot-provide)")
    A("")
    A("**Generated:** `tools/gen_band3_port_worklist.py` (regenerable). "
      "**Source:** `ghidriff_identities.json` (ACCEPT tier) minus `scripts/target_symbol_map.json`.")
    A("**Data feed:** `band3_port_worklist.json` (machine-readable, one row per fn; gitignored/regenerable).")
    A("")
    A("## What this is")
    A("")
    A(f"{len(rows)} RB3 **game-code** functions across **{len(by_tu)} TUs**, each pinned to a specific "
      "Wii (Bank-8, CodeWarrior-mangled) function by the forked-ghidriff/BSim Wii→Xenon identity "
      "pipeline. These are **net-new**: their Xenon address is NOT yet in the production pairing set "
      "(`target_symbol_map.json`), and they live in band3 TUs the active class-A port has **not yet "
      "reached**. band3 is RB3-specific gameplay/scoring/song/tracker code — **DC3 (Dance Central 3, "
      "no Rock Band gameplay) fundamentally cannot identify these.** This is the irreplaceable core of "
      "the Wii→Xenon lever.")
    A("")
    A("**This is a targeting/porting worklist + per-fn identity oracle, NOT a "
      "`target_symbol_map.json` injection.** The TUs aren't compiled yet, so there is no MSVC symbol "
      "to pair against; and our `wii_symbol` is CW/MWCC-mangled, not MSVC-mangled — injecting it as a "
      "map key would mis-pair objdiff at our ~0.90 precision (actively harmful). Use this to pick "
      "which TU to port next and, when porting, to name each function from the Wii body. Both outputs "
      "are additive + reversible.")
    A("")
    A("## How to consume")
    A("")
    A("- **Pick the next TU** from the ranking below (highest yield × certainty first). The "
      "`wf_classa_harvest.js` Scan stage / coordinator picks the next band3 TU from this ranking "
      "instead of a blind string-anchor guess.")
    A("- **Cross-check `OWN` attribution.** A function this worklist pins to a TU at `high`/`bsim≥30` "
      "confidence is strong independent evidence for the Validate stage's `OWN` verdict (better than "
      "the near-random `unified_id_rb3wii.json` oracle).")
    A("- **Name from the Wii body.** In the rb3 repo: `bin/analyze-function <wii_symbol>` shows the "
      "Bank-8-accurate body + real arg shape; `wii_addr_bank8` is the Bank-8 address.")
    A("- **fn_resolver T4b** (`ghidriff_wii_b8`) already serves all 978 identities for per-address "
      "resolution; this worklist adds the missing **TU-priority + per-TU member roster**.")
    A("")
    A("## Confidence strata (the measured prior)")
    A("")
    A(f"Band3 human-judged precision (round 2, n=30) = **0.900**. Totals here: "
      f"**{high_total} high** · **{s30} bsim≥30** · **{s2030} bsim20-30** · **{s1520} bsim15-20**.")
    A("")
    A("- **high** — `ExactInstructions`/`SwitchSig`/`Implied`/`SymbolsHash`, or BSim simconf ≥ 30. "
      "The safest-first targets.")
    A("- **bsim≥30 / bsim20-30 / bsim15-20** — BSim similarity×confidence bands; lower = vet harder.")
    A("")
    A("**Dominant failure mode (~10%): same-TU sibling aliasing.** Near-identical template/sibling "
      "bodies differing only in a type-tag immediate (e.g. `kDataFloat` vs `kDataInt`) or an STL "
      "node-size literal, or a hash-shape match a string later refutes. When confirming a name, diff "
      "the small immediates / node-size literals and referenced strings against the Wii body.")
    A("")

    # ---- HIGH-confidence subset (safest first) ------------------------------
    A("## HIGH-confidence subset (safest first — verify these names with most trust)")
    A("")
    A("| Xenon addr | TU | src | confidence | match | Wii signature | wii_symbol |")
    A("|---|---|---|---|---|---|---|")
    high_rows = sorted(
        [r for r in rows if r["confidence_label"] in ("high", "bsim>=30")],
        key=lambda r: (LABEL_ORDER[r["confidence_label"]], r["tu"], r["rb3_addr"]),
    )
    for r in high_rows:
        conf = r["confidence_label"]
        if conf == "bsim>=30" and r["simconf"] is not None:
            conf = f"bsim {r['simconf']:.0f}"
        A(f"| `{r['rb3_addr']}` | {r['tu']} | `{r['src_path']}` | {conf} | "
          f"{r['match_type']} | {md_esc(r['wii_demangled'])} | `{r['wii_symbol']}` |")
    A("")

    # ---- TU ranking ---------------------------------------------------------
    A("## TU ranking (port these first — by #high+#bsim≥30 desc, then total desc)")
    A("")
    A("| Rank | TU | src | #ids | high | ≥30 | 20-30 | 15-20 | DC3? |")
    A("|---|---|---|---|---|---|---|---|---|")
    for i, tu in enumerate(ranked_tus, 1):
        s = tu_summary[tu]
        A(f"| {i} | {tu} | `{s['src_path']}` | {s['n']} | {s['high']} | "
          f"{s['bsim>=30']} | {s['bsim20-30']} | {s['bsim15-20']} | cannot-provide |")
    A("")

    # ---- per-TU rosters -----------------------------------------------------
    A("## Per-TU function rosters")
    A("")
    A("Each TU's identities, confidence-ranked. `wii_symbol` is the CW/MWCC ground-truth name "
      "(`bin/analyze-function <wii_symbol>` in the rb3 repo for the real body).")
    A("")
    for tu in ranked_tus:
        s = tu_summary[tu]
        A(f"### {tu} — {s['n']} ids "
          f"(high {s['high']}, ≥30 {s['bsim>=30']}, 20-30 {s['bsim20-30']}, 15-20 {s['bsim15-20']})  ·  `{s['src_path']}`")
        A("")
        A("| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |")
        A("|---|---|---|---|---|---|")
        tu_rows = sorted(
            by_tu[tu],
            key=lambda r: (LABEL_ORDER[r["confidence_label"]], r["rb3_addr"]),
        )
        for r in tu_rows:
            conf = r["confidence_label"]
            if conf.startswith("bsim") and r["simconf"] is not None:
                conf = f"bsim {r['simconf']:.0f}"
            A(f"| `{r['rb3_addr']}` | `{r['wii_addr_bank8']}` | {conf} | {r['match_type']} | "
              f"{md_esc(r['wii_demangled'])} | `{r['wii_symbol']}` |")
        A("")

    A("---")
    A("")
    A("## For the next agent")
    A("")
    A("- **Regenerate:** `python3 tools/gen_band3_port_worklist.py` (cwd-independent; reads "
      "`ghidriff_identities.json` + `scripts/target_symbol_map.json` + the rb3 CW map; the script "
      "VERIFIES every `wii_symbol` resolves to its claimed Bank-8 addr and that 0 entries are "
      "already in the production map, exiting non-zero on any failure).")
    A("- **Data feed:** `band3_port_worklist.json` — per-fn rows + `tu_summary` + `ranked_tus` for "
      "machine ingestion by the `wf_classa_harvest.js` Scan/Validate stages.")
    A("- **Do NOT inject these into `target_symbol_map.json`** — CW≠MSVC mangling, TUs uncompiled; "
      "wrong key mis-pairs objdiff at ~0.90 precision. Confirm each name when the TU is actually ported.")
    A("- **Validation gap still open:** the ~530 net-new system/network identities are unjudged at "
      "human grade (round-2 judging was band3-only).")
    A("")
    with open(path, "w") as f:
        f.write("\n".join(L))


def md_esc(s):
    return s.replace("|", "\\|")


if __name__ == "__main__":
    main()
