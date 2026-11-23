// rb3-app joypad layer (lane A14, 2026-11-22). Native only, rb3-app only; it
// replaces native/src/platform/Joypad_Stub.cpp in rb3-app (native/CMakeLists.txt).
//
// Retail's 360 joypad back end is Joypad_Xbox.cpp over Joypad_Xinput.cpp
// (band.exe TU5, base 0x82000000):
//   JoypadInit      0x8252A440  SystemConfig("joypad") -> JoypadInitCommon
//                               (0x82526760) -> JoypadInitXboxPCDeadzone
//                               (0x82531D90) -> JoypadResetXboxPC(4) (0x825323C0)
//                               -> XinputJoypadThreadStart (0x8252A3D8)
//   JoypadReset     0x82529AB0  `li r3,4 ; b JoypadResetXboxPC`; UserMgr's
//                               SetTheUserMgr tail-calls it (0x825231E0), which
//                               is when the local users exist to be bound
//   JoypadPoll      0x82529AE0  `b JoypadPollCommon` (0x82526A00)
//   JoypadTerminate 0x82529AB8  XinputJoypadThreadDestruction (0x82529A60) ->
//                               JoypadTerminateCommon (0x82524838)
//   ReadSingleJoypad 0x82529AF8 copies the raw HID report, then defers to
//                               ReadSingleXinputJoypad(pad, pad, ...)
// JoypadPollCommon calls ReadSingleJoypad for each pad and turns the result
// into JoypadData and ButtonDown/ButtonUp/JoypadConnection messages, with the
// pad's bound user (JoypadResetXboxPC: local user i <-> pad i). All of that is
// src/ code compiled into rb3-app unchanged (os/Joypad.cpp, os/Joypad_Xinput.cpp).
//
// This file supplies only what Joypad_Xbox.cpp's XInput thread did, with the
// native input source in place of the hardware: GetXinputSinceLastFrame
// (retail 0x825298E0) hands back pad state, and XInputGetCapabilities describes
// the pad. Nothing else is invented, so binding, actions and messages are
// retail's.
//
// The native input sources are the shared engine's, through its game-agnostic
// platform/JoypadNativeSource.h (JoypadNativeSourceAdvance once per poll,
// JoypadNativeSourceRead once per pad): headless, the MILO_INPUT_SCRIPT script
// on pad 0 (`N button`, `+N button`, `wait_screen name`, `wake`, `highlight`;
// MILO_INPUT_LOG=1 logs each scripted poll here); in a GLFW window, the
// keyboard as pad 0 and a GLFW gamepad on any pad. A pad no source drives
// reads as unplugged (dwPacketNumber -1), which is retail with no controller:
// a headless run with no script boots unchanged.
#include "os/Joypad.h"
#include "os/Joypad_Xbox.h"
#include "os/Joypad_Xinput.h"
#include "os/System.h"
#include "os/UserMgr.h"
#include "os/User.h"
#include "os/Debug.h"
#include "ui/UI.h"
#include "platform/JoypadNativeSource.h"
#include "platform/JoypadScriptHook.h"
#include "xdk/xapilibi/xinput.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

// The A14 gate (rb3_app_joypad_gate.cpp): a no-op unless RB3_APP_JOYPAD_GATE=1.
void RB3AppJoypadGateInstall();
void RB3AppJoypadGatePoll(int poll);
// rb3_app_chartplay.cpp (lane chartplay): RB3_APP_CHARTPLAY=1 plays pad 0's chart.
bool RB3AppChartPlayOn();
unsigned int RB3AppChartPlayButtons(int pad, int poll);
bool RB3AppChartPlayLog();
void RB3AppChartPlayAfterPoll(int poll);
const unsigned char *RB3AppChartPlayRaw(int pad); // lane chartplay2: keys / pro guitar report
bool RB3AppChartPlayAxes(int pad, short *rx, short *ry); // lane codawalk: whammy / tilt

