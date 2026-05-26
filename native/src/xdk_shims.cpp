// DC3 Native Port - XDK API Shims
// POSIX implementations of Xbox SDK functions declared in src/xdk/ headers

#include "xdk/XBOXKRNL.h"
#include "xdk/XAPILIB.h"
#include "xdk/XGRAPHICS.h"
#include "os/Debug.h"

#include <pthread.h>
#include <unistd.h>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <sys/stat.h>
#include <sys/time.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <map>
#include <vector>

// ============================================================================
// Critical Section (XBOXKRNL.h)
// RTL_CRITICAL_SECTION is 0x1C bytes on Xbox. We store a pthread_mutex_t
// pointer in the RawEvent field (which gives us 16 bytes = plenty).
// ============================================================================

static pthread_mutex_t *GetMutex(RTL_CRITICAL_SECTION *cs) {
    uintptr_t ptr = (uintptr_t)cs->Synchronization.RawEvent[0] |
                    ((uintptr_t)cs->Synchronization.RawEvent[1] << 32);
    return (pthread_mutex_t *)ptr;
}

void RtlInitializeCriticalSection(RTL_CRITICAL_SECTION *cs) {
    // Store pthread_mutex in the union space
    static_assert(sizeof(cs->Synchronization.RawEvent) >= sizeof(void *), "Need pointer space");
    pthread_mutex_t *mtx = new pthread_mutex_t;
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(mtx, &attr);
    pthread_mutexattr_destroy(&attr);
    memset(cs, 0, sizeof(*cs));
    cs->Synchronization.RawEvent[0] = (unsigned int)(uintptr_t)mtx;
    cs->Synchronization.RawEvent[1] = (unsigned int)((uintptr_t)mtx >> 32);
    cs->LockCount = -1;
    cs->RecursionCount = 0;
    cs->OwningThread = nullptr;
}

void RtlEnterCriticalSection(RTL_CRITICAL_SECTION *cs) {
    pthread_mutex_t *mtx = GetMutex(cs);
    pthread_mutex_lock(mtx);
    cs->RecursionCount++;
    cs->OwningThread = (void *)(uintptr_t)pthread_self();
}

void RtlDeleteCriticalSection(RTL_CRITICAL_SECTION *cs) {
    pthread_mutex_t *mtx = GetMutex(cs);
    if (mtx) {
        pthread_mutex_destroy(mtx);
        delete mtx;
        memset(cs, 0, sizeof(*cs));
    }
}

void RtlLeaveCriticalSection(RTL_CRITICAL_SECTION *cs) {
    pthread_mutex_t *mtx = GetMutex(cs);
    cs->RecursionCount--;
    if (cs->RecursionCount == 0) {
        cs->OwningThread = nullptr;
    }
    pthread_mutex_unlock(mtx);
}

int RtlTryEnterCriticalSection(RTL_CRITICAL_SECTION *cs) {
    uintptr_t ptr = (uintptr_t)cs->Synchronization.RawEvent[0] |
                    ((uintptr_t)cs->Synchronization.RawEvent[1] << 32);
    pthread_mutex_t *mtx = (pthread_mutex_t *)ptr;
    if (pthread_mutex_trylock(mtx) == 0) {
        cs->RecursionCount++;
        cs->OwningThread = (void *)(uintptr_t)pthread_self();
        return 1;
    }
    return 0;
}

// ============================================================================
// Handle Management (handleapi.h)
// ============================================================================

// Simple handle table for events, threads, mutexes, etc.
enum HandleType { kHandleNone, kHandleEvent, kHandleThread, kHandleMutex, kHandleSemaphore, kHandleFile, kHandleTimer, kHandleNotify, kHandleFindFile };

struct HandleEntry {
    HandleType type;
    union {
        struct {
            pthread_mutex_t mutex;
            pthread_cond_t cond;
            bool signaled;
            bool manualReset;
        } event;
        struct {
            pthread_t thread;
            bool finished;
            DWORD exitCode;
        } thread;
        struct {
            pthread_mutex_t mutex;
        } mutex;
        struct {
            int fd;
        } file;
    };
};

// Use function-local static to avoid static initialization order issues
// (CreateEventA can be called during global constructors)
static pthread_mutex_t gHandleTableMutex = PTHREAD_MUTEX_INITIALIZER;
static uintptr_t gNextHandle = 0x1000;

static std::map<HANDLE, HandleEntry *> &GetHandleTable() {
    static std::map<HANDLE, HandleEntry *> table;
    return table;
}

