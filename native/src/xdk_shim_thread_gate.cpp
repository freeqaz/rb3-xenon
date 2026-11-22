// xdk_shim_thread_gate.cpp: one case per process, driven by tools/xdk_shim_thread_gate.sh,
// which compiles this file with src/xdk_shims.cpp under AddressSanitizer.
//
// Each case checks that a handle shim in xdk_shims.cpp separates handle lifetime from object
// lifetime the way Win32 does: CloseHandle drops one reference, a running thread keeps its own
// object alive, and a wait in progress keeps the object it waits on alive. A case prints
// "CASE <name> PASS" and exits 0, or prints "CASE <name> FAIL: <why>" and exits 1. A crash, an
// ASan report or a hang (the script's timeout) also counts as a failure.
//
// Control cases:
//   control-join    the ordinary lifecycle (create, wait, read the exit code, close). It passes
//                   with or without the fix, so a FAIL there means the harness is broken.
//   control-asan    writes through a freed pointer on purpose. The script expects ASan to report
//                   it; if ASan stays quiet, the instrument cannot see a use-after-free and every
//                   other PASS is vacuous.

#include "xdk/XBOXKRNL.h"
#include "xdk/XAPILIB.h"

#include <atomic>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <unistd.h>

static const DWORD kStillActive = 0x103;
static const DWORD kWaitTimeout = 0x102;
static const DWORD kInfinite = 0xFFFFFFFF;

static int Fail(const char *name, const char *why) {
    printf("CASE %s FAIL: %s\n", name, why);
    fflush(stdout);
    return 1;
}

static int Pass(const char *name) {
    printf("CASE %s PASS\n", name);
    fflush(stdout);
    return 0;
}

static void SleepMs(int ms) { usleep(ms * 1000); }