namespace {
JoypadNativePadState gPad[kNumJoypads]; // this poll's native source state, per pad
DWORD gPacket = 0;
unsigned int gChart[kNumJoypads]; // this poll's RB3_APP_CHARTPLAY levels, per pad

bool InputLogEnabled() {
    static const bool on = [] {
        const char *v = getenv("MILO_INPUT_LOG");
        return v && v[0] && strcmp(v, "0") != 0;
    }();
    return on;
}

// The script's waits (wait_screen, highlight) see TheUI: the current screen's
// name and whether a transition is running.
bool ScriptUi(const char **screen, bool *inTransition) {
    if (!TheUI) return false;
    *screen = TheUI->CurrentScreen() ? TheUI->CurrentScreen()->Name() : nullptr;
    *inTransition = TheUI->InTransition();
    return true;
}

short StickToThumb(float v, bool flip) {
    // GLFW: -1..1, y positive down. XInput: -32768..32767, y positive up.
    float f = (flip ? -v : v) * 32767.0f;
    if (f > 32767.0f) f = 32767.0f;
    if (f < -32768.0f) f = -32768.0f;
    return (short)f;
}

BYTE TriggerByte(const JoypadNativePadState &s, int i, JoypadButton bit) {
    // ReadSingleXinputJoypad (Joypad_Xinput.cpp) presses kPad_L2/kPad_R2 for any
    // nonzero trigger on an analog pad and clears them at zero, so the byte is
    // nonzero exactly when the engine's source pressed the trigger's button (its
    // 0.3 threshold): a scripted or keyboard LT/RT is a fully pulled trigger, a
    // GLFW trigger past the threshold keeps its analog value.
    if (!(s.buttons & (1u << bit))) return 0;
    if (!s.hasTriggers) return 0xFF;
    float f = s.triggers[i] * 255.0f;
    if (f < 1.0f) f = 1.0f;
    if (f > 255.0f) f = 255.0f;
    return (BYTE)f;
}
} // namespace

