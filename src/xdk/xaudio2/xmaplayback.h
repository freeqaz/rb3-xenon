#pragma once
#include "xdk/win_types.h"

// XMA playback library (xmahal), as XMAReader uses it. Declared from the call
// sites: retail 0x82C11F60 tears a playback object down, 0x82C11E70 flushes one
// of its streams (it walks a 0x60-byte context per stream).
#ifdef __cplusplus
extern "C" {
#endif

typedef struct XMAPLAYBACK XMAPLAYBACK;

typedef struct XMA_PLAYBACK_INIT { /* Size=0xc */
    /* 0x0000 */ DWORD sampleRate;
    /* 0x0004 */ DWORD outputBufferSizeInSamples;
    /* 0x0008 */ BYTE channelCount;
    /* 0x0009 */ BYTE subframesToDecode;
} XMA_PLAYBACK_INIT;

/* Retail 0x82C12098, from XMAReader::Init: (stream count, init array, 0,
   &playback, 0, 0). */
HRESULT XMAPlaybackCreate(
    DWORD dwStreamCount,
    XMA_PLAYBACK_INIT *pStreamInits,
    DWORD dwFlags,
    XMAPLAYBACK **ppPlayback,
    void *pReserved,
    DWORD dwReserved
);

HRESULT XMAPlaybackDestroy(XMAPLAYBACK *pPlayback);
HRESULT XMAPlaybackFlushData(XMAPLAYBACK *pPlayback, DWORD dwStream);

/* The rest are declared from XMAReader::Poll's call sites (retail addresses
   beside each); the bodies are anonymous in retail. */
HRESULT XMAPlaybackRequestModifyLock(XMAPLAYBACK *pPlayback); /* 0x82C11908 */
BOOL XMAPlaybackQueryModifyLockObtained(XMAPLAYBACK *pPlayback); /* 0x82C119A0 */
HRESULT XMAPlaybackResumePlayback(XMAPLAYBACK *pPlayback); /* 0x82C11AF0 */
DWORD XMAPlaybackGetErrorBits(XMAPLAYBACK *pPlayback, DWORD dwStream); /* 0x82C11F20 */
/* 0x82C11DC8: decoded samples ready on a stream, and where they start. */
DWORD XMAPlaybackQueryAvailableData(XMAPLAYBACK *pPlayback, DWORD dwStream, void **ppData);
/* 0x82C11C18 */
DWORD XMAPlaybackConsumeDecodedData(
    XMAPLAYBACK *pPlayback, DWORD dwStream, DWORD dwMaxSamples, void **ppData
);
BOOL XMAPlaybackQueryReadyForMoreData(XMAPLAYBACK *pPlayback, DWORD dwStream); /* 0x82C118A8 */
/* 0x82C11828: is this input buffer still queued on the stream? */
BOOL XMAPlaybackQueryInputDataPending(XMAPLAYBACK *pPlayback, DWORD dwStream, void *pBuffer);
/* 0x82C11FB8 */
HRESULT XMAPlaybackSubmitData(
    XMAPLAYBACK *pPlayback, DWORD dwStream, void *pBuffer, DWORD dwBufferSize
);

#ifdef __cplusplus
}
#endif
