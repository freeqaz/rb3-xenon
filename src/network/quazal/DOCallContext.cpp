// Quazal NetZ - .\DOCallContext.cpp
//
// The call context of a duplicated-object call (an RMC or action sent to a
// DO's station): it tracks the target station, the outcome, and an event the
// caller can wait on.
//
// The retail TU is 0x82AA06E0..0x82AA1650. It starts at the constructor (the
// functions before it are DDLDeclarations' static initialisers, and its vtable
// at 0x8217F270 follows DDLDeclarations' vtables in .rdata) and ends at the
// 8-byte EH prefix of StationURL's constructor.
//
// Built /Od /Ob1 with EH off, like the other Quazal TUs: the helpers the
// classes below define in the class body are expanded in place (their `this`,
// arguments and return values spilled to stack temps), the ones defined out of
// line are called. The NetZ classes are declared here only as far as this TU
// uses them; their members are defined in other TUs. At /Od the local names
// set the stack layout, so they were chosen to reproduce retail's frames.

#include "Core/InstantiationContext.h"
#include "Platform/CriticalSection.h"
#include "Platform/RefCountedObject.h"
#include "Platform/Result.h"
#include "Platform/ScopedCS.h"
#include "Platform/SystemError.h"
#include "Platform/Time.h"
#include "Platform/qStd.h"

namespace Quazal {

    // Retail's constructor assigns the local station through an expanded
    // operator= (its `this` and right-hand side both go to stack temps), so the
    // handle is declared here with one.
    class DOHandle : public RootObject {
    public:
        DOHandle(unsigned int uiValue = 0) : mValue(uiValue) {}
        DOHandle(const DOHandle &h) : mValue(h.mValue) {}
        ~DOHandle() {}
        DOHandle &operator=(const DOHandle &h) {
            mValue = h.mValue;
            return *this;
        }
        bool operator==(const DOHandle &h) const { return mValue == h.mValue; }

        unsigned int mValue; // 0x0
    };

