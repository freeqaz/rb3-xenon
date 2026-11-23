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
//   RB3_APP_CHARTPLAY_MISS_TO=n     (lane dropwalk) and only gems with index < n,
//                                   so _MISS=100 sits out the window [from, to)
//   RB3_APP_CHARTPLAY_SEED=n        the skip hash's seed (default 0)
//   RB3_APP_CHARTPLAY_HOPO=tap      a HOPO gem (GameGem no-strum bit, which
//                                   CanHopo reads) is played by the fret change
//                                   alone at its time, no strum (default: strum)
//   RB3_APP_CHARTPLAY_SUSTAIN=f     hold a sustain for fraction f of its length
//                                   (default 1); frets go up after that
// Keys, pro keys and pro guitar (lane chartplay2, record
// docs/decomp/CHARTPLAY2_KEYS_PROKEYS_PROGUITAR_2026-11-23.md) are played through
// the pad's raw 16-byte peripheral report instead of buttons: see KeysPoll and
// RGPoll. The native ReadSingleJoypad copies it (RB3AppChartPlayRaw); from
// UsbMidiKeyboard::Poll / UsbMidiGuitar::Poll on it is the retail path.
//
// Lane chartplay3 (record docs/decomp/CHARTPLAY3_PRO_STRINGS_PEDAL_2026-11-23.md):
//   RB3_APP_CHARTPLAY_MUTE=open|keep|openall  pro guitar/bass: a kRGMuted string
//                                   goes down open / keeps its last fret; `openall`
//                                   opens every string of a gem that has one
//   RB3_APP_CHARTPLAY_SLIDE=on|all  move a LeftHandSlide gem's fretted strings 2
//                                   frets (down for ReverseSlide) half way through
//                                   its sustain; `all` (a control) every sustain
//   RB3_APP_CHARTPLAY_BEND=1        pitch-bend byte 100 through a kRGBend gem
//   RB3_APP_CHARTPLAY_DEPLOY=pedal|stomp|button  pro keys: deploy overdrive with
//                                   the sustain pedal, the stomp pedal or the
//                                   controller's OD button (mForceMercuryBut)
//   RB3_APP_CHARTPLAY_HANDS=1       pro keys: low/high hand-placement bytes from
//                                   the held keys
// Lane codawalk (record docs/decomp/CODAWALK_INSONG_BONUS_WALK_2026-11-23.md), guitar/bass:
//   RB3_APP_CHARTPLAY_WHAMMY=od|all  move the whammy (XInput sThumbRX, which the
//                                   joypad.dta whammy entry GetWhammyBar reads) full
//                                   travel every other poll while a sustain is held:
//                                   of a gem in an overdrive phrase (od) or any (all)
//   RB3_APP_CHARTPLAY_TILT=1        deploy overdrive by tilt (sThumbRY full positive,
//                                   the xbox_mercury_switch axis) for 15 polls (TU5
//                                   beatmatch.dta any_sign_filter: 12 frames >= 0.93) whenever
//                                   the player has half a bar and is not deploying
//   RB3_APP_CHARTPLAY_TILT_MS=N     (lane framerate) hold each tilt for N ms of song time
//                                   instead of 15 polls, at most one tilt per 1000 ms, so
//                                   one hold is the same input at any frame rate
//   RB3_APP_CHARTPLAY_CODA=mash|blow  big rock ending: `mash` skips the chart's gems
//                                   inside the coda freestyle (GetCodaFreestyleExtents)
//                                   and instead holds all five frets and strums every
//                                   third poll (drums: one pad a poll in rotation);
//                                   `blow` plays the freestyle as charted and skips
//                                   every gem after it (the ending's last gems)
//   RB3_APP_CHARTPLAY_LOG=1         one line per gem (`rb3-app-chartplay: gem ...`) and
//                                   per pressing poll (what JoypadData made of it);
//                                   2 also turns on TheBeatMatchOutput (see K())
// Every song also prints a summary line when its gem list goes away or is
// replaced. Record: docs/decomp/CHARTPLAY_CHART_DRIVEN_PAD_WALK_2026-11-23.md.
#include "beatmatch/BeatMatcher.h"
#include "beatmatch/GameGemList.h"
#include "beatmatch/GuitarController.h"
#include "beatmatch/JoypadController.h"
#include "beatmatch/JoypadMidiController.h"
#include "beatmatch/KeyboardController.h"
#include "beatmatch/RealGuitarController.h"
#include "beatmatch/TrackWatcher.h"
#include "beatmatch/TrackWatcherImpl.h"
#include "game/BandUser.h"
#include "game/Game.h"
#include "game/GemPlayer.h"
#include "os/User.h"
#include "beatmatch/Output.h"
#include "os/Joypad.h"
#include "os/UsbMidiKeyboard.h"
#include "beatmatch/GemInfo.h"
#include "beatmatch/SongData.h"
#include "game/SongDB.h"
#include "utl/TimeConversion.h"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

int RB3AppHz(); // rb3_app_determinism.cpp (RB3_APP_HZ; 0 = legacy 30 Hz)

