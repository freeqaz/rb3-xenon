// rb3-xenon native -- W16-UJ: every shipped venue milo loads and unloads, and
// float Min/Max/Clamp treat NaN the way retail's fsel does.
//
// docs/decomp/W16UJ_VENUE_LOADS_AND_CLAMP_NAN_2026-10-07.md records the causes,
// the sweep and the sabotage runs. Two native-only defects stopped venues:
//
//   1. ChunkStream::ReadImpl (HX_NATIVE cross-chunk path) failed the stream on
//      TempEof. A read that crosses a whole chunk without an Eof() poll in
//      between reaches the boundary with the next buffer still kReading, so
//      the stream failed and ReadDead then spun on zeros: small_club_15 (W16-TY)
//      and big_club_07 (W16-UF, the "spin").
//   2. ObjOwnerPtr (HX_NATIVE) had no user-declared copy-assign, so the
//      implicit one copied mOwner and the X16 self-seed from the source. Every
//      arena and festival then crashed (SIGSEGV) in its own unload.
//
// THE REFERENCES. A venue loads cleanly when its root comes back, no read was
// attempted on a failed stream (gNativeFailedStreamReads, BinStream.cpp), every
// created object left the stream exactly on its 0xADDEADDE marker (the X4d
// stream audit, DirLoader.cpp, run quietly) and its unload returns. The
// small_club_15 LightPreset names were read from the decompressed shipped
// bytes: its directory table has exactly these 29 LightPreset entries. The NaN
// reference is retail fn_822C7040's two fsel, decoded in Utl.h and written
// down here as a model, not as a call into the code under test.

#include "obj/Dir.h"
#include "obj/DirLoader.h"
#include "obj/Object.h"
#include "math/Utl.h"
#include "os/Archive.h"
#include "os/File.h"
#include "utl/BinStream.h"
#include "utl/FilePath.h"
#include "utl/Loader.h"
#include "world/LightPreset.h"

#include <chrono>
#include <cmath>
#include <csignal>
#include <unistd.h>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include <vector>

// obj/DirLoader.cpp (HX_NATIVE), W16-UJ hooks.
void NativeStreamAuditBegin();
int NativeStreamAuditObjects();
int NativeStreamAuditAnomalies();
int NativeStreamAuditMissSkips();

typedef void (*GateFn)(const char *, bool, const char *);

// obj/Dir.cpp (HX_NATIVE), W16-UL: ObjectDirs constructed and not destroyed.
extern int gNativeLiveObjectDirs;

namespace {

GateFn gGate = nullptr;
char gBuf[2048];
int gRan = 0;

void Gate(const char *name, bool ok, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
void Gate(const char *name, bool ok, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(gBuf, sizeof(gBuf), fmt, ap);
    va_end(ap);
    gGate(name, ok, gBuf);
    gRan++;
    fflush(stdout); // a later fault must not swallow the verdicts already reached
}

// ================================================================ clamp ==
// PowerPC fsel fD,fA,fC,fB: fD = fA >= 0 ? fC : fB, and an unordered fA (NaN)
// selects fB. Retail fn_822C7040 (CompressDelta's Clamp(-2, 2, d)):
//     fsubs f9,f13,f0 ; fsel f0,f9,f13,f0      Max(min, v) = fsel(min - v, min, v)
//     fsubs f9,f0,f12 ; fsel f0,f9,f12,f0      Min(x, max) = fsel(x - max, max, x)
float Fsel(float a, float c, float b) { return a >= 0.0f ? c : b; }
float RetailMax(float x, float y) { return Fsel(x - y, x, y); }
float RetailMin(float x, float y) { return Fsel(x - y, y, x); }
float RetailClamp(float lo, float hi, float v) { return RetailMin(RetailMax(lo, v), hi); }
// The pre-W16-UJ native spelling, for the non-NaN identity check only.
float OldMax(float x, float y) { return (x - y < 0) ? y : x; }
float OldMin(float x, float y) { return (x - y < 0) ? x : y; }

bool Same(float a, float b) {
    if (std::isnan(a) || std::isnan(b))
        return std::isnan(a) && std::isnan(b);
    uint32_t ua, ub;
    memcpy(&ua, &a, 4);
    memcpy(&ub, &b, 4);
    return ua == ub;
}

void ClampChecks() {
    const float inf = INFINITY, nan = NAN;
    const float grid[] = { nan,   -nan, inf,  -inf,  0.0f,  -0.0f, 1e-40f, -1e-40f,
                           1.0f,  -1.0f, 2.0f, -2.0f, 2.5f,  -2.5f, 3e38f,  -3e38f,
                           0.25f, 63.5f };
    const int n = sizeof(grid) / sizeof(grid[0]);
    int nanCases = 0, nanBad = 0, finCases = 0, finBad = 0;
    char first[256] = "";
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            float x = grid[i], y = grid[j];
            float d = x - y;
            bool isNanCase = std::isnan(d);
            float got[3] = { Max(x, y), Min(x, y), Clamp(-2.0f, 2.0f, x) };
            float want[3] = { RetailMax(x, y), RetailMin(x, y), RetailClamp(-2.0f, 2.0f, x) };
            const char *what[3] = { "Max", "Min", "Clamp(-2,2,x)" };
            for (int k = 0; k < 3; k++) {
                bool ok = Same(got[k], want[k]);
                bool nanArg = k < 2 ? isNanCase : std::isnan(x);
                if (nanArg) {
                    nanCases++;
                    if (!ok) {
                        if (!nanBad)
                            snprintf(first, sizeof(first), "%s(%g, %g) = %g, retail fsel %g",
                                     what[k], x, y, got[k], want[k]);
                        nanBad++;
                    }
                } else {
                    finCases++;
                    if (!ok)
                        finBad++;
                }
            }
        }
    }
    // Headline values, from the retail decode in the comment above.
    float c = Clamp(-2.0f, 2.0f, nan);
    float mx = Max(-2.0f, nan), mn = Min(nan, 2.0f);
    Gate("uj-clamp-nan", nanBad == 0 && std::isnan(c) && std::isnan(mx) && std::isnan(mn),
         "%d NaN-operand cases vs the retail fsel model, %d differ%s%s; Clamp(-2,2,NaN) = %g "
         "(retail: NaN), Max(-2,NaN) = %g, Min(NaN,2) = %g",
         nanCases, nanBad, nanBad ? ", first: " : "", first, c, mx, mn);
    // Every case without a NaN difference, against both the model and the old
    // native spelling: the fix must not move a single non-NaN result.
    int oldBad = 0;
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            float x = grid[i], y = grid[j];
            if (std::isnan(x - y))
                continue;
            if (!Same(Max(x, y), OldMax(x, y)) || !Same(Min(x, y), OldMin(x, y)))
                oldBad++;
        }
    }
    Gate("uj-clamp-finite", finBad == 0 && oldBad == 0,
         "%d non-NaN cases: %d differ from the retail fsel model, %d differ from the "
         "pre-W16-UJ native spelling",
         finCases, finBad, oldBad);
}

