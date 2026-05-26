#include "os/Joypad_Xinput.h"
#include "os/Joypad_Xbox.h"
#include "obj/Data.h"
#include "os/CritSec.h"
#include "os/Joypad.h"
#include "os/UserMgr.h"
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

void JoypadResetXboxPC(int pad) {
    ResetAllUsersPads();
    if (TheUserMgr && TheUserMgr->GetBool()) {
        std::vector<LocalUser *> users;
        TheUserMgr->GetLocalUsers(users);
        for (int i = 0; i < pad; i++) {
            if (i >= users.size())
                break;
            AssociateUserAndPad(users[i], i);
        }
    }
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
    float *const pad_float_a,
    float *const pad_float_b,
    unsigned char *const out_char_a
) {
    XINPUT_STATE state;
    XINPUT_CAPABILITIES caps;
    unsigned int unused;
    JoypadType joypad_type = kJoypadAnalog;

    GetXinputSinceLastFrame(user_idx, &state, &unused);

    if (-1 == state.dwPacketNumber) {
        return kJoypadNone;
    }
    unsigned char setup_flag = 0;

    if (!JoypadGetCachedXInputCaps(user_idx, &caps, false)) {
        return kJoypadNone;
    }

    unsigned char caps_type = ((unsigned char *)&caps)[1];

    if (caps_type != 0) {
        switch (caps_type) {
        case 6:
        case 11:
            joypad_type = SetupHXGuitar(pad, caps);
            if (joypad_type == kJoypadNone) {
                return kJoypadNone;
            }
            if (joypad_type != kJoypadXboxButtonGuitar) {
                setup_flag = 1;
            }
            break;
        case 7:
            joypad_type = (JoypadType)7;
            setup_flag = 1;
            break;
        case 8:
            joypad_type = SetupHXDrums(pad, caps);
            break;
        case 9:
            joypad_type = (JoypadType)11;
            break;
        case 15:
            joypad_type = SetupHXKeytar(pad, caps);
            break;
        case 25:
            joypad_type = SetupHXRealGuitar(pad, caps);
            break;
        default:
            return kJoypadNone;
        }
    }

    short lx = state.Gamepad.sThumbLX;
    unsigned char deadzone_apply = (setup_flag == 0) ? 1 : 0;
    TranslateStick(stick_lx, lx, 0, deadzone_apply);

    short ry = state.Gamepad.sThumbRY;
    if ((joypad_type == kJoypadXboxDrums) && (ry > 0) && (ry < 0x100)) {
        float f = (float)ry;
        float f2 = (248.0f - f >= 0.0f) ? 248.0f : f;
        float f3 = (244.0f - f2 >= 0.0f) ? 244.0f : f2;
        int scaled = (int)((f3 - 248.0f) * 0.03054f * -55001.0f);
        short result = (short)(-0x8000 - scaled);
        TranslateStick(stick_ry, result, 1, 0);
    } else {
        TranslateStick(stick_ry, 1, 1, deadzone_apply);
    }

    short rx = state.Gamepad.sThumbRX;
    if (joypad_type == kJoypadXboxDrums && (rx > 0) && (rx < 0x100)) {
            float f = (float)rx;
            float f2 = (248.0f - f >= 0.0f) ? 248.0f : f;
            float f3 = ((int)244.0f - f2 >= 0.0f) ? 244.0f : f2;
            int scaled = (int)((f3 - 248.0f) * 0.03054f * -55001.0f);
            short result = (short)(-0x8000 - scaled);
            TranslateStick(stick_rx, result, 1, 0);
        } else {
        TranslateStick(stick_rx, rx, 1, 0);
    }

        unsigned char deadzone_apply2;
    if ((setup_flag != 0 || joypad_type == kJoypadXboxDrums)) {
        deadzone_apply2 = 0;
    } else {
        deadzone_apply2 = 1;
    }
    short ly = state.Gamepad.sThumbLY;
    TranslateStick(stick_ly, ly, 1, deadzone_apply2);

    if ((joypad_type == kJoypadXboxMidiBoxKeyboard) || (joypad_type == kJoypadXboxMidiBoxDrums)) {
        void *keyboard = *(void **)0x83099D18;
        if (keyboard != 0) {
            void **vtbl = *(void ***)keyboard;
            unsigned char sustain = ((unsigned char (*)(void *, int))(vtbl[7]))(keyboard, pad);
            if (sustain != 0) {
                *buttons |= 4;
            } else {
                *buttons &= 0xFFFFFFFB;
            }
        } else {
            *buttons &= 0xFFFFFFFB;
        }
    }

    if (joypad_type == kJoypadAnalog) {
        unsigned char lt = state.Gamepad.bLeftTrigger;
        unsigned char rt = state.Gamepad.bRightTrigger;
        unsigned char threshold = *(unsigned char *)0x83099C7C;

        if (lt > threshold) {
            *buttons |= 1;
        } else {
            *buttons &= 0xFFFFFFFE;
        }

        if (rt > threshold) {
            *buttons |= 2;
        } else {
            *buttons &= 0xFFFFFFFD;
        }
    }

    unsigned char lt = state.Gamepad.bLeftTrigger;
    unsigned char rt = state.Gamepad.bRightTrigger;
    *ltrigger = (lt >> 1) & 0x7F;
    *rtrigger = (rt >> 1) & 0x7F;

    return joypad_type;
}
