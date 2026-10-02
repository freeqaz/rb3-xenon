// Quazal NetZ - .\CallRegister.cpp
//
// The retail TU is 0x82ABAB58..0x82ABD488: the CallRegister methods, then the
// ItemRegister<DOCallContext> base, the two MethodCallJob instantiations the
// register queues, the PeriodicJob vtable they need, and the
// map<unsigned short, DOCallContext *> helpers. It is built /Od /Ob1 with EH
// off, so every helper the classes below define in the class body is expanded
// in place (one level deep), while the ones defined out of line are called.
//
// The surrounding NetZ classes are declared here only as far as this TU uses
// them; their members are defined in other TUs.

#include "Core/InstanceControl.h"
#include "Core/PseudoSingleton.h"
#include "Platform/StringStream.h"
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
        Time(unsigned long long ullValue) : m_ullValue(ullValue) {}
        ~Time() {}
        bool operator==(const Time &t) const { return m_ullValue == t.m_ullValue; }
        bool operator>(const Time &t) const { return m_ullValue > t.m_ullValue; }

        static Time GetTime();

        unsigned long long m_ullValue;
    };

    class UserContext {
    public:
        UserContext(unsigned int uiValue) : m_uiValue(uiValue) {}
        ~UserContext() {}

        unsigned int m_uiValue;
    };

    class DOHandle {
    public:
        DOHandle() : m_uiValue(0) {}
        DOHandle(const DOHandle &o) : m_uiValue(o.m_uiValue) {}
        ~DOHandle() {}
        bool operator==(const DOHandle &o) const { return m_uiValue == o.m_uiValue; }

        unsigned int m_uiValue;
    };

    StringStream &operator<<(StringStream &, const String &);
    StringStream &operator<<(StringStream &, const DOHandle &);

    class RefCountedObject : public RootObject {
    public:
        RefCountedObject();
        virtual ~RefCountedObject();
        virtual RefCountedObject *AcquireRef();
        virtual void ReleaseRef();

        unsigned short m_ui16RefCount; // 0x4
    };

    class CallContext : public RefCountedObject {
    public:
        enum _State {
            CallInit = 0,
            CallPending = 1,
            CallSuccess = 2,
            CallError = 3,
            CallCancelled = 4,
        };

        virtual ~CallContext();
        virtual bool FlagsAreValid() const;
        virtual void BeginTransition(_State, int, bool);
        virtual void ProcessCallCompletion();

        _State GetState() const { return m_eState; }
        bool HasTimedOut(Time tNow) {
            if (m_tTimeout == Time(0)) {
                return false;
            } else {
                return !!(tNow > m_tTimeout);
            }
        }

        unsigned int m_uiFlags; // 0x8
        _State m_eState; // 0xc
        char m_pad10[0x38];
        Time m_tTimeout; // 0x48
    };

    // A reference to a duplicated object: the object, then its handle.
    class DOCoreRef {
    public:
        DOHandle GetHandle() const { return m_hObject; }

        void *m_pObject; // 0x0
        DOHandle m_hObject; // 0x4
    };

    class DOCallContext : public CallContext {
    public:
        enum _Outcome {
        };

        virtual ~DOCallContext();
        virtual void ProcessResponse(UserContext, _State *, _Outcome *);
        virtual void ProcessOutcome(DOHandle, _Outcome);
        virtual void Trace(unsigned int);
        virtual int GetType() const = 0;
        virtual void ProcessFault();

        void SetID(unsigned short);
        unsigned short GetID() const { return m_usID; }
        DOHandle GetTargetStation() const { return m_hTargetStation; }
        DOHandle GetTargetObject() const { return m_refTarget.GetHandle(); }
        void SignalResponse(UserContext);
        bool InternalCancel(_State, _Outcome);

        unsigned short m_usID; // 0x50
        char m_pad54[8];
        DOHandle m_hTargetStation; // 0x5c
        char m_pad60[4];
        DOCoreRef m_refTarget; // 0x64
        char m_pad6c[0x3c];
    };

    class FetchContext : public DOCallContext {};
    class MigrationContext : public DOCallContext {};

    class Job : public RefCountedObject {
    public:
        Job(const DebugString &);
        virtual ~Job();
        virtual void DecoratedExecute();
        virtual void Execute() = 0;
        virtual void TestSuspendedJobState();
        virtual void AddActivity(const char *);
        virtual String GetTraceInfo() const;
        virtual void SetDefaultPostExecutionState();
        virtual bool SkipWaitDelayAtTermination();

        char m_pad8[0x20];
        Time m_tDeadline; // 0x28
        char m_pad30[8];
    };

    class PeriodicJob : public Job {
    public:
        PeriodicJob(const DebugString &);
        virtual ~PeriodicJob() {}
        virtual void SetDefaultPostExecutionState();
        virtual bool SkipWaitDelayAtTermination();

        void SetPeriod(int tiPeriod) { m_tiPeriod = tiPeriod; }

        int m_tiPeriod; // 0x38
    };

    template <class T1, class T2, class T3>
    class MethodCallJob : public T3 {
    public:
        typedef void (T1::*JobFunc)(T2);

        MethodCallJob(const String &strName, T1 *pTarget, JobFunc pMethod, T2 arg)
            : T3(DebugString()), m_pTargetObject(pTarget) {
            m_pMethod = pMethod;
            m_arg = arg;
        }
        virtual ~MethodCallJob() {}
        virtual void Execute() {
            T1 *pTarget = m_pTargetObject;
            JobFunc pMethod = m_pMethod;
            (pTarget->*pMethod)(m_arg);
        }
        virtual String GetTraceInfo() const {
            StringStream ss;
            ss << T3::GetTraceInfo();
            ss << "\tArgument: " << m_arg;
            return String(ss.m_szBuffer);
        }

        JobFunc m_pMethod;
        T1 *m_pTargetObject;
        T2 m_arg;
    };

    class Scheduler {
    public:
        void Queue(Job *, bool);

        char m_pad0[0x3C];
        CriticalSection m_csSystemLock; // 0x3C
    };

    class ScopedCS {
    public:
        ScopedCS(CriticalSection &cs) : m_bInScope(true), m_pCS(&cs) {
            CriticalSection *pCS = m_pCS;
            if (!MutexPrimitive::s_bNoOp) {
                pCS->EnterImpl();
            }
        }
        ~ScopedCS() { EndScope(); }
        void EndScope() {
            if (m_bInScope) {
                CriticalSection *pCS = m_pCS;
                if (!MutexPrimitive::s_bNoOp) {
                    pCS->LeaveImpl();
                }
                m_bInScope = false;
            }
        }

        bool m_bInScope; // 0x0
        CriticalSection *m_pCS; // 0x4
    };

    // The type-3 component; it owns the scheduler.
    class SchedulerHolder {
    public:
        static SchedulerHolder *GetInstance() {
            InstanceControl *inst =
                (InstanceControl *)InstanceControl::s_oInstanceTable.GetInstanceFromVector(
                    3, PseudoSingleton::GetCurrentContext()
                );
            return inst ? (SchedulerHolder *)inst->m_pDelegatorInstance : 0;
        }
        Scheduler *GetScheduler() { return m_pScheduler; }

        char m_pad0[8];
        Scheduler *m_pScheduler; // 0x8
    };

    inline Scheduler *GetScheduler() {
        SchedulerHolder *pHolder = SchedulerHolder::GetInstance();
        if (pHolder == 0) {
            return 0;
        } else {
            return pHolder->GetScheduler();
        }
    }

    // A register of reference-counted items keyed by their 16-bit ID.
    template <class T>
    class ItemRegister : public RootObject {
    public:
        // Walks the register under the system lock, holding a reference on the
        // current item.
        class Iterator {
        public:
            Iterator(ItemRegister *pRegister) : m_pRegister(pRegister), m_pCurrent(0) {
                GotoStart();
            }
            ~Iterator() {
                if (m_pCurrent != 0) {
                    m_pCurrent->ReleaseRef();
                }
            }

            void GotoStart() {
                T *pItem = 0;
                if (m_pRegister != 0) {
                    ScopedCS oCS(GetScheduler()->m_csSystemLock);
                    pItem = m_pRegister->GetFirst();
                    if (pItem != 0) {
                        pItem->AcquireRef();
                    }
                }
                SetCurrent(pItem);
                if (pItem != 0) {
                    pItem->ReleaseRef();
                }
            }
            void Next() {
                if (m_pCurrent != 0) {
                    T *pItem = 0;
                    {
                        ScopedCS oCS(GetScheduler()->m_csSystemLock);
                        pItem = m_pRegister->GetNext(m_pCurrent->GetID());
                        if (pItem != 0) {
                            pItem->AcquireRef();
                        }
                    }
                    SetCurrent(pItem);
                    if (pItem != 0) {
                        pItem->ReleaseRef();
                    }
                }
            }
            bool EndReached() { return m_pCurrent == 0; }
            T *operator*() { return m_pCurrent; }

            void SetCurrent(T *pItem) {
                if (pItem != 0) {
                    pItem->AcquireRef();
                }
                if (m_pCurrent != 0) {
                    m_pCurrent->ReleaseRef();
                }
                m_pCurrent = pItem;
            }

            ItemRegister *m_pRegister; // 0x0
            T *m_pCurrent; // 0x4
        };

        ItemRegister() {}
        virtual ~ItemRegister() {}
        virtual void Register(T *pItem) {
            ScopedCS oCS(GetScheduler()->m_csSystemLock);
            pItem->AcquireRef();
            m_mapItems[pItem->GetID()] = pItem;
        }
        virtual void Unregister(T *pItem) {
            ScopedCS oCS(GetScheduler()->m_csSystemLock);
            typename qMap<unsigned short, T *>::iterator it = m_mapItems.find(pItem->GetID());
            m_mapItems.erase(it);
            pItem->ReleaseRef();
        }

        void UnregisterAll() {
            ScopedCS oCS(GetScheduler()->m_csSystemLock);
            typename qMap<unsigned short, T *>::iterator it;
            while (!m_mapItems.empty()) {
                it = m_mapItems.begin();
                Unregister(it->second);
            }
        }
        T *GetFirst() {
            typename qMap<unsigned short, T *>::iterator it = m_mapItems.begin();
            if (it != m_mapItems.end()) {
                return it->second;
            } else {
                return 0;
            }
        }
        T *GetNext(unsigned short usID) {
            typename qMap<unsigned short, T *>::iterator it = m_mapItems.upper_bound(usID);
            if (it != m_mapItems.end()) {
                return it->second;
            } else {
                return 0;
            }
        }

        qMap<unsigned short, T *> m_mapItems; // 0x4
    };

    class CallRegister;

    class ObjDupProtocol {
    public:
        static ObjDupProtocol *GetInstance();
        CallRegister *GetCallRegister() { return (CallRegister *)m_oCallRegister; }

        char m_pad0[8];
        char m_oCallRegister[0x28]; // 0x8
    };

    class CallRegister : public ItemRegister<DOCallContext> {
    public:
        CallRegister();
        virtual ~CallRegister();
        virtual void Register(DOCallContext *);
        virtual void Unregister(DOCallContext *);

        static CallRegister &GetInstanceRef();
        void Start();
        void CheckExpiredCalls(int);
        unsigned short GenerateCallID();
        void Trace(unsigned int);
        void CancelCallToStation(DOHandle);
        void QueueCancelCallToStation(DOHandle);
        void CancelPendingCalls();
        void CancelPeriodicJobs();
        void CancelExpiredCalls();
        void SignalRelevantFetchContextes(DOHandle, DOCallContext::_Outcome);
        FetchContext *GetFetchContext(DOHandle, DOHandle);
        bool MigrationInProgress(DOHandle, DOHandle);

        DOCallContext *FindCallContext(unsigned short usID) {
            qMap<unsigned short, DOCallContext *>::iterator it = m_mapItems.find(usID);
            if (it != m_mapItems.end()) {
                return it->second;
            } else {
                return 0;
            }
        }

        unsigned short m_usNextID; // 0x20
        MethodCallJob<CallRegister, int, PeriodicJob> *m_pCheckExpiredCallsJob; // 0x24
    };

    CallRegister::CallRegister() : m_usNextID(1), m_pCheckExpiredCallsJob(0) {}

    CallRegister::~CallRegister() { UnregisterAll(); }

    CallRegister &CallRegister::GetInstanceRef() {
        return *ObjDupProtocol::GetInstance()->GetCallRegister();
    }

    void CallRegister::Start() {
        m_pCheckExpiredCallsJob = new (__FILE__, 0x2C) MethodCallJob<CallRegister, int, PeriodicJob>(
            "CallRegister::CheckExpiredCalls", this, &CallRegister::CheckExpiredCalls, 0
        );
        m_pCheckExpiredCallsJob->SetPeriod(100);
        m_pCheckExpiredCallsJob->AcquireRef();
        GetScheduler()->Queue(m_pCheckExpiredCallsJob, false);
    }

    void CallRegister::CheckExpiredCalls(int) { CancelExpiredCalls(); }

    unsigned short CallRegister::GenerateCallID() {
        unsigned short usID = m_usNextID;
        m_usNextID++;
        while (m_usNextID == 0 || FindCallContext(m_usNextID) != 0) {
            m_usNextID++;
        }
        return usID;
    }

    void CallRegister::Register(DOCallContext *pContext) {
        ScopedCS oCS(GetScheduler()->m_csSystemLock);
        pContext->SetID(GenerateCallID());
        ItemRegister<DOCallContext>::Register(pContext);
    }

    void CallRegister::Unregister(DOCallContext *pContext) {
        ScopedCS oCS(GetScheduler()->m_csSystemLock);
        ItemRegister<DOCallContext>::Unregister(pContext);
        pContext->SetID(0);
    }

    void CallRegister::Trace(unsigned int uiFlag) {
        Iterator it(this);
        while (!it.EndReached()) {
            (*it)->Trace(uiFlag);
            it.Next();
        }
    }

    void CallRegister::CancelCallToStation(DOHandle hStation) {
        Iterator it(this);
        while (!it.EndReached()) {
            DOCallContext *pContext = *it;
            if (pContext->GetTargetStation() == hStation) {
                (*it)->ProcessFault();
            }
            it.Next();
        }
    }

    void CallRegister::QueueCancelCallToStation(DOHandle hStation) {
        MethodCallJob<CallRegister, DOHandle, Job> *pJob =
            new (__FILE__, 0x73) MethodCallJob<CallRegister, DOHandle, Job>(
                "CallRegister::CancelCallToStation", this, &CallRegister::CancelCallToStation, hStation
            );
        GetScheduler()->Queue(pJob, false);
    }

    void CallRegister::CancelPendingCalls() {
        Iterator it(this);
        while (!it.EndReached()) {
            DOCallContext *pContext = *it;
            pContext->InternalCancel(CallContext::CallError, (DOCallContext::_Outcome)0x80060003);
            it.Next();
        }
    }

    void CallRegister::CancelPeriodicJobs() {
        m_pCheckExpiredCallsJob->SetPeriod(0);
        m_pCheckExpiredCallsJob->ReleaseRef();
        m_pCheckExpiredCallsJob = 0;
    }

    void CallRegister::CancelExpiredCalls() {
        Iterator it(this);
        Time tNow = Time::GetTime();
        while (!it.EndReached()) {
            DOCallContext *pContext = *it;
            if (pContext->GetState() == CallContext::CallPending && pContext->HasTimedOut(tNow)) {
                DOHandle hStation = pContext->m_hTargetStation;
                pContext->InternalCancel(CallContext::CallError, (DOCallContext::_Outcome)0x80060006);
            }
            it.Next();
        }
    }

    void CallRegister::SignalRelevantFetchContextes(DOHandle hObject, DOCallContext::_Outcome eOutcome) {
        Iterator it(this);
        while (!it.EndReached()) {
            DOCallContext *pContext = *it;
            if (pContext->GetType() == 1) {
                FetchContext *pFetch = (FetchContext *)pContext;
                if (pFetch->m_refTarget.m_hObject == hObject) {
                    pContext->SignalResponse(UserContext(eOutcome));
                }
            }
            it.Next();
        }
    }

    FetchContext *CallRegister::GetFetchContext(DOHandle hObject, DOHandle hStation) {
        Iterator it(this);
        while (!it.EndReached()) {
            DOCallContext *pContext = *it;
            if (pContext->GetType() == 1 && pContext->GetState() == CallContext::CallPending) {
                FetchContext *pFetch = (FetchContext *)pContext;
                if (pFetch->GetTargetObject() == hObject) {
                    if (hStation == DOHandle() || pFetch->GetTargetStation() == hStation) {
                        return pFetch;
                    }
                }
            }
            it.Next();
        }
        return 0;
    }

    bool CallRegister::MigrationInProgress(DOHandle hObject, DOHandle hStation) {
        Iterator it(this);
        while (!it.EndReached()) {
            DOCallContext *pContext = *it;
            if (pContext->GetType() == 2 && pContext->GetState() == CallContext::CallPending) {
                MigrationContext *pMigration = (MigrationContext *)pContext;
                if (pMigration->GetTargetObject() == hObject) {
                    if (hStation == DOHandle()) {
                        return true;
                    }
                    if (pMigration->GetTargetStation() == hStation) {
                        return true;
                    }
                }
            }
            it.Next();
        }
        return false;
    }
}
