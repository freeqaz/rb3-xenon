#pragma once

#include "xdk/win_types.h"
#include "xdk/unknwn.h"
#include "xdk/xapilibi/xbase.h"
// IUnknown + tWAVEFORMATEX / WAVEFORMATEX are owned by unknwn.h + xapo.h (mirrors DC3
// include wiring): a single definition regardless of include order across TUs, so the
// XAPO effect TUs can pull both xaudio2.h and xapobase.h without a redefinition clash.
#include "xdk/xaudio2/xapo.h"

struct XAUDIO2_BUFFER { /* Size=0x24 */
    /* 0x0000 */ UINT32 Flags;
    /* 0x0004 */ UINT32 AudioBytes;
    /* 0x0008 */ const BYTE *pAudioData;
    /* 0x000c */ UINT32 PlayBegin;
    /* 0x0010 */ UINT32 PlayLength;
    /* 0x0014 */ UINT32 LoopBegin;
    /* 0x0018 */ UINT32 LoopLength;
    /* 0x001c */ UINT32 LoopCount;
    /* 0x0020 */ void *pContext;
};

#pragma pack(push, 1)
struct XMA2WAVEFORMATEX { /* Size=0x34 */
    /* 0x0000 */ tWAVEFORMATEX wfx;
    /* 0x0012 */ WORD NumStreams;
    /* 0x0014 */ DWORD ChannelMask;
    /* 0x0018 */ DWORD SamplesEncoded;
    /* 0x001c */ DWORD BytesPerBlock;
    /* 0x0020 */ DWORD PlayBegin;
    /* 0x0024 */ DWORD PlayLength;
    /* 0x0028 */ DWORD LoopBegin;
    /* 0x002c */ DWORD LoopLength;
    /* 0x0030 */ BYTE LoopCount;
    /* 0x0031 */ BYTE EncoderVersion;
    /* 0x0032 */ WORD BlockCount;
};
#pragma pack(pop)

struct XAUDIO2_VOICE_STATE { /* Size=0x10 */
    /* 0x0000 */ void *pCurrentBufferContext;
    /* 0x0004 */ UINT32 BuffersQueued;
    /* 0x0008 */ UINT64 SamplesPlayed;
};

// THREE fields on RB3's XDK.  This corrects the four-field layout this header
// carried between 6c5d2d50 and now, and it is corrected from RB3's OWN binary
// rather than from a sibling tree.
//
// The four-field note was inferred from dc3-decomp: DC3's
// ?UpdateMix@Voice@@AAAXXZ (DC3 0x82E373B8) fills one of these via
// GetVoiceDetails and reads offset **8** as the channel count, so on DC3's XDK
// there is an extra ActiveFlags word at 0x4.  That inference was sound about
// DC3 and its one unchecked step was "same XDK, so the layout carries over".
// It does not.  RB3's own ?UpdateMix@Voice@@AAAXXZ (retail 0x82B65510) does
// `addi r4, r1, 0x58` / GetVoiceDetails / **`lwz r28, 0x5c(r1)`** -- offset
// **4** -- and then compares that value against 6 / 2 / 1 and passes it to
// SetOutputMatrix as DestinationChannels.  Same field, same use, one word
// earlier.  RB3 retail is cl 10224 and DC3 is cl 11886 (see
// docs/decomp/xdk-11164-compiler.md); the two XDKs genuinely differ here, which
// is the early-XAudio2 layout gaining ActiveFlags later.
//
// Nothing else in this tree reads the fields -- every other mention passes the
// struct by pointer -- so the blast radius is Voice::UpdateMix alone.
struct XAUDIO2_VOICE_DETAILS { /* Size=0xc */
    /* 0x0000 */ UINT32 CreationFlags;
    /* 0x0004 */ UINT32 InputChannels;
    /* 0x0008 */ UINT32 InputSampleRate;
};

struct XAUDIO2_EFFECT_DESCRIPTOR { /* Size=0xc */
    /* 0x0000 */ IUnknown *pEffect;
    /* 0x0004 */ BOOL InitialState;
    /* 0x0008 */ UINT32 OutputChannels;
};

struct XAUDIO2_EFFECT_CHAIN { /* Size=0x8 */
    /* 0x0000 */ UINT32 EffectCount;
    /* 0x0004 */ XAUDIO2_EFFECT_DESCRIPTOR *pEffectDescriptors;
};

struct XAUDIO2_FILTER_PARAMETERS { /* Size=0xc */
    /* 0x0000 */ XAUDIO2_FILTER_TYPE Type;
    /* 0x0004 */ float Frequency;
    /* 0x0008 */ float OneOverQ;
};

struct IXAudio2Voice;

