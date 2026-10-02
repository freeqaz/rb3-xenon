#pragma once
#include "Platform/RootObject.h"
#include "Platform/ScopedCS.h"
#include "Platform/String.h"
#include "Platform/Result.h"
#include "Platform/WaterMark.h"
#include "Platform/qStd.h"
#include "Platform/Time.h"
#include "Platform/TraceLog.h"

namespace Quazal {

    class RootTransport;
    class Buffer;
    class Packet;
    class PacketOut;
    class PRUDPStream;

    // Retail X360 layout (0x80 bytes): only the members this TU touches are spelled out.
    class InetAddress {
    public:
        InetAddress();
        InetAddress(const InetAddress &);
        ~InetAddress();
        InetAddress &operator=(const InetAddress &);
        unsigned int GetAddress() const;
        bool operator<(const InetAddress &) const;
        bool operator==(const InetAddress &) const;
        unsigned short GetPortNumber() const;

        char m_data[0x80];
    };

    class StreamID {
    public:
        StreamID(unsigned char id) { m_byID = id; }
        ~StreamID() {}
        operator unsigned char() const { return m_byID; }

        unsigned char m_byID;
    };

    class StationURL : public RootObject {
    public:
        StationURL();
        virtual ~StationURL();
        StationURL &operator=(const StationURL &);
        bool IsValid() const;
        unsigned char GetStreamID() const;
        InetAddress *GetInetAddress() const;
        const InetAddress &GetAddress() const { return *GetInetAddress(); }
        int GetURLType() const;
        void SetAddress(const char *);
        void SetAddress(const InetAddress *);
        void SetPortNumber(unsigned short);
        void SetStreamID(unsigned char);

        char m_data[0x60];
    };

    class EndPointUniqueID {
    public:
        EndPointUniqueID(InetAddress addr, unsigned char id) {
            m_oAddress = addr;
            m_byStreamID = id;
        }
        bool operator<(const EndPointUniqueID &o) const {
            return m_oAddress < o.m_oAddress
                || (m_oAddress == o.m_oAddress && m_byStreamID < o.m_byStreamID);
        }

        InetAddress m_oAddress; // 0x0
        unsigned char m_byStreamID; // 0x80
    };

    class EndPoint : public RootObject {
    public:
        virtual void Unk00();
        virtual void Unk04();
        virtual void Unk08();
        virtual bool IsClosed();
        virtual void Unk10();
        virtual bool IsDisconnected();
        virtual void Unk18();
        virtual void Unk1C();
        virtual void Unk20();
        virtual void Unk24();
        virtual void Unk28();
        virtual void Unk2C();
        virtual void Unk30();
        virtual void Unk34();
        virtual void Unk38();
        virtual void Disconnect(unsigned int);
        virtual void Unk40();
        virtual void Trace(unsigned int);
        virtual void Unk48();
        virtual void Unk4C();
        virtual void Unk50();
        virtual void Unk54();
        virtual void Unk58();
        virtual ~EndPoint();

        void Open();
        void Close();
        void SetPID(unsigned int);
        void SetCID(unsigned int);
        bool IsReleased() { return m_pReleaser != NULL; }
        bool IsAlive() { return !IsReleased(); }
        InetAddress *GetAddress() { return m_oURL.GetInetAddress(); }
        unsigned char GetStreamID() { return m_oURL.GetStreamID(); }

        unsigned short m_usRefCount; // 0x4
        StationURL m_oURL; // 0x8
        char m_pad60[0x7c - 0x8 - sizeof(StationURL)];
        void *m_pReleaser; // 0x7c
        unsigned short m_usPort; // 0x80
    };

    class PRUDPEndPoint : public EndPoint {
    public:
        PRUDPEndPoint(PRUDPStream *, const StationURL *);
        void ProcessPacket(Packet *);
        char m_pad84[0x138 - 0x84];
    };

    class EndPointMap : public qMap<EndPointUniqueID, PRUDPEndPoint *> {};

    class EndPointTable : public EndPointMap {
    public:
        PRUDPEndPoint *Find(const InetAddress *addr, StreamID id) {
            EndPointUniqueID key(*addr, id);
            iterator it = find(key);
            if (it == end())
                return NULL;
            else
                return it->second;
        }
        void Add(PRUDPEndPoint *ep, StreamID id) {
            insert(value_type(EndPointUniqueID(*ep->GetAddress(), id), ep));
        }
        void Remove(const InetAddress *addr, StreamID id) {
            EndPointUniqueID key(*addr, id);
            iterator it = find(key);
            if (it == end())
                return;
            else
                erase(it);
        }
        void Trace(unsigned int flags) {
            if (!TraceLog::GetInstance()->IsTraceEnabled(flags))
                return;
            iterator it = begin();
            while (it != end()) {
                it->second->Trace(flags);
                ++it;
            }
        }
    };

    class StreamListener;

    class Stream : public RootObject {
    public:
        enum Type {
        };

