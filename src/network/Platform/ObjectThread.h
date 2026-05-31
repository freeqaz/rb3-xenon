#pragma once
#include "Platform/MemoryManager.h"
#include "Platform/RootObject.h"
#include "Platform/String.h"
// Xbox 360 port: replace revolution/os OSThread/OSAlarm with Win32/XBOXKRNL handles
#include "xdk/XAPILIB.h"

namespace Quazal {
    class ObjectThreadRoot : public RootObject {
    public:
        class Handle : public RootObject {
        public:
            Handle() : mThread(0), mStack(0), mHasJoined(0) {}
            ~Handle() {
                if (mThread) {
                    if (!mHasJoined) {
                        // Xbox 360: WaitForSingleObject + CloseHandle (vs Wii OSJoinThread)
                        WaitForSingleObject(mThread, 0xFFFFFFFF);
                    }
                    CloseHandle(mThread);
                    mThread = 0;
                    // mStack is NULL on Xbox (OS-managed stack); no free needed
                }
            }

            HANDLE mThread; // 0x0 (was OSThread* on Wii, same pointer size)
            char *mStack;   // 0x4 (unused on Xbox, kept for layout parity)
            bool mHasJoined; // 0x8
        };
        ObjectThreadRoot(const String &);
        virtual ~ObjectThreadRoot();
        virtual void CallObjectMethod() = 0;

        void Launch();
        bool Wait(unsigned int);
        void ReadyToRun();

        static unsigned int GetCurrentThreadID();
        static unsigned int s_uiDefaultPrio;

        String mName; // 0x4
        Handle *mHandle; // 0x8
        unsigned int mThreadID; // 0xc
        unsigned int mThreadPrio; // 0x10
        bool mLaunched; // 0x14
        bool mRunning; // 0x15
        bool mFinished; // 0x16
    };

    template <class T1, class T2>
    class ObjectThread : public ObjectThreadRoot {
    public:
        typedef void (T1::*ObjectFunc)(T2);

        ObjectThread(const String &s) : ObjectThreadRoot(s), m_pTargetObject(0) {
            m_pMethod = 0;
        }
        virtual ~ObjectThread() {}
        virtual void CallObjectMethod() {
            T1 *obj = m_pTargetObject;
            ObjectFunc method = m_pMethod;
            T2 arg = m_arg;
            ReadyToRun();
            (obj->*method)(arg);
        }

        void Update(T1 *obj, ObjectFunc func, T2 arg, bool scheduled) {
            m_pTargetObject = obj;
            m_pMethod = func;
            m_arg = arg;
            m_bScheduled = scheduled;
            Launch();
        }

        ObjectFunc m_pMethod; // 0x18
        T1 *m_pTargetObject; // 0x24
        T2 m_arg; // 0x28
        bool m_bScheduled;
    };
}