#pragma once
#include "xdk/win_types.h"
#include "xdk/xapilibi/xbase.h"

struct IXHV2Engine { /* Size=0x4 */

    virtual UINT32 AddRef();
    virtual UINT32 Release();
    virtual DWORD Lock(UINT32);
    virtual DWORD StartLocalProcessingModes(UINT32, void **, UINT32);
    virtual DWORD StopLocalProcessingModes(UINT32, void **, UINT32);
    virtual DWORD StartRemoteProcessingModes(UINT32, void **, UINT32);
    virtual DWORD StopRemoteProcessingModes(UINT32, void **, UINT32);
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
    virtual DWORD IsLocalTalking(UINT32);
    virtual DWORD IsRemoteTalking(UINT64);
    virtual DWORD SetRemoteTalkerOutputVoice(UINT64, IXAudio2SubmixVoice *);
    virtual DWORD SetRemoteTalkerEffectParam(UINT64, DWORD, UINT32, const void *, UINT32);
    virtual UINT32 GetDataReadyFlags();
    virtual DWORD GetLocalChatData(UINT32, unsigned char *, UINT32 *, UINT32 *);
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
