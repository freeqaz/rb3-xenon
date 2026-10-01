#include "os/System.h"
#include "HolmesClient.h"
#include "math/FileChecksum.h"
#include "math/Geo.h"
#include "math/Rand.h"
#include "math/Trig.h"
#include "net/WebSvcMgr.h"
#include "obj/Data.h"
#include "obj/DataFile.h"
#include "obj/DataFunc.h"
#include "obj/DataUtl.h"
#include "obj/Dir.h"
#include "obj/Task.h"
#include "os/AppChild.h"
#include "os/Archive.h"
#include "os/ContentMgr.h"
#include "os/DateTime.h"
#include "os/Debug.h"
#include "os/File.h"
#include "os/FileCache.h"
#include "os/Joypad.h"
#include "os/JoypadClient.h"
#include "os/Keyboard.h"
#include "os/MapFile_Xbox.h"
#include "os/Memcard_Xbox.h"
#include "os/Platform.h"
#include "os/PlatformMgr.h"
#include "os/ThreadCall.h"
#include "os/Timer.h"
#include "os/VirtualKeyboard.h"
#include "utl/CacheMgr.h"
#include "utl/Cheats.h"
#include "utl/GlitchFinder.h"
#include "utl/DataPointMgr.h"
#include "utl/Licenses.h"
#include "utl/Loader.h"
#include "utl/Locale.h"
#include "utl/MakeString.h"
#include "utl/MemMgr.h"
#include "utl/NetCacheMgr.h"
#include "utl/Option.h"
#include "utl/Spew.h"
#include "utl/Str.h"
#include "utl/Symbol.h"
#include "utl/TimeConversion.h"
#include "xdk/XAPILIB.h"
#include <cstdarg>
#include <cstdio>
#include <cstring>

const char *gNullStr = "";

GfxMode gGfxMode;
bool gHostConfig;
bool gHostLogging;
bool gHostCached;

#ifdef HX_NATIVE
DataArray *gSystemConfig;
DataArray *gSystemTitles;

int gUsingCD;
#else
// RB3 retail references these only from the System TU and co-addresses them
// off one base (PreInitSystem 0x82510BB8 reaches gSystemConfig as -8 off
// &gUsingCD), which MSVC does only for internal-linkage data. The compiler
// lays these statics out in reverse declaration order, so gUsingCD is
// declared first to sit above gSystemConfig (0x82CC9990 config, 0x82CC9994
// titles, 0x82CC9998 gUsingCD).
static int gUsingCD;
static DataArray *gSystemTitles;
static DataArray *gSystemConfig;
#endif
int gSystemMs;
float gSystemFrac;
const char *gHostFile;

Symbol gSystemLanguage;
Symbol gSystemLocale;

Timer gSystemTimer;
bool gNetUseTimedSleep;
bool(__cdecl *ParseStack)(char const *, struct StackData *, int, class FixedString &) =
    XboxMapFile::ParseStack;

std::vector<char *> TheSystemArgs;
std::vector<char *> gPristineSystemArgs;

namespace {
    bool gPreconfigOverride;
    bool gHasPreconfig;

    void CheckForArchive() {
        gUsingCD = true;
        if (FileGetStat(
                MakeString("gen/main_%s.hdr", PlatformSymbol(TheLoadMgr.GetPlatform()))
            ) < 0) {
            gUsingCD = false;
        }
    }
}

static Licenses sLicense("system/src/stlport", Licenses::kRequirementNotification);

int Hx_snprintf(char *c, unsigned int ui, char const *cc, ...) {
    std::va_list args;
    va_start(args, cc);
    int ret = vsnprintf(c, ui, cc, args);
    if (ret < 0) {
        c[ui - 1] = '\0';
        return -1;
    }
    return ret;
}

GfxMode GetGfxMode() { return gGfxMode; }

