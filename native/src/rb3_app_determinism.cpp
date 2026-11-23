// rb3-app deterministic walk (lane detwalk, 2026-11-23). Native only, opt-in.
//
// RB3_APP_DETERMINISTIC=1 makes two runs of one rb3-app binary on one input
// script reach the same state at every frame, so a walk can A/B two builds
// frame by frame. Unset (the default), every hook below passes straight
// through to the real call and the run is what it was before this file.
//
// Three inputs are fixed; nothing in src/ is changed:
//   1. The RNG seed. Retail SystemInit seeds the game RNG from the clock
//      (`SeedRand(time.wMilliseconds)`, os/System.cpp; SeedRand is Rand.cpp's
//      Rand::sRand.Seed). -Wl,--wrap=_Z8SeedRandi routes that one call here,
//      which seeds with RB3_APP_SEED (default 0, a value wMilliseconds can
//      take) instead. Every later draw (shell music loop, FileMergerOrganizer,
//      song_select's pick) is the game's own Rand::sRand path, unchanged.
//   2. The clock. Every native Timer reads __mftb() (os/Timer.h), which is
//      std::chrono::steady_clock::now(); so does the game's song clock
//      (Game::Poll's mTime in realtime mode), LoadMgr::CheckSplit's per-frame
//      load budget and UIManager's timer. -Wl,--wrap of steady_clock::now
//      routes every call in the link here. On the main thread it returns a
//      virtual clock: frame N starts at kBaseNs + N * kFrameNs (1/30 s, the
//      step headless TaskMgr::Poll and UIManager::Poll already use), and each
//      call advances it by kTickNs (1 us, __mftb's resolution) so a loop that
//      waits on a timer still ends. A frame that consumes more than kFrameNs
//      of ticks pushes the next frame's start later (the clock never goes
//      backwards). Other threads keep the real clock: their timed waits
//      (condition_variable deadlines) must stay real.
//   3. ThreadCall's worker. native/src/platform/ThreadCall_Native.cpp runs
//      each queued job (DTA loads and the like) on a pthread and delivers its
//      done callback at the first ThreadCallPoll after it finishes, so the
//      frame a job lands on depends on scheduling. With the flag set, the
//      --wrap'ed ThreadCall/ThreadCallPoll keep their own 12-slot queue (the
//      real one's size) and run the job on the main thread: a poll first
//      delivers the done callback of the job started at the previous poll,
//      then starts the next queued job. That is the threaded path's shortest
//      latency (start released at poll k, done seen at poll k+1), made fixed.
//      The real worker is still created and simply never released.
//
// Per frame, at the SystemPoll wrapper (main_app.cpp), it prints
//   rb3-app-det: frame N state XXXXXXXX chain XXXXXXXXXXXXXXXX rand A/B seconds S beat B ui U screen S1 transition S2
// where state hashes the RNG object's bytes, the three TaskMgr clocks and the
// screen names, and chain folds every earlier frame's state in. Two runs are
// identical iff their rb3-app-det lines are (wall-clock lines such as
// `world_poll took` and the `(%.2f s)` suffixes are not part of the record).
// Record: docs/decomp/DETWALK_DETERMINISTIC_RB3APP_WALK_2026-11-23.md.
//
// Scoring walk (lane scorewalk, 2026-11-23). RB3_APP_AUTOPLAY=1 turns on the
// game's own autoplay for every local band user: each frame, before the poll,
// it sets BandUser::mAutoplay (the `autoplay` sync property). Retail
// GemPlayer::Start (0x826BC460) reads that byte (`lbz r4,0x7c(mUser)`) and
// passes it to SetAutoplay for a local player, so the beatmatcher hits the
// chart and the real scoring path (score, streak, multiplier, overdrive
// phrases, stars, crowd) runs. Nothing else is forced. With the walk flag
// also set, every frame inside a Game appends to its rb3-app-det line
//   band acc A stars T mult M score .. p<i> track T en E auto <0|1> energy E deploy D
//   long L score S hit H miss M streak K stars F crowd C ...
// and folds the same values into `state`; outside a Game the line is as before.
// Record: docs/decomp/SCOREWALK_DETERMINISTIC_SCORING_WALK_2026-11-23.md.
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <pthread.h>
#include "math/Rand.h"
#include "os/ThreadCall.h"
#include "obj/Task.h"
#include "game/Band.h"
#include "game/BandUser.h"
#include "game/BandUserMgr.h"
#include "game/Game.h"
#include "game/Player.h"
#include "game/Scoring.h"
#include "game/VocalPlayer.h"
#include "ui/UI.h"
#include <string>
#include "ui/UIScreen.h"