// ================================================================ venues ==
// The 52 venue milos under world/venue in the shipped Xbox data, sorted.
const char *const kVenues[] = {
    "world/venue/arena/arena_01/gen/arena_01.milo_xbox",
    "world/venue/arena/arena_02/gen/arena_02.milo_xbox",
    "world/venue/arena/arena_02/props/gen/backwall_angles_02.milo_xbox",
    "world/venue/arena/arena_02/props/gen/backwall_middle_02.milo_xbox",
    "world/venue/arena/arena_02/props/gen/backwall_wing_02.milo_xbox",
    "world/venue/arena/arena_02/props/gen/riser_panels02.milo_xbox",
    "world/venue/arena/arena_03/gen/arena_03.milo_xbox",
    "world/venue/arena/arena_04/gen/arena_04.milo_xbox",
    "world/venue/arena/arena_06/gen/arena_06.milo_xbox",
    "world/venue/arena/arena_07/gen/arena_07.milo_xbox",
    "world/venue/arena/arena_10/gen/arena_10.milo_xbox",
    "world/venue/arena/arena_11/gen/arena_11.milo_xbox",
    "world/venue/arena/arena_11/gen/banner.milo_xbox",
    "world/venue/arena/arena_11/gen/stone_block.milo_xbox",
    "world/venue/arena/arena_12/gen/arena_12.milo_xbox",
    "world/venue/big_club/big_club_01/gen/big_club_01.milo_xbox",
    "world/venue/big_club/big_club_02/gen/big_club_02.milo_xbox",
    "world/venue/big_club/big_club_04/gen/big_club_04.milo_xbox",
    "world/venue/big_club/big_club_05/gen/big_club_05.milo_xbox",
    "world/venue/big_club/big_club_06/gen/big_club_06.milo_xbox",
    "world/venue/big_club/big_club_07/gen/big_club_07.milo_xbox",
    "world/venue/big_club/big_club_08/gen/big_club_08.milo_xbox",
    "world/venue/big_club/big_club_09/gen/big_club_09.milo_xbox",
    "world/venue/big_club/big_club_10/gen/big_club_10.milo_xbox",
    "world/venue/big_club/big_club_11/gen/big_club_11.milo_xbox",
    "world/venue/big_club/big_club_12/gen/big_club_12.milo_xbox",
    "world/venue/big_club/big_club_12/gen/test.milo_xbox",
    "world/venue/big_club/big_club_13/gen/big_club_13.milo_xbox",
    "world/venue/big_club/big_club_14/gen/banner_mim.milo_xbox",
    "world/venue/big_club/big_club_14/gen/big_club_14.milo_xbox",
    "world/venue/big_club/big_club_15/gen/big_club_15.milo_xbox",
    "world/venue/big_club/big_club_17/gen/big_club_17.milo_xbox",
    "world/venue/festival/festival_01/gen/festival_01.milo_xbox",
    "world/venue/festival/festival_02/gen/festival_02.milo_xbox",
    "world/venue/small_club/small_club_01/gen/small_club_01.milo_xbox",
    "world/venue/small_club/small_club_02/gen/small_club_02.milo_xbox",
    "world/venue/small_club/small_club_03/gen/small_club_03.milo_xbox",
    "world/venue/small_club/small_club_04/gen/small_club_04.milo_xbox",
    "world/venue/small_club/small_club_05/gen/small_club_05.milo_xbox",
    "world/venue/small_club/small_club_06/gen/small_club_06.milo_xbox",
    "world/venue/small_club/small_club_10/gen/small_club_10.milo_xbox",
    "world/venue/small_club/small_club_11/gen/small_club_11.milo_xbox",
    "world/venue/small_club/small_club_13/gen/small_club_13.milo_xbox",
    "world/venue/small_club/small_club_14/gen/small_club_14.milo_xbox",
    "world/venue/small_club/small_club_15/gen/small_club_15.milo_xbox",
    "world/venue/video/video_01/gen/video_01.milo_xbox",
    "world/venue/video/video_02/gen/video_02.milo_xbox",
    "world/venue/video/video_03/gen/video_03.milo_xbox",
    "world/venue/video/video_04/gen/video_04.milo_xbox",
    "world/venue/video/video_05/gen/video_05.milo_xbox",
    "world/venue/video/video_06/gen/video_06.milo_xbox",
    "world/venue/video/video_07/gen/video_07.milo_xbox",
};

