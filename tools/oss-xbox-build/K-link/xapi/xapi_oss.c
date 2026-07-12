/*
    xapi_oss.c  --  Strategy B, Lane A
    XDK-free reconstruction of the XDK "xapilib" Win32->Nt shim layer.

    The stock RB3Enhanced.dll statically linked xapilib, which implements a
    handful of Win32 file / thread / string helpers on top of the Xbox 360
    xboxkrnl Nt/Ex/Ke/Rtl primitives.  Our from-source build lacks that
    static lib, so RB3E's own TUs (xbox360_files.c, xbox360_exceptions.c,
    xbox_keyboard.c, net_liveless_online.c) reference the Win32 names below
    with nothing to resolve them.  This TU provides them.

    SELF-CONTAINED BY DESIGN:  to avoid a race with Lane H's concurrent header
    edits, this file includes NONE of the include/xdk-oss tree.  Every Win32
    scalar type, the Win32 function signatures (ABI-identical to xdk-oss/xapi.h
    + xdk-oss/xboxkrnl.h so callers bind correctly), and the underlying
    Nt/Ex/Ke/Rtl prototypes + minimal kernel structs are declared locally.
    Struct layouts verified against Xenia (xbox.h / kernel/info/file.h /
    kernel/xfile.h) and the NT/Xbox-360 kernel ABI.

    Only <stdarg.h> (CRT, not xdk-oss) is pulled in, for wsprintfW's va_list.
    memset/memcpy are freestanding-CRT symbols resolved by Lane C's crt.obj.

    Compile:  cl.exe -c ... -TC  ->  xapi_oss.obj   (machine 0x01F2 / PPCBE)
*/

#include <stdarg.h>   /* va_list / va_start / va_end (MSVC intrinsic lowering) */

/* memset/memcpy: freestanding CRT, provided by crt.obj (Lane C). */
void *memset(void *dst, int c, unsigned int n);
void *memcpy(void *dst, const void *src, unsigned int n);

/* ------------------------------------------------------------------ *
 *  Win32 scalar / handle typedefs  (ILP32, big-endian, 4-byte long)  *
 *  ABI-identical to xdk-oss/xdk_base.h.                              *
 * ------------------------------------------------------------------ */
typedef unsigned long   DWORD;
typedef unsigned long   ULONG;
typedef long            LONG;
typedef int             BOOL;
typedef int             INT;
typedef unsigned int    UINT;
typedef unsigned short  USHORT;
typedef unsigned short  WCHAR;      /* 2-byte on this target */
typedef unsigned char   UCHAR;
typedef unsigned char   BOOLEAN;
typedef char            CHAR;
typedef void            VOID;
typedef void           *PVOID;
typedef void           *HANDLE;
typedef HANDLE         *PHANDLE;
typedef char           *LPSTR;
typedef const char     *LPCSTR;
typedef WCHAR          *LPWSTR;
typedef const WCHAR    *LPCWSTR;
typedef void           *LPVOID;
typedef const void     *LPCVOID;
typedef DWORD          *LPDWORD;
typedef LONG           *PLONG;
typedef BOOL           *LPBOOL;
typedef unsigned long   SIZE_T;
typedef long long           LONGLONG;
typedef unsigned long long  ULONGLONG;
typedef long            NTSTATUS;

#define OSS_NULL   ((void *)0)
#define TRUE       1
#define FALSE      0

typedef union _LARGE_INTEGER {
    struct { DWORD LowPart; LONG HighPart; } u;
    LONGLONG QuadPart;
} LARGE_INTEGER, *PLARGE_INTEGER;

typedef struct _FILETIME {
    DWORD dwLowDateTime;
    DWORD dwHighDateTime;
} FILETIME;

typedef struct _SYSTEMTIME {
    USHORT wYear;
    USHORT wMonth;
    USHORT wDayOfWeek;
    USHORT wDay;
    USHORT wHour;
    USHORT wMinute;
    USHORT wSecond;
    USHORT wMilliseconds;
} SYSTEMTIME, *LPSYSTEMTIME;

/* WIN32_FIND_DATAA -- MUST match xdk-oss/xapi.h field layout exactly, since
   the caller (xbox360_files.c) allocates it and reads .dwFileAttributes /
   .cFileName from the offsets that header defines. */