// RB3_APP_EXTRA_PADS (lane bandwalk, 2026-11-23): opt-in extra natively driven
// pads, so one headless walk can play a 2-4 player band. Unset, nothing below
// runs and pads 1-3 read as unplugged, as before.
//
//   RB3_APP_EXTRA_PADS=KIND=SCRIPT[,KIND=SCRIPT...]
//
// names pads 1, 2, 3 in order. KIND is gamepad, guitar, drums, keys or proguitar, the same
// XInput subtypes RB3_APP_JOYPAD_KIND gives pad 0 (a bass player is a guitar pad
// that picks bass in the overshell). SCRIPT is that pad's own input script in
// the engine's MILO_INPUT_SCRIPT grammar, subset: `wait_screen NAME`, `+N
// button` (N polls after this pad's last satisfied wait), `N button` (poll N),
// `#` comments, and the engine's button names. A press lasts one poll, a wait
// gives up after 1800 polls and goes on, as the engine's do. The pad's state goes
// through the same retail path as pad 0: GetXinputSinceLastFrame here, then
// ReadSingleXinputJoypad and JoypadPollCommon, so the press reaches the game as
// a ButtonDownMsg from local user N (JoypadResetXboxPC binds user N to pad N).
// Joining the band is the game's own overshell flow, driven by those presses.
namespace {
struct ExtraStep {
    bool wait; // wait_screen
    bool relative;
    int frame;
    int button;
    char screen[64];
};
struct ExtraPad {
    bool on;
    BYTE subtype;
    std::vector<ExtraStep> steps;
    size_t cursor;
    bool waiting;
    int waitStart;
    int satisfied; // poll of the last satisfied (or timed-out) wait, -1 before any
};
ExtraPad gExtra[kNumJoypads];
int gNumExtra = 0;

int ExtraButtonBit(const char *n) {
    // The engine's ParseButtonName (JoypadNativeSource.cpp), name for name.
    static const struct { const char *name; int bit; } kNames[] = {
        {"confirm", kPad_X}, {"a", kPad_X}, {"cancel", kPad_Circle}, {"b", kPad_Circle},
        {"start", kPad_Start}, {"option", kPad_Select}, {"back", kPad_Select},
        {"select", kPad_Select}, {"up", kPad_DUp}, {"down", kPad_DDown}, {"left", kPad_DLeft},
        {"right", kPad_DRight}, {"l1", kPad_L1}, {"lb", kPad_L1}, {"r1", kPad_R1},
        {"rb", kPad_R1}, {"l2", kPad_L2}, {"lt", kPad_L2}, {"r2", kPad_R2}, {"rt", kPad_R2},
        {"x", kPad_Square}, {"y", kPad_Tri}, {"l3", kPad_L3}, {"ls", kPad_L3},
        {"r3", kPad_R3}, {"rs", kPad_R3},
    };
    for (size_t i = 0; i < sizeof kNames / sizeof kNames[0]; i++)
        if (!strcmp(n, kNames[i].name)) return kNames[i].bit;
    return -1;
}

[[noreturn]] void ExtraFail(const char *what, const char *arg) {
    fprintf(stderr, "rb3-app joypad: RB3_APP_EXTRA_PADS: %s '%s'\n", what, arg);
    fflush(stderr);
    _Exit(2);
}

// The XInput subtype of each pad kind (RB3_APP_JOYPAD_KIND and
// RB3_APP_EXTRA_PADS share it); 0 for an unknown kind.
BYTE KindToSubType(const char *v) {
    if (!strcmp(v, "gamepad")) return 1;  // XINPUT_DEVSUBTYPE_GAMEPAD
    if (!strcmp(v, "guitar")) return 6;   // XINPUT_DEVSUBTYPE_GUITAR
    if (!strcmp(v, "drums")) return 8;    // XINPUT_DEVSUBTYPE_DRUM_KIT
    if (!strcmp(v, "keys")) return 15;    // the keytar's subtype (SetupHXKeytar)
    if (!strcmp(v, "proguitar")) return 25; // the pro guitar's (SetupHXRealGuitar)
    return 0;
}
#define RB3_APP_KINDS "gamepad, guitar, drums, keys or proguitar"

BYTE KindSubType(const char *v) {
    BYTE sub = KindToSubType(v);
    if (!sub) ExtraFail("kind is not " RB3_APP_KINDS ":", v);
    return sub;
}

void LoadExtraScript(ExtraPad &x, int pad, const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) ExtraFail("cannot open script", path);
    char line[256];
    while (fgets(line, sizeof line, f)) {
        if (char *h = strchr(line, '#')) *h = 0;
        ExtraStep st = {};
        char a[64];
        if (sscanf(line, " wait_screen %63s", st.screen) == 1) {
            st.wait = true;
            x.steps.push_back(st);
            continue;
        }
        const char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (!*p || *p == '\n' || *p == '\r') continue;
        if (*p == '+') {
            st.relative = true;
            p++;
        }
        if (sscanf(p, "%d %63s", &st.frame, a) != 2) ExtraFail("cannot parse line", line);
        st.button = ExtraButtonBit(a);
        if (st.button < 0) ExtraFail("unknown button", a);
        x.steps.push_back(st);
    }
    fclose(f);
    printf("rb3-app joypad: extra pad %d (subtype %d): %d directives from '%s'\n", pad,
           (int)x.subtype, (int)x.steps.size(), path);
    fflush(stdout);
}