// small_club_15's directory table, read from the decompressed shipped bytes.
const char *const kSc15Presets[] = {
    "coop_Stomp.pst",         "coop_blackout_fast.pst", "coop_blackout_slow.pst",
    "coop_blackout_spot.pst", "coop_bre.pst",           "coop_chorus.pst",
    "coop_dischord.pst",      "coop_flare_fast.pst",    "coop_flare_slow.pst",
    "coop_frenzy.pst",        "coop_harmony.pst",       "coop_intro.pst",
    "coop_intro_quick.pst",   "coop_intro_venue.pst",   "coop_loop_cool.pst",
    "coop_loop_warm.pst",     "coop_lose.pst",          "coop_manual_cool.pst",
    "coop_manual_warm.pst",   "coop_searchlights.pst",  "coop_silhouettes.pst",
    "coop_silhouettes_spot.pst", "coop_strobe_fast.pst", "coop_strobe_slow.pst",
    "coop_sweep.pst",         "coop_verse.pst",         "coop_win.pst",
    "coop_win_bre.pst",       "freeze.pst",
};

// Every LightPreset reachable from root, deduplicated (ObjDirItr(d, true)
// already descends into subdirectories).
std::vector<LightPreset *> Presets(ObjectDir *root) {
    std::vector<LightPreset *> out;
    std::set<Hmx::Object *> seen;
    for (ObjDirItr<Hmx::Object> it(root, true); it; ++it) {
        LightPreset *p = dynamic_cast<LightPreset *>(&*it);
        if (p && seen.insert(p).second)
            out.push_back(p);
    }
    return out;
}

void LightPresetCheck(ObjectDir *root) {
    std::vector<LightPreset *> ps = Presets(root);
    std::set<std::string> want(kSc15Presets, kSc15Presets + 29), got;
    int withKeys = 0, keys = 0;
    for (LightPreset *p : ps) {
        got.insert(p->Name());
        if (!p->mKeyframes.empty())
            withKeys++;
        keys += p->mKeyframes.size();
    }
    int missing = 0;
    for (const std::string &s : want)
        if (!got.count(s))
            missing++;
    Gate("uj-sc15-lightpresets", ps.size() == 29 && missing == 0 && withKeys == 29,
         "small_club_15: %zu LightPresets loaded (file table: 29), %d of the 29 names "
         "missing, %d carry keyframes (%d keyframes in all)",
         ps.size(), missing, withKeys, keys);
}

// A fault inside a load or an unload kills the process before any gate can
// report it. Armed only around those two steps: name the venue in a FAIL line,
// then
// re-raise so the run still ends on the signal (native_health counts it as a
// crash). write(2) only; the process is already lost.
char gUnloading[160];
const char *gStage = "unload";
void OnUnloadFault(int sig) {
    char line[256];
    int len = snprintf(line, sizeof(line),
                       "  [FAIL] uj-venue-%s %s — signal %d during the %s\n",
                       gStage, gUnloading, sig, gStage);
    if (len > 0)
        (void)!write(1, line, len < (int)sizeof(line) ? len : (int)sizeof(line) - 1);
    signal(sig, SIG_DFL);
    raise(sig);
}

