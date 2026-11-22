// rb3-app native glue (lane A1, 2026-11-22). Native only, rb3-app only.
//
// Real, minimal native definitions for globals rb3-app used to get from
// dta_link_stubs.s, where every data symbol was 128 weak ZERO bytes. That is
// harmless for rb3-dta (nothing on its parse path reads them) and a silent
// crash for the real boot: two of these are C++ references, and zero storage
// for a reference is a null reference. Each definition below states the
// semantics it gives natively. Aborting function stubs are NOT here; they are
// in rb3_app_link_stubs.s/.cpp.
#include "movie/MovieSys.h"
#include "net/WebSvcMgr.h"
#include "os/ContentMgr.h"
#include "os/CritSec.h"
#include "os/Debug.h"
#include "synth/Synth.h"
#include <cstdio>

// Retail defines it outside the decompiled tree (os/Debug.cpp, which rb3-app
// compiles, declares but does not define it). The
// class is a 0x100-byte char buffer with no constructor, so static zero
// storage IS the retail initial state ("no message printed yet").
DebugNotifyOncePrinter TheDebugNotifyOncePrinter; // ring: native-real

// Retail: MemInit() (MemMgr.cpp, `#ifndef HX_NATIVE`) does
// `gMemStackLock = new CriticalSection()`. Natively the engine owns the heap
// and MemMgr.cpp's HX_NATIVE arm defines gMemLock but not this one, while the
// shared ThreadMemStack/MemTemp bodies still name it. Null = no lock:
// CritSecTracker (os/CritSec.h) skips a null section, the same state those
// bodies see on retail before MemInit runs.
CriticalSection *gMemStackLock = nullptr; // ring: native-real

// Retail: ContentMgr_Xbox.cpp binds TheContentMgr to the Xbox content manager
// (DLC packages on the hard drive / marketplace). Natively there is no
// content storage: a base ContentMgr means no content packages, no refresh in
// flight, an on-disc-only session. Same binding bandtrack_link_stubs.cpp
// (rb3-render's phase stubs) has used since W16-TJ.
//
// A2: TitleContentPath. Retail ContentMgr_Xbox returns ContentPath(0), the
// mounted title-update package, and ArchiveInit (os/Archive.cpp) merges
// <path>/gen/patch_xbox.hdr ahead of the disc when it exists. Natively the
// title update is opt-in: RB3_TITLE_UPDATE_DIR names the directory that holds
// gen/patch_xbox.hdr + gen/patch_xbox_0.ark (the official TU5 package is
// /home/free/code/milohax/rb3-xenon/orig/45410914/tu5-package). Unset or
// empty = no title update, which is what the base class answers.
#include <cstdlib>
class Rb3AppContentMgr : public ContentMgr {
public:
    virtual const char *TitleContentPath() { // ring: native-real
        const char *dir = std::getenv("RB3_TITLE_UPDATE_DIR");
        return (dir && *dir) ? dir : nullptr;
    }
};
static Rb3AppContentMgr gRb3AppContentMgr;
ContentMgr &TheContentMgr = gRb3AppContentMgr; // ring: native-real

// NativeArchiveInit (platform/System_Native.cpp, every target) asks this weak
// hook; only rb3-app defines it. Non-null = run the retail ArchiveInit.
const char *Rb3NativeTitleContentPath() { return TheContentMgr.TitleContentPath(); } // ring: native-real

// Retail: WebSvcMgrCurl.cpp binds TheWebSvcMgr to the curl web-service
// manager; rb3-app excludes it (online ring, curl). An offline WebSvcMgr: the
// base class (net/WebSvcMgr.cpp, compiled here) with an empty request list, so
// SystemPoll's TheWebSvcMgr.Poll() walks nothing. The three transport hooks
// the base leaves pure are the online ring and abort if anything reaches them.
extern "C" [[noreturn]] void rb3_app_stub_reached(const char *ring, const char *fn);
namespace {
class Rb3AppOfflineWebSvcMgr : public WebSvcMgr {
public:
    bool DoRequest(ReqType, unsigned int, unsigned short, const char *, const char *,
                   unsigned int, const char *, unsigned int) override {
        rb3_app_stub_reached("online-websvc", "WebSvcMgr::DoRequest"); // ring: online-websvc
    }
    bool InitRequest(WebSvcRequest *, ReqType, const char *, unsigned short,
                     const char *, unsigned int) override {
        rb3_app_stub_reached("online-websvc", "WebSvcMgr::InitRequest(host)"); // ring: online-websvc
    }
    bool InitRequest(WebSvcRequest *, ReqType, unsigned int, unsigned short,
                     const char *, unsigned int) override {
        rb3_app_stub_reached("online-websvc", "WebSvcMgr::InitRequest(ip)"); // ring: online-websvc
    }
};
Rb3AppOfflineWebSvcMgr gRb3AppWebSvcMgr;
} // namespace
WebSvcMgr &TheWebSvcMgr = gRb3AppWebSvcMgr; // ring: native-real

// Retail: the XDK sends the string to the attached debugger (Debug::Print's
// last sink, os/Debug.cpp:133). Natively the debugger channel is stderr,
// unbuffered, the string as given (callers supply their own newlines).
extern "C" void OutputDebugStringA(const char *msg) { // ring: native-real
    if (msg)
        fputs(msg, stderr);
}