namespace {
const int64_t kBaseNs = 1000000000000LL; // 1000 s: __mftb's 32-bit us counter stays far from wrap
const int64_t kFrameNs = 33333333; // 1/30 s
const int64_t kTickNs = 1000; // 1 us per main-thread clock read

bool gDet = false;
bool gAutoplay = false;
pthread_t gMainThread;
int64_t gVirtNs = kBaseNs; // main thread only
uint64_t gChain = 0xcbf29ce484222325ULL;

uint64_t Fnv(uint64_t h, const void *p, size_t n) {
    const unsigned char *b = (const unsigned char *)p;
    for (size_t i = 0; i < n; i++) {
        h ^= b[i];
        h *= 0x100000001b3ULL;
    }
    return h;
}

const char *Name(const UIScreen *s) {
    if (!s) return "(none)";
    const char *n = s->Name();
    return n && *n ? n : "(unnamed)";
}

// RB3_APP_AUTOPLAY: set the retail autoplay byte on every local band user.
void ArmAutoplay() {
    if (!gAutoplay || !TheBandUserMgr) return;
    std::vector<BandUser *> &users = TheBandUserMgr->GetBandUsers();
    for (size_t i = 0; i < users.size(); i++) {
        BandUser *u = users[i];
        if (u && u->IsLocal() && !u->mAutoplay) u->SetAutoplay(true);
    }
}

void AddPerf(uint64_t &h, std::string &out, const Performer *p, bool withStars) {
    const Stats &st = p->GetStats();
    int score = (int)p->mScore;
    int hit = st.GetHitCount(), miss = st.GetMissCount();
    int streak = st.GetCurrentStreak();
    float crowd = p->Crowd() ? p->Crowd()->GetValue() : -1.0f;
    // Scoring::GetNumStarsFloat asserts its thresholds are non-empty before it
    // returns 0 for a zero score, and during the song load they are still empty:
    // ask only once there is a score, so the record never trips the assert.
    float stars = !withStars || !TheScoring ? -1.0f : score ? p->GetNumStarsFloat() : 0.0f;
    h = Fnv(h, &score, sizeof score);
    h = Fnv(h, &hit, sizeof hit);
    h = Fnv(h, &miss, sizeof miss);
    h = Fnv(h, &streak, sizeof streak);
    h = Fnv(h, &crowd, sizeof crowd);
    h = Fnv(h, &stars, sizeof stars);
    char buf[160];
    snprintf(buf, sizeof buf, " score %d hit %d miss %d streak %d stars %.4f crowd %.4f", score, hit,
             miss, streak, stars, crowd);
    out += buf;
}

// Band and per-player scoring state while a Game exists ("" otherwise).
std::string ScoreState(uint64_t &h) {
    std::string out;
    if (!TheGame) return out;
    Band *band = TheGame->GetBand();
    if (band) {
        int acc = band->AccumulatedScore();
        float total = band->GetTotalStars();
        int mult = band->EnergyMultiplier();
        bool multOn = band->IsMultiplierActive();
        h = Fnv(h, &acc, sizeof acc);
        h = Fnv(h, &total, sizeof total);
        h = Fnv(h, &mult, sizeof mult);
        h = Fnv(h, &multOn, sizeof multOn);
        char buf[96];
        snprintf(buf, sizeof buf, " band acc %d stars %.4f mult %d%s", acc, total, mult,
                 multOn ? "" : "(off)");
        out += buf;
        if (Performer *bp = band->GetBand()) AddPerf(h, out, bp, true);
    }
    std::vector<Player *> &players = TheGame->GetActivePlayers();
    for (size_t i = 0; i < players.size(); i++) {
        Player *p = players[i];
        if (!p) continue;
        int track = (int)p->GetTrackType();
        bool autoOn = p->IsAutoplay();
        float energy = p->GetBandEnergy();
        bool deploy = p->IsDeployingBandEnergy();
        int longest = p->GetStats().GetLongestStreak();
        int enabled = (int)p->GetEnabledState();
        h = Fnv(h, &enabled, sizeof enabled);
        h = Fnv(h, &track, sizeof track);
        h = Fnv(h, &autoOn, sizeof autoOn);
        h = Fnv(h, &energy, sizeof energy);
        h = Fnv(h, &deploy, sizeof deploy);
        h = Fnv(h, &longest, sizeof longest);
        char buf[128];
        snprintf(buf, sizeof buf, " p%d track %d en %d auto %d energy %.4f deploy %d long %d",
                 (int)i, track, enabled, autoOn, energy, deploy, longest);
        out += buf;
        if (VocalPlayer *vp = dynamic_cast<VocalPlayer *>(p)) {
            // Vocal-only: whether Poll has a track to run (VocalPlayer::Poll returns
            // early without one), and the singer / part counts it scores over.
            int vt = vp->mTrack != nullptr, ns = (int)vp->mSingers.size(),
                np = (int)vp->mVocalParts.size(), se = vp->mScoringEnabled;
            h = Fnv(h, &vt, sizeof vt);
            h = Fnv(h, &ns, sizeof ns);
            h = Fnv(h, &np, sizeof np);
            h = Fnv(h, &se, sizeof se);
            snprintf(buf, sizeof buf, " vtrack %d singers %d parts %d scoring %d", vt, ns, np, se);
            out += buf;
        }
        AddPerf(h, out, p, true);
    }
    return out;
}
} // namespace

