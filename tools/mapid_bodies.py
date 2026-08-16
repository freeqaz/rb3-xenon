#!/usr/bin/env python3
"""MAPID-1: dump retail + our bodies with relocations for the addresses under test."""
import json, struct, sys
from pathlib import Path

wt = Path(sys.argv[1]).resolve()
sys.path.insert(0, str(wt / "tools"))
from alias_forgiveness_audit import Sides                                   # noqa

S = Sides(wt)
mangled = sum(1 for n in S.traw if n.startswith("?"))
print("target bodies %d (mangled %d) | our bodies %d" % (len(S.traw), mangled, len(S.oraw)))
assert mangled > 1000, "PRE-RENAMER"


def show(label, store, name, limit=40):
    b = store.get(name)
    if b is None:
        print("  %-8s %-70s ABSENT" % (label, name[:70]))
        return None
    raw, rel = b
    rm = {o: (n, t) for o, n, t in rel}
    print("  %-8s %-60s %4d B, %d relocs" % (label, name[:60], len(raw), len(rel)))
    for i in range(0, min(len(raw), limit * 4), 4):
        w = struct.unpack_from(">I", raw, i)[0]
        tag = ""
        if i in rm:
            tag = "   <== RELOC %s (type %s)" % (rm[i][0][:64], rm[i][1])
        print("      %04x  %08x%s" % (i, w, tag))
    return b


print("\n" + "#" * 100)
print("# FAMILY B: the six survivors' own bodies -- are they really distinct functions?")
print("#" * 100)
survs = ["??_ECX2SubmixVoice@XAUDIO2@@UAAPAXI@Z", "??_ECX2SourceVoice@XAUDIO2@@UAAPAXI@Z",
         "??_GCAudioSRC@LEAPFX@@UAAPAXI@Z", "??_GCAudioFilter@LEAPFX@@UAAPAXI@Z",
         "??_GCX2Engine@XAUDIO2@@UAAPAXI@Z", "??_ECX2SourceVoiceWMA@XAUDIO2@@UAAPAXI@Z"]
bodies = {}
for s in survs:
    b = S.traw.get(s)
    bodies[s] = b
    print("%-46s %s" % (s[:46], ("%3d B  relocs=%s" % (len(b[0]), [ (hex(o), n[:40]) for o,n,_ in b[1] ])) if b else "ABSENT"))

print("\nPairwise RAW-byte identity among the six survivors:")
for i in range(len(survs)):
    for j in range(i + 1, len(survs)):
        a, b = bodies[survs[i]], bodies[survs[j]]
        if a and b:
            print("  %-28s vs %-28s  bytes %s" % (survs[i][:28], survs[j][:28],
                  "IDENTICAL" if a[0] == b[0] else "DIFFER (%d vs %d B)" % (len(a[0]), len(b[0]))))

print("\n" + "#" * 100)
print("# FAMILY B: our ObjRefConcrete thunks + dtors")
print("#" * 100)
for n in ["??_G?$ObjRefConcrete@VRndCubeTex@@VObjectDir@@@@UAAPAXI@Z",
          "??_G?$ObjRefConcrete@VRndFur@@VObjectDir@@@@UAAPAXI@Z",
          "??1?$ObjRefConcrete@VRndCubeTex@@VObjectDir@@@@UAA@XZ",
          "??1?$ObjRefConcrete@VRndFur@@VObjectDir@@@@UAA@XZ"]:
    show("OURS", S.oraw, n, limit=30)

print("\n" + "#" * 100)
print("# FAMILY A: MemOrPoolAlloc / MemAlloc")
print("#" * 100)
show("RETAIL", S.traw, "?MemOrPoolAlloc@@YAPAXHPBDH0@Z", limit=16)
show("OURS", S.oraw, "?MemOrPoolAlloc@@YAPAXH@Z", limit=16)
show("OURS", S.oraw, "?MemOrPoolAllocSTL@@YAPAXH@Z", limit=16)
show("OURS", S.oraw, "?MemAlloc@@YAPAXHH@Z", limit=16)
show("RETAIL", S.traw, "fn_827BCD38", limit=24)
print("\n-- GROUNDED-1's big block --")
show("RETAIL", S.traw, "??2CriticalSection@@SAPAXI@Z", limit=16)
show("OURS", S.oraw, "??2CriticalSection@@SAPAXI@Z", limit=16)
show("OURS", S.oraw, "??2@YAPAXI@Z", limit=16)

print("\n" + "#" * 100)
print("# FAMILY C: BinStream list operator<<")
print("#" * 100)
for a in ["fn_82327050", "fn_824C8F68"]:
    show("RETAIL", S.traw, a, limit=24)
show("OURS", S.oraw, "??6@YAAAVBinStream@@AAV0@ABVPracticeSectionMapping@SongSectionController@@@Z", limit=24)
