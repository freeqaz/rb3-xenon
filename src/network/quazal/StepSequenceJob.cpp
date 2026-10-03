// Quazal NetZ - .\StepSequenceJob.cpp
//
// Retail TU: .text 0x82AF8F50..0x82AF9538, compiled /Od /Oi- /Ob1 /GR- /EHs-c-
// (see objects.json). It starts at the StepSequenceJob constructor and ends
// with the Callback<StepSequenceJob, Step *> code it instantiates; the next
// object's constructor follows at 0x82AF9538. Its .rdata is the StepSequenceJob
// vtable (0x821861A4), the Callback vtable (0x821861D0) and the __FILE__
// string: no complete-object locator in front of either vtable and no EH tables.
//
// The job runs the member function held by its current Step for as long as it
// stays Running; ResumeOnCallCompletion parks it until a CallContext completes
// and then continues with the given Step.
//
// The classes are declared here with the layouts retail uses rather than taken
// from the shared Quazal headers: a step's state function is an 8-byte pointer
// to member (StepSequenceJob is declared __multiple_inheritance, as the other
// TUs that derive from it do), so the current Step sits at 0x48.
//
// This TU is built /Od: its locals are laid out by a walk over the scope's
// symbol hash table, so the local NAMES below determine the stack offsets, and
// the allocation passes __FILE__/__LINE__, so #line reproduces retail's line.

#include "Platform/Callback.h"
#include "Platform/RefCountedObject.h"
#include "Platform/RootObject.h"
#include "Platform/String.h"
#include "Platform/Time.h"

namespace Quazal {
    class PseudoSingleton : public RootObject {
    public:
        static unsigned int GetCurrentContext();
    };

    class InstanceTable : public RootObject {
    public:
        unsigned int GetInstanceFromVector(unsigned int, unsigned int);
    };

    class InstanceControl : public RootObject {
    public:
        static InstanceTable s_oInstanceTable;

        char m_data[0xc];
        void *m_pDelegatorInstance; // 0xc
    };

    class Scheduler;

    // Retail calls GetInstance out of line (0x823EA910): the /Ob1 inliner
    // declines it after reserving its locals.
    class Core : public RootObject {
    public:
        static Core *GetInstance() {
            unsigned int uiContext = PseudoSingleton::GetCurrentContext();
            InstanceControl *pInstance = (InstanceControl *)
                InstanceControl::s_oInstanceTable.GetInstanceFromVector(3, uiContext);
            Core *pCore = 0;
            if (pInstance != 0) {
                pCore = (Core *)pInstance->m_pDelegatorInstance;
            }
            return pCore;
        }
        Scheduler *GetScheduler() const { return m_pScheduler; }

        char m_data[0x8];
        Scheduler *m_pScheduler; // 0x8
    };

    class Scheduler : public RootObject {
    public:
        static Scheduler *GetInstance() {
            Core *pCore = Core::GetInstance();
            if (pCore == 0) {
                return 0;
            } else {
                return pCore->GetScheduler();
            }
        }
        bool StepTracingIsOn() const { return m_bTraceSteps; }

        char m_data[0x1d9];
        bool m_bTraceSteps; // 0x1d9
    };

    // The trace output: a global pointer (0x82CA431C) with a String writer.
    class TraceOutput : public RootObject {
    public:
        static TraceOutput *GetInstance();
        void Write(const String &);
    };

    class CallContext : public RefCountedObject {
    public:
        void RegisterCompletionCallback(CallbackRoot *, bool, bool);
    };

    class SystemComponent : public RootObject {
    public:
        enum _State {
            Uninitialized = 0x1,
            Initializing = 0x2,
            Ready = 0x4,
            ReadyInUse = 0x8,
            TerminatingInUse = 0x10,
            Terminating = 0x20,
            Terminated = 0x40,
            Faulty = 0x80,
            Unknown = 0x100,
            Invalid = -1
        };

        _State Terminate();
        _State GetState() const { return m_eState; }

        char m_data[0xc];
        _State m_eState; // 0xc
    };

