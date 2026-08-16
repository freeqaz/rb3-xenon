#!/usr/bin/env python3
"""MAPID-1: identify the remaining unknown addresses + check downstream reloc rows."""
import json, struct, sys, collections
from pathlib import Path

wt = Path(sys.argv[1]).resolve()
sys.path.insert(0, str(wt / "tools"))
from alias_forgiveness_audit import Sides                                   # noqa

S = Sides(wt)
assert sum(1 for n in S.traw if n.startswith("?")) > 1000, "PRE-RENAMER"
m = json.loads((wt / "scripts/target_symbol_map.json").read_text())
inv = {}
for a, n in m.items():
    for x in (n if isinstance(n, list) else [n]):
        if x:
            inv.setdefault(x, []).append(a)


def relocs(store, n):
    b = store.get(n)
    return None if b is None else [(o, nm) for o, nm, _t in b[1]]


def dump(label, store, n, lim=200):
    b = store.get(n)
    if b is None:
        print("   %-7s %-64s ABSENT" % (label, n[:64])); return
    print("   %-7s %-64s %4d B  addr=%s" % (label, n[:64], len(b[0]), inv.get(n, "-")))
    for o, nm, _t in b[1]:
        if nm != "@comp.id":
            print("        reloc @0x%-4x -> %s" % (o, nm[:78]))


print("=" * 100)
print("FAMILY A: does the alias map already equate the two PoolAlloc spellings?")
print("=" * 100)
for a, b in [("?PoolAlloc@@YAPAXHHPBDH0@Z", "?PoolAlloc@@YAPAXHH@Z"),
             ("fn_827BCD38", "?MemAlloc@@YAPAXHH@Z")]:
    print("  equiv(%s, %s) = %s" % (a[:40], b[:40], S.equiv(a, b)))
    print("     map addr %-30s %s" % (a[:30], inv.get(a, "ABSENT")))
    print("     map addr %-30s %s" % (b[:30], inv.get(b, "ABSENT")))
print("\n  fn_827BCD38 callees (identifying evidence):")
dump("RETAIL", S.traw, "fn_827BCD38")
print("\n  every RETAIL site that calls fn_827BCD38:")
for n, (raw, rel) in sorted(S.traw.items()):
    for o, nm, _t in rel:
        if nm == "fn_827BCD38":
            print("     %-64s @0x%-4x (%d B)" % (n[:64], o, len(raw)))

print("\n" + "=" * 100)
print("FAMILY B: identify the six retail destructors")
print("=" * 100)
for a in ["fn_82BC4B28", "fn_82BD0EB8", "fn_82BD2E78", "fn_82BD4C48", "fn_82BD4EF0", "fn_82BF72E0"]:
    dump("RETAIL", S.traw, a)
print("\n  our two ObjRefConcrete dtors -- identical to each other?")
a, b = S.oraw.get("??1?$ObjRefConcrete@VRndCubeTex@@VObjectDir@@@@UAA@XZ"), \
       S.oraw.get("??1?$ObjRefConcrete@VRndFur@@VObjectDir@@@@UAA@XZ")
print("     raw bytes: %s" % ("IDENTICAL" if a and b and a[0] == b[0] else "DIFFER"))
print("     relocs RndCubeTex: %s" % [(hex(o), n[:44]) for o, n, _ in a[1] if n != "@comp.id"])
print("     relocs RndFur    : %s" % [(hex(o), n[:44]) for o, n, _ in b[1] if n != "@comp.id"])
print("\n  is our ObjRefConcrete thunk raw-identical to retail's six survivors?")
ours = S.oraw.get("??_G?$ObjRefConcrete@VRndCubeTex@@VObjectDir@@@@UAAPAXI@Z")
ret = S.traw.get("??_ECX2SubmixVoice@XAUDIO2@@UAAPAXI@Z")
print("     %s (ours %d B vs retail %d B)" % ("RAW-IDENTICAL" if ours[0] == ret[0] else "DIFFER",
                                              len(ours[0]), len(ret[0])))

print("\n" + "=" * 100)
print("FAMILY C: identify fn_82327050 / fn_824C8F68 and check the pigeonhole")
print("=" * 100)
for a in ["fn_82327050", "fn_824C8F68"]:
    dump("RETAIL", S.traw, a)
print()
for n in ["??$?6VSymbol@@V?$StlNodeAlloc@VSymbol@@@stlpmtx_std@@@@YAAAVBinStream@@AAV0@ABV?$list@VSymbol@@V?$StlNodeAlloc@VSymbol@@@stlpmtx_std@@@stlpmtx_std@@@Z",
          "??$?6UTarget@HamCamShot@@V?$StlNodeAlloc@UTarget@HamCamShot@@@stlpmtx_std@@@@YAAAVBinStream@@AAV0@ABV?$list@UTarget@HamCamShot@@V?$StlNodeAlloc@UTarget@HamCamShot@@@stlpmtx_std@@@stlpmtx_std@@@Z"]:
    dump("RETAIL", S.traw, n)
dump("OURS", S.oraw, "??6@YAAAVBinStream@@AAV0@ABVPracticeSectionMapping@SongSectionController@@@Z")
print("\n  groups folding the PracticeSectionMapping list serializer:")
gs = json.loads((wt / "scripts/symbol_aliases.json").read_text())["groups"]
t = "??$?6VPracticeSectionMapping@SongSectionController@@V?$StlNodeAlloc@VPracticeSectionMapping@SongSectionController@@@stlpmtx_std@@@@YAAAVBinStream@@AAV0@ABV?$list@VPracticeSectionMapping@SongSectionController@@V?$StlNodeAlloc@VPracticeSectionMapping@SongSectionController@@@stlpmtx_std@@@stlpmtx_std@@@Z"
for i, g in enumerate(gs):
    if t in g.get("folded", []):
        print("     group %-5d survivor=%-70s addr=%s" % (i, g["survivor"][:70], inv.get(g["survivor"], "-")))
