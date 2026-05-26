#include "os/SynchronizationEvent.h"
#include "os/Debug.h"
#include "xdk/XAPILIB.h"

SynchronizationEvent::~SynchronizationEvent() { CloseHandle(mEvent); }

bool SynchronizationEvent::Wait(int timeoutMs) {
    // Redundant assignment/check required for codegen match
    // Generates the cmpwi/bne pattern seen in original binary
    int timeout = timeoutMs;
    if (timeoutMs == -1) {
        timeout = -1;
    }
    // 0x102 = WAIT_TIMEOUT from Win32/XDK API
    // Returns true if signaled, false if timeout
    return WaitForSingleObject(mEvent, timeout) != 0x102;
}

SynchronizationEvent::SynchronizationEvent()
    : mEvent(CreateEventA(nullptr, 0, 0, nullptr)) {
    MILO_ASSERT(mEvent, 0x12);
}

void SynchronizationEvent::Set() {
    BOOL success = SetEvent(mEvent);
    MILO_ASSERT(success, 0x1E);
}
