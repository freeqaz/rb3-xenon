#pragma once
#include "Plugins/KeyedChecksumAlgorithm.h"

namespace Quazal {
    class HMACChecksum : public KeyedChecksumAlgorithm {
    public:
        HMACChecksum();
        virtual ~HMACChecksum();
        virtual bool ComputeChecksum(const Buffer &, Buffer *);
        virtual unsigned char GetChecksumLength();
        virtual void KeyHasChanged();

        unsigned char mKIPad[65]; // 0x24
        unsigned char mKOPad[65]; // 0x65
    };
}