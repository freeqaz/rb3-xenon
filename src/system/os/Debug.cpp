template <class T, int InitVal, int DestroyVal>
class ScopedState {
public:
    ScopedState(T *ptr) : mPtr(ptr) { *mPtr = (T)InitVal; }
    ~ScopedState();
    T *mPtr;
};

template <class T, int InitVal, int DestroyVal>
ScopedState<T, InitVal, DestroyVal>::~ScopedState() {
    *mPtr = (T)DestroyVal;
}

// Force instantiation of the destructor COMDAT
template ScopedState<bool, 1, 0>::~ScopedState();

#include "os/Debug.h"
#include "HolmesClient.h"
#include "obj/Data.h"
#include "os/AppChild.h"
#include "os/CritSec.h"
#include "os/File.h"
#include "os/OSFuncs.h"
#include "os/SynchronizationEvent.h"
#include "os/System.h"
#include "os/Timer.h"
#include "os/NetworkSocket.h"
#include "utl/Cheats.h"
#include "utl/DataPointMgr.h"
#include "utl/Loader.h"
#include "utl/MemMgr.h"
#include "utl/Option.h"
#include "utl/TextFileStream.h"
#include "utl/MakeString.h"
#include "world/CameraShot.h"
#include <vector>
#include "xdk/XAPILIB.h"
#include "xdk/xbdm/xbdm.h"
#include "utl/Std.h"

const char *GetExpCode(int code);

long HmxGlobalHandler(_EXCEPTION_POINTERS *ep) {
    if (DmIsDebuggerPresent()) {
        return 1;
    }
    void *addr = ep->ContextRecord;
    const char *code = GetExpCode(ep->ExceptionRecord->ExceptionCode);
    TheDebug.Fail(code, addr);
    return 0;
}

const char *kAssertStr = "File: %s Line: %d Error: %s\n";
extern bool gMemoryUsageTest;
DebugWarner TheDebugWarner;
DebugNotifier TheDebugNotifier;
DebugFailer TheDebugFailer;
SynchronizationEvent gNotifyThreadSync;
CriticalSection gNotifyThreadSec;
Debug TheDebug;
std::vector<String> gNotifies;

typedef void ModalCallbackFunc(bool &, char *, bool);

void Debug::SetDisabled(bool d) { mNoDebug = d; }

void Debug::StopLog() { RELEASE(mLog); }

const char *DevHostname(Symbol s) {
    static Symbol hostnames = "hostnames";
    return SystemConfig() ? SystemConfig(hostnames, s)->Str(1) : nullptr;
}

ModalCallbackFunc *Debug::SetModalCallback(ModalCallbackFunc *func) {
    if (mNoModal)
        return nullptr;
    ModalCallbackFunc *oldFunc = mModalCallback;
    mModalCallback = func;
    if (gNotifies.size() > 0) {
        for (int i = 0; i < gNotifies.size(); i++) {
            MILO_LOG("%s\n", gNotifies[i].c_str());
        }
        gNotifies.clear();
    }
    return oldFunc;
}

void DebugModal(bool &fail, char *msg, bool wait) {
    if (fail) {
        strcat(msg, "\n\n-- Program ended --\n");
    } else {
        gNotifies.push_back(msg);
    }
    MILO_LOG("%s\n", msg);
}

// Retail's ctor init list omits mAlwaysFlush @0x14 -- there is no
// `stb r11, 0x14(r31)` in the target COMDAT (0x10 and 0x18 are both stored).
// It is only ever assigned from the debug console, so keep it initialized on
// native only.
Debug::Debug()
    : mNoDebug(0), mFailing(0), mExiting(0), mNoTry(0), mNoModal(0), mTry(0), mLog(0),
#ifdef HX_NATIVE
      mAlwaysFlush(0),
#endif
      mReflect(0), mModalCallback(DebugModal), mFailThreadMsg(0),
      mNotifyThreadMsg(0) {}

