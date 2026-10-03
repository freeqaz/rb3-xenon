// Quazal NetZ - .\JobListenOnWellKnown.cpp
//
// The job that keeps trying to make the duplicated-object protocol listen on
// its well-known port: each run asks the protocol to grab the port, and after
// the run the job either completes (the port is held, or no attempts are
// left) or waits for the retry delay and runs again.
//
// The retail TU is 0x82ACE5B0..0x82ACE828 (five functions): Activate, which
// creates and queues the job (Station calls it; the TU's .rdata starts with the file string it
// passes to operator new, followed by the job's vtable), the constructor, the
// scalar deleting destructor, and the two overrides. The code after it is
// called only from other TUs, and the next .rdata object is another class's
// vtable. Built /Od /Oi- /EHs-c- /Ob1 /GR- (objects.json): no EH records and
// no RTTI locator before the vtable.
//
// The two remaining overrides (SkipWaitDelayAtTermination returning true, and
// the empty slot-10 virtual) are byte-identical to functions elsewhere, so the
// linker folded them and retail's vtable points at those copies.
//
// The declarations below are local to this TU; their layouts are the ones the
// retail code uses.

#include "Platform/qStd.h"

namespace Quazal {

    class String : public RootObject {
    public:
        String(const char *);
        ~String();

        char *m_szContent;
    };

    class DebugString {
    public:
        DebugString() {}
    };

    class Time {
    public:
        Time() : m_ullValue(0) {}
        ~Time() {}

        unsigned long long m_ullValue;
    };

    class InstantiationContext : public RootObject {
    public:
        unsigned int GetInstance(unsigned int);
        char m_pad[0x30];
    };

    // GetInstanceFromVector is a plain inline that /Ob1 does not expand where
    // it is reached through another inline, so its callers still reserve its
    // frame (W16-OE, section 6).
    class InstanceTable : public RootObject {
    public:
        unsigned int GetInstanceFromVector(unsigned int ui, unsigned int idx) {
            if (idx == 0) {
                return m_oDefaultContext.GetInstance(ui);
            } else if (idx >= m_pvContextVector->size()) {
                return -1;
            } else {
                return (*m_pvContextVector)[idx]->GetInstance(ui);
            }
        }

        InstantiationContext m_oDefaultContext; // 0x0
        qVector<InstantiationContext *> *m_pvContextVector; // 0x30
    };

    class InstanceControl : public RootObject {
    public:
        virtual ~InstanceControl();

        static InstanceTable s_oInstanceTable;

        unsigned int m_icInstanceContext; // 0x4
        unsigned int m_icInstanceType; // 0x8
        void *m_pDelegatorInstance; // 0xc
    };

    class PseudoSingleton : public InstanceControl {
    public:
        static unsigned int GetCurrentContext();
    };

    class RefCountedObject : public RootObject {
    public:
        RefCountedObject();
        virtual ~RefCountedObject();
        virtual RefCountedObject *AcquireRef();
        virtual void ReleaseRef();

        unsigned short m_ui16RefCount; // 0x4
    };

    class Job : public RefCountedObject {
    public:
        enum _State {
            Waiting = 1,
            Complete = 5,
        };

        Job(const DebugString &);
        virtual ~Job();
        virtual void DecoratedExecute();
        virtual void Execute() = 0;
        virtual void TestSuspendedJobState();
        virtual void AddActivity(const char *);
        virtual String GetTraceInfo() const;
        virtual void SetDefaultPostExecutionState();
        virtual bool SkipWaitDelayAtTermination();

        void SetToComplete();
        void Wait(unsigned int);
        unsigned int GetWaitDelay() const { return m_tiWaitDelay; }

        char m_pad8[0x14];
        _State m_eState; // 0x1c
        unsigned int m_tiWaitDelay; // 0x20
        char m_pad24[4];
        Time m_tDeadline; // 0x28
        char m_pad30[8];
    };

    class Scheduler;

    // The type-3 component; it owns the scheduler.
    class Core {
    public:
        static Core *GetInstance() {
            unsigned int uiContext = PseudoSingleton::GetCurrentContext();
            InstanceControl *inst =
                (InstanceControl *)InstanceControl::s_oInstanceTable.GetInstanceFromVector(3, uiContext);
            Core *pCore;
            if (inst) {
                pCore = (Core *)inst->m_pDelegatorInstance;
            } else {
                pCore = 0;
            }
            return pCore;
        }
        Scheduler *GetScheduler() { return m_pScheduler; }

        char m_pad0[8];
        Scheduler *m_pScheduler; // 0x8
    };

    class Scheduler {
    public:
        static Scheduler *GetInstance() {
            Core *inst = Core::GetInstance();
            if (inst == 0) {
                return 0;
            } else {
                return inst->GetScheduler();
            }
        }
        void Queue(Job *, bool);
    };

    // The type-4 instance (.\DOCore.cpp).
    class DOCore {
    public:
        static DOCore *GetInstance(unsigned int uiContext) {
            return (DOCore *)InstanceControl::s_oInstanceTable.GetInstanceFromVector(4, uiContext);
        }
        static DOCore *GetInstance() { return GetInstance(PseudoSingleton::GetCurrentContext()); }
        bool IsTerminated() const;
    };

    class ObjDupProtocol {
    public:
        static ObjDupProtocol *GetInstance();
        bool ShouldGrabWellKnown();
        bool ListenOnWellKnown();
        bool IsListeningOnWellKnown() const;
    };

    class JobListenOnWellKnown : public Job {
    public:
        JobListenOnWellKnown(unsigned int uiNbAttempts, unsigned int tiRetryDelay);
        virtual ~JobListenOnWellKnown() {}
        virtual void Execute();
        virtual void SetDefaultPostExecutionState();
        virtual bool SkipWaitDelayAtTermination() { return true; }
        virtual void TraceDescription(unsigned int) {}

        static void Activate();

        unsigned int m_uiNbAttemptsLeft; // 0x38
    };

    void JobListenOnWellKnown::Activate() {
        Job *pJob = new (__FILE__, 24) JobListenOnWellKnown(10, 500);
        Scheduler::GetInstance()->Queue(pJob, false);
    }

    JobListenOnWellKnown::JobListenOnWellKnown(unsigned int uiNbAttempts, unsigned int tiRetryDelay)
        : Job(DebugString()) {
        m_uiNbAttemptsLeft = uiNbAttempts;
        m_tiWaitDelay = tiRetryDelay;
    }

    void JobListenOnWellKnown::Execute() {
        if (DOCore::GetInstance()->IsTerminated()) {
            SetToComplete();
        }
        ObjDupProtocol *pProtocol = ObjDupProtocol::GetInstance();
        if (pProtocol->ShouldGrabWellKnown()) {
            pProtocol->ListenOnWellKnown();
        }
    }

    void JobListenOnWellKnown::SetDefaultPostExecutionState() {
        ObjDupProtocol *pProtocol = ObjDupProtocol::GetInstance();
        if (pProtocol->IsListeningOnWellKnown()) {
            SetToComplete();
        } else if (m_uiNbAttemptsLeft == 0) {
            SetToComplete();
        } else {
            m_uiNbAttemptsLeft--;
            Wait(GetWaitDelay());
        }
    }

}
