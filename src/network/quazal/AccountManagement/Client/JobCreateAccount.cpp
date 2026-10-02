// Quazal NetZ - AccountManagement/Client/JobCreateAccount.cpp
// Retail TU: .text 0x82B13598..0x82B14AA0 (13 functions), built
// /Od /Oi- /EHs-c- /Ob1 /GR- (objects.json).
//
// The job logs in as the guest account, issues one of four account-creation
// calls (chosen by FuncType), and logs the guest out again. It is a
// StepSequenceJob: each step either sets the next step or issues an asynchronous
// call and resumes on its completion. Step names are the strings retail passes
// with each step.
//
// The TU starts at the constructor, which follows JobManageAccount's last
// function, and ends after CompleteJob. The next function, 0x82B14AA0, is the
// constructor of an AccountManagementCommand subclass whose vtable follows this
// TU's strings in .rdata (its InvokeImpl calls into the client protocol).
//
// The declarations below are local to this TU; their layouts are the ones the
// retail code uses.

#include "Platform/qStd.h"
#include "Platform/Holder.h"

#define JCA_FILE ".\\AccountManagement\\Client\\JobCreateAccount.cpp"

extern "C" {
unsigned int strlen(const char *);
char *strcpy(char *, const char *);
}

namespace Quazal {

