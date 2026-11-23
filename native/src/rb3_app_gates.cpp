// rb3-app boot gates (lane A1, 2026-11-22). Native only, rb3-app only.
//
// One `  [PASS] rb3-app <step>` line (the format tools/native_health.sh counts)
// each time an App::App init step RETURNS, with no src/ edit: every step below
// is routed through a link-time `-Wl,--wrap=<mangled>` (native/CMakeLists.txt),
// whose wrapper calls the real function and then prints. A step that crashes or
// reaches an aborting stub prints nothing, so the last [PASS] line names the
// last step that completed. The wrapper adds no behaviour: same arguments, same
// (void) result. Member functions take `this` as the first argument, which is
// the Itanium ABI for a non-virtual direct call. Generated from App.cpp.o's
// undefined symbols; the list is rb3_app_gates.txt.
#include <cstring>
#include "utl/MakeString.h"
#include "net/Net.h"
#include "os/ContentMgr_Xbox.h"
#include "net/SyncStore.h"
#include <cstdio>
#include <unistd.h>
#include "synth/StreamReceiver.h"
#include "platform/StreamReceiver_Native.h"
#include "obj/Data.h"
#include "obj/Object.h"
#include "os/Memcard.h"
#include "os/Memcard_Xbox.h"
#include "world/SpotlightDrawer.h"
#include "os/ProfilePicture.h"
#include "rndobj/Tex.h"
#include "ui/UI.h"
#include "ui/UIScreen.h"
#include "obj/Msg.h"
#include <new>

extern "C" void rb3_app_gate(const char *step) { // ring: native-real
    printf("  [PASS] rb3-app %s\n", step);
    fflush(stdout);
}

// A10: after SynthInit, StreamReceiver::New must have a factory. Retail
// SynthInit (0x82701710) calls TheSynth->Init() (vtable +0x58, 0x82701764);
// TheSynth is Synth::New()'s Synth360, whose Init (0x82B5DC78) runs Synth::Init
// (0x82700270), SynthSample360::Init (0x82B6CAB0), then StreamReceiver360::Init
// (0x82B6C688, the bl at 0x82B5DC94), which stores New360Receiver (0x82B6C608)
// into StreamReceiver::sFactory (0x82E04ACC). The native equivalent is the
// engine's StreamReceiverNative::Create. Unset, the first streamed sound
// (splash_screen, frame ~13 of App::Run) calls through null in
// StreamReceiver::New. A failure stops the boot here, rc 7, rather than there.
static void rb3_app_check_stream_receiver_factory() { // ring: native-real
    StreamReceiverFactoryFunc *f = StreamReceiver::sFactory;
    if (f == &StreamReceiverNative::Create) {
        rb3_app_gate("SynthInit() set StreamReceiver::sFactory = StreamReceiverNative::Create "
                     "(retail Synth360::Init 0x82B5DC78 -> StreamReceiver360::Init 0x82B6C688)");
        return;
    }
    printf("  [FAIL] rb3-app SynthInit() left StreamReceiver::sFactory = %s, not "
           "StreamReceiverNative::Create (retail Synth360::Init 0x82B5DC78 -> "
           "StreamReceiver360::Init 0x82B6C688 sets it)\n",
           f ? "another function" : "NULL");
    printf("RESULT: GATE FAILED (stream receiver factory)\n");
    fflush(stdout);
    fflush(stderr);
    _exit(7);
}

// memcard lane: SaveLoadManager state 8 (kS_AutoloadSetDevice, the "no save
// found" dialog's A press) calls TheMemcardMgr.SelectDevice, which calls
// TheMC.ShowDeviceSelector(..., this, pad), and sets mWaiting; only
// OnMsg(DeviceChosenMsg) (-> 0xB kS_AutoloadStartLoad) or
// OnMsg(NoDeviceChosenMsg) moves it on. Retail MemcardXbox::ShowDeviceSelector
// (0x8252B9A8) zeroes the XOVERLAPPED, calls XShowDeviceSelectorUI (bl at
// 0x8252BA2C) and keeps the request pending only on ERROR_IO_PENDING (0x3E5,
// 0x8252BA30); MemcardXbox::Poll (0x8252B858) fires once InternalLow is no
// longer 0x3E5 (0x8252B880): device_chosen with the device, no_device_chosen
// for DeviceID 0. This replays that handshake with a probe in the callback
// slot, so it moves no game state (retail resets every field it touches on
// the next ShowDeviceSelector). It must see device_chosen 1, the hard drive of
// xdk_shims.cpp, within a few polls; the old platform/Memcard_Stub.cpp never
// answered, which left state 8 waiting for good.
namespace {
class Rb3AppDeviceSelectorProbe : public Hmx::Object {
public:
    int mChosen = -2; // -2 nothing yet, -1 no_device_chosen, else the DeviceID
    int mMessages = 0;
    virtual DataNode Handle(DataArray *msg, bool) {
        mMessages++;
        if (msg && msg->Size() >= 2 && msg->Type(1) == kDataSymbol) {
            Symbol s = msg->Sym(1);
            if (s == DeviceChosenMsg::Type()) mChosen = msg->Int(2);
            else if (s == NoDeviceChosenMsg::Type()) mChosen = -1;
        }
        return DataNode(kDataInt, 0);
    }
};
}

static void rb3_app_check_device_selector() { // ring: native-real
    Rb3AppDeviceSelectorProbe probe;
    ContainerId cid;
    cid.Set(0, 0);
    TheMC.ShowDeviceSelector(cid, false, &probe, 0);
    int polls = 0;
    for (; polls < 8 && probe.mChosen == -2; polls++) TheMC.Poll();
    if (probe.mChosen == 1 && probe.mMessages == 1) {
        printf("  [PASS] rb3-app TheMC.ShowDeviceSelector answered device_chosen 1 after %d "
               "Poll (SaveLoadManager state 8 kS_AutoloadSetDevice -> 0xB kS_AutoloadStartLoad; "
               "retail MemcardXbox::ShowDeviceSelector 0x8252B9A8, Poll 0x8252B858)\n",
               polls);
        fflush(stdout);
        return;
    }
    printf("  [FAIL] rb3-app TheMC.ShowDeviceSelector answered %s after %d Poll (%d messages); "
           "SaveLoadManager state 8 kS_AutoloadSetDevice would not reach 0xB (retail "
           "MemcardXbox::ShowDeviceSelector 0x8252B9A8 + Poll 0x8252B858 deliver "
           "device_chosen <DeviceID>)\n",
           probe.mChosen == -2   ? "nothing"
           : probe.mChosen == -1 ? "no_device_chosen"
                                 : "device_chosen <not the hard drive>",
           polls, probe.mMessages);
    printf("RESULT: GATE FAILED (device selector)\n");
    fflush(stdout);
    fflush(stderr);
    _exit(7);
}

// profpic lane: a returning boot loads the saved profile, and
// BandProfile::LoadSaved calls ProfilePicture::Update for a signed-in pad;
// ProfileMgr::Poll -> BandProfile::Poll -> ProfilePicture::Poll (0x82B902F0)
// then runs the fetch. Retail FetchUserPicture (0x82B903B8) creates a 64x64
// D3D texture before it reads (bl D3DDevice_CreateTexture at 0x82B9040C),
// locks it, and keeps the XUserReadGamerPicture read (0x82B9047C) only on
// ERROR_IO_PENDING; a null texture skips the read without Fail (0x82B90428).
// ReceiveUserPicture (0x82B904E0) copies the pixels into a new RndTex, releases
// the D3D texture and the XOVERLAPPED, and Succeed sends
// profile_picture_fetched_msg true. This drives one ProfilePicture for pad 0
// (signed in locally in xdk_shims.cpp's console model) through that path with
// a probe as its callback and checks each step: a texture and a read in
// flight after the first Poll, then exactly one message (true), kComplete, a
// 64x64 32 bpp mUserPicture, and every D3D texture released. Before this lane
// the first Poll aborted in the D3DDevice_CreateTexture stub. The probe's
// ProfilePicture is its own; its destructor releases the picture.
extern "C" int rb3_app_d3d_live_textures();
namespace {
class Rb3AppProfilePictureProbe : public Hmx::Object {
public:
    int mResult = -1; // -1 nothing yet, else the message's bool
    int mMessages = 0;
    virtual DataNode Handle(DataArray *msg, bool) {
        mMessages++;
        if (msg && msg->Size() >= 3 && msg->Type(1) == kDataSymbol
            && msg->Sym(1) == ProfilePictureFetchedMsg::Type())
            mResult = msg->Int(2) != 0;
        return DataNode(kDataInt, 0);
    }
};
}

static void rb3_app_profile_picture_fail(const char *what) { // ring: native-real
    printf("  [FAIL] rb3-app ProfilePicture fetch for pad 0: %s (retail FetchUserPicture "
           "0x82B903B8 creates the texture and reads into it; ReceiveUserPicture 0x82B904E0 "
           "builds the RndTex and Succeed sends profile_picture_fetched_msg true)\n",
           what);
    printf("RESULT: GATE FAILED (profile picture)\n");
    fflush(stdout);
    fflush(stderr);
    _exit(7);
}

static void rb3_app_check_profile_picture() { // ring: native-real
    Rb3AppProfilePictureProbe probe;
    const int liveBefore = rb3_app_d3d_live_textures();
    char why[256];
    {
        ProfilePicture pic(0, &probe);
        pic.Update(); // kIdle -> kFetchingUserData (FetchUserData is empty)
        pic.Poll();   // ReceiveUserData is true -> kFetchingUserPicture, FetchUserPicture
        if (pic.mState != ProfilePicture::kFetchingUserPicture || !pic.mTexture
            || !pic.mOverlapped) {
            snprintf(why, sizeof why,
                     "after the first Poll: state %d, texture %s, read %s, %d messages "
                     "(want state 2 with a texture and a read in flight)",
                     (int)pic.mState, pic.mTexture ? "created" : "NULL",
                     pic.mOverlapped ? "in flight" : "not started", probe.mMessages);
            rb3_app_profile_picture_fail(why);
        }
        int polls = 1;
        for (; polls < 8 && probe.mMessages == 0; polls++) pic.Poll();
        RndTex *t = pic.mUserPicture;
        const int live = rb3_app_d3d_live_textures();
        if (probe.mMessages != 1 || probe.mResult != 1 || pic.mState != ProfilePicture::kComplete
            || !t || t->Width() != 64 || t->Height() != 64 || t->Bpp() != 32 || pic.mTexture
            || pic.mOverlapped || live != liveBefore) {
            snprintf(why, sizeof why,
                     "after %d Poll: %d messages (result %d), state %d, picture %s %dx%d %d bpp, "
                     "texture %s, read %s, %d live D3D textures (was %d)",
                     polls, probe.mMessages, probe.mResult, (int)pic.mState, t ? "set" : "NULL",
                     t ? t->Width() : 0, t ? t->Height() : 0, t ? t->Bpp() : 0,
                     pic.mTexture ? "still held" : "released",
                     pic.mOverlapped ? "still held" : "released", live, liveBefore);
            rb3_app_profile_picture_fail(why);
        }
        printf("  [PASS] rb3-app ProfilePicture fetch for pad 0 answered "
               "profile_picture_fetched_msg true after %d Poll: a 64x64 32 bpp picture, the "
               "D3D texture released (retail FetchUserPicture 0x82B903B8, ReceiveUserPicture "
               "0x82B904E0)\n",
               polls);
        fflush(stdout);
    }
}

extern "C" void __real__Z10SystemInitPKc(const char * a0);
extern "C" void __wrap__Z10SystemInitPKc(const char * a0) { // ring: native-real
    __real__Z10SystemInitPKc(a0);
    rb3_app_gate("SystemInit(char const*)");
}

extern "C" void __real__Z12SynthPreInitv(void);
extern "C" void __wrap__Z12SynthPreInitv(void) { // ring: native-real
    __real__Z12SynthPreInitv();
    rb3_app_gate("SynthPreInit()");
}

extern "C" void __real__Z13BeatMatchInitv(void);
extern "C" void __wrap__Z13BeatMatchInitv(void) { // ring: native-real
    __real__Z13BeatMatchInitv();
    rb3_app_gate("BeatMatchInit()");
}

extern "C" void __real__Z13SystemPreInitiPPcPKc(int a0, char ** a1, const char * a2);
extern "C" void __wrap__Z13SystemPreInitiPPcPKc(int a0, char ** a1, const char * a2) { // ring: native-real
    __real__Z13SystemPreInitiPPcPKc(a0, a1, a2);
    rb3_app_gate("SystemPreInit(int, char**, char const*)");
}

extern "C" void __real__Z15BandUserMgrInitv(void);
extern "C" void __wrap__Z15BandUserMgrInitv(void) { // ring: native-real
    __real__Z15BandUserMgrInitv();
    rb3_app_gate("BandUserMgrInit()");
}

extern "C" void __real__Z15EnableKeyCheatsb(bool a0);
extern "C" void __wrap__Z15EnableKeyCheatsb(bool a0) { // ring: native-real
    __real__Z15EnableKeyCheatsb(a0);
    rb3_app_gate("EnableKeyCheats(bool)");
}

extern "C" void __real__Z18ContextCheckerInitv(void);
extern "C" void __wrap__Z18ContextCheckerInitv(void) { // ring: native-real
    __real__Z18ContextCheckerInitv();
    rb3_app_gate("ContextCheckerInit()");
}

extern "C" void __real__Z8BandInitv(void);
extern "C" void __wrap__Z8BandInitv(void) { // ring: native-real
    __real__Z8BandInitv();
    rb3_app_gate("BandInit()");
}

extern "C" void __real__Z8CharInitv(void);
extern "C" void __wrap__Z8CharInitv(void) { // ring: native-real
    __real__Z8CharInitv();
    rb3_app_gate("CharInit()");
}

extern "C" void __real__Z8GameInitv(void);
extern "C" void __wrap__Z8GameInitv(void) { // ring: native-real
    __real__Z8GameInitv();
    rb3_app_gate("GameInit()");
}

extern "C" void __real__Z9SynthInitv(void);
extern "C" void __wrap__Z9SynthInitv(void) { // ring: native-real
    __real__Z9SynthInitv();
    rb3_app_gate("SynthInit()");
    rb3_app_check_stream_receiver_factory();
}

extern "C" void __real__Z9TrackInitv(void);
extern "C" void __wrap__Z9TrackInitv(void) { // ring: native-real
    __real__Z9TrackInitv();
    rb3_app_gate("TrackInit()");
}

// A15: after WorldInit, SpotlightDrawer::DeSelect must switch back to the
// default drawer. Retail WorldInit (World.cpp) runs SpotlightDrawer::Init, which
// makes sDefault and Select()s it. Retail's out-of-line DeSelect (0x824D4C90,
// 0x2C bytes; WorldDir::DrawShowing calls it every frame) returns unless this
// is sCurrent, returns if it is sDefault, else calls sDefault->Select(). The
// call below is from outside world/SpotlightDrawer.cpp, which defines the body
// __forceinline, so it links whatever out-of-line DeSelect rb3-app carries, the
// same one world/Dir.cpp gets. Not an rb3-milo gate: rb3-milo links
// SpotlightDrawer, but its TheRnd is zero-filled storage, so Select()'s
// RegisterPostProcessor faults there. A failure stops the boot here, rc 7.
static void rb3_app_check_spotlight_deselect() { // ring: native-real
    SpotlightDrawer *def = SpotlightDrawer::Current();
    SpotlightDrawer *other = def ? Hmx::Object::New<SpotlightDrawer>() : nullptr;
    bool selected = false;
    SpotlightDrawer *after = def;
    if (other) {
        other->Select();
        selected = SpotlightDrawer::Current() == other;
        other->DeSelect();
        after = SpotlightDrawer::Current();
    }
    if (def && selected && after == def) {
        delete other;
        rb3_app_gate("WorldInit(): Select() of another SpotlightDrawer, then DeSelect(), "
                     "selects the default drawer again (retail DeSelect 0x824D4C90)");
        return;
    }
    printf("  [FAIL] rb3-app SpotlightDrawer::DeSelect: default %p, Select(other %p) -> current "
           "%s, DeSelect() -> current %p, not the default (retail 0x824D4C90 calls "
           "sDefault->Select())\n",
           (void *)def, (void *)other, selected ? "other" : "NOT other", (void *)after);
    printf("RESULT: GATE FAILED (spotlight drawer deselect)\n");
    fflush(stdout);
    fflush(stderr);
    _exit(7);
}

extern "C" void __real__Z9WorldInitv(void);
extern "C" void __wrap__Z9WorldInitv(void) { // ring: native-real
    __real__Z9WorldInitv();
    rb3_app_gate("WorldInit()");
    rb3_app_check_spotlight_deselect();
}

extern "C" void __real__ZN11AuditionMgr4InitEv(void *self);
extern "C" void __wrap__ZN11AuditionMgr4InitEv(void *self) { // ring: native-real
    __real__ZN11AuditionMgr4InitEv(self);
    rb3_app_gate("AuditionMgr::Init()");
}

extern "C" void __real__ZN11RockCentral4InitEb(void *self, bool a0);
// GATED, not called (lane A1b). TheRockCentral is zero storage in rb3-app
// (rb3_app_link_stubs.cpp, ring online-rockcentral) and RockCentral::Init is an
// aborting stub: retail connects the Rock Central web service here. Skipped
// LOUDLY; every other RockCentral entry point is still an aborting stub.
extern "C" void __wrap__ZN11RockCentral4InitEb(void *self, bool a0) { // ring: online-rockcentral
    (void)self;
    (void)a0;
    printf("  [GATED] rb3-app RockCentral::Init(bool) -- ring online-rockcentral: no web service natively, not called\n");
    fflush(stdout);
}

// A7: RockCentral::Poll (App::RunWithoutDebugging, every frame; RockCentral.cpp
// :174) is GATED, not called, for the same reason as Init above: it polls
// mContextWrapperPool (ContextWrapper.cpp, Quazal protocol contexts; aborting
// stub ContextWrapperPool::Poll) and, when connected, TheNet's Quazal server.
// RockCentral::Init never ran, so there is nothing to poll. GATED once.
extern "C" void __wrap__ZN11RockCentral4PollEv(void *self) { // ring: online-rockcentral
    (void)self;
    static bool sPrinted = false;
    if (!sPrinted) {
        sPrinted = true;
        printf("  [GATED] rb3-app RockCentral::Poll() -- ring online-rockcentral: Init not run, not called\n");
        fflush(stdout);
    }
}

