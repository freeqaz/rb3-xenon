#pragma once
#include "utl/TextStream.h"
#include "utl/TextFileStream.h"
#include <list>
#include <string.h>

typedef void ExitCallbackFunc(void);
typedef void FixedStringFunc(FixedString &);

// size 0x100 (retail RB3-360). DC3 added a Crucible telemetry block
// (mFailAppendCallbacks/mCrucibleCallback + mCrucibleHostname/App/Project,
// mKernelVersion, unk124, mHostName) that RB3 never had -- confirmed against
// rb3-Wii's Debug.h, which ends at mNotifyThreadMsg with the same 0x100 total,
// and against the literal `new Debug()` allocation size embedded in
// DataFile.cpp's DataWriteFile (retail: 0x100, ours was 0x144 before this).
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
    // 0x30 is a struct, StackData
    unsigned int mFailThreadStack[50]; // starts at 0x30
    const char *mFailThreadMsg; // 0xf8
    const char *mNotifyThreadMsg; // 0xfc

public:
    Debug();
    virtual ~Debug();
    virtual void Print(const char *);

    void Poll();
    void SetDisabled(bool);
    void SetTry(bool);
    void AddExitCallback(ExitCallbackFunc *func) { mExitCallbacks.push_front(func); }
    void RemoveExitCallback(ExitCallbackFunc *);
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
// Retail RB3-360 stripped the debug-OUTPUT family's EMISSION (WARN/NOTIFY/LOG/
// PRINT_ONCE/NOTIFY_ONCE/WARN_ONCE): their format strings are absent from orig
// band.exe and no Warn/Notify/Print calls survive. BUT — like MILO_FAIL above —
// retail still EVALUATED the argument expressions of the non-ONCE macros
// (verified via Ghidra callee IDs: TickFormat, PathName/ClassName vcalls,
// MetaPerformer::Current()->Song() all survive at stripped warn sites). So
// WARN/NOTIFY/NOTIFY_BETA/LOG use the same ((void)(args)) comma form as
// MILO_FAIL: each arg evaluated exactly once, in order; format-string literal +
// MakeString + emission vanish. A/B (2026-07-10, worktree warn-lever): +6 strict
// (FileMerger FindMerger/FindMergerIndex via NOTIFY PathName; CharClipDriver
// SetBeatOffset via NOTIFY; band3 CharData fn_825BE52C via WARN; SongUpgradeMgr
// fn_82631F18/fn_82631FA8 via LOG Sym(0)).
// The ONCE variants stay sizeof-stripped: eval-ing them gained 0 and lost 3
// TexBlender funclets (spurious EH/temp scopes where retail has none).
// MILO_FAIL / MILO_FAIL_DTA are NOT gated — retail retained the fatal/abort
// paths. HX_NATIVE keeps real output for the native port.
#ifdef HX_NATIVE
#define MILO_WARN(...) TheDebugWarner << MakeString(__VA_ARGS__)
#else
#define MILO_WARN(...) ((void)(__VA_ARGS__))
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
#define MILO_NOTIFY(...) ((void)(__VA_ARGS__))
#define MILO_NOTIFY_BETA(...) ((void)(__VA_ARGS__))
#endif
#ifdef HX_NATIVE
#define MILO_LOG(...) do { TheDebug << MakeString(__VA_ARGS__); fprintf(stderr, "%s", MakeString(__VA_ARGS__)); } while(0)
#elif defined(RB3_LOG_NO_EVAL)
// Per-TU opt-out (/DRB3_LOG_NO_EVAL in objects.json extra_cflags; only works
// for NON-PCH TUs — Debug.h is baked into the PCH, so the #if resolves at
// PCH-create time for PCH TUs). Needed for band3/game/Game.cpp: retail's
// Game::Handle cross-jumps the HANDLE_EXPR(print_base_points) Song() eval, and
// LOG-evaluating PrintBasePoints' inlined `Current()->Song()` arg splits the
// shared Symbol temp slot (0x58 -> 0x54/0x58), breaking the tail-merge (-1).
#define MILO_LOG(...) ((void)sizeof(MakeString(__VA_ARGS__)))
#else
#define MILO_LOG(...) ((void)(__VA_ARGS__))
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
#elif defined(RB3_NOTIFY_ONCE_EVAL)
// Per-TU opt-in (/DRB3_NOTIFY_ONCE_EVAL in objects.json extra_cflags; NON-PCH
// TUs only). Retail RB3 stripped MILO_NOTIFY_ONCE of its MakeString/notify and
// once-guard but KEPT the side-effecting argument calls, evaluated right-to-left
// as function arguments (the unused format-string address is DCE'd). See
// char/CharFaceServo.cpp::ScaleAdd — retail still emits PathName(clip)/PathName(this).
namespace {
    inline void _MiloNotifyOnceEval(const char *) {}
    template <class A> inline void _MiloNotifyOnceEval(const char *, A) {}
    template <class A, class B>
    inline void _MiloNotifyOnceEval(const char *, A, B) {}
    template <class A, class B, class C>
    inline void _MiloNotifyOnceEval(const char *, A, B, C) {}
    template <class A, class B, class C, class D>
    inline void _MiloNotifyOnceEval(const char *, A, B, C, D) {}
}
#define MILO_NOTIFY_ONCE(...)                                                            \
    { _MiloNotifyOnceEval(__VA_ARGS__); }
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