// Retail: src/ChecksumData_xbox.cpp registers two SHA-1 tables (1,065 game
// files, 62 song .mid files) that FileStream/BufStream then validate every
// listed file against; a mismatch raises the disk-error loop. rb3-app
// registers NEITHER table, so GetFileChecksum() returns null for every file
// and no file is validated: the same state retail is in for any file the
// tables do not list. Two measured reasons (lane A1, 2026-11-22):
//   1. the tables do not describe this disc: config/gen/macros.dtb read
//      straight out of main_xbox_0.ark (native/tools/ark_extract.py, and the
//      extracted copy) hashes 4611a568..., the table entry is fe866dce...;
//      with the tables registered the boot stops at "Checksum failure for file
//      config/gen/macros.dtb" and loops DISK ERROR;
//   2. natively CSHA1 is wrong on LP64/little-endian (SHA1.h `unsigned long
//      l[16]` overruns m_workspace into m_block: segfault in
//      CSHA1::Transform). Fix drafted, not landed: a1/sha1_lp64_fix.patch.
//
// A2 (2026-11-22) retires both reasons: (2) the CSHA1 fix landed (8b219ac4e)
// and the native CSHA1 matches sha1sum on macros.dtb (4611a568...), the
// community patch's macros.dtb and the TU5 patch_xbox.hdr, fed in uneven
// chunks (a2/sha1drv); (1) the tables describe TU5 (41 of 41 TU5 rows match
// the official package, a1b/scripts/checksum_xref.py). So the REAL registrars
// (src/ChecksumData_xbox.cpp) are now compiled in, and rb3_app_gates.cpp
// calls them only when a title update is mounted (RB3_TITLE_UPDATE_DIR); on
// the bare disc they are skipped with a [GATED] line, since 41 listed files
// would fail by measurement and stop the boot.

// Retail: SynthPreInit (synth/Synth.cpp) takes Synth::New(), the Xbox audio
// synth; its HX_NATIVE arm asks for CreateNativeSynth() instead. rb3-app has
// no audio device yet (A5 owns NativeSynth), so this returns the base null
// Synth: the object retail itself builds when config synth/use_null_synth is
// set, and the fallback SynthPreInit uses when a synth Fail()s. No sound plays.
Synth *CreateNativeSynth() { return new Synth(); } // ring: native-real

// Retail: TheMovieSys (movie/MovieSys.cpp) is bound to gBinkMovieSys, the
// Bink/XAudio movie system (moviebink/*_Xbox.cpp, not native). rb3-app gives
// that symbol a BASE MovieSys instead of the zero bytes A0 reserved (a null
// vtable: Movie::Init() segfaulted on it). Semantics: Init/Terminate are the
// base bodies (initialized flag, TexMovie::Init, exit callback), and every
// movie is a base MovieImpl, which never opens a file: no movie plays, and
// the splash movies are skipped. Only the MovieSys interface is ever used
// through TheMovieSys, never BinkMovieSys members. A2 owns real playback.
MovieSys gRb3AppMovieSys __asm__("gBinkMovieSys"); // ring: native-real

// Retail: BeatMatchInit (declared in beatmatch/BeatMatch.h) has no decompiled
// body: its one instruction, at 0x8276E210, sits inside the DataFlex.c split
// range (0x8276D858-0x8276E228), so no rb3-xenon TU owns it yet. The retail
// bytes are `4807CF18  b 0x827EB128`, a tail call to MidiParser::Init
// (tools/va_disasm.py 0x8276E210 8, lane A1b). This is that body, for rb3-app
// only, until a carve gives it a TU.
#include "beatmatch/BeatMatch.h"
#include "midi/MidiParser.h"
void BeatMatchInit() { MidiParser::Init(); } // ring: native-real

// XBDM's DmMapDevkitDrive maps the devkit: drive on a development kit and
// returns a failure HRESULT on a retail console, where xbdm is absent. A native
// host is not a devkit, so this is that failure. Natively it is reached from
// Locale::Init's HX_NATIVE devkit-locale-override arm (utl/Locale.cpp), which
// then reads the shipped locale config, exactly as retail always does.
#include "xdk/xbdm/xbdm.h"
extern "C" HRESULT DmMapDevkitDrive() { return (HRESULT)0x82DA0000; } // ring: native-real (XBDM_NOERR is 0x02DA0000; 0x82DA... = a failure facility code, < 0)

// Retail body, src/system/os/Memcard_Xbox.cpp:43 (MemcardMgr::Init calls it for
// each of the four pads). That TU is not in rb3-app: the rest of it is the
// XContent device/container layer, which natively is platform/Memcard_Stub.cpp
// (A4 replaces that). This one member is three stores, so it is copied
// verbatim rather than stubbed.
#include "os/Memcard_Xbox.h"
void ContainerId::Set(int idx, DWORD dw) { // ring: native-real
    mUserIndex = idx;
    unk8 = dw;
    mDeviceId = XCONTENTDEVICE_ANY;
}

// Win32 MultiByteToWideChar (xdk/xapilibi/stringapiset.h). Retail calls the XDK
// export (MemcardMgr::Init widens the localized title_name for
// SetContainerDisplayName). Natively there is no code-page table: each byte
// widens to the code unit of the same value (the Latin-1 view of CP_ACP),
// which is exact for the ASCII the game passes and is the inverse of
// rb3_app_quazal_native.cpp's WideCharToMultiByte convention. The Win32
// contract is kept: cbMultiByte -1 means "through the NUL, counted";
// cchWideChar 0 returns the size needed; a buffer too small returns 0.
#include "xdk/xapilibi/stringapiset.h"
#include <cstring>
extern "C" int MultiByteToWideChar(UINT, DWORD, LPCSTR src, int cb, LPWSTR dst, int cch) { // ring: native-real
    if (!src)
        return 0;
    int n = cb < 0 ? (int)std::strlen(src) + 1 : cb;
    if (cch == 0)
        return n;
    if (!dst || cch < n)
        return 0;
    for (int i = 0; i < n; i++)
        dst[i] = (unsigned char)src[i];
    return n;
}
