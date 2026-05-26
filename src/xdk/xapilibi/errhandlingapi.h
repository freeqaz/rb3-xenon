#pragma once
#include "../win_types.h"
#include "minwinbase.h"
#include "wtypesbase.h"
#include "winnt.h"

#ifdef __cplusplus
extern "C" {
#endif

DWORD GetLastError();
VOID RaiseException(
    DWORD dwExceptionCode,
    DWORD dwExceptionFlags,
    DWORD nNumberOfArguments,
    const ULONG_PTR *lpArguments
);
VOID SetLastError(DWORD dwErrCode);

LPTOP_LEVEL_EXCEPTION_FILTER
SetUnhandledExceptionFilter(LPTOP_LEVEL_EXCEPTION_FILTER lpTopLevelExceptionFilter);

LONG UnhandledExceptionFilter(_EXCEPTION_POINTERS *ExceptionInfo);

#ifdef __cplusplus
}
#endif
