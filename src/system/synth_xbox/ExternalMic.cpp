#include "synth_xbox/ExternalMic.h"
#include "os/Debug.h"
#include "xdk/xapilibi/handleapi.h"
#include "xdk/xapilibi/processthreadsapi.h"
#include "xdk/xapilibi/synchapi.h"
#include "xdk/xapilibi/xbox.h"

namespace {
    unsigned long ExternalMicThreadEntry(void *v) {
        return reinterpret_cast<ExternalMic *>(v)->sampleProcessThread();
    }
}

ExternalMic::ExternalMic(unsigned long ul)
    : mDeviceId(ul), mQuit(false), unk9(false), unkc(-1.0f) {
    mThread = CreateThread(0, 0, ExternalMicThreadEntry, this, 4, 0);
    MILO_ASSERT(mThread, 0x6a);
    SetThreadPriority(mThread, 15);
    XSetThreadProcessor(mThread, 3);
    ResumeThread(mThread);
}

ExternalMic::~ExternalMic() {
    mQuit = true;
    WaitForSingleObject(mThread, -1);
    CloseHandle(mThread);
}
