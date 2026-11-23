// rb3-app chart player (lane chartplay, 2026-11-23). Native only, rb3-app only.
//
// RB3_APP_CHARTPLAY=1 plays pad 0's gem part from the loaded chart through real
// pad input: each poll it finds the local GemPlayer on pad 0, reads the gem list
// its TrackWatcher scores against (TrackWatcherImpl::mGemList, the difficulty
// the player picked) and the BeatMatcher's clock (BeatMatcher::GetNow, the song
// ms every Swing / FretButtonDown is judged at), and returns the button levels a
// player would hold this poll. rb3_app_joypad.cpp ORs them into pad 0's state
// before JoypadPollCommon, so everything after the press -- ButtonDownMsg, the
// controller (GuitarController / JoypadController), BeatMatcher, the watcher's
// hit windows, scoring, streak, crowd -- is the game's own code. Unlike
// RB3_APP_AUTOPLAY, nothing in the game is told to hit anything.
//
// Guitar / bass (GuitarController): the gem's frets go down one poll before its
// strum, and the strum (the controller's own strum buttons, alternating so two
// strums on consecutive polls are two presses) lands on the poll whose clock is
// nearest the gem's time. Frets stay down through a sustain and up to the next
// gem's fret change. Drums (JoypadController): each slot of the gem is pressed
// on that poll and released on the next. Buttons come from the controller's own
// SlotToButton, so the joypad.dta mapping is the game's.
//
// Knobs (all optional, read once):
//   RB3_APP_CHARTPLAY_OFFSET_MS=f   press f ms after each gem (negative: early)
//   RB3_APP_CHARTPLAY_MISS=p        skip p% of gems (no press at all), chosen by
//                                   a hash of the gem index and _SEED; the game's
//                                   RNG is not touched
//   RB3_APP_CHARTPLAY_MISS_FROM=n   only gems with index >= n may be skipped
//   RB3_APP_CHARTPLAY_SEED=n        the skip hash's seed (default 0)
//   RB3_APP_CHARTPLAY_HOPO=tap      a HOPO gem (GameGem no-strum bit, which
//                                   CanHopo reads) is played by the fret change
//                                   alone at its time, no strum (default: strum)
//   RB3_APP_CHARTPLAY_SUSTAIN=f     hold a sustain for fraction f of its length
//                                   (default 1); frets go up after that
//   RB3_APP_CHARTPLAY_LOG=1         one line per gem (`rb3-app-chartplay: gem ...`)
// Every song also prints a summary line when its gem list goes away or is
// replaced. Record: docs/decomp/CHARTPLAY_CHART_DRIVEN_PAD_WALK_2026-11-23.md.
#include "beatmatch/BeatMatcher.h"
#include "beatmatch/GameGemList.h"
#include "beatmatch/GuitarController.h"
#include "beatmatch/JoypadController.h"
#include "beatmatch/TrackWatcher.h"
#include "beatmatch/TrackWatcherImpl.h"
#include "game/BandUser.h"
#include "game/Game.h"
#include "game/GemPlayer.h"
#include "os/User.h"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
struct Knobs {
    bool on = false;
    float offset = 0;
    int missPct = 0;
    int missFrom = 0;
    unsigned seed = 0;
    bool tapHopo = false;
    float sustain = 1;
    bool log = false;
};

bool EnvOn(const char *n) {
    const char *v = getenv(n);
    return v && *v && strcmp(v, "0") != 0;
}

