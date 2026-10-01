#!/usr/bin/env python3
"""Adjudicate the 8-byte allocator-thunk fold class on RETAIL BYTES.

THE CHARGE
----------
`name_check` charges 585 sites / ~420 functions where retail's relocation names
``??2CriticalSection@@SAPAXI@Z`` (0x827bd2f0) and ours names one of ~44 other
``operator new`` spellings.  It is the largest single open charge on the board
and the census bucketed it `cannot_adjudicate`, for a purely structural reason:
the `fold_thunk_naming` bucket requires a <=4-byte tail jump and this body is 8.

Eight bytes is BETTER evidence than four, not worse.  A 4-byte `b X` compares
equal to every other 4-byte `b Y` once the displacement is masked -- the vacuity
that b606f610 withdrew the CF1 tier over.  An 8-byte body carries a full
non-branch word that no masking touches.

WHAT THIS GATE ASKS, AND WHY IT DOES NOT NEED TO NAME THE BRANCH TARGET
----------------------------------------------------------------------
`tools/comdat_fold_gate.py` asks whether OUR COMDAT equals RETAIL's body at the
survivor address, which requires resolving retail's branch destination through
target_symbol_map.json to a NAME.  Here that destination is 0x827bcd38, which the
map does not name, so that chain REFUSES -- fail-closed on missing evidence, and
lane ALIAS-X2 correctly declined to invent the pin (our ?MemAlloc@@YAPAXHH@Z is a
20-byte stub against 644 bytes of real allocator, so no body match is available).

This gate asks a different question that the available evidence CAN answer:

    is our COMDAT for F byte- and RELOCATION-identical to our COMDAT for the
    map-resident survivor S?

If yes, /OPT:ICF *must* fold F onto S -- that is the linker condition itself,
applied to two of our own COMDATs.  Both sides carry the SAME relocation to the
SAME symbol, so whatever that symbol denotes, it denotes the same thing for both.
The unnamed 0x827bcd38 never enters the argument.

The retail side is not assumed, it is corroborated:  S is map-resident at
0x827bd2f0; retail's body there is 8 bytes whose word 0 equals ours as a FULL
32-bit value (0x38800000) and whose word 1 is a branch of matching opcode/AA/LK.
And the pair (word0, resolved destination) is UNIQUE image-wide -- see
--shape-census: 712 retail 8-byte <word>+<branch> bodies carry 707 distinct
combinations, and the 12 that share `li r4,0` have 12 DISTINCT destinations.  So
shape does none of the work; the destination does all of it, exactly the trap
lane MAP-B raised and ALIAS-X2 tested for `b MemFree`.

FAIL-CLOSED
-----------
* our objs disagree on F (two distinct COMDAT variants)          -> REFUSE
* F's body differs from S's in ANY byte or ANY relocation        -> REFUSE
* target_symbol_map.json places F at a DIFFERENT address         -> REFUSE
  (injectivity: one mangled name must not end up at two addresses)
* F already sits in an alias group at a different address        -> REFUSE

THE DISCRIMINATING CONTROL IS BUILT IN, AND IT ONCE REFUSED THE BIGGEST PAIR
---------------------------------------------------------------------------
⚠ DATED RECORD -- DO NOT ACT ON THIS PARAGRAPH AS A CURRENT VERDICT.  It
describes the state BEFORE the source fix in this file's own introducing commit
(e92a6c80), which is why it was already stale the day it was written.

``??2@YAPAXI@Z`` -- global ``operator new``, the single largest open charge at
510 sites / 367 functions -- WAS REFUSED on body.  Ours was 12 bytes
(``lis``/``lwz`` of ``?gNewOperatorAlign@@3HA`` then the branch); retail's is 8
(``li r4,0``).  That was a real SOURCE divergence inherited from dc3, which is
NEWER than RB3: the rb3-Wii oracle says ``operator new(size){return
_MemAlloc(size,0);}`` and knows no ``gNewOperatorAlign`` at all.  An alias there
would have hidden a genuine defect, which is precisely the failure mode this
gate exists to avoid.  That control WORKED, and the lane fixed source instead of
asserting an alias -- which is the durable lesson here.

★ TODAY IT ADMITS.  ``src/system/utl/MemMgr.cpp`` passes a literal 0, so our
``??2@YAPAXI@Z`` is 8 B / ``38800000 4bfffffc`` / one reloc to
``?MemAlloc@@YAPAXHH@Z`` -- byte- and relocation-identical to the survivor.  The
admission was installed in b288c232 (the very next commit) and MEASURED at
+67,884 B / +339 complete fns.  Re-verified from a freshly compiled obj by lane
ALLOCGATE-1 (2026-08-14), which also confirmed the group carries it.

⚠ NOTHING IN THIS GATE IS HARDCODED.  Every verdict is recomputed from the
compiled COMDAT bytes on each run, so a refusal recorded in prose here can never
be an operative refusal -- fix the source and the gate re-adjudicates itself.
Re-run it rather than reading this docstring for a verdict.

``??2Task@@SAPAXI@Z`` is REFUSED separately, on injectivity.

★★ W16-KF (2026-10-01) -- THE SURVIVOR IS NO LONGER HARDCODED, AND ITS
COMDAT-NESS IS CHECKED, NOT ASSUMED
---------------------------------------------------------------------------
W16-KD renamed 0x827BD2F0 from ``??2CriticalSection@@SAPAXI@Z`` to the global
``??2@YAPAXI@Z`` (retail MemInit inlines the global operator new for both of
its locks, so CriticalSection has no class allocator in retail).  This gate
hardcoded the old spelling and so REFUSED ALL with "our objs do not define the
survivor spelling" -- a refusal about a constant, not about evidence.  The
survivor is now read from target_symbol_map.json at SURVIVOR_VA.

The brief that reopened this said MemMgr.cpp defines the global operator new
"as an ordinary function, not a COMDAT".  MEASURED, that is half right:
under /Gy it IS a COMDAT section (IMAGE_SCN_LNK_COMDAT set), but with
selection 1 = IMAGE_COMDAT_SELECT_NODUPLICATES, where every inline class
allocator is selection 2 = SELECT_ANY.  The difference is real and the gate now
reads it (tools/comdat_bytes.py `section_comdat` / `comdat_select`):

  * survivor in a NON-COMDAT section          -> REFUSE ALL.  /OPT:ICF folds
    only COMDATs, so nothing can have folded onto a plain-.text body; members
    identical to it would fold among THEMSELVES, at an address of their own.
  * survivor SELECT_NODUPLICATES, >1 definer  -> REFUSE ALL.  A strong
    definition in two objs is LNK2005, so our objs are not one program.
  * a MEMBER in a non-COMDAT section          -> not foldable, REFUSE it.
  * the survivor's branch target, when the map names it (0x827bcd38 is
    ``?MemAlloc@@YAPAXHH@Z`` since MAPID-1), must equal OUR survivor's
    relocation target BY NAME.  This is new and strictly narrower.

MEMBERSHIP AUDIT (--audit-members): the install path above only ever asked
"which of our COMDATs equal the survivor?".  It never re-asked the question of
memberships already installed, so a membership admitted under an older source
state could rot silently -- and 39 had: OBJ_MEM_OVERLOAD's operator new became
class-specific (``(void)StaticClassName().Str()``, 60 B) and no longer equals
the 8-byte survivor.  The audit adjudicates every installed membership on three
independent legs:

  BODY       our COMDAT single-variant, a COMDAT, byte- AND reloc-identical to
             our survivor COMDAT (the /OPT:ICF condition).
  SITES      RETAIL call sites, from the dtk-split target objs, paired by
             caller name with ours (tools/icf_site_census.py's unit pairing).
             STRICT tier only decides: identical size and (offset, type)
             sequence.  retail-names-survivor where ours names F = WITNESS;
             retail names a different, NAMED function = CONTRADICTION.  The
             bl-index tier is printed and never decides -- measured: it
             mis-slots CharLipSyncDriver::Sync by one call (retail has an
             extra Release), reading a witness as a "contradiction".
  NEWOBJECT  retail ``?NewObject@<Class>@@`` (when map-resident): does it call
             the survivor (THUNK), or inline StaticClassName + MemAlloc itself
             (INLINED -- the class has no out-of-line allocator to fold)?

  PROVEN                 BODY identical, no strict contradiction, NewObject
                         not INLINED.  Graded SITE / NEWOBJECT / BODY-ONLY by
                         the strongest retail witness found.
  CONTRADICTED_BODY      our COMDAT cannot fold onto the survivor.
  CONTRADICTED_SITE      a strict-aligned retail site names something else.
  CONTRADICTED_NEWOBJECT retail inlines the class allocator.
  NO_DEFINITION          we compile no such spelling: unprovable today.

--withdraw moves every non-PROVEN membership to the group's `withdrawn` list
with a record (nothing is deleted).  NO_DEFINITION records carry
``"regate": true``: they are unproven, not refuted, so a future port whose
COMDAT really equals the survivor may be re-admitted by --install; every other
record keeps the old never-re-admit ban.

CONTROLS (--selftest), each a decoy that ONE check exists to catch.
``--self-break CHECK`` removes that check and exits 0 only if exactly the
controls tagged CHECK go red and every other control stays green -- i.e. each
control is shown to be able to FAIL, and to fail for its own reason.
"""