static HANDLE AllocHandle(HandleEntry *entry) {
    pthread_mutex_lock(&gHandleTableMutex);
    HANDLE h = (HANDLE)(void *)gNextHandle++;
    GetHandleTable()[h] = entry;
    pthread_mutex_unlock(&gHandleTableMutex);
    return h;
}

static HandleEntry *LookupHandle(HANDLE h) {
    pthread_mutex_lock(&gHandleTableMutex);
    auto &table = GetHandleTable();
    auto it = table.find(h);
    HandleEntry *e = (it != table.end()) ? it->second : nullptr;
    pthread_mutex_unlock(&gHandleTableMutex);
    return e;
}

BOOL CloseHandle(HANDLE hObject) {
    pthread_mutex_lock(&gHandleTableMutex);
    auto &table = GetHandleTable();
    auto it = table.find(hObject);
    if (it == table.end()) {
        pthread_mutex_unlock(&gHandleTableMutex);
        return 0;
    }
    HandleEntry *e = it->second;
    table.erase(it);
    pthread_mutex_unlock(&gHandleTableMutex);

    switch (e->type) {
    case kHandleEvent:
        pthread_cond_destroy(&e->event.cond);
        pthread_mutex_destroy(&e->event.mutex);
        break;
    case kHandleThread:
        break;
    case kHandleMutex:
        pthread_mutex_destroy(&e->mutex.mutex);
        break;
    case kHandleFile:
        if (e->file.fd >= 0) close(e->file.fd);
        break;
    default:
        break;
    }
    delete e;
    return 1;
}

BOOL DuplicateHandle(HANDLE, HANDLE, HANDLE, LPHANDLE lpTargetHandle,
                     DWORD, BOOL, DWORD) {
    // Stub - just return the same handle
    if (lpTargetHandle) *lpTargetHandle = nullptr;
    return 0;
}

// ============================================================================
// Thread Functions (processthreadsapi.h)
// ============================================================================

struct ThreadStartData {
    LPTHREAD_START_ROUTINE *func;
    LPVOID param;
    HandleEntry *entry;
};

static void *ThreadStartRoutine(void *arg) {
    ThreadStartData *data = (ThreadStartData *)arg;
    DWORD result = data->func(data->param);
    data->entry->thread.exitCode = result;
    data->entry->thread.finished = true;
    delete data;
    return nullptr;
}

HANDLE CreateThread(LPSECURITY_ATTRIBUTES, SIZE_T stackSize,
                    LPTHREAD_START_ROUTINE lpStartAddress,
                    LPVOID lpParameter, DWORD dwCreationFlags,
                    LPDWORD lpThreadId) {
    HandleEntry *e = new HandleEntry;
    e->type = kHandleThread;
    e->thread.finished = false;
    e->thread.exitCode = 0;

    ThreadStartData *data = new ThreadStartData;
    data->func = lpStartAddress;
    data->param = lpParameter;
    data->entry = e;

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    if (stackSize > 0) {
        pthread_attr_setstacksize(&attr, stackSize);
    }

    int rc = pthread_create(&e->thread.thread, &attr, ThreadStartRoutine, data);
    pthread_attr_destroy(&attr);

    if (rc != 0) {
        delete data;
        delete e;
        return nullptr;
    }

    if (lpThreadId) {
        *lpThreadId = (DWORD)(uintptr_t)e->thread.thread;
    }

    return AllocHandle(e);
}

VOID ExitThread(DWORD dwExitCode) {
    pthread_exit((void *)(uintptr_t)dwExitCode);
}

DWORD GetCurrentThreadId() {
    return (DWORD)(uintptr_t)pthread_self();
}

BOOL GetExitCodeThread(HANDLE hThread, LPDWORD lpExitCode) {
    HandleEntry *e = LookupHandle(hThread);
    if (!e || e->type != kHandleThread) return 0;
    if (lpExitCode) *lpExitCode = e->thread.exitCode;
    return 1;
}

int GetThreadPriority(HANDLE) { return 0; }
HANDLE OpenThread(DWORD, BOOL, DWORD) { return nullptr; }
DWORD ResumeThread(HANDLE) { return 0; }
BOOL SetThreadPriority(HANDLE, int) { return 1; }
BOOL SwitchToThread() { sched_yield(); return 1; }

// TLS
static pthread_key_t gTlsKeys[256];
static bool gTlsUsed[256] = {};
static pthread_mutex_t gTlsMutex = PTHREAD_MUTEX_INITIALIZER;

