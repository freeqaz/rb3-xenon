// Quazal NetZ - AccountManagement/Client/JobLoginOrCreateAccount.cpp
// Retail TU: .text 0x82B0E148..0x82B0EA88 (10 functions), built
// /Od /Oi- /EHs-c- /Ob1 /GR- (objects.json).
//
// The job logs in to Rendez-Vous and, if the account does not exist yet,
// creates it through the GuestCreateAccountCommand its owner installed and then
// logs in again. It is a StepSequenceJob: each step either sets the next step
// or issues an asynchronous call and resumes on its completion. Step names are
// the strings retail passes with each step.
//
// The TU starts at the constructor (it stores this class's vtable, the first
// object in this TU's .rdata). It ends after CompleteJob: the next function,
// 0x82B0EA88, constructs GuestCustomCreateAccountCommand, whose vtable follows
// this TU's strings in .rdata, and after that come the SandboxConnectionInfo
// helpers ("production").
//
// The declarations below are local to this TU; their layouts are the ones the
// retail code uses.

#include "Platform/qStd.h"

#define JLOCA_FILE ".\\AccountManagement\\Client\\JobLoginOrCreateAccount.cpp"

namespace Quazal {

    class DebugString {
    public:
        DebugString() {}
    };

    class qResult {
    public:
        qResult();
        qResult(const int &);
        bool Equals(const bool &) const;
        operator bool() const;
        qResult &operator=(const qResult &);
        bool operator==(const qResult &r) const { return m_iReturnCode == r.m_iReturnCode; }

        unsigned int m_iReturnCode;
        const char *m_cszFilename;
        int m_iLineNumber;
    };

    class Time : public RootObject {
    public:
        Time() { m_ui64Value = 0; }
        Time(unsigned int);
        Time &operator=(const Time &);
        long long operator-(const Time &) const;
        static Time GetTime();

        unsigned long long m_ui64Value;
    };

    class String : public RootObject {
    public:
        String();
        String(const char *);
        String(const String &);
        ~String();
        void CreateCopy(char **) const;
        static void ReleaseCopy(char *);
        operator const char *() const { return m_szContent; }

        char *m_szContent;
    };

    class Data;
    template <class T1, class T2>
    class AnyObjectHolder;

    class RefCountedObject : public RootObject {
    public:
        virtual ~RefCountedObject();
        virtual void AcquireRef();
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
        CallContext();
        virtual ~CallContext();
        void Reset();
        void SetStateToSuccess(qResult);
        void SetStateToError(qResult);

        _State GetState() const { return m_eState; }
        qResult GetOutcome() const { return m_oOutcome; }

        unsigned int m_unk8; // 0x8
        _State m_eState; // 0xc
        unsigned int m_unk10[6];
        qResult m_oOutcome; // 0x28
        unsigned int m_unk34[5];
        Time m_tTimeout; // 0x48
    };

    class ProtocolCallContext : public CallContext {
    public:
        ProtocolCallContext();
        virtual ~ProtocolCallContext();

        unsigned int m_unk50[6];
    };

    class CallContextRegister {
    public:
        CallContext *GetContext(unsigned int);
    };

    class InstantiationContext : public RootObject {
    public:
        unsigned int GetInstance(unsigned int);
        char m_pad[0x30];
    };

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

    // Core::GetInstance is inline, but /Ob1 does not expand it (it calls another
    // inline), so it is called out of line and CompleteJob's frame still reserves
    // its locals and those of GetInstanceFromVector: the 0x18 bytes retail leaves
    // between the context temporaries and GetInstance's return temporary.
    class Core : public RefCountedObject {
    public:
        static Core *GetInstance() {
            unsigned int uiContext = PseudoSingleton::GetCurrentContext();
            InstanceControl *pInstance =
                (InstanceControl *)InstanceControl::s_oInstanceTable.GetInstanceFromVector(3, uiContext);
            Core *pCore = 0;
            if (pInstance != 0) {
                pCore = (Core *)pInstance->m_pDelegatorInstance;
            }
            return pCore;
        }
        CallContextRegister *GetCallContextRegister() { return m_pCallContextRegister; }

        void *m_pScheduler; // 0x8
        CallContextRegister *m_pCallContextRegister; // 0xc
    };

    class Credentials;

    // One server of a sandbox: address, port and two further strings.
    class ServerInfo {
    public:
        String m_strAddress; // 0x0
        unsigned short m_usPort; // 0x4
        String m_strUnk8; // 0x8
        String m_strUnkC; // 0xc
    };