void ExtraPadsInit() {
    static bool done = false;
    if (done) return;
    done = true;
    const char *v = getenv("RB3_APP_EXTRA_PADS");
    if (!v || !v[0]) return;
    std::string all(v);
    size_t pos = 0;
    int pad = 1;
    while (pos <= all.size()) {
        size_t comma = all.find(',', pos);
        std::string item = all.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos);
        pos = comma == std::string::npos ? all.size() + 1 : comma + 1;
        if (item.empty()) continue;
        if (pad >= kNumJoypads) ExtraFail("more than three extra pads:", v);
        size_t eq = item.find('=');
        if (eq == std::string::npos) ExtraFail("item is not KIND=SCRIPT:", item.c_str());
        ExtraPad &x = gExtra[pad];
        x.on = true;
        x.subtype = KindSubType(item.substr(0, eq).c_str());
        x.cursor = 0;
        x.waiting = false;
        x.satisfied = -1;
        LoadExtraScript(x, pad, item.substr(eq + 1).c_str());
        pad++;
    }
    gNumExtra = pad - 1;
}

// The engine's GetScriptedButtons, for one extra pad.
unsigned int ExtraButtons(int pad, int poll) {
    ExtraPad &x = gExtra[pad];
    unsigned int buttons = 0;
    while (x.cursor < x.steps.size()) {
        ExtraStep &st = x.steps[x.cursor];
        if (st.wait) {
            if (!x.waiting) {
                x.waiting = true;
                x.waitStart = poll;
            }
            const char *screen = nullptr;
            bool tr = false;
            bool ok = ScriptUi(&screen, &tr) && screen && !tr && !strcmp(screen, st.screen);
            if (ok || poll - x.waitStart > 1800) {
                printf("rb3-app joypad: pad %d wait_screen '%s' %s at poll %d\n", pad, st.screen,
                       ok ? "satisfied" : "TIMEOUT", poll);
                x.waiting = false;
                x.satisfied = poll;
                x.cursor++;
                continue;
            }
            break;
        }
        int target = st.relative ? (x.satisfied >= 0 ? x.satisfied + st.frame : st.frame) : st.frame;
        if (poll < target) break;
        if (poll == target) buttons |= 1u << st.button;
        x.cursor++;
    }
    return buttons;
}
} // namespace

int RB3AppExtraPadCount() {
    ExtraPadsInit();
    return gNumExtra;
}

void JoypadInit() {
    DataArray *cfg = SystemConfig("joypad");
    JoypadInitCommon(cfg);
    JoypadInitXboxPCDeadzone(cfg);
    JoypadReset();
    // XinputJoypadThreadStart: no XInput thread natively. The sources are read
    // on the frame, in JoypadPoll.
    JoypadScriptSetUiQuery(ScriptUi);
    JoypadNativeSourceInit();
    ExtraPadsInit();
    RB3AppJoypadGateInstall();
}

void JoypadReset() { JoypadResetXboxPC(4); }

