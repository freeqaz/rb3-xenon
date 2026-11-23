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
#include "os/ContentMgr_Xbox.h"
#include "net/SyncStore.h"
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

// Retail: TheContentMgr is the Xbox content manager, XboxContentMgr
// (os/ContentMgr_Xbox.cpp, compiled into rb3-app since lane postcal). Its
// refresh is what `content_mgr refresh_done` reports: StartRefresh (retail
// 0x82520FD0) adds the config's content roots ("." = the disc) and starts one
// cross-title enumeration per signed-in pad plus two device-wide ones;
// PollRefresh (0x82520C18) and the base ContentMgr::PollRefresh (0x8251F1E0)
// walk the callbacks over every root and reach mState 1, which RefreshDone
// (0x8251E510: `mState == 1`) reads. A base ContentMgr has an empty
// StartRefresh, so mState stayed 0 and meta_loading's kMetaLoading_Entered
// state, which waits on refresh_done, never advanced. The enumerations run
// over the xdk_shims.cpp content store (XContentCreateCrossTitleEnumerator).
// The one native override is TitleContentPath, below: retail answers
// ContentPath(0), the "UPDATE:" drive, which natively is a host directory.
//
// A2: TitleContentPath. Retail ContentMgr_Xbox returns ContentPath(0), the
// mounted title-update package, and ArchiveInit (os/Archive.cpp) merges
// <path>/gen/patch_xbox.hdr ahead of the disc when it exists.
//
// A4: the title update is mounted BY DEFAULT when the data is present. Our
// code is TU5 code (orig/45410914/default.xex is clean TU5) and it trips on
// disc-only data (docs/decomp/TU5_PATCH_DATA_VS_SOURCE_2026-11-22.md:
// Game::Properties wants TU5's allow_drum_fills, BandDirector handlers read
// arguments only TU5 scripts pass), so the bare disc is the degraded mode.
// Resolution, decided once and printed once:
//   RB3_TITLE_UPDATE_DIR=none   explicit opt-out: bare disc, no title update.
//   RB3_TITLE_UPDATE_DIR=<dir>  explicit override, used AS GIVEN: <dir> holds
//                               gen/patch_xbox.hdr + gen/patch_xbox_0.ark. A
//                               <dir> without the hdr is still "mounted" (so
//                               the TU5 checksum tables register over the
//                               bare disc: the A2 negative control), with a
//                               NOTIFY saying so.
//   unset or empty              auto-detect <checkout>/orig/45410914/
//                               tu5-package, probing the MAIN checkout first
//                               (git rev-parse --git-common-dir at configure
//                               time, so a worktree finds the main checkout's
//                               gitignored orig/), then this source tree.
//                               Found = both gen/patch_xbox.hdr and
//                               gen/patch_xbox_0.ark exist. Not found = one
//                               `NOTIFY rb3-app title update: NOT FOUND` line
//                               naming every path probed, then the bare disc.
//   RB3_TITLE_UPDATE_PROBE_ROOT=<root>  test hook: probe only
//                               <root>/orig/45410914/tu5-package instead of
//                               the configured checkouts (simulates absence).
// rb3-app only: this TU is linked into no other target, and the hook it
// feeds (Rb3NativeTitleContentPath, weak in System_Native.cpp) is undefined
// everywhere else, so every other target keeps the bare-disc archive path.
#include <cstdlib>
#include <cstring>
#include <string>
#include <sys/stat.h>
#ifndef RB3_APP_MAIN_CHECKOUT
#define RB3_APP_MAIN_CHECKOUT ""
#endif
#ifndef RB3_APP_SOURCE_CHECKOUT
#define RB3_APP_SOURCE_CHECKOUT ""
#endif
namespace {
bool Rb3AppIsFile(const std::string &p) {
    struct stat st;
    return stat(p.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

const char *Rb3AppResolveTitleUpdate() {
    static std::string sDir;
    static int sState = -1; // -1 undecided, 0 bare disc, 1 mounted
    if (sState >= 0)
        return sState ? sDir.c_str() : nullptr;
    sState = 0;
    const char *env = std::getenv("RB3_TITLE_UPDATE_DIR");
    if (env && std::strcmp(env, "none") == 0) {
        printf("rb3-app title update: OPTED OUT (RB3_TITLE_UPDATE_DIR=none): bare disc, "
               "TU5 checksum tables not registered\n");
        fflush(stdout);
        return nullptr;
    }
    if (env && *env) {
        sDir = env;
        sState = 1;
        if (Rb3AppIsFile(sDir + "/gen/patch_xbox.hdr")) {
            printf("rb3-app title update: OVERRIDE RB3_TITLE_UPDATE_DIR=%s\n", env);
        } else {
            printf("NOTIFY rb3-app title update: OVERRIDE RB3_TITLE_UPDATE_DIR=%s has no "
                   "gen/patch_xbox.hdr: nothing merges over the disc, but the TU5 checksum "
                   "tables ARE registered (they will fail on the disc's files)\n", env);
        }
        fflush(stdout);
        return sDir.c_str();
    }
    const char *tail = "/orig/45410914/tu5-package";
    std::string roots[2];
    int nroots = 0;
    const char *probeRoot = std::getenv("RB3_TITLE_UPDATE_PROBE_ROOT");
    if (probeRoot && *probeRoot) {
        roots[nroots++] = probeRoot;
    } else {
        if (*RB3_APP_MAIN_CHECKOUT)
            roots[nroots++] = RB3_APP_MAIN_CHECKOUT;
        if (*RB3_APP_SOURCE_CHECKOUT && roots[0] != RB3_APP_SOURCE_CHECKOUT)
            roots[nroots++] = RB3_APP_SOURCE_CHECKOUT;
    }
    std::string probed;
    for (int i = 0; i < nroots; i++) {
        std::string dir = roots[i] + tail;
        if (Rb3AppIsFile(dir + "/gen/patch_xbox.hdr")
            && Rb3AppIsFile(dir + "/gen/patch_xbox_0.ark")) {
            sDir = dir;
            sState = 1;
            printf("rb3-app title update: TU5 auto-detected at %s (opt out with "
                   "RB3_TITLE_UPDATE_DIR=none)\n", dir.c_str());
            fflush(stdout);
            return sDir.c_str();
        }
        if (!probed.empty())
            probed += ", ";
        probed += dir + "/gen/patch_xbox{.hdr,_0.ark}";
    }
    printf("NOTIFY rb3-app title update: NOT FOUND, booting the BARE DISC (TU5-only data "
           "such as modes.dta allow_drum_fills will be missing; TU5 checksum tables not "
           "registered). Probed: %s. Set RB3_TITLE_UPDATE_DIR=<dir> or =none\n",
           probed.empty() ? "(no checkout configured)" : probed.c_str());
    fflush(stdout);
    return nullptr;
}
} // namespace

class Rb3AppContentMgr : public XboxContentMgr {
public:
    virtual const char *TitleContentPath() { // ring: native-real
        return Rb3AppResolveTitleUpdate();
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
// synth (Synth360); its HX_NATIVE arm asks for CreateNativeSynth() instead.
// The engine's NativeSynth (platform/Synth_Stub.cpp) is excluded here for its
// DC3 header shape (native/CMakeLists.txt, MILO_ENGINE_DECOMP_PLATFORM_EXCLUDE),
// so this is the xenon-side replacement for it.
//
// A10: the base Synth's HX_NATIVE NewStream/NewBufStream build a real
// StandardStream (not retail base Synth's StreamNull), and StandardStream's
// channels come from StreamReceiver::New, i.e. StreamReceiver::sFactory. In
// retail that factory is registered by Synth360::Init (0x82B5DC78), which runs
// Synth::Init (0x82700270), SynthSample360::Init (0x82B6CAB0) and then
// StreamReceiver360::Init (0x82B6C688, the bl at 0x82B5DC94): one store,
// sFactory = New360Receiver (0x82B6C608). SynthInit (0x82701710) reaches it
// through TheSynth->Init() (vtable +0x58, 0x82701764). With the base Synth
// nothing registered it, and App::Run's first streamed sound (splash_screen,
// frame ~13) called through null in StreamReceiver::New (lane A9, gdb).
//
// So this Synth's Init does what Synth360::Init does for streams: the base
// Init, then the stream receiver factory, here the engine's
// StreamReceiverNative::Create (the receiver feeding AudioDevice), and the
// output device that receiver mixes into (the engine NativeSynth's Init does
// the same two). Headless, AudioDevice::Init opens nothing: rb3-app forces
// MILO_HEADLESS=1 (main_app.cpp), which AudioDevice::Init honours by skipping
// the device ("AudioDevice: skipped"); receivers are still created and fed,
// just never rendered. MILO_AUDIO=1 overrides that. SynthSample360::Init's
// native counterpart (sample voices) is not wired: no native SampleInst
// backend is in rb3-app yet. rb3_app_gates.cpp checks the factory after
// SynthInit.
#include "audio/AudioDevice.h"
#include "platform/StreamReceiver_Native.h"
#include "synth/StreamReceiver.h"
// ring-class: Rb3AppNativeSynth native-real
bool rb3_app_sung_mic_init();
Mic *rb3_app_sung_mic(int);
int rb3_app_sung_mic_count();
int rb3_app_sung_mic_next_available();
void rb3_app_sung_mic_capture(int, bool);
bool rb3_app_sung_mic_changed();
void rb3_app_sung_mic_reset_changed();
class Rb3AppNativeSynth : public Synth {
public:
    virtual void Init() override {
        Synth::Init();
        StreamReceiver::sFactory = StreamReceiverNative::Create; // as StreamReceiver360::Init 0x82B6C688
        AudioDevice::GetInstance().Init(44100); // the engine NativeSynth's rate
    }
    virtual void Terminate() override {
        AudioDevice::GetInstance().Terminate();
        Synth::Terminate();
    }
    // micwalk: RB3_APP_SUNG_MIC=1 (rb3_app_mic_native.cpp) gives this Synth one
    // connected mic, mic 0, singing the chart (micwalk2: RB3_APP_SUNG_MIC_COUNT=n gives
    // n mics, mic k singing part k); the base Synth's mic virtuals
    // (no mic connected, none available) stand while it is off. Retail's
    // Synth360 answers the same calls from MicManagerXbox.
    Rb3AppNativeSynth() { mSungMic = rb3_app_sung_mic_init(); }
    virtual Mic *GetMic(int idx) override {
        Mic *m = mSungMic ? rb3_app_sung_mic(idx) : nullptr;
        return m ? m : Synth::GetMic(idx);
    }
    virtual int GetNumConnectedMics() override { return mSungMic ? rb3_app_sung_mic_count() : 0; }
    virtual int GetNextAvailableMicID() const override {
        return mSungMic ? rb3_app_sung_mic_next_available() : -1;
    }
    virtual bool IsMicConnected(int id) const override {
        return mSungMic && id >= 0 && id < rb3_app_sung_mic_count();
    }
    virtual void CaptureMic(int id) override {
        if (mSungMic) rb3_app_sung_mic_capture(id, true);
    }
    virtual void ReleaseMic(int id) override {
        if (mSungMic) rb3_app_sung_mic_capture(id, false);
    }
    virtual bool DidMicsChange() const override { return mSungMic && rb3_app_sung_mic_changed(); }
    virtual void ResetMicsChanged() override {
        if (mSungMic) rb3_app_sung_mic_reset_changed();
    }
    bool mSungMic;
};
Synth *CreateNativeSynth() { return new Rb3AppNativeSynth(); } // ring: native-real

// Retail: TheMovieSys (movie/MovieSys.cpp) is bound to gBinkMovieSys, the
// Bink/XAudio movie system (moviebink/*_Xbox.cpp, not native). rb3-app gives
// that symbol a MovieSys instead of the zero bytes A0 reserved (a null
// vtable: Movie::Init() segfaulted on it). Init/Terminate are the base bodies
// (initialized flag, TexMovie::Init, exit callback). Only the MovieSys
// interface is ever used through TheMovieSys, never BinkMovieSys members.
//
// A17: every movie is an Rb3AppMovieImpl: retail RB3's Movie::Impl (the
// decompiled !HX_NATIVE arm of movie/Movie.cpp, TU5 0x82742C08-0x82746128)
// with no Bink decoder. rb3-app links none: the engine's FFmpegMovieImpl is
// excluded from this tree's engine build (native/CMakeLists.txt,
// MILO_ENGINE_DECOMP_PLATFORM_EXCLUDE). So BinkOpen is modelled as failing,
// and the rest is retail's own path for a movie that Bink cannot open:
//  * Begin (0x82745D90): a preloaded movie is read by a front FileLoader,
//    from the milo stream itself when that stream is cached (TexMovie::Load
//    passes it). That consumes the movie bytes a cached .milo embeds
//    (ui/startup/gen/startup_movie_keep.milo_xbox's splash.tmov carries
//    startup.bik), so the DirLoader resumes on the object's 0xADDEADDE
//    marker. The base MovieImpl MILO_FAILs on a cached stream instead.
//    A streamed (not preloaded) movie needs no bytes read before BinkOpen.
//  * CheckOpen (0x82745428): when the loader is done the buffer is kept as
//    the preload buffer, the streamed arm runs the `videos stream_begin`
//    script, and MovieOpen's BinkOpen fails: mBink stays null.
//  * IsOpen (0x82742F08) is mBink != null: false. Poll (0x82745628) returns
//    false once CheckOpen is done (`if (!mBink) return false`), so TexMovie
//    ends the movie and MoviePanel sends movie_done.
//  * End (0x82744BF0): runs `videos stream_end` when there is no preload
//    buffer, then releases the loader and the buffer.
//  * Save (Impl::Save): a cached save writes the preload buffer back.
// Not modelled, since they act on an open Bink: the shared frame buffers
// (SharedFinishOpen), the BIKi endian swap of the buffer before BinkOpen,
// pause/volume/frame queries, and the disc contention report.
// Real playback is the engine backend (FFmpegMovieImpl), and needs work there
// first: it ignores the stream, so it cannot play an embedded movie yet.
#include "movie/MovieImpl.h"
#include "obj/Data.h"
#include "os/File.h"
#include "os/System.h"
#include "utl/Loader.h"
#include "utl/MemMgr.h"
#include "utl/Str.h"
namespace {
void Rb3AppMovieScript(const char *name) { // ring: native-real
    DataArray *videos = SystemConfig()->FindArray("videos", false);
    if (videos) {
        DataArray *script = videos->FindArray(name, false);
        if (script)
            script->ExecuteScript(1, nullptr, nullptr, 1);
    }
}
} // namespace

class Rb3AppMovieImpl : public MovieImpl { // ring-class: Rb3AppMovieImpl native-real
public:
    Rb3AppMovieImpl()
        : mLoader(nullptr), mLoading(false), mPreloadBuf(nullptr), mPreloadBufLen(0) {}
    virtual ~Rb3AppMovieImpl() { End(); }
    virtual bool BeginFromFile(
        const char *file, float, bool, bool, bool preload, bool, int, BinStream *stream,
        LoaderPos
    ) {
        mFilename = FileMakePath(FileRoot(), file);
        if (preload) {
            const char *name = mFilename.c_str();
            mLoader = new FileLoader(
                name, name, kLoadFront, 0, true, true,
                stream && stream->Cached() ? stream : nullptr
            );
        }
        mLoading = true;
        return true;
    }
    virtual bool Ready() const { return mLoader ? mLoader->IsLoaded() : true; }
    virtual bool IsOpen() { return false; } // mBink: BinkOpen failed
    virtual bool IsLoading() { return mLoader != nullptr; }
    virtual bool CheckOpen(bool) {
        if (!mLoading)
            return false;
        if (mLoader) {
            if (!mLoader->IsLoaded())
                return true;
            mLoading = false;
            mPreloadBuf = mLoader->GetBuffer(nullptr);
            mPreloadBufLen = mLoader->GetSize();
            delete mLoader;
            mLoader = nullptr;
            if (!mPreloadBuf) {
                End();
                return false;
            }
        } else {
            mLoading = false;
            Rb3AppMovieScript("stream_begin");
        }
        return false; // MovieOpen: BinkOpen returns null
    }
    virtual bool Poll() { return CheckOpen(true); } // then `if (!mBink) return false`
    virtual void End() {
        mLoading = false;
        if (!mPreloadBuf)
            Rb3AppMovieScript("stream_end");
        delete mLoader;
        mLoader = nullptr;
        if (mPreloadBuf) {
            MemFree(mPreloadBuf);
            mPreloadBuf = nullptr;
        }
        mPreloadBufLen = 0;
    }
    virtual void Save(BinStream *bs) {
        if (bs->Cached()) {
            while (CheckOpen(false))
                TheLoadMgr.Poll();
            FileLoader::SaveData(*bs, mPreloadBuf, mPreloadBufLen);
        }
    }
    // Probes for rb3_app_check_tex_movie (rb3_app_gates.cpp).
    const char *PreloadBuf() const { return mPreloadBuf; }
    int PreloadBufLen() const { return mPreloadBufLen; }

private:
    FileLoader *mLoader;
    bool mLoading;
    char *mPreloadBuf;
    int mPreloadBufLen;
    String mFilename;
};

const char *Rb3AppMoviePreload(MovieImpl *impl, int *len) { // ring: native-real
    Rb3AppMovieImpl *m = dynamic_cast<Rb3AppMovieImpl *>(impl);
    if (!m)
        return nullptr;
    *len = m->PreloadBufLen();
    return m->PreloadBuf();
}

class Rb3AppMovieSys : public MovieSys { // ring-class: Rb3AppMovieSys native-real
public:
    virtual MovieImpl *CreateMovieImpl() { return new Rb3AppMovieImpl(); }
};
Rb3AppMovieSys gRb3AppMovieSys __asm__("gBinkMovieSys"); // ring: native-real

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

// ContainerId::Set (0x8252A828) and MemcardXbox::FindValidUnit (0x8252AA40)
// lived here while platform/Memcard_Stub.cpp stood in for the XContent
// device/container layer. rb3-app now compiles retail's
// src/system/os/Memcard_Xbox.cpp (native/CMakeLists.txt), which defines both.

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

// A3: RecursePatternInternal, the pattern walker under FileRecursePattern
// (MakeFileList, the framerate list, AssetMgr). The retail 360 body is in
// src/system/os/File.cpp:956, behind a scaffold-era `#ifndef HX_NATIVE`, so no
// native TU defines it (other native targets get a weak no-op from
// dta_link_stubs.s, which rb3-app does not link). This is that body, logic for
// logic: split at the first '&' or wildcard; with `recurse` and a '/' after the
// split, list the directories at this level (DirListCB into File.cpp's
// gDirList) and recurse into each with the rest of the pattern; otherwise
// enumerate the directory before the last separator with the whole pattern.
// FileEnumerate is the engine's native primitive. Only the matching-driven
// spellings of the retail source (temporaries, the two-arg substr) are dropped.
// Reached at boot from AssetMgr's ctor through VerifyAssets, which is a
// `MILO_DEBUG && HX_NATIVE` arm (retail fn_8256B740 makes no such call): a dev
// check rb3-app runs because native builds define MILO_DEBUG.
#include "os/File.h"
#include "utl/MakeString.h"
#include "utl/Str.h"
#include <vector>
extern std::vector<String> gDirList;
void DirListCB(const char *, const char *);
void RecursePatternInternal( // ring: native-real
    const char *pattern, void (*cb)(const char *, const char *), bool recurse, bool recurse_dirs
) {
    MILO_ASSERT(pattern && pattern[0], 0x5B8);
    String pttn(pattern);
    unsigned int ampPos = pttn.find_first_of("&", 0);
    unsigned int wildcardPos = pttn.find_first_of("?*", 0);
    int splitPos = ampPos == FixedString::npos ? (int)pttn.length() - 1 : (int)ampPos;
    if (wildcardPos != FixedString::npos && wildcardPos < (unsigned int)splitPos)
        splitPos = wildcardPos;
    if (recurse && ampPos == FixedString::npos) {
        int len = (int)pttn.length();
        int fwd = splitPos;
        while (fwd < len && pttn[fwd] != '/' && pttn[fwd] != '\\')
            fwd++;
        if (fwd == len) {
            recurse = false;
        } else {
            String subPattern = pttn.substr((unsigned int)fwd, (unsigned int)(len - fwd));
            pttn = pttn.substr(0, (unsigned int)fwd);
            RecursePatternInternal(pttn.c_str(), DirListCB, false, true);
            std::vector<String> dirs(gDirList);
            gDirList.clear();
            pttn = FileGetPath(pttn.c_str());
            for (unsigned int i = 0; i < dirs.size(); i++) {
                RecursePatternInternal(
                    MakeString("%s/%s%s", pttn.c_str(), dirs[i].c_str(), subPattern.c_str()),
                    cb, recurse, recurse_dirs
                );
            }
            return;
        }
    }
    int pos = splitPos;
    while (pos >= 0 && pttn[pos] != '/' && pttn[pos] != '\\')
        pos--;
    String dirStr = pos <= 0 ? String(".") : pttn.substr(0, (unsigned int)pos);
    FileEnumerate(dirStr.c_str(), cb, recurse, pttn.c_str(), recurse_dirs);
}

// ---- Net::Init, the idle offline part (lane A5) ----------------------------
// Called by the Net::Init gate (rb3_app_gates.cpp, which says what it skips).
// Each step is retail Net::Init's own statement (0x823E0648), in its order:
// the vcall SetName("net", ObjectDir::Main()), then `mSession = NetSession::New()`
// and `mSearcher = SessionSearcher::New()`. The bodies are the compiled retail
// TUs (NetSession_Xbox.cpp, SessionSearcher_Xbox.cpp, SessionSearcher.cpp).
#include "net/Net.h"
#include "net/Server_Xbox.h"
#include "obj/Dir.h"
#include "os/PlatformMgr.h"
#include "synth_xbox/Synth.h"
// rbx-net: TheServer is a reference to the static XboxServer (Server.cpp,
// compiled into rb3-app). Retail .data 0x82C6EB50 holds 0x82CBFD14, the object
// gXboxServer's dynamic initializer 0x82C3EC40 constructs; Net::Init reads it
// (`lwz r3, -0x14b0(r29)`, 0x823E06E4). No src/ TU defines the reference, so
// rb3-app defines it here.
// Retail 0x8251BE80 (PlatformMgr_Xbox.cpp): the id is written only when the
// pad has a XUID. XUserGetXUID answers from the xdk_shims.cpp console model
// (index 0 is a signed-in local profile with an offline XUID).
void PlatformMgr::GetOnlineID(int padNum, OnlineID *id) const { // ring: native-real
    XUID xuid;
    if (XUserGetXUID(padNum, &xuid) == 0) {
        id->SetXUID(xuid);
    }
}
extern XboxServer gXboxServer;
Server &TheServer = gXboxServer; // ring: native-real
extern "C" void rb3_app_net_init_offline(void *self, void **session, void **searcher, void **server) { // ring: native-real
    Net *net = static_cast<Net *>(self);
    net->SetName("net", ObjectDir::Main());
    // postcal: retail's `mSyncStore = new SyncStore()` (operator new(0xC) at
    // 0x823E06B0, ctor 0x823EA300 at 0x823E06C0), before NetSession::New. The
    // ctor registers the three sync messages with the NetMessage factory and
    // sets TheSyncStore; NetSync::SyncScreen (retail 0x8259CB48) polls it on
    // every leader-local screen sync. Natively Synchronizable::Publish does not
    // insert into it (Synchronize.cpp, HX_NATIVE arm), so the store is empty.
    net->mSyncStore = new SyncStore();
    net->mSession = NetSession::New();
    net->mSearcher = SessionSearcher::New();
    // rbx-net: retail's `TheServer.Init(); mServer = &TheServer;` (0x823E06E0-
    // 0x823E06FC), offline. TheServer.Init() is XboxServer::Init (0x823EC980);
    // of it this runs the first statement, Server::Init (0x823F5DD8: SetName
    // "server", the net/server config, mPlayerIDs zeroed) and the last, the
    // signin_changed sink on ThePlatformMgr. The Quazal LSP back-end setup in
    // between (SetAccessKey, the login filter, `new LSPBackEndServices`) has
    // no native body, so mBackEnd stays null. mLoginState stays 0 (the ctor's),
    // so IsConnected() is false, as retail with no LIVE connection, and
    // XboxServer::Poll touches nothing (no call contexts, not connected).
    TheServer.Server::Init();
    static Symbol signin_changed("signin_changed");
    ThePlatformMgr.AddSink(&TheServer, signin_changed);
    net->mServer = &TheServer;
    *session = net->mSession;
    *searcher = net->mSearcher;
    *server = net->mServer;
}
// A7: the offline subset of Net::Poll (retail 0x823E03A8): mSearcher->Poll()
// and, since rbx-net, mServer->Poll() (XboxServer::Poll, retail 0x823ED490),
// in retail's order. Why TheNetMessenger.Poll is skipped: the gate in
// rb3_app_gates.cpp.
//
// A23: and mSession->Poll(), retail's vcall on +0x2C at 0x823E03D0..0x823E03E0
// (XboxSession::Poll, 0x823F06F0). It is the only thing that starts a game:
// NetSession::StartGame -> BeginGameStartCountdown sets mGameState to
// kStartingGame and, for a local session, no start time; NetSession::Poll
// (0x823E67A8, called first by XboxSession::Poll at 0x823F0704) then sees
// mGameState@0x4C == 1 with a null mGameStartTime@0x60 and calls
// EnterInGameState (0x823E2810, the bl at 0x823E6828): offline
// (mOnlineEnabled@0x6C false) kInLocalGame and Handle(SyncStartGameMsg), which
// is what moves SyncGameStartPanel to its synced state and lets game_screen
// load. Skipping it parked every offline song in tv3_a_screen. The Quazal
// half of that poll is retail's too: mQNet is null offline (no session was
// ever made), and QuazalSession::Poll (0x823F2B80, QuazalSession.cpp,
// compiled since A23) frees nothing while no session is terminating.
extern "C" void rb3_app_net_poll_offline(void *self) { // ring: native-real
    Net *net = static_cast<Net *>(self);
    net->mSyncStore->Poll(); // retail 0x823E03CC: bl SyncStore::Poll (0x823E9918) on +0x3C
    net->mSession->Poll(); // retail 0x823E03D0..0x823E03E0: vtable slot 0 on +0x2C
    net->mSearcher->Poll();
    net->mServer->Poll();
}

// A23: XboxSession::Poll (0x823F06F0) reads each local session user's outgoing
// voice chat from TheXboxSynth (bl Synth360::GetChatData 0x82B5BC00 in its
// loop) and sends nothing while it returns <= 0. Retail TheXboxSynth is the
// Synth360 TheSynth, built by Synth360's ctor. Natively TheSynth is
// Rb3AppNativeSynth (above): there is no Synth360 and no XHV voice engine, so
// TheXboxSynth is this never-constructed, zeroed Synth360: its mMics@0xA8 is an
// empty vector, as a retail Synth360 holding no mics.
alignas(Synth360) static unsigned char gRb3AppXboxSynthStorage[sizeof(Synth360)];
Synth360 *TheXboxSynth = reinterpret_cast<Synth360 *>(gRb3AppXboxSynthStorage); // ring: native-real
// Retail 0x82B5BC00: `if (!mMics.empty()) return MicManagerXbox::GetInstance()
// ->GetChatData(pad, buf, max); return -1;`. TheXboxSynth holds no mics
// natively (see above), so this is retail's empty-mics arm.
int Synth360::GetChatData(int, void *, int) { // ring: native-real
    MILO_ASSERT(mMics.empty(), 0);
    return -1;
}
// Lane dropwalk: retail 0x82B5BBA8 is `if (!mMics.empty())
// MicManagerXbox::GetInstance()->ActivateLocalChat(pad, active);` (lwz 0xA8/0xAC,
// cmplw, beq past both calls). XboxSession::RemoveLocalFromSession (0x823EEEF8)
// calls it unconditionally, so every local user leaving the session -- the
// overshell's "leave band" (OvershellSlot::RemoveUser -> SessionMgr ->
// NetSession::RemoveLocalUser), in song or on a menu -- reached the aborting
// online-netsession stub. TheXboxSynth holds no mics natively (above), so this is
// retail's empty-mics arm: nothing.
void Synth360::ActivateLocalChat(int, bool) { // ring: native-real
    MILO_ASSERT(mMics.empty(), 0);
}
