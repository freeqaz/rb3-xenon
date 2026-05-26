#include "obj/Data.h"
#include "os/Keyboard.h"

void lmaoKeyboardJoypadAdapter(DataArray *a) {
    KeyboardKeyMsg msg(a);
    KeyboardKeyReleaseMsg msg2(a);
    Symbol msgType = KeyboardKeyMsg::Type();
    Symbol msg2Type = KeyboardKeyReleaseMsg::Type();
}
