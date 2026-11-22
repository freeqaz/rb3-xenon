// rb3-xenon native -- A11: Rnd::Mode carries RB3 retail's draw-mode values, and
// the shader selection keys off them the way retail does.
//
// THE DEFECT. rndobj/Rnd.h's Mode enum was DC3's (kDrawOcclusionDepth = 5,
// kDrawVelocity = 6). The shared rndobj code stores RB3 retail's raw values,
// so on native the soft-particle pass (RndSoftParticleBuffer::DoPost stores 6)
// read as kDrawVelocity and RndShader::SelectConfig's native arm drew every
// soft particle with the velocity-object shader, while the velocity pass's 5
// selected nothing by name. NgEnviron::Select's native arm compared DC3's
// {4, 2, 6, 3} where retail early-outs on {3, 1, 5, 2}.
//
// THE REFERENCE is band.exe (TU-less retail, base 0x82000000). Every access is
// TheRnd->mDrawMode, Rnd +0xfc, through the Rnd & at 0x82C76B68:
//
//   value  pass                                   retail evidence
//   0      normal                                 Rnd ctor; 15 compares against 0
//   1      RndShadowMap caster pass               stw 1 at 0x82464388;
//                                                 SelectConfig 0x824A5740 cmpwi 1 -> shadowmap shader
//   2      spot-cutout / extrude                  compared only: Character::DrawLod 0x8236D460
//                                                 (needs mSpotCutout), Shader.cpp x4
//   3      NgLight projected-shadow colour pass   stw 3 at 0x82B8A834 (0x82B8A7D0);
//                                                 DrawLod needs mFloorShadow
//   4      occlusion                              compared only: DrawLod returns, Crowd
//                                                 0x824E3134, Spotlight 0x824D97B0
//   5      RndVelocityBuffer::Draw                stw 5 at 0x82B85820; DxMesh::DrawShowing
//                                                 cmpwi 5 at 0x82738E64; SelectConfig cmpwi 5
//   6      RndSoftParticleBuffer::DoPost          stw 6 at 0x824A8C7C
//   7      WorldReflection::DrawShowing           stw 7 at 0x824E8994
//
// Names 0..4 are the RB3 (Wii) decomp's enum, whose shared passes store the same
// values (ShadowMap 1, Reflection 7, Crowd compares kDrawOcclusion 4); 5..7 are
// named for the pass. DC3 numbers 1..7 as 2..8.
//
// The behavioural cases run SelectConfig in a forked child against a stand-in
// TheRnd and TheShaderMgr (rb3-milo links no renderer: both are null
// references) with a recording shader in every sShaders slot.

#include "rndobj/Rnd.h"
#include "rndobj/Shader.h"
#include "rndobj/ShaderMgr.h"

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

typedef void (*GateFn)(const char *, bool, const char *);

// The reference storage of the two renderer singletons, by symbol name.
extern char gRndModeTheRndRef[] __asm__("TheRnd");
extern char gRndModeTheShaderMgrRef[] __asm__("TheShaderMgr");

namespace {

    GateFn gGate = nullptr;

    void Gate(const char *name, bool ok, const char *fmt, ...)
        __attribute__((format(printf, 3, 4)));
    void Gate(const char *name, bool ok, const char *fmt, ...) {
        char buf[512];
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(buf, sizeof(buf), fmt, ap);
        va_end(ap);
        gGate(name, ok, buf);
    }

    struct RetailMode {
        Rnd::Mode name;
        int retail;
        const char *pass;
    };
    const RetailMode kTable[] = {
        { Rnd::kDrawNormal, 0, "normal" },
        { Rnd::kDrawShadowDepth, 1, "RndShadowMap (stw 0x82464388)" },
        { Rnd::kDrawExtrude, 2, "spot cutout (Character::DrawLod 0x8236D460)" },
        { Rnd::kDrawShadowColor, 3, "NgLight shadow colour (stw 0x82B8A834)" },
        { Rnd::kDrawOcclusion, 4, "occlusion (Crowd 0x824E3134)" },
        { Rnd::kDrawVelocity, 5, "RndVelocityBuffer (stw 0x82B85820)" },
        { Rnd::kDrawSoftParticle, 6, "RndSoftParticleBuffer (stw 0x824A8C7C)" },
        { Rnd::kDrawReflection, 7, "WorldReflection (stw 0x824E8994)" },
    };

