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
    void AddFailCallback(ExitCallbackFunc *func) { mFailCallbacks.push_front(func); }
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

#ifndef HX_NATIVE
// ---------------------------------------------------------------------------
// Stripped debug-output argument sink (retail RB3-360).
//
// Retail's stripped MILO_WARN/NOTIFY/LOG sites do NOT just evaluate their args
// as a comma expression -- they COPY-CONSTRUCT the class-typed ones. Evidence:
// SongParser::ParseText (0x827840C8) has four copies of
//     addi r3,r31,0x60 / addi r4,r30,0x28 / stw r3,0x54(r31)
//     bl ??0String@@QAA@ABV0@@Z ... bl TickFormat ... bl ??1String@@UAA@XZ
// plus four EH funclets destroying those temps -- i.e. `mFilename` (a String
// member) is copied into a temporary that is destroyed at end-of-full-expression.
// That is exactly what MakeString's BY-VALUE template parameters (T1 t1, ...)
// do. The format-string literal is NOT materialised at those sites, so the
// callee body must be gone.
//
// MiloStripEval reproduces both halves: by-value params force the copy ctor +
// destructible temp (and hence the EH state/funclet), while the empty inline
// body lets /Ob2 delete the call and DCE the unused format literal.
inline void MiloStripEval(const char *) {}
template <class T1> inline void MiloStripEval(const char *, T1) {}
template <class T1, class T2> inline void MiloStripEval(const char *, T1, T2) {}
template <class T1, class T2, class T3>
inline void MiloStripEval(const char *, T1, T2, T3) {}
template <class T1, class T2, class T3, class T4>
inline void MiloStripEval(const char *, T1, T2, T3, T4) {}
template <class T1, class T2, class T3, class T4, class T5>
inline void MiloStripEval(const char *, T1, T2, T3, T4, T5) {}
template <class T1, class T2, class T3, class T4, class T5, class T6>
inline void MiloStripEval(const char *, T1, T2, T3, T4, T5, T6) {}
template <class T1, class T2, class T3, class T4, class T5, class T6, class T7>
inline void MiloStripEval(const char *, T1, T2, T3, T4, T5, T6, T7) {}
template <class T1, class T2, class T3, class T4, class T5, class T6, class T7, class T8>
inline void MiloStripEval(const char *, T1, T2, T3, T4, T5, T6, T7, T8) {}
template <
    class T1,
    class T2,
    class T3,
    class T4,
    class T5,
    class T6,
    class T7,
    class T8,
    class T9>
inline void MiloStripEval(const char *, T1, T2, T3, T4, T5, T6, T7, T8, T9) {}
template <
    class T1,
    class T2,
    class T3,
    class T4,
    class T5,
    class T6,
    class T7,
    class T8,
    class T9,
    class T10>