const Knobs &K() {
    static const Knobs k = [] {
        Knobs k;
        k.on = EnvOn("RB3_APP_CHARTPLAY");
        if (const char *v = getenv("RB3_APP_CHARTPLAY_OFFSET_MS")) k.offset = (float)atof(v);
        if (const char *v = getenv("RB3_APP_CHARTPLAY_MISS")) k.missPct = atoi(v);
        if (const char *v = getenv("RB3_APP_CHARTPLAY_MISS_FROM")) k.missFrom = atoi(v);
        if (const char *v = getenv("RB3_APP_CHARTPLAY_SEED")) k.seed = (unsigned)atoi(v);
        if (const char *v = getenv("RB3_APP_CHARTPLAY_HOPO")) k.tapHopo = !strcmp(v, "tap");
        if (const char *v = getenv("RB3_APP_CHARTPLAY_SUSTAIN")) k.sustain = (float)atof(v);
        k.log = EnvOn("RB3_APP_CHARTPLAY_LOG");
        if (k.on) {
            printf("rb3-app-chartplay: on (pad 0) offset %.1f ms miss %d%% from gem %d seed %u "
                   "hopo %s sustain %.2f\n",
                   k.offset, k.missPct, k.missFrom, k.seed, k.tapHopo ? "tap" : "strum", k.sustain);
            fflush(stdout);
        }
        return k;
    }();
    return k;
}

struct Song {
    const GameGemList *list = nullptr;
    int next = 0; // first gem not yet acted on
    unsigned frets = 0; // guitar: fret slots held
    float fretsUpAt = -1; // guitar: song ms the held frets go up (sustain end), -1 never
    unsigned prevButtons = 0; // last poll's levels
    int strumAlt = 0;
    float prevNow = 0;
    int acted = 0, skipped = 0, late = 0, collide = 0;
    float maxLate = 0;
};
Song gSong;

bool Skip(int gem) {
    const Knobs &k = K();
    if (k.missPct <= 0 || gem < k.missFrom) return false;
    uint32_t h = 2166136261u ^ k.seed;
    for (int i = 0; i < 4; i++) {
        h ^= (uint32_t)(gem >> (8 * i)) & 0xff;
        h *= 16777619u;
    }
    return (int)(h % 100) < k.missPct;
}

void Summary(const char *why) {
    if (!gSong.list) return;
    printf("rb3-app-chartplay: song %s: gems %d acted %d skipped %d late %d (max %.1f ms) "
           "same-pad collisions %d\n",
           why, gSong.list->NumGems(), gSong.acted, gSong.skipped, gSong.late, gSong.maxLate,
           gSong.collide);
    fflush(stdout);
}

BeatMatcher *PadMatcher(int pad) {
    if (!TheGame) return nullptr;
    std::vector<Player *> &players = TheGame->GetActivePlayers();
    for (size_t i = 0; i < players.size(); i++) {
        GemPlayer *gp = dynamic_cast<GemPlayer *>(players[i]);
        if (!gp || !gp->IsLocal()) continue;
        BandUser *u = gp->GetUser();
        if (!u || !u->GetLocalUser() || u->GetLocalUser()->GetPadNum() != pad) continue;
        return gp->mMatcher;
    }
    return nullptr;
}

unsigned SlotsToButtons(const BeatMatchController *c, unsigned slots) {
    unsigned b = 0;
    for (int s = 0; s < 32; s++)
        if (slots & (1u << s)) {
            int btn = c->SlotToButton(s);
            if (btn >= 0 && btn < 32) b |= 1u << btn;
        }
    return b;
}
} // namespace

bool RB3AppChartPlayOn() { return K().on; }

