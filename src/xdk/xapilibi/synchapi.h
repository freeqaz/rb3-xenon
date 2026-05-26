#pragma once
#include "../win_types.h"
#include "minwinbase.h"
#include "wtypesbase.h"

#ifdef __cplusplus
extern "C" {
#endif

VOID TimerAPCProc(
    LPVOID lpArgToCompletionRoutine, DWORD dwTimerLowValue, DWORD dwTimerHighValue
);

typedef VOID PTIMERAPCROUTINE(LPVOID, DWORD, DWORD);

BOOL CancelWaitableTimer(HANDLE hTimer);
HANDLE CreateEventA(
    LPSECURITY_ATTRIBUTES lpEventAttributes,
    BOOL bManualReset,
    BOOL bInitialState,
    LPCSTR lpName
);
HANDLE
CreateMutexA(LPSECURITY_ATTRIBUTES lpMutexAttributes, BOOL bInitialOwner, LPCSTR lpName);
BOOL ReleaseMutex(HANDLE hMutex);
BOOL ReleaseSemaphore(HANDLE hSemaphore, LONG lReleaseCount, LPLONG lpPreviousCount);
BOOL ResetEvent(HANDLE hEvent);
BOOL SetEvent(HANDLE hEvent);
BOOL SetWaitableTimer(
    HANDLE hTimer,
    const LARGE_INTEGER *lpDueTime,
    LONG lPeriod,
    PTIMERAPCROUTINE pfnCompletionRoutine,
    LPVOID lpArgToCompletionRoutine,
    BOOL fResume
);
VOID Sleep(DWORD dwMilliseconds);
DWORD SleepEx(DWORD dwMilliseconds, BOOL bAlertable);
DWORD WaitForMultipleObjects(
    DWORD nCount, const HANDLE *lpHandles, BOOL bWaitAll, DWORD dwMilliseconds
);
DWORD WaitForMultipleObjectsEx(
    DWORD nCount,
    const HANDLE *lpHandles,
    BOOL bWaitAll,
    DWORD dwMilliseconds,
    BOOL bAlertable
);
DWORD WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds);
DWORD WaitForSingleObjectEx(HANDLE hHandle, DWORD dwMilliseconds, BOOL bAlertable);

#ifdef __cplusplus
}
#endif
