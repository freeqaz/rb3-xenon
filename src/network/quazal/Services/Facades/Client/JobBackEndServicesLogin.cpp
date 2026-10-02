// Quazal NetZ - Services/Facades/Client/JobBackEndServicesLogin.cpp
// Retail TU: .text 0x82ADB5F8..0x82ADD088 (22 functions), built
// /Od /Oi- /EHs-c- /Ob1 /GR- (objects.json).
//
// The login job is a StepSequenceJob: every step either sets the next step
// directly or issues an asynchronous call and resumes on its completion.
// Step and member-function names are the ones retail passes with each step
// ("JobBackEndServicesLogin::ValidateArguments", ...); the names of the classes
// it calls into are descriptive.
//
// Two /Od /Ob1 rules this file depends on:
// - Small inline helpers (getters, StringStream::operator<<(int), Step's ctor)
//   are expanded in place, each with its this/return temporaries.
// - An inline function too large to expand (ConnectStream, LoginURLs' ctor and
//   dtor) is called out of line, emitted after its first caller, and still
//   reserves stack in every caller for its temporaries. Retail's frames carry
//   that reservation, so those functions must stay inline.
//
// The declarations below are local to this TU; their layouts are the ones the
// retail code uses.

#include "Platform/qStd.h"

#define JBESL_FILE "..\\Services\\Facades\\Client\\JobBackEndServicesLogin.cpp"

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

        unsigned int m_iReturnCode;
        const char *m_cszFilename;
        int m_iLineNumber;
    };

    class Time : public RootObject {
    public:
        Time(unsigned int);
        Time &operator=(const Time &);
        static unsigned int ToMilliseconds(Time);

        unsigned long long m_ui64Value;
    };

    class String : public RootObject {
    public:
        String();
        String(const char *);
        String(const String &);
        ~String();
        unsigned int GetLength() const;
        void Format(const char *, ...);
        operator const char *() const { return m_szContent; }

        char *m_szContent;
    };

    class StringStream : public RootObject {
    public:
        StringStream();
        ~StringStream();
        StringStream &operator<<(const char *);
        StringStream &operator<<(long);
        StringStream &operator<<(int i) { return *this << (long)i; }

        const char *m_szBuffer;
        char m_data[0x10C];
    };
    StringStream &operator<<(StringStream &, const String &);

    class StationURL {
    public:
        StationURL();
        StationURL(const char *);
        ~StationURL();
        StationURL &operator=(const StationURL &);
        StationURL &operator=(const String &);
        bool operator==(const StationURL &) const;
        bool operator!=(const StationURL &) const;
        void SetPID(unsigned int);

        char m_data[0x64];
    };

    class RefCountedObject : public RootObject {
    public:
        virtual ~RefCountedObject();
        virtual void AcquireRef();
        virtual void ReleaseRef();

        unsigned short m_ui16RefCount; // 0x4
    };

    class Buffer : public RefCountedObject {
    public:
        Buffer(unsigned int);
        virtual ~Buffer();

        unsigned char *m_pData; // 0x8
        unsigned int m_uiContentSize; // 0xc
        unsigned int m_uiBufferSize; // 0x10
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
        void SetTimeout(Time t) { m_tTimeout = t; }

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

    class InstanceControl {
    public:
        static InstanceControl *GetInstance();
        CallContextRegister *GetCallContextRegister() const { return m_pRegister; }

        unsigned int m_unk0[3];
        CallContextRegister *m_pRegister; // 0xc
    };

    class StreamCredentials;
    class LoginURLs;

    class Credentials : public RootObject {
    public:
        void SetAuthenticationConnection(StreamCredentials *);
        void SetSecureConnection(StreamCredentials *);
        void SetSpecialConnection(StreamCredentials *);
        void SetSpecialURLs(qList<unsigned int> *);
        unsigned int GetGuest() const { return m_uiGuest; }
        unsigned int GetConnectionID() const { return m_uiConnectionID; }

        unsigned int m_unk0[2];
        unsigned int m_uiPID; // 0x8
        unsigned int m_uiGuest; // 0xc
        unsigned int m_unk10[3];
        unsigned int m_uiConnectionID; // 0x1c
    };

    class ServiceClient : public RootObject {
    public:
        virtual ~ServiceClient();
        virtual bool IsAvailable();
        void SetCredentials(Credentials *);
    };

    class AuthenticationClient : public ServiceClient {
    public:
        bool Login(
            ProtocolCallContext *, qResult *, String *, const char *, LoginURLs *, int, int
        );
        void ClearCredentials(Credentials *);
        unsigned int GetPID() const { return m_uiPID; }

        unsigned int m_unk4[10];
        unsigned int m_uiPID; // 0x2c
    };

    class SecureConnectionClient : public ServiceClient {
    public:
        bool RegisterURLs(ProtocolCallContext *, qResult *, StationURL *, void *);
    };

    class StreamClient : public RootObject {
    public:
        bool Connect(
            CallContext *, int, int, qList<StationURL> &, StreamCredentials **,
            unsigned int
        );
        bool Disconnect(CallContext *, unsigned int);
    };

    class StreamManager : public RootObject {
    public:
        StreamClient *GetClient() const { return m_pClient; }

        unsigned int m_unk0[4];
        StreamClient *m_pClient; // 0x10
    };

    class BackEndServicesState : public RootObject {
    public:
        Credentials *GetCredentials() const { return m_pCredentials; }
        void SetCredentials(Credentials *credentials) { m_pCredentials = credentials; }

        unsigned int m_unk0[8];
        Credentials *m_pCredentials; // 0x20
    };

    class BackEndServices : public RootObject {
    public:
        StreamManager *GetStreamManager();
        BackEndServicesState *GetState();
        AuthenticationClient *GetAuthenticationClient();
        SecureConnectionClient *GetSecureConnectionClient();
        void RegisterLogin();
        bool Logout(CallContext *, Credentials *);
        Credentials *GetCredentials() const { return m_pCredentials; }

        unsigned int m_unk0[0x1C];
        Credentials *m_pCredentials; // 0x70
    };

    void ReleaseStreamCredentials(StreamCredentials *);

    extern unsigned int g_uiGuestPID;
    inline unsigned int GetGuestPID() { return g_uiGuestPID; }
    extern StationURL g_urlRegistration;

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

        unsigned int m_unk8[20];
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

    class LoginURLs : public RootObject {
    public:
        LoginURLs();
        virtual ~LoginURLs();

        StationURL m_urlSecure; // 0x4
        qList<unsigned int> m_lstSpecial; // 0x68
        StationURL m_urlSpecial; // 0x70
    };

    // Retail constructs the member below by calling LoginURLs' ctor and then
    // storing the vtable again: a derived class with an inline ctor, whose vtable
    // and destructor fold into LoginURLs'.
    class ServiceURLs : public LoginURLs {
    public:
        ServiceURLs() {}
    };

    class JobBackEndServicesLogin : public StepSequenceJob {
    public:
        JobBackEndServicesLogin(
            unsigned int, BackEndServices *, qResult *, const String &, const char *,
            const char *, unsigned short, Credentials **, int, int, unsigned int
        );
        virtual ~JobBackEndServicesLogin();

        void ValidateArguments();
        void ConnectToAuthenticationService();
        void ProcessAuthConnectionResult();
        void Authenticate();
        void ProcessAuthenticationResult();
        void ConnectToSecureConnectionService();
        void ProcessSecConnConnectionResult();
        void ConnectToSpecialConnection();
        void ProcessSpecialConnResult();
        void RegisterURLs();
        void CompleteLogin();
        void DisconnectFromAuthenticationService();
        void ProcessAuthDisconnectionResult();
        void CompleteLogout();
        void Complete(qResult);

        static bool ConnectStream(
            StreamClient *, CallContext *, const StationURL &, StreamCredentials **,
            unsigned int
        );

        unsigned int m_uiCallID; // 0x60
        String m_strUsername; // 0x64
        char *m_szPassword; // 0x68
        String m_strAddress; // 0x6c
        unsigned short m_usPort; // 0x70
        Credentials **m_ppCredentials; // 0x74
        CallContext m_oCallContext; // 0x78
        ProtocolCallContext m_oProtocolCallContext; // 0xc8
        qResult m_rAuthResult; // 0x130
        qResult m_rResult; // 0x13c
        Buffer m_oBuffer; // 0x148
        BackEndServices *m_pServices; // 0x15c
        StationURL m_urlSecure; // 0x160
        ServiceURLs m_oURLs; // 0x1c4
        qResult *m_pResult; // 0x298
        int m_iLoginArg1; // 0x29c
        int m_iLoginArg2; // 0x2a0
        StreamCredentials *m_pAuthConnection; // 0x2a4
        StreamCredentials *m_pSecureConnection; // 0x2a8
        StreamCredentials *m_pSpecialConnection; // 0x2ac
        Time m_tTimeout; // 0x2b0
    };

    inline LoginURLs::LoginURLs() {}

    inline LoginURLs::~LoginURLs() {}

    JobBackEndServicesLogin::JobBackEndServicesLogin(
        unsigned int callID, BackEndServices *services, qResult *result,
        const String &username, const char *password, const char *address,
        unsigned short port, Credentials **credentials, int loginArg1, int loginArg2, unsigned int timeout
    )
        : StepSequenceJob(DebugString()), m_uiCallID(callID), m_strUsername(username),
          m_strAddress(address), m_usPort(port), m_ppCredentials(credentials), m_oBuffer(0x400),
          m_pServices(services), m_pResult(result), m_iLoginArg1(loginArg1), m_iLoginArg2(loginArg2),
          m_pAuthConnection(0), m_pSecureConnection(0), m_pSpecialConnection(0),
          m_tTimeout(timeout) {
        m_unk58 = 4;
        m_szPassword = 0;
        if (password != 0) {
            m_szPassword = qNewArray<char>(strlen(password) + 1, JBESL_FILE, 0x42);
            if (strlen(password) != 0) {
                strcpy(m_szPassword, password);
            } else {
                m_szPassword[0] = 0;
            }
        }
        *m_ppCredentials = 0;
        SetStep(Step(
            (JobStateFunc)&JobBackEndServicesLogin::ValidateArguments,
            "JobBackEndServicesLogin::ValidateArguments"
        ));
    }

    JobBackEndServicesLogin::~JobBackEndServicesLogin() {
        if (m_szPassword != 0) {
            qDeleteArray(m_szPassword);
        }
    }

    void JobBackEndServicesLogin::ValidateArguments() {
        if (m_strUsername.GetLength() == 0) {
            Complete(qResult(0x8001000A));
        } else if (m_szPassword == 0) {
            Complete(qResult(0x8001000A));
        } else {
            SetStep(Step(
                (JobStateFunc)&JobBackEndServicesLogin::ConnectToAuthenticationService,
                "JobBackEndServicesLogin::ConnectToAuthenticationService"
            ));
        }
    }

    void JobBackEndServicesLogin::ConnectToAuthenticationService() {
        StringStream oss;
        oss << "prudp:/address=" << m_strAddress << ";port=" << m_usPort << ";stream=" << 3
           << ";sid=" << 1 << ";type=" << 2;
        StationURL url(oss.m_szBuffer);
        if (!ConnectStream(
                m_pServices->GetStreamManager()->GetClient(), &m_oCallContext, url,
                &m_pAuthConnection, Time::ToMilliseconds(m_tTimeout)
            )) {
            Complete(qResult(0x8001000D));
        } else {
            SetToWaiting();
            ResumeOnCallCompletion(
                &m_oCallContext,
                new (JBESL_FILE, 0x7A) Step(
                    (JobStateFunc)&JobBackEndServicesLogin::ProcessAuthConnectionResult,
                    "JobBackEndServicesLogin::ProcessAuthConnectionResult"
                )
            );
        }
    }

    inline bool JobBackEndServicesLogin::ConnectStream(
        StreamClient *client, CallContext *context, const StationURL &url,
        StreamCredentials **connection, unsigned int timeout
    ) {
        qList<StationURL> urls;
        urls.push_back(url);
        return client->Connect(context, 0, 0, urls, connection, timeout);
    }

    void JobBackEndServicesLogin::ProcessAuthConnectionResult() {
        if (m_oCallContext.GetState() == CallContext::CallSuccess) {
            m_pServices->GetCredentials()->SetAuthenticationConnection(m_pAuthConnection);
            m_pServices->GetAuthenticationClient()->SetCredentials(
                m_pServices->GetCredentials()
            );
            SetStep(Step(
                (JobStateFunc)&JobBackEndServicesLogin::Authenticate,
                "JobBackEndServicesLogin::Authenticate"
            ));
        } else {
            if (m_pAuthConnection != 0) {
                ReleaseStreamCredentials(m_pAuthConnection);
                m_pAuthConnection = 0;
            }
            Complete(m_oCallContext.GetOutcome());
        }
    }

    void JobBackEndServicesLogin::Authenticate() {
        if (m_pServices->GetState()->GetCredentials() != 0) {
            Complete(qResult(0x80030001));
        } else {
            AuthenticationClient *client = m_pServices->GetAuthenticationClient();
            if (!client->IsAvailable()) {
                Complete(qResult(0x80030002));
            } else {
                m_oProtocolCallContext.SetTimeout(m_tTimeout);
                if (!client->Login(
                        &m_oProtocolCallContext, &m_rAuthResult, &m_strUsername,
                        m_szPassword, &m_oURLs, m_iLoginArg1, m_iLoginArg2
                    )) {
                    Complete(qResult(0x8001000D));
                    return;
                } else {
                    SetToWaiting();
                    ResumeOnCallCompletion(
                        &m_oProtocolCallContext,
                        new (JBESL_FILE, 0xA9) Step(
                            (JobStateFunc)&JobBackEndServicesLogin::ProcessAuthenticationResult,
                            "JobBackEndServicesLogin::ProcessAuthenticationResult"
                        )
                    );
                }
            }
        }
    }

    void JobBackEndServicesLogin::ProcessAuthenticationResult() {
        if (m_oProtocolCallContext.GetState() == CallContext::CallSuccess
            && m_rAuthResult.Equals(true)) {
            AuthenticationClient *client = m_pServices->GetAuthenticationClient();
            m_pServices->GetCredentials()->m_uiPID = client->GetPID();
            m_pServices->GetState()->SetCredentials(m_pServices->GetCredentials());
            SetStep(Step(
                (JobStateFunc)&JobBackEndServicesLogin::ConnectToSecureConnectionService,
                "JobBackEndServicesLogin::ConnectToSecureConnectionService"
            ));
        } else {
            Complete(m_rAuthResult);
        }
    }

    void JobBackEndServicesLogin::ConnectToSecureConnectionService() {
        if (m_pServices->GetAuthenticationClient()->GetPID() == g_uiGuestPID) {
            m_pServices->GetCredentials()->m_uiGuest = 1;
            SetStep(Step(
                (JobStateFunc)&JobBackEndServicesLogin::CompleteLogin,
                "JobBackEndServicesLogin::CompleteLogin"
            ));
            return;
        }
        String strAddress;
        int iOffset = 0;
        switch (g_uiGuestPID) {
        case 2:
            iOffset = 1;
            break;
        case 3:
            iOffset = 2;
            break;
        }
        StationURL url;
        if (m_oURLs.m_urlSecure != "") {
            url = m_oURLs.m_urlSecure;
        } else {
            strAddress.Format(
                "prudps:/address=%s;port=%d;stream=%d;sid=%d;PID=%d;CID=1;type=%d",
                (const char *)m_strAddress, m_usPort + iOffset, 3, 1, GetGuestPID(), 2
            );
            url = strAddress;
        }
        m_oCallContext.Reset();
        if (!ConnectStream(
                m_pServices->GetStreamManager()->GetClient(), &m_oCallContext, url,
                &m_pSecureConnection, Time::ToMilliseconds(m_tTimeout)
            )) {
            Complete(qResult(0x8001000D));
            return;
        } else {
            SetToWaiting();
            ResumeOnCallCompletion(
                &m_oCallContext,
                new (JBESL_FILE, 0xF0) Step(
                    (JobStateFunc)&JobBackEndServicesLogin::ProcessSecConnConnectionResult,
                    "JobBackEndServicesLogin::ProcessSecConnConnectionResult"
                )
            );
        }
    }

    void JobBackEndServicesLogin::ProcessSecConnConnectionResult() {
        if (m_oCallContext.GetState() == CallContext::CallSuccess) {
            m_pServices->GetCredentials()->SetSecureConnection(m_pSecureConnection);
            m_pServices->GetSecureConnectionClient()->SetCredentials(
                m_pServices->GetCredentials()
            );
            SetStep(Step(
                (JobStateFunc)&JobBackEndServicesLogin::ConnectToSpecialConnection,
                "JobBackEndServicesLogin::ConnectToSpecialConnection"
            ));
        } else {
            if (m_pSecureConnection != 0) {
                ReleaseStreamCredentials(m_pSecureConnection);
                m_pSecureConnection = 0;
            }
            Complete(m_oCallContext.GetOutcome());
        }
    }

    void JobBackEndServicesLogin::ConnectToSpecialConnection() {
        if (m_oURLs.m_urlSpecial == "") {
            SetStep(Step(
                (JobStateFunc)&JobBackEndServicesLogin::RegisterURLs,
                "JobBackEndServicesLogin::RegisterURLs"
            ));
        } else {
            m_oCallContext.Reset();
            if (!ConnectStream(
                    m_pServices->GetStreamManager()->GetClient(), &m_oCallContext,
                    m_oURLs.m_urlSpecial, &m_pSpecialConnection,
                    Time::ToMilliseconds(m_tTimeout)
                )) {
                Complete(qResult(0x8001000D));
            } else {
                SetToWaiting();
                ResumeOnCallCompletion(
                    &m_oCallContext,
                    new (JBESL_FILE, 0x125) Step(
                        (JobStateFunc)&JobBackEndServicesLogin::ProcessSpecialConnResult,
                        "JobBackEndServicesLogin::ProcessSpecialConnResult"
                    )
                );
            }
        }
    }

    void JobBackEndServicesLogin::ProcessSpecialConnResult() {
        if (m_oCallContext.GetState() == CallContext::CallSuccess) {
            m_pServices->GetCredentials()->SetSpecialConnection(m_pSpecialConnection);
            m_pServices->GetCredentials()->SetSpecialURLs(&m_oURLs.m_lstSpecial);
            SetStep(Step(
                (JobStateFunc)&JobBackEndServicesLogin::RegisterURLs,
                "JobBackEndServicesLogin::RegisterURLs"
            ));
        } else {
            if (m_pSpecialConnection != 0) {
                ReleaseStreamCredentials(m_pSpecialConnection);
                m_pSpecialConnection = 0;
            }
            Complete(m_oCallContext.GetOutcome());
        }
    }

    void JobBackEndServicesLogin::RegisterURLs() {
        m_oProtocolCallContext.Reset();
        m_oProtocolCallContext.SetTimeout(m_tTimeout);
        if (!m_pServices->GetSecureConnectionClient()->RegisterURLs(
                &m_oProtocolCallContext, &m_rAuthResult, &m_urlSecure, &g_urlRegistration
            )) {
            Complete(qResult(0x8001000D));
        } else {
            SetToWaiting();
            ResumeOnCallCompletion(
                &m_oProtocolCallContext,
                new (JBESL_FILE, 0x146) Step(
                    (JobStateFunc)&JobBackEndServicesLogin::CompleteLogin,
                    "JobBackEndServicesLogin::CompleteLogin"
                )
            );
        }
    }

    void JobBackEndServicesLogin::CompleteLogin() {
        if (m_pServices->GetAuthenticationClient()->GetPID() != g_uiGuestPID) {
            if (m_oProtocolCallContext.GetState() != CallContext::CallSuccess) {
                Complete(m_oProtocolCallContext.GetOutcome());
                return;
            } else if (m_rAuthResult.Equals(false)) {
                Complete(m_rAuthResult);
                return;
            }
        }
        m_urlSecure.SetPID(m_pServices->GetCredentials()->GetGuest());
        m_pServices->RegisterLogin();
        SetStep(Step(
            (JobStateFunc)&JobBackEndServicesLogin::DisconnectFromAuthenticationService,
            "JobBackEndServicesLogin::DisconnectFromAuthenticationService"
        ));
    }

    void JobBackEndServicesLogin::DisconnectFromAuthenticationService() {
        m_oCallContext.Reset();
        m_oCallContext.SetTimeout(m_tTimeout);
        if (!m_pServices->GetStreamManager()->GetClient()->Disconnect(
                &m_oCallContext, m_pServices->GetCredentials()->GetConnectionID()
            )) {
            Complete(qResult(0x00010001));
        } else {
            SetToWaiting();
            ResumeOnCallCompletion(
                &m_oCallContext,
                new (JBESL_FILE, 0x17C) Step(
                    (JobStateFunc)&JobBackEndServicesLogin::ProcessAuthDisconnectionResult,
                    "JobBackEndServicesLogin::ProcessAuthDisconnectionResult"
                )
            );
        }
    }

    void JobBackEndServicesLogin::ProcessAuthDisconnectionResult() {
        if (m_oCallContext.GetState() == CallContext::CallSuccess) {
            m_pServices->GetCredentials()->SetAuthenticationConnection(m_pSecureConnection);
            m_pServices->GetAuthenticationClient()->ClearCredentials(
                m_pServices->GetCredentials()
            );
        }
        Complete(qResult(0x00010001));
    }

    // Below 100: retail's frame is 12 bytes larger, reserved at the Logout call
    // (between the temporaries of its argument and of GetInstance). That is the
    // same reservation an inline-but-not-expanded callee leaves, so Logout is most
    // likely inline in the BackEndServices header; its body is not written here.
    void JobBackEndServicesLogin::Complete(qResult result) {
        m_rResult = result;
        if (!m_rResult && m_pServices->GetCredentials() != 0) {
            m_oCallContext.Reset();
            if (m_pServices->Logout(&m_oCallContext, m_pServices->GetCredentials())) {
                SetToWaiting();
                ResumeOnCallCompletion(
                    &m_oCallContext,
                    new (JBESL_FILE, 0x1A2) Step(
                        (JobStateFunc)&JobBackEndServicesLogin::CompleteLogout,
                        "JobBackEndServicesLogin::CompleteLogout"
                    )
                );
                return;
            }
        }
        CallContext *context =
            InstanceControl::GetInstance()->GetCallContextRegister()->GetContext(m_uiCallID);
        if (context != 0) {
            if (m_pResult != 0) {
                *m_pResult = m_rResult;
            }
            if (m_rResult) {
                *m_ppCredentials = m_pServices->GetCredentials();
                context->SetStateToSuccess(m_rResult);
            } else {
                context->SetStateToError(m_rResult);
            }
        }
        SetToComplete();
    }

    void JobBackEndServicesLogin::CompleteLogout() { Complete(m_rResult); }

}
