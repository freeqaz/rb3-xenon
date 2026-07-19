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

typedef void ModalCallbackFunc(Debug::ModalType &, FixedString &, bool);

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

void DebugModal(Debug::ModalType &ty, FixedString &str, bool b3) {
    if (ty == Debug::kModalFail) {
        str += "\n\n-- Program ended --\n";
    } else {
        gNotifies.push_back(str.c_str());
    }
    MILO_LOG("%s\n", str.c_str());
}

Debug::Debug()
    : mNoDebug(0), mFailing(0), mExiting(0), mNoTry(0), mNoModal(0), mTry(0), mLog(0),
      mAlwaysFlush(0), mReflect(0), mModalCallback(DebugModal), mFailThreadMsg(0),
      mNotifyThreadMsg(0) {}

void Debug::RemoveExitCallback(ExitCallbackFunc *func) {
    if (!mExiting) {
        mExitCallbacks.remove(func);
    }
}

Debug::~Debug() { StopLog(); }

void Debug::Print(const char *msg) {
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
}

void Debug::Exit(int exitCode, bool call_exit) {
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
            ModalType type = kModalWarn;
            Modal(type, msg, nullptr);
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
        } else {
            ModalType type = kModalNotify;
            Modal(type, msg, nullptr);
        }
    }
}

void Debug::Fail(const char *msg, void *v) {
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
    if (!mNoDebug && !mFailing) {
        mFailing = true;
        StackString<256> msgStr(msg);
        StackString<4096> stackTrace;
        DataAppendStackTrace(stackTrace);
        MILO_LOG(stackTrace.c_str());
        static int heap = MemFindHeap("main");
        MemPushHeap(heap);
        if (!MainThread()) {
            CaptureStackTrace(0x32, (StackData *)mFailThreadStack, v);
            mFailThreadMsg = msg;
            MILO_LOG("THREAD-FAIL: %s\n", msgStr);
            while (true) {
                Timer::Sleep(200);
                PlatformDebugBreak();
            }
        }
        if (mTry) {
            mTry--;
            throw msg;
        }
        FOREACH (it, mFailCallbacks) {
            (*it)();
        }
        mFailCallbacks.clear();
        ModalType t = kModalFail;
        Modal(t, msgStr.c_str(), v);
        if (t != kModalFail) {
            mFailing = false;
        }
        MemPopHeap();
        mFailing = false;
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
        Fail(mFailThreadMsg, nullptr);
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

void Debug::Modal(ModalType &type, const char *msg, void *addr) {
    String msgCopy(msg);
    StackString<4096> modalMsg(msgCopy.c_str());
    StackString<256> shortMsg;
    StackString<512> dataCallstack;
    StackString<2048> callstack;
    if (type == kModalFail) {
        MILO_LOG("FAIL-MSG: %s\n", msg);
        if (mModalCallback) {
            mModalCallback(type, modalMsg, false);
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
            // telemetry addition RB3 never had); rb3-Wii's equivalent Modal() body
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
        if (type == kModalFail && TheAppChild) {
            TheAppChild->Sync(2);
        }
    }
    if (mModalCallback) {
        mModalCallback(type, modalMsg, true);
    } else {
        const char *typeNames[] = { "WARN", "NOTIFY", "FAIL" };
        MILO_LOG("%s: %s\n", typeNames[type], modalMsg);
    }
    if (type == kModalFail) {
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
extern Symbol gSystemLocale;

void SetUsingCD(bool b) { gUsingCD = b; }

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

Symbol SystemLocale() { return gSystemLocale; }

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

void SystemPreInit(const char *cmdLine, const char *cfg) {
    SetSystemArgs(cmdLine);
    SystemPreInit(cfg);
}

void Timer::Sleep(int ms) { ::Sleep(ms); }
#endif