inline void MiloStripEval(const char *, T1, T2, T3, T4, T5, T6, T7, T8, T9, T10) {}
#endif

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
// to NO code. But unlike the WARN family, the FAIL *arguments*
// were still EVALUATED for side effects (Find<T>'s fail path calls PathName(this)
// twice — the ternary — but never MakeString/Fail). So use (void)(args), a comma
// (CORRECTED, lane W13-CHARINFO: this block used to justify the above with
// "DebugFailer::operator<< (0x8235C970) has ZERO bl callers ... and the fail
// strings are absent from .text/.rdata". BOTH halves are FALSE.
// `"Could not find %s in dir \"%s\""` IS present in .rdata
// (auto_00_82000400_rdata.s), and 0x8235C970 names no function at all — it is
// interior to fn_8235C94C, a 40-byte body in CharClip.s. The macro definition
// below is unaffected: it rests on the argument-evaluation argument above,
// which was checked independently and stands.)
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
//
// UPDATE 2026-07-26 (worktree gbE): the comma form was INCOMPLETE for WARN.
// Retail's stripped WARN sites also COPY-CONSTRUCT class-typed args (see the
// MiloStripEval block above), which the comma form never does. Switching only
// MILO_WARN to MiloStripEval measured whole-binary:
//     WARN   -> MiloStripEval : 36705 -> 36738 strict (+33; 41 gained / 8 lost),
//               fuzzy 36.93869 -> 36.955505. SongParser +34 (ParseText's four
//               EH funclets 0x82784380/3A8/3D0/3F8 all flip to 100%).
//               Losses are funclet-pairing shifts: PrefabMgr x5,
//               GamePanel::SetType, Group/MidiSynth anon funclets.
//     NOTIFY -> MiloStripEval : -20 on top of that (36739 -> 36719). It kills
//               ~20 `?SetType@*@@UAAXVSymbol@@@Z` bodies across rndobj/synth/
//               bandobj. DO NOT apply to NOTIFY / NOTIFY_BETA.
//     LOG    -> MiloStripEval : -5 on top of WARN (SongData x5, Console,
//               FileMerger::MergeAction). DO NOT apply to LOG.
//
// CORRECTED 2026-07-27 (lane guardbit-fix) -- WHY the NOTIFY leg lost 20.
// The first reading of that -20 was "NOTIFY only evaluates, it does not copy".
// That is NOT the operative cause.  The real cause is ARGUMENT EVALUATION ORDER:
//
//     MSVC evaluates FUNCTION ARGUMENTS RIGHT-TO-LEFT.
//     A comma expression evaluates LEFT-TO-RIGHT.
//
// MiloStripEval is a function call, so switching a site to it silently REVERSES
// the order in which its argument expressions run.  Where those expressions have
// side effects that the target emits in source order, the body stops matching.
//
// The whole -20 was one family -- `?SetType@*@@UAAXVSymbol@@@Z` -- whose stripped
// residue retail emits LEFT-TO-RIGHT: `PathName(this)` is called BEFORE the
// `ClassName()` vcall.  Control case, decisive: OBJ_SET_TYPE_ENGINE (Object.h)
// spells that residue with MILO_NOTIFY (comma form) and RndGroup::SetType stayed
// at 100.0% through every leg; OBJ_SET_TYPE (ObjMacros.h) spelled the SAME
// residue with MILO_WARN, and GamePanel::SetType fell 100% -> 96.2% the moment
// MILO_WARN became MiloStripEval.  Same code, two macros, one broke.
//
// So the operative rule is two-part, and they are INDEPENDENT:
//   * COPYING   -- retail's stripped WARN residue copy-constructs class-typed
//                  args.  Only MiloStripEval reproduces that.  This is what the
//                  +33 buys (destructible String temps -> EH states -> funclets).
//   * ORDERING  -- retail's residue runs left-to-right.  Only the comma form
//                  reproduces that.  MiloStripEval inverts it.
// A site needs whichever half its arguments actually depend on.  Sites with
// destructible class args need COPYING; sites whose args have ordered side
// effects need ORDERING; a site needing both is not expressible in either form
// and would need the temps hoisted into explicit locals in source order.
// NOTIFY and LOG were measured negative because their populations are dominated
// by ordering-sensitive sites, NOT because retail's NOTIFY residue "does not
// copy" -- we have no evidence either way on that, and should not imply we do.
//
// ==> THE ASYMMETRY IS BY DESIGN, NOT AN OVERSIGHT.  MILO_WARN uses
//     MiloStripEval; MILO_NOTIFY / MILO_NOTIFY_BETA / MILO_LOG deliberately
//     keep ((void)(args)).  Making them uniform is the obvious-looking next
//     step and it is MEASURED NEGATIVE: +33 / -20 / -5 respectively.
//     Do not "finish the job" -- the job is finished.
#ifdef HX_NATIVE
#define MILO_WARN(...) TheDebugWarner << MakeString(__VA_ARGS__)
#else
#define MILO_WARN(...) MiloStripEval(__VA_ARGS__)
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
// DELIBERATE: comma form, NOT MiloStripEval.  The comma operator evaluates
// LEFT-TO-RIGHT; MiloStripEval is a function call and MSVC evaluates function
// arguments RIGHT-TO-LEFT.  This population is dominated by ordering-sensitive
// sites -- ~20 ?SetType@*@@UAAXVSymbol@@@Z bodies whose residue calls
// PathName(this) BEFORE the ClassName() vcall -- so switching these to
// MiloStripEval inverts the order and measured -20 strict whole-binary.
// See the corrected analysis above MILO_WARN.
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
// DELIBERATE: comma form (LEFT-TO-RIGHT), NOT MiloStripEval -- measured -5
// strict (SongData x5, Console, FileMerger::MergeAction).  Same argument-
// ordering cause as NOTIFY; see the corrected analysis above MILO_WARN.
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
