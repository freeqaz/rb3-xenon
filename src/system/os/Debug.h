#pragma once
#include "utl/TextStream.h"
#include "utl/TextFileStream.h"
#include <list>
#include <string.h>

class DataPoint;
typedef void ExitCallbackFunc(void);
typedef void FixedStringFunc(FixedString &);

// size 0x134
class Debug : public TextStream {
public:
    enum ModalType {
        kModalWarn = 0,
        kModalNotify = 1,
        kModalFail = 2
    };

    typedef void ModalCallbackFunc(ModalType &, FixedString &, bool);

private:
    void Modal(ModalType &, const char *, void *);

    bool mNoDebug; // 0x4
    bool mFailing; // 0x5
    bool mExiting; // 0x6
    bool mNoTry; // 0x7
    bool mNoModal; // 0x8
    int mTry; // 0xc
    TextFileStream *mLog; // 0x10
    bool mAlwaysFlush; // 0x14
    TextStream *mReflect; // 0x18
    ModalCallbackFunc *mModalCallback; // 0x1c
    std::list<ExitCallbackFunc *> mFailCallbacks; // 0x20
    std::list<ExitCallbackFunc *> mExitCallbacks; // 0x28
    std::list<FixedStringFunc *> mFailAppendCallbacks; // 0x30
    void (*mCrucibleCallback)(ModalType, DataPoint &); // 0x38
    // 0x3c is a struct, StackData
    unsigned int mFailThreadStack[50]; // starts at 0x3c
    const char *mFailThreadMsg; // 0x104
    const char *mNotifyThreadMsg; // 0x108
    const char *mCrucibleHostname; // 0x10c
    const char *mCrucibleApp; // 0x110
    String mCrucibleProject; // 0x114
    String mKernelVersion; // 0x11c
    String unk124; // 0x124
    String mHostName; // 0x12c

public:
    Debug();
    virtual ~Debug();
    virtual void Print(const char *);

    void Poll();
    void SetDisabled(bool);
    void SetTry(bool);
    void AddExitCallback(ExitCallbackFunc *func) { mExitCallbacks.push_front(func); }
    void AddFailAppendCallback(FixedStringFunc *func) { mFailAppendCallbacks.push_front(func); }
    void RemoveExitCallback(ExitCallbackFunc *);
    void AddFixedStrCallback(FixedStringFunc *func) { mFailAppendCallbacks.push_front(func); }
    bool CheckModalCallback(ModalCallbackFunc *func) { return mModalCallback == func; }
    ModalCallbackFunc *ModalCallback() const { return mModalCallback; }
    bool NoModal() const { return mNoModal; }
    void SetNoModal(bool nomodal) { mNoModal = nomodal; }

    void StartLog(const char *, bool);
    void StopLog();
    void Init();
    ModalCallbackFunc *SetModalCallback(ModalCallbackFunc *);
    void Exit(int, bool);
    void Warn(const char *msg);
    void Notify(const char *msg);
    void Fail(const char *msg, void *);
    // rb3-Wii uses 1-arg Fail; inline wrapper for portability
    inline void Fail(const char *msg) { Fail(msg, nullptr); }
    void DoCrucible(ModalType, const char *, void *);
    TextStream *Reflect() const { return mReflect; }
    TextStream *SetReflect(TextStream *ts) {
        TextStream *ret = mReflect;
        mReflect = ts;
        return ret;
    }
};

typedef void ModalCallbackFunc(Debug::ModalType &, FixedString &, bool);

#include "utl/Str.h"
#include "utl/MakeString.h"
#include <list>

extern Debug TheDebug;
extern const char *kAssertStr;

