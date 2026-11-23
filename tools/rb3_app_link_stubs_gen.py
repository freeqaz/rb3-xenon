#!/usr/bin/env python3
"""Generate native/src/rb3_app_link_stubs.s from an rb3-app link log (lane A0).

Usage: rb3_app_link_stubs_gen.py <native_build_dir> <lld_link_log> <out.s>

Input: the lld log of an rb3-app link with this file EMPTY (every undefined
symbol reported, -Wl,--error-limit=0). For each undefined FUNCTION, vtable or
typeinfo it writes a strong definition (strong on purpose: when a real TU joins
the target the link fails with a duplicate instead of silently picking one):

  * function  -> jumps to rb3_app_stub_reached(ring, name), which prints the
                 ring and the function and aborts. No stub returns.
  * vtable    -> offset-to-top 0, typeinfo 0, then 64 slots that all abort
                 through rb3_app_vcall_unreached.
  * typeinfo  -> a base-less __class_type_info with the right name, so a
                 dynamic_cast of a foreign object fails (null) rather than
                 crashing. No object of the class can exist: its ctor is absent.

Data globals (TheNet, TheBandUI, ...) are NOT generated: they need the C++ type
for their size, so rb3_app_link_stubs.cpp defines them by hand.

Every symbol carries `// ring: <tag>` on its label line, which
tools/native_stub_census.py --rings reads. The ring is the first RULES match on
the demangled name; the tag says which out-of-scope ring the stub stands in for,
or which later milestone owes the real body (a1-*, a4-*, a6-*).
"""
import glob, os, re, subprocess, sys

BD, LOG, OUT = sys.argv[1], sys.argv[2], sys.argv[3]

