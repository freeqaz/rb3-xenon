// Quazal NetZ - SystemComponent.cpp
// Retail .text 0x82AA6B98..0x82AA78F0, written from the retail asm.
// Flags /Od /Oi- /Ob1 /EHs-c-: the TU has no EH records (Trace's ScopedIndent has
// no unwind funclet). It starts at the ctor; RefCountedObject (0x82AA6918..) and
// Buffer (0x82AA78F0..) are separate TUs.
#include "Core/SystemComponent.h"
#include "Core/Scheduler.h"
#include "Platform/SystemError.h"
#include "Platform/TraceLog.h"

namespace Quazal {

    // Polls a condition until a timeout; its .text lies outside this TU (0x82AD3388..).
    class SpinTest : public RootObject {
    public:
        SpinTest(unsigned int, unsigned int);
        ~SpinTest();
        void LeaveOnTimeout();
        bool SpinOnce(char *, unsigned int, char *);
        unsigned int GetRemainingTime();

        unsigned char m_aData[0x30];
    };

    SystemComponent::SystemComponent(const String &s)
        : mName(s), mState(Uninitialized), mRefs(0), mParent(NULL) {}

    SystemComponent::~SystemComponent() { SetParent(NULL); }

    void SystemComponent::SetName(const String &name) { mName = name; }

    bool SystemComponent::SetState(_State state, bool force) {
        if (!force && !ValidTransition(state))
            return false;
        StateTransition(state);
        mState = state;
        return true;
    }

    void SystemComponent::SetParent(SystemComponent *parent) {
        if (mParent != NULL)
            mParent = NULL;
        if (parent != NULL)
            mParent = parent;
    }

    // Both helpers test the user's name and do nothing with it: the retail
    // frames allocate a register for that test (an empty `if`), presumably a
    // trace compiled out of this build.
    bool SystemComponent::BeginUse(const char *name) {
        if (UseIsAllowed()) {
            if (name) {}
            if (mState == Ready)
                SetState(ReadyInUse, false);
            mRefs++;
            return true;
        } else
            return false;
    }

    void SystemComponent::EndUse(const char *name) {
        if (name) {}
        mRefs--;
        if (mRefs == 0) {
            if (mState == ReadyInUse)
                SetState(Ready, false);
            else
                SetState(TerminatingInUse, false);
        }
    }

    bool SystemComponent::ValidTransition(_State state) {
        if (state == Faulty)
            return true;
        if (state == Unknown)
            return true;
        switch (mState) {
        case Uninitialized:
            return state == Initializing || state == TerminatingInUse || state == Terminated;
        case Initializing:
            return state == Ready;
        case Ready:
            return state == ReadyInUse || state == TerminatingInUse;
        case ReadyInUse:
            return state == Ready || state == Terminating;
        case Terminating:
            return GetUseCount() == 0 && state == TerminatingInUse;
        case TerminatingInUse:
            return state == Terminated;
        case Terminated:
            return state == Uninitialized || state == Initializing;
        case Faulty:
            return true;
        case Unknown:
            return true;
        default:
            return false;
        }
    }

    bool SystemComponent::UseIsAllowed() { return mState == Ready || mState == ReadyInUse; }

    void SystemComponent::Trace(unsigned int flags, bool b) const {
        if (b) {
            TraceLog::ScopedIndent indent(2);
            TraceImpl(flags);
        }
    }

    SystemComponent::_State SystemComponent::Initialize() {
        OnInitialize();
        switch (GetState()) {
        case Uninitialized:
        case Terminated:
            SetState(Initializing, false);
            if (BeginInitialization())
                SetState(Ready, false);
            return GetState();
        case Initializing:
            if (EndInitialization())
                SetState(Ready, false);
            return GetState();
        case Ready:
        case ReadyInUse:
            return GetState();
        default:
            Trace(1, true);
            return GetState();
        }
    }

    bool SystemComponent::BeginInitialization() { return true; }

    SystemComponent::_State SystemComponent::Terminate() {
        OnTerminate();
        switch (GetState()) {
        case Terminated:
            return GetState();
        case ReadyInUse:
            SetState(Terminating, false);
            return GetState();
        case Uninitialized:
        case Initializing:
        case Ready:
        case Unknown:
        case Faulty:
            SetState(TerminatingInUse, true);
            if (BeginTermination())
                SetState(Terminated, false);
            return GetState();
        case Terminating:
            if (GetUseCount() == 0) {
                SetState(TerminatingInUse, false);
                if (BeginTermination())
                    SetState(Terminated, false);
            }
            return GetState();
        case TerminatingInUse:
            if (EndTermination())
                SetState(Terminated, false);
            return GetState();
        default:
            Trace(1, true);
            return GetState();
        }
    }

    bool SystemComponent::BeginTermination() { return true; }

    SystemComponent::Use::Use(SystemComponent *sc, const char *name)
        : mComponent(sc), mName(name) {
        mComponentExists = sc->BeginUse(mName);
    }

    SystemComponent::Use::~Use() {
        if (mComponentExists)
            mComponent->EndUse(mName);
    }

    qResult SystemComponent::WaitForTerminatedState(unsigned int uiTimeout) {
        Terminate();
        if (uiTimeout > 0 && !Scheduler::CurrentThreadCanWaitForJob()) {
            if (Scheduler::GetInstance()->unk16c.CurrentThreadIsDispatchingJobs()) {
                SystemError::SignalError(0, 0, 0xE000001A, 0);
                return qResult(0x8001000B);
            } else {
                qResult r(0x8001000B);
                SpinTest spin(10, uiTimeout);
                spin.LeaveOnTimeout();
                while (!r) {
                    r = WaitForTerminatedState(0);
                    if (!spin.SpinOnce(__FILE__, 246, "WaitForTerminatedState(0)"))
                        break;
                    Scheduler::GlobalSingleThreadDispatch(spin.GetRemainingTime());
                }
                if (!r) {
                    SystemError::SignalError(0, 0, 0xE000000C, 0);
                    return qResult(0x8001000B);
                }
                return r;
            }
        }
        SpinTest spin(10, uiTimeout);
        spin.LeaveOnTimeout();
        while (TestState() != Terminated
               && spin.SpinOnce(__FILE__, 250, "TestState()!=Terminated")) {}
        if (TestState() == Terminated)
            return qResult(0x10001);
        else
            return qResult(0x8001000B);
    }

}
