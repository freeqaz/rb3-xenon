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
#include <memory>
#include <string>
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

// Handle table for events, threads, mutexes, files and XContent enumerations.
//
// Win32 separates a handle from the object it names: CloseHandle drops one
// reference, and the object lives while anything still refers to it. Here
// every handle is a std::shared_ptr<HandleEntry> in the table; LookupHandle
// hands its caller another reference, so a SetEvent, a wait or a ReadFile in
// progress keeps the entry (and its mutex, condvar or fd) alive across a
// concurrent CloseHandle, and ~HandleEntry frees the resources when the last
// reference goes. A thread's state is a separate ShimThread shared by its
// handles and by the running thread itself, so a thread can outlive every
// handle to it and a handle can outlive the thread.
struct ShimThread;

enum HandleType { kHandleNone, kHandleEvent, kHandleThread, kHandleMutex, kHandleSemaphore, kHandleFile, kHandleTimer, kHandleNotify, kHandleFindFile, kHandleEnum };

struct HandleEntry {
    HandleType type;
    std::shared_ptr<ShimThread> thr; // kHandleThread only
    ~HandleEntry();
    union {
        struct {
            pthread_mutex_t mutex;
            pthread_cond_t cond;
            bool signaled;
            bool manualReset;
        } event;
        struct {
            pthread_mutex_t mutex;
        } mutex;
        struct {
            int fd;
        } file;
        struct {
            std::vector<XCONTENT_DATA> *items; // the packages, read at creation
            size_t next;
        } enumer;
    };
};

// Use function-local static to avoid static initialization order issues
// (CreateEventA can be called during global constructors)
static pthread_mutex_t gHandleTableMutex = PTHREAD_MUTEX_INITIALIZER;
static uintptr_t gNextHandle = 0x1000;

typedef std::shared_ptr<HandleEntry> HandleRef;

// Never destroyed: a detached thread still running at exit() may look a
// handle up after static destructors have run.
static std::map<HANDLE, HandleRef> &GetHandleTable() {
    static std::map<HANDLE, HandleRef> *table = new std::map<HANDLE, HandleRef>;
    return *table;
}

// Takes ownership of entry.
static HANDLE AllocHandle(HandleEntry *entry) {
    HandleRef ref(entry);
    pthread_mutex_lock(&gHandleTableMutex);
    HANDLE h = (HANDLE)(void *)gNextHandle++;
    GetHandleTable()[h] = std::move(ref);
    pthread_mutex_unlock(&gHandleTableMutex);
    return h;
}

static HANDLE AllocHandleRef(HandleRef ref) {
    pthread_mutex_lock(&gHandleTableMutex);
    HANDLE h = (HANDLE)(void *)gNextHandle++;
    GetHandleTable()[h] = std::move(ref);
    pthread_mutex_unlock(&gHandleTableMutex);
    return h;
}

// A new reference: valid for as long as the caller holds it, whatever
// CloseHandle does meanwhile.
static HandleRef LookupHandle(HANDLE h) {
    pthread_mutex_lock(&gHandleTableMutex);
    auto &table = GetHandleTable();
    auto it = table.find(h);
    HandleRef e = (it != table.end()) ? it->second : HandleRef();
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
    HandleRef e = std::move(it->second);
    table.erase(it);
    pthread_mutex_unlock(&gHandleTableMutex);
    // e goes out of scope here; ~HandleEntry runs only if no wait, SetEvent,
    // ReadFile etc. still holds a reference.
    return 1;
}

HandleEntry::~HandleEntry() {
    switch (type) {
    case kHandleEvent:
        pthread_cond_destroy(&event.cond);
        pthread_mutex_destroy(&event.mutex);
        break;
    case kHandleMutex:
        pthread_mutex_destroy(&mutex.mutex);
        break;
    case kHandleFile:
        if (file.fd >= 0) close(file.fd);
        break;
    case kHandleEnum:
        delete enumer.items;
        break;
    default: // kHandleThread: thr's destructor; the rest own nothing
        break;
    }
}