RULES = [
    (r"^(vtable for |typeinfo for )?Quazal::", "online-quazal"),
    (r"^(NetSession::|BandNetGameData::)", "online-netsession"),
    # A3: NetSession.cpp + NetSession_Xbox.cpp are compiled, so the idle
    # XboxSession exists; what its online paths call (XSession jobs, the Quazal
    # session, search results, the messenger, voice chat) stays aborting.
    (r"^(vtable for )?((AddLocalPlayer|AddRemotePlayer|DeleteSession|EndSession|MakeSession|"
     r"ModifySession|RegisterArbitration|RemoveLocalPlayer|RemoveRemotePlayer|StartSession|"
     r"WriteTrueSkill|MakeQuazalSession)Job|QuazalSession|NetSearchResult|NetMessenger|"
     r"SyncUserMsg|SyncAllMsg|MicManagerXbox)\b", "online-netsession"),
    (r"^Synth360::(ActivateLocalChat|GetChatData)", "online-netsession"),
    (r"^(vtable for )?(MatchmakingSettings|SearchSettings|SessionSettings|SessionSearcher)\b",
     "online-matchmaking"),
    (r"^(Net::|SyncStore::)", "online-net"),
    (r"^RockCentral::", "online-rockcentral"),
    # A3: RockCentral.cpp is compiled (TheRockCentral is a real static-init
    # object); its web-service plumbing stays aborting.
    (r"^(ContextWrapper|ContextWrapperPool|JsonObject|UpdateFriendsListJob)::", "online-rockcentral"),
    # A5: SessionSearcher_Xbox.cpp is compiled (the idle XSessionSearcher);
    # its search and invite paths call XSessionSearchEx / XInviteGetAcceptedInfo.
    (r"^(XNet|XUserMuteListQuery|XUserSetProperty|XSessionSearchEx|XInviteGetAcceptedInfo)",
     "xbox-live"),
    # rbx-net: XLSPConnection.cpp is compiled (NetCacheMgrXbox's member); its
    # connect path enumerates LIVE title servers.
    (r"^XTitleServerCreateEnumerator$", "xbox-live"),
    (r"^(AssetOffer::|XMarketplace|XBackgroundDownload|StoreRootPanel::)", "xbox-store"),
    (r"^(XMsgCancelIORequest|XUserReadGamerPicture|XGetOverlappedResult|XStringVerify|ShowParentalControlUI|SessionUsersProvider::ShowGamercard)",
     "xbox-live"),
    (r"^D3D", "xbox-d3d"),
    (r"^__v", "a1-xbox-vmx"),
    (r"^PlatformMgr::", "a1-platformmgr"),
    (r"^(Wii|OvershellProfileProvider::)", "a4-wii-residue"),
    # Bodies that live in a TU that does not compile natively yet (PLAN.md 2.1).
    (r"^(typeinfo for )?(Metronome|HeldNote|TexMovie|NameGenerator|SongSelectPanel|"
     r"JoinInvitePanel|CampaignGoalsLeaderboardPanel|CampaignCareerLeaderboardPanel|"
     r"Label3d|BeatClock)\b", "a6-failing-tu"),
    (r"^Movie::SetTimeCallback", "a2-movie"),
    # rbx-audmgr: AuditionMgr.cpp is compiled; what only audition mode reaches
    # has no source: the RBN song Validator (retail 0x8272BAC8 onward), the
    # UGCNet_Server download client (0x82B8C8C8 onward) and the utility-drive
    # mount AuditionMgr::ThreadStart does for the song's files.
    (r"^(vtable for |typeinfo for )?(Validator|UGCNet_Server)\b", "audition-only"),
    (r"^X(Mount|Unmount)UtilityDrive$", "audition-only"),
    # Retail never calls these: VocalPlayer never reads mVocalOverlay (0x3c8);
    # every call site is under HX_NATIVE and the pointer is null unless
    # ToggleOverlay runs (session rb3-xenon-07, 114186523, VocalOverlay.h).
    # The stubs still abort; the ring says no retail body is owed for them.
    (r"^VocalOverlay::", "native-only-callsite"),
    # A14: ReadSingleXinputJoypad (Joypad_Xinput.cpp, compiled) classifies a
    # Harmonix peripheral by its XInput capability subtype through these
    # Joypad_Xbox.cpp bodies (not compiled: XInput2 thread). rb3-app's pad
    # (rb3_app_joypad.cpp) reports a standard gamepad, subtype 1, by default;
    # since lane gemwalk RB3_APP_JOYPAD_KIND=guitar|drums reports subtype 6/8,
    # and rb3_app_joypad.cpp defines SetupHXGuitar/SetupHXDrums itself; since
    # lane keyswalk (KIND=keys|proguitar, subtype 15/25) it defines
    # SetupHXKeytar/SetupHXRealGuitar too, so none of the four is undefined and
    # this rule matches nothing (kept so a future peripheral stub is tagged).
    (r"^SetupHX(Guitar|Drums|Keytar|RealGuitar)\(", "peripheral-only"),
    (r".*", "a1-absent-body"),
]

def ring(dm):
    for rx, tag in RULES:
        if re.search(rx, dm):
            return tag
    raise AssertionError(dm)

log = open(LOG, errors="replace").read()
want = set(re.findall(r"undefined symbol: (.+)\n", log))
want = {w.replace("> >", ">>") for w in want}
if not want:
    sys.exit("no undefined symbols in %s" % LOG)

# mangled names: nm -u over every rb3-app object, demangled to match lld's text
objs = glob.glob(os.path.join(BD, "CMakeFiles/rb3-app.dir/**/*.o"), recursive=True)
und = set()
for o in objs:
    r = subprocess.run(["nm", "-u", o], capture_output=True, text=True)
    und |= {l.split()[-1] for l in r.stdout.splitlines() if l.strip()}
und = sorted(und)
r = subprocess.run(["c++filt"], input="\n".join(und), capture_output=True, text=True)
def norm(d):
    # c++filt prints `> >`, lld prints `>>`
    while "> >" in d:
        d = d.replace("> >", ">>")
    return d
dm_of = {m: norm(d) for m, d in zip(und, r.stdout.splitlines())}

