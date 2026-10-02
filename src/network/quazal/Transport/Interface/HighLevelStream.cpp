// Quazal NetZ - .\Transport\Interface\HighLevelStream.cpp
//
// Retail TU: .text 0x82B4EAC8..0x82B4F040 (from the EH prefix of the
// constructor to the end of the scalar deleting destructor). Built
// /Od /Oi- /Ob1 /GR- (see objects.json): the vtable at 0x8218FD20 has no
// RTTI locator in front of it.
//
// The classes this TU touches are declared here with the retail X360
// layouts, as far as the TU reads them. DoWork is an empty in-class virtual
// (its COMDAT folds into the shared empty body at 0x82AC5BA8);
// ReceiveIncomingMsg is out of line and stays in the TU.
//
// /Od: local NAMES decide the stack offsets, and an inline that /Ob1 declines
// still reserves its frame in the caller.

namespace Quazal {

    class RootObject {
    public:
        static void *operator new(unsigned int, const char *, unsigned int);
        static void operator delete(void *);
        static void operator delete(void *, const char *, unsigned int);
        ~RootObject() {}
    };

    class InetAddress {
    public:
        InetAddress &operator=(const InetAddress &);
        char m_data[0x80];
    };

    class Buffer;
    class StationURL;

    class Stream : public RootObject {
    public:
        enum Type {
        };
        Stream(Type);
        virtual ~Stream();
        virtual bool ReceiveIncomingPacket(unsigned short, unsigned char, class Packet *) = 0;
        virtual void DoWork() = 0;

        Type GetType() { return m_eType; }

        Type m_eType; // 0x4
    };

    class StationURL : public RootObject {
    public:
        StationURL();
        ~StationURL();
        bool SetInetAddress(const InetAddress *);
        InetAddress *GetInetAddress() const;
        void SetStreamType(Stream::Type);
        Stream::Type GetStreamType() const;
        void SetStreamID(unsigned char);
        unsigned char GetStreamID() const;

        unsigned int m_data[0x64 / 4];
    };

    // The PRUDP header byte that addresses a stream: type in the high nibble,
    // stream ID in the low one.
    class VirtualPort {
    public:
        VirtualPort(Stream::Type type, unsigned char id) { m_byValue = (type << 4) | id; }
        ~VirtualPort() {}
        unsigned char GetValue() const { return m_byValue; }

        unsigned char m_byValue;
    };

    class RefCountedObject : public RootObject {
    public:
        virtual ~RefCountedObject();
        virtual RefCountedObject *AcquireRef();
        virtual void ReleaseRef();
        unsigned short m_ui16RefCount; // 0x4
    };

    class Packet : public RefCountedObject {
    public:
        VirtualPort GetSourceVPort();
        void SetSourceVPort(VirtualPort vport) { m_bySourceVPort = vport.m_byValue; }
        void SetDestinationVPort(VirtualPort vport) { m_byDestinationVPort = vport.m_byValue; }
        Buffer *GetPayload() { return m_pPayload; }
        void SetDestination(const InetAddress *addr) { m_oAddress = *addr; }

        char m_pad08[0x10 - 8];
        unsigned char m_bySourceVPort; // 0x10
        unsigned char m_byDestinationVPort; // 0x11
        char m_pad12[0x24 - 0x12];
        Buffer *m_pPayload; // 0x24
        InetAddress m_oAddress; // 0x28
    };

    class PRUDPEndPoint;

    class PacketOut : public Packet {
    public:
        PacketOut(PRUDPEndPoint *, unsigned char, unsigned char, Buffer *);
        char m_padA8[0xe0 - 0xa8];
    };

    class TransportStreamManager {
    public:
        void RegisterStream(Stream *, unsigned char, unsigned char *);
        void UnregisterStream(Stream::Type, unsigned char);
        char m_data[0x18];
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
        virtual void Send(unsigned short, Stream::Type, unsigned char, unsigned char, PacketOut *, bool);

        char m_pad04[0x4d0 - 4];
        void RegisterStream(Stream *pStream, unsigned char byStreamID) {
            m_oStreamManager.RegisterStream(pStream, byStreamID, 0);
        }

        TransportStreamManager m_oStreamManager; // 0x4d0
        unsigned short m_usPort; // 0x4e8
    };