#define OSS_MAX_PATH 260
typedef struct _WIN32_FIND_DATAA {
    DWORD    dwFileAttributes;
    FILETIME ftCreationTime;
    FILETIME ftLastAccessTime;
    FILETIME ftLastWriteTime;
    DWORD    nFileSizeHigh;
    DWORD    nFileSizeLow;
    DWORD    dwReserved0;
    DWORD    dwReserved1;
    CHAR     cFileName[OSS_MAX_PATH];
    CHAR     cAlternateFileName[14];
} WIN32_FIND_DATAA, *LPWIN32_FIND_DATAA;

/* ---- Win32 access / disposition / attribute constants (xapi.h) ---- */
#define GENERIC_READ            0x80000000UL
#define GENERIC_WRITE           0x40000000UL
#define FILE_SHARE_READ         0x00000001UL
#define CREATE_NEW              1
#define CREATE_ALWAYS           2
#define OPEN_EXISTING           3
#define OPEN_ALWAYS             4
#define TRUNCATE_EXISTING       5
#define FILE_BEGIN              0
#define FILE_CURRENT            1
#define FILE_END                2
#define FILE_ATTRIBUTE_NORMAL   0x00000080UL
#define INVALID_FILE_ATTRIBUTES ((DWORD)-1)
#define INVALID_FILE_SIZE       ((DWORD)-1)
#define INVALID_SET_FILE_POINTER ((DWORD)-1)
#define CREATE_SUSPENDED        0x00000004UL
#define INVALID_HANDLE_VALUE    ((HANDLE)(LONG)-1)

/* ------------------------------------------------------------------ *
 *  Xbox 360 kernel (xboxkrnl) types + prototypes  (self-declared).   *
 * ------------------------------------------------------------------ */

/* Xbox object names are ANSI (OBJECT_STRING == ANSI_STRING). */
typedef struct _ANSI_STRING {
    USHORT Length;          /* bytes, excluding NUL */
    USHORT MaximumLength;   /* bytes, including NUL */
    LPSTR  Buffer;
} ANSI_STRING, *PANSI_STRING;

/* Xbox 360 OBJECT_ATTRIBUTES is the 3-field ANSI variant (NOT desktop NT's
   6-field UNICODE form). Verified: Xenia xbox.h X_OBJECT_ATTRIBUTES. */
typedef struct _OBJECT_ATTRIBUTES {
    HANDLE       RootDirectory;   /* 0x00 */
    PANSI_STRING ObjectName;      /* 0x04 */
    ULONG        Attributes;      /* 0x08 */
} OBJECT_ATTRIBUTES, *POBJECT_ATTRIBUTES;

typedef struct _IO_STATUS_BLOCK {
    union { NTSTATUS Status; PVOID Pointer; } u;
    ULONG Information;
} IO_STATUS_BLOCK, *PIO_STATUS_BLOCK;

/* FileInformationClass values (NT/Xbox shared). */
#define FileStandardInformation     5
#define FilePositionInformation     14
#define FileEndOfFileInformation    20

typedef struct _FILE_STANDARD_INFORMATION {
    LARGE_INTEGER AllocationSize;   /* 0x00 */
    LARGE_INTEGER EndOfFile;        /* 0x08 */
    ULONG         NumberOfLinks;    /* 0x10 */
    BOOLEAN       DeletePending;    /* 0x14 */
    BOOLEAN       Directory;        /* 0x15 */
} FILE_STANDARD_INFORMATION;

typedef struct _FILE_POSITION_INFORMATION {
    LARGE_INTEGER CurrentByteOffset;
} FILE_POSITION_INFORMATION;

/* Xenia: X_FILE_NETWORK_OPEN_INFORMATION (56 bytes, attributes @0x30). */
typedef struct _FILE_NETWORK_OPEN_INFORMATION {
    LARGE_INTEGER CreationTime;     /* 0x00 */
    LARGE_INTEGER LastAccessTime;   /* 0x08 */
    LARGE_INTEGER LastWriteTime;    /* 0x10 */
    LARGE_INTEGER ChangeTime;       /* 0x18 */
    LARGE_INTEGER AllocationSize;   /* 0x20 */
    LARGE_INTEGER EndOfFile;        /* 0x28 */
    ULONG         FileAttributes;   /* 0x30 */
    ULONG         Pad;              /* 0x34 */
} FILE_NETWORK_OPEN_INFORMATION;

