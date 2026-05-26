#pragma once
#include "os/Joypad.h"
#include "xdk/XAPILIB.h"

void GetXinputSinceLastFrame(int, XINPUT_STATE *, unsigned int *);
JoypadType SetupHXKeytar(int, const XINPUT_CAPABILITIES &);
JoypadType SetupHXRealGuitar(int, const XINPUT_CAPABILITIES &);
JoypadType SetupHXGuitar(int, const XINPUT_CAPABILITIES &);
JoypadType SetupHXDrums(int, const XINPUT_CAPABILITIES &);