        Stream(Type, RootTransport *);
        virtual ~Stream();
        virtual bool ReceiveIncomingPacket(unsigned short, unsigned char, Packet *) = 0;
        virtual void DoWork() = 0;
        virtual void Unk0C();
        virtual bool StopListen();
        virtual bool ResponsibleForURL(const StationURL *);
        virtual const char *GetURLType();
        virtual bool OpenEndPoint(EndPoint *) = 0;
        virtual EndPoint *OpenEndPoint(unsigned int) = 0;
        virtual EndPoint *OpenEndPoint(const StationURL *) = 0;
        virtual void AddPIDEndPointAssociation(unsigned int, EndPoint *) = 0;
        virtual bool RemovePIDEndPointAssociation(unsigned int, EndPoint *) = 0;
        virtual EndPoint *FindEndPointByPID(unsigned int) = 0;
        virtual unsigned int GetEndPointNumber() = 0;
        virtual void AddCIDEndPointAssociation(unsigned int, EndPoint *) = 0;
        virtual bool RemoveCIDEndPointAssociation(unsigned int, EndPoint *) = 0;
        virtual EndPoint *FindEndPointByCID(unsigned int) = 0;
        virtual void CloseEndPoint(EndPoint *) = 0;
        virtual void Lock();
        virtual void Unlock();
        virtual qResult Initialize();
        virtual qResult Teardown();
        virtual void Unk58();
        virtual qResult SendBroadcast(StationURL *, Buffer *);
        virtual qResult Send(StationURL *, Buffer *);
        virtual void Unk64();
        virtual void Unk68();
        virtual bool IsCapable(unsigned int);
        virtual void Trace(unsigned int);

        Type GetType() { return m_eType; }
        unsigned char GetStreamID() { return m_byStreamID; }
        bool IsListening() { return m_byStreamID != 0; }
        StreamListener *GetListener() { return m_pListener; }

        Type m_eType; // 0x4
        StreamListener *m_pListener; // 0x8
        RootTransport *m_pTransport; // 0xc
        unsigned char m_byStreamID; // 0x10
    };

    class PRUDPStream : public Stream {
    public:
        PRUDPStream(Stream::Type, RootTransport *);
        virtual ~PRUDPStream();
        virtual bool ReceiveIncomingPacket(unsigned short, unsigned char, Packet *);
        virtual void DoWork();
        virtual bool StopListen();
        virtual bool ResponsibleForURL(const StationURL *);
        virtual const char *GetURLType();
        virtual bool OpenEndPoint(EndPoint *);
        virtual EndPoint *OpenEndPoint(unsigned int);
        virtual EndPoint *OpenEndPoint(const StationURL *);
        virtual void AddPIDEndPointAssociation(unsigned int, EndPoint *);
        virtual bool RemovePIDEndPointAssociation(unsigned int, EndPoint *);
        virtual EndPoint *FindEndPointByPID(unsigned int);
        virtual unsigned int GetEndPointNumber();
        virtual void AddCIDEndPointAssociation(unsigned int, EndPoint *);
        virtual bool RemoveCIDEndPointAssociation(unsigned int, EndPoint *);
        virtual EndPoint *FindEndPointByCID(unsigned int);
        virtual void CloseEndPoint(EndPoint *);
        virtual qResult Initialize();
        virtual qResult Teardown();
        virtual qResult SendBroadcast(StationURL *, Buffer *);
        virtual qResult Send(StationURL *, Buffer *);
        virtual bool IsCapable(unsigned int);
        virtual void Trace(unsigned int);

        unsigned short GetListeningPort();
        PRUDPEndPoint *CreateEndPoint(const StationURL *, unsigned short, bool);
        void ReleaseEndPoint(PRUDPEndPoint *);
        bool Send(unsigned short, unsigned char, PacketOut *);
        PRUDPEndPoint *ServiceConnectionRequest(InetAddress *, Buffer *, unsigned short, unsigned char);
        void ServiceDisconnection(InetAddress *, Buffer *);
        void DeleteReleasedEndPoints();
        void ServiceTimeouts() {
            if (m_tLastTimeoutCheck == Time(0)) {
                m_tLastTimeoutCheck = Time::GetTime();
            } else if (Time::GetTime() - m_tLastTimeoutCheck > 25) {
                m_tLastTimeoutCheck = Time::GetTime();
                m_oPacketQueue.CheckTimeouts();
            }
        }

        typedef std::multimap<
            unsigned int,
            EndPoint *,
            std::less<unsigned int>,
            MemAllocator<std::pair<const unsigned int, EndPoint *> > >
            EndPointIDMap;

        class PacketQueue {
        public:
            PacketQueue();
            ~PacketQueue();
            void CheckTimeouts();
            char m_data[0x60];
        };

        class SessionIDTable {
        public:
            SessionIDTable();
            ~SessionIDTable();
            unsigned int Lookup(unsigned int, unsigned short);
            void *m_p;
        };

        PacketQueue m_oPacketQueue; // 0x14
        EndPointTable m_oEndPoints; // 0x74
        qList<PRUDPEndPoint *> m_lstReleasedEndPoints; // 0x90
        Time m_tLastTimeoutCheck; // 0x98
        bool m_bTerminating; // 0xa0
        unsigned int m_uiThreadID; // 0xa4
        String m_strWaterMarkLabel; // 0xa8
        WaterMark m_oWaterMark; // 0xac
        unsigned short m_usListeningPort; // 0xdc
        SessionIDTable m_oSessionIDs; // 0xe0
        EndPointIDMap m_mapPIDs; // 0xe4
        EndPointIDMap m_mapCIDs; // 0xfc
    };

}
