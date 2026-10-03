// Quazal NetZ - .\Transport\PRUDP\PRUDPStream.cpp
// Retail TU 0x82AFB6E0..0x82AFFEC0 (own functions to 0x82AFE9E0, then the
// COMDAT tail), compiled /Od /Oi- /Ob1. Written from the retail asm.
//
// Load-bearing for the /Od frame layouts:
// - Under /Ob1 an inline candidate the inliner rejects still reserves its
//   locals in the caller's frame. Scheduler::GetSystemLock (0x82A6F650),
//   TransportDelegator::GetInstance (0x823EBC90), EndPointTable::Find/Add/
//   Remove/Trace, ServiceTimeouts and Packet::GetSequenceID are such
//   candidates, so they are defined inline even though retail calls them.
// - Local names decide slot order (a hash walk over the names), so they are
//   chosen to reproduce retail's offsets.
// - StreamID temporaries are created by implicit conversion at the call;
//   retail keeps their address in a slot, which a functional cast does not do.
#include "network/quazal/Transport/PRUDP/PRUDPStream.h"
#include "Platform/RefCountedObject.h"
#include "Core/Scheduler.h"

namespace Quazal {


    class TransportDelegator {
    public:
        static TransportDelegator *GetInstance() {
            unsigned int uiContext = PseudoSingleton::GetCurrentContext();
            InstanceControl *inst =
                (InstanceControl *)InstanceControl::s_oInstanceTable.GetInstanceFromVector(1, uiContext);
            TransportDelegator *pDelegator = inst ? (TransportDelegator *)inst->m_pDelegatorInstance : nullptr;
            return pDelegator;
        }
        RootTransport *GetTransport() { return m_pTransport; }
        char m_pad[0x4c];
        RootTransport *m_pTransport; // 0x4c
    };

    class RootTransport {
    public:
        virtual ~RootTransport();
        virtual void Unk04();
        virtual void Unk08();
        virtual void Unk0C();
        virtual void Unk10();
        virtual void Unk14();
        virtual void Unk18();
        virtual bool Send(unsigned short, unsigned int, unsigned char, unsigned char, PacketOut *, unsigned int);

        unsigned short GetPort() { return m_usPort; }

        char m_pad[0x4e8 - 4];
        unsigned short m_usPort; // 0x4e8
    };

    class StreamListener {
    public:
        virtual void Unk00();
        virtual void ConnectionLost(PRUDPStream *, Buffer *, StationURL *);
        virtual void Unk08();
        virtual void Unk0C();
        virtual bool ConnectionRequest(PRUDPStream *, StationURL *, Buffer *, PRUDPEndPoint *);
    };

    class Buffer {
    public:
        unsigned int GetContentSize() const;
    };

    class LogicalClock {
    public:
        ~LogicalClock() {}
        LogicalClock &operator=(const LogicalClock &o) {
            m_usValue = o.m_usValue;
            return *this;
        }
        unsigned short m_usValue;
    };

    class Packet : public RefCountedObject {
    public:
        enum Type {
            SYN = 0,
            CONNECT = 1,
            DISCONNECT = 5,
        };
        enum Flags {
            FLAG_ACK = 8,
        };

        void SetDestination(const InetAddress *addr) { m_oSource = *addr; }
        unsigned char GetType() { return m_byTypeFlags & 7; }
        bool HasFlag(unsigned char flag) { return (m_byTypeFlags & flag) != 0; }
        void SetFlag(unsigned char flag) { m_byTypeFlags |= flag & 0xF8; }
        unsigned int GetSignature() { return m_uiSignature; }
        LogicalClock GetSequenceID() { return m_oSequenceID; }
        void SetSequenceID(const LogicalClock &id) { m_oSequenceID = id; }
        Buffer *GetPayload() { return m_pPayload; }
        void *GetPendingRequest() { return m_pPendingRequest; }

        char m_pad08[0x12 - 8];
        unsigned char m_byTypeFlags; // 0x12
        char m_pad13[0x14 - 0x13];
        unsigned int m_uiSignature; // 0x14
        LogicalClock m_oSequenceID; // 0x18
        char m_pad1a[0x1c - 0x1a];
        unsigned int m_uiSessionID; // 0x1c
        char m_pad20[0x24 - 0x20];
        Buffer *m_pPayload; // 0x24
        InetAddress m_oSource; // 0x28
        char m_padA8[0xb8 - 0xa8];
        void *m_pPendingRequest; // 0xb8
    };

    class PacketOut : public Packet {
    public:
        PacketOut(unsigned char, unsigned char, unsigned int, Buffer *);
        char m_padBC[0xe0 - 0xbc];
    };