    // Records the shader type SelectConfig dispatches to.
    struct ModeRecorder : public RndShader {
        static int sLast;
        virtual void Select(RndMat *, ShaderType s, bool) { sLast = s; }
        virtual u64 CalcShaderOpts(NgMat *, ShaderType, bool) { return 0; }
        static void Install(RndShader *r) {
            for (int i = 0; i < kMaxShaderTypes; i++)
                sShaders[i] = r;
        }
    };
    int ModeRecorder::sLast = -1;

    void PointRef(char *ref, void *target) {
        long page = sysconf(_SC_PAGESIZE);
        uintptr_t base = (uintptr_t)ref & ~(uintptr_t)(page - 1);
        mprotect((void *)base, (size_t)page * 2, PROT_READ | PROT_WRITE);
        memcpy(ref, &target, sizeof(target));
    }

    // The shader SelectConfig picks for a standard material in raw mode m.
    int SelectedFor(int m) {
        TheRnd.SetDrawMode((Rnd::Mode)m);
        ModeRecorder::sLast = -1;
        RndShader::SelectConfig(nullptr, kStandardShader, true);
        return ModeRecorder::sLast;
    }

    int ExpectedFor(int m) {
        if (m == 1) return kShadowmapShader;     // retail cmpwi 1 at 0x824A574C
        if (m == 5) return kVelocityObjectShader; // retail cmpwi 5
        return kStandardShader;                   // not in depth volume
    }

    // Child: bit m of the exit code = mode m selected retail's shader.
    int SelectionChild() {
        void *rnd = calloc(1, sizeof(Rnd));
        void *mgr = calloc(1, sizeof(RndShaderMgr));
        PointRef(gRndModeTheRndRef, rnd);
        PointRef(gRndModeTheShaderMgrRef, mgr);
        static ModeRecorder recorder;
        ModeRecorder::Install(&recorder);
        int bits = 0;
        for (int m = 0; m < 8; m++) {
            int got = SelectedFor(m);
            if (got == ExpectedFor(m)) bits |= 1 << m;
            fprintf(stderr, "  [rnd-mode] mode %d -> shader %d (retail %d)\n", m, got,
                    ExpectedFor(m));
        }
        return bits;
    }
}

int RunRndModePhase(GateFn gate) {
    gGate = gate;
    printf("\n=== A11: Rnd::Mode is RB3 retail's draw-mode numbering ===\n");

    int bad = 0;
    char detail[512] = "";
    for (const RetailMode &r : kTable) {
        if ((int)r.name != r.retail) {
            bad++;
            size_t n = strlen(detail);
            snprintf(detail + n, sizeof(detail) - n, "%s=%d not %d; ", r.pass, (int)r.name,
                     r.retail);
        }
    }
    Gate("rnd-mode-retail-table", bad == 0, "%d of %d enumerators off retail. %s", bad,
         (int)(sizeof(kTable) / sizeof(kTable[0])), detail);

    fflush(stdout);
    fflush(stderr);
    pid_t pid = fork();
    if (pid == 0) {
        alarm(20);
        _exit(SelectionChild());
    }
    int status = 0;
    waitpid(pid, &status, 0);
    bool exited = WIFEXITED(status);
    int bits = exited ? WEXITSTATUS(status) : 0;
    // Control: mode 0 must come back as the requested shader; if not, the
    // stand-in harness, not the mode table, is wrong.
    Gate("rnd-mode-select-control", exited && (bits & 1),
         "normal pass selects the material's own shader (child %s, bits 0x%02x)",
         exited ? "exited" : "crashed", bits);
    Gate("rnd-mode-shadow-pass-selects-shadowmap", exited && (bits & (1 << 1)),
         "mode 1 (RndShadowMap) -> kShadowmapShader");
    Gate("rnd-mode-velocity-pass-selects-velocity", exited && (bits & (1 << 5)),
         "mode 5 (RndVelocityBuffer) -> kVelocityObjectShader");
    Gate("rnd-mode-softparticle-pass-not-velocity", exited && (bits & (1 << 6)),
         "mode 6 (RndSoftParticleBuffer) keeps the material's shader");
    Gate("rnd-mode-other-passes-keep-shader", exited && (bits & 0x9d) == 0x9d,
         "modes 0,2,3,4,7 keep the material's shader (bits 0x%02x)", bits);
    return 0;
}