funcs, vtabs, tinfos, data, missing = [], [], [], [], set(want)
for m, d in sorted(dm_of.items(), key=lambda kv: kv[1]):
    if d not in want:
        continue
    missing.discard(d)
    if d.startswith("vtable for "):
        vtabs.append((m, d))
    elif d.startswith("typeinfo for "):
        tinfos.append((m, d))
    elif "(" in d or (m == d and not re.search(r"^(The[A-Z]|g[A-Z])|_msg$", d)):
        # a C++ function, or an extern "C" one (mangled == demangled) that is
        # not named like a global (TheFoo, gFoo, foo_msg)
        funcs.append((m, d))
    else:
        data.append((m, d))
if missing:
    sys.exit("lld names with no mangled match: %s" % sorted(missing)[:10])

rings = sorted({ring(d) for _, d in funcs + vtabs + tinfos})
lab = {t: ".Lring_" + re.sub(r"\W", "_", t) for t in rings}
o = []
o.append("// GENERATED by tools/rb3_app_link_stubs_gen.py from an rb3-app link log -- do not")
o.append("// edit by hand; re-derive (empty this file, link, regenerate). Lane A0, 2026-11-22.")
o.append("// %d functions, %d vtables, %d typeinfo. Every one fails loudly if reached;" %
         (len(funcs), len(vtabs), len(tinfos)))
o.append("// the `// ring:` tag on each label is read by tools/native_stub_census.py --rings.")
o.append("// Data globals are in rb3_app_link_stubs.cpp (they need their C++ type).")
o.append("")
o.append(".section .rodata.rb3_app_stub_names,\"a\",@progbits")
for t in rings:
    o.append("%s: .asciz \"%s\"" % (lab[t], t))
for i, (m, d) in enumerate(funcs):
    o.append(".Lname_%d: .asciz \"%s\"" % (i, d.replace("\\", "\\\\").replace('"', '\\"')))
o.append("")
o.append(".text")
for i, (m, d) in enumerate(funcs):
    t = ring(d)
    o.append("// %s" % d)
    o.append(".globl %s" % m)
    o.append(".type %s,@function" % m)
    o.append("%s: // ring: %s" % (m, t))
    o.append("    leaq %s(%%rip), %%rdi" % lab[t])
    o.append("    leaq .Lname_%d(%%rip), %%rsi" % i)
    o.append("    jmp rb3_app_stub_reached")
    o.append("")
if vtabs:
    o.append(".section .data.rel.ro.rb3_app_stub_vtables,\"aw\",@progbits")
    for m, d in vtabs:
        o.append("// %s: every slot aborts" % d)
        o.append(".globl %s" % m)
        o.append(".p2align 3")
        o.append("%s: // ring: %s" % (m, ring(d)))
        o.append("    .quad 0, 0")
        o.append("    .rept 64")
        o.append("    .quad rb3_app_vcall_unreached")
        o.append("    .endr")
        o.append("")
if tinfos:
    o.append(".section .data.rel.ro.rb3_app_stub_typeinfo,\"aw\",@progbits")
    for i, (m, d) in enumerate(tinfos):
        name = m[len("_ZTI"):]
        o.append("// %s: base-less __class_type_info" % d)
        o.append(".globl %s" % m)
        o.append(".p2align 3")
        o.append("%s: // ring: %s" % (m, ring(d)))
        o.append("    .quad _ZTVN10__cxxabiv117__class_type_infoE+16")
        o.append("    .quad .Ltsname_%d" % i)
        o.append("")
    o.append(".section .rodata.rb3_app_stub_tsnames,\"a\",@progbits")
    for i, (m, d) in enumerate(tinfos):
        o.append(".Ltsname_%d: .asciz \"%s\"" % (i, m[len("_ZTI"):]))
o.append("")
o.append(".section .note.GNU-stack,\"\",@progbits")
open(OUT, "w").write("\n".join(o) + "\n")
print("functions %d vtables %d typeinfo %d; data left for the .cpp: %d" %
      (len(funcs), len(vtabs), len(tinfos), len(data)))
for m, d in data:
    print("  DATA %s  (%s)" % (d, m))
for t in rings:
    print("  ring %-22s %d" % (t, sum(1 for _, d in funcs + vtabs + tinfos if ring(d) == t)))