Symbol PlatformSymbol(Platform pform) {
#ifdef HX_NATIVE
    static Symbol sym[] = { gNullStr, gNullStr, "xbox", "pc", "ps3", "wii", "3ds" };
    if (pform >= 0 && pform < 7) {
        return sym[pform];
    } else
        return gNullStr;
#else
    // RB3 retail (0x8250FDF8): a six-entry table, no 3DS slot and no bounds
    // check.
    static Symbol sym[6] = { gNullStr, gNullStr, "xbox", "pc", "ps3", "wii" };
    return sym[pform];
#endif
}

bool UsingCD() { return gUsingCD; }
void SetUsingCD(bool b) { gUsingCD = b; }

#ifdef HX_NATIVE
// SystemConfig() lives in Debug.cpp for the X360 match build: retail places
// it at 0x8250FEF8 between SetUsingCD and the SystemConfig(Symbol...) overloads,
// i.e. inside Debug.cpp's TU (lane W3-A, 2026-09-11). The native build keeps
// its copy here because Debug.cpp's block is #ifndef HX_NATIVE.
DataArray *SystemConfig() { return gSystemConfig; }
// A missing config section is FATAL on X360 by design: DataArray::FindArray's
// fail=true path MILO_FAILs and returns null, and the caller dereferences it
// immediately -- which is fine there, because the shipped merged config always
// has every section a shipped code path asks for.
//
// A headless native harness does NOT have that guarantee. It assembles a config
// from the subset of DTBs it can read without standing up App/UI/renderer (see
// native/src/main_milo.cpp), so a class block can genuinely be absent -- e.g.
// config/objects.dta ships 74 class blocks and has none for CharClipGroup, and
// obj/Utl.cpp:82 ClassExt() asks for exactly that during a CharClipSet load.
// Turning that into a SIGSEGV would make "how complete is my config" unaskable.
//
// So on native the lookup degrades to a shared EMPTY array: every FindArray on
// it misses, which is the same answer the caller gets for a section that exists
// but lacks the key, and MILO_FAIL still prints the miss. Nothing is silently
// invented -- the diagnostic survives, only the crash is removed. X360 keeps
// the original chain byte for byte (this whole block is #ifdef HX_NATIVE).
static DataArray *EmptyConfigSection() {
    static DataArray *sEmpty = new DataArray(0);
    return sEmpty;
}
static DataArray *SafeFind(DataArray *a, Symbol s) {
    if (!a) return EmptyConfigSection();
    DataArray *r = a->FindArray(s, false);
    if (!r) {
        MILO_NOTIFY("SystemConfig: no '%s' section (native partial config)", s.Str());
        return EmptyConfigSection();
    }
    return r;
}

DataArray *SystemConfig(Symbol s) {
    DataArray *result = SafeFind(gSystemConfig, s);
    result->SetContextPath(s.Str());
    return result;
}
DataArray *SystemConfig(Symbol s1, Symbol s2) {
    return SafeFind(SafeFind(gSystemConfig, s1), s2);
}
DataArray *SystemConfig(Symbol s1, Symbol s2, Symbol s3) {
    return SafeFind(SafeFind(SafeFind(gSystemConfig, s1), s2), s3);
}
#else
DataArray *SystemConfig(Symbol s) {
    DataArray *result = gSystemConfig->FindArray(s);
    result->SetContextPath(s.Str());
    return result;
}

DataArray *SystemConfig(Symbol s1, Symbol s2) {
    // FindArray chain auto-propagates context since FindArray sets it
    DataArray *result = gSystemConfig->FindArray(s1)->FindArray(s2);
    return result;
}
DataArray *SystemConfig(Symbol s1, Symbol s2, Symbol s3) {
    return gSystemConfig->FindArray(s1)->FindArray(s2)->FindArray(s3);
}
#endif

DataArray *SystemConfig(Symbol s1, Symbol s2, Symbol s3, Symbol s4) {
    return gSystemConfig->FindArray(s1)->FindArray(s2)->FindArray(s3)->FindArray(s4);
}