    class Job : public RefCountedObject {
    public:
        enum State {
            Initial = 0,
            Waiting = 1,
            Suspended = 2,
            Ready = 3,
            Running = 4,
            Complete = 5
        };

        Job(const DebugString &);
        virtual ~Job();
        virtual void DecoratedExecute();
        virtual void Execute();
        virtual void TestSuspendedJobState();
        virtual void AddActivity(const char *);
        virtual String GetTraceInfo() const;
        virtual void SetDefaultPostExecutionState();
        virtual bool SkipWaitDelayAtTermination();

        void SetToWaiting(int);
        void SetToReady();
        State GetState() const { return m_eState; }

        unsigned int m_unk8[5];
        State m_eState; // 0x1c
        unsigned int m_unk20; // 0x20
        unsigned int m_uiExecutionCount; // 0x24
        char m_unk28[0x10];
    };

    class __multiple_inheritance StepSequenceJob;

    class StepSequenceJob : public Job {
    public:
        typedef void (StepSequenceJob::*JobStateFunc)(void);

        class Step : public RootObject {
        public:
            Step() : m_pfState(0), m_szName(0) {}
            Step(JobStateFunc func, const char *name) : m_pfState(func), m_szName(name) {}
            ~Step() {}
            void Run(StepSequenceJob *pJob) { (pJob->*m_pfState)(); }

            JobStateFunc m_pfState; // 0x0
            const char *m_szName; // 0x8
            unsigned int m_unkc;
        };

        StepSequenceJob(const DebugString &);
        virtual ~StepSequenceJob();
        virtual void Execute();
        virtual void CheckExceptions() {}

        CallbackRoot *CreateCallResultCallback(Step *);
        void ResumeOnCallCompletion(CallContext *, Step *);
        void SetStep(const Step &);
        void ProcessCallResult(Step *);
        void TraceStep();
        void TerminateComponent(SystemComponent *, const Step &);

        Time m_tStepStart; // 0x38
        unsigned int m_uiNbStepsExecuted; // 0x40
        Step m_oCurrentStep; // 0x48
        unsigned int m_unk58; // 0x58
    };

    StepSequenceJob::StepSequenceJob(const DebugString &strName) : Job(strName) {
        m_uiNbStepsExecuted = 0;
        m_unk58 = 0;
    }

    StepSequenceJob::~StepSequenceJob() {}

    void StepSequenceJob::Execute() {
        unsigned int uiExecutionCount = m_uiExecutionCount;
        do {
            CheckExceptions();
            if (GetState() == Running) {
                if (Scheduler::GetInstance()->StepTracingIsOn()) {
                    TraceStep();
                }
                m_oCurrentStep.Run(this);
                m_uiNbStepsExecuted++;
            }
        } while (GetState() == Running);
    }

    CallbackRoot *StepSequenceJob::CreateCallResultCallback(Step *pStep) {
        AcquireRef();
#line 56
        return new (__FILE__, __LINE__) Callback<StepSequenceJob, Step *>(
            this, &StepSequenceJob::ProcessCallResult, pStep
        );
    }

    void StepSequenceJob::ResumeOnCallCompletion(CallContext *pContext, Step *pStep) {
        pContext->RegisterCompletionCallback(CreateCallResultCallback(pStep), false, true);
    }

    void StepSequenceJob::SetStep(const Step &oStep) {
        m_tStepStart = Time::GetTime();
        m_oCurrentStep = oStep;
    }

    void StepSequenceJob::ProcessCallResult(Step *pStep) {
        SetToReady();
        SetStep(*pStep);
        delete pStep;
        ReleaseRef();
    }

    void StepSequenceJob::TraceStep() { TraceOutput::GetInstance()->Write(GetTraceInfo()); }

    void StepSequenceJob::TerminateComponent(SystemComponent *pComponent, const Step &oStep) {
        pComponent->Terminate();
        if (pComponent->GetState() != SystemComponent::Terminated) {
            SetToWaiting(50);
        } else {
            SetStep(oStep);
        }
    }
}