    class SandboxConnectionInfo {
    public:
        ~SandboxConnectionInfo();
        const ServerInfo &GetCredentialsProd() const;

        ServerInfo m_oInfo0; // 0x0
        ServerInfo m_oInfo10; // 0x10
        ServerInfo m_oProd; // 0x20
    };

    class BackEndServices : public RootObject {
    public:
        bool Login(
            CallContext *, int, const char *, const char *, const char *, unsigned short,
            Credentials **, String *, AnyObjectHolder<Data, String> *
        );
        // RVLogin calls this inline overload. Retail stages every argument the
        // caller loads into a temporary home (the inline's parameters) and
        // evaluates them right to left, which puts pData before ppCredentials
        // and pstr; the out-of-line overload takes them in the other order.
        bool Login(
            CallContext *pContext, const char *szUsername, const char *szPassword,
            const char *szAddress, unsigned short usPort, AnyObjectHolder<Data, String> *pData,
            Credentials **ppCredentials, String *pstr
        ) {
            return Login(
                pContext, 0, szUsername, szPassword, szAddress, usPort, ppCredentials, pstr, pData
            );
        }
        bool HasLoginTimeout();
        void SetLoginTimeout(int);
        unsigned int GetTimeout() const { return m_uiTimeout; }

        unsigned int m_unk0[0x1B];
        unsigned int m_uiTimeout; // 0x6c
    };

    class AccountManagementCommand : public RootObject {
    public:
        virtual ~AccountManagementCommand();
        bool Invoke(ProtocolCallContext *);
    };

    class GuestCreateAccountCommand : public AccountManagementCommand {};

    class Job : public RefCountedObject {
    public:
        virtual ~Job();
        virtual void DecoratedExecute();
        virtual void Execute();
        virtual void TestSuspendedJobState();
        virtual void AddActivity(const char *);
        virtual void GetTraceInfo();
        virtual void SetDefaultPostExecutionState();
        virtual bool SkipWaitDelayAtTermination();

        void SetToWaiting();
        void SetToComplete();

        unsigned int m_unk8[10];
        bool m_bUnk30; // 0x30
        unsigned int m_unk34[9];
        unsigned int m_unk58; // 0x58
    };

    class __multiple_inheritance StepSequenceJob;

    class StepSequenceJob : public Job {
    public:
        typedef void (StepSequenceJob::*JobStateFunc)(void);

        class Step : public RootObject {
        public:
            Step(JobStateFunc func, const char *name) : m_pfState(func), m_szName(name) {}
            ~Step() {}

            JobStateFunc m_pfState; // 0x0
            const char *m_szName; // 0x8
            unsigned int m_unkc;
        };

        StepSequenceJob(const DebugString &);
        virtual ~StepSequenceJob();
        virtual void CheckExceptions();

        void SetStep(const Step &);
        void ResumeOnCallCompletion(CallContext *, Step *);

        unsigned int m_unk5c;
    };

    class JobLoginOrCreateAccount : public StepSequenceJob {
    public:
        JobLoginOrCreateAccount(
            unsigned int, BackEndServices *, const SandboxConnectionInfo &, const String &,
            const String &, const String &, unsigned int, Credentials **, String *,
            AnyObjectHolder<Data, String> *
        );
        virtual ~JobLoginOrCreateAccount();

        void SetCreateAccountCommand(GuestCreateAccountCommand *);
        void RVLogin();
        void ProcessRVLoginResult();
        void CreateAccount();
        void ProcessCreateAccountResult();
        void CompleteJob(const qResult &);

        unsigned int m_uiCallID; // 0x60
        BackEndServices *m_pServices; // 0x64
        String m_strUsername; // 0x68
        String m_strPassword; // 0x6c
        String m_strUnk70; // 0x70
        unsigned int m_uiUnk74; // 0x74
        Credentials **m_ppCredentials; // 0x78
        String *m_pstrUnk7c; // 0x7c
        AnyObjectHolder<Data, String> *m_pData; // 0x80
        CallContext m_oCallContext; // 0x88
        ProtocolCallContext m_oProtocolCallContext; // 0xd8
        GuestCreateAccountCommand *m_pCreateAccountCommand; // 0x140
        bool m_bAccountCreated; // 0x144
        SandboxConnectionInfo m_oSandboxInfo; // 0x148
        Time m_tDeadline; // 0x178
    };