extern "C" void __real__ZN11TrainingMgr4InitEv(void);
extern "C" void __wrap__ZN11TrainingMgr4InitEv(void) { // ring: native-real
    __real__ZN11TrainingMgr4InitEv();
    rb3_app_gate("TrainingMgr::Init()");
}

extern "C" void __real__ZN13UsbMidiGuitar4InitEv(void);
extern "C" void __wrap__ZN13UsbMidiGuitar4InitEv(void) { // ring: native-real
    __real__ZN13UsbMidiGuitar4InitEv();
    rb3_app_gate("UsbMidiGuitar::Init()");
}

extern "C" void __real__ZN14GameMicManager4InitEv(void);
extern "C" void __wrap__ZN14GameMicManager4InitEv(void) { // ring: native-real
    __real__ZN14GameMicManager4InitEv();
    rb3_app_gate("GameMicManager::Init()");
}

extern "C" void __real__ZN15SaveLoadManager4InitEv(void);
extern "C" void __wrap__ZN15SaveLoadManager4InitEv(void) { // ring: native-real
    __real__ZN15SaveLoadManager4InitEv();
    rb3_app_gate("SaveLoadManager::Init()");
    rb3_app_check_device_selector();
}

extern "C" void __real__ZN15UsbMidiKeyboard4InitEv(void);
extern "C" void __wrap__ZN15UsbMidiKeyboard4InitEv(void) { // ring: native-real
    __real__ZN15UsbMidiKeyboard4InitEv();
    rb3_app_gate("UsbMidiKeyboard::Init()");
}

extern "C" void __real__ZN16AutoGlitchReport14EnableCallbackEv(void);
extern "C" void __wrap__ZN16AutoGlitchReport14EnableCallbackEv(void) { // ring: native-real
    __real__ZN16AutoGlitchReport14EnableCallbackEv();
    rb3_app_gate("AutoGlitchReport::EnableCallback()");
}

extern "C" void __real__ZN17FixedSizeSaveable4InitEii(int a0, int a1);
extern "C" void __wrap__ZN17FixedSizeSaveable4InitEii(int a0, int a1) { // ring: native-real
    __real__ZN17FixedSizeSaveable4InitEii(a0, a1);
    rb3_app_gate("FixedSizeSaveable::Init(int, int)");
}


// postcal: TheContentMgr is retail's XboxContentMgr and its refresh completes.
// RefreshDone (0x8251E510) is `mState == 1`; meta_loading's
// kMetaLoading_Entered state waits on `content_mgr refresh_done`. The probe is
// its own XboxContentMgr (static storage: XboxContentMgr's ctor leaves its
// fields to the static-init zero fill, as retail's gContentMgr has), never
// Init'd (Init would take the "content_mgr" name) and with no callbacks, so
// no game state changes. It runs retail's StartRefresh (0x82520FD0): the
// config's roots plus the cross-title enumerations over the content store,
// then PollRefresh (0x82520C18 + base 0x8251F1E0) until RefreshDone. Needed:
// TheContentMgr is an XboxContentMgr, the refresh is done within 16 polls,
// it holds the one disc root and no package (the store's saved games are on
// PollRefresh's ignored list), and every enumeration buffer was released.
namespace {
struct Rb3AppContentRefreshProbe : public XboxContentMgr {
    void Run(int &polls, bool &done, int &contents, int &state) {
        mDirty = true;
        mState = kDone;
        mRootLoaded = 0;
        StartRefresh();
        for (polls = 0; polls < 16 && !RefreshDone(); polls++) PollRefresh();
        done = RefreshDone();
        contents = (int)mContents.size();
        state = (int)mState;
        DeleteAll(mContents);
    }
};
Rb3AppContentRefreshProbe gRb3AppContentProbe;
} // namespace
extern "C" int rb3_app_xdk_live_enum_handles();
static void rb3_app_check_content_refresh() { // ring: native-real
    bool isXbox = dynamic_cast<XboxContentMgr *>(&TheContentMgr) != nullptr;
    int enumsBefore = rb3_app_xdk_live_enum_handles();
    int polls = 0, contents = -1, state = -1;
    bool done = false;
    gRb3AppContentProbe.Run(polls, done, contents, state);
    int enumsAfter = rb3_app_xdk_live_enum_handles();
    if (!isXbox || !done || contents != 1 || enumsAfter != enumsBefore) {
        printf("  [FAIL] rb3-app content refresh: TheContentMgr %s an XboxContentMgr; "
               "after %d PollRefresh: state %d, refresh_done %d, %d contents, live enumerators "
               "%d -> %d\n",
               isXbox ? "is" : "is NOT", polls, state, (int)done, contents, enumsBefore, enumsAfter);
        printf("RESULT: GATE FAILED (content refresh)\n");
        fflush(stdout);
        _exit(7);
    }
    printf("  [PASS] rb3-app content refresh: XboxContentMgr refresh_done after %d PollRefresh "
           "(state 1, 1 root, 0 packages, enumerators released)\n", polls);
    fflush(stdout);
}

// postcal: the offline Net::Init built TheNet.mSyncStore (retail `new
// SyncStore`, 0x823E06C0) and it is TheSyncStore; NetSync::SyncScreen (retail
// 0x8259CB48) polls it on a leader-local screen sync.
static void rb3_app_check_sync_store(void *net) { // ring: native-real
    SyncStore *store = static_cast<Net *>(net)->mSyncStore;
    if (!store || store != TheSyncStore) {
        printf("  [FAIL] rb3-app SyncStore: TheNet.mSyncStore=%p TheSyncStore=%p\n", (void *)store,
               (void *)TheSyncStore);
        printf("RESULT: GATE FAILED (sync store)\n");
        fflush(stdout);
        _exit(7);
    }
    store->Poll();
    printf("  [PASS] rb3-app SyncStore: TheNet.mSyncStore is TheSyncStore and polls\n");
    fflush(stdout);
}

extern "C" void __real__ZN3Net4InitEv(void *self);
// NARROWED (lanes A3, A5). Retail Net::Init (src/network/net/Net.cpp, 0x823E0648)
// runs on an offline 360 too, in this order: SetName("net", ObjectDir::Main()),
// the two Quazal allocator hooks and the system-check hook, `new SyncStore`,
// `mSession = NetSession::New()` (= new XboxSession, NetSession_Xbox.cpp; sets
// TheNetSession; kIdle, online disabled), `mSearcher = SessionSearcher::New()`
// (= new XSessionSearcher, 0x823EE140), TheServer.Init() and mServer, a
// NetworkEmulator, TheNetMessenger.Init(), and the TerminateTheNet exit callback.
// Since A5 TheNet is the real object (Net.cpp is compiled into rb3-app), and this
// gate runs, in retail's order, the steps that touch no network:
//   SetName, (postcal) mSyncStore = new SyncStore(), mSession = NetSession::New(),
//   mSearcher = SessionSearcher::New(),
//   and (rbx-net) TheServer's offline Init and mServer = &TheServer
// (rb3_app_net_init_offline in rb3_app_native_glue.cpp; each a retail body).
// NOT run, as the printed line says:
//   * Quazal hooks: there is no Quazal stack natively to read them;
//   * XboxServer::Init's Quazal LSP back end (mBackEnd stays null; the server is
//     offline, IsConnected() false), NetworkEmulator (Quazal emulation
//     devices), TheNetMessenger.Init (NetMessenger.cpp absent): networked;
//   * the exit callback: Net::Terminate calls TheServer.Terminate, which would
//     call through the null mBackEnd at exit.
// So TheNet.mEmulator stays null. Net.cpp's own Init is
// not called. Printed as GATED, not PASS, because it is not retail's whole call.
extern "C" void rb3_app_net_init_offline(void *net, void **session, void **searcher, void **server);
extern "C" void __wrap__ZN3Net4InitEv(void *self) { // ring: online-net
    void *session = nullptr, *searcher = nullptr, *server = nullptr;
    rb3_app_net_init_offline(self, &session, &searcher, &server);
    printf("  [GATED] rb3-app Net::Init() -- ring online-net: built ONLY the idle XboxSession "
           "(TheNetSession=%p), XSessionSearcher (TheNet.mSearcher=%p) and the offline XboxServer "
           "(TheNet.mServer=%p, no LSP back end) and the SyncStore (TheSyncStore=%p); Quazal "
           "hooks/NetworkEmulator/NetMessenger/exit callback not built\n",
           session, searcher, server, (void *)TheSyncStore);
    fflush(stdout);
    rb3_app_check_sync_store(self);
    rb3_app_check_content_refresh();
}

// A7: Net::Poll (App::RunWithoutDebugging, every frame). Retail 0x823E03A8:
// TheNetMessenger.Poll() (0x823F5460), mSyncStore->Poll(), mSession->Poll(),
// mSearcher->Poll(), mServer->Poll() (vtable +8). The Net::Init gate above built
// mSession, mSearcher and (rbx-net) the offline mServer. This gate runs
// mSyncStore->Poll() (postcal), (A23) mSession->Poll(), mSearcher->Poll() and
// mServer->Poll() (rb3_app_net_poll_offline; the retail SyncStore,
// XboxSession, XSessionSearcher and XboxServer bodies; offline the session has
// no Quazal session and XboxServer::Poll has no call context to poll and is not
// connected) and NOT:
//   * TheNetMessenger.Poll: NetMessenger.cpp is absent from src/.
// A23: the session poll is what enters the game (NetSession::Poll ->
// EnterInGameState -> SyncStartGameMsg); rb3_app_check_offline_game_start
// checks it. QuazalSession::Poll is QuazalSession.cpp's retail body.
// GATED once, on the first frame.
extern "C" void rb3_app_net_poll_offline(void *net);
extern "C" void __wrap__ZN3Net4PollEv(void *self) { // ring: online-net
    static bool sPrinted = false;
    if (!sPrinted) {
        sPrinted = true;
        printf("  [GATED] rb3-app Net::Poll() -- ring online-net: polls ONLY mSyncStore, "
               "the session, mSearcher and the offline mServer; NetMessenger not polled\n");
        fflush(stdout);
    }
    rb3_app_net_poll_offline(self);
}

extern "C" void __real__ZN5Movie4InitEv(void);
extern "C" void __wrap__ZN5Movie4InitEv(void) { // ring: native-real
    __real__ZN5Movie4InitEv();
    rb3_app_gate("Movie::Init()");
}

extern "C" void __real__ZN8AssetMgr4InitEv(void);
extern "C" void __wrap__ZN8AssetMgr4InitEv(void) { // ring: native-real
    __real__ZN8AssetMgr4InitEv();
    rb3_app_gate("AssetMgr::Init()");
}

extern "C" void __real__ZN8CharSync4InitEP11BandUserMgr(void * a0);
extern "C" void __wrap__ZN8CharSync4InitEP11BandUserMgr(void * a0) { // ring: native-real
    __real__ZN8CharSync4InitEP11BandUserMgr(a0);
    rb3_app_gate("CharSync::Init(BandUserMgr*)");
}

extern "C" void __real__ZN8PatchDir4InitEv(void);
extern "C" void __wrap__ZN8PatchDir4InitEv(void) { // ring: native-real
    __real__ZN8PatchDir4InitEv();
    rb3_app_gate("PatchDir::Init()");
}

extern "C" void __real__ZN9CharCache4InitEv(void);
extern "C" void __wrap__ZN9CharCache4InitEv(void) { // ring: native-real
    __real__ZN9CharCache4InitEv();
    rb3_app_gate("CharCache::Init()");
}

extern "C" void __real__ZN9ClosetMgr4InitEv(void);
extern "C" void __wrap__ZN9ClosetMgr4InitEv(void) { // ring: native-real
    __real__ZN9ClosetMgr4InitEv();
    rb3_app_gate("ClosetMgr::Init()");
}

extern "C" void __real__ZN9LessonMgr4InitEv(void);
extern "C" void __wrap__ZN9LessonMgr4InitEv(void) { // ring: native-real
    __real__ZN9LessonMgr4InitEv();
    rb3_app_gate("LessonMgr::Init()");
}

extern "C" void __real__ZN9MetaPanel4InitEv(void);
extern "C" void __wrap__ZN9MetaPanel4InitEv(void) { // ring: native-real
    __real__ZN9MetaPanel4InitEv();
    rb3_app_gate("MetaPanel::Init()");
}

extern "C" void __real__ZN9PrefabMgr4InitEP11BandUserMgr(void * a0);
extern "C" void __wrap__ZN9PrefabMgr4InitEP11BandUserMgr(void * a0) { // ring: native-real
    __real__ZN9PrefabMgr4InitEP11BandUserMgr(a0);
    rb3_app_gate("PrefabMgr::Init(BandUserMgr*)");
}

// ---- lane A2: MetaPanel::Init's steps, and the App::App steps after it ------
extern "C" void __real__Z12GameModeInitv(void);
extern "C" void __wrap__Z12GameModeInitv(void) { // ring: native-real
    __real__Z12GameModeInitv();
    rb3_app_gate("GameModeInit()");
}

extern "C" void __real__ZN11ModifierMgr4InitEv(void);
extern "C" void __wrap__ZN11ModifierMgr4InitEv(void) { // ring: native-real
    __real__ZN11ModifierMgr4InitEv();
    rb3_app_gate("ModifierMgr::Init()");
}

extern "C" void __real__ZN11SongSortMgr4InitEv(void);
extern "C" void __wrap__ZN11SongSortMgr4InitEv(void) { // ring: native-real
    __real__ZN11SongSortMgr4InitEv();
    rb3_app_gate("SongSortMgr::Init()");
}

extern "C" void __real__ZN10SessionMgr4InitEv(void);
// Lane A2 gated this call and A3 narrowed the reason to the sourceless
// BandNetGameData ctor. A4: rb3-xenon-07's bngd landed BandNetGameData.cpp
// (written from retail TU5 bytes), rb3-app compiles it, so this is retail's
// call again: SessionMgr's ctor (builds BandNetGameData, subscribes to the idle
// XboxSession), SessionUsersProvidersInit, BandMachineMgr::Init.
extern "C" void __wrap__ZN10SessionMgr4InitEv(void) { // ring: native-real
    __real__ZN10SessionMgr4InitEv();
    rb3_app_gate("SessionMgr::Init()");
}

extern "C" void __real__ZN10MemcardMgr4InitEv(void *self);
extern "C" void __wrap__ZN10MemcardMgr4InitEv(void *self) { // ring: native-real
    __real__ZN10MemcardMgr4InitEv(self);
    rb3_app_gate("MemcardMgr::Init()");
}

extern "C" void __real__ZN10ProfileMgr4InitEv(void *self);
extern "C" void __wrap__ZN10ProfileMgr4InitEv(void *self) { // ring: native-real
    __real__ZN10ProfileMgr4InitEv(self);
    rb3_app_gate("ProfileMgr::Init()");
    rb3_app_check_profile_picture();
}

extern "C" void __real__ZN13MetaPerformer4InitEv(void);
extern "C" void __wrap__ZN13MetaPerformer4InitEv(void) { // ring: native-real
    __real__ZN13MetaPerformer4InitEv();
    rb3_app_gate("MetaPerformer::Init()");
}

extern "C" void __real__ZN14UploadErrorMgr4InitEv(void);
extern "C" void __wrap__ZN14UploadErrorMgr4InitEv(void) { // ring: native-real
    __real__ZN14UploadErrorMgr4InitEv();
    rb3_app_gate("UploadErrorMgr::Init()");
}

extern "C" void __real__ZN25AppMiniLeaderboardDisplay4InitEv(void);
extern "C" void __wrap__ZN25AppMiniLeaderboardDisplay4InitEv(void) { // ring: native-real
    __real__ZN25AppMiniLeaderboardDisplay4InitEv();
    rb3_app_gate("AppMiniLeaderboardDisplay::Init()");
}

extern "C" void __real__Z7UtlInitv(void);
extern "C" void __wrap__Z7UtlInitv(void) { // ring: native-real
    __real__Z7UtlInitv();
    rb3_app_gate("UtlInit()");
}

extern "C" void __real__ZN11PresenceMgr4InitEv(void *self);
extern "C" void __wrap__ZN11PresenceMgr4InitEv(void *self) { // ring: native-real
    __real__ZN11PresenceMgr4InitEv(self);
    rb3_app_gate("PresenceMgr::Init()");
}

extern "C" void __real__ZN12QuestManager4InitEP9DataArray(void *self, void *a0);
extern "C" void __wrap__ZN12QuestManager4InitEP9DataArray(void *self, void *a0) { // ring: native-real
    __real__ZN12QuestManager4InitEP9DataArray(self, a0);
    rb3_app_gate("QuestManager::Init(DataArray*)");
}

// ---- lane A2: the retail checksum tables, registered only over TU5 content ----
// src/ChecksumData_xbox.cpp's two tables are TU5's (disc + the 41 files TU5
// changed or added; a1b/checksum_xref.txt). Over the bare disc 41 listed files
// differ, so registering them would stop the boot at the first one, by
// measurement. With a title update mounted (A4: by default when the TU5 data
// is found, or RB3_TITLE_UPDATE_DIR=<dir>) the real registrar runs; with none
// (RB3_TITLE_UPDATE_DIR=none, or not found) it is skipped.
const char *Rb3NativeTitleContentPath();
extern "C" void __real__Z19SetFileChecksumDatav(void);
extern "C" void __wrap__Z19SetFileChecksumDatav(void) { // ring: native-real
    if (Rb3NativeTitleContentPath()) {
        __real__Z19SetFileChecksumDatav();
        rb3_app_gate("SetFileChecksumData()");
    } else {
        printf("  [GATED] rb3-app SetFileChecksumData() -- ring tu5-content: the table describes TU5, "
               "no title update mounted (opted out or not found: see the rb3-app title update line), not registered\n");
        fflush(stdout);
    }
}

extern "C" void __real__Z23SetSongMidiChecksumDatav(void);
extern "C" void __wrap__Z23SetSongMidiChecksumDatav(void) { // ring: native-real
    if (Rb3NativeTitleContentPath()) {
        __real__Z23SetSongMidiChecksumDatav();
        rb3_app_gate("SetSongMidiChecksumData()");
    } else {
        printf("  [GATED] rb3-app SetSongMidiChecksumData() -- ring tu5-content: the table describes TU5, "
               "no title update mounted (opted out or not found: see the rb3-app title update line), not registered\n");
        fflush(stdout);
    }
}

