#pragma once
#include "../win_types.h"
#include "minwinbase.h"
#include "wtypesbase.h"

#ifdef __cplusplus
extern "C" {
#endif

VOID DebugBreak();
VOID OutputDebugStringA(LPCSTR lpOutputString);
VOID OutputDebugStringW(LPCWSTR lpOutputString);

#ifdef __cplusplus
}
#endif
