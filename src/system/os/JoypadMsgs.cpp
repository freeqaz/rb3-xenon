#include "os/JoypadMsgs.h"
#include "os/Joypad.h"

ButtonDownMsg::ButtonDownMsg(LocalUser *user, JoypadButton butt, JoypadAction act, int i)
    : Message(Type(), user, butt, act, i) {}

LocalUser *ButtonDownMsg::GetUser() const { return mData->Obj<LocalUser>(2); }

ButtonUpMsg::ButtonUpMsg(LocalUser *user, JoypadButton butt, JoypadAction act, int i)
    : Message(Type(), user, butt, act, i) {}

LocalUser *ButtonUpMsg::GetUser() const { return mData->Obj<LocalUser>(2); }

JoypadConnectionMsg::JoypadConnectionMsg(LocalUser *user, bool b, int i, int j)
    : Message(Type(), user, b, i, j) {}

LocalUser *JoypadConnectionMsg::GetUser() const { return mData->Obj<LocalUser>(2); }

JoypadBreedDataReadMsg::JoypadBreedDataReadMsg(LocalUser *user, JoypadBreedDataStatus s)
    : Message(Type(), user, s) {}

JoypadBreedDataWriteMsg::JoypadBreedDataWriteMsg(LocalUser *user, JoypadBreedDataStatus s)
    : Message(Type(), user, s) {}