// A16: after a screen transition, a ui message reaches the current screen once
// and never a screen that was popped. Retail UIManager::Handle tries mSink
// first (HANDLE_MEMBER_PTR(mSink)) and the current screen last
// (HANDLE_MEMBER_PTR(mCurrentScreen)); mSink is zeroed by the ctor
// (fn_82804588, `stw r29,0x3c(r30)` at 0x82804648) and no other code in UI.cpp
// stores to +0x3c, so retail delivers through mCurrentScreen alone. A
// native-only `mSink = trans` in UIManager::Poll made every entering screen the
// sink, and a pop left the sink naming the popped screen: on a returning boot
// main_hub_screen's A press reached hint_rb3_welcome_screen's continue.btn
// (pushed over main_hub, then popped), whose SELECT_MSG `{ui pop_screen}`
// printed "No screen to pop", and main_hub_panel's state never moved.
// The probe is a private UIManager (zeroed storage, like the static TheUI
// retail constructs: the ctor leaves mCurrentScreen alone) with two bare
// counting screens, TheUI swapped to it for the probe and restored after:
// goto A, push B, pop B, then one probe message. Want: A once, B never.
// Construct-only runs (main_app.cpp), which exit right after.
namespace {
class Rb3AppCountingScreen : public UIScreen { // ring: native-real
public:
    virtual DataNode Handle(DataArray *msg, bool warn) {
        if (msg->Size() > 1 && msg->Type(1) == kDataSymbol && msg->Sym(1) == "rb3_app_ui_probe")
            mProbes++;
        return UIScreen::Handle(msg, warn);
    }
    int mProbes = 0;
};

bool Rb3AppSettle(UIManager *ui, UIScreen *want) { // ring: native-real
    for (int i = 0; i < 32 && (ui->InTransition() || ui->CurrentScreen() != want); i++)
        ui->Poll();
    return !ui->InTransition() && ui->CurrentScreen() == want;
}
} // namespace

void rb3_app_check_ui_sink() { // ring: native-real
    alignas(UIManager) static unsigned char storage[sizeof(UIManager)];
    memset(storage, 0, sizeof storage);
    UIManager *ui = new (storage) UIManager();
    UIManager *saved = TheUI;
    TheUI = ui;
    Rb3AppCountingScreen *a = new Rb3AppCountingScreen();
    Rb3AppCountingScreen *b = new Rb3AppCountingScreen();
    bool ok = true;
    ui->GotoScreen(a, false, false);
    ok = ok && Rb3AppSettle(ui, a);
    if (ok) ui->PushScreen(b);
    ok = ok && Rb3AppSettle(ui, b);
    if (ok) ui->PopScreen(nullptr);
    ok = ok && Rb3AppSettle(ui, a);
    static Message probe("rb3_app_ui_probe");
    if (ok) ui->Handle(probe, false);
    TheUI = saved;
    char what[320];
    snprintf(what, sizeof what,
             "UIManager: after goto A, push B, pop B, a ui message reached A %d time(s) and "
             "the popped B %d time(s) (retail: mSink only zeroed, fn_82804588 0x82804648; "
             "want 1 and 0)",
             a->mProbes, b->mProbes);
    if (!ok) {
        printf("  [FAIL] rb3-app UIManager probe: a transition did not settle (current %p, "
               "A %p, B %p)\n", (void *)ui->CurrentScreen(), (void *)a, (void *)b);
    } else if (a->mProbes == 1 && b->mProbes == 0) {
        rb3_app_gate(what);
        return;
    } else {
        printf("  [FAIL] rb3-app %s\n", what);
    }
    printf("RESULT: GATE FAILED (ui sink)\n");
    fflush(stdout);
    fflush(stderr);
    _exit(7);
}

// A17: a cached milo that embeds a movie loads, and its TexMovie exists.
// ui/startup/gen/startup_movie_keep.milo_xbox holds splash.tmov, a TexMovie
// (output_texture movie_splash.tex) whose Load passes the milo stream to its
// Movie (TexMovie::Load -> DoBeginMovieFromFile, save rev 5 > 4), followed by
// the embedded startup.bik. Retail Movie::Impl::Begin
// (0x82745D90) reads those bytes with a front FileLoader on that stream, and
// CheckOpen (0x82745428) keeps them as the preload buffer. rb3-app has no Bink
// decoder, so BinkOpen fails and the movie is not open (IsOpen 0x82742F08,
// Poll 0x82745628 returns false). Fails when TexMovie is an aborting stub
// (UNREACHED abort at TexMovie::TexMovie, the a6-failing-tu stop) and when the
// movie is a base MovieImpl (MILO_FAIL: cached load not implemented).
#include "movie/TexMovie.h"
#include "obj/Dir.h"
#include "obj/DirLoader.h"
const char *Rb3AppMoviePreload(MovieImpl *impl, int *len); // rb3_app_native_glue.cpp
void NativeStreamAuditBegin(); // obj/DirLoader.cpp: each object must end on its 0xADDEADDE
int NativeStreamAuditObjects();
int NativeStreamAuditAnomalies();
void rb3_app_check_tex_movie() { // ring: native-real
    const char *path = "ui/startup/startup_movie_keep.milo";
    NativeStreamAuditBegin(); // construct-only runs exit after the gates
    int objs0 = NativeStreamAuditObjects(), bad0 = NativeStreamAuditAnomalies();
    ObjectDir *dir = DirLoader::LoadObjects(FilePath(path), nullptr, nullptr);
    int objs = NativeStreamAuditObjects() - objs0, bad = NativeStreamAuditAnomalies() - bad0;
    TexMovie *tm = dir ? dir->Find<TexMovie>("splash.tmov", false) : nullptr;
    int len = 0;
    const char *buf = nullptr;
    bool open = true, polled = true;
    if (tm) {
        for (int i = 0; i < 1000 && tm->GetMovie().CheckOpen(false); i++)
            TheLoadMgr.Poll();
        buf = Rb3AppMoviePreload(tm->GetMovie().GetImpl(), &len);
        open = tm->IsOpen();
        polled = tm->GetMovie().Poll();
    }
    bool tag = buf && len >= 4 && (!memcmp(buf, "iKIB", 4) || !memcmp(buf, "BIKi", 4));
    char what[512];
    snprintf(what, sizeof what,
             "TexMovie: %s dir %s (%d objects, %d off their marker), splash.tmov %s, file %s, "
             "preloaded %d bytes (tag %s), "
             "open %d, poll %d (retail Begin 0x82745D90 reads the embedded movie, "
             "BinkOpen fails natively; want 0 off marker, a TexMovie, a tagged buffer, open 0, "
             "poll 0)",
             path, dir ? "loaded" : "NOT loaded", objs, bad, tm ? "found" : "MISSING",
             tm ? (tm->IsEmpty() ? "empty" : "set") : "-", len, tag ? "Bink" : "none",
             open, polled);
    if (dir && objs > 0 && bad == 0 && tm && !tm->IsEmpty() && tag && !open && !polled) {
        rb3_app_gate(what);
        delete dir;
        return;
    }
    printf("  [FAIL] rb3-app %s\n", what);
    printf("RESULT: GATE FAILED (tex movie)\n");
    fflush(stdout);
    fflush(stderr);
    _exit(7);
}

// A19: the net cache's load fails the way it does on an offline 360, and
// unloads cleanly. Retail NetCacheMgr::Load enters kNCMS_Load, whose
// EnterLoadState calls NetCacheMgrXbox::LoadInit (0x827D7498): a non-local
// server makes it XLSPConnection::Connect (0x827D9998), which goes to state 1
// and runs StartEnumeration (0x827D9918). With no Live logon
// XTitleServerCreateEnumerator fails (xdk_shims.cpp), and the nonzero result
// takes the connection to state 4 (li r4,4 at 0x827D997C). The next
// NetCacheMgrXbox::Poll (0x827D73E0) sees state 4 while mState < 2 and calls
// SetFail (0x827D7480) with 1 if IsEthernetCableConnected (0x8251BE08) else 3;
// mState stays kNCMS_Load and IsReady is false, which is what
// MusicLibraryStore::Poll reads as "the store failed" (its state 1). Unload
// then Disconnects (state 5, no enumerator to cancel, so 0) and Poll returns
// the manager to kNCMS_Nil with mHasFailed cleared. Fails when
// XTitleServerCreateEnumerator is an aborting stub (UNREACHED abort inside
// Load) and when any of that state differs.
#include "utl/NetCacheMgr_Xbox.h"
#include "os/PlatformMgr.h"
namespace {
struct Rb3AppNetCacheProbe : NetCacheMgrXbox {
    int State() const { return mState; }
    int LoadCount() const { return mLoadCount; }
    bool HasCache() const { return mCache != nullptr; }
    const char *Filter() const { return mXLSPFilter.c_str(); }
    int Conn() { return (int)mConnection.GetState(); }
    int ConnRequest() const { return mConnection.GetConnectionRequest(); }
};
} // namespace
void rb3_app_check_net_cache_offline() { // ring: native-real
    NetCacheMgrXbox *mgr = dynamic_cast<NetCacheMgrXbox *>(TheNetCacheMgr);
    if (!mgr) {
        printf("  [FAIL] rb3-app net cache offline: TheNetCacheMgr=%p is not a NetCacheMgrXbox\n",
               (void *)TheNetCacheMgr);
        printf("RESULT: GATE FAILED (net cache offline)\n");
        fflush(stdout);
        _exit(7);
    }
    Rb3AppNetCacheProbe *p = static_cast<Rb3AppNetCacheProbe *>(mgr);
    int state0 = p->State(), loads0 = p->LoadCount(), enums0 = rb3_app_xdk_live_enum_handles();
    bool local = mgr->IsServerLocal();
    bool cable = ThePlatformMgr.IsEthernetCableConnected();
    mgr->Load((NetCacheMgr::CacheSize)0);
    int connLoad = p->Conn(), stateLoad = p->State();
    int polls = 0;
    for (; polls < 10 && !mgr->GetHasFailed(); polls++)
        mgr->Poll();
    bool failed = mgr->GetHasFailed(), ready = mgr->IsReady();
    int failType = (int)mgr->GetFailType(), stateFail = p->State(), connFail = p->Conn();
    mgr->Unload();
    for (int i = 0; i < 10 && p->State() != -1; i++)
        mgr->Poll();
    int state2 = p->State(), conn2 = p->Conn(), req2 = p->ConnRequest(), loads2 = p->LoadCount();
    bool failed2 = mgr->GetHasFailed(), cache2 = p->HasCache();
    int enums2 = rb3_app_xdk_live_enum_handles();
    int wantType = cable ? 1 : 3;
    char what[640];
    snprintf(what, sizeof what,
             "net cache offline: filter \"%s\", server %s, cable %d; before state %d loads %d; "
             "Load -> state %d, XLSP %d; after %d Poll: failed %d type %d (want %d), state %d, "
             "ready %d, XLSP %d; Unload -> state %d, loads %d, failed %d, cache %d, XLSP %d "
             "request %d, live enumerators %d -> %d (want -1 0; 0 4; 1 %d 0 0 4; -1 0 0 0 0 0)",
             p->Filter(), local ? "LOCAL" : "remote", (int)cable, state0, loads0, stateLoad,
             connLoad, polls, (int)failed, failType, wantType, stateFail, (int)ready, connFail,
             state2, loads2, (int)failed2, (int)cache2, conn2, req2, enums0, enums2, wantType);
    if (state0 == -1 && loads0 == 0 && !local && stateLoad == 0 && connLoad == 4 && failed
        && failType == wantType && stateFail == 0 && !ready && connFail == 4 && state2 == -1
        && loads2 == 0 && !failed2 && !cache2 && conn2 == 0 && req2 == 0 && enums2 == enums0) {
        rb3_app_gate(what);
        return;
    }
    printf("  [FAIL] rb3-app %s\n", what);
    printf("RESULT: GATE FAILED (net cache offline)\n");
    fflush(stdout);
    fflush(stderr);
    _exit(7);
}

// A20: a texture a cascade destroys leaves the engine's GPU tables.
// Retail's texture is DxTex; its dtor (0x827356B8) runs ResetSurfaces
// (0x827355D0) before ??1RndTex (0x823FEBB8), so a dead texture holds no GPU
// surface. Natively the surfaces sit in Tex_Wgpu.cpp's side tables, keyed on the
// RndTex address: sTexGpuData, and sSharedTex, which names one holder of each
// identical bitmap as its reference. On the cal_welcome -> cal_manual_audio
// transition, cal_welcome.milo's header_song_shadow.tex died in UnloadPanels'
// cascade while staying the share reference, and cal_manual.milo's same-named
// texture then read the dead one's mBitmap in PresyncBitmap (ASan
// heap-use-after-free, RndBitmap::Pixels). Here: tex A in a dir and a twin B
// outside it share one GPU texture; the dir dies through ~ObjectDir's cascade
// inside Begin/EndBatchDelete, as UIScreen::UnloadPanels does. A's entry must be
// gone, B must keep its texture, and a third twin C must share B's. Fails when
// ~RndTex does not call CleanupGpuTex (A's entry stays, owner dangling).
#include "platform/TexGpu.h"
namespace {
    RndTex *A20Twin(const char *name, ObjectDir *dir) {
        RndTex *t = Hmx::Object::New<RndTex>();
        if (dir)
            t->SetName(name, dir);
        t->SetBitmap(16, 16, 32, RndTex::kRegular, false, "");
        u8 *px = t->Bitmap().Pixels();
        if (px) {
            for (int i = 0; i < t->Bitmap().PixelBytes(); i++)
                px[i] = (u8)(i * 7 + 0xA2);
        }
        t->PresyncBitmap();
        return t;
    }
}
void rb3_app_check_tex_gpu_cleanup() { // ring: native-real
    ObjectDir *dir = Hmx::Object::New<ObjectDir>();
    RndTex *a = A20Twin("a20_twin.tex", dir);
    RndTex *b = A20Twin(nullptr, nullptr);
    GpuTexDebugInfo ia = GetGpuTexDebugInfo(a), ib = GetGpuTexDebugInfo(b);
    size_t holders0 = GpuTexHolderCount();
    bool px = a->Bitmap().Pixels() && b->Bitmap().Pixels();
    void *aAddr = a;
    ObjectDir::BeginBatchDelete();
    delete dir;
    ObjectDir::EndBatchDelete();
    // A's block is freed now: only its address is used, as a map key.
    GpuTexDebugInfo ia2 = GetGpuTexDebugInfo((RndTex *)aAddr);
    GpuTexDebugInfo ib2 = GetGpuTexDebugInfo(b);
    size_t holders1 = GpuTexHolderCount();
    unsigned long long share0 = GpuTexShareCount();
    RndTex *c = A20Twin(nullptr, nullptr);
    GpuTexDebugInfo ic = GetGpuTexDebugInfo(c);
    bool cShared = ic.uploaded && ic.shared && ic.view == ib2.view
        && GpuTexShareCount() == share0 + 1;
    char what[640];
    snprintf(what, sizeof what,
             "tex gpu cleanup: pixels %d; A uploaded %d shared %d, B uploaded %d shared %d "
             "same view %d; holders %zu; dir cascade (batch) -> A entry present %d (want 0), "
             "holders %zu (want %zu), B uploaded %d same view %d; twin C shared %d with B "
             "(retail DxTex dtor 0x827356B8 -> ResetSurfaces 0x827355D0)",
             px, ia.uploaded, ia.shared, ib.uploaded, ib.shared, ia.view == ib.view, holders0,
             ia2.present, holders1, holders0 - 1, ib2.uploaded, ib2.view == ib.view, cShared);
    bool ok = px && ia.uploaded && ib.uploaded && ib.shared && ia.view == ib.view
        && !ia2.present && holders1 == holders0 - 1 && ib2.uploaded && ib2.view == ib.view
        && cShared;
    delete b;
    delete c;
    if (ok) {
        rb3_app_gate(what);
        return;
    }
    printf("  [FAIL] rb3-app %s\n", what);
    printf("RESULT: GATE FAILED (tex gpu cleanup)\n");
    fflush(stdout);
    fflush(stderr);
    _exit(7);
}

// A20: a CharDriver's clip driver leaves the chain when its clip dies.
// Retail CharDriver::Replace (0x82376E40) prunes mFirst with
// CharClipDriver::DeleteRef (0x8239FF50), which compares the held clip
// (lwz 0x2c) with `from`, the dying object on X360. Natively the ring passes
// the ObjRef member as `from` (obj/Object.h RefIs), so the retail compare never
// matched: the driver stayed aimed at the freed clip and the next Play's Exit
// read it (ASan heap-use-after-free in CharClipDriver::ExecuteEvent <- Exit <-
// CharClipDriver ctor <- CharDriver::Play <- BandCharacter::PlayMainClip at
// main_hub; the clip died in UIScreen::UnloadPanels' cascade). Cases: the clip's
// dir dies through ~ObjectDir in Begin/EndBatchDelete (the UnloadPanels path),
// and a plain `delete` of the clip (the ReplaceRefs path). Both: First() NULL.
#include "char/CharClip.h"
#include "char/CharClipDriver.h"
#include "char/CharDriver.h"
#include "math/Utl.h"
void rb3_app_check_clip_driver_prune() { // ring: native-real
    CharDriver *drv = Hmx::Object::New<CharDriver>();
    ObjectDir *dir = Hmx::Object::New<ObjectDir>();
    CharClip *clip = Hmx::Object::New<CharClip>();
    clip->SetName("a20_clip", dir);
    CharClipDriver *d1 = drv->Play(clip, 0, -1.0f, kHugeFloat, 0.0f);
    bool held1 = d1 && drv->First() == d1 && d1->GetClip() == clip;
    ObjectDir::BeginBatchDelete();
    delete dir;
    ObjectDir::EndBatchDelete();
    bool pruned1 = drv->First() == nullptr;
    CharClip *clip2 = Hmx::Object::New<CharClip>();
    CharClipDriver *d2 = drv->Play(clip2, 0, -1.0f, kHugeFloat, 0.0f);
    bool held2 = d2 && drv->First() == d2 && d2->GetClip() == clip2;
    delete clip2;
    bool pruned2 = drv->First() == nullptr;
    char what[512];
    snprintf(what, sizeof what,
             "clip driver prune: cascade (dir dies, batch): held %d -> First NULL %d; "
             "plain delete: held %d -> First NULL %d (retail CharDriver::Replace 0x82376E40 "
             "-> CharClipDriver::DeleteRef 0x8239FF50; want 1 1 1 1)",
             held1, pruned1, held2, pruned2);
    bool ok = held1 && pruned1 && held2 && pruned2;
    if (ok) {
        delete drv;
        rb3_app_gate(what);
        return;
    }
    printf("  [FAIL] rb3-app %s\n", what);
    printf("RESULT: GATE FAILED (clip driver prune)\n");
    fflush(stdout);
    fflush(stderr);
    _exit(7);
}