import argparse
import collections
import copy
import glob
import json
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
sys.path.insert(0, str(ROOT / "scripts"))
from comdat_bytes import comdats                        # noqa: E402
from wrong_callee_triage import Image, load_sizes       # noqa: E402

BUILD_ID = "45410914"
SURVIVOR_VA = 0x827BD2F0
BRANCH_OPS = (16, 18)
SELECT_NODUPLICATES = 1
LANE = "W16-KF 2026-10-01"

# Every check the gate applies, by name.  --self-break removes exactly one.
ALL_CHECKS = ("retail", "dest", "comdat", "nodup", "relocs", "sites", "newobject")
PLACEHOLDER = ("fn_", "lbl_", "vftable_", "data_", "jumptable_", "string_",
               "unnamed_")


def branch_dest(w, va):
    op = w >> 26
    if op == 18:
        d = w & 0x03FFFFFC
        if d & 0x02000000:
            d -= 0x04000000
    elif op == 16:
        d = w & 0x0000FFFC
        if d & 0x8000:
            d -= 0x10000
    else:
        return None
    return d if (w & 2) else va + d


# ---------------------------------------------------------------- inputs
class Retail:
    def __init__(self):
        self.img = Image(ROOT / "orig" / BUILD_ID / "band.exe")
        self.size = load_sizes()
        smap = json.loads((ROOT / "scripts" / "target_symbol_map.json").read_text())
        self.byva = {int(a, 16): n for a, n in smap.items()
                     if a.startswith("0x") and isinstance(n, str)}
        self.byname = collections.defaultdict(set)
        for va, n in self.byva.items():
            self.byname[n].add(va)

    def words(self, va):
        n = self.size.get(va)
        off = self.img.off(va)
        if not n or off is None:
            return None
        return list(struct.unpack_from(">%dI" % (n // 4), self.img.data, off))

    def calls(self, va):
        """(offset, destination) of every b/bl in the retail body at va."""
        out = []
        for i, w in enumerate(self.words(va) or []):
            if (w >> 26) == 18:
                out.append((i * 4, branch_dest(w, va + 4 * i)))
        return out


def our_comdats():
    """name -> {(raw, relocs): [(obj path, section_comdat, comdat_select)]}."""
    out = collections.defaultdict(lambda: collections.defaultdict(list))
    for p in glob.glob(str(ROOT / "build" / BUILD_ID / "src" / "**" / "*.obj"),
                       recursive=True):
        try:
            c = comdats(p)
        except Exception:
            continue
        for n, v in c.items():
            if not v.get("is_code"):
                continue
            rel = tuple(sorted((o, s, t) for o, s, t in (v["fn_relocs"] or [])
                               if s != "@comp.id"))
            out[n][(v["fn_raw"], rel)].append(
                (p, v.get("section_comdat"), v.get("comdat_select")))
    return out


# ---------------------------------------------------------------- checks
def retail_survivor(R):
    rw = R.words(SURVIVOR_VA)
    rdest = branch_dest(rw[1], SURVIVOR_VA + 4) if rw and len(rw) > 1 else None
    return {"name": R.byva.get(SURVIVOR_VA), "words": rw, "dest": rdest,
            "dest_name": R.byva.get(rdest), "fanin": R.img.fanin().get(SURVIVOR_VA)}


def survivor_check(sv_variants, rs, checks):
    """(ok, reasons, (raw, rel)) for OUR survivor definition vs retail."""
    why = []
    if not sv_variants:
        return False, ["our objs do not define the survivor spelling "
                       f"{rs['name']}"], None
    if len(sv_variants) != 1:
        return False, [f"our objs disagree on the survivor: {len(sv_variants)} "
                       "distinct COMDAT variants"], None
    (raw, rel), defs = next(iter(sv_variants.items()))
    if "comdat" in checks and not all(d[1] for d in defs):
        why.append("survivor is NOT in a COMDAT section in "
                   f"{sum(1 for d in defs if not d[1])} obj(s): /OPT:ICF folds "
                   "only COMDATs, so nothing can have folded onto it")
    if "nodup" in checks and any(d[2] == SELECT_NODUPLICATES for d in defs) \
            and len(defs) != 1:
        why.append(f"survivor is SELECT_NODUPLICATES but defined in {len(defs)} "
                   "objs (LNK2005): our objs are not one program")
    rw = rs["words"] or []
    sw = list(struct.unpack(">%dI" % (len(raw) // 4), raw))
    relo = {o: (s, t) for o, s, t in rel}
    if "retail" in checks:
        ok_len = len(raw) == 4 * len(rw)
        ok_w0 = bool(rw) and bool(sw) and sw[0] == rw[0]
        ok_br = (len(rw) > 1 and len(sw) > 1 and 4 in relo
                 and (sw[1] & 0xFC000003) == (rw[1] & 0xFC000003))
        if not (ok_len and ok_w0 and ok_br):
            why.append(f"survivor does not corroborate on retail bytes (size "
                       f"{ok_len}, word0 {ok_w0}, branch opcode/AA/LK {ok_br})")
    if "dest" in checks and rs["dest_name"]:
        ours = relo.get(4, (None,))[0]
        if ours != rs["dest_name"]:
            why.append(f"retail's survivor branches to {rs['dest_name']} but our "
                       f"survivor relocates to {ours}")
    return (not why), why, (raw, rel)


def body_verdict(variants, sv_key, checks):
    """IDENTICAL, or a reason our COMDAT for a member cannot fold on S."""
    if not variants:
        return "NO_DEFINITION", "no compiled obj defines this spelling"
    if len(variants) != 1:
        return "DIFFERS", f"our objs define {len(variants)} distinct COMDAT variants"
    (raw, rel), defs = next(iter(variants.items()))
    if "comdat" in checks and not all(d[1] for d in defs):
        return "DIFFERS", "not in a COMDAT section: /OPT:ICF cannot fold it"
    if len(raw) != len(sv_key[0]):
        return "DIFFERS", (f"body {len(raw)} B != survivor {len(sv_key[0])} B "
                           "-- different-size COMDATs cannot fold")
    if raw != sv_key[0]:
        return "DIFFERS", "body bytes differ from the survivor"
    if "relocs" in checks and rel != sv_key[1]:
        return "DIFFERS", ("bytes equal but relocation targets differ: "
                           f"{[r[1] for r in rel]} vs {[r[1] for r in sv_key[1]]}")
    return "IDENTICAL", "COMDAT byte- and reloc-identical to the survivor"


def site_census(members, S):
    """Per member: strict/bl witnesses and contradictions from RETAIL call sites.

    Retail side = the dtk-split target objs (relocation names written by
    obj_target_symbol_renamer from the map); our side = our compiled objs; units
    paired from objdiff.json, callers paired by NAME.  Only the strict tier
    (same size, same (offset, type) sequence) decides anything."""
    import icf_site_census as C
    members = set(members)
    out = {m: collections.Counter() for m in members}
    ex = collections.defaultdict(list)
    for unit, (tp, op) in sorted(C.load_unit_objs(ROOT).items()):
        try:
            tf, of = C.index_fns(tp), C.index_fns(op)
        except Exception:
            continue
        for fn, (ts, trl) in tf.items():
            ob = of.get(fn)
            if ob is None:
                continue
            os_, orl = ob
            if ts == os_ and [(o, t) for o, _n, t in trl] == [(o, t) for o, _n, t in orl]:
                slots, tier = list(zip(trl, orl)), "strict"
            else:
                tb = [x for x in trl if x[2] == 0x06]
                obl = [x for x in orl if x[2] == 0x06]
                if not tb or len(tb) != len(obl):
                    continue
                slots, tier = list(zip(tb, obl)), "bl"
            for (o, tn, _ty), (_o, bn, _t) in slots:
                if bn not in members:
                    continue
                if tn == S:
                    k = "wit"
                elif tn == bn:
                    k = "same"
                elif tn.startswith(PLACEHOLDER):
                    k = "ph"
                else:
                    k = "con"
                out[bn][f"{k}_{tier}"] += 1
                if k in ("wit", "con") and len(ex[(bn, k, tier)]) < 3:
                    ex[(bn, k, tier)].append(f"{fn}@+0x{o:x}->{tn}")
    return out, ex


def class_of(spelling):
    """'??2Foo@Bar@@SAPAXI@Z' -> 'Foo@Bar' (None if not a class allocator)."""
    if not spelling.startswith("??2") or not spelling.endswith("@@SAPAXI@Z"):
        return None
    return spelling[3:-len("@@SAPAXI@Z")]


def newobject_class(R, cls, S_va=SURVIVOR_VA):
    """Retail NewObject of `cls`: THUNK / INLINED / NEITHER / None (unmapped)."""
    vas = R.byname.get(f"?NewObject@{cls}@@SAPAVObject@Hmx@@XZ")
    if not vas or len(vas) != 1:
        return None, None
    va = next(iter(vas))
    dests = [d for _o, d in R.calls(va)]
    names = [R.byva.get(d) or "" for d in dests]
    if S_va in dests:
        return "THUNK", va
    if any(n.startswith(f"?StaticClassName@{cls}@@") for n in names) \
            and any(n.startswith(("?MemAlloc@@", "?_MemAlloc")) for n in names):
        return "INLINED", va
    return "NEITHER", va


def retail_copies(R, raw, rel):
    """Retail .pdata functions equal to our body: (exact, shape).

    exact: non-branch words equal and every relocated branch's retail
    destination is MAP-NAMED and equals our relocation target; shape: the same
    with branch destinations masked (worthless alone for an 8-byte thunk --
    see --shape-census -- it is printed only to show the exact test bit)."""
    n = len(raw)
    ow = list(struct.unpack(">%dI" % (n // 4), raw))
    relo = {o: s for o, s, _t in rel}
    exact, shape = [], 0
    for va, sz in R.size.items():
        if sz != n:
            continue
        rw = R.words(va)
        if rw is None:
            continue
        ok_shape, ok_exact = True, True
        for i, (a, b) in enumerate(zip(rw, ow)):
            if 4 * i in relo and (b >> 26) in BRANCH_OPS:
                if (a & 0xFC000003) != (b & 0xFC000003):
                    ok_shape = ok_exact = False
                    break
                if R.byva.get(branch_dest(a, va + 4 * i)) != relo[4 * i]:
                    ok_exact = False
            elif 4 * i in relo:
                ok_exact = False          # data reloc: not decided here
            elif a != b:
                ok_shape = ok_exact = False
                break
        shape += ok_shape
        if ok_exact:
            exact.append(va)
    return exact, shape


def audit(ours, sv_key, S, members, R, checks, sites=None, nob_override=None):
    """Verdict for every installed membership (see the module docstring)."""
    if sites is None:
        sites, _ex = site_census(members, S)
    rows = []
    for m in members:
        bv, bwhy = body_verdict(ours.get(m), sv_key, checks)
        st = sites.get(m, collections.Counter())
        cls = class_of(m)
        nob, nob_va = (nob_override or {}).get(m) or (
            newobject_class(R, cls) if cls else (None, None))
        if bv == "NO_DEFINITION":
            v = "NO_DEFINITION"
        elif bv != "IDENTICAL":
            v = "CONTRADICTED_BODY"
        elif "sites" in checks and st["con_strict"]:
            v = "CONTRADICTED_SITE"
        elif "newobject" in checks and nob == "INLINED":
            v = "CONTRADICTED_NEWOBJECT"
        else:
            v = "PROVEN"
        grade = ("SITE" if st["wit_strict"] else
                 "NEWOBJECT" if nob == "THUNK" else "BODY-ONLY")
        rows.append({"spelling": m, "verdict": v,
                     "grade": grade if v == "PROVEN" else None,
                     "body": bwhy, "sites": dict(st),
                     "newobject": nob, "newobject_va": nob_va and hex(nob_va)})
    return rows


# ---------------------------------------------------------------- selftest
def selftest(R, rs, ours, S, members, checks):
    """Run every control; return [(tag, label, wanted, got)]."""
    sv = ours.get(S)
    res = []

    def rec(tag, label, want, got):
        res.append((tag, label, want, got))

    ok, why, sv_key = survivor_check(sv, rs, checks)
    rec("-", "POSITIVE: live survivor corroborates", True, ok)
    if sv_key is None:
        return res
    (raw, rel), defs = next(iter(sv.items()))

    # retail: flip retail word0 -> must refuse
    bad = dict(rs, words=[rs["words"][0] ^ 0x1] + rs["words"][1:])
    rec("retail", "DECOY: retail survivor word0 altered", False,
        survivor_check(sv, bad, checks)[0])
    # dest: our survivor relocating to a different allocator -> must refuse
    rel2 = tuple((o, "?_MemAllocTemp@@YAPAXHH@Z" if o == 4 else s, t)
                 for o, s, t in rel)
    rec("dest", "DECOY: our survivor relocates to _MemAllocTemp", False,
        survivor_check({(raw, rel2): defs}, rs, checks)[0])
    # comdat: survivor in plain .text -> must refuse
    rec("comdat", "DECOY: survivor in a NON-COMDAT section", False,
        survivor_check({(raw, rel): [(d[0], False, None) for d in defs]},
                       rs, checks)[0])
    # nodup: strong survivor defined twice -> must refuse
    rec("nodup", "DECOY: SELECT_NODUPLICATES survivor defined in 2 objs", False,
        survivor_check({(raw, rel): [(defs[0][0], True, SELECT_NODUPLICATES),
                                     ("dup.obj", True, SELECT_NODUPLICATES)]},
                       rs, checks)[0])

    # relocs: a LIVE same-bytes / different-reloc COMDAT (e.g. a _MemAllocTemp
    # thunk) must not read IDENTICAL.
    # Prefer the IN-FAMILY decoy -- an operator new branching to a DIFFERENT
    # allocator (??2AsyncFileWin -> _MemAllocTemp on this tree) -- over an
    # unrelated byte-twin.
    twins = [n for n, v in sorted(ours.items()) if n != S and len(v) == 1
             and next(iter(v))[0] == raw and next(iter(v))[1] != rel]
    decoy = next((n for n in twins if n.startswith("??2")), None) or \
        next(iter(twins), None)
    if decoy is None:
        rec("relocs", "DECOY: live same-bytes/different-reloc COMDAT (NONE FOUND)",
            "DIFFERS", "MISSING")
    else:
        rec("relocs", f"DECOY: {decoy} (same bytes, different reloc)",
            "DIFFERS", body_verdict(ours[decoy], sv_key, checks)[0])
    # comdat (member side): an identical member in plain .text
    mem_ok = next((m for m in sorted(members)
                   if body_verdict(ours.get(m), sv_key, checks)[0] == "IDENTICAL"),
                  None)
    if mem_ok:
        v = copy.deepcopy(dict(ours[mem_ok]))
        k = next(iter(v))
        v[k] = [(p, False, None) for p, _c, _s in v[k]]
        rec("comdat", f"DECOY: {mem_ok} moved to a NON-COMDAT section",
            "DIFFERS", body_verdict(v, sv_key, checks)[0])

    # sites + newobject: one live PROVEN-by-site member, then the same member
    # with an injected strict contradiction / an injected INLINED NewObject.
    sites, _ex = site_census(members, S)
    pos = next((m for m in sorted(members) if sites[m]["wit_strict"]
                and body_verdict(ours.get(m), sv_key, checks)[0] == "IDENTICAL"),
               None)
    if pos is None:
        rec("sites", "POSITIVE: a strict-witnessed member (NONE FOUND)",
            "PROVEN", "MISSING")
        return res
    base = audit(ours, sv_key, S, [pos], R, checks, sites=sites)[0]
    rec("-", f"POSITIVE: {pos} (strict retail witness)", "PROVEN", base["verdict"])
    s2 = {pos: collections.Counter(sites[pos], con_strict=1)}
    rec("sites", f"DECOY: {pos} + injected strict retail contradiction",
        "CONTRADICTED_SITE",
        audit(ours, sv_key, S, [pos], R, checks, sites=s2)[0]["verdict"])
    rec("newobject", f"DECOY: {pos} + injected INLINED retail NewObject",
        "CONTRADICTED_NEWOBJECT",
        audit(ours, sv_key, S, [pos], R, checks, sites=sites,
              nob_override={pos: ("INLINED", 0)})[0]["verdict"])

    # newobject classifier must be able to say BOTH things on live retail bytes
    seen = collections.Counter()
    for m in members:
        c = class_of(m)
        if c:
            seen[newobject_class(R, c)[0]] += 1
    rec("-", f"LIVE NewObject classifier says THUNK somewhere ({dict(seen)})",
        True, seen["THUNK"] > 0)
    rec("-", "LIVE NewObject classifier says INLINED somewhere", True,
        seen["INLINED"] > 0)
    return res


# ---------------------------------------------------------------- main
def withdrawal_record(row, R, ours, S):
    v = row["verdict"]
    cls = {"CONTRADICTED_BODY": "BODY_CANNOT_FOLD",
           "CONTRADICTED_SITE": "RETAIL_SITE_CONTRADICTS",
           "CONTRADICTED_NEWOBJECT": "RETAIL_NEWOBJECT_INLINES",
           "NO_DEFINITION": "NO_DEFINITION"}[v]
    ev = [f"tools/alloc_fold_gate.py --audit-members: {v}. body: {row['body']}."]
    variants = ours.get(row["spelling"])
    if variants and len(variants) == 1:
        (raw, rel), _d = next(iter(variants.items()))
        exact, shape = retail_copies(R, raw, rel)
        ev.append(f"our {len(raw)}-B body occurs in retail exactly at "
                  f"{[hex(x) for x in exact] or 'NO address'} "
                  f"(shape-only matches {shape}); "
                  f"relocs {[r[1] for r in rel]}.")
        if exact and SURVIVOR_VA not in exact:
            cls = "FOLDS_ELSEWHERE"
            ev.append("retail keeps this exact body at a DIFFERENT address "
                      f"({', '.join(R.byva.get(x) or hex(x) for x in exact)}), so "
                      "if retail emitted it out of line it folded THERE, not at "
                      f"0x{SURVIVOR_VA:08x}.")
    if row["newobject"]:
        ev.append(f"retail NewObject ({row['newobject_va']}): {row['newobject']}"
                  + (" -- calls StaticClassName and MemAlloc itself; the class "
                     "allocator is inlined, there is no out-of-line body to fold"
                     if row["newobject"] == "INLINED" else "") + ".")
    st = row["sites"]
    ev.append(f"retail call sites (strict/bl tier): witness {st.get('wit_strict', 0)}"
              f"/{st.get('wit_bl', 0)}, contradiction {st.get('con_strict', 0)}"
              f"/{st.get('con_bl', 0)} -- the membership forgives no aligned site.")
    out = {"spelling": row["spelling"], "lane": LANE, "class": cls,
           "disposition": "withdrawn from folded; the record is the membership's "
                          "audit trail",
           "evidence": " ".join(ev)}
    if v == "NO_DEFINITION":
        out["regate"] = True
        out["evidence"] += (" UNPROVEN, not refuted: --install may re-admit it "
                            "once a compiled COMDAT is byte- and reloc-identical "
                            "to the survivor.")
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--shape-census", action="store_true",
                    help="run the MAP-B uniqueness test on retail's 8-byte thunks")
    ap.add_argument("--json", help="write the verdict table here")
    ap.add_argument("--install", action="store_true",
                    help="merge the ADMITted group into scripts/symbol_aliases.json")
    ap.add_argument("--audit-members", action="store_true",
                    help="adjudicate every INSTALLED membership on retail bytes")
    ap.add_argument("--withdraw", action="store_true",
                    help="with --audit-members: withdraw every non-PROVEN "
                         "membership, with a record")
    ap.add_argument("--selftest", action="store_true",
                    help="run the controls; exits 1 if any control fails")
    ap.add_argument("--self-break", choices=ALL_CHECKS,
                    help="run the controls with ONE check removed; exits 0 only "
                         "if exactly that check's controls go red")
    args = ap.parse_args()

    R = Retail()
    rs = retail_survivor(R)
    S = rs["name"]
    if not S:
        sys.exit(f"0x{SURVIVOR_VA:08x} is not named in target_symbol_map.json -> REFUSE")
    print(f"RETAIL survivor 0x{SURVIVOR_VA:08x} = {S}")
    print(f"  words={[f'{x:08x}' for x in rs['words'] or []]}")
    print(f"  word1 -> 0x{rs['dest'] or 0:08x} (map name: {rs['dest_name']})")
    print(f"  fan-in={rs['fanin']}")

    ali = json.loads((ROOT / "scripts" / "symbol_aliases.json").read_text())
    addr = f"0x{SURVIVOR_VA:08x}"
    grp = [g for g in ali["groups"] if (g.get("address") or "").lower() == addr]
    members = sorted(grp[0].get("folded", [])) if grp else []

    if args.shape_census:
        size, rw, rdest = R.size, rs["words"], rs["dest"]
        cand = collections.Counter()
        exact = []
        for va, sz in size.items():
            if sz != 8:
                continue
            w = R.words(va)
            if w is None or (w[1] >> 26) != 18:
                continue
            d = branch_dest(w[1], va + 4)
            cand[(w[0], d)] += 1
            if w[0] == rw[0] and d == rdest:
                exact.append(va)
        li0 = [k for k in cand if k[0] == rw[0]]
        print("\n-- MAP-B shape-uniqueness test --")
        print(f"  retail 8-byte <word>+<b> bodies : {sum(cand.values())}")
        print(f"  distinct (word0, dest) combos   : {len(cand)}")
        print(f"  bodies sharing word0={rw[0]:08x}    : "
              f"{sum(v for k, v in cand.items() if k[0] == rw[0])} "
              f"across {len(li0)} DISTINCT destinations")
        print(f"  bodies matching BOTH            : {len(exact)} "
              f"-> {[hex(v) for v in exact]}")
        print("  => shape alone is worthless; the resolved destination is "
              "what discriminates.")

    ours = our_comdats()

    if args.selftest or args.self_break:
        checks = set(ALL_CHECKS) - ({args.self_break} if args.self_break else set())
        res = selftest(R, rs, ours, S, members, checks)
        red = collections.Counter()
        for tag, label, want, got in res:
            bad = want != got
            red[tag] += bad
            print(f"  [{'RED ' if bad else 'ok  '}] ({tag:9s}) {label}: "
                  f"wanted {want}, got {got}")
        n_tag = collections.Counter(t for t, *_ in res)
        if args.self_break:
            b = args.self_break
            hit = n_tag[b] and red[b] == n_tag[b]
            other = sum(v for t, v in red.items() if t != b)
            if hit and not other:
                print(f"\nself-break {b} OK -- all {n_tag[b]} control(s) tagged "
                      f"'{b}' went RED with the check removed; no other control moved.")
                return 0
            print(f"\nself-break {b} FAILED -- {red[b]}/{n_tag[b]} '{b}' controls red, "
                  f"{other} other controls red.  The control does not discriminate.")
            return 1
        if sum(red.values()):
            print("\nselftest FAILED")
            return 1
        print(f"\nselftest PASSED -- {len(res)} controls, every check tagged in "
              f"{sorted(set(n_tag) - {'-'})}")
        return 0

    sv = ours.get(S)
    ok, why, sv_key = survivor_check(sv, rs, set(ALL_CHECKS))
    if sv:
        (raw, rel), defs = next(iter(sv.items()))
        sw = list(struct.unpack(">%dI" % (len(raw) // 4), raw))
        print(f"\nOUR survivor COMDAT {S}: size={len(raw)} "
              f"words={[f'{x:08x}' for x in sw]}")
        print(f"  relocs={list(rel)}  ({len(defs)} defs; COMDAT="
              f"{[d[1] for d in defs]}, selection={[d[2] for d in defs]})")
    if not ok:
        sys.exit("REFUSE ALL: " + "; ".join(why))
    print("  corroborated on retail bytes (size, word0, branch, destination name)")

    if args.audit_members:
        rows = audit(ours, sv_key, S, members, R, set(ALL_CHECKS))
        tally = collections.Counter((r["verdict"], r["grade"]) for r in rows)
        print(f"\n==== MEMBERSHIP AUDIT: {len(rows)} installed memberships ====")
        for k, n in sorted(tally.items(), key=lambda kv: (kv[0][0], str(kv[0][1]))):
            print(f"  {n:4d}  {k[0]}" + (f" [{k[1]}]" if k[1] else ""))
        for r in rows:
            if r["verdict"] != "PROVEN":
                print(f"  {r['verdict']:24s} {r['spelling']}  -- {r['body']}; "
                      f"NewObject={r['newobject']}; sites={r['sites']}")
        if args.json:
            Path(args.json).write_text(json.dumps(
                {"survivor": S, "address": addr, "rows": rows}, indent=1))
            print(f"\nwrote {args.json}")
        if args.withdraw:
            g = grp[0]
            gone = [r for r in rows if r["verdict"] != "PROVEN"]
            recs = [withdrawal_record(r, R, ours, S) for r in gone]
            out = set(r["spelling"] for r in gone)
            g["folded"] = [f for f in g["folded"] if f not in out]
            g.setdefault("withdrawn", []).extend(recs)
            p = ROOT / "scripts" / "symbol_aliases.json"
            p.write_text(json.dumps(ali, indent=1, ensure_ascii=False) + "\n", encoding="utf-8")
            print(f"\nwithdrew {len(recs)} memberships "
                  f"({dict(collections.Counter(r['class'] for r in recs))}); "
                  f"{len(g['folded'])} remain folded")
        return 0

    # existing alias membership
    in_group = {}
    for g in ali["groups"]:
        for nm in [g["survivor"]] + list(g.get("folded", [])):
            in_group.setdefault(nm, set()).add((g.get("address") or "").lower())

    verdicts = []
    for name, variants in sorted(ours.items()):
        if name == S:
            continue
        # candidate iff SOME variant equals the survivor's body+relocs
        if not any(k == sv_key for k in variants):
            continue
        why, okm = [], True
        bv, bwhy = body_verdict(variants, sv_key, set(ALL_CHECKS))
        if bv != "IDENTICAL":
            okm = False
            why.append(bwhy)
        placed = R.byname.get(name, set())
        if placed and placed != {SURVIVOR_VA}:
            okm = False
            why.append("map places it at " +
                       ",".join(hex(v) for v in sorted(placed)))
        grpa = in_group.get(name, set())
        if grpa - {addr}:
            okm = False
            why.append(f"already aliased at {sorted(grpa)}")
        verdicts.append({
            "name": name, "verdict": "ADMIT" if okm else "REFUSE",
            "why": "; ".join(why) or
                   "COMDAT byte- and reloc-identical to the map-resident survivor",
            "defs": sum(len(v) for v in variants.values()),
        })

    # explicit REFUSE side: charged spellings whose body DIFFERS
    charged = []
    census = ROOT / "scripts" / "namecheck_df_census.json"
    if census.exists():
        for r in json.loads(census.read_text())["rows"]:
            if r["target"] == S:
                charged.append((r["base"], r["sites"], r["fns"]))
    for base, sites, fns in charged:
        if any(v["name"] == base for v in verdicts):
            continue
        v = ours.get(base)
        why = body_verdict(v, sv_key, set(ALL_CHECKS))[1]
        verdicts.append({"name": base, "verdict": "REFUSE", "why": why,
                         "sites": sites, "fns": fns, "defs": len(v or ())})

    na = sum(1 for v in verdicts if v["verdict"] == "ADMIT")
    print(f"\n==== VERDICTS: {na} ADMIT / {len(verdicts) - na} REFUSE ====")
    for v in sorted(verdicts, key=lambda v: (v["verdict"], v["name"])):
        print(f"  {v['verdict']:7s} {v['name']}")
        if v["verdict"] == "REFUSE":
            print(f"          {v['why']}")
    if args.json:
        Path(args.json).write_text(json.dumps(
            {"survivor": S, "address": addr, "verdicts": verdicts}, indent=1))
        print(f"\nwrote {args.json}")

    if args.install:
        folded = sorted(v["name"] for v in verdicts if v["verdict"] == "ADMIT")
        # injectivity, asserted rather than assumed: no admitted spelling may be
        # map-resident anywhere but the survivor, and none may already sit in a
        # group at another address.  MAP-B's assert is what caught a bad admit a
        # shared-string heuristic had blessed.
        for f in folded:
            placed = R.byname.get(f, set()) - {SURVIVOR_VA}
            assert not placed, f"INJECTIVITY: {f} also at {[hex(v) for v in placed]}"
            other = in_group.get(f, set()) - {addr}
            assert not other, f"INJECTIVITY: {f} already aliased at {sorted(other)}"
        assert S not in folded
        # Preserve the existing group (its evidence text, which later lanes
        # extended, and its `withdrawn` records) and never re-admit a withdrawn
        # spelling: ALIAS-2 withdrew four SURVIVOR_SIZE_MISMATCH spellings with
        # "Do NOT re-add", and a rebuild-from-scratch here used to drop that
        # record silently.  W16-KF: a record carrying "regate": true (an
        # UNPROVEN, not refuted, membership) is the one exception.
        prev = [g for g in ali["groups"] if (g.get("address") or "").lower() == addr]
        withdrawn = [w for g in prev for w in g.get("withdrawn", [])]
        banned = {w["spelling"] for w in withdrawn if not w.get("regate")}
        folded = sorted(set(f for g in prev for f in g.get("folded", []))
                        | {f for f in folded if f not in banned})
        if prev:
            g = prev[0]
            g["survivor"], g["folded"] = S, folded
        else:
            ali["groups"].append({
                "name": "operator_new_alloc_thunk", "address": addr,
                "survivor": S, "folded": folded,
                "evidence": "tools/alloc_fold_gate.py -- 8-byte allocator-thunk "
                            "fold class (see the tool's docstring)."})
        p = ROOT / "scripts" / "symbol_aliases.json"
        p.write_text(json.dumps(ali, indent=1, ensure_ascii=False) + "\n", encoding="utf-8")
        print(f"\ninstalled group of {len(folded)} folded spellings into {p}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