    // Holds StationURLs: retail's dtor clears it through
    // _List_base<StationURL>::clear (0x82A79DA8), which runs ~StationURL.
    class InetAddressList : public qList<StationURL> {
    public:
        InetAddressList() {}
    };

    class NetworkInterfaces {
    public:
        virtual void Unk00();
        virtual void GetLocalAddresses(InetAddressList *);
        static NetworkInterfaces *GetInstance() { return s_pInstance; }
        static NetworkInterfaces *s_pInstance;
    };

    unsigned short GetBroadcastPort();
    unsigned int GetCurrentThreadID();

}

using namespace Quazal;

const char *GetStreamTypeDescription(Stream::Type t) {
    switch (t) {
    case 1:
        return "PRUDP Connections [DO]";
        break;
    case 2:
        return "PRUDP Connections [RVAuthentication]";
        break;
    case 3:
        return "PRUDP Connections [RVSecure]";
        break;
    case 4:
        return "PRUDP Connections [SandBoxMgmt]";
        break;
    case 5:
        return "PRUDP Connections [NAT]";
        break;
    case 6:
        return "PRUDP Connections [SessionDiscovery]";
        break;
    case 7:
        return "PRUDP Connections [NATEcho]";
        break;
    case 8:
        return "PRUDP Connections [Routing]";
        break;
    }
    return "PRUDP Connections";
}

String GetWaterMarkLabel(Stream::Type t) {
    static unsigned short s_usInstance;
    String str;
    str.Format("%s %i", GetStreamTypeDescription(t), s_usInstance++);
    return str;
}

namespace Quazal {

    PRUDPStream::PRUDPStream(Stream::Type type, RootTransport *transport)
        : Stream(type, transport), m_strWaterMarkLabel(GetWaterMarkLabel(type)),
          m_oWaterMark(m_strWaterMarkLabel, true, 60000) {
        m_bTerminating = false;
        m_tLastTimeoutCheck = 0;
        m_uiThreadID = GetCurrentThreadID();
        m_pTransport = transport;
        m_usListeningPort = 0;
    }

    PRUDPStream::~PRUDPStream() {
        if (IsListening())
            StopListen();
        m_bTerminating = true;
        Lock();
        EndPointTable::iterator it;
        for (it = m_oEndPoints.begin(); it != m_oEndPoints.end(); ++it) {
            delete it->second;
        }
        Unlock();
        DeleteReleasedEndPoints();
    }

    unsigned short PRUDPStream::GetListeningPort() {
        if (m_usListeningPort == 0)
            return m_pTransport->GetPort();
        else
            return m_usListeningPort;
    }

    inline RootTransport *GetDefaultTransport() {
        TransportDelegator *d = TransportDelegator::GetInstance();
        if (d == NULL)
            return NULL;
        else
            return d->GetTransport();
    }

    qResult PRUDPStream::Initialize() {
        qResult r = Stream::Initialize();
        if (r.Equals(true) && m_pTransport == NULL) {
            m_pTransport = GetDefaultTransport();
        }
        return r;
    }

    qResult PRUDPStream::Teardown() { return Stream::Teardown(); }

    bool PRUDPStream::StopListen() {
        Stream::StopListen();
        Lock();
        EndPointTable::iterator it;
        for (it = m_oEndPoints.begin(); it != m_oEndPoints.end(); ++it) {
            it->second->Disconnect(4);
        }
        Unlock();
        return true;
    }

    bool PRUDPStream::ResponsibleForURL(const StationURL *url) { return url->GetURLType() == 1; }

    const char *PRUDPStream::GetURLType() { return "prudp"; }

    PRUDPEndPoint *PRUDPStream::CreateEndPoint(const StationURL *url, unsigned short port, bool b) {
        PRUDPEndPoint *ep = NULL;
        ScopedCS cs(*Scheduler::GetSystemLock());
        InetAddress inet;
        if (url->IsValid()) {
            ep = m_oEndPoints.Find(url->GetInetAddress(), url->GetStreamID());
            if (ep == NULL) {
                ep = new (__FILE__, 0xae) PRUDPEndPoint((ConnectionOrientedStream *)this, url);
                m_oWaterMark.Increment(1);
                ep->m_usPort = port;
                m_oEndPoints.Add(ep, url->GetStreamID());
            }
        }
        return ep;
    }

    EndPoint *PRUDPStream::OpenEndPoint(const StationURL *url) {
        ScopedCS cs(*Scheduler::GetSystemLock());
        PRUDPEndPoint *ep = CreateEndPoint(url, GetListeningPort(), true);
        if (ep)
            ep->Open();
        return ep;
    }