    class InstanceControl {
    public:
        void *m_pad[3];
        void *m_pDelegatorInstance; // 0xc
    };

    class InstanceTable {
    public:
        unsigned int GetInstanceFromVector(unsigned int, unsigned int);
        static InstanceTable s_oInstanceTable;
    };

    class PseudoSingleton {
    public:
        static unsigned int GetCurrentContext();
    };

    // GetInstance's only retail body is the /O1 copy it folded into
    // (0x823EBC90, mapped as NetworkEmulator's anonymous-namespace function).
    // /Ob1 declines it, so the constructor reserves its locals; the five
    // below are the count that reproduces the constructor's frame, not a
    // body read from retail.
    class TransportDelegator {
    public:
        static TransportDelegator *GetInstance() {
            unsigned int uiContext = PseudoSingleton::GetCurrentContext();
            unsigned int uiInstance = InstanceTable::s_oInstanceTable.GetInstanceFromVector(1, uiContext);
            InstanceControl *inst = (InstanceControl *)uiInstance;
            TransportDelegator *pDelegator = inst ? (TransportDelegator *)inst->m_pDelegatorInstance : 0;
            TransportDelegator *pResult = pDelegator;
            return pResult;
        }
        RootTransport *GetTransport() { return m_pTransport; }

        char m_pad[0x4c];
        RootTransport *m_pTransport; // 0x4c
    };

    inline RootTransport *GetDefaultTransport() {
        TransportDelegator *d = TransportDelegator::GetInstance();
        if (d == 0)
            return 0;
        else
            return d->GetTransport();
    }

    class HighLevelStream : public Stream {
    public:
        HighLevelStream(Stream::Type, unsigned char, RootTransport *);
        virtual ~HighLevelStream();
        virtual bool ReceiveIncomingPacket(unsigned short, unsigned char, Packet *);
        virtual void DoWork() {}
        virtual void ReceiveIncomingMsg(StationURL *, Buffer *, unsigned short);
        virtual void Send(StationURL *, Buffer *, unsigned short);

        RootTransport *GetTransport() { return m_pTransport; }
        unsigned char GetStreamID() { return m_byStreamID; }

        RootTransport *m_pTransport; // 0x8
        unsigned char m_byStreamID; // 0xc
    };

    HighLevelStream::HighLevelStream(Stream::Type type, unsigned char byStreamID, RootTransport *pTransport)
        : Stream(type) {
        m_pTransport = pTransport;
        m_byStreamID = byStreamID;
        if (m_pTransport == 0) {
            m_pTransport = GetDefaultTransport();
        }
        m_pTransport->RegisterStream(this, m_byStreamID);
    }

    HighLevelStream::~HighLevelStream() {
        GetTransport()->m_oStreamManager.UnregisterStream(GetType(), 1);
    }

    bool HighLevelStream::ReceiveIncomingPacket(unsigned short usPort, unsigned char, Packet *pPacket) {
        StationURL url;
        url.SetInetAddress(&pPacket->m_oAddress);
        VirtualPort source = pPacket->GetSourceVPort();
        url.SetStreamType((Stream::Type)(source.m_byValue >> 4));
        url.SetStreamID(source.m_byValue & 0xF);
        ReceiveIncomingMsg(&url, pPacket->GetPayload(), usPort);
        pPacket->ReleaseRef();
        return true;
    }

    void HighLevelStream::ReceiveIncomingMsg(StationURL *, Buffer *, unsigned short) {}

    void HighLevelStream::Send(StationURL *pURL, Buffer *pBuffer, unsigned short usPort) {
        Stream::Type eType = pURL->GetStreamType();
        unsigned char byID = pURL->GetStreamID();
        unsigned short usSendPort;
        if (usPort == 0)
            usSendPort = m_pTransport->m_usPort;
        else
            usSendPort = usPort;
        PacketOut *pPacket = new (__FILE__, 0x44) PacketOut(0, 5, 0, pBuffer);
        pPacket->SetDestination(pURL->GetInetAddress());
        pPacket->SetSourceVPort(VirtualPort(GetType(), m_byStreamID));
        pPacket->SetDestinationVPort(VirtualPort(eType, byID));
        m_pTransport->Send(usSendPort, GetType(), m_byStreamID, byID, pPacket, false);
        pPacket->ReleaseRef();
    }

}