// A21: the music library's exit runs retail's body to its end.
// Retail TU5 MusicLibrary::OnEnter (fn_82542238) and OnExit (fn_82542A00) make
// no PlatformMgr::SetHomeMenuEnabled call; band.exe has no such function (the
// HOME menu is Wii-only). OnExit ends `stb r11,0x50(r30)` (unk40 = false) at
// 0x82542CE0, then `b 0x828292A4` (the epilogue). The native arms used to keep
// the Wii call, which reached an UNREACHED link stub [ring a1-platformmgr] as
// song_select_screen loaded. Here: OnLoad (0x8253ABB8: a MusicLibraryStore),
// then OnExit on an entered library. It must return with unk40 cleared and the
// store's preview manager gone and the background-download mode back to AUTO
// (ClearPreview, fn_825BC908: `li r3,2`, `bl 0x828406B8` at 0x825BC9C8). Fails
// (rc 134, an UNREACHED stub's abort) when OnExit calls SetHomeMenuEnabled or
// XBackgroundDownloadSetMode has no extern "C" body.
#include "meta/SongPreview.h"
#include "meta_band/BandSongMgr.h"
#include "meta_band/MusicLibrary.h"
#include "meta_band/MusicLibraryStore.h"
#include "utl/NetCacheMgr.h"
#include "xdk/xapilibi/xbox.h"
extern "C" int rb3_app_xdk_background_download_mode(); // xdk_shims.cpp
void rb3_app_check_music_library_exit() { // ring: native-real
    bool made = false;
    if (!TheMusicLibrary) {
        // MetaPanel's ctor does this on a real boot (MetaPanel.cpp).
        SongPreview *prev = new SongPreview(TheSongMgr);
        MusicLibrary::Init(*prev);
        made = true;
    }
    MusicLibrary *lib = TheMusicLibrary;
    MusicLibraryStore *prevStore = lib->unk19c;
    lib->OnLoad();
    MusicLibraryStore *store = lib->unk19c;
    bool previewMgr0 = store && store->mPreviewMgr;
    lib->unk40 = true; // OnEnter's first body store (0x82542250)
    XBackgroundDownloadSetMode(XBACKGROUND_DOWNLOAD_MODE_ALWAYS_ALLOW);
    int mode0 = rb3_app_xdk_background_download_mode();
    lib->OnExit();
    bool entered1 = lib->unk40;
    bool previewMgr1 = store && store->mPreviewMgr;
    int mode1 = rb3_app_xdk_background_download_mode();
    delete store;
    lib->unk19c = prevStore;
    TheNetCacheMgr->Unload();
    for (int i = 0; i < 10 && !TheNetCacheMgr->IsUnloaded(); i++)
        TheNetCacheMgr->Poll();
    char what[512];
    snprintf(what, sizeof what,
             "music library exit: library %s; OnLoad -> store %d, preview mgr %d; OnExit "
             "returned, unk40 %d (want 0), preview mgr %d (want 0), download mode %d -> %d "
             "(want 1 -> 2) (retail OnExit "
             "fn_82542A00 ends stb 0x50 at 0x82542CE0; no SetHomeMenuEnabled in TU5)",
             made ? "made here" : "from MetaPanel", store != nullptr, (int)previewMgr0,
             (int)entered1, (int)previewMgr1, mode0, mode1);
    if (store && previewMgr0 && !entered1 && !previewMgr1 && mode0 == 1 && mode1 == 2) {
        rb3_app_gate(what);
        return;
    }
    printf("  [FAIL] rb3-app %s\n", what);
    printf("RESULT: GATE FAILED (music library exit)\n");
    fflush(stdout);
    fflush(stderr);
    _exit(7);
}

// A21: Hmx::Object's copy constructor (retail 0x822774F8, the memberwise copy
// SongRecord's copy constructor 0x82598190 calls). Song select's
// SongSortMgr::BuildFilteredSongList copies SongRecords into a std::pair, and
// natively the declaration had no body (UNREACHED stub [ring a1-absent-body],
// rb3-app aborted entering song_select_screen). Here: A carries a type prop and
// one ref; copy B from it through a derived class's implicit copy constructor
// (the base-object form SongRecord's uses). B must hold A's name and dir, an equal but separate
// prop (an edit to B leaves A), no refs; A keeps its one ref, through B's death.
#include "obj/ObjPtr_p.h"
namespace {
    struct A21Copyable : public Hmx::Object {}; // implicit copy -> Hmx::Object base copy
}
void rb3_app_check_object_copy() { // ring: native-real
    static Symbol a21_prop("a21_prop");
    A21Copyable *a = new A21Copyable();
    a->SetProperty(a21_prop, DataNode(7));
    ObjPtr<Hmx::Object> ref(nullptr, a);
    int aRefs0 = a->RefCount();
    int bRefs = -1, bProp = -1, aPropAfterEdit = -1;
    bool sameName = false, sameDir = false;
    {
        A21Copyable b(*a);
        bRefs = b.RefCount();
        sameName = b.Name() == a->Name();
        sameDir = b.Dir() == a->Dir();
        const DataNode *p = b.Property(a21_prop, false);
        bProp = p ? p->Int() : -1;
        b.SetProperty(a21_prop, DataNode(9));
        const DataNode *pa = a->Property(a21_prop, false);
        aPropAfterEdit = pa ? pa->Int() : -1;
    }
    int aRefs1 = a->RefCount();
    bool refHeld = ref.Ptr() == a;
    const DataNode *pa = a->Property(a21_prop, false);
    int aProp = pa ? pa->Int() : -1;
    char what[512];
    snprintf(what, sizeof what,
             "object copy: A refs %d; B: refs %d (want 0), name %d dir %d, prop %d (want 7); "
             "B's edit -> A prop %d (want 7); B dies -> A refs %d (want %d), ref held %d, "
             "prop %d (want 7) (retail Object copy 0x822774F8 <- SongRecord copy 0x82598190)",
             aRefs0, bRefs, (int)sameName, (int)sameDir, bProp, aPropAfterEdit, aRefs1, aRefs0,
             (int)refHeld, aProp);
    bool ok = aRefs0 == 1 && bRefs == 0 && sameName && sameDir && bProp == 7
        && aPropAfterEdit == 7 && aRefs1 == aRefs0 && refHeld && aProp == 7;
    ref = nullptr;
    delete a;
    if (ok) {
        rb3_app_gate(what);
        return;
    }
    printf("  [FAIL] rb3-app %s\n", what);
    printf("RESULT: GATE FAILED (object copy)\n");
    fflush(stdout);
    fflush(stderr);
    _exit(7);
}

// A22: a drawable that dies leaves Rnd's pre-clear draw lists. The walk loads a
// song, a band character's OutfitConfig registers (Rnd::PreClearDrawAddOrRemove,
// retail fn_82415C10 -> PreClearCompilerHelper 0x824156B8: push_back, then
// sort 0x8240F188), and the character's dir unloads. Retail's ~Hmx::Object
// (0x8275CBF0) walks its ring; the list is the ring-ref, and the list's
// Replace (ObjPtrList<RndDrawable>::Replace 0x8236F338) finds the node holding
// the dying object (cmplw at 0x8236F3AC) and erases it (Unlink 0x8227D0E8,
// PoolFree 0x827BADB0). Retail's sort swaps the held words (0x8240F1EC/F0),
// which is sound there because the ring entry is the list, not the node. On
// native each node is its own ring-ref, so the sort must move the ring
// membership with the object. Here: register X (order 1) then Y (order 0) in
// mDraws, so the sort swaps them; delete X; X must be gone and Y kept.
#include "rndobj/Group.h"
#include "rndobj/Rnd.h"
namespace {
    struct A22RndPeek : public Rnd { // reads the protected list through a member pointer
        static ObjPtrList<RndDrawable> Rnd::*Draws() { return &A22RndPeek::mDraws; }
    };
    // Pointer compare only: a dead entry must not be upcast (its vptr is gone).
    int A22Count(ObjPtrList<RndDrawable> &list, RndDrawable *d) {
        int n = 0;
        for (ObjPtrList<RndDrawable>::iterator it = list.begin(); it != list.end(); ++it)
            n += *it == d;
        return n;
    }
}
void rb3_app_check_pre_clear_unregister() { // ring: native-real
    ObjPtrList<RndDrawable> &draws = TheRnd.*A22RndPeek::Draws();
    int size0 = draws.size();
    RndGroup *x = Hmx::Object::New<RndGroup>();
    RndGroup *y = Hmx::Object::New<RndGroup>();
    x->SetOrder(1.0f);
    y->SetOrder(0.0f);
    TheRnd.PreClearDrawAddOrRemove(x, true, false);
    TheRnd.PreClearDrawAddOrRemove(y, true, false);
    int size1 = draws.size();
    RndDrawable *xd = x, *yd = y;
    bool yFirst = false;
    for (ObjPtrList<RndDrawable>::iterator k = draws.begin(); k != draws.end(); ++k) {
        if (*k == yd) { yFirst = true; break; }
        if (*k == xd) break;
    }
    int xRefs = x->RefCount(), yRefs = y->RefCount();
    delete x; // ~Hmx::Object's ring walk is the unregister
    int size2 = draws.size();
    bool xGone = A22Count(draws, xd) == 0;
    bool yKept = A22Count(draws, yd) == 1;
    char what[512];
    snprintf(what, sizeof what,
             "pre-clear unregister: mDraws %d -> %d after X,Y (want +2), Y sorted first %d; "
             "refs X %d Y %d (want 1 1); X dies -> mDraws %d (want %d), X gone %d, Y kept %d "
             "(retail ~Object 0x8275CBF0 -> ObjPtrList<RndDrawable>::Replace 0x8236F338 erase; "
             "sort 0x8240F188)",
             size0, size1, (int)yFirst, xRefs, yRefs, size2, size0 + 1, (int)xGone, (int)yKept);
    bool ok = size1 == size0 + 2 && yFirst && xRefs == 1 && yRefs == 1 && size2 == size0 + 1
        && xGone && yKept;
    if (xGone)
        TheRnd.PreClearDrawAddOrRemove(y, false, false);
    bool yGone = A22Count(draws, yd) == 0;
    if (xGone && yGone)
        delete y;
    if (ok && yGone) {
        rb3_app_gate(what);
        return;
    }
    printf("  [FAIL] rb3-app %s; Y removed %d\n", what, (int)yGone);
    printf("RESULT: GATE FAILED (pre-clear unregister)\n");
    fflush(stdout);
    fflush(stderr);
    _exit(7);
}

// A23: an offline song starts. SyncGameStartPanel (game_screen's sync panel)
// is loaded only once it reaches its synced state, which only
// OnMsg(SyncStartGameMsg) sets. Offline the local machine leads a one-machine
// session: the leader's lock-step completes locally (LockStepMgr::RespondToLock
// -> OnLockResponseMsg -> CheckAllMachinesResponded -> ReleaseLock, all on the
// local machine), the panel's LockStepCompleteMsg handler calls
// TheNetSession->StartGame(), and BeginGameStartCountdown sets mGameState to
// kStartingGame with no start time (IsLocal). The message itself is sent by
// NetSession::Poll (0x823E67A8): mGameState@0x4C == 1 and a null
// mGameStartTime -> EnterInGameState (0x823E2810, bl at 0x823E6828) ->
// kInLocalGame + Handle(SyncStartGameMsg). Net::Poll (0x823E03A8) runs it
// through mSession->Poll() (0x823E03D0..E0 -> XboxSession::Poll 0x823F06F0).
// The native offline Net::Poll used to skip the session, so the walk stalled in
// tv3_a_screen -> game_screen until the frame cap.
// Here: a LockStepMgr whose callback does what SyncGameStartPanel's two
// handlers do (LockStepCompleteMsg as leader: StartGame; SyncStartGameMsg:
// synced), and is a TheNetSession sink as the panel is. StartLock, then
// RespondToLock(true) as the panel's state-2 poll does; then Net::Poll up to
// three frames. It must see LockStepCompleteMsg, kStartingGame, then
// SyncStartGameMsg and IsInGame. Afterwards EndGame returns the session to the
// lobby (LeaveInGameState, kInLocalGame arm). Fails (rc 7) when Net::Poll
// does not poll the session (sabotage: drop the mSession->Poll() line in
// rb3_app_net_poll_offline).
#include "meta_band/LockStepMgr.h"
#include "meta_band/SessionMgr.h"
#include "net/NetSession.h"
namespace {
    struct A23GameStartSink : public Hmx::Object {
        int completes = 0, starts = 0;
        bool startingAtComplete = false;
        virtual DataNode Handle(DataArray *msg, bool warn) override {
            Symbol sym = msg->Sym(1);
            if (sym == LockStepCompleteMsg::Type()) {
                completes++;
                // SyncGameStartPanel::OnMsg(LockStepCompleteMsg), leader arm
                TheNetSession->StartGame();
                startingAtComplete = TheNetSession->IsStartingGame();
                return 1;
            }
            if (sym == SyncStartGameMsg::Type()) {
                starts++;
                return 1;
            }
            return Hmx::Object::Handle(msg, warn);
        }
    };
}
void rb3_app_check_offline_game_start() { // ring: native-real
    char what[640];
    if (!TheSessionMgr || !TheNetSession) {
        snprintf(what, sizeof what, "offline game start: TheSessionMgr %p, TheNetSession %p",
                 (void *)TheSessionMgr, (void *)TheNetSession);
        printf("  [FAIL] rb3-app %s\n", what);
        printf("RESULT: GATE FAILED (offline game start)\n");
        fflush(stdout);
        _exit(7);
    }
    bool lobby0 = !TheNetSession->IsInGame() && !TheNetSession->IsStartingGame();
    // A walk reaches song select through a mode change, and
    // BandMatchmaker::OnMsg(ModeChangedMsg) stores SetRanked(IsRanked()), false
    // for quickplay. Construct-only has had no mode change, and neither the
    // SessionSettings ctor (retail 0x823F3C78) nor Clear (0x823F3BE8) writes
    // mRanked@0xC, so the gate stores the quickplay value itself; otherwise
    // StartGame would read an unset byte and may take the ranked
    // (arbitration) arm.
    SessionSettings *settings = TheNetSession->GetSessionSettings();
    if (settings)
        settings->SetRanked(false);
    A23GameStartSink *sink = new A23GameStartSink();
    TheNetSession->AddSink(sink);
    int startsAtPoll[3] = { -1, -1, -1 };
    bool inGame = false;
    {
        LockStepMgr lock("a23_game_start", sink);
        lock.StartLock(); // SyncGameStartPanel state 0, leader local
        bool inLock = lock.InLock();
        lock.RespondToLock(true); // state 2 -> 3
        bool released = !lock.InLock();
        for (int i = 0; i < 3; i++) {
            TheNet.Poll();
            startsAtPoll[i] = sink->starts;
        }
        inGame = TheNetSession->IsInGame();
        snprintf(what, sizeof what,
                 "offline game start: lobby %d; lock in %d, released %d; LockStepCompleteMsg "
                 "x%d (want 1), kStartingGame after StartGame %d (want 1); SyncStartGameMsg "
                 "after Net::Poll 1/2/3: %d/%d/%d (want 1/1/1), in game %d (want 1) (retail "
                 "Net::Poll 0x823E03A8 polls the session at 0x823E03D0; NetSession::Poll "
                 "0x823E67A8 -> EnterInGameState 0x823E2810)",
                 (int)lobby0, (int)inLock, (int)released, sink->completes,
                 (int)sink->startingAtComplete, startsAtPoll[0], startsAtPoll[1],
                 startsAtPoll[2], (int)inGame);
    }
    bool ok = lobby0 && sink->completes == 1 && sink->startingAtComplete
        && startsAtPoll[0] == 1 && startsAtPoll[2] == 1 && inGame;
    if (TheNetSession->IsInGame())
        TheNetSession->EndGame(0, false, 0.0f);
    bool lobby1 = !TheNetSession->IsInGame() && !TheNetSession->IsStartingGame();
    TheNetSession->RemoveSink(sink);
    delete sink;
    if (ok && lobby1) {
        rb3_app_gate(what);
        return;
    }
    printf("  [FAIL] rb3-app %s; back in lobby %d\n", what, (int)lobby1);
    printf("RESULT: GATE FAILED (offline game start)\n");
    fflush(stdout);
    fflush(stderr);
    _exit(7);
}

// A24: a transformable's parent ref, retargeted by a ref walk, moves the child.
// Retail RndTransformable::Replace (fn_823F94E8) compares the held parent's
// Hmx::Object subobject with `from` (fn_823F94E8+0x34, `cmplw cr6,r11,r4`), the
// dying object on X360, and on a match calls SetTransParent(to) (the bl at
// fn_823F94E8+0x64 -> fn_823F9318), which takes the child out of the old
// parent's mChildren and puts it in the new one's. The native ring passes the
// ObjRef member itself as `from` (obj/Object.h RefIs), so the retail compare
// never matched: ReplaceRefs (BandCharacter::Filter's merges, CharBoneDir,
// Character::UnhookShadow) left the child in the old parent's list and out of
// the new one's. Here: C under P1, P1->ReplaceRefs(P2) must leave C under P2
// and only in P2's list; deleting C must leave both lists empty, and NullifyAllRefs
// (a cascade's Phase 0) on a parent must take C out of that parent's list. Fails
// with the retail compare natively (rc 7).
#include "rndobj/Trans.h"
void rb3_app_check_trans_parent_replace() { // ring: native-real
    RndTransformable *p1 = Hmx::Object::New<RndTransformable>();
    RndTransformable *p2 = Hmx::Object::New<RndTransformable>();
    RndTransformable *c = Hmx::Object::New<RndTransformable>();
    c->SetTransParent(p1, false);
    bool held = c->TransParent() == p1 && p1->Children().size() == 1;
    p1->ReplaceRefs(p2);
    bool moved = c->TransParent() == p2;
    int n1 = (int)p1->Children().size(), n2 = (int)p2->Children().size();
    bool listed = n2 == 1 && p2->Children().front() == c;
    // The cascade arm: Phase 0 runs NullifyAllRefs on a dying parent.
    p2->NullifyAllRefs();
    bool orphaned = c->TransParent() == nullptr;
    int n2b = (int)p2->Children().size();
    c->SetTransParent(p1, false);
    delete c;
    int n1c = (int)p1->Children().size();
    char what[512];
    snprintf(what, sizeof what,
             "trans parent replace: C under P1 %d; P1->ReplaceRefs(P2): C under P2 %d, "
             "P1/P2 children %d/%d (want 0/1, P2's is C %d); P2->NullifyAllRefs: C parent "
             "NULL %d, P2 children %d (want 0); C back under P1 and deleted: P1 children %d "
             "(want 0) (retail RndTransformable::Replace fn_823F94E8 -> SetTransParent "
             "fn_823F9318)",
             (int)held, (int)moved, n1, n2, (int)listed, (int)orphaned, n2b, n1c);
    bool ok = held && moved && n1 == 0 && listed && orphaned && n2b == 0 && n1c == 0;
    if (ok) {
        delete p1;
        delete p2;
        rb3_app_gate(what);
        return;
    }
    printf("  [FAIL] rb3-app %s\n", what);
    printf("RESULT: GATE FAILED (trans parent replace)\n");
    fflush(stdout);
    fflush(stderr);
    _exit(7);
}