void JoypadPoll() {
    int poll = JoypadNativeSourceAdvance(); // loads MILO_INPUT_SCRIPT at poll 0
    for (int pad = 0; pad < kNumJoypads; pad++) {
        JoypadNativeSourceRead(pad, &gPad[pad]);
        if (gExtra[pad].on) { // RB3_APP_EXTRA_PADS: this pad is its own script's
            gPad[pad] = JoypadNativePadState();
            gPad[pad].connected = true;
            gPad[pad].buttons = ExtraButtons(pad, poll);
        }
        // RB3_APP_CHARTPLAY: the chart player's fret / strum / drum levels join
        // the pad's scripted presses; from here on it is the retail path.
        // MILO_INPUT_LOG logs the scripted presses only (gChart is not logged).
        gChart[pad] = 0;
        if (pad == 0 && gPad[0].connected && RB3AppChartPlayOn())
            gChart[0] = RB3AppChartPlayButtons(0, poll) & ~gPad[0].buttons;
        if (gPad[pad].buttons && InputLogEnabled() && (pad == 0 || gExtra[pad].on))
            printf("rb3-app joypad: poll %d pad %d scripted buttons 0x%x\n", poll, pad,
                   gPad[pad].buttons);
        gPad[pad].buttons |= gChart[pad];
    }
    JoypadPollCommon();
    if (gChart[0] && RB3AppChartPlayLog()) {
        // What the retail path made of the chart player's levels on pad 0.
        JoypadData *d = JoypadGetPadData(0);
        printf("rb3-app-chartplay: poll %d pad 0 levels 0x%x -> JoypadData type %d buttons 0x%x "
               "newly 0x%x\n",
               poll, gChart[0], (int)d->mType, d->mButtons, d->mNewPressed);
        RB3AppChartPlayAfterPoll(poll);
    }
    if (InputLogEnabled()) {
        // Lane keyswalk: what the retail path made of each press -- the pad's
        // JoypadType, its joypad.dta controller and the button's action there.
        for (int pad = 0; pad < kNumJoypads; pad++) {
            unsigned scripted = gPad[pad].buttons & ~gChart[pad];
            if (!scripted || !(pad == 0 || gExtra[pad].on)) continue;
            Symbol ct = JoypadControllerTypePadNum(pad);
            for (int b = 0; b < 32; b++)
                if (scripted & (1u << b))
                    printf("rb3-app joypad: poll %d pad %d type %d controller %s button %d "
                           "action %d\n",
                           poll, pad, (int)JoypadGetPadData(pad)->mType, ct.Str(), b,
                           (int)ButtonToAction((JoypadButton)b, ct));
        }
    }
    RB3AppJoypadGatePoll(poll);
}

void JoypadTerminate() { JoypadTerminateCommon(); }

// Retail 0x825298E0 reads the XInput thread's latest XINPUT_STATE and ORs in the
// buttons that went down since the last frame. Natively the state is this poll's
// JoypadNativeSourceRead of the pad.
namespace {
BYTE PadSubType(); // below: the XInput subtype RB3_APP_JOYPAD_KIND gives pad 0
} // namespace

void GetXinputSinceLastFrame(int pad, XINPUT_STATE *state, unsigned int *buttons) {
    memset(state, 0, sizeof(*state));
    *buttons = 0;
    if (pad < 0 || pad >= kNumJoypads || !gPad[pad].connected) {
        state->dwPacketNumber = (DWORD)-1; // retail's "nothing plugged in"
        return;
    }
    const JoypadNativePadState &s = gPad[pad];
    state->dwPacketNumber = ++gPacket;
    state->Gamepad.bLeftTrigger = TriggerByte(s, 0, kPad_L2);
    state->Gamepad.bRightTrigger = TriggerByte(s, 1, kPad_R2);
    if (s.hasSticks) {
        state->Gamepad.sThumbLX = StickToThumb(s.sticks[0][0], false);
        state->Gamepad.sThumbLY = StickToThumb(s.sticks[0][1], true);
        state->Gamepad.sThumbRX = StickToThumb(s.sticks[1][0], false);
        state->Gamepad.sThumbRY = StickToThumb(s.sticks[1][1], true);
    }
    // A guitar's whammy rests at full negative sThumbRX (lane codawalk): joypad.dta's
    // Xbox guitar entry reads it as traditional_whammy_val, -(RX + 1) / 2
    // (GuitarController::GetWhammyBar), so a stickless native guitar pad that left
    // RX at 0 held the whammy half down for the whole song (GetWhammyBar -0.5,
    // the sustain pitch bend at half travel, send_whammy -0.5).
    if (!s.hasSticks && (gExtra[pad].on ? gExtra[pad].subtype : PadSubType()) == 6)
        state->Gamepad.sThumbRX = -32768;
    // RB3_APP_CHARTPLAY_WHAMMY / _TILT (lane codawalk): the chart player's whammy
    // (sThumbRX) and tilt (sThumbRY) axes on pad 0.
    RB3AppChartPlayAxes(pad, &state->Gamepad.sThumbRX, &state->Gamepad.sThumbRY);
    *buttons = s.buttons;
}

