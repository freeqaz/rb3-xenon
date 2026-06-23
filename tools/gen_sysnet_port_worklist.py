#!/usr/bin/env python3
"""Generate the system/network porting worklist from the Wii->Xenon ghidriff identities.

Sibling of `gen_band3_port_worklist.py`, generalized to `category in {system, network}`.
The ~530 net-new system/network Wii->Xenon identities were human-validated at 0.967
precision (system 14/15, network 15/15; HIGH+BSim>=30 core 11/11), clearing the >=0.85
handoff bar (see rb3 docs/decomp/xenon-hardening/round3/SYNTHESIS.md). This emits them
as a porting worklist + per-fn identity oracle, mirroring band3.

CONSUMES (no ghidriff run): the ACCEPT identities ingested into rb3-xenon
(`ghidriff_identities.json`, gitignored/regenerable) and the PRODUCTION pairing set
(`scripts/target_symbol_map.json`).

NET-NEW = ghidriff identity whose normalized Xenon `rb3_addr` is NOT already a key in
`target_symbol_map.json` (the file objdiff actually pairs against), with
`category in {system, network}`. Re-derived against the LIVE map on each run (the map
drifts as porters land identities) so counts track reality, not a stale snapshot.

This is a TARGETING/PORTING worklist + per-fn identity oracle, NOT a target_symbol_map
injection. CW/MWCC mangling != MSVC mangling and many TUs aren't compiled yet, so
injecting the CW name as a key would mis-pair objdiff (actively harmful). Both outputs
are additive + reversible.

UNLIKE band3 (which DC3 fundamentally cannot supply), most of system/network is shared
Milo engine + Quazal netcode where DC3 BinDiff also helps, so `dc3_cannot_provide` is
False by default. We mark a row `dc3_cannot_provide=True` only when it is GENUINELY
DC3-unreachable: its rb3 source file does not exist (undecompiled TU) AND no same-named
source file exists anywhere in the DC3 tree (DC3 carries no equivalent). This cleanly
isolates the Quazal/ObjDup netcode that is RB3/Wii-proprietary and has no DC3 twin.

Outputs (relative to rb3-xenon repo root):
  sysnet_port_worklist.json          gitignored data feed (one row per identity)
  docs/plans/sysnet-port-worklist.md tracked, TU-ranked human checklist

Run from anywhere:  python3 tools/gen_sysnet_port_worklist.py
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
DC3 = "/home/free/code/milohax/dc3-decomp"
IDENT_PATH = os.path.join(RB3X, "ghidriff_identities.json")
TSM_PATH = os.path.join(RB3X, "scripts", "target_symbol_map.json")
CW_MAP = os.path.join(RB3, "orig", "SZBE69_B8", "files", "band_r_wii.map")
OUT_JSON = os.path.join(RB3X, "sysnet_port_worklist.json")
OUT_MD = os.path.join(RB3X, "docs", "plans", "sysnet-port-worklist.md")

CATEGORIES = ("system", "network")

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
    """symbol -> (bank8_addr_set, raw_o_path) for the requested symbols."""
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


# CW .o path:  ...\band3_wii\<top>\src\<sub>\wii_release\<File>.o
#   top in {system, network}; <sub> can contain nested dirs (rare). The rb3 tree
#   mirrors this exactly: src/system/<sub>/<File>.cpp and src/network/<sub>/...
#   (network preserves CW capitalization: Core, Plugins, ObjDup, ...; system is
#   lowercase: beatmatch, bandobj, ...).
_SRC_RE = re.compile(
    r"band3_wii[\\/](system|network)[\\/]src[\\/](.+?)[\\/]wii_release[\\/]([^\\/]+\.o)"
)


def src_relpath(category, tu, raw_o_path):
    """Map a CW .o path -> rb3 src/<top>/<sub>/<TU>.cpp."""
    m = _SRC_RE.search(raw_o_path or "")
    if not m:
        return f"src/{category}/?/{tu[:-2]}.cpp"
    top = m.group(1)
    sub = m.group(2).replace("\\", "/")
    cpp = m.group(3)[:-2] + ".cpp"
    return f"src/{top}/{sub}/{cpp}"


# DC3 source basenames, indexed once (for genuinely-DC3-unreachable detection).
_DC3_BASENAMES = None


def _dc3_basenames():
    global _DC3_BASENAMES
    if _DC3_BASENAMES is None:
        names = set()
        for top in CATEGORIES:
            root = os.path.join(DC3, "src", top)
            if not os.path.isdir(root):
                continue
            for _, _, files in os.walk(root):
                for fn in files:
                    if fn.endswith(".cpp"):
                        names.add(fn)
        _DC3_BASENAMES = names
    return _DC3_BASENAMES


def dc3_cannot_provide(src_path):
    """True iff this identity is GENUINELY DC3-unreachable.

    Genuinely-unreachable = the rb3 source file does not exist (the TU is
    undecompiled in the Wii dev tree) AND no same-named .cpp exists anywhere
    under the DC3 src/system or src/network tree (DC3 carries no equivalent).
    This isolates the Quazal/ObjDup netcode that is proprietary to RB3's Wii
    build with no DC3 twin. Defaults False: most of system/network is shared
    Milo engine that DC3 (and its BinDiff oracle) does supply.
    """
    rb3_full = os.path.join(RB3, src_path)
    if os.path.exists(rb3_full):
        return False  # rb3 already has the source -> reachable
    base = os.path.basename(src_path)
    return base not in _dc3_basenames()


def main():
    ident = json.load(open(IDENT_PATH))
    tsm_n = {norm(k) for k in json.load(open(TSM_PATH))}

    sysnet = [
        e
        for e in ident
        if e.get("category") in CATEGORIES and norm(e["rb3_addr"]) not in tsm_n
    ]

    syms = {e["wii_symbol"] for e in sysnet}
    sym2addr, sym2src = parse_cw_map(syms)

    # ---- self-verifying VERIFY pass -----------------------------------------
    errs = []
    for e in sysnet:
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
    for e in sysnet:
        tu = e["tu"]
        cat = e["category"]
        sp = src_relpath(cat, tu, sym2src.get(e["wii_symbol"], ""))
        row = {
            "rb3_addr": e["rb3_addr"],
            "wii_addr_bank8": e["wii_addr_bank8"],
            "wii_symbol": e["wii_symbol"],
            "wii_demangled": demangle(e["wii_symbol"]),
            "tu": tu,
            "category": cat,
            "src_path": sp,
            "src_exists": os.path.exists(os.path.join(RB3, sp)),
            "match_type": (e.get("match_types") or ["?"])[0],
            "match_types": e.get("match_types", []),
            "confidence_label": confidence_label(e),
            "simconf": e.get("bsim_simconf"),
            "dc3_cannot_provide": dc3_cannot_provide(sp),
        }
        rows.append(row)

    rows.sort(key=lambda r: (r["category"], r["tu"], r["rb3_addr"]))

    # ---- per-TU aggregation + ranking ---------------------------------------
    by_tu = defaultdict(list)
    for r in rows:
        by_tu[r["tu"]].append(r)

    def tu_stats(tu_rows):
        c = Counter(r["confidence_label"] for r in tu_rows)
        return {
            "n": len(tu_rows),
            "category": tu_rows[0]["category"],
            "high": c.get("high", 0),
            "bsim>=30": c.get("bsim>=30", 0),
            "bsim20-30": c.get("bsim20-30", 0),
            "bsim15-20": c.get("bsim15-20", 0),
            "src_path": tu_rows[0]["src_path"],
            "src_exists": tu_rows[0]["src_exists"],
            "dc3_cannot_provide": any(r["dc3_cannot_provide"] for r in tu_rows),
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
    cat_totals = Counter(r["category"] for r in rows)
    safe_core = sum(1 for r in rows if r["confidence_label"] in ("high", "bsim>=30"))
    dc3_unreachable = sum(1 for r in rows if r["dc3_cannot_provide"])
    feed = {
        "_meta": {
            "description": "Net-new system/network Wii->Xenon identities. Porting worklist + "
            "per-fn identity oracle. NOT a target_symbol_map injection.",
            "generated_by": "tools/gen_sysnet_port_worklist.py",
            "source_identities": "ghidriff_identities.json (ACCEPT tier, ghidriff-run3)",
            "net_new_filter": "rb3_addr not a key in scripts/target_symbol_map.json (normalized join); "
            "category in {system, network}",
            "precision_sysnet_human_judged": 0.967,
            "precision_by_category": {"system": 0.933, "network": 1.000},
            "safe_first_slice": "HIGH + BSim>=30 (human-judged 11/11 = 1.000 on the in-sample core)",
            "confirm_on_consume_tier": "bsim15-20 (holds the lone miss: TrackWidget Init-vs-Empty "
            "sibling aliasing, vtable-slot 0x44 vs 0xc); verify per-fn when consumed",
            "dominant_failure_mode": "same-TU sibling aliasing: near-identical bodies differing only "
            "in a vtable-slot / type-tag / node-size immediate; concentrated in bsim15-20",
            "count": len(rows),
            "count_by_category": dict(cat_totals),
            "tu_count": len(by_tu),
            "confidence_totals": dict(totals),
            "safe_first_core_count": safe_core,
            "dc3_genuinely_unreachable_count": dc3_unreachable,
            "dc3_cannot_provide_definition": "True iff rb3 source file absent AND no same-named .cpp "
            "in DC3 src/system|src/network (isolates Quazal/ObjDup netcode with no DC3 twin). "
            "Defaults False: most system/network is shared Milo engine DC3 supplies.",
            "confidence_legend": {
                "high": "ExactInstructions/SwitchSig/Implied/SymbolsHash, or BSim simconf>=30",
                "bsim>=30": "BSim simconf >= 30",
                "bsim20-30": "BSim simconf 20-30",
                "bsim15-20": "BSim simconf 15-20 (confirm-on-consume)",
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
    write_markdown(OUT_MD, rows, by_tu, tu_summary, ranked_tus, totals, cat_totals)
    print(f"wrote {OUT_MD}")


def write_markdown(path, rows, by_tu, tu_summary, ranked_tus, totals, cat_totals):
    high_total = totals.get("high", 0)
    s30 = totals.get("bsim>=30", 0)
    s2030 = totals.get("bsim20-30", 0)
    s1520 = totals.get("bsim15-20", 0)
    safe_core = high_total + s30
    n_sys = cat_totals.get("system", 0)
    n_net = cat_totals.get("network", 0)
    dc3_unreachable = sum(1 for r in rows if r["dc3_cannot_provide"])
    L = []
    A = L.append
    A("# system/network porting worklist — net-new Wii→Xenon identities")
    A("")
    A("**Generated:** `tools/gen_sysnet_port_worklist.py` (regenerable). "
      "**Source:** `ghidriff_identities.json` (ACCEPT tier) minus `scripts/target_symbol_map.json`, "
      "`category ∈ {system, network}`.")
    A("**Data feed:** `sysnet_port_worklist.json` (machine-readable, one row per fn; gitignored/regenerable).")
    A("")
    A("## What this is")
    A("")
    A(f"{len(rows)} RB3 **engine + netcode** functions across **{len(by_tu)} TUs** "
      f"(**{n_sys} system** + **{n_net} network**), each pinned to a specific Wii (Bank-8, "
      "CodeWarrior-mangled) function by the forked-ghidriff/BSim Wii→Xenon identity pipeline. These "
      "are **net-new**: their Xenon address is NOT yet in the production pairing set "
      "(`target_symbol_map.json`), re-derived against the **live** map on each regen.")
    A("")
    A("These ~530 identities were **human-validated at 0.967 precision** (system 14/15 = 0.933, "
      "network 15/15 = 1.000; **HIGH + BSim≥30 core = 11/11 = 1.000**), clearing the ≥0.85 handoff bar "
      "— so, like band3, they get a worklist. This is the **second-priority** lever behind band3: much "
      "of system/network is shared Milo engine + Quazal netcode where **DC3 BinDiff also helps**, so "
      "the marginal value is lower even though precision is higher.")
    A("")
    A("**This is a targeting/porting worklist + per-fn identity oracle, NOT a "
      "`target_symbol_map.json` injection.** Many TUs aren't compiled yet (no MSVC symbol to pair), and "
      "our `wii_symbol` is CW/MWCC-mangled, not MSVC-mangled — injecting it as a map key would mis-pair "
      "objdiff (actively harmful). Use this to pick which engine/netcode TU to port next and to name "
      "each function from the Wii body. Both outputs are additive + reversible.")
    A("")
    A("## Safe-first slice + the confirm-on-consume tier")
    A("")
    A(f"- **Safe-first core = HIGH + BSim≥30: {safe_core} rows** ({high_total} high + {s30} bsim≥30). "
      "This is the human-judged-1.000 slice — port/name these with the most trust. Table below.")
    A("- **BSim 15–20 = confirm-on-consume.** That band holds the **only** measured miss across the "
      "30-pair sample: `TrackWidget::Init` was aliased to its sibling `TrackWidget::Empty` (the two "
      "20-byte `mImp->virtual()` forwarders differ ONLY in the vtable-slot immediate — Init forwards "
      "through slot `0x44`, Empty through `0xc`). **Verify each BSim 15–20 name per-fn when a porter "
      "actually consumes it** (diff vtable-slot / type-tag / node-size immediates + referenced strings "
      "+ resolved callees against the Wii body).")
    A("")
    A("## DC3 reachability")
    A("")
    A(f"Most system/network is shared Milo engine that **DC3 can supply** (DC3 is the same engine on "
      f"the same Xbox 360 toolchain — `dc3_cannot_provide` defaults **False**). "
      f"**{dc3_unreachable} rows are flagged genuinely DC3-unreachable** (`dc3_cannot_provide=true`): "
      "their rb3 source file is absent AND no same-named `.cpp` exists in the DC3 tree — overwhelmingly "
      "the Quazal/ObjDup netcode proprietary to RB3's Wii build with no DC3 twin. Those rows mark "
      "`DC3?=cannot-provide` in the rosters; all others mark `DC3?=shared`.")
    A("")
    A("## Confidence strata (the measured prior)")
    A("")
    A(f"system/network human-judged precision (n=30) = **0.967**. Totals here: "
      f"**{high_total} high** · **{s30} bsim≥30** · **{s2030} bsim20-30** · **{s1520} bsim15-20**.")
    A("")
    A("- **high** — `ExactInstructions`/`SwitchSig`/`Implied`/`SymbolsHash`, or BSim simconf ≥ 30. "
      "The safest-first targets.")
    A("- **bsim≥30 / bsim20-30 / bsim15-20** — BSim similarity×confidence bands; lower = vet harder. "
      "**bsim15-20 = confirm-on-consume.**")
    A("")

    # ---- HIGH + bsim>=30 subset (safest first) ------------------------------
    A("## Safe-first subset — HIGH + BSim≥30 (verify these names with most trust)")
    A("")
    A("| Xenon addr | cat | TU | src | confidence | match | Wii signature | wii_symbol | DC3? |")
    A("|---|---|---|---|---|---|---|---|---|")
    high_rows = sorted(
        [r for r in rows if r["confidence_label"] in ("high", "bsim>=30")],
        key=lambda r: (LABEL_ORDER[r["confidence_label"]], r["category"], r["tu"], r["rb3_addr"]),
    )
    for r in high_rows:
        conf = r["confidence_label"]
        if conf == "bsim>=30" and r["simconf"] is not None:
            conf = f"bsim {r['simconf']:.0f}"
        dc3 = "cannot-provide" if r["dc3_cannot_provide"] else "shared"
        A(f"| `{r['rb3_addr']}` | {r['category']} | {r['tu']} | `{r['src_path']}` | {conf} | "
          f"{r['match_type']} | {md_esc(r['wii_demangled'])} | `{r['wii_symbol']}` | {dc3} |")
    A("")

    # ---- TU ranking ---------------------------------------------------------
    A("## TU ranking (port these first — by #high+#bsim≥30 desc, then total desc)")
    A("")
    A("| Rank | cat | TU | src | #ids | high | ≥30 | 20-30 | 15-20 | DC3? |")
    A("|---|---|---|---|---|---|---|---|---|---|")
    for i, tu in enumerate(ranked_tus, 1):
        s = tu_summary[tu]
        dc3 = "cannot-provide" if s["dc3_cannot_provide"] else "shared"
        A(f"| {i} | {s['category']} | {tu} | `{s['src_path']}` | {s['n']} | {s['high']} | "
          f"{s['bsim>=30']} | {s['bsim20-30']} | {s['bsim15-20']} | {dc3} |")
    A("")

    # ---- per-TU rosters -----------------------------------------------------
    A("## Per-TU function rosters")
    A("")
    A("Each TU's identities, confidence-ranked. `wii_symbol` is the CW/MWCC ground-truth name "
      "(`bin/analyze-function <wii_symbol>` in the rb3 repo for the real body). Rows in the "
      "**bsim15-20** tier are confirm-on-consume.")
    A("")
    for tu in ranked_tus:
        s = tu_summary[tu]
        dc3 = "DC3 cannot-provide" if s["dc3_cannot_provide"] else "DC3 shared"
        exists = "" if s["src_exists"] else "  *(rb3 src absent)*"
        A(f"### {tu} — {s['category']}, {s['n']} ids "
          f"(high {s['high']}, ≥30 {s['bsim>=30']}, 20-30 {s['bsim20-30']}, 15-20 {s['bsim15-20']})  ·  "
          f"`{s['src_path']}`  ·  {dc3}{exists}")
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
    A("- **Regenerate:** `python3 tools/gen_sysnet_port_worklist.py` (cwd-independent; reads "
      "`ghidriff_identities.json` + `scripts/target_symbol_map.json` + the rb3 CW map; the script "
      "VERIFIES every `wii_symbol` resolves to its claimed Bank-8 addr and that 0 entries are "
      "already in the production map, exiting non-zero on any failure).")
    A("- **Data feed:** `sysnet_port_worklist.json` — per-fn rows + `tu_summary` + `ranked_tus` for "
      "machine ingestion.")
    A("- **Port the safe-first core first** (HIGH + BSim≥30, human-judged 1.000), then the BSim "
      "20–30 tier, then **confirm-on-consume each BSim 15–20 row** before trusting its name.")
    A("- **Do NOT inject these into `target_symbol_map.json`** — CW≠MSVC mangling, TUs uncompiled; "
      "wrong key mis-pairs objdiff. Confirm each name when the TU is actually ported "
      "(`gen_game_target_map.py --tu <TU>`).")
    A("- **DC3 first for shared rows.** For `DC3?=shared` engine TUs, DC3's already-decomp'd body "
      "(`/dc3-pair`, BinDiff) is the faster base; this worklist's value is highest on the "
      "`DC3?=cannot-provide` Quazal/ObjDup netcode rows.")
    A("- **Watch the dominant failure mode:** same-TU sibling aliasing (the lone miss was "
      "`TrackWidget::Init` vs `::Empty`, vtable slot 0x44 vs 0xc). It bites hardest in **bsim15-20**.")
    A("")
    with open(path, "w") as f:
        f.write("\n".join(L))


def md_esc(s):
    return s.replace("|", "\\|")


if __name__ == "__main__":
    main()