DWORD TlsAlloc() {
    pthread_mutex_lock(&gTlsMutex);
    for (int i = 0; i < 256; i++) {
        if (!gTlsUsed[i]) {
            gTlsUsed[i] = true;
            pthread_key_create(&gTlsKeys[i], nullptr);
            pthread_mutex_unlock(&gTlsMutex);
            return i;
        }
    }
    pthread_mutex_unlock(&gTlsMutex);
    return 0xFFFFFFFF;
}

BOOL TlsFree(DWORD idx) {
    if (idx >= 256) return 0;
    pthread_mutex_lock(&gTlsMutex);
    if (gTlsUsed[idx]) {
        pthread_key_delete(gTlsKeys[idx]);
        gTlsUsed[idx] = false;
    }
    pthread_mutex_unlock(&gTlsMutex);
    return 1;
}

LPVOID TlsGetValue(DWORD idx) {
    if (idx >= 256 || !gTlsUsed[idx]) return nullptr;
    return pthread_getspecific(gTlsKeys[idx]);
}

BOOL TlsSetValue(DWORD idx, LPVOID val) {
    if (idx >= 256 || !gTlsUsed[idx]) return 0;
    pthread_setspecific(gTlsKeys[idx], val);
    return 1;
}

// ============================================================================
// Synchronization (synchapi.h)
// ============================================================================

HANDLE CreateEventA(LPSECURITY_ATTRIBUTES, BOOL bManualReset,
                    BOOL bInitialState, LPCSTR) {
    HandleEntry *e = new HandleEntry;
    e->type = kHandleEvent;
    pthread_mutex_init(&e->event.mutex, nullptr);
    pthread_cond_init(&e->event.cond, nullptr);
    e->event.signaled = bInitialState != 0;
    e->event.manualReset = bManualReset != 0;
    return AllocHandle(e);
}

BOOL SetEvent(HANDLE hEvent) {
    HandleEntry *e = LookupHandle(hEvent);
    if (!e || e->type != kHandleEvent) return 0;
    pthread_mutex_lock(&e->event.mutex);
    e->event.signaled = true;
    if (e->event.manualReset)
        pthread_cond_broadcast(&e->event.cond);
    else
        pthread_cond_signal(&e->event.cond);
    pthread_mutex_unlock(&e->event.mutex);
    return 1;
}

BOOL ResetEvent(HANDLE hEvent) {
    HandleEntry *e = LookupHandle(hEvent);
    if (!e || e->type != kHandleEvent) return 0;
    pthread_mutex_lock(&e->event.mutex);
    e->event.signaled = false;
    pthread_mutex_unlock(&e->event.mutex);
    return 1;
}

HANDLE CreateMutexA(LPSECURITY_ATTRIBUTES, BOOL bInitialOwner, LPCSTR) {
    HandleEntry *e = new HandleEntry;
    e->type = kHandleMutex;
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&e->mutex.mutex, &attr);
    pthread_mutexattr_destroy(&attr);
    if (bInitialOwner) {
        pthread_mutex_lock(&e->mutex.mutex);
    }
    return AllocHandle(e);
}

BOOL ReleaseMutex(HANDLE hMutex) {
    HandleEntry *e = LookupHandle(hMutex);
    if (!e || e->type != kHandleMutex) return 0;
    pthread_mutex_unlock(&e->mutex.mutex);
    return 1;
}

HANDLE CreateSemaphoreA(LPSECURITY_ATTRIBUTES, LONG, LONG, LPCSTR) {
    // Stub as event for now
    return CreateEventA(nullptr, 0, 0, nullptr);
}

BOOL ReleaseSemaphore(HANDLE h, LONG, LPLONG) {
    return SetEvent(h);
}

DWORD WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds) {
    HandleEntry *e = LookupHandle(hHandle);
    if (!e) return 0xFFFFFFFF; // WAIT_FAILED

    if (e->type == kHandleEvent) {
        pthread_mutex_lock(&e->event.mutex);
        if (dwMilliseconds == 0xFFFFFFFF) { // INFINITE
            while (!e->event.signaled) {
                pthread_cond_wait(&e->event.cond, &e->event.mutex);
            }
        } else {
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec += dwMilliseconds / 1000;
            ts.tv_nsec += (dwMilliseconds % 1000) * 1000000;
            if (ts.tv_nsec >= 1000000000) {
                ts.tv_sec++;
                ts.tv_nsec -= 1000000000;
            }
            while (!e->event.signaled) {
                if (pthread_cond_timedwait(&e->event.cond, &e->event.mutex, &ts) != 0) {
                    pthread_mutex_unlock(&e->event.mutex);
                    return 0x102; // WAIT_TIMEOUT
                }
            }
        }
        if (!e->event.manualReset) {
            e->event.signaled = false;
        }
        pthread_mutex_unlock(&e->event.mutex);
        return 0; // WAIT_OBJECT_0
    }
    else if (e->type == kHandleThread) {
        if (e->thread.finished) return 0;
        pthread_join(e->thread.thread, nullptr);
        e->thread.finished = true;
        return 0;
    }
    else if (e->type == kHandleMutex) {
        pthread_mutex_lock(&e->mutex.mutex);
        return 0;
    }

    return 0;
}

