// Quazal NetZ - .\Stack\Core\IOCompletionNotifier.cpp
//
// Retail TU: .text 0x82B383E0..0x82B392D0, compiled /Od /Oi- /Ob1 /GR- /EHs-c-
// (see objects.json). No .pdata record in the TU has the EH bit although
// CreateIOCompletionContext, DeleteIOCompletionContext and Wait hold a
// ScopedCS, and the TU defines no vtable (so /GR- cannot be read from its
// .rdata; it follows the transport TUs around it). Its .rdata is the __FILE__
// string alone.
//
// The notifier owns up to eight outstanding IOCompletionContexts, each with a
// WSA event. Wait either asks the installed SocketDriver to poll the pending
// sockets, or waits on the events.
//
// The classes are declared here with the layouts retail uses rather than taken
// from the shared Quazal headers.
//
// This TU is built /Od: its locals are laid out by a walk over the scope's
// symbol hash table, so the local NAMES below determine the stack offsets.

#define IOCN_FILE ".\\Stack\\Core\\IOCompletionNotifier.cpp"

extern "C" {
typedef void *WSAEVENT;
typedef unsigned int SOCKET;

struct WSAOVERLAPPED {
    unsigned long Internal;
    unsigned long InternalHigh;
    unsigned long Offset;
    unsigned long OffsetHigh;
    WSAEVENT hEvent;
};

WSAEVENT WSACreateEvent();
int WSACloseEvent(WSAEVENT);
int WSAResetEvent(WSAEVENT);
unsigned long WSAWaitForMultipleEvents(unsigned long, const WSAEVENT *, int, unsigned long, int);
int WSAGetOverlappedResult(SOCKET, WSAOVERLAPPED *, unsigned long *, int, unsigned long *);
int WSAGetLastError();
}

namespace Quazal {

    class RootObject {
    public:
        static void *operator new(unsigned int, const char *, unsigned int);
        static void operator delete(void *);
        ~RootObject() {}
    };

    // Array allocation with a count header (retail 0x82B2B498 / 0x82A87E98).
    template <class T>
    T *qNewArray(unsigned int, const char *, unsigned int);
    template <class T>
    void qDeleteArray(T *);

    class MutexPrimitive {
    public:
        static bool s_bNoOp;
    };

    class CriticalSection : public RootObject {
    public:
        CriticalSection(unsigned int);
        ~CriticalSection();
        void EnterImpl();
        void LeaveImpl();
        void Enter() {
            if (!MutexPrimitive::s_bNoOp)
                EnterImpl();
        }
        void Leave() {
            if (!MutexPrimitive::s_bNoOp)
                LeaveImpl();
        }

        char m_data[0x14];
    };

    class ScopedCS : public RootObject {
    public:
        ScopedCS(CriticalSection &cs) : m_bInScope(true), critSec(&cs) { critSec->Enter(); }
        ~ScopedCS() { EndScope(); }
        void EndScope() {
            if (m_bInScope) {
                critSec->Leave();
                m_bInScope = false;
            }
        }

        bool m_bInScope; // 0x0
        CriticalSection *critSec; // 0x4
    };

    class SocketDriver : public RootObject {
    public:
        class Socket;

        // Poll entry: the driver socket, the events asked for, the events seen
        // (bit 0 read, bit 1 write).
        struct PollInfo {
            PollInfo() : m_pSocket(0), m_iFlags(0), m_iResult(0) {}

            Socket *m_pSocket; // 0x0
            int m_iFlags; // 0x4
            int m_iResult; // 0x8
        };

        virtual ~SocketDriver() {}
        virtual Socket *Create() = 0;
        virtual void Delete(Socket *) = 0;
        virtual bool Poll(PollInfo *, unsigned int, unsigned int) = 0;
    };

    class InetAddress;
    class IOCompletionContext;

    class Socket : public RootObject {
    public:
        static SocketDriver *s_pSocketDriver;
        static SocketDriver *GetSocketDriver() { return s_pSocketDriver; }

        void CompleteSend(unsigned char *, unsigned int, InetAddress *, IOCompletionContext *);
        void CompleteRecv(unsigned char *, unsigned int, InetAddress *, IOCompletionContext *);
        SOCKET GetHandle() const { return m_hSocket; }
        SocketDriver::Socket *GetDriverSocket() const { return m_pDriverSocket; }

        char m_pad0[0x90];
        SOCKET m_hSocket; // 0x90
        char m_pad94[0x9c - 0x94];
        SocketDriver::Socket *m_pDriverSocket; // 0x9c
    };

    class UserContext : public RootObject {
    public:
        UserContext() { m_uiValue = 0; }
        unsigned int m_uiValue;
    };