static double NowMs() {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

// A thread that sleeps for (ms) and returns (code).
struct SleepArgs {
    int ms;
    DWORD code;
};
static DWORD SleepThenReturn(LPVOID p) {
    SleepArgs *a = (SleepArgs *)p;
    SleepMs(a->ms);
    return a->code;
}

// A thread that waits for an event, then returns 11.
static DWORD WaitEventThenReturn(LPVOID p) {
    WaitForSingleObject((HANDLE)p, kInfinite);
    return 11;
}

static DWORD CallsExitThread(LPVOID) {
    SleepMs(20);
    ExitThread(9);
    return 1; // not reached
}

static int ControlJoin(const char *n) {
    SleepArgs a = { 10, 7 };
    HANDLE h = CreateThread(nullptr, 0, SleepThenReturn, &a, 0, nullptr);
    if (!h) return Fail(n, "CreateThread returned null");
    if (WaitForSingleObject(h, kInfinite) != 0) return Fail(n, "wait did not return WAIT_OBJECT_0");
    DWORD code = 0;
    if (!GetExitCodeThread(h, &code)) return Fail(n, "GetExitCodeThread failed");
    if (code != 7) return Fail(n, "exit code is not 7");
    if (!CloseHandle(h)) return Fail(n, "CloseHandle failed");
    return Pass(n);
}

static int ControlAsan(const char *n) {
    volatile int *p = new int[4];
    delete[] p;
    p[1] = 5; // ASan must report this
    return Pass(n);
}

// The memcard record's hazard: CloseHandle while the thread runs, then the thread finishes.
// Win32: the thread runs on, and its exit code stays readable by id (OpenThread).
static int CloseWhileRunning(const char *n) {
    SleepArgs a = { 100, 5 };
    DWORD id = 0;
    HANDLE h = CreateThread(nullptr, 0, SleepThenReturn, &a, 0, &id);
    if (!h) return Fail(n, "CreateThread returned null");
    if (!CloseHandle(h)) return Fail(n, "CloseHandle failed");
    SleepMs(300); // the thread finishes and records its exit code after the close
    HANDLE h2 = OpenThread(0x40, 0, id);
    DWORD code = 0;
    if (!h2 || !GetExitCodeThread(h2, &code)) return Fail(n, "OpenThread/GetExitCodeThread failed");
    CloseHandle(h2);
    if (code != 5) return Fail(n, "exit code read by id is not 5");
    return Pass(n);
}

// WaitForSingleObject(thread, 0) is a poll: WAIT_TIMEOUT while the thread runs. The
// WaitForMultipleObjects shim polls every handle with timeout 0, so this must not block.
static int PollRunningThread(const char *n) {
    HANDLE ev = CreateEventA(nullptr, 1, 0, nullptr);
    HANDLE h = CreateThread(nullptr, 0, WaitEventThenReturn, ev, 0, nullptr);
    if (!h) return Fail(n, "CreateThread returned null");
    double t0 = NowMs();
    DWORD r = WaitForSingleObject(h, 0);
    if (r != kWaitTimeout) return Fail(n, "poll of a running thread did not return WAIT_TIMEOUT");
    r = WaitForSingleObject(h, 50);
    double dt = NowMs() - t0;
    if (r != kWaitTimeout) return Fail(n, "50 ms wait on a running thread did not time out");
    if (dt < 40) return Fail(n, "50 ms wait returned early");
    DWORD code = 0;
    GetExitCodeThread(h, &code);
    if (code != kStillActive) return Fail(n, "exit code of a running thread is not STILL_ACTIVE");
    SetEvent(ev);
    if (WaitForSingleObject(h, kInfinite) != 0) return Fail(n, "final wait failed");
    GetExitCodeThread(h, &code);
    if (code != 11) return Fail(n, "exit code is not 11");
    CloseHandle(h);
    CloseHandle(ev);
    return Pass(n);
}

// Two threads wait on one thread handle. Win32 wakes both; pthread_join from two threads is
// undefined behaviour.
static HANDLE gTarget;
static std::atomic<int> gWoke;
static DWORD WaitOnTarget(LPVOID) {
    if (WaitForSingleObject(gTarget, kInfinite) == 0) gWoke++;
    return 0;
}
static int TwoWaiters(const char *n) {
    SleepArgs a = { 100, 3 };
    gTarget = CreateThread(nullptr, 0, SleepThenReturn, &a, 0, nullptr);
    HANDLE w1 = CreateThread(nullptr, 0, WaitOnTarget, nullptr, 0, nullptr);
    HANDLE w2 = CreateThread(nullptr, 0, WaitOnTarget, nullptr, 0, nullptr);
    if (WaitForSingleObject(w1, 2000) != 0 || WaitForSingleObject(w2, 2000) != 0)
        return Fail(n, "a waiter did not return within 2 s");
    if (gWoke != 2) return Fail(n, "not both waiters saw WAIT_OBJECT_0");
    CloseHandle(w1);
    CloseHandle(w2);
    CloseHandle(gTarget);
    return Pass(n);
}

// A handle from OpenThread names the same thread as CreateThread's handle: a wait on it
// returns when the thread exits and reads its exit code.
static int OpenThreadWait(const char *n) {
    SleepArgs a = { 100, 4 };
    DWORD id = 0;
    HANDLE h = CreateThread(nullptr, 0, SleepThenReturn, &a, 0, &id);
    SleepMs(20);
    HANDLE h2 = OpenThread(0x100000 /* SYNCHRONIZE */, 0, id);
    if (!h2) return Fail(n, "OpenThread returned null");
    CloseHandle(h); // the opened handle alone must keep the thread object alive
    if (WaitForSingleObject(h2, 2000) != 0) return Fail(n, "wait on the opened handle failed");
    DWORD code = 0;
    if (!GetExitCodeThread(h2, &code) || code != 4) return Fail(n, "exit code via OpenThread is not 4");
    CloseHandle(h2);
    return Pass(n);
}

// ExitThread ends the thread with that exit code; waiters wake and read it.
static int ExitThreadCode(const char *n) {
    HANDLE h = CreateThread(nullptr, 0, CallsExitThread, nullptr, 0, nullptr);
    if (WaitForSingleObject(h, 2000) != 0) return Fail(n, "wait on an ExitThread thread failed");
    DWORD code = 0;
    GetExitCodeThread(h, &code);
    if (code != 9) return Fail(n, "exit code after ExitThread(9) is not 9");
    CloseHandle(h);
    return Pass(n);
}

// CloseHandle on an event while another thread waits on it with a timeout. Win32 keeps the
// object alive for the wait, which then times out, and CloseHandle returns at once. The old
// shim destroyed the condvar and mutex and freed the entry under the waiter: glibc's
// pthread_cond_destroy blocks until the waiter leaves (with an INFINITE waiter, forever), then
// the waiter unlocks a freed mutex. That unlock is inside uninstrumented libc, so ASan cannot
// see it; the observable is CloseHandle's duration.
static std::atomic<DWORD> gEventWaitResult;
static DWORD TimedEventWait(LPVOID p) {
    gEventWaitResult = WaitForSingleObject((HANDLE)p, 500);
    return 0;
}
static int CloseEventDuringWait(const char *n) {
    gEventWaitResult = 0xDEAD;
    HANDLE ev = CreateEventA(nullptr, 0, 0, nullptr);
    HANDLE w = CreateThread(nullptr, 0, TimedEventWait, ev, 0, nullptr);
    SleepMs(50); // the waiter is inside the wait
    double t0 = NowMs();
    if (!CloseHandle(ev)) return Fail(n, "CloseHandle on the event failed");
    if (NowMs() - t0 > 100) return Fail(n, "CloseHandle blocked on the waiter");
    if (WaitForSingleObject(w, 2000) != 0) return Fail(n, "waiter did not finish");
    CloseHandle(w);
    if (gEventWaitResult != kWaitTimeout) return Fail(n, "the timed wait did not return WAIT_TIMEOUT");
    return Pass(n);
}

// WaitForSingleObject(mutex, 0) on a mutex another thread owns: WAIT_TIMEOUT, not a block.
static HANDLE gMutex;
static DWORD HoldMutex(LPVOID p) {
    WaitForSingleObject(gMutex, kInfinite);
    WaitForSingleObject((HANDLE)p, kInfinite); // hold until released by the test
    ReleaseMutex(gMutex);
    return 0;
}
static int PollOwnedMutex(const char *n) {
    gMutex = CreateMutexA(nullptr, 0, nullptr);
    HANDLE go = CreateEventA(nullptr, 1, 0, nullptr);
    HANDLE t = CreateThread(nullptr, 0, HoldMutex, go, 0, nullptr);
    SleepMs(50);
    DWORD r = WaitForSingleObject(gMutex, 0);
    if (r != kWaitTimeout) return Fail(n, "poll of an owned mutex did not return WAIT_TIMEOUT");
    r = WaitForSingleObject(gMutex, 50);
    if (r != kWaitTimeout) return Fail(n, "50 ms wait on an owned mutex did not time out");
    SetEvent(go);
    WaitForSingleObject(t, kInfinite);
    if (WaitForSingleObject(gMutex, 0) != 0) return Fail(n, "released mutex not acquired");
    ReleaseMutex(gMutex);
    CloseHandle(t);
    CloseHandle(go);
    CloseHandle(gMutex);
    return Pass(n);
}

struct Case {
    const char *name;
    int (*fn)(const char *);
};
static const Case kCases[] = {
    { "control-join", ControlJoin },
    { "control-asan", ControlAsan },
    { "close-while-running", CloseWhileRunning },
    { "poll-running-thread", PollRunningThread },
    { "two-waiters", TwoWaiters },
    { "openthread-wait", OpenThreadWait },
    { "exitthread-code", ExitThreadCode },
    { "close-event-during-wait", CloseEventDuringWait },
    { "poll-owned-mutex", PollOwnedMutex },
};

int main(int argc, char **argv) {
    if (argc == 2 && !strcmp(argv[1], "--list")) {
        for (const Case &c : kCases) printf("%s\n", c.name);
        return 0;
    }
    if (argc != 2) {
        fprintf(stderr, "usage: %s <case> | --list\n", argv[0]);
        return 2;
    }
    for (const Case &c : kCases)
        if (!strcmp(argv[1], c.name)) return c.fn(c.name);
    fprintf(stderr, "unknown case %s\n", argv[1]);
    return 2;
}