    EndPoint *PRUDPStream::OpenEndPoint(unsigned int cid) {
        Lock();
        EndPoint *ep = FindEndPointByCID(cid);
        if (ep)
            ep->Open();
        Unlock();
        return ep;
    }

    bool PRUDPStream::OpenEndPoint(EndPoint *ep) {
        ScopedCS cs(*Scheduler::GetSystemLock());
        ep->Open();
        return true;
    }

    void PRUDPStream::CloseEndPoint(EndPoint *ep) {
        ScopedCS cs(*Scheduler::GetSystemLock());
        ep->Close();
        ReleaseEndPoint((PRUDPEndPoint *)ep);
    }

    void PRUDPStream::ReleaseEndPoint(PRUDPEndPoint *ep) {
        ScopedCS cs(*Scheduler::GetSystemLock());
        if (ep->IsAlive() && !ep->IsDisconnected()) {
            ep->Trace(0x2000000);
            m_oEndPoints.Remove(ep->GetAddress(), ep->GetStreamID());
            ep->SetPID(0);
            ep->SetCID(0);
            m_oWaterMark.Decrement(1);
            m_lstReleasedEndPoints.push_back(ep);
        }
    }

    void PRUDPStream::AddPIDEndPointAssociation(unsigned int pid, EndPoint *ep) {
        ScopedCS cs(*Scheduler::GetSystemLock());
        m_mapPIDs.insert(std::make_pair(pid, ep));
    }

    bool PRUDPStream::RemovePIDEndPointAssociation(unsigned int pid, EndPoint *ep) {
        ScopedCS cs(*Scheduler::GetSystemLock());
        EndPointIDMap::iterator it = m_mapPIDs.find(pid);
        while (it != m_mapPIDs.end() && it->first == pid) {
            if (it->second == ep) {
                m_mapPIDs.erase(it);
                return true;
            }
            ++it;
        }
        return false;
    }

    EndPoint *PRUDPStream::FindEndPointByPID(unsigned int pid) {
        ScopedCS cs(*Scheduler::GetSystemLock());
        EndPointIDMap::iterator it = m_mapPIDs.find(pid);
        if (it != m_mapPIDs.end())
            return it->second;
        else
            return NULL;
    }

    void PRUDPStream::AddCIDEndPointAssociation(unsigned int cid, EndPoint *ep) {
        ScopedCS cs(*Scheduler::GetSystemLock());
        m_mapCIDs.insert(std::make_pair(cid, ep));
    }

    bool PRUDPStream::RemoveCIDEndPointAssociation(unsigned int cid, EndPoint *ep) {
        ScopedCS cs(*Scheduler::GetSystemLock());
        EndPointIDMap::iterator it = m_mapCIDs.find(cid);
        while (it != m_mapCIDs.end() && it->first == cid) {
            if (it->second == ep) {
                m_mapCIDs.erase(it);
                return true;
            }
            ++it;
        }
        return false;
    }

    EndPoint *PRUDPStream::FindEndPointByCID(unsigned int cid) {
        ScopedCS cs(*Scheduler::GetSystemLock());
        EndPointIDMap::iterator it = m_mapCIDs.find(cid);
        if (it != m_mapCIDs.end())
            return it->second;
        else
            return NULL;
    }

    unsigned int PRUDPStream::GetEndPointNumber() { return m_mapCIDs.size(); }

    qResult PRUDPStream::SendBroadcast(StationURL *url, Buffer *buf) {
        StationURL target;
        if (url != NULL)
            target = *url;
        InetAddressList addresses;
        NetworkInterfaces::GetInstance()->GetLocalAddresses(&addresses);
        if (!addresses.empty())
            target.SetAddress("255.255.255.255");
        else
            target.SetAddress("127.0.0.1");
        target.SetPortNumber(GetBroadcastPort());
        return Send(&target, buf);
    }

    qResult PRUDPStream::Send(StationURL *url, Buffer *buf) {
        if (!url->IsValid())
            return qResult(0x80050003);
        PacketOut *packet;
        if (buf != NULL) {
            packet = new (__FILE__, 0x18f) PacketOut(0, 5, 0, buf);
            packet->SetDestination(url->GetInetAddress());
        } else {
            return qResult(0x8001000a);
        }
        bool ok = false;
        ok = Send(GetListeningPort(), GetStreamID(), packet);
        packet->ReleaseRef();
        if (!ok)
            return qResult(0x80050007);
        return qResult(0x10001);
    }