// Retail 0x82529AF8.
int ReadSingleJoypad(int pad, unsigned int *buttons, char *lx, char *ly, char *rx, char *ry,
                     char *lt, char *rt, float *sensors, float *pressures,
                     unsigned char *pro_guitar) {
    if (pad >= kNumJoypads) return kJoypadNone;
    for (int i = 0; i < 16; i++) pro_guitar[i] = 0; // no raw HID report natively
    // RB3_APP_CHARTPLAY on a keytar or pro guitar (lane chartplay2): the chart
    // player's report bytes (keys, string frets and velocities) stand in for the
    // HID report; UsbMidiKeyboard / UsbMidiGuitar::Poll read them as retail's.
    if (const unsigned char *raw = RB3AppChartPlayRaw(pad))
        for (int i = 0; i < 16; i++) pro_guitar[i] = raw[i];
    return ReadSingleXinputJoypad(pad, pad, buttons, lx, ly, rx, ry, lt, rt, sensors,
                                  pressures, pro_guitar);
}

namespace {
// RB3_APP_JOYPAD_KIND picks the XInput subtype a natively driven pad reports
// (lane gemwalk, 2026-11-23). Unset or `gamepad`: a standard controller,
// XINPUT_DEVSUBTYPE_GAMEPAD (1), which ReadSingleXinputJoypad reads as
// kJoypadAnalog and the joypad config's `analog` entry maps to vocals.
// `guitar`: XINPUT_DEVSUBTYPE_GUITAR (6) with Flags 0 and sThumbRX/LX 0, which
// SetupHXGuitar (Joypad_Xbox.cpp) reads as kJoypadXboxHxGuitar -- the wired
// first-generation guitar, `strat_xbox` (GUITAR_WITHOUT_CALBERT, so calibration
// stays the manual screens: there is no Calbert sensor to read natively).
// `drums`: XINPUT_DEVSUBTYPE_DRUM_KIT (8), Flags 0, which SetupHXDrums reads as
// kJoypadXboxDrums, `hx_drums_xbox`. Lane keyswalk (2026-11-23): `keys`:
// subtype 15 with sThumbLY 0, which SetupHXKeytar reads as kJoypadXboxKeytar
// (not the MIDI Pro Adapter's keyboard, 0x1730); `proguitar`: subtype 25 with
// caps sThumbLY 0x1530, which SetupHXRealGuitar reads as
// kJoypadXboxRealGuitar22Fret (the 22-fret guitar). Not the button guitar
// (any other sThumbLY, kJoypadXboxButtonGuitar): JoypadIsCalbertGuitar is true
// for it, so cal_welcome starts automatic calibration, which reads the sensor
// from the raw pro-guitar report that is all zeros natively, fails and
// re-offers itself (measured, keyswalk dg walk). Any other value fails at the
// first query.
BYTE PadSubType() {
    static const BYTE sub = [] {
        const char *v = getenv("RB3_APP_JOYPAD_KIND");
        if (!v || !v[0]) return (BYTE)1;
        BYTE k = KindToSubType(v);
        if (k) return k;
        fprintf(stderr, "rb3-app joypad: RB3_APP_JOYPAD_KIND=%s is not " RB3_APP_KINDS "\n", v);
        fflush(stderr);
        _Exit(2);
    }();
    return sub;
}
} // namespace

