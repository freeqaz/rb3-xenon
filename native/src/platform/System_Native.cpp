// DC3 Native Port - System Implementation
// Replaces System_Xbox.cpp - POSIX implementations of system functions

#include "os/Archive.h"
#include "os/Block.h"
#include "os/Debug.h"
#include "os/File.h"
#include "os/System.h"
#include "utl/Loader.h"
#include "utl/MakeString.h"
#include "utl/Symbol.h"

#include <cstdio>
#include <cstdlib>
#include <sys/stat.h>

namespace {
    DiscErrorCallbackFunc *gCallback;
}

unsigned long ULSystemLocale() { return 0x0409; } // en-US
unsigned long ULSystemLanguage() { return 1; }    // English

DiscErrorCallbackFunc *SetDiskErrorCallback(DiscErrorCallbackFunc *func) {
    DiscErrorCallbackFunc *old = gCallback;
    gCallback = func;
    return old;
}

DiscErrorCallbackFunc *GetDiskErrorCallback() { return gCallback; }

Symbol GetSystemLanguage(Symbol s) {
    static Symbol eng("eng");
    return eng;
}

Symbol GetSystemLocale(Symbol s) {
    static Symbol locale("en_US");
    return locale;
}

void ShowDirtyDiscError() {
    MILO_LOG("Dirty disc error - ignoring on native\n");
}

bool PlatformDebugBreak() {
    return false;
}

void GetMapFileName(String &s) {
    s = "";
}

void CaptureStackTrace(int, struct StackData *, void *) {}
void AppendStackTrace(FixedString &, void *) {}
void AppendThreadStackTrace(FixedString &, struct StackData *) {}

// Defined in File_Native.cpp
void NativeSetDataDir(const char *dir);
const char *NativeGetDataDir();
void NativeSetOverlayDir(const char *dir);
const char *NativeGetOverlayDir();

static bool FileExistsRaw(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

// Find the game data directory containing gen/main_xbox.hdr
void NativeDetectDataDir() {
    // Check environment variable first
    const char *env = getenv("DC3_DATA");
    if (env && FileExistsRaw(MakeString("%s/gen/main_xbox.hdr", env))) {
        NativeSetDataDir(env);
        printf("DC3 Native: data dir from DC3_DATA=%s\n", env);
        return;
    }

    // Search common locations relative to CWD
    static const char *searchPaths[] = {
        "orig-assets",
        "../orig-assets",
        "../../orig-assets",
        ".",
        nullptr
    };
    for (const char **p = searchPaths; *p; p++) {
        char buf[512];
        snprintf(buf, sizeof(buf), "%s/gen/main_xbox.hdr", *p);
        if (FileExistsRaw(buf)) {
            NativeSetDataDir(*p);
            printf("DC3 Native: data dir=%s\n", *p);
            return;
        }
    }
    printf("DC3 Native: WARNING - could not find game data (gen/main_xbox.hdr)\n");
    printf("  Set DC3_DATA env var or run from the repo root.\n");
}

// The DTA overlay directory: files there shadow the archive (native-only DTA
// patches). It is a read from OUTSIDE the disc image, so it is OPT-IN, named
// explicitly by RB3_OVERLAY_DIR (lane W16-UC). It used to be guessed from
// "<dataDir>/../native/dta" or "native/dta" under the working directory, which
// meant the same binary read different game data depending on where it was
// started from. tools/native_file_audit.py fails any run that reads it unless
// the overlay is allow-listed for that run.
void NativeDetectOverlayDir() {
    const char *env = getenv("RB3_OVERLAY_DIR");
    if (env && *env && FileExistsRaw(env)) {
        NativeSetOverlayDir(env);
        printf("DC3 Native: overlay dir=%s (RB3_OVERLAY_DIR)\n", env);
        return;
    }
    printf("DC3 Native: no overlay dir (set RB3_OVERLAY_DIR to use one)\n");
}

// Native archive initialization - simplified from ArchiveInit()
// Skips TheContentMgr dependency, directly creates archive from known path
void NativeArchiveInit() {
#ifdef __EMSCRIPTEN__
    // Web port: files are pre-extracted and served via HTTP into MEMFS.
    // No .ark archive needed — disable the CD/archive system so files
    // are opened directly via POSIX (backed by Emscripten MEMFS).
    SetUsingCD(false);
    printf("DC3 Web: archive system bypassed (MEMFS mode)\n");
#else
    Symbol plat = PlatformSymbol(TheLoadMgr.GetPlatform());
    const char *mainArk = MakeString("gen/main_%s", plat);
    printf("DC3 Native: Loading archive %s.hdr\n", mainArk);
    TheArchive = new Archive(mainArk, 0);
    static int preinitArk = 1;
    TheArchive->SetArchivePermission(1, &preinitArk);
    printf("DC3 Native: Archive loaded, %d ark files\n", TheArchive->NumArkFiles());
    TheBlockMgr.Init();
    printf("DC3 Native: BlockMgr initialized\n");
#endif
}

// Native version of SystemPreInit(argc, argv, config)
// On Xbox this is in System_Xbox.cpp - converts argc/argv to command line string
void SystemPreInit(const char *cmdLine, const char *cfg);

void SystemPreInit(int argc, char **argv, const char *config) {
    // Build command line string from argc/argv
    String cmdLine;
    for (int i = 0; i < argc; i++) {
        if (i > 0) cmdLine += ' ';
        cmdLine += argv[i];
    }
    // InitMakeString before NativeDetectDataDir (which uses MakeString)
    InitMakeString();
    // Detect game data directory before engine init
#ifndef __EMSCRIPTEN__
    NativeDetectDataDir();
    NativeDetectOverlayDir();
#endif
    SystemPreInit(cmdLine.c_str(), config);
}