// A24: a Character's sphere base follows a ref walk on the object it names.
// Retail Character::Replace (fn_8236DBF8) compares `from`, the dying object on
// X360, with the held sphere base (fn_8236DBF8+0x3C, `cmplw cr6,r30,r11`); on a
// match it retargets the pointer to `to` (SetOwnerObj, the bl at +0x70) and,
// when that leaves it null, back to the Character itself (+0x94). The native
// ring passes the ObjRef member as `from` (obj/Object.h RefIs), so the compare
// never matched natively. On the walk, BandCharacter::Filter's
// mInstDir->CopyBoundingSphere(T) aimed the instrument Character's sphere base at
// a temp merge Character T that FileMerger::FinishLoading then deleted, and
// BandCharacter::CalcBoundingSphere read it (ASan heap-use-after-free, rb3-app
// tv3 -> game_screen). Here: A->CopyBoundingSphere(T) aims A's base at T;
// T->ReplaceRefs(U) must move it to U; U->ReplaceRefs(nullptr) (X360 ~Object
// 0x8275CBF0's walk) must bring it back to A; deleting T and U must leave it on
// A. Fails with the retail compare natively (rc 7).
#include "char/Character.h"
namespace {
    struct A24CharPeek : public Character { // reads the protected member through a member pointer
        static ObjOwnerPtr<RndTransformable> Character::*Base() {
            return &A24CharPeek::mSphereBase;
        }
    };
}
void rb3_app_check_sphere_base_replace() { // ring: native-real
    Character *a = Hmx::Object::New<Character>();
    Character *t = Hmx::Object::New<Character>();
    Character *u = Hmx::Object::New<Character>();
    ObjOwnerPtr<RndTransformable> &base = a->*A24CharPeek::Base();
    bool self0 = base.Ptr() == a;
    a->CopyBoundingSphere(t); // t's base is t itself
    bool onT = base.Ptr() == t;
    t->ReplaceRefs(u);
    bool onU = base.Ptr() == u;
    u->ReplaceRefs(nullptr);
    bool backA = base.Ptr() == a;
    a->CopyBoundingSphere(t);
    bool onT2 = base.Ptr() == t;
    delete t;
    delete u;
    bool afterDelete = base.Ptr() == a;
    char what[512];
    snprintf(what, sizeof what,
             "sphere base replace: A's base is A %d; A->CopyBoundingSphere(T): on T %d; "
             "T->ReplaceRefs(U): on U %d; U->ReplaceRefs(NULL): back on A %d; on T again %d, "
             "T and U deleted: on A %d (want all 1) (retail Character::Replace fn_8236DBF8)",
             (int)self0, (int)onT, (int)onU, (int)backA, (int)onT2, (int)afterDelete);
    bool ok = self0 && onT && onU && backA && onT2 && afterDelete;
    if (ok) {
        delete a;
        rb3_app_gate(what);
        return;
    }
    printf("  [FAIL] rb3-app %s\n", what);
    printf("RESULT: GATE FAILED (sphere base replace)\n");
    fflush(stdout);
    fflush(stderr);
    _exit(7);
}

// A26: the rest of A24's same-shape census -- owner-control holders whose
// retail Replace compares `from`, the dying object on X360, with the held
// pointer. The native ring passes the ObjRef member itself as `from`
// (obj/Object.h RefIs), so each compare never matched natively and a
// ReplaceRefs walk (snapshot, self-loop, Replace) left the member on the dying
// object. Retail compares (band.exe TU5):
//   RndCamAnim::Replace         fn_82485C80+0x34 `cmplw cr6,r11,r4`  (to ? to's keys owner : this)
//   RndParticleSysAnim::Replace fn_8247F168+0x34 `cmplw cr6,r11,r4`  (same shape)
//   Spotlight::Replace          fn_824D81C0+0x3C `cmplw cr6,r11,r30` (to as Spotlight, then null -> this)
//   BandCharacter::Replace      fn_8227E168+0x3C `cmplw cr6,r30,r11` (to as BandCharDesc, CopyCharDesc)
//   CharClipGroup::Replace      fn_8238F270+0x58 `cmplw cr6,r11,r22` (entry := to as CharClip; null entry erased)
// Each sub-check: X->ReplaceRefs(Y) must move the member to Y's answer, then
// Y->ReplaceRefs(nullptr) (X360 ~Object 0x8275CBF0's walk) must apply retail's
// null arm. ReplaceRefs, not a plain delete: A24 found a dir-free delete takes
// NullifyAllRefs, whose X16 seed restore hides the self-seeded members.
// Fails with the retail compares natively (rc 7).
#include "rndobj/CamAnim.h"
#include "rndobj/PartAnim.h"
#include "world/Spotlight.h"
#include "bandobj/BandCharacter.h"
#include "bandobj/BandCharDesc.h"
#include "char/CharClipGroup.h"
#include "char/CharClip.h"
namespace {
    struct A26CamPeek : public RndCamAnim {
        static ObjOwnerPtr<RndCamAnim> RndCamAnim::*Keys() { return &A26CamPeek::mKeysOwner; }
    };
    struct A26PartPeek : public RndParticleSysAnim {
        static ObjOwnerPtr<RndParticleSysAnim> RndParticleSysAnim::*Keys() {
            return &A26PartPeek::mKeysOwner;
        }
    };
    struct A26SpotPeek : public Spotlight {
        static ObjOwnerPtr<Spotlight> Spotlight::*Color() { return &A26SpotPeek::mColorOwner; }
    };
    struct A26GroupPeek : public CharClipGroup {
        static ObjVector<ObjOwnerPtr<CharClip> > CharClipGroup::*Clips() {
            return &A26GroupPeek::mClips;
        }
    };
}
void rb3_app_check_owner_ptr_replace() { // ring: native-real
    // RndCamAnim: A's keys owner K; K->ReplaceRefs(K2) -> K2's keys owner (K2);
    // K2->ReplaceRefs(NULL) -> A itself.
    RndCamAnim *ca = Hmx::Object::New<RndCamAnim>();
    RndCamAnim *ck = Hmx::Object::New<RndCamAnim>();
    RndCamAnim *ck2 = Hmx::Object::New<RndCamAnim>();
    ObjOwnerPtr<RndCamAnim> &camKeys = ca->*A26CamPeek::Keys();
    camKeys = ck;
    bool camOnK = camKeys.Ptr() == ck;
    ck->ReplaceRefs(ck2);
    bool camOnK2 = camKeys.Ptr() == ck2;
    ck2->ReplaceRefs(nullptr);
    bool camSelf = camKeys.Ptr() == ca;
    bool cam = camOnK && camOnK2 && camSelf;

    RndParticleSysAnim *pa = Hmx::Object::New<RndParticleSysAnim>();
    RndParticleSysAnim *pk = Hmx::Object::New<RndParticleSysAnim>();
    RndParticleSysAnim *pk2 = Hmx::Object::New<RndParticleSysAnim>();
    ObjOwnerPtr<RndParticleSysAnim> &partKeys = pa->*A26PartPeek::Keys();
    partKeys = pk;
    bool partOnK = partKeys.Ptr() == pk;
    pk->ReplaceRefs(pk2);
    bool partOnK2 = partKeys.Ptr() == pk2;
    pk2->ReplaceRefs(nullptr);
    bool partSelf = partKeys.Ptr() == pa;
    bool part = partOnK && partOnK2 && partSelf;

    // Spotlight: S's color owner C; C->ReplaceRefs(C2) -> C2; C2->ReplaceRefs(NULL)
    // -> null, then the `!Ptr()` tail -> S itself.
    Spotlight *sa = Hmx::Object::New<Spotlight>();
    Spotlight *sc = Hmx::Object::New<Spotlight>();
    Spotlight *sc2 = Hmx::Object::New<Spotlight>();
    ObjOwnerPtr<Spotlight> &color = sa->*A26SpotPeek::Color();
    color = sc;
    bool spotOnC = color.Ptr() == sc;
    sc->ReplaceRefs(sc2);
    bool spotOnC2 = color.Ptr() == sc2;
    sc2->ReplaceRefs(nullptr);
    bool spotSelf = color.Ptr() == sa;
    bool spot = spotOnC && spotOnC2 && spotSelf;

    // BandCharacter: test prefab D; D->ReplaceRefs(D2) -> D2; D2->ReplaceRefs(NULL)
    // -> null (mTestPrefab is not self-seeded).
    BandCharacter *bc = Hmx::Object::New<BandCharacter>();
    BandCharDesc *bd = Hmx::Object::New<BandCharDesc>();
    BandCharDesc *bd2 = Hmx::Object::New<BandCharDesc>();
    bc->mTestPrefab = bd;
    bool bandOnD = bc->mTestPrefab.Ptr() == bd;
    bd->ReplaceRefs(bd2);
    bool bandOnD2 = bc->mTestPrefab.Ptr() == bd2;
    bd2->ReplaceRefs(nullptr);
    bool bandNull = bc->mTestPrefab.Ptr() == nullptr;
    bool band = bandOnD && bandOnD2 && bandNull;

    // CharClipGroup: clips {C1, C2}; C1->ReplaceRefs(C3) -> {C3, C2};
    // C2->ReplaceRefs(NULL) -> {C3} (retail erases the null entry).
    CharClipGroup *g = Hmx::Object::New<CharClipGroup>();
    CharClip *c1 = Hmx::Object::New<CharClip>();
    CharClip *c2 = Hmx::Object::New<CharClip>();
    CharClip *c3 = Hmx::Object::New<CharClip>();
    ObjVector<ObjOwnerPtr<CharClip> > &clips = g->*A26GroupPeek::Clips();
    g->AddClip(c1);
    g->AddClip(c2);
    bool grpTwo = clips.size() == 2 && clips[0].Ptr() == c1 && clips[1].Ptr() == c2;
    c1->ReplaceRefs(c3);
    bool grpOnC3 = clips.size() == 2 && clips[0].Ptr() == c3 && clips[1].Ptr() == c2;
    c2->ReplaceRefs(nullptr);
    bool grpErased = clips.size() == 1 && clips[0].Ptr() == c3;
    bool grp = grpTwo && grpOnC3 && grpErased;

    char what[768];
    snprintf(what, sizeof what,
             "owner ptr replace: RndCamAnim keys on K %d, on K2 %d, self %d; "
             "RndParticleSysAnim keys on K %d, on K2 %d, self %d; "
             "Spotlight color on C %d, on C2 %d, self %d; "
             "BandCharacter prefab on D %d, on D2 %d, null %d; "
             "CharClipGroup {C1,C2} %d, {C3,C2} %d, {C3} %d (want all 1)",
             (int)camOnK, (int)camOnK2, (int)camSelf, (int)partOnK, (int)partOnK2,
             (int)partSelf, (int)spotOnC, (int)spotOnC2, (int)spotSelf, (int)bandOnD,
             (int)bandOnD2, (int)bandNull, (int)grpTwo, (int)grpOnC3, (int)grpErased);
    if (cam && part && spot && band && grp) {
        delete ck;
        delete ck2;
        delete ca;
        delete pk;
        delete pk2;
        delete pa;
        delete sc;
        delete sc2;
        delete sa;
        delete bd;
        delete bd2;
        delete bc;
        delete c1;
        delete c2;
        delete c3;
        delete g;
        rb3_app_gate(what);
        return;
    }
    printf("  [FAIL] rb3-app %s\n", what);
    printf("RESULT: GATE FAILED (owner ptr replace)\n");
    fflush(stdout);
    fflush(stderr);
    _exit(7);
}

// A27: the two unconditional Replace bodies A26's census left. Both compare
// `from`, the dying object on X360, with a held pointer; natively `from` is the
// ring ref itself, so neither compare ever matched. Retail (band.exe TU5):
//   CharBonesMeshes::Replace fn_8237B338: dummy guard +0x38 `cmplw cr6,r4,r11`,
//     entry compare +0x68 `cmplw cr6,r11,r4`; match: entry := to as
//     RndTransformable (+0x98, SetOwnerObj +0xA4), null -> dummy (+0xBC).
//   TypeProps::Replace fn_827663D8 -> ReplaceObject fn_82766338, compare +0x20
//     `cmplw cr6,r3,r5`; match: Release from +0x2C, value := to, AddRef to +0x6C.
//     Same for an object-valued prop and each object element of an array prop.
// CharBonesMeshes: {T1, dummy}; T1->ReplaceRefs(T2) -> {T2, dummy};
// T2->ReplaceRefs(NULL) -> {dummy, dummy}; dummy->ReplaceRefs(T3) leaves both on
// the dummy (the guard). TypeProps: X as prop "obj" AND element 0 of array prop
// "arr" (two nodes, two ring entries), W as element 1; X->ReplaceRefs(Y) moves
// both values to Y and both nodes onto Y's ring (Y gains 2 refs), W untouched;
// Y->ReplaceRefs(NULL) nulls both values and takes both nodes off Y's ring; and a
// plain `delete Z` of prop "z"'s value leaves "z" null, not the freed address.
// ReplaceRefs for the first two, as A24/A26 (a dir delete hides this class).
// Fails with the retail compares natively (rc 7).
#include "char/CharBonesMeshes.h"
#include "rndobj/Trans.h"
namespace {
    struct A27BonesPeek : public CharBonesMeshes {
        static ObjVector<ObjOwnerPtr<RndTransformable> > CharBonesMeshes::*Meshes() {
            return &A27BonesPeek::mMeshes;
        }
        static RndTransformable *CharBonesMeshes::*Dummy() { return &A27BonesPeek::mDummyMesh; }
    };
    Hmx::Object *A27Val(TypeProps *tp, const char *key) {
        DataNode *n = tp->KeyValue(Symbol(key), false);
        return n && n->Type() == kDataObject ? n->UncheckedObj() : (Hmx::Object *)-1;
    }
    Hmx::Object *A27Elem(TypeProps *tp, const char *key, int i) {
        DataNode *n = tp->KeyValue(Symbol(key), false);
        if (!n || n->Type() != kDataArray || n->UncheckedArray()->Size() <= i)
            return (Hmx::Object *)-1;
        DataNode &e = n->UncheckedArray()->Node(i);
        return e.Type() == kDataObject ? e.UncheckedObj() : (Hmx::Object *)-1;
    }
}
void rb3_app_check_bones_typeprops_replace() { // ring: native-real
    CharBonesMeshes *b = new CharBonesMeshes();
    ObjVector<ObjOwnerPtr<RndTransformable> > &meshes = b->*A27BonesPeek::Meshes();
    RndTransformable *dummy = b->*A27BonesPeek::Dummy();
    RndTransformable *t1 = Hmx::Object::New<RndTransformable>();
    RndTransformable *t2 = Hmx::Object::New<RndTransformable>();
    RndTransformable *t3 = Hmx::Object::New<RndTransformable>();
    meshes.resize(2);
    meshes[0] = t1;
    meshes[1] = dummy;
    bool bonesSet = meshes[0].Ptr() == t1 && meshes[1].Ptr() == dummy;
    t1->ReplaceRefs(t2);
    bool bonesOnT2 = meshes[0].Ptr() == t2 && meshes[1].Ptr() == dummy;
    t2->ReplaceRefs(nullptr);
    bool bonesDummy = meshes[0].Ptr() == dummy && meshes[1].Ptr() == dummy;
    dummy->ReplaceRefs(t3);
    bool bonesGuard = meshes[0].Ptr() == dummy && meshes[1].Ptr() == dummy;
    bool bones = bonesSet && bonesOnT2 && bonesDummy && bonesGuard;

    RndTransformable *owner = Hmx::Object::New<RndTransformable>();
    RndTransformable *x = Hmx::Object::New<RndTransformable>();
    RndTransformable *y = Hmx::Object::New<RndTransformable>();
    RndTransformable *w = Hmx::Object::New<RndTransformable>();
    RndTransformable *z = Hmx::Object::New<RndTransformable>();
    TypeProps *tp = new TypeProps(owner);
    tp->SetKeyValue(Symbol("a27_obj"), DataNode(x), true);
    DataArray *arr = new DataArray(0);
    tp->SetKeyValue(Symbol("a27_arr"), DataNode(arr, kDataArray), false);
    arr->Release();
    tp->InsertArrayValue(Symbol("a27_arr"), 0, DataNode(x), nullptr);
    tp->InsertArrayValue(Symbol("a27_arr"), 1, DataNode(w), nullptr);
    tp->SetKeyValue(Symbol("a27_z"), DataNode(z), true);
    bool tpSet = A27Val(tp, "a27_obj") == x && A27Elem(tp, "a27_arr", 0) == x
        && A27Elem(tp, "a27_arr", 1) == w && A27Val(tp, "a27_z") == z;
    int yRefs0 = y->RefCount();
    x->ReplaceRefs(y);
    int yGain = y->RefCount() - yRefs0;
    bool tpOnY = A27Val(tp, "a27_obj") == y && A27Elem(tp, "a27_arr", 0) == y
        && A27Elem(tp, "a27_arr", 1) == w && yGain == 2;
    y->ReplaceRefs(nullptr);
    bool tpNull = A27Val(tp, "a27_obj") == nullptr && A27Elem(tp, "a27_arr", 0) == nullptr
        && A27Elem(tp, "a27_arr", 1) == w && y->RefCount() == yRefs0;
    delete z; // a plain delete: ~Object's ReplaceRefs(nullptr) walk
    bool tpDeleted = A27Val(tp, "a27_z") == nullptr;
    bool typeProps = tpSet && tpOnY && tpNull && tpDeleted;

    char what[512];
    snprintf(what, sizeof what,
             "bones typeprops replace: CharBonesMeshes {T1,D} %d, {T2,D} %d, {D,D} %d, "
             "dummy guard %d; TypeProps set %d, on Y %d (Y gained %d refs), null %d, "
             "deleted value null %d (want all 1)",
             (int)bonesSet, (int)bonesOnT2, (int)bonesDummy, (int)bonesGuard, (int)tpSet,
             (int)tpOnY, yGain, (int)tpNull, (int)tpDeleted);
    if (bones && typeProps) {
        delete tp;
        delete owner;
        delete x;
        delete y;
        delete w;
        delete b;
        delete t1;
        delete t2;
        delete t3;
        rb3_app_gate(what);
        return;
    }
    printf("  [FAIL] rb3-app %s\n", what);
    printf("RESULT: GATE FAILED (bones typeprops replace)\n");
    fflush(stdout);
    fflush(stderr);
    _exit(7);
}

