#include "synth_xbox/ExternalMic.h"
#include "obj/Data.h"
#include "os/System.h"
#include "utl/Symbol.h"
#include "xdk/xapilibi/winerror.h"
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

// Retail fn_82B67A00 (120 B), unit `default/ExternalMic`. The header has DECLARED
// `static void Init()` all along and no TU ever defined it, so the address read
// fuzzy 0 with nothing of ours to pair against (an EMISSION gap, not a naming one).
//
// Retail-byte evidence (lane W16-BO):
//   bl 0x82B67948            <- ?Init@ExternalMicClientMgr@@SAXXZ, first thing
//   r30 = 0x82E1218C         <- the anonymous-namespace gMics vector below
//   loop r31 = 0 .. 3:       <- `cmplwi cr6,r31,4 / blt` back-edge, so exactly 4 mics
//       li r3, 0x18          <- sizeof(ExternalMic) == 24, confirmed by
//                               cl /d1reportSingleClassLayout
//       bl operator new / null guard
//       mr r4, r31           <- the loop index IS the ctor argument
//       bl ??0ExternalMic@@QAA@K@Z     (ctor takes unsigned long)
//       bl vector<T*>::push_back
// Its one retail caller is ?Init@Synth360@@UAAXXZ at 0x82B5DF40, which is what a
// static mic-subsystem Init is called from.
void ExternalMic::Init() {
    ExternalMicClientMgr::Init();
    for (unsigned int i = 0; i < 4; i++) {
        gMics.push_back(new ExternalMic(i));
    }
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
        unsigned char *pAlloc;   // 0x14 (operator new[] block; pData = pAlloc + 0x80)
        unsigned short aFrameSizes[8]; // 0x18
        ExternalMic *owner;      // 0x28 (read by dataReadyEntry)
    };
}

