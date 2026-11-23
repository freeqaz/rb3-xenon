// rb3-xenon native -- A18: Rnd::DrawPreClear dispatches the list retail does.
//
// THE REFERENCE is band.exe (TU5, base 0x82000000), fn_824158B8, the
// ?DrawPreClear@Rnd@@MAAXXZ body (0x824158B8..0x82415B34). Its list
// select is at +0x1E0..+0x1F0 (0x82415A98..0x82415AA8):
//     lbz   r11, 0x150(r26)     mReleaseImmediate
//     addi  r11, r26, 0x128     mPreClearDraws
//     bne   +0x1F4              set: keep it
//     addi  r11, r26, 0x13c     clear: mDraws
// and PreClearDrawAddOrRemove (fn_82415C10) files a drawable
// in 0x128 when b3 is set and in 0x13c when it is not. So a drawable registered
// with b3 = false (OutfitConfig, TexRenderer, TexProc, StreamRenderer) is drawn on
// a normal frame, and one registered with b3 = true (TexMovie during the splash)
// is drawn while mReleaseImmediate is set.
//
// The native arm used to select the other list, so a normal frame drew
// mPreClearDraws and no b3 = false drawable was ever pre-clear drawn.
//
// The case: two probe drawables, one per list, against the real TheRnd. Each
// counts its DrawPreClear calls.
//   preclear-select-normal     mReleaseImmediate = 0: the mDraws probe is drawn
//                              once, the mPreClearDraws probe is not.
//   preclear-select-immediate  mReleaseImmediate = 1: the reverse.
// Run in a forked child (no GPU work: the probes only count), so a crash is a
// failed gate, not a dead runner.

#include "rndobj/Draw.h"
#include "rndobj/Rnd.h"

#include <cstdarg>
#include <cstdio>
#include <sys/wait.h>
#include <unistd.h>

typedef void (*GateFn)(const char *, bool, const char *);

namespace {
    GateFn gGate;

    void Gate(const char *name, bool pass, const char *fmt, ...) {
        char buf[512];
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(buf, sizeof(buf), fmt, ap);
        va_end(ap);
        gGate(name, pass, buf);
    }

    class PreClearProbe : public RndDrawable {
    public:
        int mCalls;
        PreClearProbe() : mCalls(0) {}
        virtual void DrawPreClear() { mCalls++; }
    };

    // Rnd::DrawPreClear is protected; call the Rnd body itself (the engine's
    // renderer does not override it).
    struct RndPeek : Rnd {
        static void PreClear(Rnd &r) { static_cast<RndPeek &>(r).Rnd::DrawPreClear(); }
    };

    enum {
        kNormalDraws = 1 << 0,    // RI=0: mDraws probe drawn once
        kNormalPreClear = 1 << 1, // RI=0: mPreClearDraws probe not drawn
        kImmDraws = 1 << 2,       // RI=1: mDraws probe not drawn
        kImmPreClear = 1 << 3,    // RI=1: mPreClearDraws probe drawn once
        kRan = 1 << 4,
    };

    int PreClearChild() {
        int bits = 0;
        PreClearProbe *inDraws = new PreClearProbe;    // b3 = false -> mDraws
        PreClearProbe *inPreClear = new PreClearProbe; // b3 = true -> mPreClearDraws
        TheRnd.PreClearDrawAddOrRemove(inDraws, true, false);
        TheRnd.PreClearDrawAddOrRemove(inPreClear, true, true);
        bool saved = TheRnd.mReleaseImmediate;

        TheRnd.mReleaseImmediate = false;
        RndPeek::PreClear(TheRnd);
        fprintf(stderr,
                "  [preclear-select] mReleaseImmediate=0: mDraws probe %d call(s), "
                "mPreClearDraws probe %d\n",
                inDraws->mCalls, inPreClear->mCalls);
        if (inDraws->mCalls == 1)
            bits |= kNormalDraws;
        if (inPreClear->mCalls == 0)
            bits |= kNormalPreClear;

        inDraws->mCalls = inPreClear->mCalls = 0;
        TheRnd.mReleaseImmediate = true;
        RndPeek::PreClear(TheRnd);
        fprintf(stderr,
                "  [preclear-select] mReleaseImmediate=1: mDraws probe %d call(s), "
                "mPreClearDraws probe %d\n",
                inDraws->mCalls, inPreClear->mCalls);
        if (inDraws->mCalls == 0)
            bits |= kImmDraws;
        if (inPreClear->mCalls == 1)
            bits |= kImmPreClear;

        TheRnd.mReleaseImmediate = saved;
        TheRnd.PreClearDrawAddOrRemove(inDraws, false, false);
        TheRnd.PreClearDrawAddOrRemove(inPreClear, false, true);
        bits |= kRan;
        return bits;
    }
}

int RunPreClearSelectPhase(GateFn gate) {
    gGate = gate;
    printf("\n=== A18: Rnd::DrawPreClear selects retail's list (band.exe fn_824158B8) ===\n");
    fflush(stdout);
    fflush(stderr);
    pid_t pid = fork();
    if (pid == 0) {
        alarm(60);
        _exit(PreClearChild());
    }
    int status = 0;
    waitpid(pid, &status, 0);
    bool exited = WIFEXITED(status);
    int bits = exited ? WEXITSTATUS(status) : 0;
    const char *how = exited ? "exited" : "crashed";
    Gate("preclear-select-normal",
         exited && (bits & kRan) && (bits & kNormalDraws) && (bits & kNormalPreClear),
         "mReleaseImmediate=0 draws mDraws (0x13c), not mPreClearDraws (child %s, bits 0x%02x)",
         how, bits);
    Gate("preclear-select-immediate",
         exited && (bits & kRan) && (bits & kImmDraws) && (bits & kImmPreClear),
         "mReleaseImmediate=1 draws mPreClearDraws (0x128), not mDraws");
    return 0;
}
