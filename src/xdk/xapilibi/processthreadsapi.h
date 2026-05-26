#pragma once
#include "../win_types.h"
#include "minwinbase.h"
#include "wtypesbase.h"

#ifdef __cplusplus
extern "C" {
#endif

DWORD ThreadProc(LPVOID lpParameter);
typedef DWORD LPTHREAD_START_ROUTINE(LPVOID);

HANDLE CreateThread(
    LPSECURITY_ATTRIBUTES lpThreadAttributes,
    SIZE_T dwStackSize,
    LPTHREAD_START_ROUTINE lpStartAddress,
    LPVOID lpParameter,
    DWORD dwCreationFlags,
    LPDWORD lpThreadId
);
VOID ExitThread(DWORD dwExitCode);
DWORD GetCurrentThreadId();
BOOL GetExitCodeThread(HANDLE hThread, LPDWORD lpExitCode);
int GetThreadPriority(HANDLE hThread);
HANDLE OpenThread(DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwThreadId);
DWORD ResumeThread(HANDLE hThread);
BOOL SetThreadPriority(HANDLE hThread, int nPriority);
BOOL SwitchToThread();
DWORD TlsAlloc();
BOOL TlsFree(DWORD dwTlsIndex);
LPVOID TlsGetValue(DWORD dwTlsIndex);
BOOL TlsSetValue(DWORD dwTlsIndex, LPVOID lpTlsValue);

#ifdef __cplusplus
}
#endif