/* Xenia: X_FILE_DIRECTORY_INFORMATION.  Xbox NtQueryDirectoryFile returns a
   SINGLE entry per call and the name is ANSI (1 byte/char). */
typedef struct _FILE_DIRECTORY_INFORMATION {
    ULONG         NextEntryOffset;  /* 0x00 */
    ULONG         FileIndex;        /* 0x04 */
    LARGE_INTEGER CreationTime;     /* 0x08 */
    LARGE_INTEGER LastAccessTime;   /* 0x10 */
    LARGE_INTEGER LastWriteTime;    /* 0x18 */
    LARGE_INTEGER ChangeTime;       /* 0x20 */
    LARGE_INTEGER EndOfFile;        /* 0x28 */
    LARGE_INTEGER AllocationSize;   /* 0x30 */
    ULONG         FileAttributes;   /* 0x38 */
    ULONG         FileNameLength;   /* 0x3C  bytes */
    CHAR          FileName[1];      /* 0x40 */
} FILE_DIRECTORY_INFORMATION;

/* NT status success test (sign bit clear). */
#define NT_SUCCESS(s)  ((NTSTATUS)(s) >= 0)

/* NtCreateFile CreateDisposition values. */
#define FILE_SUPERSEDE      0
#define FILE_OPEN           1
#define FILE_CREATE         2
#define FILE_OPEN_IF        3
#define FILE_OVERWRITE      4
#define FILE_OVERWRITE_IF   5

/* NtCreateFile / NtOpenFile CreateOptions. */
#define FILE_DIRECTORY_FILE         0x00000001UL
#define FILE_SYNCHRONOUS_IO_NONALERT 0x00000020UL
#define FILE_NON_DIRECTORY_FILE     0x00000040UL

/* DesiredAccess bits. */
#define SYNCHRONIZE          0x00100000UL
#define FILE_LIST_DIRECTORY  0x00000001UL

/* OBJECT_ATTRIBUTES.Attributes */
#define OBJ_CASE_INSENSITIVE 0x00000040UL

/* ---- underlying kernel imports (resolved by xboxkrnl.lib / Lane I) ---- */
NTSTATUS NtCreateFile(PHANDLE FileHandle, ULONG DesiredAccess,
                      POBJECT_ATTRIBUTES ObjectAttributes, PIO_STATUS_BLOCK IoStatusBlock,
                      PLARGE_INTEGER AllocationSize, ULONG FileAttributes,
                      ULONG ShareAccess, ULONG CreateDisposition, ULONG CreateOptions);
NTSTATUS NtOpenFile(PHANDLE FileHandle, ULONG DesiredAccess,
                    POBJECT_ATTRIBUTES ObjectAttributes, PIO_STATUS_BLOCK IoStatusBlock,
                    ULONG ShareAccess, ULONG OpenOptions);
NTSTATUS NtReadFile(HANDLE FileHandle, HANDLE Event, PVOID ApcRoutine, PVOID ApcContext,
                    PIO_STATUS_BLOCK IoStatusBlock, PVOID Buffer, ULONG Length,
                    PLARGE_INTEGER ByteOffset);
NTSTATUS NtWriteFile(HANDLE FileHandle, HANDLE Event, PVOID ApcRoutine, PVOID ApcContext,
                     PIO_STATUS_BLOCK IoStatusBlock, PVOID Buffer, ULONG Length,
                     PLARGE_INTEGER ByteOffset);
NTSTATUS NtClose(HANDLE Handle);
NTSTATUS NtQueryInformationFile(HANDLE FileHandle, PIO_STATUS_BLOCK IoStatusBlock,
                                PVOID FileInformation, ULONG Length,
                                ULONG FileInformationClass);
NTSTATUS NtSetInformationFile(HANDLE FileHandle, PIO_STATUS_BLOCK IoStatusBlock,
                              PVOID FileInformation, ULONG Length,
                              ULONG FileInformationClass);
NTSTATUS NtQueryFullAttributesFile(POBJECT_ATTRIBUTES ObjectAttributes,
                                   FILE_NETWORK_OPEN_INFORMATION *Attributes);