DataArray *SystemConfig(Symbol s1, Symbol s2, Symbol s3, Symbol s4, Symbol s5) {
    return gSystemConfig->FindArray(s1)
        ->FindArray(s2)
        ->FindArray(s3)
        ->FindArray(s4)
        ->FindArray(s5);
}

Symbol SystemLanguage() { return gSystemLanguage; }
Symbol SystemLocale() { return gSystemLocale; }

DataArray *SystemTitles() { return gSystemTitles; }

Symbol GetSongTitlePronunciationLanguage() {
    Symbol lang = HongKongExceptionMet() ? "eng" : gSystemLanguage;
    static Symbol fre("fre");
    static Symbol frc("frc");
    static Symbol can("can");
    if (lang == fre && gSystemLocale == can) {
        lang = frc;
    }
    return lang;
}

int SystemExec(const char *args) {
    if (gUsingCD)
        return -1;
    else
        return HolmesClientSysExec(args);
}

bool PlatformLittleEndian(Platform p) {
    MILO_ASSERT(p != kPlatformNone, 0x175);
#ifdef HX_NATIVE
    return p == kPlatformPC || p == kPlatform3DS || p == kPlatformNone;
#else
    // RB3 retail (0x8250FEC8) tests PC then None; there is no 3DS platform.
    return p == kPlatformPC || p == kPlatformNone;
#endif
}

Platform ConsolePlatform() { return kPlatformXBox; }

bool gReadingSystemConfig;

DataArray *ReadSystemConfig(const char *config) {
    Timer timer;
    timer.Start();
    DataArray *cfgArr = DataReadFile(config, true);
    timer.Stop();
    MILO_LOG("reading system config file \"%s\" took %f ms\n", config, timer.Ms());
    return cfgArr;
}

void StripEditorData() {
    Symbol editor("editor");
    Symbol types("types");
    DataArray *objectsCfg = SystemConfig("objects");
    for (int i = 1; i < objectsCfg->Size(); i++) {
        DataArray *objectsArr = objectsCfg->Array(i);
        DataArray *objEditorArr = objectsArr->FindArray(editor, false);
        if (objEditorArr != 0)
            objEditorArr->Resize(1);
        DataArray *typesArr = objectsArr->FindArray(types, false);
        if (typesArr != 0) {
            for (int j = 1; j < typesArr->Size(); j++) {
                DataArray *typesEditorArr = typesArr->Array(j)->FindArray(editor, false);
                if (typesEditorArr != 0)
                    typesEditorArr->Resize(1);
            }
        }
    }
}

int SystemMs() {
    gSystemTimer.Restart();
    float lastMs = gSystemTimer.GetLastMs();
    int ms = gSystemFrac + lastMs;
    gSystemFrac = (gSystemFrac + lastMs) - ms;
    gSystemMs += ms;
    return gSystemMs;
}

