#include "os/VirtualKeyboard.h"
#include "obj/Dir.h"
#include "os/User.h"
#include "utl/Symbol.h"
#ifdef HX_NATIVE
#include "os/Debug.h"
#include "utl/Str.h"
#include "utl/UTF8.h"
#include <cstdlib>
#include <cstring>
#endif

VirtualKeyboard TheVirtualKeyboard;

VirtualKeyboard::VirtualKeyboard()
    : mPobjKeyboardCallback(nullptr), mCallbackReady(false), mMsgOk(false) {}

VirtualKeyboard::~VirtualKeyboard() {}

void VirtualKeyboard::Init() { SetName("virtual_keyboard", ObjectDir::Main()); }

VirtualKeyboardResultMsg::VirtualKeyboardResultMsg(bool ok, const char *text)
    : Message(Type(), ok, text ? text : gNullStr) {}

void VirtualKeyboard::Poll() {
    PlatformPoll();
    if (mCallbackReady) {
        VirtualKeyboardResultMsg msg(mMsgOk, mCallbackMsg.c_str());
        if (mPobjKeyboardCallback) {
            mPobjKeyboardCallback->Handle(msg, true);
        }
        mPobjKeyboardCallback = nullptr;
        mCallbackMsg = gNullStr;
        mMsgOk = false;
        mCallbackReady = false;
    }
}

DataNode VirtualKeyboard::OnShowKeyboardUI(const DataArray *array) {
    // Retail 0x82527F08: the requesting user is a dynamic_cast object arg.
    LocalUser *user = array->Obj<LocalUser>(2);
    int i3 = array->Int(3);
    class String s4(array->Str(4));
    class String s5(array->Str(5));
    class String s6(array->Str(6));
    mPobjKeyboardCallback = array->GetObj(7);
    int i8 = 0;
    if (array->Size() >= 9)
        i8 = array->Int(8);
    return ShowKeyboardUI(user, i3, s4, s5, s6, i8);
}

// Retail: `b PlatformTerminate`, whose Xbox body is empty and folds into the
// shared empty function. ICF then folds this 4-byte forwarder with
// MemcardXbox::Terminate at 0x8252A8A0, which is the address SystemTerminate
// calls for TheVirtualKeyboard.Terminate().
void VirtualKeyboard::Terminate() { PlatformTerminate(); }

void VirtualKeyboard::ClearKeyboardCallback() { mPobjKeyboardCallback = nullptr; }

#ifdef HX_NATIVE
void VirtualKeyboard::PlatformPoll() {
    // Xbox keyboard not available on native
}

void VirtualKeyboard::PlatformTerminate() {}

const char *VirtualKeyboard::GetInputString() {
    return "";
}

// The host has no system keyboard UI (retail's XShowKeyboardUI guide overlay,
// VirtualKeyboard_Xbox.cpp 0x82532EB8). The old stub returned without ever
// answering, so the caller's virtual_keyboard_result_msg never came: a setlist,
// band or character could not be named. Native now always answers, through
// retail's own paths: an entry from RB3_VK_TEXT (`|`-separated, one per prompt,
// in order) is the typed text, run through PlatformPoll's success arm
// (UTF8FilterKeyboardString, RemoveSpaces, ok TRUE); with no entry left it is
// retail's "Unable to show Keyboard UI" arm (empty text, ok FALSE), which the
// callers treat as a cancel. The answer goes out on the next Poll, as retail's.
DataNode VirtualKeyboard::ShowKeyboardUI(
    LocalUser *, int maxLen, String, String, String defaultTxt, int
) {
    MILO_ASSERT(!mCallbackReady, 0x62);
    static int sNext = 0;
    const char *all = getenv("RB3_VK_TEXT");
    String entry;
    bool have = false;
    if (all) {
        const char *p = all;
        for (int i = 0; i < sNext && p; i++) {
            p = strchr(p, '|');
            if (p) p++;
        }
        if (p) {
            const char *end = strchr(p, '|');
            entry = end ? String(p).substr(0, end - p) : String(p);
            have = true;
            sNext++;
        }
    }
    if (have) {
        // XShowKeyboardUI's result buffer holds maxLen wide chars, NUL included.
        char typed[512];
        strncpy(typed, entry.c_str(), sizeof typed - 1);
        typed[sizeof typed - 1] = 0;
        if (maxLen > 0 && maxLen < (int)sizeof typed && (int)strlen(typed) >= maxLen)
            typed[maxLen - 1] = 0;
        char utf8buf[512];
        char buf[512];
        UTF8FilterKeyboardString(utf8buf, 0x200, typed);
        RemoveSpaces(buf, 0x200, utf8buf);
        MILO_LOG("VirtualKeyboard: typed \"%s\" (default \"%s\")\n", buf, defaultTxt.c_str());
        mCallbackMsg = buf;
        mMsgOk = true;
    } else {
        MILO_LOG("VirtualKeyboard: no host keyboard UI and no RB3_VK_TEXT entry: cancelled\n");
        mCallbackMsg = gNullStr;
        mMsgOk = false;
    }
    mCallbackReady = true;
    return 0;
}
#endif

BEGIN_HANDLERS(VirtualKeyboard)
    HANDLE(show_keyboard, OnShowKeyboardUI)
    HANDLE_ACTION(clear_callback, ClearKeyboardCallback())
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS
