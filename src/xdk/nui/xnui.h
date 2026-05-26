#pragma once
#include "../win_types.h"
#include "nuiskeleton.h"
#include "nuiidentity.h"

#ifdef __cplusplus
extern "C" {
#endif

// NUI Hardware Status
typedef struct _NUI_HARDWARE_STATUS {
    BOOL bConnected;
    BOOL bReady;
} NUI_HARDWARE_STATUS;

// Title-level NUI initialization
HRESULT XNuiTitleInitialize(DWORD dwFlags, DWORD dwHardwareThreadSkeleton);
HRESULT XNuiTitleShutdown();

// Frame interpretation and UI
HRESULT XNuiInterpretFrame(NUI_SKELETON_FRAME *pSkeletonFrame, DWORD dwFlags);
HRESULT XNuiGetHardwareStatus(NUI_HARDWARE_STATUS *pStatus);

// Identity management
HRESULT XNuiIdentityBindEnrollment(DWORD dwUserIndex, DWORD dwEnrollmentIndex, XOVERLAPPED *pOverlapped);
HRESULT XNuiIdentityCompleteEnroll(DWORD dwTrackingID, DWORD dwFlags, XOVERLAPPED *pOverlapped);
HRESULT XNuiIdentityCompleteIdentify(DWORD dwTrackingID, DWORD dwFlags, XOVERLAPPED *pOverlapped);
HRESULT XNuiIdentityCompleteQualityFlags(DWORD dwTrackingID, DWORD dwQualityFlags);
HRESULT XNuiIdentitySetNotifyCallback(NUI_IDENTITY_CALLBACK *pfnCallback, VOID *pvContext);
HRESULT XNuiIdentityUpdateData(DWORD dwTrackingID, DWORD dwFlags, XOVERLAPPED *pOverlapped);
HRESULT XNuiIdentityMatchOnTruthTable(DWORD dwTrackingID, DWORD dwFlags, BOOL *pfResult);

// Biometric cache
HRESULT XNuiCacheBiometricInvalidate(DWORD dwUserIndex, DWORD dwFlags, XOVERLAPPED *pOverlapped);
HRESULT XNuiCacheBiometricRetrieve(DWORD dwUserIndex, VOID *pvBuffer, DWORD cbBuffer, XOVERLAPPED *pOverlapped);
HRESULT XNuiCacheBiometricStore(DWORD dwUserIndex, CONST VOID *pvBuffer, DWORD cbBuffer);

// Session cache
HRESULT XNuiCacheSessionInvalidate(DWORD dwUserIndex, DWORD dwFlags, XOVERLAPPED *pOverlapped);
HRESULT XNuiCacheSessionRetrieve(DWORD dwUserIndex, VOID *pvBuffer, DWORD cbBuffer, XOVERLAPPED *pOverlapped);
HRESULT XNuiCacheSessionStore(DWORD dwUserIndex, CONST VOID *pvBuffer, DWORD cbBuffer);

#ifdef __cplusplus
}
#endif
