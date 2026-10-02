// Quazal NetZ - .\Transport\Routing\RoutingStream.cpp
//
// Retail TU: .text 0x82B0A518..0x82B0ADF0 (from the EH prefix of the
// constructor to the EH prefix of RoutingTable::RoutingTable, the first
// function of the next TU; its .rdata starts with its own "basic_string"
// string at 0x82188790). Built /Od /Oi- /Ob1 /GR- (see objects.json): the
// vtable at 0x821886F8 has no RTTI locator in front of it.
//
// The TU's own functions are the constructor, BuildRoutingPacket,
// ReceiveIncomingPacket and DoWork; then the COMDATs it instantiates, in
// first-reference order: Stream::GetSettings (declined by /Ob1 in the
// constructor), the scalar deleting destructor (which expands the in-class
// destructors of RoutingStream and RoutingTable) and ExtractRoutingHeader
// (declined in ReceiveIncomingPacket, which still reserves its frame).
//
// The classes are declared here with the retail X360 layouts, as far as the
// TU reads them. /Od: local NAMES decide the stack offsets.

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
        InetAddress();
        InetAddress(const InetAddress &);
        ~InetAddress();
        InetAddress &operator=(const InetAddress &);
        unsigned int GetAddress() const;
        unsigned short GetPortNumber() const;
        void SetAddress(unsigned int);
        void SetPortNumber(unsigned short);

        char m_data[0x80];
    };

    class Buffer : public RootObject {
    public:
        Buffer(unsigned int);
        virtual ~Buffer();
        void Clear();
        unsigned int GetContentSize() const;
        unsigned char *GetContentPtr() const;
        void AppendData(const void *, unsigned int, unsigned int);

        char m_data[0x1c];
    };

    class ByteStream : public RootObject {
    public:
        ByteStream();
        ByteStream(Buffer *);
        virtual ~ByteStream();
        void Append(const unsigned char *, unsigned int, unsigned int);
        void Extract(unsigned char *, unsigned int, unsigned int);
        ByteStream &operator<<(const Buffer &);

        Buffer *GetBuffer() { return m_pBuffer; }
        unsigned int GetPosition() { return m_uiPosition; }
        unsigned int GetSize() { return m_pBuffer->GetContentSize(); }

        Buffer *m_pBuffer; // 0x4
        unsigned int m_uiPosition; // 0x8
    };

    // 0x50 bytes, 8-aligned (the bundling settings at 0x10 hold a Time).
    class StreamSettings {
    public:
        unsigned int GetMaxSilenceTime() const;
        char m_pad00[0x10];
        __int64 m_tBundlingDelay; // 0x10
        char m_pad18[0x50 - 0x18];
    };

    class PseudoSingleton {
    public:
        static unsigned int GetCurrentContext();
    };

    template <class T>
    class PseudoGlobalVariable {
    public:
        T &GetValue() {
            unsigned int uiContext = PseudoSingleton::GetCurrentContext();
            if (uiContext == 0) {
                return mValueInDefaultContext;
            } else {
                return mValueInContextList[uiContext];
            }
        }
        operator T &() { return GetValue(); }

        void *m_vtable; // 0x0
        void *mNext; // 0x4
        T *mValueInContextList; // 0x8
        T mValueInDefaultContext; // 0x10
        T mDefaultValue; // 0x60
    };

    class Packet;
    class PacketOut;

    class Stream : public RootObject {
    public:
        enum Type {
        };
        Stream(Type);
        virtual ~Stream();
        virtual bool ReceiveIncomingPacket(unsigned short, unsigned char, Packet *) = 0;
        virtual void DoWork() = 0;

        Type GetType() { return m_eType; }
        StreamSettings *GetSettings() { return &(StreamSettings &)s_oStreamSettings[m_eType]; }

        static PseudoGlobalVariable<StreamSettings> s_oStreamSettings[16];

        Type m_eType; // 0x4
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
        unsigned char GetType() { return m_byTypeFlags & 7; }
        Buffer *GetPayload() { return m_pPayload; }

        char m_pad08[0x12 - 8];
        unsigned char m_byTypeFlags; // 0x12
        char m_pad13[0x24 - 0x13];
        Buffer *m_pPayload; // 0x24
        InetAddress m_oAddress; // 0x28
    };

    class PRUDPEndPoint;

    class PacketOut : public Packet {
    public:
        PacketOut(PRUDPEndPoint *, unsigned char, unsigned char, Buffer *);
        char m_padA8[0xe0 - 0xa8];
    };

    class RoutingAddressResolver {
    public:
        bool ResolveToID(const InetAddress &, unsigned short *) const;
        bool ResolveToAddress(unsigned short, InetAddress *) const;
    };

    class Router : public RoutingAddressResolver {
    public:
        unsigned int GetRoutingIPAddressTemplate();
    };

    class TransportPerfCounters {
    public:
        void Inc(unsigned int, int);
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
        virtual bool Receive(unsigned short, Buffer *, const InetAddress *);
        virtual Router *GetRouter();

        char m_pad04[0x18 - 4];
        TransportPerfCounters m_oPerfCounters; // 0x18
        TransportPerfCounters *GetPerfCounters() { return &m_oPerfCounters; }
    };

    class RoutingTable : public RootObject {
    public:
        RoutingTable(unsigned int);
        ~RoutingTable() { Clear(); }
        void Clear();
        bool Add(const InetAddress &, const InetAddress &, bool);
        void Remove(const InetAddress &);

        char m_data[0x28];
    };

    typedef bool (*RoutingCallback)(const InetAddress &, const InetAddress &, Packet *);

    class RoutingStream : public Stream {
    public:
        RoutingStream();
        virtual bool ReceiveIncomingPacket(unsigned short, unsigned char, Packet *);
        virtual void DoWork();

        static PacketOut *BuildRoutingPacket(Buffer *, unsigned char, const InetAddress &);
        static unsigned short GetHeaderSize() { return 12; }
        static bool IsValidRoutingPayload(Buffer *pBuffer) { return pBuffer != 0 && pBuffer->GetContentSize() > GetHeaderSize(); }
        static void ExtractRoutingHeader(Buffer *pPayload, InetAddress *pAddress, Buffer *pData) {
            ByteStream bs(pPayload);
            unsigned int uiIP;
            unsigned short usPort;
            bs.Extract((unsigned char *)&uiIP, 4, 1);
            bs.Extract((unsigned char *)&usPort, 2, 1);
            pAddress->SetAddress(uiIP);
            pAddress->SetPortNumber(usPort);
            pData->Clear();
            pData->AppendData(
                bs.GetBuffer()->GetContentPtr() + bs.GetPosition(), bs.GetSize() - bs.GetPosition(), -1
            );
        }

        RoutingTable m_oRoutingTable; // 0x8
        RootTransport *m_pTransport; // 0x30
        RoutingCallback m_pfnRoutingCallback; // 0x34
    };

    RoutingStream::RoutingStream()
        : Stream((Stream::Type)8), m_oRoutingTable(GetSettings()->GetMaxSilenceTime() * 3) {
        m_pTransport = 0;
        m_pfnRoutingCallback = 0;
    }

    PacketOut *RoutingStream::BuildRoutingPacket(Buffer *pBuffer, unsigned char byType, const InetAddress &oAddress) {
        ByteStream oStream;
        unsigned int uiAddress = oAddress.GetAddress();
        oStream.Append((unsigned char *)&uiAddress, 4, 1);
        unsigned short usPort = oAddress.GetPortNumber();
        oStream.Append((unsigned char *)&usPort, 2, 1);
        oStream << *pBuffer;
        PacketOut *pOut = new (__FILE__, 0x29) PacketOut(0, byType, 0, oStream.GetBuffer());
        return pOut;
    }

    bool RoutingStream::ReceiveIncomingPacket(unsigned short usPort, unsigned char, Packet *pPacket) {
        Router *pRouter = m_pTransport->GetRouter();
        bool bReturn = false;
        Buffer *pPayload = pPacket->GetPayload();
        if (IsValidRoutingPayload(pPayload)) {
            InetAddress oAddress;
            Buffer oPayload(0x400);
            ExtractRoutingHeader(pPayload, &oAddress, &oPayload);
            if (pPacket->GetType() == 2) {
                InetAddress oSender(oAddress);
                m_oRoutingTable.Add(oSender, pPacket->m_oAddress, false);
                if (m_pTransport->Receive(usPort, &oPayload, &oSender)) {
                    bReturn = true;
                } else {
                    m_oRoutingTable.Remove(oSender);
                }
            } else {
                InetAddress oDestination;
                InetAddress oSource;
                if (oAddress.GetAddress() == pRouter->GetRoutingIPAddressTemplate()) {
                    pRouter->ResolveToAddress(oAddress.GetPortNumber(), &oDestination);
                    unsigned short usID;
                    pRouter->ResolveToID(pPacket->m_oAddress, &usID);
                    oSource.SetAddress(pRouter->GetRoutingIPAddressTemplate());
                    oSource.SetPortNumber(usID);
                } else {
                    oDestination = oAddress;
                    oSource = pPacket->m_oAddress;
                }
                bool bAccept = true;
                if (m_pfnRoutingCallback != 0) {
                    bAccept = m_pfnRoutingCallback(oSource, oDestination, pPacket);
                }
                if (bAccept) {
                    PacketOut *pOut = BuildRoutingPacket(&oPayload, 2, oSource);
                    m_pTransport->GetPerfCounters()->Inc(11, (oPayload.GetContentSize() + 28) * 8);
                    m_pTransport->m_oPerfCounters.Inc(10, 1);
                    pOut->m_oAddress = oDestination;
                    m_pTransport->Send(usPort, GetType(), 1, 1, pOut, false);
                    pOut->ReleaseRef();
                    bReturn = true;
                }
            }
        }
        if (bReturn) {
            pPacket->ReleaseRef();
        }
        return bReturn;
    }

    void RoutingStream::DoWork() {}

}