/* Xbox NtQueryDirectoryFile: single-entry variant, ANSI file mask, no
   FileInformationClass / ReturnSingleEntry args (Xenia-confirmed). */
NTSTATUS NtQueryDirectoryFile(HANDLE FileHandle, HANDLE Event, PVOID ApcRoutine,
                              PVOID ApcContext, PIO_STATUS_BLOCK IoStatusBlock,
                              PVOID FileInformation, ULONG Length,
                              PANSI_STRING FileName, BOOLEAN RestartScan);
NTSTATUS ExCreateThread(PHANDLE Handle, ULONG StackSize, ULONG *ThreadId,
                        PVOID XapiThreadStartup, PVOID StartAddress,
                        PVOID StartContext, ULONG CreationFlags);
VOID     ExTerminateThread(ULONG ExitCode);
NTSTATUS KeDelayExecutionThread(UCHAR WaitMode, UCHAR Alertable, PLARGE_INTEGER Interval);
VOID     KeQuerySystemTime(PLARGE_INTEGER CurrentTime);
ULONG    RtlNtStatusToDosError(NTSTATUS Status);
NTSTATUS RtlUnicodeToMultiByteN(LPSTR MultiByteString, ULONG MaxBytesInMultiByteString,
                                ULONG *BytesInMultiByteString, LPCWSTR UnicodeString,
                                ULONG BytesInUnicodeString);
int      vswprintf(WCHAR *buffer, const WCHAR *format, va_list argptr);

/* ------------------------------------------------------------------ *
 *  Last-error storage.                                               *
 *  NOTE: process-global, not per-thread.  Sufficient for the         *
 *  single-threaded first-boot config-load path (the only consumer,   *
 *  RB3E_FileExists, runs during startup before any RB3E thread is    *
 *  spawned).  A KeTls*-backed per-thread upgrade is a follow-up.     *
 * ------------------------------------------------------------------ */
static DWORD g_oss_last_error = 0;
static void oss_set_last_error_status(NTSTATUS s) {
    g_oss_last_error = RtlNtStatusToDosError(s);
}

/* ------------------------------------------------------------------ *
 *  Helpers.                                                          *
 * ------------------------------------------------------------------ */
static unsigned int oss_strlen(const char *s) {
    const char *p = s;
    while (*p) p++;
    return (unsigned int)(p - s);
}

/* Build an absolute Xbox object path + ANSI_STRING from a Win32 path.
   Drive-relative names ("GAME:\..", "RB3HDD:\..") are prefixed with "\??\"
   to hit the DosDevices symlinks RB3E creates via ObCreateSymbolicLink
   ("\??\RB3HDD:" -> device).  Names already starting with '\' (e.g.
   "\Device\..") are passed through. */
static void oss_build_objname(const char *win32path, char *outbuf, unsigned int outcap,
                              ANSI_STRING *as) {
    unsigned int n = 0;
    const char *p = win32path;
    if (win32path[0] != '\\') {
        outbuf[n++] = '\\'; outbuf[n++] = '?'; outbuf[n++] = '?'; outbuf[n++] = '\\';
    }
    while (*p && n + 1 < outcap) outbuf[n++] = *p++;
    outbuf[n] = '\0';
    as->Buffer = outbuf;
    as->Length = (USHORT)n;
    as->MaximumLength = (USHORT)(n + 1);
}

/* 100ns-since-1601 UTC -> SYSTEMTIME (self-contained Gregorian conversion,
   avoids a RtlTimeToTimeFields import). */
static void oss_filetime_to_systemtime(LONGLONG ft, SYSTEMTIME *st) {
    /* days & remainder */
    LONGLONG secs = ft / 10000000LL;
    ULONG    ms   = (ULONG)((ft / 10000LL) % 1000LL);
    LONGLONG days = secs / 86400LL;
    ULONG    rem  = (ULONG)(secs % 86400LL);
    ULONG    y, mo;
    static const USHORT mdays[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    st->wHour = (USHORT)(rem / 3600);
    st->wMinute = (USHORT)((rem % 3600) / 60);
    st->wSecond = (USHORT)(rem % 60);
    st->wMilliseconds = (USHORT)ms;
    st->wDayOfWeek = (USHORT)((days + 1) % 7);   /* 1601-01-01 was a Monday */
    y = 1601;
    for (;;) {
        int leap = ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0);
        ULONG ydays = leap ? 366 : 365;
        if (days < (LONGLONG)ydays) break;
        days -= ydays; y++;
    }
    st->wYear = (USHORT)y;
    for (mo = 0; mo < 12; mo++) {
        ULONG dm = mdays[mo];
        if (mo == 1 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) dm = 29;
        if (days < (LONGLONG)dm) break;
        days -= dm;
    }
    st->wMonth = (USHORT)(mo + 1);
    st->wDay = (USHORT)(days + 1);
}

