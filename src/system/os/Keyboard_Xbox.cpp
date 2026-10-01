#include "os/Keyboard.h"

// The Xbox 360 build has no keyboard input path. Retail's calls to
// KeyboardInit (SystemPreInit), KeyboardPoll (SystemPoll) and KeyboardTerminate
// (SystemTerminate) all land on the shared empty-body `blr` at 0x826C3888, so
// all three are empty here, and the linker drops the keyboard message source
// setup they would otherwise reach (no KeyboardInitCommon, KeyboardSendMsg or
// XInputGetKeystroke caller survives in retail).
void KeyboardInit() {}
void KeyboardTerminate() {}
void KeyboardPoll() {}
