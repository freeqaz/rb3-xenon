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
#include <cstdio>

extern "C" void rb3_app_gate(const char *step) { // ring: native-real
    printf("  [PASS] rb3-app %s\n", step);
    fflush(stdout);
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
}

extern "C" void __real__Z9TrackInitv(void);
extern "C" void __wrap__Z9TrackInitv(void) { // ring: native-real
    __real__Z9TrackInitv();
    rb3_app_gate("TrackInit()");
}

extern "C" void __real__Z9WorldInitv(void);
extern "C" void __wrap__Z9WorldInitv(void) { // ring: native-real
    __real__Z9WorldInitv();
    rb3_app_gate("WorldInit()");
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

extern "C" void __real__ZN3Net4InitEv(void *self);
// NARROWED (lane A3). Retail Net::Init (src/network/net/Net.cpp:79, 0x823E0648)
// runs on an offline 360 too: it installs the Quazal allocator hooks, builds a
// SyncStore, `mSession = NetSession::New()` (= new XboxSession, NetSession_Xbox.cpp:441,
// which sets TheNetSession; state kIdle, online disabled), SessionSearcher::New(),
// TheServer.Init(), a NetworkEmulator and TheNetMessenger.Init(). Natively this
// gate runs ONLY the session construction: the idle session object is what
// SessionMgr::Init, MemcardMgr and ProfileMgr::Init subscribe to, and its ctor
// sends nothing (the Quazal settings it writes have native bodies in
// rb3_app_quazal_native.cpp). Everything else Net::Init builds is networked and
// is NOT built: TheNet itself stays zero storage (Net.cpp does not compile
// natively), so TheNet.mSession/mSearcher/mServer stay null, and any later use
// of them still reaches an aborting stub or zero storage. Net.cpp's body is not
// called. Printed as GATED, not PASS, because it is not retail's whole call.
extern "C" void *rb3_app_netsession_new() __asm__("_ZN10NetSession3NewEv");
extern "C" void __wrap__ZN3Net4InitEv(void *self) { // ring: online-net
    (void)self;
    void *session = rb3_app_netsession_new();
    printf("  [GATED] rb3-app Net::Init() -- ring online-net: built ONLY the idle XboxSession "
           "(TheNetSession=%p); SyncStore/SessionSearcher/Server/NetworkEmulator/NetMessenger not built\n",
           session);
    fflush(stdout);
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
// GATED, not called (lane A2). Retail 360 makes this call offline too: Net::Init
// has already built an idle XboxSession (NetSession::New, NetSession_Xbox.cpp)
// whose base ctor configures the Quazal transport (RootTransport connection
// timeout, Stream::s_oStreamSettings via PseudoSingleton contexts), and
// SessionMgr/BandMatchmaker subscribe to it. rb3-app has none of that: Net::Init
// is GATED, TheNetSession is null, and the Quazal StreamSettings / Stream /
// PseudoSingleton bodies are absent from the tree, so the real NetSession ctor
// cannot run. Skipped LOUDLY; TheSessionMgr stays null (zero storage in
// rb3_app_link_stubs.cpp), so its first reader crashes visibly rather than
// talking to a fake session. SessionUsersProvidersInit and BandMachineMgr::Init,
// which SessionMgr::Init also calls, are skipped with it.
extern "C" void __wrap__ZN10SessionMgr4InitEv(void) { // ring: online-netsession
    printf("  [GATED] rb3-app SessionMgr::Init() -- ring online-netsession: no NetSession natively "
           "(Net::Init gated), TheSessionMgr stays null, not called\n");
    fflush(stdout);
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
// measurement. With RB3_TITLE_UPDATE_DIR set the real registrar runs.
const char *Rb3NativeTitleContentPath();
extern "C" void __real__Z19SetFileChecksumDatav(void);
extern "C" void __wrap__Z19SetFileChecksumDatav(void) { // ring: native-real
    if (Rb3NativeTitleContentPath()) {
        __real__Z19SetFileChecksumDatav();
        rb3_app_gate("SetFileChecksumData()");
    } else {
        printf("  [GATED] rb3-app SetFileChecksumData() -- ring tu5-content: the table describes TU5, "
               "no title update mounted (RB3_TITLE_UPDATE_DIR), not registered\n");
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
               "no title update mounted (RB3_TITLE_UPDATE_DIR), not registered\n");
        fflush(stdout);
    }
}