/* ================================================================== *
 *  Win32 file API.                                                   *
 * ================================================================== */
HANDLE CreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
                   LPVOID lpSecurityAttributes, DWORD dwCreationDisposition,
                   DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) {
    char objbuf[OSS_MAX_PATH + 8];
    ANSI_STRING as;
    OBJECT_ATTRIBUTES oa;
    IO_STATUS_BLOCK iosb;
    HANDLE h = OSS_NULL;
    NTSTATUS s;
    ULONG ntDisp;
    (void)lpSecurityAttributes; (void)dwFlagsAndAttributes; (void)hTemplateFile;

    oss_build_objname(lpFileName, objbuf, sizeof(objbuf), &as);
    oa.RootDirectory = OSS_NULL;
    oa.ObjectName = &as;
    oa.Attributes = OBJ_CASE_INSENSITIVE;

    switch (dwCreationDisposition) {
    case CREATE_NEW:         ntDisp = FILE_CREATE;       break;
    case CREATE_ALWAYS:      ntDisp = FILE_OVERWRITE_IF; break;
    case OPEN_EXISTING:      ntDisp = FILE_OPEN;         break;
    case OPEN_ALWAYS:        ntDisp = FILE_OPEN_IF;      break;
    case TRUNCATE_EXISTING:  ntDisp = FILE_OVERWRITE;    break;
    default:                 ntDisp = FILE_OPEN;         break;
    }

    s = NtCreateFile(&h, dwDesiredAccess | SYNCHRONIZE, &oa, &iosb,
                     OSS_NULL, FILE_ATTRIBUTE_NORMAL, dwShareMode, ntDisp,
                     FILE_SYNCHRONOUS_IO_NONALERT | FILE_NON_DIRECTORY_FILE);
    if (!NT_SUCCESS(s)) {
        oss_set_last_error_status(s);
        return INVALID_HANDLE_VALUE;
    }
    return h;
}

BOOL ReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead,
              LPDWORD lpNumberOfBytesRead, LPVOID lpOverlapped) {
    IO_STATUS_BLOCK iosb;
    NTSTATUS s;
    (void)lpOverlapped;
    iosb.Information = 0;
    s = NtReadFile(hFile, OSS_NULL, OSS_NULL, OSS_NULL, &iosb,
                   lpBuffer, nNumberOfBytesToRead, OSS_NULL);
    if (lpNumberOfBytesRead) *lpNumberOfBytesRead = (DWORD)iosb.Information;
    if (!NT_SUCCESS(s)) { oss_set_last_error_status(s); return FALSE; }
    return TRUE;
}

BOOL WriteFile(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite,
               LPDWORD lpNumberOfBytesWritten, LPVOID lpOverlapped) {
    IO_STATUS_BLOCK iosb;
    NTSTATUS s;
    (void)lpOverlapped;
    iosb.Information = 0;
    s = NtWriteFile(hFile, OSS_NULL, OSS_NULL, OSS_NULL, &iosb,
                    (PVOID)lpBuffer, nNumberOfBytesToWrite, OSS_NULL);
    if (lpNumberOfBytesWritten) *lpNumberOfBytesWritten = (DWORD)iosb.Information;
    if (!NT_SUCCESS(s)) { oss_set_last_error_status(s); return FALSE; }
    return TRUE;
}

BOOL CloseHandle(HANDLE hObject) {
    NTSTATUS s = NtClose(hObject);
    if (!NT_SUCCESS(s)) { oss_set_last_error_status(s); return FALSE; }
    return TRUE;
}