// Called first thing in main(), before App::App (SystemInit seeds the RNG).
void rb3_app_determinism_init() { // ring: native-real
    const char *v = getenv("RB3_APP_DETERMINISTIC");
    gDet = v && *v && strcmp(v, "0") != 0;
    const char *a = getenv("RB3_APP_AUTOPLAY");
    gAutoplay = a && *a && strcmp(a, "0") != 0;
    if (gAutoplay) {
        printf("rb3-app-det: autoplay on (BandUser::mAutoplay for every local user)\n");
        fflush(stdout);
    }
    if (!gDet) return;
    gMainThread = pthread_self();
    printf("rb3-app-det: deterministic walk on (seed %d, frame %lld ns, tick %lld ns)\n",
           atoi(getenv("RB3_APP_SEED") ? getenv("RB3_APP_SEED") : "0"), (long long)kFrameNs,
           (long long)kTickNs);
    fflush(stdout);
}

// Called by the SystemPoll wrapper at the start of frame `frame` (1-based).
void rb3_app_determinism_frame(int frame) { // ring: native-real
    ArmAutoplay();
    if (!gDet) return;
    int64_t start = kBaseNs + (int64_t)frame * kFrameNs;
    if (gVirtNs < start) gVirtNs = start;
    UIScreen *cur = TheUI ? TheUI->CurrentScreen() : nullptr;
    bool tr = TheUI && TheUI->InTransition();
    const char *curName = Name(cur);
    const char *trName = tr ? Name(TheUI->TransitionScreen()) : "none";
    float secs = TheTaskMgr.Seconds(TaskMgr::kRealTime);
    float beat = TheTaskMgr.Beat();
    float ui = TheTaskMgr.UISeconds();
    uint64_t h = 0xcbf29ce484222325ULL;
    h = Fnv(h, &frame, sizeof frame);
    h = Fnv(h, &Rand::sRand, sizeof(Rand));
    h = Fnv(h, &secs, sizeof secs);
    h = Fnv(h, &beat, sizeof beat);
    h = Fnv(h, &ui, sizeof ui);
    h = Fnv(h, curName, strlen(curName));
    h = Fnv(h, trName, strlen(trName));
    std::string game = ScoreState(h);
    gChain = Fnv(gChain, &h, sizeof h);
    unsigned int idx[2];
    memcpy(idx, &Rand::sRand, sizeof idx); // mRandIndex1, mRandIndex2 (Rand.h)
    printf("rb3-app-det: frame %d state %08x chain %016llx rand %u/%u seconds %.4f beat %.4f "
           "ui %.4f screen %s transition %s%s\n",
           frame, (unsigned)(h ^ (h >> 32)), (unsigned long long)gChain, idx[0], idx[1], secs,
           beat, ui, curName, trName, game.c_str());
    fflush(stdout);
}

