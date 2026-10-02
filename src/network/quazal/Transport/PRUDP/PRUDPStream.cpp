// Quazal NetZ - .\Transport\PRUDP\PRUDPStream.cpp
// Retail TU 0x82AFB6E0..0x82AFFEC0, compiled /Od /Oi- /Ob1 like the rest of the
// Quazal block. Written from the retail asm.
#include "network/quazal/Transport/PRUDP/PRUDPStream.h"
#include "Platform/RefCountedObject.h"

namespace Quazal {

    class SystemLock {
    public:
        static CriticalSection *Get();
    };

    class InstanceDelegator {
    public:
        static InstanceDelegator *GetInstance();
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

    class Packet : public RefCountedObject {
    public:

        InetAddress *GetSourceAddress() { return &m_oSource; }
        char m_pad06[0x12 - 6];
        unsigned char m_byTypeFlags; // 0x12
        char m_pad13[0x14 - 0x13];
        unsigned int m_uiSessionID; // 0x14
        unsigned short m_usSequenceID; // 0x18
        char m_pad1a[0x24 - 0x1a];
        Buffer *m_pPayload; // 0x24
        InetAddress m_oSource; // 0x28
    };

    class PacketOut : public Packet {
    public:
        PacketOut(unsigned char, unsigned char, unsigned int, Buffer *);
    };

    class NetworkInterfaces {
    public:
        virtual void Unk00();
        virtual void GetLocalAddresses(qList<InetAddress> *);
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
        if (m_byStreamID != 0)
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

    static RootTransport *GetDefaultTransport() {
        InstanceDelegator *d = InstanceDelegator::GetInstance();
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
        ScopedCS cs(*SystemLock::Get());
        InetAddress addr;
        if (url->IsValid()) {
            ep = m_oEndPoints.Find(url->GetInetAddress(), StreamID(url->GetStreamID()));
            if (ep == NULL) {
                ep = new (__FILE__, 0xae) PRUDPEndPoint(this, url);
                m_oWaterMark.Increment(1);
                ep->m_usPort = port;
                m_oEndPoints.Add(ep, StreamID(url->GetStreamID()));
            }
        }
        return ep;
    }

    EndPoint *PRUDPStream::OpenEndPoint(const StationURL *url) {
        ScopedCS cs(*SystemLock::Get());
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
        ScopedCS cs(*SystemLock::Get());
        ep->Open();
        return true;
    }

    void PRUDPStream::CloseEndPoint(EndPoint *ep) {
        ScopedCS cs(*SystemLock::Get());
        ep->Close();
        ReleaseEndPoint((PRUDPEndPoint *)ep);
    }

    void PRUDPStream::ReleaseEndPoint(PRUDPEndPoint *ep) {
        ScopedCS cs(*SystemLock::Get());
        if (!ep->IsReleased() && !ep->IsDisconnected()) {
            ep->SetFlag(0x2000000);
            m_oEndPoints.Remove(ep->m_oURL.GetInetAddress(), StreamID(ep->m_oURL.GetStreamID()));
            ep->SetPID(0);
            ep->SetCID(0);
            m_oWaterMark.Decrement(1);
            m_lstReleasedEndPoints.push_back(ep);
        }
    }

    void PRUDPStream::AddPIDEndPointAssociation(unsigned int pid, EndPoint *ep) {
        ScopedCS cs(*SystemLock::Get());
        m_mapPIDs.insert(std::make_pair(pid, ep));
    }

    bool PRUDPStream::RemovePIDEndPointAssociation(unsigned int pid, EndPoint *ep) {
        ScopedCS cs(*SystemLock::Get());
        for (EndPointIDMap::iterator it = m_mapPIDs.find(pid);
             it != m_mapPIDs.end() && it->first == pid;
             ++it) {
            if (it->second == ep) {
                m_mapPIDs.erase(it);
                return true;
            }
        }
        return false;
    }

    EndPoint *PRUDPStream::FindEndPointByPID(unsigned int pid) {
        ScopedCS cs(*SystemLock::Get());
        EndPointIDMap::iterator it = m_mapPIDs.find(pid);
        if (it != m_mapPIDs.end())
            return it->second;
        else
            return NULL;
    }

    void PRUDPStream::AddCIDEndPointAssociation(unsigned int cid, EndPoint *ep) {
        ScopedCS cs(*SystemLock::Get());
        m_mapCIDs.insert(std::make_pair(cid, ep));
    }

    bool PRUDPStream::RemoveCIDEndPointAssociation(unsigned int cid, EndPoint *ep) {
        ScopedCS cs(*SystemLock::Get());
        for (EndPointIDMap::iterator it = m_mapCIDs.find(cid);
             it != m_mapCIDs.end() && it->first == cid;
             ++it) {
            if (it->second == ep) {
                m_mapCIDs.erase(it);
                return true;
            }
        }
        return false;
    }

    EndPoint *PRUDPStream::FindEndPointByCID(unsigned int cid) {
        ScopedCS cs(*SystemLock::Get());
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
        qList<InetAddress> addresses;
        NetworkInterfaces::s_pInstance->GetLocalAddresses(&addresses);
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
            packet->m_oSource = *url->GetInetAddress();
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
        url.SetAddress(addr);
        url.SetStreamID(id);
        PRUDPEndPoint *ep = CreateEndPoint(&url, port, false);
        if (!ep->IsDisconnected()) {
            bool accepted = true;
            if (m_pListener != NULL) {
                if (buf->GetContentSize() != 0)
                    accepted = m_pListener->ConnectionRequest(this, &ep->m_oURL, buf, ep);
                else
                    accepted = m_pListener->ConnectionRequest(this, &ep->m_oURL, NULL, ep);
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
        if (m_pListener != NULL) {
            StationURL url;
            url.SetAddress(addr);
            if (buf->GetContentSize() != 0)
                m_pListener->ConnectionLost(this, buf, &url);
            else
                m_pListener->ConnectionLost(this, NULL, &url);
        }
    }

    bool PRUDPStream::ReceiveIncomingPacket(unsigned short port, unsigned char id, Packet *packet) {
        if (packet == NULL)
            return true;
        PRUDPEndPoint *ep = NULL;
        InetAddress *source = packet->GetSourceAddress();
        unsigned int session = m_oSessionIDs.Lookup(
            &packet->m_oSource, packet->m_oSource.m_data[0]
        );
        return true;
    }

    void PRUDPStream::DeleteReleasedEndPoints() {
        ScopedCS cs(*SystemLock::Get());
        while (!m_lstReleasedEndPoints.empty()) {
            delete m_lstReleasedEndPoints.front();
            m_lstReleasedEndPoints.pop_front();
        }
    }

    void PRUDPStream::DoWork() {
        ServiceTimeouts();
        DeleteReleasedEndPoints();
    }

    void PRUDPStream::Trace(unsigned int flags) {
        ScopedCS cs(*SystemLock::Get());
        m_oEndPoints.Trace(flags);
    }

}