DWORD WaitForSingleObjectEx(HANDLE hHandle, DWORD dwMilliseconds, BOOL) {
    return WaitForSingleObject(hHandle, dwMilliseconds);
}

DWORD WaitForMultipleObjects(DWORD nCount, const HANDLE *lpHandles,
                             BOOL bWaitAll, DWORD dwMilliseconds) {
    // Simple implementation: wait for each sequentially
    if (bWaitAll) {
        for (DWORD i = 0; i < nCount; i++) {
            WaitForSingleObject(lpHandles[i], dwMilliseconds);
        }
        return 0;
    }
    // Wait for first
    for (DWORD i = 0; i < nCount; i++) {
        DWORD result = WaitForSingleObject(lpHandles[i], 0);
        if (result == 0) return i;
    }
    // None ready, wait on first
    return WaitForSingleObject(lpHandles[0], dwMilliseconds);
}

DWORD WaitForMultipleObjectsEx(DWORD nCount, const HANDLE *lpHandles,
                                BOOL bWaitAll, DWORD dwMilliseconds, BOOL) {
    return WaitForMultipleObjects(nCount, lpHandles, bWaitAll, dwMilliseconds);
}

VOID Sleep(DWORD dwMilliseconds) {
    usleep(dwMilliseconds * 1000);
}

DWORD SleepEx(DWORD dwMilliseconds, BOOL) {
    Sleep(dwMilliseconds);
    return 0;
}

BOOL CancelWaitableTimer(HANDLE) { return 1; }
BOOL SetWaitableTimer(HANDLE, const LARGE_INTEGER *, LONG, PTIMERAPCROUTINE, LPVOID, BOOL) { return 1; }

// ============================================================================
// File API (fileapi.h)
// ============================================================================

BOOL CreateDirectoryA(LPCSTR lpPathName, LPSECURITY_ATTRIBUTES) {
    return mkdir(lpPathName, 0755) == 0 ? 1 : 0;
}

HANDLE CreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD,
                   LPSECURITY_ATTRIBUTES, DWORD dwCreationDisposition,
                   DWORD, HANDLE) {
    int flags = 0;
    if ((dwDesiredAccess & 0x80000000) && (dwDesiredAccess & 0x40000000))
        flags = O_RDWR;
    else if (dwDesiredAccess & 0x80000000)
        flags = O_RDONLY;
    else if (dwDesiredAccess & 0x40000000)
        flags = O_WRONLY;

    switch (dwCreationDisposition) {
    case 1: flags |= O_CREAT | O_EXCL; break;   // CREATE_NEW
    case 2: flags |= O_CREAT | O_TRUNC; break;   // CREATE_ALWAYS
    case 3: break;                                 // OPEN_EXISTING
    case 4: flags |= O_CREAT; break;              // OPEN_ALWAYS
    case 5: flags |= O_TRUNC; break;              // TRUNCATE_EXISTING
    }

    int fd = open(lpFileName, flags, 0644);
    if (fd < 0) return (HANDLE)(LONG_PTR)-1; // INVALID_HANDLE_VALUE

    HandleEntry *e = new HandleEntry;
    e->type = kHandleFile;
    e->file.fd = fd;
    return AllocHandle(e);
}

BOOL DeleteFileA(LPCSTR lpFileName) {
    return unlink(lpFileName) == 0 ? 1 : 0;
}

BOOL RemoveDirectoryA(LPCSTR lpPathName) {
    return rmdir(lpPathName) == 0 ? 1 : 0;
}

DWORD GetFileAttributesA(LPCSTR lpFileName) {
    struct stat st;
    if (stat(lpFileName, &st) != 0) return 0xFFFFFFFF;
    DWORD attrs = 0;
    if (S_ISDIR(st.st_mode)) attrs |= 0x10; // FILE_ATTRIBUTE_DIRECTORY
    if (!(st.st_mode & S_IWUSR)) attrs |= 0x01; // FILE_ATTRIBUTE_READONLY
    if (attrs == 0) attrs = 0x80; // FILE_ATTRIBUTE_NORMAL
    return attrs;
}

