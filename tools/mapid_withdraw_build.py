#!/usr/bin/env python3
"""MAPID-1: build the withdrawal set from the nine-address adjudication.

Each of the 16 NEEDS_MAP_ID memberships is blocked at ONE relocated offset whose
retail target is an unnamed fn_XXXXXXXX. Identifying those nine addresses on
retail bytes splits the class cleanly:

  fn_827BCD38  IDENTIFIED as ?MemAlloc@@YAPAXHH@Z  -> LICENSES its 2 memberships
  the other 8  identified as functions our folded spelling PROVABLY is not
               -> CONTRADICTS their 14 memberships

Withdrawal is PER-MEMBERSHIP; groups are kept (never pruned) with a `withdrawn`
record, per a745039e.
"""
import json, sys
from pathlib import Path

wt = Path(sys.argv[1]).resolve()
mem = json.load(open(sys.argv[2]))

# ---- the adjudication, keyed by the blocking address ------------------------
VENDOR = ("CONTRADICTED_VENDOR_CALLEE",
          "Retail's callee at the blocking offset is fn_%s, identified from its OWN named "
          "relocations as a vendor XAUDIO2/LEAPFX destructor (%s). Our folded spelling is "
          "Milo's ObjRefConcrete<T,ObjectDir>::~ObjRefConcrete, whose body stores the per-T "
          "vtable ??_7?$ObjRefConcrete@V<T>@@VObjectDir@@@@6B@ and calls "
          "?Release@Object@Hmx@@QAAXPAVObjRefOwner@@@Z. A deleting-dtor thunk is ~90%% "
          "relocation-free boilerplate whose ENTIRE information content is the two bl "
          "targets, so T1 accepted this pair across a game<->vendor boundary vacuously. "
          "PIGEONHOLE: this same folded spelling is claimed by SIX groups whose survivors "
          "sit at six DISTINCT retail addresses (0x82bf7570, 0x82bd4c98, 0x82bc4d38, "
          "0x82bd0f30, 0x82bd2ee0, 0x82bd4f40) -- a folded function has ONE address, so at "
          "most one could be true and none is. Do NOT re-add.")

LIST = ("CONTRADICTED_ELEMENT_SERIALIZER_RESIDENT",
        "Retail's list<T> serializer calls its per-element operator<< at 0x50; retail's is "
        "fn_%s. Our folded spelling's element serializer "
        "??6@YAAAVBinStream@@AAV0@ABVPracticeSectionMapping@SongSectionController@@@Z is "
        "ITSELF map-resident at 0x8230d6e8 -- retail kept it at its own address, so it is "
        "not fn_%s. Two list serializers whose element callees are different functions "
        "cannot be one COMDAT under reloc-restricted /OPT:ICF. This argument is "
        "build-independent (two retail addresses), not a cross-build size test. "
        "PIGEONHOLE: groups 344 and 857 both claim this spelling, survivors at 0x82327878 "
        "and 0x824c93c0. Do NOT re-add.")

VENDOR_ID = {
    "fn_82BC4B28": "calls ?Uninitialize@CAudioFilter@LEAPFX@@AAAXXZ and ??1CXAPOParametersBase@@UAA@XZ",
    "fn_82BD0EB8": "calls ??1?$CNonBlockingQueue@PAUMusicCommand@CX2Engine@XAUDIO2@@@@QAA@XZ and ??1CX2SourceVoice@XAUDIO2@@UAA@XZ",
    "fn_82BD2E78": "calls ??1?$CNonBlockingQueue@PAUMusicCommand@CX2Engine@XAUDIO2@@@@QAA@XZ and ??1CX2SourceVoice@XAUDIO2@@UAA@XZ",
    "fn_82BD4C48": "calls fn_82BCE0D0 twice; sits in the XAUDIO2 vendor band with CX2SourceVoice",
    "fn_82BD4EF0": "calls fn_82BCE0D0 twice; sits in the XAUDIO2 vendor band with CX2SourceVoiceWMA",
    "fn_82BF72E0": "calls ?TB_SAFE_CLOSE_HANDLE@@YAXAAPAX@Z (x3) and ??3@YAXPAX@Z",
}
LICENSED = {"fn_827BCD38"}

out = []
for m in mem:
    if m["verdict"] != "NEEDS_MAP_ID":
        continue
    addr = m["why"].split()[2]                      # "unidentified target fn_X @0xN"
    if addr in LICENSED:
        continue
    if addr in VENDOR_ID:
        cls, why = VENDOR[0], VENDOR[1] % (addr[3:], VENDOR_ID[addr])
    else:
        cls, why = LIST[0], LIST[1] % (addr[3:], addr[3:])
    out.append({"i": m["i"], "folded": m["folded"], "survivor": m["survivor"],
                "decisive": cls, "detail": why, "blocking_address": addr})

json.dump({"decisive": out}, open(sys.argv[3], "w"), indent=1)
print("withdrawal set: %d memberships over %d groups"
      % (len(out), len({x["i"] for x in out})))
for x in out:
    print("  group %-5d %-34s <- %s" % (x["i"], x["blocking_address"], x["folded"][:58]))
print("LICENSED (kept): %d memberships on fn_827BCD38"
      % sum(1 for m in mem if m["verdict"] == "NEEDS_MAP_ID" and "827BCD38" in m["why"]))