// Byte-packed, like the rest of the XDK's XAudio2 structures.  Same size and
// field offsets either way; what alignment 1 changes is that MSVC copy-
// constructs one through memcpy instead of a pair of word loads.  RB3 RETAIL
// does exactly that: ??$__uninitialized_copy@PAUXAUDIO2_SEND_DESCRIPTOR
// (0x82B5C710), ??$__uninitialized_fill_n@... (0x82B5C828) and
// ?push_back@?$vector@UXAUDIO2_SEND_DESCRIPTOR (0x82B5D3B0) each copy an
// element with `li r5, 0x8` / `bl memcpy` (fn_8282A900).  The pack was present
// in dc3-decomp's copy of this header (same finding on DC3's binary) and was
// dropped when the header was ported here; without it those three rows read
// 47.1 / 40.8 / 53.8 (lane W3-D, 2026-09-11).
#pragma pack(push, 1)
struct XAUDIO2_SEND_DESCRIPTOR { /* Size=0x8 */
    /* 0x0000 */ UINT32 Flags;
    /* 0x0004 */ IXAudio2Voice *pOutputVoice;
};
#pragma pack(pop)

struct XAUDIO2_VOICE_SENDS { /* Size=0x8 */
    /* 0x0000 */ UINT32 SendCount;
    /* 0x0004 */ XAUDIO2_SEND_DESCRIPTOR *pSends;
};

struct NUI_TALKER_POSITION { /* Size=0x8 */
    /* 0x0000 */ float fDirection;
    /* 0x0004 */ float fConfidence;
};

struct IXAudio2Voice { /* Size=0x4 */

    virtual void GetVoiceDetails(XAUDIO2_VOICE_DETAILS *);
    virtual HRESULT SetOutputVoices(const XAUDIO2_VOICE_SENDS *);
    virtual HRESULT SetEffectChain(const XAUDIO2_EFFECT_CHAIN *);
    virtual HRESULT EnableEffect(UINT32, UINT32);
    virtual HRESULT DisableEffect(UINT32, UINT32);
    virtual void GetEffectState(UINT32, BOOL *);
    virtual HRESULT SetEffectParameters(UINT32, const void *, UINT32, UINT32);
    virtual HRESULT GetEffectParameters(UINT32, void *, UINT32);
    virtual HRESULT SetFilterParameters(const XAUDIO2_FILTER_PARAMETERS *, UINT32);
    virtual void GetFilterParameters(XAUDIO2_FILTER_PARAMETERS *);
    virtual HRESULT
    SetOutputFilterParameters(IXAudio2Voice *, const XAUDIO2_FILTER_PARAMETERS *, UINT32);
    virtual void GetOutputFilterParameters(IXAudio2Voice *, XAUDIO2_FILTER_PARAMETERS *);
    virtual HRESULT SetVolume(float, UINT32);
    virtual void GetVolume(float *);
    virtual HRESULT SetChannelVolumes(UINT32, const float *, UINT32);
    virtual void GetChannelVolumes(UINT32, float *);
    virtual HRESULT
    SetOutputMatrix(IXAudio2Voice *, UINT32, UINT32, const float *, UINT32);
    virtual void GetOutputMatrix(IXAudio2Voice *, UINT32, UINT32, float *);
    virtual void DestroyVoice();
    IXAudio2Voice(const IXAudio2Voice &);
    IXAudio2Voice();
    IXAudio2Voice &operator=(const IXAudio2Voice &);
};

struct IXAudio2SubmixVoice : public IXAudio2Voice { /* Size=0x4 */
    /* 0x0000: fields for IXAudio2Voice */

    virtual void GetVoiceDetails(XAUDIO2_VOICE_DETAILS *) = 0;
    virtual HRESULT SetOutputVoices(const XAUDIO2_VOICE_SENDS *) = 0;
    virtual HRESULT SetEffectChain(const XAUDIO2_EFFECT_CHAIN *) = 0;
    virtual HRESULT EnableEffect(UINT32, UINT32) = 0;
    virtual HRESULT DisableEffect(UINT32, UINT32) = 0;
    virtual void GetEffectState(UINT32, UINT32 *) = 0;
    virtual HRESULT SetEffectParameters(UINT32, const void *, UINT32, UINT32) = 0;
    virtual HRESULT GetEffectParameters(UINT32, void *, UINT32) = 0;
    virtual HRESULT SetFilterParameters(const XAUDIO2_FILTER_PARAMETERS *, UINT32) = 0;
    virtual void GetFilterParameters(XAUDIO2_FILTER_PARAMETERS *) = 0;
    virtual HRESULT SetOutputFilterParameters(
        IXAudio2Voice *, const XAUDIO2_FILTER_PARAMETERS *, UINT32
    ) = 0;
    virtual void
    GetOutputFilterParameters(IXAudio2Voice *, XAUDIO2_FILTER_PARAMETERS *) = 0;
    virtual HRESULT SetVolume(float, UINT32) = 0;
    virtual void GetVolume(float *) = 0;
    virtual HRESULT SetChannelVolumes(UINT32, const float *, UINT32) = 0;
    virtual void GetChannelVolumes(UINT32, float *) = 0;
    virtual HRESULT
    SetOutputMatrix(IXAudio2Voice *, UINT32, UINT32, const float *, UINT32) = 0;
    virtual void GetOutputMatrix(IXAudio2Voice *, UINT32, UINT32, float *) = 0;
    virtual void DestroyVoice() = 0;
    IXAudio2SubmixVoice(const IXAudio2SubmixVoice &);
    IXAudio2SubmixVoice();
    IXAudio2SubmixVoice &operator=(const IXAudio2SubmixVoice &);
};

