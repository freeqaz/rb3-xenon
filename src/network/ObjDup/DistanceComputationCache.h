#pragma once
#include "Platform/RootObject.h"
#include "Platform/Time.h"

namespace Quazal {
    class DistanceComputationCache : public RootObject {
    public:
        DistanceComputationCache();
        ~DistanceComputationCache();

        float m_fLastDistance;      // 0x0
        int m_unk4;                 // 0x4
        int m_unk8;                 // 0x8
        int m_unkC;                 // 0xc
        int m_unk10;                // 0x10
        int m_unk14;                // 0x14

        static Time s_tMaximumUpdateDelay;
    };
}