extern "C" void __real__Z8SeedRandi(int);
extern "C" void __wrap__Z8SeedRandi(int seed) { // ring: native-real
    if (gDet) {
        const char *s = getenv("RB3_APP_SEED");
        int fixed = s && *s ? atoi(s) : 0;
        printf("rb3-app-det: SeedRand(%d) -> SeedRand(%d)\n", seed, fixed);
        fflush(stdout);
        seed = fixed;
    }
    __real__Z8SeedRandi(seed);
}

// std::chrono::steady_clock::now() (libstdc++ _V2). time_point<steady_clock,
// nanoseconds> is one int64 returned in a register, so int64_t is its ABI.
extern "C" int64_t __real__ZNSt6chrono3_V212steady_clock3nowEv();
extern "C" int64_t __wrap__ZNSt6chrono3_V212steady_clock3nowEv() { // ring: native-real
    if (!gDet || !pthread_equal(pthread_self(), gMainThread))
        return __real__ZNSt6chrono3_V212steady_clock3nowEv();
    gVirtNs += kTickNs;
    return gVirtNs;
}

// ThreadCall (os/ThreadCall.h). Deterministic mode only; otherwise the real
// queue and worker (ThreadCall_Native.cpp) run as before.
namespace {
ThreadCallData gJobs[12];
int gJobHead = 0; // next job to start
int gJobTail = 0; // next free slot
bool gJobStarted = false; // gJobs[gJobHead] ran ThreadStart / mFunc at the previous poll
int gJobResult = 0;
} // namespace

extern "C" void __real__Z10ThreadCallPFivEPFviE(ThreadCallFunc *, ThreadCallCallbackFunc *);
extern "C" void __wrap__Z10ThreadCallPFivEPFviE(ThreadCallFunc *f, ThreadCallCallbackFunc *cb) { // ring: native-real
    if (!gDet) return __real__Z10ThreadCallPFivEPFviE(f, cb);
    ThreadCallData &d = gJobs[gJobTail];
    if (d.mType != kTCDT_None) {
        printf("rb3-app-det: ThreadCall queue full\n");
        fflush(stdout);
        abort();
    }
    d.mType = kTCDT_Func;
    d.mFunc = f;
    d.mCallback = cb;
    d.mClass = nullptr;
    gJobTail = (gJobTail + 1) % 12;
}

extern "C" void __real__Z10ThreadCallP14ThreadCallback(ThreadCallback *);
extern "C" void __wrap__Z10ThreadCallP14ThreadCallback(ThreadCallback *c) { // ring: native-real
    if (!gDet) return __real__Z10ThreadCallP14ThreadCallback(c);
    ThreadCallData &d = gJobs[gJobTail];
    if (d.mType != kTCDT_None) {
        printf("rb3-app-det: ThreadCall queue full\n");
        fflush(stdout);
        abort();
    }
    d.mType = kTCDT_Class;
    d.mFunc = nullptr;
    d.mCallback = nullptr;
    d.mClass = c;
    gJobTail = (gJobTail + 1) % 12;
}

extern "C" void __real__Z14ThreadCallPollv();
extern "C" void __wrap__Z14ThreadCallPollv() { // ring: native-real
    if (!gDet) return __real__Z14ThreadCallPollv();
    if (gJobStarted) {
        ThreadCallData d = gJobs[gJobHead];
        gJobs[gJobHead].mType = kTCDT_None;
        gJobHead = (gJobHead + 1) % 12;
        gJobStarted = false;
        if (d.mType == kTCDT_Func) d.mCallback(gJobResult);
        else d.mClass->ThreadDone(gJobResult);
    }
    if (!gJobStarted && gJobs[gJobHead].mType != kTCDT_None) {
        ThreadCallData &d = gJobs[gJobHead];
        gJobResult = d.mType == kTCDT_Func ? d.mFunc() : d.mClass->ThreadStart();
        gJobStarted = true;
    }
}
