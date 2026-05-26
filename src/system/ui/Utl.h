#pragma once
#include "os/Joypad.h"
#include "os/JoypadMsgs.h"

int PageDirection(JoypadAction);
bool IsNavAction(JoypadAction);
int ScrollDirection(const ButtonDownMsg &, bool, bool, int);
