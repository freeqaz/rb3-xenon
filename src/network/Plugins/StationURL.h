#pragma once
#include "Platform/RootObject.h"
#include "Platform/String.h"
#include "Platform/qStd.h"
#include "Platform/Result.h"
#include "Plugins/Stream.h"

struct XNADDR;
struct XNKID;
struct XNKEY;

namespace Quazal {
    class InetAddress;
    class StringStream;

    // Retail layout (0x64 bytes, not polymorphic: no constructor stores a vtable).
    class StationURL : public RootObject {
    public:
        enum _URLType {
            Unknown = 0,
            prudp = 1,
            prudps = 2,
            udp = 3
        };

        StationURL();
        StationURL(const String &);
        StationURL(const char *);
        StationURL(const StationURL &);
        ~StationURL();

        StationURL &operator=(const StationURL &);
        StationURL &operator=(const char *);
        StationURL &operator=(const String &);
        bool operator==(const StationURL &) const;
        bool operator!=(const StationURL &) const;

        void SetURLType(_URLType);
        _URLType GetURLType() const;
        bool SetAddress(const char *);
        String GetAddress() const;
        bool SetInetAddress(const InetAddress *);
        InetAddress *GetInetAddress() const;
        void SetXboxAddress(const XNADDR *, const XNKID *, const XNKEY *);
        void SetXNAddr(const XNADDR *);
        void SetXNKid(const XNKID *);
        void SetXNKey(const XNKEY *);
        bool GetXNAddr(XNADDR *) const;
        bool GetXNKid(XNKID *) const;
        bool GetXNKey(XNKEY *) const;
        void SetPortNumber(unsigned short);
        unsigned short GetPortNumber() const;
        void SetStreamType(Stream::Type);
        Stream::Type GetStreamType() const;
        void SetStreamID(unsigned char);
        unsigned char GetStreamID() const;
        void SetPrincipalID(unsigned int);
        unsigned int GetPrincipalID() const;
        void SetConnectionID(unsigned int);
        unsigned int GetConnectionID() const;
        void SetRVConnectionID(unsigned int);
        unsigned int GetRVConnectionID() const;
        void SetType(unsigned int);
        unsigned int GetType() const;
        void SetXLSPID(unsigned int);
        unsigned int GetXLSPID() const;

        bool SetParam(const String &, const String &, bool);
        bool RemoveParam(const String &, bool);
        bool GetParam(const String &, String *, bool) const;

        void SetURL(const char *);
        const char *GetURL() const;
        qResult GetURLType(char *, unsigned int) const;
        unsigned int GetNbParams(bool) const;
        bool IsValid() const;
        bool IsSameHost(const StationURL &) const;
        void Trace(unsigned int) const;

    private:
        void ParseIfNeeded() const;
        void FormatIfNeeded() const;
        void Format();
        void Parse();
        void Copy(const StationURL &);
        _URLType ExtractURLType() const;
        bool ParseParams(const char *, bool);
        bool ParseParam(const char *, bool);
        qResult SetUIntParam(const String &, unsigned int);
        unsigned int GetUIntParam(const String &, unsigned int) const;
        qResult SetBinaryParam(const String &, const void *, unsigned int);
        bool GetBinaryParam(const String &, void *, unsigned int) const;

        InetAddress *m_pInetAddress; // 0x0
        _URLType m_eURLType; // 0x4
        qMap<String, unsigned int> m_mapUIntParams; // 0x8
        qMap<String, String> m_mapParams; // 0x24
        qMap<String, String> m_mapOptions; // 0x40
        char *m_szURL; // 0x5c
        bool m_bParsed; // 0x60
        bool m_bFormatted; // 0x61
        bool m_bValid; // 0x62
    };

    StringStream &operator<<(StringStream &, const StationURL &);
}