BOOL GetFileAttributesExA(LPCSTR lpFileName, GET_FILEEX_INFO_LEVELS, LPVOID lpFileInformation) {
    struct stat st;
    if (stat(lpFileName, &st) != 0) return 0;
    // Fill WIN32_FILE_ATTRIBUTE_DATA - same layout as first part of WIN32_FIND_DATA
    // For now just set the size
    WIN32_FIND_DATAA *data = (WIN32_FIND_DATAA *)lpFileInformation;
    memset(data, 0, sizeof(*data));
    data->nFileSizeLow = (DWORD)(st.st_size & 0xFFFFFFFF);
    data->nFileSizeHigh = (DWORD)(st.st_size >> 32);
    if (S_ISDIR(st.st_mode)) data->dwFileAttributes |= 0x10;
    return 1;
}

BOOL GetFileInformationByHandle(HANDLE hFile, LPBY_HANDLE_FILE_INFORMATION lpInfo) {
    HandleEntry *e = LookupHandle(hFile);
    if (!e || e->type != kHandleFile) return 0;
    struct stat st;
    if (fstat(e->file.fd, &st) != 0) return 0;
    memset(lpInfo, 0, sizeof(*lpInfo));
    lpInfo->nFileSizeLow = (DWORD)(st.st_size & 0xFFFFFFFF);
    lpInfo->nFileSizeHigh = (DWORD)(st.st_size >> 32);
    return 1;
}

DWORD GetFileSize(HANDLE hFile, LPDWORD lpFileSizeHigh) {
    HandleEntry *e = LookupHandle(hFile);
    if (!e || e->type != kHandleFile) return 0xFFFFFFFF;
    struct stat st;
    if (fstat(e->file.fd, &st) != 0) return 0xFFFFFFFF;
    if (lpFileSizeHigh) *lpFileSizeHigh = (DWORD)(st.st_size >> 32);
    return (DWORD)(st.st_size & 0xFFFFFFFF);
}

BOOL GetFileSizeEx(HANDLE hFile, PLARGE_INTEGER lpFileSize) {
    HandleEntry *e = LookupHandle(hFile);
    if (!e || e->type != kHandleFile) return 0;
    struct stat st;
    if (fstat(e->file.fd, &st) != 0) return 0;
    if (lpFileSize) lpFileSize->QuadPart = st.st_size;
    return 1;
}

BOOL ReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead,
              LPDWORD lpNumberOfBytesRead, LPOVERLAPPED) {
    HandleEntry *e = LookupHandle(hFile);
    if (!e || e->type != kHandleFile) return 0;
    ssize_t n = read(e->file.fd, lpBuffer, nNumberOfBytesToRead);
    if (n < 0) return 0;
    if (lpNumberOfBytesRead) *lpNumberOfBytesRead = (DWORD)n;
    return 1;
}

BOOL WriteFile(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite,
               LPDWORD lpNumberOfBytesWritten, LPOVERLAPPED) {
    HandleEntry *e = LookupHandle(hFile);
    if (!e || e->type != kHandleFile) return 0;
    ssize_t n = write(e->file.fd, lpBuffer, nNumberOfBytesToWrite);
    if (n < 0) return 0;
    if (lpNumberOfBytesWritten) *lpNumberOfBytesWritten = (DWORD)n;
    return 1;
}

DWORD SetFilePointer(HANDLE hFile, LONG lDistanceToMove,
                     PLONG lpDistanceToMoveHigh, DWORD dwMoveMethod) {
    HandleEntry *e = LookupHandle(hFile);
    if (!e || e->type != kHandleFile) return 0xFFFFFFFF;
    int whence = SEEK_SET;
    if (dwMoveMethod == 1) whence = SEEK_CUR;
    else if (dwMoveMethod == 2) whence = SEEK_END;
    off_t result = lseek(e->file.fd, lDistanceToMove, whence);
    if (result < 0) return 0xFFFFFFFF;
    return (DWORD)result;
}

BOOL SetEndOfFile(HANDLE hFile) {
    HandleEntry *e = LookupHandle(hFile);
    if (!e || e->type != kHandleFile) return 0;
    off_t pos = lseek(e->file.fd, 0, SEEK_CUR);
    return ftruncate(e->file.fd, pos) == 0 ? 1 : 0;
}

