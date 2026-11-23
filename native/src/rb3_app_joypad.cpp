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

// The A14 gate (rb3_app_joypad_gate.cpp): a no-op unless RB3_APP_JOYPAD_GATE=1.
void RB3AppJoypadGateInstall();
void RB3AppJoypadGatePoll(int poll);

namespace {
JoypadNativePadState gPad[kNumJoypads]; // this poll's native source state, per pad
DWORD gPacket = 0;

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

void JoypadInit() {
    DataArray *cfg = SystemConfig("joypad");
    JoypadInitCommon(cfg);
    JoypadInitXboxPCDeadzone(cfg);
    JoypadReset();
    // XinputJoypadThreadStart: no XInput thread natively. The sources are read
    // on the frame, in JoypadPoll.
    JoypadScriptSetUiQuery(ScriptUi);
    JoypadNativeSourceInit();
    RB3AppJoypadGateInstall();
}

void JoypadReset() { JoypadResetXboxPC(4); }

void JoypadPoll() {
    int poll = JoypadNativeSourceAdvance(); // loads MILO_INPUT_SCRIPT at poll 0
    for (int pad = 0; pad < kNumJoypads; pad++)
        JoypadNativeSourceRead(pad, &gPad[pad]);
    if (gPad[0].buttons && InputLogEnabled())
        printf("rb3-app joypad: poll %d pad 0 scripted buttons 0x%x\n", poll, gPad[0].buttons);
    JoypadPollCommon();
    RB3AppJoypadGatePoll(poll);
}

void JoypadTerminate() { JoypadTerminateCommon(); }

// Retail 0x825298E0 reads the XInput thread's latest XINPUT_STATE and ORs in the
// buttons that went down since the last frame. Natively the state is this poll's
// JoypadNativeSourceRead of the pad.
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
    *buttons = s.buttons;
}

// Retail 0x82529AF8.
int ReadSingleJoypad(int pad, unsigned int *buttons, char *lx, char *ly, char *rx, char *ry,
                     char *lt, char *rt, float *sensors, float *pressures,
                     unsigned char *pro_guitar) {
    if (pad >= kNumJoypads) return kJoypadNone;
    for (int i = 0; i < 16; i++) pro_guitar[i] = 0; // no raw HID report natively
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
// kJoypadXboxDrums, `hx_drums_xbox`. Any other value fails at the first query.
BYTE PadSubType() {
    static const BYTE sub = [] {
        const char *v = getenv("RB3_APP_JOYPAD_KIND");
        if (!v || !v[0] || !strcmp(v, "gamepad")) return (BYTE)1;
        if (!strcmp(v, "guitar")) return (BYTE)6;
        if (!strcmp(v, "drums")) return (BYTE)8;
        fprintf(stderr, "rb3-app joypad: RB3_APP_JOYPAD_KIND=%s is not gamepad, guitar or drums\n", v);
        fflush(stderr);
        _Exit(2);
    }();
    return sub;
}
} // namespace

// ReadSingleXinputJoypad (Joypad_Xinput.cpp) classifies a guitar (subtype 6/11)
// or drum kit (subtype 8) through these two Joypad_Xbox.cpp functions. That TU
// is not in rb3-app (its XInput2 thread and JoypadInit/Poll are this file's job),
// so the two bodies are here, as retail:
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

// A natively driven pad reports XINPUT_DEVTYPE_GAMEPAD with PadSubType()'s
// subtype; everything ReadSingleXinputJoypad derives from it is retail's.
DWORD XInputGetCapabilities(DWORD pad, DWORD, XINPUT_CAPABILITIES *caps) {
    if (pad >= kNumJoypads || !JoypadNativeSourceConnected((int)pad))
        return 1167; // ERROR_DEVICE_NOT_CONNECTED
    memset(caps, 0, sizeof(*caps));
    caps->Type = 1;
    caps->SubType = PadSubType();
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