void SystemPoll(bool b1) {
#ifdef HX_NATIVE
    static Timer *_t = AutoTimer::GetTimer("system_poll");
    AutoTimer _at(_t, 50.0f, nullptr, nullptr);
    Timer::ClearSlowFrame();
    SystemMs();
    TheDebug.Poll();
    TheMC.Poll();
    if (gUsingCD == 0) {
        HolmesClientPoll();
    }
    JoypadPoll();
    JoypadClientPoll();
    KeyboardPoll();
    ThreadCallPoll();
    FileCache::PollAll();
    TheLoadMgr.Poll();
#ifdef HX_NATIVE
    if (TheCacheMgr) TheCacheMgr->Poll();
    if (TheNetCacheMgr) TheNetCacheMgr->Poll();
#else
    TheCacheMgr->Poll();
    TheNetCacheMgr->Poll();
#endif
    TheWebSvcMgr.Poll();
    if (TheAppChild != nullptr) {
        TheAppChild->Poll();
    }
    if (b1) {
        TheTaskMgr.Poll();
    }
    ThePlatformMgr.Poll();
    TheVirtualKeyboard.Poll();
    TheContentMgr.PollRefresh();
#else
    // RB3 retail 0x82510270: no system_poll AutoTimer, no gUsingCD-gated
    // HolmesClientPoll and no WebSvcMgr poll (DC3-era, kept for native).
    // Retail ends with an unidentified stage-kit poll (fn_82521ED0, which feeds
    // JoypadStageKitSetRaw from a 32-entry ring) with no known source; it is
    // not called here.
    Timer::ClearSlowFrame();
    SystemMs();
    TheDebug.Poll();
    TheMC.Poll();
    JoypadPoll();
    JoypadClientPoll();
    KeyboardPoll();
    ThreadCallPoll();
    FileCache::PollAll();
    TheLoadMgr.Poll();
    TheCacheMgr->Poll();
    TheNetCacheMgr->Poll();
    if (TheAppChild != nullptr) {
        TheAppChild->Poll();
    }
    if (b1) {
        TheTaskMgr.Poll();
    }
    ThePlatformMgr.Poll();
    TheVirtualKeyboard.Poll();
    TheContentMgr.PollRefresh();
#endif
}

DataArray *SupportedLanguages(bool cheats) {
    static Symbol system("system");
    static Symbol language("language");
    static Symbol supported("supported");
    static Symbol cheat_supported("cheat_supported");
    return SystemConfig(system, language, cheats ? cheat_supported : supported)->Array(1);
}

bool IsSupportedLanguage(Symbol language, bool cheats) {
    DataArray *languages = SupportedLanguages(cheats);
    for (int i = 0; i < languages->Size(); i++) {
        if (languages->Sym(i) == language)
            return true;
    }
    return false;
}

void SetSystemLanguage(Symbol lang, bool cheats) {
    if (!IsSupportedLanguage(lang, cheats)) {
        static Symbol system("system");
        static Symbol language("language");
        static Symbol defaultSym("default");
        DataArray *langConfig = SystemConfig(system, language);
        DataArray *arr = langConfig->FindArray(defaultSym, false);
        if (arr != 0) {
            Symbol arrLang = arr->Sym(1);  // Element 0 is the config key
            if (IsSupportedLanguage(arrLang, cheats)) {
                lang = arrLang;
            } else {
                MILO_NOTIFY(
                    "Both %s and the default language (%s) are not supported!\n",
                    lang,
                    arrLang
                );
                return;
            }
        } else {
            MILO_NOTIFY(
                "Language %s is not supported, and there is no default language found!\n",
                lang
            );
            return;
        }
    }

#ifdef HX_NATIVE
    // Only reinitialize locale if language is actually changing
    if (gSystemLanguage.Null() || lang == gSystemLanguage) {
        gSystemLanguage = lang;
    } else {
        TheLocale.Terminate();
        gSystemLanguage = lang;
        TheLocale.Init();
    }
#else
    // RB3 retail 0x82510590: a single compare, no Null() guard.
    if (lang != gSystemLanguage) {
        TheLocale.Terminate();
        gSystemLanguage = lang;
        TheLocale.Init();
    } else {
        gSystemLanguage = lang;
    }
#endif
}

void SetGfxMode(GfxMode mode) {
    gGfxMode = mode;
    HolmesClientReInit();
    DataVariable("gfx_mode") = mode;
}

DataNode OnSystemLanguage(DataArray *) { return gSystemLanguage; }
DataNode OnSystemLocale(DataArray *) { return gSystemLocale; }
DataNode OnSystemExec(DataArray *a) { return SystemExec(a->Str(1)); }
DataNode OnUsingCD(DataArray *) { return UsingCD(); }
DataNode OnSupportedLanguages(DataArray *) { return SupportedLanguages(false); }
DataNode OnSystemMs(DataArray *) { return SystemMs(); }