int VenueChecks() {
    NativeStreamAuditBegin();
    const int n = sizeof(kVenues) / sizeof(kVenues[0]);
    int clean = 0, released = 0;
    int freed = 0, dirsCreated = 0, dirsLeft = 0; // W16-UL
    for (int i = 0; i < n; i++) {
        const char *path = kVenues[i];
        const char *base = strrchr(path, '/') + 1;
        int arkNum = 0, fileSize = 0, ucSize = 0;
        unsigned long long off = 0;
        bool inArk =
            TheArchive && TheArchive->GetFileInfo(FileMakePath(".", path), arkNum, off, fileSize, ucSize);
        int r0 = gNativeFailedStreamReads, o0 = NativeStreamAuditObjects();
        int a0 = NativeStreamAuditAnomalies(), m0 = NativeStreamAuditMissSkips();
        auto t0 = std::chrono::steady_clock::now();
        const int liveDirs0 = gNativeLiveObjectDirs; // W16-UL
        ObjDirPtr<ObjectDir> dir;
        snprintf(gUnloading, sizeof(gUnloading), "%s", base);
        gStage = "load";
        void (*oldSegv)(int) = signal(SIGSEGV, OnUnloadFault);
        void (*oldBus)(int) = signal(SIGBUS, OnUnloadFault);
        if (inArk)
            dir.LoadFile(FilePath(path), false, false, kLoadFront, false);
        signal(SIGSEGV, oldSegv);
        signal(SIGBUS, oldBus);
        double ms = std::chrono::duration<double, std::milli>(
                        std::chrono::steady_clock::now() - t0)
                        .count();
        int reads = gNativeFailedStreamReads - r0, objs = NativeStreamAuditObjects() - o0;
        int anomalies = NativeStreamAuditAnomalies() - a0;
        int misses = NativeStreamAuditMissSkips() - m0;
        ObjectDir *root = dir;
        bool ok = inArk && root && reads == 0 && anomalies == 0 && objs > 0;
        char name[96];
        snprintf(name, sizeof(name), "uj-venue %s", base);
        Gate(name, ok,
             "%s: %s, root %s [%s], %d objects audited, %d off their marker, %d factory "
             "misses skipped, %d reads on a failed stream, %.0f ms",
             path, inArk ? "in the archive" : "NOT in the archive", root ? root->Name() : "(null)",
             root ? root->ClassName().Str() : "-", objs, anomalies, misses, reads, ms);
        if (ok)
            clean++;
        if (root && !strcmp(base, "small_club_15.milo_xbox"))
            LightPresetCheck(root);
        // The unload. Before W16-UJ every arena and festival died here
        // (SIGSEGV in ~ObjectDir's seed restore); a crash ends the run, which
        // native_health reports as runtime_crashed.
        const int created = gNativeLiveObjectDirs - liveDirs0; // W16-UL
        gStage = "unload";
        oldSegv = signal(SIGSEGV, OnUnloadFault);
        oldBus = signal(SIGBUS, OnUnloadFault);
        dir = nullptr;
        signal(SIGSEGV, oldSegv);
        signal(SIGBUS, oldBus);
        released++;
        // W16-UL: the unload destroys every ObjectDir the load created. Before
        // W16-UL the native ~ObjectDir cascade nulled the parent's mSubDirs
        // ObjDirPtr instead of releasing it, so each venue's inlined subdir
        // tree (16.1 MB for small_club_01) outlived the unload.
        {
            const int left = gNativeLiveObjectDirs - liveDirs0;
            const bool f = created > 0 && left == 0;
            if (f)
                freed++;
            dirsCreated += created;
            dirsLeft += left;
            char fname[96];
            snprintf(fname, sizeof(fname), "ul-venue-freed %s", base);
            Gate(fname, f,
                 "%d ObjectDir(s) alive after the load, %d of them still alive after the "
                 "unload",
                 created, left);
        }
        fflush(stdout);
    }
    Gate("uj-venues", clean == n && released == n,
         "%d of %d shipped venue milos loaded cleanly, %d unloaded", clean, n, released);
    Gate("ul-venues-freed", freed == n,
         "%d of %d venue unloads destroyed every ObjectDir their load created (%d created, "
         "%d left alive)",
         freed, n, dirsCreated, dirsLeft);
    return clean;
}

} // namespace

int RunW16UJPhase(GateFn gate) {
    gGate = gate;
    printf("\n=== W16-UJ phase: shipped venue loads, fsel NaN in Min/Max/Clamp ===\n");
    ClampChecks();
    VenueChecks();
    return gRan;
}
