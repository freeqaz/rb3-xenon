#include "os/Joypad_Xinput.h"
#include <cstring>
#include "os/Joypad_Xbox.h"
#include "obj/Data.h"
#include "os/CritSec.h"
#include "os/Joypad.h"
#include "os/UserMgr.h"
#include "os/UsbMidiKeyboard.h"
#include "math/Utl.h"

extern UsbMidiKeyboard *TheKeyboard;
#include "xdk/XAPILIB.h"
#include "xdk/xapilibi/winerror.h"

namespace {
    XINPUT_CAPABILITIES gCaps[kNumJoypads];
    float gXboxDeadzone;
    bool gCapsValid[kNumJoypads];
    CriticalSection gCritSection;
}

void JoypadInitXboxPCDeadzone(DataArray *arr) {
    arr->FindData("deadzone", gXboxDeadzone);
    gXboxDeadzone /= 256.0f;
}

void TranslateStick(char *keys, short s, bool param_a, bool param_b) {
    float var1 = (s + 0.5f) * 0.000030518044f; // this should be / 32768

    if (param_b) {
        if (var1 > gXboxDeadzone) {
            var1 = (var1 - gXboxDeadzone) / (1 - gXboxDeadzone);
        } else if (var1 < -gXboxDeadzone) {
            var1 = (var1 + gXboxDeadzone) / (1 - gXboxDeadzone);
        } else {
            var1 = 0;
        }
    }
    char c = (var1 * 127);
    *keys = c;

    if (param_a) {
        *keys = -c;
    }
}

void TranslateButtons(unsigned int *buttons, unsigned short s) {
    static int var2[16] = { 0xC, 0xE, 0xF, 0xD, 0xB, 8, 9, 0xA, 2, 3, 0, 0, 6, 5, 7, 4 };
    *buttons = 0;

    for (int i = 0; i < 16; i++) {
        if (s & 1 << i) {
            *buttons = 1 << var2[i] | *buttons;
        }
    }
}

bool JoypadGetCachedXInputCaps(int pad, XINPUT_CAPABILITIES *caps, bool b3) {
    if (gCapsValid[pad] && !b3) {
        *caps = gCaps[pad];
    } else {
        CritSecTracker tracker(&gCritSection);
        if (XInputGetCapabilities(pad, 0, caps) == ERROR_SUCCESS) {
            gCaps[pad] = *caps;
            gCapsValid[pad] = true;
        } else
            return false;
    }
    return true;
}

// Retail 0x82531FF8: pads without a Calbert sensor approximate the calibration
// mode with the right rumble motor (full, 0x6000, off).
void JoypadSetXinputCalbertMode(int pad, int mode) {
    switch (mode) {
    case 1:
        JoypadSetRumble(pad, 0, 0xffff);
        break;
    case 2:
        JoypadSetRumble(pad, 0, 0x6000);
        break;
    default:
        JoypadSetRumble(pad, 0, 0);
        break;
    }
}

// Retail 0x82532030: the next JoypadGetCachedXInputCaps re-queries this pad.
void JoypadInvalidateXinputCaps(int pad) { gCapsValid[pad] = false; }

// Retail 0x82532378: the vibration block is zeroed whole before both motor
// speeds are written.
void JoypadSetXinputActuators(int pad, int left, int right) {
    XINPUT_VIBRATION vib;
    memset(&vib, 0, sizeof(vib));
    vib.wLeftMotorSpeed = left;
    vib.wRightMotorSpeed = right;
    XInputSetState(pad, &vib);
}

void JoypadResetXboxPC(int pad) {
    ResetAllUsersPads();
    if (TheUserMgr) {
        std::vector<LocalUser *> users;
        TheUserMgr->GetLocalUsers(users);
        for (int i = 0; i < pad; i++) {
            if (i >= users.size())
                break;
            AssociateUserAndPad(users[i], i);
        }
    }
}

// Scales a drum-pad stick reading (27..122 when hit) into the stick range.
static inline short DrumStickValue(short s) {
    float t = (Clamp(27.0f, 122.0f, (float)s) - 27.0f) / 95.0f;
    return -0x8000 - (short)(t * -26539.0f);
}

JoypadType ReadSingleXinputJoypad(
    int pad,
    int user_idx,
    unsigned int *buttons,
    char *stick_lx,
    char *stick_ly,
    char *stick_rx,
    char *stick_ry,
    char *ltrigger,
    char *rtrigger,
    float *const,
    float *const,
    unsigned char *const
) {
    XINPUT_STATE state;
    XINPUT_CAPABILITIES caps;
    JoypadType type = kJoypadAnalog;
    GetXinputSinceLastFrame(user_idx, &state, buttons);
    if (state.dwPacketNumber == -1)
        return kJoypadNone;

    bool guitar = false;
    if (JoypadGetCachedXInputCaps(user_idx, &caps, false)) {
        switch (caps.SubType) {
        case 6:
        case 11:
            type = SetupHXGuitar(pad, caps);
            if (type == kJoypadNone)
                return kJoypadNone;
            if (type != kJoypadXboxMidiBoxKeyboard)
                guitar = true;
            break;
        case 7:
            type = kJoypadXboxRoGuitar;
            guitar = true;
            break;
        case 8:
            type = SetupHXDrums(pad, caps);
            break;
        case 9:
            type = kJoypadXboxStageKit;
            break;
        case 15:
            type = SetupHXKeytar(pad, caps);
            break;
        case 25:
            type = SetupHXRealGuitar(pad, caps);
            break;
        }
    }

    TranslateStick(stick_lx, state.Gamepad.sThumbLX, false, !guitar);
    short rx = state.Gamepad.sThumbRX;
    short ly = state.Gamepad.sThumbLY;
    if (type == kJoypadXboxDrums && ly > 0 && ly < 0x100) {
        TranslateStick(stick_ly, DrumStickValue(ly), true, false);
    } else {
        TranslateStick(stick_ly, ly, true, !guitar);
    }
    if (type == kJoypadXboxDrums && rx > 0 && rx < 0x100) {
        TranslateStick(stick_rx, DrumStickValue(rx), true, false);
    } else {
        TranslateStick(stick_rx, rx, type == kJoypadXboxDrums && rx > 0x100, !guitar);
    }
    TranslateStick(stick_ry, state.Gamepad.sThumbRY, true, !guitar && type != kJoypadXboxDrums);

    if (type == kJoypadXboxMidiBoxKeyboard || type == kJoypadXboxKeytar) {
        bool sustain = TheKeyboard ? TheKeyboard->GetSustain(pad) : false;
        if (sustain)
            *buttons |= 4;
        else
            *buttons &= ~4;
    }
    unsigned char lt = state.Gamepad.bLeftTrigger;
    unsigned char rt = state.Gamepad.bRightTrigger;
    if (type == kJoypadAnalog) {
        if (lt)
            *buttons |= 1;
        else
            *buttons &= ~1;
        if (rt)
            *buttons |= 2;
        else
            *buttons &= ~2;
    }
    *ltrigger = lt >> 1;
    *rtrigger = rt >> 1;
    return type;
}