DataNode OnSwitchSystemLanguage(DataArray *a) {
    DataArray *langs = SupportedLanguages(true);
    int i;
    for (i = 0; i < langs->Size(); i++) {
        if (gSystemLanguage == langs->Sym(i)) {
            break;
        }
    }
    SetSystemLanguage(langs->Sym((i + 1) % langs->Size()), true);
    return 1;
}

void LanguageInit() {
#ifdef HX_NATIVE
    // RB3 retail 0x825108B8 has no region check (the warning compiled out).
    if (ThePlatformMgr.GetRegion() == kRegionNone) {
        MILO_NOTIFY("LanguageInit called, but region has not been initialized");
    }
#endif
    DataArray *cfg = SystemConfig("system", "language");
    Symbol lang = GetSystemLanguage("eng");
    DataArray *remapArr = cfg->FindArray("remap", false);
    if (remapArr) {
        remapArr->FindData(lang, lang, false);
    }
    Symbol forceSym;
    if (cfg->FindData("force", forceSym, false)) {
        if (forceSym != "") {
            lang = forceSym;
        }
    }
    const char *str = OptionStr("lang", nullptr);
    if (str) {
        lang = str;
    }
    SetSystemLanguage(lang, false);
}

void CaptureStackTrace(int, unsigned int *) {}

#ifndef HX_NATIVE
void AppendStackTrace(FixedString &str, void *v) {
    StackData data;
    memset(&data, 0, sizeof(StackData));
    CaptureStackTrace(50, &data, v);
    int stackIdx;
    for (stackIdx = 0; stackIdx < 50; stackIdx++) {
        if (data.mFailThreadStack[stackIdx] == 0)
            break;
    }
    String mapName;
    GetMapFileName(mapName);
    str += "Stack Trace: \r\n";
    bool parse;
    if (!UsingCD() && !FileIsLocal(mapName.c_str())) {
        String strf8;
        HolmesClientStackTrace(mapName.c_str(), &data, stackIdx, strf8);
        str += strf8.c_str();
        parse = !strf8.empty();
    } else if (TheArchive && TheArchive->Patched()) {
        parse = false;
    } else {
        parse = (*ParseStack)(mapName.c_str(), &data, stackIdx, str);
    }
    if (!parse) {
        GenericMapFile::ParseStack(mapName.c_str(), &data, stackIdx, str);
    }
    str += "\r\n";
}

void AppendThreadStackTrace(FixedString &str, StackData *stack) {
    str += "\n\n-- Thread failure, no stack yet --";
    int idx;
    for (idx = 0; idx < 50; idx++) {
        if (stack->mFailThreadStack[idx] == 0)
            break;
    }
    GenericMapFile::ParseStack(nullptr, stack, idx, str);
}

bool GenericMapFile::ParseStack(
    const char *cc, struct StackData *stack, int stackIdx, FixedString &str
) {
    str += " (map file unavailable)";
    for (int i = 0; i < stackIdx; i++) {
        str += MakeString("\n   %08x", stack->mFailThreadStack[i]);
    }
    return true;
}
#endif

void InitSystem(const char *config) {
    if (!gPreconfigOverride && config) {
        bool oldCD = UsingCD();
        Archive *oldArchive = TheArchive;
        if (gHostConfig) {
            gUsingCD = false;
            TheArchive = nullptr;
        }
#ifdef HX_NATIVE
        DataArray *systemConfig = ReadSystemConfig(config);
        MILO_ASSERT(systemConfig, 0x267);
        DataMergeTags(systemConfig, gSystemConfig);
        DataReplaceTags(systemConfig, gSystemConfig);
        gSystemConfig->Release();
        gSystemConfig = systemConfig;
        DataVariable("syscfg") = gSystemConfig;
        gUsingCD = oldCD;
#else
        // RB3 retail 0x82510A08: DataReadFile directly, and gUsingCD is
        // restored to true (as in PreInitSystem), not to the saved value.
        DataArray *systemConfig = DataReadFile(config, true);
        DataMergeTags(systemConfig, gSystemConfig);
        DataReplaceTags(systemConfig, gSystemConfig);
        gSystemConfig->Release();
        gSystemConfig = systemConfig;
        {
            DataNode cfgNode(systemConfig, kDataArray);
            DataVariable("syscfg") = cfgNode;
        }
        TheArchive = oldArchive;
        gUsingCD = true;
#endif
#ifdef HX_NATIVE
        TheArchive = oldArchive;
#endif
        StripEditorData();
    }
    FinishDataRead();
}

