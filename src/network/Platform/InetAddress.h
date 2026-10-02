#pragma once

#include "types.h"
#include "decomp.h"
#include "Platform/RootObject.h"

namespace Quazal {
    class InetAddress : public RootObject {
    public:
        u8 unk0;
        u8 unk1;
        u16 port;
        s32 address;
        u8 unk8[0x78]; // retail sizeof is 0x80 (StationURL allocates 0x80)
        InetAddress();
        InetAddress(const InetAddress &);
        InetAddress(const char *, u16);
        ~InetAddress();

        void Init();

        bool SetAddress(const char *);
        void SetAddress(unsigned int);
        void SetNetworkAddress(unsigned int);
        unsigned int GetAddress() const;
        s32 GetAddress(char *, unsigned int) const;

        void SetPortNumber(u16);
        u16 GetPortNumber() const;

        bool operator<(const InetAddress &) const;
        bool operator==(const InetAddress &) const;
        InetAddress &operator=(const InetAddress &);
    };
}
