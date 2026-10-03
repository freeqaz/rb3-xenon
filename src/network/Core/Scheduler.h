#pragma once
#include "Core.h"
#include "Core/Job.h"
#include "Platform/CriticalSection.h"
#include "Platform/EventHandler.h"
#include "Platform/HighResolutionChrono.h"
#include "Platform/ProfilingUnit.h"
#include "Platform/RootObject.h"
#include "Platform/WaterMark.h"
#include "Platform/qChain.h"
#include "SingleThreadCallPolicy.h"
#include "WorkerThreads.h"

namespace Quazal {
    class Scheduler : public RootObject {
    public:
        class SchedulerWorkerThread : public WorkerThreads {
        public:
            SchedulerWorkerThread() : m_pScheduler(0) {}
            virtual ~SchedulerWorkerThread() {}
            virtual void Initialize() {}
            virtual void Work() { m_pScheduler->Dispatch(0x64, 1); }

            Scheduler *m_pScheduler; // 0x28
        };

        Scheduler(unsigned char, SchedulerWorkerThread *);
        virtual ~Scheduler();

        void StartDispatcherThread();
        void Dispatch(unsigned int, unsigned int);
        void Queue(Job *, bool);
        bool Cancel(Job *);

        static bool CurrentThreadCanWaitForJob();
        static void GlobalSingleThreadDispatch(unsigned int);
        static CriticalSection s_csGlobalSystemLock;
        // The braced if/else with an explicit == 0 test is what retail's /Od
        // callers' frames require (the unbraced and !p forms move their
        // temporaries).
        static Scheduler *GetInstance() {
            Core *pCore = Core::GetInstance();
            if (pCore == 0) {
                return 0;
            } else {
                return pCore->GetScheduler();
            }
        }

        // Retail 0x82A6F650. Inline, but /Ob1 rejects it at every call site
        // (callers reserve its frame), so it is always called out of line.
        // Retail's out-of-line copy has a 0x80 frame (temporaries at 0x68 and
        // 0x6c); ours has 0x70 (0x60, 0x64). Its callers' reservations match
        // ours, so the extra eight bytes are not from this definition.
        static CriticalSection *GetSystemLock() { return &GetInstance()->unk38; }

        int unk4; // 0x4
        bool unk8; // 0x8
        SchedulerWorkerThread *m_pWorkerThreads; // 0xc
        EventHandler unk10; // 0x10
        Event *unk34; // 0x34
        CriticalSection unk38;
        CriticalSection unk4c;
        qChain<Job *> unk60;
        qChain<Job *> unk70;
        qChain<Job *> unk80;
        // 0x94, 0x18 bytes: retail's ctor (0x82AC57F0) builds a bare tree here and puts
        // unka8 at 0xac, so this is not a qMap (whose RootObject base makes it 0x1c,
        // as StationURL's maps are). map vs multimap is not decidable from the ctor.
        std::multimap<Time, Job *, std::less<Time>, MemAllocator<std::pair<const Time, Job *> > >
            unk90;
        bool unka8;
        WaterMark unkac;
        WaterMark unkdc;
        ProfilingUnit unk110;
        Time unk158;
        Time unk160;
        unsigned int unk168;
        SingleThreadCallPolicy unk16c;
        ProfilingUnit unk180;
        HighResolutionChrono unk1c8;
        bool unk1d8;
        bool unk1d9;
    };
}
