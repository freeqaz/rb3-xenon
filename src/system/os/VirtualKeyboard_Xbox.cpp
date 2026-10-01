#include "os/Debug.h"
#include "os/VirtualKeyboard.h"
#include "os/User.h"
#include "utl/Str.h"
#include "utl/Symbol.h"
#include "utl/UTF8.h"
#include "xdk/XAPILIB.h"

namespace {
    XOVERLAPPED *gXoKeyboard;
    wchar_t *gWstrKeyboard;
    char gCstrKeyboard[512];
    wchar_t gDefaultText[512];
    wchar_t gWindowTitle[512];
    wchar_t gDescrptionText[512];
}

// Retail 0x82532DC8. There is no "check pending" flag: the live overlapped
// block is the flag, and once the UI completes both buffers are freed.
void VirtualKeyboard::PlatformPoll() {
    if (gXoKeyboard && gXoKeyboard->InternalLow != 0x3E5) {
        if (XGetOverlappedExtendedError(gXoKeyboard) == 0) {
            char buf[512];
            int ret = WideCharToMultiByte(
                0xFDE9, 0, gWstrKeyboard, -1, buf, 0x200, nullptr, nullptr
            );
            MILO_ASSERT(ret, 0x35);
            char utf8buf[512];
            UTF8FilterKeyboardString(utf8buf, 0x200, buf);
            RemoveSpaces(buf, 0x200, utf8buf);
            mCallbackMsg = buf;
            mMsgOk = true;
            mCallbackReady = true;
        } else {
            mCallbackMsg = gNullStr;
            mMsgOk = false;
            mCallbackReady = true;
        }
        delete gWstrKeyboard;
        gWstrKeyboard = nullptr;
        delete gXoKeyboard;
        gXoKeyboard = nullptr;
    }
}

const char *VirtualKeyboard::GetInputString() {
    if (gWstrKeyboard && *gWstrKeyboard) {
        memset(gCstrKeyboard, 0, 512);
        int ret = WideCharToMultiByte(
            0xFDE9, 0, gWstrKeyboard, -1, gCstrKeyboard, 0x200, nullptr, nullptr
        );
        MILO_ASSERT(ret, 0xCC);
        char buf[512];
        UTF8FilterKeyboardString(buf, 0x200, gCstrKeyboard);
        RemoveSpaces(gCstrKeyboard, 0x200, buf);
        return gCstrKeyboard;
    } else {
        return gNullStr;
    }
}

// Retail 0x82532EB8: the pad comes from the requesting user; the previous
// buffers are not freed here (PlatformPoll owns that).
DataNode VirtualKeyboard::ShowKeyboardUI(
    LocalUser *user, int i2, String windowTitle, String descText, String defaultTxt, int i8
) {
    MILO_ASSERT(!mCallbackReady, 0x62);
    wchar_t *newWStr = new wchar_t[i2];
    *newWStr = 0;
    XOVERLAPPED *newXo = new XOVERLAPPED;
    memset(newXo, 0, sizeof(XOVERLAPPED));
    int pad = user->GetPadNum();
    UTF8toWChar_t(gDefaultText, defaultTxt.c_str());
    UTF8toWChar_t(gWindowTitle, windowTitle.c_str());
    UTF8toWChar_t(gDescrptionText, descText.c_str());
    DWORD flags = 0;
    if ((unsigned int)i8 >= 1) {
        if ((unsigned int)i8 != 1) {
            MILO_ASSERT(false, 0x98);
        } else {
            flags = 0x20;
        }
    } else {
        flags = 0x20000001;
    }
    DWORD res = XShowKeyboardUI(
        pad, flags, gDefaultText, gWindowTitle, gDescrptionText, newWStr, i2, newXo
    );
    if (res != 0x3E5) {
        delete newWStr;
        delete newXo;
        MILO_NOTIFY("Unable to show Keyboard UI.  XShowKeyboardUI returned %d.\n", res);
        mCallbackMsg = gNullStr;
        mMsgOk = false;
        mCallbackReady = true;
        return 0;
    } else {
        gWstrKeyboard = newWStr;
        gXoKeyboard = newXo;
        return 0;
    }
}