namespace {
// Lane walk60: the driver's own poll counts (one deploy or tilt a second, a tilt held half a
// second, the first poll's dt) were written for 30 polls a second; at RB3_APP_HZ=60 they halved
// in time (two deploys a second). Count them in polls of the run's rate instead; legacy is 30,
// so a legacy run is unchanged.
int PollsPerSecond() {
    static const int pps = RB3AppHz() > 0 ? RB3AppHz() : 30;
    return pps;
}

struct Knobs {
    bool on = false;
    float offset = 0;
    int missPct = 0;
    int missFrom = 0;
    int missTo = 0; // lane dropwalk: 0 = no upper bound
    unsigned seed = 0;
    bool tapHopo = false;
    float sustain = 1;
    int log = 0;
    // lane chartplay3 (record docs/decomp/CHARTPLAY3_PRO_STRINGS_PEDAL_2026-11-23.md)
    int mute = 0;   // pro guitar/bass muted strings: 0 chart fret, 1 open, 2 keep, 3 openall
    int slide = 0;  // 1 slide a left-hand-slide gem's frets mid-sustain, 2 every sustain
    bool bend = false; // pitch-bend byte up through a gem with a bend string
    int deploy = 0; // pro keys overdrive: 1 sustain pedal, 2 stomp pedal, 3 OD button
    bool hands = false; // pro keys low/high hand-placement bytes from the held keys
    // lane codawalk
    int whammy = 0; // 1 sustains of overdrive-phrase gems, 2 every sustain
    bool tilt = false;
    float tiltMs = 0; // lane framerate: hold a tilt this long (song ms) instead of 15 polls
    int coda = 0; // 1 mash the freestyle, 2 blow the ending
};

int PickKnob(const char *n, const char *const *names, int count) {
    const char *v = getenv(n);
    if (!v || !*v) return 0;
    for (int i = 0; i < count; i++)
        if (!strcmp(v, names[i])) return i;
    fprintf(stderr, "rb3-app-chartplay: %s=%s is not a known value\n", n, v);
    fflush(stderr);
    _Exit(2);
}

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
        if (const char *v = getenv("RB3_APP_CHARTPLAY_MISS_TO")) k.missTo = atoi(v);
        if (const char *v = getenv("RB3_APP_CHARTPLAY_SEED")) k.seed = (unsigned)atoi(v);
        if (const char *v = getenv("RB3_APP_CHARTPLAY_HOPO")) k.tapHopo = !strcmp(v, "tap");
        if (const char *v = getenv("RB3_APP_CHARTPLAY_SUSTAIN")) k.sustain = (float)atof(v);
        if (const char *v = getenv("RB3_APP_CHARTPLAY_LOG")) k.log = atoi(v);
        static const char *const kMute[] = {"chart", "open", "keep", "openall"};
        static const char *const kSlide[] = {"off", "on", "all"};
        static const char *const kDeploy[] = {"off", "pedal", "stomp", "button"};
        k.mute = PickKnob("RB3_APP_CHARTPLAY_MUTE", kMute, 4);
        k.slide = PickKnob("RB3_APP_CHARTPLAY_SLIDE", kSlide, 3);
        k.deploy = PickKnob("RB3_APP_CHARTPLAY_DEPLOY", kDeploy, 4);
        k.bend = EnvOn("RB3_APP_CHARTPLAY_BEND");
        k.hands = EnvOn("RB3_APP_CHARTPLAY_HANDS");
        static const char *const kWhammy[] = {"off", "od", "all"};
        static const char *const kCoda[] = {"off", "mash", "blow"};
        k.whammy = PickKnob("RB3_APP_CHARTPLAY_WHAMMY", kWhammy, 3);
        k.coda = PickKnob("RB3_APP_CHARTPLAY_CODA", kCoda, 3);
        k.tilt = EnvOn("RB3_APP_CHARTPLAY_TILT");
        if (const char *v = getenv("RB3_APP_CHARTPLAY_TILT_MS")) k.tiltMs = atof(v);
        // _LOG=2 also switches on the game's own beatmatch trace (TheBeatMatchOutput,
        // the SWING / NOSWING / HIT / MISS lines BeatMatcher and the watchers print),
        // which LogFile writes to its file pattern in the run directory.
        if (k.on && k.log >= 2) TheBeatMatchOutput.SetActive(true);
        if (k.on) {
            printf("rb3-app-chartplay: on (pad 0) offset %.1f ms miss %d%% from gem %d to %d seed %u "
                   "hopo %s sustain %.2f\n",
                   k.offset, k.missPct, k.missFrom, k.missTo, k.seed, k.tapHopo ? "tap" : "strum", k.sustain);
            printf("rb3-app-chartplay: mute %s slide %s bend %d deploy %s hands %d\n",
                   kMute[k.mute], kSlide[k.slide], (int)k.bend, kDeploy[k.deploy], (int)k.hands);
            printf("rb3-app-chartplay: whammy %s tilt %d tilt-ms %.0f coda %s\n",
                   kWhammy[k.whammy], (int)k.tilt, k.tiltMs, kCoda[k.coda]);
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
    float lastEnd = 0; // song ms the last acted gem ends (its sustain, else one poll after it)
    bool done = false; // every gem acted on and the last one's frets released
    unsigned prevButtons = 0; // last poll's levels
    int strumAlt = 0;
    float prevNow = 0;
    // the list's size and first gem time when this Song started (lane trainwalk2): a
    // trainer reloads the same GameGemList in place for every loop and section
    // (GemTrainerPanel::CopyGems -> GameGemList::SetGems), with later times and the
    // index restarting at 0, so a change in either is a new chart on the same list
    int numGems = 0;
    float firstMs = 0;
    int acted = 0, skipped = 0, late = 0, collide = 0;
    float maxLate = 0;
    // keys / pro keys (lane chartplay2): key index 0..24 (MIDI note 0x30 + i) -> song
    // ms it goes up, -1 while up
    float keyUpAt[25];
    // pro guitar (lane chartplay2): report string j's fret and velocity
    int rgFret[6] = {0, 0, 0, 0, 0, 0};
    int rgVel[6] = {0, 0, 0, 0, 0, 0};
    // lane chartplay3: pro guitar slide (song ms it happens, -1 none; the gem), pitch
    // bend byte; pro keys deploy press state and hand-placement bytes
    float slideAt = -1;
    int slideGem = -1;
    int rgBend = 0;
    int deployHeld = 0; // polls the deploy press has left (1: release next poll)
    int deployLast = -1000; // poll of the last deploy press
    int deploys = 0;
    int lowHand = 0, highHand = 0;
    int slides = 0, mutedPlayed = 0, bends = 0;
    // lane codawalk: the coda freestyle in song ms (-1: none / not read yet), the
    // held gem whose sustain is whammied, tilt bookkeeping
    float codaStartMs = -1, codaEndMs = -1;
    bool codaRead = false;
    int codaPolls = 0, codaSkipped = 0, whammyPolls = 0, tilts = 0;
    int holdGem = -1;
    float holdEnd = -1;
    int tiltHeld = 0, tiltLast = -1000;
    float tiltUntil = -1, tiltLastMs = -1e9f; // _TILT_MS: hold end and start (song ms)
    Song() {
        for (int i = 0; i < 25; i++) keyUpAt[i] = -1;
    }
};
Song gSong;

// Lane chartplay2: this poll's raw 16-byte peripheral report for pad 0 (the
// ProGuitarData / ProKeysData bytes retail's ReadSingleJoypad 0x82529AF8 copies
// from the HID report), and whether the chart player owns it this poll.
unsigned char gRaw[sizeof(ProGuitarData)];
bool gRawOn = false;

bool Skip(int gem) {
    const Knobs &k = K();
    // Lane codawalk _CODA=blow: every gem after the coda freestyle is left unplayed.
    if (k.coda == 2 && gSong.codaEndMs >= 0 && gSong.list && gem < gSong.list->NumGems()
        && gSong.list->GetGem(gem).mMs >= gSong.codaEndMs)
        return true;
    if (k.missPct <= 0 || gem < k.missFrom) return false;
    if (k.missTo > 0 && gem >= k.missTo) return false;
    uint32_t h = 2166136261u ^ k.seed;
    for (int i = 0; i < 4; i++) {
        h ^= (uint32_t)(gem >> (8 * i)) & 0xff;
        h *= 16777619u;
    }
    return (int)(h % 100) < k.missPct;
}

// Prints the gem count cached at bind time: on "ended" the Game (SongDB -> GameGemDB)
// has already freed the list, so gSong.list must not be dereferenced here (ASan
// heap-use-after-free on quitting a song, lane practicewalk).
void Summary(const char *why) {
    if (!gSong.list) return;
    printf("rb3-app-chartplay: song %s: gems %d acted %d skipped %d late %d (max %.1f ms) "
           "same-pad collisions %d muted %d slides %d bends %d deploys %d coda-polls %d "
           "coda-skipped %d whammy-polls %d tilts %d\n",
           why, gSong.numGems, gSong.acted, gSong.skipped, gSong.late, gSong.maxLate,
           gSong.collide, gSong.mutedPlayed, gSong.slides, gSong.bends, gSong.deploys,
           gSong.codaPolls, gSong.codaSkipped, gSong.whammyPolls, gSong.tilts);
    fflush(stdout);
}

GemPlayer *gPadPlayer = nullptr; // lane chartplay3: the player PadMatcher last found

BeatMatcher *PadMatcher(int pad) {
    gPadPlayer = nullptr;
    if (!TheGame) return nullptr;
    std::vector<Player *> &players = TheGame->GetActivePlayers();
    for (size_t i = 0; i < players.size(); i++) {
        GemPlayer *gp = dynamic_cast<GemPlayer *>(players[i]);
        if (!gp || !gp->IsLocal()) continue;
        BandUser *u = gp->GetUser();
        if (!u || !u->GetLocalUser() || u->GetLocalUser()->GetPadNum() != pad) continue;
        gPadPlayer = gp;
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
// Keys (lane chartplay2). JoypadMidiController (5-lane keys on a keytar,
// kTrackKeys) turns a key into a pad button by MidiNoteToButton (note % 12:
// 0 L2, 2 R2, 4 L1, 5 R1, 7 Tri) and hands it to JoypadController's ButtonDown,
// which maps the button to a slot; so slot s is the white key, in the octave from
// note 0x30, whose button is the controller's SlotToButton(s). KeyboardController
// (pro keys, kTrackRealKeys) takes slot = note - 0x30 directly (MidiNoteToSlot).
int SlotToKey(const BeatMatchController *c, bool pro, int s) {
    if (pro) return s >= 0 && s < 25 ? s : -1;
    switch (c->SlotToButton(s)) {
    case kPad_L2: return 0;
    case kPad_R2: return 2;
    case kPad_L1: return 4;
    case kPad_R1: return 5;
    case kPad_Tri: return 7;
    default: return -1;
    }
}

unsigned KeysOf(const BeatMatchController *c, bool pro, unsigned slots) {
    unsigned keys = 0;
    for (int s = 0; s < 32; s++)
        if (slots & (1u << s)) {
            int key = SlotToKey(c, pro, s);
            if (key >= 0) keys |= 1u << key;
        }
    return keys;
}

// UsbMidiKeyboard::Poll reads key i (note 0x30 + i, 0..24) from report bit
// (7 - i % 8) of byte i / 8, and the first five pressed keys' velocities from the
// low 7 bits of bytes 3..7 (GetSlottedKeyVelocityFromExtended).
void KeysToRaw(unsigned keys) {
    memset(gRaw, 0, sizeof(gRaw));
    for (int i = 0; i < 25; i++)
        if (keys & (1u << i)) gRaw[i / 8] |= (unsigned char)(0x80 >> (i % 8));
    for (int b = 3; b < 8; b++) gRaw[b] = (unsigned char)((gRaw[b] & 0x80) | 100);
}

// Lane chartplay3: the pro keys pedals and hand-placement fields, written through
// ProKeysData (the reader's own bitfields). Retail UsbMidiKeyboard::Poll 0x8251AC10
// reads the sustain pedal from byte 8 bit 7 and the stomp pedal from byte 9 bit 7
// (KeyboardSustainMsg / KeyboardStompBoxMsg on each change, which KeyboardController
// turns into ForceMercurySwitch(true) then (false)), the low hand placement from the
// low 5 bits of byte 0xe and the high one from bit 7 of bytes 0xb, 0xc, 0xd and bits
// 5-6 of byte 0xe (Keyboard{Low,High}HandPlacementMsg; no handler in the game).
void KeysExtraToRaw(int pedal, int low, int high) {
    ProKeysData *d = (ProKeysData *)gRaw;
    d->mSustain = pedal == 1;
    d->mStompPedal = pedal == 2;
    d->mLowHandPlacement = low & 0x1f;
    d->unkbbool = high & 1;
    d->unkcbool = (high >> 1) & 1;
    d->unkdbool = (high >> 2) & 1;
    d->unkemiddle = (high >> 3) & 3;
}

// UsbMidiGuitar::Poll reads report string j's fret and velocity through these
// ProGuitarData fields; written through the same struct, so the layout is the
// reader's own.
void RGToRaw() {
    memset(gRaw, 0, sizeof(gRaw));
    ProGuitarData *d = (ProGuitarData *)gRaw;
    const int *f = gSong.rgFret, *v = gSong.rgVel;
    d->mString0Fret = f[0];
    d->mString1FretBottomHalf = f[1] & 7;
    d->mString1FretTopHalf = f[1] >> 3;
    d->mString2Fret = f[2];
    d->mString3Fret = f[3];
    d->mString4FretBottomHalf = f[4] & 7;
    d->mString4FretTopHalf = f[4] >> 3;
    d->mString5Fret = f[5];
    d->mString0Velocity = v[0];
    d->mString1Velocity = v[1];
    d->mString2Velocity = v[2];
    d->mString3Velocity = v[3];
    d->mString4Velocity = v[4];
    d->mString5Velocity = v[5];
}
} // namespace

bool RB3AppChartPlayOn() { return K().on; }

namespace {
short gAxisRX = 0, gAxisRY = 0;
bool gAxesOn = false;
} // namespace

// Lane codawalk: the whammy (sThumbRX) and tilt (sThumbRY) axes the chart player
// gives pad `pad` this poll; false leaves the pad's own sticks.
bool RB3AppChartPlayAxes(int pad, short *rx, short *ry) {
    if (pad != 0 || !gAxesOn) return false;
    *rx = gAxisRX;
    *ry = gAxisRY;
    return true;
}

// Lane chartplay2: the raw report the chart player gives pad `pad` this poll, or
// null (the native ReadSingleJoypad then leaves it zeroed, as before).
const unsigned char *RB3AppChartPlayRaw(int pad) { return pad == 0 && gRawOn ? gRaw : nullptr; }

namespace {
void NoteLate(float lateBy, float dt) {
    if (lateBy > dt / 2) {
        gSong.late++;
        if (lateBy > gSong.maxLate) gSong.maxLate = lateBy;
    }
}

// Keys and pro keys (lane chartplay2): every gem due this poll presses its keys
// (KeysOf) through the raw report; a key is held to the gem's sustain end (times
// _SUSTAIN), else released on the next poll. A key that was down last poll goes
// up for one poll before it is pressed again (the gem waits a poll, counted as a
// same-pad collision). Returns no buttons: the keys reach the game only through
// UsbMidiKeyboard::Poll's KeyboardKeyPressed/ReleasedMsg.
unsigned KeysPoll(const BeatMatchController *c, bool pro, float now, float dt, int poll) {
    const Knobs &k = K();
    const GameGemList *gl = gSong.list;
    int n = gl->NumGems();
    unsigned prev = gSong.prevButtons, keys = 0, pressed = 0;
    for (int i = 0; i < 25; i++)
        if (gSong.keyUpAt[i] >= 0) {
            if (now >= gSong.keyUpAt[i]) gSong.keyUpAt[i] = -1;
            else keys |= 1u << i;
        }
    while (gSong.next < n) {
        int g = gSong.next;
        const GameGem &gem = gl->GetGem(g);
        float t = gem.mMs + k.offset;
        if (now < t - dt / 2) break;
        unsigned gk = KeysOf(c, pro, gem.mSlots);
        const char *what;
        if (Skip(g)) {
            gSong.skipped++;
            what = "skip";
        } else {
            if (gk & (prev | pressed)) {
                for (int i = 0; i < 25; i++)
                    if (gk & (1u << i)) gSong.keyUpAt[i] = -1;
                keys &= ~gk;
                gSong.collide++;
                break; // the key is still down: press it next poll
            }
            pressed |= gk;
            float dur = gem.mIgnoreDuration ? 0 : (float)gem.mDurationMs;
            float up = dur > 0 ? t + dur * k.sustain : now + dt / 2;
            for (int i = 0; i < 25; i++)
                if (gk & (1u << i)) gSong.keyUpAt[i] = up;
            if (up > gSong.lastEnd) gSong.lastEnd = up;
            gSong.acted++;
            what = "hit";
        }
        NoteLate(now - t, dt);
        if (k.log)
            printf("rb3-app-chartplay: poll %d gem %d ms %.1f dur %d slots 0x%x now %.1f (%+.1f) "
                   "%s keys 0x%x\n",
                   poll, g, gem.mMs, (int)gem.mDurationMs, gem.mSlots, now, now - gem.mMs, what,
                   gk);
        gSong.next++;
    }
    keys |= pressed;
    KeysToRaw(keys);
    // Lane chartplay3: _DEPLOY presses the sustain pedal, the stomp pedal or the
    // controller's own overdrive button (BeatMatchController::mForceMercuryBut) for
    // one poll whenever the player has half a bar of energy and is not deploying,
    // at most once a second; _HANDS puts the held keys' lowest index in the low
    // hand placement and their highest (capped at 15) in the high one.
    unsigned buttons = 0;
    int pedal = 0;
    if (gSong.deployHeld > 0) {
        gSong.deployHeld--;
    } else if (k.deploy && gPadPlayer && gPadPlayer->GetBandEnergy() >= 0.5f &&
               !gPadPlayer->IsDeployingBandEnergy() && poll - gSong.deployLast >= PollsPerSecond()) {
        gSong.deployHeld = 1;
        gSong.deployLast = poll;
        gSong.deploys++;
        if (k.log)
            printf("rb3-app-chartplay: poll %d deploy %d (energy %.4f) now %.1f\n", poll,
                   k.deploy, gPadPlayer->GetBandEnergy(), now);
    }
    if (gSong.deployHeld > 0) {
        if (k.deploy == 3) {
            int b = c->mForceMercuryBut;
            if (b >= 0 && b < 32) buttons |= 1u << b;
        } else {
            pedal = k.deploy;
        }
    }
    if (k.hands && keys) {
        int lo = __builtin_ctz(keys), hi = 31 - __builtin_clz(keys);
        gSong.lowHand = lo;
        gSong.highHand = hi > 15 ? 15 : hi;
    }
    KeysExtraToRaw(pedal, gSong.lowHand, gSong.highHand);
    gRawOn = true;
    if (!gSong.done && gSong.next >= n && !keys) {
        gSong.done = true;
        Summary("done");
    }
    gSong.prevButtons = keys;
    return buttons;
}

// Pro guitar (lane chartplay2), through the raw report UsbMidiGuitar::Poll reads.
// Report string j is the controller's string 5 - j (j when lefty; see
// RealGuitarController::OnMsg(StringStrummedMsg)). The gem's frets
// (GameGem::GetFret, -1 = string not played) go down one poll before its time
// (a fret change alone is a velocity-0 StringStrummedMsg: FretButtonDown and
// NonStrumSwing), unplayed strings open (fret 0); on the gem's poll each played
// string -- the gem's mSlots: its fretted strings minus kRGGhost ones (the
// GameGem(const RGGemInfo &) ctor), so a ghost note is fretted but not strummed --
// has its velocity change (100 <-> 101), which UsbMidiGuitar turns into a
// StringStrummedMsg with that velocity and one RGSwingMsg for all of them. With
// _HOPO=tap a HOPO gem is the fret change alone, at its time.
unsigned RGPoll(const RealGuitarController *rc, float now, float dt, int poll) {
    const Knobs &k = K();
    const GameGemList *gl = gSong.list;
    int n = gl->NumGems();
    bool lefty = rc->IsLefty();
    int g = gSong.next;
    if (g < n) {
        const GameGem &gem = gl->GetGem(g);
        float t = gem.mMs + k.offset;
        bool tap = k.tapHopo && gem.GetForceStrum();
        bool due = now >= t - dt / 2;
        bool skip = Skip(g);
        if (!skip && (due || (!tap && now >= t - dt * 1.5f))) {
            // A new gem's frets: any pending slide of the last one is over.
            gSong.slideAt = -1;
            bool muted = gem.IsMuted();
            for (int s = 0; s < 6; s++) {
                int f = gem.GetFret(s);
                int &rf = gSong.rgFret[lefty ? s : 5 - s];
                // Lane chartplay3 _MUTE: a kRGMuted string (FretMatchImpl counts it
                // matched at any fret) goes down open, or keeps its last fret;
                // `openall`, the control, opens every string of a muted gem.
                bool m = f >= 0 && gem.GetRGNoteType(s) == kRGMuted;
                if ((k.mute == 1 && m) || (k.mute == 3 && muted && f >= 0)) f = 0;
                else if (k.mute == 2 && m) continue;
                rf = f >= 0 ? f : 0;
            }
        }
        if (due) {
            const char *what = "skip";
            unsigned strummed = 0;
            if (skip) {
                gSong.skipped++;
            } else {
                gSong.acted++;
                if (gem.IsMuted()) gSong.mutedPlayed++;
                // Lane chartplay3 _SLIDE: half way through the sustain of a
                // left-hand-slide gem (`all`, the control: of every sustained gem)
                // each fretted string moves 2 frets, up, or down for ReverseSlide --
                // a fret change alone, as a slide is. TrackWatcherImpl::KillSustain
                // spares a LeftHandSlide gem when RealGuitarTrackWatcherImpl::PollHook
                // sees the frets no longer match.
                float dur = gem.mIgnoreDuration ? 0 : (float)gem.mDurationMs;
                if (dur > 0 && ((k.slide == 1 && gem.LeftHandSlide()) || k.slide == 2)) {
                    gSong.slideAt = t + dur * 0.5f;
                    gSong.slideGem = g;
                }
                // _BEND: the pitch-bend byte goes to 100 on a gem with a kRGBend
                // string, back to 0 on the next gem (RGPitchBendMsg and, from the
                // same byte, RGConnectedAccessoriesMsg; no handler in the game).
                if (k.bend) {
                    int b = 0;
                    for (int s = 0; s < 6; s++)
                        if (gem.GetFret(s) >= 0 && gem.GetRGNoteType(s) == kRGBend) b = 100;
                    if (b) gSong.bends++;
                    gSong.rgBend = b;
                }
                what = "tap";
                if (!tap) {
                    what = "strum";
                    for (int s = 0; s < 6; s++)
                        if (gem.mSlots & (1u << s)) {
                            int &v = gSong.rgVel[lefty ? s : 5 - s];
                            v = v == 100 ? 101 : 100;
                            strummed |= 1u << s;
                        }
                }
            }
            NoteLate(now - t, dt);
            if (k.log)
                printf("rb3-app-chartplay: poll %d gem %d ms %.1f dur %d slots 0x%x frets "
                       "%d,%d,%d,%d,%d,%d hopo %d now %.1f (%+.1f) %s strings 0x%x\n",
                       poll, g, gem.mMs, (int)gem.mDurationMs, gem.mSlots, gem.GetFret(0),
                       gem.GetFret(1), gem.GetFret(2), gem.GetFret(3), gem.GetFret(4),
                       gem.GetFret(5), (int)gem.GetForceStrum(), now, now - gem.mMs, what,
                       strummed);
            gSong.next++;
        }
    }
    if (gSong.slideAt >= 0 && now >= gSong.slideAt) {
        const GameGem &sg = gl->GetGem(gSong.slideGem);
        int step = sg.ReverseSlide() ? -2 : 2;
        for (int s = 0; s < 6; s++) {
            int f = sg.GetFret(s);
            if (f <= 0) continue;
            int nf = f + step;
            if (nf < 1) nf = f + 2;
            if (nf > 22) nf = f - 2;
            gSong.rgFret[lefty ? s : 5 - s] = nf;
        }
        gSong.slides++;
        if (k.log)
            printf("rb3-app-chartplay: poll %d slide gem %d lhs %d rev %d now %.1f (+%.1f)\n",
                   poll, gSong.slideGem, (int)sg.LeftHandSlide(), (int)sg.ReverseSlide(), now,
                   now - sg.mMs);
        gSong.slideAt = -1;
    }
    RGToRaw();
    ((ProGuitarData *)gRaw)->mPitchBend = gSong.rgBend;
    gRawOn = true;
    if (!gSong.done && gSong.next >= n) {
        gSong.done = true;
        Summary("done");
    }
    return 0;
}
} // namespace

// The button levels pad `pad` holds this poll. Pad 0 only.
unsigned int RB3AppChartPlayButtons(int pad, int poll) {
    const Knobs &k = K();
    if (!k.on || pad != 0) return 0;
    gRawOn = false;
    BeatMatcher *m = PadMatcher(pad);
    const GameGemList *gl = nullptr;
    if (m && m->mWatcher && m->mWatcher->mImpl && m->mController)
        gl = m->mWatcher->mImpl->mGemList;
    float now = gl ? m->GetNow() : 0;
    bool reloaded = gl && gl == gSong.list
        && (gl->NumGems() != gSong.numGems
            || (gSong.numGems > 0 && gl->GetGem(0).mMs != gSong.firstMs));
    if (gl != gSong.list || reloaded || (gl && now + 1000 < gSong.prevNow)) {
        if (!gSong.done) Summary(gl ? "replaced" : "ended");
        gSong = Song();
        gSong.list = gl;
        if (!gl) return 0;
        gSong.numGems = gl->NumGems();
        gSong.firstMs = gSong.numGems > 0 ? gl->GetGem(0).mMs : 0;
        while (gSong.next < gl->NumGems() && gl->GetGem(gSong.next).mMs + k.offset < now)
            gSong.next++;
        // Lane chartplay3: the song track's type (6/7 pro guitar 17/22-fret, 8/9 pro
        // bass) and how many of its gems are muted, left-hand slides, reverse
        // slides and bends (pro strings), (other parts: 0).
        TrackWatcherImpl *wi = m->mWatcher->mImpl;
        int nMuted = 0, nSlide = 0, nRev = 0, nBend = 0, nSustSlide = 0;
        for (int i = 0; i < gl->NumGems(); i++) {
            const GameGem &g = gl->GetGem(i);
            if (!g.IsRealGuitar()) continue;
            if (g.IsMuted()) nMuted++;
            if (g.LeftHandSlide()) {
                nSlide++;
                if (!g.mIgnoreDuration && g.mDurationMs) nSustSlide++;
            }
            if (g.ReverseSlide()) nRev++;
            for (int s = 0; s < 6; s++)
                if (g.GetFret(s) >= 0 && g.GetRGNoteType(s) == kRGBend) {
                    nBend++;
                    break;
                }
        }
        printf("rb3-app-chartplay: poll %d gem list %p gems %d track %d type %d now %.1f starting "
               "at gem %d; muted %d slides %d (sustained %d, reverse %d) bends %d\n",
               poll, (const void *)gl, gl->NumGems(), wi->mTrack,
               wi->mSongData ? (int)wi->mSongData->TrackTypeAt(wi->mTrack) : -1, now, gSong.next,
               nMuted, nSlide, nSustSlide, nRev, nBend);
        fflush(stdout);
    }
    if (!gl) return 0;
    float dt = now - gSong.prevNow;
    if (dt <= 0 || dt > 100) dt = 1000.0f / PollsPerSecond();
    gSong.prevNow = now;
    BeatMatchController *c = m->mController;
    GuitarController *gc = dynamic_cast<GuitarController *>(c);
    KeyboardController *kc = dynamic_cast<KeyboardController *>(c);
    JoypadMidiController *mc = dynamic_cast<JoypadMidiController *>(c);
    RealGuitarController *rc = dynamic_cast<RealGuitarController *>(c);
    JoypadController *jc = gc || mc ? nullptr : dynamic_cast<JoypadController *>(c);
    if (kc || mc) return KeysPoll(c, kc != nullptr, now, dt, poll);
    if (rc) return RGPoll(rc, now, dt, poll);
    if (!gc && !jc) return 0;

    unsigned buttons = 0;
    int n = gl->NumGems();
    // Lane codawalk: the coda freestyle extent (GemPlayer::GetCodaFreestyleExtents,
    // ticks -> song ms), read once per song.
    if (!gSong.codaRead && gPadPlayer) {
        gSong.codaRead = true;
        Extent e;
        if (gPadPlayer->GetCodaFreestyleExtents(e)) {
            gSong.codaStartMs = TickToMs(e.unk0);
            gSong.codaEndMs = TickToMs(e.unk4);
        }
        int lastIn = -1, after = 0;
        for (int i = 0; i < n; i++) {
            float ms = gl->GetGem(i).mMs;
            if (gSong.codaStartMs >= 0 && ms >= gSong.codaStartMs && ms < gSong.codaEndMs) lastIn = i;
            if (gSong.codaEndMs >= 0 && ms >= gSong.codaEndMs) after++;
        }
        printf("rb3-app-chartplay: poll %d coda freestyle ms %.1f..%.1f last gem inside %d, "
               "gems after %d\n",
               poll, gSong.codaStartMs, gSong.codaEndMs, lastIn, after);
        fflush(stdout);
    }
    bool inCoda = gSong.codaStartMs >= 0 && now >= gSong.codaStartMs - dt / 2 && now < gSong.codaEndMs;
    if (k.coda == 1 && inCoda) {
        // Mash: the chart's gems inside the freestyle are passed over unpressed.
        while (gSong.next < n && gl->GetGem(gSong.next).mMs < gSong.codaEndMs - dt) {
            gSong.next++;
            gSong.codaSkipped++;
        }
        int ph = gSong.codaPolls++;
        if (gc) {
            gSong.frets = 0x1f;
            gSong.fretsUpAt = -1;
            buttons |= SlotsToButtons(c, 0x1f);
            if (ph % 3 == 2) {
                const std::vector<int> &sb = gc->mStrumBarButtons;
                buttons |= 1u << (sb.empty() ? kPad_DDown : sb[gSong.strumAlt++ % sb.size()]);
            }
        } else if (jc) {
            unsigned b = SlotsToButtons(c, 1u << (ph % 5));
            if (!(b & gSong.prevButtons)) buttons |= b;
        }
        gSong.prevButtons = buttons;
        return buttons;
    }
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
                    gSong.holdGem = dur > 0 ? g : -1;
                    gSong.holdEnd = dur > 0 ? t + dur * k.sustain : -1;
                    gSong.lastEnd = dur > 0 ? t + dur * k.sustain : t + dt;
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
                           "now %.1f (%+.1f) %s buttons 0x%x\n",
                           poll, g, gem.mMs, (int)gem.mDurationMs, gem.mSlots,
                           (int)gem.GetForceStrum(), now, now - gem.mMs, what,
                           buttons | SlotsToButtons(c, gSong.frets));
                gSong.next++;
            }
        }
        // After the last gem the frets go up (a held green is the endgame screens'
        // Confirm, so holding it would swallow the walk's presses).
        if (gSong.next >= n && now >= gSong.lastEnd) gSong.frets = 0;
        buttons |= SlotsToButtons(c, gSong.frets);
        // Lane codawalk _WHAMMY: the whammy goes full travel and back every other
        // poll while the last acted gem's sustain is held (frets still down).
        gAxesOn = k.whammy || k.tilt;
        gAxisRX = -32768;
        gAxisRY = 0;
        if (k.whammy && gSong.holdGem >= 0 && now < gSong.holdEnd && gSong.frets) {
            bool od = TheSongDB && gPadPlayer
                && TheSongDB->GetPhraseID(gPadPlayer->GetTrackNum(), gSong.holdGem) != -1;
            if (k.whammy == 2 || od) {
                gSong.whammyPolls++;
                if ((poll >> 1) & 1) gAxisRX = 32767;
            }
        }
        // _TILT: tilt for 15 polls with half a bar of energy, not deploying, at
        // most once a second.
        if (k.tiltMs > 0) {
            // _TILT_MS: the hold and the once-a-second gate in song time.
            if (now >= gSong.tiltUntil && k.tilt && gPadPlayer
                && gPadPlayer->GetBandEnergy() >= 0.5f && !gPadPlayer->IsDeployingBandEnergy()
                && now - gSong.tiltLastMs >= 1000) {
                gSong.tiltUntil = now + k.tiltMs;
                gSong.tiltLastMs = now;
                gSong.tilts++;
                if (k.log)
                    printf("rb3-app-chartplay: poll %d tilt %.0f ms (energy %.4f) now %.1f\n",
                           poll, k.tiltMs, gPadPlayer->GetBandEnergy(), now);
            }
            gSong.tiltHeld = now < gSong.tiltUntil ? 1 : 0;
        } else if (gSong.tiltHeld > 0) {
            gSong.tiltHeld--;
        } else if (k.tilt && gPadPlayer && gPadPlayer->GetBandEnergy() >= 0.5f
                   && !gPadPlayer->IsDeployingBandEnergy() && poll - gSong.tiltLast >= PollsPerSecond()) {
            gSong.tiltHeld = PollsPerSecond() / 2;
            gSong.tiltLast = poll;
            gSong.tilts++;
            if (k.log)
                printf("rb3-app-chartplay: poll %d tilt (energy %.4f) now %.1f\n", poll,
                       gPadPlayer->GetBandEnergy(), now);
        }
        // Full positive: ReadSingleXinputJoypad negates RY (TranslateStick), and
        // GuitarController::Poll's xbox_mercury_switch passes -GetRY() on.
        if (gSong.tiltHeld > 0) gAxisRY = 32767;
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
                       "(%+.1f) %s buttons 0x%x\n",
                       poll, g, gem.mMs, gem.mSlots, (int)gem.IsCymbal(), now, now - gem.mMs, what,
                       SlotsToButtons(c, gem.mSlots));
            gSong.next++;
        }
    }
    if (!gSong.done && gSong.next >= n && !buttons) {
        gSong.done = true;
        Summary("done");
    }
    gSong.prevButtons = buttons;
    return buttons;
}

bool RB3AppChartPlayLog() { return K().on && K().log > 0; }

// _LOG: after JoypadPollCommon on a pressing poll, what the controller and the
// BeatMatcher made of it.
void RB3AppChartPlayAfterPoll(int poll) {
    BeatMatcher *m = PadMatcher(0);
    if (!m || !m->mController) return;
    BeatMatchController *c = m->mController;
    int disabled = c->IsDisabled(), mask = -1, cuser = -1;
    if (JoypadController *jc = dynamic_cast<JoypadController *>(c)) {
        mask = jc->mFretMask;
        cuser = jc->mLocalUser ? jc->mLocalUser->GetPadNum() : -2;
    } else if (GuitarController *gc = dynamic_cast<GuitarController *>(c)) {
        mask = gc->mFretMask;
    }
    printf("rb3-app-chartplay: poll %d controller disabled %d fretmask 0x%x user-pad %d "
           "matcher auto %d now %.1f lastswing %.1f\n",
           poll, disabled, mask, cuser, (int)m->mAutoplay, m->GetNow(), m->mLastSwing);
}