void Debug::RemoveExitCallback(ExitCallbackFunc *func) {
    if (!mExiting) {
        mExitCallbacks.remove(func);
    }
}

Debug::~Debug() { StopLog(); }

// Retail's slot 1 (TextStream::Print) is the empty-body fold: retail Debug
// prints nothing (lane W16-OT, BODY check). The sinks are native-only.
void Debug::Print(const char *msg) {
#ifdef HX_NATIVE
    if (mLog) {
        mLog->Print(msg);
        if (mAlwaysFlush) {
            mLog->File().Flush();
        }
    }
    if (MainThread() && mReflect) {
        mReflect->Print(msg);
    }
    if (!UsingCD()) {
        HolmesClientPrint(msg);
    }
    OutputDebugStringA(msg);
#endif
}

void Debug::Exit(int exitCode, bool call_exit) {
#ifndef HX_NATIVE
    // RB3-360 retail: unconditional, no memory-usage-test gate,
    // and the log is stopped before the optional relaunch.
    mExiting = true;
    FOREACH (it, mExitCallbacks) {
        (*it)();
    }
    mExitCallbacks.clear();
    StopLog();
    if (call_exit) {
        XLaunchNewImage("", 0);
    }
#else
    if (!mExiting) {
        mExiting = true;
        MILO_LOG("APP EXITING\n");
        MILO_LOG("EXIT CODE %d call_exit %d\n", exitCode, call_exit);
        if (!gMemoryUsageTest) {
            FOREACH (it, mExitCallbacks) {
                (*it)();
            }
        }
        mExitCallbacks.clear();
        if (call_exit) {
            XLaunchNewImage("", 0);
        }
    }
#endif
}

void Debug::Warn(const char *msg) {
    if (!mNoDebug) {
        if (!MainThread()) {
            MILO_LOG("THREAD-NOTIFY: %s\n", msg);
            if (mModalCallback) {
                CritSecTracker tracker(&gNotifyThreadSec);
                mNotifyThreadMsg = msg;
                gNotifyThreadSync.Wait(200);
            }
        } else {
            bool fail = false;
            Modal(fail, msg, nullptr);
        }
    }
}

void Debug::Notify(const char *msg) {
    if (!mNoDebug) {
        if (!MainThread()) {
            MILO_LOG("THREAD-NOTIFY: %s\n", msg);
            if (mModalCallback) {
                CritSecTracker tracker(&gNotifyThreadSec);
                mNotifyThreadMsg = msg;
                gNotifyThreadSync.Wait(200);
            }
        }
#ifdef HX_NATIVE
        // RB3 retail (0x8250F538) has no main-thread Modal here: it returns.
        else {
            bool fail = false;
            Modal(fail, msg, nullptr);
        }
#endif
    }
}

void Debug::Fail(const char *msg) {
#ifdef HX_NATIVE
    fprintf(stderr, "FAIL: %s\n", msg);
#ifdef HX_WEB
    // Web port: never fatal — matches Xbox "Continue" dialog behavior.
    // Many init paths trigger benign FAILs (missing assets, stubs).
    return;
#endif
    // Default: non-fatal (match Xbox 360 "Continue" dialog behavior).
    // DTA scripts trigger many benign FAILs during gameplay (missing assets,
    // songs not in lookup tables, etc.). Set MILO_FATAL_FAILS=1 to abort.
    static int sFatalFails = -1;
    if (sFatalFails == -1) {
        const char *env = getenv("MILO_FATAL_FAILS");
        sFatalFails = (env && atoi(env) != 0) ? 1 : 0;
    }
    if (sFatalFails)
        abort();
    return;
#endif
    // RB3 retail (0x8250F6D0): the heap push brackets everything, mNoDebug only
    // skips the body, a thread fail parks the thread, a try throws, and the fail
    // callbacks run once. No stack-trace string and no Modal in this build.
    static int heap = MemFindHeap("main");
    MemPushHeap(heap);
    if (mNoDebug)
        MemPopHeap();
    else {
        if (!MainThread()) {
            CaptureStackTrace(0x32, mFailThreadStack);
            mFailThreadMsg = msg;
            MILO_LOG("THREAD-FAIL: %s\n", msg);
            do {
                Timer::Sleep(200);
                PlatformDebugBreak();
            } while (true);
        }
        if (mTry != 0) {
            mTry--;
            throw msg;
        }
        if (mFailing)
            MemPopHeap();
        else {
            mFailing = true;
            for (std::list<ExitCallbackFunc *>::iterator it = mFailCallbacks.begin();
                 it != mFailCallbacks.end();
                 it++) {
                (*it)();
            }
            mFailCallbacks.clear();
            MemPopHeap();
        }
    }
}

