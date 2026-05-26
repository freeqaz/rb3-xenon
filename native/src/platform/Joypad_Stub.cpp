// DC3 Native Port - Joypad Stub
// Replaces Joypad_Xbox.cpp - no input for now

#include "os/Joypad.h"
#include "os/Debug.h"

void JoypadInit() {
    MILO_LOG("[Native] JoypadInit (stub)\n");
}

void JoypadPoll() {
    // No input polling
}

void JoypadReset() {}
void JoypadTerminate() {}
