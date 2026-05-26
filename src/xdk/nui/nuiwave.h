#pragma once
#include "../win_types.h"

#ifdef __cplusplus
extern "C" {
#endif

HRESULT NuiWaveGetGestureOwnerProgress(DWORD *pdwTrackingID, FLOAT *pfWaveProgress);
HRESULT NuiWaveSetEnabled(BOOL bEnabled);

#ifdef __cplusplus
}
#endif
