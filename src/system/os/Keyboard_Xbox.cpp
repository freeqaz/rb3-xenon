#include "os/Keyboard.h"
#include "os/HolmesClient.h"
#include "xdk/XAPILIB.h"
#include <ctype.h>

namespace {
    int TranslateVK(unsigned short vk, bool shift) {
        int key = 0;
        // Alphanumeric keys: 0-9 (0x30-0x39), A-Z (0x41-0x5A)
        if ((vk >= 0x30 && vk <= 0x39) || (vk >= 0x41 && vk <= 0x5A)) {
            key = (int)vk;
            if (!shift) {
                key = tolower(key);
            }
        }
        // Function keys: F1-F12 (0x70-0x7B) → 0x191-0x19C
        if (vk >= 0x70 && vk <= 0x7B) {
            key = vk + 0x121;
        }
        switch (vk) {
        case 0x08: key = 0x08; break;   // VK_BACK → backspace
        case 0x09: key = 0x09; break;   // VK_TAB → tab
        case 0x0D: key = 0x0A; break;   // VK_RETURN → newline
        case 0x13: key = 0x12D; break;  // VK_PAUSE
        case 0x14: key = 0x122; break;  // VK_CAPITAL → caps lock
        case 0x1B: key = 0x12E; break;  // VK_ESCAPE
        case 0x21: key = 0x13A; break;  // VK_PRIOR → page up
        case 0x22: key = 0x13B; break;  // VK_NEXT → page down
        case 0x23: key = 0x139; break;  // VK_END
        case 0x24: key = 0x138; break;  // VK_HOME
        case 0x25: key = 0x140; break;  // VK_LEFT
        case 0x26: key = 0x142; break;  // VK_UP
        case 0x27: key = 0x141; break;  // VK_RIGHT
        case 0x28: key = 0x143; break;  // VK_DOWN
        case 0x2A: key = 0x12C; break;  // VK_PRINT
        case 0x2D: key = 0x136; break;  // VK_INSERT
        case 0x2E: key = 0x137; break;  // VK_DELETE
        case 0x90: key = 0x123; break;  // VK_NUMLOCK
        case 0x91: key = 0x124; break;  // VK_SCROLL
        }
        return key;
    }
}

void KeyboardInit() { KeyboardInitCommon(); }
void KeyboardTerminate() { KeyboardTerminateCommon(); }

void KeyboardPoll() {
    XINPUT_KEYSTROKE keystroke;
    DWORD res = XInputGetKeystroke(0xFF, 2, &keystroke);
    if (res != 0x10D2 && res == 0) {
        if (!(keystroke.Flags & 2)) {
            WCHAR w[2] = { keystroke.Unicode, 0 };
            char c;
            bool shift = keystroke.Flags & 8;
            bool ctrl = keystroke.Flags & 16;
            bool alt = keystroke.Flags & 32;
            WideCharToMultiByte(0, 0, w, 1, &c, 2, nullptr, nullptr);
            int key;
            if (c != '\0' && c >= ' ' && c <= '~') {
                key = c;
            } else {
                key = TranslateVK(keystroke.VirtualKey, shift);
            }
            if (key != 0) {
                KeyboardSendMsg(key, shift, ctrl, alt);
            }
        }
    }
    HolmesClientPollKeyboard();
}