DWORD GetFileSize(HANDLE hFile, LPDWORD lpFileSizeHigh) {
    FILE_STANDARD_INFORMATION fsi;
    IO_STATUS_BLOCK iosb;
    NTSTATUS s = NtQueryInformationFile(hFile, &iosb, &fsi, sizeof(fsi),
                                        FileStandardInformation);
    if (!NT_SUCCESS(s)) {
        oss_set_last_error_status(s);
        if (lpFileSizeHigh) *lpFileSizeHigh = 0;
        return INVALID_FILE_SIZE;
    }
    if (lpFileSizeHigh) *lpFileSizeHigh = (DWORD)fsi.EndOfFile.u.HighPart;
    return (DWORD)fsi.EndOfFile.u.LowPart;
}

DWORD SetFilePointer(HANDLE hFile, LONG lDistanceToMove, PLONG lpDistanceToMoveHigh,
                     DWORD dwMoveMethod) {
    FILE_POSITION_INFORMATION fpi;
    IO_STATUS_BLOCK iosb;
    NTSTATUS s;
    LARGE_INTEGER base;
    LARGE_INTEGER target;

    base.QuadPart = 0;
    if (dwMoveMethod == FILE_CURRENT) {
        FILE_POSITION_INFORMATION cur;
        s = NtQueryInformationFile(hFile, &iosb, &cur, sizeof(cur),
                                   FilePositionInformation);
        if (!NT_SUCCESS(s)) { oss_set_last_error_status(s); return INVALID_SET_FILE_POINTER; }
        base.QuadPart = cur.CurrentByteOffset.QuadPart;
    } else if (dwMoveMethod == FILE_END) {
        FILE_STANDARD_INFORMATION fsi;
        s = NtQueryInformationFile(hFile, &iosb, &fsi, sizeof(fsi),
                                   FileStandardInformation);
        if (!NT_SUCCESS(s)) { oss_set_last_error_status(s); return INVALID_SET_FILE_POINTER; }
        base.QuadPart = fsi.EndOfFile.QuadPart;
    } /* else FILE_BEGIN: base = 0 */

    if (lpDistanceToMoveHigh) {
        LONGLONG delta = ((LONGLONG)(*lpDistanceToMoveHigh) << 32) |
                         (LONGLONG)((ULONG)lDistanceToMove);
        target.QuadPart = base.QuadPart + delta;
    } else {
        /* single 32-bit signed distance */
        target.QuadPart = base.QuadPart + (LONGLONG)lDistanceToMove;
    }

    fpi.CurrentByteOffset.QuadPart = target.QuadPart;
    s = NtSetInformationFile(hFile, &iosb, &fpi, sizeof(fpi),
                             FilePositionInformation);
    if (!NT_SUCCESS(s)) { oss_set_last_error_status(s); return INVALID_SET_FILE_POINTER; }
    if (lpDistanceToMoveHigh) *lpDistanceToMoveHigh = (LONG)target.u.HighPart;
    return (DWORD)target.u.LowPart;
}

DWORD GetFileAttributesA(LPCSTR lpFileName) {
    char objbuf[OSS_MAX_PATH + 8];
    ANSI_STRING as;
    OBJECT_ATTRIBUTES oa;
    FILE_NETWORK_OPEN_INFORMATION info;
    NTSTATUS s;

    oss_build_objname(lpFileName, objbuf, sizeof(objbuf), &as);
    oa.RootDirectory = OSS_NULL;
    oa.ObjectName = &as;
    oa.Attributes = OBJ_CASE_INSENSITIVE;

    s = NtQueryFullAttributesFile(&oa, &info);
    if (!NT_SUCCESS(s)) {
        oss_set_last_error_status(s);
        return INVALID_FILE_ATTRIBUTES;
    }
    g_oss_last_error = 0;
    return (DWORD)info.FileAttributes;
}

/* ---- directory enumeration ---- */
typedef struct _OSS_FIND {
    int         inUse;
    HANDLE      dirHandle;
    ANSI_STRING mask;
    char        maskbuf[64];
    unsigned char qbuf[sizeof(FILE_DIRECTORY_INFORMATION) + 512];
} OSS_FIND;
static OSS_FIND g_oss_finds[8];