    // Array new/delete with an element-count header; defined in another TU.
    template <class T>
    T *qNewArray(unsigned int count, const char *file, int line);
    template <class T>
    void qDeleteArray(T *arr);

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
            InstanceControl *inst =
                (InstanceControl *)InstanceControl::s_oInstanceTable.GetInstanceFromVector(3, uiContext);
            Core *pCore = inst ? (Core *)inst->m_pDelegatorInstance : 0;
            return pCore;
        }
        CallContextRegister *GetCallContextRegister() { return m_pCallContextRegister; }

        void *m_pScheduler; // 0x8
        CallContextRegister *m_pCallContextRegister; // 0xc
    };

    class Credentials;

    class Data : public RootObject {
    public:
        virtual ~Data();
        virtual Data *Clone() const;
    };

    class ServiceClient : public RootObject {
    public:
        virtual ~ServiceClient();
        void SetCredentials(Credentials *);
    };

    class AccountManagementClient : public ServiceClient {
    public:
        AccountManagementClient();
        virtual ~AccountManagementClient();

        bool CreateAccount(
            ProtocolCallContext *, qResult *, const String &, const String &, unsigned int,
            const String &
        );
        bool CreateAccountWithCustomData(
            ProtocolCallContext *, const String &, const String &, unsigned int, const String &,
            const AnyObjectHolder<Data, String> &, const AnyObjectHolder<Data, String> &
        );
        bool CustomCreateAccount(
            ProtocolCallContext *, const String &, const String &, unsigned int, const String &,
            const AnyObjectHolder<Data, String> &, unsigned int *
        );
        bool ChangePasswordByGuest(
            ProtocolCallContext *, const String &, const String &, const String &
        );

        char m_pad4[0x50];
    };

    class RendezVous : public RootObject {
    public:
        RendezVous();
        virtual ~RendezVous();

        char m_pad4[0x9C];

        bool Login(
            CallContext *, const char *, const char *, const char *, unsigned short,
            Credentials **, String *
        );
        bool Logout(CallContext *, Credentials *);
    };

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

    class JobCreateAccount : public StepSequenceJob {
    public:
        enum FuncType {
            CreateAccountType = 0,
            CreateAccountWithCustomDataType = 1,
            ChangePasswordByGuestType = 2,
            CustomCreateAccountType = 3,
        };

        JobCreateAccount(
            unsigned int, const char *, const String &, unsigned short, const String &,
            const char *, unsigned int, const String &, FuncType,
            AnyObjectHolder<Data, String> *, AnyObjectHolder<Data, String> *,
            AnyObjectHolder<Data, String> *, unsigned int *
        );
        virtual ~JobCreateAccount();

        void LoginGuest();
        void ProcessLoginGuestResult();
        void CreateAccount();
        void CreateAccountWithCustomData();
        void CustomCreateAccount();
        void ChangePasswordByGuest();
        void ProcessCreateAccountResult();
        void LogoutGuest();
        void ProcessLogoutGuestResult();
        void CompleteJob(qResult);

        unsigned int m_uiCallID; // 0x60
        Credentials *m_pCredentials; // 0x64
        char *m_szGuestPassword; // 0x68
        String m_strAddress; // 0x6c
        unsigned short m_usPort; // 0x70
        String m_strUsername; // 0x74
        char *m_szPassword; // 0x78
        unsigned int m_uiGroups; // 0x7c
        String m_strEmail; // 0x80
        FuncType m_eFuncType; // 0x84
        AnyObjectHolder<Data, String> m_oPublicData; // 0x88
        AnyObjectHolder<Data, String> m_oPrivateData; // 0x90
        AnyObjectHolder<Data, String> m_oCustomData; // 0x98
        unsigned int *m_pPID; // 0xa0
        qResult m_rCallResult; // 0xa4
        qResult m_rResult; // 0xb0
        RendezVous *m_pRendezVous; // 0xbc
        AccountManagementClient *m_pClient; // 0xc0
        ProtocolCallContext m_oLoginContext; // 0xc8
        ProtocolCallContext m_oCreateContext; // 0x130
        ProtocolCallContext m_oLogoutContext; // 0x198
        ProtocolCallContext m_oUnusedContext; // 0x200
    };

    JobCreateAccount::JobCreateAccount(
        unsigned int callID, const char *guestPassword, const String &address,
        unsigned short port, const String &username, const char *password, unsigned int groups,
        const String &email, FuncType type, AnyObjectHolder<Data, String> *publicData,
        AnyObjectHolder<Data, String> *privateData, AnyObjectHolder<Data, String> *customData,
        unsigned int *pid
    )
        : StepSequenceJob(DebugString()), m_uiCallID(callID), m_pCredentials(0),
          m_strAddress(address), m_usPort(port), m_strUsername(username), m_uiGroups(groups),
          m_strEmail(email), m_eFuncType(type), m_pPID(pid),
          m_pRendezVous(new (JCA_FILE, 0x2C) RendezVous()),
          m_pClient(new (JCA_FILE, 0x2D) AccountManagementClient()) {
        m_rResult = qResult(0x00010001);
        SetStep(Step(
            (JobStateFunc)&JobCreateAccount::LoginGuest, "JobCreateAccount::LoginGuest"
        ));
        m_bUnk30 = true;
        m_szPassword = 0;
        if (password != 0) {
            m_szPassword = qNewArray<char>(strlen(password) + 1, JCA_FILE, 0x35);
            if (strlen(password) != 0) {
                strcpy(m_szPassword, password);
            } else {
                m_szPassword[0] = 0;
            }
        }
        m_szGuestPassword = 0;
        if (guestPassword != 0) {
            m_szGuestPassword = qNewArray<char>(strlen(guestPassword) + 1, JCA_FILE, 0x41);
            if (strlen(guestPassword) != 0) {
                strcpy(m_szGuestPassword, guestPassword);
            } else {
                m_szGuestPassword[0] = 0;
            }
        }
        if (publicData != 0) {
            m_oPublicData = (*publicData)->Clone();
        }
        if (privateData != 0) {
            m_oPrivateData = (*privateData)->Clone();
        }
        if (customData != 0) {
            m_oCustomData = (*customData)->Clone();
        }
    }

    JobCreateAccount::~JobCreateAccount() {
        if (m_szPassword != 0) {
            qDeleteArray(m_szPassword);
        }
        if (m_szGuestPassword != 0) {
            qDeleteArray(m_szGuestPassword);
        }
        delete m_pClient;
        delete m_pRendezVous;
    }

    void JobCreateAccount::LoginGuest() {
        if (!m_pRendezVous->Login(
                &m_oLoginContext, "guest", m_szGuestPassword, m_strAddress, m_usPort,
                &m_pCredentials, 0
            )) {
            CompleteJob(m_oLoginContext.GetOutcome());
        } else {
            SetToWaiting();
            ResumeOnCallCompletion(
                &m_oLoginContext,
                new (JCA_FILE, 0x74) Step(
                    (JobStateFunc)&JobCreateAccount::ProcessLoginGuestResult,
                    "JobCreateAccount::ProcessLoginGuestResult"
                )
            );
        }
    }

    void JobCreateAccount::ProcessLoginGuestResult() {
        if (m_oLoginContext.GetState() == CallContext::CallSuccess) {
            switch (m_eFuncType) {
            case CreateAccountType:
                SetStep(Step(
                    (JobStateFunc)&JobCreateAccount::CreateAccount,
                    "JobCreateAccount::CreateAccount"
                ));
                break;
            case CreateAccountWithCustomDataType:
                SetStep(Step(
                    (JobStateFunc)&JobCreateAccount::CreateAccountWithCustomData,
                    "JobCreateAccount::CreateAccountWithCustomData"
                ));
                break;
            case ChangePasswordByGuestType:
                SetStep(Step(
                    (JobStateFunc)&JobCreateAccount::ChangePasswordByGuest,
                    "JobCreateAccount::ChangePasswordByGuest"
                ));
                break;
            case CustomCreateAccountType:
                SetStep(Step(
                    (JobStateFunc)&JobCreateAccount::CustomCreateAccount,
                    "JobCreateAccount::CustomCreateAccount"
                ));
                break;
            }
        } else {
            CompleteJob(m_oLoginContext.GetOutcome());
        }
    }

    // The three create calls pass m_szPassword to a const String& parameter, so the
    // conversion temporary is built before the other arguments are evaluated;
    // ChangePasswordByGuest builds it explicitly as its last argument.
    void JobCreateAccount::CreateAccount() {
        m_pClient->SetCredentials(m_pCredentials);
        if (!m_pClient->CreateAccount(
                &m_oCreateContext, &m_rCallResult, m_strUsername, m_szPassword,
                m_uiGroups, m_strEmail
            )) {
            m_rResult = m_oCreateContext.GetOutcome();
            SetStep(Step(
                (JobStateFunc)&JobCreateAccount::LogoutGuest, "JobCreateAccount::LogoutGuest"
            ));
        } else {
            SetToWaiting();
            ResumeOnCallCompletion(
                &m_oCreateContext,
                new (JCA_FILE, 0xA3) Step(
                    (JobStateFunc)&JobCreateAccount::ProcessCreateAccountResult,
                    "JobCreateAccount::ProcessCreateAccountResult"
                )
            );
        }
    }

    void JobCreateAccount::CreateAccountWithCustomData() {
        m_pClient->SetCredentials(m_pCredentials);
        if (!m_pClient->CreateAccountWithCustomData(
                &m_oCreateContext, m_strUsername, m_szPassword, m_uiGroups, m_strEmail,
                m_oPublicData, m_oPrivateData
            )) {
            m_rResult = qResult(0x8001000D);
            SetStep(Step(
                (JobStateFunc)&JobCreateAccount::LogoutGuest, "JobCreateAccount::LogoutGuest"
            ));
        } else {
            SetToWaiting();
            ResumeOnCallCompletion(
                &m_oCreateContext,
                new (JCA_FILE, 0xBC) Step(
                    (JobStateFunc)&JobCreateAccount::ProcessCreateAccountResult,
                    "JobCreateAccount::ProcessCreateAccountResult"
                )
            );
        }
    }

    void JobCreateAccount::CustomCreateAccount() {
        m_pClient->SetCredentials(m_pCredentials);
        if (!m_pClient->CustomCreateAccount(
                &m_oCreateContext, m_strUsername, m_szPassword, m_uiGroups, m_strEmail,
                m_oCustomData, m_pPID
            )) {
            m_rResult = m_oCreateContext.GetOutcome();
            SetStep(Step(
                (JobStateFunc)&JobCreateAccount::LogoutGuest, "JobCreateAccount::LogoutGuest"
            ));
        } else {
            SetToWaiting();
            ResumeOnCallCompletion(
                &m_oCreateContext,
                new (JCA_FILE, 0xD0) Step(
                    (JobStateFunc)&JobCreateAccount::ProcessCreateAccountResult,
                    "JobCreateAccount::ProcessCreateAccountResult"
                )
            );
        }
    }

    void JobCreateAccount::ChangePasswordByGuest() {
        m_pClient->SetCredentials(m_pCredentials);
        if (!m_pClient->ChangePasswordByGuest(
                &m_oCreateContext, m_strUsername, m_strEmail, String(m_szPassword)
            )) {
            m_rResult = qResult(0x8001000D);
            SetStep(Step(
                (JobStateFunc)&JobCreateAccount::LogoutGuest, "JobCreateAccount::LogoutGuest"
            ));
        } else {
            SetToWaiting();
            ResumeOnCallCompletion(
                &m_oCreateContext,
                new (JCA_FILE, 0xE4) Step(
                    (JobStateFunc)&JobCreateAccount::ProcessCreateAccountResult,
                    "JobCreateAccount::ProcessCreateAccountResult"
                )
            );
        }
    }

    void JobCreateAccount::ProcessCreateAccountResult() {
        if (m_oCreateContext.GetState() == CallContext::CallSuccess) {
            if (m_eFuncType == CreateAccountType) {
                if (!m_rCallResult) {
                    m_rResult = m_rCallResult;
                } else {
                    m_rResult = qResult(0x00010001);
                }
            }
        } else {
            m_rResult = m_oCreateContext.GetOutcome();
        }
        delete m_pClient;
        m_pClient = 0;
        SetStep(Step(
            (JobStateFunc)&JobCreateAccount::LogoutGuest, "JobCreateAccount::LogoutGuest"
        ));
    }

    void JobCreateAccount::LogoutGuest() {
        if (!m_pRendezVous->Logout(&m_oLogoutContext, m_pCredentials)) {
            CompleteJob(m_oLogoutContext.GetOutcome());
        } else {
            SetToWaiting();
            ResumeOnCallCompletion(
                &m_oLogoutContext,
                new (JCA_FILE, 0x10A) Step(
                    (JobStateFunc)&JobCreateAccount::ProcessLogoutGuestResult,
                    "JobCreateAccount::ProcessLogoutGuestResult"
                )
            );
        }
    }

    void JobCreateAccount::ProcessLogoutGuestResult() {
        m_pCredentials = 0;
        CompleteJob(m_oLogoutContext.GetOutcome());
    }

    void JobCreateAccount::CompleteJob(qResult result) {
        CallContext *context =
            Core::GetInstance()->GetCallContextRegister()->GetContext(m_uiCallID);
        if (context != 0) {
            if (result && m_rResult) {
                context->SetStateToSuccess(result);
            } else {
                context->SetStateToError(result ? m_rResult : result);
            }
        }
        SetToComplete();
    }

}
