#pragma once
#include "Platform/RootObject.h"
#include "Platform/ScopedCS.h"
#include "Platform/String.h"
#include "Platform/Result.h"
#include "Platform/WaterMark.h"
#include "Platform/qStd.h"

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

        char m_data[0x80];
    };

    class StreamID {
    public:
        StreamID(unsigned char id) : m_byID(id) {}
        unsigned char m_byID;
    };

    class StationURL : public RootObject {
    public:
        StationURL();
        ~StationURL();
        StationURL &operator=(const StationURL &);
        bool IsValid() const;
        unsigned char GetStreamID() const;
        InetAddress *GetInetAddress() const;
        int GetURLType() const;
        void SetAddress(const char *);
        void SetAddress(const InetAddress *);
        void SetPortNumber(unsigned short);
        void SetStreamID(unsigned char);

        char m_data[0x58];
    };

    class Time {
    public:
        Time() : m_ui64Value(0) {}
        Time(long long t) : m_ui64Value(t) {}
        Time &operator=(const Time &);
        Time &operator=(unsigned int);
        bool operator==(const Time &t) const { return m_ui64Value == t.m_ui64Value; }
        long long operator-(const Time &) const;
        static Time GetTime();

        unsigned long long m_ui64Value;
    };

    class EndPointUniqueID {
    public:
        EndPointUniqueID(InetAddress addr, unsigned char id) {
            m_oAddress = addr;
            m_byStreamID = id;
        }
        bool operator<(const EndPointUniqueID &) const;

        InetAddress m_oAddress; // 0x0
        unsigned char m_byStreamID; // 0x80
    };

    class EndPoint : public RootObject {
    public:
        virtual ~EndPoint();
        virtual void Unk04();
        virtual void Unk08();
        virtual void Unk0C();
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
        virtual void SetFlag(unsigned int);
        virtual void Unk48();
        virtual void Unk4C();
        virtual void Unk50();
        virtual void Unk54();
        virtual void Unk58();
        virtual void Release();

        void Open();
        void Close();
        void SetPID(unsigned int);
        void SetCID(unsigned int);
        bool IsReleased() { return m_pReleaser != NULL; }

        StationURL m_oURL; // 0x8
        char m_pad[0x7c - 0x8 - sizeof(StationURL)];
        void *m_pReleaser; // 0x7c
        unsigned short m_usPort; // 0x80
    };

    class PRUDPEndPoint : public EndPoint {
    public:
        PRUDPEndPoint(PRUDPStream *, const StationURL *);
    };

    class EndPointTable : public qMap<EndPointUniqueID, PRUDPEndPoint *> {
    public:
        PRUDPEndPoint *Find(const InetAddress *addr, StreamID id) {
            EndPointUniqueID key(*addr, id.m_byID);
            iterator it = find(key);
            if (it == end())
                return NULL;
            else
                return it->second;
        }
        void Add(PRUDPEndPoint *ep, StreamID id);
        void Remove(const InetAddress *addr, StreamID id) {
            EndPointUniqueID key(*addr, id.m_byID);
            iterator it = find(key);
            if (it == end())
                return;
            else
                erase(it);
        }
        void Trace(unsigned int);
    };

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

        Type m_eType; // 0x4
        class StreamListener *m_pListener; // 0x8
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
        void ServiceTimeouts();

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
            void Flush();
            char m_data[0x60];
        };

        class SessionIDTable {
        public:
            SessionIDTable();
            ~SessionIDTable();
            unsigned int Lookup(const InetAddress *, unsigned short);
            void *m_p;
        };

        PacketQueue m_oPacketQueue; // 0x14
        EndPointTable m_oEndPoints; // 0x74
        int m_unk8c; // 0x8c
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
