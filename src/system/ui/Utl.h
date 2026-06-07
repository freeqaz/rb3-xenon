#pragma once
#include "os/Joypad.h"
#include "os/JoypadMsgs.h"
#include "utl/Symbol.h"

int PageDirection(JoypadAction);
bool IsNavAction(JoypadAction);
int ScrollDirection(const ButtonDownMsg &, Symbol, bool, int);
