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
// The native input source is the shared engine's scripted input
// (MILO_INPUT_SCRIPT, platform/Joypad_Native.cpp: `N button`, `+N button`,
// `wait_screen name`, ...; MILO_INPUT_LOG=1 logs each scripted press here). It
// drives pad 0. With no script, every pad reads as unplugged
// (dwPacketNumber -1), which is retail with no controller connected: the boot
// is unchanged. (A windowed GLFW pad/keyboard is not wired; rb3-app runs
// headless.)
//
// The engine's Joypad_Native.cpp cannot be linked as-is for xenon (its
// JoypadReset writes JoypadData::mNumAnalogSticks, DC3's shape; xenon's
// JoypadData has the retail bool mHasAnalogSticks at 0x4d), and its script
// parser is file-static. So, as A10 did for Synth_Stub with a local subclass,
// this TU compiles the engine's file inside itself with its four lifecycle
// entry points renamed out of the way: that keeps ONE parser, the engine's,
// with no engine change. The renamed EngineJoypad*_ bodies are never called;
// the mNumAnalogSticks spelling exists only so the renamed JoypadReset compiles.
#include "os/Joypad.h"
#include "os/Joypad_Xbox.h"
#include "os/Joypad_Xinput.h"
#include "os/System.h"
#include "os/UserMgr.h"
#include "os/User.h"
#include "os/Debug.h"
#include "xdk/xapilibi/xinput.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#define JoypadInit EngineJoypadInit_
#define JoypadReset EngineJoypadReset_
#define JoypadPoll EngineJoypadPoll_
#define JoypadTerminate EngineJoypadTerminate_
#define mNumAnalogSticks mHasAnalogSticks
void EngineJoypadReset_();
#include RB3_APP_ENGINE_JOYPAD_NATIVE
#undef mNumAnalogSticks
#undef JoypadTerminate
#undef JoypadPoll
#undef JoypadReset
#undef JoypadInit

// The A14 gate (rb3_app_joypad_gate.cpp): a no-op unless RB3_APP_JOYPAD_GATE=1.
void RB3AppJoypadGateInstall();
void RB3AppJoypadGatePoll(int poll);

namespace {
int gPollCount = 0; // JoypadPoll calls; the script's frame number
unsigned int gScriptButtons = 0; // pad 0's scripted buttons for this poll
DWORD gPacket = 0;

bool ScriptActive() {
    if (!gInputScriptLoaded) LoadInputScript(); // engine: reads MILO_INPUT_SCRIPT once
    return !gScript.empty();
}
} // namespace

void JoypadInit() {
    DataArray *cfg = SystemConfig("joypad");
    JoypadInitCommon(cfg);
    JoypadInitXboxPCDeadzone(cfg);
    JoypadReset();
    // XinputJoypadThreadStart: no XInput thread natively. The source is polled
    // on the frame, in GetXinputSinceLastFrame.
    RB3AppJoypadGateInstall();
}

void JoypadReset() { JoypadResetXboxPC(4); }

void JoypadPoll() {
    int poll = gPollCount++;
    gScriptButtons = ScriptActive() ? GetScriptedButtons(poll) : 0;
    if (gScriptButtons && InputLogEnabled())
        printf("rb3-app joypad: poll %d pad 0 scripted buttons 0x%x\n", poll, gScriptButtons);
    JoypadPollCommon();
    RB3AppJoypadGatePoll(poll);
}

void JoypadTerminate() { JoypadTerminateCommon(); }

// Retail 0x825298E0 reads the XInput thread's latest XINPUT_STATE and ORs in the
// buttons that went down since the last frame. Natively pad 0 is the script and
// no other pad is plugged in.
void GetXinputSinceLastFrame(int pad, XINPUT_STATE *state, unsigned int *buttons) {
    memset(state, 0, sizeof(*state));
    *buttons = 0;
    if (pad != 0 || !ScriptActive()) {
        state->dwPacketNumber = (DWORD)-1; // retail's "nothing plugged in"
        return;
    }
    state->dwPacketNumber = ++gPacket;
    // ReadSingleXinputJoypad sets kPad_L2/kPad_R2 from the triggers on an analog
    // pad (and clears them otherwise), so a scripted LT/RT is a pulled trigger.
    if (gScriptButtons & (1u << kPad_L2)) state->Gamepad.bLeftTrigger = 0xFF;
    if (gScriptButtons & (1u << kPad_R2)) state->Gamepad.bRightTrigger = 0xFF;
    *buttons = gScriptButtons;
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

// The scripted pad is a standard controller: XINPUT_DEVTYPE_GAMEPAD,
// XINPUT_DEVSUBTYPE_GAMEPAD, which ReadSingleXinputJoypad reads as kJoypadAnalog.
DWORD XInputGetCapabilities(DWORD pad, DWORD, XINPUT_CAPABILITIES *caps) {
    if (pad != 0 || !ScriptActive()) return 1167; // ERROR_DEVICE_NOT_CONNECTED
    memset(caps, 0, sizeof(*caps));
    caps->Type = 1;
    caps->SubType = 1;
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
