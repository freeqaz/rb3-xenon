#pragma once
#include "xdk/win_types.h"

// XMA playback library (xmahal), as XMAReader uses it. Declared from the call
// sites: retail 0x82C11F60 tears a playback object down, 0x82C11E70 flushes one
// of its streams (it walks a 0x60-byte context per stream).
#ifdef __cplusplus
extern "C" {
#endif

typedef struct XMAPLAYBACK XMAPLAYBACK;

HRESULT XMAPlaybackDestroy(XMAPLAYBACK *pPlayback);
HRESULT XMAPlaybackFlushData(XMAPLAYBACK *pPlayback, DWORD dwStream);

#ifdef __cplusplus
}
#endif