// ReadSingleXinputJoypad (Joypad_Xinput.cpp) classifies a guitar (subtype 6/11),
// drum kit (8), keytar (15) or pro guitar (25) through these four Joypad_Xbox.cpp
// functions. That TU
// is not in rb3-app (its XInput2 thread and JoypadInit/Poll are this file's job),
// so the four bodies are here, as retail:
//   SetupHXGuitar 0x82529E20: Flags bit 1 -> JoypadData +0x4b (mIsWireless),
//     bit 0 -> +0x4a (mHasCapFlag1); sThumbLX (caps+8) == 0x1BAD -> GetBreedData,
//     return 0x1d (kJoypadXboxCoreGuitar); else bit1 && (bit0 || sThumbRX
//     (caps+0xc) >= 0x100) ? 6 (Rb2) : 5.
//   SetupHXDrums 0x82529EB8: the same two flag stores; 0x1BAD -> GetBreedData,
//     0x21 (kJoypadXboxMidiBoxDrums); bit0 || sThumbRX >= 0x100 -> 9 (Rb2);
//     bit1 && !bit0 -> kJoypadXboxRoDrums; else kJoypadXboxDrums.
// The one difference: the 0x1BAD arm does not call GetBreedData (0x82529D18),
// which reads the per-pad breed table Joypad_Xbox.cpp's XInput2 raw-data thread
// fills; that thread does not exist natively and XInputGetCapabilities below
// zeroes the caps, so the arm is never taken here.
JoypadType SetupHXGuitar(int pad, const XINPUT_CAPABILITIES &c) {
    bool wireless = c.Flags & 0x2;
    bool flag1 = c.Flags & 1;
    bool rb2 = wireless && (flag1 || c.Gamepad.sThumbRX >= 0x100);
    JoypadGetPadData(pad)->mIsWireless = wireless;
    JoypadGetPadData(pad)->mHasCapFlag1 = flag1;
    if (c.Gamepad.sThumbLX == 0x1BAD) return kJoypadXboxCoreGuitar;
    return rb2 ? kJoypadXboxHxGuitarRb2 : kJoypadXboxHxGuitar;
}

JoypadType SetupHXDrums(int pad, const XINPUT_CAPABILITIES &c) {
    bool flag1 = c.Flags & 1;
    bool wireless = c.Flags & 0x2;
    bool rb2 = flag1 || c.Gamepad.sThumbRX >= 0x100;
    bool rockOfAges = wireless && !flag1;
    JoypadGetPadData(pad)->mIsWireless = wireless;
    JoypadGetPadData(pad)->mHasCapFlag1 = flag1;
    if (c.Gamepad.sThumbLX == 0x1BAD) return kJoypadXboxMidiBoxDrums;
    if (rb2) return kJoypadXboxDrumsRb2;
    if (rockOfAges) return kJoypadXboxRoDrums;
    return kJoypadXboxDrums;
}

// SetupHXKeytar 0x82529BA0 (lane keyswalk): `lhz r11,0xa(r4)` (sThumbLY),
// `rlwinm r11,r11,0,0,27` (& ~0xF), `cmplwi 0x1730`: 0x20
// (kJoypadXboxMidiBoxKeyboard, the MIDI Pro Adapter in keyboard mode) on a
// match, else 0x22 (kJoypadXboxKeytar). It never touches the pad's JoypadData
// (r3 is dead), unlike the guitar and drums bodies above.
JoypadType SetupHXKeytar(int, const XINPUT_CAPABILITIES &c) {
    if (((unsigned short)c.Gamepad.sThumbLY & ~0xFu) == 0x1730) return kJoypadXboxMidiBoxKeyboard;
    return kJoypadXboxKeytar;
}

// SetupHXRealGuitar 0x82529DC8 (lane keyswalk): the same masked sThumbLY. 0x1530
// returns 0x1f (kJoypadXboxRealGuitar22Fret); otherwise retail computes
// `is1430 || !is1530` into r10 and returns `r10 ? 0x1e : 2` through
// subfic/subfe/rlwinm/addi. With 0x1530 already handled, r10 is always 1, so
// every other value is 0x1e (kJoypadXboxButtonGuitar) and the `2` arm is dead.
// No JoypadData store either.
JoypadType SetupHXRealGuitar(int, const XINPUT_CAPABILITIES &c) {
    if (((unsigned short)c.Gamepad.sThumbLY & ~0xFu) == 0x1530) return kJoypadXboxRealGuitar22Fret;
    return kJoypadXboxButtonGuitar;
}

