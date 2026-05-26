#pragma once
#include "obj/Object.h"

class VirtualKeyboard : public Hmx::Object {
private:
    Hmx::Object *mPobjKeyboardCallback; // 0x2c
    bool mCallbackReady; // 0x30
    bool mMsgOk; // 0x31
    String mCallbackMsg; // 0x34

    void PlatformPoll();
    const char *GetInputString();
    DataNode ShowKeyboardUI(int, int, String, String, String, int);

public:
    VirtualKeyboard();
    virtual ~VirtualKeyboard();
    virtual DataNode Handle(DataArray *, bool);

    void Init();
    void ClearKeyboardCallback();
    void Poll();
    void Terminate();
    bool IsKeyboardShowing();
    void NotifyCallbacksOpen();
    void NotifyCallbacksClose();

    void PlatformTerminate();
    DataNode OnShowKeyboardUI(const DataArray *);
};

#include "obj/Msg.h"

DECLARE_MESSAGE(VirtualKeyboardResultMsg, "virtual_keyboard_result_msg");
VirtualKeyboardResultMsg(bool ok, const char *text);
bool IsOK() const { return mData->Int(2); }
const char *Text() const { return mData->Str(3); }
END_MESSAGE

extern VirtualKeyboard TheVirtualKeyboard;
