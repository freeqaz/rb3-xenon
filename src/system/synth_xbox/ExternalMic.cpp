#include "synth_xbox/ExternalMic.h"
#include "os/Debug.h"
#include <string.h>
#include <vector>
#include "xdk/xapilibi/handleapi.h"
#include "xdk/xapilibi/processthreadsapi.h"
#include "xdk/xapilibi/synchapi.h"
#include "xdk/xapilibi/xbox.h"

namespace {
    unsigned long ExternalMicThreadEntry(void *v) {
        return reinterpret_cast<ExternalMic *>(v)->sampleProcessThread();
    }

    std::vector<ExternalMic *> gMics;
}

int ExternalMic::NumConnectedMics() {
    int count = 0;
    for (unsigned int i = 0; i < gMics.size(); i++) {
        if (gMics[i]->unk9) {
            count++;
        }
    }
    return count;
}

ExternalMic::ExternalMic(unsigned long ul)
    : mDeviceId(ul), mQuit(false), unk9(false), mLastGain(-1.0f) {
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

namespace {
    struct XMicData {
        HANDLE hMic;             // 0x0
        unsigned long clientId;  // 0x4
        unsigned long numFrames; // 0x8
        unsigned long stride;    // 0xc
        unsigned char *pData;    // 0x10
        unsigned long unk14;     // 0x14
        unsigned short aFrameSizes[1]; // 0x18
    };
}

void ExternalMic::dataReady(unsigned long, unsigned long, _XOVERLAPPED *pOverlapped) {
    XMicData *data = (XMicData *)pOverlapped->dwCompletionContext;
    if (data) {
        unsigned char buf[2048] = {0};
        unsigned char *pSrc = data->pData;
        if (0 < data->numFrames) {
            unsigned int total = 0;
            unsigned short *pFrameSize = data->aFrameSizes;
            for (unsigned int i = 0; i < data->numFrames; i++) {
                unsigned short frameSize = *pFrameSize;
                if (frameSize != 0) {
                    if (frameSize & 1) {
                        MILO_LOG(
                            "Mic data frame length was odd: %x bytes; truncating last byte\n",
                            frameSize
                        );
                        frameSize = frameSize - 1;
                    }
                    memcpy(buf + total, pSrc, frameSize);
                    total += frameSize;
                }
                pSrc += data->stride;
                pFrameSize++;
            }
            if (total != 0) {
                ExternalMicClientMgr::AddAudio(mDeviceId, buf, total);
            }
        }
        if (XMicGetStatus(data->hMic) == 2) {
            XMicRequestData(
                data->hMic, data->numFrames, data->pData, data->aFrameSizes, data->clientId
            );
        }
    }
}

long ExternalMic::processGain(unsigned long deviceId) {
    float gainReq = ExternalMicClientMgr::GetRequiredGain(mDeviceId);
    if (gainReq != mLastGain) {
        MILO_ASSERT(0.0f <= gainReq && gainReq <= 1.0f, 0x26f);
        float gain = (mGainRight - mGainLeft) * gainReq + mGainLeft;
        DWORD result = XMicSetGain(deviceId, gain, 0);
        mLastGain = gainReq;
        if (result != 0) {
            return 0x80004005;
        }
    }
    return 0;
}

long ExternalMic::gatherGainAttribs(unsigned long deviceId) {
    if (XMicGetGain(deviceId, 1, &mGainLeft) > 0) {
        return 0x80004005;
    }
    DWORD result = XMicGetGain(deviceId, 2, &mGainRight);
    return 0 != result ? 0x80004005 : 0;
}

void ExternalMic::Terminate() {
    for (unsigned int i = 0; i < gMics.size(); i++) {
        delete gMics[i];
    }
    gMics.clear();
    ExternalMicClientMgr::Terminate();
}

// sw2 scatter-include (default/ExternalMic <- bandobj/OutfitConfig.cpp)
#define gRev gRev_OutfitConfig
#define gAltRev gAltRev_OutfitConfig
#include "bandobj/OutfitConfig.cpp"
#undef gRev
#undef gAltRev
