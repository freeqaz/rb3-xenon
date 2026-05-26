// DC3 Native Port - ThreadCall Implementation
// Replaces ThreadCall_Win.cpp - uses pthreads

#include "os/ThreadCall.h"
#include "os/CritSec.h"
#include "os/Debug.h"
#include "xdk/XAPILIB.h"

#ifndef __EMSCRIPTEN__
#include <pthread.h>
#endif
#include <cstring>

namespace {
    bool gReadyForNext = true;
    ThreadCallData gData[12];
    HANDLE gThreadHandle;
    HANDLE gThreadSema;
    bool gTerminate;
    bool gCallDone;
    int gCurCall;
    int gFreeCall;

    void *MyThreadFunc(void *) {
        WaitForSingleObject(gThreadSema, 0xFFFFFFFF);
        while (!gTerminate) {
            switch (gData[gCurCall].mType) {
            case kTCDT_Func:
                gData[gCurCall].mArg = gData[gCurCall].mFunc();
                gCallDone = true;
                break;
            case kTCDT_Class:
                gData[gCurCall].mArg = gData[gCurCall].mClass->ThreadStart();
                gCallDone = true;
                break;
            default:
                MILO_ASSERT(false, 199);
                break;
            }
            WaitForSingleObject(gThreadSema, 0xFFFFFFFF);
        }
        CloseHandle(gThreadSema);
        return nullptr;
    }
}

u32 gMainThreadID = -1;

namespace JobQueue {
    struct Entry {
        int lol;
    };

    int shouldExit;
    int numWorkers;
    int maxJobs;
    volatile int numIdle;
    Entry *allEntries;
    Entry *idleEntries;
    Entry *waitingEntries;
    void **jobThreadHandle;
    CriticalSection jobQueueMutex;
}

void ThreadCallInit() {
    memset(gData, 0, sizeof(gData));
    gCurCall = 0;
    gFreeCall = 0;
#ifdef __EMSCRIPTEN__
    // Single-threaded WASM: no worker thread. Work runs synchronously in ThreadCallPoll().
    gThreadSema = nullptr;
    gThreadHandle = nullptr;
#else
    gThreadSema = CreateSemaphoreA(nullptr, 0, 1, nullptr);
    if (!gThreadSema) {
        MILO_LOG("CreateSemaphore() failed.\n");
    } else {
        gThreadHandle = CreateThread(nullptr, 0x10000, (DWORD (*)(LPVOID))MyThreadFunc, nullptr, 0, nullptr);
        if (!gThreadHandle) {
            MILO_LOG("CreateThread() failed.\n");
        }
    }
#endif
}

void ThreadCall(ThreadCallFunc *func, ThreadCallCallbackFunc *callback) {
    ThreadCallData &data = gData[gFreeCall];
    MILO_ASSERT(data.mType == kTCDT_None, 0x6E);
    data.mFunc = func;
    data.mCallback = callback;
    data.mType = kTCDT_Func;
    data.mClass = nullptr;
    gFreeCall = (gFreeCall + 1) % 12;
}

void ThreadCall(ThreadCallback *callback) {
    ThreadCallData &data = gData[gFreeCall];
    MILO_ASSERT(data.mType == kTCDT_None, 0x7B);
    data.mType = kTCDT_Class;
    data.mFunc = nullptr;
    data.mCallback = nullptr;
    data.mClass = callback;
    gFreeCall = (gFreeCall + 1) % 12;
}

void ThreadCallPoll() {
#ifdef __EMSCRIPTEN__
    // Single-threaded WASM: run one pending job synchronously per poll
    ThreadCallData &data = gData[gCurCall];
    if (data.mType != kTCDT_None) {
        int result = 0;
        ThreadCallDataType type = data.mType;
        switch (type) {
        case kTCDT_Func:
            result = data.mFunc();
            data.mType = kTCDT_None;
            gCurCall = (gCurCall + 1) % 12;
            data.mCallback(result);
            break;
        case kTCDT_Class: {
            ThreadCallback *cls = data.mClass;
            result = cls->ThreadStart();
            data.mType = kTCDT_None;
            gCurCall = (gCurCall + 1) % 12;
            cls->ThreadDone(result);
            break;
        }
        default:
            break;
        }
    }
#else
    if (gCallDone) {
        ThreadCallData &data = gData[gCurCall];
        ThreadCallDataType oldType = data.mType;
        if (data.mType) {
            data.mType = kTCDT_None;
            gCallDone = false;
            gCurCall = (gCurCall + 1) % 12;
            gReadyForNext = true;
            switch (oldType) {
            case kTCDT_None:
                MILO_ASSERT(false, 0x97);
                break;
            case kTCDT_Func:
                data.mCallback(data.mArg);
                break;
            case kTCDT_Class:
                data.mClass->ThreadDone(data.mArg);
                break;
            }
        }
    }
    if (gReadyForNext && gData[gCurCall].mType != kTCDT_None) {
        gReadyForNext = false;
        ReleaseSemaphore(gThreadSema, 1, nullptr);
    }
#endif
}

void ThreadCallPreInit() { gMainThreadID = GetCurrentThreadId(); }

void ThreadCallTerminate() {
#ifndef __EMSCRIPTEN__
    if (gThreadHandle) {
        gTerminate = true;
        ReleaseSemaphore(gThreadSema, 1, nullptr);
    }
#endif
}