#ifdef HX_NATIVE
#define MILO_ASSERT(cond, line)                                                          \
    do {                                                                                 \
        if (!(cond)) {                                                                   \
            TheDebugFailer << MakeString(kAssertStr, __FILE__, line, #cond);             \
        }                                                                                \
    } while (0)

#define MILO_ASSERT_FMT(cond, ...)                                                       \
    do {                                                                                 \
        if (!(cond)) {                                                                   \
            TheDebugFailer << MakeString(__VA_ARGS__);                                   \
        }                                                                                \
    } while (0)
#else
// Retail RB3-360 stripped the assert STRING + failure branch (only 41 .cpp assert
// strings survive), but still EVALUATED the condition: `(void)(cond)` keeps
// side-effect calls in the cond (e.g. MILO_ASSERT(i<NumData(),..) keeps the
// NumData() virtual call) while DCE'ing pure conds to nothing. Verified
// whole-binary A/B: (void)(cond) vs (void)sizeof() = +1 alone, zero regressions,
// and unblocks body-ports whose asserts call into vtables (TourDescPanel +2).
// (void)sizeof() was over-stripping. FMT keeps no-op (its args are the message,
// not a side-effect cond). HX_NATIVE keeps real asserts.
#define MILO_ASSERT(cond, line) ((void)(cond))
#define MILO_ASSERT_FMT(cond, ...) ((void)sizeof(!(cond)))
#endif

#ifdef HX_NATIVE
#define MILO_FAIL(...) TheDebugFailer << MakeString(__VA_ARGS__)
#else
// Retail RB3-360 compiled MILO_FAIL's emission (TheDebugFailer << MakeString)
// to NO code: DebugFailer::operator<< (0x8235C970) has ZERO bl callers in the
// whole binary, and the fail strings ("Could not find %s in dir", "**no file**")
// are absent from .text/.rdata. But unlike the WARN family, the FAIL *arguments*
// were still EVALUATED for side effects (Find<T>'s fail path calls PathName(this)
// twice — the ternary — but never MakeString/Fail). So use (void)(args), a comma
// expression that evaluates each arg and discards: side-effecting args (PathName
// vcalls) survive, the format-string materialization + MakeString + Fail vanish.
// This is what flips the ObjectDir::Find<T> template family to 100%.
// (sizeof() — the WARN form — would strip arg evaluation too and regress inlined
// Find<> fail-paths; (void)(args) keeps it. HX_NATIVE keeps the real fatal path.)
#define MILO_FAIL(...) ((void)(__VA_ARGS__))
#endif
// Retail RB3-360 compiled the debug-OUTPUT family (WARN/NOTIFY/LOG/PRINT_ONCE/
// NOTIFY_ONCE/WARN_ONCE) to NO code in release (verified: their format strings
// are absent from orig band.exe, and gating them raises the match count +23
// with zero real regressions — engine logging-heavy TUs like MidiParser jump).
// No-op them for the match build (sizeof keeps args type-checked, zero runtime
// code). MILO_FAIL / MILO_FAIL_DTA are NOT gated — retail retained the
// fatal/abort paths (gating them regresses CharBoneDir/CharFaceServo via inlined
// Find<> fail-paths). HX_NATIVE keeps real output for the native port.
#ifdef HX_NATIVE
#define MILO_WARN(...) TheDebugWarner << MakeString(__VA_ARGS__)
#else
#define MILO_WARN(...) ((void)sizeof(MakeString(__VA_ARGS__)))
#endif
// DTA runtime errors: FAIL on Xbox (shows dialog + Continue), WARN on native
#ifdef HX_NATIVE
#define MILO_FAIL_DTA(...) TheDebugWarner << MakeString(__VA_ARGS__)
#else
#define MILO_FAIL_DTA(...) TheDebugFailer << MakeString(__VA_ARGS__)
#endif
#ifdef HX_NATIVE
#define MILO_NOTIFY(...) TheDebugNotifier << MakeString(__VA_ARGS__)
#define MILO_NOTIFY_BETA(...) DebugBeta() << MakeString(__VA_ARGS__)
#else
#define MILO_NOTIFY(...) ((void)sizeof(MakeString(__VA_ARGS__)))
#define MILO_NOTIFY_BETA(...) ((void)sizeof(MakeString(__VA_ARGS__)))
#endif
#ifdef HX_NATIVE
#define MILO_LOG(...) do { TheDebug << MakeString(__VA_ARGS__); fprintf(stderr, "%s", MakeString(__VA_ARGS__)); } while(0)
#else
#define MILO_LOG(...) ((void)sizeof(MakeString(__VA_ARGS__)))
#endif

// Usage:
// MILO_TRY {
//     // The code to try
// } MILO_CATCH(errMsg) {
//     // Use errMsg here, e.g.:
//     MILO_NOTIFY("An unexpected thing happened: %s", errMsg);
// }
#define MILO_TRY                                                                         \
    TheDebug.SetTry(true);                                                               \
    try {                                                                                \
        do

#define MILO_CATCH(name)                                                                 \
    while (false)                                                                        \
        ;                                                                                \
    TheDebug.SetTry(false);                                                              \
    }                                                                                    \
    catch (const char *name)

// (min) <= (value) && (value) < (max)
#define MILO_ASSERT_RANGE(value, min, max, line)                                         \
    MILO_ASSERT((min) <= (value) && (value) < (max), line)

// (min) <= (value) && (value) <= (max)
#define MILO_ASSERT_RANGE_EQ(value, min, max, line)                                      \
    MILO_ASSERT((min) <= (value) && (value) <= (max), line)

class DebugWarner {
public:
    void operator<<(const char *c) { TheDebug.Warn(c); }
};

extern DebugWarner TheDebugWarner;

class DebugNotifier {
public:
    void operator<<(const char *c) { TheDebug.Notify(c); }
};

extern DebugNotifier TheDebugNotifier;

class DebugFailer {
public:
    void operator<<(const char *cc) { TheDebug.Fail(cc, nullptr); }
};

extern DebugFailer TheDebugFailer;

class DebugNotifyOncePrinter {
    char msg[0x100];

public:
    void operator<<(const char *cc) {
        if (strcmp(msg, cc)) {
            strncpy(msg, cc, 0xFF);
            TheDebug.Print(cc);
        }
    }
};

extern DebugNotifyOncePrinter TheDebugNotifyOncePrinter;

#ifdef HX_NATIVE
#define MILO_PRINT_ONCE(...) TheDebugNotifyOncePrinter << MakeString(__VA_ARGS__)
#else
#define MILO_PRINT_ONCE(...) ((void)sizeof(MakeString(__VA_ARGS__)))
#endif

namespace {
    inline bool AddToStrings(const char *name, std::list<String> &strings) {
        unsigned int count = 0;
        std::list<String>::iterator it = strings.begin();
        for (; it != strings.end(); ++it)
            count++;
        if (count > 0x10)
            return false;
        it = strings.begin();
        for (; it != strings.end(); ++it) {
            if (strcmp(it->c_str(), name) == 0)
                return false;
        }
        String s(name);
        strings.push_back(s);
        return true;
    }
}

class DebugNotifyOncer {
private:
    std::list<String> mStrings;

public:
    DebugNotifyOncer() {}
    ~DebugNotifyOncer() {}

    void operator<<(const char *cc) {
        if (AddToStrings(cc, mStrings)) {
            TheDebugNotifier << cc;
        }
    }
};

#ifdef HX_NATIVE
#define MILO_NOTIFY_ONCE(...)                                                            \
    {                                                                                    \
        static DebugNotifyOncer _dw;                                                     \
        _dw << MakeString(__VA_ARGS__);                                                  \
    }
#else
// Brace-block no-op (some call sites omit the trailing ';').
#define MILO_NOTIFY_ONCE(...)                                                            \
    { (void)sizeof(MakeString(__VA_ARGS__)); }
#endif

class DebugWarnOncer {
private:
    std::list<String> mStrings;

public:
    DebugWarnOncer() {}
    ~DebugWarnOncer() {}

    void operator<<(const char *cc) {
        if (AddToStrings(cc, mStrings)) {
            TheDebugWarner << cc;
        }
    }
};

#ifdef HX_NATIVE
#define MILO_WARN_ONCE(...)                                                              \
    {                                                                                    \
        static DebugWarnOncer _dw;                                                       \
        _dw << MakeString(__VA_ARGS__);                                                  \
    }
#else
// Brace-block no-op (some call sites omit the trailing ';').
#define MILO_WARN_ONCE(...)                                                              \
    { (void)sizeof(MakeString(__VA_ARGS__)); }
#endif