// The button levels pad `pad` holds this poll. Pad 0 only.
unsigned int RB3AppChartPlayButtons(int pad, int poll) {
    const Knobs &k = K();
    if (!k.on || pad != 0) return 0;
    BeatMatcher *m = PadMatcher(pad);
    const GameGemList *gl = nullptr;
    if (m && m->mWatcher && m->mWatcher->mImpl && m->mController)
        gl = m->mWatcher->mImpl->mGemList;
    float now = gl ? m->GetNow() : 0;
    if (gl != gSong.list || (gl && now + 1000 < gSong.prevNow)) {
        Summary(gl ? "replaced" : "ended");
        gSong = Song();
        gSong.list = gl;
        if (!gl) return 0;
        while (gSong.next < gl->NumGems() && gl->GetGem(gSong.next).mMs + k.offset < now)
            gSong.next++;
        printf("rb3-app-chartplay: poll %d gem list %p gems %d track %d now %.1f starting at gem %d\n",
               poll, (const void *)gl, gl->NumGems(), m->mWatcher->mImpl->mTrack, now, gSong.next);
        fflush(stdout);
    }
    if (!gl) return 0;
    float dt = now - gSong.prevNow;
    if (dt <= 0 || dt > 100) dt = 1000.0f / 30;
    gSong.prevNow = now;
    BeatMatchController *c = m->mController;
    GuitarController *gc = dynamic_cast<GuitarController *>(c);
    JoypadController *jc = gc ? nullptr : dynamic_cast<JoypadController *>(c);
    if (!gc && !jc) return 0; // keys / pro guitar: not driven

    unsigned buttons = 0;
    int n = gl->NumGems();
    if (gc) {
        // Frets: up at a sustain's end; the next acted gem's frets go down one
        // poll before its time (a tapped HOPO's at its time, with no strum).
        if (gSong.fretsUpAt >= 0 && now >= gSong.fretsUpAt) {
            gSong.frets = 0;
            gSong.fretsUpAt = -1;
        }
        int g = gSong.next;
        if (g < n) {
            const GameGem &gem = gl->GetGem(g);
            float t = gem.mMs + k.offset;
            bool tap = k.tapHopo && gem.GetForceStrum();
            bool due = now >= t - dt / 2;
            if (!Skip(g) && !tap && now >= t - dt * 1.5f) {
                gSong.frets = gem.mSlots;
                gSong.fretsUpAt = -1;
            }
            if (due) {
                float lateBy = now - t;
                if (lateBy > dt / 2) {
                    gSong.late++;
                    if (lateBy > gSong.maxLate) gSong.maxLate = lateBy;
                }
                const char *what;
                if (Skip(g)) {
                    gSong.skipped++;
                    gSong.frets = 0;
                    what = "skip";
                } else {
                    gSong.acted++;
                    gSong.frets = gem.mSlots;
                    float dur = gem.mIgnoreDuration ? 0 : (float)gem.mDurationMs;
                    gSong.fretsUpAt = dur > 0 && k.sustain < 1 ? t + dur * k.sustain : -1;
                    if (tap) {
                        what = "tap";
                    } else {
                        const std::vector<int> &sb = gc->mStrumBarButtons;
                        int sbtn = sb.empty() ? kPad_DDown : sb[gSong.strumAlt % sb.size()];
                        gSong.strumAlt++;
                        buttons |= 1u << sbtn;
                        what = "strum";
                    }
                }
                if (k.log)
                    printf("rb3-app-chartplay: poll %d gem %d ms %.1f dur %d slots 0x%x hopo %d "
                           "now %.1f (%+.1f) %s\n",
                           poll, g, gem.mMs, (int)gem.mDurationMs, gem.mSlots,
                           (int)gem.GetForceStrum(), now, now - gem.mMs, what);
                gSong.next++;
            }
        }
        buttons |= SlotsToButtons(c, gSong.frets);
    } else {
        // Drums: every gem due this poll (a pad held last poll waits one poll).
        while (gSong.next < n) {
            int g = gSong.next;
            const GameGem &gem = gl->GetGem(g);
            float t = gem.mMs + k.offset;
            if (now < t - dt / 2) break;
            const char *what;
            if (Skip(g)) {
                gSong.skipped++;
                what = "skip";
            } else {
                unsigned b = SlotsToButtons(c, gem.mSlots);
                if (b & (gSong.prevButtons | buttons)) {
                    gSong.collide++;
                    break; // the pad is still down: hit it next poll
                }
                buttons |= b;
                gSong.acted++;
                what = "hit";
            }
            float lateBy = now - t;
            if (lateBy > dt / 2) {
                gSong.late++;
                if (lateBy > gSong.maxLate) gSong.maxLate = lateBy;
            }
            if (k.log)
                printf("rb3-app-chartplay: poll %d gem %d ms %.1f slots 0x%x cymbal %d now %.1f "
                       "(%+.1f) %s\n",
                       poll, g, gem.mMs, gem.mSlots, (int)gem.IsCymbal(), now, now - gem.mMs, what);
            gSong.next++;
        }
    }
    gSong.prevButtons = buttons;
    return buttons;
}