void Debug::Poll() {
    MILO_ASSERT(MainThread(), 0x1D4);
    if (mTry) {
        int oldTry = mTry;
        mTry = 0;
        MILO_FAIL("TRY conditional not exited %d", oldTry);
    }
    if (mFailThreadMsg) {
        Fail(mFailThreadMsg);
    }
    if (mNotifyThreadMsg) {
        String notifyStr(mNotifyThreadMsg);
        mNotifyThreadMsg = nullptr;
        gNotifyThreadSync.Set();
        Notify(notifyStr.c_str());
    }
}

void Debug::SetTry(bool tryBool) {
    MILO_ASSERT(MainThread(), 0x1F5);
    if (!mNoTry) {
        if (tryBool) {
            mTry++;
        } else
            mTry--;
    }
}

void Debug::StartLog(const char *log, bool flush) {
    RELEASE(mLog);
    mLog = new TextFileStream(log, false);
    mAlwaysFlush = flush;
    if (mLog->File().Fail()) {
        MILO_NOTIFY("Couldn't open log %s", log);
        RELEASE(mLog);
    }
}

void Debug::Init() {
    mNoTry = OptionBool("no_try", false);
    const char *log = OptionStr("log", nullptr);
    if (log) {
        StartLog(log, true);
    }
    if (OptionBool("no_modal", false)) {
        SetModalCallback(nullptr);
        mNoModal = true;
    }
    log = OptionStr("log", nullptr);
    if (log) {
        StartLog(log, true);
    }
}

const char *GetExpCode(int code) {
    if (code <= (int)0xC000008D) {
        if (code != (int)0xC000008D) {
            if (code <= (int)0xC0000006) {
                if (code != (int)0xC0000006) {
                    int temp = code - (int)0x80000001;
                    if (temp != 0) {
                        switch ((unsigned int)temp) {
                        case 0x40000004:
                            return "EXCEPTION_ACCESS_VIOLATION";
                        case 0x3:
                            return "EXCEPTION_SINGLE_STEP";
                        case 0x2:
                            return "EXCEPTION_BREAKPOINT";
                        case 0x1:
                            return "EXCEPTION_DATATYPE_MISALIGNMENT";
                        default:
                            break;
                        }
                    } else {
                        return "EXCEPTION_GUARD_PAGE";
                    }
                } else {
                    return "EXCEPTION_IN_PAGE_ERROR";
                }
            } else {
                int temp = code - (int)0xC0000008;
                if (temp != 0) {
                    switch ((unsigned int)temp) {
                    case 0x15:
                        return "EXCEPTION_ILLEGAL_INSTRUCTION";
                    case 0x1D:
                        return "EXCEPTION_NONCONTINUABLE_EXCEPTION";
                    case 0x1E:
                        return "EXCEPTION_INVALID_DISPOSITION";
                    case 0x84:
                        return "EXCEPTION_ARRAY_BOUNDS_EXCEEDED";
                    default:
                        break;
                    }
                } else {
                    return "EXCEPTION_INVALID_HANDLE";
                }
            }
        } else {
            return "EXCEPTION_FLT_DENORMAL_OPERAND";
        }
    } else {
        if (code <= (int)0xC00000FD) {
            if (code != (int)0xC00000FD) {
                int temp = code + 0x3FFFFF72;
                if ((unsigned int)temp <= 8U) {
                    switch (temp) {
                    case 0:
                        return "EXCEPTION_FLT_DIVIDE_BY_ZERO";
                    case 1:
                        return "EXCEPTION_FLT_INEXACT_RESULT";
                    case 2:
                        return "EXCEPTION_FLT_INVALID_OPERATION";
                    case 3:
                        return "EXCEPTION_FLT_OVERFLOW";
                    case 4:
                        return "EXCEPTION_FLT_STACK_CHECK";
                    case 5:
                        return "EXCEPTION_FLT_UNDERFLOW";
                    case 6:
                        return "EXCEPTION_INT_DIVIDE_BY_ZERO";
                    case 7:
                        return "EXCEPTION_INT_OVERFLOW";
                    case 8:
                        return "EXCEPTION_PRIV_INSTRUCTION";
                    }
                }
            } else {
                return "EXCEPTION_STACK_OVERFLOW";
            }
        }
        if (code != (int)0xC000013A) {
            return MakeString("Unhandled Exception %d", (const CamShotFrame::BlendEaseMode &)code);
        }
        return "CONTROL_C_EXIT";
    }
}