    // The instance lookup behind every NetZ singleton, in the shape of the
    // shared Core/InstanceTable.h, Core/Core.h and Core/Scheduler.h (those
    // headers are not included because Core.h brings in the shared
    // CallContext declaration, which this file declares in full below).
    class InstanceTable : public RootObject {
    public:
        __declspec(noinline) unsigned int GetInstanceFromVector(unsigned int ui, unsigned int idx) {
            if (idx == 0) {
                return m_oDefaultContext.GetInstance(ui);
            } else if (idx >= m_pvContextVector->size()) {
                SystemError::SignalError(0, 0, 0xe0000003, 0);
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
        static InstanceTable s_oInstanceTable;

        void *m_pVTable; // 0x0
        unsigned int m_icInstanceContext; // 0x4
        unsigned int m_icInstanceType; // 0x8
        void *m_pDelegatorInstance; // 0xc
    };

    class PseudoSingleton {
    public:
        static unsigned int GetCurrentContext();
    };

    class DuplicatedObject {
    public:
        bool IsADuplicationMaster() const;
    };

    class DORef : public RootObject {
    public:
        DORef();
        ~DORef();
        DORef &operator=(const DOHandle &);
        void Release();
        DuplicatedObject *GetDOPtr() const { return m_poReferencedDO; }

        DuplicatedObject *m_poReferencedDO; // 0x0
        DOHandle m_hReferencedDO; // 0x4
        bool m_bLockRelevance; // 0x8
    };

    class Station {
    public:
        static DOHandle GetLocalStation();
    };

    class Event : public RootObject {
    public:
        void Set();
        void Reset();
    };

    class EventHandler : public RootObject {
    public:
        EventHandler(unsigned short);
        ~EventHandler();
        Event *CreateEventObject(unsigned int, unsigned int);
        void DeleteEventObject(Event *);
        bool WaitForEvent(unsigned int, Event **) const;

        char m_pad[0x24];
    };

    class SingleThreadCallPolicy {
    public:
        bool CurrentThreadIsDispatchingJobs() const;
    };

    class Scheduler;

    class Core : public RefCountedObject {
    public:
        static Core *GetInstance() {
            unsigned int uiContext = PseudoSingleton::GetCurrentContext();
            InstanceControl *inst =
                (InstanceControl *)InstanceControl::s_oInstanceTable.GetInstanceFromVector(3, uiContext);
            Core *pCore = inst ? (Core *)inst->m_pDelegatorInstance : nullptr;
            return pCore;
        }

        Scheduler *GetScheduler() { return m_pScheduler; }

        Scheduler *m_pScheduler; // 0x8
    };

    class Scheduler : public RootObject {
    public:
        static bool CurrentThreadCanWaitForJob();
        static void GlobalSingleThreadDispatch(unsigned int);
        static Scheduler *GetInstance() {
            Core *inst = Core::GetInstance();
            if (!inst)
                return nullptr;
            else
                return inst->GetScheduler();
        }

        char m_pad0[0x3c];
        CriticalSection m_csSystemLock; // 0x3c
        char m_pad50[0x11c];
        SingleThreadCallPolicy m_oCallPolicy; // 0x16c
    };

    // Spins on a condition with a timeout, dispatching jobs in between.
    class SpinTest {
    public:
        SpinTest(unsigned int, unsigned int);
        ~SpinTest();
        void LeaveOnTimeout();
        unsigned int GetRemainingTime();
        bool SpinOnce(const char *, unsigned int, const char *);

        char m_pad[0x30];
    };

    class UserContext : public RootObject {
    public:
        UserContext() { m_uiValue = 0; }
        ~UserContext() {}

        unsigned int m_uiValue;
    };

    class CallbackRoot;

    class CallContext : public RefCountedObject {
    public:
        enum _State {
            CallInit = 0,
            CallPending = 1,
            CallSuccess = 2,
            CallError = 3,
            CallCancelled = 4,
        };
        CallContext();
        virtual ~CallContext();
        virtual bool FlagsAreValid() const;
        virtual void BeginTransition(_State, qResult, bool);
        virtual void ProcessCallCompletion();

        void SetFlag(unsigned int);
        void ClearFlag(unsigned int);
        bool FlagIsSet(unsigned int) const;
        void Trace(unsigned int);
        void SetStateImpl(_State, qResult, bool);
        void SignalFailure(qResult);

        _State GetState() const { return m_eState; }
        qResult GetOutcome() const { return m_oOutcome; }
        void SetState(_State eState, qResult oResult) { SetStateImpl(eState, oResult, true); }

        unsigned int m_uiFlags; // 0x8
        _State m_eState; // 0xc
        char m_pad10[0x18]; // 0x10
        qResult m_oOutcome; // 0x28
        char m_pad34[0x1c]; // 0x34
    };

    class DOCallContext : public CallContext {
    public:
        enum _Outcome {
            Success = 0x60001,
            CallPostponed = 0x60002,
            ErrorStationNotReached = 0x80060001,
            ErrorTargetStationDisconnect = 0x80060002,
            ErrorLocalStationLeaving = 0x80060003,
            ErrorObjectNotFound = 0x80060004,
            ErrorInvalidRole = 0x80060005,
            ErrorCallTimeout = 0x80060006,
            ErrorRMCDispatchFailed = 0x80060007,
            ErrorMigrationInProgress = 0x80060008,
            ErrorNoAuthority = 0x80060009,
            UnknownOutcome = 0x80010001,
            ErrorAccessDenied = 0x80010006,
            ErrorInvalidParameters = 0x8001000A,
        };

        DOCallContext(DOHandle, bool);
        virtual ~DOCallContext();
        virtual bool FlagsAreValid() const;
        virtual void ProcessCallCompletion();
        virtual void ProcessResponse(UserContext, _State *, _Outcome *);
        virtual void ProcessOutcome(DOHandle, _Outcome);
        virtual void Trace(unsigned int);
        virtual int Slot9() const = 0; // pure in every DO call context; name not attested
        virtual void ProcessFault();

        static const char *GetOutcomeString(_Outcome);
        void SetID(unsigned short);
        void SetTargetStation(DOHandle);
        unsigned int GetTargetCount() const;
        bool Wait(unsigned int) const;
        void SignalResponse(UserContext);
        void SignalOutcome(DOHandle, _Outcome);
        bool InternalCancel(_State, _Outcome);
        bool Cancel(_State);
        void SignalFailure(_Outcome);
        void PrepareForDestruction();
        _Outcome GetOutcome();

        unsigned short GetID() const { return m_usID; }

        unsigned short m_usID; // 0x50
        DOHandle m_hLocalStation; // 0x54
        DORef m_refTargetStation; // 0x58
        DORef m_refTargetObject; // 0x64
        EventHandler m_oEventHandler; // 0x70
        Event *m_pEvent; // 0x94
    };

    class CallRegister : public RootObject {
    public:
        virtual ~CallRegister();
        virtual void Register(DOCallContext *);
        virtual void Unregister(DOCallContext *);

        static CallRegister *GetInstanceRef();
    };

    DOCallContext::DOCallContext(DOHandle hTarget, bool bFlag) : m_oEventHandler(1) {
        m_usID = 0;
        m_hLocalStation = Station::GetLocalStation();
        m_pEvent = m_oEventHandler.CreateEventObject(0, 0);
        if (bFlag)
            SetFlag(0x20);
        SetTargetStation(hTarget);
    }

    DOCallContext::~DOCallContext() {
        if (GetState() == CallPending)
            Cancel(CallCancelled);
        m_oEventHandler.DeleteEventObject(m_pEvent);
        m_pEvent = 0;
    }

    const char *DOCallContext::GetOutcomeString(_Outcome eOutcome) {
        if (eOutcome == 0x10001) // the generic success result code
            return "Success";
        switch (eOutcome) {
        case Success:
            return "Success";
            break;
        case ErrorStationNotReached:
            return "ErrorStationNotReached";
            break;
        case ErrorTargetStationDisconnect:
            return "ErrorTargetStationDisconnect";
            break;
        case ErrorLocalStationLeaving:
            return "ErrorLocalStationLeaving";
            break;
        case ErrorObjectNotFound:
            return "ErrorObjectNotFound";
            break;
        case ErrorInvalidRole:
            return "ErrorInvalidRole";
            break;
        case ErrorRMCDispatchFailed:
            return "ErrorRMCDispatchFailed";
            break;
        case ErrorCallTimeout:
            return "ErrorCallTimeout";
            break;
        case ErrorMigrationInProgress:
            return "ErrorMigrationInProgress";
            break;
        case ErrorInvalidParameters:
            return "ErrorInvalidParameters";
            break;
        case ErrorAccessDenied:
            return "ErrorAccessDenied";
            break;
        case ErrorNoAuthority:
            return "ErrorNoAuthority";
            break;
        case UnknownOutcome:
            return "UnknownOutcome";
            break;
        case CallPostponed:
            return "CallPostponed";
            break;
        default:
            return "InvalidOutcome";
        }
    }

    void DOCallContext::SetID(unsigned short usID) { m_usID = usID; }

    void DOCallContext::SetTargetStation(DOHandle hTarget) {
        bool bNoTarget = hTarget == DOHandle(0);
        if (bNoTarget)
            ClearFlag(0x800);
        else
            SetFlag(0x800);
        m_refTargetStation = hTarget;
    }

    unsigned int DOCallContext::GetTargetCount() const {
        unsigned int uiCount = 0;
        if (FlagIsSet(0x400))
            uiCount++;
        if (FlagIsSet(0x2000))
            uiCount++;
        if (FlagIsSet(0x800))
            uiCount++;
        if (FlagIsSet(0x200))
            uiCount = 2;
        if (FlagIsSet(0x1000))
            uiCount = 2;
        if (uiCount > 2)
            uiCount = 2;
        return uiCount;
    }

    bool DOCallContext::FlagsAreValid() const {
        if (!CallContext::FlagsAreValid())
            return false;
        if (FlagIsSet(0x200) && m_refTargetObject.GetDOPtr() != 0
            && !m_refTargetObject.GetDOPtr()->IsADuplicationMaster())
            return false;
        if (FlagIsSet(4) && Slot9() != 0)
            return false;
        if (FlagIsSet(0x40) && !FlagIsSet(0x400))
            return false;
        if (FlagIsSet(0x80) && !FlagIsSet(0x400))
            return false;
        return true;
    }

    // Retail reserves 8 more bytes of stack for the Scheduler lookup here than
    // this shape does (its two return temps sit at 0xac/0xb0, ours at 0xa4/0xa8);
    // the Core::GetInstance shape that gives retail's frame here gives the wrong
    // one in InternalCancel, so the shared headers' shape is kept.
    bool DOCallContext::Wait(unsigned int uiTimeout) const {
        if (uiTimeout > 0 && !Scheduler::CurrentThreadCanWaitForJob()) {
            if (Scheduler::GetInstance()->m_oCallPolicy.CurrentThreadIsDispatchingJobs()) {
                SystemError::SignalError(0, 0, 0xE000001A, 0);
                return false;
            } else {
                bool vResult = false;
                SpinTest oSpinTest(10, uiTimeout);
                oSpinTest.LeaveOnTimeout();
                while ((vResult = Wait(0)) == false) {
                    Scheduler::GlobalSingleThreadDispatch(oSpinTest.GetRemainingTime());
                    if (!oSpinTest.SpinOnce(__FILE__, 178, "(vResult=Wait(0))==false"))
                        break;
                }
                if (!vResult)
                    SystemError::SignalError(0, 0, 0xE000000C, 0);
                return vResult;
            }
        }
        Event *pEvent = 0;
        if (!m_oEventHandler.WaitForEvent(uiTimeout, &pEvent)) {
            SystemError::SignalError(0, 0, 0xE000000C, 0);
            return false;
        }
        pEvent->Reset();
        return true;
    }

    void DOCallContext::SignalResponse(UserContext oContext) {
        _Outcome eOutcome;
        _State eState;
        ProcessResponse(oContext, &eState, &eOutcome);
        if (eState != CallPending)
            SetState(eState, qResult(eOutcome));
    }

    void DOCallContext::ProcessResponse(UserContext, _State *peState, _Outcome *peOutcome) {
        *peState = CallError;
        *peOutcome = UnknownOutcome;
    }

    void DOCallContext::SignalOutcome(DOHandle hStation, _Outcome eOutcome) {
        ProcessOutcome(hStation, eOutcome);
        if (eOutcome == Success)
            SetStateImpl(CallSuccess, qResult(0x10001), true);
        else
            SignalFailure(eOutcome);
    }

    void DOCallContext::ProcessFault() { InternalCancel(CallError, ErrorTargetStationDisconnect); }

    bool DOCallContext::InternalCancel(_State eState, _Outcome eOutcome) {
        if (FlagIsSet(1) && eState == CallCancelled) {
            Wait(-1);
            SystemError::SignalError(0, 0, 0xE000000E, 0);
            return false;
        } else {
            ScopedCS lock(Scheduler::GetInstance()->m_csSystemLock);
            if (GetState() != CallPending) {
                SystemError::SignalError(0, 0, 0xE000000E, 0);
                return false;
            } else {
                SetState(eState, qResult(eOutcome));
                return true;
            }
        }
    }

    bool DOCallContext::Cancel(_State eState) {
        if (!InternalCancel(eState, UnknownOutcome)) {
            SystemError::SignalError(0, 0, 0xE000000E, 0);
            return false;
        }
        return true;
    }

    void DOCallContext::SignalFailure(_Outcome eOutcome) { CallContext::SignalFailure(qResult(eOutcome)); }

    void DOCallContext::ProcessCallCompletion() {
        if (GetID() != 0)
            CallRegister::GetInstanceRef()->Unregister(this);
        m_refTargetStation.Release();
        m_pEvent->Set();
    }

    void DOCallContext::PrepareForDestruction() {
        if (GetState() == CallPending)
            Cancel(CallCancelled);
        SpinTest oSpinTest(10, 10000);
        while (GetRefCount() > 1) {
            Scheduler::GlobalSingleThreadDispatch(-1);
            if (!oSpinTest.SpinOnce(__FILE__, 277, "GetRefCount()>1"))
                break;
        }
    }

    DOCallContext::_Outcome DOCallContext::GetOutcome() {
        _Outcome eOutcome = (_Outcome)CallContext::GetOutcome().m_iReturnCode;
        return eOutcome;
    }

    void DOCallContext::Trace(unsigned int uiFlags) { CallContext::Trace(uiFlags); }

}