struct IXAudio2SourceVoice : public IXAudio2Voice { /* Size=0x4 */
    /* 0x0000: fields for IXAudio2Voice */
    virtual void GetVoiceDetails(XAUDIO2_VOICE_DETAILS *) = 0;
    virtual HRESULT SetOutputVoices(const XAUDIO2_VOICE_SENDS *) = 0;
    virtual HRESULT SetEffectChain(const XAUDIO2_EFFECT_CHAIN *) = 0;
    virtual HRESULT EnableEffect(UINT32, UINT32) = 0;
    virtual HRESULT DisableEffect(UINT32, UINT32) = 0;
    virtual void GetEffectState(UINT32, BOOL *) = 0;
    virtual HRESULT SetEffectParameters(UINT32, const void *, UINT32, UINT32) = 0;
    virtual HRESULT GetEffectParameters(UINT32, void *, UINT32) = 0;
    virtual HRESULT SetFilterParameters(const XAUDIO2_FILTER_PARAMETERS *, UINT32) = 0;
    virtual void GetFilterParameters(XAUDIO2_FILTER_PARAMETERS *) = 0;
    virtual HRESULT SetOutputFilterParameters(
        IXAudio2Voice *, const XAUDIO2_FILTER_PARAMETERS *, UINT32
    ) = 0;
    virtual void
    GetOutputFilterParameters(IXAudio2Voice *, XAUDIO2_FILTER_PARAMETERS *) = 0;
    virtual HRESULT SetVolume(float, UINT32) = 0;
    virtual void GetVolume(float *) = 0;
    virtual HRESULT SetChannelVolumes(UINT32, const float *, UINT32) = 0;
    virtual void GetChannelVolumes(UINT32, float *) = 0;
    virtual HRESULT
    SetOutputMatrix(IXAudio2Voice *, UINT32, UINT32, const float *, UINT32) = 0;
    virtual void GetOutputMatrix(IXAudio2Voice *, UINT32, UINT32, float *) = 0;
    virtual void DestroyVoice() = 0;
    virtual HRESULT Start(UINT32, UINT32);
    virtual HRESULT Stop(UINT32, UINT32);
    virtual HRESULT SubmitSourceBuffer(const XAUDIO2_BUFFER *, const void *);
    virtual HRESULT FlushSourceBuffers();
    virtual HRESULT Discontinuity();
    virtual HRESULT ExitLoop(UINT32);
    virtual void GetState(XAUDIO2_VOICE_STATE *, UINT32);
    virtual HRESULT SetFrequencyRatio(float, UINT32);
    virtual void GetFrequencyRatio(float *);
    virtual HRESULT SetSourceSampleRate(UINT32);
    IXAudio2SourceVoice(const IXAudio2SourceVoice &);
    IXAudio2SourceVoice();
    IXAudio2SourceVoice &operator=(const IXAudio2SourceVoice &);
};

struct IXAudioRefCount { /* Size=0x4 */
    virtual UINT32 AddRef();
    virtual UINT32 Release();
    IXAudioRefCount(const IXAudioRefCount &);
    IXAudioRefCount();
    IXAudioRefCount &operator=(const IXAudioRefCount &);
};

struct IXAudioBatchAllocator : public IXAudioRefCount { /* Size=0x4 */
    /* 0x0000: fields for IXAudioRefCount */
    virtual UINT32 AddRef() = 0;
    virtual UINT32 Release() = 0;
    virtual void GrowHeap(UINT32);
    virtual DWORD CreateHeap(UINT32);
    virtual UINT32 GetFreeHeapSize();
    virtual void *Alloc(UINT32);
    IXAudioBatchAllocator(const IXAudioBatchAllocator &);
    IXAudioBatchAllocator();
    IXAudioBatchAllocator &operator=(const IXAudioBatchAllocator &);
};