void PreInitSystem(const char *config) {
#ifdef HX_NATIVE
    Archive *oldArchive = TheArchive;
    bool oldCD = UsingCD();
    if (gHostConfig) {
        gUsingCD = false;
        TheArchive = nullptr;
    }
    DataArrayPtr ptr(1);
    DataSetMacro("HX_XBOX", ptr);
    DataSetMacro("HX_WIN", ptr);
    DataSetMacro("HX_NG", ptr);
    while (true) {
        const char *str = OptionStr("define", nullptr);
        if (!str)
            break;
        DataSetMacro(str, ptr);
    }
    const char *cfgStr = OptionStr("config", nullptr);
    if (cfgStr && !gHasPreconfig) {
        config = cfgStr;
    }
    BeginDataRead();
        MILO_ASSERT(gSystemConfig = ReadSystemConfig(config), 0x1FF);
    DataVariable("syscfg") = gSystemConfig;
    gUsingCD = oldCD;
    TheArchive = oldArchive;
    DataRegisterFunc("system_language", OnSystemLanguage);
    DataRegisterFunc("system_locale", OnSystemLocale);
    DataRegisterFunc("system_exec", OnSystemExec);
    DataRegisterFunc("using_cd", OnUsingCD);
    DataRegisterFunc("supported_languages", OnSupportedLanguages);
    DataRegisterFunc("switch_system_language", OnSwitchSystemLanguage);
    DataRegisterFunc("system_ms", OnSystemMs);
    SetGfxMode(kNewGfx);
    if (cfgStr && gHasPreconfig) {
        InitSystem(cfgStr);
        gPreconfigOverride = true;
    }
#else
    // RB3 retail 0x82510BB8 (with the X360 macros): adds _SHIP,
    // reads the config with DataReadFile directly, restores gUsingCD to true,
    // and registers neither system_locale nor switch_system_language.
    Archive *oldArchive = TheArchive;
    if (gHostConfig) {
        gUsingCD = false;
        TheArchive = nullptr;
    }
    DataArrayPtr ptr(1);
    DataSetMacro("HX_XBOX", ptr);
    DataSetMacro("HX_WIN", ptr);
    DataSetMacro("HX_NG", ptr);
    DataSetMacro("_SHIP", ptr);
    while (true) {
        const char *str = OptionStr("define", nullptr);
        if (!str)
            break;
        DataSetMacro(str, ptr);
    }
    const char *cfgStr = OptionStr("config", nullptr);
    if (cfgStr && !gHasPreconfig) {
        config = cfgStr;
    }
    BeginDataRead();
    gSystemConfig = DataReadFile(config, true);
    {
        DataNode cfgNode(gSystemConfig, kDataArray);
        DataVariable("syscfg") = cfgNode;
    }
    TheArchive = oldArchive;
    gUsingCD = true;
    DataRegisterFunc("system_language", OnSystemLanguage);
    DataRegisterFunc("system_exec", OnSystemExec);
    DataRegisterFunc("using_cd", OnUsingCD);
    DataRegisterFunc("supported_languages", OnSupportedLanguages);
    DataRegisterFunc("system_ms", OnSystemMs);
    SetGfxMode(kNewGfx);
    if (cfgStr && gHasPreconfig) {
        InitSystem(cfgStr);
        gPreconfigOverride = true;
    }
#endif
}

