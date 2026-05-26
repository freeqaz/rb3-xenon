#pragma once
#include "xdk/XAPILIB.h"
#include "os/ThreadCall.h"

inline DWORD CurrentThreadId() { return GetCurrentThreadId(); }

// Returns true if on main thread, or if gMainThreadID == -1 (thread checks disabled)
inline bool MainThread() {
    return gMainThreadID == -1 || gMainThreadID == CurrentThreadId();
}

bool ValidateThreadId(unsigned long);