void ExternalMic::dataReady(unsigned long, unsigned long, _XOVERLAPPED *pOverlapped) {
    XMicData *data = (XMicData *)pOverlapped->dwCompletionContext;
    if (data) {
        // `i` and `total` both live outside the `0 < numFrames` guard: retail
        // zeroes the loop counter with the same register as the buf[0] store
        // and copies it into `total` before the memset (from DC3).
        unsigned int i = 0;
        unsigned int total = 0;
        unsigned char buf[2048] = {0};
        unsigned char *pSrc = data->pData;
        if (0 < data->numFrames) {
            unsigned short *pFrameSize = data->aFrameSizes;
            for (; i < data->numFrames; i++) {
                if (*pFrameSize != 0) {
                    unsigned short frameSize = *pFrameSize;
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

namespace {
    void dataReadyEntry(unsigned long a, unsigned long b, _XOVERLAPPED *pOverlapped) {
        XMicData *data = (XMicData *)pOverlapped->dwCompletionContext;
        data->owner->dataReady(a, b, pOverlapped);
    }
}

// Retail fn_82B67288 (1300 B): the header declared it and no TU defined it (an
// emission gap). Ported from DC3, whose shape retail matches: two 0x2c XMicData
// blocks memset per connection, a Sleep(10) poll, SystemConfig(synth, mic_types,
// xbox) scan over `capabilities`/`min_gain`/`max_gain`, two XMicRequestData.
unsigned long ExternalMic::sampleProcessThread() {
    DWORD deviceId = mDeviceId | 0x04000000;
    DWORD frameBytes = 0;
    XMIC_CAPABILITIES caps;
    XMicData first;
    XMicData second;
    while (!mQuit) {
        memset(&first, 0, sizeof(first));
        memset(&second, 0, sizeof(second));
        do {
            Sleep(10);
        } while (XMicGetStatus((HANDLE)deviceId) != 1 && !mQuit);
        unk9 = true;
        if (XMicGetCapabilities(deviceId, &caps) == 0) {
            DWORD multiMic = caps.dwFlags & 1;
            if (XMicStart(deviceId, 0x100, &frameBytes, 0) == 0) {
                long hr = gatherGainAttribs(deviceId);
                if (hr >= 0) {
                    static Symbol generic_usb("generic_usb");
                    Symbol micName = generic_usb;
                    DataArray *cfg = SystemConfig("synth", "mic_types", "xbox");
                    for (int i = 1; i < cfg->Size(); i++) {
                        DataArray *entry = cfg->Array(i);
                        DataArray *capsCfg = entry->FindArray("capabilities", true);
                        if (capsCfg->Int(1) == caps.dwFlags && capsCfg->Int(2) == caps.wFormatTag
                            && capsCfg->Int(3) == caps.nChannels
                            && capsCfg->Int(4) == caps.nSamplesPerSec
                            && capsCfg->Int(5) == caps.nBlockAlign
                            && capsCfg->Int(6) == caps.wBitsPerSample) {
                            DataArray *minGain = entry->FindArray("min_gain", true);
                            if (minGain->Float(1) == mGainLeft) {
                                DataArray *maxGain = entry->FindArray("max_gain", true);
                                if (maxGain->Float(1) == mGainRight) {
                                    micName = entry->Sym(0);
                                    break;
                                }
                            }
                        }
                    }
                    ExternalMicClientProxy *master =
                        ExternalMicClientMgr::GetMasterForIndex(mDeviceId);
                    if (master) {
                        hr = master->OnMicConnected(0x100, multiMic, micName);
                        if (hr >= 0) {
                            XOVERLAPPED firstOverlapped;
                            XOVERLAPPED secondOverlapped;
                            DWORD numFrames = multiMic ? 1 : 2;
                            first.hMic = (HANDLE)deviceId;
                            first.clientId = (unsigned long)&firstOverlapped;
                            first.numFrames = numFrames;
                            first.stride = frameBytes;
                            first.pAlloc = new unsigned char[numFrames * frameBytes + 0x100];
                            first.pData = first.pAlloc + 0x80;
                            first.owner = this;
                            second.hMic = (HANDLE)deviceId;
                            second.numFrames = numFrames;
                            second.clientId = (unsigned long)&secondOverlapped;
                            second.stride = frameBytes;
                            second.pAlloc = new unsigned char[numFrames * frameBytes + 0x100];
                            second.pData = second.pAlloc + 0x80;
                            second.owner = this;
                            firstOverlapped.dwExtendedError = 0;
                            firstOverlapped.hEvent = 0;
                            firstOverlapped.pCompletionRoutine = dataReadyEntry;
                            firstOverlapped.dwCompletionContext = (DWORD_PTR)&first;
                            secondOverlapped.dwExtendedError = 0;
                            secondOverlapped.hEvent = 0;
                            secondOverlapped.pCompletionRoutine = dataReadyEntry;
                            secondOverlapped.dwCompletionContext = (DWORD_PTR)&second;
                            if (XMicRequestData(
                                    first.hMic, first.numFrames, first.pData,
                                    first.aFrameSizes, first.clientId
                                ) != ERROR_IO_PENDING) {
                                hr = 0x80004005;
                            }
                            if (hr >= 0) {
                                if (XMicRequestData(
                                        second.hMic, second.numFrames, second.pData,
                                        second.aFrameSizes, second.clientId
                                    ) != ERROR_IO_PENDING) {
                                    hr = 0x80004005;
                                }
                                if (hr >= 0) {
                                    while (!mQuit) {
                                        SleepEx(10, true);
                                        hr = processGain(deviceId);
                                        MILO_ASSERT(SUCCEEDED(hr), 0x149);
                                        if (XMicGetStatus((HANDLE)deviceId) != 2) {
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        XMicStop(deviceId, 0);
        unk9 = false;
        ExternalMicClientMgr::OnMicDisconnected(mDeviceId);
        mLastGain = -1.0f;
        if (first.pAlloc) {
            delete[] first.pAlloc;
            first.pData = 0;
            first.pAlloc = 0;
        }
        if (second.pAlloc) {
            delete[] second.pAlloc;
            second.pData = 0;
            second.pAlloc = 0;
        }
    }
    return 0;
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
    return 1.0f; // retail 0x820009FC: no mic -> unity gain
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

// sw2 scatter-include (default/ExternalMic <- bandobj/OutfitConfig.cpp).
// What this supplies to ExternalMic.obj is the vector<int> template COMDATs
// retail's ExternalMic TU also emitted (??$_M_allocate_and_copy@PBH@... and a
// funclet twin at 0x82B6779C).  It used to be credited with pairing
// ??2OutfitConfig@@SAPAXI@Z at 0x82b66c48 as well; that 8-byte body is
// `li r4,0; b blockingStart` = ?Start@Voice@@QAAXXZ, and is now named so
// (lane W3-D, 2026-09-11).  Removing the include was measured at -92 B on the
// two legitimate rows, so it stays.
#define gRev gRev_OutfitConfig
#define gAltRev gAltRev_OutfitConfig
#include "bandobj/OutfitConfig.cpp"
#undef gRev
#undef gAltRev