// A25: TrackPanel::UpdateReservedVocalSlot must find the vocal slot. Retail
// fn_82B912A8 walks mTrackSlots at sizeof(TrackSlot) == 8 on X360 (the literal 8
// in our source); on LP64 the slot is 16 bytes, so the literal stride read
// half-slots and named the wrong vocal slot, and game_screen's
// TrackPanelDir::AssignTrack failed ReservedVocalPlayerSlot (0x10A). Here: put
// kInstVocals in each slot k in turn and in none; the reserved slot must be k,
// and 4 when no slot sings (fn_82B912A8+0x58). Fails with the literal stride
// natively (rc 7).
#include "bandtrack/TrackPanel.h"
namespace {
    struct A25TrackPanelPeek : public TrackPanel { // reads protected members via member pointers
        static std::vector<TrackPanel::TrackSlot> TrackPanel::*Slots() {
            return &A25TrackPanelPeek::mTrackSlots;
        }
    };
}
void rb3_app_check_reserved_vocal_slot() { // ring: native-real
    TrackPanel *saved = TheTrackPanel;
    TrackPanel *tp = new TrackPanel();
    std::vector<TrackPanel::TrackSlot> &slots = tp->*A25TrackPanelPeek::Slots();
    char found[64];
    int n = 0;
    bool ok = slots.size() == 5;
    for (int want = 0; want <= (int)slots.size(); want++) {
        for (int i = 0; i < (int)slots.size(); i++) {
            slots[i].mTrack = nullptr;
            slots[i].mInstrument = i == want ? kInstVocals : kInstGuitar;
        }
        tp->UpdateReservedVocalSlot();
        int got = -1;
        for (int i = 0; i < (int)slots.size(); i++)
            if (tp->SlotReservedForVocals(i))
                got = i;
        int expect = want < (int)slots.size() ? want : 4;
        ok = ok && got == expect;
        n += snprintf(found + n, sizeof found - n, "%s%d", want ? " " : "", got);
    }
    for (int i = 0; i < (int)slots.size(); i++)
        slots[i].mInstrument = kInstNone;
    char what[384];
    snprintf(what, sizeof what,
             "reserved vocal slot: vocals in slot 0..4 then none -> reserved %s "
             "(want 0 1 2 3 4 4) (retail TrackPanel::UpdateReservedVocalSlot fn_82B912A8)",
             found);
    delete tp;
    TheTrackPanel = saved;
    if (ok) {
        rb3_app_gate(what);
        return;
    }
    printf("  [FAIL] rb3-app %s\n", what);
    printf("RESULT: GATE FAILED (reserved vocal slot)\n");
    fflush(stdout);
    fflush(stderr);
    _exit(7);
}

// A25: after boot (WorldInit -> Spotlight::Init -> BuildBoard), the shared
// spotlight disk must exist. Retail BuildBoard fn_824DC5E8 builds a 4-vert,
// 2-face RndMesh into sDiskMesh (stw at +0x18); SpotlightDrawer's
// DrawAccessories<LensExtract> and Spotlight's lens/floor-spot draws use it
// unchecked. The native host used to skip the build, so the first lit
// game_screen frame crashed in RndDrawable::Showing. Fails (rc 7) when the
// skip is restored.
#include "world/Spotlight.h"
#include "rndobj/Mesh.h"
void rb3_app_check_spotlight_disk() { // ring: native-real
    RndMesh *disk = Spotlight::GetDiskMesh();
    int verts = disk ? (int)disk->Verts().size() : -1;
    int faces = disk ? (int)disk->Faces().size() : -1;
    char what[256];
    snprintf(what, sizeof what,
             "spotlight disk: sDiskMesh %s, verts %d faces %d (want 4 2) "
             "(retail Spotlight::BuildBoard fn_824DC5E8)",
             disk ? "set" : "NULL", verts, faces);
    if (disk && verts == 4 && faces == 2) {
        rb3_app_gate(what);
        return;
    }
    printf("  [FAIL] rb3-app %s\n", what);
    printf("RESULT: GATE FAILED (spotlight disk)\n");
    fflush(stdout);
    fflush(stderr);
    _exit(7);
}

// A28: song_select's panel `exit` handler runs `{music_library on_exit}`, and
// that DTA message must reach MusicLibrary::OnExit. Retail Handle fn_82542D20
// compares "on_exit" (0x820915B4) and calls OnExit fn_82542A00 (bl at +0xCC),
// whose MusicLibraryStore::ClearPreview (fn_825BC908) deletes the store preview
// manager (+0x4C). Natively the handler's Symbol was the stringified text
// `Symbol("on_exit")` (RB3_HANDLE_LOCAL_STATIC), so the message went unhandled,
// the manager outlived the screen, and the second song_select load FAILed
// "store_preview_mgr already exists". Unlike A21's music_library_exit gate this
// one dispatches through Handle, as the DTA does. Fails (rc 7) when the
// stringified handler comes back.
#include "obj/Msg.h"
void rb3_app_check_music_library_on_exit() { // ring: native-real
    if (!TheMusicLibrary) {
        SongPreview *prev = new SongPreview(TheSongMgr);
        MusicLibrary::Init(*prev);
    }
    MusicLibrary *lib = TheMusicLibrary;
    MusicLibraryStore *prevStore = lib->unk19c;
    lib->OnLoad();
    MusicLibraryStore *store = lib->unk19c;
    bool named0 = ObjectDir::Main()->Find<Hmx::Object>("store_preview_mgr", false) != nullptr;
    lib->unk40 = true;
    static Message msg("on_exit");
    DataNode r = lib->Handle(msg, false);
    bool handled = r.Type() != kDataUnhandled;
    bool previewMgr1 = store && store->mPreviewMgr;
    bool named1 = ObjectDir::Main()->Find<Hmx::Object>("store_preview_mgr", false) != nullptr;
    if (store && store->mPreviewMgr) { // the unhandled path: undo by hand
        delete store->mPreviewMgr;
        store->mPreviewMgr = nullptr;
    }
    lib->unk40 = false;
    delete store;
    lib->unk19c = prevStore;
    TheNetCacheMgr->Unload();
    for (int i = 0; i < 10 && !TheNetCacheMgr->IsUnloaded(); i++)
        TheNetCacheMgr->Poll();
    char what[384];
    snprintf(what, sizeof what,
             "music library on_exit: OnLoad -> store_preview_mgr named %d; Handle(on_exit) "
             "%s; preview mgr %d, named %d (want 1, handled, 0 0) (retail Handle fn_82542D20 "
             "-> OnExit fn_82542A00 -> ClearPreview fn_825BC908)",
             (int)named0, handled ? "handled" : "UNHANDLED", (int)previewMgr1, (int)named1);
    if (store && named0 && handled && !previewMgr1 && !named1) {
        rb3_app_gate(what);
        return;
    }
    printf("  [FAIL] rb3-app %s\n", what);
    printf("RESULT: GATE FAILED (music library on_exit)\n");
    fflush(stdout);
    fflush(stderr);
    _exit(7);
}

// A28: a CharClipGroup must forget a clip that is deleted. Retail Replace
// fn_8238F270 compares each entry's held clip (+8) with the dying object (r4,
// at +0x58) and erases the entry. Natively the compare was against the ObjRef
// the native ObjOwnerPtr passes, so it never matched; the second song's crowd
// anim merge (CharClipGroup::Copy, kCopyFromMax -> FindClip) then read a freed
// clip's name: SIGSEGV in streq (walk w1, core 2812177). Fails (rc 7) when the
// raw compare comes back.
#include "char/CharClipGroup.h"
#include "char/CharClip.h"
void rb3_app_check_clip_group_replace() { // ring: native-real
    CharClipGroup *group = Hmx::Object::New<CharClipGroup>();
    CharClip *a = Hmx::Object::New<CharClip>();
    CharClip *b = Hmx::Object::New<CharClip>();
    a->SetName("a28_clip_a", ObjectDir::Main());
    b->SetName("a28_clip_b", ObjectDir::Main());
    group->AddClip(a);
    group->AddClip(b);
    bool both0 = group->HasClip(a) && group->HasClip(b);
    CharClip *aAddr = a;
    delete a;
    bool keptA = group->HasClip(aAddr); // compares addresses only
    bool keptB = group->HasClip(b);
    bool found = !keptA && group->FindClip("a28_clip_b") == b;
    char what[320];
    snprintf(what, sizeof what,
             "clip group replace: A and B in group %d; A deleted -> group holds A %d, B %d, "
             "FindClip(B) %d (want 1 0 1 1) (retail CharClipGroup::Replace fn_8238F270)",
             (int)both0, (int)keptA, (int)keptB, (int)found);
    if (both0 && !keptA && keptB && found) {
        delete group;
        delete b;
        rb3_app_gate(what);
        return;
    }
    printf("  [FAIL] rb3-app %s\n", what);
    printf("RESULT: GATE FAILED (clip group replace)\n");
    fflush(stdout);
    fflush(stderr);
    _exit(7);
}

// A29: a clip's transition table drops the vector whose ring node is dying, not
// the first vector on the same clip. Retail Transitions::Replace fn_8237D470
// matches by clip (cmplw at +0x24) and drops with RemoveNodes(it->clip)
// fn_8237D3C8 (bl at +0x74), whose FindNodes finds that same `it`. Natively the
// match is by ring node and a retarget (the D2 -> D replace below) leaves two
// vectors on D, so re-finding by clip freed the other vector's node while the
// dying clip's ReplaceRefs snapshot still held it (lane A28 core h2: ~CharClip
// <- Merger::Clear, "calling get obj on abstract ObjRef", SIGSEGV). The check
// calls Replace for the second vector's node directly, so a regression fails
// (rc 7) on node identity before anything is freed under a ring walk.
void rb3_app_check_clip_transition_drop() { // ring: native-real
    CharClip *x = Hmx::Object::New<CharClip>();
    CharClip *d = Hmx::Object::New<CharClip>();
    CharClip *d2 = Hmx::Object::New<CharClip>();
    CharGraphNode node;
    node.curBeat = 0;
    node.nextBeat = 0;
    CharClip::Transitions &t = x->GetTransitions();
    t.AddNode(d2, node);
    t.AddNode(d, node);
    d2->ReplaceRefs(d); // vector 0 retargets onto d: two vectors on d
    bool two = t.Size() == 2 && t.GetNodes(0)->clip == d && t.GetNodes(1)->clip == d;
    ObjRef *first = t.GetNodes(0)->ref;
    ObjRef *second = two ? t.GetNodes(1)->ref : nullptr;
    if (two)
        x->Replace(second, nullptr); // the ring step for the second vector's node
    bool keptFirst = two && t.Size() == 1 && t.GetNodes(0)->ref == first;
    bool cleared = false;
    if (keptFirst) {
        delete d; // the remaining node's own ring step drops the last vector
        cleared = t.Size() == 0;
    }
    char what[320];
    snprintf(what, sizeof what,
             "clip transition drop: D2 retargeted onto D -> two vectors %d; D's second "
             "node steps -> first vector kept %d; D deleted -> table empty %d (want 1 1 1) "
             "(retail Transitions::Replace fn_8237D470 -> RemoveNodes fn_8237D3C8)",
             (int)two, (int)keptFirst, (int)cleared);
    if (two && keptFirst && cleared) {
        delete x;
        delete d2;
        rb3_app_gate(what);
        return;
    }
    printf("  [FAIL] rb3-app %s\n", what);
    printf("RESULT: GATE FAILED (clip transition drop)\n");
    fflush(stdout);
    fflush(stderr);
    _exit(7);
}

// A30: BandCharDesc::sDrumVenueMappings is retail's 11-word table at 0x82C6D728
// (NameToDrumVenue fn_82334250, lis/addi at +0xC/+0x14): five {name, venue}
// pairs and a "" terminator (0x82000C55) that ends the loop (lbz at +0x5C). The
// decomp had ten entries, wrong venues and no terminator, so an unmatched name
// walked off the array (lane A29 ASan: SetTempoGenreVenue <- OnUnloadVenue).
// The check never reads past the table: it takes the count from the defining TU
// and finds the loop's terminator inside it before calling NameToDrumVenue with
// a miss, so a missing terminator fails (rc 7) without ASan.
int BandCharDescDrumVenueMappingCount(); // BandCharDesc.cpp (HX_NATIVE)
void rb3_app_check_drum_venue_lookup() { // ring: native-real
    const char **tbl = BandCharDesc::sDrumVenueMappings;
    int count = BandCharDescDrumVenueMappingCount();
    int term = -1; // the even index whose string is "": where the retail loop stops
    for (int i = 0; i < count; i += 2) {
        if (*tbl[i] == 0) {
            term = i;
            break;
        }
    }
    bool bounded = count == 11 && term == 10;
    static const char *const kWant[][2] = { { "small_club", "small_club" },
                                            { "arena", "arena" },
                                            { "big_club", "big_club" },
                                            { "festival", "arena" },
                                            { "video", "big_club" } };
    int matched = 0;
    for (const auto &w : kWant) {
        char name[64];
        snprintf(name, sizeof name, "venue_%s_test", w[0]);
        if (BandCharDesc::NameToDrumVenue(name) == Symbol(w[1]))
            matched++;
    }
    bool miss = bounded && BandCharDesc::NameToDrumVenue("no_such_venue") == Symbol(tbl[0]);
    char what[320];
    snprintf(what, sizeof what,
             "drum venue lookup: table %d entries, terminator at %d (want 11, 10); "
             "pairs matched %d (want 5); miss -> %s %d (want 1) "
             "(retail NameToDrumVenue fn_82334250, table 0x82C6D728)",
             count, term, matched, tbl[0], (int)miss);
    if (bounded && matched == 5 && miss) {
        rb3_app_gate(what);
        return;
    }
    printf("  [FAIL] rb3-app %s\n", what);
    printf("RESULT: GATE FAILED (drum venue lookup)\n");
    fflush(stdout);
    fflush(stderr);
    _exit(7);
}

// A31: Hmx::Object::ReplaceRefs steps every ref of a dying object, including a
// ref that an owner's Replace moves back onto it. A merge retarget (E -> D)
// leaves CharClipGroup holding D twice; deleting D steps the first entry,
// whose erase (retail CharClipGroup::Replace fn_8238F270, erase at +0xD8)
// shifts D's second entry down a slot (AddRef'd back onto D) and X into the
// second slot. The native vector snapshot then stepped the second slot's
// address, now X's entry, so the group dropped X and kept D after D was freed:
// the ASan heap-use-after-free in ObjRef::AddRef under ~CharClip <-
// FileMerger::Merger::Clear (A31 asan1) and A28's endgame SIGSEGV. The check
// reads only addresses until the group is known to be clean, so a regression
// fails (rc 7) before anything touches the freed clip.
void rb3_app_check_replace_refs_shifted_entry() { // ring: native-real
    CharClipGroup *group = Hmx::Object::New<CharClipGroup>();
    CharClip *d = Hmx::Object::New<CharClip>();
    CharClip *e = Hmx::Object::New<CharClip>();
    CharClip *x = Hmx::Object::New<CharClip>();
    d->SetName("a31_clip_d", ObjectDir::Main());
    e->SetName("a31_clip_e", ObjectDir::Main());
    x->SetName("a31_clip_x", ObjectDir::Main());
    group->AddClip(d);
    group->AddClip(e);
    group->AddClip(x);
    e->ReplaceRefs(d); // the merge retarget: the group now holds D, D, X
    bool twice = d->RefCount() == 2 && !group->HasClip(e) && x->RefCount() == 1;
    CharClip *dAddr = d;
    delete d;
    bool keptD = group->HasClip(dAddr); // compares addresses only
    bool keptX = group->HasClip(x);
    int xRefs = (!keptD && keptX) ? x->RefCount() : -1;
    char what[320];
    snprintf(what, sizeof what,
             "replace refs shifted entry: group holds D twice %d; D deleted -> group holds D %d, "
             "X %d, X refs %d (want 1 0 1 1) (retail CharClipGroup::Replace fn_8238F270)",
             (int)twice, (int)keptD, (int)keptX, xRefs);
    if (twice && !keptD && keptX && xRefs == 1) {
        delete group;
        delete e;
        delete x;
        rb3_app_gate(what);
        return;
    }
    printf("  [FAIL] rb3-app %s\n", what);
    printf("RESULT: GATE FAILED (replace refs shifted entry)\n");
    fflush(stdout);
    fflush(stderr);
    _exit(7);
}