    class IOCompletionContext : public RootObject {
    public:
        enum _State {
            Idle = 0,
            Pending = 1,
            Completed = 2,
        };
        enum _Operation {
            None = 0,
            Connect = 1,
            Send = 2,
            Recv = 3,
        };

        IOCompletionContext() { Reset(); }
        void Reset();
        bool IsPending() const { return m_ucState == Pending; }
        bool IsCompleted() const { return m_ucState == Completed; }
        bool IsConnect() const { return m_ucOperation == Connect; }
        bool IsSend() const { return m_ucOperation == Send; }
        bool IsRecv() const { return m_ucOperation == Recv; }
        WSAEVENT GetEvent() const { return m_oOverlapped.hEvent; }
        Socket *GetSocket() const { return m_pSocket; }
        unsigned char *GetData() const { return m_pData; }
        unsigned int GetSize() const { return m_uiSize; }
        InetAddress *GetAddress() const { return m_pAddress; }
        void SetEvent(WSAEVENT hEvent) { m_oOverlapped.hEvent = hEvent; }
        void SetCompleted() {
            m_ucState = Completed;
            m_ucOperation = None;
        }
        void ResetResult() { m_uiResult = 0; }

        unsigned char *m_pData; // 0x0
        unsigned int m_uiSize; // 0x4
        unsigned int m_uiTransferred; // 0x8
        unsigned int m_uiResult; // 0xc
        InetAddress *m_pAddress; // 0x10
        unsigned char m_ucState; // 0x14
        unsigned char m_ucOperation; // 0x15
        UserContext m_oUserContext; // 0x18
        WSAOVERLAPPED m_oOverlapped; // 0x1c
        char m_pad30[0x38 - 0x30];
        Socket *m_pSocket; // 0x38
    };

    class IOCompletionNotifier : public RootObject {
    public:
        enum {
            MaxContexts = 8
        };

        IOCompletionNotifier();
        ~IOCompletionNotifier();

        IOCompletionContext *CreateIOCompletionContext();
        void DeleteIOCompletionContext(IOCompletionContext *);
        int GetOverlappedResult(IOCompletionContext *, bool *, unsigned int *);
        bool WaitForIOCompletion(IOCompletionContext *, unsigned int);
        static unsigned int WaitForEvents(WSAEVENT *, unsigned int, unsigned int);
        bool PollSocketDriver(unsigned int);
        bool Wait(unsigned int);

        CriticalSection m_oCS; // 0x0
        IOCompletionContext *m_apContexts[MaxContexts]; // 0x14
        unsigned int m_uiUnused34; // 0x34
        WSAEVENT *m_phEvents; // 0x38
    };

    IOCompletionNotifier::IOCompletionNotifier() : m_oCS(0x40000000) {
        m_phEvents = qNewArray<WSAEVENT>(MaxContexts, IOCN_FILE, 0x22);
        for (unsigned int i = 0; i < MaxContexts; i++) {
            m_apContexts[i] = 0;
            m_phEvents[i] = WSACreateEvent();
            WSAResetEvent(m_phEvents[i]);
        }
    }

    IOCompletionNotifier::~IOCompletionNotifier() {
        for (unsigned int i = 0; i < MaxContexts; i++) {
            WSACloseEvent(m_phEvents[i]);
        }
        qDeleteArray(m_phEvents);
    }

    IOCompletionContext *IOCompletionNotifier::CreateIOCompletionContext() {
        ScopedCS oCS(m_oCS);
        unsigned int i;
        for (i = 0; i < MaxContexts && m_apContexts[i] != 0; i++) {
        }
        m_apContexts[i] = new (IOCN_FILE, 0x3e) IOCompletionContext;
        m_apContexts[i]->SetEvent(m_phEvents[i]);
        return m_apContexts[i];
    }

    void IOCompletionNotifier::DeleteIOCompletionContext(IOCompletionContext *pContext) {
        ScopedCS oCS(m_oCS);
        unsigned int i;
        for (i = 0; i < MaxContexts && m_apContexts[i] != pContext; i++) {
        }
        delete m_apContexts[i];
        m_apContexts[i] = 0;
        WSAResetEvent(m_phEvents[i]);
    }

    int IOCompletionNotifier::GetOverlappedResult(
        IOCompletionContext *pContext, bool *pbIncomplete, unsigned int *puiError
    ) {
        *puiError = 0;
        *pbIncomplete = false;
        unsigned long ulFlags;
        int iResult;
        unsigned long cbTransfer;
        iResult = WSAGetOverlappedResult(
            pContext->GetSocket()->GetHandle(), &pContext->m_oOverlapped, &cbTransfer, 0, &ulFlags
        );
        if (iResult == 0) {
            *puiError = WSAGetLastError();
            if (*puiError == 996) { // WSA_IO_INCOMPLETE
                *pbIncomplete = true;
            }
        }
        return iResult != 0;
    }

