// Quazal NetZ - .\Transport\Interface\TransportSignatureGenerator.cpp
// Retail TU: .text 0x82B2FC40..0x82B30278, built /Od /Oi- /Ob1 /GR-
// (objects.json).
//
// The generator keys an HMAC with a checksum of the current time, then signs
// (station id, port) pairs with it. The key buffer and both bit streams are
// locals, so every function carries unwind actions for them.

#include "Platform/Holder.h"
#include "Platform/Time.h"
#include "Plugins/BitStream.h"
#include "Plugins/HMACChecksum.h"

#define TSG_FILE ".\\Transport\\Interface\\TransportSignatureGenerator.cpp"

namespace Quazal {

    class TransportSignatureGenerator : public RootObject {
    public:
        TransportSignatureGenerator();
        ~TransportSignatureGenerator();

        unsigned int ComputeSourceSignature(unsigned int, unsigned short);

        Holder<KeyedChecksumAlgorithm> m_hChecksum; // 0x0
    };

    inline void AppendTime(BitStream &oStream, const unsigned long long &ui64Time) {
        oStream.Append((const unsigned char *)&ui64Time, sizeof(ui64Time), 1);
    }


    TransportSignatureGenerator::TransportSignatureGenerator() {
#line 20
        m_hChecksum = new (TSG_FILE, __LINE__) HMACChecksum();
        Time tNow = Time::GetTime();
        Buffer oSourceBuffer(0x400);
        BitStream oSourceStream(&oSourceBuffer);
        AppendTime(oSourceStream, Time::GetTime());
        Buffer oKeyBuffer(0x400);
        m_hChecksum->ComputeChecksum(oSourceBuffer, &oKeyBuffer);
        Key oSeedKey(oKeyBuffer.GetContentPtr(), oKeyBuffer.GetContentSize());
        ((KeyedChecksumAlgorithm *)m_hChecksum)->SetKey(oSeedKey);
    }

    TransportSignatureGenerator::~TransportSignatureGenerator() {}

    unsigned int
    TransportSignatureGenerator::ComputeSourceSignature(unsigned int uiID, unsigned short usPort) {
        Buffer oInBuffer(0x400);
        BitStream oInStream(&oInBuffer);
        oInStream.Append((const unsigned char *)&uiID, 4, 1);
        oInStream.Append((const unsigned char *)&usPort, 2, 1);
        Buffer oSigBuffer(0x400);
        m_hChecksum->ComputeChecksum(oInBuffer, &oSigBuffer);
        BitStream oSigStream(&oSigBuffer);
        unsigned int uiSig;
        oSigStream.Extract((unsigned char *)&uiSig, 4, 1);
        return uiSig;
    }

}
