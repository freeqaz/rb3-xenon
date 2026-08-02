#pragma once
#include "xdk/win_types.h"
#include "xdk/xapilibi/xbase.h"
#include <vector>

class MicXbox;
class Symbol;

class ExternalMic {
public:
    ~ExternalMic();
    ExternalMic(unsigned long);
    long gatherGainAttribs(unsigned long);
    long processGain(unsigned long);
    void dataReady(unsigned long, unsigned long, _XOVERLAPPED *);
    unsigned long sampleProcessThread();

    static int NumConnectedMics();
    static void Terminate();
    static void Init();

    HANDLE mThread; // 0x0
    unsigned long mDeviceId;
    bool mQuit;
    bool unk9;
    float mLastGain;  // 0xc
    float mGainLeft;  // 0x10
    float mGainRight; // 0x14
};

// Per-"mic master" client slot. Allocated lazily by
// ExternalMicClientMgr::GetMasterForIndex; sizeof == 8, no vtable (retail's
// operator new site asks for 8 bytes and the inlined ctor stores only +0).
class ExternalMicClientProxy {
public:
    ExternalMicClientProxy(int index) : mIndex(index) {}

    long OnMicConnected(unsigned long, bool, const Symbol &);

    int mIndex;      // 0x0
    bool mConnected; // 0x4
};

class ExternalMicClientMgr {
public:
    static void Associate(int, MicXbox *);
    static bool ConnectedForClient(const MicXbox *);
    static void AddAudio(unsigned long, unsigned char *, unsigned long);
    static float GetRequiredGain(unsigned long);
    static ExternalMicClientProxy *GetMasterForIndex(unsigned long);
    static void OnMicDisconnected(unsigned long);
    static void Terminate();
    static void Init();

private:
    friend class ExternalMicClientProxy;

    static std::vector<ExternalMicClientProxy *> mMicMasters;
    static std::vector<unsigned long> mDevToMicMaster;
    static std::vector<unsigned long> mMicMasterToDev;
    static std::vector<MicXbox *> mAssocMicXbox;
};