    bool IOCompletionNotifier::WaitForIOCompletion(IOCompletionContext *pContext, unsigned int uiTimeout) {
        if (pContext->IsPending()) {
            WSAEVENT hEvent = pContext->GetEvent();
            unsigned long ulResult = WSAWaitForMultipleEvents(1, &hEvent, 0, uiTimeout, 0);
            if (ulResult == 0) {
                return true;
            } else {
                return false;
            }
        }
        return true;
    }

    unsigned int IOCompletionNotifier::WaitForEvents(WSAEVENT *phEvents, unsigned int uiNbEvents, unsigned int uiTimeout) {
        unsigned long ulResult = WSAWaitForMultipleEvents(uiNbEvents, phEvents, 0, uiTimeout, 0);
        switch (ulResult) {
        case 0x102: // WAIT_TIMEOUT
            return -1;
        case 0xc0: // WAIT_IO_COMPLETION
            break;
        case 0xffffffff: // WSA_WAIT_FAILED
            break;
        default:
            return ulResult;
        }
        return -1;
    }

    bool IOCompletionNotifier::PollSocketDriver(unsigned int uiTimeout) {
        SocketDriver::PollInfo aPollInfo[MaxContexts];
        unsigned int uiNbReadOps;
        unsigned int uiNbOps = 0;
        unsigned int uiNbWriteOps = 0;
        unsigned int auiIndices[MaxContexts];
        unsigned int i;
        uiNbReadOps = 0;
        {
            ScopedCS oCS(m_oCS);
            for (i = 0; i < MaxContexts; i++) {
                if (m_apContexts[i] != 0 && m_apContexts[i]->IsPending()) {
                    aPollInfo[uiNbOps].m_pSocket = m_apContexts[i]->GetSocket()->GetDriverSocket();
                    auiIndices[uiNbOps] = i;
                    if (m_apContexts[i]->IsSend()) {
                        aPollInfo[uiNbOps].m_iFlags = 2;
                        uiNbWriteOps++;
                    }
                    if (m_apContexts[i]->IsRecv()) {
                        aPollInfo[uiNbOps].m_iFlags = 1;
                        uiNbReadOps++;
                    }
                    if (m_apContexts[i]->IsConnect()) {
                        aPollInfo[uiNbOps].m_iFlags = 2;
                        uiNbReadOps++;
                    }
                    uiNbOps++;
                }
            }
        }
        if (uiNbOps == 0 || !Socket::GetSocketDriver()->Poll(aPollInfo, uiNbOps, uiTimeout)) {
            return false;
        }
        {
            ScopedCS oCS(m_oCS);
            for (i = 0; i < uiNbOps; i++) {
                IOCompletionContext *pContext = m_apContexts[auiIndices[i]];
                if (pContext != 0 && pContext->IsPending()) {
                    if (pContext->IsSend() && (aPollInfo[i].m_iResult & 2)) {
                        pContext->GetSocket()->CompleteSend(
                            pContext->GetData(), pContext->GetSize(), pContext->GetAddress(), pContext
                        );
                    }
                    if (pContext->IsConnect() && (aPollInfo[i].m_iResult & 2)) {
                        m_apContexts[i]->SetCompleted();
                        m_apContexts[i]->ResetResult();
                    }
                    if (pContext->IsRecv() && (aPollInfo[i].m_iResult & 1)) {
                        pContext->GetSocket()->CompleteRecv(
                            pContext->GetData(), pContext->GetSize(), pContext->GetAddress(), pContext
                        );
                    }
                }
            }
        }
        return true;
    }

    bool IOCompletionNotifier::Wait(unsigned int uiTimeout) {
        {
            ScopedCS oCS(m_oCS);
            for (unsigned int i = 0; i < MaxContexts; i++) {
                if (m_apContexts[i] != 0 && m_apContexts[i]->IsCompleted()) {
                    return true;
                }
            }
        }
        if (Socket::s_pSocketDriver != 0) {
            return PollSocketDriver(uiTimeout);
        }
        if (Socket::s_pSocketDriver == 0) {
            unsigned int uiEvent = WaitForEvents(m_phEvents, MaxContexts, uiTimeout);
            if (uiEvent != -1) {
                ScopedCS oCS(m_oCS);
                for (unsigned int j = 0; j < MaxContexts; j++) {
                    if (m_apContexts[j] != 0 && WaitForEvents(&m_phEvents[j], 1, 0) == 0) {
                        m_apContexts[j]->SetCompleted();
                        WSAResetEvent(m_phEvents[j]);
                    }
                }
                return true;
            } else {
                return false;
            }
        }
        return false;
    }

}