BOOL SetFileAttributesA(LPCSTR, DWORD) { return 1; }

BOOL FileTimeToLocalFileTime(const FILETIME *, LPFILETIME lpLocalFileTime) {
    if (lpLocalFileTime) memset(lpLocalFileTime, 0, sizeof(*lpLocalFileTime));
    return 1;
}

HANDLE FindFirstFileA(LPCSTR lpFileName, LPWIN32_FIND_DATAA lpFindFileData) {
    // Simple stub - doesn't support wildcards properly yet
    struct stat st;
    if (stat(lpFileName, &st) == 0) {
        memset(lpFindFileData, 0, sizeof(*lpFindFileData));
        strncpy(lpFindFileData->cFileName, lpFileName, 259);
        lpFindFileData->nFileSizeLow = (DWORD)st.st_size;
        if (S_ISDIR(st.st_mode)) lpFindFileData->dwFileAttributes = 0x10;
    }
    return (HANDLE)(LONG_PTR)-1;
}

BOOL FindNextFileA(HANDLE, LPWIN32_FIND_DATAA) { return 0; }

BOOL GetDiskFreeSpaceExA(LPCSTR, PULARGE_INTEGER lpFree, PULARGE_INTEGER lpTotal, PULARGE_INTEGER) {
    if (lpFree) lpFree->QuadPart = 1024ULL * 1024 * 1024 * 10; // 10GB
    if (lpTotal) lpTotal->QuadPart = 1024ULL * 1024 * 1024 * 100;
    return 1;
}

// ============================================================================
// System Info (sysinfoapi.h)
// ============================================================================

VOID GetLocalTime(LPSYSTEMTIME lpSystemTime) {
    time_t now = time(nullptr);
    struct tm *tm = localtime(&now);
    lpSystemTime->wYear = tm->tm_year + 1900;
    lpSystemTime->wMonth = tm->tm_mon + 1;
    lpSystemTime->wDayOfWeek = tm->tm_wday;
    lpSystemTime->wDay = tm->tm_mday;
    lpSystemTime->wHour = tm->tm_hour;
    lpSystemTime->wMinute = tm->tm_min;
    lpSystemTime->wSecond = tm->tm_sec;
    lpSystemTime->wMilliseconds = 0;
}

VOID GetSystemTime(LPSYSTEMTIME lpSystemTime) {
    time_t now = time(nullptr);
    struct tm *tm = gmtime(&now);
    lpSystemTime->wYear = tm->tm_year + 1900;
    lpSystemTime->wMonth = tm->tm_mon + 1;
    lpSystemTime->wDayOfWeek = tm->tm_wday;
    lpSystemTime->wDay = tm->tm_mday;
    lpSystemTime->wHour = tm->tm_hour;
    lpSystemTime->wMinute = tm->tm_min;
    lpSystemTime->wSecond = tm->tm_sec;
    lpSystemTime->wMilliseconds = 0;
}

VOID GetSystemTimeAsFileTime(LPFILETIME lpFileTime) {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    // Convert to Windows FILETIME (100ns intervals since 1601-01-01)
    unsigned long long t = (unsigned long long)tv.tv_sec * 10000000ULL +
                           (unsigned long long)tv.tv_usec * 10ULL +
                           116444736000000000ULL;
    lpFileTime->dwLowDateTime = (DWORD)(t & 0xFFFFFFFF);
    lpFileTime->dwHighDateTime = (DWORD)(t >> 32);
}

DWORD GetTickCount() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (DWORD)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

// ============================================================================
// Error Handling (errhandlingapi.h)
// ============================================================================

// Forward declarations for anything in errhandlingapi.h
// These are usually just stubs

// ============================================================================
// Debug API (debugapi.h)
// ============================================================================

// OutputDebugStringA - forward declared in debugapi.h
// Implementation provided here

// ============================================================================
// String API (stringapiset.h)
// ============================================================================

// WideCharToMultiByte / MultiByteToWideChar stubs if needed

// ============================================================================
// Xbox-specific functions (xbox.h)
// ============================================================================

DWORD XBackgroundDownloadSetMode(unsigned int) { return 0; }
DWORD XEnableScreenSaver(BOOL) { return 0; }
DWORD XGetLocale() { return 0x0409; } // en-US
DWORD XTLGetLanguage() { return 1; }  // English
VOID XLaunchNewImage(LPCSTR, DWORD) { exit(0); }