void Debug::Modal(bool &fail, const char *msg, void *addr) {
    String msgCopy(msg);
    StackString<4096> modalMsg(msgCopy.c_str());
    StackString<256> shortMsg;
    StackString<512> dataCallstack;
    StackString<2048> callstack;
    if (fail) {
        MILO_LOG("FAIL-MSG: %s\n", msg);
        if (mModalCallback) {
            mModalCallback(fail, (char *)modalMsg.c_str(), false);
        }
        if (mFailThreadMsg) {
            AppendThreadStackTrace(modalMsg, (StackData *)mFailThreadStack);
        } else {
            String config;
            String version;
            if (SystemConfig()) {
                config = SystemConfig()->File();
                SystemConfig()->FindData("version", version, false);
            } else {
                config = "<unknown>";
            }
            modalMsg += MakeString(
                "\n\nConsoleName: %s   %s   Plat: %s   ",
                NetworkSocket::GetHostName(),
                version,
                PlatformSymbol(TheLoadMgr.GetPlatform())
            );
            modalMsg += MakeString("\nLang: %s   SystemConfig: %s", SystemLanguage(), config);
            // Retail RB3-360 has no live kernel-version query here (that's a DC3
            // telemetry addition RB3 never had); RB3's Modal() body
            // just prints a static "n/a" placeholder for SDK.
            String sdk("n/a");
            modalMsg += MakeString(
                "\nUptime: %.2f hrs   UsingCD: %s   SDK: %s",
                SystemMs() * (1.0 / 3600000.0),
                UsingCD() ? "true" : "false",
                sdk
            );
            AppendCheatsLog(shortMsg);
            modalMsg += shortMsg.c_str();
            DataAppendStackTrace(dataCallstack);
            modalMsg += dataCallstack.c_str();
            AppendStackTrace(callstack, addr);
            modalMsg += "\n";
            modalMsg += callstack.c_str();
        }
        if (TheAppChild) {
            TheAppChild->Sync(2);
        }
    }
    if (mModalCallback) {
        mModalCallback(fail, (char *)modalMsg.c_str(), true);
    } else {
        MILO_LOG("%s: %s\n", fail ? "FAIL" : "NOTIFY", modalMsg);
    }
    if (fail) {
        if (mModalCallback) {
            PlatformDebugBreak();
        }
        Exit(1, true);
    }
}

