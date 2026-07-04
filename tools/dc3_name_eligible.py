#!/usr/bin/env python3
"""dc3_name_eligible.py — the DC3-oracle FUZZY-drain tool.

Emit ADD-ONLY {rb3_va -> dc3_name} entries for ANONYMOUS engine targets that the
DC3 BinDiff oracle names, gated by the HONESTY rule so naming actually moves the
fuzzy metric (0% -> the body's real 82-94%) instead of producing foreign/ICF noise.

A dc3_oracle.json row is ELIGIBLE iff ALL hold:
  1. similarity >= --min-sim (default 0.7)
  2. dc3_tu is "<subdir>:<File>.obj" with <subdir> a DIRECT rb3-xenon system subdir
     (DC-specific / xdk / thirdparty subdirs excluded — see EXCLUDED note below)
  3. rb3-xenon has a PINNED unit src/system/<subdir>/<File>.cpp (in splits.txt .text)
  4. rb3_va falls inside THAT unit's pinned .text range (so our obj compiles the body)
  5. rb3_va is currently ANONYMOUS: a fn_<VA> in that unit's report.json AND not
     already keyed in scripts/target_symbol_map.json

Mapping: dc3_tu "<subdir>:<File>.obj" -> report.json unit whose metadata.source_path
== "src/system/<subdir>/<File>.cpp". rb3-xenon uses DC3's PLAIN file names (PropAnim,
EventTrigger, Part, Mesh ...), so the asserted aliases (Cam->RndCam, Dir->ObjectDir,
PropAnim->RndPropAnim) do NOT apply here -- a basename that does not match a pinned
unit simply is not eligible (safe by construction). DC's Dir lives in world/, not
rndobj/, so a subdir mismatch also drops the row.

EXCLUDED dc3 subdirs (not wired rb3-xenon engine units): xgraphics, d3dx9, d3d9i,
xaudio2, xapilibi, xhv2, xinput2, xmcore, xmic, xmp, xnet, xonline, xparty, nuiapi,
nuiaudio, nuispeech, net_ham, meta_ham, game, LIBCMT, XBOXKRNL, ST, zlib, jpeg,
binkxenon (DC-game-specific, toolchain, or unwired). bandobj/beatmatch rb3 subdirs
have no DC3 oracle subdir.

Usage:
  tools/dc3_name_eligible.py [subdir|all] [-o out.json] [--min-sim 0.7] [--summary]
Output JSON: {"0x82XXXXXX": "?mangled@...@@Z", ...}  (ADD-ONLY to target_symbol_map)
"""
import argparse, json, re, sys
from collections import defaultdict, Counter

ROOT = __file__.rsplit("/tools/", 1)[0]
REPORT = f"{ROOT}/build/45410914/report.json"
SPLITS = f"{ROOT}/config/45410914/splits.txt"
TMAP   = f"{ROOT}/scripts/target_symbol_map.json"
ORACLE = f"{ROOT}/dc3_oracle.json"

# rb3-xenon system subdirs that directly match a DC3 oracle subdir (engine, wired).
DIRECT = {"char","flow","gesture","hamobj","math","meta","midi","movie","moviebink",
          "net","obj","oggvorbis","os","rnddx9","rndobj","synth","synth_xbox","ui",
          "utl","world"}

def parse_splits():
    """TU basename ('Foo.cpp') -> list[(start,end)] of pinned .text ranges."""
    tu, out = None, defaultdict(list)
    for ln in open(SPLITS):
        m = re.match(r"^([A-Za-z0-9_]+\.cpp):\s*$", ln)
        if m: tu = m.group(1); continue
        m = re.match(r"^\s+\.text\s+start:0x([0-9A-Fa-f]+)\s+end:0x([0-9A-Fa-f]+)", ln)
        if m and tu: out[tu].append((int(m.group(1),16), int(m.group(2),16)))
    return out

def load_units():
    """(subdir,base) -> {'ranges':[(s,e)], 'anon':set(va)} for PINNED system units."""
    tu_text = parse_splits()
    units = {}
    for u in json.load(open(REPORT))["units"]:
        sp = (u.get("metadata") or {}).get("source_path") or ""
        m = re.match(r"src/system/([^/]+)/(.+)\.cpp$", sp)
        if not m: continue
        sub, base = m.group(1), m.group(2)
        if base + ".cpp" not in tu_text: continue
        anon = set()
        for f in (u.get("functions") or []):
            fm = re.fullmatch(r"fn_([0-9A-Fa-f]{8})", f["name"])
            if fm: anon.add(int(fm.group(1), 16))
        units[(sub, base)] = {"ranges": tu_text[base + ".cpp"], "anon": anon}
    return units

def load_named():
    named = set()
    for k in json.load(open(TMAP)):
        if re.fullmatch(r"0[xX][0-9A-Fa-f]+", k): named.add(int(k, 16))
    return named

def eligible(subdir_filter, min_sim):
    units, named = load_units(), load_named()
    rows, seen = [], set()
    for r in json.load(open(ORACLE)):
        if r["similarity"] < min_sim: continue
        tu = r.get("dc3_tu")
        if not tu or ":" not in tu: continue
        sub, obj = tu.split(":", 1)
        if sub not in DIRECT: continue
        if subdir_filter not in ("all", sub): continue
        key = (sub, obj.rsplit(".obj", 1)[0])
        u = units.get(key)
        if not u: continue
        va = int(r["rb3_va"], 16)
        if not any(s <= va < e for s, e in u["ranges"]): continue
        if va not in u["anon"] or va in named or va in seen: continue
        seen.add(va)
        rows.append((sub, key[1], va, r["dc3_name"], r["similarity"]))
    return rows

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("subdir", nargs="?", default="all")
    ap.add_argument("-o", "--out")
    ap.add_argument("--min-sim", type=float, default=0.7)
    ap.add_argument("--summary", action="store_true")
    a = ap.parse_args()
    rows = eligible(a.subdir, a.min_sim)
    if a.summary or not a.out:
        by = Counter(); us = defaultdict(set)
        for sub, base, va, name, sim in rows:
            by[sub] += 1; us[sub].add(base)
        print(f"TOTAL ELIGIBLE: {len(rows)}  (subdir={a.subdir}, min_sim={a.min_sim})", file=sys.stderr)
        for sub, n in by.most_common():
            print(f"  {sub:12s} rows={n:4d} units={len(us[sub])}", file=sys.stderr)
    if a.out:
        out = {f"0x{va:08X}": name for _, _, va, name, _ in rows}
        json.dump(out, open(a.out, "w"), indent=1)
        print(f"wrote {len(out)} entries -> {a.out}", file=sys.stderr)

if __name__ == "__main__":
    main()