LPVOID XPhysicalAlloc(SIZE_T dwSize, ULONG_PTR, ULONG_PTR ulAlignment, DWORD) {
    void *ptr = nullptr;
    if (ulAlignment > 0) {
        posix_memalign(&ptr, ulAlignment > sizeof(void*) ? ulAlignment : sizeof(void*), dwSize);
    } else {
        ptr = malloc(dwSize);
    }
    return ptr;
}

VOID XPhysicalFree(LPVOID lpAddress) {
    free(lpAddress);
}

DWORD XShowFriendsUI(DWORD) { return 0; }
DWORD XShowPartyUI(DWORD) { return 0; }
DWORD XShowNuiFriendsUI(DWORD, DWORD) { return 0; }
DWORD XShowNuiPartyUI(DWORD, DWORD) { return 0; }
DWORD XShowNuiGuideUI(DWORD) { return 0; }
HRESULT XNuiDelayUI(ULONG) { return 0; }

DWORD XUserCheckPrivilege(DWORD, unsigned int, BOOL *pfResult) {
    if (pfResult) *pfResult = 1;
    return 0;
}

XUSER_SIGNIN_STATE XUserGetSigninState(DWORD) {
    return eXUserSigninState_NotSignedIn;
}

DWORD XUserGetXUID(DWORD, XUID *pxuid) {
    if (pxuid) *pxuid = 0;
    return 0;
}

VOID XUserSetContext(DWORD, DWORD, DWORD) {}

DWORD XUserWriteAchievements(DWORD, XUSER_ACHIEVEMENT *, XOVERLAPPED *) { return 0; }
DWORD XContentClose(LPCSTR, XOVERLAPPED *) { return 0; }
DWORD XContentGetDeviceData(DWORD, XDEVICE_DATA *) { return 1; }
DWORD XCancelOverlapped(XOVERLAPPED *) { return 0; }
DWORD XGetOverlappedExtendedError(XOVERLAPPED *) { return 0; }

DWORD XShowKeyboardUI(DWORD, DWORD, LPCWSTR, LPCWSTR, LPCWSTR, LPWSTR, DWORD, XOVERLAPPED *) { return 1; }