static OSS_FIND *oss_find_alloc(void) {
    int i;
    for (i = 0; i < (int)(sizeof(g_oss_finds) / sizeof(g_oss_finds[0])); i++) {
        if (!g_oss_finds[i].inUse) {
            g_oss_finds[i].inUse = 1;
            return &g_oss_finds[i];
        }
    }
    return OSS_NULL;
}

static BOOL oss_find_query(OSS_FIND *f, LPWIN32_FIND_DATAA data, BOOLEAN restart) {
    IO_STATUS_BLOCK iosb;
    FILE_DIRECTORY_INFORMATION *di = (FILE_DIRECTORY_INFORMATION *)f->qbuf;
    NTSTATUS s;
    ULONG n;

    s = NtQueryDirectoryFile(f->dirHandle, OSS_NULL, OSS_NULL, OSS_NULL, &iosb,
                             f->qbuf, sizeof(f->qbuf), &f->mask, restart);
    if (!NT_SUCCESS(s)) {           /* STATUS_NO_MORE_FILES ends the walk */
        oss_set_last_error_status(s);
        return FALSE;
    }
    memset(data, 0, sizeof(*data));
    data->dwFileAttributes = (DWORD)di->FileAttributes;
    data->nFileSizeLow  = (DWORD)di->EndOfFile.u.LowPart;
    data->nFileSizeHigh = (DWORD)di->EndOfFile.u.HighPart;
    n = di->FileNameLength;
    if (n > OSS_MAX_PATH - 1) n = OSS_MAX_PATH - 1;
    memcpy(data->cFileName, di->FileName, n);
    data->cFileName[n] = '\0';
    return TRUE;
}

HANDLE FindFirstFileA(LPCSTR lpFileName, LPWIN32_FIND_DATAA lpFindFileData) {
    char objbuf[OSS_MAX_PATH + 8];
    ANSI_STRING dirAs;
    OBJECT_ATTRIBUTES oa;
    IO_STATUS_BLOCK iosb;
    OSS_FIND *f;
    NTSTATUS s;
    int len, sep, i;
    char dirpart[OSS_MAX_PATH];

    f = oss_find_alloc();
    if (!f) return INVALID_HANDLE_VALUE;

    /* split "<dir>\<mask>" at the last backslash. */
    len = (int)oss_strlen(lpFileName);
    sep = -1;
    for (i = 0; i < len; i++)
        if (lpFileName[i] == '\\') sep = i;
    if (sep < 0) {                     /* no dir component: mask against "." */
        dirpart[0] = '.'; dirpart[1] = '\0';
        f->maskbuf[0] = '\0';
        for (i = 0; i < len && i < (int)sizeof(f->maskbuf) - 1; i++) f->maskbuf[i] = lpFileName[i];
        f->maskbuf[i] = '\0';
    } else {
        for (i = 0; i < sep && i < (int)sizeof(dirpart) - 1; i++) dirpart[i] = lpFileName[i];
        dirpart[i] = '\0';
        { int j = 0; for (i = sep + 1; i < len && j < (int)sizeof(f->maskbuf) - 1; i++) f->maskbuf[j++] = lpFileName[i]; f->maskbuf[j] = '\0'; }
    }
    f->mask.Buffer = f->maskbuf;
    f->mask.Length = (USHORT)oss_strlen(f->maskbuf);
    f->mask.MaximumLength = (USHORT)(f->mask.Length + 1);

    oss_build_objname(dirpart, objbuf, sizeof(objbuf), &dirAs);
    oa.RootDirectory = OSS_NULL;
    oa.ObjectName = &dirAs;
    oa.Attributes = OBJ_CASE_INSENSITIVE;

    s = NtOpenFile(&f->dirHandle, FILE_LIST_DIRECTORY | SYNCHRONIZE, &oa, &iosb,
                   FILE_SHARE_READ, FILE_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT);
    if (!NT_SUCCESS(s)) {
        oss_set_last_error_status(s);
        f->inUse = 0;
        return INVALID_HANDLE_VALUE;
    }
    if (!oss_find_query(f, lpFindFileData, TRUE)) {
        NtClose(f->dirHandle);
        f->inUse = 0;
        return INVALID_HANDLE_VALUE;
    }
    return (HANDLE)f;
}