// A32: ~Object's ReplaceRefs steps each ref of a dying object with
// Replace(nullptr) -> SetObj(nullptr), and it runs after the derived
// destructors. For a CharBonesBlender, ~CharBones has by then reset the shared
// vptr of the CharBonesObject subobject to CharBones' own vtable, which has no
// virtual-base slot. SetObj compared `mObject != root_obj`, which converts
// mObject to its virtual base Hmx::Object and so reads the virtual-base offset
// from that vtable: the ASan global-buffer-overflow 8 bytes before `vtable for
// CharBones` on every walk (A31, ~CharBonesBlender <- FileMerger::Merger::Clear
// on the BandCharacter reload). The ref and its ring were sound. The check
// makes that slot deterministic: a live blender's vptr is pointed, for one
// Replace(nullptr) with the rings dirty as in that walk, at a copy of its
// vtable header whose virtual-base slot would convert mObject to null. A
// compare that reads the slot finds `null == null` and keeps the ref (rc 7);
// the T1* compare clears it. Then a real delete of a held blender.
#include "char/CharBonesBlender.h"
#include <csetjmp>
#include <csignal>
#include <sys/mman.h>
namespace {
    sigjmp_buf gA32Jmp;
    volatile sig_atomic_t gA32Faulted;
    void A32Fault(int) {
        gA32Faulted = 1;
        siglongjmp(gA32Jmp, 1);
    }
    // Runs fn with the vptr of bones' CharBonesObject subobject aimed into a
    // PROT_NONE page, with the rings clean (sRingsDirty false), and reports
    // whether fn read through that vptr. A ref unlinking itself from a referent
    // has no reason to read the referent's vtable.
    template <class F>
    int A32ReadsVtable(CharBonesObject *bones, F fn) {
        if (ObjectDir::InDeleteObjects())
            return -1; // the Release path is not reached during a cascade
        long pg = sysconf(_SC_PAGESIZE);
        void *page = mmap(nullptr, pg, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (page == MAP_FAILED)
            return -1;
        void **volatile slot = reinterpret_cast<void **>(bones);
        void *volatile real = *slot;
        bool dirty = Hmx::Object::sRingsDirty;
        struct sigaction sa = {}, oldSegv, oldBus;
        sa.sa_handler = A32Fault;
        sigemptyset(&sa.sa_mask);
        sigaction(SIGSEGV, &sa, &oldSegv);
        sigaction(SIGBUS, &sa, &oldBus);
        gA32Faulted = 0;
        if (sigsetjmp(gA32Jmp, 1) == 0) {
            Hmx::Object::sRingsDirty = false;
            *slot = static_cast<char *>(page) + pg / 2;
            fn();
        }
        *slot = real;
        Hmx::Object::sRingsDirty = dirty;
        sigaction(SIGSEGV, &oldSegv, nullptr);
        sigaction(SIGBUS, &oldBus, nullptr);
        munmap(page, pg);
        return gA32Faulted ? 1 : 0;
    }
}
void rb3_app_check_bones_ref_dying_compare() { // ring: native-real
    Hmx::Object *owner = Hmx::Object::New<Hmx::Object>();
    CharBonesBlender *blender = Hmx::Object::New<CharBonesBlender>();
    CharBonesObject *bones = blender;
    ObjPtr<CharBonesObject> ref(owner, bones);
    bool held = ref.Ptr() == bones && blender->RefCount() == 1;
    void **vptrSlot = reinterpret_cast<void **>(bones);
    void *realVptr = *vptrSlot;
    const intptr_t *realHdr = reinterpret_cast<const intptr_t *>(realVptr) - 3;
    intptr_t vbase = static_cast<intptr_t>(
        reinterpret_cast<char *>(static_cast<Hmx::Object *>(bones)) - reinterpret_cast<char *>(bones));
    bool layout = realHdr[0] == vbase; // Itanium: virtual-base offset at vptr - 3 words
    int cleared = -1, refsAfter = -1;
    if (held && layout) {
        intptr_t fakeHdr[3] = { -reinterpret_cast<intptr_t>(bones), realHdr[1], realHdr[2] };
        bool dirty = Hmx::Object::sRingsDirty;
        Hmx::Object::sRingsDirty = true;
        // A virtual Replace through an opaque ObjRef*, as ReplaceRefs steps it,
        // so the compare is not folded against a constant null.
        ObjRef *volatile step = &ref;
        *vptrSlot = fakeHdr + 3; // no virtual call goes through it in SetObj(nullptr)
        step->Replace(nullptr);
        *vptrSlot = realVptr;
        Hmx::Object::sRingsDirty = dirty;
        cleared = ref.Ptr() == nullptr;
        refsAfter = blender->RefCount();
    }
    // The unlink paths with the rings clean: SetObjConcrete(nullptr) (reached
    // through Replace) and ~ObjRefConcrete, each on a held, live blender.
    int setReads = -1, setCleared = -1, dtorReads = -1, refsClean = -1;
    if (refsAfter == 0) {
        ObjPtr<CharBonesObject> ref3(owner, bones);
        ObjRef *volatile step3 = &ref3;
        setReads = A32ReadsVtable(bones, [&] { step3->Replace(nullptr); });
        setCleared = ref3.Ptr() == nullptr && blender->RefCount() == 0;
        ObjPtr<CharBonesObject> *ref4 = new ObjPtr<CharBonesObject>(owner, bones);
        if (setReads == 0)
            dtorReads = A32ReadsVtable(bones, [&] { delete ref4; });
        refsClean = blender->RefCount() == 0;
    }
    CharBonesBlender *blender2 = Hmx::Object::New<CharBonesBlender>();
    ObjPtr<CharBonesObject> ref2(owner, blender2);
    delete blender2; // the walk's path: ~Object -> ReplaceRefs -> SetObj(nullptr)
    bool deleted = ref2.Ptr() == nullptr;
    char what[512];
    snprintf(what, sizeof what,
             "bones ref dying compare: held %d, vbase slot found %d; with the "
             "slot reading null -> Replace(nullptr) cleared %d, blender refs %d; rings clean: "
             "Replace(nullptr) reads the vtable %d, cleared %d, ~ref reads it %d, refs 0 %d; "
             "deleted blender -> ref null %d (want 1 1 1 0 0 1 0 1 1)",
             (int)held, (int)layout, cleared, refsAfter, setReads, setCleared, dtorReads,
             refsClean, (int)deleted);
    if (held && layout && cleared == 1 && refsAfter == 0 && setReads == 0 && setCleared == 1
        && dtorReads == 0 && refsClean == 1 && deleted) {
        delete blender;
        delete owner;
        rb3_app_gate(what);
        return;
    }
    printf("  [FAIL] rb3-app %s\n", what);
    printf("RESULT: GATE FAILED (bones ref dying compare)\n");
    fflush(stdout);
    fflush(stderr);
    _exit(7);
}

// A33: TypeProps' native setters must free ONE mObjects node per overwritten
// value. Retail releases one ring entry per value: SetKeyValue fn_82765D40
// +0x170, SetArrayValue fn_827665D0 +0x48 and RemoveArrayValue fn_82766650
// +0x48 each `bl` Hmx::Object::Release (fn_8275B378), which erases the first
// entry owned by the TypeProps (+0x7C) and returns. The native arms called
// ObjPtrList::remove(obj), which frees EVERY node holding obj: with X held by
// two props, overwriting one dropped the other's ring entry, and when X died
// that prop kept X's freed address. Here X is held by "a33_keep" and, in turn,
// by a second prop that SetKeyValue overwrites, by an array element that
// SetArrayValue overwrites, by one that RemoveArrayValue removes, and by a prop
// ClearKeyValue clears. After each, X must still carry exactly the one ring
// entry of "a33_keep"; then `delete X` must leave "a33_keep" null. The values
// are only compared as pointers, never dereferenced. Fails with remove(obj)
// natively (rc 7).
namespace {
    Hmx::Object *A33Val(TypeProps *tp, const char *key) {
        DataNode *n = tp->KeyValue(Symbol(key), false);
        return n && n->Type() == kDataObject ? n->UncheckedObj() : (Hmx::Object *)-1;
    }
}
void rb3_app_check_typeprops_remove_one() { // ring: native-real
    RndTransformable *owner = Hmx::Object::New<RndTransformable>();
    RndTransformable *x = Hmx::Object::New<RndTransformable>();
    RndTransformable *w = Hmx::Object::New<RndTransformable>();
    TypeProps *tp = new TypeProps(owner);
    int x0 = x->RefCount();
    tp->SetKeyValue(Symbol("a33_keep"), DataNode(x), true);
    tp->SetKeyValue(Symbol("a33_over"), DataNode(x), true);
    bool set = x->RefCount() - x0 == 2;
    tp->SetKeyValue(Symbol("a33_over"), DataNode(w), true);
    int afterKey = x->RefCount() - x0;
    DataArray *arr = new DataArray(0);
    tp->SetKeyValue(Symbol("a33_arr"), DataNode(arr, kDataArray), false);
    arr->Release();
    tp->InsertArrayValue(Symbol("a33_arr"), 0, DataNode(x), nullptr);
    tp->SetArrayValue(Symbol("a33_arr"), 0, DataNode(w), nullptr);
    int afterSetArr = x->RefCount() - x0;
    tp->InsertArrayValue(Symbol("a33_arr"), 0, DataNode(x), nullptr);
    tp->RemoveArrayValue(Symbol("a33_arr"), 0, nullptr);
    int afterRemoveArr = x->RefCount() - x0;
    tp->SetKeyValue(Symbol("a33_clear"), DataNode(x), true);
    tp->ClearKeyValue(Symbol("a33_clear"));
    int afterClear = x->RefCount() - x0;
    bool keepIsX = A33Val(tp, "a33_keep") == x;
    delete x; // ~Object's ReplaceRefs(nullptr) walk must reach "a33_keep"'s node
    bool keepNull = A33Val(tp, "a33_keep") == nullptr;
    bool ok = set && afterKey == 1 && afterSetArr == 1 && afterRemoveArr == 1 && afterClear == 1
        && keepIsX && keepNull;
    char what[512];
    snprintf(what, sizeof what,
             "typeprops remove one: X refs from the TypeProps after SetKeyValue %d, "
             "SetArrayValue %d, RemoveArrayValue %d, ClearKeyValue %d (want 1 1 1 1); set %d, "
             "keep is X %d, keep null after delete X %d (want 1) (retail Release per value: "
             "fn_82765D40+0x170, fn_827665D0+0x48, fn_82766650+0x48)",
             afterKey, afterSetArr, afterRemoveArr, afterClear, (int)set, (int)keepIsX,
             (int)keepNull);
    if (ok) {
        delete tp;
        delete owner;
        delete w;
        rb3_app_gate(what);
        return;
    }
    printf("  [FAIL] rb3-app %s\n", what);
    printf("RESULT: GATE FAILED (typeprops remove one)\n");
    fflush(stdout);
    fflush(stderr);
    _exit(7);
}

// A34: a fresh CharPollableSorter::Dep's searchID. Retail's mDeps operator[]
// (fn_82373BB0) never writes the default value's searchID, and the Dep copy
// (fn_823710E8 +0x3C) carries that stack slot into the node, so
// ChangedByRecurse (fn_8236E980 +0x30..+0x38) compares garbage with sSearchID
// and, on a match, skips the fresh dep's changed-by chain. Natively Dep() now
// zeroes it (char/CharPollable.h, HX_NATIVE), and sSearchID is >= 1 inside
// every search (Sort and ChangedBy increment it first). The check makes the
// garbage deterministic: two Deps are placement-built over bytes holding the
// next search's ID, chained a -> f -> target, and ChangedBy(a, target) must
// find the target. A Dep() that leaves searchID alone early-outs at a (rc 7).
// Then a real mDeps[] node must start at 0. Keys are compared, never
// dereferenced.
#include "char/CharPollable.h"
namespace {
    struct A34Sorter : CharPollableSorter {
        static int NextSearchID() { return sSearchID + 1; }
        static int SearchID() { return sSearchID; }
        bool Changed(Dep *a, Dep *b) { return ChangedBy(a, b); }
        Dep &Node(Hmx::Object *key) { return mDeps[key]; }
    };
    using A34Dep = CharPollableSorter::Dep;
    // A Dep built over storage whose every int already holds `fill`.
    A34Dep *A34DepOver(void *storage, int fill) {
        int *words = static_cast<int *>(storage);
        for (size_t i = 0; i < sizeof(A34Dep) / sizeof(int); i++)
            words[i] = fill;
        return new (storage) A34Dep();
    }
}
void rb3_app_check_poll_sorter_search_id() {
    A34Sorter sorter;
    int next = A34Sorter::NextSearchID();
    alignas(A34Dep) unsigned char bufA[sizeof(A34Dep)];
    alignas(A34Dep) unsigned char bufF[sizeof(A34Dep)];
    A34Dep *a = A34DepOver(bufA, next);
    A34Dep *f = A34DepOver(bufF, next);
    A34Dep target;
    a->changedBy.push_back(f);
    f->changedBy.push_back(&target);
    int freshA = a->searchID, freshF = f->searchID;
    bool found = sorter.Changed(a, &target);
    bool searched = A34Sorter::SearchID() == next; // the search we pre-filled for
    bool stamped = a->searchID == next && f->searchID == next;
    // A second search over the same chain: the stamps are the old ID now.
    bool again = sorter.Changed(a, &target);
    // A real map node, keyed by an address that is never dereferenced.
    A34Dep &node = sorter.Node(reinterpret_cast<Hmx::Object *>(&target));
    int nodeID = node.searchID;
    a->~A34Dep();
    f->~A34Dep();
    char what[384];
    snprintf(what, sizeof what,
             "poll sorter search id: fresh deps over bytes = next id %d read %d %d (want 0 0); "
             "ChangedBy(a, target) through a -> f -> target found %d, search id %d, both stamped "
             "%d; second search found %d; fresh mDeps node %d (want 1 1 1 1 0)",
             next, freshA, freshF, (int)found, (int)searched, (int)stamped, (int)again, nodeID);
    if (freshA == 0 && freshF == 0 && found && searched && stamped && again && nodeID == 0) {
        rb3_app_gate(what);
        return;
    }
    printf("  [FAIL] rb3-app %s\n", what);
    printf("RESULT: GATE FAILED (poll sorter search id)\n");
    fflush(stdout);
    fflush(stderr);
    _exit(7);
}

// A36: a deleted cube map leaves the engine's GPU table.
// Retail's cube map is DxCubeTex; its dtor (fn_8273F040) runs DxCubeTex::Reset
// (fn_8273EFF0, bl at +0x2C), which releases the D3D texture at 0x1A4, before
// ??1RndCubeTex (fn_82495410, bl at +0x34). Natively the GPU copy sits in
// Tex_Wgpu.cpp's sCubeTexGpuData, keyed on the RndCubeTex ADDRESS, and
// GetGpuCubeTexView serves any entry marked uploaded. Here: cube A gets six
// 16x16 faces and is uploaded through GetGpuCubeTexView (the draw path's call);
// then `delete A`. A's entry must be gone (GetGpuCubeTexMipLevels -1, holders
// one fewer), and a fresh faceless cube B must get no view. B is often placed
// at A's address; with a stale entry B would be served A's faces. Fails when
// ~RndCubeTex (rndobj/CubeTex.cpp, HX_NATIVE) does not call CleanupGpuCubeTex.
// A's freed block is only used as a map key, never dereferenced.
#include "rndobj/CubeTex.h"
void rb3_app_check_cube_tex_gpu_cleanup() { // ring: native-real
    RndCubeTex *a = Hmx::Object::New<RndCubeTex>();
    bool px = true;
    for (int f = 0; f < RndCubeTex::kNumCubeFaces; f++) {
        RndBitmap &bm = a->GetBitmap((RndCubeTex::CubeFace)f);
        bm.Create(16, 16, 0, 32, 0, nullptr, nullptr, nullptr);
        u8 *p = bm.Pixels();
        if (!p) {
            px = false;
            continue;
        }
        for (int i = 0; i < bm.PixelBytes(); i++)
            p[i] = (u8)(i * 5 + f * 37 + 0x11);
    }
    bool view0 = (bool)GetGpuCubeTexView(a);
    int mips0 = GetGpuCubeTexMipLevels(a);
    size_t holders0 = GpuTexHolderCount();
    void *aAddr = a;
    delete a;
    int mips1 = GetGpuCubeTexMipLevels((RndCubeTex *)aAddr);
    size_t holders1 = GpuTexHolderCount();
    RndCubeTex *b = Hmx::Object::New<RndCubeTex>();
    bool sameAddr = (void *)b == aAddr;
    bool viewB = (bool)GetGpuCubeTexView(b);
    delete b;
    size_t holders2 = GpuTexHolderCount();
    char what[512];
    snprintf(what, sizeof what,
             "cube tex gpu cleanup: pixels %d; A view %d mips %d, holders %zu; delete A -> A "
             "mips %d (want -1), holders %zu (want %zu); fresh faceless B view %d (want 0, B at "
             "A's address %d); after delete B holders %zu (retail DxCubeTex dtor 0x8273F040 -> "
             "Reset 0x8273EFF0)",
             (int)px, (int)view0, mips0, holders0, mips1, holders1, holders0 - 1, (int)viewB,
             (int)sameAddr, holders2);
    bool ok = px && view0 && mips0 >= 1 && mips1 == -1 && holders1 == holders0 - 1 && !viewB
        && holders2 == holders1;
    if (ok) {
        rb3_app_gate(what);
        return;
    }
    printf("  [FAIL] rb3-app %s\n", what);
    printf("RESULT: GATE FAILED (cube tex gpu cleanup)\n");
    fflush(stdout);
    fflush(stderr);
    _exit(7);
}

// A36: a fresh SessionSettings is unranked.
// Neither the SessionSettings ctor (retail fn_823F3C78) nor Clear
// (fn_823F3BE8) writes mRanked@0xC; retail's heap supplies it until
// BandMatchmaker::OnMsg(ModeChangedMsg) stores SetRanked(IsRanked()). Natively
// the ctor now starts it at false, the quickplay value (MatchmakingSettings.cpp,
// HX_NATIVE). The check makes the garbage deterministic: a SessionSettings is
// placement-built over bytes that are all 0x01, and must read Ranked() false,
// with Clear's mModeFilter -1 and mPublic false. Then Clear on a ranked
// settings must keep it ranked, as retail's Clear does. Fails (rc 7) when the
// HX_NATIVE init is dropped.
#include "net/MatchmakingSettings.h"
void rb3_app_check_session_settings_unranked() { // ring: native-real
    alignas(SessionSettings) unsigned char buf[sizeof(SessionSettings)];
    memset(buf, 0x01, sizeof buf);
    SessionSettings *s = new (buf) SessionSettings();
    int ranked0 = (int)s->Ranked();
    int filter0 = s->ModeFilter();
    int public0 = (int)s->mPublic;
    s->mRanked = true; // SetRanked would SetSyncDirty; the field itself is the subject
    s->Clear();
    int rankedAfterClear = (int)s->Ranked();
    s->~SessionSettings();
    char what[384];
    snprintf(what, sizeof what,
             "session settings unranked: fresh over 0x01 bytes -> ranked %d (want 0), mode "
             "filter %d (want -1), public %d (want 0); ranked then Clear -> ranked %d (want 1) "
             "(retail ctor 0x823F3C78 and Clear 0x823F3BE8 write no 0xC)",
             ranked0, filter0, public0, rankedAfterClear);
    if (ranked0 == 0 && filter0 == -1 && public0 == 0 && rankedAfterClear == 1) {
        rb3_app_gate(what);
        return;
    }
    printf("  [FAIL] rb3-app %s\n", what);
    printf("RESULT: GATE FAILED (session settings unranked)\n");
    fflush(stdout);
    fflush(stderr);
    _exit(7);
}

// A35: Rnd::DrawPreClear services the texture-compress queue natively. Retail
// fn_824158B8 retires the front CompressTexDesc once CompressThread is done
// (+0x0A0 erase, +0x0A8 the dtor fn_824118D8, which calls TextureCompressed with
// the request's address at +0x02C..+0x040) and drops requests whose texture died
// or whose callback was cancelled (+0x10C..+0x124). CompressTexture
// (fn_82415B68 +0x070) returns that address as the ID. Natively nothing drained
// the queue, so BandCharacter::IsLoading (its mCompressedTextureIDs) stayed true
// for good once OutfitConfig::DrawPreClear ran, and game_screen never loaded;
// and the ID was an `int`, so the 64-bit callback argument never matched it.
// Three requests: a live one, a cancelled one and one whose texture is deleted.
// Within four DrawPreClear passes the queue must be empty; the live and the
// dead-texture callbacks must each have run exactly once, with the very ID
// CompressTexture returned (the prune destroys a dead request through the same
// dtor, +0x150 bl fn_824118D8, so its ID is retired too); the cancelled one must
// never call back.
#include "rndobj/Rnd.h"
namespace {
    struct A35CompressProbe : Rnd::CompressTextureCallback {
        intptr_t mGot;
        int mCalls;
        A35CompressProbe() : mGot(0), mCalls(0) {}
        virtual void TextureCompressed(intptr_t id) {
            mGot = id;
            mCalls++;
        }
    };
    struct A35RndPeek : Rnd {
        static void PreClear(Rnd &r) { static_cast<A35RndPeek &>(r).Rnd::DrawPreClear(); }
        static int Queued(Rnd &r) {
            return (int)static_cast<A35RndPeek &>(r).mCompressTexQueue.size();
        }
        static int Draws(Rnd &r) { return (int)static_cast<A35RndPeek &>(r).mDraws.size(); }
    };
}
void rb3_app_check_compress_queue_drain() { // ring: native-real
    RndTex *liveTex = Hmx::Object::New<RndTex>();
    RndTex *cancelTex = Hmx::Object::New<RndTex>();
    RndTex *deadTex = Hmx::Object::New<RndTex>();
    A35CompressProbe live, cancelled, dead;
    int before = A35RndPeek::Queued(TheRnd);
    intptr_t liveID = TheRnd.CompressTexture(liveTex, (RndTex::AlphaCompress)0, &live);
    TheRnd.CompressTexture(cancelTex, (RndTex::AlphaCompress)0, &cancelled);
    intptr_t deadID = TheRnd.CompressTexture(deadTex, (RndTex::AlphaCompress)0, &dead);
    TheRnd.CompressTextureCancel(&cancelled);
    delete deadTex;
    int queued = A35RndPeek::Queued(TheRnd);
    int draws = A35RndPeek::Draws(TheRnd);
    int passes = 0;
    while (passes < 4 && A35RndPeek::Queued(TheRnd) > before) {
        A35RndPeek::PreClear(TheRnd);
        passes++;
    }
    int left = A35RndPeek::Queued(TheRnd) - before;
    bool sameID = live.mGot == liveID && dead.mGot == deadID;
    // Whatever stays queued holds probes on this stack; never leave it behind.
    if (left > 0) {
        TheRnd.CompressTextureCancel(&live);
        TheRnd.CompressTextureCancel(&dead);
    }
    delete liveTex;
    delete cancelTex;
    char what[384];
    snprintf(what, sizeof what,
             "compress queue drain: queued %d (+%d already, mDraws %d), %d DrawPreClear pass(es), "
             "%d left; callbacks live %d dead tex %d, same ids %d; cancelled %d "
             "(want 3, 0 left, 1 1 1, 0) (retail fn_824158B8 +0x0A8/+0x150 -> fn_824118D8)",
             queued - before, before, draws, passes, left, live.mCalls, dead.mCalls,
             (int)sameID, cancelled.mCalls);
    if (queued - before == 3 && left == 0 && live.mCalls == 1 && dead.mCalls == 1 && sameID
        && cancelled.mCalls == 0) {
        rb3_app_gate(what);
        return;
    }
    printf("  [FAIL] rb3-app %s\n", what);
    printf("RESULT: GATE FAILED (compress queue drain)\n");
    fflush(stdout);
    fflush(stderr);
    _exit(7);
}

// A38: no screen is skipped by its name. Retail UIManager::GotoScreenImpl
// (fn_828036E8) has no name test: its first test is the force flag, `clrlwi.
// r11,r5,24` at +0x01C, then mTransitionState (+0x10) and mCurrentScreen
// (+0x2c), then CancelTransition (`bl fn_82802FA8` at +0x060). A
// DC3-inherited native early return for any name containing "campaign"
// dropped campaign_rewardvignette_firstgig_screen, which meta_loading pushes
// after a band's first song; PushScreen (fn_828047E8) had already cleared
// mCurrentScreen, so the UI was left with no screen and no transition for the
// rest of the run. The probe uses A16's private UIManager: goto a bare screen,
// push a bare screen named "a38_campaign_probe_screen", pop it. Want: the
// pushed screen becomes current, and the pop returns to the first. Fails (rc 7)
// when the name skip comes back.
void rb3_app_check_campaign_screen_push() { // ring: native-real
    alignas(UIManager) static unsigned char storage[sizeof(UIManager)];
    memset(storage, 0, sizeof storage);
    UIManager *ui = new (storage) UIManager();
    UIManager *saved = TheUI;
    TheUI = ui;
    Rb3AppCountingScreen *a = new Rb3AppCountingScreen();
    Rb3AppCountingScreen *c = new Rb3AppCountingScreen();
    c->SetName("a38_campaign_probe_screen", ObjectDir::Main());
    ui->GotoScreen(a, false, false);
    bool first = Rb3AppSettle(ui, a);
    bool pushed = false, popped = false;
    UIScreen *afterPush = nullptr;
    if (first) {
        ui->PushScreen(c);
        pushed = Rb3AppSettle(ui, c);
        afterPush = ui->CurrentScreen();
        if (pushed) {
            ui->PopScreen(nullptr);
            popped = Rb3AppSettle(ui, a);
        }
    }
    TheUI = saved;
    const char *afterName =
        afterPush ? (afterPush == c ? "the campaign probe" : "another screen") : "(none)";
    c->SetName(nullptr, nullptr);
    char what[512];
    snprintf(what, sizeof what,
             "campaign screen push: goto A settled %d; push 'a38_campaign_probe_screen' "
             "settled %d, current %s; pop back to A %d (want 1, 1 the campaign probe, 1) "
             "(retail GotoScreenImpl fn_828036E8 has no name test: first test +0x01C, "
             "CancelTransition +0x060)",
             (int)first, (int)pushed, afterName, (int)popped);
    if (first && pushed && popped) {
        rb3_app_gate(what);
        return;
    }
    printf("  [FAIL] rb3-app %s\n", what);
    printf("RESULT: GATE FAILED (campaign screen push)\n");
    fflush(stdout);
    fflush(stderr);
    _exit(7);
}

// A37: a kObjListNoNull ObjPtrList must drop the entry of a referent that dies
// inside a ReplaceList. ~Object runs ReplaceRefs(nullptr) with gInReplaceList
// set (obj/Object.cpp), and each ring ref of the dying object is stepped with
// Replace(nullptr) -> ObjPtrList::ReplaceNode. BandCharacter::SyncObjects's
// drain (retail fn_8228A2A0 +0x1B8..+0x1EC) is
//     while (unk610.size() /* 0x688 */) { df = head(0x68c)->obj;
//         df->Mesh() /* lwz r3,0x30(r30), no null test, +0x1C4 */ ...;
//         delete df /* vtbl[0](df, 1), +0x1E0 */; }
// so retail relies on that erase: the list holds no null, and each delete
// shrinks it by one. A native guard once suppressed the erase whenever
// gInReplaceList was set (copied from ObjPtrVec, whose inline nodes do move on
// erase), which left a null at front(): the RB3_BAND_PLACE SIGSEGV in
// RndMeshDeform::Mesh() <- SyncObjects, and a loop that could never empty. X9
// (278a38920) removed it for ObjPtrList. The check: two deforms in a NoNull
// list; delete the first -> one entry left, and it is the second; then the
// SyncObjects-shaped drain, bounded at four steps, must empty the list in one
// step without ever reading a null front. Only addresses are compared.
#include "rndobj/MeshDeform.h"
void rb3_app_check_nonull_list_replace_drain() { // ring: native-real
    RndMeshDeform *owner = Hmx::Object::New<RndMeshDeform>();
    RndMeshDeform *a = Hmx::Object::New<RndMeshDeform>();
    RndMeshDeform *b = Hmx::Object::New<RndMeshDeform>();
    ObjPtrList<RndMeshDeform> *list = new ObjPtrList<RndMeshDeform>(owner, kObjListNoNull);
    list->push_back(a);
    list->push_back(b);
    int sizeHeld = (int)list->size();
    delete a; // ~Object -> ReplaceRefs(nullptr) under gInReplaceList
    int sizeAfter = (int)list->size();
    int nulls = 0;
    for (ObjPtrList<RndMeshDeform>::iterator it = list->begin(); it != list->end(); ++it)
        if (!*it) nulls++;
    bool frontIsB = sizeAfter > 0 && list->front() == b;
    int steps = 0, nullFront = 0;
    while (!list->empty() && steps < 4) {
        RndMeshDeform *df = list->front();
        if (!df) { nullFront = 1; break; }
        delete df;
        steps++;
    }
    int left = (int)list->size();
    char what[384];
    snprintf(what, sizeof what,
             "nonull list replace drain: held %d; first deleted -> size %d, nulls %d, front is "
             "the second %d; drain steps %d, null front %d, left %d (want 2, 1 0 1, 1 0 0) "
             "(retail SyncObjects fn_8228A2A0 +0x1B8..+0x1EC)",
             sizeHeld, sizeAfter, nulls, (int)frontIsB, steps, nullFront, left);
    if (sizeHeld == 2 && sizeAfter == 1 && nulls == 0 && frontIsB && steps == 1 &&
        !nullFront && left == 0) {
        delete list;
        delete owner;
        rb3_app_gate(what);
        return;
    }
    printf("  [FAIL] rb3-app %s\n", what);
    printf("RESULT: GATE FAILED (nonull list replace drain)\n");
    fflush(stdout);
    fflush(stderr);
    _exit(7);
}

// A41: the placed band had no hair. Each hair piece's bone_hair_* bones are merged
// into the outfit dir, parented to `bone_hair.mesh` of the piece's
// skeleton_unshared.milo, which the native FilterSubdir shim keeps as a shared
// subdir instead of walking. Retail walks it through Filter's sBoneMergeDir arm
// (band.exe TU5 fn_82285868 +0x26C..+0x2B4, then ::ReplaceRefs fn_82281F00 at
// +0x210), which repoints the merge's references onto the member's OWN bone, so
// the hair follows the member's head. Without it every member's hair sits on the
// one shared skeleton at the venue origin. The check builds that shape: a kept dir
// holding the skeleton as a subdir, a merged hair bone in the outfit dir parented
// to the skeleton's bone_hair, a bystander bone outside the merge's dirs parented
// to the same shared bone, and the member's own bone_hair. Only addresses are
// compared.
#include "rndobj/Trans.h"
int BandCharacterGateBoneMergeRepoint(ObjectDir *self, ObjectDir *kept, ObjectDir *skeleton,
                                      ObjectDir *toDir); // BandCharacter.cpp (HX_NATIVE)
void rb3_app_check_bone_merge_repoint() { // ring: native-real
    ObjectDir *self = Hmx::Object::New<ObjectDir>();
    ObjectDir *kept = Hmx::Object::New<ObjectDir>();
    ObjectDir *skeleton = Hmx::Object::New<ObjectDir>();
    ObjectDir *outfit = Hmx::Object::New<ObjectDir>();
    ObjectDir *other = Hmx::Object::New<ObjectDir>();
    RndTransformable *own = Hmx::Object::New<RndTransformable>();
    RndTransformable *shared = Hmx::Object::New<RndTransformable>();
    RndTransformable *pelvis = Hmx::Object::New<RndTransformable>();
    RndTransformable *hair = Hmx::Object::New<RndTransformable>();
    RndTransformable *bystander = Hmx::Object::New<RndTransformable>();
    own->SetName("bone_hair.mesh", self);
    shared->SetName("bone_hair.mesh", skeleton);
    pelvis->SetName("bone_pelvis.mesh", skeleton); // no own copy: left alone
    hair->SetName("bone_hair_a41.mesh", outfit);
    bystander->SetName("bone_hair_a41.mesh", other);
    hair->SetTransParent(shared, false);
    bystander->SetTransParent(shared, false);
    kept->AppendSubDir(ObjDirPtr<ObjectDir>(skeleton));
    int n = BandCharacterGateBoneMergeRepoint(self, kept, skeleton, outfit);
    bool hairOwn = hair->TransParent() == own;
    bool bystanderShared = bystander->TransParent() == shared;
    char what[320];
    snprintf(what, sizeof what,
             "bone merge repoint: %d skeleton bone(s) repointed, merged hair bone parent is the "
             "member's own %d, bystander keeps the shared one %d (want 1 1 1) (retail "
             "fn_82285868 +0x26C..+0x2B4 -> fn_82281F00)",
             n, (int)hairOwn, (int)bystanderShared);
    if (n == 1 && hairOwn && bystanderShared) {
        rb3_app_gate(what);
        return;
    }
    printf("  [FAIL] rb3-app %s\n", what);
    printf("RESULT: GATE FAILED (bone merge repoint)\n");
    fflush(stdout);
    fflush(stderr);
    _exit(7);
}

// Lane credits: the credits list auto-scrolls to its end and only then stops.
// CreditsPanel::Poll (fn_827AF3D0) fires credits_done (credits.dta: {ui pop_screen})
// the first poll after Enter at which credits.lst is not scrolling (+0x174 bl
// fn_827F7F08 UIList::IsScrolling, +0x17C bne skips). UIList::AutoScroll
// (fn_827F8868) arms it when NumData > NumDisplay (+0x50 cmpw, +0x68 stb 1 to
// mAutoScrolling +0x230, +0x6C dir 1), UIList::Poll (fn_827F8078 +0x24..+0x54)
// fires the first step once UISeconds reaches mAutoScrollTimer, and each completed
// step chains the next from UIList::CompleteScroll (fn_827F9540, the
// UIListStateCallback thunk: +0x24 tests mAutoScrolling, +0x4C MaxFirstShowing,
// +0x58 compares, +0xA0 bl fn_8280ED90 UIListState::Scroll(dir) until the end, where
// +0x68 negates the direction and +0x80 re-arms the pause timer). So "not
// scrolling" is true exactly once the list reaches MaxFirstShowing. The first 29
// steps only move the highlight down the 30 visible rows, each completing inside
// Scroll and chaining the next, which is why the focus log's `sel=29` (the
// highlight's display row) never changes while the rows scroll under it.
// The real credits list (credits.milo: num_display 30, speed 0.85 s per row,
// auto_scroll_pause 0; 1777 rows from ui/locale/locale_credits_keep.dta) takes
// 1747 steps x 0.85 s = 1485 s, about 44,550 frames at 30 fps: the det walk enters
// credits_screen at frame 1117 and pops it at frame 45674 (lane credits,
// 2026-11-23), so a 40,000-frame walk ends before the credits do. This drives the
// same code on a 60-row list with credits' settings and a 30 fps synthetic clock:
// it must scroll every frame from the first step to row 30 (MaxFirstShowing) and
// then stop with the direction reversed, in 30 x 0.85 x 30 = 765 frames.
#include "obj/Task.h"
#include "ui/UIList.h"
#include "ui/UIListDir.h"
#include "ui/UIListProvider.h"
namespace {
    struct A39CreditsRows : UIListProvider {
        virtual int NumData() const { return 60; }
    };
    struct A39ListPeek : UIList {
        static A39ListPeek &P(UIList &l) { return static_cast<A39ListPeek &>(l); }
        static UIListState &State(UIList &l) { return P(l).mListState; }
        static void Wire(UIList &l, UIListDir *d) {
            P(l).mListDir = d;
            P(l).mAutoScrollPause = 0.0f;
        }
        static bool Auto(UIList &l) { return P(l).mAutoScrolling; }
        static int Dir(UIList &l) { return P(l).mAutoScrollDir; }
    };
}
void rb3_app_check_credits_auto_scroll() { // ring: native-real
    UIList *list = Hmx::Object::New<UIList>();
    UIListDir *dir = Hmx::Object::New<UIListDir>();
    A39CreditsRows rows;
    A39ListPeek::Wire(*list, dir);
    UIListState &st = A39ListPeek::State(*list);
    st.SetNumDisplay(30, false);
    st.SetSpeed(0.85f);
    st.SetProvider(&rows, nullptr);
    list->AutoScroll();
    bool armed = A39ListPeek::Auto(*list);
    float t0 = TheTaskMgr.UISeconds();
    list->Poll(); // pause 0: the timer fires now and the first step starts at t0
    bool started = st.IsScrolling();
    int maxFirst = st.MaxFirstShowing();
    int frames = 0, idle = 0;
    while (frames < 2000 && st.IsScrolling()) {
        frames++;
        st.Poll(t0 + frames / 30.0f);
        if (!st.IsScrolling() && st.FirstShowing() != maxFirst) idle++;
    }
    int first = st.FirstShowing(), dirAfter = A39ListPeek::Dir(*list);
    bool autoAfter = A39ListPeek::Auto(*list), scrolling = st.IsScrolling();
    list->StopAutoScroll();
    delete list;
    delete dir;
    char what[384];
    snprintf(what, sizeof what,
             "credits auto-scroll: armed %d, started %d, %d frame(s) to stop at first %d of max %d "
             "(%d early stop(s)), still scrolling %d, auto %d, dir %d (want 1 1, 760..770 frames, "
             "%d, 0, 0 1 -1) (retail fn_827F9540 +0xA0 bl fn_8280ED90, +0x68 neg; "
             "fn_827AF3D0 +0x174)",
             (int)armed, (int)started, frames, first, maxFirst, idle, (int)scrolling,
             (int)autoAfter, dirAfter, maxFirst);
    if (armed && started && maxFirst == 30 && first == maxFirst && idle == 0 && !scrolling
        && frames >= 760 && frames <= 770 && autoAfter && dirAfter == -1) {
        rb3_app_gate(what);
        return;
    }
    printf("  [FAIL] rb3-app %s\n", what);
    printf("RESULT: GATE FAILED (credits auto-scroll)\n");
    fflush(stdout);
    fflush(stderr);
    _exit(7);
}
