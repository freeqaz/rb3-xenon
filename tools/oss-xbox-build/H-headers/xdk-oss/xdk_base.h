/*
    RB3Enhanced xdk-oss - xdk_base.h
    Base Win32/Xbox scalar + handle typedefs shared by every group header.
    Self-contained (only stdint). Xbox 360: big-endian, ILP32, 4-byte long,
    2-byte wchar_t.
*/
#ifndef _XDK_OSS_BASE_H
#define _XDK_OSS_BASE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* calling conventions / attributes (no-ops on PPC target) */
#ifndef WINAPI
#define WINAPI  __stdcall
#endif
#ifndef APIENTRY
#define APIENTRY __stdcall
#endif
#ifndef CALLBACK
#define CALLBACK __stdcall
#endif
#ifndef WINAPIV
#define WINAPIV __cdecl
#endif
#ifndef CONST
#define CONST const
#endif
#ifndef IN
#define IN
#endif
#ifndef OUT
#define OUT
#endif
#ifndef OPTIONAL
#define OPTIONAL
#endif
#ifndef FAR
#define FAR
#endif
#ifndef NEAR
#define NEAR
#endif
#ifndef VOID
#define VOID void
#endif

/* wchar_t: cl.exe in C mode has no built-in wchar_t keyword */
#if !defined(__cplusplus) && !defined(_WCHAR_T_DEFINED)
typedef unsigned short wchar_t;
#define _WCHAR_T_DEFINED
#endif

/* Match LIBCMT's stddef.h (#define NULL 0) to avoid a redefinition warning
   when both headers are on the include path. */
#ifndef NULL
#define NULL 0
#endif

#ifndef TRUE
#define TRUE  1
#endif
#ifndef FALSE
#define FALSE 0
#endif

typedef void            *PVOID, *LPVOID;
typedef const void      *LPCVOID;

typedef int              BOOL, *PBOOL, *LPBOOL;
typedef unsigned char    BOOLEAN, *PBOOLEAN;
typedef unsigned char    BYTE, *PBYTE, *LPBYTE;
typedef unsigned char    UCHAR, *PUCHAR;
typedef unsigned char    UINT8;
typedef signed char      INT8;
typedef char             CCHAR;

typedef char             CHAR, *PCHAR, *PSTR, *LPSTR;
typedef const char      *PCSTR, *LPCSTR;

typedef short            SHORT, *PSHORT;
typedef unsigned short   USHORT, *PUSHORT;
typedef unsigned short   WORD, *PWORD, *LPWORD;
typedef short            INT16;
typedef unsigned short   UINT16;

typedef wchar_t          WCHAR, *PWCHAR, *PWSTR, *LPWSTR;
typedef const wchar_t   *PCWSTR, *LPCWSTR;
typedef unsigned short   WCHAR_T;

typedef int              INT, *PINT, *LPINT;
typedef int              INT32;
typedef unsigned int     UINT, *PUINT;
typedef unsigned int     UINT32;

typedef long             LONG, *PLONG, *LPLONG;
typedef unsigned long    ULONG, *PULONG;
typedef unsigned long    DWORD, *PDWORD, *LPDWORD;
typedef long             LONG32;
typedef unsigned long    ULONG32;

typedef float            FLOAT, *PFLOAT;

typedef long long          LONGLONG, *PLONGLONG;
typedef unsigned long long ULONGLONG, *PULONGLONG;
typedef long long          LONG64, INT64, *PLONG64, *PINT64;
typedef unsigned long long ULONG64, UINT64, *PULONG64, *PUINT64;
typedef long long          QWORD;

/* pointer-sized (ILP32) */
typedef int              INT_PTR, *PINT_PTR;
typedef unsigned int     UINT_PTR, *PUINT_PTR;
typedef long             LONG_PTR, *PLONG_PTR;
typedef unsigned long    ULONG_PTR, *PULONG_PTR;
typedef ULONG_PTR        DWORD_PTR, *PDWORD_PTR;
typedef ULONG_PTR        SIZE_T, *PSIZE_T;
typedef LONG_PTR         SSIZE_T, *PSSIZE_T;

typedef void            *HANDLE, **PHANDLE;
typedef HANDLE           HMODULE, HINSTANCE, HWND, HKEY, HLOCAL;
typedef DWORD            COLORREF;
typedef DWORD            LCID;
typedef WORD             ATOM;
typedef DWORD           *PDWORD_PTR;

/* Big-endian (Xenon) member order: the high word is FIRST in memory, so
 * HighPart is declared first — as in the real XDK winnt.h __BIG_ENDIAN__
 * variant. The little-endian order made GetFileSize return the HIGH half of
 * EndOfFile (= 0 for any file < 4 GB) on hardware (2026-07-18). */
typedef union _LARGE_INTEGER {
    struct { LONG HighPart; DWORD LowPart; } u;
    struct { LONG HighPart; DWORD LowPart; };
    LONGLONG QuadPart;
} LARGE_INTEGER, *PLARGE_INTEGER;

typedef union _ULARGE_INTEGER {
    struct { DWORD HighPart; DWORD LowPart; } u;
    struct { DWORD HighPart; DWORD LowPart; };
    ULONGLONG QuadPart;
} ULARGE_INTEGER, *PULARGE_INTEGER;

typedef struct _FILETIME {
    DWORD dwLowDateTime;
    DWORD dwHighDateTime;
} FILETIME, *PFILETIME, *LPFILETIME;

typedef struct _GUID {
    DWORD Data1;
    WORD  Data2;
    WORD  Data3;
    BYTE  Data4[8];
} GUID;

/* MAKEWORD/MAKELONG/LOWORD/HIWORD helpers */
#define MAKEWORD(a, b) ((WORD)(((BYTE)((DWORD)(a) & 0xff)) | ((WORD)((BYTE)((DWORD)(b) & 0xff))) << 8))
#define MAKELONG(a, b) ((LONG)(((WORD)((DWORD)(a) & 0xffff)) | ((DWORD)((WORD)((DWORD)(b) & 0xffff))) << 16))
#define LOWORD(l)      ((WORD)((DWORD)(l) & 0xffff))
#define HIWORD(l)      ((WORD)((DWORD)(l) >> 16))
#define LOBYTE(w)      ((BYTE)((DWORD)(w) & 0xff))
#define HIBYTE(w)      ((BYTE)((DWORD)(w) >> 8))

/* common Win32 return / error codes used by RB3E */
#define ERROR_SUCCESS            0L
#define ERROR_FILE_NOT_FOUND     2L
#define ERROR_PATH_NOT_FOUND     3L
#define ERROR_ACCESS_DENIED      5L
#define ERROR_INVALID_HANDLE     6L
#define ERROR_NOT_READY          21L
#define ERROR_DEVICE_NOT_CONNECTED 1167L
#define ERROR_NO_SUCH_DEVICE     1617L
#define ERROR_IO_PENDING         997L
#define ERROR_IO_INCOMPLETE      996L

#define INVALID_HANDLE_VALUE     ((HANDLE)(LONG_PTR)-1)
#define MAX_PATH                 260

#ifdef __cplusplus
}
#endif

#endif /* _XDK_OSS_BASE_H */