DWORD XContentCrossTitleCreate(DWORD, LPCSTR, XCONTENT_CROSS_TITLE_DATA *, DWORD, DWORD *, DWORD *, SIZE_T, ULARGE_INTEGER, XOVERLAPPED *) { return 1; }
DWORD XContentCrossTitleDelete(DWORD, const XCONTENT_CROSS_TITLE_DATA *, XOVERLAPPED *) { return 0; }
DWORD XGetOverlappedResult(XOVERLAPPED *, DWORD *pdwResult, BOOL) {
    if (pdwResult) *pdwResult = 0;
    return 0;
}
DWORD XUserGetSigninInfo(DWORD, DWORD, XUSER_SIGNIN_INFO *pInfo) {
    if (pInfo) memset(pInfo, 0, sizeof(*pInfo));
    return 0;
}
DWORD XSetThreadProcessor(HANDLE, DWORD) { return 0; }
DWORD XContentCreateEx(DWORD, LPCSTR, const XCONTENT_DATA *, DWORD, DWORD *, DWORD *, SIZE_T, ULARGE_INTEGER, XOVERLAPPED *) { return 1; }
DWORD XContentGetCreator(DWORD, const XCONTENT_DATA *, BOOL *, XUID *, XOVERLAPPED *) { return 0; }
DWORD XContentGetDeviceState(DWORD, XOVERLAPPED *) { return 0; }
DWORD XContentDelete(DWORD, const XCONTENT_DATA *, XOVERLAPPED *) { return 0; }
DWORD XContentCreateEnumerator(DWORD, DWORD, DWORD, DWORD, DWORD, DWORD *pcbBuffer, HANDLE *phEnum) {
    if (pcbBuffer) *pcbBuffer = 0;
    if (phEnum) *phEnum = nullptr;
    return 0;
}
DWORD XEnumerate(HANDLE, VOID *, DWORD, DWORD *pcItemsReturned, XOVERLAPPED *) {
    if (pcItemsReturned) *pcItemsReturned = 0;
    return 0;
}
DWORD XContentFlush(LPCSTR, XOVERLAPPED *) { return 0; }
ULONGLONG XContentCalculateSize(ULONGLONG cbData, DWORD) { return cbData; }
VOID XAudioGetSpeakerConfig() {}
VOID XGetVideoMode(XVIDEO_MODE *pMode) {
    if (pMode) {
        memset(pMode, 0, sizeof(*pMode));
        pMode->dwDisplayWidth = 1280;
        pMode->dwDisplayHeight = 720;
        pMode->fIsInterlaced = 0;
        pMode->fIsWideScreen = 1;
        pMode->VideoStandard = 1; // NTSC
    }
}
VOID *XMemSet(VOID *dest, INT c, SIZE_T count) { return memset(dest, c, count); }
VOID *XMemAlloc(SIZE_T dwSize, DWORD) { return malloc(dwSize); }
VOID XMemFree(LPVOID lpHandle, DWORD) { free(lpHandle); }
INT XMemSize(LPVOID, DWORD) { return 0; }
DWORD XPhysicalSize(LPVOID) { return 0; }
DWORD XUserAwardGamerPicture(DWORD, DWORD, DWORD, XOVERLAPPED *) { return 0; }
DWORD XUserAwardAvatarAssets(DWORD, const XUSER_AVATARASSET *, XOVERLAPPED *) { return 0; }
DWORD XUserGetName(DWORD, LPSTR szUserName, DWORD cchUserName) {
    if (szUserName && cchUserName > 0) {
        strncpy(szUserName, "Player", cchUserName);
    }
    return 0;
}
DWORD XShowTokenRedemptionUI(DWORD) { return 0; }
DWORD XUserAreUsersFriends(DWORD, XUID *, DWORD, BOOL *pfResult, XOVERLAPPED *) {
    if (pfResult) *pfResult = 0;
    return 0;
}
DWORD XShowNuiMarketplaceUI(DWORD, DWORD, DWORD, QWORD, DWORD) { return 0; }
DWORD XShowMarketplaceUI(DWORD, DWORD, QWORD, DWORD) { return 0; }
DWORD XShowNuiDeviceSelectorUI(DWORD, DWORD, DWORD, DWORD, ULARGE_INTEGER, DWORD *, XOVERLAPPED *) { return 0; }
DWORD XShowDeviceSelectorUI(DWORD, DWORD, DWORD, ULARGE_INTEGER, DWORD *, XOVERLAPPED *) { return 0; }
DWORD XGetGameRegion() { return 0x00FF; } // All regions
DWORD XShowNuiDirtyDiscErrorUI(DWORD, DWORD) { exit(1); }
DWORD XShowDirtyDiscErrorUI(DWORD) { exit(1); }
DWORD XMarketplaceCreateOfferEnumerator(DWORD, DWORD, ULONGLONG, DWORD, DWORD *, HANDLE *) { return 1; }
DWORD XMarketplaceCreateOfferEnumeratorByOffering(DWORD, DWORD, ULONGLONG *, WORD, DWORD *, HANDLE *) { return 1; }
HANDLE XNotifyCreateListener(DWORD) { return nullptr; }
BOOL XNotifyGetNext(HANDLE, DWORD, DWORD *, ULONG_PTR *) { return 0; }

// ============================================================================
// Memory Status (winbase.h)
// ============================================================================

VOID GlobalMemoryStatus(LPMEMORYSTATUS lpBuffer) {
    if (!lpBuffer) return;
    memset(lpBuffer, 0, sizeof(*lpBuffer));
    lpBuffer->dwLength = sizeof(MEMORYSTATUS);
    lpBuffer->dwTotalPhys = 512 * 1024 * 1024;  // 512MB (Xbox 360 memory)
    lpBuffer->dwAvailPhys = 256 * 1024 * 1024;
    lpBuffer->dwTotalVirtual = 512 * 1024 * 1024;
    lpBuffer->dwAvailVirtual = 256 * 1024 * 1024;
}

// ============================================================================
// Process Environment (processenv.h)
// ============================================================================

// GetCommandLineA, GetEnvironmentStrings, etc. - stub if needed

// ============================================================================
// PIX (pix3.h) - GPU debugging, all no-ops
// ============================================================================

// PIX functions are typically macros/empty in release builds

// ============================================================================
// XInput (xinput.h) - Controller input
// ============================================================================

// XInputGetState, XInputSetState etc. - handled in Joypad_Stub

// ============================================================================
// Timezone API (timezoneapi.h)
// ============================================================================

// FileTimeToSystemTime etc. - stub if needed

// ============================================================================
// XGRAPHICS functions
// ============================================================================

// XGSurfaceSize, XGGetTextureDesc, etc. - stub
unsigned int XGSurfaceSize(int width, int height, int, unsigned int) {
    return width * height * 4;
}

// Forward declare the rest as needed - these will be added as compilation
// reveals which ones are actually referenced
