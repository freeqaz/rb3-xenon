#pragma once
#include "xdk/win_types.h"
#include "xdk/xapilibi/xbase.h"

struct IXHV2Engine { /* Size=0x4 */

    virtual UINT32 AddRef();
    virtual UINT32 Release();
    virtual DWORD Lock(UINT32);
    virtual DWORD StartLocalProcessingModes(UINT32, void **, UINT32);
    virtual DWORD StopLocalProcessingModes(UINT32, void **, UINT32);
    // The remote talker is addressed by its 64-bit XUID: retail
    // MicManagerXbox::AddRemoteMic (0x82B60F98) loads it with `ld r4`.
    virtual DWORD StartRemoteProcessingModes(UINT64, void **, UINT32);
    // XUID-addressed like its Start twin: retail MicManagerXbox::RemoveRemoteMic
    // (0x82B5F748) loads it with `ld r4`.
    virtual DWORD StopRemoteProcessingModes(UINT64, void **, UINT32);
    virtual DWORD SetMaxDecodePackets(UINT32);
    virtual DWORD RegisterLocalTalker(UINT32);
    virtual DWORD UnregisterLocalTalker(UINT32);
    virtual DWORD RegisterRemoteTalker(
        UINT64,
        XAUDIO2_EFFECT_CHAIN *,
        XAUDIO2_EFFECT_CHAIN *,
        struct IXAudio2SubmixVoice *
    );
    virtual DWORD UnregisterRemoteTalker(UINT64);
    virtual DWORD GetRemoteTalkers(UINT32 *, UINT64 *);
    virtual DWORD IsHeadsetPresent(UINT32);
    // BOOL / HRESULT, not DWORD: retail ChatReceiver::ReadLocalChat (0x82B5E2C8)
    // tests both results with a signed `cmpwi`.
    virtual BOOL IsLocalTalking(UINT32);
    virtual DWORD IsRemoteTalking(UINT64);
    virtual DWORD SetRemoteTalkerOutputVoice(UINT64, IXAudio2SubmixVoice *);
    virtual DWORD SetRemoteTalkerEffectParam(UINT64, DWORD, UINT32, const void *, UINT32);
    virtual UINT32 GetDataReadyFlags();
    virtual HRESULT GetLocalChatData(UINT32, unsigned char *, UINT32 *, UINT32 *);
    // NOTE (lane CJ-3): RB3's XDK predates the Kinect/NUI additions to
    // IXHV2Engine.  This slot exists in the DC3-era header we inherited, but
    // retail RB3 calls SubmitIncomingChatData through vtable offset 0x54, not
    // 0x58 -- i.e. there is exactly one fewer virtual before it.  Measured on
    // MicManagerXbox::Poll: with NuiGetLocalChatData present the two call sites
    // were the ONLY mismatches in the function (99.64%); removing it took the
    // function to 100%.  It has no callers anywhere in the tree.
    // virtual DWORD NuiGetLocalChatData(
    //     unsigned char *, UINT32 *, UINT32 *, NUI_TALKER_POSITION *, UINT32 *
    // );
    virtual DWORD SetPlaybackPriority(UINT64, UINT32, UINT32);
    virtual DWORD SubmitIncomingChatData(UINT64, const unsigned char *, UINT32 *);
    virtual DWORD IsSharedMicPresent(UINT32);
    IXHV2Engine(const IXHV2Engine &);
    IXHV2Engine();
    IXHV2Engine &operator=(const IXHV2Engine &);
};

// Raw microphone data callback: (dwUserIndex, pvData, dwSize, pFlags).
typedef void XHV2_MIC_RAW_DATA_READY(DWORD, void *, DWORD, int *);

// Offsets are read off MicManagerXbox::Init (retail 0x82B61220); the fields
// that listing does not write are left as Unk with their offsets.
struct XHV2INIT { /* Size=0x34 */
    /* 0x0000 */ DWORD MaxRemoteTalkers;
    /* 0x0004 */ DWORD MaxLocalTalkers;
    /* 0x0008 */ void **LocalProcessingModes;
    /* 0x000c */ DWORD NumLocalProcessingModes;
    /* 0x0010 */ void **RemoteProcessingModes;
    /* 0x0014 */ DWORD NumRemoteProcessingModes;
    /* 0x0018 */ DWORD MaxNumPackets;
    /* 0x001c */ DWORD Unk1c;
    /* 0x0020 */ XHV2_MIC_RAW_DATA_READY *pfnMicrophoneRawDataReady;
    /* 0x0024 */ DWORD Unk24;
    /* 0x0028 */ DWORD Unk28;
    /* 0x002c */ DWORD Unk2c;
    /* 0x0030 */ DWORD Unk30;
};

extern "C" HRESULT XHV2CreateEngine(const XHV2INIT *, DWORD *, IXHV2Engine **);
