#include "synth_xbox/ExternalMic.h"
#include "os/Debug.h"
#include <string.h>
#include <vector>
#include "xdk/xapilibi/handleapi.h"
#include "xdk/xapilibi/processthreadsapi.h"
#include "synth_xbox/Mic.h"
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
                if (*pFrameSize != 0) {
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

std::vector<ExternalMicClientProxy *> ExternalMicClientMgr::mMicMasters;
std::vector<unsigned long> ExternalMicClientMgr::mDevToMicMaster;
std::vector<unsigned long> ExternalMicClientMgr::mMicMasterToDev;
std::vector<MicXbox *> ExternalMicClientMgr::mAssocMicXbox;

void ExternalMicClientMgr::Init() {
    mAssocMicXbox.reserve(4);
    mMicMasters.reserve(4);
    mDevToMicMaster.reserve(4);
    mMicMasterToDev.reserve(4);
    for (int i = 0; i < 4; i++) {
        mDevToMicMaster.push_back(-1);
        mMicMasterToDev.push_back(-1);
        mMicMasters.push_back(NULL);
        mAssocMicXbox.push_back(NULL);
    }
}

void ExternalMicClientMgr::Terminate() {
    // The explicit null check is load-bearing: ExternalMicClientProxy has a
    // trivial destructor, so a bare `delete p` lets MSVC elide the implicit
    // test and call operator delete unconditionally. Retail tests first.
    for (unsigned int i = 0; i < mMicMasters.size(); i++) {
        if (mMicMasters[i]) {
            delete mMicMasters[i];
        }
    }
    mMicMasters.clear();
}

ExternalMicClientProxy *ExternalMicClientMgr::GetMasterForIndex(unsigned long dev) {
    unsigned long master = mDevToMicMaster[dev];
    if (master != -1) {
        return mMicMasters[master];
    }
    for (unsigned int i = 0; i < mMicMasters.size(); i++) {
        if (mMicMasters[i] == NULL) {
            mMicMasters[i] = new ExternalMicClientProxy(i);
        }
        if (mMicMasterToDev[i] == -1) {
            mDevToMicMaster[dev] = i;
            mMicMasterToDev[i] = dev;
            return mMicMasters[i];
        }
    }
    return NULL;
}

void ExternalMicClientMgr::Associate(int i, MicXbox *mic) { mAssocMicXbox[i] = mic; }

bool ExternalMicClientMgr::ConnectedForClient(const MicXbox *mic) {
    for (unsigned int i = 0; i < mAssocMicXbox.size(); i++) {
        if (mAssocMicXbox[i] == mic && mMicMasters[i] != NULL && mMicMasters[i]->mConnected) {
            return true;
        }
    }
    return false;
}

long ExternalMicClientProxy::OnMicConnected(unsigned long dev, bool b, const Symbol &s) {
    mConnected = true;
    MicXbox *mic = ExternalMicClientMgr::mAssocMicXbox[mIndex];
    if (mic) {
        mic->OnMicConnected(dev, b, s);
        return 0;
    }
    return 0x8000FFFF;
}

void ExternalMicClientMgr::AddAudio(unsigned long dev, unsigned char *data, unsigned long len) {
    ExternalMicClientProxy *master = GetMasterForIndex(dev);
    if (master) {
        MicXbox *mic = mAssocMicXbox[master->mIndex];
        if (mic) {
            mic->AddData(data, len);
        }
    }
}

float ExternalMicClientMgr::GetRequiredGain(unsigned long dev) {
    ExternalMicClientProxy *master = GetMasterForIndex(dev);
    if (master) {
        MicXbox *mic = mAssocMicXbox[master->mIndex];
        if (mic) {
            return mic->GetGain();
        }
    }
    return 0.0f;
}

void ExternalMicClientMgr::OnMicDisconnected(unsigned long dev) {
    ExternalMicClientProxy *master = GetMasterForIndex(dev);
    if (master) {
        master->mConnected = false;
        MicXbox *mic = mAssocMicXbox[master->mIndex];
        if (mic) {
            mic->OnMicDisconnected();
        }
        unsigned long m = mDevToMicMaster[dev];
        if (m != -1) {
            mMicMasterToDev[m] = -1;
            mDevToMicMaster[dev] = -1;
        }
    }
}

// sw2 scatter-include (default/ExternalMic <- bandobj/OutfitConfig.cpp)
#define gRev gRev_OutfitConfig
#define gAltRev gAltRev_OutfitConfig
#include "bandobj/OutfitConfig.cpp"
#undef gRev
#undef gAltRev
