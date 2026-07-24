#include "os/JoypadMsgs.h"
#include "os/Joypad.h"

ButtonDownMsg::ButtonDownMsg(LocalUser *user, JoypadButton butt, JoypadAction act, int i)
    : Message(Type(), user, butt, act, i) {}

LocalUser *ButtonDownMsg::GetUser() const { return mData->Obj<LocalUser>(2); }

ButtonUpMsg::ButtonUpMsg(LocalUser *user, JoypadButton butt, JoypadAction act, int i)
    : Message(Type(), user, butt, act, i) {}

LocalUser *ButtonUpMsg::GetUser() const { return mData->Obj<LocalUser>(2); }

JoypadConnectionMsg::JoypadConnectionMsg(LocalUser *user, bool b, int i)
    : Message(Type(), user, b, i) {}

LocalUser *JoypadConnectionMsg::GetUser() const { return mData->Obj<LocalUser>(2); }

JoypadBreedDataReadMsg::JoypadBreedDataReadMsg(LocalUser *user, JoypadBreedDataStatus s)
    : Message(Type(), user, s) {}

JoypadBreedDataWriteMsg::JoypadBreedDataWriteMsg(LocalUser *user, JoypadBreedDataStatus s)
    : Message(Type(), user, s) {}

// sw2 scatter-include (default/JoypadMsgs <- os/NetStream.cpp)
#define gRev gRev_NetStream
#define gAltRev gAltRev_NetStream
#if !HX_NATIVE  // native: skip X360 scatter/COMDAT-pairing include
#include "os/NetStream.cpp"
#endif
#undef gRev
#undef gAltRev

// sw2 scatter-include (default/JoypadMsgs <- synth/Sfx.cpp)
#define gRev gRev_Sfx
#define gAltRev gAltRev_Sfx
#if !HX_NATIVE  // native: skip X360 scatter/COMDAT-pairing include
#include "synth/Sfx.cpp"
#endif
#undef gRev
#undef gAltRev
