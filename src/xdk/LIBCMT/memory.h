#pragma once
// CRT compatibility header: the mem* family lives in string.h here.
// Without this file, <memory.h> case-insensitively resolves to the game's
// src/Memory.h under wibo, and the recorded dep (lowercase) doesn't exist
// on a case-sensitive FS — making every includer perpetually ninja-dirty.
#include "string.h"