#ifndef HX_NATIVE
// --- retail TU-reunification (matching build only) ---
// In retail RB3 these System/Timer helpers were compiled into the SAME TU as
// Debug (proven: their .text interleaves Debug's functions under /O1's
// no-cross-TU-reorder). DC3 split them into System.cpp / Timer.cpp. Duplicate
// them here so Debug.obj emits+matches those bytes; canonical definitions stay
// in their DC3-split files for the native (HX_NATIVE) link. No final link in the
// matching build, so the duplicate symbols never collide.
extern int gUsingCD;
// Static (internal-linkage) + adjacent so MSVC addresses them section-relative and
// folds gSystemMs (+0) + gSystemFrac (+4) under one shared base, matching retail's
// SystemMs anchor (lbl_82CC999C). Canonical defs live in System.cpp; matching build
// has no final link (native excludes this reunification block).
static float gSystemFrac;
static int gSystemMs;
extern Timer gSystemTimer;
extern DataArray *gSystemConfig;
extern DataArray *gSystemTitles;
extern Symbol gSystemLanguage;

void SetUsingCD(bool b) { gUsingCD = b; }

// Retail defines the no-arg SystemConfig() here (0x8250FEF8), not in System.cpp;
// the map had this address as ??__EgNotifies and SystemConfig at 0x82569E78,
// which is AssetMgr::GetAssetMgr (lane W3-A, 2026-09-11).
DataArray *SystemConfig() { return gSystemConfig; }

DataArray *SystemConfig(Symbol s) {
    DataArray *result = gSystemConfig->FindArray(s);
    result->SetContextPath(s.Str());
    return result;
}

DataArray *SystemConfig(Symbol s1, Symbol s2) {
    DataArray *result = gSystemConfig->FindArray(s1)->FindArray(s2);
    return result;
}

DataArray *SystemConfig(Symbol s1, Symbol s2, Symbol s3) {
    return gSystemConfig->FindArray(s1)->FindArray(s2)->FindArray(s3);
}

DataArray *SystemConfig(Symbol s1, Symbol s2, Symbol s3, Symbol s4, Symbol s5) {
    return gSystemConfig->FindArray(s1)
        ->FindArray(s2)
        ->FindArray(s3)
        ->FindArray(s4)
        ->FindArray(s5);
}

// 0x82510040 reads the global SetSystemLanguage writes (0x82CC99A8): this is
// SystemLanguage(), which the map had as ?SystemLocale@@ (W16-HP).
Symbol SystemLanguage() { return gSystemLanguage; }

DataArray *SystemTitles() { return gSystemTitles; }

int SystemMs() {
    gSystemTimer.Restart();
    float lastMs = gSystemTimer.GetLastMs();
    float sum = lastMs + gSystemFrac;
    int ms = sum;
    gSystemFrac = sum - ms;
    gSystemMs += ms;
    return gSystemMs;
}

DataArray *SupportedLanguages(bool cheats) {
    static Symbol system("system");
    static Symbol language("language");
    static Symbol supported("supported");
    static Symbol cheat_supported("cheat_supported");
    return SystemConfig(system, language, cheats ? cheat_supported : supported)->Array(1);
}

DataNode OnSupportedLanguages(DataArray *) { return SupportedLanguages(false); }
DataNode OnSystemMs(DataArray *) { return SystemMs(); }

void NormalizeSystemArgs() {
    // Retail's entry guard is `srawi. r9,r9,2 / beqlr` -- the rotated copy of a
    // real for-loop latch (`i < TheSystemArgs.size()`), which keeps the /4 shift.
    // A hand-written `if (size() == 0) return;` guard instead gets MSVC's
    // standalone zero-test peephole and compiles to `clrrwi. r9,r9,2`.
    for (unsigned int i = 0; i < TheSystemArgs.size(); i++) {
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
    }
}

void SystemPreInit(const char *cmdLine, const char *cfg) {
    SetSystemArgs(cmdLine);
    SystemPreInit(cfg);
}

// Timer::Sleep (0x82511428) is NOT part of this reunified TU: Debug::Fail calls it
// out of line, and a definition visible here is inlined into Fail. It is the first
// function of Timer.cpp's block (TimerStats' ctor follows at 0x82511430).
#endif