// An absolute CLOCK_REALTIME deadline dwMilliseconds from now.
static void DeadlineAfterMs(struct timespec *ts, DWORD dwMilliseconds) {
    clock_gettime(CLOCK_REALTIME, ts);
    ts->tv_sec += dwMilliseconds / 1000;
    ts->tv_nsec += (dwMilliseconds % 1000) * 1000000;
    if (ts->tv_nsec >= 1000000000) {
        ts->tv_sec++;
        ts->tv_nsec -= 1000000000;
    }
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

// Win32's STILL_ACTIVE: GetExitCodeThread's answer while a thread runs.
static const DWORD kShimStillActive = 0x103;

// Exit codes by thread id (GetCurrentThreadId's value) for every thread
// CreateThread started: kShimStillActive while it runs, its exit code after.
// OpenThread reads it. Retail's only OpenThread caller is ValidateThreadId
// (utl/MakeString.cpp, 0x827BB7A8): OpenThread(0x40, FALSE, id) at
// 0x827BB7C4, GetExitCodeThread at 0x827BB7E0, alive iff the code is 0x103.
// NextBuf (0x827C3D80, the call at 0x827C3E44) hands a dead thread's
// MakeString buffers to the calling thread and keeps a live one's.
// The record outlives the thread object, so an id whose every handle was
// closed still reads its exit code.
static pthread_mutex_t gShimThreadExitMutex = PTHREAD_MUTEX_INITIALIZER;
static std::map<DWORD, DWORD> &ShimThreadExitCodes() {
    static std::map<DWORD, DWORD> *codes = new std::map<DWORD, DWORD>;
    return *codes;
}
// Live thread objects by id, so OpenThread's handle names the same object as
// CreateThread's. Guarded by gShimThreadExitMutex.
static std::map<DWORD, std::weak_ptr<ShimThread>> &ShimLiveThreads() {
    static auto *live = new std::map<DWORD, std::weak_ptr<ShimThread>>;
    return *live;
}

// A Win32 thread object. Shared by every handle to the thread and by the
// running thread itself (tCurrentShimThread), and freed after the last of
// them lets go: CloseHandle while the thread runs is harmless, and a handle
// may outlive the thread. The pthread is detached; waits use the condvar,
// so any number of threads can wait, with timeouts.
struct ShimThread {
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
    bool finished = false;
    DWORD exitCode = kShimStillActive;
    DWORD id = 0;

    ~ShimThread() {
        if (id != 0) {
            pthread_mutex_lock(&gShimThreadExitMutex);
            auto &live = ShimLiveThreads();
            auto it = live.find(id);
            // A newer thread may have been given the same id; leave its entry.
            if (it != live.end() && it->second.expired()) live.erase(it);
            pthread_mutex_unlock(&gShimThreadExitMutex);
        }
        pthread_cond_destroy(&cond);
        pthread_mutex_destroy(&mutex);
    }

    // The first call wins (ExitThread, then the start routine's unwind).
    void Finish(DWORD code) {
        pthread_mutex_lock(&mutex);
        bool first = !finished;
        if (first) {
            finished = true;
            exitCode = code;
            pthread_cond_broadcast(&cond);
        }
        pthread_mutex_unlock(&mutex);
        if (first && id != 0) {
            pthread_mutex_lock(&gShimThreadExitMutex);
            ShimThreadExitCodes()[id] = code;
            pthread_mutex_unlock(&gShimThreadExitMutex);
        }
    }

    DWORD ExitCode() {
        pthread_mutex_lock(&mutex);
        DWORD code = exitCode;
        pthread_mutex_unlock(&mutex);
        return code;
    }

    // WAIT_OBJECT_0 once the thread has finished, WAIT_TIMEOUT after
    // dwMilliseconds (0 polls, INFINITE never times out).
    DWORD Wait(DWORD dwMilliseconds) {
        pthread_mutex_lock(&mutex);
        if (dwMilliseconds == 0xFFFFFFFF) {
            while (!finished) pthread_cond_wait(&cond, &mutex);
        } else if (!finished) {
            struct timespec ts;
            DeadlineAfterMs(&ts, dwMilliseconds);
            while (!finished) {
                if (pthread_cond_timedwait(&cond, &mutex, &ts) == ETIMEDOUT) break;
            }
        }
        bool done = finished;
        pthread_mutex_unlock(&mutex);
        return done ? 0 : 0x102; // WAIT_OBJECT_0 : WAIT_TIMEOUT
    }
};

// The running thread's own reference to its object. A thread_local, so
// ExitThread's pthread_exit releases it too (its destructor runs at thread
// exit whether or not the start routine's frame is unwound).
static thread_local std::shared_ptr<ShimThread> tCurrentShimThread;

struct ThreadStartData {
    LPTHREAD_START_ROUTINE *func;
    LPVOID param;
    std::shared_ptr<ShimThread> thr;
};

static void *ThreadStartRoutine(void *arg) {
    ThreadStartData *data = (ThreadStartData *)arg;
    tCurrentShimThread = std::move(data->thr);
    LPTHREAD_START_ROUTINE *func = data->func;
    LPVOID param = data->param;
    delete data;
    DWORD result = func(param);
    tCurrentShimThread->Finish(result);
    tCurrentShimThread.reset();
    return nullptr;
}

HANDLE CreateThread(LPSECURITY_ATTRIBUTES, SIZE_T stackSize,
                    LPTHREAD_START_ROUTINE lpStartAddress,
                    LPVOID lpParameter, DWORD dwCreationFlags,
                    LPDWORD lpThreadId) {
    std::shared_ptr<ShimThread> thr = std::shared_ptr<ShimThread>(new ShimThread);

    ThreadStartData *data = new ThreadStartData;
    data->func = lpStartAddress;
    data->param = lpParameter;
    data->thr = thr;

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    if (stackSize > 0) {
        pthread_attr_setstacksize(&attr, stackSize);
    }

    // Register the id before the new thread can finish (Finish takes this
    // lock to record its exit code) or anyone can OpenThread it.
    pthread_mutex_lock(&gShimThreadExitMutex);
    pthread_t pt;
    int rc = pthread_create(&pt, &attr, ThreadStartRoutine, data);
    if (rc == 0) {
        thr->id = (DWORD)(uintptr_t)pt;
        ShimThreadExitCodes()[thr->id] = kShimStillActive;
        ShimLiveThreads()[thr->id] = thr;
    }
    pthread_mutex_unlock(&gShimThreadExitMutex);
    pthread_attr_destroy(&attr);

    if (rc != 0) {
        delete data;
        return nullptr;
    }

    if (lpThreadId) {
        *lpThreadId = thr->id;
    }

    HandleEntry *e = new HandleEntry;
    e->type = kHandleThread;
    e->thr = std::move(thr);
    return AllocHandle(e);
}

VOID ExitThread(DWORD dwExitCode) {
    if (tCurrentShimThread) tCurrentShimThread->Finish(dwExitCode);
    pthread_exit(nullptr);
}

DWORD GetCurrentThreadId() {
    return (DWORD)(uintptr_t)pthread_self();
}

BOOL GetExitCodeThread(HANDLE hThread, LPDWORD lpExitCode) {
    HandleRef e = LookupHandle(hThread);
    if (!e || e->type != kHandleThread) return 0;
    if (lpExitCode) *lpExitCode = e->thr->ExitCode();
    return 1;
}

int GetThreadPriority(HANDLE) { return 0; }
// A handle to the thread with that id. A thread CreateThread started and
// whose object is still alive shares that object (waits and exit codes track
// it). One whose object is gone answers from its exit-code record. A thread
// CreateThread did not start (the main thread, a host std::thread) has no
// record and reads as running, which is all a ValidateThreadId caller asks.
HANDLE OpenThread(DWORD, BOOL, DWORD dwThreadId) {
    std::shared_ptr<ShimThread> thr;
    DWORD code = kShimStillActive;
    pthread_mutex_lock(&gShimThreadExitMutex);
    auto lit = ShimLiveThreads().find(dwThreadId);
    if (lit != ShimLiveThreads().end()) thr = lit->second.lock();
    auto it = ShimThreadExitCodes().find(dwThreadId);
    if (it != ShimThreadExitCodes().end()) code = it->second;
    pthread_mutex_unlock(&gShimThreadExitMutex);
    if (!thr) {
        thr = std::shared_ptr<ShimThread>(new ShimThread); // id 0: not registered
        thr->finished = code != kShimStillActive;
        thr->exitCode = code;
    }
    HandleEntry *e = new HandleEntry;
    e->type = kHandleThread;
    e->thr = std::move(thr);
    return AllocHandle(e);
}
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
    HandleRef e = LookupHandle(hEvent);
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
    HandleRef e = LookupHandle(hEvent);
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
    HandleRef e = LookupHandle(hMutex);
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
    HandleRef e = LookupHandle(hHandle);
    if (!e) return 0xFFFFFFFF; // WAIT_FAILED

    if (e->type == kHandleEvent) {
        pthread_mutex_lock(&e->event.mutex);
        if (dwMilliseconds == 0xFFFFFFFF) { // INFINITE
            while (!e->event.signaled) {
                pthread_cond_wait(&e->event.cond, &e->event.mutex);
            }
        } else {
            struct timespec ts;
            DeadlineAfterMs(&ts, dwMilliseconds);
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
        return e->thr->Wait(dwMilliseconds);
    }
    else if (e->type == kHandleMutex) {
        if (dwMilliseconds == 0xFFFFFFFF) {
            pthread_mutex_lock(&e->mutex.mutex);
        } else if (dwMilliseconds == 0) {
            if (pthread_mutex_trylock(&e->mutex.mutex) != 0) return 0x102; // WAIT_TIMEOUT
        } else {
            struct timespec ts;
            DeadlineAfterMs(&ts, dwMilliseconds);
            if (pthread_mutex_timedlock(&e->mutex.mutex, &ts) != 0) return 0x102;
        }
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

// ---- Last error (errhandlingapi.h) ----------------------------------------
// CacheXbox's file workers read GetLastError() after a failed CreateFile,
// ReadFile, WriteFile or DeleteFile; the file shims below set it from errno.
static thread_local DWORD sShimLastError = 0;
DWORD GetLastError() { return sShimLastError; }
static void ShimSetLastErrorFromErrno() {
    switch (errno) {
    case ENOENT: sShimLastError = ERROR_FILE_NOT_FOUND; break;
    case ENOTDIR: sShimLastError = ERROR_PATH_NOT_FOUND; break;
    case EEXIST: sShimLastError = ERROR_ALREADY_EXISTS; break;
    case ENOSPC: sShimLastError = ERROR_DISK_FULL; break;
    case EACCES: case EPERM: sShimLastError = 0x0005; break; // ERROR_ACCESS_DENIED
    case ENOTEMPTY: sShimLastError = 0x0091; break;          // ERROR_DIR_NOT_EMPTY
    default: sShimLastError = ERROR_INVALID_FUNCTION; break;
    }
}

// ---- Content packages (the hard drive's XContent store) --------------------
// A package is a host directory. XContentCreateEx mounts it under the
// caller's root name, and the file shims translate "root:\dir\file" paths
// through the mount table. Layout under the content directory
// ($RB3_CONTENT_DIR, else $XDG_DATA_HOME/rb3-xenon/content, else
// ~/.local/share/rb3-xenon/content):
//   <DeviceID %08X>/<content type %08X>/<szFileName>/          the files
//   <DeviceID %08X>/<content type %08X>/<szFileName>.xcontent  its XCONTENT_DATA
// An empty content directory is the console with no saved game.
static std::string ShimContentDir() {
    if (const char *d = getenv("RB3_CONTENT_DIR")) {
        if (*d) return d;
    }
    if (const char *x = getenv("XDG_DATA_HOME")) {
        if (*x) return std::string(x) + "/rb3-xenon/content";
    }
    const char *home = getenv("HOME");
    return std::string(home ? home : ".") + "/.local/share/rb3-xenon/content";
}
static void ShimMkdirs(const std::string &path) {
    for (size_t i = 1; i <= path.size(); i++) {
        if (i == path.size() || path[i] == '/') mkdir(path.substr(0, i).c_str(), 0755);
    }
}
static std::string ShimPackageBase(DWORD deviceId, DWORD contentType) {
    char sub[32];
    snprintf(sub, sizeof(sub), "/%08X/%08X", (unsigned)deviceId, (unsigned)contentType);
    return ShimContentDir() + sub;
}
static std::string ShimPackageName(const XCONTENT_DATA *data) {
    char name[XCONTENT_MAX_FILENAME_LENGTH + 1];
    memcpy(name, data->szFileName, XCONTENT_MAX_FILENAME_LENGTH);
    name[XCONTENT_MAX_FILENAME_LENGTH] = 0;
    return name;
}
static pthread_mutex_t gShimMountMutex = PTHREAD_MUTEX_INITIALIZER;
static std::map<std::string, std::string> &ShimMounts() { // root name -> dir
    static std::map<std::string, std::string> mounts;
    return mounts;
}
// "root:\a\b" -> "<package dir>/a/b" when root is mounted; any other path
// (the data dir's relative paths) is returned unchanged.
static std::string ShimHostPath(LPCSTR path) {
    if (!path) return std::string();
    const char *colon = strchr(path, ':');
    if (!colon) return path;
    std::string root(path, colon - path);
    pthread_mutex_lock(&gShimMountMutex);
    auto it = ShimMounts().find(root);
    std::string dir = it != ShimMounts().end() ? it->second : std::string();
    pthread_mutex_unlock(&gShimMountMutex);
    if (dir.empty()) return path;
    std::string rest(colon + 1);
    for (char &c : rest) {
        if (c == '\\') c = '/';
    }
    if (rest.empty() || rest[0] != '/') rest.insert(rest.begin(), '/');
    return dir + rest;
}
// Completes an XContent call the way the XDK reports it: with an XOVERLAPPED
// the call returns ERROR_IO_PENDING and the outcome is read back through
// XGetOverlappedResult (ERROR_FUNCTION_FAILED plus the extended error
// HRESULT_FROM_WIN32(code) on failure); without one the code is returned.
static DWORD ShimComplete(DWORD code, XOVERLAPPED *pOverlapped) {
    if (!pOverlapped) return code;
    pOverlapped->InternalHigh = 0;
    pOverlapped->dwExtendedError = code ? (0x80070000u | code) : 0;
    pOverlapped->InternalLow = code ? 0x065B : ERROR_SUCCESS; // ERROR_FUNCTION_FAILED
    return ERROR_IO_PENDING;
}

BOOL CreateDirectoryA(LPCSTR lpPathName, LPSECURITY_ATTRIBUTES) {
    if (mkdir(ShimHostPath(lpPathName).c_str(), 0755) == 0) return 1;
    ShimSetLastErrorFromErrno();
    return 0;
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

    int fd = open(ShimHostPath(lpFileName).c_str(), flags, 0644);
    if (fd < 0) {
        ShimSetLastErrorFromErrno();
        return (HANDLE)(LONG_PTR)-1; // INVALID_HANDLE_VALUE
    }

    HandleEntry *e = new HandleEntry;
    e->type = kHandleFile;
    e->file.fd = fd;
    return AllocHandle(e);
}

BOOL DeleteFileA(LPCSTR lpFileName) {
    if (unlink(ShimHostPath(lpFileName).c_str()) == 0) return 1;
    ShimSetLastErrorFromErrno();
    return 0;
}

BOOL RemoveDirectoryA(LPCSTR lpPathName) {
    if (rmdir(ShimHostPath(lpPathName).c_str()) == 0) return 1;
    ShimSetLastErrorFromErrno();
    return 0;
}

DWORD GetFileAttributesA(LPCSTR lpFileName) {
    struct stat st;
    if (stat(ShimHostPath(lpFileName).c_str(), &st) != 0) {
        ShimSetLastErrorFromErrno();
        return 0xFFFFFFFF;
    }
    DWORD attrs = 0;
    if (S_ISDIR(st.st_mode)) attrs |= 0x10; // FILE_ATTRIBUTE_DIRECTORY
    if (!(st.st_mode & S_IWUSR)) attrs |= 0x01; // FILE_ATTRIBUTE_READONLY
    if (attrs == 0) attrs = 0x80; // FILE_ATTRIBUTE_NORMAL
    return attrs;
}

BOOL GetFileAttributesExA(LPCSTR lpFileName, GET_FILEEX_INFO_LEVELS, LPVOID lpFileInformation) {
    struct stat st;
    if (stat(ShimHostPath(lpFileName).c_str(), &st) != 0) {
        ShimSetLastErrorFromErrno();
        return 0;
    }
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
    HandleRef e = LookupHandle(hFile);
    if (!e || e->type != kHandleFile) return 0;
    struct stat st;
    if (fstat(e->file.fd, &st) != 0) return 0;
    memset(lpInfo, 0, sizeof(*lpInfo));
    lpInfo->nFileSizeLow = (DWORD)(st.st_size & 0xFFFFFFFF);
    lpInfo->nFileSizeHigh = (DWORD)(st.st_size >> 32);
    return 1;
}

DWORD GetFileSize(HANDLE hFile, LPDWORD lpFileSizeHigh) {
    HandleRef e = LookupHandle(hFile);
    if (!e || e->type != kHandleFile) return 0xFFFFFFFF;
    struct stat st;
    if (fstat(e->file.fd, &st) != 0) return 0xFFFFFFFF;
    if (lpFileSizeHigh) *lpFileSizeHigh = (DWORD)(st.st_size >> 32);
    return (DWORD)(st.st_size & 0xFFFFFFFF);
}

BOOL GetFileSizeEx(HANDLE hFile, PLARGE_INTEGER lpFileSize) {
    HandleRef e = LookupHandle(hFile);
    if (!e || e->type != kHandleFile) return 0;
    struct stat st;
    if (fstat(e->file.fd, &st) != 0) return 0;
    if (lpFileSize) lpFileSize->QuadPart = st.st_size;
    return 1;
}

BOOL ReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead,
              LPDWORD lpNumberOfBytesRead, LPOVERLAPPED) {
    HandleRef e = LookupHandle(hFile);
    if (!e || e->type != kHandleFile) return 0;
    ssize_t n = read(e->file.fd, lpBuffer, nNumberOfBytesToRead);
    if (n < 0) return 0;
    if (lpNumberOfBytesRead) *lpNumberOfBytesRead = (DWORD)n;
    return 1;
}

BOOL WriteFile(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite,
               LPDWORD lpNumberOfBytesWritten, LPOVERLAPPED) {
    HandleRef e = LookupHandle(hFile);
    if (!e || e->type != kHandleFile) return 0;
    ssize_t n = write(e->file.fd, lpBuffer, nNumberOfBytesToWrite);
    if (n < 0) return 0;
    if (lpNumberOfBytesWritten) *lpNumberOfBytesWritten = (DWORD)n;
    return 1;
}

DWORD SetFilePointer(HANDLE hFile, LONG lDistanceToMove,
                     PLONG lpDistanceToMoveHigh, DWORD dwMoveMethod) {
    HandleRef e = LookupHandle(hFile);
    if (!e || e->type != kHandleFile) return 0xFFFFFFFF;
    int whence = SEEK_SET;
    if (dwMoveMethod == 1) whence = SEEK_CUR;
    else if (dwMoveMethod == 2) whence = SEEK_END;
    off_t result = lseek(e->file.fd, lDistanceToMove, whence);
    if (result < 0) return 0xFFFFFFFF;
    return (DWORD)result;
}

BOOL SetEndOfFile(HANDLE hFile) {
    HandleRef e = LookupHandle(hFile);
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
    if (stat(ShimHostPath(lpFileName).c_str(), &st) == 0) {
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
// The two shape-only XDK calls meta_band/Utl.cpp's MaxAllowedHmxMaturityLevel
// declares (retail 0x82B54328 / 0x82B543A8; names are descriptive). Natively
// there is no parental-control restriction, so every content level is allowed.
extern "C" DWORD XContentRestrictionFlags(DWORD, DWORD *pdwFlags) {
    *pdwFlags = 0;
    return 0;
}
extern "C" DWORD XContentRatingLimit(DWORD, DWORD *pdwBoard, DWORD *pdwRating) {
    *pdwBoard = 0;
    *pdwRating = 0xff;
    return 0;
}
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

// ---------------------------------------------------------------------------
// The console model the XUser* and XContent* shims answer for: ONE local
// profile is signed in on user index 0, offline (signed in locally, never to
// Live); user indices 1-3 are empty. The hard drive is present and holds no
// saved game. Each answer below is the one the retail caller is written to
// receive from that console:
//   * PlatformMgr::UpdateSigninState (retail 0x8251C620) tests
//     XUserGetSigninState != 0, asks XUserGetSigninInfo(i, 2 = online XUID
//     only) and falls back to XUserGetXUID when that left the xuid zero, so a
//     local profile's XUID arrives through XUserGetXUID.
//   * PlatformMgr::IsPadAGuest compares XUserGetSigninInfo's result with
//     ERROR_NO_SUCH_USER for an empty index.
//   * PlatformMgr::GetOnlineID (retail 0x8251BE80) writes the id only when
//     XUserGetXUID returns 0.
// An offline XUID carries 0xE in its top nibble (a Live XUID 0x0009...).
// ---------------------------------------------------------------------------
static const XUID kShimOfflineXuid = 0xE000000052423301ULL;
static const DWORD kShimErrorFunctionFailed = 0x065B; // ERROR_FUNCTION_FAILED
static const DWORD kShimErrorInvalidHandle = 0x0006;  // ERROR_INVALID_HANDLE
// The one storage device: the hard drive. Its DeviceID is nonzero (0 is
// XCONTENTDEVICE_ANY, which CacheMgrXbox::PollChoose reads as a cancel).
static const DWORD kShimHddDeviceId = 1;

static bool ShimUserSignedIn(DWORD dwUserIndex) { return dwUserIndex == 0; }

XUSER_SIGNIN_STATE XUserGetSigninState(DWORD dwUserIndex) {
    return ShimUserSignedIn(dwUserIndex) ? eXUserSigninState_SignedInLocally
                                         : eXUserSigninState_NotSignedIn;
}

DWORD XUserGetXUID(DWORD dwUserIndex, XUID *pxuid) {
    if (!ShimUserSignedIn(dwUserIndex)) {
        if (pxuid) *pxuid = 0;
        return ERROR_NO_SUCH_USER;
    }
    if (pxuid) *pxuid = kShimOfflineXuid;
    return ERROR_SUCCESS;
}

VOID XUserSetContext(DWORD, DWORD, DWORD) {}

DWORD XUserWriteAchievements(DWORD, XUSER_ACHIEVEMENT *, XOVERLAPPED *) { return 0; }
// Unmounts the root name; the files stay in the package directory.
DWORD XContentClose(LPCSTR szRootName, XOVERLAPPED *pOverlapped) {
    pthread_mutex_lock(&gShimMountMutex);
    size_t erased = szRootName ? ShimMounts().erase(szRootName) : 0;
    pthread_mutex_unlock(&gShimMountMutex);
    return ShimComplete(erased ? ERROR_SUCCESS : ERROR_FILE_NOT_FOUND, pOverlapped);
}
// The hard drive (XCONTENTDEVICETYPE_HDD = 1), 20 GB with 10 GB free;
// CacheXbox::GetFreeSpaceSync adds ulDeviceFreeBytes to its own count.
DWORD XContentGetDeviceData(DWORD DeviceID, XDEVICE_DATA *pDeviceData) {
    if (DeviceID != kShimHddDeviceId) return ERROR_DEVICE_NOT_CONNECTED;
    if (pDeviceData) {
        memset(pDeviceData, 0, sizeof(*pDeviceData));
        pDeviceData->DeviceID = kShimHddDeviceId;
        pDeviceData->DeviceType = 1;
        pDeviceData->ulDeviceBytes = 20ULL << 30;
        pDeviceData->ulDeviceFreeBytes = 10ULL << 30;
        const char *name = "Hard Drive";
        for (int i = 0; name[i]; i++) pDeviceData->wszFriendlyName[i] = (WCHAR)name[i];
    }
    return ERROR_SUCCESS;
}
DWORD XCancelOverlapped(XOVERLAPPED *) { return 0; }
// The XDK reports an asynchronous failure as ERROR_FUNCTION_FAILED from
// XGetOverlappedResult with the real code (an HRESULT) here; CacheMgrXbox's
// PollMount masks it to its low 16 bits.
DWORD XGetOverlappedExtendedError(XOVERLAPPED *pOverlapped) {
    return pOverlapped ? pOverlapped->dwExtendedError : 0;
}

DWORD XShowKeyboardUI(DWORD, DWORD, LPCWSTR, LPCWSTR, LPCWSTR, LPWSTR, DWORD, XOVERLAPPED *) { return 1; }

DWORD XContentCrossTitleCreate(DWORD, LPCSTR, XCONTENT_CROSS_TITLE_DATA *, DWORD, DWORD *, DWORD *, SIZE_T, ULARGE_INTEGER, XOVERLAPPED *) { return 1; }
DWORD XContentCrossTitleDelete(DWORD, const XCONTENT_CROSS_TITLE_DATA *, XOVERLAPPED *) { return 0; }
// An operation's status is InternalLow (ERROR_IO_PENDING while it runs) and
// its result (bytes, items) is InternalHigh. The shims complete every
// operation before they return, so a still-pending one only appears when a
// caller polls an XOVERLAPPED no shim filled in. Shims that leave the
// caller's zeroed XOVERLAPPED untouched read as success with a result of 0,
// as before.
DWORD XGetOverlappedResult(XOVERLAPPED *pOverlapped, DWORD *pdwResult, BOOL) {
    if (!pOverlapped) {
        if (pdwResult) *pdwResult = 0;
        return ERROR_SUCCESS;
    }
    if (pOverlapped->InternalLow == ERROR_IO_PENDING) return ERROR_IO_INCOMPLETE;
    if (pdwResult) *pdwResult = (DWORD)pOverlapped->InternalHigh;
    return (DWORD)pOverlapped->InternalLow;
}
// Flags bit 2 is XUSER_GET_SIGNIN_INFO_ONLINE_XUID_ONLY: a local profile has
// no online XUID, so that query leaves the xuid zero (and UpdateSigninState
// then asks XUserGetXUID). dwInfoFlags 0: not a guest, not Live-enabled.
DWORD XUserGetSigninInfo(DWORD dwUserIndex, DWORD dwFlags, XUSER_SIGNIN_INFO *pInfo) {
    if (pInfo) memset(pInfo, 0, sizeof(*pInfo));
    if (!ShimUserSignedIn(dwUserIndex)) return ERROR_NO_SUCH_USER;
    if (pInfo) {
        pInfo->xuid = (dwFlags & 2) ? 0 : kShimOfflineXuid;
        pInfo->UserSigninState = eXUserSigninState_SignedInLocally;
        strncpy(pInfo->szUserName, "Player", sizeof(pInfo->szUserName) - 1);
    }
    return ERROR_SUCCESS;
}
DWORD XSetThreadProcessor(HANDLE, DWORD) { return 0; }
// Creates and/or opens a package and mounts it as szRootName. The low bits of
// dwContentFlags are the disposition: 1 CREATENEW, 2 CREATEALWAYS,
// 3 OPENEXISTING, 4 OPENALWAYS (CacheMgrXbox::MountAsync), 5 TRUNCATEEXISTING.
// *pdwDisposition is 1 (created new) or 2 (opened existing). Retail's
// MountAsync requires ERROR_IO_PENDING and reads the outcome in PollMount.
DWORD XContentCreateEx(DWORD, LPCSTR szRootName, const XCONTENT_DATA *pContentData, DWORD dwContentFlags,
                       DWORD *pdwDisposition, DWORD *pdwLicenseMask, SIZE_T, ULARGE_INTEGER,
                       XOVERLAPPED *pOverlapped) {
    if (pdwLicenseMask) *pdwLicenseMask = 0;
    if (!szRootName || !*szRootName || !pContentData)
        return ShimComplete(0x0057, pOverlapped); // ERROR_INVALID_PARAMETER
    DWORD device = pContentData->DeviceID ? pContentData->DeviceID : kShimHddDeviceId;
    if (device != kShimHddDeviceId) return ShimComplete(ERROR_DEVICE_NOT_CONNECTED, pOverlapped);
    std::string name = ShimPackageName(pContentData);
    if (name.empty()) return ShimComplete(ERROR_INVALID_NAME, pOverlapped);
    std::string dir = ShimPackageBase(device, pContentData->dwContentType) + "/" + name;
    struct stat st;
    bool exists = stat(dir.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
    DWORD disposition = dwContentFlags & 0xF;
    if (disposition == 1 && exists) return ShimComplete(ERROR_ALREADY_EXISTS, pOverlapped);
    if ((disposition == 3 || disposition == 5) && !exists)
        return ShimComplete(ERROR_PATH_NOT_FOUND, pOverlapped);
    if (!exists) {
        ShimMkdirs(dir);
        if (stat(dir.c_str(), &st) != 0) return ShimComplete(ERROR_PATH_NOT_FOUND, pOverlapped);
        XCONTENT_DATA meta = *pContentData;
        meta.DeviceID = device;
        if (FILE *f = fopen((dir + ".xcontent").c_str(), "wb")) {
            fwrite(&meta, sizeof(meta), 1, f);
            fclose(f);
        }
    }
    if (pdwDisposition) *pdwDisposition = exists ? 2 : 1;
    pthread_mutex_lock(&gShimMountMutex);
    ShimMounts()[szRootName] = dir;
    pthread_mutex_unlock(&gShimMountMutex);
    return ShimComplete(ERROR_SUCCESS, pOverlapped);
}
// Whether the user created a package. MCContainerXbox::Mount (0x8252AEA8)
// asks right after its XContentCreateEx (bl at 0x8252AF94, no XOVERLAPPED)
// and reads FALSE as someone else's save: it closes the package (0x8252AFB4)
// and fails the mount as ERROR_FILE_CORRUPT (0x570, kMCCorrupt). This console
// has one profile, and every package in the store was written by it, so a
// signed-in user created every package that exists; the creator XUID is that
// profile's offline XUID.
DWORD XContentGetCreator(DWORD dwUserIndex, const XCONTENT_DATA *pContentData,
                         BOOL *pfUserIsCreator, XUID *pxuidCreator, XOVERLAPPED *pOverlapped) {
    if (pfUserIsCreator) *pfUserIsCreator = 0;
    if (pxuidCreator) *pxuidCreator = 0;
    if (!pContentData) return ShimComplete(0x0057, pOverlapped); // ERROR_INVALID_PARAMETER
    DWORD device = pContentData->DeviceID ? pContentData->DeviceID : kShimHddDeviceId;
    std::string name = ShimPackageName(pContentData);
    if (name.empty()) return ShimComplete(ERROR_INVALID_NAME, pOverlapped);
    std::string dir = ShimPackageBase(device, pContentData->dwContentType) + "/" + name;
    struct stat st;
    if (stat((dir + ".xcontent").c_str(), &st) != 0) return ShimComplete(ERROR_FILE_NOT_FOUND, pOverlapped);
    if (pxuidCreator) *pxuidCreator = kShimOfflineXuid;
    if (pfUserIsCreator) *pfUserIsCreator = ShimUserSignedIn(dwUserIndex) ? 1 : 0;
    return ShimComplete(ERROR_SUCCESS, pOverlapped);
}
DWORD XContentGetDeviceState(DWORD, XOVERLAPPED *) { return 0; }
// Removes a package: its files and directories, then its record.
static void ShimRemoveTree(const std::string &dir) {
    if (DIR *d = opendir(dir.c_str())) {
        while (struct dirent *ent = readdir(d)) {
            if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, "..")) continue;
            std::string p = dir + "/" + ent->d_name;
            struct stat st;
            if (lstat(p.c_str(), &st) == 0 && S_ISDIR(st.st_mode))
                ShimRemoveTree(p);
            else
                unlink(p.c_str());
        }
        closedir(d);
    }
    rmdir(dir.c_str());
}
DWORD XContentDelete(DWORD, const XCONTENT_DATA *pContentData, XOVERLAPPED *pOverlapped) {
    if (!pContentData) return ShimComplete(0x0057, pOverlapped); // ERROR_INVALID_PARAMETER
    DWORD device = pContentData->DeviceID ? pContentData->DeviceID : kShimHddDeviceId;
    std::string name = ShimPackageName(pContentData);
    if (name.empty()) return ShimComplete(ERROR_INVALID_NAME, pOverlapped);
    std::string dir = ShimPackageBase(device, pContentData->dwContentType) + "/" + name;
    struct stat st;
    if (stat((dir + ".xcontent").c_str(), &st) != 0) return ShimComplete(ERROR_FILE_NOT_FOUND, pOverlapped);
    ShimRemoveTree(dir);
    unlink((dir + ".xcontent").c_str());
    return ShimComplete(ERROR_SUCCESS, pOverlapped);
}
// A content enumerator over the hard drive's packages (the content store
// above), read when the enumerator is created. Retail's callers:
// CacheMgrXbox::SearchAsync (0x827D7878) passes user 0xFE (any), device 0
// (any), type 1 (saved game), cItem 1; it treats 0x12 as "no storage
// device", requires 0 plus a handle other than INVALID_HANDLE_VALUE, and its
// (compiled out) assert expects the buffer size of one XCONTENT_DATA. It then
// calls XEnumerate with an XOVERLAPPED and requires ERROR_IO_PENDING.
// MemcardXbox::FindValidUnit (0x8252AA40) enumerates synchronously until
// XEnumerate stops returning 0.
DWORD XContentCreateEnumerator(DWORD, DWORD DeviceID, DWORD dwContentType, DWORD, DWORD cItem,
                               DWORD *pcbBuffer, HANDLE *phEnum) {
    if (pcbBuffer) *pcbBuffer = cItem * (DWORD)sizeof(XCONTENT_DATA);
    if (DeviceID != 0 && DeviceID != kShimHddDeviceId) return ERROR_DEVICE_NOT_CONNECTED;
    if (phEnum) {
        std::vector<XCONTENT_DATA> *items = new std::vector<XCONTENT_DATA>();
        std::string base = ShimPackageBase(kShimHddDeviceId, dwContentType);
        if (DIR *d = opendir(base.c_str())) {
            while (struct dirent *ent = readdir(d)) {
                size_t len = strlen(ent->d_name);
                if (len <= 9 || strcmp(ent->d_name + len - 9, ".xcontent") != 0) continue;
                XCONTENT_DATA data;
                FILE *f = fopen((base + "/" + ent->d_name).c_str(), "rb");
                if (!f) continue;
                bool ok = fread(&data, sizeof(data), 1, f) == 1;
                fclose(f);
                if (ok) items->push_back(data);
            }
            closedir(d);
        }
        HandleEntry *e = new HandleEntry();
        e->type = kHandleEnum;
        e->enumer.items = items;
        e->enumer.next = 0;
        *phEnum = AllocHandle(e);
    }
    return ERROR_SUCCESS;
}
// One package per call. cbBuffer is counted in retail units (the callers pass
// the literal 0x134, retail's sizeof(XCONTENT_DATA); natively the struct is
// wider, wchar_t being 4 bytes), so 0x134 or more holds one item. Past the
// last package an overlapped call completes as ERROR_FUNCTION_FAILED with
// extended error HRESULT_FROM_WIN32(ERROR_NO_MORE_FILES), which
// CacheMgrXbox::PollSearch (0x827D7E38) turns into kCache_ErrorCacheNotFound;
// a synchronous one returns ERROR_NO_MORE_FILES with no items.
DWORD XEnumerate(HANDLE hEnum, VOID *pvBuffer, DWORD cbBuffer, DWORD *pcItemsReturned, XOVERLAPPED *pOverlapped) {
    if (pcItemsReturned) *pcItemsReturned = 0;
    HandleRef e = LookupHandle(hEnum);
    if (!e || e->type != kHandleEnum) return kShimErrorInvalidHandle;
    if (!pvBuffer || cbBuffer < 0x134) return ShimComplete(0x007A, pOverlapped); // ERROR_INSUFFICIENT_BUFFER
    if (e->enumer.next >= e->enumer.items->size()) return ShimComplete(ERROR_NO_MORE_FILES, pOverlapped);
    memcpy(pvBuffer, &(*e->enumer.items)[e->enumer.next++], sizeof(XCONTENT_DATA));
    if (pOverlapped) {
        ShimComplete(ERROR_SUCCESS, pOverlapped);
        pOverlapped->InternalHigh = 1;
        return ERROR_IO_PENDING;
    }
    if (pcItemsReturned) *pcItemsReturned = 1;
    return ERROR_SUCCESS;
}
DWORD XContentFlush(LPCSTR, XOVERLAPPED *) { return 0; }
// Cross-title enumeration over the same content store. The one caller is
// XboxContentMgr::StartRefresh (retail 0x82520FD0): for each signed-in pad
// 0..3 it asks for (user i, device 0 = any, type 2 = marketplace content,
// flags 0, cItem 1, pcbBuffer NULL) (bl 0x8283EF70 at 0x825212F0), and once
// each for user 0xFF with type 2 and type 1 (saved games). It keeps the
// enumeration only on 0 and then needs XEnumerateCrossTitle(h, buf, 0x138, NULL,
// overlapped) to return ERROR_IO_PENDING (bl 0x8283F340 at 0x82521340); else it
// frees the XOVERLAPPED and closes the handle. PollRefresh (0x82520C18) reads
// the result through XGetOverlappedResult: 0 with the item count, or any
// failure ends that buffer. The store holds this title's packages only, so
// every item reports dwTitleId 0x45410914; there is no marketplace package
// (type 2) natively, and the saved games (band3, globaloptions, songcache)
// are on PollRefresh's ignored list. The declarations ContentMgr_Xbox.cpp
// uses are extern "C" with int/void* parameters, so these are too.
extern "C" long XContentCreateCrossTitleEnumerator(int dwUserIndex, void *DeviceID, int dwContentType,
                                                    int dwFlags, int cItem, int pcbBuffer, void *phEnum) {
    (void)dwUserIndex;
    (void)dwFlags;
    (void)pcbBuffer; // retail passes NULL
    return (long)XContentCreateEnumerator(0xFE, (DWORD)(uintptr_t)DeviceID, (DWORD)dwContentType, 0,
                                          (DWORD)cItem, nullptr, (HANDLE *)phEnum);
}
// One package per call, as XEnumerate; cbBuffer is in retail units (0x138 =
// retail's sizeof(XCONTENT_CROSS_TITLE_DATA)). The overlapped completion sets
// InternalHigh to the item count XGetOverlappedResult hands back.
extern "C" unsigned long XEnumerateCrossTitle(void *hEnum, void *pvBuffer, int cbBuffer,
                                              int pcItemsReturned, void *pOverlapped) {
    XOVERLAPPED *ov = (XOVERLAPPED *)pOverlapped;
    DWORD *pcItems = (DWORD *)(uintptr_t)pcItemsReturned;
    if (pcItems) *pcItems = 0;
    HandleRef e = LookupHandle((HANDLE)hEnum);
    if (!e || e->type != kHandleEnum) return kShimErrorInvalidHandle;
    if (!pvBuffer || cbBuffer < 0x138) return ShimComplete(0x007A, ov); // ERROR_INSUFFICIENT_BUFFER
    if (e->enumer.next >= e->enumer.items->size()) return ShimComplete(ERROR_NO_MORE_FILES, ov);
    const XCONTENT_DATA &src = (*e->enumer.items)[e->enumer.next++];
    XCONTENT_CROSS_TITLE_DATA *dst = (XCONTENT_CROSS_TITLE_DATA *)pvBuffer;
    memset(dst, 0, sizeof(*dst));
    dst->DeviceID = src.DeviceID;
    dst->dwContentType = src.dwContentType;
    memcpy(dst->szDisplayName, src.szDisplayName, sizeof(dst->szDisplayName));
    memcpy(dst->szFileName, src.szFileName, sizeof(dst->szFileName));
    dst->dwTitleId = 0x45410914;
    if (ov) {
        ShimComplete(ERROR_SUCCESS, ov);
        ov->InternalHigh = 1;
        return ERROR_IO_PENDING;
    }
    if (pcItems) *pcItems = 1;
    return ERROR_SUCCESS;
}
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
DWORD XUserGetName(DWORD dwUserIndex, LPSTR szUserName, DWORD cchUserName) {
    if (!ShimUserSignedIn(dwUserIndex)) return ERROR_NO_SUCH_USER;
    if (szUserName && cchUserName > 0) {
        strncpy(szUserName, "Player", cchUserName);
        szUserName[cchUserName - 1] = '\0';
    }
    return ERROR_SUCCESS;
}
// The signed-in local profile's gamer picture, read into the caller's buffer
// (64x64, or 32x32 when fSmall) at dwPitch bytes per row. On a console an
// offline profile has a picture (a dashboard tile stored with the profile),
// and the read completes asynchronously: ERROR_IO_PENDING, then success
// through XGetOverlappedResult. The one caller, ProfilePicture::
// FetchUserPicture (retail 0x82B903B8, bl at 0x82B9047C), keeps the read only
// on ERROR_IO_PENDING (0x82B90484) and ReceiveUserPicture (0x82B904E0) then
// turns success into a 64x64 RndTex and profile_picture_fetched_msg true.
// This console model holds no tile bytes (the dashboard's pictures are not in
// the game data), so the read succeeds without writing: the caller's buffer
// keeps what it held, which for ProfilePicture is the zero fill at 0x82B90460
// (a transparent tile). An empty user index has no picture: ERROR_NO_SUCH_USER.
DWORD XUserReadGamerPicture(DWORD dwUserIndex, BOOL fSmall, BYTE *pbTextureBuffer, DWORD dwPitch,
                            DWORD dwHeight, XOVERLAPPED *pOverlapped) {
    if (!ShimUserSignedIn(dwUserIndex)) return ShimComplete(ERROR_NO_SUCH_USER, pOverlapped);
    const DWORD tile = fSmall ? 32 : 64;
    if (!pbTextureBuffer || dwHeight < tile || dwPitch < tile * 4)
        return ShimComplete(0x0057, pOverlapped); // ERROR_INVALID_PARAMETER
    return ShimComplete(ERROR_SUCCESS, pOverlapped);
}
DWORD XShowTokenRedemptionUI(DWORD) { return 0; }
DWORD XUserAreUsersFriends(DWORD, XUID *, DWORD, BOOL *pfResult, XOVERLAPPED *) {
    if (pfResult) *pfResult = 0;
    return 0;
}
DWORD XShowNuiMarketplaceUI(DWORD, DWORD, DWORD, QWORD, DWORD) { return 0; }
DWORD XShowMarketplaceUI(DWORD, DWORD, QWORD, DWORD) { return 0; }
// With a single storage device the system picks it without showing the
// selector. Retail's callers (CacheMgrXbox::ShowUserSelectUIAsync 0x827D79C8,
// MemcardXbox::ShowDeviceSelector 0x82518458) require ERROR_IO_PENDING and
// then poll the XOVERLAPPED; a DeviceID of 0 on completion means cancelled.
static DWORD ShimDeviceSelected(DWORD *pdwDeviceID, XOVERLAPPED *pOverlapped) {
    if (pdwDeviceID) *pdwDeviceID = kShimHddDeviceId;
    if (!pOverlapped) return ERROR_SUCCESS;
    pOverlapped->InternalHigh = 0;
    pOverlapped->dwExtendedError = 0;
    pOverlapped->InternalLow = ERROR_SUCCESS;
    return ERROR_IO_PENDING;
}
DWORD XShowNuiDeviceSelectorUI(DWORD, DWORD, DWORD, DWORD, ULARGE_INTEGER, DWORD *pdwDeviceID, XOVERLAPPED *pOverlapped) {
    return ShimDeviceSelected(pdwDeviceID, pOverlapped);
}
DWORD XShowDeviceSelectorUI(DWORD, DWORD, DWORD, ULARGE_INTEGER, DWORD *pdwDeviceID, XOVERLAPPED *pOverlapped) {
    return ShimDeviceSelected(pdwDeviceID, pOverlapped);
}
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

// FILETIME is 100 ns ticks since 1601-01-01 UTC. DateTime::FromFileTime
// (os/DateTime.cpp) and CacheXbox's directory listing convert through this.
BOOL FileTimeToSystemTime(const FILETIME *lpFileTime, LPSYSTEMTIME lpSystemTime) {
    if (!lpFileTime || !lpSystemTime) return 0;
    unsigned long long t = ((unsigned long long)lpFileTime->dwHighDateTime << 32) |
                           lpFileTime->dwLowDateTime;
    const unsigned long long kEpochDelta = 116444736000000000ULL; // 1601 -> 1970
    if (t < kEpochDelta) return 0;
    unsigned long long ticks = t - kEpochDelta;
    time_t secs = (time_t)(ticks / 10000000ULL);
    struct tm tmv;
    if (!gmtime_r(&secs, &tmv)) return 0;
    lpSystemTime->wYear = tmv.tm_year + 1900;
    lpSystemTime->wMonth = tmv.tm_mon + 1;
    lpSystemTime->wDayOfWeek = tmv.tm_wday;
    lpSystemTime->wDay = tmv.tm_mday;
    lpSystemTime->wHour = tmv.tm_hour;
    lpSystemTime->wMinute = tmv.tm_min;
    lpSystemTime->wSecond = tmv.tm_sec;
    lpSystemTime->wMilliseconds = (WORD)((ticks / 10000ULL) % 1000ULL);
    return 1;
}

// ============================================================================
// XGRAPHICS functions
// ============================================================================

// XGSurfaceSize, XGGetTextureDesc, etc. - stub
unsigned int XGSurfaceSize(int width, int height, int, unsigned int) {
    return width * height * 4;
}

// Forward declare the rest as needed - these will be added as compilation
// reveals which ones are actually referenced

// ============================================================================
// XNetRandom (xdk/xnet/xnetapi.h) -- W16-PJ
// The XDK's cryptographically-random byte source. It used to be a weak no-op in
// dta_link_stubs.s that returned 0 WITHOUT filling the buffer, so the real
// HxGuid::Generate (utl/HxGuid.cpp) -- which retries until the GUID is non-null --
// spun forever on any native path that generates a UserGuid. Backed by the
// kernel CSPRNG, as XNetRandom is on the console.
// ============================================================================
#include <sys/random.h>

extern "C" int XNetRandom(unsigned char *pb, unsigned int cb) {
    unsigned int done = 0;
    while (done < cb) {
        ssize_t n = getrandom(pb + done, cb - done, 0);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        done += (unsigned int)n;
    }
    return 0;
}