void SystemInit(const char *config) {
#ifdef HX_NATIVE
    if (OptionBool("force_cd", false)) {
        MILO_FAIL("force_cd is deprecated in favor of no_cd");
    }
#endif
    gSystemTimer.Start();
    Symbol::Init();
    InitSystem(config);
#ifdef HX_NATIVE
    gSystemTitles = SystemConfig("system", "titles");
    ObjectDir::Init();
    TrigTableInit();
    ThreadCallInit();
    GeoInit();
    TrigInit();
    SpewInit();
    TheLocale.Terminate();
    TheLocale.Init();
    CheatsInit();
    FileCache::Init();
    TheContentMgr.Init();
    TheDebug.AddExitCallback(SystemTerminate);
#else
    gSystemTitles = SystemConfig("system", "titles");
    ObjectDir::Init();
    TrigTableInit();
    ThreadCallInit();
    GeoInit();
    TrigInit();
    SpewInit();
    TheLocale.Terminate();
    TheLocale.Init();
    // RB3 retail 0x825112E8: FileCache::Init before the cache managers; no
    // DataPointMgr / WebSvcMgr init and no licenses option.
    CheatsInit();
    TheMC.Init();
    FileCache::Init();
    CacheMgrInit();
    NetCacheMgrInit();
    ThePlatformMgr.Init();
    TheVirtualKeyboard.Init();
    TheContentMgr.Init();
    GlitchFinder::Init();
    TheDebug.AddExitCallback(SystemTerminate);
#endif
}

static char sCommandLineBuffer[kCommandLineSz];

void SetSystemArgs(const char *commandLine) {
    MILO_ASSERT(commandLine && strlen(commandLine) < kCommandLineSz, 0x39A);

    strncpy(sCommandLineBuffer, commandLine, kCommandLineSz - 1);
    sCommandLineBuffer[kCommandLineSz - 1] = 0;

    char *ptr = sCommandLineBuffer;
    bool newToken = true;
    if (sCommandLineBuffer[0] != 0) {
        unsigned int inQuotes = 0;
        for (;;) {
            // Retail 0x825110E0 splits on a SPACE outside quotes; the space test
            // was missing here, so every unquoted character became NUL.
            if (!inQuotes && *ptr == ' ') {
                *ptr = 0;
                newToken = true;
                ptr++;
            } else if (*ptr == '"') {
                *ptr = 0;
                ptr++;
                inQuotes = !inQuotes;
                if (inQuotes) {
                    TheSystemArgs.push_back(ptr);
                    newToken = false;
                } else {
                    newToken = true;
                }
            } else {
                if (newToken) {
                    TheSystemArgs.push_back(ptr);
                    newToken = false;
                }
                ptr++;
            }
            if (*ptr == 0)
                break;
        }
    }

    NormalizeSystemArgs();
#ifdef HX_NATIVE
    // DC3-era; RB3 retail has no pristine copy of the argument vector.
    gPristineSystemArgs = TheSystemArgs;
#endif
}

void NormalizeSystemArgs() {
    unsigned int i = 0;
    if (TheSystemArgs.size() == 0)
        return;

    do {
        char *p = TheSystemArgs[i];
        char c = *p;
        while (c != '\0') {
            if (*p == (char)0x96) {
                *p = '-';
            }
            if (*p == (char)0x93 || *p == (char)0x94) {
                *p = '"';
            }
            p++;
            c = *p;
        }
        i++;
    } while (i < TheSystemArgs.size());
}

