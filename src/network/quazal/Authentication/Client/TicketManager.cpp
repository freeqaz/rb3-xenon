// Quazal NetZ - Authentication/Client/TicketManager.cpp
// Retail TU: .text 0x82B3F4C8..0x82B3FBF0 (9 functions), built
// /Od /Oi- /EHs-c- /Ob1 (objects.json).
//
// The TU continues into the map's _M_erase at 0x82B3FBF0, which is the ICF
// survivor of the identical map<unsigned int, X*> instantiations and is pinned
// with BandwidthCounter.
//
// The declarations below are local to this TU; their layouts are the ones the
// retail code uses.

#include "Core/Scheduler.h"
#include "Platform/ScopedCS.h"

#define TICKETMANAGER_FILE ".\\Authentication\\Client\\TicketManager.cpp"

namespace Quazal {

    class Data;
    class RVConnectionData;
    template <class T1, class T2>
    class AnyObjectHolder;

    class Ticket : public RefCountedObject {
    public:
        unsigned int GetKey() const { return m_uiKey; }

        unsigned int m_unk8[7];
        unsigned int m_uiKey; // 0x24
    };

    class AuthenticationClient : public RootObject {
    public:
        Time GetTimeout() const { return m_tTimeout; }

        unsigned int m_unk0[12];
        Time m_tTimeout; // 0x30
    };

    class JobTicketManagerLogin;
    class JobTicketManagerAcquireTicket;

    class TicketManager : public RootObject {
    public:
        TicketManager(AuthenticationClient *);
        ~TicketManager();

        bool Login(
            CallContext *, qResult *, const String &, const char *, unsigned int *,
            RVConnectionData *, String *, AnyObjectHolder<Data, String> *
        );
        bool AcquireTicket(CallContext *, unsigned int, unsigned int, Ticket **);
        Ticket *FindTicket(unsigned int);
        void InsertTicket(unsigned int, Ticket *);
        void ReleaseTicket(Ticket *);

        qMap<unsigned int, Ticket *> m_mapTickets; // 0x0
        AuthenticationClient *m_pAuthenticationClient; // 0x1c
    };

    class JobTicketManagerLogin : public Job {
    public:
        JobTicketManagerLogin(
            unsigned int, TicketManager *, qResult *, const String &, const char *,
            unsigned int *, RVConnectionData *, String *, AnyObjectHolder<Data, String> *
        );
        virtual void Execute();

        unsigned char m_unk38[0x138];
    };

    class JobTicketManagerAcquireTicket : public Job {
    public:
        JobTicketManagerAcquireTicket(
            unsigned int, TicketManager *, unsigned int, unsigned int, Ticket **
        );
        virtual void Execute();

        unsigned char m_unk38[0xc8];
    };

    TicketManager::TicketManager(AuthenticationClient *pAuthenticationClient)
        : m_pAuthenticationClient(pAuthenticationClient) {}

    TicketManager::~TicketManager() {}

    bool TicketManager::Login(
        CallContext *pContext,
        qResult *pResult,
        const String &strUserName,
        const char *szPassword,
        unsigned int *puiPID,
        RVConnectionData *pConnectionData,
        String *pstrReturnMsg,
        AnyObjectHolder<Data, String> *pCustomData
    ) {
        ScopedCS oCS(Scheduler::GetInstance()->unk38);
        if (!pContext->InitiateCall())
            return false;
        Scheduler::GetInstance()->Queue(
            new (TICKETMANAGER_FILE, 0x2d) JobTicketManagerLogin(
                pContext->GetID(), this, pResult, strUserName, szPassword, puiPID,
                pConnectionData, pstrReturnMsg, pCustomData
            ),
            false
        );
        return true;
    }

    bool TicketManager::AcquireTicket(
        CallContext *pContext, unsigned int uiSource, unsigned int uiTarget, Ticket **ppTicket
    ) {
        ScopedCS oCS(Scheduler::GetInstance()->unk38);
        if (!pContext->InitiateCall())
            return false;
        pContext->SetTimeout(m_pAuthenticationClient->GetTimeout());
        Scheduler::GetInstance()->Queue(
            new (TICKETMANAGER_FILE, 0x3d) JobTicketManagerAcquireTicket(
                pContext->GetID(), this, uiSource, uiTarget, ppTicket
            ),
            false
        );
        return true;
    }

    Ticket *TicketManager::FindTicket(unsigned int uiKey) {
        qMap<unsigned int, Ticket *>::iterator it = m_mapTickets.find(uiKey);
        if (it != m_mapTickets.end())
            return it->second;
        return 0;
    }

    void TicketManager::InsertTicket(unsigned int uiKey, Ticket *pTicket) {
        m_mapTickets[uiKey] = pTicket;
    }

    void TicketManager::ReleaseTicket(Ticket *pTicket) {
        pTicket->ReleaseRef();
        if (pTicket->GetRefCount() == 1) {
            m_mapTickets.erase(pTicket->GetKey());
            pTicket->ReleaseRef();
        }
    }

}