    JobLoginOrCreateAccount::JobLoginOrCreateAccount(
        unsigned int callID, BackEndServices *services, const SandboxConnectionInfo &info,
        const String &username, const String &password, const String &str3, unsigned int ui,
        Credentials **credentials, String *pstr, AnyObjectHolder<Data, String> *data
    )
        : StepSequenceJob(DebugString()), m_uiCallID(callID), m_pServices(services),
          m_strUsername(username), m_strPassword(password), m_strUnk70(str3), m_uiUnk74(ui),
          m_ppCredentials(credentials), m_pstrUnk7c(pstr), m_pData(data),
          m_pCreateAccountCommand(0), m_bAccountCreated(false), m_oSandboxInfo(info) {
        *m_ppCredentials = 0;
        m_bUnk30 = true;
        m_unk58 = 4;
        m_tDeadline = Time(m_pServices->GetTimeout());
    }

    JobLoginOrCreateAccount::~JobLoginOrCreateAccount() {
        if (m_pCreateAccountCommand != 0) {
            delete m_pCreateAccountCommand;
        }
    }

    void JobLoginOrCreateAccount::SetCreateAccountCommand(GuestCreateAccountCommand *command) {
        m_pCreateAccountCommand = command;
    }

    void JobLoginOrCreateAccount::RVLogin() {
        if (!m_pServices->HasLoginTimeout()) {
            int iTimeout = m_tDeadline - Time::GetTime();
            if (iTimeout < 0) {
                CompleteJob(qResult(0x8001000B));
                return;
            }
            m_pServices->SetLoginTimeout(iTimeout);
        }
        char *szPassword = 0;
        m_strPassword.CreateCopy(&szPassword);
        if (!m_pServices->Login(
                &m_oCallContext, m_strUsername, szPassword,
                m_oSandboxInfo.GetCredentialsProd().m_strAddress,
                m_oSandboxInfo.GetCredentialsProd().m_usPort, m_pData, m_ppCredentials,
                m_pstrUnk7c
            )) {
            CompleteJob(qResult(0x8001000D));
        } else {
            SetToWaiting();
            ResumeOnCallCompletion(
                &m_oCallContext,
                new (JLOCA_FILE, 0x83) Step(
                    (JobStateFunc)&JobLoginOrCreateAccount::ProcessRVLoginResult,
                    "JobLoginOrCreateAccount::ProcessRVLoginResult"
                )
            );
        }
        String::ReleaseCopy(szPassword);
    }

    void JobLoginOrCreateAccount::ProcessRVLoginResult() {
        if (m_oCallContext.GetState() == CallContext::CallSuccess) {
            CompleteJob(m_oCallContext.GetOutcome());
        } else if (m_oCallContext.GetOutcome() == qResult(0x80030064)) {
            if (!m_bAccountCreated) {
                m_bAccountCreated = true;
                SetStep(Step(
                    (JobStateFunc)&JobLoginOrCreateAccount::CreateAccount,
                    "JobLoginOrCreateAccount::CreateAccount"
                ));
            } else {
                CompleteJob(qResult(0x80010004));
            }
        } else {
            CompleteJob(m_oCallContext.GetOutcome());
        }
    }

    void JobLoginOrCreateAccount::CreateAccount() {
        if (!m_pCreateAccountCommand->Invoke(&m_oProtocolCallContext)) {
            CompleteJob(qResult(0x8001000D));
        } else {
            SetToWaiting();
            ResumeOnCallCompletion(
                &m_oProtocolCallContext,
                new (JLOCA_FILE, 0xBA) Step(
                    (JobStateFunc)&JobLoginOrCreateAccount::ProcessCreateAccountResult,
                    "JobLoginOrCreateAccount::ProcessCreateAccountResult"
                )
            );
        }
    }

    void JobLoginOrCreateAccount::ProcessCreateAccountResult() {
        if (m_oProtocolCallContext.GetState() == CallContext::CallSuccess) {
            m_oCallContext.Reset();
            SetStep(Step(
                (JobStateFunc)&JobLoginOrCreateAccount::RVLogin,
                "JobLoginOrCreateAccount::RVLogin"
            ));
        } else {
            CompleteJob(m_oProtocolCallContext.GetOutcome());
        }
    }

    void JobLoginOrCreateAccount::CompleteJob(const qResult &result) {
        CallContext *context =
            Core::GetInstance()->GetCallContextRegister()->GetContext(m_uiCallID);
        if (context != 0) {
            if (result.Equals(true)) {
                context->SetStateToSuccess(result);
            } else {
                context->SetStateToError(result);
            }
        }
        SetToComplete();
    }

}