    bool PRUDPStream::Send(unsigned short port, unsigned char id, PacketOut *packet) {
        bool ok = m_pTransport->Send(port, GetType(), GetStreamID(), id, packet, 0);
        return ok;
    }

    bool PRUDPStream::IsCapable(unsigned int cap) {
        switch (cap) {
        case 1:
            return true;
        case 2:
            return true;
        case 3:
            return true;
        case 4:
            return true;
        case 6:
            return true;
        case 7:
            return true;
        default:
            return false;
        }
    }

    PRUDPEndPoint *PRUDPStream::ServiceConnectionRequest(
        InetAddress *addr, Buffer *buf, unsigned short port, unsigned char id
    ) {
        StationURL url;
        url.SetInetAddress(addr);
        url.SetStreamID(id);
        PRUDPEndPoint *ep = CreateEndPoint(&url, port, false);
        if (!ep->IsDisconnected()) {
            bool accepted = true;
            if (GetListener() != NULL) {
                if (buf->GetContentSize() != 0)
                    accepted = GetListener()->ConnectionRequest(this, &ep->m_oURL, buf, ep);
                else
                    accepted = GetListener()->ConnectionRequest(this, &ep->m_oURL, NULL, ep);
            }
            if (accepted) {
            } else {
                ReleaseEndPoint(ep);
                ep = NULL;
            }
        }
        return ep;
    }

    void PRUDPStream::ServiceDisconnection(InetAddress *addr, Buffer *buf) {
        if (GetListener() != NULL) {
            StationURL url;
            url.SetInetAddress(addr);
            if (buf->GetContentSize() != 0)
                GetListener()->ConnectionLost(this, buf, &url);
            else
                GetListener()->ConnectionLost(this, NULL, &url);
        }
    }

    bool PRUDPStream::ReceiveIncomingPacket(unsigned short port, unsigned char id, Packet *packet) {
        if (packet != NULL) {
            PRUDPEndPoint *pEndPoint = NULL;
            InetAddress *pSource = &packet->m_oSource;
            unsigned int uiSession =
                m_oSignatureGenerator.ComputeSourceSignature(packet->m_oSource.GetAddress(), packet->m_oSource.GetPortNumber());
            if (packet->GetSignature() != uiSession && packet->GetType() != Packet::SYN
                && packet->GetType() != Packet::DISCONNECT)
                return false;
            if (packet->GetPendingRequest() == NULL) {
                switch (packet->GetType()) {
                case Packet::SYN:
                    if (packet->HasFlag(Packet::FLAG_ACK)) {
                        Lock();
                        pEndPoint = m_oEndPoints.Find(pSource, id);
                        Unlock();
                    } else {
                        PacketOut *reply = new (__FILE__, 0x212) PacketOut(0, 0, 0, NULL);
                        reply->SetFlag(Packet::FLAG_ACK);
                        reply->SetSequenceID(packet->GetSequenceID());
                        reply->m_uiSessionID = uiSession;
                        reply->m_oSource = packet->m_oSource;
                        Send(port, id, reply);
                        reply->ReleaseRef();
                    }
                    break;
                case Packet::CONNECT:
                    Lock();
                    if (!packet->HasFlag(Packet::FLAG_ACK)) {
                        if (packet->GetSignature() != uiSession) {
                            // retail branches over an empty block here
                        } else {
                            Unlock();
                            pEndPoint = ServiceConnectionRequest(pSource, packet->GetPayload(), port, id);
                            Lock();
                        }
                    } else {
                        pEndPoint = m_oEndPoints.Find(pSource, id);
                    }
                    Unlock();
                    break;
                case Packet::DISCONNECT:
                    ServiceDisconnection(pSource, packet->GetPayload());
                    break;
                default:
                    pEndPoint = m_oEndPoints.Find(pSource, id);
                    break;
                }
                if (pEndPoint != NULL && !pEndPoint->IsClosed())
                    pEndPoint->ServiceIncomingPacket((PacketIn *)packet);
            }
            packet->ReleaseRef();
        }
        return true;
    }

    void PRUDPStream::DeleteReleasedEndPoints() {
        ScopedCS cs(*Scheduler::GetSystemLock());
        while (!m_lstReleasedEndPoints.empty()) {
            PRUDPEndPoint *ep = m_lstReleasedEndPoints.front();
            delete ep;
            m_lstReleasedEndPoints.pop_front();
        }
    }

    void PRUDPStream::DoWork() {
        ServiceTimeouts();
        DeleteReleasedEndPoints();
    }

    void PRUDPStream::Trace(unsigned int flags) {
        ScopedCS cs(*Scheduler::GetSystemLock());
        m_oEndPoints.Trace(flags);
    }

}