void SystemPreInit(const char *config) {
#ifdef HX_NATIVE
    InitMakeString();
    Symbol::PreInit(640000, 80000);
    ThePlatformMgr.RegionInit();
    OptionInit();
    TimeConversionInit();
    Timer::Init();
    FileInit();
    AppChild::Init();
    DateTimeInit();
    SYSTEMTIME time;
    GetLocalTime(&time);
    SeedRand(time.wMilliseconds);
    SetUsingCD(true);
    {
        extern void NativeArchiveInit();
        NativeArchiveInit();
    }
    TheDebug.Init();
    DataInit();
    DataArray_InitDtaTrace();
    DataArray_InitDtaValidate();
    PreInitSystem(config);
    LanguageInit();
    gSystemLocale = GetSystemLocale("usa");
    // Skip MemInit on native — heap manager uses 32-bit pointer arithmetic.
    TheLoadMgr.Init();
    JoypadInit();
    KeyboardInit();
    AutoTimer::Init();
    ThreadCallPreInit();
    TheTaskMgr.Init();
#else
    // RB3 retail 0x82510EC8.  The argument string is built from TheSystemArgs
    // first (no pristine copy, no reserve(0)); CheckForArchive runs on
    // force_cd (default true) instead of !no_cd; there is no no_checksum
    // hook and no GetSystemLocale("usa") (DC3-era, both kept for native).
    String str;
    for (int i = 0; i < TheSystemArgs.size(); i++) {
        str += ' ';
        str += TheSystemArgs[i];
    }
    InitMakeString();
    Symbol::PreInit(640000, 80000);
    ThePlatformMgr.RegionInit();
    ThePlatformMgr.PreInit();
    if (OptionBool("force_cd", true)) {
        CheckForArchive();
    }
    OptionInit();
    TimeConversionInit();
    Timer::Init();
    gHostConfig = OptionBool("host_config", false);
    gHostLogging = OptionBool("host_logging", false);
    gHostFile = OptionStr("host_file", nullptr);
    if (gHostFile) {
        gHostConfig = true;
    }
    gHostCached = OptionBool("host_cached", false);
    FileInit();
    AppChild::Init();
    DateTimeInit();
    SYSTEMTIME time;
    GetLocalTime(&time);
    SeedRand(time.wMilliseconds);
    TheContentMgr.PreInit();
    ArchiveInit();
    TheDebug.Init();
#ifdef HX_NATIVE
    MILO_LOG("SystemInit Params:%s\n", String(str));
#else
    // Retail's stripped residue copy-constructs the String and destroys it
    // through the pointer the copy constructor returned (no reload of the
    // temporary's address), which is MiloStripEval's by-value shape.
    MiloStripEval("SystemInit Params:%s\n", String(str));
#endif
    DataInit();
    PreInitSystem(config);
    LanguageInit();
    MemInit();
    TheLoadMgr.Init();
    JoypadInit();
    KeyboardInit();
    AutoTimer::Init();
    ThreadCallPreInit();
    TheTaskMgr.Init();
#endif
}

void SystemPreInit(const char *cmdLine, const char *cfg) {
    SetSystemArgs(cmdLine);
    SystemPreInit(cfg);
}

void SystemTerminate() {
    TheDebug.RemoveExitCallback(SystemTerminate);
    TheVirtualKeyboard.Terminate();
    CacheMgrTerminate();
    NetCacheMgrTerminate();
    FileCache::Terminate();
    TheLocale.Terminate();
    TheMC.Memcard::Terminate();
    CheatsTerminate();
    KeyboardTerminate();
    JoypadTerminate();
    SpewTerminate();
    ThreadCallTerminate();
    TheTaskMgr.Terminate();
    ObjectDir::Terminate();
    TheContentMgr.Terminate();
    TrigTableTerminate();
#ifndef HX_NATIVE
    // RB3 retail 0x82511208: FileTerminate before the config
    // release, MemTerminate after Symbol::Terminate.
    FileTerminate();
#endif
    gSystemConfig->Release();
    DataTerminate();
    Symbol::Terminate();
#ifndef HX_NATIVE
    void MemTerminate(); // utl/MemMgr.cpp
    MemTerminate();
#endif
    AppChild::Terminate();
    TheSystemArgs.erase(TheSystemArgs.begin(), TheSystemArgs.end());
    TerminateMakeString();
}

#ifdef HX_NATIVE
bool HongKongExceptionMet() {
    return false;
}
#endif