BOOL FindNextFileA(HANDLE hFindFile, LPWIN32_FIND_DATAA lpFindFileData) {
    OSS_FIND *f = (OSS_FIND *)hFindFile;
    if (!f || !f->inUse) return FALSE;
    return oss_find_query(f, lpFindFileData, FALSE);
}

BOOL FindClose(HANDLE hFindFile) {
    OSS_FIND *f = (OSS_FIND *)hFindFile;
    if (!f || !f->inUse) return FALSE;
    NtClose(f->dirHandle);
    f->inUse = 0;
    return TRUE;
}

/* ================================================================== *
 *  Thread / time.                                                    *
 * ================================================================== */
/* Kernel calls XapiThreadStartup(StartAddress, StartContext); run the user
   routine, then ExTerminateThread so returning from it is well-defined. */
static VOID oss_thread_startup(PVOID startAddress, PVOID startContext) {
    DWORD (*fn)(PVOID) = (DWORD (*)(PVOID))startAddress;
    DWORD rc = fn(startContext);
    ExTerminateThread(rc);
}

HANDLE CreateThread(LPVOID lpThreadAttributes, SIZE_T dwStackSize,
                    PVOID lpStartAddress, LPVOID lpParameter,
                    DWORD dwCreationFlags, LPDWORD lpThreadId) {
    HANDLE h = OSS_NULL;
    ULONG localTid = 0;
    NTSTATUS s;
    (void)lpThreadAttributes;
    s = ExCreateThread(&h, (ULONG)dwStackSize,
                       lpThreadId ? (ULONG *)lpThreadId : &localTid,
                       (PVOID)oss_thread_startup, lpStartAddress, lpParameter,
                       (dwCreationFlags & CREATE_SUSPENDED) ? 1u : 0u);
    if (!NT_SUCCESS(s)) { oss_set_last_error_status(s); return OSS_NULL; }
    return h;
}

VOID Sleep(DWORD dwMilliseconds) {
    LARGE_INTEGER interval;
    /* relative 100ns units (negative); UserMode, non-alertable */
    interval.QuadPart = -(LONGLONG)((ULONGLONG)dwMilliseconds * 10000ULL);
    KeDelayExecutionThread((UCHAR)1, (UCHAR)0, &interval);
}

VOID GetSystemTime(LPSYSTEMTIME lpSystemTime) {
    LARGE_INTEGER now;
    KeQuerySystemTime(&now);
    oss_filetime_to_systemtime(now.QuadPart, lpSystemTime);
}

DWORD GetLastError(void) {
    return g_oss_last_error;
}

VOID SetLastError(DWORD dwErrCode) {
    g_oss_last_error = dwErrCode;
}

/* ================================================================== *
 *  String / format helpers.                                          *
 * ================================================================== */
int WideCharToMultiByte(UINT CodePage, DWORD dwFlags, LPCWSTR lpWideCharStr,
                        int cchWideChar, LPSTR lpMultiByteStr, int cbMultiByte,
                        LPCSTR lpDefaultChar, LPBOOL lpUsedDefaultChar) {
    ULONG srcBytes;
    ULONG outBytes = 0;
    NTSTATUS s;
    (void)CodePage; (void)dwFlags; (void)lpDefaultChar;

    if (cchWideChar < 0) {              /* NUL-terminated: include the NUL */
        int i = 0;
        while (lpWideCharStr[i]) i++;
        cchWideChar = i + 1;
    }
    srcBytes = (ULONG)cchWideChar * (ULONG)sizeof(WCHAR);

    if (lpUsedDefaultChar) *lpUsedDefaultChar = FALSE;
    if (cbMultiByte == 0) {             /* required-size query (unused by RB3E) */
        return cchWideChar;
    }
    s = RtlUnicodeToMultiByteN(lpMultiByteStr, (ULONG)cbMultiByte, &outBytes,
                               lpWideCharStr, srcBytes);
    if (!NT_SUCCESS(s)) { oss_set_last_error_status(s); return 0; }
    return (int)outBytes;
}

int wsprintfW(LPWSTR lpOut, LPCWSTR lpFmt, ...) {
    va_list ap;
    int n;
    va_start(ap, lpFmt);
    n = vswprintf(lpOut, lpFmt, ap);
    va_end(ap);
    return n;
}
