#pragma once
//
// X4c — boot-time invariant checks for the hand-rolled native bring-up.
//
// WHY THIS FILE EXISTS
// --------------------
// All 18 native drivers hand-roll a reduced bring-up instead of calling the
// real PreInitSystem()/SystemInit() (os/System.cpp:472 / :512). That is a
// deliberate and largely defensible choice for a headless tool, but it has a
// failure mode that has now bitten TWICE, and both times SILENTLY:
//
//   X4b defect 1  TrigTableInit() never ran, so gBigSinTable stayed all-zero
//                 and EVERY Sine()/Cosine() in the whole native tree returned
//                 0.0 -- across all 17 targets, for four milestones, with no
//                 assert, no log line, and no crash.
//   X4b defect 2  SetGfxMode(kNewGfx) never ran, so gGfxMode stayed at its
//                 zero-init value kOldGfx, so RndMesh::MaxBones() (Mesh.h:227)
//                 answers 4 instead of 40, so RndMesh::Load DESTRUCTIVELY
//                 truncates every skinned mesh's bone list (Mesh.cpp:567-578).
//
// Both share one shape: **a zero-initialised global, a getter with no validity
// check, and a sub-init that nobody called.** A zero-initialised global is
// indistinguishable from a legitimately-initialised one at every read site, so
// the program does not fail -- it quietly computes the wrong answer.
//
// The lesson X4b drew, and the reason for this header: a render can look
// entirely plausible while six bones are singular and world-transform
// determinants have reached -3.9e14. Pictures are not oracles. These checks
// are cheap, run once at boot, and each one is written so that it would have
// caught a defect that actually shipped.
//
// USAGE
//   #include "boot_invariants.h"
//   ...after the archive is mounted and the config is stood up...
//   BootInvariants::CheckAll();            // prints a report, returns #failures
//
// The check is ADVISORY BY DEFAULT (it reports and returns a count) because
// several drivers legitimately skip sub-inits they do not need. Set
// RB3_STRICT_BOOT=1 to make any failure fatal.
//
#include "math/Trig.h"
#include "os/System.h"
#include "utl/Symbol.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace BootInvariants {

    inline bool Near(float a, float b, float tol) { return std::fabs(a - b) <= tol; }

    // ---------------------------------------------------------------------
    // 1. TRIG TABLE.  Would have caught X4b defect 1 in X1.
    //
    // Sine() is NOT std::sin -- it is a lookup into gBigSinTable (Trig.cpp:33),
    // which has static storage and is therefore legitimately all zeroes until
    // TrigTableInit() fills it. Three points pin the table down: sin(0)==0
    // catches nothing on its own (an all-zero table passes it), so it is the
    // sin(pi/2)==1 and cos(0)==1 probes that carry the check. Deliberately
    // includes a value an all-zero table would pass, so the test is not
    // trivially satisfiable by the failure mode it targets.
    inline bool CheckTrigTable(const char *&detail) {
        static const float kPi = 3.14159265358979f;
        float s90 = Sine(kPi * 0.5f);
        float c0 = Cosine(0.0f);
        float s0 = Sine(0.0f);
        static char buf[160];
        snprintf(buf, sizeof(buf), "Sine(pi/2)=%.4f (want 1) Cosine(0)=%.4f (want 1) "
                                   "Sine(0)=%.4f (want 0)",
                 s90, c0, s0);
        detail = buf;
        return Near(s90, 1.0f, 1e-3f) && Near(c0, 1.0f, 1e-3f) && Near(s0, 0.0f, 1e-3f);
    }

    // ---------------------------------------------------------------------
    // 2. GFX MODE / BONE CAP.  Would have caught X4b defect 2 in X2.
    //
    // This is the one check that is NOT simply "must be initialised", because
    // whether kNewGfx is correct for a given headless driver is a real
    // question (X4c measured that flipping it is necessary but exposes a
    // separate, pre-existing clip-decode defect). So it reports the CONSEQUENCE
    // rather than asserting a value: at kOldGfx any skinned mesh with more than
    // 4 bones is silently truncated at load, and the shipped RB3 crowd assets
    // ship 12- and 20-bone meshes. A driver that loads skinned characters and
    // leaves this at kOldGfx is getting wrong geometry, and should say so.
    inline bool CheckGfxMode(const char *&detail, bool loadsSkinnedMeshes) {
        static char buf[224];
        bool isNew = (GetGfxMode() != kOldGfx);
        snprintf(buf, sizeof(buf),
                 "GetGfxMode()=%s -> RndMesh::MaxBones()==%d%s", isNew ? "kNewGfx" : "kOldGfx",
                 isNew ? 40 : 4,
                 isNew ? ""
                       : "  [any skinned mesh with >4 bones is TRUNCATED at load "
                         "(Mesh.cpp:567-578) -- RB3 crowd assets ship 12- and 20-bone "
                         "meshes]");
        detail = buf;
        return isNew || !loadsSkinnedMeshes;
    }

    // ---------------------------------------------------------------------
    // 3. SYMBOL TABLE. Symbol::Init() guards on `if (!gStringTable)`
    // (utl/Symbol.cpp:67) so it is idempotent, but if it never ran at all every
    // Symbol construction is undefined. Round-tripping a string through a
    // Symbol is the cheapest positive proof the table is live.
    inline bool CheckSymbolTable(const char *&detail) {
        static char buf[128];
        Symbol probe("x4c_boot_probe");
        const char *back = probe.Str();
        bool ok = back && *back && !strcmp(back, "x4c_boot_probe");
        snprintf(buf, sizeof(buf), "Symbol round-trip -> '%s'", back ? back : "(null)");
        detail = buf;
        return ok;
    }

    // ---------------------------------------------------------------------
    // 4. SYSTEM CONFIG. PreInitSystem sets DataVariable("syscfg"); several
    // engine reads (notably Rnd::SetupFont, which INDEXES elements 66..123 of
    // SystemConfig("rnd","font")) are not tolerant of a missing or synthesised
    // section -- an empty DataArray there is an out-of-range Array(), not a
    // degraded font.
    inline bool CheckSystemConfig(const char *&detail) {
        static char buf[128];
        DataArray *cfg = SystemConfig();
        int n = cfg ? cfg->Size() : -1;
        snprintf(buf, sizeof(buf), "SystemConfig() = %p, %d top-level section(s)",
                 (void *)cfg, n);
        detail = buf;
        return cfg && n > 0;
    }

    // ---------------------------------------------------------------------
    inline int CheckAll(bool loadsSkinnedMeshes = true) {
        struct Row {
            const char *name;
            bool ok;
            const char *detail;
        };
        Row rows[4];
        const char *d = "";
        rows[0].name = "trig-table";     rows[0].ok = CheckTrigTable(d);     rows[0].detail = d;
        rows[1].name = "symbol-table";   rows[1].ok = CheckSymbolTable(d);   rows[1].detail = d;
        rows[2].name = "system-config";  rows[2].ok = CheckSystemConfig(d);  rows[2].detail = d;
        rows[3].name = "gfx-mode";
        rows[3].ok = CheckGfxMode(d, loadsSkinnedMeshes);
        rows[3].detail = d;

        int fails = 0;
        printf("  --- boot invariants ---\n");
        for (int i = 0; i < 4; i++) {
            printf("  [%s] %s — %s\n", rows[i].ok ? "PASS" : "FAIL", rows[i].name,
                   rows[i].detail);
            if (!rows[i].ok) fails++;
        }
        if (fails && getenv("RB3_STRICT_BOOT")) {
            fprintf(stderr,
                    "FATAL: %d boot invariant(s) failed and RB3_STRICT_BOOT is set\n",
                    fails);
            exit(3);
        }
        return fails;
    }

} // namespace BootInvariants
