#pragma once
#include "xdk/win_types.h"
#include "xdk/xapilibi/xbase.h"

class MicXbox;

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

class ExternalMicClientMgr {
public:
    static void Associate(int, MicXbox *);
    static bool ConnectedForClient(const MicXbox *);
    static void AddAudio(unsigned long, unsigned char *, unsigned long);
    static float GetRequiredGain(unsigned long);
    static void Terminate();
    static void Init();
};