// A natively driven pad reports XINPUT_DEVTYPE_GAMEPAD with PadSubType()'s
// subtype; everything ReadSingleXinputJoypad derives from it is retail's.
DWORD XInputGetCapabilities(DWORD pad, DWORD, XINPUT_CAPABILITIES *caps) {
    ExtraPadsInit();
    bool extra = pad < kNumJoypads && gExtra[pad].on;
    if (pad >= kNumJoypads || (!extra && !JoypadNativeSourceConnected((int)pad)))
        return 1167; // ERROR_DEVICE_NOT_CONNECTED
    memset(caps, 0, sizeof(*caps));
    caps->Type = 1;
    caps->SubType = extra ? gExtra[pad].subtype : PadSubType();
    if (caps->SubType == 25) caps->Gamepad.sThumbLY = 0x1530; // the 22-fret pro guitar
    return 0;
}

// Retail 0x82529AF0 is `b XamInputSendStayAliveRequest`, a wireless-pad keepalive:
// nothing to keep alive natively.
void JoypadSendKeepAlive(int) {}

// Retail requestBreedWrite (Joypad_Xbox.cpp) writes a breed packet to a Harmonix
// peripheral over XInput2 (SendRawData, 0x82529BC0); JoypadPollCommon only calls it for a pad with an
// EEPROM write pending, which a standard pad never has.
bool requestBreedWrite(int, unsigned char *) { return false; }

// Empty in retail too (the call lands on a shared `blr`, Joypad_Xbox.cpp).
void JoypadSetActuatorsImp(int, int, int) {}

// stubring: JoypadSetRumble (retail 0x82529AE8) is a lone `b` into
// JoypadSetXinputActuators (0x82532378, Joypad_Xinput.cpp, compiled here),
// which fills an XINPUT_VIBRATION and calls XInputSetState. Retail's callers
// are JoypadStageKitSetRaw and the Calbert-mode rumble fallback
// (JoypadSetXinputCalbertMode, Joypad_Xinput.cpp).
void JoypadSetRumble(int pad, int left, int right) { JoypadSetXinputActuators(pad, left, right); }

// stubring: retail 0x82529C98. Calbert-capable pads (JoypadData::mType
// 0x1e..0x2e) take the mode as a downstream 0x83 report through SendRawData
// (0x82529BC0, XInput2); every other pad approximates it with the right
// rumble motor. No native pad has an XInput2 report channel (see
// requestBreedWrite above), so the report arm sends nothing.
void JoypadSetCalbertMode(int pad, int mode) {
    int type = JoypadGetPadData(pad)->mType;
    if (type >= 0x1e && type <= 0x2e) {
        return; // retail: SendRawData(pad, 0x83, mode, 0, 0, 0, 0, 0)
    }
    JoypadSetXinputCalbertMode(pad, mode);
}

// stubring: the vibration sink. A native pad has no motors; the call reports
// a changed motor pair once (MILO_INPUT_LOG lines are what a walk greps) and
// succeeds, as XInputSetState does for a connected pad.
DWORD XInputSetState(DWORD pad, XINPUT_VIBRATION *vib) {
    static int sLast[kNumJoypads][2] = { { -1, -1 }, { -1, -1 }, { -1, -1 }, { -1, -1 } };
    if (pad >= kNumJoypads)
        return 1167; // ERROR_DEVICE_NOT_CONNECTED
    int l = vib ? vib->wLeftMotorSpeed : 0, r = vib ? vib->wRightMotorSpeed : 0;
    if (sLast[pad][0] != l || sLast[pad][1] != r) {
        fprintf(stderr, "rb3-app: XInputSetState pad %u left 0x%04x right 0x%04x\n", pad, l, r);
        sLast[pad][0] = l;
        sLast[pad][1] = r;
    }
    return 0;
}
